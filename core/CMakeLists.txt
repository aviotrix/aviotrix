add_library(aviotrix_core STATIC
  src/status.cc
  src/log.cc
  src/avio_input.cc
  src/avio_output.cc
  src/log_router.cc
  src/metadata.cc
  src/json.cc
  src/media_reader.cc
  src/dts_synthesizer.cc
  src/remux.cc)
target_include_directories(aviotrix_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(aviotrix_core PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Werror)
target_link_libraries(aviotrix_core PUBLIC ffmpeg::avformat)
