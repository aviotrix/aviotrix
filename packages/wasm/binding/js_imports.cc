#include "js_imports.h"

#include <emscripten.h>

// clang-format off
EM_JS(int, avx_js_copy_host_error, (char* dest, int maxBytes), {
  const s = Module.aviotrixLastHostError || "";
  Module.aviotrixLastHostError = "";
  stringToUTF8(s, dest, maxBytes);
  return lengthBytesUTF8(s);
});

EM_ASYNC_JS(double, avx_js_source_open, (int hostId), {
  const host = Module.aviotrixHosts.get(hostId);
  if (!host) { Module.aviotrixLastHostError = 'no host registered for id ' + hostId; return -2; }
  try {
    const size = await host.sourceOpen();
    return size === null || size === undefined ? -1 : Number(size);
  } catch (e) { Module.aviotrixLastHostError = String(e && e.message ? e.message : e); return -2; }
});

EM_ASYNC_JS(int, avx_js_source_read, (int hostId, double offset, int length, uint8_t* dest), {
  const host = Module.aviotrixHosts.get(hostId);
  if (!host) { Module.aviotrixLastHostError = 'no host registered for id ' + hostId; return -1; }
  try {
    const bytes = await host.sourceRead(offset, length);
    const n = Math.min(bytes.byteLength, length);  // over-long reads are truncated
    HEAPU8.set(bytes.subarray(0, n), dest);
    return n;
  } catch (e) { Module.aviotrixLastHostError = String(e && e.message ? e.message : e); return -1; }
});

EM_ASYNC_JS(int, avx_js_source_close, (int hostId), {
  const host = Module.aviotrixHosts.get(hostId);
  if (!host) return 0;
  try { await host.sourceClose(); return 0; }
  catch (e) { Module.aviotrixLastHostError = String(e && e.message ? e.message : e); return -1; }
});

EM_ASYNC_JS(int, avx_js_sink_open, (int hostId), {
  const host = Module.aviotrixHosts.get(hostId);
  if (!host) { Module.aviotrixLastHostError = 'no host registered for id ' + hostId; return -1; }
  try { await host.sinkOpen(); return 0; }
  catch (e) { Module.aviotrixLastHostError = String(e && e.message ? e.message : e); return -1; }
});

EM_ASYNC_JS(int, avx_js_sink_write, (int hostId, double offset, const uint8_t* src, int length), {
  const host = Module.aviotrixHosts.get(hostId);
  if (!host) { Module.aviotrixLastHostError = 'no host registered for id ' + hostId; return -1; }
  try {
    await host.sinkWrite(offset, HEAPU8.slice(src, src + length));  // copy: the libav buffer is reused
    return 0;
  } catch (e) { Module.aviotrixLastHostError = String(e && e.message ? e.message : e); return -1; }
});

EM_ASYNC_JS(int, avx_js_sink_close, (int hostId), {
  const host = Module.aviotrixHosts.get(hostId);
  if (!host) return 0;
  try { await host.sinkClose(); return 0; }
  catch (e) { Module.aviotrixLastHostError = String(e && e.message ? e.message : e); return -1; }
});

EM_JS(void, avx_js_log, (int hostId, const char* level, const char* text), {
  const host = Module.aviotrixHosts.get(hostId);
  if (host && typeof host.onLog === 'function') host.onLog(UTF8ToString(level), UTF8ToString(text));
});

EM_JS(void, avx_js_progress, (int hostId, double bytesRead, double bytesWritten, double timestamp, int hasTimestamp), {
  const host = Module.aviotrixHosts.get(hostId);
  if (host && typeof host.onProgress === 'function') host.onProgress(bytesRead, bytesWritten, hasTimestamp ? timestamp : null);
});
// clang-format on
