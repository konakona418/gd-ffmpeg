#ifndef GDFFMPEG_FFMPEG_DECODER_H
#define GDFFMPEG_FFMPEG_DECODER_H

#include "ffmpeg_common.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

/// Result of probing a video file's metadata without starting playback.
struct FFmpegVideoInfo {
	bool valid = false;
	AVCodecID codec_id = AV_CODEC_ID_NONE;
	String codec_name;
	int width = 0;
	int height = 0;
	/// Duration in seconds, 0 if unknown.
	double duration = 0.0;
	/// Duration of one frame in seconds, a sensible fallback if unknown.
	double frame_duration = 1.0 / 30.0;
};

/// Demuxes and software-decodes the best video stream of a file.
///
/// Reads through Godot's FileAccess (so files inside an exported PCK work) and
/// is single threaded: all decoding happens on the calling thread.
class FFmpegDecoder {
public:
	FFmpegDecoder() = default;
	~FFmpegDecoder() { close(); }

	FFmpegDecoder(const FFmpegDecoder &) = delete;
	FFmpegDecoder &operator=(const FFmpegDecoder &) = delete;

	/// Opens p_path and prepares the decoder for its best video stream.
	Error open(const String &p_path);
	void close();
	bool is_open() const { return m_format_ctx != nullptr; }

	/// Probes metadata only; does not keep any state.
	static Error probe(const String &p_path, FFmpegVideoInfo &r_info);

	AVCodecID get_codec_id() const;
	String get_codec_name() const;
	int get_width() const;
	int get_height() const;
	/// Duration in seconds, 0 if unknown.
	double get_duration() const;
	/// Duration of one frame in seconds.
	double get_frame_duration() const;

	/// Decodes the next frame into an internal slot.
	/// @return 1 if a frame is available, 0 on end of stream, negative AVERROR on error.
	int decode_next();
	AVFrame *get_frame() { return m_frame.get(); }
	/// Presentation time of the current frame in seconds, relative to the stream
	/// start. -1 if no frame has been decoded yet.
	double get_frame_time() const { return m_frame_time; }

	/// Seeks to the last keyframe at or before p_time and flushes the decoder.
	Error seek_keyframe(double p_time);

	/// Converts the current frame to RGBA8 into r_buffer (width * height * 4
	/// bytes). The buffer is resized as needed.
	bool convert_to_rgba(PackedByteArray &r_buffer);

private:
	Error _open_codec(const String &p_path);
	void _update_frame_time();

	static int _read_packet(void *p_opaque, uint8_t *p_buf, int p_buf_size);
	static int64_t _seek_file(void *p_opaque, int64_t p_offset, int p_whence);

	Ref<FileAccess> m_file;
	AVIOContextPtr m_io_ctx;
	AVFormatContextPtr m_format_ctx;
	AVCodecContextPtr m_codec_ctx;
	AVStream *m_video_stream = nullptr;
	AVPacketPtr m_packet;
	AVFramePtr m_frame;
	SwsContext *m_sws_ctx = nullptr;

	/// First presentation timestamp of the stream; subtracted so that playback
	/// positions start at zero even for containers with a non-zero start time.
	int64_t m_start_pts = 0;
	double m_frame_time = -1.0;
	int64_t m_prev_pts = AV_NOPTS_VALUE;
	double m_last_frame_duration = 0.0;

	/// A packet read from the container that has not been accepted by the
	/// decoder yet.
	bool m_have_pending_packet = false;
	bool m_draining = false;
	bool m_drained = false;
};

} // namespace godot

#endif // GDFFMPEG_FFMPEG_DECODER_H
