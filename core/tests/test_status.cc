#include <catch2/catch_test_macros.hpp>

#include "aviotrix/log.h"
#include "aviotrix/status.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/log.h>
}

using aviotrix::ErrorCode;
using aviotrix::Status;

TEST_CASE("Status::Ok is ok and named OK") {
  Status s = Status::Ok();
  REQUIRE(s.ok());
  REQUIRE(aviotrix::errorCodeName(s.code) == "OK");
}

TEST_CASE("Status::FromAv names tagged AVERRORs and keeps context") {
  Status s = Status::FromAv(AVERROR_INVALIDDATA, "avformat_open_input");
  REQUIRE_FALSE(s.ok());
  REQUIRE(s.code == AVERROR_INVALIDDATA);
  REQUIRE(aviotrix::errorCodeName(s.code) == "AVERROR_INVALIDDATA");
  REQUIRE(s.message.find("avformat_open_input") != std::string::npos);
  REQUIRE(aviotrix::errorCodeName(AVERROR_EOF) == "AVERROR_EOF");
  REQUIRE(aviotrix::errorCodeName(AVERROR_EXIT) == "AVERROR_EXIT");
}

TEST_CASE("Status::FromAv names POSIX AVERRORs") {
  REQUIRE(aviotrix::errorCodeName(AVERROR(ENOMEM)) == "AVERROR(ENOMEM)");
  REQUIRE(aviotrix::errorCodeName(AVERROR(EINVAL)) == "AVERROR(EINVAL)");
  REQUIRE(aviotrix::errorCodeName(AVERROR(12345)) == "AVERROR(12345)");
}

TEST_CASE("Status::Error names aviotrix codes") {
  Status s = Status::Error(ErrorCode::SinkNotSeekable, "mp4 needs a seekable sink");
  REQUIRE(s.code == static_cast<int>(ErrorCode::SinkNotSeekable));
  REQUIRE(aviotrix::errorCodeName(s.code) == "SINK_NOT_SEEKABLE");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::IncompatibleStream)) == "INCOMPATIBLE_STREAM");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::Aborted)) == "ABORTED");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::InvalidArgument)) == "INVALID_ARGUMENT");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::IoFailed)) == "IO_FAILED");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::NotOpen)) == "NOT_OPEN");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::Unsupported)) == "UNSUPPORTED");
}

TEST_CASE("LogLevel maps from av levels") {
  REQUIRE(aviotrix::logLevelFromAv(AV_LOG_ERROR) == aviotrix::LogLevel::Error);
  REQUIRE(aviotrix::logLevelFromAv(AV_LOG_INFO) == aviotrix::LogLevel::Info);
  REQUIRE(aviotrix::logLevelFromAv(AV_LOG_TRACE) == aviotrix::LogLevel::Trace);
  REQUIRE(std::string(aviotrix::logLevelName(aviotrix::LogLevel::Warning)) == "warning");
}
