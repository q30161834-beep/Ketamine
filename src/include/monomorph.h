#ifndef KETAMINE_MONOMORPH_H
#define KETAMINE_MONOMORPH_H

#include "types.h"
#include "typecheck.h"

// ═══════════════════════════════════════════════════════════════════════════════
// MONOMORPHIZATION ENGINE — Generic instantiation via template specialization
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Type Arg Mapping ─────────────────────────────────────────────────────────
typedef struct {
    ASTNode    *generic_param;   // the original generic param (e.g., T)
    struct Type *concrete;       // the concrete type to substitute
} TypeArgMap;

// ─── Specialization Record ────────────────────────────────────────────────────
typedef struct {
    const char    *original_name;  // e.g., "Vec"
    ASTNode       *original_decl;  // the generic declaration
    TypeArgMap    *args;           // type arg bindings
    int            arg_count;
    ASTNode       *specialized;    // the monomorphized copy
    bool           generated;
    uint64_t       key_hash;       // hash of (name + arg types) for dedup
} Specialization;

// ─── Impl Record ──────────────────────────────────────────────────────────────
typedef struct {
    const char    *trait_name;
    struct Type   *impl_type;      // the type implementing the trait
    ASTNode       *impl_ast;       // the impl block AST
    struct Type  **generic_bindings;
    int            generic_count;
} ImplRecord;

// ─── Monomorphization State ───────────────────────────────────────────────────
typedef struct {
    // All generic declarations
    ASTNode **generic_decls;
    int       generic_decl_count;
    int       generic_decl_cap;

    // Trait implementations: trait_name -> impl records
    ImplRecord **impls;
    int          impl_count;
    int          impl_cap;

    // Specializations already generated (dedup)
    Specialization *specs;
    int             spec_count;
    int             spec_cap;

    // New specialized declarations to add to the module
    ASTNode **new_decls;
    int       new_decl_count;
    int       new_decl_cap;

    // The arena for allocation
    Arena    *arena;

    // Diagnostic context
    Context  *ctx;
} MonoState;

// ─── API ──────────────────────────────────────────────────────────────────────

/// Create monomorphization state
MonoState *mono_state_new(Context *ctx);

/// Register a generic declaration (fn, struct, enum)
void mono_register_generic(MonoState *ms, ASTNode *decl);

/// Register a trait impl block
void mono_register_impl(MonoState *ms, ASTNode *impl_block);

/// Find the concrete type from an instantiation of generics.
/// Returns a new Type where TY_GENERIC references are replaced with concrete types.
struct Type *mono_substitute_types(MonoState *ms, struct Type *type,
                                   TypeArgMap *args, int arg_count);

/// Substitute generic params with concrete types in an AST subtree
ASTNode *mono_substitute_ast(MonoState *ms, ASTNode *node,
                             TypeArgMap *args, int arg_count);

/// Specialize a generic function with concrete type arguments.
/// Returns the specialized (monomorphized) function ASTNode.
ASTNode *mono_specialize_fn(MonoState *ms, ASTNode *generic_fn,
                            TypeArgMap *args, int arg_count);

/// Specialize a generic struct with concrete type arguments.
/// Returns the specialized struct declaration ASTNode.
ASTNode *mono_specialize_struct(MonoState *ms, ASTNode *generic_struct,
                                TypeArgMap *args, int arg_count);

/// Check if trait bounds are satisfied for a given type
bool mono_check_trait_bounds(MonoState *ms, struct Type *type,
                             struct Type *required_trait, Location loc);

/// Resolve a trait method call: find the impl for (type, trait) and return the method
ASTNode *mono_resolve_trait_method(MonoState *ms, struct Type *type,
                                   const char *trait_name, const char *method_name);

/// Run the full monomorphization pass on a module.
/// Returns the module AST with all generics specialized.
ASTNode *mono_run_pass(MonoState *ms, ASTNode *module);

/// Free monomorphization state
void mono_state_free(MonoState *ms);

#endif // KETAMINE_MONOMORPH_H
