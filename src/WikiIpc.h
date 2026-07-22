#pragma once

#include <cstdint>

/* Shared memory IPC between addon DLL and CEF helper process.
   Uses GW2's own bin64/cef runtime — no WebView2 / winetricks.
   Browser is windowless (OSR); frames are BGRA in a second mapping.
   Up to kWikiMaxTabs OSR browsers; only the active tab paints.

   v3: url/title seq+len fencing, double-buffered frames + reader lock,
   named wake event for idle helper. */

static constexpr uint32_t kWikiIpcMagic = 0x484C4933u; /* 'HLI3' */
static constexpr const char* kWikiIpcMapName = "Local\\GW2InGameHelper_CEF_IPC_v3";
static constexpr const char* kWikiFrameMapName = "Local\\GW2InGameHelper_CEF_FRAME_v3";
static constexpr const char* kWikiWakeEventName = "Local\\GW2InGameHelper_CEF_WAKE_v3";

static constexpr uint32_t kWikiFrameMaxW = 1920;
static constexpr uint32_t kWikiFrameMaxH = 1200;
static constexpr uint32_t kWikiFrameStride = kWikiFrameMaxW * 4;
static constexpr uint32_t kWikiFrameBytes = kWikiFrameStride * kWikiFrameMaxH;
/* Two flip buffers — helper paints back while DLL may read front. */
static constexpr uint32_t kWikiFrameBufferCount = 2;
static constexpr uint32_t kWikiFrameMapBytes = kWikiFrameBytes * kWikiFrameBufferCount;

static constexpr uint32_t kWikiInputQueueSize = 128;
static constexpr uint32_t kWikiCmdQueueSize = 32;
static constexpr int kWikiMaxTabs = 8;

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
	WIKI_CMD_CREATE_TAB = 9,   /* a=slot, arg=url */
	WIKI_CMD_ACTIVATE_TAB = 10,/* a=slot */
	WIKI_CMD_CLOSE_TAB = 11,   /* a=slot */
	WIKI_CMD_FIND = 12,        /* arg=text; a bit0=forward bit1=matchCase bit2=findNext */
	WIKI_CMD_STOP_FIND = 13,   /* a=clearSelection */
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

struct WikiCmdEvent
{
	uint32_t cmd;
	int32_t  a;
	char     arg[1536];
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
	/* Index of the completed buffer the DLL should read (0 or 1). */
	uint32_t frame_front;
	/* Set to frame_front while PresentFrame copies; 0xFFFFFFFF when idle.
	   Helper skips paints that would overwrite the buffer being read. */
	uint32_t frame_reading;

	/* Legacy single-slot (debug mirror / ring-full fallback for non-tab cmds). */
	uint32_t cmd;
	uint32_t cmd_seq;
	uint32_t last_cmd_seq;
	char     url[2048];
	uint32_t url_len;
	uint32_t url_seq; /* odd = writer in progress; even = stable */
	char     status[256];
	char     cmd_arg[2048];
	int32_t  cmd_a;

	int32_t  active_tab;
	uint32_t tab_mask; /* bit i set if slot i has a browser */

	uint32_t find_count;
	uint32_t find_ordinal;
	char     title[128]; /* active tab page title (UTF-8) */
	uint32_t title_len;
	uint32_t title_seq; /* odd = writer in progress; even = stable */

	/* Live mouse (coalesced every frame — no queue loss). */
	int32_t  mouse_x;
	int32_t  mouse_y;
	uint32_t mouse_mods;
	uint32_t mouse_leave;
	uint32_t mouse_seq;

	/* Discrete events (clicks / wheel / keys) — ring buffer. */
	uint32_t input_write;
	uint32_t input_read;
	WikiInputEvent input_q[kWikiInputQueueSize];

	/* Commands — ring buffer (avoids CREATE/NAVIGATE stomps). */
	uint32_t cmd_write;
	uint32_t cmd_read;
	WikiCmdEvent cmd_q[kWikiCmdQueueSize];
};
#pragma pack(pop)
