#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Run All Tests — Complete test suite for Ketamine compiler
# ═══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

BUILD_DIR="${1:-./build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
TOTAL=0

run_test_suite() {
    local name="$1"
    local binary="$2"
    shift 2

    TOTAL=$((TOTAL + 1))
    echo -e "\n${CYAN}━━━ $name ━━━${NC}"
    
    if [ -x "$binary" ]; then
        if $binary "$@" 2>&1; then
            echo -e "${GREEN}✓ $name passed${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "${RED}✗ $name failed${NC}"
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "${YELLOW}⚠ $name not built (skipping)${NC}"
    fi
}

echo "╔══════════════════════════════════════════════════╗"
echo "║      Ketamine Compiler — Full Test Suite         ║"
echo "╚══════════════════════════════════════════════════╝"
echo "Build dir: $BUILD_DIR"
echo "Project:   $PROJECT_DIR"
echo "Date:      $(date)"
echo ""

# Phase 1: Compile the compiler
echo -e "${YELLOW}Building compiler...${NC}"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DKET_BUILD_TESTS=ON 2>&1 | tail -5
cmake --build "$BUILD_DIR" --parallel 2>&1 | tail -5
echo ""

# Phase 2: Run all test suites
run_test_suite "Lexer Tests"       "$BUILD_DIR/test_lexer"
run_test_suite "Parser Tests"      "$BUILD_DIR/test_parser"
run_test_suite "Type Checker Tests" "$BUILD_DIR/test_typecheck"
run_test_suite "Borrow Checker Tests" "$BUILD_DIR/test_borrowcheck"
run_test_suite "Optimizer Tests"   "$BUILD_DIR/test_optimizer"
run_test_suite "Codegen Tests"     "$BUILD_DIR/test_codegen"
run_test_suite "LSP Tests"         "$BUILD_DIR/test_lsp"
run_test_suite "Package Tests"     "$BUILD_DIR/test_package"
run_test_suite "Integration Tests" "$BUILD_DIR/test_integration"
run_test_suite "Test Runner"       "$BUILD_DIR/test_runner" "$PROJECT_DIR"

# Phase 3: Test examples
echo -e "\n${CYAN}━━━ Testing Examples ───${NC}"
if "$PROJECT_DIR/scripts/test_examples.sh" "$BUILD_DIR/ketc" "$PROJECT_DIR/examples"; then
    echo -e "${GREEN}✓ Examples passed${NC}"
else
    echo -e "${RED}✗ Examples failed${NC}"
fi

# Summary
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║                  Results                         ║"
echo "╚══════════════════════════════════════════════════╝"
echo "  Test suites: $TOTAL total"
echo -e "  ${GREEN}Passed: $PASS${NC}"
echo -e "  ${RED}Failed: $FAIL${NC}"
echo ""

if [ $FAIL -gt 0 ]; then
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
fi
