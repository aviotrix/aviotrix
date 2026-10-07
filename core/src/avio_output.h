#pragma once

#include <cstdint>
#include <memory>

#include "aviotrix/io.h"
#include "aviotrix/status.h"

extern "C" {
#include <libavformat/avio.h>
}

namespace aviotrix::detail {

// Adapts libav's sequential write/seek callbacks onto a positioned IoSink.
class AvioOutput {
 public:
  static Status create(IoSink& sink, std::unique_ptr<AvioOutput>& out);
  ~AvioOutput();
  AvioOutput(const AvioOutput&) = delete;
  AvioOutput& operator=(const AvioOutput&) = delete;

  AVIOContext* context() const { return ctx_; }
  bool seekable() const { return sink_->seekable(); }
  int64_t bytesWritten() const { return maxPosition_; }
  const Status& lastError() const { return lastError_; }
  Status closeIo();  // flushes and closes the sink once; safe to call twice

 private:
  AvioOutput() = default;
  static int writePacket(void* opaque, const uint8_t* buf, int bufSize);
  static int64_t seek(void* opaque, int64_t offset, int whence);

  IoSink* sink_ = nullptr;
  int64_t position_ = 0;
  int64_t maxPosition_ = 0;
  AVIOContext* ctx_ = nullptr;
  Status lastError_;
  bool sinkOpen_ = false;
};

}  // namespace aviotrix::detail
