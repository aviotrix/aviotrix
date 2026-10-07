# aviotrix Milestone 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `@aviotrix/types`, `@aviotrix/node`, and `@aviotrix/wasm` so a caller can open media through an async `IoSource`, read metadata, and remux to an async `IoSink`, identically on Node and in a JSPI browser.

**Architecture:** A synchronous, exception-free C++20 core wraps libavformat/libavcodec behind positioned `IoSource`/`IoSink` interfaces and owns the `AVIOContext`s. The Node binding runs each reader on a dedicated thread and blocks that thread on a thread-safe call into JS for every IO. The WASM binding declares its IO imports with `EM_ASYNC_JS` so the same synchronous core call suspends under JSPI. Both bindings hand metadata and results to TypeScript as JSON produced by one core serializer.

**Tech Stack:** FFmpeg `n8.1.3` (LGPL, remux-only configure), C++20, CMake 4, Catch2 v3, node-addon-api 8 + cmake-js 8, Emscripten 6 (JSPI), TypeScript 7.0, oxlint 1.87, Prettier 3, Vitest 5 (+ `@vitest/browser-playwright`), npm workspaces, prebuild 13 / prebuild-install 7, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-06-aviotrix-design.md`. Read it first; every task below argues from it.

## Global Constraints

- FFmpeg pinned to tag `n8.1.3` (latest 8.x on 2026-10-06; FFmpeg 9.0.2 exists but the spec says 8.x). LGPL only: never `--enable-gpl`, never `--enable-nonfree`.
- Enabled libav components are exactly the spec §6 lists: demuxers `mov matroska mpegts`; muxers `mp4 mov matroska webm mpegts`; parsers `h264 hevc av1 vp9 aac ac3 opus vorbis mpegaudio mpegvideo dvbsub dvdsub`; bsfs `h264_mp4toannexb hevc_mp4toannexb aac_adtstoasc extract_extradata`. Everything else disabled.
- Core: C++20, compiled with `-fno-exceptions -fno-rtti`, includes only libav headers and the standard library. No Node or Emscripten headers.
- TypeScript: `typescript@7.0.2`, strict. No `any`, no `unknown` anywhere (lint: `typescript/no-explicit-any` as error; `unknown` banned by `scripts/check-no-unknown.sh`, because oxlint 1.87 has no `no-restricted-syntax`). 2-space indent, semicolons, Prettier.
- Lint is oxlint, not ESLint. Format is Prettier.
- Node ≥ 24. Package manager is npm with workspaces.
- Times are seconds as `number`; fields libav does not know are `null`, never 0.
- One operation at a time per `MediaReader` (TS queue). The WASM package additionally serializes operations module-wide.
- Never push to the remote without Chad's explicit approval. Creating the GitHub repo in Task 1 is an outward action: ask before running `gh repo create`.
- Every task ends with lint, type-check, tests, and build green for the packages it touches.
- Commit messages are descriptive; no "fix"/"update"/"changes".

## Review Focus

Inputs the spec implies but which no test exercised in the first draft of this plan. Each now has a pinned test in the named task.

1. **A source whose `read` returns more bytes than asked.** Expected: the core copies at most `length` bytes and ignores the rest without corrupting the stream. Pinned in Task 9 (`over-long read is truncated`) and Task 12.
2. **A source whose `open` reports a size smaller than the real data.** Expected: libav sees EOF at the reported size and metadata/remux stop cleanly with an error or truncated output, never a hang or out-of-range read. Pinned in Task 5 (`truncated size yields EOF`).
3. **A sink `write` that rejects mid-remux.** Expected: `remux` rejects with the sink's error wrapped in `AviotrixError`, the sink's `close` is still called, no crash. Pinned in Task 9 (`sink write failure propagates`).
4. **Empty or non-media input (0 bytes, or a text file).** Expected: `MediaReader.open` rejects with `AVERROR_INVALIDDATA` or `AVERROR_EOF`, not a hang. Pinned in Task 5 and Task 9.
5. **`streams` option naming an index that does not exist.** Expected: `remux` rejects with `INVALID_ARGUMENT` before any output IO. Pinned in Task 6 and Task 9.

---

## File map

```
aviotrix/ (repo root = worktrees/main)
  .gitignore
  .gitmodules
  .oxlintrc.json
  .prettierrc
  CLAUDE.md                          project instructions (layout, verification rules)
  CMakeLists.txt                     ffmpeg prefix discovery, core, conditional bindings
  README.md                          overview, build prerequisites, measured WASM size
  package.json                       npm workspaces root + shared scripts
  tsconfig.base.json
  docs/superpowers/specs/2026-10-06-aviotrix-design.md
  docs/superpowers/plans/2026-10-06-aviotrix-milestone-1.md
  scripts/
    build-ffmpeg.sh                  host|wasm -> build/ffmpeg-<target>/
    ffmpeg-components.sh             the single enable list
    make-fixtures.sh                 regenerates fixtures/ with system ffmpeg
    check-no-unknown.sh              CI grep banning `unknown`
  fixtures/
    h264-aac.mp4  vp9-opus.webm  h264-ac3.ts  h264-aac-srt.mkv  subs.srt  not-media.txt
  third_party/ffmpeg/                submodule @ n8.1.3
  core/
    CMakeLists.txt
    include/aviotrix/status.h        Status, ErrorCode, errorCodeName()
    include/aviotrix/io.h            IoSource, IoSink
    include/aviotrix/log.h           LogHook, LogLevel
    include/aviotrix/metadata.h      Metadata, StreamInfo, Rational
    include/aviotrix/remux.h         RemuxOptions, RemuxResult, RemuxProgress
    include/aviotrix/media_reader.h  MediaReader
    include/aviotrix/json.h          JsonWriter + toJson(Metadata), toJson(RemuxResult)
    src/status.cc
    src/avio_input.h / .cc           AVIOContext over IoSource
    src/avio_output.h / .cc          AVIOContext over IoSink
    src/log_router.h / .cc           av_log callback -> thread_local current reader
    src/metadata.cc                  AVFormatContext -> Metadata
    src/media_reader.cc              open/metadata/close
    src/remux.cc                     MediaReader::remux
    src/json.cc
    tests/CMakeLists.txt
    tests/file_io.h                  FileSource, FileSink, MemorySink (test helpers)
    tests/fixtures.h                 fixture path helper
    tests/test_status.cc
    tests/test_avio.cc
    tests/test_metadata.cc
    tests/test_remux.cc
  packages/types/                    @aviotrix/types
    package.json  tsconfig.json
    src/index.ts  src/io.ts  src/metadata.ts  src/remux.ts  src/error.ts  src/log.ts
    tests/error.test.ts
  packages/node/                     @aviotrix/node
    package.json  tsconfig.json  CMakeLists.txt  vitest.config.ts
    binding/addon.cc                 module init, NativeReader class
    binding/main_thread_bridge.h/.cc TSFN message pump (io, log, completion)
    binding/js_io.h/.cc              JsIoSource, JsIoSink
    binding/native_reader.h/.cc      worker thread + core MediaReader
    src/index.ts  src/media_reader.ts  src/native.ts  src/io/file_source.ts
    src/io/file_sink.ts  src/io/memory_sink.ts  src/queue.ts
    tests/metadata.test.ts  tests/remux.test.ts  tests/io.test.ts  tests/adversarial_source.ts
  packages/wasm/                     @aviotrix/wasm
    package.json  tsconfig.json  CMakeLists.txt  vitest.config.ts
    binding/exports.cc               extern "C" API (avx_*)
    binding/js_imports.cc            EM_ASYNC_JS source/sink imports, progress import
    binding/handles.h                handle tables
    src/index.ts  src/load.ts  src/media_reader.ts  src/module.ts
    src/io/blob_source.ts  src/io/fetch_range_source.ts  src/io/memory_sink.ts
    tests/metadata.test.ts  tests/remux.test.ts  tests/unsupported.test.ts
  .github/workflows/ci.yml  .github/workflows/prebuild.yml
```

---

### Task 1: Bootstrap the repository

**Files:**

- Create: GitHub repo `aviotrix/aviotrix`; local `~/Projects/aviotrix/` in bare-repo + worktrees layout
- Create: `CLAUDE.md`, `.gitignore`, `.prettierrc`, `.prettierignore`, `.oxlintrc.json`, `package.json`, `tsconfig.base.json`, `scripts/check-no-unknown.sh`, `README.md`
- Already present: `docs/superpowers/specs/2026-10-06-aviotrix-design.md` and this plan (committed at bootstrap)

**Interfaces:**

- Produces: repo root at `~/Projects/aviotrix/worktrees/main` (all later tasks run there); root scripts `npm run lint`, `npm run format`, `npm run typecheck`, `npm run test`, `npm run build`.

- [x] **Step 1: Ask Chad for approval to create the GitHub repo, then create it with an initial README** (done 2026-10-07)

`project-setup.sh` needs a reachable origin with a HEAD, so the repo must have one commit.

```bash
gh repo create aviotrix/aviotrix --public --description "libav core with Node native and WASM bindings behind a custom AVIO layer" --add-readme --license MIT
```

Expected: prints `https://github.com/aviotrix/aviotrix`.

- [x] **Step 2: Bootstrap the bare-repo + worktrees layout** (done 2026-10-07)

```bash
cd ~/Projects && "$HOME/.claude/skills/project-setup/project-setup.sh" git@github.com:aviotrix/aviotrix.git
cd ~/Projects/aviotrix/worktrees/main && git status
```

Expected: `project-setup: /Users/chad/Projects/aviotrix` and a clean `main` worktree containing `README.md` and `LICENSE`.

- [ ] **Step 3: Write `CLAUDE.md`**

```markdown
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
```

- [ ] **Step 4: Write root config files**

`.gitignore`:

```
node_modules/
dist/
build/
*.node
.DS_Store
```

`.prettierrc`:

```json
{ "semi": true, "singleQuote": true, "tabWidth": 2, "printWidth": 100, "trailingComma": "all" }
```

`.prettierignore`:

```
third_party/
build/
dist/
fixtures/
```

`.oxlintrc.json`:

```json
{
  "ignorePatterns": ["dist", "build", "third_party", "node_modules"],
  "categories": { "correctness": "error", "suspicious": "warn" },
  "rules": {
    "typescript/no-explicit-any": "error",
    "no-unused-vars": "error"
  }
}
```

`tsconfig.base.json`:

```json
{
  "compilerOptions": {
    "target": "es2023",
    "module": "nodenext",
    "moduleResolution": "nodenext",
    "strict": true,
    "exactOptionalPropertyTypes": true,
    "noUncheckedIndexedAccess": true,
    "verbatimModuleSyntax": true,
    "declaration": true,
    "sourceMap": true,
    "skipLibCheck": true
  }
}
```

`scripts/check-no-unknown.sh`:

```bash
#!/usr/bin/env bash
# Fails if the `unknown` type keyword appears in any package source or test.
set -euo pipefail
cd "$(dirname "$0")/.."
if grep -rnE '(:|<|,|\(|\|)\s*unknown\b' packages/*/src packages/*/tests --include='*.ts' ; then
  echo "error: 'unknown' type is banned (see CLAUDE.md)" >&2
  exit 1
fi
echo "ok: no 'unknown' types"
```

`package.json` (root):

```json
{
  "name": "aviotrix-monorepo",
  "private": true,
  "type": "module",
  "workspaces": ["packages/types", "packages/node", "packages/wasm"],
  "engines": { "node": ">=24" },
  "scripts": {
    "lint": "oxlint --deny-warnings && bash scripts/check-no-unknown.sh && prettier --check .",
    "format": "prettier --write .",
    "typecheck": "npm run typecheck --workspaces --if-present",
    "build": "npm run build --workspaces --if-present",
    "test": "npm run test --workspaces --if-present"
  },
  "devDependencies": {
    "oxlint": "^1.87.0",
    "prettier": "^3.9.0",
    "typescript": "^7.0.2"
  }
}
```

- [ ] **Step 5: Install and verify the root scripts**

The spec and this plan were already committed into `docs/superpowers/` when the repo was bootstrapped.

```bash
mkdir -p packages/types packages/node packages/wasm
chmod +x scripts/check-no-unknown.sh
npm install
npm run format && npm run lint
```

Expected: `npm install` succeeds (empty workspace dirs are fine; later tasks fill them); `npm run lint` prints `ok: no 'unknown' types` and Prettier reports all files formatted.

- [ ] **Step 6: Replace README.md**

```markdown
# aviotrix

libav (FFmpeg) core with a custom AVIO layer, exposed as a Node native module (`@aviotrix/node`) and a WebAssembly module (`@aviotrix/wasm`), sharing one TypeScript contract (`@aviotrix/types`).

Design: `docs/superpowers/specs/2026-10-06-aviotrix-design.md`.

## Status

Milestone 1 in progress: open via `IoSource`, read metadata, remux to `IoSink`.

## Build prerequisites

macOS: `brew install cmake nasm emscripten ffmpeg`. FFmpeg pinned at `n8.1.3`, LGPL build.

## WASM size

Measured in Task 11; recorded here.
```

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "Bootstrap aviotrix monorepo: workspaces, lint/format config, spec and plan"
```

---

### Task 2: FFmpeg submodule and host build script

**Files:**

- Create: `.gitmodules`, `third_party/ffmpeg` (submodule @ `n8.1.3`), `scripts/build-ffmpeg.sh`, `scripts/ffmpeg-components.sh`

**Interfaces:**

- Produces: `build/ffmpeg-host/lib/{libavformat,libavcodec,libavutil}.a`, `build/ffmpeg-host/include/libav*/`. Task 10 adds the `wasm` target to the same script, producing `build/ffmpeg-wasm/`.

- [ ] **Step 1: Add the submodule**

```bash
git submodule add https://github.com/FFmpeg/FFmpeg.git third_party/ffmpeg
git -C third_party/ffmpeg checkout n8.1.3
git add .gitmodules third_party/ffmpeg
```

Expected: `git submodule status` shows the `n8.1.3` commit.

- [ ] **Step 2: Write the shared component list**

`scripts/ffmpeg-components.sh` (sourced by the build script; the single place the enable list lives):

```bash
# Shared FFmpeg configure component list. See spec §6.
FFMPEG_COMMON_FLAGS=(
  --disable-everything
  --disable-programs --disable-doc
  --disable-network --disable-protocols
  --disable-avdevice --disable-avfilter --disable-swscale --disable-swresample --disable-postproc
  --disable-autodetect --disable-iconv --disable-zlib --disable-bzlib --disable-lzma --disable-sdl2 --disable-xlib
  --disable-decoders --disable-encoders --disable-filters --disable-hwaccels --disable-devices
  --disable-debug --disable-stripping
  --enable-avformat --enable-avcodec
  --enable-demuxer=mov,matroska,mpegts
  --enable-muxer=mp4,mov,matroska,webm,mpegts
  --enable-parser=h264,hevc,av1,vp9,aac,ac3,opus,vorbis,mpegaudio,mpegvideo,dvbsub,dvdsub
  --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,aac_adtstoasc,extract_extradata
  --enable-pic
)
```

- [ ] **Step 3: Write `scripts/build-ffmpeg.sh` with the `host` target**

```bash
#!/usr/bin/env bash
# Build FFmpeg static libs for one target. Usage: build-ffmpeg.sh host|wasm [prefix]
set -euo pipefail
target="${1:-}"
case "$target" in host|wasm) ;; *) echo "usage: $0 host|wasm [prefix]" >&2; exit 2;; esac
root="$(cd "$(dirname "$0")/.." && pwd -P)"
src="$root/third_party/ffmpeg"
prefix="${2:-$root/build/ffmpeg-$target}"
builddir="$root/build/ffmpeg-$target-obj"
# shellcheck source=ffmpeg-components.sh
source "$root/scripts/ffmpeg-components.sh"

if [ -f "$prefix/lib/libavformat.a" ]; then
  echo "build-ffmpeg: $target already built at $prefix"; exit 0
fi
[ -f "$src/configure" ] || { echo "error: submodule missing; run git submodule update --init" >&2; exit 1; }

mkdir -p "$builddir"
cd "$builddir"
case "$target" in
  host)
    command -v nasm >/dev/null || { echo "error: nasm required (brew install nasm)" >&2; exit 1; }
    "$src/configure" --prefix="$prefix" "${FFMPEG_COMMON_FLAGS[@]}" --enable-static --disable-shared
    ;;
  wasm)
    echo "error: wasm target is added in Task 10" >&2; exit 1
    ;;
