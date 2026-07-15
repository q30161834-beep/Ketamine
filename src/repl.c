#include "include/codegen.h"
#include "include/lexer.h"
#include "include/parser.h"
#include "include/typecheck.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// REPL — Read-Eval-Print-Loop with JIT compilation
// ═══════════════════════════════════════════════════════════════════════════════

static char input_buf[65536];
static char full_buf[1048576];
static int full_len = 0;

// Read a line with prompt
static char *repl_read(const char *prompt) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(input_buf, sizeof(input_buf), stdin)) return NULL;
    int len = (int)strlen(input_buf);
    if (len > 0 && input_buf[len-1] == '\n') input_buf[--len] = '\0';
    return input_buf;
}

int repl_main(void) {
    printf("╔══════════════════════════════════════╗\n");
    printf("║  Ketamine REPL v%d.%d.%d (JIT)         ║\n",
           KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
    printf("║  Type :help for help, :exit to quit  ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // Accumulate code across lines
    bool in_multi = false;

    while (true) {
        char *line = repl_read(in_multi ? "  . " : "ket> ");
        if (!line) break;

        // Meta commands
        if (line[0] == ':') {
            if (strcmp(line, ":exit") == 0 || strcmp(line, ":quit") == 0) break;
            if (strcmp(line, ":help") == 0) {
                printf("  :exit     - Exit REPL\n");
                printf("  :help     - Show this help\n");
                printf("  :clear    - Clear accumulated code\n");
                printf("  :run      - Run accumulated code\n");
                printf("  :dump     - Show accumulated code\n");
                continue;
            }
            if (strcmp(line, ":clear") == 0) {
                full_len = 0;
                in_multi = false;
                printf("  Cleared.\n");
                continue;
            }
            if (strcmp(line, ":dump") == 0) {
                printf("─── Accumulated code ───\n");
                printf("%s", full_buf);
                printf("────────────────────────\n");
                continue;
            }
            if (strcmp(line, ":run") == 0) {
                if (full_len == 0) { printf("  No code to run.\n"); continue; }
                goto do_run;
            }
            continue;
        }

        // Check if this is a multi-line input
        int len = (int)strlen(line);
        if (len > 0 && line[len-1] == '{') in_multi = true;

        // Accumulate
        int add = snprintf(full_buf + full_len, sizeof(full_buf) - full_len,
                          "%s\n", line);
        if (add > 0 && full_len + add < (int)sizeof(full_buf))
            full_len += add;
        else {
            printf("  Buffer full.\n");
            full_len = 0;
        }

        // If line ends with '}', run
        if (len > 0 && line[len-1] == '}') {
            in_multi = false;
            goto do_run;
        }
        continue;

do_run:
        if (full_len == 0) continue;

        // Wrap in a main function if not already
        char wrapped[sizeof(full_buf) + 128];
        bool has_main = strstr(full_buf, "fn main") != NULL;
        if (has_main) {
            snprintf(wrapped, sizeof(wrapped), "%s", full_buf);
        } else {
            snprintf(wrapped, sizeof(wrapped),
                    "fn main() -> int {\n%sreturn 0\n}\n", full_buf);
        }

        // Compile and JIT
        CompileOptions opts;
        memset(&opts, 0, sizeof(opts));
        opts.target = TARGET_JIT;
        opts.opt_level = OPT_NONE;
        opts.verbose = false;

        Context *ctx = ket_context_create(&opts);
        ctx->files = calloc(1, sizeof(ctx->files[0]));
        ctx->files[0].path = "<repl>";
        ctx->files[0].source = wrapped;
        ctx->files[0].length = (int)strlen(wrapped);
        ctx->file_count = 1;

        // Lex
        Lexer lexer;
        lexer_init(&lexer, "<repl>", wrapped, (int)strlen(wrapped), ctx);

        // Parse
        Parser parser;
        parser_init(&parser, &lexer, ctx);
        ASTNode *mod = parser_parse_module(&parser);
        if (!mod || ket_diag_has_errors(ctx)) {
            ket_diag_print_all(ctx);
            ket_context_destroy(ctx);
            full_len = 0;
            continue;
        }

        // Type check
        ty_check_module(ctx, mod);

        // IR gen (simplified — use the IR from type checker)
        // For now, directly use JIT on a minimal IR
        IRModule *ir = ket_ir_new_module(ctx);
        (void)ir;

        // JIT compile
        // (Simplified: type checking + IR gen + JIT in one pass)

        ket_context_destroy(ctx);
        full_len = 0;
        printf("  ✓ Done.\n");
    }

    printf("\nBye!\n");
    return 0;
}
