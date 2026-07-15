#include "include/codegen.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// ═══════════════════════════════════════════════════════════════════════════════
// FORTH CODEGEN — Translates Ketamine IR to ANS Forth source
// ═══════════════════════════════════════════════════════════════════════════════
//
// Key mapping:
//   IR ADD t1 t2 t3  →  (stack: t2 t3)  +  (result: t1 on stack)
//   IR CALL print    →  .
//   IR BR cond L1 L2 →  IF L1 ELSE L2 THEN
//   IR RET           →  EXIT
//
// Since Forth is stack-based, SSA register IDs map to stack positions.
// We emit local VARIABLEs for any value that must persist across basic blocks.
// ═══════════════════════════════════════════════════════════════════════════════

#define MAX_FORTH_LABELS 4096

typedef struct {
    FILE    *out;
    int      indent;
    int      label_count;
    int      label_map[MAX_FORTH_LABELS];  // IR block -> Forth label ID
    int      var_count;                    // local variables allocated
    bool     had_error;
    IRModule *mod;
} ForthGen;

static int forth_label(ForthGen *g) {
    return g->label_count++;
}

static void forth_emit(ForthGen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->out, fmt, ap);
    va_end(ap);
}

// ─── Map an IR op to its Forth equivalent ────────────────────────────────────
static const char *ir_to_forth_op(int ir_op) {
    switch (ir_op) {
        case IR_ADD:  return "+";
        case IR_SUB:  return "-";
        case IR_MUL:  return "*";
        case IR_DIV:  return "/";
        case IR_MOD:  return "MOD";
        case IR_AND:  return "AND";
        case IR_OR:   return "OR";
        case IR_XOR:  return "XOR";
        case IR_SHL:  return "LSHIFT";
        case IR_SHR:  return "ARSHIFT";
        case IR_NEG:  return "NEGATE";
        case IR_NOT:  return "0=";
        case IR_EQ:   return "=";
        case IR_NE:   return "<>";
        case IR_LT:   return "<";
        case IR_GT:   return ">";
        case IR_LE:   return "<=";
        case IR_GE:   return ">=";
        default:      return NULL;
    }
}

// ─── Emit a value onto the Forth stack ──────────────────────────────────────
static void forth_emit_value(ForthGen *g, IRValue *v) {
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            forth_emit(g, "%lld", (long long)v->const_int);
            break;
        case IR_VAL_CONST_FLOAT:
            forth_emit(g, "%f", v->const_float);
            break;
        case IR_VAL_CONST_BOOL:
            forth_emit(g, "%s", v->const_bool ? "-1" : "0");
            break;
        case IR_VAL_REG:
            // SSA values live on the stack — just reference by name
            forth_emit(g, "v%d", v->reg_id);
            break;
        case IR_VAL_GLOBAL:
            forth_emit(g, "%s", v->global_name ? v->global_name : "?");
            break;
        case IR_VAL_FUNCTION:
            forth_emit(g, "' %s", v->func_name ? v->func_name : "?");
            break;
        default:
            forth_emit(g, "0");
    }
}

