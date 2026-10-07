#pragma once

#include <string>
#include <string_view>

namespace aviotrix {

// Positive codes are aviotrix's own. Negative codes are libav AVERROR values. 0 is success.
enum class ErrorCode : int {
  Ok = 0,
  SinkNotSeekable = 1,
  IncompatibleStream = 2,
  Aborted = 3,
  InvalidArgument = 4,
  IoFailed = 5,
  NotOpen = 6,
  Unsupported = 7,
};

struct Status {
  int code = 0;
  std::string message;

  bool ok() const { return code == 0; }
  static Status Ok() { return Status{}; }
  // Wraps a libav return value. `context` names the call that failed.
  static Status FromAv(int averror, std::string_view context);
  static Status Error(ErrorCode code, std::string message);
};

// Stable, machine-readable name for any code, e.g. "AVERROR_INVALIDDATA", "AVERROR(ENOMEM)", "SINK_NOT_SEEKABLE".
std::string errorCodeName(int code);

}  // namespace aviotrix
