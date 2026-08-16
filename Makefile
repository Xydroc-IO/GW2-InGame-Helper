# Cross-compile GW2-InGame-Helper.dll (+ embedded CEF helper) for Windows / Wine
# Private CEF 150 runtime downloads into addons/GW2-InGame-Helper/cef/ on first use.
#
# build/ layout:
#   bin/    shipping DLL + GW2HelperBrowser.exe + gw2igh-watchd
#   embed/  flattened copies for `ld -r -b binary` (stable _binary_<basename>_*)
#   src/    compile objects (mirrors source tree)
#   test/   host / Wine test binaries
#   deps/   vendored C objects
CXX      = x86_64-w64-mingw32-g++
LD       = x86_64-w64-mingw32-ld
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS
CXXFLAGS += -DCEF_API_VERSION=15000
CXXFLAGS += -Isrc -Isrc/app -Isrc/ui -Isrc/ui/browse -Isrc/ui/settings -Isrc/ui/quickaccess \
	-Isrc/ui/chrome -Isrc/api -Isrc/browse -Isrc/browser -Isrc/browse/sites -Isrc/browse/livepanels \
	-Isrc/browse/tabs -Isrc/account -Isrc/account/crafting -Isrc/account/tpwatch -Isrc/account/unlocks \
	-Isrc/account/wallet -Isrc/account/vault -Isrc/account/lookup -Isrc/account/progress -Isrc/pathing \
	-Isrc/pathing/world -Isrc/pathing/lua -Isrc/pathing/packs \
	-Isrc/pathing/trails -Isrc/pathing/waypoints -Isrc/pathing/mapassist \
	-Isrc/logs -Isrc/logs/logmanager -Isrc/logs/eiruntime \
	-Isrc/events -Isrc/notes -Isrc/helper \
	-Isrc/economy -Isrc/instances -Isrc/completion -Isrc/farming -Isrc/overlay -Isrc/watch
CXXFLAGS += -Ideps -Ideps/imgui -Ideps/cef -Ideps/miniz -Ideps/qrcodegen -Ideps/lua
# Dependency files: emit only from the build/%.o rule via -MF (never beside sources).
# Helper prefers msvcrt over UCRT so Wine CreateProcess doesn't fail on api-ms-win-crt-*.dll
CXXFLAGS_EXE = $(CXXFLAGS) -mcrtdll=msvcrt
LDFLAGS_DLL  = -shared -static -static-libgcc -static-libstdc++ -Wl,--image-base,0x180000000
LDFLAGS_EXE  = -static -static-libgcc -static-libstdc++ -mwindows -municode -mcrtdll=msvcrt
LIBS_DLL = -ldxgi -ld3d11 -lgdi32 -luser32 -lole32 -luuid -lshell32 -lwinhttp -lcrypt32 -lbcrypt -lcomdlg32 -ladvapi32 -lws2_32 -lwindowsapp -lruntimeobject
LIBS_EXE = -lgdi32 -lole32 -luuid -lshell32 -lwinhttp

HELPER_SRC = src/helper/main.cpp src/helper/HelperState.cpp src/helper/HelperPaths.cpp \
	src/helper/HelperResolve.cpp src/helper/HelperBrowseActions.cpp \
	src/helper/HelperTabs.cpp \
	src/helper/HelperHandlers.cpp src/helper/HelperCommands.cpp \
	src/helper/HelperNavPolicy.cpp src/helper/HelperNavPolicyHandlers.cpp \
	src/helper/HelperOsrRender.cpp \
	src/helper/CssCompat.cpp src/helper/CssCompatYoutube.cpp \
	src/helper/CssCompatLegacy.cpp src/helper/CssCompatLegacyRewrite.cpp \
	src/helper/CssProxy.cpp
HELPER_OUT = build/bin/GW2HelperBrowser.exe
EMBED_DIR = build/embed
TEST_DIR  = build/test
HELPER_BLOB_SRC = $(EMBED_DIR)/helper_blob.exe
HELPER_BLOB_OBJ = $(EMBED_DIR)/helper_blob.o
HOME_LOGO_SRC  = $(EMBED_DIR)/home_logo.png
HOME_COVER_SRC = $(EMBED_DIR)/home_cover.jpg
HOME_LOGO_OBJ  = $(EMBED_DIR)/home_logo.o
HOME_COVER_OBJ = $(EMBED_DIR)/home_cover.o
SITES_JSON_SRC = $(EMBED_DIR)/sites.json
SITES_JSON_OBJ = $(EMBED_DIR)/sites_json.o
LEGENDARIES_CATALOG_SRC = $(EMBED_DIR)/legendaries_catalog.json
LEGENDARIES_CATALOG_OBJ = $(EMBED_DIR)/legendaries_catalog_json.o
CHEATSHEETS_ZIP_SRC = $(EMBED_DIR)/cheatsheets.zip
CHEATSHEETS_ZIP_OBJ = $(EMBED_DIR)/cheatsheets_zip.o
UI_CHROME_ZIP_SRC = $(EMBED_DIR)/ui_chrome.zip
UI_CHROME_ZIP_OBJ = $(EMBED_DIR)/ui_chrome_zip.o

