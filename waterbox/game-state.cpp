/* game-state.cpp - what the tools see of the game: chimera docs/game-cores.md.
 *
 * Four memory domains:
 *
 *   - "Game State": the core's own block, copied from the engine after every
 *     step. The engine's mode, the stage and its timer: read-only, as the
 *     engine derives them.
 *   - "Players": the engine's two players (playerList), their numbers only -
 *     the engine's own structure also holds two pointers, which are host
 *     addresses and no part of the game. Copied out after every step and back
 *     in before the next, so a poke lands when the next step begins.
 *   - "Objects": the engine's object list (objectEntityList), in place and
 *     writable: every object of the stage, the players' own among them.
 *   - "Global Variables": the game's global variables, in place and writable,
 *     each a property named as the game names it (its GameConfig.bin), so a
 *     watch reads "player.score" or "stage.timeEnabled" whatever the game is.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "RetroEngine.hpp"

#include "rsdk-driver.h"

namespace {

struct __attribute__((packed)) GameState
{
	uint64_t frames;          /* 0 */
	uint8_t game_mode;        /* 8: Engine.gameMode */
	uint8_t stage_mode;       /* 9 */
	uint8_t stage_list;       /* 10: activeStageList */
	uint8_t stage_position;   /* 11: stageListPosition */
	int32_t act;              /* 12 */
	uint32_t stage_time_ms;   /* 16: the stage timer, in milliseconds */
	int32_t frame_counter;    /* 20 */
	uint8_t time_enabled;     /* 24 */
	uint8_t pad[7];
	char stage_folder[64];    /* 32 */
	char stage_name[64];      /* 96 */
};

struct __attribute__((packed)) PlayerState
{
	int32_t entity;           /* 0 */
	int32_t x, y;             /* 4, 8: 16.16 fixed point */
	int32_t x_velocity, y_velocity;   /* 12, 16 */
	int32_t speed;            /* 20 */
	int32_t screen_x, screen_y;       /* 24, 28 */
	int32_t angle;            /* 32 */
	int32_t timer;            /* 36 */
	int32_t look_pos;         /* 40 */
	int32_t values[8];        /* 44 */
	uint8_t collision_mode, skidding, pushing, collision_plane;   /* 76 */
	int8_t control_mode;      /* 80 */
	uint8_t control_lock;     /* 81 */
	uint8_t visible, tile_collisions, object_interactions;       /* 82 */
	uint8_t left, right, up, down, jump_press, jump_hold;         /* 85 */
	uint8_t follow_player1, track_scroll, gravity, water;         /* 91 */
	uint8_t pad;              /* 95 */
	int32_t top_speed, acceleration, deceleration, air_acceleration, air_deceleration;  /* 96 */
	int32_t gravity_strength, jump_strength, jump_cap, rolling_acceleration, rolling_deceleration; /* 116 */
};
static_assert(sizeof(PlayerState) == 136, "PlayerState layout");

GameState gs;
PlayerState ps[PLAYER_COUNT];

#define COPY_PLAYER(X)                                                                                            \
	X(entity, entityNo) X(x, XPos) X(y, YPos) X(x_velocity, XVelocity) X(y_velocity, YVelocity) X(speed, speed)    \
	X(screen_x, screenXPos) X(screen_y, screenYPos) X(angle, angle) X(timer, timer) X(look_pos, lookPos)          \
	X(collision_mode, collisionMode) X(skidding, skidding) X(pushing, pushing) X(collision_plane, collisionPlane) \
	X(control_mode, controlMode) X(control_lock, controlLock) X(visible, visible)                                \
	X(tile_collisions, tileCollisions) X(object_interactions, objectInteractions) X(left, left) X(right, right)   \
	X(up, up) X(down, down) X(jump_press, jumpPress) X(jump_hold, jumpHold) X(follow_player1, followPlayer1)      \
	X(track_scroll, trackScroll) X(gravity, gravity) X(water, water) X(top_speed, topSpeed)                      \
	X(acceleration, acceleration) X(deceleration, deceleration) X(air_acceleration, airAcceleration)              \
	X(air_deceleration, airDeceleration) X(gravity_strength, gravityStrength) X(jump_strength, jumpStrength)       \
	X(jump_cap, jumpCap) X(rolling_acceleration, rollingAcceleration) X(rolling_deceleration, rollingDeceleration)

} // namespace

