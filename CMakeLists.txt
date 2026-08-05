cmake_minimum_required(VERSION 3.16)
project(GW2-InGame-Helper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/browse
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/settings
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/quickaccess
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/chrome
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browse
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browser
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browse/sites
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browse/livepanels
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browse/tabs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/crafting
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/tpwatch
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/unlocks
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/wallet
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/vault
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/lookup
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account/progress
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing/trailtools
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing/world
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing/lua
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing/packs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing/trails
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing/waypoints
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logs
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logs/logmanager
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logs/eiruntime
  ${CMAKE_CURRENT_SOURCE_DIR}/src/events
  ${CMAKE_CURRENT_SOURCE_DIR}/src/notes
  ${CMAKE_CURRENT_SOURCE_DIR}/src/helper
)

add_executable(GW2HelperBrowser WIN32
  src/helper/main.cpp
  src/helper/HelperState.cpp
  src/helper/HelperTabs.cpp
  src/helper/HelperHandlers.cpp
  src/helper/HelperCommands.cpp
  src/helper/HelperNavPolicy.cpp
  src/helper/HelperNavPolicyHandlers.cpp
  src/helper/HelperOsrRender.cpp
  src/helper/CssCompat.cpp
  src/helper/CssCompatYoutube.cpp
  src/helper/CssCompatLegacy.cpp
  src/helper/CssCompatLegacyRewrite.cpp
  src/helper/CssProxy.cpp
)
target_include_directories(GW2HelperBrowser PRIVATE
  ${SRC_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/cef
)
target_compile_definitions(GW2HelperBrowser PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MINGW)
  target_link_libraries(GW2HelperBrowser PRIVATE gdi32 ole32 uuid shell32 winhttp)
  target_link_options(GW2HelperBrowser PRIVATE -static -static-libgcc -static-libstdc++)
endif()
set_target_properties(GW2HelperBrowser PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  OUTPUT_NAME "GW2HelperBrowser"
)

add_library(GW2-InGame-Helper SHARED
  src/entry.cpp
  src/entryHotkeys.cpp
  src/entryWndProc.cpp
  src/entryLoad.cpp
  src/entryUnload.cpp
  src/app/Settings.cpp
  src/app/AddonPaths.cpp
  src/app/MumbleIdentity.cpp
  src/app/AspectLayout.cpp
  src/api/Gw2Http.cpp
  src/browse/sites/Sites.cpp
  src/browse/sites/SitesState.cpp
  src/browse/sites/SitesUrlMatch.cpp
  src/browse/sites/SitesFavorites.cpp
  src/browse/sites/SitesLoad.cpp
  src/browse/sites/SitesLoadParse.cpp
  src/browse/tabs/BrowserTabs.cpp
  src/browse/tabs/BrowserTabsState.cpp
  src/browse/tabs/BrowserTabsNav.cpp
  src/browse/HomePage.cpp
  src/browse/HomePageHtml.cpp
  src/browse/RaidFood.cpp
  src/browse/RaidFoodHtml.cpp
  src/browse/CheatSheets.cpp
  src/browse/livepanels/LivePanels.cpp
  src/browse/livepanels/LivePanelsAsync.cpp
  src/browse/livepanels/LivePanelsCraftPlan.cpp
  src/browse/livepanels/LivePanelsBuildCommon.cpp
  src/browse/livepanels/LivePanelsBuildJson.cpp
  src/browse/livepanels/LivePanelsBuildPage.cpp
  src/browse/livepanels/LivePanelsBuildDailies.cpp
  src/browse/livepanels/LivePanelsBuildNews.cpp
  src/browse/livepanels/LivePanelsBuildFashion.cpp
  src/browse/livepanels/LivePanelsBuildProgress.cpp
  src/browse/livepanels/LivePanelsBuildProgressArmory.cpp
  src/browse/livepanels/LivePanelsBuildLegendaryLedger.cpp
  src/browse/livepanels/LivePanelsBuildLegendaryDetail.cpp
  src/browse/livepanels/LivePanelsBuildCheatSheetsHub.cpp
  src/browse/livepanels/LivePanelsBuildBrowseHub.cpp
  src/browse/livepanels/LivePanelsBuildApiCheck.cpp
  src/browse/livepanels/LivePanels_Html.cpp
  src/notes/NotesPad.cpp
  src/notes/NotesPadWaypoints.cpp
  src/pathing/waypoints/WaypointsData.cpp
  src/pathing/waypoints/WaypointsDataParse.cpp
  src/pathing/waypoints/RoutingSuggest.cpp
  src/pathing/waypoints/ConfirmedWaypoints.cpp
  src/account/CharacterProfiles.cpp
  src/account/unlocks/UnlocksData.cpp
  src/account/unlocks/UnlocksDataLoad.cpp
  src/account/unlocks/UnlocksPad.cpp
  src/account/InventoryData.cpp
  src/account/SessionHistoryData.cpp
  src/account/tpwatch/TpWatchPad.cpp
  src/account/tpwatch/TpWatchPadUi.cpp
  src/account/tpwatch/TpWatchData.cpp
  src/account/tpwatch/TpWatchResolve.cpp
  src/account/tpwatch/TpWatchFetch.cpp
  src/account/lookup/LookupPad.cpp
  src/account/lookup/LookupFetch.cpp
  src/account/wallet/WalletPad.cpp
  src/account/wallet/WalletFetch.cpp
  src/account/wallet/WalletFetchAcc.cpp
  src/account/vault/VaultPad.cpp
  src/account/vault/VaultData.cpp
  src/account/vault/VaultFetch.cpp
  src/account/AccountPad.cpp
  src/account/progress/ProgressData.cpp
  src/account/progress/ProgressFetch.cpp
  src/account/crafting/CraftingData.cpp
  src/account/crafting/CraftingApi.cpp
  src/account/crafting/CraftingApiRecipe.cpp
  src/account/crafting/CraftingWiki.cpp
  src/account/crafting/CraftingWikiAcquire.cpp
  src/account/crafting/CraftingCurated.cpp
  src/account/crafting/CraftingPlan.cpp
  src/account/crafting/CraftingPlanResolve.cpp
  src/account/crafting/CraftingPlanSnapshot.cpp
  src/account/crafting/CraftingDailies.cpp
  src/events/EventsPad.cpp
  src/events/EventsPadState.cpp
  src/events/EventsData.cpp
  src/logs/logmanager/LogManagerPad.cpp
  src/logs/logmanager/LogManagerPadState.cpp
  src/logs/logmanager/LogManagerParse.cpp
  src/logs/logmanager/LogManagerParsePlayers.cpp
  src/logs/logmanager/LogManagerUpload.cpp
  src/logs/logmanager/LogManagerUploadWorkers.cpp
  src/logs/logmanager/LogManagerEi.cpp
  src/logs/logmanager/LogManagerCache.cpp
  src/logs/logmanager/LogManagerKillProof.cpp
  src/logs/logmanager/LogManagerScan.cpp
  src/logs/logmanager/LogManagerStats.cpp
  src/logs/logmanager/LogManagerUi.cpp
  src/logs/logmanager/LogManagerUiDetail.cpp
  src/logs/logmanager/LogManagerUiTabs.cpp
  src/logs/eiruntime/EiRuntime.cpp
  src/logs/eiruntime/EiRuntimeFs.cpp
  src/logs/eiruntime/EiRuntimeHttp.cpp
  src/pathing/waypoints/PathingGuidesPad.cpp
  src/pathing/trailtools/TrailToolsPad.cpp
  src/pathing/trailtools/TrailToolsPadLive.cpp
  src/pathing/trailtools/TrailToolsPadTrail.cpp
  src/pathing/trailtools/TrailToolsPadMarkers.cpp
  src/pathing/trailtools/TrailToolsPadPack.cpp
  src/pathing/trailtools/TrailToolsState.cpp
  src/pathing/trailtools/TrailToolsTrl.cpp
  src/pathing/trailtools/TrailToolsXml.cpp
  src/pathing/trailtools/TrailToolsBuild.cpp
  src/pathing/trailtools/TrailToolsPreview.cpp
  src/pathing/trails/PathingTrails.cpp
  src/pathing/trails/PathingTrailsCore.cpp
  src/pathing/packs/PathingLoad.cpp
  src/pathing/packs/PathingLoadLady.cpp
  src/pathing/packs/PathingLoadHttp.cpp
  src/pathing/packs/PathingLoadGuide.cpp
  src/pathing/packs/PathingLoadIcons.cpp
  src/pathing/trails/PathingTrailsGps.cpp
  src/pathing/trails/PathingTrailsPresets.cpp
  src/pathing/trails/PathingTrailsUi.cpp
  src/pathing/packs/PathingIndex.cpp
  src/pathing/packs/PathingIndexDiscover.cpp
  src/pathing/packs/PathingParse.cpp
  src/pathing/packs/PathingParseXml.cpp
  src/pathing/packs/PathingParseZip.cpp
  src/pathing/packs/PathingPacks.cpp
  src/pathing/packs/PathingPacksHttp.cpp
  src/pathing/packs/PathingFeatures.cpp
  src/pathing/world/MarkerBehaviors.cpp
  src/pathing/world/MarkerBehaviorsState.cpp
  src/pathing/world/CompassOverlay.cpp
  src/pathing/world/WorldGpsMath.cpp
  src/pathing/world/WorldGpsD3dDevice.cpp
  src/pathing/world/WorldGpsD3dDraw.cpp
  src/pathing/world/WorldGpsImgui.cpp
  src/pathing/world/WorldOverlay.cpp
  src/pathing/world/DirectionCompass.cpp
  src/pathing/lua/PathingLua.cpp
  src/pathing/lua/PathingLuaApi.cpp
  src/pathing/lua/PathingLuaCdn.cpp
  src/pathing/lua/PathingLuaLoad.cpp
  src/pathing/lua/PathingLuaMarker.cpp
  src/pathing/lua/PathingLuaMenu.cpp
  src/pathing/lua/PathingLuaMumble.cpp
  src/pathing/lua/PathingLuaPack.cpp
  src/pathing/lua/PathingLuaTrail.cpp
  src/pathing/lua/PathingLuaTypes.cpp
  src/pathing/lua/PathingLuaWorld.cpp
  src/pathing/packs/PathingSchedule.cpp
  src/pathing/trails/PathingTrailsPresetsMc.cpp
  src/pathing/trailtools/TrailToolsAssets.cpp
  src/pathing/trailtools/TrailToolsBinds.cpp
  src/pathing/trailtools/TrailToolsDraftStyle.cpp
  src/pathing/trailtools/TrailToolsImport.cpp
  src/pathing/trailtools/TrailToolsPadKeybinds.cpp
  src/pathing/trailtools/TrailToolsPadLua.cpp
  src/pathing/trailtools/TrailToolsPadMarkersScript.cpp
  src/pathing/trailtools/TrailToolsPersist.cpp
  src/pathing/trailtools/TrailToolsPreviewCompass.cpp
  src/ui/quickaccess/HelperQuickAccess.cpp
  src/browser/WikiBrowser.cpp
  src/browser/WikiBrowserApi.cpp
  src/browser/WikiBrowserHelper.cpp
  src/browser/WikiBrowserHelperLifecycle.cpp
  src/browser/WikiBrowserHelperLaunch.cpp
  src/browser/WikiBrowserIpc.cpp
  src/browser/WikiBrowserPresent.cpp
  src/browser/CefRuntime.cpp
  src/browser/CefRuntimeFs.cpp
  src/browser/CefRuntimeVerify.cpp
  src/browser/CefRuntimeHttp.cpp
  src/ui/chrome/UI.cpp
  src/ui/chrome/UI_Helpers.cpp
  src/ui/chrome/UI_ChromeTabs.cpp
  src/ui/chrome/UI_ChromeToolbar.cpp
  src/ui/chrome/UI_Render.cpp
  src/ui/chrome/UI_RenderPage.cpp
  src/ui/browse/UI_Browse.cpp
  src/ui/browse/UI_BrowseHelpers.cpp
  src/ui/browse/UI_BrowsePanel.cpp
  src/ui/chrome/UI_Options.cpp
  src/ui/settings/SettingsPad.cpp
  src/ui/settings/SettingsPadBody.cpp
  deps/imgui/imgui.cpp
  deps/imgui/imgui_draw.cpp
  deps/imgui/imgui_tables.cpp
  deps/imgui/imgui_widgets.cpp
  deps/miniz/miniz.c
  deps/miniz/miniz_tdef.c
  deps/miniz/miniz_tinfl.c
  deps/miniz/miniz_zip.c
)
target_include_directories(GW2-InGame-Helper PRIVATE
  ${SRC_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/deps
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/cef
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/miniz
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/qrcodegen
)
target_compile_definitions(GW2-InGame-Helper PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
if(MINGW)
  target_link_libraries(GW2-InGame-Helper PRIVATE dxgi d3d11 gdi32 ole32 uuid shell32 winhttp crypt32 bcrypt)
  target_link_options(GW2-InGame-Helper PRIVATE -static -static-libgcc -static-libstdc++)
  set_target_properties(GW2-InGame-Helper PROPERTIES PREFIX "")
endif()
set_target_properties(GW2-InGame-Helper PROPERTIES
  OUTPUT_NAME "GW2-InGame-Helper"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_dependencies(GW2-InGame-Helper GW2HelperBrowser)

# Note: embedding the helper blob is handled by the Makefile.
# Prefer `make` for a single-DLL distribution build.
