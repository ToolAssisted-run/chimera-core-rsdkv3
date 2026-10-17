/* rsdk-driver.h - Retro Engine v3 (the RSDKv3 decompilation) as a machine
 * that is stepped.
 *
 * The same driver is compiled for the miniBox guest and for the native
 * reference (run-native); wbx-entry.c puts the guest ABI on top of it.
 *
 * Wire format (waterbox.config "input.buttons", same order): the engine's
 * eight inputs, which every game made with it reads - the pad's four
 * directions, its A, B and C buttons and Start. On a PC keyboard they are
 * the arrows, Z, X, C and Enter, which is what the core presses for them.
 */
#ifndef RSDK_DRIVER_H
#define RSDK_DRIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RsdkButton
{
	RSDK_BTN_UP,
	RSDK_BTN_DOWN,
	RSDK_BTN_LEFT,
	RSDK_BTN_RIGHT,
	RSDK_BTN_A,
	RSDK_BTN_B,
	RSDK_BTN_C,
	RSDK_BTN_START,
	RSDK_BTN_COUNT
};

/* the picture: the engine's screen, 240 lines and as wide as the screenWidth
 * setting (424 by default, as upstream), or twice that each way on the frames
 * the engine draws at twice the size (Sonic CD's special stages, with the
 * hqModes setting); the live size is the last frame's (rsdkdrv_video) */
#define RSDK_VIDEO_HEIGHT 240
#define RSDK_VIDEO_MAX_WIDTH (424 * 2)
#define RSDK_VIDEO_MAX_HEIGHT (240 * 2)

/* stereo, 44100 frames a second (the engine's mixing rate); one step is one
 * sixtieth of a second, 735 frames */
#define RSDK_AUDIO_RATE 44100
#define RSDK_FRAME_RATE 60
#define RSDK_AUDIO_SAMPLES (RSDK_AUDIO_RATE / RSDK_FRAME_RATE)

/* 0 on failure, with the reason in err */
/* the button after the engine's eight: the PC keyboard's Escape, which opens
 * the engine's developer menu when the Developer Menu setting is on (it is
 * live only then) */
#define RSDK_BTN_DEV_MENU RSDK_BTN_COUNT

int rsdkdrv_init(char *err, int errsize);
void rsdkdrv_set_button(int index, int down);
int rsdkdrv_dev_menu(void);   /* the Developer Menu setting */
/* runs the engine through its next frame */
void rsdkdrv_frame(int render);
const uint32_t *rsdkdrv_video(int *w, int *h);
const int16_t *rsdkdrv_audio(int *samples);   /* stereo frames, left then right */
int rsdkdrv_input_was_read(void);
uint64_t rsdkdrv_frames(void);

/* game-state: the property block, the raw domains, the table */
int rsdkdrv_domain_count(void);
const char *rsdkdrv_domain_name(int i);
uint8_t *rsdkdrv_domain_ptr(int i);
int64_t rsdkdrv_domain_size(int i);
int rsdkdrv_domain_writable(int i);
const char *rsdkdrv_game_properties(void);

/* rsdk-files.c: the game's data, the settings file the engine reads, its
 * saves */
struct rsdk_settings
{
	int language;        /* RETRO_EN .. RETRO_JP */
	int screen_width;    /* 320 .. 424 */
	int hq_modes;        /* UseHQModes */
	int original_controls;   /* -1 (the game's own setting), 0, 1 */
	long random_seed;
	int dev_menu;        /* DevMenu */
};
void rsdk_settings_read(struct rsdk_settings *s);
int rsdk_files_load(const struct rsdk_settings *s, char *err, int errsize);

/* The game's saves (SData.bin, SGame.bin) for Emulator > Export Save Data:
 * a snapshot of those that hold anything, then each one's name and bytes. */
int rsdk_saves_count(void);
const char *rsdk_saves_name(int index);
int64_t rsdk_saves_size(int index);
const uint8_t *rsdk_saves_data(int index);

#ifdef __cplusplus
}
#endif

#endif
