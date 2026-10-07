#pragma once

#include <cstdint>

// Implemented in js_imports.cc with EM_ASYNC_JS / EM_JS. All return <0 on host error;
// the message is then available via avx_js_copy_host_error.
extern "C" {
double avx_js_source_open(int hostId);                                         // size, -1 = null (unknown), -2 = error
int avx_js_source_read(int hostId, double offset, int length, uint8_t* dest);  // bytes copied, <0 error
int avx_js_source_close(int hostId);
int avx_js_sink_open(int hostId);
int avx_js_sink_write(int hostId, double offset, const uint8_t* src, int length);
int avx_js_sink_close(int hostId);
void avx_js_log(int hostId, const char* level, const char* text);
void avx_js_progress(int hostId, double bytesRead, double bytesWritten, double timestamp, int hasTimestamp);
int avx_js_copy_host_error(char* dest, int maxBytes);  // copies and clears Module.aviotrixLastHostError
}
