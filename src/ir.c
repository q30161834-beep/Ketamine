#include "include/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// IR — Intermediate Representation (SSA form with basic blocks)
// ═══════════════════════════════════════════════════════════════════════════════

// ─── IR builder ───────────────────────────────────────────────────────────────

IRModule *ket_ir_new_module(Context *ctx) {
    IRModule *mod = (IRModule*)arena_alloc_zero(ctx->arena, sizeof(IRModule));
    mod->functions = NULL;
    mod->func_count = 0;
    mod->types = NULL;
    mod->type_count = 0;
    mod->globals = NULL;
    mod->global_count = 0;
    return mod;
}

IRFunction *ket_ir_new_function(IRModule *mod, const char *name, Type *type) {
    IRFunction *fn = (IRFunction*)calloc(1, sizeof(IRFunction));
    fn->name = name;
    fn->type = type;
    fn->blocks = NULL;
    fn->block_count = 0;
    fn->entry = NULL;
    fn->reg_count = 0;
    fn->var_arg_count = 0;
    fn->is_extern = false;

    // Add to module
    mod->func_count++;
    mod->functions = (IRFunction**)realloc(mod->functions,
                                            sizeof(IRFunction*) * mod->func_count);
    mod->functions[mod->func_count - 1] = fn;

    return fn;
}

IRBlock *ket_ir_new_block(IRFunction *fn, const char *name) {
    IRBlock *b = (IRBlock*)calloc(1, sizeof(IRBlock));
    b->id = fn->block_count;
    b->name = name;
    b->first = NULL;
    b->last = NULL;
    b->inst_count = 0;

    // Add to function
    fn->block_count++;
    fn->blocks = (IRBlock**)realloc(fn->blocks, sizeof(IRBlock*) * fn->block_count);
    fn->blocks[fn->block_count - 1] = b;

    if (!fn->entry) fn->entry = b;

    return b;
}

static IRInst *new_inst(IRBlock *block, IROp op, Type *type, Location loc) {
    IRInst *inst = (IRInst*)calloc(1, sizeof(IRInst));
    inst->op = op;
    inst->id = block->id * 10000 + block->inst_count; // unique-ish
    inst->type = type;
    inst->loc = loc;
    inst->next = NULL;

    // SSA destination register
    inst->dst.kind = IR_VAL_REG;
    inst->dst.reg_id = inst->id;
    inst->dst.type = type;

    if (block->last) {
        block->last->next = inst;
    } else {
        block->first = inst;
    }
    block->last = inst;
    block->inst_count++;

    return inst;
}

IRInst *ket_ir_add_inst(IRBlock *block, IROp op, Type *type, Location loc) {
    return new_inst(block, op, type, loc);
}

// ─── IR Printer (debug) ───────────────────────────────────────────────────────

static void print_ir_value(IRValue *v) {
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            printf("%lld", (long long)v->const_int);
            break;
        case IR_VAL_CONST_FLOAT:
            printf("%f", v->const_float);
            break;
        case IR_VAL_CONST_BOOL:
            printf("%s", v->const_bool ? "true" : "false");
            break;
        case IR_VAL_REG:
            printf("%%t%d", v->reg_id);
            break;
        case IR_VAL_GLOBAL:
            printf("@%s", v->global_name);
            break;
        case IR_VAL_FUNCTION:
            printf("@%s", v->func_name);
            break;
        case IR_VAL_BLOCK:
            printf("%%b%d", v->block_id);
            break;
        default:
            printf("undef");
    }
}

