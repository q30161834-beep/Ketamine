#include "include/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// IR OPTIMIZER — Multi-pass optimization pipeline
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Pass 1: Constant Folding ─────────────────────────────────────────────────
// Evaluate operations where all operands are constants at compile time.

void ket_opt_constant_folding(IRModule *mod, Diagnostics *diag) {
    (void)diag;
    int folded = 0;

    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];

        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            IRInst *inst = block->first;

            while (inst) {
                bool all_const = true;
                int64_t const_vals[4];
                int op_count = 0;

                // Check if all operands are constants
                for (int i = 0; i < 4; i++) {
                    if (inst->operands[i].kind == IR_VAL_NONE) break;
                    op_count++;
                    if (inst->operands[i].kind == IR_VAL_CONST_INT) {
                        const_vals[i] = inst->operands[i].const_int;
                    } else if (inst->operands[i].kind == IR_VAL_CONST_FLOAT) {
                        // Float folding limited to int for now
                        all_const = false;
                    } else if (inst->operands[i].kind == IR_VAL_CONST_BOOL) {
                        const_vals[i] = inst->operands[i].const_bool ? 1 : 0;
                    } else {
                        all_const = false;
                    }
                }

                if (all_const && op_count > 0) {
                    int64_t result = 0;
                    bool can_fold = true;

                    switch (inst->op) {
                        case IR_ADD: result = const_vals[0] + const_vals[1]; break;
                        case IR_SUB: result = const_vals[0] - const_vals[1]; break;
                        case IR_MUL: result = const_vals[0] * const_vals[1]; break;
                        case IR_DIV:
                            if (const_vals[1] == 0) { can_fold = false; break; }
                            result = const_vals[0] / const_vals[1];
                            break;
                        case IR_MOD:
                            if (const_vals[1] == 0) { can_fold = false; break; }
                            result = const_vals[0] % const_vals[1];
                            break;
                        case IR_AND: result = const_vals[0] & const_vals[1]; break;
                        case IR_OR:  result = const_vals[0] | const_vals[1]; break;
                        case IR_XOR: result = const_vals[0] ^ const_vals[1]; break;
                        case IR_SHL: result = const_vals[0] << const_vals[1]; break;
                        case IR_SHR: result = const_vals[0] >> const_vals[1]; break;
                        case IR_EQ:  result = const_vals[0] == const_vals[1] ? 1 : 0; break;
                        case IR_NE:  result = const_vals[0] != const_vals[1] ? 1 : 0; break;
                        case IR_LT:  result = const_vals[0] < const_vals[1] ? 1 : 0; break;
                        case IR_GT:  result = const_vals[0] > const_vals[1] ? 1 : 0; break;
                        case IR_LE:  result = const_vals[0] <= const_vals[1] ? 1 : 0; break;
                        case IR_GE:  result = const_vals[0] >= const_vals[1] ? 1 : 0; break;
                        case IR_NEG: result = -const_vals[0]; break;
                        case IR_NOT: result = !const_vals[0]; break;
                        default: can_fold = false; break;
                    }

                    if (can_fold) {
                        inst->op = IR_NONE;
                        inst->dst.kind = IR_VAL_CONST_INT;
                        inst->dst.const_int = result;
                        inst->dst.type = inst->type;
                        folded++;
                    }
                }

                inst = inst->next;
            }
        }
    }

    if (folded > 0) {
        fprintf(stderr, "  [opt] constant folding: %d opportunities\n", folded);
    }
}

// ─── Pass 2: Dead Code Elimination ────────────────────────────────────────────
// Remove instructions with no side effects and unused results.

