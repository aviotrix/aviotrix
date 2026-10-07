#include "aviotrix/json.h"

#include <cmath>
#include <cstdio>

namespace aviotrix {

void JsonWriter::separator() {
  if (afterKey_) {
    afterKey_ = false;
    return;
  }
  if (needComma_.empty()) return;
  if (needComma_.back()) out_ += ',';
  needComma_.back() = true;
}

void JsonWriter::beginObject() {
  separator();
  out_ += '{';
  needComma_.push_back(false);
}
void JsonWriter::endObject() {
  out_ += '}';
  needComma_.pop_back();
}
void JsonWriter::beginArray() {
  separator();
  out_ += '[';
  needComma_.push_back(false);
}
void JsonWriter::endArray() {
  out_ += ']';
  needComma_.pop_back();
}

void JsonWriter::key(std::string_view name) {
  separator();
  escape(name);
  out_ += ':';
  afterKey_ = true;
}

void JsonWriter::value(std::string_view s) {
  separator();
  escape(s);
}
void JsonWriter::value(int64_t n) {
  separator();
  out_ += std::to_string(n);
}
void JsonWriter::value(bool b) {
  separator();
  out_ += b ? "true" : "false";
}
void JsonWriter::null() {
  separator();
  out_ += "null";
}

void JsonWriter::value(double d) {
  separator();
  if (!std::isfinite(d)) {
    out_ += "null";
    return;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.17g", d);
  out_ += buf;
}

void JsonWriter::escape(std::string_view s) {
  out_ += '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out_ += "\\\"";
        break;
      case '\\':
        out_ += "\\\\";
        break;
      case '\n':
        out_ += "\\n";
        break;
      case '\r':
        out_ += "\\r";
        break;
      case '\t':
        out_ += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out_ += buf;
        } else {
          out_ += static_cast<char>(c);
        }
    }
  }
  out_ += '"';
}

namespace {

void writeOptional(JsonWriter& w, const char* key, const std::optional<double>& v) {
  w.key(key);
  if (v)
    w.value(*v);
  else
    w.null();
}
void writeOptional(JsonWriter& w, const char* key, const std::optional<int64_t>& v) {
  w.key(key);
  if (v)
    w.value(*v);
  else
    w.null();
}
void writeOptional(JsonWriter& w, const char* key, const std::optional<std::string>& v) {
  w.key(key);
  if (v)
    w.value(*v);
  else
    w.null();
}
void writeRational(JsonWriter& w, const Rational& r) {
  w.beginObject();
  w.key("num");
  w.value(r.num);
  w.key("den");
  w.value(r.den);
  w.endObject();
}
void writeTags(JsonWriter& w, const std::map<std::string, std::string>& tags) {
  w.key("tags");
  w.beginObject();
  for (const auto& [k, v] : tags) {
    w.key(k);
    w.value(v);
  }
  w.endObject();
}

}  // namespace

std::string toJson(const Metadata& m) {
  JsonWriter w;
  w.beginObject();
  w.key("format");
  w.value(m.format);
  w.key("formatLongName");
  w.value(m.formatLongName);
  writeOptional(w, "startTime", m.startTime);
  writeOptional(w, "duration", m.duration);
  writeOptional(w, "bitRate", m.bitRate);
  writeTags(w, m.tags);
  w.key("streams");
  w.beginArray();
  for (const StreamInfo& s : m.streams) {
    w.beginObject();
    w.key("index");
    w.value(s.index);
    w.key("type");
    w.value(streamTypeName(s.type));
    w.key("codec");
    w.value(s.codec);
    writeOptional(w, "codecTag", s.codecTag);
    w.key("timeBase");
    writeRational(w, s.timeBase);
    writeOptional(w, "startTime", s.startTime);
    writeOptional(w, "duration", s.duration);
    writeOptional(w, "bitRate", s.bitRate);
    writeOptional(w, "language", s.language);
    writeTags(w, s.tags);
    if (s.video) {
      w.key("video");
      w.beginObject();
      w.key("width");
      w.value(s.video->width);
      w.key("height");
      w.value(s.video->height);
      w.key("frameRate");
      if (s.video->frameRate)
        writeRational(w, *s.video->frameRate);
      else
        w.null();
      writeOptional(w, "pixelFormat", s.video->pixelFormat);
      w.endObject();
    }
    if (s.audio) {
      w.key("audio");
      w.beginObject();
      w.key("sampleRate");
      w.value(s.audio->sampleRate);
      w.key("channels");
      w.value(s.audio->channels);
      writeOptional(w, "channelLayout", s.audio->channelLayout);
      w.endObject();
    }
    w.endObject();
  }
  w.endArray();
  w.endObject();
  return w.str();
}

std::string toJson(const RemuxResult& r) {
  JsonWriter w;
  w.beginObject();
  w.key("streams");
  w.beginArray();
  for (const RemuxStreamMapping& s : r.streams) {
    w.beginObject();
    w.key("input");
    w.value(s.input);
    w.key("output");
    if (s.output)
      w.value(*s.output);
    else
      w.null();
    if (!s.output) {
      w.key("skippedReason");
      w.value(s.skippedReason);
    }
    w.endObject();
  }
  w.endArray();
  w.key("bytesRead");
  w.value(r.bytesRead);
  w.key("bytesWritten");
  w.value(r.bytesWritten);
  w.key("packets");
  w.value(r.packets);
  w.endObject();
  return w.str();
}

}  // namespace aviotrix
