cmake_minimum_required(VERSION 3.16)
project(GW2-InGame-Helper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
  ${CMAKE_CURRENT_SOURCE_DIR}/src/api
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browse
  ${CMAKE_CURRENT_SOURCE_DIR}/src/browser
  ${CMAKE_CURRENT_SOURCE_DIR}/src/account
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pathing
  ${CMAKE_CURRENT_SOURCE_DIR}/src/logs
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
  src/api/Gw2Http.cpp
  src/browse/Sites.cpp
  src/browse/SitesState.cpp
  src/browse/SitesUrlMatch.cpp
  src/browse/SitesFavorites.cpp
  src/browse/SitesLoad.cpp
  src/browse/SitesLoadParse.cpp
  src/browse/BrowserTabs.cpp
  src/browse/BrowserTabsState.cpp
  src/browse/BrowserTabsNav.cpp
  src/browse/HomePage.cpp
  src/browse/HomePageHtml.cpp
  src/browse/RaidFood.cpp
  src/browse/RaidFoodHtml.cpp
  src/browse/CheatSheets.cpp
  src/browse/LivePanels.cpp
  src/browse/LivePanelsAsync.cpp
  src/browse/LivePanelsBuildCommon.cpp
  src/browse/LivePanelsBuildJson.cpp
  src/browse/LivePanelsBuildPage.cpp
  src/browse/LivePanelsBuildDailies.cpp
  src/browse/LivePanelsBuildNews.cpp
  src/browse/LivePanelsBuildFashion.cpp
  src/browse/LivePanelsBuildProgress.cpp
  src/browse/LivePanelsBuildProgressArmory.cpp
  src/browse/LivePanels_Html.cpp
  src/notes/NotesPad.cpp
  src/notes/NotesPadWaypoints.cpp
  src/pathing/WaypointsData.cpp
  src/pathing/WaypointsDataParse.cpp
  src/pathing/RoutingSuggest.cpp
  src/pathing/ConfirmedWaypoints.cpp
  src/account/CharacterProfiles.cpp
  src/account/UnlocksData.cpp
  src/account/UnlocksDataLoad.cpp
  src/account/UnlocksPad.cpp
  src/account/InventoryData.cpp
  src/account/SessionHistoryData.cpp
  src/account/TpWatchPad.cpp
  src/account/TpWatchPadUi.cpp
  src/account/TpWatchData.cpp
  src/account/TpWatchResolve.cpp
  src/account/TpWatchFetch.cpp
  src/account/LookupPad.cpp
  src/account/LookupFetch.cpp
  src/account/WalletPad.cpp
  src/account/WalletFetch.cpp
  src/account/WalletFetchAcc.cpp
  src/account/VaultPad.cpp
  src/account/VaultData.cpp
  src/account/VaultFetch.cpp
  src/account/AccountPad.cpp
  src/account/ProgressData.cpp
  src/account/ProgressFetch.cpp
  src/account/CraftingData.cpp
  src/account/CraftingApi.cpp
  src/account/CraftingApiRecipe.cpp
  src/account/CraftingWiki.cpp
  src/account/CraftingPlan.cpp
  src/account/CraftingPlanResolve.cpp
  src/account/CraftingDailies.cpp
  src/events/EventsPad.cpp
  src/events/EventsPadState.cpp
  src/events/EventsData.cpp
  src/logs/LogManagerPad.cpp
  src/logs/LogManagerPadState.cpp
  src/logs/LogManagerParse.cpp
  src/logs/LogManagerParsePlayers.cpp
  src/logs/LogManagerUpload.cpp
  src/logs/LogManagerUploadWorkers.cpp
  src/logs/LogManagerEi.cpp
  src/logs/LogManagerCache.cpp
  src/logs/LogManagerKillProof.cpp
  src/logs/LogManagerScan.cpp
  src/logs/LogManagerStats.cpp
  src/logs/LogManagerUi.cpp
  src/logs/LogManagerUiDetail.cpp
  src/logs/LogManagerUiTabs.cpp
  src/logs/EiRuntime.cpp
  src/logs/EiRuntimeFs.cpp
  src/logs/EiRuntimeHttp.cpp
  src/pathing/PathingGuidesPad.cpp
  src/pathing/PathingTrails.cpp
  src/pathing/PathingTrailsCore.cpp
  src/pathing/PathingLoad.cpp
  src/pathing/PathingLoadLady.cpp
  src/pathing/PathingLoadHttp.cpp
  src/pathing/PathingLoadGuide.cpp
  src/pathing/PathingLoadIcons.cpp
  src/pathing/PathingTrailsGps.cpp
  src/pathing/PathingTrailsPresets.cpp
  src/pathing/PathingTrailsUi.cpp
  src/pathing/PathingIndex.cpp
  src/pathing/PathingIndexDiscover.cpp
  src/pathing/PathingParse.cpp
  src/pathing/PathingParseXml.cpp
  src/pathing/PathingParseZip.cpp
  src/pathing/PathingPacks.cpp
  src/pathing/PathingPacksHttp.cpp
  src/pathing/PathingFeatures.cpp
  src/pathing/MarkerBehaviors.cpp
  src/pathing/MarkerBehaviorsState.cpp
  src/pathing/CompassOverlay.cpp
  src/pathing/WorldGpsMath.cpp
  src/pathing/WorldGpsD3dDevice.cpp
  src/pathing/WorldGpsD3dDraw.cpp
  src/pathing/WorldGpsImgui.cpp
  src/pathing/WorldOverlay.cpp
  src/pathing/DirectionCompass.cpp
  src/ui/HelperQuickAccess.cpp
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
  src/ui/UI.cpp
  src/ui/UI_Helpers.cpp
  src/ui/UI_ChromeTabs.cpp
  src/ui/UI_ChromeToolbar.cpp
  src/ui/UI_Render.cpp
  src/ui/UI_RenderPage.cpp
  src/ui/UI_Browse.cpp
  src/ui/UI_BrowseHelpers.cpp
  src/ui/UI_BrowsePanel.cpp
  src/ui/UI_Options.cpp
  src/ui/SyncQr.cpp
  deps/qrcodegen/qrcodegen.c
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
