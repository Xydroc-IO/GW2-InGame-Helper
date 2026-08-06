# Cross-compile GW2-InGame-Helper.dll (+ embedded CEF helper) for Windows / Wine
# Private CEF 150 runtime downloads into addons/GW2-InGame-Helper/cef/ on first use.
CXX      = x86_64-w64-mingw32-g++
LD       = x86_64-w64-mingw32-ld
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS
CXXFLAGS += -DCEF_API_VERSION=15000
CXXFLAGS += -Isrc -Isrc/app -Isrc/ui -Isrc/ui/browse -Isrc/ui/settings -Isrc/ui/quickaccess \
	-Isrc/ui/chrome -Isrc/api -Isrc/browse -Isrc/browser -Isrc/browse/sites -Isrc/browse/livepanels \
	-Isrc/browse/tabs -Isrc/account -Isrc/account/crafting -Isrc/account/tpwatch -Isrc/account/unlocks \
	-Isrc/account/wallet -Isrc/account/vault -Isrc/account/lookup -Isrc/account/progress -Isrc/pathing \
	-Isrc/pathing/trailtools -Isrc/pathing/world -Isrc/pathing/lua -Isrc/pathing/packs \
	-Isrc/pathing/trails -Isrc/pathing/waypoints -Isrc/logs -Isrc/logs/logmanager -Isrc/logs/eiruntime \
	-Isrc/events -Isrc/notes -Isrc/helper \
	-Isrc/economy -Isrc/instances -Isrc/completion -Isrc/farming -Isrc/overlay
CXXFLAGS += -Ideps -Ideps/imgui -Ideps/cef -Ideps/miniz -Ideps/qrcodegen -Ideps/lua
# Dependency files: emit only from the build/%.o rule via -MF (never beside sources).
# Helper prefers msvcrt over UCRT so Wine CreateProcess doesn't fail on api-ms-win-crt-*.dll
CXXFLAGS_EXE = $(CXXFLAGS) -mcrtdll=msvcrt
LDFLAGS_DLL  = -shared -static -static-libgcc -static-libstdc++
LDFLAGS_EXE  = -static -static-libgcc -static-libstdc++ -mwindows -municode -mcrtdll=msvcrt
LIBS_DLL = -ldxgi -ld3d11 -lgdi32 -lole32 -luuid -lshell32 -lwinhttp -lcrypt32 -lbcrypt -lcomdlg32 -ladvapi32
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
HELPER_BLOB_SRC = build/helper_blob.exe
HELPER_BLOB_OBJ = build/helper_blob.o
HOME_LOGO_SRC  = build/home_logo.png
HOME_COVER_SRC = build/home_cover.jpg
HOME_LOGO_OBJ  = build/home_logo.o
HOME_COVER_OBJ = build/home_cover.o
SITES_JSON_SRC = build/sites.json
SITES_JSON_OBJ = build/sites_json.o
LEGENDARIES_CATALOG_SRC = build/legendaries_catalog.json
LEGENDARIES_CATALOG_OBJ = build/legendaries_catalog_json.o
CHEATSHEETS_ZIP_SRC = build/cheatsheets.zip
CHEATSHEETS_ZIP_OBJ = build/cheatsheets_zip.o
UI_CHROME_ZIP_SRC = build/ui_chrome.zip
UI_CHROME_ZIP_OBJ = build/ui_chrome_zip.o

