#ifndef KETAMINE_TRAIT_RESOLVE_H
#define KETAMINE_TRAIT_RESOLVE_H

#include "types.h"
#include "typecheck.h"

// ═══════════════════════════════════════════════════════════════════════════════
// TRAIT RESOLUTION — Trait bound checking, impl lookup, where clause satisfaction
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Trait Bound ──────────────────────────────────────────────────────────────
typedef struct {
    const char    *trait_name;
    struct Type   *trait_type;     // the trait type itself
    struct Type   *self_type;      // the type implementing the trait
    struct Type  **type_args;      // generic args on the trait (e.g., Into<String>)
    int            type_arg_count;
} TraitBound;

// ─── Trait Resolution State ───────────────────────────────────────────────────
typedef struct TraitTable {
    // trait name -> list of implementations
    struct {
        const char  *trait_name;
        ImplRecord **impls;
        int          impl_count;
        int          impl_cap;
    } *entries;
    int entry_count;
    int entry_cap;

    // For super-trait resolution: trait -> parent traits
    struct {
        const char  *trait_name;
        const char **supertrait_names;
        int          supertrait_count;
    } *hierarchy;
    int hierarchy_count;
    int hierarchy_cap;

    Context *ctx;
    Arena   *arena;
} TraitTable;

// ─── API ──────────────────────────────────────────────────────────────────────

/// Create trait resolution table
TraitTable *trait_table_new(Context *ctx);

/// Register a trait declaration
void trait_register(TraitTable *tt, ASTNode *trait_decl);

/// Register an impl block
void trait_register_impl(TraitTable *tt, ASTNode *impl_block);

/// Find impls of a trait by name
ImplRecord **trait_find_impls(TraitTable *tt, const char *trait_name, int *count);

/// Check if a type satisfies a trait bound
bool trait_check_bound(TraitTable *tt, struct Type *type,
                       const char *trait_name, Location loc);

/// Check if a where clause is satisfied
bool trait_check_where(TraitTable *tt, ASTNode *where_clause,
                       struct Type **generic_bindings, int binding_count);

/// Resolve method call through trait dispatch.
/// Given (type, method_name), find the impl and return the actual method.
ASTNode *trait_resolve_method(TraitTable *tt, struct Type *type,
                              const char *method_name,
                              const char *trait_name);

/// Resolve a trait method by explicit trait qualification (e.g., Trait::method)
ASTNode *trait_resolve_qualified(TraitTable *tt, const char *trait_name,
                                  const char *method_name);

/// Dump trait table (debug)
void trait_table_dump(TraitTable *tt);

/// Free trait table
void trait_table_free(TraitTable *tt);

#endif // KETAMINE_TRAIT_RESOLVE_H
