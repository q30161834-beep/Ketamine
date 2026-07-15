#ifndef KETAMINE_TYPECHECK_H
#define KETAMINE_TYPECHECK_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE CHECKER — Hindley-Milner bidirectional type inference
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Type Variable ────────────────────────────────────────────────────────────
typedef struct TypeVar {
    int             id;
    struct Type    *instance;   // resolved type (NULL if unresolved)
    struct TypeVar *link;       // union-find for unification
    int             rank;       // union-find rank
    bool            is_generic; // universally quantified
} TypeVar;

// ─── Unification State ───────────────────────────────────────────────────────
typedef struct UnifyState {
    TypeVar **vars;
    int       var_count;
    int       var_cap;
    int       next_id;
} UnifyState;

// ─── Constraint ──────────────────────────────────────────────────────────────
typedef enum {
    CON_EQ,          // type equality
    CON_SUBTYPE,     // subtype relation
    CON_TRAIT,       // trait bound
    CON_LIFETIME,    // lifetime constraint
} ConstraintKind;

typedef struct Constraint {
    ConstraintKind  kind;
    struct Type    *left;
    struct Type    *right;
    struct Type    *trait_type;  // for CON_TRAIT
    Location        loc;
    struct Constraint *next;
} Constraint;

// ─── Environment ─────────────────────────────────────────────────────────────
typedef struct TyEnv {
    struct TyEnv   *parent;
    SymTable       *syms;
    // Type variable context
    UnifyState     *unify;
    // Current function return type
    struct Type    *ret_type;
    // Generic params in scope
    TypeVar       **generics;
    int             generic_count;
    // Constraints accumulated
    Constraint     *constraints;
    Constraint     *constraints_last;
    int             constraint_count;
    // Error reporting
    Context        *ctx;
    // Loop context for break/continue
    struct Type    *break_type;
    bool            in_loop;
} TyEnv;

// ─── Type Checker API ────────────────────────────────────────────────────────

/// Initialize the type checker environment
TyEnv *ty_env_new(TyEnv *parent, SymTable *syms, Context *ctx);

/// Create a fresh type variable
Type *ty_fresh_var(TyEnv *env);

/// Create a fresh generic type variable
Type *ty_fresh_generic(TyEnv *env, const char *name);

/// Infer the type of an expression
Type *ty_infer_expr(TyEnv *env, ASTNode *expr);

/// Check a statement (typechecks in place)
void ty_check_stmt(TyEnv *env, ASTNode *stmt);

/// Check a function declaration
Type *ty_check_fn(TyEnv *env, ASTNode *fn);

/// Check a top-level declaration
void ty_check_decl(TyEnv *env, ASTNode *decl);

/// Unify two types (make them equal)
bool ty_unify(TyEnv *env, Type *a, Type *b, Location loc);

/// Add a constraint
void ty_add_constraint(TyEnv *env, ConstraintKind kind, Type *left,
                        Type *right, Location loc);

/// Solve all accumulated constraints
bool ty_solve_constraints(TyEnv *env);

/// Instantiate a generic type with fresh vars
Type *ty_instantiate(TyEnv *env, Type *generic_type);

/// Generalize a type (quantify free vars)
Type *ty_generalize(TyEnv *env, Type *type, ASTNode *at);

/// Run full type checking pass on a module
bool ty_check_module(Context *ctx, ASTNode *program);

/// Print type (for debugging)
void ty_print_type(Type *type);

/// Get a string representation of a type
const char *ty_type_to_str(Type *type, Arena *arena);

#endif // KETAMINE_TYPECHECK_H
