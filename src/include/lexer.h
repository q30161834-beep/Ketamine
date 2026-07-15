#ifndef KETAMINE_LEXER_H
#define KETAMINE_LEXER_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// LEXER — Tokenizer with full Unicode support, error recovery, and
//          integrated preprocessor-like features (line directives, etc.)
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Lexer State Machine ──────────────────────────────────────────────────────
typedef enum {
    LEX_NORMAL,
    LEX_STRING,
    LEX_RAW_STRING,
    LEX_CHAR,
    LEX_LINE_COMMENT,
    LEX_BLOCK_COMMENT,
    LEX_PREPROC,
    LEX_TEMPLATE,       // template literal ${}
} LexerState;

// ─── Lexer Mode ───────────────────────────────────────────────────────────────
typedef enum {
    LEX_MODE_NORMAL,
    LEX_MODE_TEMPLATE,      // inside ${}
    LEX_MODE_TYPE_ONLY,     // only parse types (for tools)
    LEX_MODE_PATTERN_ONLY,  // only parse patterns
} LexerMode;

// ─── Lexer ────────────────────────────────────────────────────────────────────
typedef struct Lexer {
    const char  *filename;
    const char  *start;         // start of source
    const char  *pos;           // current position
    const char  *token_start;   // start of current token
    int          length;        // source length
    int          line;
    int          col;

    // Current / Peek tokens
    Token        current;
    Token        peek;

    // Lexer state
    LexerState   state;
    LexerMode    mode;
    int          paren_depth;   // for template lexing
    int          brace_depth;   // for template lexing

    // Error recovery
    int          error_count;
    bool         sync_point;    // resync after error

    // Context
    Context     *ctx;

    // UTF-8 decoder state
    uint32_t     pending_codepoint;
    int          pending_bytes;

    // Position tracking (for #line directives)
    int          source_line;
    int          source_col;

    // Macro tracking
    bool         in_macro;

    // Raw string delimiter
    int          raw_delim_len;     // #"..."# delimiter count
} Lexer;

// ─── Unicode Database (minimal) ───────────────────────────────────────────────
typedef struct {
    uint32_t    low;
    uint32_t    high;
} UnicodeRange;

// ─── API ──────────────────────────────────────────────────────────────────────

// Initialize lexer with source text
void     lexer_init(Lexer *lexer, const char *filename,
                    const char *source, int length, Context *ctx);

// Advance to next token, return current
Token    lexer_next(Lexer *lexer);

// Peek at next token without consuming
Token    lexer_peek(Lexer *lexer);

// Consume token if it matches, return true. Otherwise return false.
bool     lexer_match(Lexer *lexer, TokenType type);

// Expect a specific token type, emit error if mismatch
Token    lexer_expect(Lexer *lexer, TokenType type, const char *msg);

// Synchronize after error (skip to statement boundary)
void     lexer_sync(Lexer *lexer);

// Get current position
Location lexer_location(Lexer *lexer);

// Span from a starting location to current position
Span     lexer_span(Lexer *lexer, Location start);

// Dump all tokens to stdout (debug)
void     lexer_dump(Lexer *lexer);

// Convert token type to string (for error messages)
const char *token_type_name(TokenType type);

// Check if token type is an operator
bool     token_is_operator(TokenType type);
bool     token_is_assignment(TokenType type);
bool     token_is_keyword(TokenType type);
bool     token_is_type_keyword(TokenType type);
bool     token_is_literal(TokenType type);

// ─── Unicode Helpers ──────────────────────────────────────────────────────────

// Decode a single UTF-8 codepoint, return codepoint, advance pos
uint32_t utf8_decode(const char **pos, const char *end);

// Encode a codepoint to UTF-8 buffer, return bytes written
int      utf8_encode(uint32_t cp, char buf[5]);

// Check if codepoint is valid in an identifier
bool     is_ident_start(uint32_t cp);
bool     is_ident_continue(uint32_t cp);

// Check if codepoint is whitespace
bool     is_whitespace(uint32_t cp);

#endif // KETAMINE_LEXER_H
