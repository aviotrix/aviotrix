#include <algorithm>
#include <memory>
#include <set>

#include "avio_output.h"
#include "aviotrix/media_reader.h"
#include "dts_synthesizer.h"
#include "log_router.h"
#include "media_reader_impl.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}

namespace aviotrix {

namespace {

bool isMp4Family(const AVOutputFormat* ofmt) {
  const std::string name = ofmt->name ? ofmt->name : "";
  return name == "mp4" || name == "mov";
}

struct OutputContextDeleter {
  void operator()(AVFormatContext* oc) const {
    if (oc) avformat_free_context(oc);
  }
};
using OutputContext = std::unique_ptr<AVFormatContext, OutputContextDeleter>;

struct PacketDeleter {
  void operator()(AVPacket* p) const { av_packet_free(&p); }
};

}  // namespace

Status MediaReader::remux(IoSink& sink, const RemuxOptions& opt, RemuxResult& result) {
  result = RemuxResult{};
  if (!impl_->open) return Status::Error(ErrorCode::NotOpen, "reader is not open");
  detail::ScopedLogTarget logTarget(&impl_->log);
  AVFormatContext* in = impl_->fmt;

  // 1. Validate everything that can be validated before touching the sink.
  const AVOutputFormat* ofmt = av_guess_format(opt.format.c_str(), nullptr, nullptr);
  if (!ofmt) return Status::Error(ErrorCode::InvalidArgument, "unknown output format: " + opt.format);
  if (!sink.seekable() && isMp4Family(ofmt) && !opt.fragmented) {
    return Status::Error(ErrorCode::SinkNotSeekable,
                         std::string(ofmt->name) + " output needs a seekable sink unless fragmented=true");
  }
  if (impl_->consumed && !impl_->input->seekable()) {
    return Status::Error(ErrorCode::Unsupported, "input is not seekable; it can be remuxed only once");
  }

  std::vector<int> selected;
  if (opt.streams) {
    std::set<int> seen;
    for (int idx : *opt.streams) {
      if (idx < 0 || idx >= static_cast<int>(in->nb_streams)) {
        return Status::Error(ErrorCode::InvalidArgument, "stream index " + std::to_string(idx) + " is out of range");
      }
      if (seen.insert(idx).second) selected.push_back(idx);
    }
  } else {
    for (unsigned i = 0; i < in->nb_streams; i++) selected.push_back(static_cast<int>(i));
  }
  if (selected.empty()) return Status::Error(ErrorCode::InvalidArgument, "no streams selected");

  AVFormatContext* rawOc = nullptr;
  int ret = avformat_alloc_output_context2(&rawOc, ofmt, nullptr, nullptr);
  if (ret < 0 || !rawOc) return Status::FromAv(ret < 0 ? ret : AVERROR(ENOMEM), "avformat_alloc_output_context2");
  OutputContext oc(rawOc);

  std::vector<int> outIndex(in->nb_streams, -1);
  for (int idx : selected) {
    const AVStream* ist = in->streams[idx];
    const AVCodecParameters* par = ist->codecpar;
    const int q = avformat_query_codec(ofmt, par->codec_id, FF_COMPLIANCE_NORMAL);
    if (q == 0) {
      std::string reason =
          std::string("codec ") + avcodec_get_name(par->codec_id) + " is not supported by muxer " + ofmt->name;
      if (opt.failOnIncompatible) return Status::Error(ErrorCode::IncompatibleStream, reason);
      av_log(oc.get(), AV_LOG_WARNING, "aviotrix: skipping stream %d: %s\n", idx, reason.c_str());
      result.streams.push_back({idx, std::nullopt, reason});
      continue;
    }
    AVStream* ost = avformat_new_stream(oc.get(), nullptr);
    if (!ost) return Status::FromAv(AVERROR(ENOMEM), "avformat_new_stream");
    ret = avcodec_parameters_copy(ost->codecpar, par);
    if (ret < 0) return Status::FromAv(ret, "avcodec_parameters_copy");
    ost->codecpar->codec_tag = 0;
    ost->time_base = ist->time_base;
    av_dict_copy(&ost->metadata, ist->metadata, 0);
    outIndex[idx] = ost->index;
    result.streams.push_back({idx, ost->index, ""});
  }
  if (oc->nb_streams == 0) {
    result.streams.clear();
    return Status::Error(ErrorCode::IncompatibleStream,
                         std::string("no selected stream is supported by muxer ") + ofmt->name);
  }

  // 2. Rewind if a previous remux consumed the input.
  if (impl_->consumed) {
    ret = avformat_seek_file(in, -1, INT64_MIN, 0, INT64_MAX, 0);
    if (ret < 0) return Status::FromAv(ret, "avformat_seek_file");
  }
  impl_->consumed = true;
  const int64_t bytesReadAtStart = impl_->input->bytesRead();

  // 3. Open the sink and write.
  std::unique_ptr<detail::AvioOutput> output;
  Status st = detail::AvioOutput::create(sink, output);
  if (!st.ok()) return st;
  oc->pb = output->context();
  oc->flags |= AVFMT_FLAG_CUSTOM_IO;
  impl_->input->setCancelFlag(opt.cancel);

  AVDictionary* muxOpts = nullptr;
  if (opt.fragmented && isMp4Family(ofmt))
    av_dict_set(&muxOpts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
  ret = avformat_write_header(oc.get(), &muxOpts);
  av_dict_free(&muxOpts);
  const char* failedCall = "avformat_write_header";
  bool headerWritten = ret >= 0;

  std::unique_ptr<AVPacket, PacketDeleter> pkt(av_packet_alloc());
  std::optional<double> lastTimestamp;
  // Video streams whose demuxer supplies PTS only (e.g. Matroska with B-frames) get DTS synthesized.
  enum class DtsMode { Undecided, Passthrough, Synthesize };
  std::vector<DtsMode> dtsMode(oc->nb_streams, DtsMode::Undecided);
  std::vector<std::unique_ptr<detail::DtsSynthesizer>> synths(oc->nb_streams);

  // Writes one packet (taking ownership of its data) and reports progress.
  auto writePacket = [&](AVPacket* p) {
    const AVStream* ost = oc->streams[p->stream_index];
    if (p->pts != AV_NOPTS_VALUE) lastTimestamp = p->pts * av_q2d(ost->time_base);
    const int wret = av_interleaved_write_frame(oc.get(), p);
    if (wret < 0) {
      failedCall = "av_interleaved_write_frame";
      return wret;
    }
    result.packets++;
    if (opt.onProgress && result.packets % opt.progressIntervalPackets == 0) {
      opt.onProgress(
          RemuxProgress{impl_->input->bytesRead() - bytesReadAtStart, output->bytesWritten(), lastTimestamp});
    }
    return 0;
  };
  // Writes and frees packets handed out by a synthesizer; frees all of them even after a failure.
  auto writeAndFree = [&](std::vector<AVPacket*>& ready) {
    int wret = 0;
    for (AVPacket*& p : ready) {
      if (wret >= 0) wret = writePacket(p);
      av_packet_free(&p);
    }
    ready.clear();
    return wret;
  };

  if (headerWritten) {
    ret = 0;
    while (true) {
      if (opt.cancel && opt.cancel->load(std::memory_order_relaxed)) {
        ret = AVERROR_EXIT;
        break;
      }
      ret = av_read_frame(in, pkt.get());
      if (ret < 0) {
        failedCall = "av_read_frame";
        break;
      }
      const int oi = outIndex[pkt->stream_index];
      if (oi < 0) {
        av_packet_unref(pkt.get());
        continue;
      }
      const AVStream* ist = in->streams[pkt->stream_index];
      const AVStream* ost = oc->streams[oi];
      av_packet_rescale_ts(pkt.get(), ist->time_base, ost->time_base);
      pkt->stream_index = oi;
      pkt->pos = -1;

      if (dtsMode[oi] == DtsMode::Undecided) {
        const bool needsDts = ost->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                              (pkt->dts == AV_NOPTS_VALUE || (pkt->pts != AV_NOPTS_VALUE && pkt->dts > pkt->pts));
        dtsMode[oi] = needsDts ? DtsMode::Synthesize : DtsMode::Passthrough;
        if (needsDts) synths[oi] = std::make_unique<detail::DtsSynthesizer>();
      }
      if (dtsMode[oi] == DtsMode::Synthesize) {
        AVPacket* owned = av_packet_alloc();
        if (!owned) {
          ret = AVERROR(ENOMEM);
          failedCall = "av_packet_alloc";
          break;
        }
        av_packet_move_ref(owned, pkt.get());
        std::vector<AVPacket*> ready;
        synths[oi]->push(owned, ready);
        ret = writeAndFree(ready);
      } else {
        ret = writePacket(pkt.get());  // takes ownership of the packet data
      }
      if (ret < 0) break;
    }
    if (ret == AVERROR_EOF) {
      ret = 0;
      for (auto& synth : synths) {
        if (!synth) continue;
        std::vector<AVPacket*> ready;
        synth->flush(ready);
        ret = writeAndFree(ready);
        if (ret < 0) break;
      }
    }
    if (ret == 0) {
      ret = av_write_trailer(oc.get());
      if (ret < 0) failedCall = "av_write_trailer";
    }
  }

  // 4. Always close the sink; decide the final status.
  impl_->input->setCancelFlag(nullptr);
  Status closeSt = output->closeIo();
  result.bytesRead = impl_->input->bytesRead() - bytesReadAtStart;
  result.bytesWritten = output->bytesWritten();

  Status final = Status::Ok();
  if (ret == AVERROR_EXIT) {
    final = Status::Error(ErrorCode::Aborted, "remux aborted");
  } else if (ret < 0) {
    if (!output->lastError().ok())
      final = output->lastError();
    else if (!impl_->input->lastError().ok())
      final = impl_->input->lastError();
    else
      final = Status::FromAv(ret, failedCall);
  } else if (!closeSt.ok()) {
    final = closeSt;
  }
  if (final.ok() && opt.onProgress) {
    opt.onProgress(RemuxProgress{result.bytesRead, result.bytesWritten, lastTimestamp});
  }
  return final;
}

}  // namespace aviotrix
