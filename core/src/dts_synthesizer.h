#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <vector>

extern "C" {
#include <libavcodec/packet.h>
}

namespace aviotrix::detail {

// Assigns monotonically increasing DTS to packets that arrive in decode order with PTS only
// (Matroska, some TS). Works without a decoder by buffering `depth` packets and handing out the
// smallest buffered PTS as each packet's DTS, shifted down by a constant so that DTS <= PTS.
// Operates in whatever time base the packets are already in.
class DtsSynthesizer {
 public:
  explicit DtsSynthesizer(size_t depth = 16);  // 16 = H.264/HEVC maximum reorder depth
  ~DtsSynthesizer();                           // frees packets still queued
  DtsSynthesizer(const DtsSynthesizer&) = delete;
  DtsSynthesizer& operator=(const DtsSynthesizer&) = delete;

  // Takes ownership of the heap-allocated `pkt`. Appends packets that are ready (0 or 1 per push,
  // in decode order) to `out`; the caller writes each one and then av_packet_free()s it.
  void push(AVPacket* pkt, std::vector<AVPacket*>& out);
  // Drains everything remaining, in decode order.
  void flush(std::vector<AVPacket*>& out);

 private:
  AVPacket* popFront();
  void computeOffset();

  size_t depth_;
  std::deque<AVPacket*> queue_;  // decode order
  std::multiset<int64_t> pts_;   // PTS of everything in queue_ that has one
  int64_t lastDts_;
  int64_t offset_ = 0;
  bool offsetKnown_ = false;
};

}  // namespace aviotrix::detail
