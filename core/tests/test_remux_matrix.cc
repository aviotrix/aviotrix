// Spec section 10.2: every fixture x every muxer, in skip mode, with a full packet-level readback.
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "aviotrix/media_reader.h"
#include "aviotrix/remux.h"
#include "file_io.h"
#include "fixtures.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
}

using aviotrix::ErrorCode;
using aviotrix::MediaReader;
using aviotrix::RemuxOptions;
using aviotrix::RemuxResult;
using aviotrix::Status;

namespace {

struct MemoryCursor {
  const std::vector<uint8_t>* bytes;
  int64_t pos = 0;
};

int readMem(void* opaque, uint8_t* buf, int size) {
  auto* c = static_cast<MemoryCursor*>(opaque);
  const int64_t left = static_cast<int64_t>(c->bytes->size()) - c->pos;
  if (left <= 0) return AVERROR_EOF;
  const int n = static_cast<int>(std::min<int64_t>(left, size));
  std::memcpy(buf, c->bytes->data() + c->pos, static_cast<size_t>(n));
  c->pos += n;
  return n;
}

int64_t seekMem(void* opaque, int64_t offset, int whence) {
  auto* c = static_cast<MemoryCursor*>(opaque);
  const auto size = static_cast<int64_t>(c->bytes->size());
  if (whence == AVSEEK_SIZE) return size;
  switch (whence & ~AVSEEK_FORCE) {
    case SEEK_SET:
      c->pos = offset;
      break;
    case SEEK_CUR:
      c->pos += offset;
      break;
    case SEEK_END:
      c->pos = size + offset;
      break;
    default:
      return AVERROR(EINVAL);
  }
  return c->pos;
}

// Demuxes every packet of `bytes` with libavformat directly; returns the packet count, or a negative AVERROR.
int64_t countPackets(const std::vector<uint8_t>& bytes) {
  MemoryCursor cursor{&bytes};
  constexpr int kBufSize = 32768;
  auto* buf = static_cast<uint8_t*>(av_malloc(kBufSize));
  AVIOContext* pb = avio_alloc_context(buf, kBufSize, 0, &cursor, readMem, nullptr, seekMem);
  AVFormatContext* fmt = avformat_alloc_context();
  fmt->pb = pb;
  int64_t count = 0;
  int ret = avformat_open_input(&fmt, nullptr, nullptr, nullptr);
  if (ret >= 0) ret = avformat_find_stream_info(fmt, nullptr);
  if (ret >= 0) {
    AVPacket* pkt = av_packet_alloc();
    while ((ret = av_read_frame(fmt, pkt)) >= 0) {
      count++;
      av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    if (ret == AVERROR_EOF) ret = 0;
  }
  avformat_close_input(&fmt);
  av_freep(static_cast<void*>(&pb->buffer));
  avio_context_free(&pb);
  return ret < 0 ? ret : count;
}

}  // namespace

TEST_CASE("remux matrix: every fixture to every muxer") {
  const char* fixtures[] = {"h264-aac.mp4", "vp9-opus.webm", "h264-ac3.ts", "h264-aac-srt.mkv"};
  const char* muxers[] = {"mp4", "mov", "matroska", "webm", "mpegts"};
  for (const char* fixture : fixtures) {
    for (const char* muxer : muxers) {
      DYNAMIC_SECTION(fixture << " -> " << muxer) {
        test::FileSource src(test::fixturePath(fixture));
        MediaReader reader;
        REQUIRE(reader.open(src).ok());
        const aviotrix::Metadata in = reader.metadata();
        test::MemorySink sink(true);
        RemuxOptions opt;
        opt.format = muxer;
        RemuxResult result;
        const Status st = reader.remux(sink, opt, result);
        std::string summary;
        for (const auto& s : result.streams) {
          summary += " " + in.streams[static_cast<size_t>(s.input)].codec + (s.output ? "=ok" : "=skip");
        }
        std::printf("matrix %s -> %s: %s%s\n", fixture, muxer, st.ok() ? "ok" : st.message.c_str(), summary.c_str());
        if (!st.ok()) {
          REQUIRE(st.code == static_cast<int>(ErrorCode::IncompatibleStream));
          REQUIRE_FALSE(sink.opened());
          continue;
        }
        std::vector<std::string> mapped;
        for (const auto& s : result.streams) {
          if (s.output) mapped.push_back(in.streams[static_cast<size_t>(s.input)].codec);
        }

        test::MemorySource outSrc(sink.bytes());
        MediaReader out;
        REQUIRE(out.open(outSrc).ok());
        const aviotrix::Metadata m = out.metadata();
        REQUIRE(m.streams.size() == mapped.size());
        for (size_t i = 0; i < mapped.size(); i++) REQUIRE(m.streams[i].codec == mapped[i]);
        REQUIRE(m.duration.has_value());
        REQUIRE(std::abs(*m.duration - *in.duration) < 0.2);
        REQUIRE(m.startTime.has_value());
        REQUIRE(*m.startTime <= 0.1);
        REQUIRE(countPackets(sink.bytes()) > 0);
      }
    }
  }
}
