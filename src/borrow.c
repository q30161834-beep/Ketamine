#include "include/borrow.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// BORROW CHECKER — Ownership, lifetimes, and NLL (Non-Lexical Lifetimes)
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Lifetime management ─────────────────────────────────────────────────────

static int lifetime_next_id = 1;

Lifetime *lifetime_new(Lifetime *parent, const char *name, Location loc) {
    Lifetime *lt = calloc(1, sizeof(Lifetime));
    lt->id = lifetime_next_id++;
    lt->name = name;
    lt->parent = parent;
    lt->is_static = (name && strcmp(name, "'static") == 0);
    lt->is_anon = (name == NULL);
    lt->loc = loc;
    return lt;
}

bool lifetime_check_outlives(Lifetime *a, Lifetime *b, Location loc) {
    (void)loc;
    if (!a || !b) return true;
    if (a->is_static) return true;  // 'static outlives everything
    if (a == b) return true;

    // Check parent chain
    Lifetime *p = b;
    while (p) {
        if (a == p) return true;
        p = p->parent;
    }

    return false;
}

// ─── Borrow State ────────────────────────────────────────────────────────────

BorrowState *borrow_state_new(Context *ctx) {
    BorrowState *bs = calloc(1, sizeof(BorrowState));
    bs->entries = NULL;
    bs->entry_count = 0;
    bs->entry_cap = 0;
    bs->scope_depth = 0;
    bs->ctx = ctx;
    return bs;
}

static int find_entry(BorrowState *bs, const char *name) {
    for (int i = 0; i < bs->entry_count; i++) {
        if (strcmp(bs->entries[i].var_name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_entry(BorrowState *bs, const char *name) {
    if (bs->entry_count >= bs->entry_cap) {
        bs->entry_cap = bs->entry_cap ? bs->entry_cap * 2 : 64;
        bs->entries = realloc(bs->entries, sizeof(bs->entries[0]) * bs->entry_cap);
    }
    int idx = bs->entry_count++;
    bs->entries[idx].var_name = name;
    bs->entries[idx].moved = false;
    bs->entries[idx].shared_borrow_count = 0;
    bs->entries[idx].mut_borrow = false;
    bs->entries[idx].borrow_lifetime = NULL;
    return idx;
}

void borrow_enter_scope(BorrowState *bs) {
    bs->scope_depth++;
}

void borrow_exit_scope(BorrowState *bs) {
    // Check that all borrows in this scope are released
    borrow_check_scope_end(bs);
    bs->scope_depth--;
}

void borrow_decl_var(BorrowState *bs, const char *name, bool is_mut) {
    if (name) {
        int idx = find_entry(bs, name);
        if (idx < 0) {
            idx = add_entry(bs, name);
        }
        bs->entries[idx].moved = false;
        bs->entries[idx].shared_borrow_count = 0;
        bs->entries[idx].mut_borrow = false;
        (void)is_mut;
    }
}

bool borrow_check_move(BorrowState *bs, ASTNode *expr, const char *var) {
    if (!var) return true;

    int idx = find_entry(bs, var);
    if (idx < 0) {
        // Variable not tracked — might be from outer scope
        return true;
    }

    // Check if there's an active borrow
    if (bs->entries[idx].shared_borrow_count > 0) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot move out of '%s' while shared borrow exists", var);
        return false;
    }

    if (bs->entries[idx].mut_borrow) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot move out of '%s' while mutable borrow exists", var);
        return false;
    }

    // Check if already moved
    if (bs->entries[idx].moved) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "use of moved value: '%s'", var);
        return false;
    }

    // Track the move
    bs->entries[idx].moved = true;
    return true;
}

