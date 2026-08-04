#pragma once

#include <cstdint>

#include "nexus/Nexus.h"

#define ADDON_NAME "GW2-InGame-Helper"
#define ADDON_SIG  0x48454C50u /* 'HELP' */

struct MumbleContext
{
	unsigned char serverAddress[28];
	uint32_t mapId;
	uint32_t mapType;
	uint32_t shardId;
	uint32_t instance;
	uint32_t buildId;
	uint32_t uiState;
	uint16_t compassWidth;
	uint16_t compassHeight;
	float    compassRotation;
	float    playerX;
	float    playerY;
	float    mapCenterX;
	float    mapCenterY;
	float    mapScale;
	uint32_t processId;
	uint8_t  mountIndex;
};

struct MumbleLinkedMem
{
	uint32_t uiVersion;
	uint32_t uiTick;
	float    fAvatarPosition[3];
	float    fAvatarFront[3];
	float    fAvatarTop[3];
	wchar_t  name[256];
	float    fCameraPosition[3];
	float    fCameraFront[3];
	float    fCameraTop[3];
	wchar_t  identity[256];
	uint32_t context_len;
	unsigned char context[256];
	wchar_t  description[2048];
};

enum class UiStateBits : uint32_t
{
	MapOpen            = 1u << 0,
	CompassTopRight    = 1u << 1,
	CompassRotation    = 1u << 2,
	GameFocus          = 1u << 3,
	Competitive        = 1u << 4,
	TextboxFocus       = 1u << 5,
	InCombat           = 1u << 6,
};

namespace G
{
	extern AddonDefinition_t AddonDef;
	extern AddonAPI_t*       API;
	extern NexusLinkData_t*  NexusLink;
	extern MumbleLinkedMem*  Mumble;
	extern HMODULE           Self;

	extern bool  ShowWiki; /* overlay window visible (name kept for settings compat) */
	extern bool  ShowOptions;
	extern bool  ShowNotes; /* ImGui Notes + clipboard helpers window */
	extern bool  ShowTpWatch; /* ImGui TP watchlist (add/remove + prices) */
	extern bool  ShowLookup; /* ImGui item lookup (chat code / ID / name) — free-floating */
	extern bool  ShowWallet; /* ImGui wallet + mats snapshot — free-floating */
	extern bool  ShowVault; /* ImGui Dailies & Vault — free-floating */
	extern bool  ShowAccount; /* ImGui Account pad (tabbed stash/vault/TP/item) */
	extern bool  ShowEvents; /* ImGui world-boss timers + track list — free-floating */
	extern bool  ShowLogManager; /* ImGui DPS Logs (ArcDPS EVTC browser) */
	extern bool  ShowPathingGuides; /* ImGui Pathing category / credit panel */
	extern bool  ShowPathingTrails; /* master: load packs + draw overlays */
	/* Lady Elyssa Features — map-completion editions are exclusive; Hearts / HP Train are independent. */
	extern bool  LadyBarefoot;   /* foot routes + bfs shortcuts (current map) */
	extern bool  LadyWpOnly;     /* waypoint trails (current map) */
	extern bool  LadyWithMounts; /* mount route + mount-guide markers (current map) */
	extern bool  LadyHearts;     /* heartpath trails (current map) */
	extern bool  LadyHeroPointTrain; /* legs.hp.* hero point train (current map) */
	extern bool  ShowCompassOverlay; /* trails/markers over stock GW2 compass */
	extern bool  ShowWorldTrails; /* in-world GPS breadcrumbs */
	extern bool  ShowDirectionCompass; /* world N/E/S/W around the character */
	extern bool  ShowCompassPad; /* ImGui Compass settings pad (sliders) */
	extern bool  ShowSettings; /* ImGui Settings pad (ex-Nexus Options body) */
	extern float DirectionLetterScale; /* × Nexus FontBig */
	extern float DirectionWorldRadiusScale; /* × hitbox-based radius */
	extern bool  HideWhenMapOpen; /* hide compass/world overlays while map open */
	extern bool  HideOutOfGameplay;
	extern float WorldTrailMaxDist; /* meters from player */
	extern float WorldTrailWidth;
	/* Soft fade / clear size around the player (0 = full path, no hole). */
	extern float WorldTrailPlayerClear;
	extern float WorldMarkerScale; /* × pack iconSize; default 2 = comfortable size */
	extern float Opacity;
	extern float FontScale;
	extern bool  FontScaleAuto; /* true: derive FontScale from display / Nexus Scaling */
	extern float WindowWidth;
	extern float WindowHeight;
	extern float WindowPosX;
	extern float WindowPosY;
	extern bool  HasSavedPos;
	extern bool  HasSavedSize; /* false until WindowWidth/Height loaded or user resizes */
	extern bool  KeepHelperWarm; /* hide helper without killing CEF (uses more RAM) */
	extern char  LastQuery[128];
	extern char  ActiveSiteId[64];
	extern char  DefaultSiteId[64]; /* Home button + landing when no tabs restored */
	extern char  Gw2ApiKey[128]; /* optional account API key — Live panels; local only */
	extern char  TpWatchIds[1024]; /* comma-separated item ids — user TP watchlist */
	extern char  TpWatchAlerts[2048]; /* id:copperThresh,… — sell ≤ alert; 0/absent = off */
	extern char  EventTrackIds[4096]; /* comma-separated event ids — user track list */
	extern char  PathingEnabled[8192]; /* '|' separated category paths — persisted (settings also read legacy TekkitEnabled=) */
	extern char  LogFolder[512]; /* ArcDPS cbtlogs folder */
	extern char  EliteInsightsPath[512]; /* GuildWars2EliteInsights-CLI.exe */
	extern char  DpsReportToken[128]; /* optional dps.report user token */
	extern float LogManagerListFrac; /* log list vs detail splitter (0.20–0.72) */
	extern float LogManagerWinW;
	extern float LogManagerWinH;
	extern float LogManagerWinX; /* <0 = unset */
	extern float LogManagerWinY;
	extern bool  LogManagerGroupByEncounter; /* collapsible encounter sections in log list */
	extern bool  LogManagerAutoParse; /* parse pending with EI after each scan */

	/* Last user placement for floating pads (x < 0 = never placed — use dock fallback). */
	struct PadGeom
	{
		float x = -1.f;
		float y = -1.f;
		float w = 0.f;
		float h = 0.f;
	};
	extern PadGeom PadAccount;
	extern PadGeom PadPathing;
	extern PadGeom PadEvents;
	extern PadGeom PadNotes;
	extern PadGeom PadCompass;
	extern PadGeom PadSettings;
	extern PadGeom PadTp;
	extern PadGeom PadLookup;
	extern PadGeom PadWallet;
	extern PadGeom PadVault;
}
