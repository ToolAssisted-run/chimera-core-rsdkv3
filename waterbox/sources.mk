# sources.mk - what native.mk and guest.mk both build: the same files with the
# same defines, so the native reference and the sandboxed core are the same
# program. Included, not run.

ROOT   := ..
RSDK   := $(ROOT)/extern/RSDKv3
SDL    := $(ROOT)/extern/SDL
OGG    := $(ROOT)/extern/ogg
VORBIS := $(ROOT)/extern/vorbis
MB     ?= $(or $(MINIBOX_DIR),$(HOME)/chimera/extern/chimera-common-minibox)
INC_DIR := $(ROOT)/build/include

# ---- SDL 2 (the submodule extern/SDL, tag release-2.32.10), built the way
# its own Makefile.minimal builds it: the platform-neutral configuration
# (include/SDL_config_minimal.h - dummy video and audio drivers, no threads,
# no timers, no joysticks, no dynamic loading) and the portable C of every
# subsystem the engine touches. On top of that, as the SDLPoP core builds it:
#   SDL_CPUINFO_DISABLED  SDL asks the CPU nothing, so every blitter and
#                         audio converter is the C one on every host
#   DYNAPI_NEEDS_DLOPEN   (without HAVE_DLOPEN) turns off SDL's dynamic API,
#                         which would otherwise let an environment variable
#                         load another SDL into the process
#   HAVE_MALLOC, HAVE_STDIO_H
#                         SDL allocates and opens files with the C library,
#                         like the engine (so its opens go through rsdk-files.c)
SDL_DIRS := src src/atomic src/audio src/audio/dummy src/cpuinfo src/events src/file \
	src/haptic src/haptic/dummy src/hidapi src/joystick src/joystick/dummy src/loadso/dummy \
	src/power src/filesystem/dummy src/locale src/locale/dummy src/misc src/misc/dummy \
	src/render src/render/software src/sensor src/sensor/dummy src/stdlib src/libm \
	src/thread src/thread/generic src/timer src/timer/dummy src/video src/video/yuv2rgb \
	src/video/dummy
