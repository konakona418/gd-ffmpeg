#include "video_stream_playback_ffmpeg.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

VideoStreamPlaybackFFmpeg::VideoStreamPlaybackFFmpeg() {
	// The texture object must exist from the start: VideoStreamPlayer caches the
	// Ref returned by get_texture() and listens to its "changed" signal, so it
	// must never be replaced after the playback is handed over.
	m_texture.instantiate();
}

VideoStreamPlaybackFFmpeg::~VideoStreamPlaybackFFmpeg() {
	m_decoder.close();
}

Error VideoStreamPlaybackFFmpeg::load(const String &p_path) {
	Error err = m_decoder.open(p_path);
	if (err != OK) {
		return err;
	}
	if (m_decoder.get_codec_id() != AV_CODEC_ID_H264) {
		ERR_PRINT(vformat("gdffmpeg: '%s' uses video codec '%s'; only H.264 is supported.", p_path, m_decoder.get_codec_name()));
		m_decoder.close();
		return ERR_UNAVAILABLE;
	}

	const int width = m_decoder.get_width();
	const int height = m_decoder.get_height();
	if (width <= 0 || height <= 0) {
		ERR_PRINT(vformat("gdffmpeg: '%s' reports an invalid video size (%dx%d).", p_path, width, height));
		m_decoder.close();
		return ERR_INVALID_DATA;
	}

	m_last_frame_duration = m_decoder.get_frame_duration();
	m_path = p_path;
	Ref<Image> image = Image::create_empty(width, height, false, Image::FORMAT_RGBA8);
	m_texture->set_image(image);

	m_time = 0.0;
	m_current_frame_time = -1.0;
	m_have_pending = false;
	m_eof = false;
	m_failed = false;
	m_seekable = true;
	m_consecutive_errors = 0;
	m_rgba_dirty = false;
	return OK;
}

void VideoStreamPlaybackFFmpeg::_advance(double p_target_time) {
	while (true) {
		if (!m_have_pending) {
			int ret = m_decoder.decode_next();
			if (ret == 0) {
				m_eof = true;
				return;
			}
			if (ret < 0) {
				m_consecutive_errors++;
				ERR_PRINT(vformat("gdffmpeg: decode error: %s", ffmpeg_error_string(ret)));
				if (m_consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
					m_failed = true;
					m_playing = false;
				}
				return;
			}
			m_consecutive_errors = 0;
			m_have_pending = true;
			m_pending_time = m_decoder.get_frame_time();
			if (m_pending_time < 0.0) {
				// No usable timestamps: synthesize a monotonic clock.
				m_pending_time = (m_current_frame_time >= 0.0) ? m_current_frame_time + m_last_frame_duration : 0.0;
			}
		}

		if (m_pending_time > p_target_time) {
			return;
		}

		const int width = m_decoder.get_width();
		const int height = m_decoder.get_height();
		if (width > 0 && height > 0 && m_decoder.convert_to_rgba(m_rgba)) {
			m_current_frame_time = m_pending_time;
			m_last_frame_duration = m_decoder.get_frame_duration();
			m_rgba_dirty = true;
		}
		m_have_pending = false;
	}
}

void VideoStreamPlaybackFFmpeg::_upload_texture() {
	if (!m_rgba_dirty) {
		return;
	}
	const int width = m_decoder.get_width();
	const int height = m_decoder.get_height();
	if (width <= 0 || height <= 0) {
		m_rgba_dirty = false;
		return;
	}
	Ref<Image> image = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, m_rgba);
	if (m_texture->get_width() != width || m_texture->get_height() != height) {
		// The video resolution changed mid-stream; recreate the texture data
		// (the ImageTexture object itself must stay the same).
		m_texture->set_image(image);
	} else {
		m_texture->update(image);
	}
	m_rgba_dirty = false;
}

void VideoStreamPlaybackFFmpeg::_seek_internal(double p_time, bool p_upload) {
	if (!m_decoder.is_open()) {
		return;
	}
	if (p_time < 0.0) {
		p_time = 0.0;
	}
	const double length = m_decoder.get_duration();
	if (length > 0.0 && p_time > length) {
		p_time = length;
	}

	m_time = p_time;
	m_have_pending = false;
	m_eof = false;
	m_failed = false;
	m_consecutive_errors = 0;
	m_current_frame_time = -1.0;
	m_rgba_dirty = false;

	if (m_seekable) {
		Error err = m_decoder.seek_keyframe(p_time);
		if (err != OK) {
			m_seekable = false;
			WARN_PRINT(vformat("gdffmpeg: '%s' cannot be seeked; playback continues without seeking.", m_path));
		}
	}

	_advance(p_time);
	if (p_upload) {
		_upload_texture();
	}
}

bool VideoStreamPlaybackFFmpeg::_is_paused() const {
	return m_paused;
}

void VideoStreamPlaybackFFmpeg::_update(double p_delta) {
	if (!m_playing || m_paused || m_failed || !m_decoder.is_open()) {
		return;
	}

	m_time += p_delta;

	if (m_current_frame_time >= 0.0 && m_time - m_current_frame_time > LAG_RESYNC_THRESHOLD) {
		_seek_internal(m_time, true);
	}

	_advance(m_time);
	_upload_texture();

	if (m_eof) {
		// Give the last frame its display duration before reporting the end, so
		// VideoStreamPlayer can emit "finished" or loop cleanly.
		const double end_time = (m_current_frame_time >= 0.0) ? m_current_frame_time + m_last_frame_duration : m_time;
		if (m_time >= end_time) {
			_stop();
		}
	}
}

bool VideoStreamPlaybackFFmpeg::_is_playing() const {
	return m_playing;
}

void VideoStreamPlaybackFFmpeg::_set_paused(bool p_paused) {
	m_paused = p_paused;
}

void VideoStreamPlaybackFFmpeg::_play() {
	if (m_playing || m_failed || !m_decoder.is_open()) {
		return;
	}
	m_playing = true;
}

void VideoStreamPlaybackFFmpeg::_stop() {
	m_playing = false;
	// Rewind, but keep the frame in the buffer: the engine calls play() again
	// when looping, and the rewound frame is uploaded by the first update.
	_seek_internal(0.0, false);
}

void VideoStreamPlaybackFFmpeg::_seek(double p_time) {
	_seek_internal(p_time, true);
}

double VideoStreamPlaybackFFmpeg::_get_length() const {
	return m_decoder.get_duration();
}

Ref<Texture2D> VideoStreamPlaybackFFmpeg::_get_texture() const {
	return m_texture;
}

double VideoStreamPlaybackFFmpeg::_get_playback_position() const {
	return m_time;
}

int32_t VideoStreamPlaybackFFmpeg::_get_mix_rate() const {
	return 0;
}

int32_t VideoStreamPlaybackFFmpeg::_get_channels() const {
	return 0;
}
