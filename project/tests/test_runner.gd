extends Node

# Integration tests for the gdffmpeg VideoStream implementation.
# Run with:
#   godot --headless --path project --fixed-fps 60 res://tests/test_runner.tscn
# Exits with code 1 if any check fails.

const ASSETS := "res://tests/assets/"

var _checks := 0
var _failures := 0


func _ready() -> void:
	await _run_all()
	print("gdffmpeg tests: %d checks, %d failures" % [_checks, _failures])
	get_tree().quit(0 if _failures == 0 else 1)


func _check(condition: bool, message: String) -> void:
	_checks += 1
	if condition:
		print("  ok   %s" % message)
	else:
		_failures += 1
		printerr("  FAIL %s" % message)


func _check_approx(value: float, expected: float, tolerance: float, message: String) -> void:
	_check(absf(value - expected) <= tolerance, "%s (got %.3f, expected %.3f +/- %.3f)" % [message, value, expected, tolerance])


func _wait_frames(count: int) -> void:
	for i in count:
		await get_tree().process_frame


func _make_player(stream: VideoStream) -> VideoStreamPlayer:
	var player := VideoStreamPlayer.new()
	player.autoplay = false
	player.stream = stream
	add_child(player)
	return player


func _run_all() -> void:
	await _test_loader()
	await _test_playback_and_finish()
	await _test_seek()
	await _test_pause()
	await _test_loop()
	await _test_rejections()
	await _test_truncated()
	await _test_raw_h264()
	await _test_bframes()


func _test_loader() -> void:
	print("loader")
	var stream: VideoStream = load(ASSETS + "cfr.mp4")
	_check(stream != null, "cfr.mp4 loads")
	_check(stream is VideoStreamFFmpeg, "cfr.mp4 is a VideoStreamFFmpeg resource")
	if stream != null:
		_check(stream.get_file().ends_with("cfr.mp4"), "file property points at cfr.mp4")
	_check(ResourceLoader.exists(ASSETS + "cfr.mp4"), "ResourceLoader.exists() sees cfr.mp4")
	_check(ResourceLoader.get_recognized_extensions_for_type("VideoStream").has("mp4"), "ResourceLoader recognizes .mp4 for VideoStream")


func _test_playback_and_finish() -> void:
	print("playback / end of stream")
	var stream: VideoStream = load(ASSETS + "cfr.mp4")
	var player := _make_player(stream)
	var saw_finished := [false]
	player.finished.connect(func() -> void: saw_finished[0] = true)

	_check(not player.is_playing(), "not playing before play()")
	player.play()
	_check(player.is_playing(), "play() starts playback")

	await _wait_frames(10)
	_check(player.get_video_texture() != null, "get_video_texture() returns a texture")
	_check_approx(player.get_stream_length(), 2.0, 0.3, "stream length is ~2s")

	var position_a: float = player.get_stream_position()
	await _wait_frames(20)
	var position_b: float = player.get_stream_position()
	_check(position_b > position_a, "stream position advances (%.3f -> %.3f)" % [position_a, position_b])

	var deadline := Time.get_ticks_msec() + 15000
	while not saw_finished[0] and Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
	_check(saw_finished[0], "finished signal is emitted at the end of the stream")
	_check(not player.is_playing(), "playback stops at the end of the stream")
	player.queue_free()
	await _wait_frames(1)


func _test_seek() -> void:
	print("seek")
	var stream: VideoStream = load(ASSETS + "cfr.mp4")
	var player := _make_player(stream)
	player.play()
	await _wait_frames(5)

	player.set_stream_position(1.0)
	await _wait_frames(2)
	_check_approx(player.get_stream_position(), 1.0, 0.25, "seek to 1.0s lands near the target")
	_check(player.is_playing(), "still playing after seeking")
	_check(player.get_video_texture() != null, "texture is still valid after seeking")

	player.set_paused(true)
	player.set_stream_position(0.5)
	await _wait_frames(2)
	_check_approx(player.get_stream_position(), 0.5, 0.25, "seek while paused lands near the target")
	_check(player.is_paused(), "still paused after seeking")

	player.set_paused(false)
	await _wait_frames(5)
	_check(player.get_stream_position() > 0.5, "playback continues after unpausing")
	player.queue_free()
	await _wait_frames(1)


