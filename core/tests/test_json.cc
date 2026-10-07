#include <catch2/catch_test_macros.hpp>

#include "aviotrix/json.h"
#include "aviotrix/media_reader.h"
#include "file_io.h"
#include "fixtures.h"

TEST_CASE("JsonWriter escapes strings and handles nesting") {
  aviotrix::JsonWriter w;
  w.beginObject();
  w.key("s");
  w.value(std::string_view("a\"b\\c\nd\x01"));
  w.key("n");
  w.value(int64_t{-5});
  w.key("d");
  w.value(1.5);
  w.key("b");
  w.value(true);
  w.key("z");
  w.null();
  w.key("arr");
  w.beginArray();
  w.value(int64_t{1});
  w.value(int64_t{2});
  w.endArray();
  w.endObject();
  REQUIRE(w.str() == R"({"s":"a\"b\\c\nd\u0001","n":-5,"d":1.5,"b":true,"z":null,"arr":[1,2]})");
}

TEST_CASE("toJson(Metadata) contains the stream fields") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  aviotrix::MediaReader reader;
  REQUIRE(reader.open(src).ok());
  const std::string json = aviotrix::toJson(reader.metadata());
  REQUIRE(json.find(R"("format":"mov,mp4,m4a,3gp,3g2,mj2")") != std::string::npos);
  REQUIRE(json.find(R"("codec":"h264")") != std::string::npos);
  REQUIRE(json.find(R"("width":320)") != std::string::npos);
  REQUIRE(json.find(R"("frameRate":{"num":30,"den":1})") != std::string::npos);
  REQUIRE(json.find(R"("type":"audio")") != std::string::npos);
  REQUIRE(json.find(R"("sampleRate":48000)") != std::string::npos);
  REQUIRE(json.find(R"("bitRate":)") != std::string::npos);
}
