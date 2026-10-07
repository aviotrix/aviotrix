#pragma once

#include "aviotrix/io.h"
#include "main_thread_bridge.h"

namespace aviotrix_node {

class JsIoSource final : public aviotrix::IoSource {
 public:
  explicit JsIoSource(MainThreadBridge& bridge) : bridge_(bridge) {}
  aviotrix::Status open(std::optional<int64_t>& size) override;
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override;
  aviotrix::Status close() override;

 private:
  MainThreadBridge& bridge_;
};

class JsIoSink final : public aviotrix::IoSink {
 public:
  JsIoSink(MainThreadBridge& bridge, bool seekable) : bridge_(bridge), seekable_(seekable) {}
  bool seekable() const override { return seekable_; }
  aviotrix::Status open() override;
  aviotrix::Status write(int64_t offset, std::span<const uint8_t> data) override;
  aviotrix::Status close() override;

 private:
  MainThreadBridge& bridge_;
  bool seekable_;
};

}  // namespace aviotrix_node
