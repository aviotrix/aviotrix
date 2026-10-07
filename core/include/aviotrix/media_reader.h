#pragma once

#include <memory>

#include "aviotrix/io.h"
#include "aviotrix/log.h"
#include "aviotrix/metadata.h"
#include "aviotrix/remux.h"
#include "aviotrix/status.h"

namespace aviotrix {

struct OpenOptions {
  LogHook log;  // receives libav log lines while this reader is operating; may be empty
};

class MediaReader {
 public:
  MediaReader();
  ~MediaReader();
  MediaReader(const MediaReader&) = delete;
  MediaReader& operator=(const MediaReader&) = delete;

  // Opens `source` (which must outlive the reader until close()) and probes stream info.
  Status open(IoSource& source, OpenOptions options = {});
  bool isOpen() const;
  const Metadata& metadata() const;

  // Stream-copies the selected streams into `sink`. See remux.h for the error contract.
  Status remux(IoSink& sink, const RemuxOptions& options, RemuxResult& result);
  Status close();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace aviotrix