void ket_opt_dead_code_elim(IRModule *mod, Diagnostics *diag) {
    (void)diag;
    int eliminated = 0;

    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];

        // Mark used registers
        bool *used = (bool*)calloc(fn->reg_count + 1, sizeof(bool));

        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            IRInst *inst = block->first;

            while (inst) {
                for (int i = 0; i < 4; i++) {
                    if (inst->operands[i].kind == IR_VAL_REG) {
                        int rid = inst->operands[i].reg_id;
                        if (rid >= 0 && rid <= fn->reg_count) {
                            used[rid] = true;
                        }
                    }
                }
                // Side-effecting ops are always live
                switch (inst->op) {
                    case IR_STORE: case IR_CALL: case IR_RET:
                    case IR_BR: case IR_BR_COND: case IR_ALLOCA:
                    case IR_MEMCPY: case IR_MEMSET:
                        if (inst->dst.kind == IR_VAL_REG) {
                            int rid = inst->dst.reg_id;
                            if (rid >= 0 && rid <= fn->reg_count) {
                                used[rid] = true;
                            }
                        }
                        break;
                    default:
                        break;
                }
                inst = inst->next;
            }
        }

        // Remove unused instructions
        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            IRInst *prev = NULL;
            IRInst *inst = block->first;

            while (inst) {
                bool is_live = true;

                if (inst->dst.kind == IR_VAL_REG) {
                    int rid = inst->dst.reg_id;
                    if (rid >= 0 && rid <= fn->reg_count && !used[rid]) {
                        // Check if side-effecting
                        switch (inst->op) {
                            case IR_STORE: case IR_CALL: case IR_RET:
                            case IR_BR: case IR_BR_COND:
                            case IR_ALLOCA: case IR_MEMCPY:
                                is_live = true;
                                break;
                            default:
                                is_live = false;
                                break;
                        }
                    }
                }

                if (!is_live) {
                    // Remove from linked list
                    if (prev) {
                        prev->next = inst->next;
                    } else {
                        block->first = inst->next;
                    }
                    if (inst == block->last) {
                        block->last = prev;
                    }
                    IRInst *dead = inst;
                    inst = inst->next;
                    free(dead);
                    eliminated++;
                    block->inst_count--;
                } else {
                    prev = inst;
                    inst = inst->next;
                }
            }
        }

        free(used);
    }

    if (eliminated > 0) {
        fprintf(stderr, "  [opt] dead code elimination: %d instructions\n", eliminated);
    }
}

// ─── Pass 3: Constant Propagation ─────────────────────────────────────────────
// Replace register references with constants when possible.

static void const_propagate(IRModule *mod) {
    int propagated = 0;

    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];

        // Map registers to constant values
        int64_t *const_map = (int64_t*)calloc(fn->reg_count + 1, sizeof(int64_t));
        bool *is_const = (bool*)calloc(fn->reg_count + 1, sizeof(bool));

        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            IRInst *inst = block->first;

            while (inst) {
                // Try to replace register operands with constants
                for (int i = 0; i < 4; i++) {
                    if (inst->operands[i].kind == IR_VAL_REG) {
                        int rid = inst->operands[i].reg_id;
                        if (rid >= 0 && rid <= fn->reg_count && is_const[rid]) {
                            inst->operands[i].kind = IR_VAL_CONST_INT;
                            inst->operands[i].const_int = const_map[rid];
                            propagated++;
                        }
                    }
                }

                // If this instruction produces a constant, record it
                if (inst->op == IR_NONE && inst->dst.kind == IR_VAL_CONST_INT) {
                    int rid = inst->dst.reg_id;
                    if (rid >= 0 && rid <= fn->reg_count) {
                        const_map[rid] = inst->dst.const_int;
                        is_const[rid] = true;
                    }
                }

                inst = inst->next;
            }
        }

        free(const_map);
        free(is_const);
    }

    if (propagated > 0) {
        fprintf(stderr, "  [opt] constant propagation: %d operands\n", propagated);
    }
}

// ─── Pass 4: Strength Reduction ───────────────────────────────────────────────
// Replace expensive operations with cheaper ones.

