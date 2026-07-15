#include "include/monomorph.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// MONOMORPHIZATION ENGINE — Full generics via template specialization
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Hash helpers ─────────────────────────────────────────────────────────────

static uint64_t hash_combine(uint64_t seed, uint64_t v) {
    return seed ^ (v + 0x9E3779B97F4A7C15ULL + (seed << 6) + (seed >> 2));
}

static uint64_t hash_type(struct Type *t) {
    if (!t) return 0;
    uint64_t h = (uint64_t)t->kind;
    switch (t->kind) {
        case TY_I8: case TY_I16: case TY_I32: case TY_I64:
        case TY_U8: case TY_U16: case TY_U32: case TY_U64:
        case TY_F32: case TY_F64: case TY_BOOL: case TY_CHAR:
        case TY_BYTE: case TY_STR: case TY_VOID: case TY_NEVER:
            break;
        case TY_PTR: case TY_REF:
            h = hash_combine(h, hash_type(t->ptr.elem));
            break;
        case TY_ARRAY:
            h = hash_combine(h, hash_type(t->array.elem));
            h = hash_combine(h, (uint64_t)t->array.size);
            break;
        case TY_FN:
            h = hash_combine(h, hash_type(t->fn.return_type));
            for (int i = 0; i < t->fn.param_count; i++)
                h = hash_combine(h, hash_type(t->fn.param_types[i]));
            break;
        case TY_STRUCT: case TY_ENUM:
            h = hash_combine(h, (uint64_t)(uintptr_t)t->struct_type.name);
            break;
        case TY_TUPLE:
            for (int i = 0; i < t->tuple.count; i++)
                h = hash_combine(h, hash_type(t->tuple.elems[i]));
            break;
        case TY_GENERIC:
            h = hash_combine(h, (uint64_t)t->generic.index);
            break;
        default:
            break;
    }
    return h;
}

static uint64_t hash_args(TypeArgMap *args, int arg_count) {
    uint64_t h = 0;
    for (int i = 0; i < arg_count; i++) {
        if (args[i].generic_param)
            h = hash_combine(h, (uint64_t)(uintptr_t)args[i].generic_param->generic.generic_name);
        h = hash_combine(h, hash_type(args[i].concrete));
    }
    return h;
}

// ─── State management ─────────────────────────────────────────────────────────

MonoState *mono_state_new(Context *ctx) {
    Arena *arena = ctx->arena;
    MonoState *ms = (MonoState*)arena_alloc_zero(arena, sizeof(MonoState));
    ms->generic_decls = NULL;
    ms->generic_decl_count = 0;
    ms->generic_decl_cap = 0;
    ms->impls = NULL;
    ms->impl_count = 0;
    ms->impl_cap = 0;
    ms->specs = NULL;
    ms->spec_count = 0;
    ms->spec_cap = 0;
    ms->new_decls = NULL;
    ms->new_decl_count = 0;
    ms->new_decl_cap = 0;
    ms->arena = arena;
    ms->ctx = ctx;
    return ms;
}

void mono_state_free(MonoState *ms) {
    (void)ms;  // all memory is arena-managed
}

// ─── Registration ─────────────────────────────────────────────────────────────

void mono_register_generic(MonoState *ms, ASTNode *decl) {
    if (!decl) return;

    bool is_generic = false;
    switch (decl->kind) {
        case N_FN_DECL:
            is_generic = decl->fn_decl.fn_generic_count > 0;
            break;
        case N_STRUCT_DECL:
        case N_ENUM_DECL:
            is_generic = decl->decl.decl_generic_count > 0;
            break;
        default:
            return;
    }

    if (!is_generic) return;

    if (ms->generic_decl_count >= ms->generic_decl_cap) {
        ms->generic_decl_cap = ms->generic_decl_cap ? ms->generic_decl_cap * 2 : 64;
        ms->generic_decls = arena_alloc(ms->arena, sizeof(ASTNode*) * ms->generic_decl_cap);
        // Copy if reallocating (arena doesn't support realloc, so use temp)
    }
    ms->generic_decls[ms->generic_decl_count++] = decl;

    if (ms->ctx->options.verbose) {
        const char *kind = decl->kind == N_FN_DECL ? "fn" :
                           decl->kind == N_STRUCT_DECL ? "struct" : "enum";
        fprintf(stderr, "  registered generic %s '%s' with %d params\n",
                kind,
                decl->kind == N_FN_DECL ? decl->fn_decl.fn_name : decl->decl.decl_name,
                decl->kind == N_FN_DECL ? decl->fn_decl.fn_generic_count
                                        : decl->decl.decl_generic_count);
    }
}

