# aviotrix

libav (FFmpeg) core with a custom AVIO layer, exposed as a Node native module (`@aviotrix/node`) and a WebAssembly module (`@aviotrix/wasm`), sharing one TypeScript contract (`@aviotrix/types`).

Design: `docs/superpowers/specs/2026-10-06-aviotrix-design.md`.

## Status

Milestone 1 complete: open via IoSource, read metadata, remux to IoSink, on Node and in JSPI browsers.

## Install

    npm install @aviotrix/node     # Node: prebuilt binary (darwin-arm64, darwin-x64, linux-x64, linux-arm64) or source build
    npm install @aviotrix/wasm     # Browser: Chrome 137+, Safari 27+, Firefox 153+ (needs WebAssembly JSPI)

Source builds need cmake, make, nasm, a C++20 compiler, and network access to clone FFmpeg n8.1.3.

## Behavior

Remux output starts at 0. Like the `ffmpeg` CLI default, the input's start time (for example the
~1.46 s offset of a typical MPEG-TS capture) is subtracted from every packet, so the output has no
leading gap or empty edit, and its duration matches the input's. Relative offsets between streams
are kept. Streams the target muxer cannot carry are skipped with a `skippedReason` (or rejected
with `INCOMPATIBLE_STREAM` when `onIncompatibleStream: 'fail'`).

## Develop

    brew install cmake nasm emscripten ffmpeg   # ffmpeg CLI only regenerates fixtures
    npm install
    npm run test:core                            # builds FFmpeg (host) on first run, then the core tests
    npm run build:native -w @aviotrix/node && npm test -w @aviotrix/node
    npm run build -w @aviotrix/wasm && npx playwright install chromium && npm test -w @aviotrix/wasm

C++ lint tools: CI pins LLVM 18 (`clang-format-18`, `clang-tidy-18` on Ubuntu 24.04). `npm run
lint:cpp` uses `$CLANG_FORMAT` or `clang-format` from PATH; the check gives identical results with
clang-format 18 through 23 for this `.clang-format`. The clang-tidy build
(`cmake -S . -B build/core-tidy -DAVIOTRIX_CLANG_TIDY=ON -DAVIOTRIX_BUILD_TESTS=OFF`) finds
clang-tidy on PATH or Homebrew's keg-only `llvm` (`brew install llvm`); findings can differ between
LLVM majors, so CI's version is the reference.

## WASM size

Measured with `npm run size -w @aviotrix/wasm` (FFmpeg `n8.1.3`, Emscripten 6.0.10, `-Oz`):

- `aviotrix.wasm`: 1320838 bytes raw, 544024 bytes gzipped (`gzip -9`).
