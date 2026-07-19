#pragma once

#include <cstdint>

/* Shared memory IPC between addon DLL and CEF helper process.
   Uses GW2's own bin64/cef runtime — no WebView2 / winetricks.
   Browser is windowless (OSR); frames are BGRA in a second mapping. */

static constexpr uint32_t kWikiIpcMagic = 0x484C4956u; /* 'HLIV' */
static constexpr const char* kWikiIpcMapName = "Local\\GW2InGameHelper_CEF_IPC_v1";
static constexpr const char* kWikiFrameMapName = "Local\\GW2InGameHelper_CEF_FRAME_v1";

static constexpr uint32_t kWikiFrameMaxW = 1920;
static constexpr uint32_t kWikiFrameMaxH = 1200;
static constexpr uint32_t kWikiFrameStride = kWikiFrameMaxW * 4;
static constexpr uint32_t kWikiFrameBytes = kWikiFrameStride * kWikiFrameMaxH;

static constexpr uint32_t kWikiInputQueueSize = 128;

enum WikiIpcCmd : uint32_t
{
	WIKI_CMD_NONE = 0,
	WIKI_CMD_NAVIGATE = 1,
	WIKI_CMD_BACK = 2,
	WIKI_CMD_FORWARD = 3,
	WIKI_CMD_RELOAD = 4,
	WIKI_CMD_HOME = 5,
	WIKI_CMD_QUIT = 6,
	WIKI_CMD_SET_BOUNDS = 7,
	WIKI_CMD_SET_VISIBLE = 8,
};

enum WikiInputType : uint32_t
{
	WIKI_IN_NONE = 0,
	WIKI_IN_MOUSE_CLICK = 1,  /* x,y, button(0L/1M/2R), up(0/1), clicks */
	WIKI_IN_MOUSE_WHEEL = 2,  /* x,y, dx, dy */
	WIKI_IN_KEY = 3,          /* a=cef_key_event_type, b=windows_vk, c=modifiers, character */
	WIKI_IN_FOCUS = 4,        /* a=0/1 */
};

#pragma pack(push, 1)
struct WikiInputEvent
{
	uint32_t type;
	int32_t  x;
	int32_t  y;
	int32_t  a;
	int32_t  b;
	int32_t  c;
	uint32_t character;
};

struct WikiIpcState
{
	uint32_t magic;
	uint32_t ready;
	uint32_t alive;
	uint32_t can_back;
	uint32_t can_forward;
	uint32_t visible;

	uint32_t view_w;
	uint32_t view_h;
	uint32_t frame_w;
	uint32_t frame_h;
	uint32_t frame_seq;

	uint32_t cmd;
	uint32_t cmd_seq;
	uint32_t last_cmd_seq;
	char     url[2048];
	char     status[256];
	char     cmd_arg[2048];

	/* Live mouse (coalesced every frame — no queue loss). */
	int32_t  mouse_x;
	int32_t  mouse_y;
	uint32_t mouse_mods;   /* cef_event_flags bits for buttons/keys */
	uint32_t mouse_leave;  /* 1 when cursor left the view */
	uint32_t mouse_seq;    /* increments when mouse_x/y/mods/leave change */

	/* Discrete events (clicks / wheel / keys) — ring buffer. */
	uint32_t input_write;
	uint32_t input_read;
	WikiInputEvent input_q[kWikiInputQueueSize];
};
#pragma pack(pop)
