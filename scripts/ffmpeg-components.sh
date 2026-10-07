# shellcheck shell=bash disable=SC2034,SC2054
# Shared FFmpeg configure component list. See spec §6.
# Note: --disable-postproc omitted; libpostproc was removed in FFmpeg 8.
FFMPEG_COMMON_FLAGS=(
  --disable-everything
  --disable-programs --disable-doc
  --disable-network --disable-protocols
  --disable-avdevice --disable-avfilter --disable-swscale --disable-swresample
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
