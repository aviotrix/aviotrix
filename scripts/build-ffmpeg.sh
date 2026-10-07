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
echo "build-ffmpeg: installed $target to $prefix"
