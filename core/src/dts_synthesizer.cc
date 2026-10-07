#include "dts_synthesizer.h"

#include <algorithm>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/log.h>
}

namespace aviotrix::detail {

DtsSynthesizer::DtsSynthesizer(size_t depth) : depth_(depth), lastDts_(AV_NOPTS_VALUE) {}

DtsSynthesizer::~DtsSynthesizer() {
  for (AVPacket* p : queue_) av_packet_free(&p);
}

void DtsSynthesizer::push(AVPacket* pkt, std::vector<AVPacket*>& out) {
  queue_.push_back(pkt);
  if (pkt->pts != AV_NOPTS_VALUE) pts_.insert(pkt->pts);
  if (queue_.size() > depth_) out.push_back(popFront());
}

void DtsSynthesizer::flush(std::vector<AVPacket*>& out) {
  while (!queue_.empty()) out.push_back(popFront());
}

// The k-th decoded packet receives the k-th smallest PTS. With reordering that can exceed the
// packet's own PTS, so measure the worst excess over the first window and subtract it everywhere.
void DtsSynthesizer::computeOffset() {
  offsetKnown_ = true;
  std::vector<int64_t> sorted(pts_.begin(), pts_.end());
  size_t k = 0;
  int64_t worst = 0;
  for (const AVPacket* p : queue_) {
    if (p->pts == AV_NOPTS_VALUE) continue;
    worst = std::max(worst, sorted[k] - p->pts);
    k++;
  }
  offset_ = worst;
}

AVPacket* DtsSynthesizer::popFront() {
  if (!offsetKnown_) computeOffset();
  AVPacket* pkt = queue_.front();
  queue_.pop_front();
  if (pkt->pts == AV_NOPTS_VALUE) return pkt;  // dts left untouched

  int64_t dts = *pts_.begin() - offset_;
  pts_.erase(pts_.begin());
  if (lastDts_ != AV_NOPTS_VALUE && dts <= lastDts_) dts = lastDts_ + 1;
  if (dts > pkt->pts) {
    if (lastDts_ != AV_NOPTS_VALUE && pkt->pts <= lastDts_) {
      av_log(nullptr, AV_LOG_WARNING, "aviotrix: raising pts %lld to keep synthesized dts monotonic\n",
             static_cast<long long>(pkt->pts));
      pkt->pts = dts;
    } else {
      dts = pkt->pts;
    }
  }
  pkt->dts = dts;
  lastDts_ = dts;
  return pkt;
}

}  // namespace aviotrix::detail
