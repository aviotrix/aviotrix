#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "aviotrix/status.h"

namespace aviotrix {

// Positioned, synchronous read access. The core tracks the stream position; implementations never do.
class IoSource {
 public:
  virtual ~IoSource() = default;
  // `size` is the total length in bytes, or std::nullopt when unknown (the input becomes unseekable).
  virtual Status open(std::optional<int64_t>& size) = 0;
  // Reads up to buffer.size() bytes at `offset`. Short reads are allowed. bytesRead == 0 means EOF.
  virtual Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) = 0;
  virtual Status close() = 0;
};

// Positioned, synchronous write access.
class IoSink {
 public:
  virtual ~IoSink() = default;
  virtual bool seekable() const = 0;
  virtual Status open() = 0;
  virtual Status write(int64_t offset, std::span<const uint8_t> data) = 0;
  virtual Status close() = 0;
};

}  // namespace aviotrix