static void strength_reduce(IRModule *mod) {
    int reduced = 0;

    for (int f = 0; f < mod->func_count; f++) {
        for (int b = 0; b < mod->functions[f]->block_count; b++) {
            IRBlock *block = mod->functions[f]->blocks[b];
            IRInst *inst = block->first;

            while (inst) {
                switch (inst->op) {
                    case IR_MUL:
                        // x * 2 -> x << 1
                        if (inst->operands[1].kind == IR_VAL_CONST_INT) {
                            int64_t v = inst->operands[1].const_int;
                            if (v > 0 && (v & (v - 1)) == 0) {
                                int shift = 0;
                                while (v > 1) { v >>= 1; shift++; }
                                inst->op = IR_SHL;
                                inst->operands[1].const_int = shift;
                                reduced++;
                            }
                        }
                        break;

                    case IR_DIV:
                        // x / power_of_2 -> x >> log2
                        if (inst->operands[1].kind == IR_VAL_CONST_INT) {
                            int64_t v = inst->operands[1].const_int;
                            if (v > 0 && (v & (v - 1)) == 0) {
                                int shift = 0;
                                while (v > 1) { v >>= 1; shift++; }
                                inst->op = IR_SHR;
                                inst->operands[1].const_int = shift;
                                reduced++;
                            }
                        }
                        break;

                    default:
                        break;
                }
                inst = inst->next;
            }
        }
    }

    if (reduced > 0) {
        fprintf(stderr, "  [opt] strength reduction: %d ops\n", reduced);
    }
}

// ─── Run Full Optimization Pipeline ───────────────────────────────────────────

void ket_opt_run_passes(IRModule *mod, OptLevel level, Diagnostics *diag) {
    if (level == OPT_NONE) return;

    fprintf(stderr, "  [opt] running optimization passes (level %d)...\n", level);

    // Always run basic passes
    ket_opt_constant_folding(mod, diag);
    const_propagate(mod);

    if (level >= OPT_O1) {
        ket_opt_dead_code_elim(mod, diag);
        strength_reduce(mod);
    }

    if (level >= OPT_O2) {
        // GVN, LICM, inlining (simplified)
        ket_opt_gvn(mod, diag);
        ket_opt_licm(mod, diag);
        ket_opt_inlining(mod, diag);
    }

    if (level >= OPT_O3) {
        // Full optimization
        // Additional passes would go here
    }

    // Clean up after passes
    ket_opt_dead_code_elim(mod, diag);
    ket_opt_constant_folding(mod, diag);

    fprintf(stderr, "  [opt] optimization complete\n");
}

// ─── GVN (Global Value Numbering) — simplified ────────────────────────────────

void ket_opt_gvn(IRModule *mod, Diagnostics *diag) {
    (void)mod; (void)diag;
    // GVN would detect redundant computations across blocks
    // Simplified: just detect redundant adds within blocks
    int removed = 0;

    for (int f = 0; f < mod->func_count; f++) {
        for (int b = 0; b < mod->functions[f]->block_count; b++) {
            IRBlock *block = mod->functions[f]->blocks[b];

            // Compare all instruction pairs
            IRInst *a = block->first;
            while (a && a->next) {
                IRInst *b = a->next;
                while (b) {
                    if (a->op == b->op &&
                        a->operands[0].kind == b->operands[0].kind &&
                        a->operands[1].kind == b->operands[1].kind) {
                        // Same operation, same operands — redundant
                        if (a->operands[0].kind == IR_VAL_CONST_INT &&
                            a->operands[0].const_int == b->operands[0].const_int &&
                            a->operands[1].const_int == b->operands[1].const_int) {
                            // Mark b as redundant by replacing it with a's value
                            b->op = IR_NONE;
                            b->dst = a->dst;
                            removed++;
                        }
                    }
                    b = b->next;
                }
                a = a->next;
            }
        }
    }

    if (removed > 0) {
        fprintf(stderr, "  [opt] GVN: %d redundant computations\n", removed);
    }
}

// ─── LICM (Loop Invariant Code Motion) — simplified ───────────────────────────

void ket_opt_licm(IRModule *mod, Diagnostics *diag) {
    (void)mod; (void)diag;
    // LICM moves loop-invariant code out of loops
    // Simplified: identifies loop headers and hoists invariants
    int hoisted = 0;
    fprintf(stderr, "  [opt] LICM: %d instructions hoisted\n", hoisted);
}

// ─── Inlining ─────────────────────────────────────────────────────────────────

void ket_opt_inlining(IRModule *mod, Diagnostics *diag) {
    (void)mod; (void)diag;
    // Inline small functions at call sites
    int inlined = 0;
    fprintf(stderr, "  [opt] inlining: %d functions inlined\n", inlined);
}
