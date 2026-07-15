#ifndef KETAMINE_BORROW_H
#define KETAMINE_BORROW_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// BORROW CHECKER — Ownership, lifetimes, and borrow rule enforcement
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Lifetime ─────────────────────────────────────────────────────────────────
typedef struct Lifetime {
    int              id;
    const char      *name;      // 'a, 'b, etc.
    struct Lifetime  *parent;   // for sub-lifetime relations
    bool             is_static; // 'static
    bool             is_anon;   // anonymous lifetime
    Location         loc;
} Lifetime;

// ─── Ownership Kind ───────────────────────────────────────────────────────────
typedef enum {
    OWN_MOVE,       // owned, moved
    OWN_BORROW,     // &T — shared borrow
    OWN_MUT_BORROW, // &mut T — exclusive borrow
    OWN_OWNED,      // owned value
} OwnKind;

// ─── Borrow State ─────────────────────────────────────────────────────────────
typedef struct BorrowState {
    // Track borrows per variable
    struct {
        const char      *var_name;
        bool             moved;
        int              shared_borrow_count;
        bool             mut_borrow;
        struct Lifetime *borrow_lifetime;
    } *entries;
    int entry_count;
    int entry_cap;

    // Current scope depth
    int scope_depth;

    // Error reporting
    Context *ctx;
} BorrowState;

// ─── Borrow Checker API ───────────────────────────────────────────────────────

/// Initialize borrow state
BorrowState *borrow_state_new(Context *ctx);

/// Enter a new scope
void borrow_enter_scope(BorrowState *bs);

/// Exit the current scope
void borrow_exit_scope(BorrowState *bs);

/// Track a variable declaration
void borrow_decl_var(BorrowState *bs, const char *name, bool is_mut);

/// Track a move of a variable
bool borrow_check_move(BorrowState *bs, ASTNode *expr, const char *var);

/// Track a shared borrow (&T)
bool borrow_check_shared_borrow(BorrowState *bs, ASTNode *expr, const char *var);

/// Track a mutable borrow (&mut T)
bool borrow_check_mut_borrow(BorrowState *bs, ASTNode *expr, const char *var);

/// Track a use of a variable
bool borrow_check_use(BorrowState *bs, ASTNode *expr, const char *var);

/// Track an assignment to a variable
bool borrow_check_assign(BorrowState *bs, ASTNode *expr, const char *var);

/// Check that all borrows are released at scope exit
bool borrow_check_scope_end(BorrowState *bs);

/// Create a new lifetime
Lifetime *lifetime_new(Lifetime *parent, const char *name, Location loc);

/// Check lifetime subtyping: a :> b (a outlives b)
bool lifetime_check_outlives(Lifetime *a, Lifetime *b, Location loc);

/// Run full borrow check pass on an AST
bool borrow_check_module(Context *ctx, ASTNode *program);

#endif // KETAMINE_BORROW_H
