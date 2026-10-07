#include "aviotrix/status.h"

#include <cerrno>
#include <cstring>

extern "C" {
#include <libavutil/error.h>
}

namespace aviotrix {

namespace {

const char* taggedName(int code) {
  switch (code) {
    case AVERROR_EOF:
      return "AVERROR_EOF";
    case AVERROR_INVALIDDATA:
      return "AVERROR_INVALIDDATA";
    case AVERROR_EXIT:
      return "AVERROR_EXIT";
    case AVERROR_BUG:
      return "AVERROR_BUG";
    case AVERROR_BUG2:
      return "AVERROR_BUG2";
    case AVERROR_PATCHWELCOME:
      return "AVERROR_PATCHWELCOME";
    case AVERROR_DEMUXER_NOT_FOUND:
      return "AVERROR_DEMUXER_NOT_FOUND";
    case AVERROR_MUXER_NOT_FOUND:
      return "AVERROR_MUXER_NOT_FOUND";
    case AVERROR_STREAM_NOT_FOUND:
      return "AVERROR_STREAM_NOT_FOUND";
    case AVERROR_OPTION_NOT_FOUND:
      return "AVERROR_OPTION_NOT_FOUND";
    case AVERROR_DECODER_NOT_FOUND:
      return "AVERROR_DECODER_NOT_FOUND";
    case AVERROR_ENCODER_NOT_FOUND:
      return "AVERROR_ENCODER_NOT_FOUND";
    case AVERROR_BSF_NOT_FOUND:
      return "AVERROR_BSF_NOT_FOUND";
    case AVERROR_PROTOCOL_NOT_FOUND:
      return "AVERROR_PROTOCOL_NOT_FOUND";
    case AVERROR_EXTERNAL:
      return "AVERROR_EXTERNAL";
    case AVERROR_UNKNOWN:
      return "AVERROR_UNKNOWN";
    default:
      return nullptr;
  }
}

const char* posixName(int err) {
  switch (err) {
    case ENOMEM:
      return "ENOMEM";
    case EINVAL:
      return "EINVAL";
    case EIO:
      return "EIO";
    case ENOSYS:
      return "ENOSYS";
    case EAGAIN:
      return "EAGAIN";
    case EPIPE:
      return "EPIPE";
    case ENOENT:
      return "ENOENT";
    case ERANGE:
      return "ERANGE";
    default:
      return nullptr;
  }
}

}  // namespace

Status Status::FromAv(int averror, std::string_view context) {
  char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(averror, buf, sizeof buf);
  Status s;
  s.code = averror;
  s.message.append(context).append(": ").append(buf);
  return s;
}

Status Status::Error(ErrorCode code, std::string message) {
  return Status{static_cast<int>(code), std::move(message)};
}

std::string errorCodeName(int code) {
  if (code == 0) return "OK";
  if (code > 0) {
    switch (static_cast<ErrorCode>(code)) {
      case ErrorCode::SinkNotSeekable:
        return "SINK_NOT_SEEKABLE";
      case ErrorCode::IncompatibleStream:
        return "INCOMPATIBLE_STREAM";
      case ErrorCode::Aborted:
        return "ABORTED";
      case ErrorCode::InvalidArgument:
        return "INVALID_ARGUMENT";
      case ErrorCode::IoFailed:
        return "IO_FAILED";
      case ErrorCode::NotOpen:
        return "NOT_OPEN";
      case ErrorCode::Unsupported:
        return "UNSUPPORTED";
      case ErrorCode::Ok:
        return "OK";
    }
    return "UNKNOWN(" + std::to_string(code) + ")";
  }
  if (const char* tagged = taggedName(code)) return tagged;
  const int err = AVUNERROR(code);
  if (const char* p = posixName(err)) return std::string("AVERROR(") + p + ")";
  return "AVERROR(" + std::to_string(err) + ")";
}

}  // namespace aviotrix
