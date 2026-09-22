#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "ffmpeg_common.h"
#include "ffmpeg_loader.h"
#include "video_stream_ffmpeg.h"
#include "video_stream_playback_ffmpeg.h"

using namespace godot;

static Ref<ResourceFormatLoaderFFmpeg> ffmpeg_loader;

void initialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// These are not abstract: godot-cpp creates instances of them directly, and
	// abstract extension classes have no creation function in ClassDB.
	GDREGISTER_CLASS(VideoStreamPlaybackFFmpeg);
	GDREGISTER_CLASS(ResourceFormatLoaderFFmpeg);
	GDREGISTER_CLASS(VideoStreamFFmpeg);

	ffmpeg_install_log_callback();

	ffmpeg_loader.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(ffmpeg_loader);
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ResourceLoader::get_singleton()->remove_resource_format_loader(ffmpeg_loader);
	ffmpeg_loader.unref();
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT gdffmpeg_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_gdextension_types);
	init_obj.register_terminator(uninitialize_gdextension_types);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
