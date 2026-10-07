#include "avio_output.h"

#include <algorithm>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace aviotrix::detail {

namespace {
constexpr int kBufferSize = 64 * 1024;
}

Status AvioOutput::create(IoSink& sink, std::unique_ptr<AvioOutput>& out) {
  std::unique_ptr<AvioOutput> o(new AvioOutput());
  o->sink_ = &sink;
  Status st = sink.open();
  if (!st.ok()) return st;
  o->sinkOpen_ = true;

  uint8_t* buffer = static_cast<uint8_t*>(av_malloc(kBufferSize));
  if (!buffer) return Status::FromAv(AVERROR(ENOMEM), "av_malloc");
  o->ctx_ = avio_alloc_context(buffer, kBufferSize, 1, o.get(), nullptr, &AvioOutput::writePacket,
                               sink.seekable() ? &AvioOutput::seek : nullptr);
  if (!o->ctx_) {
    av_free(buffer);
    return Status::FromAv(AVERROR(ENOMEM), "avio_alloc_context");
  }
  if (!sink.seekable()) o->ctx_->seekable = 0;
  out = std::move(o);
  return Status::Ok();
}

AvioOutput::~AvioOutput() {
  (void)closeIo();
  if (ctx_) {
    // av_free on a copy instead of av_freep(&ctx_->buffer): no uint8_t** -> void* conversion.
    uint8_t* buffer = ctx_->buffer;
    ctx_->buffer = nullptr;
    av_free(buffer);
    avio_context_free(&ctx_);
  }
}

Status AvioOutput::closeIo() {
  if (!sinkOpen_) return Status::Ok();
  if (ctx_) avio_flush(ctx_);
  sinkOpen_ = false;
  Status st = sink_->close();
  if (!st.ok() && lastError_.ok()) lastError_ = st;
  return lastError_;  // a failed write or final flush wins over a clean close
}

int AvioOutput::writePacket(void* opaque, const uint8_t* buf, int bufSize) {
  auto* self = static_cast<AvioOutput*>(opaque);
  if (bufSize <= 0) return 0;
  Status st = self->sink_->write(self->position_, std::span<const uint8_t>(buf, static_cast<size_t>(bufSize)));
  if (!st.ok()) {
    self->lastError_ = st;
    return AVERROR(EIO);
  }
  self->position_ += bufSize;
  self->maxPosition_ = std::max(self->maxPosition_, self->position_);
  return bufSize;
}

int64_t AvioOutput::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<AvioOutput*>(opaque);
  const int mode = whence & ~AVSEEK_FORCE;
  int64_t target = 0;
  switch (mode) {
    case AVSEEK_SIZE:
      return self->maxPosition_;
    case SEEK_SET:
      target = offset;
      break;
    case SEEK_CUR:
      target = self->position_ + offset;
      break;
    case SEEK_END:
      target = self->maxPosition_ + offset;
      break;
    default:
      return AVERROR(EINVAL);
  }
  if (target < 0) return AVERROR(EINVAL);
  self->position_ = target;
  return target;
}

}  // namespace aviotrix::detail
