#ifndef KETAMINE_PARSER_H
#define KETAMINE_PARSER_H

#include "types.h"
#include "lexer.h"

// ═══════════════════════════════════════════════════════════════════════════════
// PARSER — Recursive descent parser with Pratt expression parsing,
//           error recovery, and incremental parsing support
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Precedence Levels ───────────────────────────────────────────────────────
typedef enum {
    PREC_NONE,
    PREC_ASSIGN,        // =
    PREC_OR,            // ||
    PREC_AND,           // &&
    PREC_COMPARE,       // == != < > <= >=
    PREC_BIT_OR,        // |
    PREC_BIT_XOR,       // ^
    PREC_BIT_AND,       // &
    PREC_SHIFT,         // << >>
    PREC_RANGE,         // ..
    PREC_TERM,          // + -
    PREC_FACTOR,        // * / %
    PREC_POWER,         // **
    PREC_CAST,          // as
    PREC_UNARY,         // ! - ~ * &
    PREC_CALL,          // () [] .
    PREC_PRIMARY
} Precedence;

// ─── Parser Flags ─────────────────────────────────────────────────────────────
typedef enum {
    PARSE_DEFAULT        = 0,
    PARSE_EXPRESSION     = 1 << 0,
    PARSE_TYPE           = 1 << 1,
    PARSE_PATTERN        = 1 << 2,
    PARSE_BLOCK          = 1 << 3,
    PARSE_TOP_LEVEL      = 1 << 4,
    PARSE_ALLOW_MUT      = 1 << 5,
} ParseFlags;

// ─── Parser ───────────────────────────────────────────────────────────────────
typedef struct Parser {
    Lexer        *lexer;
    Context      *ctx;

    // Current token state
    Token         current;
    Token         peek;

    // Error recovery
    bool          panic_mode;
    int           error_count;
    int           recovery_tokens;

    // Nesting
    int           nesting_depth;
    int           brace_depth;
    int           paren_depth;

    // Label tracking
    struct LabelStack {
        const char *name;
        Location    loc;
    }            *labels;
    int           label_count;
    int           label_cap;

    // Generic parameter stack (for parser)
    struct Type **generics;
    int           generic_count;
    int           generic_cap;
} Parser;

// ─── API ──────────────────────────────────────────────────────────────────────

// Initialize parser
void     parser_init(Parser *parser, Lexer *lexer, Context *ctx);

// Parse a complete program
ASTNode *parse_program(Parser *parser);

// Parse a single expression
ASTNode *parse_expr(Parser *parser);

// Parse a type
struct Type *parse_type(Parser *parser);

// Parse a block
ASTNode *parse_block(Parser *parser);

// Advance tokens
void     parser_advance(Parser *parser);
Token    parser_cur(Parser *parser);
Token    parser_peek(Parser *parser);

// Check / match / expect
bool     parser_check(Parser *parser, TokenType type);
bool     parser_match(Parser *parser, TokenType type);
Token    parser_expect(Parser *parser, TokenType type, const char *msg);

// Synchronize to next statement
void     parser_sync(Parser *parser);

// Error reporting
void     parser_error(Parser *parser, Location loc, const char *fmt, ...);

// Nested parsing entry points
ASTNode *parse_stmt(Parser *parser);
ASTNode *parse_decl(Parser *parser);
ASTNode *parse_fn_decl(Parser *parser, NodeFlags flags);
ASTNode *parse_struct_decl(Parser *parser, NodeFlags flags);
ASTNode *parse_enum_decl(Parser *parser, NodeFlags flags);
ASTNode *parse_trait_decl(Parser *parser, NodeFlags flags);
ASTNode *parse_impl_block(Parser *parser);
ASTNode *parse_var_decl(Parser *parser, NodeFlags flags);
ASTNode *parse_if(Parser *parser);
ASTNode *parse_while(Parser *parser);
ASTNode *parse_for(Parser *parser);
ASTNode *parse_loop(Parser *parser);
ASTNode *parse_match(Parser *parser);
ASTNode *parse_return(Parser *parser);
ASTNode *parse_break(Parser *parser);
ASTNode *parse_continue(Parser *parser);
ASTNode *parse_closure(Parser *parser);
ASTNode *parse_pattern(Parser *parser);

// Expression parsing (Pratt)
ASTNode *parse_expr_prec(Parser *parser, Precedence min_prec);
ASTNode *parse_primary_expr(Parser *parser);
ASTNode *parse_postfix_expr(Parser *parser, ASTNode *left);
ASTNode *parse_prefix_expr(Parser *parser);

// Type parsing
struct Type *parse_type_path(Parser *parser);
struct Type *parse_type_fn(Parser *parser);
struct Type *parse_type_tuple(Parser *parser);
struct Type *parse_type_ref(Parser *parser);
struct Type *parse_type_slice(Parser *parser);
struct Type *parse_type_generic(Parser *parser, struct Type *base,
                                 struct Type **args, int arg_count);

// Generic parsing
ASTNode *parse_generic_params(Parser *parser);
ASTNode *parse_generic_args(Parser *parser, ASTNode *base);
ASTNode *parse_where_clause(Parser *parser);

// Attribute parsing
ASTNode *parse_attrs(Parser *parser);

#endif // KETAMINE_PARSER_H
