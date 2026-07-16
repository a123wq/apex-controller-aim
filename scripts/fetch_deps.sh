#!/bin/bash
# =============================================================================
# Apex Stick Trainer V1.0 — Dependency Fetcher
# =============================================================================
# Downloads all third-party dependencies to deps/ directory.
# Run once before first build.
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DEPS_DIR="$PROJECT_DIR/deps"

echo "=== Apex Stick Trainer: Fetching Dependencies ==="
echo "Project dir: $PROJECT_DIR"
echo ""

# Create deps directory
mkdir -p "$DEPS_DIR"

# -----------------------------------------------------------------------------
# SDL2 — Window, OpenGL context, event loop
# -----------------------------------------------------------------------------
if [ ! -d "$DEPS_DIR/SDL2" ]; then
    echo "[1/5] Downloading SDL2..."
    SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-devel-2.30.3-mingw.zip"
    SDL2_ZIP="/tmp/SDL2-devel-2.30.3-mingw.zip"
    curl -L -o "$SDL2_ZIP" "$SDL2_URL"
    unzip -q "$SDL2_ZIP" -d "$DEPS_DIR"
    # SDL extracts into SDL2-x.x.x/ — move contents up
    mv "$DEPS_DIR/SDL2-"* "$DEPS_DIR/SDL2_tmp"
    mv "$DEPS_DIR/SDL2_tmp"/* "$DEPS_DIR/SDL2/"
    rm -rf "$DEPS_DIR/SDL2_tmp"
    rm "$SDL2_ZIP"
    echo "      SDL2 installed: $DEPS_DIR/SDL2"
else
    echo "[1/5] SDL2 already present (skip)"
fi

# -----------------------------------------------------------------------------
# GLM — Header-only math library
# -----------------------------------------------------------------------------
if [ ! -d "$DEPS_DIR/glm" ]; then
    echo "[2/5] Downloading GLM..."
    git clone --depth 1 https://github.com/g-truc/glm.git "$DEPS_DIR/glm"
    echo "      GLM installed: $DEPS_DIR/glm"
else
    echo "[2/5] GLM already present (skip)"
fi

# -----------------------------------------------------------------------------
# nlohmann/json — Single-header JSON library
# -----------------------------------------------------------------------------
if [ ! -f "$DEPS_DIR/json.hpp" ]; then
    echo "[3/5] Downloading nlohmann/json..."
    curl -L -o "$DEPS_DIR/json.hpp" \
        "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp"
    echo "      nlohmann/json installed: $DEPS_DIR/json.hpp"
else
    echo "[3/5] nlohmann/json already present (skip)"
fi

# -----------------------------------------------------------------------------
# Dear ImGui — Immediate mode UI
# -----------------------------------------------------------------------------
if [ ! -d "$DEPS_DIR/imgui" ]; then
    echo "[4/5] Downloading Dear ImGui..."
    git clone --depth 1 https://github.com/ocornut/imgui.git "$DEPS_DIR/imgui"
    echo "      ImGui installed: $DEPS_DIR/imgui"
else
    echo "[4/5] Dear ImGui already present (skip)"
fi

# -----------------------------------------------------------------------------
# glad — OpenGL loader (single header, GL 3.3 Core)
# -----------------------------------------------------------------------------
if [ ! -f "$DEPS_DIR/glad.h" ]; then
    echo "[5/5] Downloading glad (OpenGL loader)..."
    curl -L -o "$DEPS_DIR/glad.h" \
        "https://raw.githubusercontent.com/Dav1dde/glad/main/glad/src/glad.h"
    curl -L -o "$DEPS_DIR/khrplatform.h" \
        "https://raw.githubusercontent.com/Dav1dde/glad/main/glad/include/khrplatform.h"
    echo "      glad installed: $DEPS_DIR/glad.h"
else
    echo "[5/5] glad already present (skip)"
fi

echo ""
echo "=== All dependencies fetched successfully ==="
echo ""
echo "Directory layout:"
find "$DEPS_DIR" -maxdepth 1 -type d | sort