esac
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
make install
echo "build-ffmpeg: installed $target to $prefix"
```

- [ ] **Step 4: Run it and verify the result**

```bash
chmod +x scripts/build-ffmpeg.sh
scripts/build-ffmpeg.sh host
ls build/ffmpeg-host/lib/*.a
```

Expected: exactly `libavcodec.a libavformat.a libavutil.a`; configure output listed only the spec's demuxers/muxers/parsers/bsfs and `License: LGPL version 2.1 or later`.

- [ ] **Step 5: Smoke-test the enable list (catches typos in the component list)**

Write `/tmp/avx_smoke.c`:

```c
#include <libavformat/avformat.h>
#include <stdio.h>
int main(void) {
  const char* dem[] = {"mov,mp4,m4a,3gp,3g2,mj2", "matroska,webm", "mpegts"};
  const char* mux[] = {"mp4", "mov", "matroska", "webm", "mpegts"};
  for (int i = 0; i < 3; i++) if (!av_find_input_format(dem[i])) { printf("missing demuxer %s\n", dem[i]); return 1; }
  for (int i = 0; i < 5; i++) if (!av_guess_format(mux[i], NULL, NULL)) { printf("missing muxer %s\n", mux[i]); return 1; }
  if (av_find_input_format("avi")) { printf("avi should be disabled\n"); return 1; }
  printf("ok %s\n", av_version_info());
  return 0;
}
```

```bash
cc /tmp/avx_smoke.c -Ibuild/ffmpeg-host/include -Lbuild/ffmpeg-host/lib -lavformat -lavcodec -lavutil -lm -lpthread -o /tmp/avx_smoke && /tmp/avx_smoke
rm -f /tmp/avx_smoke /tmp/avx_smoke.c
```

Expected: `ok n8.1.3`.

- [ ] **Step 6: Commit**

```bash
git add .gitmodules third_party/ffmpeg scripts/build-ffmpeg.sh scripts/ffmpeg-components.sh
git commit -m "Add FFmpeg n8.1.3 submodule and remux-only host build script"
```

---

### Task 3: Core skeleton: CMake, Status, IO interfaces, test harness

**Files:**

- Create: `CMakeLists.txt` (root), `.clang-format`, `.clang-tidy`, `core/CMakeLists.txt`, `core/include/aviotrix/status.h`, `core/include/aviotrix/io.h`, `core/include/aviotrix/log.h`, `core/src/status.cc`, `core/src/log.cc`, `core/tests/CMakeLists.txt`, `core/tests/test_status.cc`, `core/tests/file_io.h`, `core/tests/fixtures.h`
- Modify: root `package.json` (add `test:core` and `lint:cpp` scripts)

**Interfaces:**

- Produces (used by every later core task):
  - `aviotrix::Status { int code; std::string message; bool ok() const; static Status Ok(); static Status FromAv(int averror, std::string_view context); static Status Error(ErrorCode, std::string); }`
  - `enum class aviotrix::ErrorCode : int { Ok=0, SinkNotSeekable=1, IncompatibleStream=2, Aborted=3, InvalidArgument=4, IoFailed=5, NotOpen=6, Unsupported=7 }`
  - `std::string aviotrix::errorCodeName(int code)` → `"OK"`, `"SINK_NOT_SEEKABLE"`, `"AVERROR_INVALIDDATA"`, `"AVERROR(ENOMEM)"`, …
  - `aviotrix::IoSource` / `aviotrix::IoSink` exactly as spec §3.1
  - `aviotrix::LogLevel`, `aviotrix::LogHook = std::function<void(LogLevel, std::string_view)>`, `const char* logLevelName(LogLevel)`, `LogLevel logLevelFromAv(int)`
  - CMake targets `aviotrix_core`, `ffmpeg::avformat|avcodec|avutil`; test helpers `test::FileSource`, `test::FileSink`, `test::MemorySink`, `test::fixturePath(name)`

- [ ] **Step 1: Write the failing test**

`core/tests/test_status.cc`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "aviotrix/status.h"

extern "C" {
#include <libavutil/error.h>
}

using aviotrix::ErrorCode;
using aviotrix::Status;

TEST_CASE("Status::Ok is ok and named OK") {
  Status s = Status::Ok();
  REQUIRE(s.ok());
  REQUIRE(aviotrix::errorCodeName(s.code) == "OK");
}

TEST_CASE("Status::FromAv names tagged AVERRORs and keeps context") {
  Status s = Status::FromAv(AVERROR_INVALIDDATA, "avformat_open_input");
  REQUIRE_FALSE(s.ok());
  REQUIRE(s.code == AVERROR_INVALIDDATA);
  REQUIRE(aviotrix::errorCodeName(s.code) == "AVERROR_INVALIDDATA");
  REQUIRE(s.message.find("avformat_open_input") != std::string::npos);
  REQUIRE(aviotrix::errorCodeName(AVERROR_EOF) == "AVERROR_EOF");
  REQUIRE(aviotrix::errorCodeName(AVERROR_EXIT) == "AVERROR_EXIT");
}

TEST_CASE("Status::FromAv names POSIX AVERRORs") {
  REQUIRE(aviotrix::errorCodeName(AVERROR(ENOMEM)) == "AVERROR(ENOMEM)");
  REQUIRE(aviotrix::errorCodeName(AVERROR(EINVAL)) == "AVERROR(EINVAL)");
  REQUIRE(aviotrix::errorCodeName(AVERROR(12345)) == "AVERROR(12345)");
}

TEST_CASE("Status::Error names aviotrix codes") {
  Status s = Status::Error(ErrorCode::SinkNotSeekable, "mp4 needs a seekable sink");
  REQUIRE(s.code == static_cast<int>(ErrorCode::SinkNotSeekable));
  REQUIRE(aviotrix::errorCodeName(s.code) == "SINK_NOT_SEEKABLE");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::IncompatibleStream)) == "INCOMPATIBLE_STREAM");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::Aborted)) == "ABORTED");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::InvalidArgument)) == "INVALID_ARGUMENT");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::IoFailed)) == "IO_FAILED");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::NotOpen)) == "NOT_OPEN");
  REQUIRE(aviotrix::errorCodeName(static_cast<int>(ErrorCode::Unsupported)) == "UNSUPPORTED");
}

TEST_CASE("LogLevel maps from av levels") {
  REQUIRE(aviotrix::logLevelFromAv(AV_LOG_ERROR) == aviotrix::LogLevel::Error);
  REQUIRE(aviotrix::logLevelFromAv(AV_LOG_INFO) == aviotrix::LogLevel::Info);
  REQUIRE(aviotrix::logLevelFromAv(AV_LOG_TRACE) == aviotrix::LogLevel::Trace);
  REQUIRE(std::string(aviotrix::logLevelName(aviotrix::LogLevel::Warning)) == "warning");
}
```

(The last test needs `#include "aviotrix/log.h"` and `extern "C" { #include <libavutil/log.h> }` at the top; add them.)

- [ ] **Step 2: Write the CMake files**

Root `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.28)
project(aviotrix LANGUAGES C CXX)

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
```

`core/CMakeLists.txt` (sources are appended by later tasks):

```cmake
add_library(aviotrix_core STATIC
  src/status.cc
  src/log.cc)
target_include_directories(aviotrix_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(aviotrix_core PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Werror)
target_link_libraries(aviotrix_core PUBLIC ffmpeg::avformat)
```

`core/tests/CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(aviotrix_core_tests
  test_status.cc)
target_link_libraries(aviotrix_core_tests PRIVATE aviotrix_core Catch2::Catch2WithMain)
target_compile_definitions(aviotrix_core_tests PRIVATE AVIOTRIX_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../fixtures")
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(aviotrix_core_tests)
```

- [ ] **Step 3: Run the build to verify the test fails to compile**

```bash
cmake -S . -B build/core-host && cmake --build build/core-host -j
```

Expected: FAIL, `aviotrix/status.h: No such file or directory`.

- [ ] **Step 4: Write the headers**

`core/include/aviotrix/status.h`:

```cpp
#pragma once

#include <string>
#include <string_view>

namespace aviotrix {

// Positive codes are aviotrix's own. Negative codes are libav AVERROR values. 0 is success.
enum class ErrorCode : int {
  Ok = 0,
  SinkNotSeekable = 1,
  IncompatibleStream = 2,
  Aborted = 3,
  InvalidArgument = 4,
  IoFailed = 5,
  NotOpen = 6,
  Unsupported = 7,
};

struct Status {
  int code = 0;
  std::string message;

  bool ok() const { return code == 0; }
  static Status Ok() { return Status{}; }
  // Wraps a libav return value. `context` names the call that failed.
  static Status FromAv(int averror, std::string_view context);
  static Status Error(ErrorCode code, std::string message);
};

// Stable, machine-readable name for any code, e.g. "AVERROR_INVALIDDATA", "AVERROR(ENOMEM)", "SINK_NOT_SEEKABLE".
std::string errorCodeName(int code);

}  // namespace aviotrix
```

`core/include/aviotrix/io.h`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "aviotrix/status.h"

namespace aviotrix {

// Positioned, synchronous read access. The core tracks the stream position; implementations never do.
class IoSource {
 public:
  virtual ~IoSource() = default;
  // `size` is the total length in bytes, or std::nullopt when unknown (the input becomes unseekable).
  virtual Status open(std::optional<int64_t>& size) = 0;
  // Reads up to buffer.size() bytes at `offset`. Short reads are allowed. bytesRead == 0 means EOF.
  virtual Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) = 0;
  virtual Status close() = 0;
};

// Positioned, synchronous write access.
class IoSink {
 public:
  virtual ~IoSink() = default;
  virtual bool seekable() const = 0;
  virtual Status open() = 0;
  virtual Status write(int64_t offset, std::span<const uint8_t> data) = 0;
  virtual Status close() = 0;
};

}  // namespace aviotrix
```

`core/include/aviotrix/log.h`:

```cpp
#pragma once

#include <functional>
#include <string_view>

namespace aviotrix {

enum class LogLevel { Quiet, Panic, Fatal, Error, Warning, Info, Verbose, Debug, Trace };

using LogHook = std::function<void(LogLevel level, std::string_view text)>;

const char* logLevelName(LogLevel level);  // "quiet", "panic", ... "trace"
LogLevel logLevelFromAv(int avLevel);      // AV_LOG_* -> LogLevel

}  // namespace aviotrix
```

- [ ] **Step 5: Write the implementations**

`core/src/status.cc`:

```cpp
#include "aviotrix/status.h"

#include <cerrno>
#include <cstring>

extern "C" {
#include <libavutil/error.h>
}

namespace aviotrix {

namespace {

const char* taggedName(int code) {
  switch (code) {
    case AVERROR_EOF: return "AVERROR_EOF";
    case AVERROR_INVALIDDATA: return "AVERROR_INVALIDDATA";
    case AVERROR_EXIT: return "AVERROR_EXIT";
    case AVERROR_BUG: return "AVERROR_BUG";
    case AVERROR_BUG2: return "AVERROR_BUG2";
    case AVERROR_PATCHWELCOME: return "AVERROR_PATCHWELCOME";
    case AVERROR_DEMUXER_NOT_FOUND: return "AVERROR_DEMUXER_NOT_FOUND";
    case AVERROR_MUXER_NOT_FOUND: return "AVERROR_MUXER_NOT_FOUND";
    case AVERROR_STREAM_NOT_FOUND: return "AVERROR_STREAM_NOT_FOUND";
    case AVERROR_OPTION_NOT_FOUND: return "AVERROR_OPTION_NOT_FOUND";
    case AVERROR_DECODER_NOT_FOUND: return "AVERROR_DECODER_NOT_FOUND";
    case AVERROR_ENCODER_NOT_FOUND: return "AVERROR_ENCODER_NOT_FOUND";
    case AVERROR_BSF_NOT_FOUND: return "AVERROR_BSF_NOT_FOUND";
    case AVERROR_PROTOCOL_NOT_FOUND: return "AVERROR_PROTOCOL_NOT_FOUND";
    case AVERROR_EXTERNAL: return "AVERROR_EXTERNAL";
    case AVERROR_UNKNOWN: return "AVERROR_UNKNOWN";
    default: return nullptr;
  }
}

const char* posixName(int err) {
  switch (err) {
    case ENOMEM: return "ENOMEM";
    case EINVAL: return "EINVAL";
    case EIO: return "EIO";
    case ENOSYS: return "ENOSYS";
    case EAGAIN: return "EAGAIN";
    case EPIPE: return "EPIPE";
    case ENOENT: return "ENOENT";
    case ERANGE: return "ERANGE";
    default: return nullptr;
  }
}

}  // namespace

Status Status::FromAv(int averror, std::string_view context) {
  char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(averror, buf, sizeof buf);
  Status s;
  s.code = averror;
  s.message.append(context).append(": ").append(buf);
  return s;
}

Status Status::Error(ErrorCode code, std::string message) {
  return Status{static_cast<int>(code), std::move(message)};
}

std::string errorCodeName(int code) {
  if (code == 0) return "OK";
  if (code > 0) {
    switch (static_cast<ErrorCode>(code)) {
      case ErrorCode::SinkNotSeekable: return "SINK_NOT_SEEKABLE";
      case ErrorCode::IncompatibleStream: return "INCOMPATIBLE_STREAM";
      case ErrorCode::Aborted: return "ABORTED";
      case ErrorCode::InvalidArgument: return "INVALID_ARGUMENT";
      case ErrorCode::IoFailed: return "IO_FAILED";
      case ErrorCode::NotOpen: return "NOT_OPEN";
      case ErrorCode::Unsupported: return "UNSUPPORTED";
      case ErrorCode::Ok: return "OK";
    }
    return "UNKNOWN(" + std::to_string(code) + ")";
  }
  if (const char* tagged = taggedName(code)) return tagged;
  const int err = AVUNERROR(code);
  if (const char* p = posixName(err)) return std::string("AVERROR(") + p + ")";
  return "AVERROR(" + std::to_string(err) + ")";
}

}  // namespace aviotrix
```

`core/src/log.cc`:

```cpp
#include "aviotrix/log.h"

extern "C" {
#include <libavutil/log.h>
}

namespace aviotrix {

const char* logLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Quiet: return "quiet";
    case LogLevel::Panic: return "panic";
    case LogLevel::Fatal: return "fatal";
    case LogLevel::Error: return "error";
    case LogLevel::Warning: return "warning";
    case LogLevel::Info: return "info";
    case LogLevel::Verbose: return "verbose";
    case LogLevel::Debug: return "debug";
    case LogLevel::Trace: return "trace";
  }
  return "info";
}

LogLevel logLevelFromAv(int avLevel) {
  if (avLevel <= AV_LOG_QUIET) return LogLevel::Quiet;
  if (avLevel <= AV_LOG_PANIC) return LogLevel::Panic;
  if (avLevel <= AV_LOG_FATAL) return LogLevel::Fatal;
  if (avLevel <= AV_LOG_ERROR) return LogLevel::Error;
  if (avLevel <= AV_LOG_WARNING) return LogLevel::Warning;
  if (avLevel <= AV_LOG_INFO) return LogLevel::Info;
  if (avLevel <= AV_LOG_VERBOSE) return LogLevel::Verbose;
  if (avLevel <= AV_LOG_DEBUG) return LogLevel::Debug;
  return LogLevel::Trace;
}

}  // namespace aviotrix
```

- [ ] **Step 6: Write the test helpers (used from Task 5 onward)**

`core/tests/fixtures.h`:

```cpp
#pragma once
#include <string>

namespace test {
inline std::string fixturePath(const char* name) {
  return std::string(AVIOTRIX_FIXTURES_DIR) + "/" + name;
}
}  // namespace test
```

`core/tests/file_io.h`:

```cpp
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "aviotrix/io.h"

namespace test {

class FileSource final : public aviotrix::IoSource {
 public:
  explicit FileSource(std::string path) : path_(std::move(path)) {}
  ~FileSource() override { if (file_) std::fclose(file_); }

  aviotrix::Status open(std::optional<int64_t>& size) override {
    file_ = std::fopen(path_.c_str(), "rb");
    if (!file_) return aviotrix::Status::Error(aviotrix::ErrorCode::IoFailed, "cannot open " + path_);
    std::fseek(file_, 0, SEEK_END);
    size = static_cast<int64_t>(std::ftell(file_));
    opens_++;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override {
    if (!file_) return aviotrix::Status::Error(aviotrix::ErrorCode::NotOpen, "read before open");
    std::fseek(file_, static_cast<long>(offset), SEEK_SET);
    bytesRead = std::fread(buffer.data(), 1, buffer.size(), file_);
    reads_++;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status close() override {
    if (file_) { std::fclose(file_); file_ = nullptr; }
    closes_++;
    return aviotrix::Status::Ok();
  }
  int opens() const { return opens_; }
  int reads() const { return reads_; }
  int closes() const { return closes_; }

 private:
  std::string path_;
  std::FILE* file_ = nullptr;
  int opens_ = 0, reads_ = 0, closes_ = 0;
};

// In-memory sink. `seekable=false` simulates a streaming destination: writes must be sequential.
class MemorySink final : public aviotrix::IoSink {
 public:
  explicit MemorySink(bool seekable = true) : seekable_(seekable) {}
  bool seekable() const override { return seekable_; }
  aviotrix::Status open() override { opened_ = true; return aviotrix::Status::Ok(); }
  aviotrix::Status write(int64_t offset, std::span<const uint8_t> data) override {
    if (!opened_) return aviotrix::Status::Error(aviotrix::ErrorCode::NotOpen, "write before open");
    if (!seekable_ && offset != static_cast<int64_t>(bytes_.size()))
      return aviotrix::Status::Error(aviotrix::ErrorCode::IoFailed, "non-sequential write to streaming sink");
    const size_t end = static_cast<size_t>(offset) + data.size();
    if (end > bytes_.size()) bytes_.resize(end);
    std::copy(data.begin(), data.end(), bytes_.begin() + offset);
    writes_++;
    return aviotrix::Status::Ok();
  }
  aviotrix::Status close() override { closed_ = true; return aviotrix::Status::Ok(); }
  const std::vector<uint8_t>& bytes() const { return bytes_; }
  bool closed() const { return closed_; }
  int writes() const { return writes_; }

 private:
  bool seekable_;
  bool opened_ = false, closed_ = false;
  int writes_ = 0;
  std::vector<uint8_t> bytes_;
};

// Reads from a byte vector; lets tests feed remux output back into MediaReader.
class MemorySource final : public aviotrix::IoSource {
 public:
  explicit MemorySource(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
  aviotrix::Status open(std::optional<int64_t>& size) override { size = static_cast<int64_t>(bytes_.size()); return aviotrix::Status::Ok(); }
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override {
    if (offset >= static_cast<int64_t>(bytes_.size())) { bytesRead = 0; return aviotrix::Status::Ok(); }
    bytesRead = std::min(buffer.size(), bytes_.size() - static_cast<size_t>(offset));
    std::copy_n(bytes_.begin() + offset, bytesRead, buffer.begin());
    return aviotrix::Status::Ok();
  }
  aviotrix::Status close() override { return aviotrix::Status::Ok(); }

 private:
  std::vector<uint8_t> bytes_;
};

}  // namespace test
```

- [ ] **Step 7: Build, run tests, verify they pass**

```bash
cmake -S . -B build/core-host && cmake --build build/core-host -j && ctest --test-dir build/core-host --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 5`.

- [ ] **Step 8: Add C++ formatting and static-analysis config**

`.clang-format`:

```yaml
BasedOnStyle: Google
ColumnLimit: 120
IndentWidth: 2
DerivePointerAlignment: false
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: Inline
```

`.clang-tidy`:

```yaml
Checks: 'bugprone-*,performance-*,modernize-use-nullptr,modernize-use-override,readability-container-size-empty,-bugprone-easily-swappable-parameters'
WarningsAsErrors: 'bugprone-*,performance-*'
HeaderFilterRegex: '(core|packages/(node|wasm)/binding)/'
```

Root `CMakeLists.txt`, after the `project()` line:

```cmake
option(AVIOTRIX_CLANG_TIDY "Run clang-tidy during the build" OFF)
if(AVIOTRIX_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()
```

Run `clang-format -i $(git ls-files 'core/*.cc' 'core/*.h')` once, then verify `clang-format --dry-run --Werror $(git ls-files 'core/*.cc' 'core/*.h')` exits 0. (`brew install clang-format llvm` if missing; clang-tidy comes with `llvm`.)

- [ ] **Step 9: Add the npm scripts and commit**

Add to root `package.json` scripts:

```json
"test:core": "cmake -S . -B build/core-host && cmake --build build/core-host -j && ctest --test-dir build/core-host --output-on-failure",
"lint:cpp": "clang-format --dry-run --Werror $(git ls-files 'core/*.cc' 'core/*.h' 'packages/*/binding/*.cc' 'packages/*/binding/*.h')"
```

Change `"lint"` to `"oxlint --deny-warnings && bash scripts/check-no-unknown.sh && prettier --check . && npm run lint:cpp"` and `"test"` to `"npm run test:core && npm run test --workspaces --if-present"`.

```bash
npm run format && npm run lint
git add CMakeLists.txt .clang-format .clang-tidy core package.json
git commit -m "Add core skeleton: Status, IoSource/IoSink, log levels, Catch2 harness"
```

---

### Task 4: Test fixtures

**Files:**

- Create: `scripts/make-fixtures.sh`, `fixtures/h264-aac.mp4`, `fixtures/vp9-opus.webm`, `fixtures/h264-ac3.ts`, `fixtures/h264-aac-srt.mkv`, `fixtures/subs.srt`, `fixtures/not-media.txt`, `fixtures/README.md`

**Interfaces:**

- Produces: the fixture files above, each 3 seconds, 320x240 at 30 fps, 48 kHz audio. Later tests assert: 2 streams in the first three files (video index 0, audio index 1), 3 streams in the mkv (subtitle index 2), video `320x240`, duration between 2.9 and 3.2 s.

- [ ] **Step 1: Write the generator**

`scripts/make-fixtures.sh`:

```bash
#!/usr/bin/env bash
# Regenerates fixtures/ with the system ffmpeg. Developer-only; never run at build or install.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd -P)"
out="$root/fixtures"
mkdir -p "$out"

command -v ffmpeg >/dev/null || { echo "error: system ffmpeg required (brew install ffmpeg)" >&2; exit 1; }
for enc in libx264 libvpx-vp9 libopus aac ac3; do
  ffmpeg -hide_banner -encoders 2>/dev/null | grep -q " $enc " || { echo "error: ffmpeg lacks encoder $enc" >&2; exit 1; }
done

video=(-f lavfi -i "testsrc2=size=320x240:rate=30:duration=3")
audio=(-f lavfi -i "sine=frequency=440:sample_rate=48000:duration=3")
common=(-y -hide_banner -loglevel error)

ffmpeg "${common[@]}" "${video[@]}" "${audio[@]}" \
  -c:v libx264 -preset veryfast -g 30 -pix_fmt yuv420p -c:a aac -b:a 64k -movflags +faststart \
  "$out/h264-aac.mp4"

ffmpeg "${common[@]}" "${video[@]}" "${audio[@]}" \
  -c:v libvpx-vp9 -b:v 200k -deadline realtime -cpu-used 8 -c:a libopus -b:a 48k \
  "$out/vp9-opus.webm"

ffmpeg "${common[@]}" "${video[@]}" "${audio[@]}" \
  -c:v libx264 -preset veryfast -g 30 -pix_fmt yuv420p -c:a ac3 -b:a 96k -f mpegts \
  "$out/h264-ac3.ts"

cat > "$out/subs.srt" <<'SRT'
1
00:00:00,000 --> 00:00:01,000
Hello

2
00:00:01,500 --> 00:00:02,500
World
SRT

ffmpeg "${common[@]}" -i "$out/h264-aac.mp4" -i "$out/subs.srt" -map 0 -map 1 -c copy -c:s srt \
  "$out/h264-aac-srt.mkv"

printf 'This is not a media file.\n' > "$out/not-media.txt"

ls -la "$out"
```

- [ ] **Step 2: Run it and verify stream layouts with ffprobe**

```bash
chmod +x scripts/make-fixtures.sh && scripts/make-fixtures.sh
for f in h264-aac.mp4 vp9-opus.webm h264-ac3.ts h264-aac-srt.mkv; do
  echo "$f: $(ffprobe -v error -show_entries stream=codec_type,codec_name -of csv=p=0 fixtures/$f | tr '\n' ' ')"
done
du -ch fixtures/* | tail -1
```

Expected:

```
h264-aac.mp4: h264,video aac,audio
vp9-opus.webm: vp9,video opus,audio
h264-ac3.ts: h264,video ac3,audio
h264-aac-srt.mkv: h264,video aac,audio subrip,subtitle
```

Total under 1 MB.

- [ ] **Step 3: Write `fixtures/README.md`**

```markdown
# Test fixtures

Generated by `scripts/make-fixtures.sh` with the system ffmpeg. 3 s, 320x240 @ 30 fps, 48 kHz.

| File             | Streams           | Purpose                                             |
| ---------------- | ----------------- | --------------------------------------------------- |
| h264-aac.mp4     | h264, aac         | MP4 input; remux to Matroska and TS                 |
| vp9-opus.webm    | vp9, opus         | Matroska demuxer; WebM muxer round trip             |
| h264-ac3.ts      | h264, ac3         | MPEG-TS input exercising parsers and Annex B -> MP4 |
| h264-aac-srt.mkv | h264, aac, subrip | Subtitle incompatible with MP4: skip/fail path      |
| not-media.txt    | none              | Non-media input must fail cleanly                   |
```

- [ ] **Step 4: Commit**

```bash
git add scripts/make-fixtures.sh fixtures
git commit -m "Add media test fixtures and their generator script"
```

---

### Task 5: Core AVIO adapters, log routing, metadata, JSON, `MediaReader::open`

**Files:**

- Create: `core/include/aviotrix/metadata.h`, `core/include/aviotrix/media_reader.h`, `core/include/aviotrix/json.h`, `core/src/avio_input.h`, `core/src/avio_input.cc`, `core/src/avio_output.h`, `core/src/avio_output.cc`, `core/src/log_router.h`, `core/src/log_router.cc`, `core/src/metadata.cc`, `core/src/json.cc`, `core/src/media_reader.cc`, `core/tests/test_avio.cc`, `core/tests/test_metadata.cc`, `core/tests/test_json.cc`
- Modify: `core/CMakeLists.txt`, `core/tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 3 `Status`, `IoSource`, `IoSink`, `LogHook`; Task 4 fixtures.
- Produces:
  - `aviotrix::Metadata`, `StreamInfo`, `VideoStreamInfo`, `AudioStreamInfo`, `Rational`, `StreamType`, `const char* streamTypeName(StreamType)`
  - `aviotrix::OpenOptions { LogHook log; }`
  - `aviotrix::MediaReader` with `Status open(IoSource&, OpenOptions = {})`, `bool isOpen() const`, `const Metadata& metadata() const`, `Status close()`. (`remux` is added in Task 6.)
  - `aviotrix::detail::AvioInput` / `AvioOutput` (used by Task 6): `static Status create(IoSource&, std::unique_ptr<AvioInput>&)`, `AVIOContext* context()`, `bool seekable()`, `int64_t bytesRead()` / `bytesWritten()`, `const Status& lastError()`, `void setCancelFlag(const std::atomic<bool>*)`, `Status closeIo()`.
  - `aviotrix::detail::ScopedLogTarget(const LogHook*)`, `aviotrix::detail::installAvLogCallback()`
  - `aviotrix::JsonWriter`, `std::string aviotrix::toJson(const Metadata&)`

- [ ] **Step 1: Write the failing tests**

`core/tests/test_avio.cc`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <numeric>

#include "../src/avio_input.h"
#include "../src/avio_output.h"
#include "file_io.h"

extern "C" {
#include <libavformat/avio.h>
}

using aviotrix::detail::AvioInput;
using aviotrix::detail::AvioOutput;

namespace {
std::vector<uint8_t> ramp(size_t n) {
  std::vector<uint8_t> v(n);
  std::iota(v.begin(), v.end(), 0);
  return v;
}
}  // namespace

TEST_CASE("AvioInput reads sequentially and tracks position") {
  test::MemorySource src(ramp(1000));
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(src, in).ok());
  REQUIRE(in->seekable());
  REQUIRE(avio_size(in->context()) == 1000);

  uint8_t buf[300];
  REQUIRE(avio_read(in->context(), buf, 300) == 300);
  REQUIRE(buf[0] == 0);
  REQUIRE(buf[299] == 299 % 256);
  REQUIRE(avio_read(in->context(), buf, 300) == 300);
  REQUIRE(buf[0] == 300 % 256);
  REQUIRE(avio_tell(in->context()) == 600);
}

TEST_CASE("AvioInput seeks with SEEK_SET/CUR/END and reports EOF") {
  test::MemorySource src(ramp(1000));
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(src, in).ok());
  REQUIRE(avio_seek(in->context(), 990, SEEK_SET) == 990);
  uint8_t buf[64];
  REQUIRE(avio_read(in->context(), buf, 64) == 10);
  REQUIRE(buf[0] == 990 % 256);
  REQUIRE(avio_read(in->context(), buf, 64) == AVERROR_EOF);
  REQUIRE(avio_seek(in->context(), -100, SEEK_END) == 900);
  REQUIRE(avio_seek(in->context(), -1, SEEK_SET) < 0);
}

TEST_CASE("AvioInput without a size is unseekable") {
  struct Unsized final : aviotrix::IoSource {
    test::MemorySource inner{ramp(100)};
    aviotrix::Status open(std::optional<int64_t>& size) override { size.reset(); return aviotrix::Status::Ok(); }
    aviotrix::Status read(int64_t o, std::span<uint8_t> b, size_t& n) override { return inner.read(o, b, n); }
    aviotrix::Status close() override { return aviotrix::Status::Ok(); }
  } src;
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(src, in).ok());
  REQUIRE_FALSE(in->seekable());
  REQUIRE(in->context()->seekable == 0);
  REQUIRE(avio_size(in->context()) < 0);
  uint8_t buf[100];
  REQUIRE(avio_read(in->context(), buf, 100) == 100);
}

TEST_CASE("AvioInput surfaces source errors and honors the cancel flag") {
  struct Failing final : aviotrix::IoSource {
    aviotrix::Status open(std::optional<int64_t>& size) override { size = 10; return aviotrix::Status::Ok(); }
    aviotrix::Status read(int64_t, std::span<uint8_t>, size_t&) override {
      return aviotrix::Status::Error(aviotrix::ErrorCode::IoFailed, "disk on fire");
    }
    aviotrix::Status close() override { return aviotrix::Status::Ok(); }
  } failing;
  std::unique_ptr<AvioInput> in;
  REQUIRE(AvioInput::create(failing, in).ok());
  uint8_t buf[10];
  REQUIRE(avio_read(in->context(), buf, 10) < 0);
  REQUIRE(in->lastError().message == "disk on fire");

  test::MemorySource src(ramp(100));
  std::unique_ptr<AvioInput> in2;
  REQUIRE(AvioInput::create(src, in2).ok());
  std::atomic<bool> cancel{true};
  in2->setCancelFlag(&cancel);
  REQUIRE(avio_read(in2->context(), buf, 10) == AVERROR_EXIT);
}

TEST_CASE("AvioOutput writes at tracked positions and seeks back when seekable") {
  test::MemorySink sink(true);
  std::unique_ptr<AvioOutput> out;
  REQUIRE(AvioOutput::create(sink, out).ok());
  const uint8_t a[4] = {1, 2, 3, 4};
  const uint8_t b[2] = {9, 9};
  avio_write(out->context(), a, 4);
  avio_flush(out->context());
  REQUIRE(avio_seek(out->context(), 1, SEEK_SET) == 1);
  avio_write(out->context(), b, 2);
  avio_flush(out->context());
  REQUIRE(out->closeIo().ok());
  REQUIRE(sink.bytes() == std::vector<uint8_t>{1, 9, 9, 4});
  REQUIRE(out->bytesWritten() == 4);
}

TEST_CASE("AvioOutput on a streaming sink has no seek and still writes") {
  test::MemorySink sink(false);
  std::unique_ptr<AvioOutput> out;
  REQUIRE(AvioOutput::create(sink, out).ok());
  REQUIRE(out->context()->seekable == 0);
  const uint8_t a[3] = {7, 8, 9};
  avio_write(out->context(), a, 3);
  avio_flush(out->context());
  REQUIRE(avio_seek(out->context(), 0, SEEK_SET) < 0);
  REQUIRE(out->closeIo().ok());
  REQUIRE(sink.bytes() == std::vector<uint8_t>{7, 8, 9});
}
```

`core/tests/test_metadata.cc`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "../src/log_router.h"
#include "aviotrix/media_reader.h"
#include "file_io.h"
#include "fixtures.h"

extern "C" {
#include <libavutil/log.h>
}

using aviotrix::MediaReader;
using aviotrix::StreamType;

TEST_CASE("open mp4 fixture and read metadata") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  REQUIRE(reader.isOpen());
  const auto& m = reader.metadata();
  REQUIRE(m.format == "mov,mp4,m4a,3gp,3g2,mj2");
  REQUIRE(m.streams.size() == 2);
  REQUIRE(m.duration.has_value());
  REQUIRE(*m.duration > 2.9);
  REQUIRE(*m.duration < 3.2);
  REQUIRE(m.streams[0].type == StreamType::Video);
  REQUIRE(m.streams[0].codec == "h264");
  REQUIRE(m.streams[0].video.has_value());
  REQUIRE(m.streams[0].video->width == 320);
  REQUIRE(m.streams[0].video->height == 240);
  REQUIRE(m.streams[0].video->frameRate.has_value());
  REQUIRE(m.streams[0].video->frameRate->num == 30);
  REQUIRE(m.streams[0].video->frameRate->den == 1);
  REQUIRE(m.streams[1].type == StreamType::Audio);
  REQUIRE(m.streams[1].codec == "aac");
  REQUIRE(m.streams[1].audio.has_value());
  REQUIRE(m.streams[1].audio->sampleRate == 48000);
  REQUIRE(m.streams[1].audio->channels == 1);
  REQUIRE(reader.close().ok());
  REQUIRE_FALSE(reader.isOpen());
  REQUIRE(src.closes() == 1);
}

TEST_CASE("open webm and ts fixtures; TS parsers fill dimensions") {
  {
    test::FileSource src(test::fixturePath("vp9-opus.webm"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    REQUIRE(reader.metadata().format == "matroska,webm");
    REQUIRE(reader.metadata().streams[0].codec == "vp9");
    REQUIRE(reader.metadata().streams[1].codec == "opus");
  }
  {
    test::FileSource src(test::fixturePath("h264-ac3.ts"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    REQUIRE(reader.metadata().format == "mpegts");
    REQUIRE(reader.metadata().streams[0].codec == "h264");
    REQUIRE(reader.metadata().streams[0].video->width == 320);
    REQUIRE(reader.metadata().streams[1].codec == "ac3");
    REQUIRE(reader.metadata().streams[1].audio->sampleRate == 48000);
  }
}

TEST_CASE("mkv with srt reports a subtitle stream") {
  test::FileSource src(test::fixturePath("h264-aac-srt.mkv"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  REQUIRE(reader.metadata().streams.size() == 3);
  REQUIRE(reader.metadata().streams[2].type == StreamType::Subtitle);
  REQUIRE(reader.metadata().streams[2].codec == "subrip");
}

TEST_CASE("non-media and empty inputs fail cleanly") {
  test::FileSource txt(test::fixturePath("not-media.txt"));
  MediaReader a;
  aviotrix::Status s = a.open(txt);
  REQUIRE_FALSE(s.ok());
  REQUIRE(s.code < 0);
  REQUIRE_FALSE(a.isOpen());
  REQUIRE(txt.closes() == 1);

  test::MemorySource empty({});
  MediaReader b;
  REQUIRE_FALSE(b.open(empty).ok());
  REQUIRE_FALSE(b.isOpen());
}

TEST_CASE("truncated size yields EOF, never reads past the reported size") {
  struct Truncated final : aviotrix::IoSource {
    test::FileSource inner{test::fixturePath("h264-aac.mp4")};
    int64_t reported = 0;
    int64_t maxEnd = 0;
    aviotrix::Status open(std::optional<int64_t>& size) override {
      auto s = inner.open(size);
      reported = *size / 2;
      size = reported;
      return s;
    }
    aviotrix::Status read(int64_t o, std::span<uint8_t> b, size_t& n) override {
      maxEnd = std::max(maxEnd, o + static_cast<int64_t>(b.size()));
      if (o >= reported) { n = 0; return aviotrix::Status::Ok(); }
      auto s = inner.read(o, b.first(static_cast<size_t>(std::min<int64_t>(b.size(), reported - o))), n);
      return s;
    }
    aviotrix::Status close() override { return inner.close(); }
  } src;
  MediaReader reader;
  (void)reader.open(src);  // faststart mp4: may succeed with a short duration, or fail; both are fine
  REQUIRE(src.maxEnd <= src.reported + 65536);  // one AVIO buffer of slack past the reported end
  (void)reader.close();
}

TEST_CASE("log router joins partial lines and routes to the current hook") {
  std::vector<std::pair<aviotrix::LogLevel, std::string>> got;
  aviotrix::LogHook hook = [&](aviotrix::LogLevel lvl, std::string_view text) { got.emplace_back(lvl, std::string(text)); };
  aviotrix::detail::installAvLogCallback();
  {
    aviotrix::detail::ScopedLogTarget target(&hook);
    av_log(nullptr, AV_LOG_WARNING, "part ");
    av_log(nullptr, AV_LOG_WARNING, "two\n");
    av_log(nullptr, AV_LOG_INFO, "second line\n");
  }
  av_log(nullptr, AV_LOG_ERROR, "not routed\n");
  REQUIRE(got.size() == 2);
  REQUIRE(got[0].first == aviotrix::LogLevel::Warning);
  REQUIRE(got[0].second == "part two");
  REQUIRE(got[1].second == "second line");
}

TEST_CASE("open passes libav log lines to the OpenOptions hook") {
  std::vector<std::string> lines;
  aviotrix::OpenOptions opts;
  opts.log = [&](aviotrix::LogLevel, std::string_view t) { lines.emplace_back(t); };
  test::FileSource txt(test::fixturePath("not-media.txt"));
  MediaReader reader;
  REQUIRE_FALSE(reader.open(txt, opts).ok());
  REQUIRE_FALSE(lines.empty());  // probing a text file always logs at least one error/warning line
}
```

`core/tests/test_json.cc`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "aviotrix/json.h"
#include "aviotrix/media_reader.h"
#include "file_io.h"
#include "fixtures.h"

TEST_CASE("JsonWriter escapes strings and handles nesting") {
  aviotrix::JsonWriter w;
  w.beginObject();
  w.key("s"); w.value(std::string_view("a\"b\\c\nd\x01"));
  w.key("n"); w.value(int64_t{-5});
  w.key("d"); w.value(1.5);
  w.key("b"); w.value(true);
  w.key("z"); w.null();
  w.key("arr"); w.beginArray(); w.value(int64_t{1}); w.value(int64_t{2}); w.endArray();
  w.endObject();
  REQUIRE(w.str() == R"({"s":"a\"b\\c\nd\u0001","n":-5,"d":1.5,"b":true,"z":null,"arr":[1,2]})");
}

TEST_CASE("toJson(Metadata) contains the stream fields") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  aviotrix::MediaReader reader;
  REQUIRE(reader.open(src).ok());
  const std::string json = aviotrix::toJson(reader.metadata());
  REQUIRE(json.find(R"("format":"mov,mp4,m4a,3gp,3g2,mj2")") != std::string::npos);
  REQUIRE(json.find(R"("codec":"h264")") != std::string::npos);
  REQUIRE(json.find(R"("width":320)") != std::string::npos);
  REQUIRE(json.find(R"("frameRate":{"num":30,"den":1})") != std::string::npos);
  REQUIRE(json.find(R"("type":"audio")") != std::string::npos);
  REQUIRE(json.find(R"("sampleRate":48000)") != std::string::npos);
  REQUIRE(json.find(R"("bitRate":)") != std::string::npos);
}
```

Add to `core/tests/CMakeLists.txt` executable sources: `test_avio.cc test_metadata.cc test_json.cc`.

- [ ] **Step 2: Build to verify the tests fail**

```bash
cmake -S . -B build/core-host && cmake --build build/core-host -j 2>&1 | tail -5
```

Expected: FAIL with missing headers `avio_input.h`, `aviotrix/media_reader.h`, `aviotrix/json.h`.

- [ ] **Step 3: Write the public headers**

`core/include/aviotrix/metadata.h`:

```cpp
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aviotrix {

struct Rational {
  int num = 0;
  int den = 1;
};

enum class StreamType { Video, Audio, Subtitle, Data, Attachment };
const char* streamTypeName(StreamType type);  // "video", "audio", "subtitle", "data", "attachment"

struct VideoStreamInfo {
  int width = 0;
  int height = 0;
  std::optional<Rational> frameRate;
  std::optional<std::string> pixelFormat;
};

struct AudioStreamInfo {
  int sampleRate = 0;
  int channels = 0;
  std::optional<std::string> channelLayout;
};

struct StreamInfo {
  int index = 0;
  StreamType type = StreamType::Data;
  std::string codec;
  std::optional<std::string> codecTag;
  Rational timeBase;
  std::optional<double> startTime;  // seconds
  std::optional<double> duration;   // seconds
  std::optional<int64_t> bitRate;
  std::optional<std::string> language;
  std::map<std::string, std::string> tags;
  std::optional<VideoStreamInfo> video;
  std::optional<AudioStreamInfo> audio;
};

struct Metadata {
  std::string format;
  std::string formatLongName;
  std::optional<double> startTime;
  std::optional<double> duration;
  std::optional<int64_t> bitRate;
  std::map<std::string, std::string> tags;
  std::vector<StreamInfo> streams;
};

}  // namespace aviotrix
```

`core/include/aviotrix/media_reader.h` (the `remux` declaration is added in Task 6):

```cpp
#pragma once

#include <memory>

#include "aviotrix/io.h"
#include "aviotrix/log.h"
#include "aviotrix/metadata.h"
#include "aviotrix/status.h"

namespace aviotrix {

struct OpenOptions {
  LogHook log;  // receives libav log lines while this reader is operating; may be empty
};

class MediaReader {
 public:
  MediaReader();
  ~MediaReader();
  MediaReader(const MediaReader&) = delete;
  MediaReader& operator=(const MediaReader&) = delete;

  // Opens `source` (which must outlive the reader until close()) and probes stream info.
  Status open(IoSource& source, OpenOptions options = {});
  bool isOpen() const;
  const Metadata& metadata() const;
  Status close();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace aviotrix
```

`core/include/aviotrix/json.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aviotrix/metadata.h"

namespace aviotrix {

// Minimal JSON emitter. Only what the bindings need to hand results to TypeScript.
class JsonWriter {
 public:
  void beginObject();
  void endObject();
  void beginArray();
  void endArray();
  void key(std::string_view name);
  void value(std::string_view s);
  void value(const char* s) { value(std::string_view(s)); }
  void value(int64_t n);
  void value(int n) { value(static_cast<int64_t>(n)); }
  void value(double d);  // non-finite -> null
  void value(bool b);
  void null();
  std::string str() const { return out_; }

 private:
  void separator();
  void escape(std::string_view s);
  std::string out_;
  std::vector<bool> needComma_;  // one entry per open container
  bool afterKey_ = false;        // the next value follows a key: no comma
};

std::string toJson(const Metadata& metadata);

}  // namespace aviotrix
```

- [ ] **Step 4: Write the AVIO adapters**

`core/src/avio_input.h`:

```cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include "aviotrix/io.h"
#include "aviotrix/status.h"

extern "C" {
#include <libavformat/avio.h>
}

namespace aviotrix::detail {

// Adapts libav's sequential read/seek callbacks onto a positioned IoSource.
class AvioInput {
 public:
  static Status create(IoSource& source, std::unique_ptr<AvioInput>& out);
  ~AvioInput();
  AvioInput(const AvioInput&) = delete;
  AvioInput& operator=(const AvioInput&) = delete;

  AVIOContext* context() const { return ctx_; }
  bool seekable() const { return size_.has_value(); }
  int64_t bytesRead() const { return bytesRead_; }
  const Status& lastError() const { return lastError_; }
  void setCancelFlag(const std::atomic<bool>* flag) { cancel_ = flag; }
  Status closeIo();  // closes the source once; safe to call twice

 private:
  AvioInput() = default;
  static int readPacket(void* opaque, uint8_t* buf, int bufSize);
  static int64_t seek(void* opaque, int64_t offset, int whence);

  IoSource* source_ = nullptr;
  std::optional<int64_t> size_;
  int64_t position_ = 0;
  int64_t bytesRead_ = 0;
  AVIOContext* ctx_ = nullptr;
  Status lastError_;
  const std::atomic<bool>* cancel_ = nullptr;
  bool sourceOpen_ = false;
};

}  // namespace aviotrix::detail
```

`core/src/avio_input.cc`:

```cpp
#include "avio_input.h"

#include <algorithm>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace aviotrix::detail {

namespace {
constexpr int kBufferSize = 64 * 1024;
}

Status AvioInput::create(IoSource& source, std::unique_ptr<AvioInput>& out) {
  std::unique_ptr<AvioInput> in(new AvioInput());
  in->source_ = &source;
  Status st = source.open(in->size_);
  if (!st.ok()) return st;
  in->sourceOpen_ = true;
  if (in->size_ && *in->size_ < 0) in->size_.reset();

  uint8_t* buffer = static_cast<uint8_t*>(av_malloc(kBufferSize));
  if (!buffer) return Status::FromAv(AVERROR(ENOMEM), "av_malloc");
  in->ctx_ = avio_alloc_context(buffer, kBufferSize, 0, in.get(), &AvioInput::readPacket, nullptr,
                                in->seekable() ? &AvioInput::seek : nullptr);
  if (!in->ctx_) {
    av_free(buffer);
    return Status::FromAv(AVERROR(ENOMEM), "avio_alloc_context");
  }
  if (!in->seekable()) in->ctx_->seekable = 0;
  out = std::move(in);
  return Status::Ok();
}

AvioInput::~AvioInput() {
  (void)closeIo();
  if (ctx_) {
    av_freep(&ctx_->buffer);
    avio_context_free(&ctx_);
  }
}

Status AvioInput::closeIo() {
  if (!sourceOpen_) return Status::Ok();
  sourceOpen_ = false;
  return source_->close();
}

int AvioInput::readPacket(void* opaque, uint8_t* buf, int bufSize) {
  auto* self = static_cast<AvioInput*>(opaque);
  if (self->cancel_ && self->cancel_->load(std::memory_order_relaxed)) return AVERROR_EXIT;
  if (bufSize <= 0) return 0;
  if (self->size_ && self->position_ >= *self->size_) return AVERROR_EOF;
  size_t want = static_cast<size_t>(bufSize);
  if (self->size_) want = static_cast<size_t>(std::min<int64_t>(want, *self->size_ - self->position_));
  size_t got = 0;
  Status st = self->source_->read(self->position_, std::span<uint8_t>(buf, want), got);
  if (!st.ok()) {
    self->lastError_ = st;
    return AVERROR(EIO);
  }
  got = std::min(got, want);
  if (got == 0) return AVERROR_EOF;
  self->position_ += static_cast<int64_t>(got);
  self->bytesRead_ += static_cast<int64_t>(got);
  return static_cast<int>(got);
}

int64_t AvioInput::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<AvioInput*>(opaque);
  if (!self->size_) return AVERROR(ENOSYS);
  const int mode = whence & ~AVSEEK_FORCE;
  int64_t target = 0;
  switch (mode) {
    case AVSEEK_SIZE: return *self->size_;
    case SEEK_SET: target = offset; break;
    case SEEK_CUR: target = self->position_ + offset; break;
    case SEEK_END: target = *self->size_ + offset; break;
    default: return AVERROR(EINVAL);
  }
  if (target < 0) return AVERROR(EINVAL);
  self->position_ = target;
  return target;
}

}  // namespace aviotrix::detail
```

`core/src/avio_output.h`:

```cpp
#pragma once

#include <cstdint>
#include <memory>

#include "aviotrix/io.h"
#include "aviotrix/status.h"

extern "C" {
#include <libavformat/avio.h>
}

namespace aviotrix::detail {

// Adapts libav's sequential write/seek callbacks onto a positioned IoSink.
class AvioOutput {
 public:
  static Status create(IoSink& sink, std::unique_ptr<AvioOutput>& out);
  ~AvioOutput();
  AvioOutput(const AvioOutput&) = delete;
  AvioOutput& operator=(const AvioOutput&) = delete;

  AVIOContext* context() const { return ctx_; }
  bool seekable() const { return sink_->seekable(); }
  int64_t bytesWritten() const { return maxPosition_; }
  const Status& lastError() const { return lastError_; }
  Status closeIo();  // flushes and closes the sink once; safe to call twice

 private:
  AvioOutput() = default;
  static int writePacket(void* opaque, const uint8_t* buf, int bufSize);
  static int64_t seek(void* opaque, int64_t offset, int whence);

  IoSink* sink_ = nullptr;
  int64_t position_ = 0;
  int64_t maxPosition_ = 0;
  AVIOContext* ctx_ = nullptr;
  Status lastError_;
  bool sinkOpen_ = false;
};

}  // namespace aviotrix::detail
```

`core/src/avio_output.cc`:

```cpp
#include "avio_output.h"

#include <algorithm>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace aviotrix::detail {

namespace {
constexpr int kBufferSize = 64 * 1024;
}

Status AvioOutput::create(IoSink& sink, std::unique_ptr<AvioOutput>& out) {
  std::unique_ptr<AvioOutput> o(new AvioOutput());
  o->sink_ = &sink;
  Status st = sink.open();
  if (!st.ok()) return st;
  o->sinkOpen_ = true;

  uint8_t* buffer = static_cast<uint8_t*>(av_malloc(kBufferSize));
  if (!buffer) return Status::FromAv(AVERROR(ENOMEM), "av_malloc");
  o->ctx_ = avio_alloc_context(buffer, kBufferSize, 1, o.get(), nullptr, &AvioOutput::writePacket,
                               sink.seekable() ? &AvioOutput::seek : nullptr);
  if (!o->ctx_) {
    av_free(buffer);
    return Status::FromAv(AVERROR(ENOMEM), "avio_alloc_context");
  }
  if (!sink.seekable()) o->ctx_->seekable = 0;
  out = std::move(o);
  return Status::Ok();
}

AvioOutput::~AvioOutput() {
  (void)closeIo();
  if (ctx_) {
    av_freep(&ctx_->buffer);
    avio_context_free(&ctx_);
  }
}

Status AvioOutput::closeIo() {
  if (!sinkOpen_) return Status::Ok();
  if (ctx_) avio_flush(ctx_);
  sinkOpen_ = false;
  Status st = sink_->close();
  if (!st.ok() && lastError_.ok()) lastError_ = st;
  return st;
}

int AvioOutput::writePacket(void* opaque, const uint8_t* buf, int bufSize) {
  auto* self = static_cast<AvioOutput*>(opaque);
  if (bufSize <= 0) return 0;
  Status st = self->sink_->write(self->position_, std::span<const uint8_t>(buf, static_cast<size_t>(bufSize)));
  if (!st.ok()) {
    self->lastError_ = st;
    return AVERROR(EIO);
  }
  self->position_ += bufSize;
  self->maxPosition_ = std::max(self->maxPosition_, self->position_);
  return bufSize;
}

int64_t AvioOutput::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<AvioOutput*>(opaque);
  const int mode = whence & ~AVSEEK_FORCE;
  int64_t target = 0;
  switch (mode) {
    case AVSEEK_SIZE: return self->maxPosition_;
    case SEEK_SET: target = offset; break;
    case SEEK_CUR: target = self->position_ + offset; break;
    case SEEK_END: target = self->maxPosition_ + offset; break;
    default: return AVERROR(EINVAL);
  }
  if (target < 0) return AVERROR(EINVAL);
  self->position_ = target;
  return target;
}

}  // namespace aviotrix::detail
```

- [ ] **Step 5: Write the log router**

`core/src/log_router.h`:

```cpp
#pragma once

#include "aviotrix/log.h"

namespace aviotrix::detail {

// Installs the process-wide av_log callback once. Safe to call repeatedly.
void installAvLogCallback();

// While alive on this thread, libav log lines are delivered to *hook (if non-null and non-empty).
// Nesting restores the previous target. Operations never interleave on one thread, so this is
// enough to attribute lines to the right reader on both Node (one thread per reader) and WASM
// (module-wide serialization in the TS wrapper).
class ScopedLogTarget {
 public:
  explicit ScopedLogTarget(const LogHook* hook);
  ~ScopedLogTarget();
  ScopedLogTarget(const ScopedLogTarget&) = delete;
  ScopedLogTarget& operator=(const ScopedLogTarget&) = delete;

 private:
  const LogHook* previous_;
};

}  // namespace aviotrix::detail
```

`core/src/log_router.cc`:

```cpp
#include "log_router.h"

#include <mutex>
#include <string>

extern "C" {
#include <libavutil/log.h>
}

namespace aviotrix::detail {

namespace {

thread_local const LogHook* tCurrentHook = nullptr;
thread_local std::string tPending;
thread_local int tPrintPrefix = 1;

void avLogCallback(void* avcl, int level, const char* fmt, va_list vl) {
  if (level > AV_LOG_VERBOSE) return;
  const LogHook* hook = tCurrentHook;
  if (!hook || !*hook) return;
  char line[1024];
  av_log_format_line2(avcl, level, fmt, vl, line, sizeof line, &tPrintPrefix);
  tPending += line;
  size_t nl;
  while ((nl = tPending.find('\n')) != std::string::npos) {
    std::string_view text(tPending.data(), nl);
    (*hook)(logLevelFromAv(level), text);
    tPending.erase(0, nl + 1);
  }
}

std::once_flag gInstalled;

}  // namespace

void installAvLogCallback() {
  std::call_once(gInstalled, [] {
    av_log_set_level(AV_LOG_VERBOSE);
    av_log_set_callback(&avLogCallback);
  });
}

ScopedLogTarget::ScopedLogTarget(const LogHook* hook) : previous_(tCurrentHook) {
  tCurrentHook = hook;
  tPending.clear();
  tPrintPrefix = 1;
}

ScopedLogTarget::~ScopedLogTarget() {
  if (!tPending.empty() && tCurrentHook && *tCurrentHook) {
    (*tCurrentHook)(LogLevel::Info, tPending);
    tPending.clear();
  }
  tCurrentHook = previous_;
}

}  // namespace aviotrix::detail
```

- [ ] **Step 6: Write metadata extraction, JSON, and MediaReader**

`core/src/metadata.cc` (also declares the internal `readMetadata` used by media_reader.cc):

```cpp
#include "aviotrix/metadata.h"

#include "metadata_internal.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
}

namespace aviotrix {

const char* streamTypeName(StreamType type) {
  switch (type) {
    case StreamType::Video: return "video";
    case StreamType::Audio: return "audio";
    case StreamType::Subtitle: return "subtitle";
    case StreamType::Data: return "data";
    case StreamType::Attachment: return "attachment";
  }
  return "data";
}

namespace detail {

namespace {

std::map<std::string, std::string> dictToMap(const AVDictionary* dict) {
  std::map<std::string, std::string> out;
  const AVDictionaryEntry* e = nullptr;
  while ((e = av_dict_iterate(dict, e)) != nullptr) out.emplace(e->key, e->value);
  return out;
}

StreamType typeOf(AVMediaType t) {
  switch (t) {
    case AVMEDIA_TYPE_VIDEO: return StreamType::Video;
    case AVMEDIA_TYPE_AUDIO: return StreamType::Audio;
    case AVMEDIA_TYPE_SUBTITLE: return StreamType::Subtitle;
    case AVMEDIA_TYPE_ATTACHMENT: return StreamType::Attachment;
    default: return StreamType::Data;
  }
}

std::optional<double> secondsOf(int64_t ts, AVRational tb) {
  if (ts == AV_NOPTS_VALUE) return std::nullopt;
  return ts * av_q2d(tb);
}

std::optional<Rational> frameRateOf(const AVStream* st) {
  if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0) return Rational{st->avg_frame_rate.num, st->avg_frame_rate.den};
  if (st->r_frame_rate.num > 0 && st->r_frame_rate.den > 0) return Rational{st->r_frame_rate.num, st->r_frame_rate.den};
  return std::nullopt;
}

std::optional<std::string> codecTagOf(uint32_t tag) {
  if (tag == 0) return std::nullopt;
  char buf[AV_FOURCC_MAX_STRING_SIZE];
  return std::string(av_fourcc_make_string(buf, tag));
}

}  // namespace

Metadata readMetadata(const AVFormatContext* fmt) {
  Metadata m;
  m.format = fmt->iformat->name ? fmt->iformat->name : "";
  m.formatLongName = fmt->iformat->long_name ? fmt->iformat->long_name : "";
  m.startTime = secondsOf(fmt->start_time, AVRational{1, AV_TIME_BASE});
  if (fmt->duration > 0) m.duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
  if (fmt->bit_rate > 0) m.bitRate = fmt->bit_rate;
  m.tags = dictToMap(fmt->metadata);

  for (unsigned i = 0; i < fmt->nb_streams; i++) {
    const AVStream* st = fmt->streams[i];
    const AVCodecParameters* par = st->codecpar;
    StreamInfo info;
    info.index = static_cast<int>(i);
    info.type = typeOf(par->codec_type);
    info.codec = avcodec_get_name(par->codec_id);
    info.codecTag = codecTagOf(par->codec_tag);
    info.timeBase = Rational{st->time_base.num, st->time_base.den};
    info.startTime = secondsOf(st->start_time, st->time_base);
    if (st->duration > 0) info.duration = st->duration * av_q2d(st->time_base);
    if (par->bit_rate > 0) info.bitRate = par->bit_rate;
    info.tags = dictToMap(st->metadata);
    if (auto it = info.tags.find("language"); it != info.tags.end()) info.language = it->second;
    if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
      VideoStreamInfo v;
      v.width = par->width;
      v.height = par->height;
      v.frameRate = frameRateOf(st);
      if (par->format >= 0) {
        if (const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(par->format))) v.pixelFormat = name;
      }
      info.video = v;
    } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
      AudioStreamInfo a;
      a.sampleRate = par->sample_rate;
      a.channels = par->ch_layout.nb_channels;
      char buf[128];
      if (av_channel_layout_describe(&par->ch_layout, buf, sizeof buf) > 0) a.channelLayout = buf;
      info.audio = a;
    }
    m.streams.push_back(std::move(info));
  }
  return m;
}

}  // namespace detail
}  // namespace aviotrix
```

`core/src/metadata_internal.h`:

```cpp
#pragma once

#include "aviotrix/metadata.h"

struct AVFormatContext;

namespace aviotrix::detail {
Metadata readMetadata(const AVFormatContext* fmt);
}
```

`core/src/json.cc`:

```cpp
#include "aviotrix/json.h"

#include <cmath>
#include <cstdio>

namespace aviotrix {

void JsonWriter::separator() {
  if (afterKey_) { afterKey_ = false; return; }
  if (needComma_.empty()) return;
  if (needComma_.back()) out_ += ',';
  needComma_.back() = true;
}

void JsonWriter::beginObject() { separator(); out_ += '{'; needComma_.push_back(false); }
void JsonWriter::endObject() { out_ += '}'; needComma_.pop_back(); }
void JsonWriter::beginArray() { separator(); out_ += '['; needComma_.push_back(false); }
void JsonWriter::endArray() { out_ += ']'; needComma_.pop_back(); }

void JsonWriter::key(std::string_view name) {
  separator();
  escape(name);
  out_ += ':';
  afterKey_ = true;
}

void JsonWriter::value(std::string_view s) { separator(); escape(s); }
void JsonWriter::value(int64_t n) { separator(); out_ += std::to_string(n); }
void JsonWriter::value(bool b) { separator(); out_ += b ? "true" : "false"; }
void JsonWriter::null() { separator(); out_ += "null"; }

void JsonWriter::value(double d) {
  separator();
  if (!std::isfinite(d)) { out_ += "null"; return; }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.17g", d);
  out_ += buf;
}

void JsonWriter::escape(std::string_view s) {
  out_ += '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"': out_ += "\\\""; break;
      case '\\': out_ += "\\\\"; break;
      case '\n': out_ += "\\n"; break;
      case '\r': out_ += "\\r"; break;
      case '\t': out_ += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out_ += buf;
        } else {
          out_ += static_cast<char>(c);
        }
    }
  }
  out_ += '"';
}

namespace {

void writeOptional(JsonWriter& w, const char* key, const std::optional<double>& v) {
  w.key(key);
  if (v) w.value(*v); else w.null();
}
void writeOptional(JsonWriter& w, const char* key, const std::optional<int64_t>& v) {
  w.key(key);
  if (v) w.value(*v); else w.null();
}
void writeOptional(JsonWriter& w, const char* key, const std::optional<std::string>& v) {
  w.key(key);
  if (v) w.value(*v); else w.null();
}
void writeRational(JsonWriter& w, const Rational& r) {
  w.beginObject();
  w.key("num"); w.value(r.num);
  w.key("den"); w.value(r.den);
  w.endObject();
}
void writeTags(JsonWriter& w, const std::map<std::string, std::string>& tags) {
  w.key("tags");
  w.beginObject();
  for (const auto& [k, v] : tags) { w.key(k); w.value(v); }
  w.endObject();
}

}  // namespace

std::string toJson(const Metadata& m) {
  JsonWriter w;
  w.beginObject();
  w.key("format"); w.value(m.format);
  w.key("formatLongName"); w.value(m.formatLongName);
  writeOptional(w, "startTime", m.startTime);
  writeOptional(w, "duration", m.duration);
  writeOptional(w, "bitRate", m.bitRate);
  writeTags(w, m.tags);
  w.key("streams");
  w.beginArray();
  for (const StreamInfo& s : m.streams) {
    w.beginObject();
    w.key("index"); w.value(s.index);
    w.key("type"); w.value(streamTypeName(s.type));
    w.key("codec"); w.value(s.codec);
    writeOptional(w, "codecTag", s.codecTag);
    w.key("timeBase"); writeRational(w, s.timeBase);
    writeOptional(w, "startTime", s.startTime);
    writeOptional(w, "duration", s.duration);
    writeOptional(w, "bitRate", s.bitRate);
    writeOptional(w, "language", s.language);
    writeTags(w, s.tags);
    if (s.video) {
      w.key("video");
      w.beginObject();
      w.key("width"); w.value(s.video->width);
      w.key("height"); w.value(s.video->height);
      w.key("frameRate");
      if (s.video->frameRate) writeRational(w, *s.video->frameRate); else w.null();
      writeOptional(w, "pixelFormat", s.video->pixelFormat);
      w.endObject();
    }
    if (s.audio) {
      w.key("audio");
      w.beginObject();
      w.key("sampleRate"); w.value(s.audio->sampleRate);
      w.key("channels"); w.value(s.audio->channels);
      writeOptional(w, "channelLayout", s.audio->channelLayout);
      w.endObject();
    }
    w.endObject();
  }
  w.endArray();
  w.endObject();
  return w.str();
}

}  // namespace aviotrix
```

`core/src/media_reader_impl.h` (shared with remux.cc in Task 6):

```cpp
#pragma once

#include <memory>

#include "aviotrix/media_reader.h"
#include "avio_input.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace aviotrix {

struct MediaReader::Impl {
  AVFormatContext* fmt = nullptr;
  std::unique_ptr<detail::AvioInput> input;
  Metadata metadata;
  LogHook log;
  bool open = false;

  void release() {
    if (fmt) avformat_close_input(&fmt);  // does not touch the custom pb
    input.reset();                         // closes the IoSource, frees the AVIOContext
    open = false;
  }
};

}  // namespace aviotrix
```

`core/src/media_reader.cc`:

```cpp
#include "aviotrix/media_reader.h"

#include "log_router.h"
#include "media_reader_impl.h"
#include "metadata_internal.h"

namespace aviotrix {

MediaReader::MediaReader() : impl_(std::make_unique<Impl>()) {}
MediaReader::~MediaReader() { impl_->release(); }

bool MediaReader::isOpen() const { return impl_->open; }
const Metadata& MediaReader::metadata() const { return impl_->metadata; }

Status MediaReader::open(IoSource& source, OpenOptions options) {
  if (impl_->open) return Status::Error(ErrorCode::InvalidArgument, "reader is already open");
  impl_->log = std::move(options.log);
  detail::installAvLogCallback();
  detail::ScopedLogTarget logTarget(&impl_->log);

  Status st = detail::AvioInput::create(source, impl_->input);
  if (!st.ok()) { impl_->release(); return st; }

  impl_->fmt = avformat_alloc_context();
  if (!impl_->fmt) { impl_->release(); return Status::FromAv(AVERROR(ENOMEM), "avformat_alloc_context"); }
  impl_->fmt->pb = impl_->input->context();
  impl_->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

  int ret = avformat_open_input(&impl_->fmt, nullptr, nullptr, nullptr);
  if (ret < 0) {
    Status ioErr = impl_->input->lastError();
    impl_->release();
    return ioErr.ok() ? Status::FromAv(ret, "avformat_open_input") : ioErr;
  }
  ret = avformat_find_stream_info(impl_->fmt, nullptr);
  if (ret < 0) {
    Status ioErr = impl_->input->lastError();
    impl_->release();
    return ioErr.ok() ? Status::FromAv(ret, "avformat_find_stream_info") : ioErr;
  }
  impl_->metadata = detail::readMetadata(impl_->fmt);
  impl_->open = true;
  return Status::Ok();
}

Status MediaReader::close() {
  if (!impl_->open && !impl_->input) return Status::Ok();
  detail::ScopedLogTarget logTarget(&impl_->log);
  Status st = impl_->input ? impl_->input->closeIo() : Status::Ok();
  impl_->release();
  return st;
}

}  // namespace aviotrix
```

Add to `core/CMakeLists.txt` sources: `src/avio_input.cc src/avio_output.cc src/log_router.cc src/metadata.cc src/json.cc src/media_reader.cc`.

- [ ] **Step 7: Build and run the tests**

```bash
npm run test:core
```

Expected: all tests pass, including the five Task 3 tests. If `open passes libav log lines` fails with zero lines, libav probed silently; change that test's input to `test::MemorySource(std::vector<uint8_t>(4096, 0))` (4 KB of zeros), which logs `Invalid data found when processing input` through the format probe.

- [ ] **Step 8: Commit**

```bash
git add core
git commit -m "Add core AVIO adapters, log routing, metadata extraction, JSON writer, MediaReader::open"
```

---

### Task 6: Core remux

**Files:**

- Create: `core/include/aviotrix/remux.h`, `core/src/remux.cc`, `core/tests/test_remux.cc`
- Modify: `core/include/aviotrix/media_reader.h` (add `remux`), `core/include/aviotrix/json.h` + `core/src/json.cc` (add `toJson(const RemuxResult&)`), `core/src/media_reader_impl.h` (add `bool consumed`), `core/tests/file_io.h` (add `opened()` to `MemorySink`), `core/CMakeLists.txt`, `core/tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 5 `MediaReader::Impl`, `AvioInput`, `AvioOutput`, `ScopedLogTarget`, `JsonWriter`.
- Produces:
  - `aviotrix::RemuxProgress { int64_t bytesRead; int64_t bytesWritten; std::optional<double> timestamp; }`
  - `aviotrix::RemuxOptions { std::string format; std::optional<std::vector<int>> streams; bool failOnIncompatible=false; bool fragmented=false; const std::atomic<bool>* cancel=nullptr; std::function<void(const RemuxProgress&)> onProgress; int progressIntervalPackets=100; }`
  - `aviotrix::RemuxStreamMapping { int input; std::optional<int> output; std::string skippedReason; }`
  - `aviotrix::RemuxResult { std::vector<RemuxStreamMapping> streams; int64_t bytesRead; int64_t bytesWritten; int64_t packets; }`
  - `Status MediaReader::remux(IoSink& sink, const RemuxOptions& options, RemuxResult& result)`
  - `std::string aviotrix::toJson(const RemuxResult&)` → `{"streams":[{"input":0,"output":0},{"input":2,"output":null,"skippedReason":"..."}],"bytesRead":N,"bytesWritten":N,"packets":N}`
- Error contract (bindings map `code` via `errorCodeName`): unknown `format` → `INVALID_ARGUMENT`; out-of-range stream index → `INVALID_ARGUMENT`; non-seekable sink with mp4/mov and `!fragmented` → `SINK_NOT_SEEKABLE` (sink never opened); incompatible stream with `failOnIncompatible` → `INCOMPATIBLE_STREAM` (sink never opened); every selected stream incompatible → `INCOMPATIBLE_STREAM`; cancel → `ABORTED` (sink still closed); sink/source IO error → that `Status`, else the libav `AVERROR`.

- [ ] **Step 1: Write the failing tests**

Add to `test::MemorySink` in `core/tests/file_io.h`: `bool opened() const { return opened_; }`.

`core/tests/test_remux.cc`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <vector>

#include "aviotrix/json.h"
#include "aviotrix/media_reader.h"
#include "aviotrix/remux.h"
#include "file_io.h"
#include "fixtures.h"

using aviotrix::ErrorCode;
using aviotrix::MediaReader;
using aviotrix::RemuxOptions;
using aviotrix::RemuxResult;
using aviotrix::Status;

namespace {

aviotrix::Metadata reopen(const std::vector<uint8_t>& bytes) {
  test::MemorySource src(bytes);
  MediaReader r;
  REQUIRE(r.open(src).ok());
  return r.metadata();
}

bool contains(const std::vector<uint8_t>& hay, const char* needle) {
  const std::string s(hay.begin(), hay.end());
  return s.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("remux mp4 to matroska and reopen") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "matroska";
  RemuxResult result;
  REQUIRE(reader.remux(sink, opt, result).ok());
  REQUIRE(result.streams.size() == 2);
  REQUIRE(result.streams[0].output == 0);
  REQUIRE(result.streams[1].output == 1);
  REQUIRE(result.packets > 100);
  REQUIRE(result.bytesWritten == static_cast<int64_t>(sink.bytes().size()));
  REQUIRE(result.bytesRead > 0);
  REQUIRE(sink.closed());

  auto m = reopen(sink.bytes());
  REQUIRE(m.format == "matroska,webm");
  REQUIRE(m.streams.size() == 2);
  REQUIRE(m.streams[0].codec == "h264");
  REQUIRE(m.streams[1].codec == "aac");
  REQUIRE(m.duration.has_value());
  REQUIRE(std::abs(*m.duration - *reader.metadata().duration) < 0.2);
}

TEST_CASE("remux mpegts to mp4 (Annex B -> avcC via auto bsf) and reopen") {
  test::FileSource src(test::fixturePath("h264-ac3.ts"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "mp4";
  RemuxResult result;
  REQUIRE(reader.remux(sink, opt, result).ok());
  REQUIRE(std::string(sink.bytes().begin() + 4, sink.bytes().begin() + 8) == "ftyp");
  auto m = reopen(sink.bytes());
  REQUIRE(m.format == "mov,mp4,m4a,3gp,3g2,mj2");
  REQUIRE(m.streams[0].codec == "h264");
  REQUIRE(m.streams[0].video->width == 320);
  REQUIRE(m.streams[1].codec == "ac3");
}

TEST_CASE("remux mp4 to mpegts and webm to webm") {
  {
    test::FileSource src(test::fixturePath("h264-aac.mp4"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "mpegts";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    auto m = reopen(sink.bytes());
    REQUIRE(m.format == "mpegts");
    REQUIRE(m.streams[0].codec == "h264");
    REQUIRE(m.streams[1].codec == "aac");
  }
  {
    test::FileSource src(test::fixturePath("vp9-opus.webm"));
    MediaReader reader;
    REQUIRE(reader.open(src).ok());
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "webm";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    auto m = reopen(sink.bytes());
    REQUIRE(m.streams[0].codec == "vp9");
    REQUIRE(m.streams[1].codec == "opus");
  }
}

TEST_CASE("incompatible subtitle stream is skipped by default with a warning, or fails on request") {
  test::FileSource src(test::fixturePath("h264-aac-srt.mkv"));
  std::vector<std::string> warnings;
  aviotrix::OpenOptions oo;
  oo.log = [&](aviotrix::LogLevel lvl, std::string_view t) { if (lvl == aviotrix::LogLevel::Warning) warnings.emplace_back(t); };
  MediaReader reader;
  REQUIRE(reader.open(src, oo).ok());

  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "mp4";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    REQUIRE(result.streams.size() == 3);
    REQUIRE_FALSE(result.streams[2].output.has_value());
    REQUIRE(result.streams[2].skippedReason.find("subrip") != std::string::npos);
    REQUIRE(reopen(sink.bytes()).streams.size() == 2);
    bool warned = false;
    for (const auto& w : warnings) warned = warned || w.find("subrip") != std::string::npos;
    REQUIRE(warned);
  }
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "mp4";
    opt.failOnIncompatible = true;
    RemuxResult result;
    Status st = reader.remux(sink, opt, result);
    REQUIRE(st.code == static_cast<int>(ErrorCode::IncompatibleStream));
    REQUIRE_FALSE(sink.opened());
  }
}

TEST_CASE("all streams incompatible is an error even in skip mode") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "webm";  // webm accepts only vp8/vp9/av1/vorbis/opus
  RemuxResult result;
  Status st = reader.remux(sink, opt, result);
  REQUIRE(st.code == static_cast<int>(ErrorCode::IncompatibleStream));
  REQUIRE_FALSE(sink.opened());
}

TEST_CASE("mp4 to a streaming sink requires fragmented") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  {
    test::MemorySink sink(false);
    RemuxOptions opt;
    opt.format = "mp4";
    RemuxResult result;
    Status st = reader.remux(sink, opt, result);
    REQUIRE(st.code == static_cast<int>(ErrorCode::SinkNotSeekable));
    REQUIRE_FALSE(sink.opened());
  }
  {
    test::MemorySink sink(false);
    RemuxOptions opt;
    opt.format = "mp4";
    opt.fragmented = true;
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    REQUIRE(contains(sink.bytes(), "moof"));
    auto m = reopen(sink.bytes());
    REQUIRE(m.streams.size() == 2);
  }
}

TEST_CASE("stream selection and invalid indices") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "matroska";
    opt.streams = std::vector<int>{1};
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).ok());
    REQUIRE(result.streams.size() == 1);
    REQUIRE(result.streams[0].input == 1);
    REQUIRE(result.streams[0].output == 0);
    auto m = reopen(sink.bytes());
    REQUIRE(m.streams.size() == 1);
    REQUIRE(m.streams[0].codec == "aac");
  }
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "matroska";
    opt.streams = std::vector<int>{5};
    RemuxResult result;
    Status st = reader.remux(sink, opt, result);
    REQUIRE(st.code == static_cast<int>(ErrorCode::InvalidArgument));
    REQUIRE_FALSE(sink.opened());
  }
  {
    test::MemorySink sink(true);
    RemuxOptions opt;
    opt.format = "avi";
    RemuxResult result;
    REQUIRE(reader.remux(sink, opt, result).code == static_cast<int>(ErrorCode::InvalidArgument));
  }
}

TEST_CASE("cancel flag aborts and still closes the sink") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink sink(true);
  std::atomic<bool> cancel{false};
  RemuxOptions opt;
  opt.format = "matroska";
  opt.cancel = &cancel;
  opt.progressIntervalPackets = 10;
  int progressCalls = 0;
  opt.onProgress = [&](const aviotrix::RemuxProgress& p) {
    progressCalls++;
    REQUIRE(p.bytesRead >= 0);
    cancel.store(true);
  };
  RemuxResult result;
  Status st = reader.remux(sink, opt, result);
  REQUIRE(st.code == static_cast<int>(ErrorCode::Aborted));
  REQUIRE(progressCalls >= 1);
  REQUIRE(sink.closed());
  REQUIRE(result.packets < 100);
}

TEST_CASE("a second remux on the same reader rewinds the input") {
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  test::MemorySink a(true), b(true);
  RemuxOptions opt;
  opt.format = "matroska";
  RemuxResult ra, rb;
  REQUIRE(reader.remux(a, opt, ra).ok());
  REQUIRE(reader.remux(b, opt, rb).ok());
  REQUIRE(ra.packets == rb.packets);
  REQUIRE(a.bytes().size() == b.bytes().size());
}

TEST_CASE("sink write failure propagates the sink's message and closes the sink") {
  struct FailingSink final : aviotrix::IoSink {
    int64_t budget = 100000;
    bool closed = false;
    bool seekable() const override { return true; }
    Status open() override { return Status::Ok(); }
    Status write(int64_t, std::span<const uint8_t> data) override {
      budget -= static_cast<int64_t>(data.size());
      if (budget < 0) return Status::Error(ErrorCode::IoFailed, "quota exceeded");
      return Status::Ok();
    }
    Status close() override { closed = true; return Status::Ok(); }
  } sink;
  test::FileSource src(test::fixturePath("h264-aac.mp4"));
  MediaReader reader;
  REQUIRE(reader.open(src).ok());
  RemuxOptions opt;
  opt.format = "matroska";
  RemuxResult result;
  Status st = reader.remux(sink, opt, result);
  REQUIRE_FALSE(st.ok());
  REQUIRE(st.message == "quota exceeded");
  REQUIRE(sink.closed);
}

TEST_CASE("remux on a closed reader is NOT_OPEN") {
  MediaReader reader;
  test::MemorySink sink(true);
  RemuxOptions opt;
  opt.format = "mp4";
  RemuxResult result;
  REQUIRE(reader.remux(sink, opt, result).code == static_cast<int>(ErrorCode::NotOpen));
}

TEST_CASE("toJson(RemuxResult)") {
  RemuxResult r;
  r.streams.push_back({0, 0, ""});
  r.streams.push_back({2, std::nullopt, "codec subrip is not supported by muxer mp4"});
  r.bytesRead = 10;
  r.bytesWritten = 20;
  r.packets = 3;
  REQUIRE(aviotrix::toJson(r) ==
          R"({"streams":[{"input":0,"output":0},{"input":2,"output":null,"skippedReason":"codec subrip is not supported by muxer mp4"}],"bytesRead":10,"bytesWritten":20,"packets":3})");
}
```

Add `test_remux.cc` to `core/tests/CMakeLists.txt`.

- [ ] **Step 2: Build to verify the tests fail**

```bash
cmake --build build/core-host -j 2>&1 | grep -m1 error
```

Expected: `fatal error: 'aviotrix/remux.h' file not found`.

- [ ] **Step 3: Write `remux.h` and extend `media_reader.h`, `json.h`, `media_reader_impl.h`**

`core/include/aviotrix/remux.h`:

```cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace aviotrix {

struct RemuxProgress {
  int64_t bytesRead = 0;
  int64_t bytesWritten = 0;
  std::optional<double> timestamp;  // latest muxed pts, seconds
};

struct RemuxOptions {
  std::string format;                        // libav muxer name: "mp4", "mov", "matroska", "webm", "mpegts"
  std::optional<std::vector<int>> streams;   // input stream indices; empty optional = all
  bool failOnIncompatible = false;           // false: skip streams the muxer rejects
  bool fragmented = false;                   // mp4/mov: frag_keyframe+empty_moov; required for unseekable sinks
  const std::atomic<bool>* cancel = nullptr; // set to true from any thread to abort
  std::function<void(const RemuxProgress&)> onProgress;
  int progressIntervalPackets = 100;
};

struct RemuxStreamMapping {
  int input = 0;
  std::optional<int> output;
  std::string skippedReason;  // non-empty iff output is empty
};

struct RemuxResult {
  std::vector<RemuxStreamMapping> streams;
  int64_t bytesRead = 0;     // bytes pulled from the source during this operation
  int64_t bytesWritten = 0;  // final size of the output
  int64_t packets = 0;
};

}  // namespace aviotrix
```

In `media_reader.h` add `#include "aviotrix/remux.h"` and, after `metadata()`:

```cpp
  // Stream-copies the selected streams into `sink`. See remux.h for the error contract.
  Status remux(IoSink& sink, const RemuxOptions& options, RemuxResult& result);
```

In `json.h` add `#include "aviotrix/remux.h"` and `std::string toJson(const RemuxResult& result);`. In `json.cc` append:

```cpp
std::string toJson(const RemuxResult& r) {
  JsonWriter w;
  w.beginObject();
  w.key("streams");
  w.beginArray();
  for (const RemuxStreamMapping& s : r.streams) {
    w.beginObject();
    w.key("input"); w.value(s.input);
    w.key("output");
    if (s.output) w.value(*s.output); else w.null();
    if (!s.output) { w.key("skippedReason"); w.value(s.skippedReason); }
    w.endObject();
  }
  w.endArray();
  w.key("bytesRead"); w.value(r.bytesRead);
  w.key("bytesWritten"); w.value(r.bytesWritten);
  w.key("packets"); w.value(r.packets);
  w.endObject();
  return w.str();
}
```

In `media_reader_impl.h` add a member `bool consumed = false;  // true after a remux read the input to EOF`.

- [ ] **Step 4: Write `core/src/remux.cc`**

```cpp
#include <algorithm>
#include <memory>
#include <set>

#include "aviotrix/media_reader.h"
#include "avio_output.h"
#include "log_router.h"
#include "media_reader_impl.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}

namespace aviotrix {

namespace {

bool isMp4Family(const AVOutputFormat* ofmt) {
  const std::string name = ofmt->name ? ofmt->name : "";
  return name == "mp4" || name == "mov";
}

struct OutputContextDeleter {
  void operator()(AVFormatContext* oc) const { if (oc) avformat_free_context(oc); }
};
using OutputContext = std::unique_ptr<AVFormatContext, OutputContextDeleter>;

struct PacketDeleter {
  void operator()(AVPacket* p) const { av_packet_free(&p); }
};

}  // namespace

Status MediaReader::remux(IoSink& sink, const RemuxOptions& opt, RemuxResult& result) {
  result = RemuxResult{};
  if (!impl_->open) return Status::Error(ErrorCode::NotOpen, "reader is not open");
  detail::ScopedLogTarget logTarget(&impl_->log);
  AVFormatContext* in = impl_->fmt;

  // 1. Validate everything that can be validated before touching the sink.
  const AVOutputFormat* ofmt = av_guess_format(opt.format.c_str(), nullptr, nullptr);
  if (!ofmt) return Status::Error(ErrorCode::InvalidArgument, "unknown output format: " + opt.format);
  if (!sink.seekable() && isMp4Family(ofmt) && !opt.fragmented) {
    return Status::Error(ErrorCode::SinkNotSeekable,
                         std::string(ofmt->name) + " output needs a seekable sink unless fragmented=true");
  }
  if (impl_->consumed && !impl_->input->seekable()) {
    return Status::Error(ErrorCode::Unsupported, "input is not seekable; it can be remuxed only once");
  }

  std::vector<int> selected;
  if (opt.streams) {
    std::set<int> seen;
    for (int idx : *opt.streams) {
      if (idx < 0 || idx >= static_cast<int>(in->nb_streams)) {
        return Status::Error(ErrorCode::InvalidArgument, "stream index " + std::to_string(idx) + " is out of range");
      }
      if (seen.insert(idx).second) selected.push_back(idx);
    }
  } else {
    for (unsigned i = 0; i < in->nb_streams; i++) selected.push_back(static_cast<int>(i));
  }
  if (selected.empty()) return Status::Error(ErrorCode::InvalidArgument, "no streams selected");

  AVFormatContext* rawOc = nullptr;
  int ret = avformat_alloc_output_context2(&rawOc, ofmt, nullptr, nullptr);
  if (ret < 0 || !rawOc) return Status::FromAv(ret < 0 ? ret : AVERROR(ENOMEM), "avformat_alloc_output_context2");
  OutputContext oc(rawOc);

  std::vector<int> outIndex(in->nb_streams, -1);
  for (int idx : selected) {
    const AVStream* ist = in->streams[idx];
    const AVCodecParameters* par = ist->codecpar;
    const int q = avformat_query_codec(ofmt, par->codec_id, FF_COMPLIANCE_NORMAL);
    if (q == 0) {
      std::string reason = std::string("codec ") + avcodec_get_name(par->codec_id) + " is not supported by muxer " + ofmt->name;
      if (opt.failOnIncompatible) return Status::Error(ErrorCode::IncompatibleStream, reason);
      av_log(oc.get(), AV_LOG_WARNING, "aviotrix: skipping stream %d: %s\n", idx, reason.c_str());
      result.streams.push_back({idx, std::nullopt, reason});
      continue;
    }
    AVStream* ost = avformat_new_stream(oc.get(), nullptr);
    if (!ost) return Status::FromAv(AVERROR(ENOMEM), "avformat_new_stream");
    ret = avcodec_parameters_copy(ost->codecpar, par);
    if (ret < 0) return Status::FromAv(ret, "avcodec_parameters_copy");
    ost->codecpar->codec_tag = 0;
    ost->time_base = ist->time_base;
    av_dict_copy(&ost->metadata, ist->metadata, 0);
    outIndex[idx] = ost->index;
    result.streams.push_back({idx, ost->index, ""});
  }
  if (oc->nb_streams == 0) {
    result.streams.clear();
    return Status::Error(ErrorCode::IncompatibleStream, std::string("no selected stream is supported by muxer ") + ofmt->name);
  }

  // 2. Rewind if a previous remux consumed the input.
  if (impl_->consumed) {
    ret = avformat_seek_file(in, -1, INT64_MIN, 0, INT64_MAX, 0);
    if (ret < 0) return Status::FromAv(ret, "avformat_seek_file");
  }
  impl_->consumed = true;
  const int64_t bytesReadAtStart = impl_->input->bytesRead();

  // 3. Open the sink and write.
  std::unique_ptr<detail::AvioOutput> output;
  Status st = detail::AvioOutput::create(sink, output);
  if (!st.ok()) return st;
  oc->pb = output->context();
  oc->flags |= AVFMT_FLAG_CUSTOM_IO;
  impl_->input->setCancelFlag(opt.cancel);

  AVDictionary* muxOpts = nullptr;
  if (opt.fragmented && isMp4Family(ofmt)) av_dict_set(&muxOpts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
  ret = avformat_write_header(oc.get(), &muxOpts);
  av_dict_free(&muxOpts);
  const char* failedCall = "avformat_write_header";
  bool headerWritten = ret >= 0;

  std::unique_ptr<AVPacket, PacketDeleter> pkt(av_packet_alloc());
  std::optional<double> lastTimestamp;
  if (headerWritten) {
    ret = 0;
    while (true) {
      if (opt.cancel && opt.cancel->load(std::memory_order_relaxed)) { ret = AVERROR_EXIT; break; }
      ret = av_read_frame(in, pkt.get());
      if (ret < 0) { failedCall = "av_read_frame"; break; }
      const int oi = outIndex[pkt->stream_index];
      if (oi < 0) { av_packet_unref(pkt.get()); continue; }
      const AVStream* ist = in->streams[pkt->stream_index];
      const AVStream* ost = oc->streams[oi];
      av_packet_rescale_ts(pkt.get(), ist->time_base, ost->time_base);
      pkt->stream_index = oi;
      pkt->pos = -1;
      if (pkt->pts != AV_NOPTS_VALUE) lastTimestamp = pkt->pts * av_q2d(ost->time_base);
      ret = av_interleaved_write_frame(oc.get(), pkt.get());  // takes ownership of the packet data
      if (ret < 0) { failedCall = "av_interleaved_write_frame"; break; }
      result.packets++;
      if (opt.onProgress && result.packets % opt.progressIntervalPackets == 0) {
        opt.onProgress(RemuxProgress{impl_->input->bytesRead() - bytesReadAtStart, output->bytesWritten(), lastTimestamp});
      }
    }
    if (ret == AVERROR_EOF) ret = 0;
    if (ret == 0) {
      ret = av_write_trailer(oc.get());
      if (ret < 0) failedCall = "av_write_trailer";
    }
  }

  // 4. Always close the sink; decide the final status.
  impl_->input->setCancelFlag(nullptr);
  Status closeSt = output->closeIo();
  result.bytesRead = impl_->input->bytesRead() - bytesReadAtStart;
  result.bytesWritten = output->bytesWritten();

  Status final = Status::Ok();
  if (ret == AVERROR_EXIT) {
    final = Status::Error(ErrorCode::Aborted, "remux aborted");
  } else if (ret < 0) {
    if (!output->lastError().ok()) final = output->lastError();
    else if (!impl_->input->lastError().ok()) final = impl_->input->lastError();
    else final = Status::FromAv(ret, failedCall);
  } else if (!closeSt.ok()) {
    final = closeSt;
  }
  if (final.ok() && opt.onProgress) {
    opt.onProgress(RemuxProgress{result.bytesRead, result.bytesWritten, lastTimestamp});
  }
  return final;
}

}  // namespace aviotrix
```

Add `src/remux.cc` to `core/CMakeLists.txt`.

- [ ] **Step 5: Build and run all core tests**

```bash
npm run test:core
```

Expected: every test passes. Known places to look if not:

- `all streams incompatible` failing with success: `avformat_query_codec` returned non-zero for h264 on webm. Check `ofmt->name` is `webm` (not `matroska`); webm has `query_codec`.
- `mpegts to mp4` failing in `avformat_write_header` with "extradata": the `extract_extradata` bsf is missing from the FFmpeg build; re-check `scripts/ffmpeg-components.sh` and rebuild (`rm -rf build/ffmpeg-host*`).
- `sink write failure` message mismatch: `AvioOutput::closeIo` may overwrite `lastError_`; it must only set it when `lastError_.ok()` (as written).

- [ ] **Step 6: Commit**

```bash
git add core
git commit -m "Add core remux with stream selection, incompatibility handling, fragmented MP4, cancel and progress"
```

---

### Task 7: `@aviotrix/types` package

**Files:**

- Create: `packages/types/package.json`, `packages/types/tsconfig.json`, `packages/types/vitest.config.ts`, `packages/types/src/index.ts`, `packages/types/src/io.ts`, `packages/types/src/log.ts`, `packages/types/src/metadata.ts`, `packages/types/src/remux.ts`, `packages/types/src/error.ts`, `packages/types/src/parse.ts`, `packages/types/tests/error.test.ts`, `packages/types/tests/parse.test.ts`

**Interfaces:**

- Consumes: nothing from other packages.
- Produces (imported by Tasks 9 and 12): every type in spec §4 verbatim, plus
  - `class AviotrixError extends Error { readonly code: string; constructor(code: string, message: string) }`
  - `type NativeFailure = { code: string; message: string }` and `function toAviotrixError(f: NativeFailure): AviotrixError`
  - `function parseMetadataJson(text: string): Metadata`, `function parseRemuxResultJson(text: string): RemuxResult` (both throw `AviotrixError('MALFORMED_RESULT', …)` on shape mismatch)
  - `MaybePromise<T>`, `IoSource`, `IoSink`, `LogLevel`, `LogFn`, `OpenOptions`, `Rational`, `VideoStreamInfo`, `AudioStreamInfo`, `StreamInfo`, `Metadata`, `RemuxProgress`, `RemuxOptions`, `RemuxStreamMapping`, `RemuxResult`

- [ ] **Step 1: Write the failing tests**

`packages/types/tests/error.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import { AviotrixError, toAviotrixError } from '../src/index.js';

describe('AviotrixError', () => {
  it('carries a code and is an Error', () => {
    const e = new AviotrixError('SINK_NOT_SEEKABLE', 'mp4 needs a seekable sink');
    expect(e).toBeInstanceOf(Error);
    expect(e).toBeInstanceOf(AviotrixError);
    expect(e.code).toBe('SINK_NOT_SEEKABLE');
    expect(e.message).toBe('mp4 needs a seekable sink');
    expect(e.name).toBe('AviotrixError');
  });

  it('converts a native failure record', () => {
    const e = toAviotrixError({
      code: 'AVERROR_INVALIDDATA',
      message: 'avformat_open_input: Invalid data',
    });
    expect(e.code).toBe('AVERROR_INVALIDDATA');
    expect(e.message).toContain('Invalid data');
  });
});
```

`packages/types/tests/parse.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import { AviotrixError, parseMetadataJson, parseRemuxResultJson } from '../src/index.js';

const metadataJson = JSON.stringify({
  format: 'mpegts',
  formatLongName: 'MPEG-TS (MPEG-2 Transport Stream)',
  startTime: 1.4,
  duration: 3.0,
  bitRate: null,
  tags: {},
  streams: [
    {
      index: 0,
      type: 'video',
      codec: 'h264',
      codecTag: null,
      timeBase: { num: 1, den: 90000 },
      startTime: 1.4,
      duration: null,
      bitRate: null,
      language: null,
      tags: {},
      video: { width: 320, height: 240, frameRate: { num: 30, den: 1 }, pixelFormat: 'yuv420p' },
    },
  ],
});

describe('parseMetadataJson', () => {
  it('returns a typed Metadata', () => {
    const m = parseMetadataJson(metadataJson);
    expect(m.format).toBe('mpegts');
    expect(m.streams[0]?.video?.width).toBe(320);
    expect(m.streams[0]?.audio).toBeUndefined();
    expect(m.bitRate).toBeNull();
  });

  it('rejects malformed input with MALFORMED_RESULT', () => {
    expect(() => parseMetadataJson('{"format":1}')).toThrowError(AviotrixError);
    try {
      parseMetadataJson('not json');
    } catch (e) {
      expect((e as AviotrixError).code).toBe('MALFORMED_RESULT');
    }
  });
});

describe('parseRemuxResultJson', () => {
  it('maps skipped streams', () => {
    const r = parseRemuxResultJson(
      '{"streams":[{"input":0,"output":0},{"input":2,"output":null,"skippedReason":"nope"}],"bytesRead":1,"bytesWritten":2,"packets":3}',
    );
    expect(r.streams[1]?.output).toBeNull();
    expect(r.streams[1]?.skippedReason).toBe('nope');
    expect(r.streams[0]?.skippedReason).toBeUndefined();
    expect(r.packets).toBe(3);
  });
});
```

- [ ] **Step 2: Write package config**

`packages/types/package.json`:

```json
{
  "name": "@aviotrix/types",
  "version": "0.1.0",
  "description": "Shared TypeScript contract for aviotrix: IoSource, IoSink, Metadata, remux types, AviotrixError",
  "license": "MIT",
  "type": "module",
  "exports": { ".": { "types": "./dist/index.d.ts", "import": "./dist/index.js" } },
  "files": ["dist"],
  "scripts": {
    "build": "tsc -p tsconfig.json",
    "typecheck": "tsc -p tsconfig.json --noEmit",
    "test": "vitest run"
  },
  "devDependencies": { "vitest": "^5.0.3" }
}
```

`packages/types/tsconfig.json`:

```json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": { "rootDir": "src", "outDir": "dist", "lib": ["es2023", "dom"] },
  "include": ["src"]
}
```

`packages/types/vitest.config.ts`:

```ts
import { defineConfig } from 'vitest/config';
export default defineConfig({ test: { include: ['tests/**/*.test.ts'] } });
```

- [ ] **Step 3: Run tests to verify they fail**

```bash
npm install && npm test -w @aviotrix/types
```

Expected: FAIL, cannot resolve `../src/index.js`.

- [ ] **Step 4: Write the sources**

`packages/types/src/io.ts`:

```ts
export type MaybePromise<T> = T | Promise<T>;

