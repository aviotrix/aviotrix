#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aviotrix {

struct Rational {
  int num = 0;
  int den = 1;
};

enum class StreamType { Video, Audio, Subtitle, Data, Attachment };
const char* streamTypeName(StreamType type);  // "video", "audio", "subtitle", "data", "attachment"

struct VideoStreamInfo {
  int width = 0;
  int height = 0;
  std::optional<Rational> frameRate;
  std::optional<std::string> pixelFormat;
};

struct AudioStreamInfo {
  int sampleRate = 0;
  int channels = 0;
  std::optional<std::string> channelLayout;
};

struct StreamInfo {
  int index = 0;
  StreamType type = StreamType::Data;
  std::string codec;
  std::optional<std::string> codecTag;
  Rational timeBase;
  std::optional<double> startTime;  // seconds
  std::optional<double> duration;   // seconds
  std::optional<int64_t> bitRate;
  std::optional<std::string> language;
  std::map<std::string, std::string> tags;
  std::optional<VideoStreamInfo> video;
  std::optional<AudioStreamInfo> audio;
};

struct Metadata {
  std::string format;
  std::string formatLongName;
  std::optional<double> startTime;
  std::optional<double> duration;
  std::optional<int64_t> bitRate;
  std::map<std::string, std::string> tags;
  std::vector<StreamInfo> streams;
};

}  // namespace aviotrix