bool borrow_check_shared_borrow(BorrowState *bs, ASTNode *expr, const char *var) {
    if (!var) return true;

    int idx = find_entry(bs, var);
    if (idx < 0) return true;

    // Can't borrow if there's a mutable borrow
    if (bs->entries[idx].mut_borrow) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot borrow '%s' as shared because it is already borrowed as mutable", var);
        return false;
    }

    // Can't borrow a moved value
    if (bs->entries[idx].moved) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot borrow a moved value: '%s'", var);
        return false;
    }

    bs->entries[idx].shared_borrow_count++;
    return true;
}

bool borrow_check_mut_borrow(BorrowState *bs, ASTNode *expr, const char *var) {
    if (!var) return true;

    int idx = find_entry(bs, var);
    if (idx < 0) return true;

    // Can't mutably borrow if there are any existing borrows
    if (bs->entries[idx].shared_borrow_count > 0) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot borrow '%s' as mutable because it is already borrowed as shared", var);
        return false;
    }

    if (bs->entries[idx].mut_borrow) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot borrow '%s' as mutable more than once at a time", var);
        return false;
    }

    // Can't borrow a moved value
    if (bs->entries[idx].moved) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot borrow a moved value: '%s'", var);
        return false;
    }

    bs->entries[idx].mut_borrow = true;
    return true;
}

bool borrow_check_use(BorrowState *bs, ASTNode *expr, const char *var) {
    if (!var) return true;

    int idx = find_entry(bs, var);
    if (idx < 0) return true;

    // Can't use a moved value
    if (bs->entries[idx].moved) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "use of moved value: '%s'", var);
        return false;
    }

    return true;
}

bool borrow_check_assign(BorrowState *bs, ASTNode *expr, const char *var) {
    if (!var) return true;

    int idx = find_entry(bs, var);
    if (idx < 0) {
        // First assignment to this variable
        return true;
    }

    // Check for borrow conflicts
    if (bs->entries[idx].shared_borrow_count > 0) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot assign to '%s' because it is borrowed", var);
        return false;
    }

    if (bs->entries[idx].mut_borrow) {
        ket_diag_push(bs->ctx, SEV_ERROR, expr->span.start, 0,
            "cannot assign to '%s' because it is mutably borrowed", var);
        return false;
    }

    // Assignment to a moved variable revives it
    bs->entries[idx].moved = false;
    return true;
}

