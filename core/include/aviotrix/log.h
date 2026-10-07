#pragma once

#include <functional>
#include <string_view>

namespace aviotrix {

enum class LogLevel { Quiet, Panic, Fatal, Error, Warning, Info, Verbose, Debug, Trace };

using LogHook = std::function<void(LogLevel level, std::string_view text)>;

const char* logLevelName(LogLevel level);  // "quiet", "panic", ... "trace"
LogLevel logLevelFromAv(int avLevel);      // AV_LOG_* -> LogLevel

}  // namespace aviotrix
