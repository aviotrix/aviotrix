#pragma once

#include <memory>

#include "avio_input.h"
#include "aviotrix/media_reader.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace aviotrix {

struct MediaReader::Impl {
  AVFormatContext* fmt = nullptr;
  std::unique_ptr<detail::AvioInput> input;
  Metadata metadata;
  LogHook log;
  bool open = false;
  bool consumed =
      false;  // set when a remux begins reading, even if it fails or aborts: the input is no longer at the start

  void release() {
    if (fmt) avformat_close_input(&fmt);  // does not touch the custom pb
    input.reset();                        // closes the IoSource, frees the AVIOContext
    open = false;
  }
};

}  // namespace aviotrix
