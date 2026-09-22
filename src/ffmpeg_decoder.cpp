#include "ffmpeg_decoder.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>
#include <cstdio>

using namespace godot;

// FileAccess based AVIO callbacks. FFmpeg never touches the filesystem itself,
// which is what makes res:// paths (including exported PCK files) work.

int FFmpegDecoder::_read_packet(void *p_opaque, uint8_t *p_buf, int p_buf_size) {
	FFmpegDecoder *self = static_cast<FFmpegDecoder *>(p_opaque);
	if (self == nullptr || self->m_file.is_null()) {
		return AVERROR(EIO);
	}
	uint64_t read = self->m_file->get_buffer(p_buf, (uint64_t)p_buf_size);
	// End of file must be reported as AVERROR_EOF, not as a zero-byte read:
	// returning 0 makes demuxers (e.g. mov on a truncated file) retry inside
	// avio_seek forever instead of treating the stream as finished.
	return read != 0 ? (int)read : AVERROR_EOF;
}

int64_t FFmpegDecoder::_seek_file(void *p_opaque, int64_t p_offset, int p_whence) {
	FFmpegDecoder *self = static_cast<FFmpegDecoder *>(p_opaque);
	if (self == nullptr || self->m_file.is_null()) {
		return -1;
	}
	Ref<FileAccess> file = self->m_file;
	if ((p_whence & AVSEEK_SIZE) != 0) {
		return (int64_t)file->get_length();
	}
	p_whence &= ~AVSEEK_FORCE;

	int64_t target = 0;
	switch (p_whence) {
		case SEEK_SET: {
			target = p_offset;
		} break;
		case SEEK_CUR: {
			target = (int64_t)file->get_position() + p_offset;
		} break;
		case SEEK_END: {
			target = (int64_t)file->get_length() + p_offset;
		} break;
		default: {
			return -1;
		} break;
	}
	if (target < 0) {
		return -1;
	}
	file->seek((uint64_t)target);
	return (int64_t)file->get_position();
}

