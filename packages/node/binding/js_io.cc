#include "js_io.h"

namespace aviotrix_node {

aviotrix::Status JsIoSource::open(std::optional<int64_t>& size) {
  IoRequest req{IoRequest::Kind::SourceOpen};
  aviotrix::Status st = bridge_.request(req);
  if (st.ok()) size = req.size;
  return st;
}

aviotrix::Status JsIoSource::read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) {
  IoRequest req{IoRequest::Kind::SourceRead};
  req.offset = offset;
  req.readInto = buffer;
  aviotrix::Status st = bridge_.request(req);
  bytesRead = st.ok() ? req.bytesRead : 0;
  return st;
}

aviotrix::Status JsIoSource::close() {
  IoRequest req{IoRequest::Kind::SourceClose};
  return bridge_.request(req);
}

aviotrix::Status JsIoSink::open() {
  IoRequest req{IoRequest::Kind::SinkOpen};
  return bridge_.request(req);
}

aviotrix::Status JsIoSink::write(int64_t offset, std::span<const uint8_t> data) {
  IoRequest req{IoRequest::Kind::SinkWrite};
  req.offset = offset;
  req.writeFrom = data;
  return bridge_.request(req);
}

aviotrix::Status JsIoSink::close() {
  IoRequest req{IoRequest::Kind::SinkClose};
  return bridge_.request(req);
}

}  // namespace aviotrix_node
