#include "include/trait_resolve.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// TRAIT RESOLUTION ENGINE — Bound checking, impl lookup, where clause satisfaction
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Table Creation ───────────────────────────────────────────────────────────

TraitTable *trait_table_new(Context *ctx) {
    Arena *arena = ctx->arena;
    TraitTable *tt = (TraitTable*)arena_alloc_zero(arena, sizeof(TraitTable));
    tt->entries = NULL;
    tt->entry_count = 0;
    tt->entry_cap = 0;
    tt->hierarchy = NULL;
    tt->hierarchy_count = 0;
    tt->hierarchy_cap = 0;
    tt->ctx = ctx;
    tt->arena = arena;
    return tt;
}

void trait_table_free(TraitTable *tt) {
    (void)tt;  // all memory is arena-managed
}

// ─── Find or create trait entry ───────────────────────────────────────────────

static int find_entry(TraitTable *tt, const char *trait_name) {
    for (int i = 0; i < tt->entry_count; i++) {
        if (strcmp(tt->entries[i].trait_name, trait_name) == 0)
            return i;
    }
    return -1;
}

static int get_or_create_entry(TraitTable *tt, const char *trait_name) {
    int idx = find_entry(tt, trait_name);
    if (idx >= 0) return idx;

    if (tt->entry_count >= tt->entry_cap) {
        tt->entry_cap = tt->entry_cap ? tt->entry_cap * 2 : 32;
    }

    idx = tt->entry_count++;
    tt->entries[idx].trait_name = trait_name;
    tt->entries[idx].impls = NULL;
    tt->entries[idx].impl_count = 0;
    tt->entries[idx].impl_cap = 0;
    return idx;
}

// ─── Registration ─────────────────────────────────────────────────────────────

void trait_register(TraitTable *tt, ASTNode *trait_decl) {
    if (!trait_decl || trait_decl->kind != N_TRAIT_DECL) return;

    const char *name = trait_decl->decl.decl_name;
    int idx = get_or_create_entry(tt, name);

    if (tt->ctx->options.verbose) {
        fprintf(stderr, "  registered trait '%s' with %d methods\n",
                name, trait_decl->decl.decl_field_count);
    }

    // Register super-traits if any
    // For now, no super-trait parsing in the AST; this is a placeholder
    (void)idx;
}

void trait_register_impl(TraitTable *tt, ASTNode *impl_block) {
    if (!impl_block || impl_block->kind != N_IMPL_BLOCK) return;

    const char *trait_name = NULL;
    if (impl_block->impl_block.impl_trait &&
        impl_block->impl_block.impl_trait->kind == TY_TRAIT) {
        trait_name = impl_block->impl_block.impl_trait->trait.name;
    }

    if (!trait_name) return;

    int idx = get_or_create_entry(tt, trait_name);

    // Add impl record
    if (tt->entries[idx].impl_count >= tt->entries[idx].impl_cap) {
        tt->entries[idx].impl_cap = tt->entries[idx].impl_cap ?
            tt->entries[idx].impl_cap * 2 : 16;
    }

    ImplRecord *rec = (ImplRecord*)arena_alloc_zero(tt->arena, sizeof(ImplRecord));
    rec->trait_name = trait_name;
    rec->impl_type = impl_block->impl_block.impl_type;
    rec->impl_ast = impl_block;
    rec->generic_bindings = NULL;
    rec->generic_count = 0;

    int ic = tt->entries[idx].impl_count++;
    tt->entries[idx].impls[ic] = rec;

    if (tt->ctx->options.verbose) {
        fprintf(stderr, "  registered impl of '%s' for ", trait_name);
        ty_print_type(impl_block->impl_block.impl_type);
        fprintf(stderr, " (%d methods)\n", impl_block->impl_block.impl_method_count);
    }
}

// ─── Find Impls ───────────────────────────────────────────────────────────────

ImplRecord **trait_find_impls(TraitTable *tt, const char *trait_name, int *count) {
    if (count) *count = 0;
    int idx = find_entry(tt, trait_name);
    if (idx < 0) return NULL;

    if (count) *count = tt->entries[idx].impl_count;
    return tt->entries[idx].impls;
}

// ─── Simple type-name matching ────────────────────────────────────────────────

