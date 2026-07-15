#include "include/typecheck.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE CHECKER — Hindley-Milner bidirectional type inference engine
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Type Variable Management ────────────────────────────────────────────────

static TypeVar *tyvar_new(UnifyState *us, bool is_generic) {
    if (us->var_count >= us->var_cap) {
        us->var_cap = us->var_cap ? us->var_cap * 2 : 64;
        us->vars = realloc(us->vars, sizeof(TypeVar*) * us->var_cap);
    }
    TypeVar *tv = calloc(1, sizeof(TypeVar));
    tv->id = us->next_id++;
    tv->instance = NULL;
    tv->link = NULL;
    tv->rank = 0;
    tv->is_generic = is_generic;
    us->vars[us->var_count++] = tv;
    return tv;
}

// ─── Environment ─────────────────────────────────────────────────────────────

TyEnv *ty_env_new(TyEnv *parent, SymTable *syms, Context *ctx) {
    TyEnv *env = calloc(1, sizeof(TyEnv));
    env->parent = parent;
    env->syms = syms;
    env->ctx = ctx;
    env->ret_type = NULL;
    env->break_type = NULL;
    env->in_loop = false;
    env->constraints = NULL;
    env->constraints_last = NULL;
    env->constraint_count = 0;
    env->generics = NULL;
    env->generic_count = 0;

    if (parent) {
        env->unify = parent->unify;
    } else {
        env->unify = calloc(1, sizeof(UnifyState));
        env->unify->vars = NULL;
        env->unify->var_count = 0;
        env->unify->var_cap = 0;
        env->unify->next_id = 0;
    }

    return env;
}

// ─── Fresh type variables ────────────────────────────────────────────────────

Type *ty_fresh_var(TyEnv *env) {
    Type *t = calloc(1, sizeof(Type));
    t->kind = TY_INFER;
    // Store the var id in bit_width field (hack)
    TypeVar *tv = tyvar_new(env->unify, false);
    t->bit_width = tv->id;
    return t;
}

Type *ty_fresh_generic(TyEnv *env, const char *name) {
    Type *t = calloc(1, sizeof(Type));
    t->kind = TY_GENERIC;
    TypeVar *tv = tyvar_new(env->unify, true);
    t->bit_width = tv->id;
    t->generic.name = name;
    return t;
}

// ─── Type printing ───────────────────────────────────────────────────────────

void ty_print_type(Type *type) {
    if (!type) { printf("(null)"); return; }
    switch (type->kind) {
        case TY_NEVER:  printf("never"); break;
        case TY_VOID:   printf("void"); break;
        case TY_BOOL:   printf("bool"); break;
        case TY_BYTE:   printf("byte"); break;
        case TY_CHAR:   printf("char"); break;
        case TY_I8:     printf("i8"); break;
        case TY_I16:    printf("i16"); break;
        case TY_I32:    printf("i32"); break;
        case TY_I64:    printf("i64"); break;
        case TY_I128:   printf("i128"); break;
        case TY_U8:     printf("u8"); break;
        case TY_U16:    printf("u16"); break;
        case TY_U32:    printf("u32"); break;
        case TY_U64:    printf("u64"); break;
        case TY_U128:   printf("u128"); break;
        case TY_F32:    printf("f32"); break;
        case TY_F64:    printf("f64"); break;
        case TY_STR:    printf("str"); break;
        case TY_INFER:  printf("'t%d", type->bit_width); break;
        case TY_GENERIC: printf("%s", type->generic.name ? type->generic.name : "T"); break;
        case TY_ARRAY:
            printf("[");
            if (type->array.elem) ty_print_type(type->array.elem);
            if (type->array.count > 0) printf("; %lld", (long long)type->array.count);
            printf("]");
            break;
        case TY_PTR:   printf("*"); ty_print_type(type->ptr.pointee); break;
        case TY_REF:   printf("&"); ty_print_type(type->ptr.pointee); break;
        case TY_TUPLE:
            printf("(");
            for (int i = 0; i < type->tuple.elem_count; i++) {
                if (i) printf(", ");
                ty_print_type(type->tuple.elems[i]);
            }
            printf(")");
            break;
        case TY_FN:
            printf("fn(");
            for (int i = 0; i < type->fn.param_count; i++) {
                if (i) printf(", ");
                ty_print_type(type->fn.param_types[i]);
            }
            printf(") -> ");
            ty_print_type(type->fn.return_type);
            break;
        case TY_STRUCT:
            printf("%s", type->struct_type.name ? type->struct_type.name : "struct");
            break;
        case TY_ENUM:
            printf("%s", type->enum_type.name ? type->enum_type.name : "enum");
            break;
        case TY_ERROR:
            printf("!!ERROR!!");
            break;
        default:
            printf("?kind=%d?", type->kind);
    }
}