// ─── Emit a single IR instruction in Forth ──────────────────────────────────
static void forth_gen_inst(ForthGen *g, IRBlock *block, IRInst *inst) {
    (void)block;

    const char *forth_op = ir_to_forth_op(inst->op);

    switch (inst->op) {
        case IR_NONE:
            if (inst->dst.kind == IR_VAL_CONST_INT && inst->dst.reg_id >= 0) {
                // Constant: push it on stack and store to variable
                forth_emit(g, "  %lld TO v%d\n",
                          (long long)inst->dst.const_int, inst->dst.reg_id);
            }
            break;

        // ── Binary arithmetic (postfix) ──
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
        case IR_AND: case IR_OR:  case IR_XOR: case IR_SHL: case IR_SHR: {
            forth_emit(g, "  v%d v%d ", inst->operands[0].reg_id, inst->operands[1].reg_id);
            forth_emit(g, "%s TO v%d\n", forth_op, inst->dst.reg_id);
            break;
        }

        // ── Negation / Not ──
        case IR_NEG: {
            forth_emit(g, "  v%d NEGATE TO v%d\n", inst->operands[0].reg_id, inst->dst.reg_id);
            break;
        }
        case IR_NOT: {
            forth_emit(g, "  v%d 0= TO v%d\n", inst->operands[0].reg_id, inst->dst.reg_id);
            break;
        }

        // ── Comparisons (leave -1/0 on stack, typical Forth) ──
        case IR_EQ: case IR_NE: case IR_LT: case IR_GT: case IR_LE: case IR_GE: {
            forth_emit(g, "  v%d v%d %s TO v%d\n",
                      inst->operands[0].reg_id, inst->operands[1].reg_id,
                      forth_op, inst->dst.reg_id);
            break;
        }

        // ── Memory ──
        case IR_ALLOCA: {
            forth_emit(g, "  HERE TO v%d  1 CELLS ALLOT\n", inst->dst.reg_id);
            break;
        }
        case IR_LOAD: {
            forth_emit(g, "  v%d @ TO v%d\n", inst->operands[0].reg_id, inst->dst.reg_id);
            break;
        }
        case IR_STORE: {
            forth_emit(g, "  v%d v%d !\n", inst->operands[1].reg_id, inst->operands[0].reg_id);
            break;
        }

        // ── Function calls ──
        case IR_CALL: {
            const char *fn = inst->operands[0].func_name ?
                inst->operands[0].func_name : "unknown";

            if (strcmp(fn, "print") == 0) {
                // Handle print: if it's a string, use ." ; if int, use .
                IRValue *arg = &inst->operands[1];
                if (arg->kind == IR_VAL_GLOBAL) {
                    forth_emit(g, "  .\" %s\"\n", arg->global_name ? arg->global_name : "");
                } else if (arg->kind == IR_VAL_CONST_INT) {
                    forth_emit(g, "  %lld .\n", (long long)arg->const_int);
                } else {
                    forth_emit(g, "  v%d .\n", arg->reg_id);
                }
            } else {
                // Call Forth word
                int argc = 0;
                for (int i = 1; i < 4; i++) {
                    if (inst->operands[i].kind == IR_VAL_NONE) break;
                    argc++;
                }

                // Push args
                for (int i = 1; i <= argc; i++) {
                    if (inst->operands[i].kind == IR_VAL_NONE) break;
                    forth_emit(g, "  ");
                    forth_emit_value(g, &inst->operands[i]);
                    forth_emit(g, "\n");
                }

                // Call word
                if (inst->dst.reg_id >= 0) {
                    forth_emit(g, "  %s TO v%d\n", fn, inst->dst.reg_id);
                } else {
                    forth_emit(g, "  %s\n", fn);
                }
            }
            break;
        }

        // ── Return ──
        case IR_RET:
            if (inst->operands[0].kind != IR_VAL_NONE) {
                forth_emit(g, "  ");
                forth_emit_value(g, &inst->operands[0]);
                forth_emit(g, " EXIT\n");
            } else {
                forth_emit(g, "  EXIT\n");
            }
            break;

        // ── Control flow ──
        case IR_BR:
            if (inst->operands[0].kind == IR_VAL_BLOCK) {
                int target = inst->operands[0].block_id;
                // Use BRANCH (available in most Forths)
                forth_emit(g, "  BRANCH label_%d\n", target);
            }
            break;

        case IR_BR_COND: {
            int true_block = inst->operands[1].block_id;
            int false_block = inst->operands[2].block_id;
            forth_emit(g, "  v%d IF\n", inst->operands[0].reg_id);
            forth_emit(g, "    BRANCH label_%d\n", true_block);
            forth_emit(g, "  ELSE\n");
            forth_emit(g, "    BRANCH label_%d\n", false_block);
            forth_emit(g, "  THEN\n");
            break;
        }

        case IR_UNREACHABLE:
            forth_emit(g, "  ABORT\" unreachable\"\n");
            break;

        default:
            forth_emit(g, "  \\ unhandled op: %d\n", inst->op);
            break;
    }
}

