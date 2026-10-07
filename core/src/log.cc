#include "aviotrix/log.h"

extern "C" {
#include <libavutil/log.h>
}

namespace aviotrix {

const char* logLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Quiet:
      return "quiet";
    case LogLevel::Panic:
      return "panic";
    case LogLevel::Fatal:
      return "fatal";
    case LogLevel::Error:
      return "error";
    case LogLevel::Warning:
      return "warning";
    case LogLevel::Info:
      return "info";
    case LogLevel::Verbose:
      return "verbose";
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Trace:
      return "trace";
  }
  return "info";
}

LogLevel logLevelFromAv(int avLevel) {
  if (avLevel <= AV_LOG_QUIET) return LogLevel::Quiet;
  if (avLevel <= AV_LOG_PANIC) return LogLevel::Panic;
  if (avLevel <= AV_LOG_FATAL) return LogLevel::Fatal;
  if (avLevel <= AV_LOG_ERROR) return LogLevel::Error;
  if (avLevel <= AV_LOG_WARNING) return LogLevel::Warning;
  if (avLevel <= AV_LOG_INFO) return LogLevel::Info;
  if (avLevel <= AV_LOG_VERBOSE) return LogLevel::Verbose;
  if (avLevel <= AV_LOG_DEBUG) return LogLevel::Debug;
  return LogLevel::Trace;
}

}  // namespace aviotrix
