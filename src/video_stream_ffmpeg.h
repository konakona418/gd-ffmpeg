#ifndef GDFFMPEG_VIDEO_STREAM_FFMPEG_H
#define GDFFMPEG_VIDEO_STREAM_FFMPEG_H

#include <godot_cpp/classes/video_stream.hpp>
#include <godot_cpp/classes/video_stream_playback.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

/// VideoStream resource backed by FFmpeg (H.264 only, video only).
///
/// Loaded either through ResourceLoader (the bundled ResourceFormatLoaderFFmpeg
/// claims mp4/m4v/mov/h264/264/mkv) or by setting `file` from script. Playback is
/// provided by VideoStreamPlaybackFFmpeg and driven by VideoStreamPlayer.
class VideoStreamFFmpeg : public VideoStream {
	GDCLASS(VideoStreamFFmpeg, VideoStream)

protected:
	static void _bind_methods() {}

public:
	Ref<VideoStreamPlayback> _instantiate_playback() override;
};

} // namespace godot

#endif // GDFFMPEG_VIDEO_STREAM_FFMPEG_H