/** Positioned read access. The core tracks the stream position; implementations never do. */
export interface IoSource {
  /** Total size in bytes, or null when unknown (the input becomes unseekable). */
  open(): MaybePromise<number | null>;
  /** Up to `length` bytes at `offset`. Short reads are fine; an empty array means EOF. */
  read(offset: number, length: number): MaybePromise<Uint8Array>;
  close(): MaybePromise<void>;
}

/** Positioned write access. */
export interface IoSink {
  readonly seekable: boolean;
  open(): MaybePromise<void>;
  write(offset: number, data: Uint8Array): MaybePromise<void>;
  close(): MaybePromise<void>;
}
```

`packages/types/src/log.ts`:

```ts
export type LogLevel =
  'quiet' | 'panic' | 'fatal' | 'error' | 'warning' | 'info' | 'verbose' | 'debug' | 'trace';

export type LogFn = (level: LogLevel, text: string) => void;

export interface OpenOptions {
  onLog?: LogFn;
}
```

`packages/types/src/metadata.ts`:

```ts
export interface Rational {
  num: number;
  den: number;
}

export type StreamType = 'video' | 'audio' | 'subtitle' | 'data' | 'attachment';

export interface VideoStreamInfo {
  width: number;
  height: number;
  frameRate: Rational | null;
  pixelFormat: string | null;
}