func _test_pause() -> void:
	print("pause")
	var stream: VideoStream = load(ASSETS + "cfr.mp4")
	var player := _make_player(stream)
	player.play()
	await _wait_frames(10)

	player.set_paused(true)
	var position_a: float = player.get_stream_position()
	await _wait_frames(10)
	var position_b: float = player.get_stream_position()
	_check(position_a == position_b, "position is frozen while paused")
	_check(player.is_playing(), "is_playing() stays true while paused")

	player.set_paused(false)
	await _wait_frames(5)
	_check(player.get_stream_position() > position_b, "position resumes after unpausing")
	player.queue_free()
	await _wait_frames(1)


func _test_loop() -> void:
	print("loop")
	var stream: VideoStream = load(ASSETS + "cfr.mp4")
	var player := _make_player(stream)
	player.loop = true
	player.play()

	var wrapped := false
	var previous: float = player.get_stream_position()
	for i in 900:
		await get_tree().process_frame
		var position: float = player.get_stream_position()
		if position < previous:
			wrapped = true
			break
		previous = position
	_check(wrapped, "looping wraps the stream position back")
	_check(player.is_playing(), "playback continues after looping")
	player.queue_free()
	await _wait_frames(1)


func _test_rejections() -> void:
	print("rejections")
	_check(load(ASSETS + "hevc.mp4") == null, "HEVC video is rejected")
	_check(load(ASSETS + "audio_only.mp4") == null, "audio-only file is rejected")
	_check(load(ASSETS + "does_not_exist.mp4") == null, "missing file fails cleanly")
	_check(ResourceLoader.get_recognized_extensions_for_type("VideoStream").has("mkv"), "ResourceLoader recognizes .mkv for VideoStream")


func _test_truncated() -> void:
	print("truncated file")
	var stream: VideoStream = load(ASSETS + "truncated.mp4")
	_check(stream != null, "truncated file still loads (moov box is present)")
	if stream == null:
		return
	var player := _make_player(stream)
	var saw_finished := [false]
	player.finished.connect(func() -> void: saw_finished[0] = true)
	player.play()
	var deadline := Time.get_ticks_msec() + 10000
	while not saw_finished[0] and Time.get_ticks_msec() < deadline:
		await get_tree().process_frame
	_check(saw_finished[0], "truncated file ends gracefully")
	player.queue_free()
	await _wait_frames(1)


func _test_raw_h264() -> void:
	print("raw .h264")
	var stream: VideoStream = load(ASSETS + "raw.h264")
	_check(stream != null, "raw .h264 loads")
	if stream == null:
		return
	var player := _make_player(stream)
	player.play()
	await _wait_frames(10)
	_check(player.is_playing(), "raw .h264 plays")
	_check(player.get_video_texture() != null, "raw .h264 has a texture")
	player.queue_free()
	await _wait_frames(1)


func _test_bframes() -> void:
	print("B-frames / VFR / 10-bit")
	for file_name in ["bframes.mp4", "vfr.mp4", "h264_10bit.mp4"]:
		var path: String = ASSETS + file_name
		if not ResourceLoader.exists(path):
			print("  skip %s (not generated)" % file_name)
			continue
		var stream: VideoStream = load(path)
		_check(stream != null, "%s loads" % file_name)
		if stream == null:
			continue
		var player := _make_player(stream)
		player.play()
		await _wait_frames(10)
		var position_a: float = player.get_stream_position()
		await _wait_frames(10)
		var position_b: float = player.get_stream_position()
		_check(player.get_video_texture() != null, "%s has a texture" % file_name)
		_check(position_b > position_a, "%s advances" % file_name)
		player.queue_free()
		await _wait_frames(1)