DLL_SRC = \
	src/entry.cpp \
	src/entryHotkeys.cpp \
	src/entryWndProc.cpp \
	src/entryLoad.cpp \
	src/entryUnload.cpp \
	src/app/Settings.cpp \
	src/app/AddonPaths.cpp \
	src/app/MumbleIdentity.cpp \
	src/app/AspectLayout.cpp \
	src/app/PanelBinds.cpp \
	src/app/PanelBindsUi.cpp \
	src/app/Gw2Icons.cpp \
	src/app/Gw2Ui.cpp \
	src/app/Gw2UiPadChrome.cpp \
	src/app/UiChrome.cpp \
	src/api/Gw2Http.cpp \
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
	src/account/lookup/LookupPad.cpp \
	src/account/lookup/LookupFetch.cpp \
	src/account/wallet/WalletPad.cpp \
	src/account/wallet/WalletFetch.cpp \
	src/account/wallet/WalletFetchAcc.cpp \
	src/account/vault/VaultPad.cpp \
	src/account/vault/VaultData.cpp \
	src/account/vault/VaultFetch.cpp \
	src/account/AccountPad.cpp \
	src/account/progress/ProgressData.cpp \
	src/account/progress/ProgressFetch.cpp \
	src/account/crafting/CraftingData.cpp \
	src/account/crafting/CraftingApi.cpp \
	src/account/crafting/CraftingApiRecipe.cpp \
	src/account/crafting/CraftingWikiIds.cpp \
	src/account/crafting/CraftingWikiFetch.cpp \
	src/account/crafting/CraftingWikiAcquire.cpp \
	src/account/crafting/CraftingCurated.cpp \
	src/account/crafting/CraftingPlan.cpp \
	src/account/crafting/CraftingPlanResolve.cpp \
	src/account/crafting/CraftingPlanSnapshot.cpp \
	src/account/crafting/CraftingDailies.cpp \
	src/events/EventsPad.cpp \
	src/events/EventsPadState.cpp \
	src/events/EventsData.cpp \
	src/economy/EconomyPad.cpp \
	src/economy/EconomyPadState.cpp \
	src/economy/EconomyFetch.cpp \
	src/economy/EconomyFetchFlip.cpp \
	src/economy/EconomyFetchCart.cpp \
	src/instances/InstancesPad.cpp \
	src/instances/InstancesPadState.cpp \
	src/instances/InstancesData.cpp \
	src/completion/CompletionPad.cpp \
	src/completion/CompletionPadState.cpp \
	src/completion/CompletionData.cpp \
	src/completion/CompletionRoute.cpp \
	src/completion/CompletionHierarchy.cpp \
	src/completion/CompletionFavorites.cpp \
	src/completion/CompletionChecklist.cpp \
	src/completion/CompletionAtlas.cpp \
	src/farming/FarmingPad.cpp \
	src/farming/FarmingPadState.cpp \
	src/farming/FarmingPersist.cpp \
	src/overlay/GpsArrow.cpp \
	src/overlay/ZoneBanner.cpp \
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
	src/pathing/trailtools/TrailToolsPad.cpp \
	src/pathing/trailtools/TrailToolsPadLive.cpp \
	src/pathing/trailtools/TrailToolsPadTrailDesk.cpp \
	src/pathing/trailtools/TrailToolsPadTrailHelpers.cpp \
	src/pathing/trailtools/TrailToolsPadTrailRaw.cpp \
	src/pathing/trailtools/TrailToolsPadMarkers.cpp \
	src/pathing/trailtools/TrailToolsPadXmlDesk.cpp \
	src/pathing/trailtools/TrailToolsPadMarkersScript.cpp \
	src/pathing/trailtools/TrailToolsPadLua.cpp \
	src/pathing/trailtools/TrailToolsPadPack.cpp \
	src/pathing/trailtools/TrailToolsPadKeybinds.cpp \
	src/pathing/trailtools/TrailToolsBinds.cpp \
	src/pathing/trailtools/TrailToolsBindsChord.cpp \
	src/pathing/trailtools/TrailToolsBindsActions.cpp \
	src/pathing/trailtools/TrailToolsState.cpp \
	src/pathing/trailtools/TrailToolsStateEditors.cpp \
	src/pathing/trailtools/TrailToolsStateCategories.cpp \
	src/pathing/trailtools/TrailToolsStateFs.cpp \
	src/pathing/trailtools/TrailToolsTrl.cpp \
	src/pathing/trailtools/TrailToolsXml.cpp \
	src/pathing/trailtools/TrailToolsBuild.cpp \
	src/pathing/trailtools/TrailToolsPreview.cpp \
	src/pathing/trailtools/TrailToolsPreviewCompass.cpp \
	src/pathing/trailtools/TrailToolsDraftStyle.cpp \
	src/pathing/trailtools/TrailToolsAssets.cpp \
	src/pathing/trailtools/TrailToolsPersist.cpp \
	src/pathing/trailtools/TrailToolsImport.cpp \
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
	src/pathing/trails/PathingTrailsGuide.cpp \
	src/pathing/packs/PathingLoad.cpp \
	src/pathing/packs/PathingLoadLady.cpp \
	src/pathing/packs/PathingLoadHttp.cpp \
	src/pathing/packs/PathingLoadGuide.cpp \
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

