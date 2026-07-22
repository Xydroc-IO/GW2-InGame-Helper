# Cross-compile GW2-InGame-Helper.dll (+ embedded CEF helper) for Windows / Wine
CXX      = x86_64-w64-mingw32-g++
LD       = x86_64-w64-mingw32-ld
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS
CXXFLAGS += -Isrc -Ideps -Ideps/imgui -Ideps/cef
# Helper prefers msvcrt over UCRT so Wine CreateProcess doesn't fail on api-ms-win-crt-*.dll
CXXFLAGS_EXE = $(CXXFLAGS) -mcrtdll=msvcrt
LDFLAGS_DLL  = -shared -static -static-libgcc -static-libstdc++
LDFLAGS_EXE  = -static -static-libgcc -static-libstdc++ -mwindows -municode -mcrtdll=msvcrt
LIBS_DLL = -ldxgi -ld3d11 -lgdi32 -lole32 -luuid -lshell32 -lwinhttp -lcrypt32
LIBS_EXE = -lgdi32 -lole32 -luuid -lshell32 -lwinhttp

HELPER_SRC = src/helper/main.cpp src/helper/CssCompat.cpp src/helper/CssProxy.cpp
HELPER_OUT = build/bin/GW2HelperBrowser.exe
HELPER_BLOB_SRC = build/helper_blob.exe
HELPER_BLOB_OBJ = build/helper_blob.o
HOME_LOGO_SRC  = build/home_logo.png
HOME_COVER_SRC = build/home_cover.jpg
HOME_LOGO_OBJ  = build/home_logo.o
HOME_COVER_OBJ = build/home_cover.o

DLL_SRC = \
	src/entry.cpp \
	src/Settings.cpp \
	src/AddonPaths.cpp \
	src/Sites.cpp \
	src/BrowserTabs.cpp \
	src/HomePage.cpp \
	src/RaidFood.cpp \
	src/CheatSheets.cpp \
	src/HelperQuickAccess.cpp \
	src/WikiBrowser.cpp \
	src/UI.cpp \
	deps/imgui/imgui.cpp \
	deps/imgui/imgui_draw.cpp \
	deps/imgui/imgui_tables.cpp \
	deps/imgui/imgui_widgets.cpp

DLL_OBJ = $(patsubst %.cpp,build/%.o,$(DLL_SRC))
DLL_OUT = build/bin/GW2-InGame-Helper.dll

GW2_ROOT   ?= $(HOME)/.local/share/Steam/steamapps/common/Guild Wars 2
GW2_ADDONS ?= $(GW2_ROOT)/addons
INSTALL_DLL = $(GW2_ADDONS)/GW2-InGame-Helper.dll
INSTALL_DIR = $(GW2_ADDONS)/GW2-InGame-Helper

.PHONY: all clean install

all: $(DLL_OUT)

$(HELPER_OUT): $(HELPER_SRC) src/WikiIpc.h src/helper/CssCompat.h src/helper/CssProxy.h src/helper/BootJs.h
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

$(DLL_OUT): $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS_DLL) -o $@ $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(HOME_LOGO_OBJ) $(HOME_COVER_OBJ) $(LIBS_DLL)
	@echo "Built $@ (CEF helper + homepage assets embedded)"

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install: $(DLL_OUT)
	@mkdir -p "$(INSTALL_DIR)"
	/bin/cp -f "$(DLL_OUT)" "$(INSTALL_DLL)"
	/bin/rm -f "$(INSTALL_DIR)/GW2-InGame-Helper.dll" \
		"$(INSTALL_DIR)/GW2HelperBrowser.exe" \
		"$(GW2_ADDONS)/GW2HelperBrowser.exe" \
		"$(GW2_ADDONS)/helper-home.html" \
		"$(GW2_ADDONS)/helper-home.ver" \
		"$(GW2_ADDONS)/home-logo.png" \
		"$(GW2_ADDONS)/home-cover.jpg" \
		"$(GW2_ADDONS)/raid-food.html" \
		"$(GW2_ADDONS)/raid-food.ver" \
		"$(GW2_ADDONS)/raid-utilities.html" \
		"$(GW2_ADDONS)/raid-utilities.ver" \
		"$(GW2_ADDONS)/fractal-consumables.html" \
		"$(GW2_ADDONS)/fractal-consumables.ver" \
		"$(GW2_ADDONS)/sigils-runes.html" \
		"$(GW2_ADDONS)/sigils-runes.ver" \
		"$(GW2_ADDONS)/relics-guide.html" \
		"$(GW2_ADDONS)/relics-guide.ver" \
		"$(GW2_ADDONS)/boon-checklist.html" \
		"$(GW2_ADDONS)/boon-checklist.ver" \
		"$(GW2_ADDONS)/cc-defiance.html" \
		"$(GW2_ADDONS)/cc-defiance.ver" \
		"$(GW2_ADDONS)/raid-wings.html" \
		"$(GW2_ADDONS)/raid-wings.ver" \
		"$(GW2_ADDONS)/home-garden.html" \
		"$(GW2_ADDONS)/home-garden.ver" \
		"$(GW2_ADDONS)/ubers-all-in-one.html" \
		"$(GW2_ADDONS)/ubers-all-in-one.ver" \
		"$(GW2_ADDONS)/settings.ini" \
		"$(GW2_ROOT)/bin64/cef/GW2HelperBrowser.exe"
	/bin/rm -rf "$(INSTALL_DIR)/cef-cache"
	@echo "Installed DLL -> $(INSTALL_DLL)"
	@echo "Data folder   -> $(INSTALL_DIR)/ (created; runtime extracts here)"
	@ls -lh "$(INSTALL_DLL)"

clean:
	rm -rf build
