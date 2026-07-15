#ifndef KETAMINE_CONST_GENERICS_H
#define KETAMINE_CONST_GENERICS_H

#include "types.h"

// ─── Const Generics — Compile-Time Value Parameters ───────────────────────
// Allows generic parameters to be constants, not just types.
// E.g.: struct Vec<T, const N: int> { data: [T; N] }

typedef enum {
    CONST_INT,
    CONST_BOOL,
    CONST_FLOAT,
    CONST_STR,
} ConstKind;

typedef struct {
    ConstKind kind;
    union {
        int64_t  int_val;
        bool     bool_val;
        double   float_val;
        const char *str_val;
    };
    bool is_known;  // false if still symbolic (dependent on outer generic)
} ConstGenericValue;

typedef struct {
    const char    *name;       // e.g., "N"
    struct Type   *ty;         // e.g., int
    ConstGenericValue value;   // resolved value (if known)
    bool           is_known;   // whether value is resolved
} ConstGenericParam;

// ─── API ──────────────────────────────────────────────────────────────────

/// Evaluate a const generic expression (constant folding)
ConstGenericValue const_eval_expr(ASTNode *expr, Context *ctx);

/// Check if a type uses const generics
bool const_type_has_const_generics(struct Type *ty);

/// Substitute const generic values in a type
struct Type *const_substitute_type(struct Type *ty, ConstGenericParam *params,
                                   int param_count, TypeTable *tt);

/// Evaluate a const generic expression at compile time
int64_t const_eval_int(ASTNode *expr, Context *ctx);

/// Check if a const generic value satisfies a where clause
bool const_check_where(ASTNode *where, ConstGenericValue val, Context *ctx);

#endif
