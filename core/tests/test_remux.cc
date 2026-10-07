#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <string>
#include <vector>

#include "aviotrix/json.h"
#include "aviotrix/media_reader.h"
#include "aviotrix/remux.h"
#include "file_io.h"
#include "fixtures.h"

using aviotrix::ErrorCode;
using aviotrix::MediaReader;
using aviotrix::RemuxOptions;
using aviotrix::RemuxResult;
using aviotrix::Status;

namespace {

aviotrix::Metadata reopen(const std::vector<uint8_t>& bytes) {
  test::MemorySource src(bytes);
  MediaReader r;
  REQUIRE(r.open(src).ok());
  return r.metadata();
}

bool contains(const std::vector<uint8_t>& hay, const char* needle) {
  const std::string s(hay.begin(), hay.end());
  return s.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("remux mp4 to matroska and reopen") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "matroska";
  RemuxResult result;
  REQUIRE(reader.remux(sink, opt, result).ok());
  REQUIRE(result.streams.size() == 2);
  REQUIRE(result.streams[0].output == 0);
  REQUIRE(result.streams[1].output == 1);
  REQUIRE(result.packets > 100);
  REQUIRE(result.bytesWritten == static_cast<int64_t>(sink.bytes().size()));
  REQUIRE(result.bytesRead > 0);
  REQUIRE(sink.closed());

  auto m = reopen(sink.bytes());
  REQUIRE(m.format == "matroska,webm");
  REQUIRE(m.streams.size() == 2);
  REQUIRE(m.streams[0].codec == "h264");
  REQUIRE(m.streams[1].codec == "aac");
  REQUIRE(m.duration.has_value());
  REQUIRE(std::abs(*m.duration - *reader.metadata().duration) < 0.2);
}

TEST_CASE("remux mpegts to mp4 (Annex B -> avcC via auto bsf) and reopen") {
  test::FileSource src(test::fixturePath("h264-ac3.ts"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "mp4";
  RemuxResult result;
  REQUIRE(reader.remux(sink, opt, result).ok());
  REQUIRE(std::string(sink.bytes().begin() + 4, sink.bytes().begin() + 8) == "ftyp");
  auto m = reopen(sink.bytes());
  REQUIRE(m.format == "mov,mp4,m4a,3gp,3g2,mj2");
  REQUIRE(m.streams[0].codec == "h264");
  REQUIRE(m.streams[0].video->width == 320);
  REQUIRE(m.streams[1].codec == "ac3");
}

TEST_CASE("remux mp4 to mpegts and webm to webm") {
  {
    test::FileSource src(test::fixturePath("h264-aac.mp4"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "mpegts";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    auto m = reopen(sink.bytes());
    REQUIRE(m.format == "mpegts");
    REQUIRE(m.streams[0].codec == "h264");
    REQUIRE(m.streams[1].codec == "aac");
  }
  {
    test::FileSource src(test::fixturePath("vp9-opus.webm"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "webm";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    auto m = reopen(sink.bytes());
    REQUIRE(m.streams[0].codec == "vp9");
    REQUIRE(m.streams[1].codec == "opus");
  }
}

TEST_CASE("incompatible subtitle stream is skipped by default with a warning, or fails on request") {
  test::FileSource src(test::fixturePath("h264-aac-srt.mkv"));
  std::vector<std::string> warnings;
  aviotrix::OpenOptions oo;
  oo.log = [&](aviotrix::LogLevel lvl, std::string_view t) {
    if (lvl == aviotrix::LogLevel::Warning) warnings.emplace_back(t);
  };
  MediaReader reader;
  REQUIRE(reader.open(src, oo).ok());

  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "mp4";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    REQUIRE(result.streams.size() == 3);
    REQUIRE_FALSE(result.streams[2].output.has_value());
    REQUIRE(result.streams[2].skippedReason.find("subrip") != std::string::npos);
    REQUIRE(reopen(sink.bytes()).streams.size() == 2);
    bool warned = false;
    for (const auto& w : warnings) warned = warned || w.find("subrip") != std::string::npos;
    REQUIRE(warned);
  }
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "mp4";
    opt.failOnIncompatible = true;
    RemuxResult result;
    Status st = reader.remux(sink, opt, result);
    REQUIRE(st.code == static_cast<int>(ErrorCode::IncompatibleStream));
    REQUIRE_FALSE(sink.opened());
  }
}

TEST_CASE("all streams incompatible is an error even in skip mode") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "webm";  // webm accepts only vp8/vp9/av1/vorbis/opus
  RemuxResult result;
  Status st = reader.remux(sink, opt, result);
  REQUIRE(st.code == static_cast<int>(ErrorCode::IncompatibleStream));
  REQUIRE_FALSE(sink.opened());
}

TEST_CASE("mp4 to a streaming sink requires fragmented") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  {
    test::MemorySink sink(false);
    RemuxOptions opt;
    opt.format = "mp4";
    RemuxResult result;
    Status st = reader.remux(sink, opt, result);
    REQUIRE(st.code == static_cast<int>(ErrorCode::SinkNotSeekable));
    REQUIRE_FALSE(sink.opened());
  }
  {
    test::MemorySink sink(false);
    RemuxOptions opt;
    opt.format = "mp4";
    opt.fragmented = true;
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    REQUIRE(contains(sink.bytes(), "moof"));
    auto m = reopen(sink.bytes());
    REQUIRE(m.streams.size() == 2);
  }
}

TEST_CASE("stream selection and invalid indices") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "matroska";
    opt.streams = std::vector<int>{1};
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    REQUIRE(result.streams.size() == 1);
    REQUIRE(result.streams[0].input == 1);
    REQUIRE(result.streams[0].output == 0);
    auto m = reopen(sink.bytes());
    REQUIRE(m.streams.size() == 1);
    REQUIRE(m.streams[0].codec == "aac");
  }
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "matroska";
    opt.streams = std::vector<int>{5};
    RemuxResult result;
    Status st = reader.remux(sink, opt, result);
    REQUIRE(st.code == static_cast<int>(ErrorCode::InvalidArgument));
    REQUIRE_FALSE(sink.opened());
  }
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "avi";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).code == static_cast<int>(ErrorCode::InvalidArgument));
  }
}

