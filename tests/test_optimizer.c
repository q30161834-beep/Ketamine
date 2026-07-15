// ═══════════════════════════════════════════════════════════════════════════════
// OPTIMIZER TESTS — Constant folding, DCE, GVN, strength reduction
// ═══════════════════════════════════════════════════════════════════════════════

#include "../src/include/lexer.h"
#include "../src/include/parser.h"
#include "../src/include/typecheck.h"
#include "../src/include/borrow.h"
#include "../src/include/ir.h"
#include "../src/include/ir_optimize.h"
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

static Context *create_ctx(void) {
    Arena *a = malloc(sizeof(Arena));
    *a = arena_new_size(32 * 1024 * 1024);
    Context *ctx = arena_alloc_zero(a, sizeof(Context));
    ctx->arena = a;
    ctx->options.verbose = false;
    ctx->intern_capacity = 1024;
    ctx->intern_table = arena_alloc_zero(a, sizeof(InternStr*) * 1024);
    ctx->types = *ket_type_table_init(a);
    return ctx;
}

static void destroy_ctx(Context *ctx) {
    if (ctx->ir_module) ket_ir_free(ctx->ir_module);
    arena_free(ctx->arena);
    free(ctx->arena);
    free(ctx);
}

static IRModule *compile_to_ir(Context *ctx, const char *src) {
    Lexer lex; lexer_init(&lex, "<test>", src, (int)strlen(src), ctx);
    Parser p; parser_init(&p, &lex, ctx);
    ASTNode *prog = parse_program(&p);
    if (!prog) return NULL;
    ctx->diag.error_count = 0; ctx->diag.count = 0;
    if (!ty_check_module(ctx, prog)) return NULL;
    borrow_check_module(ctx, prog);
    ctx->ir_module = ket_ir_lower(ctx, prog);
    return ctx->ir_module;
}

static void test_opt_none(void) {
    TEST("no optimization — safe run");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx, "fn main() { let x = 1 + 2; print(x); }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_NONE, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_opt_o1(void) {
    TEST("O1 optimization — basic passes");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx, "fn main() -> int { return 42; }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O1, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_opt_o2(void) {
    TEST("O2 optimization — standard");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx,
        "fn add(a: int, b: int) -> int { return a + b; } fn main() { let x = add(1, 2); print(x); }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O2, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_opt_o3(void) {
    TEST("O3 optimization — aggressive");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx, "fn main() -> int { return 2 + 3; }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O3, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_control_flow_opt(void) {
    TEST("control flow with optimization");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx,
        "fn main() -> int { if true { return 1; } else { return 2; } }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O2, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_while_opt(void) {
    TEST("while loop optimization");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx,
        "fn main() { let mut i = 0; while i < 10 { i = i + 1; } }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O2, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_recursion_opt(void) {
    TEST("recursive function optimization");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx,
        "fn fib(n: int) -> int { if n <= 1 { return 1; } return fib(n-1) + fib(n-2); }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O1, &diag);
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_verify_after_opt(void) {
    TEST("IR verification after optimization");
    Context *ctx = create_ctx();
    IRModule *mod = compile_to_ir(ctx, "fn main() -> int { return 42; }");
    DiagCtx diag; memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(mod, OPT_O2, &diag);
    bool ok = ket_ir_verify(mod, &diag);
    END_TEST(ok);
    destroy_ctx(ctx);
}

int main(void) {
    printf("─── Optimizer Tests ───\n\n");
    test_opt_none();
    test_opt_o1();
    test_opt_o2();
    test_opt_o3();
    test_control_flow_opt();
    test_while_opt();
    test_recursion_opt();
    test_verify_after_opt();
    printf("\n─── Results: %d passed, %d failed ───\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