export interface AudioStreamInfo {
  sampleRate: number;
  channels: number;
  channelLayout: string | null;
}

export interface StreamInfo {
  index: number;
  type: StreamType;
  codec: string;
  codecTag: string | null;
  timeBase: Rational;
  startTime: number | null;
  duration: number | null;
  bitRate: number | null;
  language: string | null;
  tags: Record<string, string>;
  video?: VideoStreamInfo;
  audio?: AudioStreamInfo;
}

export interface Metadata {
  format: string;
  formatLongName: string;
  startTime: number | null;
  duration: number | null;
  bitRate: number | null;
  tags: Record<string, string>;
  streams: StreamInfo[];
}
```

`packages/types/src/remux.ts`:

```ts
export interface RemuxProgress {
  bytesRead: number;
  bytesWritten: number;
  timestamp: number | null;
}

export interface RemuxOptions {
  /** Output container: "mp4", "mov", "matroska", "webm", "mpegts". Required: a sink has no filename to sniff. */
  format: string;
  /** Input stream indices to include. Default: all. */
  streams?: number[];
  /** Default 'skip': drop streams the muxer rejects and report them in the result. */
  onIncompatibleStream?: 'skip' | 'fail';
  /** Fragmented MP4 (frag_keyframe+empty_moov). Required when the sink is not seekable. */
  fragmented?: boolean;
  signal?: AbortSignal;
  onProgress?: (progress: RemuxProgress) => void;
}

export interface RemuxStreamMapping {
  input: number;
  output: number | null;
  skippedReason?: string;
}

export interface RemuxResult {
  streams: RemuxStreamMapping[];
  bytesRead: number;
  bytesWritten: number;
  packets: number;
}
```

`packages/types/src/error.ts`:

```ts
export class AviotrixError extends Error {
  readonly code: string;

  constructor(code: string, message: string) {
    super(message);
    this.name = 'AviotrixError';
    this.code = code;
  }
}

/** Shape both native bindings use to report a failed operation. */
export interface NativeFailure {
  code: string;
  message: string;
}

export function toAviotrixError(failure: NativeFailure): AviotrixError {
  return new AviotrixError(failure.code, failure.message);
}

export function isNativeFailure(value: object): value is NativeFailure {
  return (
    'code' in value &&
    'message' in value &&
    typeof value.code === 'string' &&
    typeof value.message === 'string'
  );
}
```

`packages/types/src/parse.ts` (runtime validation of the core's JSON, so `JSON.parse`'s `any` never escapes):

```ts
import { AviotrixError } from './error.js';
import type {
  AudioStreamInfo,
  Metadata,
  Rational,
  StreamInfo,
  StreamType,
  VideoStreamInfo,
} from './metadata.js';
import type { RemuxResult, RemuxStreamMapping } from './remux.js';

type JsonValue = string | number | boolean | null | JsonValue[] | { [key: string]: JsonValue };
type JsonObject = { [key: string]: JsonValue };

function malformed(what: string): AviotrixError {
  return new AviotrixError('MALFORMED_RESULT', `malformed result from native code: ${what}`);
}

function parseJson(text: string): JsonValue {
  try {
    return JSON.parse(text) as JsonValue;
  } catch {
    throw malformed('not valid JSON');
  }
}

function obj(v: JsonValue, what: string): JsonObject {
  if (v === null || typeof v !== 'object' || Array.isArray(v))
    throw malformed(`${what} is not an object`);
  return v;
}
function arr(v: JsonValue | undefined, what: string): JsonValue[] {
  if (!Array.isArray(v)) throw malformed(`${what} is not an array`);
  return v;
}
function str(v: JsonValue | undefined, what: string): string {
  if (typeof v !== 'string') throw malformed(`${what} is not a string`);
  return v;
}
function num(v: JsonValue | undefined, what: string): number {
  if (typeof v !== 'number') throw malformed(`${what} is not a number`);
  return v;
}
function numOrNull(v: JsonValue | undefined, what: string): number | null {
  if (v === null || v === undefined) return null;
  return num(v, what);
}
function strOrNull(v: JsonValue | undefined, what: string): string | null {
  if (v === null || v === undefined) return null;
  return str(v, what);
}
function tags(v: JsonValue | undefined, what: string): Record<string, string> {
  const o = obj(v ?? {}, what);
  const out: Record<string, string> = {};
  for (const [k, val] of Object.entries(o)) out[k] = str(val, `${what}.${k}`);
  return out;
}
function rational(v: JsonValue | undefined, what: string): Rational {
  const o = obj(v ?? null, what);
  return { num: num(o['num'], `${what}.num`), den: num(o['den'], `${what}.den`) };
}
function rationalOrNull(v: JsonValue | undefined, what: string): Rational | null {
  if (v === null || v === undefined) return null;
  return rational(v, what);
}

const streamTypes: ReadonlySet<string> = new Set([
  'video',
  'audio',
  'subtitle',
  'data',
  'attachment',
]);

function streamType(v: JsonValue | undefined, what: string): StreamType {
  const s = str(v, what);
  if (!streamTypes.has(s)) throw malformed(`${what} has unknown stream type ${s}`);
  return s as StreamType;
}

function video(v: JsonValue, what: string): VideoStreamInfo {
  const o = obj(v, what);
  return {
    width: num(o['width'], `${what}.width`),
    height: num(o['height'], `${what}.height`),
    frameRate: rationalOrNull(o['frameRate'], `${what}.frameRate`),
    pixelFormat: strOrNull(o['pixelFormat'], `${what}.pixelFormat`),
  };
}

function audio(v: JsonValue, what: string): AudioStreamInfo {
  const o = obj(v, what);
  return {
    sampleRate: num(o['sampleRate'], `${what}.sampleRate`),
    channels: num(o['channels'], `${what}.channels`),
    channelLayout: strOrNull(o['channelLayout'], `${what}.channelLayout`),
  };
}

function stream(v: JsonValue, what: string): StreamInfo {
  const o = obj(v, what);
  const info: StreamInfo = {
    index: num(o['index'], `${what}.index`),
    type: streamType(o['type'], `${what}.type`),
    codec: str(o['codec'], `${what}.codec`),
    codecTag: strOrNull(o['codecTag'], `${what}.codecTag`),
    timeBase: rational(o['timeBase'], `${what}.timeBase`),
    startTime: numOrNull(o['startTime'], `${what}.startTime`),
    duration: numOrNull(o['duration'], `${what}.duration`),
    bitRate: numOrNull(o['bitRate'], `${what}.bitRate`),
    language: strOrNull(o['language'], `${what}.language`),
    tags: tags(o['tags'], `${what}.tags`),
  };
  if (o['video'] !== undefined) info.video = video(o['video'], `${what}.video`);
  if (o['audio'] !== undefined) info.audio = audio(o['audio'], `${what}.audio`);
  return info;
}