TEST_CASE("cancel flag aborts and still closes the sink") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  std::atomic<bool> cancel{false};
  RemuxOptions opt;
  opt.format = "matroska";
  opt.cancel = &cancel;
  opt.progressIntervalPackets = 10;
  int progressCalls = 0;
  opt.onProgress = [&](const aviotrix::RemuxProgress& p) {
    progressCalls++;
    REQUIRE(p.bytesRead >= 0);
    cancel.store(true);
  };
  RemuxResult result;
  Status st = reader.remux(sink, opt, result);
  REQUIRE(st.code == static_cast<int>(ErrorCode::Aborted));
  REQUIRE(progressCalls >= 1);
  REQUIRE(sink.closed());
  REQUIRE(result.packets < 100);
}

TEST_CASE("a second remux on the same reader rewinds the input") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink a(true), b(true);
  RemuxOptions opt;
  opt.format = "matroska";
  RemuxResult ra, rb;
  REQUIRE(reader.remux(a, opt, ra).ok());
  REQUIRE(reader.remux(b, opt, rb).ok());
  REQUIRE(ra.packets == rb.packets);
  REQUIRE(a.bytes().size() == b.bytes().size());
}

TEST_CASE("sink write failure propagates the sink's message and closes the sink") {
  struct FailingSink final : aviotrix::IoSink {
    int64_t budget = 100000;
    bool closed = false;
    bool seekable() const override { return true; }
    Status open() override { return Status::Ok(); }
    Status write(int64_t, std::span<const uint8_t> data) override {
      budget -= static_cast<int64_t>(data.size());
      if (budget < 0) return Status::Error(ErrorCode::IoFailed, "quota exceeded");
      return Status::Ok();
    }
    Status close() override {
      closed = true;
      return Status::Ok();
    }
  } sink;
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  RemuxOptions opt;
  opt.format = "matroska";
  RemuxResult result;
  Status st = reader.remux(sink, opt, result);
  REQUIRE_FALSE(st.ok());
  REQUIRE(st.message == "quota exceeded");
  REQUIRE(sink.closed);
}

TEST_CASE("remux on a closed reader is NOT_OPEN") {
  MediaReader reader;
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "mp4";
  RemuxResult result;
  REQUIRE(reader.remux(sink, opt, result).code == static_cast<int>(ErrorCode::NotOpen));
}

TEST_CASE("toJson(RemuxResult)") {
  RemuxResult r;
  r.streams.push_back({0, 0, ""});
  r.streams.push_back({2, std::nullopt, "codec subrip is not supported by muxer mp4"});
  r.bytesRead = 10;
  r.bytesWritten = 20;
  r.packets = 3;
  REQUIRE(
      aviotrix::toJson(r) ==
      R"({"streams":[{"input":0,"output":0},{"input":2,"output":null,"skippedReason":"codec subrip is not supported by muxer mp4"}],"bytesRead":10,"bytesWritten":20,"packets":3})");
}