static bool types_match(struct Type *a, struct Type *b) {
    if (!a || !b) return false;
    if (a == b) return true;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case TY_STRUCT: case TY_ENUM:
            return a->struct_type.name && b->struct_type.name &&
                   strcmp(a->struct_type.name, b->struct_type.name) == 0;

        case TY_TUPLE: {
            if (a->tuple.count != b->tuple.count) return false;
            for (int i = 0; i < a->tuple.count; i++)
                if (!types_match(a->tuple.elems[i], b->tuple.elems[i]))
                    return false;
            return true;
        }

        case TY_PTR: case TY_REF:
            return types_match(a->ptr.elem, b->ptr.elem);

        case TY_ARRAY:
            return a->array.size == b->array.size &&
                   types_match(a->array.elem, b->array.elem);

        case TY_FN: {
            if (a->fn.param_count != b->fn.param_count) return false;
            for (int i = 0; i < a->fn.param_count; i++)
                if (!types_match(a->fn.param_types[i], b->fn.param_types[i]))
                    return false;
            return types_match(a->fn.return_type, b->fn.return_type);
        }

        case TY_GENERIC:
            return true;  // any generic matches any type

        default:
            return a->kind == b->kind;
    }
}

// ─── Trait Bound Check ────────────────────────────────────────────────────────

bool trait_check_bound(TraitTable *tt, struct Type *type,
                       const char *trait_name, Location loc) {
    if (!type || !trait_name) return true;

    int idx = find_entry(tt, trait_name);
    if (idx < 0) {
        ket_diag_push(tt->ctx, SEV_ERROR, loc.start, 0,
            "unknown trait '%s'", trait_name);
        return false;
    }

    // Check each impl of this trait
    for (int i = 0; i < tt->entries[idx].impl_count; i++) {
        ImplRecord *rec = tt->entries[idx].impls[i];
        if (types_match(rec->impl_type, type)) {
            return true;
        }
    }

    // Not found — check if it's a generic type param that has a bound
    if (type->kind == TY_GENERIC) {
        // Generic type params are assumed to satisfy their declared bounds
        // This will be checked when concrete types are substituted
        return true;
    }

    // Check super-trait bounds
    for (int i = 0; i < tt->hierarchy_count; i++) {
        if (strcmp(tt->hierarchy[i].trait_name, trait_name) == 0) {
            for (int j = 0; j < tt->hierarchy[i].supertrait_count; j++) {
                if (trait_check_bound(tt, type, tt->hierarchy[i].supertrait_names[j], loc))
                    return true;
            }
        }
    }

    // Trait not implemented
    ket_diag_push(tt->ctx, SEV_ERROR, loc.start, 0,
        "the trait '%s' is not implemented for the type '", trait_name);

    if (tt->ctx->options.verbose) {
        ty_print_type(type);
        fprintf(stderr, "'\n");
    }

    // Print available impls for this trait
    if (tt->entries[idx].impl_count > 0) {
        ket_diag_push(tt->ctx, SEV_NOTE, loc.start, 0,
            "the following implementations were found:");
        for (int i = 0; i < tt->entries[idx].impl_count; i++) {
            const char *impl_type_str = "?";
            if (tt->entries[idx].impls[i]->impl_type &&
                tt->entries[idx].impls[i]->impl_type->kind == TY_STRUCT &&
                tt->entries[idx].impls[i]->impl_type->struct_type.name) {
                impl_type_str = tt->entries[idx].impls[i]->impl_type->struct_type.name;
            }
            ket_diag_push(tt->ctx, SEV_NOTE, loc.start, 0,
                "  %d: impl %s for %s", i + 1, trait_name, impl_type_str);
        }
    }

    return false;
}

// ─── Where Clause Check ───────────────────────────────────────────────────────

bool trait_check_where(TraitTable *tt, ASTNode *where_clause,
                       struct Type **generic_bindings, int binding_count) {
    if (!where_clause) return true;

    // Walk the where clause AST
    if (where_clause->kind == N_WHERE_CLAUSE) {
        struct Type *type = where_clause->where_clause.where_type;
        struct Type *bound = where_clause->where_clause.where_bound;

        // If the type is a generic, substitute with the concrete binding
        if (type->kind == TY_GENERIC && type->generic.index < binding_count) {
            type = generic_bindings[type->generic.index];
        }

        if (bound && bound->kind == TY_TRAIT) {
            return trait_check_bound(tt, type, bound->trait.name, where_clause->span.start);
        }
    }

    return true;
}

// ─── Method Resolution ────────────────────────────────────────────────────────

