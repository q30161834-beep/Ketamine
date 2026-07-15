#ifndef KETAMINE_TRAIT_ASSOCIATED_H
#define KETAMINE_TRAIT_ASSOCIATED_H

#include "types.h"

// ─── Traits with Associated Types ──────────────────────────────────────────
// Supports: `type Item`, `type Error`, `const` in traits.
// Enables: `impl<T> Trait for T where T: ...`

typedef struct {
    const char    *name;         // e.g., "Item", "Error"
    struct Type   *type;         // the concrete type in the impl
    struct Type   *default_type; // default if not specified
    bool           is_const;     // const associated value
    ConstGenericValue const_val; // for const associated items
} AssociatedType;

typedef struct {
    const char      *trait_name;
    AssociatedType  *assoc_types;
    int              assoc_count;
    struct Type    **type_bounds;    // T: Trait bounds
    int              bound_count;
} TraitWithAssoc;

// ─── API ──────────────────────────────────────────────────────────────────

/// Resolve an associated type for a given impl
struct Type *trait_resolve_assoc_type(struct Type *impl_type,
                                       const char *trait_name,
                                       const char *assoc_name,
                                       Context *ctx);

/// Add an associated type declaration to a trait
void trait_add_assoc_type(ASTNode *trait_decl, const char *name,
                          struct Type *default_type);

/// Check if an impl satisfies all associated types
bool trait_check_assoc_types(ASTNode *impl_block, ASTNode *trait_decl,
                              Context *ctx);

/// Build the full type for a trait with associated types resolved
struct Type *trait_build_resolved_type(struct Type *trait_type,
                                        AssociatedType *assoc_types,
                                        int assoc_count,
                                        TypeTable *tt);

/// Check trait bound satisfaction: T: Trait<Item = X>
bool trait_check_bound_with_assoc(struct Type *ty, struct Type *trait_bound,
                                   Context *ctx);

#endif
