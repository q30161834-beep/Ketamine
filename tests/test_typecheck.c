// ═══════════════════════════════════════════════════════════════════════════════
// TYPE CHECKER TESTS — Integration tests for the Hindley-Milner type checker
// ═══════════════════════════════════════════════════════════════════════════════

#include "../src/include/lexer.h"
#include "../src/include/parser.h"
#include "../src/include/typecheck.h"
#include "../src/include/arena.h"
#include "../src/include/diagnostics.h"
#include "../src/include/symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

static Arena *test_arena;
static Context *test_ctx;

static void test_init(void) {
    test_arena = (Arena*)malloc(sizeof(Arena));
    *test_arena = arena_new_size(32 * 1024 * 1024);
    test_ctx = (Context*)arena_alloc_zero(test_arena, sizeof(Context));
    test_ctx->arena = test_arena;
    test_ctx->options.verbose = false;
    test_ctx->diag.count = 0;
    test_ctx->diag.error_count = 0;

    test_ctx->intern_capacity = 512;
    test_ctx->intern_table = (InternStr**)arena_alloc_zero(test_arena,
        sizeof(InternStr*) * 512);
    test_ctx->intern_count = 0;

    test_ctx->types = *ket_type_table_init(test_arena);
}

static void test_cleanup(void) {
    arena_free(test_arena);
    free(test_arena);
}

static ASTNode *parse_and_typecheck(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, "<test>", source, (int)strlen(source), test_ctx);

    Parser parser;
    parser_init(&parser, &lexer, test_ctx);

    ASTNode *prog = parse_program(&parser);
    if (!prog) return NULL;

    // Reset diagnostics before type checking
    test_ctx->diag.count = 0;
    test_ctx->diag.error_count = 0;

    if (!ty_check_module(test_ctx, prog)) {
        return NULL;
    }

    return prog;
}

static void test_infer_int_literal(void) {
    const char *src = "fn main() { let x = 42; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_infer_float_literal(void) {
    const char *src = "fn main() { let x = 3.14; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_infer_bool_literal(void) {
    const char *src = "fn main() { let a = true; let b = false; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_infer_string_literal(void) {
    const char *src = "fn main() { let s = \"hello\"; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_binary_addition(void) {
    const char *src = "fn main() { let x = 1 + 2; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_binary_all_ops(void) {
    const char *src = "fn main() { let a = 1 + 2; let b = 3 - 4; let c = 5 * 6; let d = 8 / 2; let e = 10 % 3; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_comparison_ops(void) {
    const char *src = "fn main() { let a = 1 == 2; let b = 3 != 4; let c = 5 < 6; let d = 7 > 8; let e = 9 <= 10; let f = 11 >= 12; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_if_expression(void) {
    const char *src = "fn main() { let x = if true { 1 } else { 2 }; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_block_expression(void) {
    const char *src = "fn main() { let x = { let y = 10; y + 5 }; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_function_call(void) {
    const char *src = "fn add(a: int, b: int) -> int { return a + b; } fn main() { let x = add(1, 2); }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_void_function(void) {
    const char *src = "fn greet() { print(\"hello\"); }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_multiple_returns(void) {
    const char *src = "fn max(a: int, b: int) -> int { if a > b { return a; } else { return b; } }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_recursive_function(void) {
    const char *src = "fn factorial(n: int) -> int { if n <= 1 { return 1; } else { return n * factorial(n - 1); } }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_while_loop(void) {
    const char *src = "fn main() { let mut i = 0; while i < 10 { i = i + 1; } }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_for_loop(void) {
    const char *src = "fn main() { for i in 0..10 { print(i); } }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_nested_scopes(void) {
    const char *src = "fn main() { let x = 1; { let y = 2; { let z = 3; } } }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_shadowing(void) {
    const char *src = "fn main() { let x = 1; let x = \"shadowed\"; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_cast_int_to_float(void) {
    const char *src = "fn main() { let x = 42 as float; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_tuple(void) {
    const char *src = "fn main() { let t = (1, \"hello\", true); }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_struct_create(void) {
    const char *src = "struct Point { x: int, y: int } fn main() { let p = Point { x: 1, y: 2 }; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_struct_field_access(void) {
    const char *src = "struct Point { x: int, y: int } fn main() { let p = Point { x: 1, y: 2 }; let px = p.x; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_method_call(void) {
    const char *src = "impl Point { fn x(self) -> int { return self.x; } } fn main() { let p = Point { x: 1, y: 2 }; let v = p.x(); }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_complex_expression(void) {
    const char *src = "fn main() { let result = (1 + 2) * (3 - 4) / 5; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_unary_negation(void) {
    const char *src = "fn main() { let x = -42; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_bool_not(void) {
    const char *src = "fn main() { let a = !true; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_ternary_style(void) {
    const char *src = "fn main() { let x = if true { 42 } else { 0 }; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_mixed_types(void) {
    const char *src = "fn main() { let i = 42; let f = 3.14; let b = true; let s = \"str\"; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_var_reassignment(void) {
    const char *src = "fn main() { let mut x = 42; x = 100; }";
    ASTNode *prog = parse_and_typecheck(src);
    assert(prog != NULL);
    tests_passed++;
}

int main(void) {
    printf("─── Type Checker Tests ───\n");

    test_init();

    test_infer_int_literal();
    test_infer_float_literal();
    test_infer_bool_literal();
    test_infer_string_literal();
    test_binary_addition();
    test_binary_all_ops();
    test_comparison_ops();
    test_if_expression();
    test_block_expression();
    test_function_call();
    test_void_function();
    test_multiple_returns();
    test_recursive_function();
    test_while_loop();
    test_for_loop();
    test_nested_scopes();
    test_shadowing();
    test_cast_int_to_float();
    test_tuple();
    test_struct_create();
    test_struct_field_access();
    test_method_call();
    test_complex_expression();
    test_unary_negation();
    test_bool_not();
    test_ternary_style();
    test_mixed_types();
    test_var_reassignment();

    test_cleanup();

    printf("\n─── Results ───\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("  Status: %s\n", tests_failed == 0 ? "ALL PASSED ✓" : "SOME FAILED ✗");

    return tests_failed > 0 ? 1 : 0;
}
