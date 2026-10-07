#include <napi.h>

#include "native_reader.h"

namespace {

Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
  exports.Set("NativeReader", aviotrix_node::NativeReader::Init(env));
  return exports;
}

}  // namespace

NODE_API_MODULE(aviotrix_node, InitModule)
