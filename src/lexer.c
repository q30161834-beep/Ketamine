#include "include/lexer.h"
#include "include/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════════
// KEYWORD TABLE
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct { const char *word; TokenType type; bool is_type; } KeywordEntry;

static const KeywordEntry keywords[] = {
    {"fn",         KW_FN,        false},
    {"let",        KW_LET,       false},
    {"mut",        KW_MUT,       false},
    {"const",      KW_CONST,     false},
    {"return",     KW_RETURN,    false},
    {"if",         KW_IF,        false},
    {"else",       KW_ELSE,      false},
    {"while",      KW_WHILE,     false},
    {"for",        KW_FOR,       false},
    {"in",         KW_IN,        false},
    {"loop",       KW_LOOP,      false},
    {"break",      KW_BREAK,     false},
    {"continue",   KW_CONTINUE,  false},
    {"struct",     KW_STRUCT,    false},
    {"enum",       KW_ENUM,      false},
    {"impl",       KW_IMPL,      false},
    {"match",      KW_MATCH,     false},
    {"import",     KW_IMPORT,    false},
    {"pub",        KW_PUB,       false},
    {"self",       KW_SELF,      false},
    {"super",      KW_SUPER,     false},
    {"type",       KW_TYPE,      false},
    {"trait",      KW_TRAIT,     false},
    {"where",      KW_WHERE,     false},
    {"as",         KW_AS,        false},
    {"use",        KW_USE,       false},
    {"mod",        KW_MOD,       false},
    {"ref",        KW_REF,       false},
    {"move",       KW_MOVE,      false},
    {"static",     KW_STATIC,    false},
    {"extern",     KW_EXTERN,    false},
    {"unsafe",     KW_UNSAFE,    false},
    {"async",      KW_ASYNC,     false},
    {"await",      KW_AWAIT,     false},
    {"yield",      KW_YIELD,     false},
    {"abstract",   KW_ABSTRACT,  false},
    {"virtual",    KW_VIRTUAL,   false},
    {"override",   KW_OVERRIDE,  false},
    {"final",      KW_FINAL,     false},
    {"default",    KW_DEFAULT,   false},
    {"macro",      KW_MACRO,     false},
    {"inline",     KW_INLINE,    false},
    {"alias",      KW_ALIAS,     false},
    {"delegate",   KW_DELEGATE,  false},
    {"checked",    KW_CHECKED,   false},
    {"unchecked",  KW_UNCHECKED, false},
    {"true",       TK_BOOL_LIT,  false},
    {"false",      TK_BOOL_LIT,  false},
    {"null",       TK_NULL_LIT,  false},

    // Type keywords
    {"void",       TK_TYPE_VOID,   true},
    {"bool",       TK_TYPE_BOOL,   true},
    {"byte",       TK_TYPE_BYTE,   true},
    {"int",        TK_TYPE_INT,    true},
    {"i8",         TK_TYPE_I8,     true},
    {"i16",        TK_TYPE_I16,    true},
    {"i32",        TK_TYPE_I32,    true},
    {"i64",        TK_TYPE_I64,    true},
    {"i128",       TK_TYPE_I128,   true},
    {"u8",         TK_TYPE_U8,     true},
    {"u16",        TK_TYPE_U16,    true},
    {"u32",        TK_TYPE_U32,    true},
    {"u64",        TK_TYPE_U64,    true},
    {"u128",       TK_TYPE_U128,   true},
    {"f16",        TK_TYPE_F16,    true},
    {"f32",        TK_TYPE_F32,    true},
    {"f64",        TK_TYPE_F64,    true},
    {"f128",       TK_TYPE_F128,   true},
    {"str",        TK_TYPE_STR,    true},
    {"char",       TK_TYPE_CHAR,   true},
    {"never",      TK_TYPE_NEVER,  true},
    {"Self",       TK_TYPE_SELF,   true},
    {"auto",       TK_TYPE_AUTO,   true},
    {NULL, 0, false}
};

// ═══════════════════════════════════════════════════════════════════════════════
// UTF-8 DECODER
// ═══════════════════════════════════════════════════════════════════════════════