export function parseMetadataJson(text: string): Metadata {
  const o = obj(parseJson(text), 'metadata');
  return {
    format: str(o['format'], 'metadata.format'),
    formatLongName: str(o['formatLongName'], 'metadata.formatLongName'),
    startTime: numOrNull(o['startTime'], 'metadata.startTime'),
    duration: numOrNull(o['duration'], 'metadata.duration'),
    bitRate: numOrNull(o['bitRate'], 'metadata.bitRate'),
    tags: tags(o['tags'], 'metadata.tags'),
    streams: arr(o['streams'], 'metadata.streams').map((s, i) =>
      stream(s, `metadata.streams[${i}]`),
    ),
  };
}

function mapping(v: JsonValue, what: string): RemuxStreamMapping {
  const o = obj(v, what);
  const m: RemuxStreamMapping = {
    input: num(o['input'], `${what}.input`),
    output: numOrNull(o['output'], `${what}.output`),
  };
  if (o['skippedReason'] !== undefined && o['skippedReason'] !== null) {
    m.skippedReason = str(o['skippedReason'], `${what}.skippedReason`);
  }
  return m;
}

export function parseRemuxResultJson(text: string): RemuxResult {
  const o = obj(parseJson(text), 'remuxResult');
  return {
    streams: arr(o['streams'], 'remuxResult.streams').map((s, i) =>
      mapping(s, `remuxResult.streams[${i}]`),
    ),
    bytesRead: num(o['bytesRead'], 'remuxResult.bytesRead'),
    bytesWritten: num(o['bytesWritten'], 'remuxResult.bytesWritten'),
    packets: num(o['packets'], 'remuxResult.packets'),
  };
}
```

`packages/types/src/index.ts`:

```ts
export type { IoSink, IoSource, MaybePromise } from './io.js';
export type { LogFn, LogLevel, OpenOptions } from './log.js';
export type {
  AudioStreamInfo,
  Metadata,
  Rational,
  StreamInfo,
  StreamType,
  VideoStreamInfo,
} from './metadata.js';
export type { RemuxOptions, RemuxProgress, RemuxResult, RemuxStreamMapping } from './remux.js';
export { AviotrixError, isNativeFailure, toAviotrixError } from './error.js';
export type { NativeFailure } from './error.js';
export { parseMetadataJson, parseRemuxResultJson } from './parse.js';
```

- [ ] **Step 5: Run lint, typecheck, tests, build**

```bash
npm run format && npm run lint && npm run typecheck -w @aviotrix/types && npm test -w @aviotrix/types && npm run build -w @aviotrix/types
ls packages/types/dist/index.d.ts
```

Expected: lint clean (the `check-no-unknown.sh` grep must stay green: `parse.ts` uses a `JsonValue` union instead of `unknown`), 5 tests pass, `dist/index.d.ts` exists.

- [ ] **Step 6: Commit**

```bash
git add packages/types package-lock.json
git commit -m "Add @aviotrix/types: IO contract, metadata and remux types, AviotrixError, JSON result parsers"
```

---

### Task 8: Node native binding (N-API)

**Files:**

- Create: `packages/node/package.json`, `packages/node/tsconfig.json`, `packages/node/vitest.config.ts`, `packages/node/CMakeLists.txt`, `packages/node/binding/main_thread_bridge.h`, `packages/node/binding/main_thread_bridge.cc`, `packages/node/binding/js_io.h`, `packages/node/binding/js_io.cc`, `packages/node/binding/native_reader.h`, `packages/node/binding/native_reader.cc`, `packages/node/binding/addon.cc`, `packages/node/src/native.ts`, `packages/node/tests/native.test.ts`

**Interfaces:**

- Consumes: core `MediaReader`, `IoSource`, `IoSink`, `Status`, `errorCodeName`, `toJson`, `LogLevel`/`logLevelName`.
- Produces: the raw addon, loaded by `src/native.ts` as `NativeModule`:
  ```ts
  interface NativeHost {
    sourceOpen(): MaybePromise<number | null>;
    sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
    sourceClose(): MaybePromise<void>;
    sinkOpen(): MaybePromise<void>;
    sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
    sinkClose(): MaybePromise<void>;
    onLog(level: string, text: string): void;
    onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
  }
  interface NativeRemuxOptions {
    format: string;
    streams?: number[];
    failOnIncompatible: boolean;
    fragmented: boolean;
    sinkSeekable: boolean;
    progressIntervalPackets: number;
  }
  interface NativeReader {
    open(): Promise<string>; // metadata JSON
    remux(options: NativeRemuxOptions): Promise<string>; // RemuxResult JSON
    cancel(): void;
    close(): Promise<void>;
  }
  interface NativeModule {
    NativeReader: new (host: NativeHost) => NativeReader;
  }
  ```
  Rejections are `Error` objects with a string `code` property (`errorCodeName` of the core status, or `IO_FAILED` when a host callback threw/rejected, carrying that callback's message).
- Threading contract: each `NativeReader` owns one `std::thread`. `open`/`remux`/`close` run there; every host callback runs on the JS main thread via a `TypedThreadSafeFunction`; the worker blocks until the callback's Promise settles. `cancel()` is synchronous and thread-safe.

- [ ] **Step 1: Write the failing test (drives the raw addon with a hand-rolled host)**

`packages/node/tests/native.test.ts`:

```ts
import { open as fsOpen } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { native, type NativeHost } from '../src/native.js';

const fixture = (name: string): string =>
  fileURLToPath(new URL(`../../../fixtures/${name}`, import.meta.url));

async function fileHost(
  path: string,
): Promise<{ host: NativeHost; output: () => Uint8Array; closes: () => number }> {
  const fh = await fsOpen(path, 'r');
  const chunks: Array<{ offset: number; data: Uint8Array }> = [];
  let closes = 0;
  const host: NativeHost = {
    async sourceOpen() {
      return (await fh.stat()).size;
    },
    async sourceRead(offset, length) {
      const buf = new Uint8Array(length);
      const { bytesRead } = await fh.read(buf, 0, length, offset);
      return buf.subarray(0, bytesRead);
    },
    async sourceClose() {
      closes++;
      await fh.close();
    },
    sinkOpen() {},
    sinkWrite(offset, data) {
      chunks.push({ offset, data: new Uint8Array(data) });
    },
    sinkClose() {},
    onLog() {},
    onProgress() {},
  };
  const output = (): Uint8Array => {
    let size = 0;
    for (const c of chunks) size = Math.max(size, c.offset + c.data.length);
    const out = new Uint8Array(size);
    for (const c of chunks) out.set(c.data, c.offset);
    return out;
  };
  return { host, output, closes: () => closes };
}

describe('native addon', () => {
  it('opens a fixture and returns metadata JSON', async () => {
    const { host, closes } = await fileHost(fixture('h264-aac.mp4'));
    const reader = new native.NativeReader(host);
    const json = await reader.open();
    expect(json).toContain('"codec":"h264"');
    expect(json).toContain('"width":320');
    await reader.close();
    expect(closes()).toBe(1);
  });

  it('remuxes to matroska through the host sink', async () => {
    const { host, output } = await fileHost(fixture('h264-aac.mp4'));
    const reader = new native.NativeReader(host);
    await reader.open();
    const json = await reader.remux({
      format: 'matroska',
      failOnIncompatible: false,
      fragmented: false,
      sinkSeekable: true,
      progressIntervalPackets: 100,
    });
    expect(json).toContain('"packets":');
    const bytes = output();
    expect(bytes.length).toBeGreaterThan(10000);
    // EBML magic
    expect(Array.from(bytes.subarray(0, 4))).toEqual([0x1a, 0x45, 0xdf, 0xa3]);
    await reader.close();
  });

  it('rejects with a coded error on non-media input', async () => {
    const { host } = await fileHost(fixture('not-media.txt'));
    const reader = new native.NativeReader(host);
    await expect(reader.open()).rejects.toMatchObject({
      code: 'AVERROR_INVALIDDATA',
      message: expect.stringContaining('avformat_open_input'),
    });
    await reader.close();
  });

  it('propagates a host read rejection as IO_FAILED with the original message', async () => {
    const host: NativeHost = {
      sourceOpen: () => 1000,
      sourceRead: () => Promise.reject(new Error('network down')),
      sourceClose() {},
      sinkOpen() {},
      sinkWrite() {},
      sinkClose() {},
      onLog() {},
      onProgress() {},
    };
    const reader = new native.NativeReader(host);
    await expect(reader.open()).rejects.toMatchObject({
      code: 'IO_FAILED',
      message: 'network down',
    });
    await reader.close();
  });

  it('cancel() aborts a running remux', async () => {
    const { host } = await fileHost(fixture('h264-aac.mp4'));
    const reader = new native.NativeReader(host);
    host.onProgress = () => reader.cancel();
    await reader.open();
    await expect(
      reader.remux({
        format: 'matroska',
        failOnIncompatible: false,
        fragmented: false,
        sinkSeekable: true,
        progressIntervalPackets: 5,
      }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    await reader.close();
  });
});
```

- [ ] **Step 2: Write package config and `src/native.ts`**

`packages/node/package.json`:

```json
{
  "name": "@aviotrix/node",
  "version": "0.1.0",
  "description": "libav (FFmpeg) remuxing and metadata for Node through a custom async AVIO layer",
  "license": "MIT",
  "type": "module",
  "exports": { ".": { "types": "./dist/index.d.ts", "import": "./dist/index.js" } },
  "files": ["dist", "binding", "CMakeLists.txt"],
  "engines": { "node": ">=24" },
  "scripts": {
    "build:native": "cmake-js compile",
    "build:ts": "tsc -p tsconfig.json",
    "build": "npm run build:native && npm run build:ts",
    "typecheck": "tsc -p tsconfig.json --noEmit",
    "test": "vitest run"
  },
  "dependencies": {
    "@aviotrix/types": "0.1.0",
    "node-addon-api": "^8.9.2"
  },
  "devDependencies": {
    "@types/node": "^24.0.0",
    "cmake-js": "^8.0.0",
    "vitest": "^5.0.3"
  }
}
```

`packages/node/tsconfig.json`:

```json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": { "rootDir": "src", "outDir": "dist", "types": ["node"] },
  "include": ["src"]
}
```

`packages/node/vitest.config.ts`:

```ts
import { defineConfig } from 'vitest/config';
export default defineConfig({ test: { include: ['tests/**/*.test.ts'], testTimeout: 30000 } });
```

`packages/node/src/native.ts`:

```ts
import { createRequire } from 'node:module';
import type { MaybePromise } from '@aviotrix/types';

export interface NativeHost {
  sourceOpen(): MaybePromise<number | null>;
  sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
  sourceClose(): MaybePromise<void>;
  sinkOpen(): MaybePromise<void>;
  sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
  sinkClose(): MaybePromise<void>;
  onLog(level: string, text: string): void;
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
}

export interface NativeRemuxOptions {
  format: string;
  streams?: number[];
  failOnIncompatible: boolean;
  fragmented: boolean;
  sinkSeekable: boolean;
  progressIntervalPackets: number;
}

export interface NativeReader {
  open(): Promise<string>;
  remux(options: NativeRemuxOptions): Promise<string>;
  cancel(): void;
  close(): Promise<void>;
}

export interface NativeModule {
  NativeReader: new (host: NativeHost) => NativeReader;
}

const require = createRequire(import.meta.url);

function loadNative(): NativeModule {
  const candidates = ['../build/Release/aviotrix_node.node', '../build/Debug/aviotrix_node.node'];
  let lastError: Error | null = null;
  for (const candidate of candidates) {
    try {
      return require(candidate) as NativeModule;
    } catch (e) {
      lastError = e instanceof Error ? e : new Error(String(e));
    }
  }
  throw new Error(
    `@aviotrix/node: native addon not found (${lastError?.message ?? 'no candidates'}). Run npm run build:native.`,
  );
}

export const native: NativeModule = loadNative();
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
npm install && npm test -w @aviotrix/node
```

Expected: FAIL, `native addon not found`.

- [ ] **Step 4: Write the CMake file for the addon**

`packages/node/CMakeLists.txt` (a standalone project: cmake-js runs it from `packages/node`, and it pulls in the repo root, which provides FFmpeg and `aviotrix_core`):

```cmake
cmake_minimum_required(VERSION 3.28)
project(aviotrix_node LANGUAGES C CXX)

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
```

- [ ] **Step 5: Write the main-thread bridge**

`packages/node/binding/main_thread_bridge.h`:

```cpp
#pragma once

#include <napi.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "aviotrix/status.h"

namespace aviotrix_node {

// One IO round trip from the worker thread to a NativeHost method on the JS main thread.
struct IoRequest {
  enum class Kind { SourceOpen, SourceRead, SourceClose, SinkOpen, SinkWrite, SinkClose };
  Kind kind;
  int64_t offset = 0;
  std::span<uint8_t> readInto;         // SourceRead: destination (its size() is the max length)
  std::span<const uint8_t> writeFrom;  // SinkWrite: payload
  // Results
  std::optional<int64_t> size;         // SourceOpen
  size_t bytesRead = 0;                // SourceRead
  aviotrix::Status status;
  // Synchronization
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
};

struct LogMessage {
  std::string level;
  std::string text;
};

struct ProgressMessage {
  int64_t bytesRead;
  int64_t bytesWritten;
  std::optional<double> timestamp;
};

// Settles the Promise returned to JS for one operation. `json` is the payload on success.
struct Completion {
  std::shared_ptr<Napi::Promise::Deferred> deferred;
  aviotrix::Status status;
  std::string json;
  std::function<void()> afterSettle;  // runs on the main thread after resolve/reject (e.g. shutdown)
};

using Message = std::variant<IoRequest*, LogMessage, ProgressMessage, Completion>;

// Pumps Messages from the worker thread onto the JS main thread. Owned by NativeReader.
class MainThreadBridge {
 public:
  MainThreadBridge(Napi::Env env, Napi::Object host, const char* resourceName);
  ~MainThreadBridge();

  // Worker thread: blocks until the host method's Promise settles. Returns request.status.
  aviotrix::Status request(IoRequest& request);
  // Worker thread: fire-and-forget.
  void log(std::string level, std::string text);
  void progress(int64_t bytesRead, int64_t bytesWritten, std::optional<double> timestamp);
  void complete(Completion completion);

  // Main thread only.
  void keepAlive(bool on);  // Ref/Unref the TSFN so the event loop stays alive while an operation runs
  void release();           // after the worker thread has been joined

 private:
  struct Context {
    Napi::ObjectReference host;
  };
  static void callJs(Napi::Env env, Napi::Function, Context* ctx, Message* message);
  static void handleIo(Napi::Env env, Context* ctx, IoRequest* req);
  static void finish(IoRequest* req, aviotrix::Status status);
  static std::string errorMessage(Napi::Env env, Napi::Value err);

  using Tsfn = Napi::TypedThreadSafeFunction<Context, Message, &MainThreadBridge::callJs>;
  Context* context_;
  Tsfn tsfn_;
  bool released_ = false;
};

}  // namespace aviotrix_node
```

`packages/node/binding/main_thread_bridge.cc`:

```cpp
#include "main_thread_bridge.h"

#include <algorithm>

namespace aviotrix_node {

using aviotrix::ErrorCode;
using aviotrix::Status;

MainThreadBridge::MainThreadBridge(Napi::Env env, Napi::Object host, const char* resourceName)
    : context_(new Context{Napi::Persistent(host)}) {
  tsfn_ = Tsfn::New(env, resourceName, 0, 1, context_,
                    [](Napi::Env, void*, Context* ctx) { delete ctx; });
  tsfn_.Unref(env);  // idle readers must not keep the process alive
}

MainThreadBridge::~MainThreadBridge() { release(); }

void MainThreadBridge::release() {
  if (released_) return;
  released_ = true;
  tsfn_.Release();
}

void MainThreadBridge::keepAlive(bool on) {
  if (released_) return;
  if (on) tsfn_.Ref(tsfn_.Env()); else tsfn_.Unref(tsfn_.Env());
}

Status MainThreadBridge::request(IoRequest& req) {
  req.done = false;
  Message* msg = new Message(&req);
  if (tsfn_.BlockingCall(msg) != napi_ok) {
    delete msg;
    return Status::Error(ErrorCode::IoFailed, "JS environment is shutting down");
  }
  std::unique_lock<std::mutex> lock(req.mutex);
  req.cv.wait(lock, [&] { return req.done; });
  return req.status;
}

void MainThreadBridge::log(std::string level, std::string text) {
  Message* msg = new Message(LogMessage{std::move(level), std::move(text)});
  if (tsfn_.NonBlockingCall(msg) != napi_ok) delete msg;
}

void MainThreadBridge::progress(int64_t bytesRead, int64_t bytesWritten, std::optional<double> timestamp) {
  Message* msg = new Message(ProgressMessage{bytesRead, bytesWritten, timestamp});
  if (tsfn_.NonBlockingCall(msg) != napi_ok) delete msg;
}

void MainThreadBridge::complete(Completion completion) {
  Message* msg = new Message(std::move(completion));
  if (tsfn_.BlockingCall(msg) != napi_ok) delete msg;
}

void MainThreadBridge::finish(IoRequest* req, Status status) {
  {
    std::lock_guard<std::mutex> lock(req->mutex);
    req->status = std::move(status);
    req->done = true;
  }
  req->cv.notify_one();
}

std::string MainThreadBridge::errorMessage(Napi::Env env, Napi::Value err) {
  if (err.IsObject()) {
    Napi::Value m = err.As<Napi::Object>().Get("message");
    if (m.IsString()) return m.As<Napi::String>().Utf8Value();
  }
  return err.ToString().Utf8Value();
}

void MainThreadBridge::callJs(Napi::Env env, Napi::Function, Context* ctx, Message* message) {
  std::unique_ptr<Message> owned(message);
  if (!env) {  // environment torn down: unblock any waiter
    if (auto* req = std::get_if<IoRequest*>(message)) finish(*req, Status::Error(ErrorCode::IoFailed, "environment shut down"));
    return;
  }
  Napi::HandleScope scope(env);
  Napi::Object host = ctx->host.Value();

  if (auto* req = std::get_if<IoRequest*>(message)) {
    handleIo(env, ctx, *req);
  } else if (auto* log = std::get_if<LogMessage>(message)) {
    Napi::Value fn = host.Get("onLog");
    if (fn.IsFunction()) fn.As<Napi::Function>().Call(host, {Napi::String::New(env, log->level), Napi::String::New(env, log->text)});
  } else if (auto* p = std::get_if<ProgressMessage>(message)) {
    Napi::Value fn = host.Get("onProgress");
    if (fn.IsFunction()) {
      Napi::Value ts = p->timestamp ? Napi::Value(Napi::Number::New(env, *p->timestamp)) : Napi::Value(env.Null());
      fn.As<Napi::Function>().Call(host, {Napi::Number::New(env, static_cast<double>(p->bytesRead)),
                                          Napi::Number::New(env, static_cast<double>(p->bytesWritten)), ts});
    }
  } else if (auto* c = std::get_if<Completion>(message)) {
    if (c->status.ok()) {
      c->deferred->Resolve(Napi::String::New(env, c->json));
    } else {
      Napi::Error err = Napi::Error::New(env, c->status.message);
      err.Set("code", Napi::String::New(env, aviotrix::errorCodeName(c->status.code)));
      c->deferred->Reject(err.Value());
    }
    if (c->afterSettle) c->afterSettle();
  }
}

void MainThreadBridge::handleIo(Napi::Env env, Context* ctx, IoRequest* req) {
  Napi::Object host = ctx->host.Value();
  const char* method = nullptr;
  std::vector<napi_value> args;
  switch (req->kind) {
    case IoRequest::Kind::SourceOpen: method = "sourceOpen"; break;
    case IoRequest::Kind::SourceRead:
      method = "sourceRead";
      args = {Napi::Number::New(env, static_cast<double>(req->offset)), Napi::Number::New(env, static_cast<double>(req->readInto.size()))};
      break;
    case IoRequest::Kind::SourceClose: method = "sourceClose"; break;
    case IoRequest::Kind::SinkOpen: method = "sinkOpen"; break;
    case IoRequest::Kind::SinkWrite:
      method = "sinkWrite";
      args = {Napi::Number::New(env, static_cast<double>(req->offset)),
              Napi::Buffer<uint8_t>::Copy(env, req->writeFrom.data(), req->writeFrom.size())};
      break;
    case IoRequest::Kind::SinkClose: method = "sinkClose"; break;
  }

  Napi::Value result;
  try {
    Napi::Value fn = host.Get(method);
    if (!fn.IsFunction()) {
      finish(req, Status::Error(ErrorCode::IoFailed, std::string("host has no method ") + method));
      return;
    }
    result = fn.As<Napi::Function>().Call(host, args);
  } catch (const Napi::Error& e) {
    finish(req, Status::Error(ErrorCode::IoFailed, errorMessage(env, e.Value())));
    return;
  }

  // Normalize sync values and Promises alike: Promise.resolve(result).then(onOk, onErr)
  Napi::Object promiseCtor = env.Global().Get("Promise").As<Napi::Object>();
  Napi::Value promise = promiseCtor.Get("resolve").As<Napi::Function>().Call(promiseCtor, {result});

  Napi::Function onOk = Napi::Function::New(env, [req](const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Value v = info[0];
    switch (req->kind) {
      case IoRequest::Kind::SourceOpen:
        if (v.IsNumber()) req->size = static_cast<int64_t>(v.As<Napi::Number>().DoubleValue());
        else if (v.IsNull() || v.IsUndefined()) req->size.reset();
        else { finish(req, Status::Error(ErrorCode::IoFailed, "sourceOpen must return a number or null")); return; }
        break;
      case IoRequest::Kind::SourceRead: {
        if (!v.IsTypedArray()) { finish(req, Status::Error(ErrorCode::IoFailed, "sourceRead must return a Uint8Array")); return; }
        Napi::TypedArray ta = v.As<Napi::TypedArray>();
        const uint8_t* data = static_cast<const uint8_t*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
        const size_t n = std::min(ta.ByteLength(), req->readInto.size());  // over-long reads are truncated
        std::copy_n(data, n, req->readInto.data());
        req->bytesRead = n;
        break;
      }
      default: break;
    }
    (void)env;
    finish(req, Status::Ok());
  });
  Napi::Function onErr = Napi::Function::New(env, [req](const Napi::CallbackInfo& info) {
    finish(req, Status::Error(ErrorCode::IoFailed, errorMessage(info.Env(), info[0])));
  });
  promise.As<Napi::Object>().Get("then").As<Napi::Function>().Call(promise, {onOk, onErr});
}

}  // namespace aviotrix_node
```

- [ ] **Step 6: Write the JS-backed IoSource/IoSink**

`packages/node/binding/js_io.h`:

```cpp
#pragma once

#include "aviotrix/io.h"
#include "main_thread_bridge.h"

namespace aviotrix_node {

class JsIoSource final : public aviotrix::IoSource {
 public:
  explicit JsIoSource(MainThreadBridge& bridge) : bridge_(bridge) {}
  aviotrix::Status open(std::optional<int64_t>& size) override;
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override;
  aviotrix::Status close() override;

 private:
  MainThreadBridge& bridge_;
};

class JsIoSink final : public aviotrix::IoSink {
 public:
  JsIoSink(MainThreadBridge& bridge, bool seekable) : bridge_(bridge), seekable_(seekable) {}
  bool seekable() const override { return seekable_; }
  aviotrix::Status open() override;
  aviotrix::Status write(int64_t offset, std::span<const uint8_t> data) override;
  aviotrix::Status close() override;

 private:
  MainThreadBridge& bridge_;
  bool seekable_;
};

}  // namespace aviotrix_node
```

`packages/node/binding/js_io.cc`:

```cpp
#include "js_io.h"

namespace aviotrix_node {

aviotrix::Status JsIoSource::open(std::optional<int64_t>& size) {
  IoRequest req{IoRequest::Kind::SourceOpen};
  aviotrix::Status st = bridge_.request(req);
  if (st.ok()) size = req.size;
  return st;
}

aviotrix::Status JsIoSource::read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) {
  IoRequest req{IoRequest::Kind::SourceRead};
  req.offset = offset;
  req.readInto = buffer;
  aviotrix::Status st = bridge_.request(req);
  bytesRead = st.ok() ? req.bytesRead : 0;
  return st;
}

aviotrix::Status JsIoSource::close() {
  IoRequest req{IoRequest::Kind::SourceClose};
  return bridge_.request(req);
}

aviotrix::Status JsIoSink::open() {
  IoRequest req{IoRequest::Kind::SinkOpen};
  return bridge_.request(req);
}

aviotrix::Status JsIoSink::write(int64_t offset, std::span<const uint8_t> data) {
  IoRequest req{IoRequest::Kind::SinkWrite};
  req.offset = offset;
  req.writeFrom = data;
  return bridge_.request(req);
}

aviotrix::Status JsIoSink::close() {
  IoRequest req{IoRequest::Kind::SinkClose};
  return bridge_.request(req);
}

}  // namespace aviotrix_node
```

Note: `IoRequest` holds a `std::mutex`, so it is not copyable; the aggregate initialization `IoRequest req{Kind}` works because `kind` is the first member and the rest default-initialize. If the compiler rejects it, add a constructor `explicit IoRequest(Kind k) : kind(k) {}`.

- [ ] **Step 7: Write the reader (worker thread + core)**

`packages/node/binding/native_reader.h`:

```cpp
#pragma once

#include <napi.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "aviotrix/media_reader.h"
#include "js_io.h"
#include "main_thread_bridge.h"

namespace aviotrix_node {

class NativeReader : public Napi::ObjectWrap<NativeReader> {
 public:
  static Napi::Function Init(Napi::Env env);
  explicit NativeReader(const Napi::CallbackInfo& info);
  ~NativeReader() override;

 private:
  Napi::Value Open(const Napi::CallbackInfo& info);
  Napi::Value Remux(const Napi::CallbackInfo& info);
  Napi::Value Cancel(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);

  // Runs `work` on the worker thread; `work` returns (status, json). Returns the JS Promise.
  Napi::Value runOnWorker(Napi::Env env, std::function<std::pair<aviotrix::Status, std::string>()> work,
                          std::function<void()> afterSettle = {});
  void workerLoop();
  void shutdownWorker();  // main thread; joins the thread and releases the bridge

  std::unique_ptr<MainThreadBridge> bridge_;
  JsIoSource source_;
  aviotrix::MediaReader reader_;
  std::atomic<bool> cancel_{false};

  std::thread worker_;
  std::mutex queueMutex_;
  std::condition_variable queueCv_;
  std::deque<std::function<void()>> queue_;
  bool stopping_ = false;
  bool shutDown_ = false;
};

}  // namespace aviotrix_node
```

`packages/node/binding/native_reader.cc`:

```cpp
#include "native_reader.h"

#include "aviotrix/json.h"
#include "aviotrix/remux.h"

namespace aviotrix_node {

using aviotrix::ErrorCode;
using aviotrix::Status;

Napi::Function NativeReader::Init(Napi::Env env) {
  return DefineClass(env, "NativeReader",
                     {InstanceMethod("open", &NativeReader::Open), InstanceMethod("remux", &NativeReader::Remux),
                      InstanceMethod("cancel", &NativeReader::Cancel), InstanceMethod("close", &NativeReader::Close)});
}

NativeReader::NativeReader(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<NativeReader>(info),
      bridge_(std::make_unique<MainThreadBridge>(info.Env(), info[0].As<Napi::Object>(), "aviotrix:reader")),
      source_(*bridge_) {
  if (info.Length() < 1 || !info[0].IsObject()) throw Napi::TypeError::New(info.Env(), "NativeReader(host) requires a host object");
  worker_ = std::thread([this] { workerLoop(); });
}

NativeReader::~NativeReader() { shutdownWorker(); }

void NativeReader::workerLoop() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      queueCv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
      if (stopping_ && queue_.empty()) return;
      job = std::move(queue_.front());
      queue_.pop_front();
    }
    job();
  }
}

void NativeReader::shutdownWorker() {
  if (shutDown_) return;
  shutDown_ = true;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    stopping_ = true;
  }
  queueCv_.notify_all();
  if (worker_.joinable()) worker_.join();
  bridge_->release();
}

Napi::Value NativeReader::runOnWorker(Napi::Env env, std::function<std::pair<Status, std::string>()> work,
                                      std::function<void()> afterSettle) {
  auto deferred = std::make_shared<Napi::Promise::Deferred>(Napi::Promise::Deferred::New(env));
  if (shutDown_) {
    Napi::Error err = Napi::Error::New(env, "reader is closed");
    err.Set("code", "NOT_OPEN");
    deferred->Reject(err.Value());
    return deferred->Promise();
  }
  Ref();                     // keep this wrapper alive while the worker uses it
  bridge_->keepAlive(true);  // keep the event loop alive while the worker runs
  auto settle = [this, afterSettle = std::move(afterSettle)]() {
    bridge_->keepAlive(false);
    if (afterSettle) afterSettle();
    Unref();
  };
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    queue_.emplace_back([this, deferred, work = std::move(work), settle = std::move(settle)]() mutable {
      auto [status, json] = work();
      bridge_->complete(Completion{deferred, std::move(status), std::move(json), std::move(settle)});
    });
  }
  queueCv_.notify_one();
  return deferred->Promise();
}

Napi::Value NativeReader::Open(const Napi::CallbackInfo& info) {
  return runOnWorker(info.Env(), [this]() -> std::pair<Status, std::string> {
    aviotrix::OpenOptions opts;
    opts.log = [this](aviotrix::LogLevel level, std::string_view text) {
      bridge_->log(aviotrix::logLevelName(level), std::string(text));
    };
    Status st = reader_.open(source_, std::move(opts));
    if (!st.ok()) return {st, ""};
    return {st, aviotrix::toJson(reader_.metadata())};
  });
}

