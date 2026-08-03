cmake_minimum_required(VERSION 3.16)
project(GW2-InGame-Helper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(GW2HelperBrowser WIN32
  src/helper/main.cpp
  src/helper/HelperNavPolicy.cpp
  src/helper/HelperOsrRender.cpp
  src/helper/CssCompat.cpp
  src/helper/CssProxy.cpp
)
target_include_directories(GW2HelperBrowser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
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
  src/Settings.cpp
  src/AddonPaths.cpp
  src/Sites.cpp
  src/SitesLoad.cpp
  src/BrowserTabs.cpp
  src/HomePage.cpp
  src/RaidFood.cpp
  src/CheatSheets.cpp
  src/Gw2Http.cpp
  src/LivePanels.cpp
  src/LivePanelsBuild.cpp
  src/LivePanels_Html.cpp
  src/NotesPad.cpp
  src/WaypointsData.cpp
  src/MumbleIdentity.cpp
  src/RoutingSuggest.cpp
  src/CharacterProfiles.cpp
  src/ConfirmedWaypoints.cpp
  src/UnlocksData.cpp
  src/UnlocksPad.cpp
  src/InventoryData.cpp
  src/SessionHistoryData.cpp
  src/TpWatchPad.cpp
  src/LookupPad.cpp
  src/WalletPad.cpp
  src/VaultPad.cpp
  src/AccountPad.cpp
  src/ProgressData.cpp
  src/CraftingData.cpp
  src/EventsPad.cpp
  src/EventsData.cpp
  src/LogManagerPad.cpp
  src/LogManagerParse.cpp
  src/LogManagerUpload.cpp
  src/LogManagerEi.cpp
  src/EiRuntime.cpp
  src/TekkitGuidesPad.cpp
  src/TekkitTrails.cpp
  src/TekkitIndex.cpp
  src/TekkitParse.cpp
  src/PathingPacks.cpp
  src/PathingFeatures.cpp
  src/MarkerBehaviors.cpp
  src/CompassOverlay.cpp
  src/WorldOverlay.cpp
  src/DirectionCompass.cpp
  src/HelperQuickAccess.cpp
  src/WikiBrowser.cpp
  src/WikiBrowserHelper.cpp
  src/WikiBrowserIpc.cpp
  src/WikiBrowserPresent.cpp
  src/CefRuntime.cpp
  src/UI.cpp
  src/UI_Browse.cpp
  src/UI_Options.cpp
  src/SyncQr.cpp
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
  ${CMAKE_CURRENT_SOURCE_DIR}/src
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
