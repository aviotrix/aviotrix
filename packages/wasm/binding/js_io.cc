#include "js_io.h"

#include <vector>

#include "js_imports.h"

namespace aviotrix_wasm {

using aviotrix::ErrorCode;
using aviotrix::Status;

std::string takeHostError() {
  std::vector<char> buf(16 * 1024);  // long messages are truncated; the JS side is cleared on the first read
  avx_js_copy_host_error(buf.data(), static_cast<int>(buf.size()));
  std::string message(buf.data());
  return message.empty() ? "host callback failed" : message;
}

Status JsIoSource::open(std::optional<int64_t>& size) {
  const double r = avx_js_source_open(hostId_);
  if (r <= -2) return Status::Error(ErrorCode::IoFailed, takeHostError());
  if (r < 0)
    size.reset();
  else
    size = static_cast<int64_t>(r);
  return Status::Ok();
}

Status JsIoSource::read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) {
  const int n =
      avx_js_source_read(hostId_, static_cast<double>(offset), static_cast<int>(buffer.size()), buffer.data());
  if (n < 0) {
    bytesRead = 0;
    return Status::Error(ErrorCode::IoFailed, takeHostError());
  }
  bytesRead = static_cast<size_t>(n);
  return Status::Ok();
}

Status JsIoSource::close() {
  return avx_js_source_close(hostId_) < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

Status JsIoSink::open() {
  return avx_js_sink_open(hostId_) < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

Status JsIoSink::write(int64_t offset, std::span<const uint8_t> data) {
  const int r = avx_js_sink_write(hostId_, static_cast<double>(offset), data.data(), static_cast<int>(data.size()));
  return r < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

Status JsIoSink::close() {
  return avx_js_sink_close(hostId_) < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

}  // namespace aviotrix_wasm