static void print_ir_op(IROp op) {
    static const char *names[] = {
        [IR_RET] = "ret", [IR_BR] = "br", [IR_BR_COND] = "br.cond",
        [IR_ADD] = "add", [IR_SUB] = "sub", [IR_MUL] = "mul", [IR_DIV] = "div",
        [IR_MOD] = "mod", [IR_AND] = "and", [IR_OR] = "or", [IR_XOR] = "xor",
        [IR_SHL] = "shl", [IR_SHR] = "shr",
        [IR_EQ] = "eq", [IR_NE] = "ne", [IR_LT] = "lt", [IR_GT] = "gt",
        [IR_LE] = "le", [IR_GE] = "ge",
        [IR_ALLOCA] = "alloca", [IR_LOAD] = "load", [IR_STORE] = "store",
        [IR_CALL] = "call", [IR_PHI] = "phi",
        [IR_SEXT] = "sext", [IR_ZEXT] = "zext", [IR_TRUNC] = "trunc",
        [IR_BITCAST] = "bitcast",
        [IR_UNREACHABLE] = "unreachable",
    };
    if (op >= 0 && op < sizeof(names)/sizeof(names[0]) && names[op])
        printf("%s", names[op]);
    else
        printf("op_%d", op);
}

void ket_ir_verify(IRModule *mod, Diagnostics *diag) {
    (void)diag;
    printf("\n; ─── IR Module ───\n");
    printf("; Functions: %d\n\n", mod->func_count);

    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];
        printf("define @%s(", fn->name);
        for (int i = 0; i < fn->var_arg_count; i++) {
            if (i) printf(", ");
            printf("i64");
        }
        printf(") {\n");

        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            printf("  b%d %s:\n", block->id, block->name ? block->name : "");

            IRInst *inst = block->first;
            while (inst) {
                printf("    t%d = ", inst->id);
                print_ir_op(inst->op);

                for (int i = 0; i < 4; i++) {
                    if (inst->operands[i].kind != IR_VAL_NONE) {
                        printf(" ");
                        print_ir_value(&inst->operands[i]);
                    }
                }

                printf("\n");
                inst = inst->next;
            }
        }
        printf("}\n\n");
    }
}

// ─── Lowering: AST → IR ──────────────────────────────────────────────────────

static IRBlock *current_block = NULL;
static IRFunction *current_fn = NULL;