// ─── Emit an entire IR function as a Forth word ─────────────────────────────
static void forth_gen_function(ForthGen *g, IRFunction *fn) {
    const char *fn_name = fn->name ? fn->name : "anon";

    // Start word definition
    forth_emit(g, "\n\\ --- %s ---\n", fn_name);

    // Scan all instructions to find max register ID and which are live across blocks
    int max_reg = -1;
    int inst_count = 0;
    for (int b = 0; b < fn->block_count; b++) {
        IRInst *inst = fn->blocks[b]->first;
        while (inst) {
            if (inst->dst.reg_id > max_reg) max_reg = inst->dst.reg_id;
            inst = inst->next;
            inst_count++;
        }
    }

    // Local variables: one CELL per SSA register that lives across blocks
    // For simplicity, allocate locals for ALL registers
    if (max_reg >= 0) {
        // Use LOCAL (gforth syntax with VALUE) — each register is a VALUE
        forth_emit(g, "  \\ Local variables\n");
        for (int i = 0; i <= max_reg; i++) {
            forth_emit(g, "  VARIABLE v%d\n", i);
        }
    }

    // Special case: main function — make it runnable
    if (strcmp(fn_name, "main") == 0) {
        forth_emit(g, ": MAIN\n");
    } else {
        forth_emit(g, ": %s\n", fn_name);
    }

    // Emit labels for each block
    for (int b = 0; b < fn->block_count; b++) {
        IRBlock *block = fn->blocks[b];

        // Label for the block
        forth_emit(g, "label_%d\n", block->id);

        // Store BRANCH target for this label
        // (Forth assembler style: LABEL defines a branch target)

        IRInst *inst = block->first;
        while (inst) {
            forth_gen_inst(g, block, inst);
            inst = inst->next;
        }
    }

    // End word
    forth_emit(g, ";\n");
}

// ═══════════════════════════════════════════════════════════════════════════════
// ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════════

int codegen_forth_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Cannot open output: %s\n", path);
        return 1;
    }

    ForthGen g;
    g.out = f;
    g.indent = 0;
    g.label_count = 0;
    g.var_count = 0;
    g.had_error = false;
    g.mod = mod;

    // Header
    forth_emit(&g, "\\ Ketamine Language v%d.%d.%d — Forth backend\n",
               KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
    forth_emit(&g, "\\ Auto-generated. Compatible with ANS Forth / gforth.\n");
    forth_emit(&g, "\\ Run with: gforth %s -e \"MAIN BYE\"\n", path);
    forth_emit(&g, "\n");

    // Print helper: redefine . to print a number followed by newline
    forth_emit(&g, "\\ Print helper (like Ketamine's print)\n");
    forth_emit(&g, ": PRINT  . CR ;\n");
    forth_emit(&g, "\n");

    // BRANCH primitive: used for basic block jumps
    forth_emit(&g, "\\ Explicit branch (for block control flow)\n");
    forth_emit(&g, "VARIABLE 'BRANCH\n");
    forth_emit(&g, ": BRANCH  R> DROP >R ;  \\ compile-time: just continue\n");
    forth_emit(&g, "\n");

    // Emit all functions
    for (int i = 0; i < mod->func_count; i++) {
        forth_gen_function(&g, mod->functions[i]);
    }

    // Entry point (if main exists)
    bool has_main = false;
    for (int i = 0; i < mod->func_count; i++) {
        if (strcmp(mod->functions[i]->name, "main") == 0) {
            has_main = true;
            break;
        }
    }

    if (has_main) {
        forth_emit(&g, "\n\\ Entry point\n");
        forth_emit(&g, "MAIN BYE\n");
    }

    fclose(f);

    if (opts->verbose)
        fprintf(stderr, "ketc: Forth source -> %s (%d functions)\n",
                path, mod->func_count);

    return g.had_error ? 1 : 0;
}
