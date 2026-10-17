/* rsdk-driver.cpp - the Retro Engine v3 decompilation as a machine that is
 * stepped.
 *
 * The engine's own main loop (RetroEngine::Run) reads events, runs the game
 * for a frame, draws it, and waits for the next sixtieth of a second. The
 * core runs the same pass without the wait (RunFrame, patches/0001), once a
 * step:
 *
 *   - A step is one frame of the engine: one sixtieth of a second of the
 *     machine's time (the engine's refresh rate, which the core fixes at 60).
 *     The machine's clock is the number of frames run, in microseconds
 *     (seams.c answers SDL's counters from it).
 *   - The sound is the engine's own mixer, run once a frame for exactly 735
 *     stereo samples, the frame's share of 44100 a second - by the driver,
 *     in place of the sound device's thread.
 *   - The controls are the engine's keyboard: the buttons hold the keys
 *     settings.ini maps to the engine's eight inputs (rsdk-files.c), and the
 *     engine reads SDL's keyboard state at the start of the frame.
 *   - The picture is the engine's software screen, read after the frame as
 *     upstream's SDL frontend reads it (Drawing.cpp FlipScreen): 16-bit
 *     colour, as wide as the screen setting, and twice the size each way on a
 *     frame the engine draws at high resolution.
 *
 * The engine is upstream's, built from source with one patch, its own
 * software renderer, mixer and script interpreter, and no OpenGL, no mod
 * loader and no videos (docs/PLAN.md).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "RetroEngine.hpp"

#include "rsdk-driver.h"

namespace {

struct Driver
{
	struct rsdk_settings settings;
	int init_done;

	uint8_t held[RSDK_BTN_COUNT];
	uint8_t keys[SDL_NUM_SCANCODES];

	uint64_t frames;
	int render;
	int read;

	SDL_AudioCallback audio_cb;
	void *audio_user;
	int16_t audio[RSDK_AUDIO_SAMPLES * 2];
	int audio_frames;

	uint32_t video[RSDK_VIDEO_MAX_WIDTH * RSDK_VIDEO_MAX_HEIGHT];
	int video_w, video_h;
};

Driver g;

/* the scancode settings.ini maps to each button (rsdk-files.c write_ini) */
const int k_scancodes[RSDK_BTN_COUNT] = {
	SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
	SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_RETURN,
};

inline uint32_t rgb565(uint16_t c)
{
	const uint32_t r = (c >> 11) & 31, gr = (c >> 5) & 63, b = c & 31;
	return 0xFF000000u | ((r << 3 | r >> 2) << 16) | ((gr << 2 | gr >> 4) << 8) | (b << 3 | b >> 2);
}

/* the engine's screen, as FlipScreen hands it to SDL (its size kept
 * whether drawn or not): the screen buffer at
 * its width, or, on a frame drawn at high resolution, the upper part of the
 * screen doubled and the lower part from the double-size buffer */
void read_screen(int render)
{
	const int w = SCREEN_XSIZE, h = SCREEN_YSIZE;
	const uint16_t *fb = Engine.frameBuffer;
	if (!fb) return;
	if (drawStageGFXHQ && Engine.frameBuffer2x)
	{
		const int W = w * 2;
		g.video_w = W;
		g.video_h = h * 2;
		if (!render) return;
		uint32_t *out = g.video;
		for (int y = 0; y < h / 2 + 12; y++, out += W * 2)
			for (int x = 0; x < w; x++)
			{
				const uint32_t c = rgb565(fb[y * GFX_LINESIZE + x]);
				out[2 * x] = out[2 * x + 1] = out[W + 2 * x] = out[W + 2 * x + 1] = c;
			}
		const uint16_t *fb2 = Engine.frameBuffer2x;
		for (int y = 0; y < (h / 2 - 12) * 2; y++, out += W)
			for (int x = 0; x < W; x++)
				out[x] = rgb565(fb2[y * GFX_LINESIZE_DOUBLE + x]);
		return;
	}
	g.video_w = w;
	g.video_h = h;
	if (!render) return;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			g.video[y * w + x] = rgb565(fb[y * GFX_LINESIZE + x]);
}

} // namespace