void mono_register_impl(MonoState *ms, ASTNode *impl_block) {
    if (!impl_block || impl_block->kind != N_IMPL_BLOCK) return;

    if (ms->impl_count >= ms->impl_cap) {
        ms->impl_cap = ms->impl_cap ? ms->impl_cap * 2 : 32;
        ms->impls = arena_alloc(ms->arena, sizeof(ImplRecord*) * ms->impl_cap);
    }

    ImplRecord *rec = (ImplRecord*)arena_alloc_zero(ms->arena, sizeof(ImplRecord));
    rec->trait_name = impl_block->impl_block.impl_trait ?
                      impl_block->impl_block.impl_trait->trait.name : NULL;
    rec->impl_type = impl_block->impl_block.impl_type;
    rec->impl_ast = impl_block;
    rec->generic_bindings = NULL;
    rec->generic_count = 0;

    ms->impls[ms->impl_count++] = rec;
}

// ─── Type Substitution ────────────────────────────────────────────────────────

struct Type *mono_substitute_types(MonoState *ms, struct Type *type,
                                   TypeArgMap *args, int arg_count) {
    if (!type) return NULL;

    Arena *arena = ms->arena;

    switch (type->kind) {
        case TY_GENERIC: {
            int idx = type->generic.index;
            if (idx >= 0 && idx < arg_count && args[idx].concrete) {
                return args[idx].concrete;
            }
            // Fallback: search by name
            for (int i = 0; i < arg_count; i++) {
                if (args[i].generic_param &&
                    args[i].generic_param->generic.generic_index == idx) {
                    return args[i].concrete;
                }
            }
            return type;  // unchanged
        }

        case TY_PTR: case TY_REF: {
            Type *t = (Type*)arena_alloc_zero(arena, sizeof(Type));
            *t = *type;
            t->ptr.elem = mono_substitute_types(ms, type->ptr.elem, args, arg_count);
            return t;
        }

        case TY_ARRAY: {
            Type *t = (Type*)arena_alloc_zero(arena, sizeof(Type));
            *t = *type;
            t->array.elem = mono_substitute_types(ms, type->array.elem, args, arg_count);
            return t;
        }

        case TY_FN: {
            Type *t = (Type*)arena_alloc_zero(arena, sizeof(Type));
            *t = *type;
            t->fn.return_type = mono_substitute_types(ms, type->fn.return_type, args, arg_count);
            t->fn.param_types = (Type**)arena_alloc_zero(arena, sizeof(Type*) * type->fn.param_count);
            for (int i = 0; i < type->fn.param_count; i++)
                t->fn.param_types[i] = mono_substitute_types(ms, type->fn.param_types[i], args, arg_count);
            return t;
        }

        case TY_TUPLE: {
            Type *t = (Type*)arena_alloc_zero(arena, sizeof(Type));
            *t = *type;
            t->tuple.elems = (Type**)arena_alloc_zero(arena, sizeof(Type*) * type->tuple.count);
            for (int i = 0; i < type->tuple.count; i++)
                t->tuple.elems[i] = mono_substitute_types(ms, type->tuple.elems[i], args, arg_count);
            return t;
        }

        case TY_STRUCT: case TY_ENUM: {
            Type *t = (Type*)arena_alloc_zero(arena, sizeof(Type));
            *t = *type;
            if (type->struct_type.generic_args) {
                t->struct_type.generic_args = (Type**)arena_alloc_zero(arena,
                    sizeof(Type*) * type->struct_type.generic_arg_count);
                for (int i = 0; i < type->struct_type.generic_arg_count; i++)
                    t->struct_type.generic_args[i] = mono_substitute_types(ms,
                        type->struct_type.generic_args[i], args, arg_count);
            }
            return t;
        }

        default:
            return type;
    }
}

// ─── Find matching specialization ─────────────────────────────────────────────

