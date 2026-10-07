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