SDL_WARN := -w -Wno-error=incompatible-pointer-types
SDL_SRCS := $(sort $(foreach d,$(SDL_DIRS),$(wildcard $(SDL)/$(d)/*.c)))
SDL_DEFS := -DSDL_CPUINFO_DISABLED=1 -DDYNAPI_NEEDS_DLOPEN=1 -DHAVE_MALLOC=1 -DHAVE_STDLIB_H=1 -DHAVE_STDIO_H=1 -DHAVE_STRING_H=1

# the transcendental functions, which detmath.c answers in both builds: not
# builtins, so the compiler neither folds them nor fuses a sin and a cos
MATH_FUNCS := sin cos tan acos atan atan2 exp log pow sinf cosf tanf atan2f expf logf powf
NO_BUILTIN_MATH := $(foreach f,$(MATH_FUNCS) sincos sincosf,-fno-builtin-$(f))

# ---- libogg (extern/ogg, v1.3.5) and libvorbis (extern/vorbis, v1.3.7): the
# engine's music and sound effects are Ogg Vorbis. The decoder's files only;
# the encoder (vorbisenc), its tools and tests are not built. Their sin, cos,
# exp, log, pow and acos are detmath.c's.
OGG_SRCS := $(OGG)/src/bitwise.c $(OGG)/src/framing.c
VORBIS_NAMES := analysis bitrate block codebook envelope floor0 floor1 info lookup lpc lsp mapping0 mdct \
	psy registry res0 sharedbook smallft synthesis vorbisfile window
VORBIS_SRCS := $(addprefix $(VORBIS)/lib/,$(addsuffix .c,$(VORBIS_NAMES)))
CODEC_INCS := -Icompat -I$(OGG)/include -I$(VORBIS)/include -I$(VORBIS)/lib

# ---- the engine: every translation unit of upstream's CMakeLists.txt but
# main.cpp (the core is the frontend), fcaseopen.c (rsdk-files.c matches
# names without regard to case itself), tinyxml2 (the mod loader is off) and
# theoraplay.c (no videos: theora-stubs.c)
RSDK_NAMES := Animation Audio Collision Debug Drawing Ini Input Math ModAPI Object Palette Player Reader \
	RetroEngine Scene Scene3D Script Sprite String Text Userdata Video
RSDK_SRCS := $(addprefix $(RSDK)/RSDKv3/,$(addsuffix .cpp,$(RSDK_NAMES)))
# As upstream's CMake configures it, with:
#   RETRO_USING_OPENGL=0     the software renderer only (no hardware one)
#   RETRO_USE_MOD_LOADER=0   no mods: the game is the data file's
#   RSDK_AUTOBUILD=1         every DLC flag off, as the licence requires of a
#                            build given to anyone (RETRO_DISABLE_PLUS)
#   RETRO_USE_SDL2=1         SDL 2 (extern/SDL)
RSDK_DEFS := -DRETRO_USING_OPENGL=0 -DRETRO_USE_MOD_LOADER=0 -DRSDK_AUTOBUILD=1 -DRETRO_USE_SDL2=1 \
	-DRETRO_USE_ORIGINAL_CODE=0 '-DRETRO_DEV_EXTRA="Chimera"'
RSDK_CXXFLAGS_COMMON := -std=gnu++17 -O2 $(NO_BUILTIN_MATH) $(RSDK_DEFS) -I$(INC_DIR) -I$(SDL)/include $(CODEC_INCS) \
	-I$(RSDK)/dependencies/all/theoraplay -I$(RSDK)/RSDKv3

# ---- the core
CORE_C_NAMES := rsdk-files seams detmath theora-stubs wbx-entry
CORE_CXX_NAMES := rsdk-driver game-state
CORE_HDRS := rsdk-driver.h $(wildcard compat/*/*.h) $(wildcard $(RSDK)/RSDKv3/*.hpp)
CORE_CFLAGS_COMMON := -std=gnu11 -O2 -I$(INC_DIR) -I$(SDL)/include -I$(RSDK)/dependencies/all/theoraplay
CORE_CXXFLAGS_COMMON := $(RSDK_CXXFLAGS_COMMON)

# ---- the calls the core answers itself
WRAPS := fopen fclose SDL_RWFromFile time rand srand SDL_GetPerformanceCounter SDL_GetPerformanceFrequency SDL_GetTicks \
	SDL_GetTicks64 SDL_Delay SDL_Init SDL_InitSubSystem SDL_OpenAudioDevice SDL_OpenAudio SDL_PauseAudioDevice \
	SDL_PauseAudio SDL_CloseAudioDevice SDL_CloseAudio SDL_LockAudio SDL_UnlockAudio SDL_LockAudioDevice \
	SDL_UnlockAudioDevice SDL_GetKeyboardState SDL_UpdateTexture SDL_RenderClear SDL_RenderCopy \
	SDL_RenderFillRect SDL_RenderPresent SDL_CreateRenderer $(MATH_FUNCS)
WRAP_FLAGS := $(foreach w,$(WRAPS),-Wl,--wrap=$(w))

# the patch series goes onto the submodule before anything of the engine builds
PATCH_STAMP := $(ROOT)/build/patches.stamp
$(PATCH_STAMP): $(wildcard $(ROOT)/patches/*.patch) apply-patches.sh
	sh apply-patches.sh
	@mkdir -p $(dir $@)
	@touch $@

# The engine includes <SDL2/SDL.h>: a directory (INC_DIR) that has SDL's
# headers under SDL2/
SDL_HDRS := $(wildcard $(SDL)/include/*.h)
$(INC_DIR)/SDL2/.stamp: $(SDL_HDRS)
	@mkdir -p $(INC_DIR)/SDL2
	cp $(SDL_HDRS) $(INC_DIR)/SDL2/
	@touch $@

SDL_CFLAGS_COMMON := -O2 -fno-strict-aliasing $(SDL_DEFS) -I$(SDL)/include
CODEC_CFLAGS_COMMON := -std=gnu11 -O2 -w $(CODEC_INCS) $(NO_BUILTIN_MATH)

# Every object depends on the flags it was built with: a change to them
# rebuilds it.
define flags_stamp
$(shell mkdir -p $(1); printf '%s\n' '$(2)' | cmp -s - $(1)/flags || printf '%s\n' '$(2)' > $(1)/flags)
endef