.PHONY: all clean install install-reset validate-sites enrich-sites export-cheatsheets pack-cheatsheets pack-ui-chrome test-css test-parse test-ipc ci pack-cef

all: $(DLL_OUT)

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
TEST_PARSE_BIN = build/test_logmanager_parse
TEST_IPC_BIN = build/test_wiki_ipc
TEST_JSON_VIEW_BIN = build/test_json_view

test-parse: $(TEST_PARSE_BIN)
	./$(TEST_PARSE_BIN) tools/fixtures/ei_players_sample.json tools/fixtures/dpsreport_players_sample.json
	python3 tools/test_trl_parse.py

TEST_TRAILTOOLS_BIN = build/test_trailtools_roundtrip.exe
test-trailtools: $(TEST_TRAILTOOLS_BIN)
	wine $(TEST_TRAILTOOLS_BIN)

$(TEST_TRAILTOOLS_BIN): tools/test_trailtools_roundtrip.cpp \
	src/pathing/trailtools/TrailToolsTrl.cpp src/pathing/trailtools/TrailToolsXml.cpp \
	src/pathing/trailtools/TrailToolsTrl.h src/pathing/trailtools/TrailToolsXml.h \
	src/pathing/trailtools/TrailToolsShared.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -o $@ \
		tools/test_trailtools_roundtrip.cpp \
		src/pathing/trailtools/TrailToolsTrl.cpp \
		src/pathing/trailtools/TrailToolsXml.cpp \
		-lole32 -luuid -lshell32 -lcrypt32

TEST_PATHING_LUA_BIN = build/test_pathing_lua.exe
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

LUA_TEST_COBJ = $(patsubst deps/lua/%.c,build/test_lua/%.o,$(LUA_TEST_C))

build/test_lua/%.o: deps/lua/%.c
	@mkdir -p build/test_lua
	x86_64-w64-mingw32-gcc -std=c11 -O2 -Ideps/lua -c -o $@ $<

$(TEST_PATHING_LUA_BIN): $(LUA_TEST_CPP) $(LUA_TEST_COBJ) \
	src/pathing/lua/PathingLua.h src/pathing/lua/PathingLuaInternal.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -o $@ \
		$(LUA_TEST_CPP) $(LUA_TEST_COBJ) \
		-lole32 -luuid -lshell32 -lcrypt32 -lwinhttp

test-ipc: $(TEST_IPC_BIN)
	./$(TEST_IPC_BIN)

test-json-view: $(TEST_JSON_VIEW_BIN)
	./$(TEST_JSON_VIEW_BIN)

$(TEST_PARSE_BIN): tools/test_logmanager_parse.cpp src/logs/logmanager/LogManagerParse.cpp src/logs/logmanager/LogManagerParsePlayers.cpp src/logs/logmanager/LogManagerParse.h
	@mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/logs -Isrc/logs/logmanager -o $@ tools/test_logmanager_parse.cpp src/logs/logmanager/LogManagerParse.cpp src/logs/logmanager/LogManagerParsePlayers.cpp

$(TEST_IPC_BIN): tools/test_wiki_ipc.cpp src/browser/WikiIpc.h
	@mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/browser -o $@ tools/test_wiki_ipc.cpp

$(TEST_JSON_VIEW_BIN): tools/test_json_view.cpp src/api/JsonView.h
	@mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -o $@ tools/test_json_view.cpp

# Local continuous integration. Also used by .githooks/pre-push and GitHub Actions.
ci:
	@bash tools/ci.sh

pack-cef:
	bash scripts/pack-cef-runtime.sh

$(HELPER_OUT): $(HELPER_SRC) src/browser/WikiIpc.h src/helper/HelperInternal.h \
	src/helper/CssCompat.h src/helper/CssCompatInternal.h src/helper/CssProxy.h src/helper/BootJs.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_EXE) $(LDFLAGS_EXE) -o $@ $(HELPER_SRC) $(LIBS_EXE)
	@echo "Built $@"

# Flatten path so ld binary symbols are stable: _binary_helper_blob_exe_*
$(HELPER_BLOB_SRC): $(HELPER_OUT)
	/bin/cp -f $(HELPER_OUT) $(HELPER_BLOB_SRC)

$(HELPER_BLOB_OBJ): $(HELPER_BLOB_SRC)
	$(LD) -r -b binary -o $@ $(HELPER_BLOB_SRC)
	@echo "Embedded helper blob $@"

