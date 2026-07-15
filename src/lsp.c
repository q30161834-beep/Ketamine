#include "include/lsp.h"
#include "include/lexer.h"
#include "include/parser.h"
#include "include/typecheck.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ═══════════════════════════════════════════════════════════════════════════════
// LSP SERVER — Language Server Protocol Implementation
// ═══════════════════════════════════════════════════════════════════════════════

// ─── JSON Helpers (minimal, LSP-specific) ─────────────────────────────────────

static const char *json_skip_ws(const char *p) {
    while (*p && (unsigned char)*p <= 32) p++;
    return p;
}

static const char *json_read_string(const char *p, char *buf, int buf_size) {
    p = json_skip_ws(p);
    if (*p != '"') return NULL;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < buf_size - 1) {
        if (*p == '\\') {
            p++;
            if (*p == '"') buf[i++] = '"';
            else if (*p == 'n') buf[i++] = '\n';
            else if (*p == 'r') buf[i++] = '\r';
            else if (*p == 't') buf[i++] = '\t';
            else if (*p == '\\') buf[i++] = '\\';
            else if (*p == 'u') {
                // Parse 4-hex-digit unicode
                char hex[5] = {0,0,0,0,0};
                for (int j = 0; j < 4 && p[1]; j++) { hex[j] = p[1]; p++; }
                unsigned int cp;
                sscanf(hex, "%x", &cp);
                if (cp < 128) buf[i++] = (char)cp;
                else { buf[i++] = '?'; }  // non-ASCII skipped
            } else buf[i++] = *p;
        } else {
            buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';
    if (*p == '"') p++;
    return p;
}

static const char *json_read_int(const char *p, int *out) {
    p = json_skip_ws(p);
    *out = 0;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    while (*p && isdigit((unsigned char)*p)) {
        *out = *out * 10 + (*p - '0');
        p++;
    }
    *out *= sign;
    return p;
}

static const char *json_find_key(const char *p, const char *key) {
    char kbuf[256];
    p = json_skip_ws(p);
    while (*p) {
        p = json_skip_ws(p);
        if (*p == '{' || *p == ',') {
            if (*p == '{') p++;
            p = json_skip_ws(p);
            if (*p == '"') {
                p = json_read_string(p, kbuf, sizeof(kbuf));
                if (kbuf && strcmp(kbuf, key) == 0) {
                    p = json_skip_ws(p);
                    if (*p == ':') return p + 1;
                }
            }
        } else if (*p == '}' || *p == ']') {
            return NULL;
        } else p++;
    }
    return NULL;
}

// ─── Content-Length framing ───────────────────────────────────────────────────

static int read_frame(int fd, char *buf, int buf_size) {
    // Read Content-Length header
    char header[256];
    int hi = 0;
    int content_length = 0;

    while (hi < (int)sizeof(header) - 4) {
        char c;
        if (fread(&c, 1, 1, stdin) != 1) return -1;
        if (c == '\r') {
            fread(&c, 1, 1, stdin);  // read \n
            if (hi == 0) break;  // end of headers
            header[hi] = '\0';
            hi = 0;
            if (sscanf(header, "Content-Length: %d", &content_length) == 1) {
                // found length
            }
        } else if (c != '\n') {
            header[hi++] = c;
        }
    }

    if (content_length <= 0 || content_length > buf_size - 1) return -1;

    size_t read_total = 0;
    while (read_total < (size_t)content_length) {
        size_t n = fread(buf + read_total, 1, content_length - read_total, stdin);
        if (n == 0) return -1;
        read_total += n;
    }
    buf[read_total] = '\0';
    return (int)read_total;
}

static void write_frame(const char *data) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", strlen(data), data);
    fflush(stdout);
}

// ─── LSP JSON Response Builders ───────────────────────────────────────────────

static char *build_json_result(const char *result, Arena *arena) {
    (void)arena;
    // Simple string concatenation
    size_t len = strlen(result);
    char *buf = (char*)calloc(len + 1, 1);
    memcpy(buf, result, len);
    return buf;
}

void lsp_send_notification(LspServer *server, const char *method, const char *params) {
    (void)server;
    char buf[65536];
    snprintf(buf, sizeof(buf),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method, params);
    write_frame(buf);
}

