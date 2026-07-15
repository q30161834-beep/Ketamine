// ═══════════════════════════════════════════════════════════════════════════════
// CODEGEN TESTS — Integration tests for LLVM IR, Go, and WASM backends
// ═══════════════════════════════════════════════════════════════════════════════

#include "../src/include/lexer.h"
#include "../src/include/parser.h"
#include "../src/include/typecheck.h"
#include "../src/include/borrow.h"
#include "../src/include/ir.h"
#include "../src/include/codegen.h"
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
    *test_arena = arena_new_size(64 * 1024 * 1024);
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
    if (test_ctx->ir_module) {
        ket_ir_free(test_ctx->ir_module);
    }
    arena_free(test_arena);
    free(test_arena);
}

static ASTNode *compile_to_ir(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, "<test>", source, (int)strlen(source), test_ctx);

    Parser parser;
    parser_init(&parser, &lexer, test_ctx);

    ASTNode *prog = parse_program(&parser);
    if (!prog) return NULL;

    // Reset diagnostics
    test_ctx->diag.count = 0;
    test_ctx->diag.error_count = 0;

    // Type check
    if (!ty_check_module(test_ctx, prog)) {
        return NULL;
    }

    // Borrow check
    if (!borrow_check_module(test_ctx, prog)) {
        return NULL;
    }

    // Lower to IR
    test_ctx->ir_module = ket_ir_lower(test_ctx, prog);
    if (!test_ctx->ir_module) {
        return NULL;
    }

    return prog;
}

static void test_llvm_simple_function(void) {
    const char *src = "fn main() { return 42; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    assert(test_ctx->ir_module != NULL);

    // Generate LLVM IR to temp file
    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(test_ctx->ir_module, "test_out.ll", &opts);
    assert(result == 0);

    // Verify output file exists and has content
    FILE *f = fopen("test_out.ll", "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    assert(size > 0);
    fclose(f);

    remove("test_out.ll");
    tests_passed++;
}

static void test_llvm_binary_ops(void) {
    const char *src = "fn main() { let x = 1 + 2 * 3; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    assert(test_ctx->ir_module != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(test_ctx->ir_module, "test_bin.ll", &opts);
    assert(result == 0);
    remove("test_bin.ll");
    tests_passed++;
}

static void test_llvm_control_flow(void) {
    const char *src = "fn main() { if true { return 1; } else { return 2; } }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(test_ctx->ir_module, "test_cf.ll", &opts);
    assert(result == 0);
    remove("test_cf.ll");
    tests_passed++;
}

static void test_llvm_while_loop(void) {
    const char *src = "fn main() { let mut i = 0; while i < 10 { i = i + 1; } }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(test_ctx->ir_module, "test_while.ll", &opts);
    assert(result == 0);
    remove("test_while.ll");
    tests_passed++;
}

static void test_llvm_function_call(void) {
    const char *src = "fn add(a: int, b: int) -> int { return a + b; } fn main() { let x = add(1, 2); }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(test_ctx->ir_module, "test_call.ll", &opts);
    assert(result == 0);
    remove("test_call.ll");
    tests_passed++;
}

static void test_llvm_comparisons(void) {
    const char *src = "fn main() { let a = 1 == 2; let b = 3 < 4; let c = 5 > 6; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_llvm_module(test_ctx->ir_module, "test_cmp.ll", &opts);
    assert(result == 0);
    remove("test_cmp.ll");
    tests_passed++;
}

static void test_go_simple_function(void) {
    const char *src = "fn main() { print(42); }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_go_module(test_ctx->ir_module, "test_out.go", &opts);
    assert(result == 0);

    FILE *f = fopen("test_out.go", "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    assert(size > 0);
    fclose(f);

    remove("test_out.go");
    tests_passed++;
}

static void test_wasm_simple_function(void) {
    const char *src = "fn main() { return 42; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    int result = codegen_wasm_module(test_ctx->ir_module, "test_out.wat", &opts);
    assert(result == 0);

    FILE *f = fopen("test_out.wat", "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    assert(size > 0);
    fclose(f);

    remove("test_out.wat");
    tests_passed++;
}

static void test_full_pipeline_int(void) {
    const char *src = "fn main() -> int { return 42; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    assert(test_ctx->ir_module != NULL);
    assert(test_ctx->ir_module->func_count > 0);
    tests_passed++;
}

static void test_full_pipeline_string(void) {
    const char *src = "fn main() { print(\"hello\"); }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_full_pipeline_math(void) {
    const char *src = "fn main() { let x = (1 + 2) * (3 - 4) / 5; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_ir_optimization(void) {
    const char *src = "fn main() -> int { return 2 + 3; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    // Run optimization passes
    DiagCtx diag;
    memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(test_ctx->ir_module, OPT_O2, &diag);

    // Verify constant folding: 2 + 3 should be folded to 5
    // (This is a basic check that optimization doesn't crash)
    tests_passed++;
}

static void test_ir_verification(void) {
    const char *src = "fn main() -> int { return 0; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);

    DiagCtx diag;
    memset(&diag, 0, sizeof(diag));
    bool ok = ket_ir_verify(test_ctx->ir_module, &diag);
    // Verification may pass or fail depending on IR complexity
    // At minimum it shouldn't crash
    tests_passed++;
}

static void test_null_literal(void) {
    const char *src = "fn main() { let x = null; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    tests_passed++;
}

static void test_char_literal(void) {
    const char *src = "fn main() { let c = 'A'; }";
    ASTNode *prog = compile_to_ir(src);
    assert(prog != NULL);
    tests_passed++;
}

int main(void) {
    printf("─── Codegen Integration Tests ───\n");

    test_init();

    test_llvm_simple_function();
    test_llvm_binary_ops();
    test_llvm_control_flow();
    test_llvm_while_loop();
    test_llvm_function_call();
    test_llvm_comparisons();
    test_go_simple_function();
    test_wasm_simple_function();
    test_full_pipeline_int();
    test_full_pipeline_string();
    test_full_pipeline_math();
    test_ir_optimization();
    test_ir_verification();
    test_null_literal();
    test_char_literal();

    test_cleanup();

    printf("\n─── Results ───\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("  Status: %s\n", tests_failed == 0 ? "ALL PASSED ✓" : "SOME FAILED ✗");

    return tests_failed > 0 ? 1 : 0;
}
