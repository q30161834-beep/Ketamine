// ═══════════════════════════════════════════════════════════════════════════════
// PARSER TESTS — Comprehensive parser test suite
// ═══════════════════════════════════════════════════════════════════════════════

#include "../src/include/lexer.h"
#include "../src/include/parser.h"
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
    *test_arena = arena_new_size(16 * 1024 * 1024);
    test_ctx = (Context*)arena_alloc_zero(test_arena, sizeof(Context));
    test_ctx->arena = test_arena;
    test_ctx->options.verbose = false;
    test_ctx->diag.count = 0;
    test_ctx->diag.error_count = 0;

    // Initialize string interner with a fixed small table
    test_ctx->intern_capacity = 512;
    test_ctx->intern_table = (InternStr**)arena_alloc_zero(test_arena,
        sizeof(InternStr*) * 512);
    test_ctx->intern_count = 0;

    // Initialize type table
    test_ctx->types = *ket_type_table_init(test_arena);
}

static void test_cleanup(void) {
    arena_free(test_arena);
    free(test_arena);
}

static ASTNode *parse_string(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, "<test>", source, (int)strlen(source), test_ctx);

    Parser parser;
    parser_init(&parser, &lexer, test_ctx);

    ASTNode *prog = parse_program(&parser);
    return prog;
}

static void test_simple_function(void) {
    const char *src = "fn main() { return 42; }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    assert(prog->kind == N_MODULE);
    assert(prog->block.stmt_count >= 1);

    ASTNode *fn = prog->block.stmts[0];
    assert(fn->kind == N_FN_DECL);
    assert(strcmp(fn->fn_decl.fn_name, "main") == 0);
    assert(fn->fn_decl.fn_param_count == 0);
    assert(fn->fn_decl.fn_body != NULL);
    assert(fn->fn_decl.fn_body->kind == N_BLOCK ||
           fn->fn_decl.fn_body->kind == N_BLOCK_EXPR);

    tests_passed++;
}

static void test_function_with_params(void) {
    const char *src = "fn add(a: int, b: int) -> int { return a + b; }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *fn = prog->block.stmts[0];
    assert(fn->fn_decl.fn_param_count == 2);

    ASTNode *p0 = fn->fn_decl.fn_params[0];
    assert(p0->kind == N_VAR_DECL);
    assert(strcmp(p0->var_decl.var_name, "a") == 0);

    ASTNode *p1 = fn->fn_decl.fn_params[1];
    assert(p1->kind == N_VAR_DECL);
    assert(strcmp(p1->var_decl.var_name, "b") == 0);

    tests_passed++;
}

static void test_variable_declaration(void) {
    const char *src = "fn main() { let x: int = 42; let mut y = 10; }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *fn = prog->block.stmts[0];
    ASTNode *body = fn->fn_decl.fn_body;
    // block should contain 2 var decls
    int var_count = 0;
    for (int i = 0; i < body->block.stmt_count; i++) {
        if (body->block.stmts[i]->kind == N_VAR_DECL)
            var_count++;
    }
    assert(var_count == 2);

    tests_passed++;
}

static void test_if_else(void) {
    const char *src = "fn main() { if x > 0 { return 1; } else { return -1; } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_while_loop(void) {
    const char *src = "fn main() { while i < 10 { i = i + 1; } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_for_loop(void) {
    const char *src = "fn main() { for i in 0..10 { print(i); } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_loop_break_continue(void) {
    const char *src = "fn main() { loop { if x > 10 { break; } x = x + 1; continue; } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_match(void) {
    const char *src = "fn main() { match x { 1 => \"one\", 2 => \"two\", _ => \"other\" } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);

    // Find the match
    ASTNode *fn = prog->block.stmts[0];
    ASTNode *body = fn->fn_decl.fn_body;
    int found_match = 0;
    for (int i = 0; i < body->block.stmt_count; i++) {
        if (body->block.stmts[i]->kind == N_MATCH ||
            body->block.stmts[i]->kind == N_MATCH_EXPR)
            found_match++;
    }
    assert(found_match > 0);
    tests_passed++;
}

static void test_struct_declaration(void) {
    const char *src = "struct Point { x: int, y: int }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    assert(prog->block.stmt_count >= 1);

    ASTNode *s = prog->block.stmts[0];
    assert(s->kind == N_STRUCT_DECL);
    assert(strcmp(s->decl.decl_name, "Point") == 0);
    assert(s->decl.decl_field_count == 2);

    tests_passed++;
}

static void test_enum_declaration(void) {
    const char *src = "enum Option { None, Some(int) }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *e = prog->block.stmts[0];
    assert(e->kind == N_ENUM_DECL);
    assert(strcmp(e->decl.decl_name, "Option") == 0);
    assert(e->decl.decl_field_count == 2);

    tests_passed++;
}

static void test_generic_function(void) {
    const char *src = "fn identity<T>(x: T) -> T { return x; }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *fn = prog->block.stmts[0];
    assert(fn->fn_decl.fn_generic_count == 1);
    assert(strcmp(fn->fn_decl.fn_generics[0]->generic.generic_name, "T") == 0);

    tests_passed++;
}

