#include "ffmpeg_loader.h"

#include "ffmpeg_decoder.h"
#include "video_stream_ffmpeg.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace {

const char *const FFMPEG_EXTENSIONS[] = { "mp4", "m4v", "mov", "h264", "264", "mkv" };
constexpr int FFMPEG_EXTENSION_COUNT = sizeof(FFMPEG_EXTENSIONS) / sizeof(FFMPEG_EXTENSIONS[0]);

bool has_ffmpeg_extension(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	for (int i = 0; i < FFMPEG_EXTENSION_COUNT; i++) {
		if (extension == FFMPEG_EXTENSIONS[i]) {
			return true;
		}
	}
	return false;
}

} // namespace

PackedStringArray ResourceFormatLoaderFFmpeg::_get_recognized_extensions() const {
	PackedStringArray extensions;
	for (int i = 0; i < FFMPEG_EXTENSION_COUNT; i++) {
		extensions.push_back(FFMPEG_EXTENSIONS[i]);
	}
	return extensions;
}

bool ResourceFormatLoaderFFmpeg::_handles_type(const StringName &p_type) const {
	return p_type == StringName("VideoStream") || p_type == StringName("VideoStreamFFmpeg") || p_type == StringName("Resource");
}

String ResourceFormatLoaderFFmpeg::_get_resource_type(const String &p_path) const {
	return has_ffmpeg_extension(p_path) ? String("VideoStreamFFmpeg") : String();
}

Variant ResourceFormatLoaderFFmpeg::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	const String path = p_path.is_empty() ? p_original_path : p_path;
	if (!has_ffmpeg_extension(path)) {
		return Variant();
	}

	FFmpegVideoInfo info;
	Error err = FFmpegDecoder::probe(path, info);
	if (err != OK || !info.valid) {
		ERR_PRINT(vformat("gdffmpeg: cannot load video '%s'.", path));
		return Variant();
	}
	if (info.codec_id != AV_CODEC_ID_H264) {
		ERR_PRINT(vformat("gdffmpeg: '%s' uses video codec '%s'; only H.264 is supported.", path, info.codec_name));
		return Variant();
	}

	Ref<VideoStreamFFmpeg> stream;
	stream.instantiate();
	stream->set_file(path);
	return stream;
}