DLL_SRC = \
	src/entry.cpp \
	src/entryHotkeys.cpp \
	src/entryWndProc.cpp \
	src/entryLoad.cpp \
	src/entryUnload.cpp \
	src/app/Settings.cpp \
	src/app/WinePadOpen.cpp \
	src/app/CrashTrail.cpp \
	src/app/CrashTrailFiles.cpp \
	src/app/CrashTrailSnapshot.cpp \
	src/app/CrashTrailStack.cpp \
	src/app/SettingsSave.cpp \
	src/app/AddonPaths.cpp \
	src/app/UserTheme.cpp \
	src/app/MumbleIdentity.cpp \
	src/app/AspectLayout.cpp \
	src/app/PanelBinds.cpp \
	src/app/PanelBindsUi.cpp \
	src/app/Gw2Icons.cpp \
	src/app/Gw2Ui.cpp \
	src/app/Gw2UiPadChrome.cpp \
	src/app/Gw2UiPadTitle.cpp \
	src/app/Gw2UiPadScroll.cpp \
	src/app/UiChrome.cpp \
	src/api/Gw2Http.cpp \
	src/api/ApiBudget.cpp \
	src/api/BgFetch.cpp \
	src/app/GameLive.cpp \
	src/pathing/mapassist/MapAssist.cpp \
	src/browse/sites/Sites.cpp \
	src/browse/sites/SitesState.cpp \
	src/browse/sites/SitesUrlMatch.cpp \
	src/browse/sites/SitesFavorites.cpp \
	src/browse/sites/SitesFavoritesStore.cpp \
	src/browse/sites/SitesLoad.cpp \
	src/browse/sites/SitesLoadParse.cpp \
	src/browse/tabs/BrowserTabs.cpp \
	src/browse/tabs/BrowserTabsState.cpp \
	src/browse/tabs/BrowserTabsNav.cpp \
	src/browse/HomePage.cpp \
	src/browse/HomePageHtml.cpp \
	src/browse/RaidFood.cpp \
	src/browse/RaidFoodHtml.cpp \
	src/browse/CheatSheets.cpp \
	src/browse/livepanels/LivePanels.cpp \
	src/browse/livepanels/LivePanelsAsync.cpp \
	src/browse/livepanels/LivePanelsAsyncFs.cpp \
	src/browse/livepanels/LivePanelsAsyncTpWatch.cpp \
	src/browse/livepanels/LivePanelsCraftPlan.cpp \
	src/browse/livepanels/LivePanelsBuildCommon.cpp \
	src/browse/livepanels/LivePanelsBuildJson.cpp \
	src/browse/livepanels/LivePanelsBuildPage.cpp \
	src/browse/livepanels/LivePanelsBuildDailies.cpp \
	src/browse/livepanels/LivePanelsBuildNews.cpp \
	src/browse/livepanels/LivePanelsBuildFashion.cpp \
	src/browse/livepanels/LivePanelsBuildProgress.cpp \
	src/browse/livepanels/LivePanelsBuildProgressArmory.cpp \
	src/browse/livepanels/LivePanelsBuildLegendaryLedger.cpp \
	src/browse/livepanels/LivePanelsBuildLegendaryIcons.cpp \
	src/browse/livepanels/LivePanelsBuildLegendaryDetail.cpp \
	src/browse/livepanels/LivePanelsBuildCheatSheetsHub.cpp \
	src/browse/livepanels/LivePanelsBuildBrowseHub.cpp \
	src/browse/livepanels/LivePanelsBuildBrowseHubAssets.cpp \
	src/browse/livepanels/LivePanelsBuildBrowseCategory.cpp \
	src/browse/livepanels/LivePanelsBuildApiCheck.cpp \
	src/browse/livepanels/LivePanels_Html.cpp \
	src/notes/NotesPad.cpp \
	src/notes/NotesPadWaypoints.cpp \
	src/watch/WatchPad.cpp \
	src/watch/WatchPadAbout.cpp \
	src/watch/WatchPadControls.cpp \
	src/watch/WatchPadControlsUi.cpp \
	src/watch/WatchCapture.cpp \
	src/watch/WatchCaptureTick.cpp \
	src/watch/WatchCaptureGpu.cpp \
	src/watch/WatchCaptureWin.cpp \
	src/watch/WatchCaptureWgc.cpp \
	src/watch/WatchCaptureWgcSession.cpp \
	src/watch/WatchLinux.cpp \
	src/watch/WatchLinuxSession.cpp \
	src/watch/WatchLinuxDaemon.cpp \
	src/watch/WatchLinuxShm.cpp \
	src/pathing/waypoints/WaypointsData.cpp \
	src/pathing/waypoints/WaypointsDataParse.cpp \
	src/pathing/waypoints/RoutingSuggest.cpp \
	src/pathing/waypoints/ConfirmedWaypoints.cpp \
	src/account/CharacterProfiles.cpp \
	src/account/unlocks/UnlocksData.cpp \
	src/account/unlocks/UnlocksDataLoad.cpp \
	src/account/unlocks/UnlocksPad.cpp \
	src/account/InventoryData.cpp \
	src/account/SessionHistoryData.cpp \
	src/account/tpwatch/TpWatchPad.cpp \
	src/account/tpwatch/TpWatchPadUi.cpp \
	src/account/tpwatch/TpWatchData.cpp \
	src/account/tpwatch/TpWatchResolve.cpp \
	src/account/tpwatch/TpWatchFetch.cpp \
	src/account/tpwatch/TpWatchOrdersUi.cpp \
	src/account/lookup/LookupPad.cpp \
	src/account/lookup/LookupFetch.cpp \
	src/account/wallet/WalletPad.cpp \
	src/account/wallet/WalletPadStash.cpp \
	src/account/wallet/WalletFetch.cpp \
	src/account/wallet/WalletFetchSlots.cpp \
	src/account/wallet/WalletFetchAcc.cpp \
	src/account/vault/VaultPad.cpp \
	src/account/vault/VaultData.cpp \
	src/account/vault/VaultFetch.cpp \
	src/account/AccountPad.cpp \
	src/account/progress/ProgressData.cpp \
	src/account/progress/ProgressDataUi.cpp \
	src/account/progress/ProgressFetch.cpp \
	src/account/crafting/CraftingData.cpp \
	src/account/crafting/CraftingPad.cpp \
	src/account/crafting/CraftingApi.cpp \
	src/account/crafting/CraftingApiRecipe.cpp \
	src/account/crafting/CraftingWikiIds.cpp \
	src/account/crafting/CraftingWikiFetch.cpp \
	src/account/crafting/CraftingWikiAcquire.cpp \
	src/account/crafting/CraftingCurated.cpp \
	src/account/crafting/CraftingPlan.cpp \
	src/account/crafting/CraftingPlanBuild.cpp \
	src/account/crafting/CraftingPlanResolve.cpp \
	src/account/crafting/CraftingPlanSnapshot.cpp \
	src/account/crafting/CraftingDailies.cpp \
	src/account/crafting/CraftingKnown.cpp \
	src/account/crafting/CraftingKnownDetails.cpp \
	src/account/crafting/CraftingKnownUi.cpp \
	src/account/crafting/CraftingBrowser.cpp \
	src/account/crafting/CraftingLevelPaths.cpp \
	src/account/crafting/CraftingCart.cpp \
	src/account/crafting/CraftingCartPlan.cpp \
	src/account/crafting/CraftingCartUi.cpp \
	src/account/crafting/CraftingOpts.cpp \
	src/account/crafting/CraftingPlanDecide.cpp \
	src/account/crafting/CraftingResults.cpp \
	src/events/EventsPad.cpp \
	src/events/EventsPadState.cpp \
	src/events/EventsData.cpp \
	src/events/EventsTiming.cpp \
	src/economy/EconomyPad.cpp \
	src/economy/EconomyPadUi.cpp \
	src/economy/EconomyPadState.cpp \
	src/economy/EconomyFetch.cpp \
	src/economy/EconomyFetchFlip.cpp \
	src/economy/EconomyFetchCart.cpp \
	src/economy/EconomyChartPoll.cpp \
	src/economy/EconomyPadFlips.cpp \
	src/economy/CommercePrices.cpp \
	src/economy/CommerceListings.cpp \
	src/economy/CommerceTransactions.cpp \
	src/economy/CommerceExchange.cpp \
	src/economy/CommerceOwned.cpp \
	src/instances/InstancesPad.cpp \
	src/instances/InstancesPadState.cpp \
	src/instances/InstancesData.cpp \
	src/instances/InstancesFetch.cpp \
	src/completion/CompletionPad.cpp \
	src/completion/CompletionPadState.cpp \
	src/completion/CompletionGlobals.cpp \
	src/completion/CompletionPins.cpp \
	src/completion/CompletionApFetch.cpp \
	src/completion/CompletionAchParse.cpp \
	src/completion/CompletionAchCatalog.cpp \
	src/completion/CompletionAchievements.cpp \
	src/completion/CompletionAchDetail.cpp \
	src/completion/CompletionAchWiki.cpp \
	src/farming/FarmingPad.cpp \
	src/farming/FarmingPadRuns.cpp \
	src/farming/FarmingPadFish.cpp \
	src/farming/FarmingPadState.cpp \
	src/farming/FarmingPersist.cpp \
	src/farming/FarmingCatalog.cpp \
	src/farming/FarmingSchedule.cpp \
	src/farming/FarmingNodes.cpp \
	src/overlay/GpsArrow.cpp \
	src/overlay/ZoneBanner.cpp \
	src/overlay/EventAlert.cpp \
	src/logs/logmanager/LogManagerPad.cpp \
	src/logs/logmanager/LogManagerPadState.cpp \
	src/logs/logmanager/LogManagerParse.cpp \
	src/logs/logmanager/LogManagerParsePlayers.cpp \
	src/logs/logmanager/LogManagerUpload.cpp \
	src/logs/logmanager/LogManagerUploadWorkers.cpp \
	src/logs/logmanager/LogManagerEi.cpp \
	src/logs/logmanager/LogManagerCache.cpp \
	src/logs/logmanager/LogManagerKillProof.cpp \
	src/logs/logmanager/LogManagerScan.cpp \
	src/logs/logmanager/LogManagerStats.cpp \
	src/logs/logmanager/LogManagerUi.cpp \
	src/logs/logmanager/LogManagerUiDetail.cpp \
	src/logs/logmanager/LogManagerUiTabs.cpp \
	src/logs/eiruntime/EiRuntime.cpp \
	src/logs/eiruntime/EiRuntimeFs.cpp \
	src/logs/eiruntime/EiRuntimeHttp.cpp \
	src/pathing/waypoints/PathingGuidesPad.cpp \
	src/pathing/packs/PathingSchedule.cpp \
	src/pathing/lua/PathingLua.cpp \
	src/pathing/lua/PathingLuaApi.cpp \
	src/pathing/lua/PathingLuaTypes.cpp \
	src/pathing/lua/PathingLuaMarker.cpp \
	src/pathing/lua/PathingLuaWorld.cpp \
	src/pathing/lua/PathingLuaPack.cpp \
	src/pathing/lua/PathingLuaMumble.cpp \
	src/pathing/lua/PathingLuaMenu.cpp \
	src/pathing/lua/PathingLuaCdn.cpp \
	src/pathing/lua/PathingLuaTrail.cpp \
	src/pathing/lua/PathingLuaLoad.cpp \
	src/pathing/lua/PathingLuaStorage.cpp \
	src/pathing/lua/PathingLuaInstance.cpp \
	src/pathing/trails/PathingTrails.cpp \
	src/pathing/trails/PathingTrailsQuery.cpp \
	src/pathing/trails/PathingTrailsBehaviors.cpp \
	src/pathing/trails/PathingTrailsMarkers.cpp \
	src/pathing/trails/PathingTrailsGuide.cpp \
	src/pathing/packs/PathingLoad.cpp \
	src/pathing/packs/PathingLoadLady.cpp \
	src/pathing/packs/PathingLoadHttp.cpp \
	src/pathing/packs/PathingLoadGuide.cpp \
	src/pathing/packs/PathingPathfind.cpp \
	src/pathing/packs/PathingLoadIcons.cpp \
	src/pathing/trails/PathingTrailsGpsSnippets.cpp \
	src/pathing/trails/PathingTrailsGpsNearby.cpp \
	src/pathing/trails/PathingTrailsPresets.cpp \
	src/pathing/trails/PathingTrailsPresetsMc.cpp \
	src/pathing/trails/PathingTrailsUi.cpp \
	src/pathing/packs/PathingIndex.cpp \
	src/pathing/packs/PathingIndexDiscover.cpp \
	src/pathing/packs/PathingParse.cpp \
	src/pathing/packs/PathingParseXml.cpp \
	src/pathing/packs/PathingParseZip.cpp \
	src/pathing/packs/PathingPacks.cpp \
	src/pathing/packs/PathingPacksHttp.cpp \
	src/pathing/packs/PathingFeatures.cpp \
	src/pathing/world/MarkerBehaviors.cpp \
	src/pathing/world/MarkerBehaviorsState.cpp \
	src/pathing/world/CompassOverlay.cpp \
	src/pathing/world/WorldGpsMath.cpp \
	src/pathing/world/WorldGpsD3dDevice.cpp \
	src/pathing/world/WorldGpsD3dDraw.cpp \
	src/pathing/world/WorldGpsImgui.cpp \
	src/pathing/world/WorldOverlay.cpp \
	src/pathing/world/DirectionCompass.cpp \
	src/pathing/world/DirectionCompassPad.cpp \
	src/ui/quickaccess/HelperQuickAccess.cpp \
	src/browser/WikiBrowser.cpp \
	src/browser/WikiBrowserApi.cpp \
	src/browser/WikiBrowserHelper.cpp \
	src/browser/WikiBrowserHelperMigrate.cpp \
	src/browser/WikiBrowserHelperLifecycle.cpp \
	src/browser/WikiBrowserHelperLaunch.cpp \
	src/browser/WikiBrowserIpc.cpp \
	src/browser/WikiBrowserIpcSec.cpp \
	src/browser/WikiBrowserPresent.cpp \
	src/browser/CefRuntime.cpp \
	src/browser/CefRuntimeFs.cpp \
	src/browser/CefRuntimeVerify.cpp \
	src/browser/CefRuntimeHttp.cpp \
	src/ui/chrome/UI.cpp \
	src/ui/chrome/UI_Helpers.cpp \
	src/ui/chrome/UI_ChromeTabs.cpp \
	src/ui/chrome/UI_ChromeToolbar.cpp \
	src/ui/chrome/UI_ChromeSideRail.cpp \
	src/ui/chrome/UI_ChromeSideRailLayout.cpp \
	src/ui/chrome/UI_Render.cpp \
	src/ui/chrome/UI_RenderPage.cpp \
	src/ui/browse/UI_Browse.cpp \
	src/ui/browse/UI_BrowseHelpers.cpp \
	src/ui/browse/UI_BrowsePanel.cpp \
	src/ui/browse/UI_BrowsePanelFavorites.cpp \
	src/ui/browse/UI_BrowsePanelSites.cpp \
	src/ui/chrome/UI_Options.cpp \
	src/ui/settings/SettingsPad.cpp \
	src/ui/settings/SettingsPadBody.cpp \
	deps/imgui/imgui.cpp \
	deps/imgui/imgui_draw.cpp \
	deps/imgui/imgui_tables.cpp \
	deps/imgui/imgui_widgets.cpp \
	deps/miniz/miniz.c \
	deps/miniz/miniz_tdef.c \
	deps/miniz/miniz_tinfl.c \
	deps/miniz/miniz_zip.c \
	deps/lua/lapi.c \
	deps/lua/lauxlib.c \
	deps/lua/lbaselib.c \
	deps/lua/lcode.c \
	deps/lua/lcorolib.c \
	deps/lua/lctype.c \
	deps/lua/ldblib.c \
	deps/lua/ldebug.c \
	deps/lua/ldo.c \
	deps/lua/ldump.c \
	deps/lua/lfunc.c \
	deps/lua/lgc.c \
	deps/lua/linit.c \
	deps/lua/liolib.c \
	deps/lua/llex.c \
	deps/lua/lmathlib.c \
	deps/lua/lmem.c \
	deps/lua/loadlib.c \
	deps/lua/lobject.c \
	deps/lua/lopcodes.c \
	deps/lua/loslib.c \
	deps/lua/lparser.c \
	deps/lua/lstate.c \
	deps/lua/lstring.c \
	deps/lua/lstrlib.c \
	deps/lua/ltable.c \
	deps/lua/ltablib.c \
	deps/lua/ltm.c \
	deps/lua/lundump.c \
	deps/lua/lutf8lib.c \
	deps/lua/lvm.c \
	deps/lua/lzio.c