Error FFmpegDecoder::open(const String &p_path) {
	close();

	m_file = FileAccess::open(p_path, FileAccess::READ);
	if (m_file.is_null()) {
		return ERR_CANT_OPEN;
	}

	constexpr int IO_BUFFER_SIZE = 1 << 16;
	uint8_t *io_buffer = (uint8_t *)av_malloc(IO_BUFFER_SIZE);
	if (io_buffer == nullptr) {
		close();
		return ERR_OUT_OF_MEMORY;
	}
	AVIOContext *io_ctx = avio_alloc_context(io_buffer, IO_BUFFER_SIZE, 0, this,
			&FFmpegDecoder::_read_packet, nullptr, &FFmpegDecoder::_seek_file);
	if (io_ctx == nullptr) {
		av_free(io_buffer);
		close();
		return ERR_OUT_OF_MEMORY;
	}
	m_io_ctx.reset(io_ctx);

	AVFormatContext *format_ctx = avformat_alloc_context();
	if (format_ctx == nullptr) {
		close();
		return ERR_OUT_OF_MEMORY;
	}
	format_ctx->pb = m_io_ctx.get();
	format_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

	// On failure avformat_open_input() frees format_ctx and sets it to nullptr.
	int ret = avformat_open_input(&format_ctx, "gdffmpeg", nullptr, nullptr);
	if (ret < 0) {
		close();
		ERR_PRINT(vformat("gdffmpeg: cannot open '%s': %s", p_path, ffmpeg_error_string(ret)));
		return ERR_CANT_OPEN;
	}
	m_format_ctx.reset(format_ctx);

	ret = avformat_find_stream_info(m_format_ctx.get(), nullptr);
	if (ret < 0) {
		close();
		ERR_PRINT(vformat("gdffmpeg: cannot read stream info from '%s': %s", p_path, ffmpeg_error_string(ret)));
		return ERR_CANT_OPEN;
	}

	int stream_index = av_find_best_stream(m_format_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (stream_index < 0) {
		close();
		ERR_PRINT(vformat("gdffmpeg: no video stream found in '%s'.", p_path));
		return ERR_FILE_UNRECOGNIZED;
	}
	m_video_stream = m_format_ctx->streams[stream_index];
	if (m_video_stream->start_time != AV_NOPTS_VALUE) {
		m_start_pts = m_video_stream->start_time;
	}

	return _open_codec(p_path);
}

Error FFmpegDecoder::_open_codec(const String &p_path) {
	const AVCodec *codec = avcodec_find_decoder(m_video_stream->codecpar->codec_id);
	if (codec == nullptr) {
		close();
		ERR_PRINT(vformat("gdffmpeg: no decoder available for codec '%s' in '%s'.", get_codec_name(), p_path));
		return ERR_UNAVAILABLE;
	}

	AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
	if (codec_ctx == nullptr) {
		close();
		return ERR_OUT_OF_MEMORY;
	}
	m_codec_ctx.reset(codec_ctx);

	int ret = avcodec_parameters_to_context(codec_ctx, m_video_stream->codecpar);
	if (ret < 0) {
		close();
		ERR_PRINT(vformat("gdffmpeg: cannot copy codec parameters from '%s': %s", p_path, ffmpeg_error_string(ret)));
		return ERR_INVALID_DATA;
	}
	codec_ctx->pkt_timebase = m_video_stream->time_base;
	// 0 lets libav pick a sane number of frame/slice threads.
	codec_ctx->thread_count = 0;

	ret = avcodec_open2(codec_ctx, codec, nullptr);
	if (ret < 0) {
		close();
		ERR_PRINT(vformat("gdffmpeg: cannot open decoder for codec '%s' in '%s': %s", get_codec_name(), p_path, ffmpeg_error_string(ret)));
		return ERR_CANT_CREATE;
	}

	m_packet.reset(av_packet_alloc());
	m_frame.reset(av_frame_alloc());
	if (m_packet == nullptr || m_frame == nullptr) {
		close();
		return ERR_OUT_OF_MEMORY;
	}

	m_last_frame_duration = get_frame_duration();
	return OK;
}

void FFmpegDecoder::close() {
	m_codec_ctx.reset();
	m_format_ctx.reset();
	m_io_ctx.reset();
	m_file.unref();
	m_video_stream = nullptr;
	if (m_sws_ctx != nullptr) {
		sws_freeContext(m_sws_ctx);
		m_sws_ctx = nullptr;
	}
	m_start_pts = 0;
	m_frame_time = -1.0;
	m_prev_pts = AV_NOPTS_VALUE;
	m_last_frame_duration = 0.0;
	m_have_pending_packet = false;
	m_draining = false;
	m_drained = false;
}

AVCodecID FFmpegDecoder::get_codec_id() const {
	if (m_video_stream == nullptr) {
		return AV_CODEC_ID_NONE;
	}
	return m_video_stream->codecpar->codec_id;
}

String FFmpegDecoder::get_codec_name() const {
	if (m_video_stream == nullptr) {
		return String();
	}
	return String(avcodec_get_name(m_video_stream->codecpar->codec_id));
}

int FFmpegDecoder::get_width() const {
	if (m_codec_ctx != nullptr) {
		return m_codec_ctx->width;
	}
	if (m_video_stream != nullptr) {
		return m_video_stream->codecpar->width;
	}
	return 0;
}

int FFmpegDecoder::get_height() const {
	if (m_codec_ctx != nullptr) {
		return m_codec_ctx->height;
	}
	if (m_video_stream != nullptr) {
		return m_video_stream->codecpar->height;
	}
	return 0;
}

double FFmpegDecoder::get_duration() const {
	if (m_video_stream == nullptr) {
		return 0.0;
	}
	if (m_video_stream->duration != AV_NOPTS_VALUE && m_video_stream->duration > 0) {
		return (double)m_video_stream->duration * av_q2d(m_video_stream->time_base);
	}
	if (m_format_ctx != nullptr && m_format_ctx->duration != AV_NOPTS_VALUE && m_format_ctx->duration > 0) {
		return (double)m_format_ctx->duration / (double)AV_TIME_BASE;
	}
	return 0.0;
}

double FFmpegDecoder::get_frame_duration() const {
	if (m_last_frame_duration > 0.0) {
		return m_last_frame_duration;
	}
	if (m_video_stream != nullptr) {
		AVRational frame_rate = m_video_stream->avg_frame_rate;
		if (frame_rate.num > 0 && frame_rate.den > 0) {
			return av_q2d(frame_rate);
		}
		frame_rate = m_video_stream->r_frame_rate;
		if (frame_rate.num > 0 && frame_rate.den > 0) {
			return av_q2d(frame_rate);
		}
	}
	return 1.0 / 30.0;
}

void FFmpegDecoder::_update_frame_time() {
	int64_t ts = m_frame->best_effort_timestamp;
	if (ts == AV_NOPTS_VALUE) {
		ts = m_frame->pts;
	}
	const double time_base = av_q2d(m_video_stream->time_base);

	if (ts == AV_NOPTS_VALUE) {
		// No timestamps at all: synthesize a monotonic clock from frame durations.
		m_frame_time = (m_frame_time < 0.0) ? 0.0 : m_frame_time + m_last_frame_duration;
		return;
	}

	m_frame_time = (double)(ts - m_start_pts) * time_base;
	if (m_frame_time < 0.0) {
		m_frame_time = 0.0;
	}
	if (m_prev_pts != AV_NOPTS_VALUE) {
		double delta = (double)(ts - m_prev_pts) * time_base;
		if (delta > 0.0 && delta < 10.0) {
			m_last_frame_duration = delta;
		}
	}
	m_prev_pts = ts;
}

int FFmpegDecoder::decode_next() {
	if (m_codec_ctx == nullptr) {
		return AVERROR(EINVAL);
	}
	if (m_drained) {
		return 0;
	}

	while (true) {
		int ret = avcodec_receive_frame(m_codec_ctx.get(), m_frame.get());
		if (ret == 0) {
			_update_frame_time();
			return 1;
		}
		if (ret == AVERROR_EOF) {
			m_drained = true;
			return 0;
		}
		if (ret != AVERROR(EAGAIN)) {
			return ret;
		}

		// The decoder wants more input.
		if (m_have_pending_packet) {
			int send_ret = avcodec_send_packet(m_codec_ctx.get(), m_packet.get());
			if (send_ret == AVERROR(EAGAIN)) {
				continue; // Receive the frames it can still produce, then retry.
			}
			if (send_ret < 0) {
				return send_ret;
			}
			m_have_pending_packet = false;
			av_packet_unref(m_packet.get());
			continue;
		}

		if (m_draining) {
			continue; // Draining; receive_frame() eventually returns AVERROR_EOF.
		}

		int read_ret = av_read_frame(m_format_ctx.get(), m_packet.get());
		if (read_ret < 0) {
			// End of file: tell the decoder to flush.
			int send_ret = avcodec_send_packet(m_codec_ctx.get(), nullptr);
			if (send_ret == AVERROR(EAGAIN)) {
				continue;
			}
			if (send_ret < 0 && send_ret != AVERROR_EOF) {
				return send_ret;
			}
			m_draining = true;
			continue;
		}

		if (m_packet->stream_index != m_video_stream->index) {
			av_packet_unref(m_packet.get());
			continue;
		}
		m_have_pending_packet = true;
	}
}

Error FFmpegDecoder::seek_keyframe(double p_time) {
	if (m_format_ctx == nullptr || m_video_stream == nullptr || m_codec_ctx == nullptr) {
		return ERR_UNCONFIGURED;
	}
	const double time_base = av_q2d(m_video_stream->time_base);
	if (time_base <= 0.0) {
		return ERR_UNCONFIGURED;
	}
	const int64_t target = (int64_t)std::llround(p_time / time_base) + m_start_pts;
	int ret = av_seek_frame(m_format_ctx.get(), m_video_stream->index, target, AVSEEK_FLAG_BACKWARD);
	if (ret < 0) {
		return ERR_CANT_CREATE;
	}

	avcodec_flush_buffers(m_codec_ctx.get());
	av_packet_unref(m_packet.get());
	av_frame_unref(m_frame.get());
	m_have_pending_packet = false;
	m_draining = false;
	m_drained = false;
	m_prev_pts = AV_NOPTS_VALUE;
	m_frame_time = -1.0;
	return OK;
}

bool FFmpegDecoder::convert_to_rgba(PackedByteArray &r_buffer) {
	if (m_frame == nullptr || m_codec_ctx == nullptr) {
		return false;
	}
	const int width = m_frame->width;
	const int height = m_frame->height;
	if (width <= 0 || height <= 0) {
		return false;
	}

	// Honor the frame's color metadata when present, fall back to the common
	// resolution based heuristic (HD and up is BT.709).
	int source_colorspace = SWS_CS_ITU601;
	switch (m_frame->colorspace) {
		case AVCOL_SPC_BT709: {
			source_colorspace = SWS_CS_ITU709;
		} break;
		case AVCOL_SPC_SMPTE170M:
		case AVCOL_SPC_BT470BG:
		case AVCOL_SPC_FCC: {
			source_colorspace = SWS_CS_ITU601;
		} break;
		case AVCOL_SPC_BT2020_NCL:
		case AVCOL_SPC_BT2020_CL: {
			source_colorspace = SWS_CS_BT2020;
		} break;
		default: {
			source_colorspace = (height >= 720) ? SWS_CS_ITU709 : SWS_CS_ITU601;
		} break;
	}
	const int source_range = (m_frame->color_range == AVCOL_RANGE_JPEG) ? 1 : 0;

	SwsContext *sws_ctx = sws_getCachedContext(m_sws_ctx,
			width, height, (AVPixelFormat)m_frame->format,
			width, height, AV_PIX_FMT_RGBA,
			SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (sws_ctx == nullptr) {
		return false;
	}
	m_sws_ctx = sws_ctx;

	const int destination_range = 1; // Godot images are full range RGBA.
	sws_setColorspaceDetails(m_sws_ctx,
			sws_getCoefficients(source_colorspace), source_range,
			sws_getCoefficients(SWS_CS_DEFAULT), destination_range,
			0, 1 << 16, 1 << 16);

	const int64_t size = (int64_t)width * 4 * height;
	if (r_buffer.size() != size) {
		r_buffer.resize(size);
	}

	uint8_t *destination[4] = { (uint8_t *)r_buffer.ptrw(), nullptr, nullptr, nullptr };
	int destination_stride[4] = { width * 4, 0, 0, 0 };
	int ret = sws_scale(m_sws_ctx, m_frame->data, m_frame->linesize, 0, height, destination, destination_stride);
	return ret > 0;
}

Error FFmpegDecoder::probe(const String &p_path, FFmpegVideoInfo &r_info) {
	FFmpegDecoder decoder;
	Error err = decoder.open(p_path);
	if (err != OK) {
		return err;
	}
	r_info.valid = true;
	r_info.codec_id = decoder.get_codec_id();
	r_info.codec_name = decoder.get_codec_name();
	r_info.width = decoder.get_width();
	r_info.height = decoder.get_height();
	r_info.duration = decoder.get_duration();
	r_info.frame_duration = decoder.get_frame_duration();
	return OK;
}
