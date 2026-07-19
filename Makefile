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

DLL_SRC = \
	src/entry.cpp \
	src/Settings.cpp \
	src/Sites.cpp \
	src/HomePage.cpp \
	src/HelperQuickAccess.cpp \
	src/WikiBrowser.cpp \
	src/ItemLookup.cpp \
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

$(DLL_OUT): $(DLL_OBJ) $(HELPER_BLOB_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS_DLL) -o $@ $(DLL_OBJ) $(HELPER_BLOB_OBJ) $(LIBS_DLL)
	@echo "Built $@ (CEF helper embedded)"

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install: $(DLL_OUT)
	@mkdir -p "$(INSTALL_DIR)"
	/bin/cp -f "$(DLL_OUT)" "$(INSTALL_DLL)"
	/bin/cp -f "$(DLL_OUT)" "$(INSTALL_DIR)/GW2-InGame-Helper.dll"
	/bin/rm -f "$(INSTALL_DIR)/GW2HelperBrowser.exe" \
		"$(GW2_ADDONS)/GW2HelperBrowser.exe" \
		"$(GW2_ROOT)/bin64/cef/GW2HelperBrowser.exe"
	/bin/rm -rf "$(INSTALL_DIR)/cef-cache"
	@echo "Installed -> $(INSTALL_DLL)"
	@ls -lh "$(INSTALL_DLL)"

clean:
	rm -rf build