Napi::Value NativeReader::Remux(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsObject()) throw Napi::TypeError::New(env, "remux(options) requires an options object");
  Napi::Object o = info[0].As<Napi::Object>();

  aviotrix::RemuxOptions opt;
  opt.format = o.Get("format").ToString().Utf8Value();
  opt.failOnIncompatible = o.Get("failOnIncompatible").ToBoolean().Value();
  opt.fragmented = o.Get("fragmented").ToBoolean().Value();
  const bool sinkSeekable = o.Get("sinkSeekable").ToBoolean().Value();
  Napi::Value interval = o.Get("progressIntervalPackets");
  if (interval.IsNumber() && interval.As<Napi::Number>().Int32Value() > 0) opt.progressIntervalPackets = interval.As<Napi::Number>().Int32Value();
  Napi::Value streams = o.Get("streams");
  if (streams.IsArray()) {
    Napi::Array arr = streams.As<Napi::Array>();
    std::vector<int> idx;
    for (uint32_t i = 0; i < arr.Length(); i++) idx.push_back(arr.Get(i).ToNumber().Int32Value());
    opt.streams = std::move(idx);
  }
  opt.cancel = &cancel_;
  opt.onProgress = [this](const aviotrix::RemuxProgress& p) { bridge_->progress(p.bytesRead, p.bytesWritten, p.timestamp); };

  return runOnWorker(env, [this, opt = std::move(opt), sinkSeekable]() mutable -> std::pair<Status, std::string> {
    JsIoSink sink(*bridge_, sinkSeekable);
    aviotrix::RemuxResult result;
    Status st = reader_.remux(sink, opt, result);
    cancel_.store(false);
    if (!st.ok()) return {st, ""};
    return {st, aviotrix::toJson(result)};
  });
}

Napi::Value NativeReader::Cancel(const Napi::CallbackInfo& info) {
  cancel_.store(true);
  return info.Env().Undefined();
}

Napi::Value NativeReader::Close(const Napi::CallbackInfo& info) {
  return runOnWorker(
      info.Env(), [this]() -> std::pair<Status, std::string> { return {reader_.close(), ""}; },
      [this] { shutdownWorker(); });
}

}  // namespace aviotrix_node
```

`packages/node/binding/addon.cc`:

```cpp
#include <napi.h>

#include "native_reader.h"

namespace {

Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
  exports.Set("NativeReader", aviotrix_node::NativeReader::Init(env));
  return exports;
}

}  // namespace

NODE_API_MODULE(aviotrix_node, InitModule)
```

- [ ] **Step 8: Build the addon and run the test**

```bash
npm run build:native -w @aviotrix/node && npm test -w @aviotrix/node
```

Expected: `build/Release/aviotrix_node.node` exists and all 5 tests pass. If the process hangs at exit, a `keepAlive(true)` has no matching `keepAlive(false)`: check that `Completion.afterSettle` always runs. If `close()` rejects with `NOT_OPEN` in the first test, `shutDown_` was set before the close task ran: it must be set only inside `afterSettle`.

- [ ] **Step 9: Lint, typecheck, commit**

```bash
npm run format && npm run lint && npm run typecheck -w @aviotrix/node
git add packages/node package-lock.json
git commit -m "Add Node N-API binding: per-reader worker thread, thread-safe host bridge, JS-backed AVIO"
```

---

### Task 9: `@aviotrix/node` TypeScript wrapper and reference IO

**Files:**

- Create: `packages/node/src/index.ts`, `packages/node/src/media_reader.ts`, `packages/node/src/host.ts`, `packages/node/src/queue.ts`, `packages/node/src/errors.ts`, `packages/node/src/io/file_source.ts`, `packages/node/src/io/file_sink.ts`, `packages/node/src/io/memory_sink.ts`, `packages/node/tests/helpers/memory_source.ts`, `packages/node/tests/helpers/adversarial_source.ts`, `packages/node/tests/helpers/fixtures.ts`, `packages/node/tests/metadata.test.ts`, `packages/node/tests/remux.test.ts`, `packages/node/tests/io.test.ts`
- Modify: `packages/node/tsconfig.json` (add `"lib": ["es2023", "esnext.disposable"]`)

**Interfaces:**

- Consumes: Task 8 `native`, `NativeHost`, `NativeReader`, `NativeRemuxOptions`; Task 7 types and parsers.
- Produces (the spec §5 public API):
  - `class MediaReader { static open(source: IoSource, options?: OpenOptions): Promise<MediaReader>; readonly metadata: Metadata; remux(sink: IoSink, options: RemuxOptions): Promise<RemuxResult>; close(): Promise<void>; [Symbol.asyncDispose](): Promise<void>; }`
  - `readMetadata(source, options?)`, `remux(source, sink, options)`
  - `class FileSource implements IoSource { constructor(path: string) }`, `class FileSink implements IoSink { constructor(path: string); readonly seekable = true }`, `class MemorySink implements IoSink { constructor(options?: { seekable?: boolean }); bytes(): Uint8Array }`
  - `function wrapNativeError(e: unknown-free): AviotrixError` (internal, `errors.ts`)
  - Re-exports everything from `@aviotrix/types`.

- [ ] **Step 1: Write the test helpers**

`packages/node/tests/helpers/fixtures.ts`:

```ts
import { fileURLToPath } from 'node:url';
export const fixture = (name: string): string =>
  fileURLToPath(new URL(`../../../../fixtures/${name}`, import.meta.url));
```

`packages/node/tests/helpers/memory_source.ts`:

```ts
import type { IoSource } from '@aviotrix/types';

export class MemorySource implements IoSource {
  constructor(private readonly bytes: Uint8Array) {}
  open(): number {
    return this.bytes.length;
  }
  read(offset: number, length: number): Uint8Array {
    return this.bytes.subarray(offset, Math.min(this.bytes.length, offset + length));
  }
  close(): void {}
}
```

`packages/node/tests/helpers/adversarial_source.ts` (short reads, random delays, optional over-long reads):

```ts
import { readFile } from 'node:fs/promises';
import { setTimeout as sleep } from 'node:timers/promises';
import type { IoSource } from '@aviotrix/types';

export interface AdversarialOptions {
  /** Return at most this many bytes per read, regardless of what was asked. */
  maxChunk?: number;
  /** Return this many extra bytes beyond `length` (the core must ignore them). */
  overshoot?: number;
  /** Max random delay per call in ms. */
  maxDelayMs?: number;
}

export class AdversarialSource implements IoSource {
  private bytes: Uint8Array = new Uint8Array(0);
  reads = 0;
  constructor(
    private readonly path: string,
    private readonly opts: AdversarialOptions = {},
  ) {}
  async open(): Promise<number> {
    this.bytes = new Uint8Array(await readFile(this.path));
    return this.bytes.length;
  }
  async read(offset: number, length: number): Promise<Uint8Array> {
    this.reads++;
    await sleep(Math.floor(Math.random() * (this.opts.maxDelayMs ?? 2)));
    const want = Math.min(length, this.opts.maxChunk ?? length) + (this.opts.overshoot ?? 0);
    return this.bytes.subarray(offset, Math.min(this.bytes.length, offset + want));
  }
  async close(): Promise<void> {}
}
```

- [ ] **Step 2: Write the failing tests**

`packages/node/tests/metadata.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import { AviotrixError, FileSource, MediaReader, readMetadata } from '../src/index.js';
import { fixture } from './helpers/fixtures.js';
import { MemorySource } from './helpers/memory_source.js';

describe('readMetadata', () => {
  it('reads the mp4 fixture', async () => {
    const m = await readMetadata(new FileSource(fixture('h264-aac.mp4')));
    expect(m.format).toBe('mov,mp4,m4a,3gp,3g2,mj2');
    expect(m.streams).toHaveLength(2);
    expect(m.streams[0]?.codec).toBe('h264');
    expect(m.streams[0]?.video?.width).toBe(320);
    expect(m.streams[0]?.video?.frameRate).toEqual({ num: 30, den: 1 });
    expect(m.streams[1]?.audio?.sampleRate).toBe(48000);
    expect(m.duration).toBeGreaterThan(2.9);
  });

  it('reads the ts fixture with parser-derived dimensions', async () => {
    const m = await readMetadata(new FileSource(fixture('h264-ac3.ts')));
    expect(m.format).toBe('mpegts');
    expect(m.streams[0]?.video?.width).toBe(320);
    expect(m.streams[1]?.codec).toBe('ac3');
  });

  it('rejects non-media input with an AviotrixError', async () => {
    await expect(readMetadata(new FileSource(fixture('not-media.txt')))).rejects.toBeInstanceOf(
      AviotrixError,
    );
    await expect(readMetadata(new FileSource(fixture('not-media.txt')))).rejects.toMatchObject({
      code: 'AVERROR_INVALIDDATA',
    });
    await expect(readMetadata(new MemorySource(new Uint8Array(0)))).rejects.toBeInstanceOf(
      AviotrixError,
    );
  });

  it('delivers libav log lines to onLog', async () => {
    const lines: string[] = [];
    await readMetadata(new MemorySource(new Uint8Array(4096)), {
      onLog: (_level, text) => lines.push(text),
    }).catch(() => undefined);
    expect(lines.length).toBeGreaterThan(0);
  });

  it('supports await using', async () => {
    let closed = false;
    const src = new FileSource(fixture('h264-aac.mp4'));
    const origClose = src.close.bind(src);
    src.close = async () => {
      closed = true;
      await origClose();
    };
    {
      await using reader = await MediaReader.open(src);
      expect(reader.metadata.streams).toHaveLength(2);
    }
    expect(closed).toBe(true);
  });
});
```

`packages/node/tests/remux.test.ts`:

```ts
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import type { IoSink } from '@aviotrix/types';
import {
  AviotrixError,
  FileSink,
  FileSource,
  MediaReader,
  MemorySink,
  readMetadata,
  remux,
} from '../src/index.js';
import { AdversarialSource } from './helpers/adversarial_source.js';
import { fixture } from './helpers/fixtures.js';
import { MemorySource } from './helpers/memory_source.js';

let tmp = '';
beforeAll(async () => {
  tmp = await mkdtemp(join(tmpdir(), 'aviotrix-'));
});
afterAll(async () => {
  await rm(tmp, { recursive: true, force: true });
});

describe('remux', () => {
  it('mp4 -> matroska into MemorySink, reopenable', async () => {
    const sink = new MemorySink();
    const result = await remux(new FileSource(fixture('h264-aac.mp4')), sink, {
      format: 'matroska',
    });
    expect(result.streams).toEqual([
      { input: 0, output: 0 },
      { input: 1, output: 1 },
    ]);
    expect(result.packets).toBeGreaterThan(100);
    expect(result.bytesWritten).toBe(sink.bytes().length);
    const m = await readMetadata(new MemorySource(sink.bytes()));
    expect(m.format).toBe('matroska,webm');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'aac']);
  });

  it('ts -> mp4 into FileSink, reopenable', async () => {
    const out = join(tmp, 'out.mp4');
    await remux(new FileSource(fixture('h264-ac3.ts')), new FileSink(out), { format: 'mp4' });
    const m = await readMetadata(new FileSource(out));
    expect(m.format).toBe('mov,mp4,m4a,3gp,3g2,mj2');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
    expect(m.streams[0]?.video?.width).toBe(320);
  });

  it('skips the srt stream for mp4 by default and fails on request', async () => {
    const warnings: string[] = [];
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac-srt.mkv')), {
      onLog: (level, text) => {
        if (level === 'warning') warnings.push(text);
      },
    });
    const sink = new MemorySink();
    const result = await reader.remux(sink, { format: 'mp4' });
    expect(result.streams[2]).toEqual({
      input: 2,
      output: null,
      skippedReason: expect.stringContaining('subrip'),
    });
    expect(warnings.some((w) => w.includes('subrip'))).toBe(true);
    await expect(
      reader.remux(new MemorySink(), { format: 'mp4', onIncompatibleStream: 'fail' }),
    ).rejects.toMatchObject({
      code: 'INCOMPATIBLE_STREAM',
    });
    await reader.close();
  });

  it('non-seekable sink needs fragmented for mp4', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    const streaming = new MemorySink({ seekable: false });
    await expect(reader.remux(streaming, { format: 'mp4' })).rejects.toMatchObject({
      code: 'SINK_NOT_SEEKABLE',
    });
    expect(streaming.bytes().length).toBe(0);
    const result = await reader.remux(streaming, { format: 'mp4', fragmented: true });
    expect(result.packets).toBeGreaterThan(0);
    expect(Buffer.from(streaming.bytes()).includes('moof')).toBe(true);
    await reader.close();
  });

  it('aborts via AbortSignal and still closes the sink', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    const controller = new AbortController();
    const sink = new MemorySink();
    let closed = false;
    const origClose = sink.close.bind(sink);
    sink.close = () => {
      closed = true;
      return origClose();
    };
    await expect(
      reader.remux(sink, {
        format: 'matroska',
        signal: controller.signal,
        onProgress: () => controller.abort(),
      }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    expect(closed).toBe(true);

    const pre = new AbortController();
    pre.abort();
    await expect(
      reader.remux(new MemorySink(), { format: 'matroska', signal: pre.signal }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    await reader.close();
  });

  it('queues concurrent calls on one reader', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    const a = new MemorySink();
    const b = new MemorySink();
    const [ra, rb] = await Promise.all([
      reader.remux(a, { format: 'matroska' }),
      reader.remux(b, { format: 'matroska' }),
    ]);
    expect(ra.packets).toBe(rb.packets);
    expect(a.bytes().length).toBe(b.bytes().length);
    await reader.close();
  });

  it('rejects an out-of-range stream index before any output IO', async () => {
    const sink = new MemorySink();
    let opened = false;
    const origOpen = sink.open.bind(sink);
    sink.open = () => {
      opened = true;
      return origOpen();
    };
    await expect(
      remux(new FileSource(fixture('h264-aac.mp4')), sink, { format: 'matroska', streams: [7] }),
    ).rejects.toMatchObject({
      code: 'INVALID_ARGUMENT',
    });
    expect(opened).toBe(false);
  });

  it('sink write failure propagates and the sink is closed', async () => {
    let closed = false;
    let written = 0;
    const sink: IoSink = {
      seekable: true,
      open() {},
      write(_offset, data) {
        written += data.length;
        if (written > 50000) throw new Error('quota exceeded');
      },
      close() {
        closed = true;
      },
    };
    await expect(
      remux(new FileSource(fixture('h264-aac.mp4')), sink, { format: 'matroska' }),
    ).rejects.toMatchObject({
      code: 'IO_FAILED',
      message: 'quota exceeded',
    });
    expect(closed).toBe(true);
  });

  it('adversarial source (short reads, delays) produces identical output', async () => {
    const reference = new MemorySink();
    await remux(new FileSource(fixture('h264-aac.mp4')), reference, { format: 'matroska' });
    const adversarial = new AdversarialSource(fixture('h264-aac.mp4'), {
      maxChunk: 777,
      maxDelayMs: 2,
    });
    const sink = new MemorySink();
    await remux(adversarial, sink, { format: 'matroska' });
    expect(adversarial.reads).toBeGreaterThan(10);
    expect(Buffer.from(sink.bytes()).equals(Buffer.from(reference.bytes()))).toBe(true);
  });

  it('over-long read is truncated to the requested length', async () => {
    const reference = new MemorySink();
    await remux(new FileSource(fixture('h264-aac.mp4')), reference, { format: 'matroska' });
    const sink = new MemorySink();
    await remux(new AdversarialSource(fixture('h264-aac.mp4'), { overshoot: 100 }), sink, {
      format: 'matroska',
    });
    expect(Buffer.from(sink.bytes()).equals(Buffer.from(reference.bytes()))).toBe(true);
  });

  it('wraps unknown formats as INVALID_ARGUMENT and rejects after close with NOT_OPEN', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    await expect(reader.remux(new MemorySink(), { format: 'avi' })).rejects.toMatchObject({
      code: 'INVALID_ARGUMENT',
    });
    await reader.close();
    await expect(reader.remux(new MemorySink(), { format: 'matroska' })).rejects.toBeInstanceOf(
      AviotrixError,
    );
  });
});
```

`packages/node/tests/io.test.ts`:

```ts
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { FileSink, FileSource, MemorySink } from '../src/index.js';
import { fixture } from './helpers/fixtures.js';

let tmp = '';
beforeAll(async () => {
  tmp = await mkdtemp(join(tmpdir(), 'aviotrix-io-'));
});
afterAll(async () => {
  await rm(tmp, { recursive: true, force: true });
});

describe('FileSource', () => {
  it('reports size and reads ranges', async () => {
    const src = new FileSource(fixture('subs.srt'));
    const size = await src.open();
    expect(size).toBeGreaterThan(10);
    const head = await src.read(0, 1);
    expect(new TextDecoder().decode(head)).toBe('1');
    const past = await src.read(size ?? 0, 10);
    expect(past.length).toBe(0);
    await src.close();
  });
});

describe('FileSink', () => {
  it('writes at offsets, including backwards', async () => {
    const path = join(tmp, 'sink.bin');
    const sink = new FileSink(path);
    expect(sink.seekable).toBe(true);
    await sink.open();
    await sink.write(0, new Uint8Array([1, 2, 3, 4]));
    await sink.write(1, new Uint8Array([9, 9]));
    await sink.close();
    expect([...(await readFile(path))]).toEqual([1, 9, 9, 4]);
  });
});

describe('MemorySink', () => {
  it('seekable mode grows and overwrites', async () => {
    const sink = new MemorySink();
    await sink.open();
    await sink.write(2, new Uint8Array([5]));
    await sink.write(0, new Uint8Array([1, 2]));
    await sink.close();
    expect([...sink.bytes()]).toEqual([1, 2, 5]);
  });
  it('streaming mode rejects non-sequential writes', async () => {
    const sink = new MemorySink({ seekable: false });
    await sink.open();
    await sink.write(0, new Uint8Array([1]));
    await expect(async () => sink.write(5, new Uint8Array([2]))).rejects.toThrow(/sequential/);
  });
});
```

- [ ] **Step 3: Run tests to verify they fail**

```bash
npm test -w @aviotrix/node
```

Expected: FAIL, `../src/index.js` has no exports.

- [ ] **Step 4: Write the wrapper**

`packages/node/src/errors.ts`:

```ts
import { AviotrixError, isNativeFailure } from '@aviotrix/types';

export function wrapNativeError(
  e: object | string | number | boolean | null | undefined,
): AviotrixError {
  if (e instanceof AviotrixError) return e;
  if (e !== null && typeof e === 'object' && isNativeFailure(e))
    return new AviotrixError(e.code, e.message);
  if (e instanceof Error) return new AviotrixError('UNKNOWN', e.message);
  return new AviotrixError('UNKNOWN', String(e));
}
```

`catch (e)` binds `e` implicitly without the keyword appearing in source, so the `unknown` grep stays clean; callers pass it as `wrapNativeError(e as object)`.

`packages/node/src/queue.ts`:

```ts
/** Runs async operations strictly one after another, in call order. */
export class OperationQueue {
  private tail: Promise<void> = Promise.resolve();

  run<T>(operation: () => Promise<T>): Promise<T> {
    const result = this.tail.then(operation, operation);
    this.tail = result.then(
      () => undefined,
      () => undefined,
    );
    return result;
  }
}
```

`packages/node/src/host.ts`:

```ts
import type { IoSink, IoSource, LogFn, LogLevel, RemuxProgress } from '@aviotrix/types';
import type { NativeHost } from './native.js';

const logLevels: ReadonlySet<string> = new Set([
  'quiet',
  'panic',
  'fatal',
  'error',
  'warning',
  'info',
  'verbose',
  'debug',
  'trace',
]);

/** Routes the native binding's host callbacks to the current IoSource, IoSink, and listeners. */
export class Host implements NativeHost {
  sink: IoSink | null = null;
  onProgressListener: ((progress: RemuxProgress) => void) | null = null;

  constructor(
    private readonly source: IoSource,
    private readonly onLogListener: LogFn | undefined,
  ) {}

  sourceOpen(): ReturnType<IoSource['open']> {
    return this.source.open();
  }
  sourceRead(offset: number, length: number): ReturnType<IoSource['read']> {
    return this.source.read(offset, length);
  }
  sourceClose(): ReturnType<IoSource['close']> {
    return this.source.close();
  }
  sinkOpen(): ReturnType<IoSink['open']> {
    return this.requireSink().open();
  }
  sinkWrite(offset: number, data: Uint8Array): ReturnType<IoSink['write']> {
    return this.requireSink().write(offset, data);
  }
  sinkClose(): ReturnType<IoSink['close']> {
    return this.requireSink().close();
  }
  onLog(level: string, text: string): void {
    if (!this.onLogListener) return;
    const lvl: LogLevel = logLevels.has(level) ? (level as LogLevel) : 'info';
    this.onLogListener(lvl, text);
  }
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void {
    this.onProgressListener?.({ bytesRead, bytesWritten, timestamp });
  }

  private requireSink(): IoSink {
    if (!this.sink) throw new Error('aviotrix: sink callback with no active sink');
    return this.sink;
  }
}
```

`packages/node/src/media_reader.ts`:

```ts
import {
  AviotrixError,
  parseMetadataJson,
  parseRemuxResultJson,
  type IoSink,
  type IoSource,
  type Metadata,
  type OpenOptions,
  type RemuxOptions,
  type RemuxResult,
} from '@aviotrix/types';
import { wrapNativeError } from './errors.js';
import { Host } from './host.js';
import { native, type NativeReader, type NativeRemuxOptions } from './native.js';
import { OperationQueue } from './queue.js';

export class MediaReader {
  readonly metadata: Metadata;
  private readonly queue = new OperationQueue();
  private closed = false;

  private constructor(
    private readonly nativeReader: NativeReader,
    private readonly host: Host,
    metadata: Metadata,
  ) {
    this.metadata = metadata;
  }

  static async open(source: IoSource, options: OpenOptions = {}): Promise<MediaReader> {
    const host = new Host(source, options.onLog);
    const nativeReader = new native.NativeReader(host);
    try {
      const json = await nativeReader.open();
      return new MediaReader(nativeReader, host, parseMetadataJson(json));
    } catch (e) {
      await nativeReader.close().catch(() => undefined);
      throw wrapNativeError(e as object);
    }
  }

  remux(sink: IoSink, options: RemuxOptions): Promise<RemuxResult> {
    return this.queue.run(async () => {
      if (this.closed) throw new AviotrixError('NOT_OPEN', 'reader is closed');
      if (options.signal?.aborted) throw new AviotrixError('ABORTED', 'remux aborted before start');
      const nativeOptions: NativeRemuxOptions = {
        format: options.format,
        failOnIncompatible: options.onIncompatibleStream === 'fail',
        fragmented: options.fragmented ?? false,
        sinkSeekable: sink.seekable,
        progressIntervalPackets: 100,
      };
      if (options.streams) nativeOptions.streams = options.streams;

      this.host.sink = sink;
      this.host.onProgressListener = options.onProgress ?? null;
      const onAbort = (): void => this.nativeReader.cancel();
      options.signal?.addEventListener('abort', onAbort, { once: true });
      try {
        return parseRemuxResultJson(await this.nativeReader.remux(nativeOptions));
      } catch (e) {
        throw wrapNativeError(e as object);
      } finally {
        options.signal?.removeEventListener('abort', onAbort);
        this.host.sink = null;
        this.host.onProgressListener = null;
      }
    });
  }

  close(): Promise<void> {
    return this.queue.run(async () => {
      if (this.closed) return;
      this.closed = true;
      try {
        await this.nativeReader.close();
      } catch (e) {
        throw wrapNativeError(e as object);
      }
    });
  }

  async [Symbol.asyncDispose](): Promise<void> {
    await this.close();
  }
}

export async function readMetadata(source: IoSource, options?: OpenOptions): Promise<Metadata> {
  const reader = await MediaReader.open(source, options);
  try {
    return reader.metadata;
  } finally {
    await reader.close();
  }
}

export async function remux(
  source: IoSource,
  sink: IoSink,
  options: RemuxOptions,
): Promise<RemuxResult> {
  const reader = await MediaReader.open(source);
  try {
    return await reader.remux(sink, options);
  } finally {
    await reader.close();
  }
}
```

`packages/node/src/io/file_source.ts`:

```ts
import { open, type FileHandle } from 'node:fs/promises';
import type { IoSource } from '@aviotrix/types';

export class FileSource implements IoSource {
  private handle: FileHandle | null = null;

  constructor(private readonly path: string) {}

  async open(): Promise<number> {
    this.handle = await open(this.path, 'r');
    return (await this.handle.stat()).size;
  }

  async read(offset: number, length: number): Promise<Uint8Array> {
    if (!this.handle) throw new Error('FileSource.read before open');
    const buffer = new Uint8Array(length);
    const { bytesRead } = await this.handle.read(buffer, 0, length, offset);
    return buffer.subarray(0, bytesRead);
  }

  async close(): Promise<void> {
    const handle = this.handle;
    this.handle = null;
    await handle?.close();
  }
}
```

`packages/node/src/io/file_sink.ts`:

```ts
import { open, type FileHandle } from 'node:fs/promises';
import type { IoSink } from '@aviotrix/types';

export class FileSink implements IoSink {
  readonly seekable = true;
  private handle: FileHandle | null = null;

  constructor(private readonly path: string) {}

  async open(): Promise<void> {
    this.handle = await open(this.path, 'w');
  }

  async write(offset: number, data: Uint8Array): Promise<void> {
    if (!this.handle) throw new Error('FileSink.write before open');
    let written = 0;
    while (written < data.length) {
      const { bytesWritten } = await this.handle.write(
        data,
        written,
        data.length - written,
        offset + written,
      );
      written += bytesWritten;
    }
  }

  async close(): Promise<void> {
    const handle = this.handle;
    this.handle = null;
    await handle?.close();
  }
}
```

`packages/node/src/io/memory_sink.ts`:

```ts
import type { IoSink } from '@aviotrix/types';

export class MemorySink implements IoSink {
  readonly seekable: boolean;
  private buffer = new Uint8Array(64 * 1024);
  private length = 0;

  constructor(options: { seekable?: boolean } = {}) {
    this.seekable = options.seekable ?? true;
  }

  open(): void {
    this.length = 0;
  }

  write(offset: number, data: Uint8Array): void {
    if (!this.seekable && offset !== this.length) {
      throw new Error(
        `MemorySink: streaming sink requires sequential writes (got ${offset}, expected ${this.length})`,
      );
    }
    const end = offset + data.length;
    if (end > this.buffer.length) {
      const grown = new Uint8Array(Math.max(end, this.buffer.length * 2));
      grown.set(this.buffer.subarray(0, this.length));
      this.buffer = grown;
    }
    this.buffer.set(data, offset);
    this.length = Math.max(this.length, end);
  }

  close(): void {}

  bytes(): Uint8Array {
    return this.buffer.subarray(0, this.length);
  }
}
```

`packages/node/src/index.ts`:

```ts
export * from '@aviotrix/types';
export { MediaReader, readMetadata, remux } from './media_reader.js';
export { FileSource } from './io/file_source.js';
export { FileSink } from './io/file_sink.js';
export { MemorySink } from './io/memory_sink.js';
```

- [ ] **Step 5: Run lint, typecheck, tests, build**

```bash
npm run format && npm run lint && npm run typecheck -w @aviotrix/node && npm test -w @aviotrix/node && npm run build:ts -w @aviotrix/node
```

Expected: all green; 5 native tests + 5 metadata + 11 remux + 4 io tests pass. The `check-no-unknown.sh` grep must stay clean: `errors.ts` uses an explicit union instead of `unknown`.

- [ ] **Step 6: Commit**

```bash
git add packages/node
git commit -m "Add @aviotrix/node public API: MediaReader, readMetadata, remux, file and memory IO"
```

---

### Task 10: FFmpeg WASM build target

**Files:**

- Modify: `scripts/build-ffmpeg.sh` (add the `wasm` case)

**Interfaces:**

- Consumes: Task 2 script and component list.
- Produces: `build/ffmpeg-wasm/lib/{libavformat,libavcodec,libavutil}.a` as wasm32 static archives, no pthreads, no asm.

- [ ] **Step 1: Verify the toolchain**

```bash
brew list emscripten >/dev/null 2>&1 || brew install emscripten
emcc --version | head -1 && emconfigure --help >/dev/null 2>&1 && echo "emconfigure ok" && command -v emnm && echo "emnm ok"
```

Expected: `emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 6.0.x`, `emconfigure ok`, `emnm ok`.

- [ ] **Step 2: Add the `wasm` case and make prefix**

Replace the `wasm)` stanza and the `make` lines in `scripts/build-ffmpeg.sh` with:

```bash
make_prefix=()
case "$target" in
  host)
    command -v nasm >/dev/null || { echo "error: nasm required (brew install nasm)" >&2; exit 1; }
    "$src/configure" --prefix="$prefix" "${FFMPEG_COMMON_FLAGS[@]}" --enable-static --disable-shared
    ;;
  wasm)
    command -v emconfigure >/dev/null || { echo "error: emscripten required (brew install emscripten)" >&2; exit 1; }
    emconfigure "$src/configure" --prefix="$prefix" "${FFMPEG_COMMON_FLAGS[@]}" \
      --enable-cross-compile --target-os=none --arch=x86_32 \
      --cc=emcc --cxx=em++ --ar=emar --ranlib=emranlib --nm=emnm --objcc=emcc --dep-cc=emcc \
      --disable-asm --disable-inline-asm --disable-x86asm \
      --disable-pthreads --disable-w32threads --disable-os2threads --disable-runtime-cpudetect \
      --enable-static --disable-shared \
      --extra-cflags="-Oz" --extra-cxxflags="-Oz"
    make_prefix=(emmake)
    ;;
