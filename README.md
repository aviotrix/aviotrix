# aviotrix

libav (FFmpeg) core with a custom AVIO layer, exposed as a Node native module (`@aviotrix/node`) and a WebAssembly module (`@aviotrix/wasm`), sharing one TypeScript contract (`@aviotrix/types`).

Design: `docs/superpowers/specs/2026-10-06-aviotrix-design.md`.

## Status

Milestone 1 in progress: open via `IoSource`, read metadata, remux to `IoSink`.

## Build prerequisites

macOS: `brew install cmake nasm emscripten ffmpeg`. FFmpeg pinned at `n8.1.3`, LGPL build.

## WASM size

Measured with `npm run size -w @aviotrix/wasm` (FFmpeg `n8.1.3`, Emscripten 6.0.10, `-Oz`):

- `aviotrix.wasm`: 1320838 bytes raw, 544024 bytes gzipped (`gzip -9`).