static IRValue lower_expr(ASTNode *node) {
    IRValue v = { .kind = IR_VAL_NONE };

    if (!node) return v;

    switch (node->kind) {
        case N_LITERAL_INT: {
            v.kind = IR_VAL_CONST_INT;
            v.const_int = node->ival;
            v.type = NULL; // will be set by typecheck
            break;
        }
        case N_LITERAL_FLOAT: {
            v.kind = IR_VAL_CONST_FLOAT;
            v.const_float = node->fval;
            break;
        }
        case N_LITERAL_BOOL: {
            v.kind = IR_VAL_CONST_BOOL;
            v.const_bool = node->bval;
            break;
        }
        case N_IDENT: {
            // Variable reference — emit load
            IRInst *load = new_inst(current_block, IR_LOAD, node->type, node->span.start);
            load->operands[0].kind = IR_VAL_GLOBAL;
            load->operands[0].global_name = node->ident.name;
            v = load->dst;
            break;
        }
        case N_BINARY: {
            IRValue left = lower_expr(node->binary.bin_left);
            IRValue right = lower_expr(node->binary.bin_right);

            if (left.kind == IR_VAL_NONE || right.kind == IR_VAL_NONE) break;

            // Map TokenType to IROp
            IROp op = IR_ADD;
            switch (node->binary.bin_op) {
                case TK_PLUS:  op = IR_ADD; break;
                case TK_MINUS: op = IR_SUB; break;
                case TK_STAR:  op = IR_MUL; break;
                case TK_SLASH: op = IR_DIV; break;
                case TK_PERCENT: op = IR_MOD; break;
                case TK_EQEQ:  op = IR_EQ; break;
                case TK_BANGEQ: op = IR_NE; break;
                case TK_LT:    op = IR_LT; break;
                case TK_GT:    op = IR_GT; break;
                case TK_LTEQ:  op = IR_LE; break;
                case TK_GTEQ:  op = IR_GE; break;
                case TK_AMPAMP: op = IR_AND; break;
                case TK_PIPEPIPE: op = IR_OR; break;
                case TK_AMPERSAND: op = IR_AND; break;
                case TK_PIPE:  op = IR_OR; break;
                case TK_CARET: op = IR_XOR; break;
                case TK_LTLT:  op = IR_SHL; break;
                case TK_GTGT:  op = IR_SHR; break;
                default: break;
            }

            IRInst *inst = new_inst(current_block, op, node->type, node->span.start);
            inst->operands[0] = left;
            inst->operands[1] = right;
            v = inst->dst;
            break;
        }
        case N_UNARY: {
            IRValue right = lower_expr(node->unary.unary_right);
            if (right.kind == IR_VAL_NONE) break;

            if (node->unary.unary_op == TK_MINUS) {
                IRInst *inst = new_inst(current_block, IR_NEG, node->type, node->span.start);
                inst->operands[0] = right;
                v = inst->dst;
            } else if (node->unary.unary_op == TK_EXCLAM) {
                IRInst *inst = new_inst(current_block, IR_NOT, node->type, node->span.start);
                inst->operands[0] = right;
                v = inst->dst;
            }
            break;
        }
        case N_CALL: {
            // Built-in print
            if (node->call.call_callee->kind == N_IDENT &&
                strncmp(node->call.call_callee->ident.name, "print", 5) == 0) {
                // emit a call to printf/puts (handled by codegen)
                IRInst *inst = new_inst(current_block, IR_CALL, node->type, node->span.start);
                inst->operands[0].kind = IR_VAL_FUNCTION;
                inst->operands[0].func_name = "print";
                if (node->call.call_argc > 0) {
                    inst->operands[1] = lower_expr(node->call.call_args[0]);
                }
                v = inst->dst;
                break;
            }

            // Regular call
            IRInst *inst = new_inst(current_block, IR_CALL, node->type, node->span.start);
            inst->operands[0].kind = IR_VAL_FUNCTION;
            inst->operands[0].func_name = node->call.call_callee->ident.name;
            for (int i = 0; i < node->call.call_argc && i < 3; i++) {
                inst->operands[1 + i] = lower_expr(node->call.call_args[i]);
            }
            v = inst->dst;
            break;
        }
        case N_ASSIGN: {
            IRValue val = lower_expr(node->assign.assign_value);
            if (val.kind != IR_VAL_NONE && node->assign.assign_target->kind == N_IDENT) {
                IRInst *store = new_inst(current_block, IR_STORE, node->type, node->span.start);
                store->operands[0].kind = IR_VAL_GLOBAL;
                store->operands[0].global_name = node->assign.assign_target->ident.name;
                store->operands[1] = val;
            }
            break;
        }
        case N_LITERAL_STR: {
            v.kind = IR_VAL_GLOBAL;
            v.global_name = node->sval;
            v.type = NULL;
            break;
        }
        default:
            break;
    }

    return v;
}

