#include "video_stream_ffmpeg.h"

#include "video_stream_playback_ffmpeg.h"

#include <godot_cpp/core/error_macros.hpp>

using namespace godot;

Ref<VideoStreamPlayback> VideoStreamFFmpeg::_instantiate_playback() {
	const String path = get_file();
	if (path.is_empty()) {
		ERR_PRINT("gdffmpeg: VideoStreamFFmpeg has no file set.");
		return Ref<VideoStreamPlayback>();
	}

	Ref<VideoStreamPlaybackFFmpeg> playback;
	playback.instantiate();
	if (playback->load(path) != OK) {
		return Ref<VideoStreamPlayback>();
	}
	return playback;
}
