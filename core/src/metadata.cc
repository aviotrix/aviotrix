#include "aviotrix/metadata.h"

#include "metadata_internal.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
}

namespace aviotrix {

const char* streamTypeName(StreamType type) {
  switch (type) {
    case StreamType::Video:
      return "video";
    case StreamType::Audio:
      return "audio";
    case StreamType::Subtitle:
      return "subtitle";
    case StreamType::Data:
      return "data";
    case StreamType::Attachment:
      return "attachment";
  }
  return "data";
}

namespace detail {

namespace {

std::map<std::string, std::string> dictToMap(const AVDictionary* dict) {
  std::map<std::string, std::string> out;
  const AVDictionaryEntry* e = nullptr;
  while ((e = av_dict_iterate(dict, e)) != nullptr) out.emplace(e->key, e->value);
  return out;
}

StreamType typeOf(AVMediaType t) {
  switch (t) {
    case AVMEDIA_TYPE_VIDEO:
      return StreamType::Video;
    case AVMEDIA_TYPE_AUDIO:
      return StreamType::Audio;
    case AVMEDIA_TYPE_SUBTITLE:
      return StreamType::Subtitle;
    case AVMEDIA_TYPE_ATTACHMENT:
      return StreamType::Attachment;
    default:
      return StreamType::Data;
  }
}

std::optional<double> secondsOf(int64_t ts, AVRational tb) {
  if (ts == AV_NOPTS_VALUE) return std::nullopt;
  return static_cast<double>(ts) * av_q2d(tb);
}

std::optional<Rational> frameRateOf(const AVStream* st) {
  if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0)
    return Rational{st->avg_frame_rate.num, st->avg_frame_rate.den};
  if (st->r_frame_rate.num > 0 && st->r_frame_rate.den > 0) return Rational{st->r_frame_rate.num, st->r_frame_rate.den};
  return std::nullopt;
}

std::optional<std::string> codecTagOf(uint32_t tag) {
  if (tag == 0) return std::nullopt;
  char buf[AV_FOURCC_MAX_STRING_SIZE];
  return std::string(av_fourcc_make_string(buf, tag));
}

}  // namespace

Metadata readMetadata(const AVFormatContext* fmt) {
  Metadata m;
  m.format = fmt->iformat->name ? fmt->iformat->name : "";
  m.formatLongName = fmt->iformat->long_name ? fmt->iformat->long_name : "";
  m.startTime = secondsOf(fmt->start_time, AVRational{1, AV_TIME_BASE});
  if (fmt->duration > 0) m.duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
  if (fmt->bit_rate > 0) m.bitRate = fmt->bit_rate;
  m.tags = dictToMap(fmt->metadata);

  for (unsigned i = 0; i < fmt->nb_streams; i++) {
    const AVStream* st = fmt->streams[i];
    const AVCodecParameters* par = st->codecpar;
    StreamInfo info;
    info.index = static_cast<int>(i);
    info.type = typeOf(par->codec_type);
    info.codec = avcodec_get_name(par->codec_id);
    info.codecTag = codecTagOf(par->codec_tag);
    info.timeBase = Rational{st->time_base.num, st->time_base.den};
    info.startTime = secondsOf(st->start_time, st->time_base);
    if (st->duration > 0) info.duration = static_cast<double>(st->duration) * av_q2d(st->time_base);
    if (par->bit_rate > 0) info.bitRate = par->bit_rate;
    info.tags = dictToMap(st->metadata);
    if (auto it = info.tags.find("language"); it != info.tags.end()) info.language = it->second;
    if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
      VideoStreamInfo v;
      v.width = par->width;
      v.height = par->height;
      v.frameRate = frameRateOf(st);
      if (par->format >= 0) {
        if (const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(par->format))) v.pixelFormat = name;
      }
      info.video = v;
    } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
      AudioStreamInfo a;
      a.sampleRate = par->sample_rate;
      a.channels = par->ch_layout.nb_channels;
      char buf[128];
      if (av_channel_layout_describe(&par->ch_layout, buf, sizeof buf) > 0) a.channelLayout = buf;
      info.audio = a;
    }
    m.streams.push_back(std::move(info));
  }
  return m;
}

}  // namespace detail
}  // namespace aviotrix