void lsp_send_response(LspServer *server, int id, const char *result) {
    (void)server;
    char buf[65536];
    if (result) {
        snprintf(buf, sizeof(buf),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);
    } else {
        snprintf(buf, sizeof(buf),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", id);
    }
    write_frame(buf);
}

void lsp_send_error(LspServer *server, int id, int code, const char *message) {
    (void)server;
    char buf[65536];
    snprintf(buf, sizeof(buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
        id, code, message);
    write_frame(buf);
}

// ─── Document Management ─────────────────────────────────────────────────────

static LspDocument *find_document(LspServer *server, const char *uri) {
    for (int i = 0; i < server->document_count; i++) {
        if (strcmp(server->documents[i]->uri, uri) == 0)
            return server->documents[i];
    }
    return NULL;
}

static LspDocument *add_document(LspServer *server, const char *uri,
                                  const char *text, int version) {
    LspDocument *doc = (LspDocument*)calloc(1, sizeof(LspDocument));
    doc->uri = strdup(uri);
    doc->text = strdup(text);
    doc->text_len = (int)strlen(text);
    doc->version = version;
    doc->ast = NULL;
    doc->ast_valid = false;

    if (server->document_count >= server->document_cap) {
        server->document_cap = server->document_cap ? server->document_cap * 2 : 16;
        server->documents = (LspDocument**)realloc(server->documents,
            sizeof(LspDocument*) * server->document_cap);
    }
    server->documents[server->document_count++] = doc;

    return doc;
}

void lsp_did_open(LspServer *server, const char *uri, const char *text, int version) {
    LspDocument *doc = find_document(server, uri);
    if (doc) {
        free((void*)doc->text);
        doc->text = strdup(text);
        doc->text_len = (int)strlen(text);
        doc->version = version;
        doc->ast_valid = false;
    } else {
        doc = add_document(server, uri, text, version);
    }
    server->diagnostics_dirty = true;
}

void lsp_did_change(LspServer *server, const char *uri, const char *text, int version) {
    lsp_did_open(server, uri, text, version);
}

void lsp_did_close(LspServer *server, const char *uri) {
    for (int i = 0; i < server->document_count; i++) {
        if (strcmp(server->documents[i]->uri, uri) == 0) {
            free((void*)server->documents[i]->uri);
            free((void*)server->documents[i]->text);
            free(server->documents[i]);
            server->documents[i] = server->documents[--server->document_count];
            break;
        }
    }
}

// ─── Parsing for LSP ─────────────────────────────────────────────────────────

static void lsp_parse_document(LspServer *server, LspDocument *doc) {
    if (doc->ast_valid) return;

    // Create a fresh context for parsing
    CompileOptions opts;
    memset(&opts, 0, sizeof(opts));

    Context *ctx = ket_context_create(&opts);
    ctx->files = (SourceFile*)arena_alloc_zero(ctx->arena, sizeof(SourceFile));
    ctx->files[0].path = doc->uri;
    ctx->files[0].source = doc->text;
    ctx->files[0].length = doc->text_len;
    ctx->file_count = 1;

    Lexer lexer;
    lexer_init(&lexer, doc->uri, doc->text, doc->text_len, ctx);

    Parser parser;
    parser_init(&parser, &lexer, ctx);

    doc->ast = parse_program(&parser);

    if (parser.error_count > 0) {
        doc->ast_valid = false;
    } else {
        doc->ast_valid = true;
    }

    // Store ctx for later use (diagnostics, completions)
    // In production, cache this
    (void)ctx;
}

// ─── Position conversion ──────────────────────────────────────────────────────

static LspPosition offset_to_position(const char *text, int offset) {
    LspPosition pos = {0, 0};
    for (int i = 0; i < offset && text[i]; i++) {
        if (text[i] == '\n') {
            pos.line++;
            pos.character = 0;
        } else {
            pos.character++;
        }
    }
    return pos;
}

static int position_to_offset(const char *text, LspPosition pos) {
    int line = 0, col = 0;
    for (int i = 0; text[i]; i++) {
        if (line == pos.line && col == pos.character) return i;
        if (text[i] == '\n') { line++; col = 0; }
        else { col++; }
    }
    return 0;
}

// ─── Diagnostics ──────────────────────────────────────────────────────────────

LspDiagnostic *lsp_compute_diagnostics(LspServer *server, const char *uri, int *count) {
    if (count) *count = 0;

    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    lsp_parse_document(server, doc);
    server->diagnostics_dirty = false;

    if (!doc->ast_valid && count) {
        // Report parse error as diagnostic
        LspDiagnostic *diags = (LspDiagnostic*)calloc(1, sizeof(LspDiagnostic));
        diags[0].range.start = (LspPosition){0, 0};
        diags[0].range.end = (LspPosition){0, 10};
        diags[0].severity = 1;  // Error
        diags[0].message = "Parse error";
        diags[0].source = "ketamine";
        *count = 1;
        return diags;
    }

    // For now, return empty diagnostics if AST is valid
    if (count) *count = 0;
    return NULL;
}

// ─── Completions ──────────────────────────────────────────────────────────────

static bool is_keyword_start(const char *word) {
    static const char *keywords[] = {
        "fn", "let", "mut", "if", "else", "while", "for", "in", "loop",
        "return", "break", "continue", "match", "struct", "enum", "trait",
        "impl", "type", "import", "pub", "const", "static", "extern",
        "unsafe", "async", "await", "move", "ref", "true", "false",
        "null", "void", "where", "as", "use", "mod", "self", "super",
        "crate", "abstract", "virtual", "override", "inline", "final",
        NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if (strncmp(keywords[i], word, strlen(word)) == 0) return true;
    }
    return false;
}

CompletionItem *lsp_complete(LspServer *server, const char *uri,
                             LspPosition pos, int *count) {
    if (!count) return NULL;
    *count = 0;

    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    lsp_parse_document(server, doc);

    // Get the word at the current position for prefix matching
    int offset = position_to_offset(doc->text, pos);
    (void)offset;

    // Build completion list based on context
    int cap = 128;
    CompletionItem *items = (CompletionItem*)calloc(cap, sizeof(CompletionItem));

    // Add keyword completions
    static const char *keywords[] = {
        "fn", "let", "mut", "if", "else", "while", "for", "loop",
        "return", "break", "continue", "match", "struct", "enum",
        "trait", "impl", "type", "import", "pub", "const", "static",
        "extern", "unsafe", "async", "true", "false", "null", "void",
        "where", "as", "move", "ref", "in", "use", "mod", "self",
        "super", "crate",
        NULL
    };

    for (int i = 0; keywords[i] && *count < cap; i++) {
        items[*count].label = keywords[i];
        items[*count].kind = CIT_KEYWORD;
        items[*count].detail = "keyword";
        items[*count].priority = 1;
        (*count)++;
    }

    // Add built-in types
    static const char *types[] = {
        "int", "float", "bool", "str", "void", "byte", "char",
        "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
        "f32", "f64", "never",
        NULL
    };

    for (int i = 0; types[i] && *count < cap; i++) {
        items[*count].label = types[i];
        items[*count].kind = CIT_STRUCT;
        items[*count].detail = "built-in type";
        items[*count].priority = 2;
        (*count)++;
    }

    // Add standard library types from the AST
    if (doc->ast && doc->ast_valid) {
        for (int i = 0; i < doc->ast->block.stmt_count && *count < cap; i++) {
            ASTNode *node = doc->ast->block.stmts[i];
            if (node->kind == N_STRUCT_DECL) {
                items[*count].label = node->decl.decl_name;
                items[*count].kind = CIT_STRUCT;
                items[*count].detail = "struct";
                items[*count].priority = 3;
                (*count)++;
            } else if (node->kind == N_ENUM_DECL) {
                items[*count].label = node->decl.decl_name;
                items[*count].kind = CIT_ENUM;
                items[*count].detail = "enum";
                items[*count].priority = 3;
                (*count)++;
            } else if (node->kind == N_FN_DECL) {
                items[*count].label = node->fn_decl.fn_name;
                items[*count].kind = CIT_FUNCTION;
                items[*count].detail = "function";
                items[*count].priority = 3;
                (*count)++;
            } else if (node->kind == N_TRAIT_DECL) {
                items[*count].label = node->decl.decl_name;
                items[*count].kind = CIT_CLASS;
                items[*count].detail = "trait";
                items[*count].priority = 3;
                (*count)++;
            }
        }
    }

    return items;
}

// ─── Hover ────────────────────────────────────────────────────────────────────

const char *lsp_hover(LspServer *server, const char *uri, LspPosition pos) {
    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    lsp_parse_document(server, doc);

    int offset = position_to_offset(doc->text, pos);

    // Extract the word at position
    int start = offset;
    while (start > 0 && (isalnum((unsigned char)doc->text[start - 1]) ||
           doc->text[start - 1] == '_')) start--;

    int end = offset;
    while (doc->text[end] && (isalnum((unsigned char)doc->text[end]) ||
           doc->text[end] == '_')) end++;

    if (start == end) return NULL;

    // Copy the word
    int len = end - start;
    char *word = (char*)calloc(len + 1, 1);
    memcpy(word, doc->text + start, len);

    // Look up the word in the AST
    const char *hover_text = NULL;

    if (doc->ast && doc->ast_valid) {
        for (int i = 0; i < doc->ast->block.stmt_count; i++) {
            ASTNode *node = doc->ast->block.stmts[i];

            if (node->kind == N_STRUCT_DECL &&
                strcmp(node->decl.decl_name, word) == 0) {
                char *buf = (char*)calloc(4096, 1);
                snprintf(buf, 4096,
                    "```ketamine\nstruct %s", node->decl.decl_name);
                if (node->decl.decl_generic_count > 0) {
                    strcat(buf, "<");
                    for (int j = 0; j < node->decl.decl_generic_count; j++) {
                        if (j > 0) strcat(buf, ", ");
                        strcat(buf, node->decl.decl_generics[j]->generic.generic_name);
                    }
                    strcat(buf, ">");
                }
                strcat(buf, "\n```\n");
                hover_text = buf;
            }
            else if (node->kind == N_FN_DECL &&
                     strcmp(node->fn_decl.fn_name, word) == 0) {
                char *buf = (char*)calloc(4096, 1);
                snprintf(buf, 4096,
                    "```ketamine\nfn %s(", node->fn_decl.fn_name);
                for (int j = 0; j < node->fn_decl.fn_param_count; j++) {
                    if (j > 0) strcat(buf, ", ");
                    ASTNode *p = node->fn_decl.fn_params[j];
                    if (p && p->kind == N_VAR_DECL) {
                        strcat(buf, p->var_decl.var_name);
                    }
                }
                strcat(buf, ")");
                if (node->fn_decl.fn_ret_type) {
                    strcat(buf, " -> ");
                    // Append return type string
                    strcat(buf, "?");
                }
                strcat(buf, "\n```\n");
                hover_text = buf;
            }
            else if (node->kind == N_ENUM_DECL &&
                     strcmp(node->decl.decl_name, word) == 0) {
                char *buf = (char*)calloc(2048, 1);
                snprintf(buf, 2048,
                    "```ketamine\nenum %s\n```",
                    node->decl.decl_name);
                hover_text = buf;
            }
        }
    }

    if (!hover_text) {
        // Check if it's a keyword
        if (is_keyword_start(word)) {
            char *buf = (char*)calloc(512, 1);
            snprintf(buf, 512, "**%s** — keyword", word);
            hover_text = buf;
        }
    }

    free(word);
    return hover_text;
}

// ─── Go To Definition ─────────────────────────────────────────────────────────

LspLocation *lsp_goto_definition(LspServer *server, const char *uri,
                                  LspPosition pos, int *count) {
    if (count) *count = 0;

    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    lsp_parse_document(server, doc);

    int offset = position_to_offset(doc->text, pos);

    // Extract word
    int start = offset;
    while (start > 0 && (isalnum((unsigned char)doc->text[start - 1]) ||
           doc->text[start - 1] == '_')) start--;

    int end = offset;
    while (doc->text[end] && (isalnum((unsigned char)doc->text[end]) ||
           doc->text[end] == '_')) end++;

    if (start == end) return NULL;

    int len = end - start;
    char *word = (char*)calloc(len + 1, 1);
    memcpy(word, doc->text + start, len);

    // Search AST for the definition
    LspLocation *loc = NULL;

    if (doc->ast && doc->ast_valid) {
        for (int i = 0; i < doc->ast->block.stmt_count; i++) {
            ASTNode *node = doc->ast->block.stmts[i];

            const char *decl_name = NULL;
            if (node->kind == N_STRUCT_DECL || node->kind == N_ENUM_DECL ||
                node->kind == N_TRAIT_DECL) {
                decl_name = node->decl.decl_name;
            } else if (node->kind == N_FN_DECL) {
                decl_name = node->fn_decl.fn_name;
            }

            if (decl_name && strcmp(decl_name, word) == 0) {
                if (count) *count = 1;
                loc = (LspLocation*)calloc(1, sizeof(LspLocation));
                loc->uri = doc->uri;
                loc->range.start = offset_to_position(doc->text, node->span.start);
                loc->range.end = offset_to_position(doc->text, node->span.end);
                break;
            }
        }
    }

    free(word);
    return loc;
}

// ─── Find References ──────────────────────────────────────────────────────────

LspLocation *lsp_find_references(LspServer *server, const char *uri,
                                  LspPosition pos, int *count) {
    // Simplified: find all occurrences of the word at position
    if (count) *count = 0;

    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    int offset = position_to_offset(doc->text, pos);

    // Extract word
    int start = offset;
    while (start > 0 && (isalnum((unsigned char)doc->text[start - 1]) ||
           doc->text[start - 1] == '_')) start--;

    int end = offset;
    while (doc->text[end] && (isalnum((unsigned char)doc->text[end]) ||
           doc->text[end] == '_')) end++;

    if (start == end) return NULL;

    int len = end - start;
    char *word = (char*)calloc(len + 1, 1);
    memcpy(word, doc->text + start, len);

    // Scan text for all occurrences
    int cap = 64;
    LspLocation *refs = (LspLocation*)calloc(cap, sizeof(LspLocation));
    int ref_count = 0;

    const char *p = doc->text;
    int scan_pos = 0;
    while (*p && ref_count < cap) {
        if (strncmp(p, word, len) == 0 &&
            (p == doc->text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
            !(isalnum((unsigned char)p[len]) || p[len] == '_')) {
            refs[ref_count].uri = doc->uri;
            refs[ref_count].range.start = offset_to_position(doc->text, scan_pos);
            refs[ref_count].range.end = offset_to_position(doc->text, scan_pos + len);
            ref_count++;
        }
        p++;
        scan_pos++;
    }

    *count = ref_count;
    free(word);
    return refs;
}

// ─── Document Symbols ─────────────────────────────────────────────────────────

SymbolInfo *lsp_document_symbols(LspServer *server, const char *uri, int *count) {
    if (count) *count = 0;

    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    lsp_parse_document(server, doc);
    if (!doc->ast || !doc->ast_valid) return NULL;

    int cap = 64;
    SymbolInfo *syms = (SymbolInfo*)calloc(cap, sizeof(SymbolInfo));
    int sym_count = 0;

    for (int i = 0; i < doc->ast->block.stmt_count && sym_count < cap; i++) {
        ASTNode *node = doc->ast->block.stmts[i];

        if (node->kind == N_FN_DECL) {
            syms[sym_count].name = node->fn_decl.fn_name;
            syms[sym_count].kind = SKT_FUNCTION;
            syms[sym_count].range.start = offset_to_position(doc->text, node->span.start);
            syms[sym_count].range.end = offset_to_position(doc->text, node->span.end);
            syms[sym_count].selection_range = syms[sym_count].range;
            sym_count++;
        }
        else if (node->kind == N_STRUCT_DECL) {
            syms[sym_count].name = node->decl.decl_name;
            syms[sym_count].kind = SKT_STRUCT;
            syms[sym_count].range.start = offset_to_position(doc->text, node->span.start);
            syms[sym_count].range.end = offset_to_position(doc->text, node->span.end);
            syms[sym_count].selection_range = syms[sym_count].range;
            sym_count++;
        }
        else if (node->kind == N_ENUM_DECL) {
            syms[sym_count].name = node->decl.decl_name;
            syms[sym_count].kind = SKT_ENUM;
            syms[sym_count].range.start = offset_to_position(doc->text, node->span.start);
            syms[sym_count].range.end = offset_to_position(doc->text, node->span.end);
            syms[sym_count].selection_range = syms[sym_count].range;
            sym_count++;
        }
        else if (node->kind == N_TRAIT_DECL) {
            syms[sym_count].name = node->decl.decl_name;
            syms[sym_count].kind = SKT_INTERFACE;
            syms[sym_count].range.start = offset_to_position(doc->text, node->span.start);
            syms[sym_count].range.end = offset_to_position(doc->text, node->span.end);
            syms[sym_count].selection_range = syms[sym_count].range;
            sym_count++;
        }
        else if (node->kind == N_IMPL_BLOCK) {
            // Could add impl blocks as symbols too
        }
    }

    *count = sym_count;
    return syms;
}

// ─── Format Document ──────────────────────────────────────────────────────────

const char *lsp_format_document(LspServer *server, const char *uri) {
    LspDocument *doc = find_document(server, uri);
    if (!doc) return NULL;

    // Simple formatter: ensure consistent indentation
    // This is a basic pass - production would use a proper formatter
    const char *src = doc->text;
    int cap = doc->text_len * 2 + 1024;
    char *result = (char*)calloc(cap, 1);
    int ri = 0;
    int indent = 0;
    bool new_line = true;

    for (int i = 0; src[i] && ri < cap - 16; i++) {
        char c = src[i];

        if (new_line) {
            for (int j = 0; j < indent && ri < cap - 2; j++) {
                result[ri++] = ' ';
                result[ri++] = ' ';
            }
            new_line = false;
        }

        if (c == '\n') {
            result[ri++] = '\n';
            new_line = true;
        } else if (c == '{') {
            result[ri++] = c;
            indent++;
            if (src[i + 1] != '\n') {
                result[ri++] = '\n';
                new_line = true;
            }
        } else if (c == '}') {
            indent--;
            if (indent < 0) indent = 0;
            if (ri > 0 && result[ri - 1] == '\n') {
                // Already on new line
            } else if (ri > 0 && result[ri - 1] != ' ') {
                result[ri++] = '\n';
                new_line = true;
            }
            result[ri++] = c;
        } else {
            result[ri++] = c;
        }
    }

    result[ri] = '\0';
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// LSP MESSAGE HANDLING
// ═══════════════════════════════════════════════════════════════════════════════

bool lsp_handle_message(LspServer *server, const char *json_msg) {
    if (!server || !json_msg) return false;

    char method[256] = {0};
    int id = -1;

    // Extract method
    const char *p = json_find_key(json_msg, "method");
    if (p) {
        p = json_read_string(p, method, sizeof(method));
    }

    // Extract id
    p = json_find_key(json_msg, "id");
    if (p) {
        p = json_read_int(p, &id);
    }

    // Handle based on method
    if (strcmp(method, "initialize") == 0) {
        server->initialized = true;
        lsp_send_response(server, id,
            "{\"capabilities\":{"
            "\"textDocumentSync\":2,"  // Incremental
            "\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
            "\"hoverProvider\":true,"
            "\"definitionProvider\":true,"
            "\"referencesProvider\":true,"
            "\"documentSymbolProvider\":true,"
            "\"documentFormattingProvider\":true"
            "}}");
    }
    else if (strcmp(method, "initialized") == 0) {
        // Client acknowledged initialization — send open document diagnostics
        lsp_send_notification(server, "window/logMessage",
            "{\"type\":3,\"message\":\"Ketamine LSP server initialized\"}");
    }
    else if (strcmp(method, "textDocument/didOpen") == 0 ||
             strcmp(method, "textDocument/didChange") == 0) {
        // Extract URI and text
        char uri[1024] = {0};
        char text[65536] = {0};
        int version = 0;

        p = json_find_key(json_msg, "textDocument");
        if (p) {
            const char *u = json_find_key(p, "uri");
            if (u) json_read_string(u, uri, sizeof(uri));

            const char *v = json_find_key(p, "version");
            if (v) json_read_int(v, &version);
        }

        // For didChange, text comes from contentChanges[0].text
        if (strcmp(method, "textDocument/didChange") == 0) {
            p = json_find_key(json_msg, "contentChanges");
            if (p) {
                // Find first array element
                p = json_skip_ws(p);
                if (*p == '[') p++;
                const char *t = json_find_key(p, "text");
                if (t) json_read_string(t, text, sizeof(text));
            }
        } else {
            p = json_find_key(json_msg, "textDocument");
            if (p) {
                p = json_find_key(p, "text");
                if (p) json_read_string(p, text, sizeof(text));
            }
        }

        if (uri[0]) {
            lsp_did_change(server, uri, text, version);

            // Compute and publish diagnostics
            if (server->diagnostics_dirty) {
                int diag_count = 0;
                LspDiagnostic *diags = lsp_compute_diagnostics(server, uri, &diag_count);

                // Build diagnostics JSON
                char diag_json[65536];
                int dj = snprintf(diag_json, sizeof(diag_json),
                    "{\"uri\":\"%s\",\"diagnostics\":[", uri);
                for (int i = 0; i < diag_count && dj < (int)sizeof(diag_json) - 128; i++) {
                    if (i > 0) diag_json[dj++] = ',';
                    dj += snprintf(diag_json + dj, sizeof(diag_json) - dj,
                        "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                        "\"end\":{\"line\":%d,\"character\":%d}},"
                        "\"severity\":%d,\"message\":\"%s\",\"source\":\"ketamine\"}",
                        diags[i].range.start.line, diags[i].range.start.character,
                        diags[i].range.end.line, diags[i].range.end.character,
                        diags[i].severity, diags[i].message);
                }
                snprintf(diag_json + dj, sizeof(diag_json) - dj, "]}");

                lsp_send_notification(server, "textDocument/publishDiagnostics", diag_json);
                free(diags);
            }
        }
    }
    else if (strcmp(method, "textDocument/didClose") == 0) {
        char uri[1024] = {0};
        p = json_find_key(json_msg, "textDocument");
        if (p) {
            p = json_find_key(p, "uri");
            if (p) json_read_string(p, uri, sizeof(uri));
        }
        if (uri[0]) lsp_did_close(server, uri);
    }
    else if (strcmp(method, "textDocument/completion") == 0) {
        char uri[1024] = {0};
        LspPosition pos = {0, 0};

        p = json_find_key(json_msg, "params");
        if (p) {
            const char *td = json_find_key(p, "textDocument");
            if (td) {
                const char *u = json_find_key(td, "uri");
                if (u) json_read_string(u, uri, sizeof(uri));
            }
            const char *pp = json_find_key(p, "position");
            if (pp) {
                const char *l = json_find_key(pp, "line");
                if (l) json_read_int(l, &pos.line);
                const char *c = json_find_key(pp, "character");
                if (c) json_read_int(c, &pos.character);
            }
        }

        if (uri[0]) {
            int complete_count = 0;
            CompletionItem *items = lsp_complete(server, uri, pos, &complete_count);

            char result[65536];
            int ri = snprintf(result, sizeof(result),
                "{\"isIncomplete\":false,\"items\":[");
            for (int i = 0; i < complete_count && ri < (int)sizeof(result) - 128; i++) {
                if (i > 0) result[ri++] = ',';
                ri += snprintf(result + ri, sizeof(result) - ri,
                    "{\"label\":\"%s\",\"kind\":%d,\"detail\":\"%s\"}",
                    items[i].label, (int)items[i].kind + 1,
                    items[i].detail ? items[i].detail : "");
            }
            snprintf(result + ri, sizeof(result) - ri, "]}");

            lsp_send_response(server, id, result);
            free(items);
        }
    }
    else if (strcmp(method, "textDocument/hover") == 0) {
        char uri[1024] = {0};
        LspPosition pos = {0, 0};

        p = json_find_key(json_msg, "params");
        if (p) {
            const char *td = json_find_key(p, "textDocument");
            if (td) {
                const char *u = json_find_key(td, "uri");
                if (u) json_read_string(u, uri, sizeof(uri));
            }
            const char *pp = json_find_key(p, "position");
            if (pp) {
                const char *l = json_find_key(pp, "line");
                if (l) json_read_int(l, &pos.line);
                const char *c = json_find_key(pp, "character");
                if (c) json_read_int(c, &pos.character);
            }
        }

        if (uri[0]) {
            const char *hover = lsp_hover(server, uri, pos);
            if (hover) {
                char result[65536];
                snprintf(result, sizeof(result),
                    "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}", hover);
                lsp_send_response(server, id, result);
                free((void*)hover);
            } else {
                lsp_send_response(server, id, NULL);
            }
        }
    }
    else if (strcmp(method, "textDocument/definition") == 0) {
        char uri[1024] = {0};
        LspPosition pos = {0, 0};

        p = json_find_key(json_msg, "params");
        if (p) {
            const char *td = json_find_key(p, "textDocument");
            if (td) {
                const char *u = json_find_key(td, "uri");
                if (u) json_read_string(u, uri, sizeof(uri));
            }
            const char *pp = json_find_key(p, "position");
            if (pp) {
                const char *l = json_find_key(pp, "line");
                if (l) json_read_int(l, &pos.line);
                const char *c = json_find_key(pp, "character");
                if (c) json_read_int(c, &pos.character);
            }
        }

        if (uri[0]) {
            int loc_count = 0;
            LspLocation *loc = lsp_goto_definition(server, uri, pos, &loc_count);
            if (loc && loc_count > 0) {
                char result[4096];
                snprintf(result, sizeof(result),
                    "[{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                    "\"end\":{\"line\":%d,\"character\":%d}}}]",
                    loc->uri, loc->range.start.line, loc->range.start.character,
                    loc->range.end.line, loc->range.end.character);
                lsp_send_response(server, id, result);
                free(loc);
            } else {
                lsp_send_response(server, id, NULL);
            }
        }
    }
    else if (strcmp(method, "textDocument/references") == 0) {
        char uri[1024] = {0};
        LspPosition pos = {0, 0};

        p = json_find_key(json_msg, "params");
        if (p) {
            const char *td = json_find_key(p, "textDocument");
            if (td) {
                const char *u = json_find_key(td, "uri");
                if (u) json_read_string(u, uri, sizeof(uri));
            }
            const char *pp = json_find_key(p, "position");
            if (pp) {
                const char *l = json_find_key(pp, "line");
                if (l) json_read_int(l, &pos.line);
                const char *c = json_find_key(pp, "character");
                if (c) json_read_int(c, &pos.character);
            }
        }

        if (uri[0]) {
            int ref_count = 0;
            LspLocation *refs = lsp_find_references(server, uri, pos, &ref_count);
            char result[131072];
            int ri = snprintf(result, sizeof(result), "[");
            for (int i = 0; i < ref_count && ri < (int)sizeof(result) - 128; i++) {
                if (i > 0) result[ri++] = ',';
                ri += snprintf(result + ri, sizeof(result) - ri,
                    "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                    "\"end\":{\"line\":%d,\"character\":%d}}}",
                    refs[i].uri, refs[i].range.start.line, refs[i].range.start.character,
                    refs[i].range.end.line, refs[i].range.end.character);
            }
            snprintf(result + ri, sizeof(result) - ri, "]");
            lsp_send_response(server, id, result);
            free(refs);
        }
    }
    else if (strcmp(method, "textDocument/documentSymbol") == 0) {
        char uri[1024] = {0};

        p = json_find_key(json_msg, "params");
        if (p) {
            const char *td = json_find_key(p, "textDocument");
            if (td) {
                const char *u = json_find_key(td, "uri");
                if (u) json_read_string(u, uri, sizeof(uri));
            }
        }

        if (uri[0]) {
            int sym_count = 0;
            SymbolInfo *syms = lsp_document_symbols(server, uri, &sym_count);
            char result[131072];
            int ri = snprintf(result, sizeof(result), "[");
            for (int i = 0; i < sym_count && ri < (int)sizeof(result) - 128; i++) {
                if (i > 0) result[ri++] = ',';
                ri += snprintf(result + ri, sizeof(result) - ri,
                    "{\"name\":\"%s\",\"kind\":%d,"
                    "\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                    "\"end\":{\"line\":%d,\"character\":%d}},"
                    "\"selectionRange\":{\"start\":{\"line\":%d,\"character\":%d},"
                    "\"end\":{\"line\":%d,\"character\":%d}}}",
                    syms[i].name, (int)syms[i].kind,
                    syms[i].range.start.line, syms[i].range.start.character,
                    syms[i].range.end.line, syms[i].range.end.character,
                    syms[i].selection_range.start.line, syms[i].selection_range.start.character,
                    syms[i].selection_range.end.line, syms[i].selection_range.end.character);
            }
            snprintf(result + ri, sizeof(result) - ri, "]");
            lsp_send_response(server, id, result);
            free(syms);
        }
    }
    else if (strcmp(method, "textDocument/formatting") == 0) {
        char uri[1024] = {0};

        p = json_find_key(json_msg, "params");
        if (p) {
            const char *td = json_find_key(p, "textDocument");
            if (td) {
                const char *u = json_find_key(td, "uri");
                if (u) json_read_string(u, uri, sizeof(uri));
            }
        }

        if (uri[0]) {
            const char *formatted = lsp_format_document(server, uri);
            if (formatted) {
                char result[65536];
                snprintf(result, sizeof(result),
                    "[{\"range\":{\"start\":{\"line\":0,\"character\":0},"
                    "\"end\":{\"line\":999999,\"character\":0}},"
                    "\"newText\":\"%s\"}]", formatted);
                lsp_send_response(server, id, result);
                free((void*)formatted);
            }
        }
    }
    else if (strcmp(method, "shutdown") == 0) {
        server->shutdown = true;
        lsp_send_response(server, id, NULL);
    }
    else if (strcmp(method, "exit") == 0) {
        return false;  // Signal to exit
    }
    else {
        // Method not recognized — return null result
        if (id >= 0) {
            lsp_send_response(server, id, NULL);
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SERVER MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════════

LspServer *lsp_server_new(void) {
    LspServer *server = (LspServer*)calloc(1, sizeof(LspServer));
    server->documents = NULL;
    server->document_count = 0;
    server->document_cap = 0;
    server->initialized = false;
    server->shutdown = false;
    server->diagnostics_dirty = false;
    server->input_fd = 0;   // stdin
    server->output_fd = 1;  // stdout
    return server;
}

int lsp_server_run(LspServer *server) {
    if (!server) return 1;

    // Set stdin/stdout to binary mode on Windows
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    char buf[1048576];  // 1MB buffer

    while (!server->shutdown) {
        int n = read_frame(server->input_fd, buf, sizeof(buf));
        if (n <= 0) break;

        bool cont = lsp_handle_message(server, buf);
        if (!cont) break;
    }

    return 0;
}

void lsp_server_free(LspServer *server) {
    if (!server) return;

    for (int i = 0; i < server->document_count; i++) {
        free((void*)server->documents[i]->uri);
        free((void*)server->documents[i]->text);
        free(server->documents[i]);
    }
    free(server->documents);
    free(server);
}

// ─── WASM Backend ─────────────────────────────────────────────────────────────
