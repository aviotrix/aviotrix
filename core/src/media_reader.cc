#include "aviotrix/media_reader.h"

#include "log_router.h"
#include "media_reader_impl.h"
#include "metadata_internal.h"

namespace aviotrix {

MediaReader::MediaReader() : impl_(std::make_unique<Impl>()) {}
MediaReader::~MediaReader() {
  impl_->release();
}

bool MediaReader::isOpen() const {
  return impl_->open;
}
const Metadata& MediaReader::metadata() const {
  return impl_->metadata;
}

Status MediaReader::open(IoSource& source, OpenOptions options) {
  if (impl_->open) return Status::Error(ErrorCode::InvalidArgument, "reader is already open");
  impl_->log = std::move(options.log);
  detail::installAvLogCallback();
  detail::ScopedLogTarget logTarget(&impl_->log);

  Status st = detail::AvioInput::create(source, impl_->input);
  if (!st.ok()) {
    impl_->release();
    return st;
  }

  impl_->fmt = avformat_alloc_context();
  if (!impl_->fmt) {
    impl_->release();
    return Status::FromAv(AVERROR(ENOMEM), "avformat_alloc_context");
  }
  impl_->fmt->pb = impl_->input->context();
  impl_->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

  int ret = avformat_open_input(&impl_->fmt, nullptr, nullptr, nullptr);
  if (ret < 0) {
    Status ioErr = impl_->input->lastError();
    impl_->release();
    return ioErr.ok() ? Status::FromAv(ret, "avformat_open_input") : ioErr;
  }
  ret = avformat_find_stream_info(impl_->fmt, nullptr);
  if (ret < 0) {
    Status ioErr = impl_->input->lastError();
    impl_->release();
    return ioErr.ok() ? Status::FromAv(ret, "avformat_find_stream_info") : ioErr;
  }
  impl_->metadata = detail::readMetadata(impl_->fmt);
  impl_->open = true;
  return Status::Ok();
}

Status MediaReader::close() {
  if (!impl_->open && !impl_->input) return Status::Ok();
  detail::ScopedLogTarget logTarget(&impl_->log);
  Status st = impl_->input ? impl_->input->closeIo() : Status::Ok();
  impl_->release();
  return st;
}

}  // namespace aviotrix
