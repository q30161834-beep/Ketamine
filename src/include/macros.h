#ifndef KETAMINE_MACROS_H
#define KETAMINE_MACROS_H

#include "types.h"

// ─── Declarative Macros (macro_rules!-style) ───────────────────────────────
// Pattern-matching macros similar to Rust's macro_rules!.
// Hygienic: macro expansions don't pollute the caller's namespace.

typedef enum {
    MATCH_EXPR,     // $x:expr
    MATCH_TYPE,     // $x:ty
    MATCH_IDENT,    // $x:ident
    MATCH_LITERAL,  // $x:literal
    MATCH_PAT,      // $x:pat
    MATCH_STMT,     // $x:stmt
    MATCH_BLOCK,    // $x:block
    MATCH_META,     // $x:meta
    MATCH_TT,       // $x:tt (token tree)
} MatchSpecKind;

typedef struct {
    const char   *name;          // variable name (or "" for literal match)
    MatchSpecKind kind;          // fragment specifier
    bool          is_repetition; // $($x),*
    const char   *separator;     // separator for repetitions
} MatchSpec;

typedef struct {
    MatchSpec    *specs;         // pattern variables
    int           spec_count;
    Token        *tokens;        // matched token sequence
    int           token_count;
    bool          is_repetition;
    int           repetition_count;
} MatchPattern;

typedef struct {
    Token        *tokens;         // expansion token sequence
    int           token_count;
    int         (*repetition_indices)[2]; // [start, end] in token array
    int           rep_count;
} MacroExpansion;

typedef struct {
    const char      *name;
    MatchPattern    *patterns;    // multiple arms
    int              pattern_count;
    MacroExpansion  *expansions;  // corresponding expansions
    bool             is_hygienic; // hygienic by default
    Location         loc;
} MacroRule;

// ─── API ──────────────────────────────────────────────────────────────────

/// Register a macro rule
MacroRule *macro_register(Context *ctx, const char *name, Location loc);

/// Match a token stream against a macro pattern
bool macro_match(MacroRule *rule, Token *input, int input_count,
                 MatchPattern *out_match);

/// Expand a matched macro into output tokens
Token *macro_expand(MacroRule *rule, MatchPattern *match,
                    int *out_count, Arena *arena);

/// Check if a token is a valid fragment specifier (expr, ty, ident, etc.)
bool macro_is_fragment_spec(Token tok);

/// Expand all macros in a token stream (recursive)
Token *macro_expand_all(Token *tokens, int count, int *out_count,
                        Context *ctx, Arena *arena);

#endif
