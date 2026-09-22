#!/usr/bin/env python
import os
import subprocess
import sys

from methods import print_error


libname = "gdffmpeg"
projectdir = "project"
addondir = "project/addons/gdffmpeg"

localEnv = Environment(tools=["default"], PLATFORM="")

# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Modify the example file as needed and uncomment the line below or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Add("ffmpeg_prefix", "Path to a vendored FFmpeg installation (containing include/ and lib/). If empty, pkg-config is used instead.", "")

opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"api_version": "4.5", "env": env, "customs": customs})

if env["platform"] != "linux":
    print_error("gdffmpeg currently only supports Linux (platform='{}').".format(env["platform"]))
    sys.exit(1)

env.Append(CPPPATH=["src/"])

ffmpeg_prefix = ARGUMENTS.get("ffmpeg_prefix", env.get("ffmpeg_prefix", ""))
if ffmpeg_prefix:
    ffmpeg_prefix = os.path.abspath(ffmpeg_prefix)
    env.Append(CPPPATH=[os.path.join(ffmpeg_prefix, "include")])
    env.Append(LIBPATH=[os.path.join(ffmpeg_prefix, "lib")])
    env.Append(LIBS=["avformat", "avcodec", "avutil", "swscale"])
else:
    pkg_config_check = subprocess.run(
        ["pkg-config", "--exists", "libavformat", "libavcodec", "libavutil", "libswscale"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if pkg_config_check.returncode != 0:
        print_error("""FFmpeg development libraries were not found.
Install the development packages (libavformat-dev, libavcodec-dev, libavutil-dev, libswscale-dev)
or pass ffmpeg_prefix=/path/to/ffmpeg (containing include/ and lib/).""")
        sys.exit(1)
    env.ParseConfig("pkg-config --cflags --libs libavformat libavcodec libavutil libswscale")

sources = Glob("src/*.cpp")

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# .dev doesn't inhibit compatibility, so we don't need to key it.
# .universal just means "compatible with all relevant arches" so we don't need to key it.
suffix = env['suffix'].replace(".dev", "").replace(".universal", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

copy = env.Install("{}/linux/".format(addondir), library)

default_args = [library, copy]
Default(*default_args)
