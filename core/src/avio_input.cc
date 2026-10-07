#include "avio_input.h"

#include <algorithm>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace aviotrix::detail {

namespace {
constexpr int kBufferSize = 64 * 1024;
}

Status AvioInput::create(IoSource& source, std::unique_ptr<AvioInput>& out) {
  std::unique_ptr<AvioInput> in(new AvioInput());
  in->source_ = &source;
  Status st = source.open(in->size_);
  if (!st.ok()) return st;
  in->sourceOpen_ = true;
  if (in->size_ && *in->size_ < 0) in->size_.reset();

  uint8_t* buffer = static_cast<uint8_t*>(av_malloc(kBufferSize));
  if (!buffer) return Status::FromAv(AVERROR(ENOMEM), "av_malloc");
  in->ctx_ = avio_alloc_context(buffer, kBufferSize, 0, in.get(), &AvioInput::readPacket, nullptr,
                                in->seekable() ? &AvioInput::seek : nullptr);
  if (!in->ctx_) {
    av_free(buffer);
    return Status::FromAv(AVERROR(ENOMEM), "avio_alloc_context");
  }
  if (!in->seekable()) in->ctx_->seekable = 0;
  out = std::move(in);
  return Status::Ok();
}

AvioInput::~AvioInput() {
  (void)closeIo();
  if (ctx_) {
    av_freep(static_cast<void*>(&ctx_->buffer));
    avio_context_free(&ctx_);
  }
}

Status AvioInput::closeIo() {
  if (!sourceOpen_) return Status::Ok();
  sourceOpen_ = false;
  return source_->close();
}

int AvioInput::readPacket(void* opaque, uint8_t* buf, int bufSize) {
  auto* self = static_cast<AvioInput*>(opaque);
  if (self->cancel_ && self->cancel_->load(std::memory_order_relaxed)) return AVERROR_EXIT;
  if (bufSize <= 0) return 0;
  if (self->size_ && self->position_ >= *self->size_) return AVERROR_EOF;
  size_t want = static_cast<size_t>(bufSize);
  if (self->size_)
    want = static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(want), *self->size_ - self->position_));
  size_t got = 0;
  Status st = self->source_->read(self->position_, std::span<uint8_t>(buf, want), got);
  if (!st.ok()) {
    self->lastError_ = st;
    return AVERROR(EIO);
  }
  got = std::min(got, want);
  if (got == 0) return AVERROR_EOF;
  self->position_ += static_cast<int64_t>(got);
  self->bytesRead_ += static_cast<int64_t>(got);
  return static_cast<int>(got);
}

int64_t AvioInput::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<AvioInput*>(opaque);
  if (!self->size_) return AVERROR(ENOSYS);
  const int mode = whence & ~AVSEEK_FORCE;
  int64_t target = 0;
  switch (mode) {
    case AVSEEK_SIZE:
      return *self->size_;
    case SEEK_SET:
      target = offset;
      break;
    case SEEK_CUR:
      target = self->position_ + offset;
      break;
    case SEEK_END:
      target = *self->size_ + offset;
      break;
    default:
      return AVERROR(EINVAL);
  }
  if (target < 0) return AVERROR(EINVAL);
  self->position_ = target;
  return target;
}

}  // namespace aviotrix::detail