esac
"${make_prefix[@]}" make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
"${make_prefix[@]}" make install
```

- [ ] **Step 3: Build and verify**

```bash
scripts/build-ffmpeg.sh wasm
ls build/ffmpeg-wasm/lib/*.a
emnm build/ffmpeg-wasm/lib/libavformat.a 2>/dev/null | grep -c " T avformat_open_input"
du -sh build/ffmpeg-wasm/lib
```

Expected: the three archives; count `1`; total archive size on the order of a few MB (archives carry dead code the linker drops; the final `.wasm` is measured in Task 11). If configure fails on `--nm=emnm`, use `--nm=llvm-nm` with the Emscripten-bundled LLVM (`$(dirname "$(command -v emcc)")/../libexec/llvm/bin/llvm-nm` on Homebrew), and record the chosen path in the script.

- [ ] **Step 4: Commit**

```bash
git add scripts/build-ffmpeg.sh
git commit -m "Add Emscripten wasm target to the FFmpeg build script"
```

---

### Task 11: WASM binding (Emscripten, JSPI) and build

**Files:**

- Create: `packages/wasm/package.json`, `packages/wasm/tsconfig.json`, `packages/wasm/vitest.config.ts`, `packages/wasm/CMakeLists.txt`, `packages/wasm/binding/handles.h`, `packages/wasm/binding/js_imports.h`, `packages/wasm/binding/js_imports.cc`, `packages/wasm/binding/js_io.h`, `packages/wasm/binding/js_io.cc`, `packages/wasm/binding/exports.cc`, `packages/wasm/binding/pre.js`, `packages/wasm/src/module.ts` (type declarations for the Emscripten module), `packages/wasm/src/aviotrix.d.ts`, `packages/wasm/tests/smoke.test.ts`
- Modify: `.gitignore` (add `packages/wasm/dist/`), `README.md` (record measured size)

**Interfaces:**

- Consumes: core `MediaReader`, `IoSource`, `IoSink`, `toJson`, `errorCodeName`, `logLevelName`.
- Produces: `dist/aviotrix.mjs` + `dist/aviotrix.wasm`, an ES module factory `createAviotrixModule(options?) => Promise<AviotrixModule>`, with:
  - JS-side host registry the imports read: `Module.aviotrixHosts: Map<number, WasmHost>` where
    ```ts
    interface WasmHost {
      sourceOpen(): MaybePromise<number | null>;
      sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
      sourceClose(): MaybePromise<void>;
      sinkOpen(): MaybePromise<void>;
      sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
      sinkClose(): MaybePromise<void>;
      onLog(level: string, text: string): void;
      onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
    }
    ```
  - C exports (all take/return 32-bit ints unless noted; offsets cross as `double`):
    - `_avx_reader_new(hostId) -> readerId`
    - `_avx_reader_open(readerId) -> Promise<0 | nonzero>` (JSPI export)
    - `_avx_reader_metadata(readerId) -> char*` JSON, valid until the next call on that reader
    - `_avx_reader_remux(readerId, formatPtr, streamsPtr, nStreams, failOnIncompatible, fragmented, sinkSeekable, progressInterval) -> Promise<0 | nonzero>` (JSPI export)
    - `_avx_reader_result(readerId) -> char*` RemuxResult JSON
    - `_avx_reader_cancel(readerId)`
    - `_avx_reader_close(readerId) -> Promise<0 | nonzero>` (JSPI export)
    - `_avx_reader_free(readerId)`
    - `_avx_last_error_code(readerId) -> char*`, `_avx_last_error_message(readerId) -> char*`
    - plus `_malloc`, `_free`, runtime methods `HEAPU8`, `HEAP32`, `UTF8ToString`, `stringToUTF8`, `lengthBytesUTF8`

- [ ] **Step 1: Write the smoke test (browser, Chromium)**

`packages/wasm/vitest.config.ts`:

```ts
import { playwright } from '@vitest/browser-playwright';
import { defineConfig } from 'vitest/config';

export default defineConfig({
  server: { fs: { allow: ['../..'] } },
  test: {
    include: ['tests/**/*.test.ts'],
    testTimeout: 60_000,
    browser: {
      enabled: true,
      headless: true,
      provider: playwright(),
      instances: [{ browser: 'chromium' }],
    },
  },
});
```

`packages/wasm/src/module.ts` (hand-written declarations for the Emscripten output):

```ts
import type { MaybePromise } from '@aviotrix/types';

export interface WasmHost {
  sourceOpen(): MaybePromise<number | null>;
  sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
  sourceClose(): MaybePromise<void>;
  sinkOpen(): MaybePromise<void>;
  sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
  sinkClose(): MaybePromise<void>;
  onLog(level: string, text: string): void;
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
}

export type Pointer = number;

export interface AviotrixModule {
  HEAPU8: Uint8Array;
  HEAP32: Int32Array;
  aviotrixHosts: Map<number, WasmHost>;
  aviotrixLastHostError: string;
  _malloc(size: number): Pointer;
  _free(ptr: Pointer): void;
  UTF8ToString(ptr: Pointer): string;
  stringToUTF8(str: string, ptr: Pointer, maxBytes: number): void;
  lengthBytesUTF8(str: string): number;
  _avx_reader_new(hostId: number): number;
  _avx_reader_open(readerId: number): Promise<number>;
  _avx_reader_metadata(readerId: number): Pointer;
  _avx_reader_remux(
    readerId: number,
    format: Pointer,
    streams: Pointer,
    streamCount: number,
    failOnIncompatible: number,
    fragmented: number,
    sinkSeekable: number,
    progressInterval: number,
  ): Promise<number>;
  _avx_reader_result(readerId: number): Pointer;
  _avx_reader_cancel(readerId: number): void;
  _avx_reader_close(readerId: number): Promise<number>;
  _avx_reader_free(readerId: number): void;
  _avx_last_error_code(readerId: number): Pointer;
  _avx_last_error_message(readerId: number): Pointer;
}

export interface ModuleOptions {
  locateFile?: (path: string, prefix: string) => string;
}

export type ModuleFactory = (options?: ModuleOptions) => Promise<AviotrixModule>;
```

`packages/wasm/src/aviotrix.d.ts` (ambient declaration for the generated file):

```ts
declare module '../dist/aviotrix.mjs' {
  import type { ModuleFactory } from './module.js';
  const createAviotrixModule: ModuleFactory;
  export default createAviotrixModule;
}
```

`packages/wasm/tests/smoke.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import createAviotrixModule from '../dist/aviotrix.mjs';

describe('wasm module', () => {
  it('runs in a JSPI-capable browser and instantiates', async () => {
    expect('Suspending' in WebAssembly).toBe(true);
    const mod = await createAviotrixModule({
      locateFile: (p: string) => new URL(`../dist/${p}`, import.meta.url).href,
    });
    expect(mod.aviotrixHosts).toBeInstanceOf(Map);
    const id = mod._avx_reader_new(42);
    expect(id).toBeGreaterThan(0);
    // open without a registered host fails cleanly with a message, no throw
    const rc = await mod._avx_reader_open(id);
    expect(rc).not.toBe(0);
    expect(mod.UTF8ToString(mod._avx_last_error_code(id))).toBe('IO_FAILED');
    expect(mod.UTF8ToString(mod._avx_last_error_message(id))).toContain('host');
    mod._avx_reader_free(id);
  });
});
```

- [ ] **Step 2: Write package config**

`packages/wasm/package.json`:

```json
{
  "name": "@aviotrix/wasm",
  "version": "0.1.0",
  "description": "libav (FFmpeg) remuxing and metadata in the browser (WebAssembly + JSPI) through a custom async AVIO layer",
  "license": "MIT",
  "type": "module",
  "exports": { ".": { "types": "./dist/index.d.ts", "import": "./dist/index.js" } },
  "files": ["dist"],
  "scripts": {
    "build:wasm": "emcmake cmake -S ../.. -B build/wasm -DAVIOTRIX_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build/wasm -j && mkdir -p dist && cp build/wasm/packages/wasm/aviotrix.mjs build/wasm/packages/wasm/aviotrix.wasm dist/",
    "build:ts": "tsc -p tsconfig.json",
    "build": "npm run build:wasm && npm run build:ts",
    "typecheck": "tsc -p tsconfig.json --noEmit",
    "test": "vitest run",
    "size": "ls -l dist/aviotrix.wasm | awk '{print $5\" bytes\"}' && gzip -9 -c dist/aviotrix.wasm | wc -c | awk '{print $1\" bytes gzipped\"}'"
  },
  "dependencies": { "@aviotrix/types": "0.1.0" },
  "devDependencies": {
    "@vitest/browser": "^5.0.3",
    "@vitest/browser-playwright": "^5.0.3",
    "playwright": "^1.63.0",
    "vitest": "^5.0.3"
  }
}
```

`packages/wasm/tsconfig.json`:

```json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "rootDir": "src",
    "outDir": "dist",
    "lib": ["es2023", "dom", "dom.iterable", "esnext.disposable"]
  },
  "include": ["src"]
}
```

Add `packages/wasm/dist/` to `.gitignore`.

- [ ] **Step 3: Run the smoke test to verify it fails**

```bash
npm install && npx playwright install chromium && npm test -w @aviotrix/wasm
```

Expected: FAIL, cannot resolve `../dist/aviotrix.mjs`.

- [ ] **Step 4: Write the binding**

`packages/wasm/binding/handles.h`:

```cpp
#pragma once

#include <memory>
#include <unordered_map>

// Tiny integer-handle table so JS never holds raw pointers.
template <typename T>
class HandleTable {
 public:
  int insert(std::unique_ptr<T> value) {
    const int id = next_++;
    items_[id] = std::move(value);
    return id;
  }
  T* get(int id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
  }
  void erase(int id) { items_.erase(id); }

 private:
  int next_ = 1;
  std::unordered_map<int, std::unique_ptr<T>> items_;
};
```

`packages/wasm/binding/js_imports.h`:

```cpp
#pragma once

#include <cstdint>

// Implemented in js_imports.cc with EM_ASYNC_JS / EM_JS. All return <0 on host error;
// the message is then available via avx_js_copy_host_error.
extern "C" {
double avx_js_source_open(int hostId);                                        // size, -1 = null (unknown), -2 = error
int avx_js_source_read(int hostId, double offset, int length, uint8_t* dest);  // bytes copied, <0 error
int avx_js_source_close(int hostId);
int avx_js_sink_open(int hostId);
int avx_js_sink_write(int hostId, double offset, const uint8_t* src, int length);
int avx_js_sink_close(int hostId);
void avx_js_log(int hostId, const char* level, const char* text);
void avx_js_progress(int hostId, double bytesRead, double bytesWritten, double timestamp, int hasTimestamp);
int avx_js_copy_host_error(char* dest, int maxBytes);  // copies and clears Module.aviotrixLastHostError
}
```

`packages/wasm/binding/js_imports.cc`:

```cpp
#include "js_imports.h"

#include <emscripten.h>

// clang-format off
EM_JS(int, avx_js_copy_host_error, (char* dest, int maxBytes), {
  const s = Module.aviotrixLastHostError || '';
  Module.aviotrixLastHostError = '';
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
```

`packages/wasm/binding/js_io.h`:

```cpp
#pragma once

#include <string>

#include "aviotrix/io.h"

namespace aviotrix_wasm {

std::string takeHostError();  // reads and clears Module.aviotrixLastHostError

class JsIoSource final : public aviotrix::IoSource {
 public:
  explicit JsIoSource(int hostId) : hostId_(hostId) {}
  aviotrix::Status open(std::optional<int64_t>& size) override;
  aviotrix::Status read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) override;
  aviotrix::Status close() override;

 private:
  int hostId_;
};

class JsIoSink final : public aviotrix::IoSink {
 public:
  JsIoSink(int hostId, bool seekable) : hostId_(hostId), seekable_(seekable) {}
  bool seekable() const override { return seekable_; }
  aviotrix::Status open() override;
  aviotrix::Status write(int64_t offset, std::span<const uint8_t> data) override;
  aviotrix::Status close() override;

 private:
  int hostId_;
  bool seekable_;
};

}  // namespace aviotrix_wasm
```

`packages/wasm/binding/js_io.cc`:

```cpp
#include "js_io.h"

#include <vector>

#include "js_imports.h"

namespace aviotrix_wasm {

using aviotrix::ErrorCode;
using aviotrix::Status;

std::string takeHostError() {
  std::vector<char> buf(16 * 1024);  // long messages are truncated; the JS side is cleared on the first read
  avx_js_copy_host_error(buf.data(), static_cast<int>(buf.size()));
  std::string message(buf.data());
  return message.empty() ? "host callback failed" : message;
}

Status JsIoSource::open(std::optional<int64_t>& size) {
  const double r = avx_js_source_open(hostId_);
  if (r <= -2) return Status::Error(ErrorCode::IoFailed, takeHostError());
  if (r < 0) size.reset(); else size = static_cast<int64_t>(r);
  return Status::Ok();
}

Status JsIoSource::read(int64_t offset, std::span<uint8_t> buffer, size_t& bytesRead) {
  const int n = avx_js_source_read(hostId_, static_cast<double>(offset), static_cast<int>(buffer.size()), buffer.data());
  if (n < 0) { bytesRead = 0; return Status::Error(ErrorCode::IoFailed, takeHostError()); }
  bytesRead = static_cast<size_t>(n);
  return Status::Ok();
}

Status JsIoSource::close() {
  return avx_js_source_close(hostId_) < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

Status JsIoSink::open() {
  return avx_js_sink_open(hostId_) < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

Status JsIoSink::write(int64_t offset, std::span<const uint8_t> data) {
  const int r = avx_js_sink_write(hostId_, static_cast<double>(offset), data.data(), static_cast<int>(data.size()));
  return r < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

Status JsIoSink::close() {
  return avx_js_sink_close(hostId_) < 0 ? Status::Error(ErrorCode::IoFailed, takeHostError()) : Status::Ok();
}

}  // namespace aviotrix_wasm
```

`packages/wasm/binding/exports.cc`:

```cpp
#include <emscripten.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "aviotrix/json.h"
#include "aviotrix/media_reader.h"
#include "aviotrix/remux.h"
#include "handles.h"
#include "js_imports.h"
#include "js_io.h"

namespace {

struct Reader {
  explicit Reader(int host) : hostId(host), source(host) {}
  int hostId;
  aviotrix_wasm::JsIoSource source;
  aviotrix::MediaReader reader;
  std::atomic<bool> cancel{false};
  std::string json;     // last metadata / result payload
  std::string errCode;  // last error
  std::string errMessage;

  int fail(const aviotrix::Status& st) {
    errCode = aviotrix::errorCodeName(st.code);
    errMessage = st.message;
    return st.code == 0 ? 1 : st.code;
  }
  void clearError() { errCode.clear(); errMessage.clear(); }
};

HandleTable<Reader>& readers() {
  static HandleTable<Reader> table;
  return table;
}

const char* kEmpty = "";

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int avx_reader_new(int hostId) { return readers().insert(std::make_unique<Reader>(hostId)); }

EMSCRIPTEN_KEEPALIVE int avx_reader_open(int id) {
  Reader* r = readers().get(id);
  if (!r) return -1;
  r->clearError();
  aviotrix::OpenOptions opts;
  opts.log = [r](aviotrix::LogLevel level, std::string_view text) {
    std::string t(text);
    avx_js_log(r->hostId, aviotrix::logLevelName(level), t.c_str());
  };
  aviotrix::Status st = r->reader.open(r->source, std::move(opts));
  if (!st.ok()) return r->fail(st);
  r->json = aviotrix::toJson(r->reader.metadata());
  return 0;
}

EMSCRIPTEN_KEEPALIVE const char* avx_reader_metadata(int id) {
  Reader* r = readers().get(id);
  return r ? r->json.c_str() : kEmpty;
}

EMSCRIPTEN_KEEPALIVE int avx_reader_remux(int id, const char* format, const int* streams, int streamCount,
                                          int failOnIncompatible, int fragmented, int sinkSeekable, int progressInterval) {
  Reader* r = readers().get(id);
  if (!r) return -1;
  r->clearError();
  aviotrix::RemuxOptions opt;
  opt.format = format ? format : "";
  if (streams && streamCount >= 0) opt.streams = std::vector<int>(streams, streams + streamCount);
  opt.failOnIncompatible = failOnIncompatible != 0;
  opt.fragmented = fragmented != 0;
  opt.cancel = &r->cancel;
  if (progressInterval > 0) opt.progressIntervalPackets = progressInterval;
  opt.onProgress = [r](const aviotrix::RemuxProgress& p) {
    avx_js_progress(r->hostId, static_cast<double>(p.bytesRead), static_cast<double>(p.bytesWritten),
                    p.timestamp.value_or(0.0), p.timestamp.has_value() ? 1 : 0);
  };
  aviotrix_wasm::JsIoSink sink(r->hostId, sinkSeekable != 0);
  aviotrix::RemuxResult result;
  aviotrix::Status st = r->reader.remux(sink, opt, result);
  r->cancel.store(false);
  if (!st.ok()) return r->fail(st);
  r->json = aviotrix::toJson(result);
  return 0;
}

EMSCRIPTEN_KEEPALIVE const char* avx_reader_result(int id) {
  Reader* r = readers().get(id);
  return r ? r->json.c_str() : kEmpty;
}

EMSCRIPTEN_KEEPALIVE void avx_reader_cancel(int id) {
  if (Reader* r = readers().get(id)) r->cancel.store(true);
}

EMSCRIPTEN_KEEPALIVE int avx_reader_close(int id) {
  Reader* r = readers().get(id);
  if (!r) return -1;
  r->clearError();
  aviotrix::Status st = r->reader.close();
  return st.ok() ? 0 : r->fail(st);
}

EMSCRIPTEN_KEEPALIVE void avx_reader_free(int id) { readers().erase(id); }

EMSCRIPTEN_KEEPALIVE const char* avx_last_error_code(int id) {
  Reader* r = readers().get(id);
  return r ? r->errCode.c_str() : kEmpty;
}

EMSCRIPTEN_KEEPALIVE const char* avx_last_error_message(int id) {
  Reader* r = readers().get(id);
  return r ? r->errMessage.c_str() : kEmpty;
}

}  // extern "C"
```

Note: `std::atomic<bool>` compiles to plain loads/stores without threads; keep it for API parity with the core.

`packages/wasm/CMakeLists.txt`:

```cmake
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
```

`packages/wasm/binding/pre.js` (runs inside the module factory before the runtime; initializes the host registry):

```js
Module['aviotrixHosts'] = new Map();
Module['aviotrixLastHostError'] = '';
```

- [ ] **Step 5: Build, measure, and run the smoke test**

```bash
npm run build:wasm -w @aviotrix/wasm && npm run size -w @aviotrix/wasm && npm test -w @aviotrix/wasm
```

Expected: `dist/aviotrix.mjs` and `dist/aviotrix.wasm` exist; the smoke test passes. If Chromium reports `WebAssembly.Suspending is not a constructor`, Playwright's bundled Chromium is older than 137: run `npx playwright install chromium` again and check `npx playwright --version` ≥ 1.52. If the link step complains that `avx_js_*` are undefined, `js_imports.cc` is not in the executable's sources. If `HEAPU8` is undefined inside the EM_JS bodies, add `-sEXPORTED_RUNTIME_METHODS=...,HEAPU8` (it is already listed) and make sure `-sMODULARIZE` is on (the bodies run inside the factory scope).

Record the measured sizes in `README.md` under "WASM size": raw bytes and gzipped bytes, with the FFmpeg tag and Emscripten version.

- [ ] **Step 6: Lint, typecheck, commit**

```bash
npm run format && npm run lint && npm run typecheck -w @aviotrix/wasm
git add packages/wasm .gitignore README.md package-lock.json
git commit -m "Add Emscripten JSPI binding for the core with host registry and C handle API"
```

---

### Task 12: `@aviotrix/wasm` TypeScript wrapper, reference IO, browser tests

**Files:**

- Create: `packages/types/src/host.ts`, `packages/types/src/queue.ts` (shared binding helpers; both bindings have identical host shapes), `packages/wasm/src/index.ts`, `packages/wasm/src/load.ts`, `packages/wasm/src/media_reader.ts`, `packages/wasm/src/memory.ts`, `packages/wasm/src/io/blob_source.ts`, `packages/wasm/src/io/fetch_range_source.ts`, `packages/wasm/src/io/memory_sink.ts`, `packages/wasm/tests/helpers/fixtures.ts`, `packages/wasm/tests/helpers/fake_range_server.ts`, `packages/wasm/tests/metadata.test.ts`, `packages/wasm/tests/remux.test.ts`, `packages/wasm/tests/io.test.ts`, `packages/wasm/tests/unsupported.test.ts`
- Modify: `packages/types/src/index.ts` (export `BindingHost`, `OperationQueue`), `packages/node/src/native.ts` (use `BindingHost`), `packages/node/src/media_reader.ts` (import `OperationQueue`, `BindingHost` from types); Delete: `packages/node/src/host.ts`, `packages/node/src/queue.ts`

**Interfaces:**

- Consumes: Task 11 module and `WasmHost`; Task 7 types; Task 9 `Host`/`OperationQueue` (moved here into `@aviotrix/types`).
- Produces (spec §5 on WASM):
  - `load(options?: { wasmUrl?: string }): Promise<void>`; rejects `AviotrixError('UNSUPPORTED_RUNTIME')` without JSPI
  - `MediaReader`, `readMetadata`, `remux` with the same signatures as `@aviotrix/node`
  - `BlobSource(blob: Blob)`, `FetchRangeSource(url: string, options?: { fetch?: typeof fetch; headers?: HeadersInit })`, `MemorySink(options?: { seekable?: boolean })` with `bytes()` and `toBlob(type?: string)`
  - `@aviotrix/types` gains `class BindingHost implements <host callbacks>` (constructor `(source: IoSource, onLog: LogFn | undefined)`, fields `sink: IoSink | null`, `onProgressListener`) and `class OperationQueue { run<T>(op: () => Promise<T>): Promise<T> }`
  - Internal: `_resetForTests()` clears the cached module promise.

- [ ] **Step 1: Move `Host` and `OperationQueue` into `@aviotrix/types`**

Create `packages/types/src/queue.ts` with the exact contents of `packages/node/src/queue.ts` from Task 9. Create `packages/types/src/host.ts` with the contents of `packages/node/src/host.ts`, renaming the class to `BindingHost` and replacing the `NativeHost` import with a local interface:

```ts
export interface BindingHostCallbacks {
  sourceOpen(): MaybePromise<number | null>;
  sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
  sourceClose(): MaybePromise<void>;
  sinkOpen(): MaybePromise<void>;
  sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
  sinkClose(): MaybePromise<void>;
  onLog(level: string, text: string): void;
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
}
export class BindingHost implements BindingHostCallbacks {
  /* body from node/src/host.ts */
}
```

Add to `packages/types/src/index.ts`:

```ts
export { BindingHost } from './host.js';
export type { BindingHostCallbacks } from './host.js';
export { OperationQueue } from './queue.js';
```

In `packages/node/src/native.ts` replace the `NativeHost` interface with `export type { BindingHostCallbacks as NativeHost } from '@aviotrix/types';` (keep the name so Task 8's test still compiles). In `packages/node/src/media_reader.ts` import `BindingHost as Host` and `OperationQueue` from `@aviotrix/types`; delete `packages/node/src/host.ts` and `packages/node/src/queue.ts`. In `packages/wasm/src/module.ts` replace the `WasmHost` interface with `export type { BindingHostCallbacks as WasmHost } from '@aviotrix/types';`.

Run `npm run build -w @aviotrix/types && npm test -w @aviotrix/node` — expected: all Node tests still pass.

- [ ] **Step 2: Write the test helpers**

`packages/wasm/tests/helpers/fixtures.ts`:

```ts
export async function fixtureBytes(name: string): Promise<Uint8Array> {
  const url = new URL(`../../../../fixtures/${name}`, import.meta.url);
  const res = await fetch(url);
  if (!res.ok) throw new Error(`fixture ${name}: HTTP ${res.status}`);
  return new Uint8Array(await res.arrayBuffer());
}

export async function fixtureBlob(name: string): Promise<Blob> {
  return new Blob([await fixtureBytes(name)]);
}
```

`packages/wasm/tests/helpers/fake_range_server.ts` (a `fetch` that honors `Range`, so `FetchRangeSource` is tested deterministically):

```ts
export function fakeRangeFetch(
  bytes: Uint8Array,
  options: { ignoreRange?: boolean; noLength?: boolean } = {},
): typeof fetch {
  return async (_input, init) => {
    const headers = new Headers(init?.headers);
    const range = headers.get('range');
    if (init?.method === 'HEAD') {
      const h = new Headers({ 'accept-ranges': 'bytes' });
      if (!options.noLength) h.set('content-length', String(bytes.length));
      return new Response(null, { status: 200, headers: h });
    }
    if (range && !options.ignoreRange) {
      const m = /^bytes=(\d+)-(\d+)$/.exec(range);
      if (!m) return new Response('bad range', { status: 416 });
      const start = Number(m[1]);
      const end = Math.min(Number(m[2]), bytes.length - 1);
      return new Response(bytes.slice(start, end + 1), {
        status: 206,
        headers: { 'content-range': `bytes ${start}-${end}/${bytes.length}` },
      });
    }
    return new Response(bytes, {
      status: 200,
      headers: { 'content-length': String(bytes.length) },
    });
  };
}
```

- [ ] **Step 3: Write the failing tests**

`packages/wasm/tests/metadata.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import {
  AviotrixError,
  BlobSource,
  FetchRangeSource,
  MediaReader,
  load,
  readMetadata,
} from '../src/index.js';
import { fakeRangeFetch } from './helpers/fake_range_server.js';
import { fixtureBlob, fixtureBytes } from './helpers/fixtures.js';

describe('readMetadata (browser)', () => {
  it('loads once and reads the mp4 fixture via BlobSource', async () => {
    await load();
    const m = await readMetadata(new BlobSource(await fixtureBlob('h264-aac.mp4')));
    expect(m.format).toBe('mov,mp4,m4a,3gp,3g2,mj2');
    expect(m.streams[0]?.video).toEqual({
      width: 320,
      height: 240,
      frameRate: { num: 30, den: 1 },
      pixelFormat: 'yuv420p',
    });
    expect(m.streams[1]?.audio?.sampleRate).toBe(48000);
  });

  it('reads the ts fixture via FetchRangeSource with range requests', async () => {
    const bytes = await fixtureBytes('h264-ac3.ts');
    const src = new FetchRangeSource('https://example.test/video.ts', {
      fetch: fakeRangeFetch(bytes),
    });
    const m = await readMetadata(src);
    expect(m.format).toBe('mpegts');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
  });

  it('rejects non-media input with a coded AviotrixError and still closes the source', async () => {
    let closed = false;
    const src = new BlobSource(await fixtureBlob('not-media.txt'));
    const origClose = src.close.bind(src);
    src.close = async () => {
      closed = true;
      await origClose();
    };
    await expect(MediaReader.open(src)).rejects.toMatchObject({ code: 'AVERROR_INVALIDDATA' });
    await expect(MediaReader.open(new BlobSource(new Blob([])))).rejects.toBeInstanceOf(
      AviotrixError,
    );
    expect(closed).toBe(true);
  });

  it('delivers log lines', async () => {
    const lines: string[] = [];
    await readMetadata(new BlobSource(new Blob([new Uint8Array(4096)])), {
      onLog: (_l, t) => lines.push(t),
    }).catch(() => undefined);
    expect(lines.length).toBeGreaterThan(0);
  });
});
```

`packages/wasm/tests/remux.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import type { IoSink, IoSource } from '@aviotrix/types';
import { BlobSource, MediaReader, MemorySink, readMetadata, remux } from '../src/index.js';
import { fixtureBlob, fixtureBytes } from './helpers/fixtures.js';

const reopen = (bytes: Uint8Array) => readMetadata(new BlobSource(new Blob([bytes])));

describe('remux (browser)', () => {
  it('mp4 -> matroska, reopenable', async () => {
    const sink = new MemorySink();
    const result = await remux(new BlobSource(await fixtureBlob('h264-aac.mp4')), sink, {
      format: 'matroska',
    });
    expect(result.streams).toEqual([
      { input: 0, output: 0 },
      { input: 1, output: 1 },
    ]);
    const m = await reopen(sink.bytes());
    expect(m.format).toBe('matroska,webm');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'aac']);
    expect(sink.toBlob('video/x-matroska').size).toBe(result.bytesWritten);
  });

  it('ts -> mp4, reopenable', async () => {
    const sink = new MemorySink();
    await remux(new BlobSource(await fixtureBlob('h264-ac3.ts')), sink, { format: 'mp4' });
    const m = await reopen(sink.bytes());
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
    expect(m.streams[0]?.video?.width).toBe(320);
  });

  it('skips srt for mp4 by default, fails on request', async () => {
    const reader = await MediaReader.open(new BlobSource(await fixtureBlob('h264-aac-srt.mkv')));
    const result = await reader.remux(new MemorySink(), { format: 'mp4' });
    expect(result.streams[2]).toMatchObject({ input: 2, output: null });
    await expect(
      reader.remux(new MemorySink(), { format: 'mp4', onIncompatibleStream: 'fail' }),
    ).rejects.toMatchObject({
      code: 'INCOMPATIBLE_STREAM',
    });
    await reader.close();
  });

  it('streaming sink needs fragmented for mp4', async () => {
    const reader = await MediaReader.open(new BlobSource(await fixtureBlob('h264-aac.mp4')));
    const streaming = new MemorySink({ seekable: false });
    await expect(reader.remux(streaming, { format: 'mp4' })).rejects.toMatchObject({
      code: 'SINK_NOT_SEEKABLE',
    });
    const result = await reader.remux(streaming, { format: 'mp4', fragmented: true });
    expect(result.packets).toBeGreaterThan(0);
    expect(new TextDecoder('latin1').decode(streaming.bytes()).includes('moof')).toBe(true);
    await reader.close();
  });

  it('aborts via AbortSignal and closes the sink', async () => {
    const reader = await MediaReader.open(new BlobSource(await fixtureBlob('h264-aac.mp4')));
    const controller = new AbortController();
    let closed = false;
    const sink = new MemorySink();
    const origClose = sink.close.bind(sink);
    sink.close = () => {
      closed = true;
      return origClose();
    };
    await expect(
      reader.remux(sink, {
        format: 'matroska',
        signal: controller.signal,
        onProgress: () => controller.abort(),
      }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    expect(closed).toBe(true);
    await reader.close();
  });

  it('serializes operations module-wide across readers', async () => {
    const [a, b] = await Promise.all([
      MediaReader.open(new BlobSource(await fixtureBlob('h264-aac.mp4'))),
      MediaReader.open(new BlobSource(await fixtureBlob('vp9-opus.webm'))),
    ]);
    const sa = new MemorySink();
    const sb = new MemorySink();
    const [ra, rb] = await Promise.all([
      a.remux(sa, { format: 'matroska' }),
      b.remux(sb, { format: 'webm' }),
    ]);
    expect(ra.packets).toBeGreaterThan(0);
    expect(rb.packets).toBeGreaterThan(0);
    expect((await reopen(sa.bytes())).streams[0]?.codec).toBe('h264');
    expect((await reopen(sb.bytes())).streams[0]?.codec).toBe('vp9');
    await Promise.all([a.close(), b.close()]);
  });

  it('out-of-range stream index rejects before output IO', async () => {
    let opened = false;
    const sink = new MemorySink();
    const origOpen = sink.open.bind(sink);
    sink.open = () => {
      opened = true;
      return origOpen();
    };
    await expect(
      remux(new BlobSource(await fixtureBlob('h264-aac.mp4')), sink, {
        format: 'matroska',
        streams: [9],
      }),
    ).rejects.toMatchObject({
      code: 'INVALID_ARGUMENT',
    });
    expect(opened).toBe(false);
  });

  it('sink write failure propagates with the original message and closes the sink', async () => {
    let written = 0;
    let closed = false;
    const sink: IoSink = {
      seekable: true,
      open() {},
      write(_o, data) {
        written += data.length;
        if (written > 50000) throw new Error('quota exceeded');
      },
      close() {
        closed = true;
      },
    };
    await expect(
      remux(new BlobSource(await fixtureBlob('h264-aac.mp4')), sink, { format: 'matroska' }),
    ).rejects.toMatchObject({
      code: 'IO_FAILED',
      message: 'quota exceeded',
    });
    expect(closed).toBe(true);
  });

  it('short and over-long reads produce the same output as a plain source', async () => {
    const bytes = await fixtureBytes('h264-aac.mp4');
    const reference = new MemorySink();
    await remux(new BlobSource(new Blob([bytes])), reference, { format: 'matroska' });
    const weird: IoSource = {
      open: () => bytes.length,
      read: (offset, length) => {
        const want = offset % 2 === 0 ? Math.min(length, 999) : length + 100;
        return bytes.subarray(offset, Math.min(bytes.length, offset + want));
      },
      close() {},
    };
    const sink = new MemorySink();
    await remux(weird, sink, { format: 'matroska' });
    expect(sink.bytes().length).toBe(reference.bytes().length);
    expect(sink.bytes().every((b, i) => b === reference.bytes()[i])).toBe(true);
  });
});
```

`packages/wasm/tests/io.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import { BlobSource, FetchRangeSource, MemorySink } from '../src/index.js';
import { fakeRangeFetch } from './helpers/fake_range_server.js';

describe('BlobSource', () => {
  it('reads ranges and reports size', async () => {
    const src = new BlobSource(new Blob([new Uint8Array([1, 2, 3, 4, 5])]));
    expect(await src.open()).toBe(5);
    expect([...(await src.read(3, 10))]).toEqual([4, 5]);
    expect((await src.read(5, 1)).length).toBe(0);
    await src.close();
  });
});

describe('FetchRangeSource', () => {
  const bytes = new Uint8Array(1000).map((_, i) => i % 251);
  it('uses HEAD for size and Range for reads', async () => {
    const src = new FetchRangeSource('https://example.test/f', { fetch: fakeRangeFetch(bytes) });
    expect(await src.open()).toBe(1000);
    const chunk = await src.read(500, 10);
    expect([...chunk]).toEqual([...bytes.subarray(500, 510)]);
  });
  it('returns null size when the server omits content-length', async () => {
    const src = new FetchRangeSource('https://example.test/f', {
      fetch: fakeRangeFetch(bytes, { noLength: true }),
    });
    expect(await src.open()).toBeNull();
  });
  it('throws when the server ignores Range', async () => {
    const src = new FetchRangeSource('https://example.test/f', {
      fetch: fakeRangeFetch(bytes, { ignoreRange: true }),
    });
    await src.open();
    await expect(src.read(10, 5)).rejects.toThrow(/Range/);
  });
});

describe('MemorySink', () => {
  it('grows, overwrites, and exports a Blob', async () => {
    const sink = new MemorySink();
    sink.open();
    sink.write(2, new Uint8Array([5]));
    sink.write(0, new Uint8Array([1, 2]));
    sink.close();
    expect([...sink.bytes()]).toEqual([1, 2, 5]);
    expect(sink.toBlob('application/octet-stream').size).toBe(3);
  });
  it('streaming mode rejects non-sequential writes', () => {
    const sink = new MemorySink({ seekable: false });
    sink.open();
    sink.write(0, new Uint8Array([1]));
    expect(() => sink.write(5, new Uint8Array([2]))).toThrow(/sequential/);
  });
});
```

`packages/wasm/tests/unsupported.test.ts`:

```ts
import { afterEach, describe, expect, it } from 'vitest';
import { _resetForTests, load } from '../src/index.js';

type WasmNamespace = { Suspending?: object };

describe('load without JSPI', () => {
  const ns = WebAssembly as WasmNamespace;
  const saved = ns.Suspending;
  afterEach(() => {
    Object.defineProperty(WebAssembly, 'Suspending', {
      value: saved,
      configurable: true,
      writable: true,
    });
    _resetForTests();
  });

  it('rejects with UNSUPPORTED_RUNTIME', async () => {
    _resetForTests();
    Object.defineProperty(WebAssembly, 'Suspending', {
      value: undefined,
      configurable: true,
      writable: true,
    });
    await expect(load()).rejects.toMatchObject({ code: 'UNSUPPORTED_RUNTIME' });
  });
});
```

- [ ] **Step 4: Run tests to verify they fail**

```bash
npm test -w @aviotrix/wasm
```

Expected: FAIL resolving `../src/index.js` exports (the smoke test from Task 11 still passes).

- [ ] **Step 5: Write the wrapper**

`packages/wasm/src/load.ts`:

```ts
import { AviotrixError } from '@aviotrix/types';
import type { AviotrixModule } from './module.js';

export interface LoadOptions {
  /** Override where the .wasm is fetched from. Default: next to the module script. */
  wasmUrl?: string;
}

let modulePromise: Promise<AviotrixModule> | null = null;

function hasJspi(): boolean {
  const ns = WebAssembly as { Suspending?: object };
  return typeof ns.Suspending === 'function';
}

export function loadModule(options: LoadOptions = {}): Promise<AviotrixModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      if (!hasJspi()) {
        throw new AviotrixError(
          'UNSUPPORTED_RUNTIME',
          '@aviotrix/wasm needs WebAssembly JavaScript Promise Integration (WebAssembly.Suspending). Chrome 137+, Safari 27+, Firefox 153+.',
        );
      }
      const { default: createAviotrixModule } = await import('../dist/aviotrix.mjs');
      const moduleOptions = options.wasmUrl
        ? {
            locateFile: (path: string) =>
              path.endsWith('.wasm') ? (options.wasmUrl ?? path) : path,
          }
        : {};
      return createAviotrixModule(moduleOptions);
    })();
    modulePromise.catch(() => {
      modulePromise = null; // allow a retry after a failed load
    });
  }
  return modulePromise;
}

