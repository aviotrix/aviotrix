# aviotrix

libav (FFmpeg) core with a custom AVIO layer, exposed as a Node native module (`@aviotrix/node`) and a WebAssembly module (`@aviotrix/wasm`), sharing one TypeScript contract (`@aviotrix/types`).

Design: `docs/superpowers/specs/2026-10-06-aviotrix-design.md`.

## Status

Milestone 1 in progress: open via `IoSource`, read metadata, remux to `IoSink`.

## Install

    npm install @aviotrix/node     # Node: prebuilt binary (darwin-arm64, darwin-x64, linux-x64, linux-arm64) or source build
    npm install @aviotrix/wasm     # Browser: Chrome 137+, Safari 27+, Firefox 153+ (needs WebAssembly JSPI)

Source builds need cmake, make, nasm, a C++20 compiler, and network access to clone FFmpeg n8.1.3.

## Develop

    brew install cmake nasm emscripten ffmpeg   # ffmpeg CLI only regenerates fixtures
    npm install
    npm run test:core                            # builds FFmpeg (host) on first run, then the core tests
    npm run build:native -w @aviotrix/node && npm test -w @aviotrix/node
    npm run build -w @aviotrix/wasm && npx playwright install chromium && npm test -w @aviotrix/wasm

## WASM size

Measured with `npm run size -w @aviotrix/wasm` (FFmpeg `n8.1.3`, Emscripten 6.0.10, `-Oz`):

- `aviotrix.wasm`: 1320838 bytes raw, 544024 bytes gzipped (`gzip -9`).
