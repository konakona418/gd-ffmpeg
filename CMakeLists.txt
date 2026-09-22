# Using the same minimum as the godot-cpp project
cmake_minimum_required(VERSION 3.17)

# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "gdffmpeg") # "The name of the library"
set(GODOT_PROJECT_DIR "project" CACHE STRING "The directory of a Godot project folder")
set(FFMPEG_PREFIX "" CACHE STRING "Path to a vendored FFmpeg installation (containing include/ and lib/). If empty, pkg-config is used.")

# Make sure all the dependencies are satisfied
find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)

# Ensure godot-cpp submodule has been updated
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/src")
    message(NOTICE "godot-cpp bindings source not found")
    message(NOTICE "initializing/updating the godot-cpp submodule...")

    # update the c++ bindings submodule to populate it with the necessary source for the library
    execute_process(
        COMMAND git submodule update --init godot-cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
add_subdirectory(godot-cpp SYSTEM)

# Create the gen directory at configure time so IDEs can see generated headers immediately after generate_bindings
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp/gen/include")

# Add godot-cpp's module path and include the exported functions.
# This is made available for documentation generation
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

# The godot-cpp target has some of useful properties attached that can be retrieved like so.
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

if(NOT GODOTCPP_PLATFORM STREQUAL "linux")
    message(FATAL_ERROR "gdffmpeg currently only supports Linux (platform='${GODOTCPP_PLATFORM}').")
endif()

# Now we can specify our own project which will inherit any global cmake properties or variables that have been defined.
project(gdffmpeg
    VERSION 1.0
    DESCRIPTION "H.264 video stream for Godot, backed by FFmpeg (video only, Linux)."
    HOMEPAGE_URL "https://github.com/konakona418/gd-ffmpeg"
    LANGUAGES CXX
)

add_library(${LIBNAME} SHARED)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    src/ffmpeg_common.cpp
    src/ffmpeg_common.h
    src/ffmpeg_decoder.cpp
    src/ffmpeg_decoder.h
    src/ffmpeg_loader.cpp
    src/ffmpeg_loader.h
    src/video_stream_ffmpeg.cpp
    src/video_stream_ffmpeg.h
    src/video_stream_playback_ffmpeg.cpp
    src/video_stream_playback_ffmpeg.h
)

# Fetch a list of the xml files to use for documentation and add to our target
file(GLOB_RECURSE DOC_XML LIST_DIRECTORIES NO CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/doc_classes/*.xml")

# conditionally add doc data to compile output
if(DOC_XML)
    if(GODOTCPP_TARGET MATCHES "editor|template_debug")
        target_doc_sources(${LIBNAME} "${DOC_XML}")
    endif()
endif()

# FFmpeg: either a vendored installation or the system pkg-config packages.
if(FFMPEG_PREFIX)
    target_include_directories(${LIBNAME} PRIVATE "${FFMPEG_PREFIX}/include")
    target_link_directories(${LIBNAME} PRIVATE "${FFMPEG_PREFIX}/lib")
    target_link_libraries(${LIBNAME} PRIVATE avformat avcodec avutil swscale)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
    target_link_libraries(${LIBNAME} PRIVATE PkgConfig::FFMPEG)
endif()

target_link_libraries(${LIBNAME} PRIVATE godot-cpp)

# Make the extension find the FFmpeg shared libraries bundled next to it, both in
# the editor and in exported projects (where Godot copies [dependencies] to the
# executable directory).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(${LIBNAME} PRIVATE "-Wl,-rpath,$ORIGIN")
endif()

# Require at least C++17 for this target
set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 17)

set(GODOT_ADDON_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/addons/gdffmpeg/linux")

set_target_properties(${LIBNAME}
    PROPERTIES
    # The generator expression here prevents msvc from adding a Debug or Release subdir.
    RUNTIME_OUTPUT_DIRECTORY "$<1:${GODOT_ADDON_DIR}>"

    PREFIX ""
    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)
