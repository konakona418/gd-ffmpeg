#!/usr/bin/env bash
# Builds both flavors of the extension, makes sure the bundled FFmpeg libraries
# exist, and writes a distributable addon zip to dist/.
set -euo pipefail

cd "$(dirname "$0")/.."

ARCH="${ARCH:-x86_64}"
FFMPEG_PREFIX="${FFMPEG_PREFIX:-$(pwd)/thirdparty/ffmpeg/linux-${ARCH}}"
ADDON_DIR="project/addons/gdffmpeg"
DIST_DIR="dist"
NAME="gdffmpeg-linux-${ARCH}"

if ! ls "$ADDON_DIR"/linux/libavcodec.so.* >/dev/null 2>&1; then
	echo "== bundled FFmpeg libraries are missing, building them =="
	bash thirdparty/build_ffmpeg.sh
fi

echo "== building extension (template_release) =="
scons target=template_release -j"$(nproc)" ffmpeg_prefix="$FFMPEG_PREFIX"

echo "== building extension (template_debug) =="
scons target=template_debug -j"$(nproc)" ffmpeg_prefix="$FFMPEG_PREFIX"

rm -rf "${DIST_DIR:?}/${NAME}"
mkdir -p "$DIST_DIR/$NAME/addons"
cp -r "$ADDON_DIR" "$DIST_DIR/$NAME/addons/gdffmpeg"

# Sanity check the layout before zipping.
test -f "$DIST_DIR/$NAME/addons/gdffmpeg/gdffmpeg.gdextension"
for lib in avcodec avformat avutil swscale; do
	ls "$DIST_DIR/$NAME/addons/gdffmpeg/linux/lib${lib}.so."* >/dev/null
done
for lib in "$DIST_DIR/$NAME/addons/gdffmpeg/linux/libgdffmpeg."*.so; do
	test -f "$lib"
done

(cd "$DIST_DIR" && rm -f "$NAME.zip" && zip -qr "$NAME.zip" "$NAME")

echo
echo "wrote $DIST_DIR/$NAME.zip"