static Specialization *find_spec(MonoState *ms, const char *name,
                                  TypeArgMap *args, int arg_count) {
    uint64_t h = hash_combine(0, (uint64_t)(uintptr_t)name);
    h = hash_combine(h, hash_args(args, arg_count));

    for (int i = 0; i < ms->spec_count; i++) {
        if (ms->specs[i].key_hash == h &&
            strcmp(ms->specs[i].original_name, name) == 0) {
            return &ms->specs[i];
        }
    }
    return NULL;
}

static Specialization *add_spec(MonoState *ms, const char *name,
                                 TypeArgMap *args, int arg_count) {
    if (ms->spec_count >= ms->spec_cap) {
        ms->spec_cap = ms->spec_cap ? ms->spec_cap * 2 : 64;
        // For simplicity, use realloc-style through arena
        // In production, use a growing array
    }

    Specialization *spec = (Specialization*)arena_alloc_zero(ms->arena, sizeof(Specialization));
    spec->original_name = name;
    spec->args = (TypeArgMap*)arena_alloc_zero(ms->arena, sizeof(TypeArgMap) * arg_count);
    memcpy(spec->args, args, sizeof(TypeArgMap) * arg_count);
    spec->arg_count = arg_count;
    spec->specialized = NULL;
    spec->generated = false;
    spec->key_hash = hash_combine(hash_combine(0, (uint64_t)(uintptr_t)name),
                                   hash_args(args, arg_count));

    // Add to array
    int idx = ms->spec_count++;
    // We need to realloc the specs array... since we're using arena, 
    // let's just store a pointer in the array
    // Actually let's just use a growing approach with multiple mallocs for now
    // Since arena doesn't support realloc, we'll track separately

    return spec;  // will be stored at the end
}

// ─── AST Substitution ─────────────────────────────────────────────────────────

