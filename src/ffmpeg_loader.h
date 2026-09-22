#ifndef GDFFMPEG_FFMPEG_LOADER_H
#define GDFFMPEG_FFMPEG_LOADER_H

#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

/// Lets `load("res://movie.mp4")` return a VideoStreamFFmpeg.
///
/// Only the containers we can stream with the bundled FFmpeg build are claimed.
/// Loading probes the file once, so unsupported codecs are reported as early as
/// possible (i.e. when the resource is loaded) instead of at playback time.
class ResourceFormatLoaderFFmpeg : public ResourceFormatLoader {
	GDCLASS(ResourceFormatLoaderFFmpeg, ResourceFormatLoader)

protected:
	static void _bind_methods() {}

public:
	PackedStringArray _get_recognized_extensions() const override;
	bool _handles_type(const StringName &p_type) const override;
	String _get_resource_type(const String &p_path) const override;
	Variant _load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const override;
};

} // namespace godot

#endif // GDFFMPEG_FFMPEG_LOADER_H
