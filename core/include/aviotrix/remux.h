#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace aviotrix {

struct RemuxProgress {
  int64_t bytesRead = 0;
  int64_t bytesWritten = 0;
  std::optional<double> timestamp;  // latest muxed pts, seconds
};

struct RemuxOptions {
  std::string format;                         // libav muxer name: "mp4", "mov", "matroska", "webm", "mpegts"
  std::optional<std::vector<int>> streams;    // input stream indices; empty optional = all
  bool failOnIncompatible = false;            // false: skip streams the muxer rejects
  bool fragmented = false;                    // mp4/mov: frag_keyframe+empty_moov; required for unseekable sinks
  const std::atomic<bool>* cancel = nullptr;  // set to true from any thread to abort
  std::function<void(const RemuxProgress&)> onProgress;
  int progressIntervalPackets = 100;
};

struct RemuxStreamMapping {
  int input = 0;
  std::optional<int> output;
  std::string skippedReason;  // non-empty iff output is empty
};

struct RemuxResult {
  std::vector<RemuxStreamMapping> streams;
  int64_t bytesRead = 0;     // bytes pulled from the source during this operation
  int64_t bytesWritten = 0;  // final size of the output
  int64_t packets = 0;
};

}  // namespace aviotrix
