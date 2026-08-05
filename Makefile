# Cross-compile GW2-InGame-Helper.dll (+ embedded CEF helper) for Windows / Wine
# Private CEF 150 runtime downloads into addons/GW2-InGame-Helper/cef/ on first use.
CXX      = x86_64-w64-mingw32-g++
LD       = x86_64-w64-mingw32-ld
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS
CXXFLAGS += -DCEF_API_VERSION=15000
CXXFLAGS += -Isrc -Isrc/app -Isrc/ui -Isrc/api -Isrc/browse -Isrc/browser \
	-Isrc/account -Isrc/pathing -Isrc/logs -Isrc/events -Isrc/notes -Isrc/helper
CXXFLAGS += -Ideps -Ideps/imgui -Ideps/cef -Ideps/miniz -Ideps/qrcodegen
# Dependency files: emit only from the build/%.o rule via -MF (never beside sources).
# Helper prefers msvcrt over UCRT so Wine CreateProcess doesn't fail on api-ms-win-crt-*.dll
CXXFLAGS_EXE = $(CXXFLAGS) -mcrtdll=msvcrt
LDFLAGS_DLL  = -shared -static -static-libgcc -static-libstdc++
LDFLAGS_EXE  = -static -static-libgcc -static-libstdc++ -mwindows -municode -mcrtdll=msvcrt
LIBS_DLL = -ldxgi -ld3d11 -lgdi32 -lole32 -luuid -lshell32 -lwinhttp -lcrypt32 -lbcrypt
LIBS_EXE = -lgdi32 -lole32 -luuid -lshell32 -lwinhttp

