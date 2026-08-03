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
  src/helper/HelperNavPolicy.cpp
  src/helper/HelperOsrRender.cpp
  src/helper/CssCompat.cpp
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
  src/app/Settings.cpp
  src/app/AddonPaths.cpp
  src/app/MumbleIdentity.cpp
  src/api/Gw2Http.cpp
  src/browse/Sites.cpp
  src/browse/SitesLoad.cpp
  src/browse/BrowserTabs.cpp
  src/browse/HomePage.cpp
  src/browse/RaidFood.cpp
  src/browse/CheatSheets.cpp
  src/browse/LivePanels.cpp
  src/browse/LivePanelsBuildCommon.cpp
  src/browse/LivePanelsBuildDailies.cpp
  src/browse/LivePanelsBuildNews.cpp
  src/browse/LivePanelsBuildFashion.cpp
  src/browse/LivePanelsBuildProgress.cpp
  src/browse/LivePanels_Html.cpp
  src/notes/NotesPad.cpp
  src/pathing/WaypointsData.cpp
  src/pathing/RoutingSuggest.cpp
  src/pathing/ConfirmedWaypoints.cpp
  src/account/CharacterProfiles.cpp
  src/account/UnlocksData.cpp
  src/account/UnlocksPad.cpp
  src/account/InventoryData.cpp
  src/account/SessionHistoryData.cpp
  src/account/TpWatchPad.cpp
  src/account/TpWatchData.cpp
  src/account/LookupPad.cpp
  src/account/WalletPad.cpp
  src/account/WalletFetch.cpp
  src/account/VaultPad.cpp
  src/account/AccountPad.cpp
  src/account/ProgressData.cpp
  src/account/CraftingData.cpp
  src/account/CraftingApi.cpp
  src/account/CraftingWiki.cpp
  src/account/CraftingPlan.cpp
  src/account/CraftingDailies.cpp
  src/events/EventsPad.cpp
  src/events/EventsData.cpp
  src/logs/LogManagerPad.cpp
  src/logs/LogManagerParse.cpp
  src/logs/LogManagerUpload.cpp
  src/logs/LogManagerEi.cpp
  src/logs/LogManagerCache.cpp
  src/logs/LogManagerKillProof.cpp
  src/logs/LogManagerScan.cpp
  src/logs/LogManagerStats.cpp
  src/logs/LogManagerUi.cpp
  src/logs/EiRuntime.cpp
  src/pathing/PathingGuidesPad.cpp
  src/pathing/PathingTrails.cpp
  src/pathing/PathingLoad.cpp
  src/pathing/PathingTrailsGps.cpp
  src/pathing/PathingTrailsPresets.cpp
  src/pathing/PathingTrailsUi.cpp
  src/pathing/PathingIndex.cpp
  src/pathing/PathingParse.cpp
  src/pathing/PathingPacks.cpp
  src/pathing/PathingFeatures.cpp
  src/pathing/MarkerBehaviors.cpp
  src/pathing/CompassOverlay.cpp
  src/pathing/WorldOverlay.cpp
  src/pathing/DirectionCompass.cpp
  src/ui/HelperQuickAccess.cpp
  src/browser/WikiBrowser.cpp
  src/browser/WikiBrowserHelper.cpp
  src/browser/WikiBrowserIpc.cpp
  src/browser/WikiBrowserPresent.cpp
  src/browser/CefRuntime.cpp
  src/ui/UI.cpp
  src/ui/UI_Browse.cpp
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