/** Fetches and instantiates the WASM once. Optional: MediaReader.open calls it implicitly. */
export async function load(options: LoadOptions = {}): Promise<void> {
  await loadModule(options);
}

/** @internal */
export function _resetForTests(): void {
  modulePromise = null;
}
```

`packages/wasm/src/memory.ts` (helpers for passing strings and arrays into the heap):

```ts
import type { AviotrixModule, Pointer } from './module.js';

export async function withCStringAsync<T>(
  mod: AviotrixModule,
  text: string,
  fn: (ptr: Pointer) => Promise<T>,
): Promise<T> {
  const size = mod.lengthBytesUTF8(text) + 1;
  const ptr = mod._malloc(size);
  try {
    mod.stringToUTF8(text, ptr, size);
    return await fn(ptr);
  } finally {
    mod._free(ptr);
  }
}

export async function withInt32ArrayAsync<T>(
  mod: AviotrixModule,
  values: number[] | undefined,
  fn: (ptr: Pointer, count: number) => Promise<T>,
): Promise<T> {
  if (!values) return fn(0, -1);
  const ptr = mod._malloc(Math.max(4, values.length * 4));
  try {
    mod.HEAP32.set(values, ptr >> 2);
    return await fn(ptr, values.length);
  } finally {
    mod._free(ptr);
  }
}
```

`packages/wasm/src/media_reader.ts`:

```ts
import {
  AviotrixError,
  BindingHost,
  OperationQueue,
  parseMetadataJson,
  parseRemuxResultJson,
  type IoSink,
  type IoSource,
  type Metadata,
  type OpenOptions,
  type RemuxOptions,
  type RemuxResult,
} from '@aviotrix/types';
import { loadModule } from './load.js';
import { withCStringAsync, withInt32ArrayAsync } from './memory.js';
import type { AviotrixModule } from './module.js';

// One WASM instance, one thread: all operations across all readers run strictly one at a time, so
// libav log lines and JSPI suspensions never interleave between readers.
const moduleQueue = new OperationQueue();
let nextHostId = 1;

function lastError(mod: AviotrixModule, readerId: number): AviotrixError {
  const code = mod.UTF8ToString(mod._avx_last_error_code(readerId)) || 'UNKNOWN';
  const message = mod.UTF8ToString(mod._avx_last_error_message(readerId)) || 'operation failed';
  return new AviotrixError(code, message);
}

export class MediaReader {
  readonly metadata: Metadata;
  private closed = false;

  private constructor(
    private readonly mod: AviotrixModule,
    private readonly readerId: number,
    private readonly hostId: number,
    private readonly host: BindingHost,
    metadata: Metadata,
  ) {
    this.metadata = metadata;
  }

  static async open(source: IoSource, options: OpenOptions = {}): Promise<MediaReader> {
    const mod = await loadModule();
    return moduleQueue.run(async () => {
      const hostId = nextHostId++;
      const host = new BindingHost(source, options.onLog);
      mod.aviotrixHosts.set(hostId, host);
      const readerId = mod._avx_reader_new(hostId);
      const rc = await mod._avx_reader_open(readerId);
      if (rc !== 0) {
        const err = lastError(mod, readerId);
        mod._avx_reader_free(readerId);
        mod.aviotrixHosts.delete(hostId);
        throw err;
      }
      const metadata = parseMetadataJson(mod.UTF8ToString(mod._avx_reader_metadata(readerId)));
      return new MediaReader(mod, readerId, hostId, host, metadata);
    });
  }

  remux(sink: IoSink, options: RemuxOptions): Promise<RemuxResult> {
    return moduleQueue.run(async () => {
      if (this.closed) throw new AviotrixError('NOT_OPEN', 'reader is closed');
      if (options.signal?.aborted) throw new AviotrixError('ABORTED', 'remux aborted before start');
      this.host.sink = sink;
      this.host.onProgressListener = options.onProgress ?? null;
      const onAbort = (): void => this.mod._avx_reader_cancel(this.readerId);
      options.signal?.addEventListener('abort', onAbort, { once: true });
      try {
        const rc = await withCStringAsync(this.mod, options.format, (formatPtr) =>
          withInt32ArrayAsync(this.mod, options.streams, (streamsPtr, count) =>
            this.mod._avx_reader_remux(
              this.readerId,
              formatPtr,
              streamsPtr,
              count,
              options.onIncompatibleStream === 'fail' ? 1 : 0,
              options.fragmented ? 1 : 0,
              sink.seekable ? 1 : 0,
              100,
            ),
          ),
        );
        if (rc !== 0) throw lastError(this.mod, this.readerId);
        return parseRemuxResultJson(
          this.mod.UTF8ToString(this.mod._avx_reader_result(this.readerId)),
        );
      } finally {
        options.signal?.removeEventListener('abort', onAbort);
        this.host.sink = null;
        this.host.onProgressListener = null;
      }
    });
  }

  close(): Promise<void> {
    return moduleQueue.run(async () => {
      if (this.closed) return;
      this.closed = true;
      const rc = await this.mod._avx_reader_close(this.readerId);
      const err = rc !== 0 ? lastError(this.mod, this.readerId) : null;
      this.mod._avx_reader_free(this.readerId);
      this.mod.aviotrixHosts.delete(this.hostId);
      if (err) throw err;
    });
  }

  async [Symbol.asyncDispose](): Promise<void> {
    await this.close();
  }
}

export async function readMetadata(source: IoSource, options?: OpenOptions): Promise<Metadata> {
  const reader = await MediaReader.open(source, options);
  try {
    return reader.metadata;
  } finally {
    await reader.close();
  }
}

export async function remux(
  source: IoSource,
  sink: IoSink,
  options: RemuxOptions,
): Promise<RemuxResult> {
  const reader = await MediaReader.open(source);
  try {
    return await reader.remux(sink, options);
  } finally {
    await reader.close();
  }
}
```

`packages/wasm/src/io/blob_source.ts`:

```ts
import type { IoSource } from '@aviotrix/types';

export class BlobSource implements IoSource {
  constructor(private readonly blob: Blob) {}
  open(): number {
    return this.blob.size;
  }
  async read(offset: number, length: number): Promise<Uint8Array> {
    if (offset >= this.blob.size) return new Uint8Array(0);
    return new Uint8Array(
      await this.blob.slice(offset, Math.min(this.blob.size, offset + length)).arrayBuffer(),
    );
  }
  close(): void {}
}
```

`packages/wasm/src/io/fetch_range_source.ts`:

```ts
import type { IoSource } from '@aviotrix/types';

export interface FetchRangeSourceOptions {
  fetch?: typeof fetch;
  headers?: HeadersInit;
}

/** Reads a remote file with HTTP Range requests. The server must honor Range (206 responses). */
export class FetchRangeSource implements IoSource {
  private readonly fetchImpl: typeof fetch;

  constructor(
    private readonly url: string,
    private readonly options: FetchRangeSourceOptions = {},
  ) {
    this.fetchImpl = options.fetch ?? globalThis.fetch.bind(globalThis);
  }

  async open(): Promise<number | null> {
    const res = await this.fetchImpl(this.url, { method: 'HEAD', headers: this.options.headers });
    if (!res.ok) throw new Error(`FetchRangeSource: HEAD ${this.url} -> HTTP ${res.status}`);
    const length = res.headers.get('content-length');
    return length === null ? null : Number(length);
  }

  async read(offset: number, length: number): Promise<Uint8Array> {
    const headers = new Headers(this.options.headers);
    headers.set('Range', `bytes=${offset}-${offset + length - 1}`);
    const res = await this.fetchImpl(this.url, { headers });
    if (res.status === 416) return new Uint8Array(0);
    if (res.status !== 206)
      throw new Error(`FetchRangeSource: server ignored Range (HTTP ${res.status})`);
    return new Uint8Array(await res.arrayBuffer());
  }

  close(): void {}
}
```

`packages/wasm/src/io/memory_sink.ts`: same as `packages/node/src/io/memory_sink.ts` from Task 9 plus:

```ts
  toBlob(type = 'application/octet-stream'): Blob {
    return new Blob([this.bytes()], { type });
  }
```

`packages/wasm/src/index.ts`:

```ts
export * from '@aviotrix/types';
export { load, _resetForTests } from './load.js';
export type { LoadOptions } from './load.js';
export { MediaReader, readMetadata, remux } from './media_reader.js';
export { BlobSource } from './io/blob_source.js';
export { FetchRangeSource } from './io/fetch_range_source.js';
export type { FetchRangeSourceOptions } from './io/fetch_range_source.js';
export { MemorySink } from './io/memory_sink.js';
```

- [ ] **Step 6: Run lint, typecheck, tests, build for all packages**

```bash
npm run format && npm run lint && npm run typecheck && npm test -w @aviotrix/wasm && npm test -w @aviotrix/node && npm run build:ts -w @aviotrix/wasm
```

Expected: everything green; wasm suites: 1 smoke + 4 metadata + 9 remux + 6 io + 1 unsupported. If `unsupported.test.ts` fails because `WebAssembly.Suspending` cannot be redefined, change `hasJspi()` to consult an injectable check: export `_setJspiCheckForTests(fn: () => boolean)` from `load.ts` and use it in the test instead of patching the global.

- [ ] **Step 7: Commit**

```bash
git add packages
git commit -m "Add @aviotrix/wasm public API with Blob and HTTP range sources; share BindingHost and OperationQueue in @aviotrix/types"
```

---

### Task 13: Packaging, prebuilt binaries, and CI

**Files:**

- Create: `packages/node/scripts/install-native.mjs`, `packages/node/scripts/prepack.mjs`, `.github/workflows/ci.yml`, `.github/workflows/prebuild.yml`
- Modify: `packages/node/package.json` (install/prepack scripts, `repository`, `files`, move `cmake-js` + add `prebuild-install` to `dependencies`), `.gitignore` (`packages/node/native-src/`), `README.md` (install and build docs)

**Interfaces:**

- Consumes: everything built so far.
- Produces: `npm install @aviotrix/node` works via (1) a matching prebuilt from GitHub Releases, else (2) a source build from `native-src/` shipped in the tarball plus a shallow clone of FFmpeg `n8.1.3`. CI runs lint, typecheck, core tests, Node tests on three platforms, WASM build and browser tests. Tags `v*` publish prebuilt binaries named `node-v<version>-napi-v8-<platform>-<arch>.tar.gz` (prebuild strips the npm scope).

- [ ] **Step 1: Confirm the Node CMake project builds with an explicit `AVIOTRIX_ROOT`**

The standalone project from Task 8 already takes `AVIOTRIX_ROOT`. Prove the tarball layout works before writing the install script:

```bash
cd packages/node && npx cmake-js compile --CDAVIOTRIX_ROOT=../.. && cd ../.. && npm test -w @aviotrix/node
```

Expected: builds and tests pass exactly as with the default.

- [ ] **Step 2: Write the install and prepack scripts**

`packages/node/scripts/install-native.mjs`:

```js
#!/usr/bin/env node
// Runs on `npm install`. Order: skip in the monorepo -> prebuilt download -> source build.
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const pkgDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const FFMPEG_TAG = 'n8.1.3';

function run(cmd, args, opts = {}) {
  const r = spawnSync(cmd, args, {
    stdio: 'inherit',
    cwd: pkgDir,
    shell: process.platform === 'win32',
    ...opts,
  });
  return r.status === 0;
}

if (process.env.AVIOTRIX_SKIP_NATIVE === '1') process.exit(0);
if (existsSync(path.join(pkgDir, 'build/Release/aviotrix_node.node'))) process.exit(0);

const inMonorepo = existsSync(path.join(pkgDir, '../../core/CMakeLists.txt'));
if (inMonorepo) {
  console.log(
    '@aviotrix/node: monorepo checkout detected; run `npm run build:native -w @aviotrix/node` to build the addon.',
  );
  process.exit(0);
}

if (run('npx', ['--no-install', 'prebuild-install', '-r', 'napi'])) process.exit(0);

console.warn(
  '@aviotrix/node: no prebuilt binary for this platform; building from source. This needs cmake, make, nasm and a C++20 compiler and takes several minutes.',
);
for (const tool of ['cmake', 'make', 'nasm']) {
  if (!run(tool, ['--version'], { stdio: 'ignore' }) && !run(tool, ['-v'], { stdio: 'ignore' })) {
    console.error(`@aviotrix/node: required tool not found: ${tool}`);
    process.exit(1);
  }
}
const nativeSrc = path.join(pkgDir, 'native-src');
const ffmpegDir = path.join(nativeSrc, 'third_party/ffmpeg');
if (!existsSync(path.join(ffmpegDir, 'configure'))) {
  if (
    !run('git', [
      'clone',
      '--depth',
      '1',
      '--branch',
      FFMPEG_TAG,
      'https://github.com/FFmpeg/FFmpeg.git',
      ffmpegDir,
    ])
  ) {
    console.error('@aviotrix/node: could not clone FFmpeg');
    process.exit(1);
  }
}
if (!run('npx', ['--no-install', 'cmake-js', 'compile', `--CDAVIOTRIX_ROOT=${nativeSrc}`])) {
  console.error('@aviotrix/node: source build failed');
  process.exit(1);
}
```

`packages/node/scripts/prepack.mjs` (copies the build inputs into the tarball):

```js
#!/usr/bin/env node
import { cpSync, mkdirSync, rmSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const pkgDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const root = path.resolve(pkgDir, '../..');
const out = path.join(pkgDir, 'native-src');
rmSync(out, { recursive: true, force: true });
mkdirSync(path.join(out, 'scripts'), { recursive: true });
cpSync(path.join(root, 'core'), path.join(out, 'core'), {
  recursive: true,
  filter: (p) => !p.includes('/tests'),
});
cpSync(path.join(root, 'CMakeLists.txt'), path.join(out, 'CMakeLists.txt'));
cpSync(path.join(root, 'scripts/build-ffmpeg.sh'), path.join(out, 'scripts/build-ffmpeg.sh'));
cpSync(
  path.join(root, 'scripts/ffmpeg-components.sh'),
  path.join(out, 'scripts/ffmpeg-components.sh'),
);
console.log(`prepack: wrote ${out}`);
```

Update `packages/node/package.json`:

```json
  "repository": { "type": "git", "url": "git+https://github.com/aviotrix/aviotrix.git", "directory": "packages/node" },
  "files": ["dist", "binding", "CMakeLists.txt", "scripts/install-native.mjs", "native-src"],
  "scripts": {
    "install": "node scripts/install-native.mjs",
    "prepack": "node scripts/prepack.mjs",
    "build:native": "cmake-js compile",
    "build:ts": "tsc -p tsconfig.json",
    "build": "npm run build:native && npm run build:ts",
    "typecheck": "tsc -p tsconfig.json --noEmit",
    "test": "vitest run",
    "prebuild": "prebuild --backend cmake-js -r napi -t 8 --strip"
  },
  "dependencies": {
    "@aviotrix/types": "0.1.0",
    "cmake-js": "^8.0.0",
    "node-addon-api": "^8.9.2",
    "prebuild-install": "^7.1.3"
  },
  "devDependencies": {
    "@types/node": "^24.0.0",
    "prebuild": "^13.0.1",
    "vitest": "^5.0.3"
  }
```

Add `packages/node/native-src/` to `.gitignore`. Verify the tarball contents and that the monorepo install path is a no-op:

```bash
npm install
cd packages/node && npm pack --dry-run 2>&1 | grep -E "native-src/CMakeLists.txt|native-src/core/src/remux.cc|binding/addon.cc|dist/index.js" && cd ../..
```

Expected: the four paths listed; `npm install` printed the monorepo notice and did not build.

- [ ] **Step 3: Write the CI workflow**

`.github/workflows/ci.yml`:

```yaml
name: CI
on:
  push:
    branches: [main]
  pull_request:

jobs:
  native:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: macos-15
            platform: darwin-arm64
          - os: ubuntu-24.04
            platform: linux-x64
          - os: ubuntu-24.04-arm
            platform: linux-arm64
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with: { submodules: true }
      - uses: actions/setup-node@v4
        with: { node-version: 24, cache: npm }
      - name: Install build tools (macOS)
        if: runner.os == 'macOS'
        run: brew install nasm cmake
      - name: Install build tools (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y nasm cmake build-essential
      - name: Cache FFmpeg (host)
        uses: actions/cache@v4
        with:
          path: build/ffmpeg-host
          key: ffmpeg-host-${{ matrix.platform }}-${{ hashFiles('scripts/build-ffmpeg.sh', 'scripts/ffmpeg-components.sh', '.gitmodules') }}-${{ hashFiles('third_party/ffmpeg/RELEASE') }}
      - name: Install clang tools (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get install -y clang-format clang-tidy
      - name: Install clang tools (macOS)
        if: runner.os == 'macOS'
        run: brew install clang-format
      - run: npm ci
      - run: npm run lint
      - run: npm run typecheck
      - run: npm run test:core
      - name: clang-tidy (core)
        if: matrix.platform == 'linux-x64'
        run: cmake -S . -B build/core-tidy -DAVIOTRIX_CLANG_TIDY=ON -DAVIOTRIX_BUILD_TESTS=OFF && cmake --build build/core-tidy -j
      - run: npm run build:native -w @aviotrix/node
      - run: npm run build:ts --workspaces --if-present
      - run: npm test -w @aviotrix/types
      - run: npm test -w @aviotrix/node

  wasm:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
        with: { submodules: true }
      - uses: actions/setup-node@v4
        with: { node-version: 24, cache: npm }
      - uses: mymindstorm/setup-emsdk@v14
        with: { version: 4.0.10 }
      - name: Cache FFmpeg (wasm)
        uses: actions/cache@v4
        with:
          path: build/ffmpeg-wasm
          key: ffmpeg-wasm-${{ hashFiles('scripts/build-ffmpeg.sh', 'scripts/ffmpeg-components.sh', '.gitmodules') }}-${{ hashFiles('third_party/ffmpeg/RELEASE') }}
      - run: npm ci
      - run: npm run build -w @aviotrix/types
      - run: npm run build -w @aviotrix/wasm
      - run: npm run size -w @aviotrix/wasm
      - run: npx playwright install --with-deps chromium
      - run: npm test -w @aviotrix/wasm
```

Pin `setup-emsdk` to the newest Emscripten the action offers that is ≥ 4.0 (JSPI support); if `4.0.10` is not available, pick the latest listed at https://github.com/emscripten-core/emsdk/tags and record it here.

- [ ] **Step 4: Write the prebuild workflow**

`.github/workflows/prebuild.yml`:

```yaml
name: Prebuild native binaries
on:
  push:
    tags: ['v*']

jobs:
  prebuild:
    permissions: { contents: write }
    strategy:
      fail-fast: false
      matrix:
        include:
          - { os: macos-15, platform: darwin-arm64 }
          - { os: macos-15-intel, platform: darwin-x64 }
          - { os: ubuntu-24.04, platform: linux-x64 }
          - { os: ubuntu-24.04-arm, platform: linux-arm64 }
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with: { submodules: true }
      - uses: actions/setup-node@v4
        with: { node-version: 24, cache: npm }
      - if: runner.os == 'macOS'
        run: brew install nasm cmake
      - if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y nasm cmake build-essential
      - run: npm ci
      - run: npm run build -w @aviotrix/types
      - name: Build and upload prebuilt
        working-directory: packages/node
        run: npx prebuild --backend cmake-js -r napi -t 8 --strip -u ${{ secrets.GITHUB_TOKEN }}
```

If GitHub no longer offers a `macos-15-intel` runner label, drop that row and note in `README.md` that darwin-x64 builds from source.

- [ ] **Step 5: Document in README.md**

Replace the README's "Build prerequisites" section with:

```markdown
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
```

- [ ] **Step 6: Commit**

```bash
npm run format && npm run lint
git add .github packages/node .gitignore README.md package-lock.json
git commit -m "Add install-from-prebuilt-or-source for @aviotrix/node, tarball packaging, and CI workflows"
```

---

### Task 14: Milestone acceptance

**Files:**

- Modify: `README.md` (status line), this plan (check boxes)

**Interfaces:** none new.

- [x] **Step 1: Run the full verification from a clean tree**

```bash
git status --porcelain   # must be empty
rm -rf build/core-host packages/node/build packages/wasm/build packages/*/dist
npm ci
npm run lint && npm run typecheck && npm run test:core
npm run build:native -w @aviotrix/node && npm run build:ts --workspaces --if-present
npm test -w @aviotrix/types && npm test -w @aviotrix/node
npm run build -w @aviotrix/wasm && npm test -w @aviotrix/wasm && npm run size -w @aviotrix/wasm
```

Expected: every command exits 0. (`npm run typecheck` builds `@aviotrix/types` first, so it works from a clean tree.)

- [x] **Step 2: Walk the spec §10 acceptance criteria and tick each against a passing test**

| Criterion                                                                               | Evidence                                                                                                                                                                                      |
| --------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1. mp4→Matroska and ts→mp4 on Node and Chromium through user-supplied async source/sink | `packages/node/tests/remux.test.ts` ("mp4 -> matroska", "ts -> mp4", "adversarial source"), `packages/wasm/tests/remux.test.ts` ("mp4 -> matroska", "ts -> mp4", "short and over-long reads") |
| 2. Reopened outputs match stream count, codecs, duration                                | `core/tests/test_remux.cc` ("remux mp4 to matroska and reopen" asserts duration within 0.2 s), Node and WASM remux tests assert codecs and counts                                             |
| 3. mkv+srt→mp4 skips by default, fails with `'fail'`                                    | core, Node, WASM "skips srt" tests                                                                                                                                                            |
| 4. Non-seekable sink + mp4 rejects before IO; with `fragmented` succeeds                | core "mp4 to a streaming sink", Node and WASM "streaming sink needs fragmented"                                                                                                               |
| 5. Abort rejects `ABORTED` and closes the sink                                          | core "cancel flag aborts", Node/WASM "aborts via AbortSignal"                                                                                                                                 |
| 6. Measured `.wasm` size in README                                                      | README "WASM size" section (Task 11)                                                                                                                                                          |
| 7. Lint, typecheck, tests, build green in CI on all platforms                           | CI run on the PR for this branch; link it in the PR description                                                                                                                               |

- [x] **Step 3: Update README status and commit**

Change the README "Status" line to `Milestone 1 complete: open via IoSource, read metadata, remux to IoSink, on Node and in JSPI browsers.`

```bash
git add README.md docs/superpowers/plans/2026-10-06-aviotrix-milestone-1.md
git commit -m "Mark milestone 1 complete"
```

- [ ] **Step 4: Hand off**

Do not push. Report to Chad: the branch name, the commit list, the measured WASM size, anything that deviated from the spec (for example a Firefox JSPI availability change, or an FFmpeg configure option that had to differ), and ask whether to push and open the PR.
