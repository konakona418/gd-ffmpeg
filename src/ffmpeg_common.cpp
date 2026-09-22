#include "ffmpeg_common.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdarg>

using namespace godot;

namespace {

void gdffmpeg_log_callback(void *p_av_class, int p_level, const char *p_format, va_list p_args) {
	if (p_level > av_log_get_level()) {
		return;
	}
	char line[1024];
	int print_prefix = 1;
	av_log_format_line2(p_av_class, p_level, p_format, p_args, line, sizeof(line), &print_prefix);
	const String message = String("[ffmpeg] ") + String(line).strip_edges();
	if (p_level <= AV_LOG_ERROR) {
		UtilityFunctions::printerr(message);
	} else {
		UtilityFunctions::print_verbose(message);
	}
}

} // namespace

void godot::ffmpeg_install_log_callback() {
	av_log_set_level(AV_LOG_WARNING);
	av_log_set_callback(gdffmpeg_log_callback);
}

String godot::ffmpeg_error_string(int p_error) {
	char buffer[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	if (av_strerror(p_error, buffer, sizeof(buffer)) < 0) {
		return vformat("FFmpeg error %d", p_error);
	}
	return String(buffer);
}
