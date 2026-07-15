#include "include/types.h"
#include "include/lexer.h"
#include "include/parser.h"
#include "include/arena.h"
#include "include/codegen.h"
#include "include/typecheck.h"
#include "include/borrow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE COMPILER — Main entry point
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef _WIN32
  #include <windows.h>
  static double now_sec(void) {
      LARGE_INTEGER freq, count;
      QueryPerformanceFrequency(&freq);
      QueryPerformanceCounter(&count);
      return (double)count.QuadPart / (double)freq.QuadPart;
  }
#else
  #include <sys/time.h>
  static double now_sec(void) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      return tv.tv_sec + tv.tv_usec / 1000000.0;
  }
#endif

// ─── String interner ──────────────────────────────────────────────────────────
#define INTERN_BUCKETS 8192

static uint32_t hash_str(const char *s, int len) {
    uint32_t h = 0x811C9DC5;
    for (int i = 0; i < len; i++)
        h = (h ^ (unsigned char)s[i]) * 0x01000193;
    return h;
}

const char *ket_intern(Context *ctx, const char *str, int len) {
    if (!str || len == 0) return NULL;
    uint32_t h = hash_str(str, len);
    int bucket = h % ctx->intern_capacity;

    InternStr *is = ctx->intern_table[bucket];
    while (is) {
        if (is->hash == h && is->len == len && memcmp(is->data, str, len) == 0)
            return is->data;
        is = is->next;
    }

    // Allocate new interned string
    is = (InternStr*)arena_alloc(ctx->arena, sizeof(InternStr) + len + 1);
    is->hash = h;
    is->len = len;
    memcpy(is->data, str, len);
    is->data[len] = '\0';
    is->next = ctx->intern_table[bucket];
    ctx->intern_table[bucket] = is;
    ctx->intern_count++;
    return is->data;
}

const char *ket_intern_fmt(Context *ctx, const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof(buf)) return NULL;
    return ket_intern(ctx, buf, n);
}

// ─── Context creation ─────────────────────────────────────────────────────────

Context *ket_context_create(const CompileOptions *opts) {
    Arena *arena = (Arena*)malloc(sizeof(Arena));
    *arena = arena_new_size(256 * 1024 * 1024); // 256 MB

    Context *ctx = (Context*)arena_alloc_zero(arena, sizeof(Context));
    ctx->arena = arena;
    ctx->options = *opts;
    ctx->phase = PHASE_LEX;
    ctx->diag.count = 0;
    ctx->diag.error_count = 0;
    ctx->diag.warning_count = 0;
    ctx->diag.fatal = false;

    // Initialize string interner
    ctx->intern_capacity = INTERN_BUCKETS;
    ctx->intern_table = (InternStr**)arena_alloc_zero(arena,
        sizeof(InternStr*) * INTERN_BUCKETS);
    ctx->intern_count = 0;

    // Initialize type table
    ctx->types = *ket_type_table_init(arena);

    // Initialize phase times
    for (int i = 0; i < PHASE_COUNT; i++)
        ctx->phase_times[i] = 0.0;

    return ctx;
}

void ket_context_destroy(Context *ctx) {
    if (ctx->ir_module) {
        ket_ir_free(ctx->ir_module);
    }
    arena_free(ctx->arena);
    free(ctx->arena);
    free(ctx);
}

// ─── Source reading ───────────────────────────────────────────────────────────

static char *read_source(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz > MAX_SOURCE_SIZE) {
        fprintf(stderr, "Source file too large (%ld bytes, max %d)\n",
                sz, MAX_SOURCE_SIZE);
        fclose(f);
        return NULL;
    }

    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read = fread(buf, 1, sz, f);
    fclose(f);
    buf[read] = '\0';
    *out_len = (int)read;
    return buf;
}

// ─── Phase timing ─────────────────────────────────────────────────────────────

static void begin_phase(Context *ctx, CompilerPhase phase) {
    ctx->phase = phase;
    ctx->phase_times[phase] = now_sec();
}

static void end_phase(Context *ctx, CompilerPhase phase) {
    ctx->phase_times[phase] = now_sec() - ctx->phase_times[phase];
}

