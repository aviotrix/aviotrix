#include "log_router.h"

#include <mutex>
#include <string>

extern "C" {
#include <libavutil/log.h>
}

namespace aviotrix::detail {

namespace {

thread_local const LogHook* tCurrentHook = nullptr;
thread_local std::string tPending;
thread_local int tPrintPrefix = 1;

void avLogCallback(void* avcl, int level, const char* fmt, va_list vl) {
  if (level > AV_LOG_VERBOSE) return;
  const LogHook* hook = tCurrentHook;
  if (!hook || !*hook) return;
  char line[1024];
  av_log_format_line2(avcl, level, fmt, vl, line, sizeof line, &tPrintPrefix);
  tPending += line;
  size_t nl;
  while ((nl = tPending.find('\n')) != std::string::npos) {
    std::string_view text(tPending.data(), nl);
    (*hook)(logLevelFromAv(level), text);
    tPending.erase(0, nl + 1);
  }
}

std::once_flag gInstalled;

}  // namespace

void installAvLogCallback() {
  std::call_once(gInstalled, [] {
    av_log_set_level(AV_LOG_VERBOSE);
    av_log_set_callback(&avLogCallback);
  });
}

ScopedLogTarget::ScopedLogTarget(const LogHook* hook) : previous_(tCurrentHook) {
  tCurrentHook = hook;
  tPending.clear();
  tPrintPrefix = 1;
}

ScopedLogTarget::~ScopedLogTarget() {
  if (!tPending.empty() && tCurrentHook && *tCurrentHook) {
    (*tCurrentHook)(LogLevel::Info, tPending);
    tPending.clear();
  }
  tCurrentHook = previous_;
}

}  // namespace aviotrix::detail
