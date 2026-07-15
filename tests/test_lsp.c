// ═══════════════════════════════════════════════════════════════════════════════
// LSP TESTS — Language Server Protocol functionality
// ═══════════════════════════════════════════════════════════════════════════════

#include "../src/include/lsp.h"
#include "../src/include/arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define TEST(name) do { printf("  TEST: %s ... ", name); fflush(stdout);
#define END_TEST(pass) do { \
    if (pass) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL\n"); tests_failed++; } \
} while(0)

static int tests_passed = 0, tests_failed = 0;

static void test_lsp_server_create(void) {
    TEST("create LSP server");
    LspServer *s = lsp_server_new();
    END_TEST(s != NULL);
    lsp_server_free(s);
}

static void test_did_open(void) {
    TEST("didOpen document");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt", "fn main() { print(42); }", 1);
    END_TEST(s->document_count == 1);
    lsp_server_free(s);
}

static void test_did_change(void) {
    TEST("didChange document");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt", "fn main() { print(42); }", 1);
    lsp_did_change(s, "file:///test.kt", "fn main() { print(100); }", 2);
    bool pass = (s->document_count == 1);
    END_TEST(pass);
    lsp_server_free(s);
}

static void test_did_close(void) {
    TEST("didClose document");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt", "fn main() { }", 1);
    lsp_did_close(s, "file:///test.kt");
    END_TEST(s->document_count == 0);
    lsp_server_free(s);
}

static void test_completion(void) {
    TEST("completion — returns items");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt", "fn main() { }", 1);
    int count = 0;
    CompletionItem *items = lsp_complete(s, "file:///test.kt", (LspPosition){0, 0}, &count);
    bool pass = (count > 0);
    free(items);
    END_TEST(pass);
    lsp_server_free(s);
}

static void test_hover_keyword(void) {
    TEST("hover on keyword");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt", "fn main() { }", 1);
    const char *h = lsp_hover(s, "file:///test.kt", (LspPosition){0, 0});  // "fn"
    // Should get hover info for "fn"
    bool pass = (h != NULL);
    free((void*)h);
    END_TEST(pass);
    lsp_server_free(s);
}

static void test_goto_definition(void) {
    TEST("go-to-definition");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt",
        "fn add(a: int, b: int) -> int { return a + b; }\nfn main() { let x = add(1, 2); }", 1);
    int count = 0;
    LspLocation *loc = lsp_goto_definition(s, "file:///test.kt",
        (LspPosition){1, 16}, &count);  // position of "add" in main
    bool pass = (count > 0);
    free(loc);
    END_TEST(pass);
    lsp_server_free(s);
}

static void test_document_symbols(void) {
    TEST("document symbols");
    LspServer *s = lsp_server_new();
    lsp_did_open(s, "file:///test.kt",
        "fn main() { }\nfn helper() { }\nstruct Point { x: int, y: int }\nenum Option { None, Some(int) }", 1);
    int count = 0;
    SymbolInfo *syms = lsp_document_symbols(s, "file:///test.kt", &count);
    bool pass = (count == 4);
    free(syms);
    END_TEST(pass);
    lsp_server_free(s);
}

int main(void) {
    printf("─── LSP Tests ───\n\n");
    test_lsp_server_create();
    test_did_open();
    test_did_change();
    test_did_close();
    test_completion();
    test_hover_keyword();
    test_goto_definition();
    test_document_symbols();
    printf("\n─── Results: %d passed, %d failed ───\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
