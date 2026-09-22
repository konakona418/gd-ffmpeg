#ifndef GDFFMPEG_VIDEO_STREAM_PLAYBACK_FFMPEG_H
#define GDFFMPEG_VIDEO_STREAM_PLAYBACK_FFMPEG_H

#include "ffmpeg_decoder.h"

#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/video_stream_playback.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {

/// Video only playback for VideoStreamFFmpeg.
///
/// Decoding happens on the main thread inside `_update()`, mirroring the engine's
/// Theora implementation: the playback clock advances by delta, frames whose
/// presentation time has passed are decoded and the newest due frame is uploaded
/// to a single, persistent ImageTexture.
class VideoStreamPlaybackFFmpeg : public VideoStreamPlayback {
	GDCLASS(VideoStreamPlaybackFFmpeg, VideoStreamPlayback)

	/// If the clock falls further behind than this, seek instead of decoding
	/// every intermediate frame (e.g. after the game window was dragged).
	static constexpr double LAG_RESYNC_THRESHOLD = 1.0;
	static constexpr int32_t MAX_CONSECUTIVE_ERRORS = 3;

	FFmpegDecoder m_decoder;
	String m_path;
	Ref<ImageTexture> m_texture;
	PackedByteArray m_rgba;
	bool m_rgba_dirty = false;

	double m_time = 0.0;
	double m_current_frame_time = -1.0;
	double m_pending_time = 0.0;
	double m_last_frame_duration = 1.0 / 30.0;
	bool m_have_pending = false;

	bool m_playing = false;
	bool m_paused = false;
	bool m_eof = false;
	bool m_failed = false;
	bool m_seekable = true;
	int32_t m_consecutive_errors = 0;

protected:
	static void _bind_methods() {}

public:
	VideoStreamPlaybackFFmpeg();
	~VideoStreamPlaybackFFmpeg() override;

	/// Opens the video file and prepares the texture. Called before the playback
	/// is handed to VideoStreamPlayer (which queries the texture immediately).
	Error load(const String &p_path);
	bool is_open() const { return m_decoder.is_open(); }

	bool _is_paused() const override;
	void _update(double p_delta) override;
	bool _is_playing() const override;
	void _set_paused(bool p_paused) override;
	void _play() override;
	void _stop() override;
	void _seek(double p_time) override;
	double _get_length() const override;
	Ref<Texture2D> _get_texture() const override;
	double _get_playback_position() const override;
	int32_t _get_mix_rate() const override;
	int32_t _get_channels() const override;

private:
	/// Decodes until the next frame lies beyond p_target_time, converting every
	/// frame that is due into m_rgba (the last one wins).
	void _advance(double p_target_time);
	void _upload_texture();
	void _seek_internal(double p_time, bool p_upload);
};

} // namespace godot

#endif // GDFFMPEG_VIDEO_STREAM_PLAYBACK_FFMPEG_H