DLL_OBJ = $(patsubst %.cpp,build/%.o,$(filter %.cpp,$(DLL_SRC))) \
	$(patsubst %.c,build/%.o,$(filter %.c,$(DLL_SRC)))
DLL_OUT = build/bin/GW2-InGame-Helper.dll

GW2_ROOT   ?= $(HOME)/.local/share/Steam/steamapps/common/Guild Wars 2
GW2_ADDONS ?= $(GW2_ROOT)/addons
INSTALL_DLL = $(GW2_ADDONS)/GW2-InGame-Helper.dll
INSTALL_DIR = $(GW2_ADDONS)/GW2-InGame-Helper

.PHONY: all clean install install-beta install-reset validate-sites enrich-sites export-cheatsheets pack-cheatsheets pack-ui-chrome test-css test-parse test-ipc ci pack-cef check-stamps watchd

all: $(DLL_OUT)

# Host ELF for Wine Watch (portal/PipeWire). Prefer system g++ — Cursor AppImage PATH breaks cc1plus.
HOST_CXX ?= /usr/bin/g++
WATCHD_OUT = build/bin/gw2igh-watchd
WATCHD_SRC = tools/watchd/watchd_main.cpp tools/watchd/watchd_shm.cpp \
	tools/watchd/watchd_scale.cpp tools/watchd/watchd_portal.cpp
