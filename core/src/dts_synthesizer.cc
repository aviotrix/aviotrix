#include "dts_synthesizer.h"

#include <algorithm>

extern "C" {
#include <libavutil/avutil.h>
}

namespace aviotrix::detail {

DtsSynthesizer::DtsSynthesizer(size_t depth, int videoDelay)
    : depth_(depth), videoDelay_(videoDelay), lastDts_(AV_NOPTS_VALUE) {}

DtsSynthesizer::~DtsSynthesizer() {
  for (AVPacket* p : queue_) av_packet_free(&p);
}

Status DtsSynthesizer::push(AVPacket* pkt, std::vector<AVPacket*>& out) {
  queue_.push_back(pkt);
  if (pkt->pts != AV_NOPTS_VALUE) pts_.insert(pkt->pts);
  if (queue_.size() <= depth_) return Status::Ok();
  AVPacket* ready = nullptr;
  Status st = popFront(ready);
  if (st.ok()) out.push_back(ready);
  return st;
}

Status DtsSynthesizer::flush(std::vector<AVPacket*>& out) {
  while (!queue_.empty()) {
    AVPacket* ready = nullptr;
    Status st = popFront(ready);
    if (!st.ok()) return st;
    out.push_back(ready);
  }
  return Status::Ok();
}

// The k-th decoded packet receives the k-th smallest PTS. With reordering that can exceed the
// packet's own PTS, so subtract a constant: the worst excess over the first window, but at least
// `max(videoDelay, 2)` minimum PTS steps to leave room for reorder depth that grows later.
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
  int64_t minStep = 0;
  for (size_t i = 1; i < sorted.size(); i++) {
    const int64_t d = sorted[i] - sorted[i - 1];
    if (d > 0 && (minStep == 0 || d < minStep)) minStep = d;
  }
  if (minStep == 0) minStep = 1;
  offset_ = std::max(worst, std::max<int64_t>(videoDelay_, 2) * minStep);
}

Status DtsSynthesizer::popFront(AVPacket*& out) {
  if (!offsetKnown_) computeOffset();
  AVPacket* pkt = queue_.front();
  queue_.pop_front();
  if (pkt->pts == AV_NOPTS_VALUE) {  // dts left untouched
    out = pkt;
    return Status::Ok();
  }

  int64_t dts = *pts_.begin() - offset_;
  pts_.erase(pts_.begin());
  if (lastDts_ != AV_NOPTS_VALUE && dts <= lastDts_) dts = lastDts_ + 1;
  if (dts > pkt->pts) {
    av_packet_free(&pkt);
    return Status::Error(ErrorCode::Unsupported, "video reorder depth exceeds what can be remuxed without decoding");
  }
  pkt->dts = dts;
  lastDts_ = dts;
  out = pkt;
  return Status::Ok();
}

}  // namespace aviotrix::detail