uint32_t utf8_decode(const char **pos, const char *end) {
    if (*pos >= end) return 0;

    uint8_t c = (uint8_t)**pos;
    (*pos)++;

    // ASCII
    if (c < 0x80) return c;

    // Continuation byte without start
    if (c < 0xC0) return 0xFFFD;

    uint32_t cp;
    int extra;

    if (c < 0xE0)       { cp = c & 0x1F; extra = 1; }
    else if (c < 0xF0)  { cp = c & 0x0F; extra = 2; }
    else if (c < 0xF8)  { cp = c & 0x07; extra = 3; }
    else return 0xFFFD;

    for (int i = 0; i < extra; i++) {
        if (*pos >= end) return 0xFFFD;
        uint8_t b = (uint8_t)**pos;
        if ((b & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (b & 0x3F);
        (*pos)++;
    }

    return cp;
}

int utf8_encode(uint32_t cp, char buf[5]) {
    if (cp < 0x80) {
        buf[0] = cp; buf[1] = 0; return 1;
    } else if (cp < 0x800) {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        buf[2] = 0; return 2;
    } else if (cp < 0x10000) {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        buf[3] = 0; return 3;
    } else {
        buf[0] = 0xF0 | (cp >> 18);
        buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F);
        buf[3] = 0x80 | (cp & 0x3F);
        buf[4] = 0; return 4;
    }
}

bool is_ident_start(uint32_t cp) {
    return (cp >= 'a' && cp <= 'z') ||
           (cp >= 'A' && cp <= 'Z') ||
           cp == '_' ||
           cp >= 0x80;  // non-ASCII
}

bool is_ident_continue(uint32_t cp) {
    return is_ident_start(cp) ||
           (cp >= '0' && cp <= '9');
}

bool is_whitespace(uint32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
           cp == 0x0B || cp == 0x0C;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TOKEN TYPE NAMES
// ═══════════════════════════════════════════════════════════════════════════════

const char *token_type_name(TokenType type) {
    static const char *names[] = {
        [TK_EOF]              = "end of file",
        [TK_ERROR]            = "error",
        [TK_INT_LIT]          = "integer literal",
        [TK_FLOAT_LIT]        = "float literal",
        [TK_STR_LIT]          = "string literal",
        [TK_RAW_STR_LIT]      = "raw string literal",
        [TK_BYTE_LIT]         = "byte literal",
        [TK_CHAR_LIT]         = "char literal",
        [TK_BOOL_LIT]         = "boolean literal",
        [TK_NULL_LIT]         = "null literal",
        [TK_IDENT]            = "identifier",
        [KW_FN]               = "'fn'",
        [KW_LET]              = "'let'",
        [KW_MUT]              = "'mut'",
        [KW_RETURN]           = "'return'",
        [KW_IF]               = "'if'",
        [KW_ELSE]             = "'else'",
        [KW_WHILE]            = "'while'",
        [KW_FOR]              = "'for'",
        [KW_IN]               = "'in'",
        [KW_LOOP]             = "'loop'",
        [KW_BREAK]            = "'break'",
        [KW_CONTINUE]         = "'continue'",
        [KW_STRUCT]           = "'struct'",
        [KW_ENUM]             = "'enum'",
        [KW_IMPL]             = "'impl'",
        [KW_MATCH]            = "'match'",
        [KW_IMPORT]           = "'import'",
        [KW_PUB]              = "'pub'",
        [KW_SELF]             = "'self'",
        [KW_SUPER]            = "'super'",
        [KW_TRAIT]            = "'trait'",
        [KW_WHERE]            = "'where'",
        [KW_AS]               = "'as'",
        [KW_UNSAFE]           = "'unsafe'",
        [KW_ASYNC]            = "'async'",
        [KW_AWAIT]            = "'await'",
        [KW_MACRO]            = "'macro'",
        [KW_INLINE]           = "'inline'",
        [KW_EXTERN]           = "'extern'",
        [KW_STATIC]           = "'static'",
        [KW_MOVE]             = "'move'",
        [KW_REF]              = "'ref'",
        [TK_PLUS]             = "'+'",
        [TK_MINUS]            = "'-'",
        [TK_STAR]             = "'*'",
        [TK_SLASH]            = "'/'",
        [TK_PERCENT]          = "'%'",
        [TK_POWER]            = "'**'",
        [TK_EQ]               = "'='",
        [TK_EQEQ]             = "'=='",
        [TK_BANGEQ]           = "'!='",
        [TK_LT]               = "'<'",
        [TK_GT]               = "'>'",
        [TK_LTEQ]             = "'<='",
        [TK_GTEQ]             = "'>='",
        [TK_AMPAMP]           = "'&&'",
        [TK_PIPEPIPE]         = "'||'",
        [TK_EXCLAM]           = "'!'",
        [TK_QUESTION]         = "'?'",
        [TK_AMPERSAND]        = "'&'",
        [TK_PIPE]             = "'|'",
        [TK_CARET]            = "'^'",
        [TK_TILDE]            = "'~'",
        [TK_LTLT]             = "'<<'",
        [TK_GTGT]             = "'>>'",
        [TK_ARROW]            = "'->'",
        [TK_FAT_ARROW]        = "'=>'",
        [TK_COLON]            = "':'",
        [TK_COLONCOLON]       = "'::'",
        [TK_SEMICOLON]        = "';'",
        [TK_COMMA]            = "','",
        [TK_DOT]              = "'.'",
        [TK_DOTDOT]           = "'..'",
        [TK_DOTDOTDOT]        = "'...'",
        [TK_LPAREN]           = "'('",
        [TK_RPAREN]           = "')'",
        [TK_LBRACKET]         = "'['",
        [TK_RBRACKET]         = "']'",
        [TK_LBRACE]           = "'{'",
        [TK_RBRACE]           = "'}'",
        [TK_AT]               = "'@'",
        [TK_POUND]            = "'#'",
        [TK_UNDERSCORE]       = "'_'",
        [TK_HASH]             = "'#'",
    };

    if (type >= 0 && type < TK_COUNT && names[type]) return names[type];
    return "unknown token";
}

bool token_is_operator(TokenType type) {
    return (type >= TK_PLUS && type <= TK_PIPE_FORWARD) ||
           (type >= TK_PLUSEQ && type <= TK_GTGTEQ);
}

bool token_is_assignment(TokenType type) {
    return type == TK_EQ ||
           (type >= TK_PLUSEQ && type <= TK_GTGTEQ);
}

bool token_is_keyword(TokenType type) {
    return (type >= KW_FN && type <= KW_UNCHECKED) ||
           (type >= TK_TYPE_VOID && type <= TK_TYPE_AUTO);
}

bool token_is_literal(TokenType type) {
    return type >= TK_INT_LIT && type <= TK_NULL_LIT;
}

bool token_is_type_keyword(TokenType type) {
    return type >= TK_TYPE_VOID && type <= TK_TYPE_AUTO;
}

// ═══════════════════════════════════════════════════════════════════════════════
// LEXER CORE
// ═══════════════════════════════════════════════════════════════════════════════

static char peek_char(Lexer *l) {
    if (l->pos >= l->start + l->length) return '\0';
    return *l->pos;
}

static char peek_char_next(Lexer *l, int n) {
    if (l->pos + n >= l->start + l->length) return '\0';
    return l->pos[n];
}

static char advance_char(Lexer *l) {
    if (l->pos >= l->start + l->length) return '\0';
    char c = *l->pos;
    l->pos++;
    if (c == '\n') { l->line++; l->col = 1; }
    else if (c == '\r') { /* skip */ }
    else { l->col++; }
    return c;
}

static Location make_loc(Lexer *l) {
    Location loc;
    loc.file = l->filename;
    loc.source = l->start;
    loc.offset = (int)(l->token_start - l->start);
    loc.line = l->line;
    loc.col = l->col;
    loc.length = (int)(l->pos - l->token_start);
    return loc;
}

static Token make_token(Lexer *l, TokenType type) {
    Token t;
    t.type = type;
    t.start = l->token_start;
    t.length = (int)(l->pos - l->token_start);
    t.loc = make_loc(l);
    t.ival = 0;
    t.fval = 0.0;
    t.overflow = false;
    return t;
}

static void skip_whitespace(Lexer *l) {
    for (;;) {
        char c = peek_char(l);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance_char(l);
        } else if (c == '/' && peek_char_next(l, 1) == '/') {
            // Line comment
            while (peek_char(l) && peek_char(l) != '\n') advance_char(l);
        } else if (c == '/' && peek_char_next(l, 1) == '*') {
            // Block comment (nestable)
            advance_char(l); advance_char(l); // skip /*
            int depth = 1;
            while (depth > 0 && peek_char(l)) {
                if (peek_char(l) == '/' && peek_char_next(l, 1) == '*') {
                    depth++;
                    advance_char(l); advance_char(l);
                } else if (peek_char(l) == '*' && peek_char_next(l, 1) == '/') {
                    depth--;
                    advance_char(l); advance_char(l);
                } else {
                    advance_char(l);
                }
            }
        } else {
            break;
        }
    }
}

static TokenType ident_or_keyword(const char *s, int len) {
    for (int i = 0; keywords[i].word; i++) {
        if ((int)strlen(keywords[i].word) == len &&
            memcmp(keywords[i].word, s, len) == 0) {
            return keywords[i].type;
        }
    }
    return TK_IDENT;
}

// ─── Number literal ───────────────────────────────────────────────────────────
static Token read_number(Lexer *l) {
    l->token_start = l->pos - 1;  // include first digit
    const char *start = l->token_start;

    // Check for hex/octal/binary prefix
    if (*start == '0' && l->pos < l->start + l->length) {
        char n = peek_char(l);
        if (n == 'x' || n == 'X') {
            advance_char(l); // skip 'x'
            while (isxdigit((unsigned char)peek_char(l))) advance_char(l);
            Token t = make_token(l, TK_INT_LIT);
            t.ival = strtoll(start + 2, NULL, 16);
            t.overflow = (t.ival == LLONG_MAX);
            return t;
        }
        if (n == 'o' || n == 'O') {
            advance_char(l);
            while (peek_char(l) >= '0' && peek_char(l) <= '7') advance_char(l);
            Token t = make_token(l, TK_INT_LIT);
            t.ival = strtoll(start + 2, NULL, 8);
            return t;
        }
        if (n == 'b' || n == 'B') {
            advance_char(l);
            while (peek_char(l) == '0' || peek_char(l) == '1') advance_char(l);
            Token t = make_token(l, TK_INT_LIT);
            t.ival = strtoll(start + 2, NULL, 2);
            return t;
        }
    }

    bool is_float = false;
    while (peek_char(l) && (isdigit((unsigned char)peek_char(l)) || peek_char(l) == '_')) {
        if (peek_char(l) == '_') { advance_char(l); continue; }
        advance_char(l);
    }

    if (peek_char(l) == '.') {
        // Make sure it's not '..'
        if (peek_char_next(l, 1) != '.') {
            is_float = true;
            advance_char(l);
            while (peek_char(l) && (isdigit((unsigned char)peek_char(l)) || peek_char(l) == '_')) {
                if (peek_char(l) == '_') { advance_char(l); continue; }
                advance_char(l);
            }
        }
    }

    // Exponent
    if (peek_char(l) == 'e' || peek_char(l) == 'E') {
        is_float = true;
        advance_char(l);
        if (peek_char(l) == '+' || peek_char(l) == '-') advance_char(l);
        while (isdigit((unsigned char)peek_char(l))) advance_char(l);
    }

    if (is_float) {
        Token t = make_token(l, TK_FLOAT_LIT);
        // Copy without underscores
        char buf[256]; int j = 0;
        for (const char *p = start; p < l->pos && j < 255; p++) {
            if (*p != '_') buf[j++] = *p;
        }
        buf[j] = '\0';
        t.fval = strtod(buf, NULL);
        return t;
    }

    Token t = make_token(l, TK_INT_LIT);
    // Copy without underscores
    char buf[256]; int j = 0;
    for (const char *p = start; p < l->pos && j < 255; p++) {
        if (*p != '_') buf[j++] = *p;
    }
    buf[j] = '\0';
    t.ival = strtoll(buf, NULL, 10);
    t.overflow = (t.ival == LLONG_MAX && strcmp(buf, "9223372036854775807") != 0);
    return t;
}

// ─── String literal ───────────────────────────────────────────────────────────
static Token read_string(Lexer *l) {
    l->token_start = l->pos - 1; // include opening quote
    char delim = l->token_start[0];

    while (peek_char(l) && peek_char(l) != '"') {
        if (peek_char(l) == '\\') {
            advance_char(l); // skip backslash
            if (peek_char(l)) advance_char(l); // skip escaped char
        } else {
            advance_char(l);
        }
    }

    if (peek_char(l) == '"') {
        advance_char(l); // closing quote
    } else {
        // Unterminated string
        Token t = make_token(l, TK_ERROR);
        ket_diag_push(l->ctx, SEV_ERROR, t.loc, 0, "unterminated string literal");
        return t;
    }

    Token t = make_token(l, TK_STR_LIT);
    return t;
}

// ─── Raw string ───────────────────────────────────────────────────────────────
static Token read_raw_string(Lexer *l) {
    l->token_start = l->pos - 1; // include opening 'r'
    if (peek_char(l) == '"') {
        advance_char(l); // skip '"'
        while (peek_char(l) && peek_char(l) != '"') advance_char(l);
        if (peek_char(l) == '"') { advance_char(l); }
        else {
            Token t = make_token(l, TK_ERROR);
            ket_diag_push(l->ctx, SEV_ERROR, t.loc, 0, "unterminated raw string");
            return t;
        }
        return make_token(l, TK_RAW_STR_LIT);
    }
    // Hash-delimited: #"..."#
    int hash_count = 0;
    while (peek_char(l) == '#') { advance_char(l); hash_count++; }
    if (peek_char(l) == '"') {
        advance_char(l);
        while (peek_char(l)) {
            if (peek_char(l) == '"') {
                // Check for matching # count
                const char *p = l->pos + 1;
                bool match = true;
                for (int i = 0; i < hash_count; i++) {
                    if (p[i] != '#') { match = false; break; }
                }
                if (match) {
                    advance_char(l); // skip "
                    for (int i = 0; i < hash_count; i++) advance_char(l);
                    return make_token(l, TK_RAW_STR_LIT);
                }
            }
            advance_char(l);
        }
        Token t = make_token(l, TK_ERROR);
        ket_diag_push(l->ctx, SEV_ERROR, t.loc, 0, "unterminated raw string");
        return t;
    }
    return make_token(l, TK_IDENT); // just 'r' identifier
}

// ─── Char literal ─────────────────────────────────────────────────────────────
static Token read_char(Lexer *l) {
    l->token_start = l->pos - 1; // include opening '
    if (peek_char(l) == '\\') {
        advance_char(l);
        if (peek_char(l)) advance_char(l);
    } else if (peek_char(l)) {
        advance_char(l);
    }
    if (peek_char(l) == '\'') {
        advance_char(l);
    } else {
        Token t = make_token(l, TK_ERROR);
        ket_diag_push(l->ctx, SEV_ERROR, t.loc, 0, "unterminated char literal");
        return t;
    }
    return make_token(l, TK_CHAR_LIT);
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN LEXER ENTRY
// ═══════════════════════════════════════════════════════════════════════════════

void lexer_init(Lexer *l, const char *filename, const char *source,
                int length, Context *ctx) {
    l->filename = filename;
    l->start = source;
    l->pos = source;
    l->token_start = source;
    l->length = length;
    l->line = 1;
    l->col = 1;
    l->state = LEX_NORMAL;
    l->mode = LEX_MODE_NORMAL;
    l->error_count = 0;
    l->sync_point = false;
    l->ctx = ctx;
    l->paren_depth = 0;
    l->brace_depth = 0;
    l->pending_codepoint = 0;
    l->pending_bytes = 0;
    l->source_line = 1;
    l->source_col = 1;
    l->in_macro = false;
    l->raw_delim_len = 0;

    // Prime the lexer
    l->current.type = TK_EOF;
    l->peek.type = TK_EOF;
}

Token lexer_next(Lexer *l) {
    // Shift peek -> current
    l->current = l->peek;

    // If current is valid and we need next, read it
    if (l->peek.type != TK_EOF && l->current.type != TK_ERROR) {
        // Already have a peek value from previous call
    }

    skip_whitespace(l);

    l->token_start = l->pos;
    if (l->pos >= l->start + l->length) {
        Token t = make_token(l, TK_EOF);
        l->peek = t;
        return t;
    }

    char c = advance_char(l);

    // ─── Identifiers & Keywords ───
    if (is_ident_start((unsigned char)c) && c < 0x80) {
        while (is_ident_continue((unsigned char)peek_char(l))) {
            advance_char(l);
        }
        int len = (int)(l->pos - l->token_start);
        Token t = make_token(l, ident_or_keyword(l->token_start, len));
        l->peek = t;
        return t;
    }

    // ─── Numbers ───
    if (isdigit((unsigned char)c)) {
        Token t = read_number(l);
        l->peek = t;
        return t;
    }

    // ─── Strings ───
    if (c == '"') {
        Token t = read_string(l);
        l->peek = t;
        return t;
    }

    // ─── Char ───
    if (c == '\'') {
        Token t = read_char(l);
        l->peek = t;
        return t;
    }

    // ─── Raw string 'r' ───
    if (c == 'r' && (peek_char(l) == '"' || peek_char(l) == '#')) {
        Token t = read_raw_string(l);
        l->peek = t;
        return t;
    }

    // ─── Multi-char operators ───
    #define TWO_CHAR(a, b, t) \
        if (c == (a) && peek_char(l) == (b)) { advance_char(l); return make_token(l, t); }
    #define THREE_CHAR(a, b, c_, t) \
        if (c == (a) && peek_char(l) == (b) && peek_char_next(l, 1) == (c_)) { \
            advance_char(l); advance_char(l); return make_token(l, t); }

    THREE_CHAR('.', '.', '.', TK_DOTDOTDOT)
    THREE_CHAR('<', '<', '=', TK_LTLTEQ)
    THREE_CHAR('>', '>', '=', TK_GTGTEQ)

    TWO_CHAR('=', '=', TK_EQEQ)
    TWO_CHAR('!', '=', TK_BANGEQ)
    TWO_CHAR('<', '=', TK_LTEQ)
    TWO_CHAR('>', '=', TK_GTEQ)
    TWO_CHAR('<', '<', TK_LTLT)
    TWO_CHAR('>', '>', TK_GTGT)
    TWO_CHAR('&', '&', TK_AMPAMP)
    TWO_CHAR('|', '|', TK_PIPEPIPE)
    TWO_CHAR('+', '=', TK_PLUSEQ)
    TWO_CHAR('-', '=', TK_MINUSEQ)
    TWO_CHAR('*', '=', TK_STAREQ)
    TWO_CHAR('/', '=', TK_SLASHEQ)
    TWO_CHAR('%', '=', TK_PERCENTEQ)
    TWO_CHAR('&', '=', TK_AMPERSANDEQ)
    TWO_CHAR('|', '=', TK_PIPEEQ)
    TWO_CHAR('^', '=', TK_CARETEQ)
    TWO_CHAR('-', '>', TK_ARROW)
    TWO_CHAR('=', '>', TK_FAT_ARROW)
    TWO_CHAR('.', '.', TK_DOTDOT)
    TWO_CHAR('.', '=', TK_DOTEQ)
    TWO_CHAR(':', ':', TK_COLONCOLON)
    TWO_CHAR('*', '*', TK_POWER)
    TWO_CHAR('|', '>', TK_PIPE_FORWARD)

    // ─── Single-char operators & delimiters ───
    switch (c) {
        case '+': return make_token(l, TK_PLUS);
        case '-': return make_token(l, TK_MINUS);
        case '*': return make_token(l, TK_STAR);
        case '/': return make_token(l, TK_SLASH);
        case '%': return make_token(l, TK_PERCENT);
        case '=': return make_token(l, TK_EQ);
        case '!': return make_token(l, TK_EXCLAM);
        case '<': return make_token(l, TK_LT);
        case '>': return make_token(l, TK_GT);
        case '&': return make_token(l, TK_AMPERSAND);
        case '|': return make_token(l, TK_PIPE);
        case '^': return make_token(l, TK_CARET);
        case '~': return make_token(l, TK_TILDE);
        case '?': return make_token(l, TK_QUESTION);
        case ':': return make_token(l, TK_COLON);
        case ';': return make_token(l, TK_SEMICOLON);
        case ',': return make_token(l, TK_COMMA);
        case '.': return make_token(l, TK_DOT);
        case '(': l->paren_depth++; return make_token(l, TK_LPAREN);
        case ')': l->paren_depth--; return make_token(l, TK_RPAREN);
        case '[': return make_token(l, TK_LBRACKET);
        case ']': return make_token(l, TK_RBRACKET);
        case '{': l->brace_depth++; return make_token(l, TK_LBRACE);
        case '}': l->brace_depth--; return make_token(l, TK_RBRACE);
        case '@': return make_token(l, TK_AT);
        case '#': return make_token(l, TK_HASH);
        case '_': return make_token(l, TK_UNDERSCORE);
        case '$': return make_token(l, TK_DOLLAR);
        case '\\': return make_token(l, TK_BACKSLASH);
    }

    // ─── Unicode identifier start ───
    // If we got here, check for non-ASCII ident start
    if (c & 0x80) {
        // We need to re-decode. Put back and re-read properly.
        l->pos--;
        l->col--;
        const char *saved = l->pos;
        uint32_t cp = utf8_decode(&saved, l->start + l->length);
        if (is_ident_start(cp)) {
            l->pos = saved;
            while (is_ident_continue(utf8_decode(&saved, l->start + l->length))) {
                l->pos = saved;
            }
            int len = (int)(l->pos - l->token_start);
            return make_token(l, ident_or_keyword(l->token_start, len));
        }
        l->pos = saved;
    }

    // ─── Error ───
    Token t = make_token(l, TK_ERROR);
    ket_diag_push(l->ctx, SEV_ERROR, t.loc, 0,
                  "unexpected character '%c' (0x%02X)", c, (unsigned char)c);
    l->peek = t;
    return t;
}

Token lexer_peek(Lexer *l) {
    // Already peeked?
    if (l->peek.type != TK_EOF || l->current.type == TK_EOF) {
        return l->peek;
    }
    // Need to read
    Lexer saved = *l;
    Token t = lexer_next(l);
    *l = saved;
    return t;
}

bool lexer_match(Lexer *l, TokenType type) {
    // Ensure peek is loaded
    Token t = lexer_next(l);
    if (t.type == type) return true;
    // Put it back conceptually - we already consumed it though
    // So we need to un-read. Store in a lookahead buffer.
    // For now, simple approach: the next call will re-read
    l->current = t;
    l->peek = t;  // This is wrong but matches our simple model
    return false;
}

Token lexer_expect(Lexer *l, TokenType type, const char *msg) {
    Token t = lexer_next(l);
    if (t.type != type) {
        ket_diag_push(l->ctx, SEV_ERROR, t.loc, 0,
                      "expected %s, got %s", msg, token_type_name(t.type));
    }
    return t;
}

Location lexer_location(Lexer *l) {
    Location loc;
    loc.file = l->filename;
    loc.source = l->start;
    loc.offset = (int)(l->pos - l->start);
    loc.line = l->line;
    loc.col = l->col;
    loc.length = 0;
    return loc;
}

void lexer_dump(Lexer *l) {
    Token t;
    do {
        t = lexer_next(l);
        const char *name = token_type_name(t.type);
        printf("[%4d:%3d] %-20s '", t.loc.line, t.loc.col, name);
        fwrite(t.start, 1, t.length > 40 ? 40 : t.length, stdout);
        if (t.length > 40) printf("...");
        printf("'");
        if (t.type == TK_INT_LIT) printf("  ival=%lld", (long long)t.ival);
        if (t.type == TK_FLOAT_LIT) printf("  fval=%f", t.fval);
        printf("\n");
    } while (t.type != TK_EOF);
}