/* ------------------------------------------------------------ seams.c */

extern "C" uint64_t rsdkdrv_clock_us(void) { return g.frames * 1000000ull / RSDK_FRAME_RATE; }
extern "C" long rsdkdrv_random_seed(void) { return g.settings.random_seed; }
extern "C" const uint8_t *rsdkdrv_keyboard_state(void) { return g.keys; }
extern "C" void rsdkdrv_audio_open(SDL_AudioCallback cb, void *user)
{
	g.audio_cb = cb;
	g.audio_user = user;
}

/* ------------------------------------------------------------ the exports */

extern "C" {

void gamestate_from_game(void);

int rsdkdrv_init(char *err, int errsize)
{
	for (int i = 0; i < RSDK_VIDEO_MAX_WIDTH * RSDK_VIDEO_MAX_HEIGHT; i++) g.video[i] = 0xFF000000u;
	rsdk_settings_read(&g.settings);
	if (g.settings.screen_width < 320 || g.settings.screen_width > 424)
	{
		snprintf(err, (size_t)errsize, "the setting screenWidth is %d; it goes from 320 to 424", g.settings.screen_width);
		return 0;
	}
	g.video_w = g.settings.screen_width;
	g.video_h = RSDK_VIDEO_HEIGHT;
	if (!rsdk_files_load(&g.settings, err, errsize)) return 0;

	Engine.Init();
	if (!Engine.running || !Engine.initialised)
	{
		snprintf(err, (size_t)errsize, "the engine did not start: %s",
		         Engine.usingDataFile_Config ? "the data file has no Data/Game/GameConfig.bin it can read, or the screen could not be made"
		                                     : "the data file is not a Retro Engine v3 data file");
		return 0;
	}
	if (!g.audio_cb)
	{
		snprintf(err, (size_t)errsize, "the engine did not open its sound device");
		return 0;
	}
	g.render = 1;
	g.init_done = 1;
	gamestate_from_game();
	return 1;
}

void rsdkdrv_set_button(int index, int down)
{
	if (index >= 0 && index < RSDK_BTN_COUNT) g.held[index] = down ? 1 : 0;
}

void gamestate_to_game(void);

void rsdkdrv_frame(int render)
{
	g.render = render;
	g.audio_frames = 0;
	if (!Engine.running) return;
	for (int i = 0; i < RSDK_BTN_COUNT; i++) g.keys[k_scancodes[i]] = g.held[i];
	gamestate_to_game();
	/* a frame reads the controls unless it is the one in which the engine
	 * loads a stage (ProcessStage's STAGEMODE_LOAD: files, scripts and the
	 * objects' startup, and no object runs) */
	g.read = !(Engine.gameMode == ENGINE_MAINGAME && stageMode == STAGEMODE_LOAD);
	Engine.RunFrame();
	g.frames++;
	memset(g.audio, 0, sizeof g.audio);
	g.audio_cb(g.audio_user, (Uint8 *)g.audio, (int)sizeof g.audio);
	g.audio_frames = RSDK_AUDIO_SAMPLES;
	read_screen(render);
	gamestate_from_game();
}

const uint32_t *rsdkdrv_video(int *w, int *h)
{
	*w = g.video_w;
	*h = g.video_h;
	return g.video;
}

const int16_t *rsdkdrv_audio(int *samples)
{
	*samples = g.audio_frames;
	return g.audio;
}

/* whether the frame just run read the controls: the engine reads them at
 * the start of every frame (ProcessInput) and hands them to the game's
 * scripts, except in the frame it loads a stage, whose scripts run no
 * object; a stopped engine reads nothing */
int rsdkdrv_input_was_read(void) { return Engine.running ? g.read : 0; }

uint64_t rsdkdrv_frames(void) { return g.frames; }

} // extern "C"
