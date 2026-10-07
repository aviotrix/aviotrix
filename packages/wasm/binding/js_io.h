#pragma once

#include <string>

#include "aviotrix/io.h"

namespace aviotrix_wasm {

std::string takeHostError();  // reads and clears Module.aviotrixLastHostError

class JsIoSource final : public aviotrix::IoSource {
 public:
  explicit JsIoSource(int hostId) : hostId_(hostId) {}
  aviotrix::Status open(std::optional<int64_t>& size) override;
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override;
  aviotrix::Status close() override;

 private:
  int hostId_;
};

class JsIoSink final : public aviotrix::IoSink {
 public:
  JsIoSink(int hostId, bool seekable) : hostId_(hostId), seekable_(seekable) {}
  bool seekable() const override { return seekable_; }
  aviotrix::Status open() override;
  aviotrix::Status write(int64_t offset, std::span<const uint8_t> data) override;
  aviotrix::Status close() override;

 private:
  int hostId_;
  bool seekable_;
};

}  // namespace aviotrix_wasm
