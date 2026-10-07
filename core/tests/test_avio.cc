#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <numeric>

#include "../src/avio_input.h"
#include "../src/avio_output.h"
#include "file_io.h"

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
}

using aviotrix::detail::AvioInput;
using aviotrix::detail::AvioOutput;

namespace {
std::vector<uint8_t> ramp(size_t n) {
  std::vector<uint8_t> v(n);
  std::iota(v.begin(), v.end(), 0);
  return v;
}
}  // namespace

TEST_CASE("AvioInput reads sequentially and tracks position") {
  test::MemorySource src(ramp(1000));
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(src, in).ok());
  REQUIRE(in->seekable());
  REQUIRE(avio_size(in->context()) == 1000);

  uint8_t buf[300];
  REQUIRE(avio_read(in->context(), buf, 300) == 300);
  REQUIRE(buf[0] == 0);
  REQUIRE(buf[299] == 299 % 256);
  REQUIRE(avio_read(in->context(), buf, 300) == 300);
  REQUIRE(buf[0] == 300 % 256);
  REQUIRE(avio_tell(in->context()) == 600);
}

TEST_CASE("AvioInput seeks with SEEK_SET/CUR/END and reports EOF") {
  test::MemorySource src(ramp(1000));
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(src, in).ok());
  REQUIRE(avio_seek(in->context(), 990, SEEK_SET) == 990);
  uint8_t buf[64];
  REQUIRE(avio_read(in->context(), buf, 64) == 10);
  REQUIRE(buf[0] == 990 % 256);
  REQUIRE(avio_read(in->context(), buf, 64) == AVERROR_EOF);
  REQUIRE(avio_seek(in->context(), avio_size(in->context()) - 100, SEEK_SET) == 900);
  REQUIRE(avio_seek(in->context(), -1, SEEK_SET) < 0);
}

TEST_CASE("AvioInput without a size is unseekable") {
  struct Unsized final : aviotrix::IoSource {
    test::MemorySource inner{ramp(100)};
    aviotrix::Status open(std::optional<int64_t>& size) override {
      size.reset();
      return aviotrix::Status::Ok();
    }
    aviotrix::Status read(int64_t o, std::span<uint8_t> b, size_t& n) override { return inner.read(o, b, n); }
    aviotrix::Status close() override { return aviotrix::Status::Ok(); }
  } src;
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(src, in).ok());
  REQUIRE_FALSE(in->seekable());
  REQUIRE(in->context()->seekable == 0);
  REQUIRE(avio_size(in->context()) < 0);
  uint8_t buf[100];
  REQUIRE(avio_read(in->context(), buf, 100) == 100);
}

TEST_CASE("AvioInput surfaces source errors and honors the cancel flag") {
  struct Failing final : aviotrix::IoSource {
    aviotrix::Status open(std::optional<int64_t>& size) override {
      size = 10;
      return aviotrix::Status::Ok();
    }
    aviotrix::Status read(int64_t, std::span<uint8_t>, size_t&) override {
      return aviotrix::Status::Error(aviotrix::ErrorCode::IoFailed, "disk on fire");
    }
    aviotrix::Status close() override { return aviotrix::Status::Ok(); }
  } failing;
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(failing, in).ok());
  uint8_t buf[10];
  REQUIRE(avio_read(in->context(), buf, 10) < 0);
  REQUIRE(in->lastError().message == "disk on fire");

  test::MemorySource src(ramp(100));
  std::unique_ptr<AvioInput> in2;
  REQUIRE(AvioInput::create(src, in2).ok());
  std::atomic<bool> cancel{true};
  in2->setCancelFlag(&cancel);
  REQUIRE(avio_read(in2->context(), buf, 10) == AVERROR_EXIT);
}

TEST_CASE("AvioOutput writes at tracked positions and seeks back when seekable") {
  test::MemorySink sink(true);
  std::unique_ptr<AvioOutput> out;
  REQUIRE(AvioOutput::create(sink, out).ok());
  const uint8_t a[4] = {1, 2, 3, 4};
  const uint8_t b[2] = {9, 9};
  avio_write(out->context(), a, 4);
  avio_flush(out->context());
  REQUIRE(avio_seek(out->context(), 1, SEEK_SET) == 1);
  avio_write(out->context(), b, 2);
  avio_flush(out->context());
  REQUIRE(out->closeIo().ok());
  REQUIRE(sink.bytes() == std::vector<uint8_t>{1, 9, 9, 4});
  REQUIRE(out->bytesWritten() == 4);
}

TEST_CASE("AvioOutput on a streaming sink has no seek and still writes") {
  test::MemorySink sink(false);
  std::unique_ptr<AvioOutput> out;
  REQUIRE(AvioOutput::create(sink, out).ok());
  REQUIRE(out->context()->seekable == 0);
  const uint8_t a[3] = {7, 8, 9};
  avio_write(out->context(), a, 3);
  avio_flush(out->context());
  REQUIRE(avio_seek(out->context(), 0, SEEK_SET) < 0);
  REQUIRE(out->closeIo().ok());
  REQUIRE(sink.bytes() == std::vector<uint8_t>{7, 8, 9});
}