const char *ty_type_to_str(Type *type, Arena *arena) {
    // Capture printed output to string
    // For now, use a simple approach
    char buf[512];
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    if (!f) return "?";
    // Would need proper ty_print_type_to_file, simplified
    fclose(f);
    return arena_strdup(arena, buf);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UNIFICATION
// ═══════════════════════════════════════════════════════════════════════════════

// Find the root type variable in the union-find structure
static TypeVar *tyvar_find(TypeVar *tv) {
    if (!tv->link) return tv;
    TypeVar *root = tyvar_find(tv->link);
    tv->link = root; // path compression
    return root;
}

// Union two type variable equivalence classes
static TypeVar *tyvar_union(TypeVar *a, TypeVar *b) {
    TypeVar *ra = tyvar_find(a);
    TypeVar *rb = tyvar_find(b);
    if (ra == rb) return ra;
    // Union by rank
    if (ra->rank < rb->rank) {
        ra->link = rb;
        return rb;
    } else if (ra->rank > rb->rank) {
        rb->link = ra;
        return ra;
    } else {
        rb->link = ra;
        ra->rank++;
        return ra;
    }
}

// Recursively resolve a type through type variable links
static Type *ty_resolve(Type *type) {
    if (!type) return NULL;

    if (type->kind == TY_INFER || type->kind == TY_GENERIC) {
        // Find the type variable
        // For now, simplified: check if we stored instance info
        if (type->ptr.pointee) {
            return ty_resolve(type->ptr.pointee);
        }
        return type;
    }

    // Recursively resolve sub-types
    if (type->kind == TY_ARRAY && type->array.elem) {
        type->array.elem = ty_resolve(type->array.elem);
    }
    if ((type->kind == TY_PTR || type->kind == TY_REF || type->kind == TY_MUT_REF) && type->ptr.pointee) {
        type->ptr.pointee = ty_resolve(type->ptr.pointee);
    }
    if (type->kind == TY_TUPLE) {
        for (int i = 0; i < type->tuple.elem_count; i++) {
            type->tuple.elems[i] = ty_resolve(type->tuple.elems[i]);
        }
    }
    if (type->kind == TY_FN) {
        for (int i = 0; i < type->fn.param_count; i++) {
            type->fn.param_types[i] = ty_resolve(type->fn.param_types[i]);
        }
        type->fn.return_type = ty_resolve(type->fn.return_type);
    }

    return type;
}

// Check if a type variable occurs in a type (occurs check)
static bool ty_occurs(TypeVar *tv, Type *type) {
    if (!type) return false;

    if (type->kind == TY_INFER || type->kind == TY_GENERIC) {
        // For now simplified
        if (type->bit_width == tv->id) return true;
        return false;
    }

    if (type->kind == TY_ARRAY && type->array.elem)
        return ty_occurs(tv, type->array.elem);
    if ((type->kind == TY_PTR || type->kind == TY_REF) && type->ptr.pointee)
        return ty_occurs(tv, type->ptr.pointee);
    if (type->kind == TY_TUPLE) {
        for (int i = 0; i < type->tuple.elem_count; i++) {
            if (ty_occurs(tv, type->tuple.elems[i])) return true;
        }
    }
    if (type->kind == TY_FN) {
        for (int i = 0; i < type->fn.param_count; i++) {
            if (ty_occurs(tv, type->fn.param_types[i])) return true;
        }
        if (ty_occurs(tv, type->fn.return_type)) return true;
    }

    return false;
}

// ─── Main unification ────────────────────────────────────────────────────────

bool ty_unify(TyEnv *env, Type *a, Type *b, Location loc) {
    (void)env;
    if (!a || !b) return true;
    if (a->kind == TY_ERROR || b->kind == TY_ERROR) return true;

    // Resolve through type variables
    a = ty_resolve(a);
    b = ty_resolve(b);

    if (a == b) return true;

    // Type variable ~ Type
    if (a->kind == TY_INFER || a->kind == TY_GENERIC) {
        if (ty_occurs(NULL, b)) {
            ket_diag_push(env->ctx, SEV_ERROR, loc, 0,
                "recursive type: %s contains itself", "type");
            return false;
        }
        a->ptr.pointee = b; // store resolution
        return true;
    }
    if (b->kind == TY_INFER || b->kind == TY_GENERIC) {
        if (ty_occurs(NULL, a)) {
            ket_diag_push(env->ctx, SEV_ERROR, loc, 0,
                "recursive type");
            return false;
        }
        b->ptr.pointee = a;
        return true;
    }

    // Structural comparison
    if (a->kind != b->kind) {
        // Implicit conversions
        if (a->kind == TY_I8 && b->kind == TY_I64) return true;
        if (a->kind == TY_I16 && b->kind == TY_I64) return true;
        if (a->kind == TY_I32 && b->kind == TY_I64) return true;
        if (a->kind == TY_U8 && b->kind == TY_I64) return true;
        if (a->kind == TY_F32 && b->kind == TY_F64) return true;

        ket_diag_push(env->ctx, SEV_ERROR, loc, 0, "type mismatch: expected ");
        ty_print_type(b);
        ket_diag_push(env->ctx, SEV_NOTE, loc, 0, "but got ");
        ty_print_type(a);
        return false;
    }

    switch (a->kind) {
        case TY_ARRAY:
            if (a->array.count != b->array.count && a->array.count >= 0 && b->array.count >= 0) {
                ket_diag_push(env->ctx, SEV_ERROR, loc, 0,
                    "array size mismatch: %lld vs %lld",
                    (long long)a->array.count, (long long)b->array.count);
                return false;
            }
            return ty_unify(env, a->array.elem, b->array.elem, loc);

        case TY_PTR: case TY_REF: case TY_MUT_REF:
            return ty_unify(env, a->ptr.pointee, b->ptr.pointee, loc);

        case TY_TUPLE:
            if (a->tuple.elem_count != b->tuple.elem_count) {
                ket_diag_push(env->ctx, SEV_ERROR, loc, 0,
                    "tuple length mismatch: %d vs %d",
                    a->tuple.elem_count, b->tuple.elem_count);
                return false;
            }
            for (int i = 0; i < a->tuple.elem_count; i++) {
                if (!ty_unify(env, a->tuple.elems[i], b->tuple.elems[i], loc))
                    return false;
            }
            return true;

        case TY_FN:
            if (a->fn.param_count != b->fn.param_count) {
                ket_diag_push(env->ctx, SEV_ERROR, loc, 0,
                    "function parameter count mismatch");
                return false;
            }
            for (int i = 0; i < a->fn.param_count; i++) {
                if (!ty_unify(env, a->fn.param_types[i], b->fn.param_types[i], loc))
                    return false;
            }
            return ty_unify(env, a->fn.return_type, b->fn.return_type, loc);

        case TY_STRUCT:
            if (a->struct_type.name && b->struct_type.name &&
                strcmp(a->struct_type.name, b->struct_type.name) != 0) {
                ket_diag_push(env->ctx, SEV_ERROR, loc, 0,
                    "struct mismatch: %s vs %s",
                    a->struct_type.name, b->struct_type.name);
                return false;
            }
            return true;

        case TY_ENUM:
            if (a->enum_type.name && b->enum_type.name &&
                strcmp(a->enum_type.name, b->enum_type.name) != 0) {
                return false;
            }
            return true;

        default:
            // Same kind and primitive — match
            return true;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRAINT SOLVER
// ═══════════════════════════════════════════════════════════════════════════════

void ty_add_constraint(TyEnv *env, ConstraintKind kind, Type *left,
                        Type *right, Location loc) {
    Constraint *c = calloc(1, sizeof(Constraint));
    c->kind = kind;
    c->left = left;
    c->right = right;
    c->loc = loc;
    c->next = NULL;

    if (env->constraints_last) {
        env->constraints_last->next = c;
    } else {
        env->constraints = c;
    }
    env->constraints_last = c;
    env->constraint_count++;
}

bool ty_solve_constraints(TyEnv *env) {
    Constraint *c = env->constraints;
    bool ok = true;

    while (c) {
        switch (c->kind) {
            case CON_EQ:
                if (!ty_unify(env, c->left, c->right, c->loc)) {
                    ok = false;
                }
                break;
            case CON_SUBTYPE:
                // Subtype checking would go here
                break;
            case CON_TRAIT:
                // Trait bound checking would go here
                break;
            case CON_LIFETIME:
                // Lifetime checking would go here
                break;
        }
        Constraint *next = c->next;
        free(c);
        c = next;
    }

    env->constraints = NULL;
    env->constraints_last = NULL;
    env->constraint_count = 0;

    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GENERICS: INSTANTIATION & GENERALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

Type *ty_instantiate(TyEnv *env, Type *generic_type) {
    if (!generic_type) return NULL;

    // Replace generic params with fresh type variables
    if (generic_type->kind == TY_GENERIC) {
        return ty_fresh_var(env);
    }

    if (generic_type->kind == TY_ARRAY && generic_type->array.elem) {
        Type *t = calloc(1, sizeof(Type));
        *t = *generic_type;
        t->array.elem = ty_instantiate(env, generic_type->array.elem);
        return t;
    }

    if (generic_type->kind == TY_FN) {
        Type *t = calloc(1, sizeof(Type));
        *t = *generic_type;
        t->fn.param_types = calloc(t->fn.param_count, sizeof(Type*));
        for (int i = 0; i < t->fn.param_count; i++) {
            t->fn.param_types[i] = ty_instantiate(env, generic_type->fn.param_types[i]);
        }
        t->fn.return_type = ty_instantiate(env, generic_type->fn.return_type);
        return t;
    }

    return generic_type;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE INFERENCE FOR EXPRESSIONS
// ═══════════════════════════════════════════════════════════════════════════════

Type *ty_infer_expr(TyEnv *env, ASTNode *expr) {
    if (!expr) return NULL;

    switch (expr->kind) {
        case N_LITERAL_INT: {
            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_I64;
            expr->type = t;
            return t;
        }
        case N_LITERAL_FLOAT: {
            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_F64;
            expr->type = t;
            return t;
        }
        case N_LITERAL_BOOL: {
            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_BOOL;
            expr->type = t;
            return t;
        }
        case N_LITERAL_STR: {
            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_STR;
            expr->type = t;
            return t;
        }
        case N_LITERAL_CHAR: {
            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_CHAR;
            expr->type = t;
            return t;
        }
        case N_LITERAL_NULL: {
            // Null can be any pointer/option type
            Type *t = ty_fresh_var(env);
            expr->type = t;
            return t;
        }
        case N_IDENT: {
            // Look up in symbol table
            const char *name = expr->ident.name;
            if (!name) {
                expr->type = env->ctx->types.ty_error;
                return expr->type;
            }

            Symbol *sym = ket_sym_lookup(env->syms, name);
            if (!sym) {
                ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                    "cannot find '%s' in this scope", name);
                expr->type = env->ctx->types.ty_error;
                return env->ctx->types.ty_error;
            }

            expr->ident.resolved = sym->decl;

            // Instantiate generic if needed
            if (sym->type && sym->type->kind == TY_GENERIC) {
                expr->type = ty_instantiate(env, sym->type);
            } else if (sym->type) {
                expr->type = sym->type;
            } else {
                expr->type = ty_fresh_var(env);
            }

            return expr->type;
        }
        case N_BINARY: {
            Type *left = ty_infer_expr(env, expr->binary.bin_left);
            Type *right = ty_infer_expr(env, expr->binary.bin_right);

            KetTokenType op = expr->binary.bin_op;

            // Type-check based on operator
            switch (op) {
                case TK_PLUS: case TK_MINUS:
                case TK_STAR: case TK_SLASH: case TK_PERCENT:
                    // Arithmetic: both operands must be same numeric type
                    if (!ty_unify(env, left, right, expr->span.start)) {
                        expr->type = env->ctx->types.ty_error;
                        return expr->type;
                    }
                    expr->type = left;
                    return left;

                case TK_EQEQ: case TK_BANGEQ:
                case TK_LT: case TK_GT: case TK_LTEQ: case TK_GTEQ:
                    // Comparison: both operands must be same type, result is bool
                    if (!ty_unify(env, left, right, expr->span.start)) {
                        // Continue anyway for error recovery
                    }
                    expr->type = env->ctx->types.ty_bool;
                    return env->ctx->types.ty_bool;

                case TK_AMPAMP: case TK_PIPEPIPE:
                    // Logical: both operands must be bool
                    if (!ty_unify(env, left, env->ctx->types.ty_bool, expr->span.start)) {
                        ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                            "logical operator requires bool operands");
                    }
                    if (!ty_unify(env, right, env->ctx->types.ty_bool, expr->span.start)) {
                        ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                            "logical operator requires bool operands");
                    }
                    expr->type = env->ctx->types.ty_bool;
                    return env->ctx->types.ty_bool;

                case TK_AMPERSAND: case TK_PIPE: case TK_CARET:
                case TK_LTLT: case TK_GTGT:
                    expr->type = left;
                    return left;

                default:
                    expr->type = left;
                    return left;
            }
        }
        case N_UNARY: {
            Type *right = ty_infer_expr(env, expr->unary.unary_right);
            KetTokenType op = expr->unary.unary_op;

            switch (op) {
                case TK_MINUS:
                    // Negation: operand must be numeric
                    expr->type = right;
                    return right;
                case TK_EXCLAM:
                    // Not: operand must be bool
                    if (!ty_unify(env, right, env->ctx->types.ty_bool, expr->span.start)) {
                        ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                            "'!' requires bool operand");
                    }
                    expr->type = env->ctx->types.ty_bool;
                    return env->ctx->types.ty_bool;
                case TK_STAR:
                    // Dereference: right must be a pointer
                    expr->type = right ? right->ptr.pointee : env->ctx->types.ty_error;
                    if (!expr->type) {
                        expr->type = ty_fresh_var(env);
                    }
                    return expr->type;
                default:
                    expr->type = right;
                    return right;
            }
        }
        case N_CALL: {
            // Infer callee type
            Type *callee_type = ty_infer_expr(env, expr->call.call_callee);

            // Infer argument types
            for (int i = 0; i < expr->call.call_argc; i++) {
                ty_infer_expr(env, expr->call.call_args[i]);
            }

            // Callee should be a function type
            if (callee_type && callee_type->kind == TY_FN) {
                // Check parameter count
                if (callee_type->fn.param_count != expr->call.call_argc) {
                    ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                        "expected %d arguments, got %d",
                        callee_type->fn.param_count, expr->call.call_argc);
                    expr->type = env->ctx->types.ty_error;
                    return env->ctx->types.ty_error;
                }

                // Check each argument against parameter type
                for (int i = 0; i < expr->call.call_argc; i++) {
                    Type *param_type = callee_type->fn.param_types[i];
                    Type *arg_type = expr->call.call_args[i]->type;
                    if (param_type && arg_type) {
                        ty_unify(env, param_type, arg_type, expr->call.call_args[i]->span.start);
                    }
                }

                expr->type = callee_type->fn.return_type;
                return expr->type;
            }

            // If callee is a generic, return fresh var
            if (callee_type && callee_type->kind == TY_GENERIC) {
                expr->type = ty_fresh_var(env);
                return expr->type;
            }

            // Unknown callee type — return fresh var
            expr->type = ty_fresh_var(env);
            return expr->type;
        }
        case N_MEMBER: {
            Type *target = ty_infer_expr(env, expr->member.member_target);
            if (!target) {
                expr->type = ty_fresh_var(env);
                return expr->type;
            }

            // Resolve target to struct/enum
            target = ty_resolve(target);

            if (target->kind == TY_STRUCT && target->struct_type.fields) {
                const char *field_name = expr->member.member_name;
                for (int i = 0; i < target->struct_type.field_count; i++) {
                    if (target->struct_type.fields[i].name &&
                        strcmp(target->struct_type.fields[i].name, field_name) == 0) {
                        expr->type = target->struct_type.fields[i].type;
                        return expr->type;
                    }
                }

                ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                    "no field '%s' in struct %s",
                    field_name, target->struct_type.name ? target->struct_type.name : "?");
            } else if (target->kind == TY_ENUM && target->enum_type.variants) {
                const char *field_name = expr->member.member_name;
                for (int i = 0; i < target->enum_type.variant_count; i++) {
                    if (target->enum_type.variants[i].name &&
                        strcmp(target->enum_type.variants[i].name, field_name) == 0) {
                        // Return the variant's payload type
                        if (target->enum_type.variants[i].payload_count > 0) {
                            expr->type = target->enum_type.variants[i].payload_types[0];
                        } else {
                            expr->type = env->ctx->types.ty_void;
                        }
                        return expr->type;
                    }
                }
            }

            expr->type = ty_fresh_var(env);
            return expr->type;
        }
        case N_INDEX: {
            Type *target = ty_infer_expr(env, expr->index.index_target);
            Type *index = ty_infer_expr(env, expr->index.index_expr);

            // Index must be int
            if (index && index->kind != TY_I64 && index->kind != TY_I32 &&
                index->kind != TY_U64 && index->kind != TY_U32 &&
                index->kind != TY_INFER) {
                ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                    "array index must be an integer");
            }

            // Target should be array
            if (target && target->kind == TY_ARRAY) {
                expr->type = target->array.elem;
                return expr->type;
            }

            expr->type = ty_fresh_var(env);
            return expr->type;
        }
        case N_ASSIGN: {
            Type *val = ty_infer_expr(env, expr->assign.assign_value);

            // Check that target is assignable
            if (expr->assign.assign_target) {
                Type *target = ty_infer_expr(env, expr->assign.assign_target);

                // For simple variable assignment, unify types
                if (expr->assign.assign_target->kind == N_IDENT) {
                    Symbol *sym = ket_sym_lookup(env->syms,
                        expr->assign.assign_target->ident.name);
                    if (sym && !sym->is_mut) {
                        ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                            "cannot assign to immutable variable '%s'",
                            sym->name);
                    }
                }

                ty_unify(env, target, val, expr->span.start);
            }

            expr->type = val;
            return val;
        }
        case N_IF_EXPR:
        case N_IF: {
            // Condition must be bool
            Type *cond = ty_infer_expr(env, expr->if_node.if_cond);
            if (cond) {
                ty_unify(env, cond, env->ctx->types.ty_bool, expr->span.start);
            }

            Type *then_type = NULL;
            Type *else_type = NULL;

            if (expr->if_node.if_then) {
                if (expr->if_node.if_then->kind == N_BLOCK) {
                    ty_check_stmt(env, expr->if_node.if_then);
                    // Return type is type of last expression
                    if (expr->if_node.if_then->block.stmt_count > 0) {
                        ASTNode *last = expr->if_node.if_then->stmts[
                            expr->if_node.if_then->block.stmt_count - 1];
                        then_type = last->type;
                    }
                } else {
                    then_type = ty_infer_expr(env, expr->if_node.if_then);
                }
            }

            if (expr->if_node.if_else) {
                if (expr->if_node.if_else->kind == N_BLOCK) {
                    ty_check_stmt(env, expr->if_node.if_else);
                    if (expr->if_node.if_else->block.stmt_count > 0) {
                        ASTNode *last = expr->if_node.if_else->stmts[
                            expr->if_node.if_else->block.stmt_count - 1];
                        else_type = last->type;
                    }
                } else {
                    else_type = ty_infer_expr(env, expr->if_node.if_else);
                }
            }

            // Both branches must have same type
            if (then_type && else_type) {
                ty_unify(env, then_type, else_type, expr->span.start);
                expr->type = then_type;
            } else if (then_type) {
                expr->type = then_type;
            } else {
                expr->type = env->ctx->types.ty_void;
            }

            return expr->type;
        }
        case N_BLOCK: {
            // Type of block is type of last expression
            expr->type = env->ctx->types.ty_void;
            for (int i = 0; i < expr->block.stmt_count; i++) {
                ty_check_stmt(env, expr->block.stmts[i]);
            }
            if (expr->block.stmt_count > 0) {
                ASTNode *last = expr->block.stmts[expr->block.stmt_count - 1];
                if (last->type && last->type->kind != TY_VOID) {
                    expr->type = last->type;
                }
            }
            return expr->type;
        }
        case N_TUPLE: {
            if (expr->tuple.tuple_count == 0) {
                expr->type = env->ctx->types.ty_void;
                return expr->type;
            }

            Type **elems = calloc(expr->tuple.tuple_count, sizeof(Type*));
            for (int i = 0; i < expr->tuple.tuple_count; i++) {
                elems[i] = ty_infer_expr(env, expr->tuple.tuple_elems[i]);
            }

            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_TUPLE;
            t->tuple.elems = elems;
            t->tuple.elem_count = expr->tuple.tuple_count;
            expr->type = t;
            return t;
        }
        case N_ARRAY_INIT: {
            if (expr->init.init_argc == 0) {
                expr->type = calloc(1, sizeof(Type));
                expr->type->kind = TY_ARRAY;
                expr->type->array.elem = ty_fresh_var(env);
                expr->type->array.count = 0;
                return expr->type;
            }

            Type *elem_type = ty_infer_expr(env, expr->init.init_args[0]);
            for (int i = 1; i < expr->init.init_argc; i++) {
                Type *arg_type = ty_infer_expr(env, expr->init.init_args[i]);
                ty_unify(env, elem_type, arg_type, expr->init.init_args[i]->span.start);
            }

            Type *t = calloc(1, sizeof(Type));
            t->kind = TY_ARRAY;
            t->array.elem = elem_type;
            t->array.count = expr->init.init_argc;
            expr->type = t;
            return t;
        }
        case N_RETURN: {
            if (expr->ret_val) {
                Type *val_type = ty_infer_expr(env, expr->ret_val);
                if (env->ret_type) {
                    ty_unify(env, val_type, env->ret_type, expr->span.start);
                }
            }
            expr->type = env->ctx->types.ty_never;
            return expr->type;
        }
        case N_BREAK:
        case N_CONTINUE:
            if (!env->in_loop) {
                ket_diag_push(env->ctx, SEV_ERROR, expr->span.start, 0,
                    "break/continue outside of loop");
            }
            expr->type = env->ctx->types.ty_never;
            return expr->type;

        case N_CAST: {
            expr->type = expr->cast.cast_type;
            return expr->type;
        }
        case N_STRUCT_INIT:
        case N_ENUM_INIT: {
            // For struct init, type is the struct type
            if (expr->init.init_type && expr->init.init_type->type) {
                expr->type = expr->init.init_type->type;
            } else {
                expr->type = ty_fresh_var(env);
            }
            return expr->type;
        }
        default:
            expr->type = ty_fresh_var(env);
            return expr->type;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE CHECKING FOR STATEMENTS
