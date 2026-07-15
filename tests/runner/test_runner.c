// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE TEST RUNNER — Formal test suite with .kt test files
// ═══════════════════════════════════════════════════════════════════════════════

#include "../../src/include/lexer.h"
#include "../../src/include/parser.h"
#include "../../src/include/typecheck.h"
#include "../../src/include/borrow.h"
#include "../../src/include/ir.h"
#include "../../src/include/codegen.h"
#include "../../src/include/arena.h"
#include "../../src/include/diagnostics.h"
#include "../../src/include/symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ─── Test Configuration ───────────────────────────────────────────────────────

typedef enum {
    TEST_PARSE,     // File must parse without errors
    TEST_TYPECHECK, // File must type-check without errors
    TEST_CODEGEN,   // File must compile to LLVM IR without errors
    TEST_GO,        // File must compile to Go without errors
    TEST_WASM,      // File must compile to WASM without errors
    TEST_FAIL_PARSE,     // File must FAIL parsing
    TEST_FAIL_TYPECHECK, // File must FAIL type checking
} TestKind;

typedef struct {
    const char *path;
    const char *name;
    TestKind    kind;
    int         expected_errors;  // 0 for expected success
    bool        skip;
} TestEntry;

// ─── Global Stats ─────────────────────────────────────────────────────────────

static int total_tests = 0;
static int passed_tests = 0;
static int failed_tests = 0;
static int skipped_tests = 0;

// ─── Read file ────────────────────────────────────────────────────────────────

static char *read_file(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz > 10 * 1024 * 1024) {  // 10MB max
        fclose(f);
        return NULL;
    }

    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t n = fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = (int)n;
    return buf;
}

// ─── Run a single test ────────────────────────────────────────────────────────

static bool run_test(TestEntry *test) {
    total_tests++;

    if (test->skip) {
        skipped_tests++;
        printf("  SKIP: %s\n", test->name);
        return true;
    }

    // Read source
    int src_len;
    char *source = read_file(test->path, &src_len);
    if (!source) {
        printf("  FAIL: %s — cannot read file\n", test->name);
        failed_tests++;
        return false;
    }

    // Create context
    Arena *arena = (Arena*)malloc(sizeof(Arena));
    *arena = arena_new_size(64 * 1024 * 1024);

    Context *ctx = (Context*)arena_alloc_zero(arena, sizeof(Context));
    ctx->arena = arena;
    ctx->options.verbose = false;
    ctx->diag.count = 0;
    ctx->diag.error_count = 0;

    ctx->intern_capacity = 1024;
    ctx->intern_table = (InternStr**)arena_alloc_zero(arena,
        sizeof(InternStr*) * 1024);
    ctx->intern_count = 0;

    ctx->types = *ket_type_table_init(arena);

    ctx->files = (SourceFile*)arena_alloc_zero(arena, sizeof(SourceFile));
    ctx->files[0].path = test->path;
    ctx->files[0].source = source;
    ctx->files[0].length = src_len;
    ctx->file_count = 1;

    // Lex
    Lexer lexer;
    lexer_init(&lexer, test->path, source, src_len, ctx);

    // Parse
    Parser parser;
    parser_init(&parser, &lexer, ctx);

    ASTNode *program = parse_program(&parser);
    bool parse_ok = (program != NULL && parser.error_count == 0 && !ket_diag_has_errors(ctx));

    // For parse-only tests
    if (test->kind == TEST_PARSE) {
        free(source);
        if (parse_ok) {
            passed_tests++;
            printf("  PASS: %s\n", test->name);
        } else {
            failed_tests++;
            printf("  FAIL: %s — parse error\n", test->name);
        }
        arena_free(arena);
        free(arena);
        return parse_ok;
    }

    if (test->kind == TEST_FAIL_PARSE) {
        free(source);
        if (!parse_ok) {
            passed_tests++;
            printf("  PASS: %s (expected parse failure)\n", test->name);
        } else {
            failed_tests++;
            printf("  FAIL: %s — expected parse failure but succeeded\n", test->name);
        }
        arena_free(arena);
        free(arena);
        return !parse_ok;
    }

    if (!parse_ok) {
        free(source);
        failed_tests++;
        printf("  FAIL: %s — parse failed\n", test->name);
        arena_free(arena);
        free(arena);
        return false;
    }

    // Type check
    ctx->diag.error_count = 0;
    ctx->diag.count = 0;
    bool tc_ok = ty_check_module(ctx, program);
    bool tc_has_errors = ket_diag_has_errors(ctx);

    if (test->kind == TEST_TYPECHECK) {
        free(source);
        if (tc_ok && !tc_has_errors) {
            passed_tests++;
            printf("  PASS: %s\n", test->name);
        } else {
            failed_tests++;
            printf("  FAIL: %s — type check error (%d errors)\n",
                   test->name, ctx->diag.error_count);
        }
        arena_free(arena);
        free(arena);
        return tc_ok && !tc_has_errors;
    }

    if (test->kind == TEST_FAIL_TYPECHECK) {
        free(source);
        if (tc_has_errors) {
            passed_tests++;
            printf("  PASS: %s (expected type error)\n", test->name);
        } else {
            failed_tests++;
            printf("  FAIL: %s — expected type error but passed\n", test->name);
        }
        arena_free(arena);
        free(arena);
        return tc_has_errors;
    }

    if (!tc_ok) {
        free(source);
        failed_tests++;
        printf("  FAIL: %s — type check failed\n", test->name);
        arena_free(arena);
        free(arena);
        return false;
    }

    // Borrow check
    ctx->diag.error_count = 0;
    bool borrow_ok = borrow_check_module(ctx, program);
    if (!borrow_ok) {
        free(source);
        failed_tests++;
        printf("  FAIL: %s — borrow check failed\n", test->name);
        arena_free(arena);
        free(arena);
        return false;
    }

    // Lower to IR
    ctx->ir_module = ket_ir_lower(ctx, program);
    if (!ctx->ir_module) {
        free(source);
        failed_tests++;
        printf("  FAIL: %s — IR lowering failed\n", test->name);
        arena_free(arena);
        free(arena);
        return false;
    }

    // Run optimization
    DiagCtx diag;
    memset(&diag, 0, sizeof(diag));
    ket_opt_run_passes(ctx->ir_module, OPT_O1, &diag);

    // Codegen based on test kind
    bool codegen_ok = false;
    switch (test->kind) {
        case TEST_CODEGEN: {
            CompileOptions opts;
            memset(&opts, 0, sizeof(opts));
            opts.verbose = false;

            const char *out_path = "test_output.ll";
            int result = codegen_llvm_module(ctx->ir_module, out_path, &opts);
            if (result == 0) {
                // Verify output exists
                FILE *check = fopen(out_path, "r");
                if (check) {
                    fseek(check, 0, SEEK_END);
                    codegen_ok = ftell(check) > 0;
                    fclose(check);
                }
                remove(out_path);
            }
            break;
        }
        case TEST_GO: {
            CompileOptions opts;
            memset(&opts, 0, sizeof(opts));
            codegen_ok = (codegen_go_module(ctx->ir_module, "test_output.go", &opts) == 0);
            remove("test_output.go");
            break;
        }
        case TEST_WASM: {
            CompileOptions opts;
            memset(&opts, 0, sizeof(opts));
            codegen_ok = (codegen_wasm_module(ctx->ir_module, "test_output.wat", &opts) == 0);
            remove("test_output.wat");
            break;
        }
        default:
            break;
    }

    free(source);
    if (test->kind == TEST_CODEGEN || test->kind == TEST_GO || test->kind == TEST_WASM) {
        if (codegen_ok) {
            passed_tests++;
            printf("  PASS: %s\n", test->name);
        } else {
            failed_tests++;
            printf("  FAIL: %s — codegen failed\n", test->name);
        }
    }

    arena_free(arena);
    free(arena);
    return codegen_ok;
}

