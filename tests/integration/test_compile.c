// ═══════════════════════════════════════════════════════════════════════════════
// INTEGRATION TESTS — Complete programs that compile end-to-end
// ═══════════════════════════════════════════════════════════════════════════════

#include "../../src/include/lexer.h"
#include "../../src/include/parser.h"
#include "../../src/include/typecheck.h"
#include "../../src/include/borrow.h"
#include "../../src/include/ir.h"
#include "../../src/include/codegen.h"
#include "../../src/include/arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST(name) do { printf("  TEST: %s ... ", name); fflush(stdout);
#define END_TEST(pass) do { \
    if (pass) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL\n"); tests_failed++; } \
} while(0)

static int tests_passed = 0, tests_failed = 0;

static Context *create_ctx(void) {
    Arena *a = malloc(sizeof(Arena));
    *a = arena_new_size(64 * 1024 * 1024);
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

static IRModule *full_compile(Context *ctx, const char *src) {
    Lexer lex; lexer_init(&lex, "<test>", src, (int)strlen(src), ctx);
    Parser p; parser_init(&p, &lex, ctx);
    ASTNode *prog = parse_program(&p);
    if (!prog) return NULL;
    ctx->diag.error_count = 0; ctx->diag.count = 0;
    if (!ty_check_module(ctx, prog)) return NULL;
    if (!borrow_check_module(ctx, prog)) return NULL;
    ctx->ir_module = ket_ir_lower(ctx, prog);
    return ctx->ir_module;
}

static void test_empty_main(void) {
    TEST("empty main function");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() { }");
    END_TEST(mod != NULL && mod->func_count > 0);
    destroy_ctx(ctx);
}

static void test_return_int(void) {
    TEST("return int literal");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() -> int { return 42; }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_call_print(void) {
    TEST("call print function");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() { print(\"hello\"); }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_arithmetic_chain(void) {
    TEST("arithmetic chain");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx,
        "fn main() -> int { let x = 1 + 2 * 3 - 4 / 2; return x; }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_if_else_return(void) {
    TEST("if-else with return");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx,
        "fn max(a: int, b: int) -> int { if a > b { return a; } else { return b; } }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_while_loop(void) {
    TEST("while loop");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx,
        "fn main() -> int { let mut i = 0; while i < 5 { i = i + 1; } return i; }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_for_loop(void) {
    TEST("for loop");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx,
        "fn main() -> int { let mut sum = 0; for i in 0..10 { sum = sum + i; } return sum; }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_recursive_fib(void) {
    TEST("recursive fibonacci");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx,
        "fn fib(n: int) -> int { if n <= 1 { return 1; } return fib(n-1) + fib(n-2); } fn main() -> int { return fib(5); }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_multiple_functions(void) {
    TEST("multiple functions");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx,
        "fn add(a: int, b: int) -> int { return a + b; }\n"
        "fn sub(a: int, b: int) -> int { return a - b; }\n"
        "fn main() -> int { return add(sub(10, 2), 3); }");
    END_TEST(mod != NULL);
    destroy_ctx(ctx);
}

static void test_llvm_codegen(void) {
    TEST("LLVM codegen output");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() -> int { return 42; }");
    CompileOptions opts; memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(mod, "test_int_out.ll", &opts);
    remove("test_int_out.ll");
    END_TEST(result == 0);
    destroy_ctx(ctx);
}

static void test_go_codegen(void) {
    TEST("Go codegen output");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() { print(\"hello\"); }");
    CompileOptions opts; memset(&opts, 0, sizeof(opts));
    int result = codegen_go_module(mod, "test_int_out.go", &opts);
    remove("test_int_out.go");
    END_TEST(result == 0);
    destroy_ctx(ctx);
}

static void test_python_codegen(void) {
    TEST("Python codegen output");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() -> int { return 42; }");
    CompileOptions opts; memset(&opts, 0, sizeof(opts));
    int result = codegen_python_module(mod, "test_int_out.py", &opts);
    remove("test_int_out.py");
    END_TEST(result == 0);
    destroy_ctx(ctx);
}

static void test_wasm_codegen(void) {
    TEST("WASM codegen output");
    Context *ctx = create_ctx();
    IRModule *mod = full_compile(ctx, "fn main() -> int { return 42; }");
    CompileOptions opts; memset(&opts, 0, sizeof(opts));
    int result = codegen_wasm_module(mod, "test_int_out.wat", &opts);
    remove("test_int_out.wat");
    END_TEST(result == 0);
    destroy_ctx(ctx);
}

int main(void) {
    printf("─── Integration Tests ───\n\n");
    test_empty_main();
    test_return_int();
    test_call_print();
    test_arithmetic_chain();
    test_if_else_return();
    test_while_loop();
    test_for_loop();
    test_recursive_fib();
    test_multiple_functions();
    test_llvm_codegen();
    test_go_codegen();
    test_python_codegen();
    test_wasm_codegen();
    printf("\n─── Results: %d passed, %d failed ───\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
