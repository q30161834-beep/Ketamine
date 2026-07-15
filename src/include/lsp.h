#ifndef KETAMINE_LSP_H
#define KETAMINE_LSP_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// LSP SERVER — Language Server Protocol implementation
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Position ─────────────────────────────────────────────────────────────────
typedef struct {
    int line;       // 0-based
    int character;  // 0-based
} LspPosition;

// ─── Range ────────────────────────────────────────────────────────────────────
typedef struct {
    LspPosition start;
    LspPosition end;
} LspRange;

// ─── Location ─────────────────────────────────────────────────────────────────
typedef struct {
    const char *uri;
    LspRange    range;
} LspLocation;

// ─── Completion Item ──────────────────────────────────────────────────────────
typedef enum {
    CIT_TEXT,
    CIT_METHOD,
    CIT_FUNCTION,
    CIT_CONSTRUCTOR,
    CIT_FIELD,
    CIT_VARIABLE,
    CIT_CLASS,
    CIT_STRUCT,
    CIT_ENUM,
    CIT_KEYWORD,
    CIT_SNIPPET,
} CompletionItemKind;

typedef struct {
    const char         *label;
    CompletionItemKind  kind;
    const char         *detail;
    const char         *documentation;
    const char         *insert_text;
    int                 priority;
} CompletionItem;

// ─── Diagnostic ───────────────────────────────────────────────────────────────
typedef struct {
    LspRange    range;
    int         severity;     // 1=error, 2=warning, 3=info, 4=hint
    const char *message;
    const char *source;
} LspDiagnostic;

// ─── Document ─────────────────────────────────────────────────────────────────
typedef struct {
    const char   *uri;
    const char   *path;
    const char   *text;
    int           text_len;
    int           version;
    ASTNode      *ast;
    bool          ast_valid;
} LspDocument;

// ─── Symbol Information ───────────────────────────────────────────────────────
typedef enum {
    SKT_FILE,
    SKT_MODULE,
    SKT_NAMESPACE,
    SKT_PACKAGE,
    SKT_CLASS,
    SKT_METHOD,
    SKT_PROPERTY,
    SKT_FIELD,
    SKT_CONSTRUCTOR,
    SKT_ENUM,
    SKT_INTERFACE,
    SKT_FUNCTION,
    SKT_VARIABLE,
    SKT_CONSTANT,
    SKT_STRING,
    SKT_NUMBER,
    SKT_BOOLEAN,
    SKT_ARRAY,
    SKT_OBJECT,
    SKT_KEY,
    SKT_NULL,
    SKT_ENUM_MEMBER,
    SKT_STRUCT,
    SKT_EVENT,
    SKT_OPERATOR,
    SKT_TYPE_PARAMETER,
} SymbolKind;

typedef struct {
    const char  *name;
    SymbolKind   kind;
    LspRange     range;
    LspRange     selection_range;
    const char  *container_name;
} SymbolInfo;

// ─── LSP Server State ─────────────────────────────────────────────────────────
typedef struct LspServer LspServer;

struct LspServer {
    // Documents tracked by URI
    LspDocument **documents;
    int           document_count;
    int           document_cap;

    // Current compilation context
    Context   *ctx;

    // Parser state (reused)
    void      *parser;

    // Server flags
    bool       initialized;
    bool       shutdown;
    int        client_capabilities;

    // Pending diagnostics
    bool       diagnostics_dirty;

    // IO
    int        input_fd;
    int        output_fd;
};

// ═══════════════════════════════════════════════════════════════════════════════
// API
// ═══════════════════════════════════════════════════════════════════════════════

/// Create a new LSP server instance
LspServer *lsp_server_new(void);

/// Start the LSP server (stdin/stdout JSON-RPC loop)
int lsp_server_run(LspServer *server);

/// Handle a single LSP message (JSON-RPC)
bool lsp_handle_message(LspServer *server, const char *json_msg);

/// Open a document (textDocument/didOpen)
void lsp_did_open(LspServer *server, const char *uri, const char *text, int version);

/// Change a document (textDocument/didChange)
void lsp_did_change(LspServer *server, const char *uri, const char *text, int version);

/// Close a document (textDocument/didClose)
void lsp_did_close(LspServer *server, const char *uri);

/// Compute diagnostics for a document
LspDiagnostic *lsp_compute_diagnostics(LspServer *server, const char *uri, int *count);

/// Compute completions at a position
CompletionItem *lsp_complete(LspServer *server, const char *uri,
                             LspPosition pos, int *count);

/// Provide hover information at a position
const char *lsp_hover(LspServer *server, const char *uri, LspPosition pos);

/// Go to definition
LspLocation *lsp_goto_definition(LspServer *server, const char *uri,
                                  LspPosition pos, int *count);

/// Find references
LspLocation *lsp_find_references(LspServer *server, const char *uri,
                                  LspPosition pos, int *count);

/// Document symbols
SymbolInfo *lsp_document_symbols(LspServer *server, const char *uri, int *count);

/// Format document
const char *lsp_format_document(LspServer *server, const char *uri);

/// Send a notification to the client
void lsp_send_notification(LspServer *server, const char *method, const char *params);

/// Send a response to the client
void lsp_send_response(LspServer *server, int id, const char *result);

/// Send an error response
void lsp_send_error(LspServer *server, int id, int code, const char *message);

/// Free the LSP server
void lsp_server_free(LspServer *server);

#endif // KETAMINE_LSP_H