// ─── Find .kt files in directory ──────────────────────────────────────────────

static int find_test_files(const char *dir, TestEntry *entries, int max_entries,
                           TestKind default_kind) {
    int count = 0;
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < max_entries) {
        const char *name = entry->d_name;
        int len = (int)strlen(name);

        if (len > 3 && strcmp(name + len - 3, ".kt") == 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, name);

            entries[count].path = strdup(path);
            entries[count].name = strdup(name);
            entries[count].kind = default_kind;
            entries[count].expected_errors = 0;
            entries[count].skip = false;

            // Check for skip marker in filename
            if (strstr(name, "skip_") == name) {
                entries[count].skip = true;
            }

            count++;
        }
    }

    closedir(d);
    return count;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char **argv) {
    const char *base_dir = ".";
    if (argc > 1) {
        base_dir = argv[1];
    }

    char parser_dir[1024], tc_dir[1024], cg_dir[1024];
    snprintf(parser_dir, sizeof(parser_dir), "%s/tests/parser", base_dir);
    snprintf(tc_dir, sizeof(tc_dir), "%s/tests/typecheck", base_dir);
    snprintf(cg_dir, sizeof(cg_dir), "%s/tests/codegen", base_dir);

    // Collect all tests
    TestEntry entries[256];
    int entry_count = 0;

    entry_count += find_test_files(parser_dir, entries + entry_count,
        100, TEST_PARSE);
    entry_count += find_test_files(tc_dir, entries + entry_count,
        100, TEST_TYPECHECK);
    entry_count += find_test_files(cg_dir, entries + entry_count,
        100, TEST_CODEGEN);

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     Ketamine Compiler — Formal Test Suite       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    printf("Found %d test files\n", entry_count);
    printf("\n─── Running Tests ───\n\n");

    for (int i = 0; i < entry_count; i++) {
        run_test(&entries[i]);
    }

    printf("\n─── Results ───\n");
    printf("  Total:   %d\n", total_tests);
    printf("  Passed:  %d\n", passed_tests);
    printf("  Failed:  %d\n", failed_tests);
    printf("  Skipped: %d\n", skipped_tests);

    // Cleanup
    for (int i = 0; i < entry_count; i++) {
        free((void*)entries[i].path);
        free((void*)entries[i].name);
    }

    return failed_tests > 0 ? 1 : 0;
}
