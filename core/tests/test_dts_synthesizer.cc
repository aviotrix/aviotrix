#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "../src/dts_synthesizer.h"

extern "C" {
#include <libavutil/avutil.h>
}

using aviotrix::detail::DtsSynthesizer;

namespace {

AVPacket* makePacket(int64_t pts) {
  AVPacket* p = av_packet_alloc();
  p->pts = pts;
  p->dts = AV_NOPTS_VALUE;
  return p;
}

// Pushes everything and flushes; returns the first error (packets emitted so far stay in `out`).
aviotrix::Status runInto(DtsSynthesizer& synth, const std::vector<int64_t>& ptsInOrder, std::vector<AVPacket*>& out) {
  for (int64_t pts : ptsInOrder) {
    aviotrix::Status st = synth.push(makePacket(pts), out);
    if (!st.ok()) return st;
  }
  return synth.flush(out);
}

std::vector<AVPacket*> run(DtsSynthesizer& synth, const std::vector<int64_t>& ptsInOrder) {
  std::vector<AVPacket*> out;
  REQUIRE(runInto(synth, ptsInOrder, out).ok());
  return out;
}

void freeAll(std::vector<AVPacket*>& packets) {
  for (AVPacket*& p : packets) av_packet_free(&p);
}

}  // namespace

TEST_CASE("DtsSynthesizer orders B-frame streams with dts <= pts and strictly increasing dts") {
  const std::vector<int64_t> pts = {0, 3, 1, 2, 6, 4, 5, 9, 7, 8};
  DtsSynthesizer synth(3);
  auto out = run(synth, pts);
  REQUIRE(out.size() == pts.size());
  for (size_t i = 0; i < out.size(); i++) {
    REQUIRE(out[i]->pts == pts[i]);
    REQUIRE(out[i]->dts != AV_NOPTS_VALUE);
    REQUIRE(out[i]->dts <= out[i]->pts);
    if (i > 0) REQUIRE(out[i]->dts > out[i - 1]->dts);
  }
  freeAll(out);
}

TEST_CASE("DtsSynthesizer on in-order PTS keeps dts <= pts and monotonic") {
  DtsSynthesizer synth(4);
  auto out = run(synth, {10, 20, 30, 40, 50, 60, 70});
  REQUIRE(out.size() == 7);
  for (size_t i = 0; i < out.size(); i++) {
    REQUIRE(out[i]->dts <= out[i]->pts);
    if (i > 0) REQUIRE(out[i]->dts > out[i - 1]->dts);
  }
  freeAll(out);
}

TEST_CASE("DtsSynthesizer passes packets without PTS through in order, dts untouched") {
  DtsSynthesizer synth(2);
  std::vector<AVPacket*> out;
  AVPacket* a = makePacket(AV_NOPTS_VALUE);
  a->dts = 7;
  a->pos = 111;
  AVPacket* b = makePacket(AV_NOPTS_VALUE);
  b->dts = 8;
  b->pos = 222;
  REQUIRE(synth.push(a, out).ok());
  REQUIRE(synth.push(b, out).ok());
  REQUIRE(synth.flush(out).ok());
  REQUIRE(out.size() == 2);
  REQUIRE(out[0]->pos == 111);
  REQUIRE(out[0]->dts == 7);
  REQUIRE(out[1]->pos == 222);
  REQUIRE(out[1]->dts == 8);
  freeAll(out);
}

TEST_CASE("DtsSynthesizer frees packets still queued when destroyed") {
  std::vector<AVPacket*> out;
  {
    DtsSynthesizer synth(8);
    REQUIRE(synth.push(makePacket(0), out).ok());
    REQUIRE(synth.push(makePacket(1), out).ok());
  }
  REQUIRE(out.empty());  // leak is caught by sanitizers; nothing handed out
}

TEST_CASE("DtsSynthesizer tolerates reorder depth that grows after the first window and never changes PTS") {
  const std::vector<int64_t> pts = {0, 1, 2, 3, 4, 5, 6, 10, 8, 9, 13, 11, 12, 16, 14, 15};
  DtsSynthesizer synth(3);
  auto out = run(synth, pts);
  REQUIRE(out.size() == pts.size());
  for (size_t i = 0; i < out.size(); i++) {
    REQUIRE(out[i]->pts == pts[i]);
    REQUIRE(out[i]->dts <= out[i]->pts);
    if (i > 0) REQUIRE(out[i]->dts > out[i - 1]->dts);
  }
  freeAll(out);
}

TEST_CASE("DtsSynthesizer reports Unsupported when reorder depth exceeds the floor, without touching PTS") {
  const std::vector<int64_t> pts = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 20, 16, 12, 11, 13, 14, 15, 17, 18, 19};
  DtsSynthesizer synth(8);
  std::vector<AVPacket*> out;
  aviotrix::Status st = runInto(synth, pts, out);
  REQUIRE(st.code == static_cast<int>(aviotrix::ErrorCode::Unsupported));
  for (size_t i = 0; i < out.size(); i++) REQUIRE(out[i]->pts == pts[i]);
  freeAll(out);
}