static void lower_stmt(ASTNode *node) {
    if (!node) return;

    switch (node->kind) {
        case N_RETURN: {
            IRValue val = lower_expr(node->ret_val);
            IRInst *ret = new_inst(current_block, IR_RET, node->type, node->span.start);
            if (val.kind != IR_VAL_NONE) {
                ret->operands[0] = val;
            }
            break;
        }
        case N_IF: {
            IRValue cond = lower_expr(node->if_node.if_cond);

            IRBlock *then_block = ket_ir_new_block(current_fn, "if.then");
            IRBlock *else_block = ket_ir_new_block(current_fn, "if.else");
            IRBlock *merge_block = ket_ir_new_block(current_fn, "if.merge");

            // Branch
            IRInst *br = new_inst(current_block, IR_BR_COND, NULL, node->span.start);
            br->operands[0] = cond;
            br->operands[1].kind = IR_VAL_BLOCK;
            br->operands[1].block_id = then_block->id;
            br->operands[2].kind = IR_VAL_BLOCK;
            br->operands[2].block_id = else_block->id;

            // Then block
            current_block = then_block;
            lower_stmt(node->if_node.if_then);
            new_inst(current_block, IR_BR, NULL, node->span.start)->operands[0].kind = IR_VAL_BLOCK;
            current_block->last->operands[0].block_id = merge_block->id;

            // Else block
            current_block = else_block;
            if (node->if_node.if_else) {
                lower_stmt(node->if_node.if_else);
            }
            new_inst(current_block, IR_BR, NULL, node->span.start)->operands[0].kind = IR_VAL_BLOCK;
            current_block->last->operands[0].block_id = merge_block->id;

            // Merge
            current_block = merge_block;
            break;
        }
        case N_WHILE: {
            IRBlock *cond_block = ket_ir_new_block(current_fn, "while.cond");
            IRBlock *body_block = ket_ir_new_block(current_fn, "while.body");
            IRBlock *end_block = ket_ir_new_block(current_fn, "while.end");

            // Branch to cond
            new_inst(current_block, IR_BR, NULL, node->span.start)->operands[0].kind = IR_VAL_BLOCK;
            current_block->last->operands[0].block_id = cond_block->id;

            // Condition
            current_block = cond_block;
            IRValue cond = lower_expr(node->loop.loop_cond);
            IRInst *br2 = new_inst(current_block, IR_BR_COND, NULL, node->span.start);
            br2->operands[0] = cond;
            br2->operands[1].kind = IR_VAL_BLOCK; br2->operands[1].block_id = body_block->id;
            br2->operands[2].kind = IR_VAL_BLOCK; br2->operands[2].block_id = end_block->id;

            // Body
            current_block = body_block;
            lower_stmt(node->loop.loop_body);
            new_inst(current_block, IR_BR, NULL, node->span.start)->operands[0].kind = IR_VAL_BLOCK;
            current_block->last->operands[0].block_id = cond_block->id;

            // End
            current_block = end_block;
            break;
        }
        case N_VAR_DECL: {
            if (node->var_decl.var_init) {
                IRValue init = lower_expr(node->var_decl.var_init);
                IRInst *alloca = new_inst(current_block, IR_ALLOCA, node->type, node->span.start);
                alloca->operands[0].kind = IR_VAL_GLOBAL;
                alloca->operands[0].global_name = node->var_decl.var_name;

                IRInst *store = new_inst(current_block, IR_STORE, node->type, node->span.start);
                store->operands[0] = alloca->dst;
                store->operands[1] = init;
            }
            break;
        }
        case N_BLOCK: {
            for (int i = 0; i < node->block.stmt_count; i++) {
                lower_stmt(node->block.stmts[i]);
            }
            break;
        }
        case N_EXPR_STMT: {
            lower_expr(node->ret_val);
            break;
        }
        default:
            lower_expr(node);
            break;
    }
}

IRModule *ket_ir_lower(Context *ctx, ASTNode *program) {
    IRModule *mod = ket_ir_new_module(ctx);

    // Create function for each top-level function
    for (int i = 0; i < program->block.stmt_count; i++) {
        ASTNode *node = program->block.stmts[i];
        if (node->kind == N_FN_DECL) {
            const char *fn_name = node->fn_decl.fn_name ?
                ket_intern(ctx, node->fn_decl.fn_name, node->fn_decl.fn_namelen) : "anon";

            IRFunction *fn = ket_ir_new_function(mod, fn_name, NULL);
            fn->var_arg_count = node->fn_decl.fn_param_count;
            current_fn = fn;

            IRBlock *entry = ket_ir_new_block(fn, "entry");
            current_block = entry;

            // Lower function body
            lower_stmt(node->fn_decl.fn_body);

            // Ensure terminator
            if (current_block && !current_block->last) {
                new_inst(current_block, IR_RET, NULL, LOC_NONE);
            }
        }
    }

    current_block = NULL;
    current_fn = NULL;

    return mod;
}

void ket_ir_free(IRModule *mod) {
    if (!mod) return;
    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];
        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            IRInst *inst = block->first;
            while (inst) {
                IRInst *next = inst->next;
                free(inst);
                inst = next;
            }
            free(block->preds);
            free(block->succs);
            free(block->dom_children);
            free(block);
        }
        free(fn->blocks);
        free(fn);
    }
    free(mod->functions);
    free(mod->globals);
    free(mod);
}
