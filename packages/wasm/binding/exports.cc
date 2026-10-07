#include <emscripten.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "aviotrix/json.h"
#include "aviotrix/media_reader.h"
#include "aviotrix/remux.h"
#include "handles.h"
#include "js_imports.h"
#include "js_io.h"

namespace {

struct Reader {
  explicit Reader(int host) : hostId(host), source(host) {}
  int hostId;
  aviotrix_wasm::JsIoSource source;
  aviotrix::MediaReader reader;
  std::atomic<bool> cancel{false};
  std::string json;     // last metadata / result payload
  std::string errCode;  // last error
  std::string errMessage;

  int fail(const aviotrix::Status& st) {
    errCode = aviotrix::errorCodeName(st.code);
    errMessage = st.message;
    return st.code == 0 ? 1 : st.code;
  }
  void clearError() {
    errCode.clear();
    errMessage.clear();
  }
};

HandleTable<Reader>& readers() {
  static HandleTable<Reader> table;
  return table;
}

const char* kEmpty = "";

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int avx_reader_new(int hostId) {
  return readers().insert(std::make_unique<Reader>(hostId));
}

EMSCRIPTEN_KEEPALIVE int avx_reader_open(int id) {
  Reader* r = readers().get(id);
  if (!r) return -1;
  r->clearError();
  aviotrix::OpenOptions opts;
  opts.log = [r](aviotrix::LogLevel level, std::string_view text) {
    std::string t(text);
    avx_js_log(r->hostId, aviotrix::logLevelName(level), t.c_str());
  };
  aviotrix::Status st = r->reader.open(r->source, std::move(opts));
  if (!st.ok()) return r->fail(st);
  r->json = aviotrix::toJson(r->reader.metadata());
  return 0;
}

EMSCRIPTEN_KEEPALIVE const char* avx_reader_metadata(int id) {
  Reader* r = readers().get(id);
  return r ? r->json.c_str() : kEmpty;
}

EMSCRIPTEN_KEEPALIVE int avx_reader_remux(int id, const char* format, const int* streams, int streamCount,
                                          int failOnIncompatible, int fragmented, int sinkSeekable,
                                          int progressInterval) {
  Reader* r = readers().get(id);
  if (!r) return -1;
  r->clearError();
  aviotrix::RemuxOptions opt;
  opt.format = format ? format : "";
  if (streams && streamCount >= 0) opt.streams = std::vector<int>(streams, streams + streamCount);
  opt.failOnIncompatible = failOnIncompatible != 0;
  opt.fragmented = fragmented != 0;
  opt.cancel = &r->cancel;
  if (progressInterval > 0) opt.progressIntervalPackets = progressInterval;
  opt.onProgress = [r](const aviotrix::RemuxProgress& p) {
    avx_js_progress(r->hostId, static_cast<double>(p.bytesRead), static_cast<double>(p.bytesWritten),
                    p.timestamp.value_or(0.0), p.timestamp.has_value() ? 1 : 0);
  };
  aviotrix_wasm::JsIoSink sink(r->hostId, sinkSeekable != 0);
  aviotrix::RemuxResult result;
  aviotrix::Status st = r->reader.remux(sink, opt, result);
  r->cancel.store(false);
  if (!st.ok()) return r->fail(st);
  r->json = aviotrix::toJson(result);
  return 0;
}

EMSCRIPTEN_KEEPALIVE const char* avx_reader_result(int id) {
  Reader* r = readers().get(id);
  return r ? r->json.c_str() : kEmpty;
}

EMSCRIPTEN_KEEPALIVE void avx_reader_cancel(int id) {
  if (Reader* r = readers().get(id)) r->cancel.store(true);
}

EMSCRIPTEN_KEEPALIVE int avx_reader_close(int id) {
  Reader* r = readers().get(id);
  if (!r) return -1;
  r->clearError();
  aviotrix::Status st = r->reader.close();
  return st.ok() ? 0 : r->fail(st);
}

EMSCRIPTEN_KEEPALIVE void avx_reader_free(int id) {
  readers().erase(id);
}

EMSCRIPTEN_KEEPALIVE const char* avx_last_error_code(int id) {
  Reader* r = readers().get(id);
  return r ? r->errCode.c_str() : kEmpty;
}

EMSCRIPTEN_KEEPALIVE const char* avx_last_error_message(int id) {
  Reader* r = readers().get(id);
  return r ? r->errMessage.c_str() : kEmpty;
}

}  // extern "C"