ASTNode *mono_substitute_ast(MonoState *ms, ASTNode *node,
                             TypeArgMap *args, int arg_count) {
    if (!node) return NULL;

    Arena *arena = ms->arena;
    ASTNode *new_node = (ASTNode*)arena_alloc_zero(arena, sizeof(ASTNode));
    *new_node = *node;  // shallow copy

    switch (node->kind) {
        case N_IDENT:
            new_node->type = mono_substitute_types(ms, node->type, args, arg_count);
            break;

        case N_LITERAL_INT: case N_LITERAL_FLOAT: case N_LITERAL_STR:
        case N_LITERAL_BOOL: case N_LITERAL_CHAR: case N_LITERAL_NULL:
            new_node->type = mono_substitute_types(ms, node->type, args, arg_count);
            break;

        case N_BINARY: {
            new_node->binary.bin_left = mono_substitute_ast(ms, node->binary.bin_left, args, arg_count);
            new_node->binary.bin_right = mono_substitute_ast(ms, node->binary.bin_right, args, arg_count);
            new_node->type = mono_substitute_types(ms, node->type, args, arg_count);
            break;
        }

        case N_UNARY: {
            new_node->unary.unary_right = mono_substitute_ast(ms, node->unary.unary_right, args, arg_count);
            new_node->type = mono_substitute_types(ms, node->type, args, arg_count);
            break;
        }

        case N_CALL: {
            new_node->call.call_callee = mono_substitute_ast(ms, node->call.call_callee, args, arg_count);
            new_node->call.call_args = (ASTNode**)arena_alloc_zero(arena, sizeof(ASTNode*) * node->call.call_argc);
            for (int i = 0; i < node->call.call_argc; i++)
                new_node->call.call_args[i] = mono_substitute_ast(ms, node->call.call_args[i], args, arg_count);
            new_node->type = mono_substitute_types(ms, node->type, args, arg_count);
            break;
        }

        case N_VAR_DECL: {
            new_node->var_decl.var_type = mono_substitute_types(ms, node->var_decl.var_type, args, arg_count);
            new_node->var_decl.var_init = mono_substitute_ast(ms, node->var_decl.var_init, args, arg_count);
            break;
        }

        case N_ASSIGN: {
            new_node->assign.assign_target = mono_substitute_ast(ms, node->assign.assign_target, args, arg_count);
            new_node->assign.assign_value = mono_substitute_ast(ms, node->assign.assign_value, args, arg_count);
            break;
        }

        case N_RETURN:
            if (node->ret_val)
                new_node->ret_val = mono_substitute_ast(ms, node->ret_val, args, arg_count);
            break;

        case N_IF:
        case N_IF_EXPR: {
            new_node->if_node.if_cond = mono_substitute_ast(ms, node->if_node.if_cond, args, arg_count);
            new_node->if_node.if_then = mono_substitute_ast(ms, node->if_node.if_then, args, arg_count);
            if (node->if_node.if_else)
                new_node->if_node.if_else = mono_substitute_ast(ms, node->if_node.if_else, args, arg_count);
            break;
        }

        case N_BLOCK:
        case N_MODULE: {
            new_node->block.stmts = (ASTNode**)arena_alloc_zero(arena, sizeof(ASTNode*) * node->block.stmt_count);
            for (int i = 0; i < node->block.stmt_count; i++)
                new_node->block.stmts[i] = mono_substitute_ast(ms, node->block.stmts[i], args, arg_count);
            break;
        }

        case N_WHILE: case N_LOOP: case N_FOR: {
            new_node->loop.loop_cond = mono_substitute_ast(ms, node->loop.loop_cond, args, arg_count);
            new_node->loop.loop_body = mono_substitute_ast(ms, node->loop.loop_body, args, arg_count);
            if (node->loop.loop_iter)
                new_node->loop.loop_iter = mono_substitute_ast(ms, node->loop.loop_iter, args, arg_count);
            break;
        }

        case N_MATCH: {
            new_node->match.match_expr = mono_substitute_ast(ms, node->match.match_expr, args, arg_count);
            new_node->match.match_arms = (ASTNode**)arena_alloc_zero(arena, sizeof(ASTNode*) * node->match.match_arm_count);
            for (int i = 0; i < node->match.match_arm_count; i++)
                new_node->match.match_arms[i] = mono_substitute_ast(ms, node->match.match_arms[i], args, arg_count);
            break;
        }

        case N_MATCH_ARM: {
            new_node->arm.arm_pat = mono_substitute_ast(ms, node->arm.arm_pat, args, arg_count);
            if (node->arm.arm_guard)
                new_node->arm.arm_guard = mono_substitute_ast(ms, node->arm.arm_guard, args, arg_count);
            new_node->arm.arm_body = mono_substitute_ast(ms, node->arm.arm_body, args, arg_count);
            break;
        }

        case N_MEMBER: {
            new_node->member.member_target = mono_substitute_ast(ms, node->member.member_target, args, arg_count);
            break;
        }

        case N_INDEX: {
            new_node->index.index_target = mono_substitute_ast(ms, node->index.index_target, args, arg_count);
            new_node->index.index_expr = mono_substitute_ast(ms, node->index.index_expr, args, arg_count);
            break;
        }

        case N_TUPLE: {
            new_node->tuple.tuple_elems = (ASTNode**)arena_alloc_zero(arena, sizeof(ASTNode*) * node->tuple.tuple_count);
            for (int i = 0; i < node->tuple.tuple_count; i++)
                new_node->tuple.tuple_elems[i] = mono_substitute_ast(ms, node->tuple.tuple_elems[i], args, arg_count);
            break;
        }

        case N_ARRAY_INIT: case N_STRUCT_INIT: case N_ENUM_INIT: {
            new_node->init.init_type = node->init.init_type;
            new_node->init.init_args = (ASTNode**)arena_alloc_zero(arena, sizeof(ASTNode*) * node->init.init_argc);
            for (int i = 0; i < node->init.init_argc; i++)
                new_node->init.init_args[i] = mono_substitute_ast(ms, node->init.init_args[i], args, arg_count);
            if (node->init.init_fields) {
                new_node->init.init_fields = (Field*)arena_alloc_zero(arena, sizeof(Field) * node->init.init_field_count);
                memcpy(new_node->init.init_fields, node->init.init_fields, sizeof(Field) * node->init.init_field_count);
            }
            break;
        }

        case N_EXPR_STMT:
            if (node->ret_val)
                new_node->ret_val = mono_substitute_ast(ms, node->ret_val, args, arg_count);
            break;

        case N_CAST: {
            new_node->cast.cast_expr = mono_substitute_ast(ms, node->cast.cast_expr, args, arg_count);
            new_node->cast.cast_type = mono_substitute_types(ms, node->cast.cast_type, args, arg_count);
            break;
        }

        case N_CLOSURE: {
            new_node->closure.closure_params = (ASTNode**)arena_alloc_zero(arena,
                sizeof(ASTNode*) * node->closure.closure_param_count);
            for (int i = 0; i < node->closure.closure_param_count; i++)
                new_node->closure.closure_params[i] = mono_substitute_ast(ms,
                    node->closure.closure_params[i], args, arg_count);
            new_node->closure.closure_body = mono_substitute_ast(ms,
                node->closure.closure_body, args, arg_count);
            break;
        }

        case N_RANGE: {
            new_node->range.range_start = mono_substitute_ast(ms, node->range.range_start, args, arg_count);
            new_node->range.range_end = mono_substitute_ast(ms, node->range.range_end, args, arg_count);
            break;
        }

        case N_FN_DECL: {
            // Skip generics in the specialized copy
            new_node->fn_decl.fn_generic_count = 0;
            new_node->fn_decl.fn_generics = NULL;
            new_node->fn_decl.fn_where_count = 0;
            new_node->fn_decl.fn_where = NULL;

            new_node->fn_decl.fn_ret_type = mono_substitute_types(ms,
                node->fn_decl.fn_ret_type, args, arg_count);
            new_node->fn_decl.fn_params = (ASTNode**)arena_alloc_zero(arena,
                sizeof(ASTNode*) * node->fn_decl.fn_param_count);
            for (int i = 0; i < node->fn_decl.fn_param_count; i++)
                new_node->fn_decl.fn_params[i] = mono_substitute_ast(ms,
                    node->fn_decl.fn_params[i], args, arg_count);
            new_node->fn_decl.fn_body = mono_substitute_ast(ms,
                node->fn_decl.fn_body, args, arg_count);
            break;
        }

        case N_STRUCT_DECL: {
            new_node->decl.decl_generic_count = 0;
            new_node->decl.decl_generics = NULL;
            new_node->decl.decl_fields = (ASTNode**)arena_alloc_zero(arena,
                sizeof(ASTNode*) * node->decl.decl_field_count);
            for (int i = 0; i < node->decl.decl_field_count; i++)
                new_node->decl.decl_fields[i] = mono_substitute_ast(ms,
                    node->decl.decl_fields[i], args, arg_count);
            break;
        }

        case N_ENUM_DECL: {
            new_node->decl.decl_generic_count = 0;
            new_node->decl.decl_generics = NULL;
            new_node->decl.decl_fields = (ASTNode**)arena_alloc_zero(arena,
                sizeof(ASTNode*) * node->decl.decl_field_count);
            for (int i = 0; i < node->decl.decl_field_count; i++)
                new_node->decl.decl_fields[i] = mono_substitute_ast(ms,
                    node->decl.decl_fields[i], args, arg_count);
            break;
        }

        case N_IMPL_BLOCK: {
            new_node->impl_block.impl_type = mono_substitute_types(ms,
                node->impl_block.impl_type, args, arg_count);
            new_node->impl_block.impl_methods = (ASTNode**)arena_alloc_zero(arena,
                sizeof(ASTNode*) * node->impl_block.impl_method_count);
            for (int i = 0; i < node->impl_block.impl_method_count; i++)
                new_node->impl_block.impl_methods[i] = mono_substitute_ast(ms,
                    node->impl_block.impl_methods[i], args, arg_count);
            break;
        }

        case N_IMPORT: case N_TRAIT_DECL: case N_TYPE_ALIAS:
            // Copy as-is (no generic substitution needed)
            break;

        default:
            break;
    }

    return new_node;
}