// ═══════════════════════════════════════════════════════════════════════════════

void ty_check_stmt(TyEnv *env, ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case N_EXPR_STMT: {
            if (stmt->ret_val) {
                ty_infer_expr(env, stmt->ret_val);
            }
            stmt->type = env->ctx->types.ty_void;
            break;
        }
        case N_VAR_DECL: {
            const char *name = stmt->var_decl.var_name;
            Type *decl_type = stmt->var_decl.var_type;

            Type *init_type = NULL;
            if (stmt->var_decl.var_init) {
                init_type = ty_infer_expr(env, stmt->var_decl.var_init);
            }

            // If explicit type annotation and init, check compatibility
            if (decl_type && init_type) {
                ty_unify(env, decl_type, init_type, stmt->span.start);
            }

            // Determine final type
            Type *final_type = decl_type ? decl_type : (init_type ? init_type : ty_fresh_var(env));

            // Insert into symbol table
            Symbol *existing = ket_sym_lookup_current(env->syms, name);
            if (existing) {
                ket_diag_push(env->ctx, SEV_ERROR, stmt->span.start, 0,
                    "variable '%s' already declared", name);
            } else {
                ket_sym_insert(env->syms, SYM_VARIABLE, name, final_type, stmt, stmt->span.start);
            }

            stmt->type = final_type;
            break;
        }
        case N_IF: {
            Type *cond = ty_infer_expr(env, stmt->if_node.if_cond);
            if (cond) {
                ty_unify(env, cond, env->ctx->types.ty_bool, stmt->span.start);
            }

            // Create new scope
            SymTable *then_syms = ket_sym_table_new(env->syms, env->ctx->arena);
            TyEnv *then_env = ty_env_new(env, then_syms, env->ctx);
            then_env->ret_type = env->ret_type;
            then_env->in_loop = env->in_loop;

            ty_check_stmt(then_env, stmt->if_node.if_then);

            if (stmt->if_node.if_else) {
                SymTable *else_syms = ket_sym_table_new(env->syms, env->ctx->arena);
                TyEnv *else_env = ty_env_new(env, else_syms, env->ctx);
                else_env->ret_type = env->ret_type;
                else_env->in_loop = env->in_loop;
                ty_check_stmt(else_env, stmt->if_node.if_else);
            }

            stmt->type = env->ctx->types.ty_void;
            break;
        }
        case N_WHILE:
        case N_LOOP: {
            if (stmt->loop.loop_cond) {
                Type *cond = ty_infer_expr(env, stmt->loop.loop_cond);
                if (cond) {
                    ty_unify(env, cond, env->ctx->types.ty_bool, stmt->span.start);
                }
            }

            SymTable *loop_syms = ket_sym_table_new(env->syms, env->ctx->arena);
            TyEnv *loop_env = ty_env_new(env, loop_syms, env->ctx);
            loop_env->ret_type = env->ret_type;
            loop_env->in_loop = true;

            ty_check_stmt(loop_env, stmt->loop.loop_body);

            stmt->type = env->ctx->types.ty_void;
            break;
        }
        case N_FOR: {
            // for var in iterable { body }
            Type *iter_type = ty_infer_expr(env, stmt->loop.loop_iter);

            // Loop variable type is the element type of the iterable
            Type *elem_type = ty_fresh_var(env);
            if (iter_type && iter_type->kind == TY_ARRAY) {
                elem_type = iter_type->array.elem;
            }

            SymTable *for_syms = ket_sym_table_new(env->syms, env->ctx->arena);

            // Insert loop variable
            const char *var_name = stmt->loop.loop_var;
            if (var_name) {
                ket_sym_insert(for_syms, SYM_VARIABLE, var_name, elem_type, stmt, stmt->span.start);
            }

            TyEnv *for_env = ty_env_new(env, for_syms, env->ctx);
            for_env->ret_type = env->ret_type;
            for_env->in_loop = true;

            ty_check_stmt(for_env, stmt->loop.loop_body);

            stmt->type = env->ctx->types.ty_void;
            break;
        }
        case N_BLOCK: {
            SymTable *block_syms = ket_sym_table_new(env->syms, env->ctx->arena);
            TyEnv *block_env = ty_env_new(env, block_syms, env->ctx);
            block_env->ret_type = env->ret_type;
            block_env->in_loop = env->in_loop;

            for (int i = 0; i < stmt->block.stmt_count; i++) {
                ty_check_stmt(block_env, stmt->block.stmts[i]);
            }

            if (stmt->block.stmt_count > 0) {
                stmt->type = stmt->block.stmts[stmt->block.stmt_count - 1]->type;
            } else {
                stmt->type = env->ctx->types.ty_void;
            }
            break;
        }
        case N_RETURN: {
            if (stmt->ret_val) {
                Type *val_type = ty_infer_expr(env, stmt->ret_val);
                if (env->ret_type) {
                    ty_unify(env, val_type, env->ret_type, stmt->span.start);
                }
            } else {
                if (env->ret_type && env->ret_type->kind != TY_VOID) {
                    ket_diag_push(env->ctx, SEV_ERROR, stmt->span.start, 0,
                        "expected return value of type ");
                    ty_print_type(env->ret_type);
                }
            }
            stmt->type = env->ctx->types.ty_never;
            break;
        }
        case N_ASSIGN: {
            ty_infer_expr(env, stmt);
            break;
        }
        case N_MATCH: {
            Type *match_type = ty_infer_expr(env, stmt->match.match_expr);

            for (int i = 0; i < stmt->match.match_arm_count; i++) {
                ASTNode *arm = stmt->match.match_arms[i];
                // Check arm pattern against match type
                ty_check_stmt(env, arm->arm.arm_body);
            }

            stmt->type = env->ctx->types.ty_void;
            break;
        }
        default: {
            // Try as expression
            Type *t = ty_infer_expr(env, stmt);
            if (t) stmt->type = t;
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTION CHECKING
// ═══════════════════════════════════════════════════════════════════════════════

Type *ty_check_fn(TyEnv *env, ASTNode *fn) {
    if (!fn || fn->kind != N_FN_DECL) return NULL;

    const char *fn_name = fn->fn_decl.fn_name ?
        (fn->fn_decl.fn_namelen > 0 ? fn->fn_decl.fn_name : "<anon>") : "<anon>";

    // Create function type
    int param_count = fn->fn_decl.fn_param_count;
    Type **param_types = calloc(param_count + 1, sizeof(Type*));

    for (int i = 0; i < param_count; i++) {
        ASTNode *param = fn->fn_decl.fn_params[i];
        if (param && param->kind == N_VAR_DECL) {
            // Use explicit type or fresh variable
            if (param->var_decl.var_type) {
                param_types[i] = param->var_decl.var_type;
            } else {
                param_types[i] = ty_fresh_var(env);
            }
            param->type = param_types[i];
        }
    }

    Type *ret_type = fn->fn_decl.fn_ret_type;
    if (!ret_type) {
        ret_type = env->ctx->types.ty_void;
    }

    Type *fn_type = calloc(1, sizeof(Type));
    fn_type->kind = TY_FN;
    fn_type->fn.param_types = param_types;
    fn_type->fn.param_count = param_count;
    fn_type->fn.return_type = ret_type;

    // Create new scope with parameters
    SymTable *fn_syms = ket_sym_table_new(env->syms, env->ctx->arena);

    for (int i = 0; i < param_count; i++) {
        ASTNode *param = fn->fn_decl.fn_params[i];
        if (param && param->kind == N_VAR_DECL && param->var_decl.var_name) {
            ket_sym_insert(fn_syms, SYM_VARIABLE,
                param->var_decl.var_name, param_types[i], param, param->span.start);
        }
    }

    // Create function environment
    TyEnv *fn_env = ty_env_new(env, fn_syms, env->ctx);
    fn_env->ret_type = ret_type;

    // Check body
    if (fn->fn_decl.fn_body) {
        ty_check_stmt(fn_env, fn->fn_decl.fn_body);
    }

    fn->type = fn_type;

    // Insert into symbol table
    if (fn_name && fn_name[0] != '<') {
        Symbol *existing = ket_sym_lookup_current(env->syms, fn_name);
        if (existing) {
            ket_diag_push(env->ctx, SEV_ERROR, fn->span.start, 0,
                "function '%s' already defined", fn_name);
        } else {
            ket_sym_insert(env->syms, SYM_FUNCTION, fn_name, fn_type, fn, fn->span.start);
        }
    }

    return fn_type;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DECLARATION CHECKING
// ═══════════════════════════════════════════════════════════════════════════════

void ty_check_decl(TyEnv *env, ASTNode *decl) {
    if (!decl) return;

    switch (decl->kind) {
        case N_FN_DECL:
            ty_check_fn(env, decl);
            break;
        case N_STRUCT_DECL: {
            const char *name = decl->decl.decl_name;
            // Register the struct type
            Field *fields = NULL;
            if (decl->decl.decl_field_count > 0) {
                fields = calloc(decl->decl.decl_field_count, sizeof(Field));
                for (int i = 0; i < decl->decl.decl_field_count; i++) {
                    ASTNode *field = decl->decl.decl_fields[i];
                    if (field && field->kind == N_VAR_DECL) {
                        fields[i].name = field->var_decl.var_name;
                        fields[i].type = field->var_decl.var_type ? field->var_decl.var_type : ty_fresh_var(env);
                        fields[i].loc = field->span.start;
                    }
                }
            }

            Type *struct_type = ket_type_struct(&env->ctx->types, name, fields, decl->decl.decl_field_count);
            decl->type = struct_type;

            ket_sym_insert(env->syms, SYM_STRUCT, name, struct_type, decl, decl->span.start);
            break;
        }
        case N_ENUM_DECL: {
            const char *name = decl->decl.decl_name;
            Variant *variants = NULL;
            if (decl->decl.decl_field_count > 0) {
                variants = calloc(decl->decl.decl_field_count, sizeof(Variant));
                for (int i = 0; i < decl->decl.decl_field_count; i++) {
                    ASTNode *variant = decl->decl.decl_fields[i];
                    if (variant) {
                        variants[i].name = variant->decl.decl_name;
                        variants[i].discriminant = i;
                        variants[i].loc = variant->span.start;
                        // Payload types
                        if (variant->decl.decl_field_count > 0) {
                            variants[i].payload_types = calloc(variant->decl.decl_field_count, sizeof(Type*));
                            variants[i].payload_count = variant->decl.decl_field_count;
                            for (int j = 0; j < variant->decl.decl_field_count; j++) {
                                if (variant->decl.decl_fields[j] && variant->decl.decl_fields[j]->type) {
                                    variants[i].payload_types[j] = variant->decl.decl_fields[j]->type;
                                } else {
                                    variants[i].payload_types[j] = ty_fresh_var(env);
                                }
                            }
                        }
                    }
                }
            }

            Type *enum_type = ket_type_enum(&env->ctx->types, name, variants, decl->decl.decl_field_count);
            decl->type = enum_type;

            ket_sym_insert(env->syms, SYM_ENUM, name, enum_type, decl, decl->span.start);
            break;
        }
        case N_IMPORT: {
            // Import handling — for now, just register as module symbol
            if (decl->path.seg_count > 0) {
                ASTNode *last = decl->path.segments[decl->path.seg_count - 1];
                if (last && last->kind == N_IDENT && last->ident.name) {
                    if (!decl->import.is_wildcard) {
                        ket_sym_insert(env->syms, SYM_MODULE, last->ident.name,
                            env->ctx->types.ty_void, decl, decl->span.start);
                    }
                }
            }
            break;
        }
        case N_IMPL_BLOCK: {
            // Check each method in the impl block
            for (int i = 0; i < decl->impl_block.impl_method_count; i++) {
                ty_check_fn(env, decl->impl_block.impl_methods[i]);
            }
            break;
        }
        case N_TRAIT_DECL: {
            const char *name = decl->decl.decl_name;
            ket_sym_insert(env->syms, SYM_TRAIT, name, env->ctx->types.ty_void, decl, decl->span.start);
            break;
        }
        case N_TYPE_ALIAS: {
            const char *name = decl->decl.decl_name;
            ket_sym_insert(env->syms, SYM_TYPE_ALIAS, name, decl->type_node.type_expr, decl, decl->span.start);
            break;
        }
        default: {
            // Treat as statement
            ty_check_stmt(env, decl);
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE CHECKING
// ═══════════════════════════════════════════════════════════════════════════════

bool ty_check_module(Context *ctx, ASTNode *program) {
    if (!program || program->kind != N_MODULE) return false;

    // Create global environment
    SymTable *global_syms = ket_sym_table_new(NULL, ctx->arena);
    TyEnv *global_env = ty_env_new(NULL, global_syms, ctx);

    // Register built-in types
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "int", ctx->types.ty_i64, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "float", ctx->types.ty_f64, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "bool", ctx->types.ty_bool, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "str", ctx->types.ty_str, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "void", ctx->types.ty_void, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "byte", ctx->types.ty_byte, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "char", ctx->types.ty_char, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "i8", ctx->types.ty_i8, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "i16", ctx->types.ty_i16, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "i32", ctx->types.ty_i32, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "i64", ctx->types.ty_i64, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "u8", ctx->types.ty_u8, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "u16", ctx->types.ty_u16, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "u32", ctx->types.ty_u32, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "u64", ctx->types.ty_u64, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "f32", ctx->types.ty_f32, NULL, LOC_NONE);
    ket_sym_insert(global_syms, SYM_TYPE_ALIAS, "f64", ctx->types.ty_f64, NULL, LOC_NONE);

    // Register built-in functions
    ket_sym_insert(global_syms, SYM_FUNCTION, "print",
        &(Type){.kind=TY_FN,.fn.param_count=1,.fn.param_types=(Type*[]){ctx->types.ty_str},
                .fn.return_type=ctx->types.ty_void},
        NULL, LOC_NONE);

    // Check all top-level declarations in order
    for (int i = 0; i < program->block.stmt_count; i++) {
        ty_check_decl(global_env, program->block.stmts[i]);

        // Report progress
        if (ctx->options.verbose && ctx->options.check_only) {
            ASTNode *node = program->block.stmts[i];
            if (node->kind == N_FN_DECL && node->fn_decl.fn_name) {
                fprintf(stderr, "  typechecked: fn %.*s\n",
                    node->fn_decl.fn_namelen, node->fn_decl.fn_name);
            }
        }
    }

    // Solve constraints
    ty_solve_constraints(global_env);

    // Check for errors
    if (ket_diag_has_errors(ctx)) {
        return false;
    }

    if (ctx->options.verbose || ctx->options.dump_type) {
        fprintf(stderr, "\n─── Type Checker Summary ───\n");
        fprintf(stderr, "  Variables: %d\n", global_env->unify->var_count);
        fprintf(stderr, "  Constraints: %d\n", global_env->constraint_count);
    }

    return true;
}