# Flatten asset paths so ld symbols stay stable: _binary_home_logo_png_* / _binary_home_cover_jpg_*
$(HOME_LOGO_SRC): docs/media/home-logo.png
	@mkdir -p $(dir $@)
	/bin/cp -f $< $@

$(HOME_COVER_SRC): docs/media/home-cover.jpg
	@mkdir -p $(dir $@)
	/bin/cp -f $< $@

$(HOME_LOGO_OBJ): $(HOME_LOGO_SRC)
	cd build && $(LD) -r -b binary -o home_logo.o home_logo.png
	@echo "Embedded home logo $@"

$(HOME_COVER_OBJ): $(HOME_COVER_SRC)
	cd build && $(LD) -r -b binary -o home_cover.o home_cover.jpg
	@echo "Embedded home cover $@"

$(SITES_JSON_SRC): $(SITES_JSON)
	@mkdir -p $(dir $@)
	/bin/cp -f $< $@

$(SITES_JSON_OBJ): $(SITES_JSON_SRC)
	$(LD) -r -b binary -o $@ $(SITES_JSON_SRC)
	@echo "Embedded sites catalog $@"

$(LEGENDARIES_CATALOG_SRC): data/legendaries/catalog.min.json
	@mkdir -p $(dir $@)
	/bin/cp -f $< $@

$(LEGENDARIES_CATALOG_OBJ): $(LEGENDARIES_CATALOG_SRC)
	$(LD) -r -b binary -o $@ $(LEGENDARIES_CATALOG_SRC)
	@echo "Embedded legendaries catalog $@"

$(CHEATSHEETS_ZIP_SRC): $(CHEATSHEETS_DIR)/manifest.json $(CHEATSHEETS_DIR)/shared.css $(wildcard $(CHEATSHEETS_DIR)/*.html)
	python3 tools/pack_cheatsheets.py

$(CHEATSHEETS_ZIP_OBJ): $(CHEATSHEETS_ZIP_SRC)
	$(LD) -r -b binary -o $@ $(CHEATSHEETS_ZIP_SRC)
	@echo "Embedded cheatsheets pack $@"

UI_CHROME_DIR = data/ui-chrome

$(UI_CHROME_ZIP_SRC): $(UI_CHROME_DIR)/manifest.txt $(wildcard $(UI_CHROME_DIR)/*.png)
	python3 tools/pack_ui_chrome.py

$(UI_CHROME_ZIP_OBJ): $(UI_CHROME_ZIP_SRC)
	$(LD) -r -b binary -o $@ $(UI_CHROME_ZIP_SRC)
	@echo "Embedded UI chrome pack $@"

$(DLL_OUT): $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(SITES_JSON_OBJ) $(LEGENDARIES_CATALOG_OBJ) $(CHEATSHEETS_ZIP_OBJ) $(UI_CHROME_ZIP_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS_DLL) -o $@ $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(SITES_JSON_OBJ) $(LEGENDARIES_CATALOG_OBJ) $(CHEATSHEETS_ZIP_OBJ) $(UI_CHROME_ZIP_OBJ) $(LIBS_DLL)
	@echo "Built $@ (CEF helper + homepage + sites.json + legendaries + cheatsheets + ui-chrome embedded)"

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
	# Beta wins and the fresh DLL is ignored as a duplicate. Keep them identical.
	/bin/cp -f "$(DLL_OUT)" "$(GW2_ADDONS)/GW2-InGame-Helper-Beta.dll"
	@echo "Also updated GW2-InGame-Helper-Beta.dll (prevents old Beta shadowing)"
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
	@printf 'uc6' > "$(INSTALL_DIR)/ui-chrome/ui-chrome.ver"
	@echo "Installed DLL -> $(INSTALL_DLL)"
	@echo "Data folder   -> $(INSTALL_DIR)/ (created; runtime extracts here)"
	@echo "Pathing       -> $(INSTALL_DIR)/pathing/"
	@ls -lh "$(INSTALL_DLL)"
	@ls -lh "$(INSTALL_DIR)/pathing/" 2>/dev/null || true
	@ls -lh "$(INSTALL_DIR)/ui-chrome/" 2>/dev/null || true

install-reset: $(DLL_OUT)
	@$(MAKE) install
	/bin/rm -f "$(INSTALL_DIR)/settings.ini"
	@echo "Also removed settings.ini (tabs/favorites reset on next launch)"

clean:
	rm -rf build
