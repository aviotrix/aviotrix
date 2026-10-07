# aviotrix

libav (FFmpeg n8.1.3, LGPL) core with Node native and WASM bindings behind a custom AVIO layer.
Spec: docs/superpowers/specs/2026-10-06-aviotrix-design.md

## Layout

- `core/` C++20 static lib. No Node/Emscripten headers. `-fno-exceptions`.
- `packages/types` `@aviotrix/types`, `packages/node` `@aviotrix/node`, `packages/wasm` `@aviotrix/wasm`.
- `scripts/build-ffmpeg.sh host|wasm` builds FFmpeg into `build/ffmpeg-<target>/`.

## Rules

- TypeScript 7 strict. No `any`, no `unknown`. Lint = oxlint, format = Prettier (2 spaces, semicolons).
- A change is done only when `npm run lint && npm run typecheck && npm test && npm run build` pass.
- Never push without explicit approval.

## Prerequisites (macOS)

`brew install cmake nasm emscripten ffmpeg` (ffmpeg CLI only regenerates fixtures).
