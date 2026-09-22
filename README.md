# gd-ffmpeg

H.264 video playback for Godot 4.5+ on Linux, implemented as a `VideoStream` backed by
[FFmpeg](https://ffmpeg.org/). It plugs into the engine's existing video pipeline: `load()`
returns a `VideoStreamFFmpeg`, and a plain `VideoStreamPlayer` plays it like any other stream.

Currently only Ogg Theora (`.ogv`) is supported by core Godot. This extension adds H.264
(`.mp4`, `.mov`, `.mkv`, ...) without patching the engine.

```gdscript
extends VideoStreamPlayer

const VIDEO_PATH := "res://cutscenes/intro.mp4"

func _ready() -> void:
	stream = load(VIDEO_PATH)
	loop = true
	play()
```

## Features

- `VideoStreamFFmpeg` resource + `ResourceFormatLoader`, so `load("res://movie.mp4")` just works.
- Playback through the stock `VideoStreamPlayer`: `play()`, `stop()`, `pause`, `loop`,
  `speed_scale`, `stream_position`, `get_stream_length()`, `finished`, and the video texture are
  all handled by the engine, including seeking while paused.
- Frame-accurate, PTS-driven presentation (handles variable frame rate and B-frames), using
  the same timing semantics as `VideoStreamTheora`.
- Works with files inside exported PCKs: libav reads through Godot's `FileAccess` via a custom
  `AVIOContext`, not from disk directly.
- The FFmpeg shared libraries are bundled with the addon and are copied next to the exported
  executable automatically (declared in `[dependencies]` in the `.gdextension`).

## Requirements

- Godot **4.5 or newer** on **Linux x86_64**.
- No system FFmpeg installation is required when using a release build: the libraries are bundled.

## Installation

Grab `gdffmpeg-linux-x86_64.zip` from the releases and copy its `addons/` folder next to your
`project.godot`:

```
your_project/
├── project.godot
└── addons/
    └── gdffmpeg/
        ├── gdffmpeg.gdextension
        ├── linux/
        │   ├── libgdffmpeg.linux.template_debug.x86_64.so
        │   ├── libgdffmpeg.linux.template_release.x86_64.so
        │   ├── libavcodec.so.63
        │   ├── libavformat.so.63
        │   ├── libavutil.so.61
        │   └── libswscale.so.10
        └── licenses/
```

Restart the editor after adding the files. Video files in the project are picked up
automatically by the loader and included when exporting.

## Usage

There is nothing to configure: assign a video to `VideoStreamPlayer.stream`, either in the
inspector (drag an `.mp4` in) or from code:

```gdscript
var video: VideoStream = load("res://videos/trailer.mp4")
$VideoStreamPlayer.stream = video
$VideoStreamPlayer.autoplay = true
```

`VideoStreamFFmpeg` only adds the inherited `file` property. Everything else is provided by
`VideoStreamPlayer` and `VideoStream`.

### Supported containers

`mp4`, `m4v`, `mov`, `h264`, `264` and `mkv`, as long as the video track is **H.264**.

### Limitations

- **Video only.** Audio tracks are ignored; `VideoStreamPlayer.volume` has no effect.
- **Software decoding on the main thread.** 1080p is fine on a modern CPU; 4K may cause frame
  hitches. Threaded decoding is planned as a follow-up.
- **H.264 only.** Other codecs are rejected with a clear error message at load time.
- **Progressive content only.** Interlaced video is not deinterlaced, and rotation metadata
  (e.g. from phone recordings) is not applied.
- **No network streams or URLs.**
- **Linux x86_64 only** for now.

### A note on H.264 patents

H.264 is covered by patent pools (MPEG LA / Via LA). This is why Godot cannot ship H.264
decoding in core. Depending on where you distribute your project, you may need to obtain a
license. See [FFmpeg's legal page](https://www.ffmpeg.org/legal.html) for background.

## Building from source

### With the bundled FFmpeg build (recommended, self-contained)

```bash
git submodule update --init --recursive
bash thirdparty/build_ffmpeg.sh          # minimal LGPL FFmpeg 9.0.x -> thirdparty/ffmpeg/
scons ffmpeg_prefix=thirdparty/ffmpeg/linux-x86_64
```

`thirdparty/build_ffmpeg.sh` builds FFmpeg without `--enable-gpl`/`--enable-nonfree`,
containing only the H.264 decoder, the needed demuxers (`mov`, `matroska`, `h264`) and
`libswscale`. Install `nasm` first to enable SIMD optimizations (much faster decoding).

### Against the system FFmpeg

Install the development packages and build normally:

```bash
sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
scons
```

The extension is linked with `-Wl,-rpath,$ORIGIN`, so a release build finds the FFmpeg
libraries sitting next to it.

### CMake

The same sources build with CMake:

```bash
cmake -B build -DFFMPEG_PREFIX=$PWD/thirdparty/ffmpeg/linux-x86_64
cmake --build build
```

## Testing

```bash
bash scripts/run_tests.sh
```

The script builds the extension, generates the engine's extension list when needed, and runs
the GDScript integration tests headlessly (`godot --headless --fixed-fps 60`). Test fixtures
are tiny H.264 files under `project/tests/assets/`, regenerated with
`thirdparty/make_test_assets.sh`.

## Packaging

```bash
bash scripts/package_addon.sh
```

This builds debug and release flavors, makes sure the FFmpeg binaries are present, and writes
`dist/gdffmpeg-linux-x86_64.zip` containing the `addons/gdffmpeg` tree.

## License

- The extension itself is released into the public domain (Unlicense, see [LICENSE.md](LICENSE.md)).
- The bundled FFmpeg libraries are LGPL-2.1-or-later, built without GPL components. See
  `addons/gdffmpeg/licenses/`.
- Rebuilding against a modified FFmpeg is supported: run `thirdparty/build_ffmpeg.sh` with your
  changes and build with `scons ffmpeg_prefix=...`.