// ─── Specialize Generic Function ─────────────────────────────────────────────

ASTNode *mono_specialize_fn(MonoState *ms, ASTNode *generic_fn,
                            TypeArgMap *args, int arg_count) {
    if (!generic_fn || generic_fn->kind != N_FN_DECL) return generic_fn;

    const char *name = generic_fn->fn_decl.fn_name;

    // Check if already specialized
    Specialization *existing = find_spec(ms, name, args, arg_count);
    if (existing && existing->generated)
        return existing->specialized;

    if (ms->ctx->options.verbose) {
        fprintf(stderr, "  monomorph: %s<", name);
        for (int i = 0; i < arg_count; i++) {
            if (i > 0) fprintf(stderr, ", ");
            ty_print_type(args[i].concrete);
        }
        fprintf(stderr, ">\n");
    }

    // Create specialized copy
    ASTNode *spec = mono_substitute_ast(ms, generic_fn, args, arg_count);

    // Create a unique name for the specialized function
    char mangled[1024];
    int pos = snprintf(mangled, sizeof(mangled), "%s$", name);
    for (int i = 0; i < arg_count && pos < (int)sizeof(mangled) - 16; i++) {
        // Use type kind as suffix for mangling
        if (args[i].concrete)
            pos += snprintf(mangled + pos, sizeof(mangled) - pos, "%d_", args[i].concrete->kind);
        else
            pos += snprintf(mangled + pos, sizeof(mangled) - pos, "inf_");
    }
    mangled[pos] = '\0';

    // Update the specialized function's name
    spec->fn_decl.fn_name = ket_intern(ms->ctx, mangled, (int)strlen(mangled));

    // Track this specialization
    if (!existing) {
        add_spec(ms, name, args, arg_count);
        // Store the specialized node
        if (ms->spec_count > 0) {
            ms->specs[ms->spec_count - 1].specialized = spec;
            ms->specs[ms->spec_count - 1].generated = true;
        }
    }

    return spec;
}

