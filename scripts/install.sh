#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Ketamine Compiler — One-Command Installer
# ═══════════════════════════════════════════════════════════════════════════════
# Usage: curl -fsSL https://raw.githubusercontent.com/q30161834-beep/Ketamine/main/scripts/install.sh | bash
# ═══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

KET_VERSION="v0.1.0"
INSTALL_DIR="${INSTALL_DIR:-/usr/local}"
BUILD_DIR="$(mktemp -d)"
REPO="https://github.com/q30161834-beep/Ketamine.git"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "╔══════════════════════════════════════════════════╗"
echo "║     Ketamine Compiler Installer v$KET_VERSION        ║"
echo "╚══════════════════════════════════════════════════╝"

# ── Check prerequisites ────────────────────────────────────────────────────────
echo -e "${YELLOW}[1/5]${NC} Checking prerequisites..."

command -v git  >/dev/null 2>&1 || { echo -e "${RED}✗ git required${NC}"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo -e "${RED}✗ cmake required${NC}"; exit 1; }
command -v make  >/dev/null 2>&1 || { echo -e "${RED}✗ make required${NC}"; exit 1; }
command -v cc    >/dev/null 2>&1 || { echo -e "${RED}✗ C compiler required${NC}"; exit 1; }

echo -e "${GREEN}  ✓ git, cmake, make, cc found${NC}"

# ── Clone ──────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}[2/5]${NC} Cloning Ketamine $KET_VERSION..."
git clone --depth 1 --branch "$KET_VERSION" "$REPO" "$BUILD_DIR/ketamine"
echo -e "${GREEN}  ✓ Cloned${NC}"

# ── Build ──────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}[3/5]${NC} Building..."
cmake -B "$BUILD_DIR/ketamine/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR/ketamine/build" --parallel
echo -e "${GREEN}  ✓ Built${NC}"

# ── Install ────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}[4/5]${NC} Installing to $INSTALL_DIR..."
cp "$BUILD_DIR/ketamine/build/ketc" "$INSTALL_DIR/bin/ketc"
cp -r "$BUILD_DIR/ketamine/stdlib" "$INSTALL_DIR/share/ketamine/stdlib"
echo -e "${GREEN}  ✓ Installed${NC}"

# ── Verify ─────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}[5/5]${NC} Verifying..."
if "$INSTALL_DIR/bin/ketc" --help >/dev/null 2>&1; then
    echo -e "${GREEN}  ✓ ketc works!${NC}"
else
    echo -e "${RED}  ✗ Installation may have issues${NC}"
fi

# ── Cleanup ────────────────────────────────────────────────────────────────────
rm -rf "$BUILD_DIR"

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Ketamine $KET_VERSION installed successfully!      ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════╝${NC}"
echo ""
echo "  Try:  ketc --help"
echo "        ketc examples/hello.kt -o hello.ll"
