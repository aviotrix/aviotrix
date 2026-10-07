#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "aviotrix/io.h"

namespace test {

class FileSource final : public aviotrix::IoSource {
 public:
  explicit FileSource(std::string path) : path_(std::move(path)) {}
  ~FileSource() override {
    if (file_) std::fclose(file_);
  }

  aviotrix::Status open(std::optional<int64_t>& size) override {
    file_ = std::fopen(path_.c_str(), "rb");
    if (!file_) return aviotrix::Status::Error(aviotrix::ErrorCode::IoFailed, "cannot open " + path_);
    std::fseek(file_, 0, SEEK_END);
    size = static_cast<int64_t>(std::ftell(file_));
    opens_++;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override {
    if (!file_) return aviotrix::Status::Error(aviotrix::ErrorCode::NotOpen, "read before open");
    std::fseek(file_, static_cast<long>(offset), SEEK_SET);
    bytesRead = std::fread(buffer.data(), 1, buffer.size(), file_);
    reads_++;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status close() override {
    if (file_) {
      std::fclose(file_);
      file_ = nullptr;
    }
    closes_++;
    return aviotrix::Status::Ok();
  }
  int opens() const { return opens_; }
  int reads() const { return reads_; }
  int closes() const { return closes_; }

 private:
  std::string path_;
  std::FILE* file_ = nullptr;
  int opens_ = 0, reads_ = 0, closes_ = 0;
};

// In-memory sink. `seekable=false` simulates a streaming destination: writes must be sequential.
class MemorySink final : public aviotrix::IoSink {
 public:
  explicit MemorySink(bool seekable = true) : seekable_(seekable) {}
  bool seekable() const override { return seekable_; }
  aviotrix::Status open() override {
    opened_ = true;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status write(int64_t offset, std::span<const uint8_t> data) override {
    if (!opened_) return aviotrix::Status::Error(aviotrix::ErrorCode::NotOpen, "write before open");
    if (!seekable_ && offset != static_cast<int64_t>(bytes_.size()))
      return aviotrix::Status::Error(aviotrix::ErrorCode::IoFailed, "non-sequential write to streaming sink");
    const size_t end = static_cast<size_t>(offset) + data.size();
    if (end > bytes_.size()) bytes_.resize(end);
    std::copy(data.begin(), data.end(), bytes_.begin() + offset);
    writes_++;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status close() override {
    closed_ = true;
    return aviotrix::Status::Ok();
  }
  const std::vector<uint8_t>& bytes() const { return bytes_; }
  bool closed() const { return closed_; }
  int writes() const { return writes_; }

 private:
  bool seekable_;
  bool opened_ = false, closed_ = false;
  int writes_ = 0;
  std::vector<uint8_t> bytes_;
};

// Reads from a byte vector; lets tests feed remux output back into MediaReader.
class MemorySource final : public aviotrix::IoSource {
 public:
  explicit MemorySource(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
  aviotrix::Status open(std::optional<int64_t>& size) override {
    size = static_cast<int64_t>(bytes_.size());
    return aviotrix::Status::Ok();
  }
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override {
    if (offset >= static_cast<int64_t>(bytes_.size())) {
      bytesRead = 0;
      return aviotrix::Status::Ok();
    }
    bytesRead = std::min(buffer.size(), bytes_.size() - static_cast<size_t>(offset));
    std::copy_n(bytes_.begin() + offset, bytesRead, buffer.begin());
    return aviotrix::Status::Ok();
  }
  aviotrix::Status close() override { return aviotrix::Status::Ok(); }

 private:
  std::vector<uint8_t> bytes_;
};

}  // namespace test