HELPER_SRC = src/helper/main.cpp src/helper/HelperState.cpp src/helper/HelperTabs.cpp \
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
	src/api/Gw2Http.cpp \
	src/browse/Sites.cpp \
	src/browse/SitesState.cpp \
	src/browse/SitesUrlMatch.cpp \
	src/browse/SitesFavorites.cpp \
	src/browse/SitesLoad.cpp \
	src/browse/SitesLoadParse.cpp \
	src/browse/BrowserTabs.cpp \
	src/browse/BrowserTabsState.cpp \
	src/browse/BrowserTabsNav.cpp \
	src/browse/HomePage.cpp \
	src/browse/HomePageHtml.cpp \
	src/browse/RaidFood.cpp \
	src/browse/RaidFoodHtml.cpp \
	src/browse/CheatSheets.cpp \
	src/browse/LivePanels.cpp \
	src/browse/LivePanelsAsync.cpp \
	src/browse/LivePanelsCraftPlan.cpp \
	src/browse/LivePanelsBuildCommon.cpp \
	src/browse/LivePanelsBuildJson.cpp \
	src/browse/LivePanelsBuildPage.cpp \
	src/browse/LivePanelsBuildDailies.cpp \
	src/browse/LivePanelsBuildNews.cpp \
	src/browse/LivePanelsBuildFashion.cpp \
	src/browse/LivePanelsBuildProgress.cpp \
	src/browse/LivePanelsBuildProgressArmory.cpp \
	src/browse/LivePanelsBuildLegendaryLedger.cpp \
	src/browse/LivePanelsBuildLegendaryDetail.cpp \
	src/browse/LivePanelsBuildCheatSheetsHub.cpp \
	src/browse/LivePanelsBuildBrowseHub.cpp \
	src/browse/LivePanelsBuildApiCheck.cpp \
	src/browse/LivePanels_Html.cpp \
	src/notes/NotesPad.cpp \
	src/notes/NotesPadWaypoints.cpp \
	src/pathing/WaypointsData.cpp \
	src/pathing/WaypointsDataParse.cpp \
	src/pathing/RoutingSuggest.cpp \
	src/pathing/ConfirmedWaypoints.cpp \
	src/account/CharacterProfiles.cpp \
	src/account/UnlocksData.cpp \
	src/account/UnlocksDataLoad.cpp \
	src/account/UnlocksPad.cpp \
	src/account/InventoryData.cpp \
	src/account/SessionHistoryData.cpp \
	src/account/TpWatchPad.cpp \
	src/account/TpWatchPadUi.cpp \
	src/account/TpWatchData.cpp \
	src/account/TpWatchResolve.cpp \
	src/account/TpWatchFetch.cpp \
	src/account/LookupPad.cpp \
	src/account/LookupFetch.cpp \
	src/account/WalletPad.cpp \
	src/account/WalletFetch.cpp \
	src/account/WalletFetchAcc.cpp \
	src/account/VaultPad.cpp \
	src/account/VaultData.cpp \
	src/account/VaultFetch.cpp \
	src/account/AccountPad.cpp \
	src/account/ProgressData.cpp \
	src/account/ProgressFetch.cpp \
	src/account/CraftingData.cpp \
	src/account/CraftingApi.cpp \
	src/account/CraftingApiRecipe.cpp \
	src/account/CraftingWiki.cpp \
	src/account/CraftingWikiAcquire.cpp \
	src/account/CraftingCurated.cpp \
	src/account/CraftingPlan.cpp \
	src/account/CraftingPlanResolve.cpp \
	src/account/CraftingPlanSnapshot.cpp \
	src/account/CraftingDailies.cpp \
	src/events/EventsPad.cpp \
	src/events/EventsPadState.cpp \
	src/events/EventsData.cpp \
	src/logs/LogManagerPad.cpp \
	src/logs/LogManagerPadState.cpp \
	src/logs/LogManagerParse.cpp \
	src/logs/LogManagerParsePlayers.cpp \
	src/logs/LogManagerUpload.cpp \
	src/logs/LogManagerUploadWorkers.cpp \
	src/logs/LogManagerEi.cpp \
	src/logs/LogManagerCache.cpp \
	src/logs/LogManagerKillProof.cpp \
	src/logs/LogManagerScan.cpp \
	src/logs/LogManagerStats.cpp \
	src/logs/LogManagerUi.cpp \
	src/logs/LogManagerUiDetail.cpp \
	src/logs/LogManagerUiTabs.cpp \
	src/logs/EiRuntime.cpp \
	src/logs/EiRuntimeFs.cpp \
	src/logs/EiRuntimeHttp.cpp \
	src/pathing/PathingGuidesPad.cpp \
	src/pathing/PathingTrails.cpp \
	src/pathing/PathingTrailsCore.cpp \
	src/pathing/PathingLoad.cpp \
	src/pathing/PathingLoadLady.cpp \
	src/pathing/PathingLoadHttp.cpp \
	src/pathing/PathingLoadGuide.cpp \
	src/pathing/PathingLoadIcons.cpp \
	src/pathing/PathingTrailsGps.cpp \
	src/pathing/PathingTrailsPresets.cpp \
	src/pathing/PathingTrailsPresetsMc.cpp \
	src/pathing/PathingTrailsUi.cpp \
	src/pathing/PathingIndex.cpp \
	src/pathing/PathingIndexDiscover.cpp \
	src/pathing/PathingParse.cpp \
	src/pathing/PathingParseXml.cpp \
	src/pathing/PathingParseZip.cpp \
	src/pathing/PathingPacks.cpp \
	src/pathing/PathingPacksHttp.cpp \
	src/pathing/PathingFeatures.cpp \
	src/pathing/MarkerBehaviors.cpp \
	src/pathing/MarkerBehaviorsState.cpp \
	src/pathing/CompassOverlay.cpp \
	src/pathing/WorldGpsMath.cpp \
	src/pathing/WorldGpsD3dDevice.cpp \
	src/pathing/WorldGpsD3dDraw.cpp \
	src/pathing/WorldGpsImgui.cpp \
	src/pathing/WorldOverlay.cpp \
	src/pathing/DirectionCompass.cpp \
	src/ui/HelperQuickAccess.cpp \
	src/browser/WikiBrowser.cpp \
	src/browser/WikiBrowserApi.cpp \
	src/browser/WikiBrowserHelper.cpp \
	src/browser/WikiBrowserHelperLifecycle.cpp \
	src/browser/WikiBrowserHelperLaunch.cpp \
	src/browser/WikiBrowserIpc.cpp \
	src/browser/WikiBrowserPresent.cpp \
	src/browser/CefRuntime.cpp \
	src/browser/CefRuntimeFs.cpp \
	src/browser/CefRuntimeVerify.cpp \
	src/browser/CefRuntimeHttp.cpp \
	src/ui/UI.cpp \
	src/ui/UI_Helpers.cpp \
	src/ui/UI_ChromeTabs.cpp \
	src/ui/UI_ChromeToolbar.cpp \
	src/ui/UI_Render.cpp \
	src/ui/UI_RenderPage.cpp \
	src/ui/UI_Browse.cpp \
	src/ui/UI_BrowseHelpers.cpp \
	src/ui/UI_BrowsePanel.cpp \
	src/ui/UI_Options.cpp \
	src/ui/SettingsPad.cpp \
	src/ui/SettingsPadBody.cpp \
	deps/imgui/imgui.cpp \
	deps/imgui/imgui_draw.cpp \
	deps/imgui/imgui_tables.cpp \
	deps/imgui/imgui_widgets.cpp \
	deps/miniz/miniz.c \
	deps/miniz/miniz_tdef.c \
	deps/miniz/miniz_tinfl.c \
	deps/miniz/miniz_zip.c

