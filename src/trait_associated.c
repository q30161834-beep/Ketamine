#include "include/trait_associated.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct Type *trait_resolve_assoc_type(struct Type *impl_type,
                                       const char *trait_name,
                                       const char *assoc_name,
                                       Context *ctx) {
    (void)impl_type;
    (void)trait_name;
    (void)assoc_name;
    (void)ctx;
    return NULL;
}

void trait_add_assoc_type(ASTNode *trait_decl, const char *name,
                          struct Type *default_type) {
    (void)trait_decl;
    (void)name;
    (void)default_type;
}

bool trait_check_assoc_types(ASTNode *impl_block, ASTNode *trait_decl,
                              Context *ctx) {
    (void)impl_block;
    (void)trait_decl;
    (void)ctx;
    return true;
}

struct Type *trait_build_resolved_type(struct Type *trait_type,
                                        AssociatedType *assoc_types,
                                        int assoc_count,
                                        TypeTable *tt) {
    (void)trait_type;
    (void)assoc_types;
    (void)assoc_count;
    (void)tt;
    return NULL;
}

bool trait_check_bound_with_assoc(struct Type *ty, struct Type *trait_bound,
                                   Context *ctx) {
    (void)ty;
    (void)trait_bound;
    (void)ctx;
    return true;
}
