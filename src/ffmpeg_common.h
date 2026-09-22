#ifndef GDFFMPEG_FFMPEG_COMMON_H
#define GDFFMPEG_FFMPEG_COMMON_H

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <godot_cpp/variant/string.hpp>

namespace godot {

struct AVIOContextDeleter {
	void operator()(AVIOContext *p_ctx) const {
		if (p_ctx != nullptr) {
			avio_context_free(&p_ctx);
		}
	}
};

struct AVFormatContextDeleter {
	void operator()(AVFormatContext *p_ctx) const {
		if (p_ctx != nullptr) {
			avformat_close_input(&p_ctx);
		}
	}
};

struct AVCodecContextDeleter {
	void operator()(AVCodecContext *p_ctx) const {
		if (p_ctx != nullptr) {
			avcodec_free_context(&p_ctx);
		}
	}
};

struct AVPacketDeleter {
	void operator()(AVPacket *p_packet) const {
		if (p_packet != nullptr) {
			av_packet_free(&p_packet);
		}
	}
};

struct AVFrameDeleter {
	void operator()(AVFrame *p_frame) const {
		if (p_frame != nullptr) {
			av_frame_free(&p_frame);
		}
	}
};

using AVIOContextPtr = std::unique_ptr<AVIOContext, AVIOContextDeleter>;
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

/// Formats an FFmpeg error code (negative AVERROR) as a human readable string.
String ffmpeg_error_string(int p_error);

/// Routes libav's log output into Godot's console (called once at startup).
void ffmpeg_install_log_callback();

} // namespace godot

#endif // GDFFMPEG_FFMPEG_COMMON_H