WATCHD_BLOB_SRC = $(EMBED_DIR)/watchd_blob
WATCHD_BLOB_OBJ = $(EMBED_DIR)/watchd_blob.o
WATCHD_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3 gio-2.0 gio-unix-2.0 2>/dev/null)
WATCHD_LIBS := $(shell pkg-config --libs libpipewire-0.3 gio-2.0 gio-unix-2.0 2>/dev/null)

watchd: $(WATCHD_OUT)
$(WATCHD_OUT): $(WATCHD_SRC) tools/watchd/watchd_internal.h src/watch/WatchProto.h
	@mkdir -p build/bin
	env -i PATH=/usr/bin:/bin HOME="$(HOME)" PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig \
		$(HOST_CXX) -std=c++17 -O2 -Wall -Wextra -pthread $(WATCHD_CFLAGS) -o $@ $(WATCHD_SRC) $(WATCHD_LIBS)
	@echo "Built $@ (portal/PipeWire watchd for Wine Watch)"

$(WATCHD_BLOB_SRC): $(WATCHD_OUT)
	@mkdir -p $(EMBED_DIR)
	/bin/cp -f $(WATCHD_OUT) $(WATCHD_BLOB_SRC)

$(WATCHD_BLOB_OBJ): $(WATCHD_BLOB_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded watchd blob $@"

SITES_JSON   = data/sites.json
CHEATSHEETS_DIR = data/cheatsheets

validate-sites:
	python3 tools/validate_sites.py $(SITES_JSON)

export-cheatsheets:
	python3 tools/export_cheatsheets.py

pack-cheatsheets: $(CHEATSHEETS_ZIP_SRC)

pack-ui-chrome: $(UI_CHROME_ZIP_SRC)

# Re-stamp browsePath / browseSections from hierarchy rules (dev tool).
enrich-sites:
	python3 tools/enrich_sites_browse.py $(SITES_JSON)
	python3 tools/validate_sites.py $(SITES_JSON)

test-css:
	python3 tools/test_css_downlevel.py

# Host (Linux) parse golden tests — no Wine / GW2 required.
TEST_PARSE_BIN = $(TEST_DIR)/test_logmanager_parse
TEST_IPC_BIN = $(TEST_DIR)/test_wiki_ipc
TEST_JSON_VIEW_BIN = $(TEST_DIR)/test_json_view

test-parse: $(TEST_PARSE_BIN)
	./$(TEST_PARSE_BIN) tools/fixtures/ei_players_sample.json tools/fixtures/dpsreport_players_sample.json
	python3 tools/test_trl_parse.py

TEST_PATHING_LUA_BIN = $(TEST_DIR)/test_pathing_lua.exe
.PHONY: test-pathing-lua
test-pathing-lua: $(TEST_PATHING_LUA_BIN)
	wine $(TEST_PATHING_LUA_BIN)

LUA_TEST_CPP = \
	tools/test_pathing_lua.cpp \
	src/pathing/lua/PathingLua.cpp \
	src/pathing/lua/PathingLuaApi.cpp \
	src/pathing/lua/PathingLuaTypes.cpp \
	src/pathing/lua/PathingLuaMarker.cpp \
	src/pathing/lua/PathingLuaWorld.cpp \
	src/pathing/lua/PathingLuaPack.cpp \
	src/pathing/lua/PathingLuaMumble.cpp \
	src/pathing/lua/PathingLuaCdn.cpp \
	src/pathing/lua/PathingLuaTrail.cpp

LUA_TEST_C = \
	deps/lua/lapi.c deps/lua/lauxlib.c deps/lua/lbaselib.c deps/lua/lcode.c \
	deps/lua/lcorolib.c deps/lua/lctype.c deps/lua/ldblib.c deps/lua/ldebug.c \
	deps/lua/ldo.c deps/lua/ldump.c deps/lua/lfunc.c deps/lua/lgc.c deps/lua/linit.c \
	deps/lua/liolib.c deps/lua/llex.c deps/lua/lmathlib.c deps/lua/lmem.c \
	deps/lua/loadlib.c deps/lua/lobject.c deps/lua/lopcodes.c deps/lua/loslib.c \
	deps/lua/lparser.c deps/lua/lstate.c deps/lua/lstring.c deps/lua/lstrlib.c \
	deps/lua/ltable.c deps/lua/ltablib.c deps/lua/ltm.c deps/lua/lundump.c \
	deps/lua/lutf8lib.c deps/lua/lvm.c deps/lua/lzio.c

LUA_TEST_COBJ = $(patsubst deps/lua/%.c,$(TEST_DIR)/lua/%.o,$(LUA_TEST_C))

$(TEST_DIR)/lua/%.o: deps/lua/%.c
	@mkdir -p $(TEST_DIR)/lua
	x86_64-w64-mingw32-gcc -std=c11 -O2 -Ideps/lua -c -o $@ $<

$(TEST_PATHING_LUA_BIN): $(LUA_TEST_CPP) $(LUA_TEST_COBJ) \
	src/pathing/lua/PathingLua.h src/pathing/lua/PathingLuaInternal.h
	@mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -o $@ \
		$(LUA_TEST_CPP) $(LUA_TEST_COBJ) \
		-lole32 -luuid -lshell32 -lcrypt32 -lwinhttp

test-ipc: $(TEST_IPC_BIN)
	./$(TEST_IPC_BIN)

test-json-view: $(TEST_JSON_VIEW_BIN)
	./$(TEST_JSON_VIEW_BIN)

$(TEST_PARSE_BIN): tools/test_logmanager_parse.cpp src/logs/logmanager/LogManagerParse.cpp src/logs/logmanager/LogManagerParsePlayers.cpp src/logs/logmanager/LogManagerParse.h
	@mkdir -p $(TEST_DIR)
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/logs -Isrc/logs/logmanager -o $@ tools/test_logmanager_parse.cpp src/logs/logmanager/LogManagerParse.cpp src/logs/logmanager/LogManagerParsePlayers.cpp

$(TEST_IPC_BIN): tools/test_wiki_ipc.cpp src/browser/WikiIpc.h
	@mkdir -p $(TEST_DIR)
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/browser -o $@ tools/test_wiki_ipc.cpp

$(TEST_JSON_VIEW_BIN): tools/test_json_view.cpp src/api/JsonView.h
	@mkdir -p $(TEST_DIR)
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -o $@ tools/test_json_view.cpp

# Local continuous integration. Also used by .githooks/pre-push and GitHub Actions.
check-stamps:
	python3 tools/check_stamps.py

ci:
	@bash tools/ci.sh

pack-cef:
	bash scripts/pack-cef-runtime.sh

$(HELPER_OUT): $(HELPER_SRC) src/browser/WikiIpc.h src/helper/HelperInternal.h \
	src/helper/CssCompat.h src/helper/CssCompatInternal.h src/helper/CssProxy.h src/helper/BootJs.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_EXE) $(LDFLAGS_EXE) -o $@ $(HELPER_SRC) $(LIBS_EXE)
	@echo "Built $@"