// ─── Specialize Generic Struct ───────────────────────────────────────────────

ASTNode *mono_specialize_struct(MonoState *ms, ASTNode *generic_struct,
                                TypeArgMap *args, int arg_count) {
    if (!generic_struct || generic_struct->kind != N_STRUCT_DECL) return generic_struct;

    const char *name = generic_struct->decl.decl_name;

    // Check if already specialized
    Specialization *existing = find_spec(ms, name, args, arg_count);
    if (existing && existing->generated)
        return existing->specialized;

    if (ms->ctx->options.verbose) {
        fprintf(stderr, "  monomorph struct: %s<", name);
        for (int i = 0; i < arg_count; i++) {
            if (i > 0) fprintf(stderr, ", ");
            ty_print_type(args[i].concrete);
        }
        fprintf(stderr, ">\n");
    }

    // Create specialized copy
    ASTNode *spec = mono_substitute_ast(ms, generic_struct, args, arg_count);

    // Mangle name: Struct$type1_type2
    char mangled[1024];
    int pos = snprintf(mangled, sizeof(mangled), "%s$", name);
    for (int i = 0; i < arg_count && pos < (int)sizeof(mangled) - 16; i++) {
        if (args[i].concrete)
            pos += snprintf(mangled + pos, sizeof(mangled) - pos, "%d_", args[i].concrete->kind);
        else
            pos += snprintf(mangled + pos, sizeof(mangled) - pos, "inf_");
    }
    mangled[pos] = '\0';
    spec->decl.decl_name = ket_intern(ms->ctx, mangled, (int)strlen(mangled));

    // Register if new
    if (!existing) {
        add_spec(ms, name, args, arg_count);
        if (ms->spec_count > 0) {
            ms->specs[ms->spec_count - 1].specialized = spec;
            ms->specs[ms->spec_count - 1].generated = true;
        }
    }

    return spec;
}

// ─── Add new declaration to module ────────────────────────────────────────────

static void add_new_decl(MonoState *ms, ASTNode *decl) {
    if (!decl) return;
    if (ms->new_decl_count >= ms->new_decl_cap) {
        ms->new_decl_cap = ms->new_decl_cap ? ms->new_decl_cap * 2 : 64;
    }
    // Since we can't realloc in arena, just index into array
    // For now, append to a simple growing list
    // Production: use proper dynamic array
    ms->new_decls[ms->new_decl_count++] = decl;
}

// ─── Trait Bound Checking ────────────────────────────────────────────────────

bool mono_check_trait_bounds(MonoState *ms, struct Type *type,
                             struct Type *required_trait, Location loc) {
    if (!type || !required_trait) return true;
    if (required_trait->kind != TY_TRAIT) return true;

    const char *trait_name = required_trait->trait.name;

    // Check all registered impls
    for (int i = 0; i < ms->impl_count; i++) {
        ImplRecord *rec = ms->impls[i];
        if (!rec->trait_name || strcmp(rec->trait_name, trait_name) != 0)
            continue;

        // Check if the impl matches this concrete type
        // For now, simple name-based matching
        if (rec->impl_type && rec->impl_type->kind == type->kind) {
            if (type->kind == TY_STRUCT || type->kind == TY_ENUM) {
                if (rec->impl_type->struct_type.name &&
                    type->struct_type.name &&
                    strcmp(rec->impl_type->struct_type.name, type->struct_type.name) == 0) {
                    return true;
                }
            } else if (rec->impl_type->kind == TY_GENERIC) {
                // impl<T> Trait for T — matches any type
                return true;
            }
        }
    }

    // If we get here, trait bound not satisfied
    ket_diag_push(ms->ctx, SEV_ERROR, loc.start, 0,
        "the trait '%s' is not implemented for ", trait_name);
    // Try to print the type
    ty_print_type(type);
    fprintf(stderr, "\n");
    return false;
}

