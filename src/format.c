#include "include/types.h"
#include "include/lexer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE CODE FORMATTER (ket fmt)
// ═══════════════════════════════════════════════════════════════════════════════
// Re-indents and normalizes whitespace in Ketamine source files.
// Usage: ket fmt <file.kt>
// ═══════════════════════════════════════════════════════════════════════════════

#define FMT_BUF_SIZE (1 << 24)

typedef struct {
    const char *input;
    int         input_len;
    char        output[FMT_BUF_SIZE];
    int         out_pos;
    int         indent;
    bool        newline;
    bool        in_string;
    bool        in_block_comment;
    int         paren_depth;
    int         brace_depth;
} Formatter;

static void fmt_write(Formatter *f, char c) {
    if (f->out_pos < FMT_BUF_SIZE - 1)
        f->output[f->out_pos++] = c;
}

static void fmt_write_indent(Formatter *f) {
    for (int i = 0; i < f->indent; i++) {
        fmt_write(f, ' '); fmt_write(f, ' '); fmt_write(f, ' '); fmt_write(f, ' ');
    }
}

static void fmt_write_newline(Formatter *f) {
    fmt_write(f, '\n');
    f->newline = true;
}

int format_file(const char *input, int input_len, const char *path) {
    Formatter f;
    memset(&f, 0, sizeof(f));
    f.input = input;
    f.input_len = input_len;
    f.out_pos = 0;
    f.indent = 0;
    f.newline = true;
    f.in_string = false;
    f.in_block_comment = false;

    for (int i = 0; i < input_len; i++) {
        char c = input[i];
        char next = (i + 1 < input_len) ? input[i + 1] : 0;

        // Handle strings
        if (c == '"' && !f.in_block_comment) {
            f.in_string = !f.in_string;
            if (f.newline) { fmt_write_indent(&f); f.newline = false; }
            fmt_write(&f, c);
            continue;
        }

        if (f.in_string) {
            if (c == '\\' && next) {
                fmt_write(&f, c);
                fmt_write(&f, next);
                i++;
            } else {
                fmt_write(&f, c);
            }
            continue;
        }

        // Handle block comments
        if (c == '/' && next == '*' && !f.in_block_comment) {
            f.in_block_comment = true;
            if (f.newline) { fmt_write_indent(&f); f.newline = false; }
            fmt_write(&f, c); fmt_write(&f, next); i++;
            continue;
        }
        if (c == '*' && next == '/' && f.in_block_comment) {
            f.in_block_comment = false;
            fmt_write(&f, c); fmt_write(&f, next); i++;
            continue;
        }
        if (f.in_block_comment) {
            if (c == '\n') {
                fmt_write(&f, c);
                fmt_write_indent(&f);
            } else {
                fmt_write(&f, c);
            }
            continue;
        }

        // Handle line comments
        if (c == '/' && next == '/') {
            if (f.newline) { fmt_write_indent(&f); f.newline = false; }
            while (i < input_len && input[i] != '\n') {
                fmt_write(&f, input[i]);
                i++;
            }
            if (i < input_len) fmt_write_newline(&f);
            continue;
        }

        // Skip whitespace (we'll add our own)
        if (c == ' ' || c == '\t' || c == '\r') continue;

        // Newlines
        if (c == '\n') {
            // Skip blank lines
            int j = i + 1;
            while (j < input_len && (input[j] == ' ' || input[j] == '\t' || input[j] == '\r')) j++;
            if (j < input_len && input[j] == '\n') { i = j; continue; }
            fmt_write_newline(&f);
            continue;
        }

        // Write indent on new line
        if (f.newline) {
            fmt_write_indent(&f);
            f.newline = false;
        }

        // Handle braces for indentation
        if (c == '{') {
            fmt_write(&f, c);
            f.brace_depth++;
            fmt_write_newline(&f);
            f.indent++;
            continue;
        }
        if (c == '}') {
            f.indent = (f.indent > 0) ? f.indent - 1 : 0;
            if (!f.newline) fmt_write_newline(&f);
            fmt_write_indent(&f);
            fmt_write(&f, c);
            f.brace_depth = (f.brace_depth > 0) ? f.brace_depth - 1 : 0;
            // Add newline after closing brace unless followed by comma or parens
            fmt_write_newline(&f);
            continue;
        }

        // Parentheses
        if (c == '(') { f.paren_depth++; fmt_write(&f, c); continue; }
        if (c == ')') { f.paren_depth = (f.paren_depth > 0) ? f.paren_depth - 1 : 0; fmt_write(&f, c); continue; }

        // Comma -> add space
        if (c == ',') { fmt_write(&f, c); fmt_write(&f, ' '); continue; }

        // Semicolon -> newline
        if (c == ';') { fmt_write(&f, c); fmt_write_newline(&f); continue; }

        // Operators: add spaces around them
        const char *binary_ops = "+-*/%=&|^<>!";
        if (strchr(binary_ops, c) && next != '=' && next != c && c != '!') {
            fmt_write(&f, ' ');
            fmt_write(&f, c);
            fmt_write(&f, ' ');
            continue;
        }

        // Default: write character
        fmt_write(&f, c);
    }

    // Null terminate
    f.output[f.out_pos] = '\0';

    // Write output file
    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write: %s\n", path);
        return 1;
    }
    fwrite(f.output, 1, f.out_pos, out);
    fclose(out);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ket-fmt <file.kt>\n");
        return 1;
    }

    const char *path = argv[1];

    // Read file
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    int len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    // Format in-place
    int result = format_file(buf, len, path);
    free(buf);

    if (result == 0) {
        fprintf(stderr, "Formatted: %s\n", path);
    }
    return result;
}