$(HELPER_BLOB_SRC): $(HELPER_OUT)
	@mkdir -p $(EMBED_DIR)
	/bin/cp -f $(HELPER_OUT) $(HELPER_BLOB_SRC)

$(HELPER_BLOB_OBJ): $(HELPER_BLOB_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded helper blob $@"

$(HOME_LOGO_SRC): docs/media/home-logo.png
	@mkdir -p $(EMBED_DIR)
	/bin/cp -f $< $@

$(HOME_COVER_SRC): docs/media/home-cover.jpg
	@mkdir -p $(EMBED_DIR)
	/bin/cp -f $< $@

$(HOME_LOGO_OBJ): $(HOME_LOGO_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded home logo $@"

$(HOME_COVER_OBJ): $(HOME_COVER_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded home cover $@"

$(SITES_JSON_SRC): $(SITES_JSON)
	@mkdir -p $(EMBED_DIR)
	/bin/cp -f $< $@

$(SITES_JSON_OBJ): $(SITES_JSON_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded sites catalog $@"

$(LEGENDARIES_CATALOG_SRC): data/legendaries/catalog.min.json
	@mkdir -p $(EMBED_DIR)
	/bin/cp -f $< $@

$(LEGENDARIES_CATALOG_OBJ): $(LEGENDARIES_CATALOG_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded legendaries catalog $@"

$(CHEATSHEETS_ZIP_SRC): $(CHEATSHEETS_DIR)/manifest.json $(CHEATSHEETS_DIR)/shared.css $(wildcard $(CHEATSHEETS_DIR)/*.html)
	python3 tools/pack_cheatsheets.py

$(CHEATSHEETS_ZIP_OBJ): $(CHEATSHEETS_ZIP_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded cheatsheets pack $@"

UI_CHROME_DIR = data/ui-chrome

$(UI_CHROME_ZIP_SRC): $(UI_CHROME_DIR)/manifest.txt $(wildcard $(UI_CHROME_DIR)/*.png)
	python3 tools/pack_ui_chrome.py

$(UI_CHROME_ZIP_OBJ): $(UI_CHROME_ZIP_SRC)
	cd $(EMBED_DIR) && $(LD) -r -b binary -o $(notdir $@) $(notdir $<)
	@echo "Embedded UI chrome pack $@"

$(DLL_OUT): $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(SITES_JSON_OBJ) $(LEGENDARIES_CATALOG_OBJ) $(CHEATSHEETS_ZIP_OBJ) $(UI_CHROME_ZIP_OBJ) $(WATCHD_BLOB_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS_DLL) -o $@ $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(SITES_JSON_OBJ) $(LEGENDARIES_CATALOG_OBJ) $(CHEATSHEETS_ZIP_OBJ) $(UI_CHROME_ZIP_OBJ) $(WATCHD_BLOB_OBJ) $(LIBS_DLL)
	@echo "Built $@ (CEF helper + homepage + sites + legendaries + cheatsheets + ui-chrome + watchd embedded)"

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ $<

-include $(DLL_OBJ:.o=.d)

build/%.o: %.c
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ideps/miniz -Ideps/lua -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ $<

install: $(DLL_OUT)
	@mkdir -p "$(INSTALL_DIR)" "$(INSTALL_DIR)/pathing"
	/bin/cp -f "$(DLL_OUT)" "$(INSTALL_DLL)"
	# Nexus loads *-Beta.dll before *.dll (hyphen sorts first). Same signature =
	# Beta wins and the fresh shipping DLL is ignored as a duplicate. On master
	# installs, remove leftover Beta instead of overwriting it — use
	# `make install-beta` only when you intentionally want the Beta channel.
	@if [ -f "$(GW2_ADDONS)/GW2-InGame-Helper-Beta.dll" ]; then \
		/bin/rm -f "$(GW2_ADDONS)/GW2-InGame-Helper-Beta.dll"; \
		echo "Removed GW2-InGame-Helper-Beta.dll (was shadowing shipping)"; \
	fi
	/bin/cp -f pathing/README.md "$(INSTALL_DIR)/pathing/README.md"
	# Curated Tekkit is tw_ALL_IN_ONE.taco (PathingPacks download). Never seed
	# the old "Tekkit's All-In-One.taco" alias — loading both doubles every GPS route.
	/bin/rm -f "$(INSTALL_DIR)/pathing/Tekkit's All-In-One.taco"
	/bin/rm -f "$(INSTALL_DIR)/GW2-InGame-Helper.dll" \
		"$(INSTALL_DIR)/GW2HelperBrowser.exe" \
		"$(GW2_ADDONS)/GW2HelperBrowser.exe" \
		"$(GW2_ROOT)/bin64/cef/GW2HelperBrowser.exe"
	# Clear cached offline pages so version bumps rewrite on next open.
	# Wipe *.ver too — leaving helper-home.ver with no .html made CEF open
	# a restored file:///…/helper-home.html → ERR_FILE_NOT_FOUND.
	# Keep settings.ini, config/ (notes/profiles/etc), and private cef/ —
	# never wipe the CEF tree or user state under config/.
	/bin/rm -f "$(INSTALL_DIR)/"*.html "$(INSTALL_DIR)/"*.ver "$(INSTALL_DIR)/"*.ok \
		"$(INSTALL_DIR)/GW2HelperBrowser.exe.ver" \
		"$(INSTALL_DIR)/home-logo.png" "$(INSTALL_DIR)/home-cover.jpg" \
		"$(INSTALL_DIR)/"*-cmd.txt "$(INSTALL_DIR)/"*.cache "$(INSTALL_DIR)/"live-*.json
	/bin/rm -rf "$(INSTALL_DIR)/pages" "$(INSTALL_DIR)/live" "$(INSTALL_DIR)/cache" \
		"$(INSTALL_DIR)/cmds" "$(INSTALL_DIR)/cef-cache" "$(INSTALL_DIR)/cheatsheets"
	# Seed Immersive chrome so pads look correct even before first extract.
	@mkdir -p "$(INSTALL_DIR)/ui-chrome"
	/bin/cp -f data/ui-chrome/*.png "$(INSTALL_DIR)/ui-chrome/" 2>/dev/null || true
	@stamp=$$(sed -n 's/.*kPackStamp = "\([^"]*\)".*/\1/p' src/app/UiChrome.cpp | head -1); \
		printf '%s' "$$stamp" > "$(INSTALL_DIR)/ui-chrome/ui-chrome.ver"
	@echo "Installed DLL -> $(INSTALL_DLL)"
	@echo "Data folder   -> $(INSTALL_DIR)/ (created; runtime extracts here)"
	@echo "Pathing       -> $(INSTALL_DIR)/pathing/"
	@ls -lh "$(INSTALL_DLL)"
	@ls -lh "$(INSTALL_DIR)/pathing/" 2>/dev/null || true
	@ls -lh "$(INSTALL_DIR)/ui-chrome/" 2>/dev/null || true

# Optional: install shipping build as the Beta DLL too (side-by-side testing).
install-beta: $(DLL_OUT)
	@$(MAKE) install
	/bin/cp -f "$(DLL_OUT)" "$(GW2_ADDONS)/GW2-InGame-Helper-Beta.dll"
	@echo "Also installed GW2-InGame-Helper-Beta.dll (explicit Beta sync)"

install-reset: $(DLL_OUT)
	@$(MAKE) install
	/bin/rm -f "$(INSTALL_DIR)/settings.ini"
	@echo "Also removed settings.ini (tabs/favorites reset on next launch)"

clean:
	rm -rf build
