# =============================================================================
# Apex Stick Trainer V1.0 — Makefile (MinGW GCC)
# =============================================================================
# Build:   mingw32-make
# Clean:   mingw32-make clean
# Run:     ./ApexStickTrainer.exe
# =============================================================================

# ---------- Compiler ----------
CXX      := g++
CXX_STD  := -std=c++20
WARNINGS := -Wall -Wextra -Wpedantic -Wconversion
OPTIMIZE := -O2

# ---------- Directories ----------
SRC_DIR   := src
DEPS_DIR  := deps
INC_DIRS  := -I$(SRC_DIR) -I$(DEPS_DIR) \
             -I$(DEPS_DIR)/glm \
             -I$(DEPS_DIR)/imgui \
             -I$(DEPS_DIR)/imgui/backends \
             -I$(DEPS_DIR)/SDL2/include \
             -I$(DEPS_DIR)

# ---------- SDL2 ----------
SDL2_DIR  := $(DEPS_DIR)/SDL2
SDL2_LIB  := $(SDL2_DIR)/lib
# Fully STATIC linking: libSDL2.a (not the dll import lib) + every Windows
# system lib SDL2's static code references. This is what makes the exe
# distributable with ZERO non-system DLL dependencies (no SDL2.dll needed).
#   oleaut32 -> SysFreeString (IME/COM in SDL_windowskeyboard)
#   advapi32 -> registry / crypto
#   dinput8  -> DirectInput (joystick)
#   setupapi -> device enumeration
LDFLAGS_SDL := -L$(SDL2_LIB) \
               -lSDL2main -lSDL2 \
               -luser32 -lgdi32 -lwinmm -ldinput8 -limm32 \
               -lshell32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 \
               -lsetupapi

# ---------- OpenGL + XInput ----------
# XInput is NOT linked here anymore — Input.cpp loads it dynamically via
# LoadLibraryA("XINPUT1_4.dll"/"XINPUT1_3.dll"/"XINPUT9_1_0.dll"). That means a
# clean Windows install with no legacy DirectX runtime still launches the app;
# the controller path simply reports "not connected" and the mouse keeps working.
LDFLAGS_GL  := -lopengl32

# Order: SDL2main first, then SDL2
LIBS := $(LDFLAGS_SDL) $(LDFLAGS_GL)

# Fully static GCC/C++ runtime + winpthread so the exe needs no libgcc_s_seh-1.dll,
# libstdc++-6.dll, or libwinpthread-1.dll. Combined with static SDL2 above, the
# only remaining DLL deps are Windows system DLLs (KERNEL32/USER32/msvcrt/...).
STATIC_RUNTIME := -static-libgcc -static-libstdc++ -static -lpthread

# ---------- Source files ----------
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)

# Object files
OBJ_DIR   := obj
APP_OBJ   := $(SRC_FILES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# ---------- ImGui core + backends ----------
IMGUI_OBJ := $(OBJ_DIR)/imgui_core.o \
             $(OBJ_DIR)/imgui_draw.o \
             $(OBJ_DIR)/imgui_widgets.o \
             $(OBJ_DIR)/imgui_tables.o \
             $(OBJ_DIR)/imgui_impl_sdl2.o \
             $(OBJ_DIR)/imgui_impl_opengl3.o

BACKEND_OBJ := $(IMGUI_OBJ)

# Auto-generated header dependencies (-MMD -MP). Without this, editing App.hpp
# (which defines EVERY struct/class) only recompiles the .cpp whose own mtime
# changed, leaving stale .o files compiled against the OLD struct layout. Those
# stale+fresh .o get linked together → ABI/ODR violation → memory corruption
# (the heisenbug-class "camera overlaps target / can't move" regressions).
# -MMD emits a <file>.d listing included headers; -include pulls them in so
# `make` knows App.o/Trainer.o/etc. depend on App.hpp and rebuilds them all.
DEPS := $(APP_OBJ:.o=.d) $(IMGUI_OBJ:.o=.d)
-include $(DEPS)

# Force the default goal to `all`. The -include above reads obj/App.d first,
# whose first line (`obj/App.o: ...`) would otherwise become .DEFAULT_GOAL —
# making bare `make` build ONLY App.o and skip every other object (so editing
# App.hpp "recompiled" just App.cpp and left the rest stale). Set this AFTER
# the include so `all` wins.
.DEFAULT_GOAL := all

TARGET    := ApexStickTrainer.exe
LDFLAGS   := $(STATIC_RUNTIME)

# =============================================================================
# Rules
# =============================================================================

.PHONY: all clean run setup deps-check

all: deps-check $(TARGET)

$(TARGET): $(APP_OBJ) $(BACKEND_OBJ)
	@echo "Linking $@ (static)"
	$(CXX) $^ -o $@ $(LIBS) $(LDFLAGS)

# Pattern rule for app source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "Compiling $<"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

# ImGui core rules
$(OBJ_DIR)/imgui_core.o: $(DEPS_DIR)/imgui/imgui.cpp | $(OBJ_DIR)
	@echo "Compiling imgui/imgui.cpp"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/imgui_draw.o: $(DEPS_DIR)/imgui/imgui_draw.cpp | $(OBJ_DIR)
	@echo "Compiling imgui/imgui_draw.cpp"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/imgui_widgets.o: $(DEPS_DIR)/imgui/imgui_widgets.cpp | $(OBJ_DIR)
	@echo "Compiling imgui/imgui_widgets.cpp"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/imgui_tables.o: $(DEPS_DIR)/imgui/imgui_tables.cpp | $(OBJ_DIR)
	@echo "Compiling imgui/imgui_tables.cpp"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

# ImGui backend rules
$(OBJ_DIR)/imgui_impl_sdl2.o: $(DEPS_DIR)/imgui/backends/imgui_impl_sdl2.cpp | $(OBJ_DIR)
	@echo "Compiling imgui_impl_sdl2.cpp"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/imgui_impl_opengl3.o: $(DEPS_DIR)/imgui/backends/imgui_impl_opengl3.cpp | $(OBJ_DIR)
	@echo "Compiling imgui_impl_opengl3.cpp"
	$(CXX) $(CXX_STD) $(WARNINGS) $(OPTIMIZE) $(INC_DIRS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

deps-check:
	@if [ ! -d "$(SDL2_DIR)" ]; then \
		echo "ERROR: SDL2 not found at $(SDL2_DIR)"; \
		echo "Run: bash scripts/fetch_deps.sh"; \
		exit 1; \
	fi
	@if [ ! -d "$(DEPS_DIR)/imgui/backends" ]; then \
		echo "ERROR: ImGui not found at $(DEPS_DIR)/imgui"; \
		echo "Run: bash scripts/fetch_deps.sh"; \
		exit 1; \
	fi
	@if [ ! -f "$(SDL2_LIB)/libSDL2.a" ]; then \
		echo "WARNING: libSDL2.a not found in $(SDL2_LIB)"; \
	fi

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TARGET:.exe=_debug.exe)

setup:
	bash scripts/fetch_deps.sh

build: setup $(TARGET)

# Debug
debug: CXX_STD = -std=c++20
debug: WARNINGS = -Wall -Wextra
debug: OPTIMIZE = -O0 -g
debug: TARGET  := ApexStickTrainer_debug.exe
debug: all
	@mv $(OBJ_DIR)/*.o $(TARGET) . 2>/dev/null; true