DLL_OBJ = $(patsubst %.cpp,build/%.o,$(filter %.cpp,$(DLL_SRC))) \
	$(patsubst %.c,build/%.o,$(filter %.c,$(DLL_SRC)))
DLL_OUT = build/bin/GW2-InGame-Helper.dll

GW2_ROOT   ?= $(HOME)/.local/share/Steam/steamapps/common/Guild Wars 2
GW2_ADDONS ?= $(GW2_ROOT)/addons
INSTALL_DLL = $(GW2_ADDONS)/GW2-InGame-Helper.dll
INSTALL_DIR = $(GW2_ADDONS)/GW2-InGame-Helper

.PHONY: all clean install install-reset validate-sites enrich-sites export-cheatsheets pack-cheatsheets test-css test-parse test-ipc ci pack-cef

all: $(DLL_OUT)

SITES_JSON   = data/sites.json
CHEATSHEETS_DIR = data/cheatsheets

validate-sites:
	python3 tools/validate_sites.py $(SITES_JSON)

export-cheatsheets:
	python3 tools/export_cheatsheets.py

pack-cheatsheets: $(CHEATSHEETS_ZIP_SRC)

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

test-ipc: $(TEST_IPC_BIN)
	./$(TEST_IPC_BIN)

test-json-view: $(TEST_JSON_VIEW_BIN)
	./$(TEST_JSON_VIEW_BIN)

$(TEST_PARSE_BIN): tools/test_logmanager_parse.cpp src/logs/LogManagerParse.cpp src/logs/LogManagerParsePlayers.cpp src/logs/LogManagerParse.h
	@mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -Isrc -Isrc/logs -o $@ tools/test_logmanager_parse.cpp src/logs/LogManagerParse.cpp src/logs/LogManagerParsePlayers.cpp

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

$(DLL_OUT): $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(SITES_JSON_OBJ) $(LEGENDARIES_CATALOG_OBJ) $(CHEATSHEETS_ZIP_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS_DLL) -o $@ $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(SITES_JSON_OBJ) $(LEGENDARIES_CATALOG_OBJ) $(CHEATSHEETS_ZIP_OBJ) $(LIBS_DLL)
	@echo "Built $@ (CEF helper + homepage + sites.json + legendaries + cheatsheets embedded)"

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ $<

-include $(DLL_OBJ:.o=.d)

build/%.o: %.c
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ideps/miniz -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ $<

install: $(DLL_OUT)
	@mkdir -p "$(INSTALL_DIR)" "$(INSTALL_DIR)/pathing"
	/bin/cp -f "$(DLL_OUT)" "$(INSTALL_DLL)"
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
	@echo "Installed DLL -> $(INSTALL_DLL)"
	@echo "Data folder   -> $(INSTALL_DIR)/ (created; runtime extracts here)"
	@echo "Pathing       -> $(INSTALL_DIR)/pathing/"
	@ls -lh "$(INSTALL_DLL)"
	@ls -lh "$(INSTALL_DIR)/pathing/" 2>/dev/null || true

install-reset: $(DLL_OUT)
	@$(MAKE) install
	/bin/rm -f "$(INSTALL_DIR)/settings.ini"
	@echo "Also removed settings.ini (tabs/favorites reset on next launch)"

clean:
	rm -rf build
