#include "include/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ─── ANSI Color Codes ─────────────────────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

static const char *severity_color(Severity sev) {
    switch (sev) {
        case SEV_NOTE:    return CYAN;
        case SEV_WARNING: return YELLOW;
        case SEV_ERROR:   return RED;
        case SEV_FATAL:   return RED BOLD;
        case SEV_ICE:     return MAGENTA BOLD;
        default:          return RESET;
    }
}

static const char *severity_label(Severity sev) {
    switch (sev) {
        case SEV_NOTE:    return "note";
        case SEV_WARNING: return "warning";
        case SEV_ERROR:   return "error";
        case SEV_FATAL:   return "fatal error";
        case SEV_ICE:     return "internal compiler error";
        default:          return "unknown";
    }
}

// ─── Show source line with caret ──────────────────────────────────────────────
static void print_source_context(Location loc) {
    if (!loc.source) return;

    const char *line_start = loc.source;
    const char *p = loc.source;

    // Find start of line
    while (p > loc.source && p[-1] != '\n') p--;
    line_start = p;

    // Find end of line
    while (*p && *p != '\n') p++;

    int line_len = (int)(p - line_start);
    if (line_len > 0) {
        fprintf(stderr, " %s-->%s ", BLUE, RESET);
        fwrite(line_start, 1, line_len, stderr);
        fprintf(stderr, "\n");
    }

    // Calculate column offset (handle tabs)
    int col = loc.col - 1;
    fprintf(stderr, " %s|%s ", BLUE, RESET);

    // Caret line
    fprintf(stderr, " %s|%s ", BLUE, RESET);
    for (int i = 0; i < col; i++) fputc(' ', stderr);
    int len = loc.length > 0 ? loc.length : 1;
    fprintf(stderr, "%s^", GREEN);
    for (int i = 1; i < len; i++) fputc('~', stderr);
    fprintf(stderr, "%s\n", RESET);
}

void ket_diag_push(Context *ctx, Severity sev, Location loc, int code,
                    const char *fmt, ...) {
    if (ctx->diag.count >= MAX_ERRORS + MAX_WARNINGS) return;
    if (sev == SEV_FATAL || sev == SEV_ICE) {
        ctx->diag.fatal = true;
    }

    Diagnostic *d = &ctx->diag.diags[ctx->diag.count++];
    d->severity = sev;
    d->loc = loc;
    d->code = code;
    d->note_count = 0;
    d->suggestion = NULL;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf((char*)&d->message, 512, fmt, ap);
    va_end(ap);

    if (sev == SEV_ERROR || sev == SEV_FATAL) {
        ctx->diag.error_count++;
    } else if (sev == SEV_WARNING) {
        ctx->diag.warning_count++;
    }
}

void ket_diag_note(Context *ctx, Location loc, const char *label) {
    if (ctx->diag.count <= 0) return;
    Diagnostic *d = &ctx->diag.diags[ctx->diag.count - 1];
    if (d->note_count >= 8) return;
    d->notes[d->note_count].loc = loc;
    d->notes[d->note_count].label = label;
    d->note_count++;
}

void ket_diag_print_all(Context *ctx) {
    for (int i = 0; i < ctx->diag.count; i++) {
        Diagnostic *d = &ctx->diag.diags[i];
        const char *file = d->loc.file ? d->loc.file : "<unknown>";
        const char *color = severity_color(d->severity);
        const char *label = severity_label(d->severity);

        fprintf(stderr, "%s%s%s[%s]%s %s",
                color, BOLD, label, RESET, DIM, RESET);

        if (d->loc.line > 0) {
            fprintf(stderr, "%s:%d:%d: ", file, d->loc.line, d->loc.col);
        } else {
            fprintf(stderr, "%s: ", file);
        }

        fprintf(stderr, "%s%s\n", color, d->message);

        if (d->loc.line > 0) {
            print_source_context(d->loc);
        }

        for (int j = 0; j < d->note_count; j++) {
            fprintf(stderr, " %snote:%s %s\n",
                    CYAN, RESET, d->notes[j].label);
        }

        if (d->suggestion) {
            fprintf(stderr, " %shelp:%s %s\n",
                    BLUE, RESET, d->suggestion);
        }

        fprintf(stderr, "\n");
    }
}

bool ket_diag_has_errors(Context *ctx) {
    return ctx->diag.error_count > 0 || ctx->diag.fatal;
}

void ket_ice(const char *file, int line, const char *func,
             const char *msg, ...) {
    fprintf(stderr, "\n%s%sINTERNAL COMPILER ERROR%s\n", RED BOLD, RESET);
    fprintf(stderr, "  %s:%d: in %s()\n", file, line, func);
    fprintf(stderr, "  ");

    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    fprintf(stderr, "\n\n%sThis is a bug in the Ketamine compiler.%s\n", DIM, RESET);
    fprintf(stderr, "%sPlease report it at: https://github.com/ketamine/ketc/issues%s\n", DIM, RESET);
    abort();
}
