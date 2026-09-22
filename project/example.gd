extends VideoStreamPlayer

# Minimal usage example: load an H.264 file and play it back.
# The bundled loader claims mp4/m4v/mov/h264/264/mkv, so load() returns a
# VideoStreamFFmpeg just like the built-in loader returns VideoStreamTheora for .ogv.

const VIDEO_PATH := "res://tests/assets/cfr.mp4"


func _ready() -> void:
	var video: VideoStream = load(VIDEO_PATH)
	if video == null:
		push_error("Could not load %s" % VIDEO_PATH)
		return
	stream = video
	loop = true
	play()
