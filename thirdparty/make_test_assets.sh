#!/usr/bin/env bash
# Generates the small H.264 fixtures used by the integration tests.
# Requires a system FFmpeg CLI built with libx264 (and libx265 for the rejection test).
set -euo pipefail

cd "$(dirname "$0")/.."
out="project/tests/assets"
mkdir -p "$out"

common=(-hide_banner -loglevel error -y)

# Constant frame rate, short GOP, faststart so the moov box is at the front.
ffmpeg "${common[@]}" -f lavfi -i "testsrc2=size=64x48:rate=30" -t 2 \
	-c:v libx264 -preset ultrafast -qp 28 -g 15 -pix_fmt yuv420p -an \
	-movflags +faststart "$out/cfr.mp4"

# Variable frame rate: irregular presentation timestamps.
ffmpeg "${common[@]}" -f lavfi -i "testsrc2=size=64x48:rate=30" \
	-vf "setpts='PTS+0.04*mod(N,3)'" -t 2 -fps_mode vfr \
	-c:v libx264 -preset ultrafast -qp 28 -pix_fmt yuv420p -an \
	"$out/vfr.mp4"

# B-frames (frame reordering).
ffmpeg "${common[@]}" -f lavfi -i "testsrc2=size=64x48:rate=30" -t 2 \
	-c:v libx264 -preset veryfast -profile:v high -bf 3 -g 30 -qp 28 -pix_fmt yuv420p -an \
	"$out/bframes.mp4"

# 10-bit H.264 (converted to RGBA8 by swscale; still H.264 so it must be accepted).
if ffmpeg "${common[@]}" -f lavfi -i "testsrc2=size=64x48:rate=30" -t 1 \
	-c:v libx264 -preset ultrafast -qp 28 -profile:v high10 -pix_fmt yuv420p10le -an \
	"$out/h264_10bit.mp4" 2>/dev/null; then
	echo "generated h264_10bit.mp4"
else
	echo "warning: libx264 without 10-bit support, skipping h264_10bit.mp4" >&2
fi

# HEVC: must be rejected by the loader.
ffmpeg "${common[@]}" -f lavfi -i "testsrc2=size=64x48:rate=10" -t 1 \
	-c:v libx265 -preset ultrafast -x265-params log-level=none -pix_fmt yuv420p -an \
	"$out/hevc.mp4"

# Audio only: must be rejected (no video stream).
ffmpeg "${common[@]}" -f lavfi -i "sine=frequency=440:duration=1" \
	-c:a aac -b:a 32k -vn "$out/audio_only.mp4"

# Valid header, truncated payload: must load and end gracefully.
size=$(stat -c%s "$out/cfr.mp4")
head -c $((size / 3)) "$out/cfr.mp4" >"$out/truncated.mp4"

# Raw Annex B H.264 without a container (no duration, not seekable).
ffmpeg "${common[@]}" -f lavfi -i "testsrc2=size=64x48:rate=30" -t 1 \
	-c:v libx264 -preset ultrafast -qp 28 -pix_fmt yuv420p -an -f h264 \
	"$out/raw.h264"

ls -l "$out"
