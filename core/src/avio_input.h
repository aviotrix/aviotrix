#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include "aviotrix/io.h"
#include "aviotrix/status.h"

extern "C" {
#include <libavformat/avio.h>
}

namespace aviotrix::detail {

// Adapts libav's sequential read/seek callbacks onto a positioned IoSource.
class AvioInput {
 public:
  static Status create(IoSource& source, std::unique_ptr<AvioInput>& out);
  ~AvioInput();
  AvioInput(const AvioInput&) = delete;
  AvioInput& operator=(const AvioInput&) = delete;

  AVIOContext* context() const { return ctx_; }
  bool seekable() const { return size_.has_value(); }
  int64_t bytesRead() const { return bytesRead_; }
  const Status& lastError() const { return lastError_; }
  void setCancelFlag(const std::atomic<bool>* flag) { cancel_ = flag; }
  Status closeIo();  // closes the source once; safe to call twice

 private:
  AvioInput() = default;
  static int readPacket(void* opaque, uint8_t* buf, int bufSize);
  static int64_t seek(void* opaque, int64_t offset, int whence);

  IoSource* source_ = nullptr;
  std::optional<int64_t> size_;
  int64_t position_ = 0;
  int64_t bytesRead_ = 0;
  AVIOContext* ctx_ = nullptr;
  Status lastError_;
  const std::atomic<bool>* cancel_ = nullptr;
  bool sourceOpen_ = false;
};

}  // namespace aviotrix::detail
