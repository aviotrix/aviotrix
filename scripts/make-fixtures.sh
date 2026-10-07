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
