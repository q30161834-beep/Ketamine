#include "include/lexer.h"
#include "include/types.h"
#include "include/arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ─── Test Framework ───────────────────────────────────────────────────────────
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", name); \
    fflush(stdout);

#define END_TEST(pass) do { \
    if (pass) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL\n"); tests_failed++; } \
} while(0)

// ─── Mock Context ─────────────────────────────────────────────────────────────
static Context *create_test_ctx(void) {
    Arena *a = malloc(sizeof(Arena));
    *a = arena_new_size(16 * 1024 * 1024);

    Context *ctx = arena_alloc_zero(a, sizeof(Context));
    ctx->arena = a;

    ctx->intern_capacity = 1024;
    ctx->intern_table = arena_alloc_zero(a, sizeof(InternStr*) * 1024);

    ctx->types = *ket_type_table_init(a);
    return ctx;
}

static void destroy_test_ctx(Context *ctx) {
    arena_free(ctx->arena);
    free(ctx->arena);
    free(ctx);
}

// ─── Tests ────────────────────────────────────────────────────────────────────

static void test_basic_tokens(void) {
    TEST("basic integer literal");

    const char *src = "42";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_INT_LIT && t.ival == 42);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_float_literal(void) {
    TEST("float literal");

    const char *src = "3.14";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_FLOAT_LIT && t.fval > 3.13 && t.fval < 3.15);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_string_literal(void) {
    TEST("string literal");

    const char *src = "\"hello world\"";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_STR_LIT);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_hex_literal(void) {
    TEST("hex literal");

    const char *src = "0xFF";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_INT_LIT && t.ival == 255);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_binary_literal(void) {
    TEST("binary literal");

    const char *src = "0b1010";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_INT_LIT && t.ival == 10);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_identifier(void) {
    TEST("identifier");

    const char *src = "hello_world";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_IDENT && strncmp(t.start, "hello_world", 11) == 0);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_keyword(void) {
    TEST("keyword 'fn'");

    const char *src = "fn";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == KW_FN);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_operators(void) {
    TEST("comparison operators");

    const char *src = "== != < > <= >=";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    TokenType expected[] = {TK_EQEQ, TK_BANGEQ, TK_LT, TK_GT, TK_LTEQ, TK_GTEQ};
    bool pass = true;

    for (int i = 0; i < 6; i++) {
        Token t = lexer_next(&l);
        if (t.type != expected[i]) {
            pass = false;
            printf("  expected %d at pos %d, got %d\n", expected[i], i, t.type);
            break;
        }
    }

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_comment_skip(void) {
    TEST("line comment skipping");

    const char *src = "// this is a comment\n42";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_INT_LIT && t.ival == 42);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_block_comment(void) {
    TEST("block comment skipping");

    const char *src = "/* comment */ 42";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_INT_LIT && t.ival == 42);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_nested_block_comment(void) {
    TEST("nested block comment");

    const char *src = "/* outer /* inner */ */ 42";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    bool pass = (t.type == TK_INT_LIT && t.ival == 42);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_multiple_tokens(void) {
    TEST("multiple tokens");

    const char *src = "let x: int = 42";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    TokenType expected[] = {KW_LET, TK_IDENT, TK_COLON, TK_TYPE_INT, TK_EQ, TK_INT_LIT};
    bool pass = true;

    for (int i = 0; i < 6; i++) {
        Token t = lexer_next(&l);
        if (t.type != expected[i]) {
            pass = false;
            break;
        }
    }

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

static void test_emoji_ident(void) {
    TEST("unicode emoji identifier");

    const char *src = "hello_🔥_world";
    Context *ctx = create_test_ctx();
    Lexer l;
    lexer_init(&l, "test.kt", src, (int)strlen(src), ctx);

    Token t = lexer_next(&l);
    // Should handle as identifier
    bool pass = (t.type == TK_IDENT);

    destroy_test_ctx(ctx);
    END_TEST(pass);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(void) {
    printf("─── Lexer Test Suite ───\n\n");

    test_basic_tokens();
    test_float_literal();
    test_string_literal();
    test_hex_literal();
    test_binary_literal();
    test_identifier();
    test_keyword();
    test_operators();
    test_comment_skip();
    test_block_comment();
    test_nested_block_comment();
    test_multiple_tokens();
    test_emoji_ident();

    printf("\n─── Results: %d passed, %d failed ───\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
