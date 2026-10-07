# Built with: emcmake cmake -S ../.. -B build/wasm (see package.json)
add_executable(aviotrix_wasm
  binding/exports.cc
  binding/js_imports.cc
  binding/js_io.cc)
set_target_properties(aviotrix_wasm PROPERTIES OUTPUT_NAME aviotrix SUFFIX ".mjs")
target_compile_options(aviotrix_wasm PRIVATE -Oz -Wall -Wextra -Werror)
target_link_libraries(aviotrix_wasm PRIVATE aviotrix_core)
target_link_options(aviotrix_wasm PRIVATE
  -Oz
  -sJSPI
  "-sJSPI_EXPORTS=avx_reader_open,avx_reader_remux,avx_reader_close"
  -sMODULARIZE -sEXPORT_ES6 -sEXPORT_NAME=createAviotrixModule
  "-sENVIRONMENT=web,worker"
  -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=32MB -sSTACK_SIZE=1MB
  "-sEXPORTED_FUNCTIONS=_malloc,_free,_avx_reader_new,_avx_reader_open,_avx_reader_metadata,_avx_reader_remux,_avx_reader_result,_avx_reader_cancel,_avx_reader_close,_avx_reader_free,_avx_last_error_code,_avx_last_error_message"
  "-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP32,UTF8ToString,stringToUTF8,lengthBytesUTF8"
  -sFILESYSTEM=0 -sNO_EXIT_RUNTIME=1 --no-entry
  "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/binding/pre.js")
