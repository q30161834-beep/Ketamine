#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Test Examples Script — Verifies all examples compile without errors
# ═══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

KETC="${1:-./build/ketc}"
EXAMPLES_DIR="${2:-./examples}"
PASS=0
FAIL=0
TOTAL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "╔══════════════════════════════════════════════════╗"
echo "║      Testing Ketamine Examples                  ║"
echo "╚══════════════════════════════════════════════════╝"
echo "Compiler: $KETC"
echo "Examples: $EXAMPLES_DIR"
echo ""

for f in "$EXAMPLES_DIR"/[0-9]*.kt; do
    TOTAL=$((TOTAL + 1))
    name=$(basename "$f")
    echo -n "  $name ... "

    if $KETC --parse "$f" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "─── Results ───"
echo "  Total:  $TOTAL"
echo -e "  Passed: ${GREEN}$PASS${NC}"
echo -e "  Failed: ${RED}$FAIL${NC}"

# Test complex examples too
echo ""
echo "─── Complex Examples ───"
for f in "$EXAMPLES_DIR"/complex_*.kt "$EXAMPLES_DIR"/raytracer.kt "$EXAMPLES_DIR"/mandelbrot.kt; do
    if [ -f "$f" ]; then
        TOTAL=$((TOTAL + 1))
        name=$(basename "$f")
        echo -n "  $name ... "

        if $KETC --parse "$f" > /dev/null 2>&1; then
            echo -e "${GREEN}PASS${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "${RED}FAIL${NC}"
            FAIL=$((FAIL + 1))
        fi
    fi
done

echo ""
echo "─── Final ───"
if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All $TOTAL examples pass!${NC}"
else
    echo -e "${RED}$FAIL/$TOTAL examples failed${NC}"
fi

exit $FAIL
