#!/usr/bin/env bash
# Builds a minimal LGPL FFmpeg shared library set that only contains what
# gdffmpeg needs to decode H.264 video, and installs it into the addon.
#
# The resulting libraries are bundled with the addon and are covered by the
# LGPL-2.1-or-later (see thirdparty/FFmpeg-COPYING.LGPLv2.1), not by the MIT
# license of this repository. They are built without --enable-gpl and without
# --enable-nonfree, so no GPL components are linked in.
set -euo pipefail

cd "$(dirname "$0")/.."

FFMPEG_VERSION="${FFMPEG_VERSION:-9.0.1}"
ARCH="${ARCH:-x86_64}"
# SHA256 of ffmpeg-9.0.1.tar.xz, downloaded from https://ffmpeg.org/releases/
EXPECTED_SHA256="${FFMPEG_SHA256:-cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635}"

PREFIX="${PREFIX:-$(pwd)/thirdparty/ffmpeg/linux-${ARCH}}"
ADDON_DIR="${ADDON_DIR:-$(pwd)/project/addons/gdffmpeg}"
ADDON_LIBS="$ADDON_DIR/linux"
ADDON_LICENSES="$ADDON_DIR/licenses"
BUILD_DIR="${BUILD_DIR:-$(pwd)/thirdparty/ffmpeg/build}"
TARBALL="ffmpeg-${FFMPEG_VERSION}.tar.xz"
URL="https://ffmpeg.org/releases/${TARBALL}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f "$TARBALL" ]; then
	echo "Downloading $URL"
	curl -fL --retry 3 -o "$TARBALL" "$URL"
fi

echo "${EXPECTED_SHA256}  ${TARBALL}" | sha256sum -c -

if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
	tar xf "$TARBALL"
fi

cd "ffmpeg-${FFMPEG_VERSION}"

# x86 assembly (nasm/yasm) is optional, but decoding is significantly faster
# with it. Fall back to pure C with a warning when the assembler is missing.
ASM_FLAGS=()
if ! command -v nasm >/dev/null 2>&1 && ! command -v yasm >/dev/null 2>&1; then
	echo "warning: nasm/yasm not found, building without x86 assembly (slower decoding)" >&2
	echo "warning: install nasm (e.g. 'sudo apt install nasm') and rebuild for full speed" >&2
	ASM_FLAGS+=(--disable-x86asm)
fi

./configure \
	--prefix="$PREFIX" \
	--arch="$ARCH" \
	--disable-everything \
	--disable-gpl \
	--disable-nonfree \
	--disable-network \
	--disable-programs \
	--disable-doc \
	--disable-autodetect \
	--disable-static \
	--enable-shared \
	--enable-pic \
	--enable-avformat \
	--enable-avcodec \
	--enable-swscale \
	--enable-decoder=h264 \
	--enable-parser=h264 \
	--enable-demuxer=h264 \
	--enable-demuxer=mov \
	--enable-demuxer=matroska \
	--enable-protocol=file \
	"${ASM_FLAGS[@]}"

make -j"$(nproc)"
make install

# Bundle the shared objects next to the extension, named after their SONAMEs
# (which is what the dynamic linker looks for via DT_NEEDED).
mkdir -p "$ADDON_LIBS"
for lib in avcodec avformat avutil swscale; do
	real="$(readlink -f "$PREFIX/lib/lib${lib}.so")"
	if [ ! -f "$real" ]; then
		echo "error: $PREFIX/lib/lib${lib}.so not found" >&2
		exit 1
	fi
	soname="$(objdump -p "$real" | sed -n 's/^ *SONAME *//p' | head -n1)"
	if [ -z "$soname" ]; then
		soname="$(basename "$real" | cut -d. -f1-3)"
	fi
	cp -f "$real" "$ADDON_LIBS/$soname"
	echo "bundled $soname"
done

# Bundle the license and a notice describing how the libraries were built.
mkdir -p "$ADDON_LICENSES"
cp -f COPYING.LGPLv2.1 "$ADDON_LICENSES/FFmpeg-COPYING.LGPLv2.1"
cat >"$ADDON_LICENSES/FFmpeg-NOTICE.txt" <<EOF
This directory contains shared libraries built from FFmpeg ${FFMPEG_VERSION}.
They are licensed under the GNU Lesser General Public License, version 2.1 or
later (see FFmpeg-COPYING.LGPLv2.1). They are distributed dynamically linked,
without the --enable-gpl and --enable-nonfree configure options, so no GPL
components are included.

To relink the extension against a modified version of these libraries, rebuild
FFmpeg with thirdparty/build_ffmpeg.sh (or any LGPL-configured build) and point
the build at it with:

    scons ffmpeg_prefix=/path/to/ffmpeg

The FFmpeg source code is available at https://ffmpeg.org/download.html
EOF

echo
echo "FFmpeg $FFMPEG_VERSION installed to $PREFIX"
echo "Shared libraries copied to $ADDON_LIBS"
echo "License and notice copied to $ADDON_LICENSES"
