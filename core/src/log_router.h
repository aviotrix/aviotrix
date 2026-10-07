#pragma once

#include "aviotrix/log.h"

namespace aviotrix::detail {

// Installs the process-wide av_log callback once. Safe to call repeatedly.
void installAvLogCallback();

// While alive on this thread, libav log lines are delivered to *hook (if non-null and non-empty).
// Nesting restores the previous target. Operations never interleave on one thread, so this is
// enough to attribute lines to the right reader on both Node (one thread per reader) and WASM
// (module-wide serialization in the TS wrapper).
class ScopedLogTarget {
 public:
  explicit ScopedLogTarget(const LogHook* hook);
  ~ScopedLogTarget();
  ScopedLogTarget(const ScopedLogTarget&) = delete;
  ScopedLogTarget& operator=(const ScopedLogTarget&) = delete;

 private:
  const LogHook* previous_;
};

}  // namespace aviotrix::detail
