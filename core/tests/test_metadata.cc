#include <catch2/catch_test_macros.hpp>

#include "../src/log_router.h"
#include "aviotrix/media_reader.h"
#include "file_io.h"
#include "fixtures.h"

extern "C" {
#include <libavutil/log.h>
}

using aviotrix::MediaReader;
using aviotrix::StreamType;

TEST_CASE("open mp4 fixture and read metadata") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  REQUIRE(reader.isOpen());
  const auto& m = reader.metadata();
  REQUIRE(m.format == "mov,mp4,m4a,3gp,3g2,mj2");
  REQUIRE(m.streams.size() == 2);
  REQUIRE(m.duration.has_value());
  REQUIRE(*m.duration > 2.9);
  REQUIRE(*m.duration < 3.2);
  REQUIRE(m.streams[0].type == StreamType::Video);
  REQUIRE(m.streams[0].codec == "h264");
  REQUIRE(m.streams[0].video.has_value());
  REQUIRE(m.streams[0].video->width == 320);
  REQUIRE(m.streams[0].video->height == 240);
  REQUIRE(m.streams[0].video->frameRate.has_value());
  REQUIRE(m.streams[0].video->frameRate->num == 30);
  REQUIRE(m.streams[0].video->frameRate->den == 1);
  REQUIRE(m.streams[1].type == StreamType::Audio);
  REQUIRE(m.streams[1].codec == "aac");
  REQUIRE(m.streams[1].audio.has_value());
  REQUIRE(m.streams[1].audio->sampleRate == 48000);
  REQUIRE(m.streams[1].audio->channels == 1);
  REQUIRE(reader.close().ok());
  REQUIRE_FALSE(reader.isOpen());
  REQUIRE(src.closes() == 1);
}

TEST_CASE("open webm and ts fixtures; TS parsers fill dimensions") {
  {
    test::FileSource src(test::fixturePath("vp9-opus.webm"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    REQUIRE(reader.metadata().format == "matroska,webm");
    REQUIRE(reader.metadata().streams[0].codec == "vp9");
    REQUIRE(reader.metadata().streams[1].codec == "opus");
  }
  {
    test::FileSource src(test::fixturePath("h264-ac3.ts"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    REQUIRE(reader.metadata().format == "mpegts");
    REQUIRE(reader.metadata().streams[0].codec == "h264");
    REQUIRE(reader.metadata().streams[0].video->width == 320);
    REQUIRE(reader.metadata().streams[1].codec == "ac3");
    REQUIRE(reader.metadata().streams[1].audio->sampleRate == 48000);
  }
}

TEST_CASE("mkv with srt reports a subtitle stream") {
  test::FileSource src(test::fixturePath("h264-aac-srt.mkv"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  REQUIRE(reader.metadata().streams.size() == 3);
  REQUIRE(reader.metadata().streams[2].type == StreamType::Subtitle);
  REQUIRE(reader.metadata().streams[2].codec == "subrip");
}

TEST_CASE("non-media and empty inputs fail cleanly") {
  test::FileSource txt(test::fixturePath("not-media.txt"));
  MediaReader a;
  aviotrix::Status s = a.open(txt);
  REQUIRE_FALSE(s.ok());
  REQUIRE(s.code < 0);
  REQUIRE_FALSE(a.isOpen());
  REQUIRE(txt.closes() == 1);

  test::MemorySource empty({});
  MediaReader b;
  REQUIRE_FALSE(b.open(empty).ok());
  REQUIRE_FALSE(b.isOpen());
}

TEST_CASE("truncated size yields EOF, never reads past the reported size") {
  struct Truncated final : aviotrix::IoSource {
    test::FileSource inner{test::fixturePath("h264-aac.mp4")};
    int64_t reported = 0;
    int64_t maxEnd = 0;
    aviotrix::Status open(std::optional<int64_t>& size) override {
      auto s = inner.open(size);
      reported = *size / 2;
      size = reported;
      return s;
    }
    aviotrix::Status read(int64_t o, std::span<uint8_t> b, size_t& n) override {
      maxEnd = std::max(maxEnd, o + static_cast<int64_t>(b.size()));
      if (o >= reported) {
        n = 0;
        return aviotrix::Status::Ok();
      }
      auto s = inner.read(
          o, b.first(static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(b.size()), reported - o))), n);
      return s;
    }
    aviotrix::Status close() override { return inner.close(); }
  } src;
  MediaReader reader;
  (void)reader.open(src);  // faststart mp4: may succeed with a short duration, or fail; both are fine
  REQUIRE(src.maxEnd <= src.reported + 65536);  // one AVIO buffer of slack past the reported end
  (void)reader.close();
}

TEST_CASE("log router joins partial lines and routes to the current hook") {
  std::vector<std::pair<aviotrix::LogLevel, std::string>> got;
  aviotrix::LogHook hook = [&](aviotrix::LogLevel lvl, std::string_view text) {
    got.emplace_back(lvl, std::string(text));
  };
  aviotrix::detail::installAvLogCallback();
  {
    aviotrix::detail::ScopedLogTarget target(&hook);
    av_log(nullptr, AV_LOG_WARNING, "part ");
    av_log(nullptr, AV_LOG_WARNING, "two\n");
    av_log(nullptr, AV_LOG_INFO, "second line\n");
  }
  av_log(nullptr, AV_LOG_ERROR, "not routed\n");
  REQUIRE(got.size() == 2);
  REQUIRE(got[0].first == aviotrix::LogLevel::Warning);
  REQUIRE(got[0].second == "part two");
  REQUIRE(got[1].second == "second line");
}

TEST_CASE("open passes libav log lines to the OpenOptions hook") {
  std::vector<std::string> lines;
  aviotrix::OpenOptions opts;
  opts.log = [&](aviotrix::LogLevel, std::string_view t) { lines.emplace_back(t); };
  // An ftyp box with nothing after it: the mov demuxer probes in, then logs "moov atom not found".
  std::vector<uint8_t> ftypOnly = {0,   0,   0, 0x14, 'f', 't', 'y', 'p', 'i', 's',
                                   'o', 'm', 0, 0,    2,   0,   'i', 's', 'o', 'm'};
  test::MemorySource zeros(std::move(ftypOnly));
  MediaReader reader;
  REQUIRE_FALSE(reader.open(zeros, opts).ok());
  REQUIRE_FALSE(lines.empty());  // probing a text file always logs at least one error/warning line
}
