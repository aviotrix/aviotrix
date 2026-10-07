cmake_minimum_required(VERSION 3.28)
project(aviotrix LANGUAGES C CXX)

option(AVIOTRIX_CLANG_TIDY "Run clang-tidy during the build" OFF)
if(AVIOTRIX_CLANG_TIDY)
  # PATH first (CI: apt clang-tidy), then Homebrew llvm, which is keg-only on macOS.
  find_program(CLANG_TIDY_EXE NAMES clang-tidy PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(EMSCRIPTEN)
  set(AVIOTRIX_FFMPEG_TARGET wasm)
else()
  set(AVIOTRIX_FFMPEG_TARGET host)
endif()
# CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: packages/node includes this file via add_subdirectory.
set(AVIOTRIX_FFMPEG_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/ffmpeg-${AVIOTRIX_FFMPEG_TARGET}" CACHE PATH "FFmpeg install prefix")

if(NOT EXISTS "${AVIOTRIX_FFMPEG_PREFIX}/lib/libavformat.a")
  message(STATUS "aviotrix: building FFmpeg for ${AVIOTRIX_FFMPEG_TARGET} into ${AVIOTRIX_FFMPEG_PREFIX}")
  execute_process(
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-ffmpeg.sh" "${AVIOTRIX_FFMPEG_TARGET}" "${AVIOTRIX_FFMPEG_PREFIX}"
    RESULT_VARIABLE ffmpeg_rc)
  if(NOT ffmpeg_rc EQUAL 0)
    message(FATAL_ERROR "aviotrix: FFmpeg build failed (${ffmpeg_rc})")
  endif()
endif()

foreach(lib avutil avcodec avformat)
  add_library(ffmpeg::${lib} STATIC IMPORTED GLOBAL)
  set_target_properties(ffmpeg::${lib} PROPERTIES
    IMPORTED_LOCATION "${AVIOTRIX_FFMPEG_PREFIX}/lib/lib${lib}.a"
    INTERFACE_INCLUDE_DIRECTORIES "${AVIOTRIX_FFMPEG_PREFIX}/include")
endforeach()
set_property(TARGET ffmpeg::avcodec APPEND PROPERTY INTERFACE_LINK_LIBRARIES ffmpeg::avutil)
set_property(TARGET ffmpeg::avformat APPEND PROPERTY INTERFACE_LINK_LIBRARIES ffmpeg::avcodec)
if(NOT EMSCRIPTEN)
  find_package(Threads REQUIRED)
  set_property(TARGET ffmpeg::avutil APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads m)
endif()

add_subdirectory(core)

option(AVIOTRIX_BUILD_TESTS "Build core tests" ON)
if(AVIOTRIX_BUILD_TESTS AND NOT EMSCRIPTEN AND NOT CMAKE_JS_VERSION)
  enable_testing()
  add_subdirectory(core/tests)
endif()
# packages/node is its own CMake project that includes this file (Task 8).
if(EMSCRIPTEN)
  add_subdirectory(packages/wasm)
endif()
