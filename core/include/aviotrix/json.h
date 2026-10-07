#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aviotrix/metadata.h"

namespace aviotrix {

// Minimal JSON emitter. Only what the bindings need to hand results to TypeScript.
class JsonWriter {
 public:
  void beginObject();
  void endObject();
  void beginArray();
  void endArray();
  void key(std::string_view name);
  void value(std::string_view s);
  void value(const char* s) { value(std::string_view(s)); }
  void value(int64_t n);
  void value(int n) { value(static_cast<int64_t>(n)); }
  void value(double d);  // non-finite -> null
  void value(bool b);
  void null();
  std::string str() const { return out_; }

 private:
  void separator();
  void escape(std::string_view s);
  std::string out_;
  std::vector<bool> needComma_;  // one entry per open container
  bool afterKey_ = false;        // the next value follows a key: no comma
};

std::string toJson(const Metadata& metadata);

}  // namespace aviotrix