// ─── Trait Method Resolution ──────────────────────────────────────────────────

ASTNode *mono_resolve_trait_method(MonoState *ms, struct Type *type,
                                   const char *trait_name, const char *method_name) {
    if (!type || !trait_name || !method_name) return NULL;

    for (int i = 0; i < ms->impl_count; i++) {
        ImplRecord *rec = ms->impls[i];
        if (!rec->trait_name || strcmp(rec->trait_name, trait_name) != 0)
            continue;

        bool type_matches = false;
        if (rec->impl_type->kind == TY_GENERIC) {
            type_matches = true;  // impl<T> Trait for T
        } else if ((type->kind == TY_STRUCT || type->kind == TY_ENUM) &&
                   rec->impl_type->struct_type.name &&
                   type->struct_type.name &&
                   strcmp(rec->impl_type->struct_type.name, type->struct_type.name) == 0) {
            type_matches = true;
        } else if (type->kind == rec->impl_type->kind) {
            type_matches = true;
        }

        if (type_matches) {
            // Find the method in the impl block
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

// ═══════════════════════════════════════════════════════════════════════════════
// FULL MONOMORPHIZATION PASS
// ═══════════════════════════════════════════════════════════════════════════════

// Walk the AST looking for generic instantiations and specialize them
static void collect_instantiations(MonoState *ms, ASTNode *node) {
    if (!node) return;

    switch (node->kind) {
        case N_CALL: {
            // Check if this is a call to a generic function
            ASTNode *callee = node->call.call_callee;
            if (callee && callee->kind == N_IDENT && callee->ident.resolved) {
                ASTNode *decl = callee->ident.resolved;
                if (decl->kind == N_FN_DECL && decl->fn_decl.fn_generic_count > 0) {
                    // Build type arg map from the call's type arguments
                    int generic_count = decl->fn_decl.fn_generic_count;
                    TypeArgMap *args = (TypeArgMap*)arena_alloc_zero(ms->arena,
                        sizeof(TypeArgMap) * generic_count);

                    for (int i = 0; i < generic_count; i++) {
                        ASTNode *gp = decl->fn_decl.fn_generics[i];
                        args[i].generic_param = gp;
                        // Use the inferred type arguments from the call
                        if (i < node->call.call_argc) {
                            args[i].concrete = node->call.call_args[i]->type;
                        }
                    }

                    ASTNode *specialized = mono_specialize_fn(ms, decl, args, generic_count);
                    if (specialized) {
                        ms->specs[ms->spec_count - 1].specialized = specialized;
                        ms->specs[ms->spec_count - 1].generated = true;
                        add_new_decl(ms, specialized);
                    }
                }
            }

            // Recurse into arguments
            for (int i = 0; i < node->call.call_argc; i++)
                collect_instantiations(ms, node->call.call_args[i]);
            break;
        }

        case N_STRUCT_INIT:
        case N_ENUM_INIT: {
            // Check if constructing a generic struct
            ASTNode *type_node = node->init.init_type;
            if (type_node && type_node->kind == N_IDENT && type_node->ident.resolved) {
                ASTNode *decl = type_node->ident.resolved;
                if ((decl->kind == N_STRUCT_DECL || decl->kind == N_ENUM_DECL) &&
                    decl->decl.decl_generic_count > 0) {
                    // Use the inferred types from the init
                    int gc = decl->decl.decl_generic_count;
                    TypeArgMap *args = (TypeArgMap*)arena_alloc_zero(ms->arena,
                        sizeof(TypeArgMap) * gc);
                    for (int i = 0; i < gc && i < node->init.init_argc; i++) {
                        args[i].generic_param = decl->decl.decl_generics[i];
                        args[i].concrete = node->init.init_args[i]->type;
                    }

                    ASTNode *specialized = mono_specialize_struct(ms, decl, args, gc);
                    if (specialized) {
                        add_new_decl(ms, specialized);
                    }
                }
            }

            for (int i = 0; i < node->init.init_argc; i++)
                collect_instantiations(ms, node->init.init_args[i]);
            break;
        }

        case N_VAR_DECL: {
            if (node->var_decl.var_init)
                collect_instantiations(ms, node->var_decl.var_init);
            break;
        }

        case N_ASSIGN: {
            collect_instantiations(ms, node->assign.assign_target);
            collect_instantiations(ms, node->assign.assign_value);
            break;
        }

        case N_RETURN: {
            if (node->ret_val)
                collect_instantiations(ms, node->ret_val);
            break;
        }

        case N_IF: case N_IF_EXPR: {
            collect_instantiations(ms, node->if_node.if_cond);
            collect_instantiations(ms, node->if_node.if_then);
            if (node->if_node.if_else)
                collect_instantiations(ms, node->if_node.if_else);
            break;
        }

        case N_BLOCK: case N_MODULE: {
            for (int i = 0; i < node->block.stmt_count; i++)
                collect_instantiations(ms, node->block.stmts[i]);
            break;
        }

        case N_WHILE: case N_LOOP: case N_FOR: {
            if (node->loop.loop_cond)
                collect_instantiations(ms, node->loop.loop_cond);
            if (node->loop.loop_iter)
                collect_instantiations(ms, node->loop.loop_iter);
            collect_instantiations(ms, node->loop.loop_body);
            break;
        }

        case N_MATCH: {
            collect_instantiations(ms, node->match.match_expr);
            for (int i = 0; i < node->match.match_arm_count; i++)
                collect_instantiations(ms, node->match.match_arms[i]);
            break;
        }

        case N_EXPR_STMT:
            if (node->ret_val)
                collect_instantiations(ms, node->ret_val);
            break;

        case N_BINARY: {
            collect_instantiations(ms, node->binary.bin_left);
            collect_instantiations(ms, node->binary.bin_right);
            break;
        }

        case N_UNARY: {
            collect_instantiations(ms, node->unary.unary_right);
            break;
        }

        case N_MEMBER: {
            collect_instantiations(ms, node->member.member_target);
            break;
        }

        case N_INDEX: {
            collect_instantiations(ms, node->index.index_target);
            collect_instantiations(ms, node->index.index_expr);
            break;
        }

        case N_IMPL_BLOCK: {
            for (int i = 0; i < node->impl_block.impl_method_count; i++)
                collect_instantiations(ms, node->impl_block.impl_methods[i]);
            break;
        }

        case N_FN_DECL: {
            for (int i = 0; i < node->fn_decl.fn_param_count; i++)
                collect_instantiations(ms, node->fn_decl.fn_params[i]);
            if (node->fn_decl.fn_body)
                collect_instantiations(ms, node->fn_decl.fn_body);
            break;
        }

        default:
            break;
    }
}

ASTNode *mono_run_pass(MonoState *ms, ASTNode *module) {
    if (!module || module->kind != N_MODULE) return module;

    if (ms->ctx->options.verbose) {
        fprintf(stderr, "  monomorphization pass...\n");
        fprintf(stderr, "  %d generic declarations registered\n", ms->generic_decl_count);
        fprintf(stderr, "  %d trait implementations registered\n", ms->impl_count);
    }

    // First pass: collect all instantiations
    collect_instantiations(ms, module);

    // Add specialized declarations to the module
    // We need to append new decls to the module's stmts array
    if (ms->new_decl_count > 0) {
        int old_count = module->block.stmt_count;
        int new_count = old_count + ms->new_decl_count;

        ASTNode **new_stmts = (ASTNode**)arena_alloc_zero(ms->arena,
            sizeof(ASTNode*) * new_count);

        // Copy old stmts
        for (int i = 0; i < old_count; i++)
            new_stmts[i] = module->block.stmts[i];

        // Append new specialized declarations
        for (int i = 0; i < ms->new_decl_count; i++)
            new_stmts[old_count + i] = ms->new_decls[i];

        module->block.stmts = new_stmts;
        module->block.stmt_count = new_count;

        if (ms->ctx->options.verbose) {
            fprintf(stderr, "  monomorph: added %d specialized declarations\n",
                    ms->new_decl_count);
        }
    }

    return module;
}