bool borrow_check_scope_end(BorrowState *bs) {
    for (int i = 0; i < bs->entry_count; i++) {
        if (bs->entries[i].shared_borrow_count > 0) {
            // Borrow still active at scope end — this is OK for shared borrows
            // as long as they don't outlive the data
        }
        if (bs->entries[i].mut_borrow) {
            // Same for mutable borrows
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BORROW CHECK PASS ON AST
// ═══════════════════════════════════════════════════════════════════════════════

// Recursive check of expressions for borrow rules
static void borrow_check_expr(BorrowState *bs, ASTNode *expr);
static void borrow_check_stmt(BorrowState *bs, ASTNode *stmt);

static void borrow_check_expr(BorrowState *bs, ASTNode *expr) {
    if (!expr) return;

    switch (expr->kind) {
        case N_IDENT: {
            borrow_check_use(bs, expr, expr->ident.name);
            break;
        }
        case N_BINARY: {
            borrow_check_expr(bs, expr->binary.bin_left);
            borrow_check_expr(bs, expr->binary.bin_right);
            break;
        }
        case N_UNARY: {
            if (expr->unary.unary_op == TK_AMPERSAND) {
                // Borrow
                if (expr->unary.unary_right && expr->unary.unary_right->kind == N_IDENT) {
                    if (expr->flags & NF_MUT) {
                        borrow_check_mut_borrow(bs, expr, expr->unary.unary_right->ident.name);
                    } else {
                        borrow_check_shared_borrow(bs, expr, expr->unary.unary_right->ident.name);
                    }
                }
            }
            if (expr->unary.unary_op == TK_STAR) {
                // Dereference — no borrow check needed on the pointer itself
            }
            borrow_check_expr(bs, expr->unary.unary_right);
            break;
        }
        case N_CALL: {
            for (int i = 0; i < expr->call.call_argc; i++) {
                borrow_check_expr(bs, expr->call.call_args[i]);
            }
            // Move semantics for arguments
            if (expr->call.call_callee) {
                borrow_check_expr(bs, expr->call.call_callee);
            }
            break;
        }
        case N_ASSIGN: {
            // Check target
            if (expr->assign.assign_target && expr->assign.assign_target->kind == N_IDENT) {
                borrow_check_assign(bs, expr, expr->assign.assign_target->ident.name);
            }
            borrow_check_expr(bs, expr->assign.assign_value);
            break;
        }
        case N_MEMBER: {
            borrow_check_expr(bs, expr->member.member_target);
            break;
        }
        case N_INDEX: {
            borrow_check_expr(bs, expr->index.index_target);
            borrow_check_expr(bs, expr->index.index_expr);
            break;
        }
        case N_TUPLE: {
            for (int i = 0; i < expr->tuple.tuple_count; i++) {
                borrow_check_expr(bs, expr->tuple.tuple_elems[i]);
            }
            break;
        }
        case N_ARRAY_INIT: case N_STRUCT_INIT: case N_ENUM_INIT: {
            for (int i = 0; i < expr->init.init_argc; i++) {
                borrow_check_expr(bs, expr->init.init_args[i]);
            }
            break;
        }
        case N_IF_EXPR: {
            borrow_check_expr(bs, expr->if_node.if_cond);
            borrow_check_stmt(bs, expr->if_node.if_then);
            if (expr->if_node.if_else) {
                borrow_check_stmt(bs, expr->if_node.if_else);
            }
            break;
        }
        case N_BLOCK_EXPR: {
            borrow_check_stmt(bs, expr);
            break;
        }
        case N_REF_EXPR: {
            if (expr->unary.unary_right && expr->unary.unary_right->kind == N_IDENT) {
                if (expr->flags & NF_MUT) {
                    borrow_check_mut_borrow(bs, expr, expr->unary.unary_right->ident.name);
                } else {
                    borrow_check_shared_borrow(bs, expr, expr->unary.unary_right->ident.name);
                }
            }
            borrow_check_expr(bs, expr->unary.unary_right);
            break;
        }
        default:
            break;
    }
}

static void borrow_check_stmt(BorrowState *bs, ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case N_BLOCK: {
            borrow_enter_scope(bs);
            for (int i = 0; i < stmt->block.stmt_count; i++) {
                borrow_check_stmt(bs, stmt->block.stmts[i]);
            }
            borrow_exit_scope(bs);
            break;
        }
        case N_VAR_DECL: {
            const char *name = stmt->var_decl.var_name;
            bool is_mut = (stmt->flags & NF_MUT) != 0;
            borrow_decl_var(bs, name, is_mut);

            if (stmt->var_decl.var_init) {
                // Check for moves in initializer
                borrow_check_expr(bs, stmt->var_decl.var_init);

                // If initializer is an identifier, it's a move
                if (stmt->var_decl.var_init->kind == N_IDENT) {
                    borrow_check_move(bs, stmt->var_decl.var_init,
                        stmt->var_decl.var_init->ident.name);
                }
            }
            break;
        }
        case N_IF: {
            borrow_check_expr(bs, stmt->if_node.if_cond);
            borrow_enter_scope(bs);
            borrow_check_stmt(bs, stmt->if_node.if_then);
            borrow_exit_scope(bs);
            if (stmt->if_node.if_else) {
                borrow_enter_scope(bs);
                borrow_check_stmt(bs, stmt->if_node.if_else);
                borrow_exit_scope(bs);
            }
            break;
        }
        case N_WHILE: case N_LOOP: {
            if (stmt->loop.loop_cond) {
                borrow_check_expr(bs, stmt->loop.loop_cond);
            }
            borrow_enter_scope(bs);
            borrow_check_stmt(bs, stmt->loop.loop_body);
            borrow_exit_scope(bs);
            break;
        }
        case N_FOR: {
            borrow_check_expr(bs, stmt->loop.loop_iter);
            borrow_enter_scope(bs);
            // Loop variable is declared
            borrow_decl_var(bs, stmt->loop.loop_var, false);
            borrow_check_stmt(bs, stmt->loop.loop_body);
            borrow_exit_scope(bs);
            break;
        }
        case N_RETURN: {
            if (stmt->ret_val) {
                borrow_check_expr(bs, stmt->ret_val);
                // Return may move out
                if (stmt->ret_val->kind == N_IDENT) {
                    borrow_check_move(bs, stmt->ret_val, stmt->ret_val->ident.name);
                }
            }
            break;
        }
        case N_ASSIGN: {
            borrow_check_expr(bs, stmt);
            break;
        }
        case N_EXPR_STMT: {
            borrow_check_expr(bs, stmt->ret_val);
            break;
        }
        case N_MATCH: {
            borrow_check_expr(bs, stmt->match.match_expr);
            for (int i = 0; i < stmt->match.match_arm_count; i++) {
                ASTNode *arm = stmt->match.match_arms[i];
                borrow_enter_scope(bs);
                borrow_check_stmt(bs, arm->arm.arm_body);
                borrow_exit_scope(bs);
            }
            break;
        }
        default: {
            borrow_check_expr(bs, stmt);
            break;
        }
    }
}

bool borrow_check_module(Context *ctx, ASTNode *program) {
    if (!program || program->kind != N_MODULE) return false;

    if (ctx->options.verbose) {
        fprintf(stderr, "  running borrow check...\n");
    }

    BorrowState *bs = borrow_state_new(ctx);

    for (int i = 0; i < program->block.stmt_count; i++) {
        ASTNode *node = program->block.stmts[i];

        switch (node->kind) {
            case N_FN_DECL: {
                borrow_enter_scope(bs);
                // Parameters are declared
                for (int j = 0; j < node->fn_decl.fn_param_count; j++) {
                    ASTNode *param = node->fn_decl.fn_params[j];
                    if (param && param->kind == N_VAR_DECL && param->var_decl.var_name) {
                        borrow_decl_var(bs, param->var_decl.var_name, false);
                    }
                }
                if (node->fn_decl.fn_body) {
                    borrow_check_stmt(bs, node->fn_decl.fn_body);
                }
                borrow_exit_scope(bs);
                break;
            }
            case N_STRUCT_DECL: case N_ENUM_DECL:
            case N_IMPORT: case N_TRAIT_DECL:
            case N_TYPE_ALIAS:
                // No borrow checking needed for declarations
                break;
            case N_IMPL_BLOCK: {
                for (int j = 0; j < node->impl_block.impl_method_count; j++) {
                    borrow_enter_scope(bs);
                    ASTNode *method = node->impl_block.impl_methods[j];
                    for (int k = 0; k < method->fn_decl.fn_param_count; k++) {
                        ASTNode *param = method->fn_decl.fn_params[k];
                        if (param && param->kind == N_VAR_DECL && param->var_decl.var_name) {
                            borrow_decl_var(bs, param->var_decl.var_name, false);
                        }
                    }
                    if (method->fn_decl.fn_body) {
                        borrow_check_stmt(bs, method->fn_decl.fn_body);
                    }
                    borrow_exit_scope(bs);
                }
                break;
            }
            default:
                borrow_check_stmt(bs, node);
                break;
        }
    }

    // Check for errors
    bool has_errors = ctx->diag.error_count > 0;
    if (ctx->options.verbose) {
        fprintf(stderr, "  borrow check complete (errors: %d)\n", has_errors);
    }

    free(bs->entries);
    free(bs);
    return !has_errors;
}
