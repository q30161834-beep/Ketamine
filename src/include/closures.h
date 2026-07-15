#ifndef KETAMINE_CLOSURES_H
#define KETAMINE_CLOSURES_H

#include "types.h"

// ─── Closure System ────────────────────────────────────────────────────────
// Three closure types: FnOnce (consumes captures), FnMut (mutates), Fn (reads).
// Each closure captures variables from its enclosing scope.

typedef enum {
    CLOSURE_FN,      // &self — immutable borrow of captures
    CLOSURE_FN_MUT,  // &mut self — mutable borrow
    CLOSURE_FN_ONCE, // self — consumes captures
} ClosureKind;

typedef struct {
    const char    *name;
    struct Type   *type;
    bool           is_mut;
    bool           is_ref;
    int            offset_in_env; // byte offset in closure env struct
} CaptureVar;

typedef struct {
    const char   *name;              // unique closure name
    ClosureKind   kind;
    ASTNode     **params;
    int           param_count;
    ASTNode      *body;
    CaptureVar   *captures;          // variables captured from env
    int           capture_count;
    int           env_size;          // size of environment struct
    ASTNode      *fn_decl;           // lowered to regular fn
    struct Type  *fn_type;
} ClosureInfo;

// ─── API ──────────────────────────────────────────────────────────────────

/// Analyze a closure expression and determine its captures
ClosureInfo *closure_analyze(ASTNode *closure_node, Context *ctx);

/// Classify a closure as Fn/FnMut/FnOnce based on usage
ClosureKind closure_classify(ClosureInfo *ci);

/// Create the environment struct type for a closure
struct Type *closure_build_env_type(ClosureInfo *ci, TypeTable *tt);

/// Lower a closure to a struct + function pair
ASTNode *closure_lower(ClosureInfo *ci, Arena *arena);

/// Emit closure for Go backend (use native Go closures)
const char *closure_go_codegen(ClosureInfo *ci);

/// Emit closure for Python backend (use native Python closures)
const char *closure_py_codegen(ClosureInfo *ci);

#endif
