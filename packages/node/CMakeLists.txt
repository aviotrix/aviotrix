cmake_minimum_required(VERSION 3.28)
project(aviotrix_node LANGUAGES C CXX)

# The root sets these in its own directory scope; they do not propagate back to this project.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Repo root in the monorepo; `native-src` inside a published tarball (Task 13).
set(AVIOTRIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Directory holding core/, scripts/, third_party/ffmpeg and the root CMakeLists.txt")
get_filename_component(AVIOTRIX_ROOT "${AVIOTRIX_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AVIOTRIX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${AVIOTRIX_ROOT}" "${CMAKE_BINARY_DIR}/aviotrix_root")

execute_process(
  COMMAND node -p "require('node-addon-api').include_dir"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE)

add_library(aviotrix_node SHARED
  binding/addon.cc
  binding/main_thread_bridge.cc
  binding/js_io.cc
  binding/native_reader.cc
  ${CMAKE_JS_SRC})
set_target_properties(aviotrix_node PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(aviotrix_node PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})
target_compile_definitions(aviotrix_node PRIVATE NAPI_VERSION=8 NAPI_CPP_EXCEPTIONS)
target_compile_options(aviotrix_node PRIVATE -Wall -Wextra -Werror)
target_link_libraries(aviotrix_node PRIVATE aviotrix_core ${CMAKE_JS_LIB})
