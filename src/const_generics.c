#include "include/const_generics.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

ConstGenericValue const_eval_expr(ASTNode *expr, Context *ctx) {
    ConstGenericValue val = {0};
    val.is_known = false;

    if (!expr) return val;

    switch (expr->kind) {
        case N_LITERAL_INT:
            val.kind = CONST_INT;
            val.int_val = expr->ival;
            val.is_known = true;
            break;

        case N_LITERAL_FLOAT:
            val.kind = CONST_FLOAT;
            val.float_val = expr->fval;
            val.is_known = true;
            break;

        case N_LITERAL_BOOL:
            val.kind = CONST_BOOL;
            val.bool_val = expr->bval;
            val.is_known = true;
            break;

        case N_BINARY: {
            ConstGenericValue l = const_eval_expr(expr->binary.bin_left, ctx);
            ConstGenericValue r = const_eval_expr(expr->binary.bin_right, ctx);
            if (!l.is_known || !r.is_known) break;

            if (l.kind == CONST_INT && r.kind == CONST_INT) {
                val.kind = CONST_INT;
                switch (expr->binary.bin_op) {
                    case TK_PLUS:  val.int_val = l.int_val + r.int_val; break;
                    case TK_MINUS: val.int_val = l.int_val - r.int_val; break;
                    case TK_STAR:  val.int_val = l.int_val * r.int_val; break;
                    case TK_SLASH: if (r.int_val) val.int_val = l.int_val / r.int_val; break;
                    default: break;
                }
                val.is_known = true;
            }
            break;
        }

        default:
            break;
    }

    return val;
}

bool const_type_has_const_generics(struct Type *ty) {
    (void)ty;
    return false;
}

struct Type *const_substitute_type(struct Type *ty, ConstGenericParam *params,
                                   int param_count, TypeTable *tt) {
    (void)params;
    (void)param_count;
    (void)tt;
    return ty;
}

int64_t const_eval_int(ASTNode *expr, Context *ctx) {
    ConstGenericValue val = const_eval_expr(expr, ctx);
    if (val.is_known && val.kind == CONST_INT) return val.int_val;
    return 0;
}

bool const_check_where(ASTNode *where, ConstGenericValue val, Context *ctx) {
    (void)where;
    (void)val;
    (void)ctx;
    return true;
}