extern "C" {

void gamestate_from_game(void)
{
	gs.frames = rsdkdrv_frames();
	gs.game_mode = (uint8_t)Engine.gameMode;
	gs.stage_mode = (uint8_t)stageMode;
	gs.stage_list = (uint8_t)activeStageList;
	gs.stage_position = (uint8_t)stageListPosition;
	gs.act = actID;
	gs.stage_time_ms = (uint32_t)(stageMinutes * 60000 + stageSeconds * 1000 + stageMilliseconds * 10);
	gs.frame_counter = frameCounter;
	gs.time_enabled = timeEnabled ? 1 : 0;
	memset(gs.stage_folder, 0, sizeof gs.stage_folder);
	memset(gs.stage_name, 0, sizeof gs.stage_name);
	if (activeStageList >= 0 && activeStageList < STAGELIST_MAX && stageListPosition >= 0 && stageListPosition < 0x100)
	{
		const SceneInfo &si = stageList[activeStageList][stageListPosition];
		snprintf(gs.stage_folder, sizeof gs.stage_folder, "%s", si.folder);
		snprintf(gs.stage_name, sizeof gs.stage_name, "%s", si.name);
	}
	for (int p = 0; p < PLAYER_COUNT; p++)
	{
		const Player &pl = playerList[p];
		PlayerState &o = ps[p];
#define X(a, b) o.a = (decltype(o.a))pl.b;
		COPY_PLAYER(X)
#undef X
		for (int i = 0; i < 8; i++) o.values[i] = pl.values[i];
	}
}

/* the players' block back into the engine before a step: what a poke or a
 * freeze wrote there */
void gamestate_to_game(void)
{
	for (int p = 0; p < PLAYER_COUNT; p++)
	{
		Player &pl = playerList[p];
		const PlayerState &o = ps[p];
#define X(a, b) pl.b = (decltype(pl.b))o.a;
		COPY_PLAYER(X)
#undef X
		for (int i = 0; i < 8; i++) pl.values[i] = o.values[i];
	}
}

int rsdkdrv_domain_count(void) { return 4; }

const char *rsdkdrv_domain_name(int i)
{
	static const char *const names[] = { "Game State", "Players", "Objects", "Global Variables" };
	return i >= 0 && i < 4 ? names[i] : "";
}

uint8_t *rsdkdrv_domain_ptr(int i)
{
	switch (i)
	{
	case 0: return (uint8_t *)&gs;
	case 1: return (uint8_t *)ps;
	case 2: return (uint8_t *)objectEntityList;
	case 3: return (uint8_t *)globalVariables;
	}
	return NULL;
}

int64_t rsdkdrv_domain_size(int i)
{
	switch (i)
	{
	case 0: return (int64_t)sizeof gs;
	case 1: return (int64_t)sizeof ps;
	case 2: return (int64_t)sizeof objectEntityList;
	case 3: return (int64_t)sizeof globalVariables;
	}
	return 0;
}

int rsdkdrv_domain_writable(int i) { return i >= 1 && i <= 3; }

/* the table: the block, the players, the objects as an array of structures,
 * and the game's global variables by name. Built once, after Init, when the
 * game's configuration has named its variables. */
const char *rsdkdrv_game_properties(void)
{
	static char json[65536];
	if (json[0]) return json;
	int n = 0;
#define P(...) n += snprintf(json + n, sizeof json - (size_t)n, __VA_ARGS__)
#define GS(name, off, type, extra) \
	P("    { \"name\": \"%s\", \"domain\": \"Game State\", \"offset\": %d, \"type\": \"%s\", \"writable\": false%s },\n", name, off, type, extra)
	P("{\n  \"gameTimer\": \"Stage.Time Ms\",\n  \"properties\": [\n");
	GS("Engine.Frames", 0, "u64", ", \"group\": \"Engine\"");
	GS("Engine.Mode", 8, "u8", ", \"group\": \"Engine\", \"values\": { \"0\": \"Developer Menu\", \"1\": \"Game\", \"2\": \"Developer Menu Start\", "
	   "\"3\": \"Exit\", \"4\": \"Script Error\", \"5\": \"High Resolution On\", \"6\": \"High Resolution Off\", \"7\": \"Pause\", \"8\": \"Wait\", \"9\": \"Video\" }");
	GS("Stage.Mode", 9, "u8", ", \"group\": \"Stage\", \"values\": { \"0\": \"Load\", \"1\": \"Normal\", \"2\": \"Paused\" }");
	GS("Stage.List", 10, "u8", ", \"group\": \"Stage\", \"values\": { \"0\": \"Presentation\", \"1\": \"Regular\", \"2\": \"Bonus\", \"3\": \"Special\" }");
	GS("Stage.Position", 11, "u8", ", \"group\": \"Stage\"");
	GS("Stage.Act", 12, "s32", ", \"group\": \"Stage\"");
	GS("Stage.Time Ms", 16, "u32", ", \"group\": \"Stage\", \"description\": \"The stage timer (minutes, seconds and hundredths), in milliseconds\"");
	GS("Stage.Frame Counter", 20, "s32", ", \"group\": \"Stage\"");
	GS("Stage.Time Enabled", 24, "bool", ", \"group\": \"Stage\"");
	P("    { \"name\": \"Stage.Folder\", \"domain\": \"Game State\", \"offset\": 32, \"type\": \"string\", \"length\": 64, \"writable\": false, \"group\": \"Stage\" },\n");
	P("    { \"name\": \"Stage.Name\", \"domain\": \"Game State\", \"offset\": 96, \"type\": \"string\", \"length\": 64, \"writable\": false, \"group\": \"Stage\" },\n");

	static const struct { const char *name; int off; const char *type; const char *desc; } k_player[] = {
		{ "Entity", 0, "s32", "The object the player is" },
		{ "X", 4, "s32", "Position, 16.16 fixed point" },
		{ "Y", 8, "s32", "Position, 16.16 fixed point" },
		{ "X Velocity", 12, "s32", "" },
		{ "Y Velocity", 16, "s32", "" },
		{ "Speed", 20, "s32", "Ground speed" },
		{ "Screen X", 24, "s32", "" },
		{ "Screen Y", 28, "s32", "" },
		{ "Angle", 32, "s32", "" },
		{ "Timer", 36, "s32", "" },
		{ "Look Pos", 40, "s32", "" },
		{ "Collision Mode", 76, "u8", "" },
		{ "Skidding", 77, "u8", "" },
		{ "Pushing", 78, "u8", "" },
		{ "Collision Plane", 79, "u8", "" },
		{ "Control Mode", 80, "s8", "" },
		{ "Control Lock", 81, "u8", "" },
		{ "Visible", 82, "u8", "" },
		{ "Tile Collisions", 83, "u8", "" },
		{ "Object Interactions", 84, "u8", "" },
		{ "Left", 85, "u8", "The player's controls this frame" },
		{ "Right", 86, "u8", "" },
		{ "Up", 87, "u8", "" },
		{ "Down", 88, "u8", "" },
		{ "Jump Press", 89, "u8", "" },
		{ "Jump Hold", 90, "u8", "" },
		{ "Follow Player 1", 91, "u8", "" },
		{ "Track Scroll", 92, "u8", "" },
		{ "Gravity", 93, "u8", "In the air" },
		{ "Water", 94, "u8", "Under water" },
		{ "Top Speed", 96, "s32", "" },
		{ "Acceleration", 100, "s32", "" },
		{ "Deceleration", 104, "s32", "" },
		{ "Air Acceleration", 108, "s32", "" },
		{ "Air Deceleration", 112, "s32", "" },
		{ "Gravity Strength", 116, "s32", "" },
		{ "Jump Strength", 120, "s32", "" },
		{ "Jump Cap", 124, "s32", "" },
		{ "Rolling Acceleration", 128, "s32", "" },
		{ "Rolling Deceleration", 132, "s32", "" },
	};
	for (int p = 0; p < PLAYER_COUNT; p++)
	{
		for (size_t i = 0; i < sizeof k_player / sizeof k_player[0]; i++)
		{
			P("    { \"name\": \"Player %d.%s\", \"domain\": \"Players\", \"offset\": %d, \"type\": \"%s\", \"group\": \"Player %d\"",
			  p + 1, k_player[i].name, p * (int)sizeof(PlayerState) + k_player[i].off, k_player[i].type, p + 1);
			if (k_player[i].desc[0]) P(", \"description\": \"%s\"", k_player[i].desc);
			P(" },\n");
		}
		P("    { \"name\": \"Player %d.Values\", \"domain\": \"Players\", \"offset\": %d, \"type\": \"s32\", \"count\": 8, \"group\": \"Player %d\" },\n",
		  p + 1, p * (int)sizeof(PlayerState) + 44, p + 1);
	}

	static const struct { const char *name; size_t off; const char *type; int count; } k_entity[] = {
		{ "X", offsetof(Entity, XPos), "s32", 1 },
		{ "Y", offsetof(Entity, YPos), "s32", 1 },
		{ "Values", offsetof(Entity, values), "s32", 8 },
		{ "Scale", offsetof(Entity, scale), "s32", 1 },
		{ "Rotation", offsetof(Entity, rotation), "s32", 1 },
		{ "Animation Timer", offsetof(Entity, animationTimer), "s32", 1 },
		{ "Animation Speed", offsetof(Entity, animationSpeed), "s32", 1 },
		{ "Type", offsetof(Entity, type), "u8", 1 },
		{ "Property Value", offsetof(Entity, propertyValue), "u8", 1 },
		{ "State", offsetof(Entity, state), "u8", 1 },
		{ "Priority", offsetof(Entity, priority), "u8", 1 },
		{ "Draw Order", offsetof(Entity, drawOrder), "u8", 1 },
		{ "Direction", offsetof(Entity, direction), "u8", 1 },
		{ "Ink Effect", offsetof(Entity, inkEffect), "u8", 1 },
		{ "Alpha", offsetof(Entity, alpha), "u8", 1 },
		{ "Animation", offsetof(Entity, animation), "u8", 1 },
		{ "Previous Animation", offsetof(Entity, prevAnimation), "u8", 1 },
		{ "Frame", offsetof(Entity, frame), "u8", 1 },
	};
	for (size_t i = 0; i < sizeof k_entity / sizeof k_entity[0]; i++)
	{
		if (k_entity[i].count == 1)
			P("    { \"name\": \"Object.%s\", \"domain\": \"Objects\", \"offset\": %d, \"type\": \"%s\", \"count\": %d, \"stride\": %d, \"group\": \"Objects\" },\n",
			  k_entity[i].name, (int)k_entity[i].off, k_entity[i].type, ENTITY_COUNT, (int)sizeof(Entity));
		else
			for (int v = 0; v < k_entity[i].count; v++)
				P("    { \"name\": \"Object.%s %d\", \"domain\": \"Objects\", \"offset\": %d, \"type\": \"%s\", \"count\": %d, \"stride\": %d, \"group\": \"Objects\" },\n",
				  k_entity[i].name, v, (int)k_entity[i].off + 4 * v, k_entity[i].type, ENTITY_COUNT, (int)sizeof(Entity));
	}

	/* the game's own names; one the JSON could not carry as it is, or a
	 * second of the same name, is left out */
	for (int v = 0; v < globalVariablesCount && v < GLOBALVAR_COUNT; v++)
	{
		const char *name = globalVariableNames[v];
		int ok = name[0] != 0;
		for (const char *c = name; *c && ok; c++) ok = *c >= 0x20 && *c < 0x7F && *c != '"' && *c != '\\';
		for (int w = 0; w < v && ok; w++) ok = strcmp(globalVariableNames[w], name) != 0;
		if (!ok) continue;
		P("    { \"name\": \"%s\", \"domain\": \"Global Variables\", \"offset\": %d, \"type\": \"s32\", \"group\": \"Global Variables\" },\n", name, v * 4);
	}
	P("    { \"name\": \"Global\", \"domain\": \"Global Variables\", \"offset\": 0, \"type\": \"s32\", \"count\": %d, \"group\": \"Global Variables\", "
	  "\"description\": \"The game's global variables, by number\" }\n", GLOBALVAR_COUNT);
	P("  ]\n}\n");
#undef GS
#undef P
	return json;
}

} // extern "C"