// ═══════════════════════════════════════════════════════════════════════════════
// USAGE / HELP
// ═══════════════════════════════════════════════════════════════════════════════

static void print_banner(void) {
    fprintf(stderr,
        "\n"
        "  ╔═══════════════════════════════════════════════╗\n"
        "  ║     Ketamine Compiler v%d.%d.%d              ║\n"
        "  ║     AOT-compiled systems language            ║\n"
        "  ╚═══════════════════════════════════════════════╝\n"
        "\n",
        KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
}

static void print_usage(void) {
    fprintf(stderr,
        "Usage: ketc [options] <source.kt>\n"
        "\n"
        "Options:\n"
        "  -o <file>          Output file path\n"
            "  --target <t>       Target backend: llvm (default), go, python, js, wasm,\n"
        "                       x86/native, asm, jit, forth\n"
        "  -O0 / -O1 / -O2 / -O3  Optimization level (default: -O0)\n"
        "\n"
        "  --lex              Dump tokens and exit\n"
        "  --parse            Parse only (check syntax)\n"
        "  --dump-ast         Print AST and exit\n"
        "  --dump-ir          Print IR and exit\n"
        "  --dump-types       Print type info\n"
        "\n"
        "  --emit-llvm        Emit LLVM IR\n"
        "  --emit-ir          Emit internal IR\n"
        "  -S                 Emit assembly\n"
        "  -c                 Compile to object file\n"
        "\n"
        "  --secure           Enable security analysis\n"
        "  --obfuscate        Obfuscate output\n"
        "  --time             Show phase timing\n"
        "  --verbose          Verbose output\n"
        "  -j <n>             Parallel jobs\n"
        "  --help             This help message\n"
        "  --version          Show version\n"
        "\n"
        "Examples:\n"
        "  ketc hello.kt                      # compile to LLVM IR (out.ll)\n"
        "  ketc hello.kt -o hello.ll           # specify output\n"
        "  ketc hello.kt --target go -o hello.go  # compile to Go\n"
        "  ketc hello.kt --target asm -o hello.asm  # compile to assembly\n"
        "  ketc hello.kt --target jit               # JIT compile & run\n"
        "  ketc hello.kt -O2                   # optimize\n"
        "  ketc --lex hello.kt                 # dump tokens\n"
        "  ketc --dump-ast hello.kt            # dump AST\n"
        "\n"
        "Full compilation pipeline:\n"
        "  ketc hello.kt -o hello.ll && llc -filetype=obj hello.ll -o hello.o && clang hello.o -o hello\n"
    );
}

// ═══════════════════════════════════════════════════════════════════════════════
// AST Dumper
// ═══════════════════════════════════════════════════════════════════════════════

static void dump_ast_node(ASTNode *node, int indent);

static void dump_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void dump_ast_node(ASTNode *node, int indent) {
    if (!node) { dump_indent(indent); printf("<null>\n"); return; }

    static const char *names[] = {
        [N_MODULE] = "Module", [N_IMPORT] = "Import", [N_FN_DECL] = "Fn",
        [N_STRUCT_DECL] = "Struct", [N_ENUM_DECL] = "Enum", [N_TRAIT_DECL] = "Trait",
        [N_IMPL_BLOCK] = "Impl", [N_TYPE_ALIAS] = "TypeAlias",
        [N_BLOCK] = "Block", [N_VAR_DECL] = "Var", [N_ASSIGN] = "Assign",
        [N_RETURN] = "Return", [N_IF] = "If", [N_WHILE] = "While",
        [N_FOR] = "For", [N_LOOP] = "Loop", [N_BREAK] = "Break",
        [N_CONTINUE] = "Continue", [N_MATCH] = "Match", [N_EXPR_STMT] = "ExprStmt",
        [N_LITERAL_INT] = "Int", [N_LITERAL_FLOAT] = "Float",
        [N_LITERAL_STR] = "Str", [N_LITERAL_BOOL] = "Bool",
        [N_LITERAL_NULL] = "Null", [N_IDENT] = "Ident",
        [N_BINARY] = "Binary", [N_UNARY] = "Unary", [N_CALL] = "Call",
        [N_INDEX] = "Index", [N_MEMBER] = "Member",
        [N_TUPLE] = "Tuple", [N_ARRAY_INIT] = "Array",
        [N_STRUCT_INIT] = "StructInit", [N_ENUM_INIT] = "EnumInit",
        [N_MATCH_ARM] = "MatchArm", [N_PAT_WILD] = "PatWild",
        [N_PAT_BIND] = "PatBind", [N_PAT_LIT] = "PatLit",
        [N_PAT_ENUM] = "PatEnum", [N_PAT_TUPLE] = "PatTuple",
        [N_PAT_RANGE] = "PatRange", [N_PAT_STRUCT] = "PatStruct",
        [N_PAT_REF] = "PatRef",
    };

    const char *name = node->kind < sizeof(names)/sizeof(names[0]) ? names[node->kind] : "?";
    dump_indent(indent);
    printf("%s", name);

    switch (node->kind) {
        case N_FN_DECL:
            printf(" '%.*s' params=%d",
                   node->fn_decl.fn_namelen, node->fn_decl.fn_name,
                   node->fn_decl.fn_param_count);
            break;
        case N_VAR_DECL:
            printf(" '%.*s'", node->var_decl.var_namelen, node->var_decl.var_name);
            break;
        case N_IDENT:
            printf(" '%.*s'", node->ident.namelen, node->ident.name);
            break;
        case N_LITERAL_INT:
            printf(" %lld", (long long)node->ival);
            break;
        case N_LITERAL_FLOAT:
            printf(" %f", node->fval);
            break;
        case N_LITERAL_STR:
            printf(" \"%.*s\"", node->slen, node->sval);
            break;
        case N_LITERAL_BOOL:
            printf(" %s", node->bval ? "true" : "false");
            break;
        case N_BINARY:
            printf(" op=%d", node->binary.bin_op);
            break;
        case N_CALL:
            printf(" argc=%d", node->call.call_argc);
            break;
        case N_STRUCT_DECL:
            printf(" '%.*s' fields=%d",
                   node->decl.decl_namelen, node->decl.decl_name,
                   node->decl.decl_field_count);
            break;
        case N_ENUM_DECL:
            printf(" '%.*s' variants=%d",
                   node->decl.decl_namelen, node->decl.decl_name,
                   node->decl.decl_field_count);
            break;
        case N_MEMBER:
            printf(" '%.*s'", node->member.member_namelen, node->member.member_name);
            break;
        default:
            break;
    }
    printf("\n");

    // Recurse children
    switch (node->kind) {
        case N_MODULE:
        case N_BLOCK:
            for (int i = 0; i < node->block.stmt_count; i++)
                dump_ast_node(node->block.stmts[i], indent + 1);
            break;
        case N_FN_DECL:
            for (int i = 0; i < node->fn_decl.fn_param_count; i++)
                dump_ast_node(node->fn_decl.fn_params[i], indent + 1);
            dump_ast_node(node->fn_decl.fn_body, indent + 1);
            break;
        case N_VAR_DECL:
            if (node->var_decl.var_type) {
                dump_indent(indent + 1);
                printf("Type: %s\n", codegen_type_str(node->var_decl.var_type));
            }
            if (node->var_decl.var_init)
                dump_ast_node(node->var_decl.var_init, indent + 1);
            break;
        case N_ASSIGN:
            dump_ast_node(node->assign.assign_target, indent + 1);
            dump_ast_node(node->assign.assign_value, indent + 1);
            break;
        case N_RETURN:
            if (node->ret_val)
                dump_ast_node(node->ret_val, indent + 1);
            break;
        case N_IF:
            dump_ast_node(node->if_node.if_cond, indent + 1);
            dump_ast_node(node->if_node.if_then, indent + 1);
            if (node->if_node.if_else)
                dump_ast_node(node->if_node.if_else, indent + 1);
            break;
        case N_WHILE:
        case N_LOOP:
        case N_FOR:
            if (node->loop.loop_cond)
                dump_ast_node(node->loop.loop_cond, indent + 1);
            dump_ast_node(node->loop.loop_body, indent + 1);
            break;
        case N_BINARY:
            dump_ast_node(node->binary.bin_left, indent + 1);
            dump_ast_node(node->binary.bin_right, indent + 1);
            break;
        case N_UNARY:
            dump_ast_node(node->unary.unary_right, indent + 1);
            break;
        case N_CALL:
            dump_ast_node(node->call.call_callee, indent + 1);
            for (int i = 0; i < node->call.call_argc; i++)
                dump_ast_node(node->call.call_args[i], indent + 1);
            break;
        case N_INDEX:
            dump_ast_node(node->index.index_target, indent + 1);
            dump_ast_node(node->index.index_expr, indent + 1);
            break;
        case N_MEMBER:
            dump_ast_node(node->member.member_target, indent + 1);
            break;
        case N_MATCH:
            dump_ast_node(node->match.match_expr, indent + 1);
            for (int i = 0; i < node->match.match_arm_count; i++)
                dump_ast_node(node->match.match_arms[i], indent + 1);
            break;
        case N_MATCH_ARM:
            dump_ast_node(node->arm.arm_pat, indent + 1);
            if (node->arm.arm_guard)
                dump_ast_node(node->arm.arm_guard, indent + 1);
            dump_ast_node(node->arm.arm_body, indent + 1);
            break;
        case N_TUPLE:
            for (int i = 0; i < node->tuple.tuple_count; i++)
                dump_ast_node(node->tuple.tuple_elems[i], indent + 1);
            break;
        case N_ARRAY_INIT:
        case N_STRUCT_INIT:
        case N_ENUM_INIT:
            for (int i = 0; i < node->init.init_argc; i++)
                dump_ast_node(node->init.init_args[i], indent + 1);
            break;
        case N_EXPR_STMT:
            dump_ast_node(node->ret_val, indent + 1);
            break;
        case N_STRUCT_DECL:
        case N_ENUM_DECL:
            for (int i = 0; i < node->decl.decl_field_count; i++)
                dump_ast_node(node->decl.decl_fields[i], indent + 1);
            break;
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char **argv) {
    if (argc < 2) { print_banner(); print_usage(); return 1; }

    // ─── Parse options ───
    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.target = TARGET_LLVM_IR;
    opts.opt_level = OPT_NONE;
    opts.output_path = "out.ll";
    opts.include_dirs = NULL;
    opts.include_dir_count = 0;
    opts.emit_llvm = true;
    opts.jobs = 1;

    const char *input_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_banner(); print_usage(); return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Ketamine Compiler v%d.%d.%d\n",
                   KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
            return 0;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            opts.output_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            const char *t = argv[++i];
            if (strcmp(t, "llvm") == 0) opts.target = TARGET_LLVM_IR;
            else if (strcmp(t, "go") == 0) opts.target = TARGET_GO;
            else if (strcmp(t, "python") == 0) opts.target = TARGET_PYTHON;
            else if (strcmp(t, "js") == 0) opts.target = TARGET_JS;
            else if (strcmp(t, "wasm") == 0) opts.target = TARGET_WASM;
            else if (strcmp(t, "x86") == 0 || strcmp(t, "native") == 0) opts.target = TARGET_X86_64;
            else if (strcmp(t, "asm") == 0) opts.target = TARGET_ASM;
            else if (strcmp(t, "jit") == 0) opts.target = TARGET_JIT;
            else if (strcmp(t, "forth") == 0) opts.target = TARGET_FORTH;
            else { fprintf(stderr, "Unknown target: %s\n", t); return 1; }
            continue;
        }
        if (strcmp(argv[i], "-O0") == 0) opts.opt_level = OPT_NONE;
        else if (strcmp(argv[i], "-O1") == 0) opts.opt_level = OPT_O1;
        else if (strcmp(argv[i], "-O2") == 0) opts.opt_level = OPT_O2;
        else if (strcmp(argv[i], "-O3") == 0) opts.opt_level = OPT_O3;
        else if (strcmp(argv[i], "--lex") == 0) opts.dump_tokens = true;
        else if (strcmp(argv[i], "--parse") == 0) opts.check_only = true;
        else if (strcmp(argv[i], "--dump-ast") == 0) opts.dump_ast = true;
        else if (strcmp(argv[i], "--dump-ir") == 0) opts.dump_ir = true;
        else if (strcmp(argv[i], "--dump-types") == 0) opts.dump_type = true;
        else if (strcmp(argv[i], "--emit-llvm") == 0) opts.emit_llvm = true;
        else if (strcmp(argv[i], "--emit-ir") == 0) opts.emit_ir = true;
        else if (strcmp(argv[i], "-S") == 0) opts.emit_asm = true;
        else if (strcmp(argv[i], "-c") == 0) opts.emit_object = true;
        else if (strcmp(argv[i], "--secure") == 0) opts.safe_mode = true;
        else if (strcmp(argv[i], "--obfuscate") == 0) opts.obfuscate = true;
        else if (strcmp(argv[i], "--time") == 0) opts.time_phases = true;
        else if (strcmp(argv[i], "--verbose") == 0) opts.verbose = true;
        else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
            opts.jobs = atoi(argv[++i]);
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
        else {
            input_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "No input file specified\n");
        print_usage();
        return 1;
    }

    // ─── Read source ───
    int src_len = 0;
    char *source = read_source(input_path, &src_len);
    if (!source) {
        fprintf(stderr, "ketc: cannot read '%s'\n", input_path);
        return 1;
    }

    // ─── Create context ───
    Context *ctx = ket_context_create(&opts);
    ctx->files = arena_alloc_zero(ctx->arena, sizeof(ctx->files[0]));
    ctx->files[0].path = input_path;
    ctx->files[0].source = source;
    ctx->files[0].length = src_len;
    ctx->file_count = 1;

    if (opts.verbose) print_banner();

    // ─── Phase 1: Lex ───
    begin_phase(ctx, PHASE_LEX);

    Lexer lexer;
    lexer_init(&lexer, input_path, source, src_len, ctx);

    if (opts.dump_tokens) {
        lexer_dump(&lexer);
        ket_context_destroy(ctx);
        free(source);
        return 0;
    }

    end_phase(ctx, PHASE_LEX);

    // ─── Phase 2: Parse ───
    begin_phase(ctx, PHASE_PARSE);

    Parser parser;
    parser_init(&parser, &lexer, ctx);

    ASTNode *program = parse_program(&parser);

    if (!program || parser.error_count > 0 || ket_diag_has_errors(ctx)) {
        ket_diag_print_all(ctx);
        fprintf(stderr, "ketc: parse failed with %d errors\n", parser.error_count);
        ket_context_destroy(ctx);
        free(source);
        return 1;
    }

    if (opts.dump_ast) {
        printf("─── AST ───\n");
        dump_ast_node(program, 0);
        printf("─── End AST ───\n");
    }

    if (opts.check_only) {
        fprintf(stderr, "ketc: syntax OK (%d top-level declarations)\n",
                program->block.stmt_count);
        ket_context_destroy(ctx);
        free(source);
        return 0;
    }

    if (opts.verbose) {
        fprintf(stderr, "ketc: parsed %d top-level declarations\n",
                program->block.stmt_count);
    }

    end_phase(ctx, PHASE_PARSE);

    // ─── Phase 3: Semantic Analysis ───
    begin_phase(ctx, PHASE_SEMA);
    if (opts.verbose) fprintf(stderr, "  semantic analysis...\n");
    end_phase(ctx, PHASE_SEMA);

    // ─── Phase 4: Type Check ───
    begin_phase(ctx, PHASE_TYPE_CHECK);

    if (!ty_check_module(ctx, program)) {
        ket_diag_print_all(ctx);
        fprintf(stderr, "ketc: type check failed\n");
        ket_context_destroy(ctx);
        free(source);
        return 1;
    }

    if (opts.verbose) {
        fprintf(stderr, "  type check passed\n");
    }

    end_phase(ctx, PHASE_TYPE_CHECK);

    // ─── Phase 5: Borrow Check ───
    begin_phase(ctx, PHASE_BORROW_CHECK);

    if (!borrow_check_module(ctx, program)) {
        ket_diag_print_all(ctx);
        fprintf(stderr, "ketc: borrow check failed\n");
        ket_context_destroy(ctx);
        free(source);
        return 1;
    }

    if (opts.verbose) {
        fprintf(stderr, "  borrow check passed\n");
    }

    end_phase(ctx, PHASE_BORROW_CHECK);

    // ─── Phase 6: IR Generation ───
    begin_phase(ctx, PHASE_IR_GEN);

    ctx->ir_module = ket_ir_lower(ctx, program);

    if (opts.dump_ir) {
        ket_ir_verify(ctx->ir_module, &ctx->diag);
    }

    if (opts.emit_ir) {
        char ir_path[1024];
        const char *base = input_path;
        const char *dot = strrchr(input_path, '.');
        if (dot) {
            int len = (int)(dot - input_path);
            snprintf(ir_path, sizeof(ir_path), "%.*s.ir", len, base);
        } else {
            snprintf(ir_path, sizeof(ir_path), "%s.ir", input_path);
        }
        FILE *irf = fopen(ir_path, "w");
        if (irf) {
            // Redirect IR printer to file
            fprintf(irf, "; Ketamine IR — %s\n", input_path);
            fprintf(irf, "; Functions: %d\n\n", ctx->ir_module->func_count);
            fclose(irf);
            fprintf(stderr, "ketc: IR written to %s\n", ir_path);
        }
    }

    end_phase(ctx, PHASE_IR_GEN);

    // ─── Phase 7: Optimize ───
    begin_phase(ctx, PHASE_IR_OPTIMIZE);

    ket_opt_run_passes(ctx->ir_module, opts.opt_level, &ctx->diag);

    end_phase(ctx, PHASE_IR_OPTIMIZE);

    // ─── Phase 8: Codegen ───
    begin_phase(ctx, PHASE_CODEGEN);

    int result = 0;
    switch (opts.target) {
        case TARGET_LLVM_IR:
            result = codegen_llvm_module(ctx->ir_module, opts.output_path, &opts);
            if (result == 0) {
                fprintf(stderr, "ketc: LLVM IR -> %s\n", opts.output_path);
            }
            break;
        case TARGET_GO:
            result = codegen_go_module(ctx->ir_module, opts.output_path, &opts);
            if (result == 0) {
                fprintf(stderr, "ketc: Go source -> %s\n", opts.output_path);
            }
            break;
        case TARGET_PYTHON:
            result = codegen_python_module(ctx->ir_module, opts.output_path, &opts);
            if (result == 0) {
                fprintf(stderr, "ketc: Python source -> %s\n", opts.output_path);
            }
            break;
        case TARGET_JS:
            result = codegen_js_module(ctx->ir_module, opts.output_path, &opts);
            break;
        case TARGET_WASM:
            result = codegen_wasm_module(ctx->ir_module, opts.output_path, &opts);
            break;
        case TARGET_ASM:
            result = codegen_asm_module(ctx->ir_module, opts.output_path, &opts);
            if (result == 0) {
                fprintf(stderr, "ketc: Assembly -> %s\n", opts.output_path);
            }
            break;
        case TARGET_X86_64:
            result = codegen_native_module(ctx->ir_module, opts.output_path, &opts);
            if (result == 0) {
                fprintf(stderr, "ketc: Native x86-64 -> %s\n", opts.output_path);
            }
            break;
        case TARGET_JIT: {
            result = jit_compile_module(ctx->ir_module, &opts);
            if (result == 0) {
                JITFunc main_fn = jit_get_function("main");
                if (main_fn) {
                    fprintf(stderr, "ketc: JIT executing main()...\n");
                    main_fn(NULL);
                } else {
                    fprintf(stderr, "ketc: JIT compiled successfully (no main() found)\n");
                }
                jit_free();
            }
            break;
        }
        case TARGET_FORTH:
            result = codegen_forth_module(ctx->ir_module, opts.output_path, &opts);
            if (result == 0) {
                fprintf(stderr, "ketc: Forth source -> %s\n", opts.output_path);
            }
            break;
        default:
            fprintf(stderr, "ketc: unsupported target\n");
            result = 1;
    }

    end_phase(ctx, PHASE_CODEGEN);

    // ─── Complexity Analysis ───
    if (opts.time_phases) {
        double total = 0;
        for (int i = 0; i < PHASE_COUNT; i++)
            total += ctx->phase_times[i];

        fprintf(stderr, "\n─── Complexity Analysis ───\n");

        // Phase timing with percentage
        fprintf(stderr, "  Phase Timing:\n");
        fprintf(stderr, "  %-18s %10s %8s\n", "Phase", "Time (ms)", "%");
        fprintf(stderr, "  %-18s %10s %8s\n", "─────", "────────", "─");
        const char *phase_names[] = {
            [PHASE_LEX]          = "Lex",
            [PHASE_PARSE]        = "Parse",
            [PHASE_SEMA]         = "Sema",
            [PHASE_TYPE_CHECK]   = "Type Check",
            [PHASE_BORROW_CHECK] = "Borrow Check",
            [PHASE_IR_GEN]       = "IR Gen",
            [PHASE_IR_OPTIMIZE]  = "Optimize",
            [PHASE_CODEGEN]      = "Codegen",
        };
        for (int i = 0; i < PHASE_COUNT - 1; i++) {  // skip PHASE_COUNT and PHASE_LINK
            double t = ctx->phase_times[i];
            double pct = total > 0 ? (t / total) * 100.0 : 0;
            fprintf(stderr, "  %-18s %8.3f ms %7.1f%%\n",
                    phase_names[i] ? phase_names[i] : "?", t * 1000, pct);
        }
        fprintf(stderr, "  %-18s %8.3f ms\n", "Total", total * 1000);

        // Memory analysis
        size_t arena_used = arena_total_used(ctx->arena);
        size_t arena_allocd = arena_total_allocated(ctx->arena);
        fprintf(stderr, "\n  Memory Profile:\n");
        fprintf(stderr, "  %-18s %10s %8s\n", "Category", "Size", "%");
        fprintf(stderr, "  %-18s %10s %8s\n", "────────", "────", "─");
        fprintf(stderr, "  %-18s %8zu KB %7.1f%%\n", "Arena Used",
                arena_used / 1024,
                arena_allocd > 0 ? (double)arena_used / arena_allocd * 100.0 : 0);
        fprintf(stderr, "  %-18s %8zu KB\n", "Arena Allocated",
                arena_allocd / 1024);
        fprintf(stderr, "  %-18s %4d KB (%d strings)\n", "Intern Table",
                (int)(ctx->intern_count * sizeof(InternStr*) / 1024),
                ctx->intern_count);
        fprintf(stderr, "  %-18s %4d\n", "Source Size",
                ctx->file_count > 0 ? ctx->files[0].length : 0);
    }

    // ─── Summary ───
    if (opts.verbose || opts.dump_ast) {
        fprintf(stderr, "\n─── Summary ───\n");
        if (ctx->ir_module) {
            fprintf(stderr, "  Functions:        %d\n", ctx->ir_module->func_count);
            fprintf(stderr, "  Basic Blocks:     %d\n", ctx->ir_module->block_count);
            // Count IR instructions
            int total_insts = 0;
            for (int f = 0; f < ctx->ir_module->func_count; f++) {
                IRFunction *fn = ctx->ir_module->functions[f];
                for (int b = 0; b < fn->block_count; b++) {
                    IRInst *inst = fn->blocks[b]->first;
                    while (inst) { total_insts++; inst = inst->next; }
                }
            }
            fprintf(stderr, "  IR Instructions:  %d\n", total_insts);
        }
        fprintf(stderr, "  Types Registered: %d\n", ctx->types.count);
        fprintf(stderr, "  Strings Interned: %d\n", ctx->intern_count);
        fprintf(stderr, "  Arena:            %zu / %zu MB",
                arena_total_used(ctx->arena) / (1024*1024),
                arena_total_allocated(ctx->arena) / (1024*1024));
        if (arena_total_allocated(ctx->arena) > 0) {
            fprintf(stderr, " (%d%% used)",
                    (int)(arena_total_used(ctx->arena) * 100 / arena_total_allocated(ctx->arena)));
        }
        fprintf(stderr, "\n");

        // AST node count (walk the tree)
        int ast_nodes = 0;
        if (program) {
            // Simple count via recursion would be nice but we just estimate
            ast_nodes = program->block.stmt_count * 8;  // rough estimate
        }
        fprintf(stderr, "  AST Nodes:        ~%d (estimated)\n", ast_nodes);
    }

    // ─── Cleanup ───
    if (result) {
        ket_diag_print_all(ctx);
    }

    ket_context_destroy(ctx);
    free(source);
    return result;
}