ASTNode *trait_resolve_method(TraitTable *tt, struct Type *type,
                              const char *method_name,
                              const char *trait_name) {
    if (!type || !method_name) return NULL;

    // If a specific trait is named, search only that trait
    if (trait_name) {
        int idx = find_entry(tt, trait_name);
        if (idx < 0) return NULL;

        for (int i = 0; i < tt->entries[idx].impl_count; i++) {
            ImplRecord *rec = tt->entries[idx].impls[i];
            if (types_match(rec->impl_type, type)) {
                for (int j = 0; j < rec->impl_ast->impl_block.impl_method_count; j++) {
                    ASTNode *method = rec->impl_ast->impl_block.impl_methods[j];
                    if (method->kind == N_FN_DECL &&
                        strcmp(method->fn_decl.fn_name, method_name) == 0) {
                        return method;
                    }
                }
            }
        }

        return NULL;
    }

    // Without a trait name, search all traits for matching impls
    for (int e = 0; e < tt->entry_count; e++) {
        for (int i = 0; i < tt->entries[e].impl_count; i++) {
            ImplRecord *rec = tt->entries[e].impls[i];
            if (types_match(rec->impl_type, type)) {
                for (int j = 0; j < rec->impl_ast->impl_block.impl_method_count; j++) {
                    ASTNode *method = rec->impl_ast->impl_block.impl_methods[j];
                    if (method->kind == N_FN_DECL &&
                        strcmp(method->fn_decl.fn_name, method_name) == 0) {
                        return method;
                    }
                }
            }
        }
    }

    return NULL;
}

ASTNode *trait_resolve_qualified(TraitTable *tt, const char *trait_name,
                                  const char *method_name) {
    if (!trait_name || !method_name) return NULL;

    int idx = find_entry(tt, trait_name);
    if (idx < 0) return NULL;

    // Search all implementations for a method with this name
    // (This doesn't check the type — it returns the first match)
    for (int i = 0; i < tt->entries[idx].impl_count; i++) {
        ImplRecord *rec = tt->entries[idx].impls[i];
        for (int j = 0; j < rec->impl_ast->impl_block.impl_method_count; j++) {
            ASTNode *method = rec->impl_ast->impl_block.impl_methods[j];
            if (method->kind == N_FN_DECL &&
                strcmp(method->fn_decl.fn_name, method_name) == 0) {
                return method;
            }
        }
    }

    // Also check the trait declaration itself for default methods
    // (not implemented yet — would need to store trait decl AST)

    return NULL;
}

// ─── Debug ────────────────────────────────────────────────────────────────────

void trait_table_dump(TraitTable *tt) {
    fprintf(stderr, "─── Trait Table ───\n");
    fprintf(stderr, "  %d traits registered\n", tt->entry_count);

    for (int i = 0; i < tt->entry_count; i++) {
        fprintf(stderr, "  trait '%s': %d implementations\n",
                tt->entries[i].trait_name, tt->entries[i].impl_count);

        for (int j = 0; j < tt->entries[i].impl_count; j++) {
            ImplRecord *rec = tt->entries[i].impls[j];
            fprintf(stderr, "    impl %d: for ", j + 1);
            if (rec->impl_type) {
                switch (rec->impl_type->kind) {
                    case TY_STRUCT:
                        if (rec->impl_type->struct_type.name)
                            fprintf(stderr, "%s", rec->impl_type->struct_type.name);
                        else
                            fprintf(stderr, "(anonymous struct)");
                        break;
                    case TY_GENERIC:
                        fprintf(stderr, "T (generic)");
                        break;
                    default:
                        fprintf(stderr, "(kind=%d)", rec->impl_type->kind);
                        break;
                }
            } else {
                fprintf(stderr, "(null)");
            }
            fprintf(stderr, " — %d methods\n",
                    rec->impl_ast ? rec->impl_ast->impl_block.impl_method_count : 0);
        }
    }

    if (tt->hierarchy_count > 0) {
        fprintf(stderr, "  Trait Hierarchy:\n");
        for (int i = 0; i < tt->hierarchy_count; i++) {
            fprintf(stderr, "    %s: ", tt->hierarchy[i].trait_name);
            for (int j = 0; j < tt->hierarchy[i].supertrait_count; j++)
                fprintf(stderr, "%s ", tt->hierarchy[i].supertrait_names[j]);
            fprintf(stderr, "\n");
        }
    }
}