static void test_generic_struct(void) {
    const char *src = "struct Box<T> { value: T }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *s = prog->block.stmts[0];
    assert(s->decl.decl_generic_count == 1);
    assert(strcmp(s->decl.decl_generics[0]->generic.generic_name, "T") == 0);

    tests_passed++;
}

static void test_trait_declaration(void) {
    const char *src = "trait Display { fn to_string(self) -> str; }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *t = prog->block.stmts[0];
    assert(t->kind == N_TRAIT_DECL);
    assert(strcmp(t->decl.decl_name, "Display") == 0);

    tests_passed++;
}

static void test_impl_block(void) {
    const char *src = "impl Point { fn x(self) -> int { return self.x; } }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *imp = prog->block.stmts[0];
    assert(imp->kind == N_IMPL_BLOCK);

    tests_passed++;
}

static void test_trait_impl(void) {
    const char *src = "impl Display for Point { fn to_string(self) -> str { return \"Point\"; } }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *imp = prog->block.stmts[0];
    assert(imp->kind == N_IMPL_BLOCK);
    assert(imp->impl_block.impl_trait != NULL);

    tests_passed++;
}

static void test_where_clause(void) {
    const char *src = "fn clamp<T: Ord>(x: T, min: T, max: T) -> T where T: Copy { return x; }";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *fn = prog->block.stmts[0];
    assert(fn->fn_decl.fn_generic_count >= 1);
    assert(fn->fn_decl.fn_where_count >= 0);

    tests_passed++;
}

static void test_closure(void) {
    const char *src = "fn main() { let f = || { return 42; }; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_pipe_forward(void) {
    const char *src = "fn main() { let result = x |> transform |> process; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_range_expression(void) {
    const char *src = "fn main() { for i in 0..10 { } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_nested_block_comment(void) {
    const char *src = "fn main() { /* outer /* inner */ more */ return 42; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_raw_string(void) {
    const char *src = "fn main() { let s = #\"hello world\"; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_type_alias(void) {
    const char *src = "type MyInt = int;";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *ta = prog->block.stmts[0];
    assert(ta->kind == N_TYPE_ALIAS);

    tests_passed++;
}

static void test_import_wildcard(void) {
    const char *src = "import core::*;";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *imp = prog->block.stmts[0];
    assert(imp->kind == N_IMPORT);
    assert(imp->import.is_wildcard);

    tests_passed++;
}

static void test_import_alias(void) {
    const char *src = "import math as m;";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    ASTNode *imp = prog->block.stmts[0];
    assert(imp->kind == N_IMPORT);
    assert(imp->import.alias != NULL);

    tests_passed++;
}

static void test_multiple_functions(void) {
    const char *src =
        "fn add(a: int, b: int) -> int { return a + b; }\n"
        "fn sub(a: int, b: int) -> int { return a - b; }\n"
        "fn mul(a: int, b: int) -> int { return a * b; }\n";
    ASTNode *prog = parse_string(src);

    assert(prog != NULL);
    assert(prog->block.stmt_count == 3);
    for (int i = 0; i < 3; i++)
        assert(prog->block.stmts[i]->kind == N_FN_DECL);

    tests_passed++;
}

static void test_nested_blocks(void) {
    const char *src = "fn main() { let x = 1; { let y = 2; { let z = 3; } } }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_unary_operators(void) {
    const char *src = "fn main() { let a = -x; let b = !flag; let c = &val; let d = *ptr; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_compound_assignment(void) {
    const char *src = "fn main() { x += 1; y -= 2; z *= 3; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_bool_literals(void) {
    const char *src = "fn main() { let a = true; let b = false; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_escape_strings(void) {
    const char *src = "fn main() { let s = \"hello\\nworld\\ttab\"; }";
    ASTNode *prog = parse_string(src);
    assert(prog != NULL);
    tests_passed++;
}

int main(void) {
    printf("─── Parser Tests ───\n");

    test_init();

    test_simple_function();
    test_function_with_params();
    test_variable_declaration();
    test_if_else();
    test_while_loop();
    test_for_loop();
    test_loop_break_continue();
    test_match();
    test_struct_declaration();
    test_enum_declaration();
    test_generic_function();
    test_generic_struct();
    test_trait_declaration();
    test_impl_block();
    test_trait_impl();
    test_where_clause();
    test_closure();
    test_pipe_forward();
    test_range_expression();
    test_nested_block_comment();
    test_raw_string();
    test_type_alias();
    test_import_wildcard();
    test_import_alias();
    test_multiple_functions();
    test_nested_blocks();
    test_unary_operators();
    test_compound_assignment();
    test_bool_literals();
    test_escape_strings();

    test_cleanup();

    printf("\n─── Results ───\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("  Status: %s\n", tests_failed == 0 ? "ALL PASSED ✓" : "SOME FAILED ✗");

    return tests_failed > 0 ? 1 : 0;
}
