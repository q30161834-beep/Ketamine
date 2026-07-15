#include "include/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE DOCUMENTATION GENERATOR (ket doc)
// ═══════════════════════════════════════════════════════════════════════════════
// Extracts doc comments (///) from Ketamine source and generates Markdown docs.
// Usage: ket doc <source.kt> [output.md]
// ═══════════════════════════════════════════════════════════════════════════════

#define MAX_DOCS 4096
#define MAX_LINE 4096

typedef struct {
    const char *name;       // function/struct/enum name
    const char *kind;       // "fn", "struct", "enum", "trait", "const"
    const char *signature;  // full signature
    const char *doc;        // doc comment text
    int         line;
} DocEntry;

static int doc_entry_count = 0;
static DocEntry doc_entries[MAX_DOCS];

static char *strdup_safe(const char *s) {
    if (!s) return NULL;
    int len = (int)strlen(s);
    char *copy = (char*)malloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

int generate_docs(const char *source_path, const char *output_path) {
    // Read source
    FILE *f = fopen(source_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", source_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    int len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = (char*)malloc(len + 1);
    fread(source, 1, len, f);
    source[len] = '\0';
    fclose(f);

    // Parse doc comments
    char current_doc[16384] = "";
    int current_doc_len = 0;
    char line[MAX_LINE];
    int line_num = 0;
    int pos = 0;

    while (pos < len) {
        // Read a line
        int line_start = pos;
        while (pos < len && source[pos] != '\n') pos++;
        int line_len = pos - line_start;
        if (pos < len) pos++; // skip \n

        if (line_len > 0) {
            memcpy(line, source + line_start, line_len);
            line[line_len] = '\0';
        } else {
            line[0] = '\0';
        }
        line_num++;

        // Skip whitespace
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // Check for doc comment (///)
        if (trimmed[0] == '/' && trimmed[1] == '/' && trimmed[2] == '/') {
            const char *doc_text = trimmed + 3;
            if (*doc_text == ' ') doc_text++;
            int add = snprintf(current_doc + current_doc_len,
                              sizeof(current_doc) - current_doc_len,
                              "%s\n", doc_text);
            if (add > 0 && current_doc_len + add < (int)sizeof(current_doc))
                current_doc_len += add;
            continue;
        }

        // If we have accumulated docs and find a declaration, record it
        if (current_doc_len > 0) {
            // Function
            char name[256] = "";
            char sig[2048] = "";
            char kind[32] = "";

            if (sscanf(trimmed, "fn %255s", name) == 1) {
                strcpy(kind, "fn");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            } else if (sscanf(trimmed, "pub fn %255s", name) == 1) {
                strcpy(kind, "fn");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            } else if (sscanf(trimmed, "struct %255s", name) == 1) {
                strcpy(kind, "struct");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            } else if (sscanf(trimmed, "pub struct %255s", name) == 1) {
                strcpy(kind, "struct");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            } else if (sscanf(trimmed, "enum %255s", name) == 1) {
                strcpy(kind, "enum");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            } else if (sscanf(trimmed, "trait %255s", name) == 1) {
                strcpy(kind, "trait");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            } else if (sscanf(trimmed, "const %255s", name) == 1) {
                strcpy(kind, "const");
                snprintf(sig, sizeof(sig), "%s", trimmed);
            }

            if (name[0] && doc_entry_count < MAX_DOCS) {
                doc_entries[doc_entry_count].name = strdup_safe(name);
                doc_entries[doc_entry_count].kind = strdup_safe(kind);
                doc_entries[doc_entry_count].signature = strdup_safe(sig);
                doc_entries[doc_entry_count].doc = strdup_safe(current_doc);
                doc_entries[doc_entry_count].line = line_num;
                doc_entry_count++;
            }

            current_doc[0] = '\0';
            current_doc_len = 0;
        }

        // Reset doc accumulation on non-comment, non-empty line
        if (trimmed[0] && !(trimmed[0] == '/' && trimmed[1] == '/')) {
            current_doc[0] = '\0';
            current_doc_len = 0;
        }
    }

    // Generate Markdown
    char output[1048576];
    int out_pos = 0;
    int out_size = sizeof(output);

    out_pos += snprintf(output + out_pos, out_size - out_pos,
        "# Ketamine Documentation — %s\n\n", source_path);

    out_pos += snprintf(output + out_pos, out_size - out_pos,
        "**Generated by `ket doc`** | %d items\n\n---\n\n",
        doc_entry_count);

    for (int i = 0; i < doc_entry_count; i++) {
        out_pos += snprintf(output + out_pos, out_size - out_pos,
            "## %s `%s`\n\n", doc_entries[i].kind, doc_entries[i].name);

        out_pos += snprintf(output + out_pos, out_size - out_pos,
            "```ketamine\n%s\n```\n\n", doc_entries[i].signature);

        out_pos += snprintf(output + out_pos, out_size - out_pos,
            "%s\n---\n\n", doc_entries[i].doc);

        if (out_pos >= out_size - 1024) break;

        // Free
        free((void*)doc_entries[i].name);
        free((void*)doc_entries[i].kind);
        free((void*)doc_entries[i].signature);
        free((void*)doc_entries[i].doc);
    }

    // Write output
    const char *out_path = output_path ? output_path : "docs.md";
    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write: %s\n", out_path);
        free(source);
        return 1;
    }
    fwrite(output, 1, out_pos, out);
    fclose(out);

    fprintf(stderr, "ket-doc: Generated %d docs -> %s\n", doc_entry_count, out_path);
    free(source);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ket-doc <source.kt> [output.md]\n");
        fprintf(stderr, "Extracts /// doc comments and generates Markdown.\n");
        return 1;
    }

    const char *source = argv[1];
    const char *output = (argc > 2) ? argv[2] : NULL;
    return generate_docs(source, output);
}
