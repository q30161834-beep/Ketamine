#ifdef _WIN32
  #include <windows.h>
#endif
#include "include/codegen.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// ═══════════════════════════════════════════════════════════════════════════════
// x86-64 BACKEND — Shared instruction selection & encoding
// ═══════════════════════════════════════════════════════════════════════════════

// ─── x86-64 Register identifiers ─────────────────────────────────────────────
#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6
#define REG_RDI 7
#define REG_R8  8
#define REG_R9  9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15
#define REG_NONE -1

static const char *reg_name(int r) {
    static const char *names[] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15"
    };
    if (r >= 0 && r < 16) return names[r];
    return "??";
}

// ─── Instruction selection: maps IR ops to x86-64 ────────────────────────────
typedef enum {
    X64_MOV, X64_MOV_IMM, X64_MOVZX,
    X64_ADD, X64_SUB, X64_IMUL, X64_IDIV, X64_AND, X64_OR, X64_XOR,
    X64_NEG, X64_NOT, X64_SHL, X64_SHR, X64_SAR,
    X64_CMP, X64_SETcc,
    X64_PUSH, X64_POP,
    X64_JMP, X64_Jcc, X64_CALL, X64_RET,
    X64_LEA, X64_ALLOCA,
    X64_LOAD, X64_STORE,
    X64_LABEL, X64_NOP,
} X64Op;

typedef struct {
    X64Op op;
    int    dst;       // destination register
    int    src;       // source register
    int64_t imm;      // immediate value
    bool   has_imm;
    int    label;     // target label for branches
    int    cond;      // condition code for Jcc/SETcc
    int    stack_off; // stack offset for alloca/load/store
    int    size;      // operand size in bytes (1,2,4,8)
} X64Inst;

#define MAX_X64_INSTS 4096
#define MAX_REGS 16
#define MAX_STACK_SLOTS 256

typedef struct {
    X64Inst insts[MAX_X64_INSTS];
    int     count;
    // Register allocation
    int     reg_map[4096];      // IR value -> physical register
    int     reg_rev[MAX_REGS];  // physical register -> IR value
    bool    reg_used[MAX_REGS];
    int     next_reg;
    // Stack slots
    int64_t stack_slots[4096];  // IR value -> stack offset
    int     stack_size;
    // Labels
    int     label_count;
    // Function info
    const char *func_name;
    int         arg_count;
    // Options
    bool        verbose;
} X64Context;

// Parameter passing registers (System V AMD64)
static const int arg_regs_sysv[] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
static const int arg_regs_win[]  = { REG_RCX, REG_RDX, REG_R8, REG_R9 };
#define MAX_ARG_REGS 6

static int x64_add_inst(X64Context *cx, X64Op op) {
    if (cx->count >= MAX_X64_INSTS) return -1;
    int idx = cx->count++;
    memset(&cx->insts[idx], 0, sizeof(X64Inst));
    cx->insts[idx].op = op;
    cx->insts[idx].size = 8;
    cx->insts[idx].dst = REG_NONE;
    cx->insts[idx].src = REG_NONE;
    return idx;
}

static int x64_label(X64Context *cx) {
    return cx->label_count++;
}

// ─── Register allocator (linear scan, simple) ───────────────────────────────
#ifdef _WIN32
static const int *arg_regs = arg_regs_win;
static int num_arg_regs = 4;
#else
static const int *arg_regs = arg_regs_sysv;
static int num_arg_regs = 6;
#endif

static int alloc_reg(X64Context *cx, int ir_val) {
    // Check if already mapped
    if (cx->reg_map[ir_val] != REG_NONE)
        return cx->reg_map[ir_val];

    // Find free register (skip RSP)
    for (int r = 0; r < MAX_REGS; r++) {
        if (!cx->reg_used[r] && r != REG_RSP) {
            cx->reg_used[r] = true;
            cx->reg_map[ir_val] = r;
            cx->reg_rev[r] = ir_val;
            return r;
        }
    }

    // Spill: use R10/R11 as scratch
    if (!cx->reg_used[REG_R10]) { cx->reg_used[REG_R10]=true; cx->reg_map[ir_val]=REG_R10; cx->reg_rev[REG_R10]=ir_val; return REG_R10; }
    if (!cx->reg_used[REG_R11]) { cx->reg_used[REG_R11]=true; cx->reg_map[ir_val]=REG_R11; cx->reg_rev[REG_R11]=ir_val; return REG_R11; }
    if (!cx->reg_used[REG_R12]) { cx->reg_used[REG_R12]=true; cx->reg_map[ir_val]=REG_R12; cx->reg_rev[REG_R12]=ir_val; return REG_R12; }
    if (!cx->reg_used[REG_R13]) { cx->reg_used[REG_R13]=true; cx->reg_map[ir_val]=REG_R13; cx->reg_rev[REG_R13]=ir_val; return REG_R13; }
    if (!cx->reg_used[REG_R14]) { cx->reg_used[REG_R14]=true; cx->reg_map[ir_val]=REG_R14; cx->reg_rev[REG_R14]=ir_val; return REG_R14; }
    if (!cx->reg_used[REG_R15]) { cx->reg_used[REG_R15]=true; cx->reg_map[ir_val]=REG_R15; cx->reg_rev[REG_R15]=ir_val; return REG_R15; }

    // Spill to stack: use R14, spill current occupant
    int spill_reg = REG_R14;
    int spill_val = cx->reg_rev[spill_reg];
    cx->stack_slots[spill_val] = cx->stack_size;
    cx->stack_size += 8;
    cx->reg_map[spill_val] = REG_NONE;
    cx->reg_map[ir_val] = spill_reg;
    cx->reg_rev[spill_reg] = ir_val;

    int idx = x64_add_inst(cx, X64_MOV);
    cx->insts[idx].dst = spill_reg;
    cx->insts[idx].has_imm = false;
    // Emit store to stack
    idx = x64_add_inst(cx, X64_STORE);
    cx->insts[idx].src = spill_reg;
    cx->insts[idx].stack_off = cx->stack_slots[spill_val];
    return spill_reg;
}

static void free_reg(X64Context *cx, int ir_val) {
    if (cx->reg_map[ir_val] != REG_NONE) {
        int r = cx->reg_map[ir_val];
        cx->reg_used[r] = false;
        cx->reg_map[ir_val] = REG_NONE;
        cx->reg_rev[r] = REG_NONE;
    }
}

// ─── Shared instruction selector: IR -> X64Inst list ────────────────────────
static bool select_instructions(X64Context *cx, IRFunction *fn) {
    memset(cx->reg_map, 0xff, sizeof(cx->reg_map));
    memset(cx->reg_rev, 0xff, sizeof(cx->reg_rev));
    memset(cx->reg_used, 0, sizeof(cx->reg_used));
    memset(cx->stack_slots, 0, sizeof(cx->stack_slots));
    cx->count = 0;
    cx->stack_size = 0;
    cx->label_count = 0;
    cx->func_name = fn->name;

    // Map IR params to arg regs
    int param_idx = 0;
    for (int i = 0; param_idx < fn->var_arg_count && i < num_arg_regs; i++) {
        cx->reg_used[arg_regs[i]] = true;
        cx->reg_map[param_idx] = arg_regs[i];
        cx->reg_rev[arg_regs[i]] = param_idx;
        param_idx++;
    }

    // Walk all instructions in all blocks
    for (int b = 0; b < fn->block_count; b++) {
        IRBlock *block = fn->blocks[b];

        // Emit block label
        int lbl = x64_add_inst(cx, X64_LABEL);
        cx->insts[lbl].label = block->id;

        IRInst *inst = block->first;
        while (inst) {
            switch (inst->op) {
                case IR_NONE: {
                    int idx = x64_add_inst(cx, X64_NOP);
                    if (inst->dst.kind == IR_VAL_CONST_INT) {
                        // Load constant into register
                        idx = x64_add_inst(cx, X64_MOV_IMM);
                        cx->insts[idx].dst = alloc_reg(cx, inst->dst.reg_id);
                        cx->insts[idx].imm = inst->dst.const_int;
                        cx->insts[idx].has_imm = true;
                    }
                    break;
                }

                case IR_ADD: case IR_SUB: case IR_MUL:
                case IR_AND: case IR_OR:  case IR_XOR:
                case IR_SHL: case IR_SHR: {
                    X64Op ops[] = {
                        [IR_ADD]=X64_ADD, [IR_SUB]=X64_SUB, [IR_MUL]=X64_IMUL,
                        [IR_AND]=X64_AND, [IR_OR]=X64_OR, [IR_XOR]=X64_XOR,
                        [IR_SHL]=X64_SHL, [IR_SHR]=X64_SHR
                    };
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);
                    int src0_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int src1_r = alloc_reg(cx, inst->operands[1].reg_id);
                    int idx = x64_add_inst(cx, X64_MOV);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].src = src0_r;
                    idx = x64_add_inst(cx, ops[inst->op]);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].src = src1_r;
                    free_reg(cx, inst->operands[0].reg_id);
                    free_reg(cx, inst->operands[1].reg_id);
                    break;
                }

                case IR_DIV: case IR_MOD: {
                    // IDIV: dividend in RDX:RAX, result in RAX, remainder in RDX
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);
                    int src0_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int src1_r = alloc_reg(cx, inst->operands[1].reg_id);
                    int idx;

                    // Move dividend to RAX
                    idx = x64_add_inst(cx, X64_MOV);
                    cx->insts[idx].dst = REG_RAX;
                    cx->insts[idx].src = src0_r;

                    // Sign-extend to RDX
                    idx = x64_add_inst(cx, X64_MOV_IMM);
                    cx->insts[idx].dst = REG_RDX;
                    cx->insts[idx].imm = 0;
                    cx->insts[idx].has_imm = true;

                    // IDIV src1_r (divides RDX:RAX by src)
                    idx = x64_add_inst(cx, X64_IDIV);
                    cx->insts[idx].src = src1_r;

                    // Move result: RAX for DIV, RDX for MOD
                    idx = x64_add_inst(cx, X64_MOV);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].src = inst->op == IR_DIV ? REG_RAX : REG_RDX;

                    free_reg(cx, inst->operands[0].reg_id);
                    free_reg(cx, inst->operands[1].reg_id);
                    break;
                }

                case IR_NEG: {
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);
                    int src_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int idx = x64_add_inst(cx, X64_MOV);
                    cx->insts[idx].dst = dst_r; cx->insts[idx].src = src_r;
                    idx = x64_add_inst(cx, X64_NEG);
                    cx->insts[idx].dst = dst_r;
                    free_reg(cx, inst->operands[0].reg_id);
                    break;
                }

                case IR_NOT: {
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);
                    int src_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int idx = x64_add_inst(cx, X64_MOV);
                    cx->insts[idx].dst = dst_r; cx->insts[idx].src = src_r;
                    idx = x64_add_inst(cx, X64_NOT);
                    cx->insts[idx].dst = dst_r;
                    free_reg(cx, inst->operands[0].reg_id);
                    break;
                }

                case IR_EQ: case IR_NE: case IR_LT: case IR_GT:
                case IR_LE: case IR_GE: {
                    static const int cond_map[] = {
                        [IR_EQ]=0, [IR_NE]=1, [IR_LT]=2,
                        [IR_GT]=3, [IR_LE]=4, [IR_GE]=5
                    };
                    static const int setcc_map[] = { 0x94, 0x95, 0x9C, 0x9F, 0x9E, 0x9D };
                    // 0x94=sete, 0x95=setne, 0x9C=setl, 0x9F=setg, 0x9E=setle, 0x9D=setge
                    int idx;
                    int src0_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int src1_r = alloc_reg(cx, inst->operands[1].reg_id);
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);

                    // CMP src0, src1
                    idx = x64_add_inst(cx, X64_CMP);
                    cx->insts[idx].dst = src0_r;
                    cx->insts[idx].src = src1_r;

                    // SETcc dst (byte)
                    idx = x64_add_inst(cx, X64_SETcc);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].cond = setcc_map[cond_map[inst->op]];

                    // MOVZX dst, dst (zero-extend byte to qword)
                    idx = x64_add_inst(cx, X64_MOVZX);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].src = dst_r;

                    free_reg(cx, inst->operands[0].reg_id);
                    free_reg(cx, inst->operands[1].reg_id);
                    break;
                }

                case IR_ALLOCA: {
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);
                    // LEA dst, [RBP - stack_offset]
                    cx->stack_slots[inst->dst.reg_id] = cx->stack_size;
                    int idx = x64_add_inst(cx, X64_ALLOCA);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].stack_off = cx->stack_size;
                    cx->stack_size += 8;
                    break;
                }

                case IR_LOAD: {
                    int dst_r = alloc_reg(cx, inst->dst.reg_id);
                    int src_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int idx = x64_add_inst(cx, X64_LOAD);
                    cx->insts[idx].dst = dst_r;
                    cx->insts[idx].src = src_r;
                    free_reg(cx, inst->operands[0].reg_id);
                    break;
                }

                case IR_STORE: {
                    int src_r = alloc_reg(cx, inst->operands[1].reg_id);
                    int ptr_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int idx = x64_add_inst(cx, X64_STORE);
                    cx->insts[idx].dst = ptr_r;
                    cx->insts[idx].src = src_r;
                    free_reg(cx, inst->operands[0].reg_id);
                    free_reg(cx, inst->operands[1].reg_id);
                    break;
                }

                case IR_CALL: {
                    const char *fn_name = inst->operands[0].func_name;
                    int idx;

                    // Handle print specially
                    if (fn_name && strcmp(fn_name, "print") == 0) {
                        // On x86-64 we call printf with the value
                        int arg_r = alloc_reg(cx, inst->operands[1].reg_id);
                        if (num_arg_regs > 0) {
                            idx = x64_add_inst(cx, X64_MOV);
                            cx->insts[idx].dst = arg_regs[0];  // RDI/RCX
                            cx->insts[idx].src = arg_r;
                        }
                        idx = x64_add_inst(cx, X64_CALL);
                        cx->insts[idx].label = x64_label(cx);  // placeholder for printf
                        cx->insts[idx].has_imm = true;
                        cx->insts[idx].imm = (intptr_t)"printf";
                        free_reg(cx, inst->operands[1].reg_id);
                        break;
                    }

                    // Regular function call: set up args in registers
                    int arg_count = 0;
                    for (int i = 1; i < 4; i++) {
                        if (inst->operands[i].kind == IR_VAL_NONE) break;
                        int arg_r = alloc_reg(cx, inst->operands[i].reg_id);
                        if (arg_count < num_arg_regs) {
                            idx = x64_add_inst(cx, X64_MOV);
                            cx->insts[idx].dst = arg_regs[arg_count];
                            cx->insts[idx].src = arg_r;
                        }
                        free_reg(cx, inst->operands[i].reg_id);
                        arg_count++;
                    }

                    idx = x64_add_inst(cx, X64_CALL);
                    cx->insts[idx].has_imm = true;
                    cx->insts[idx].imm = fn_name ? (intptr_t)fn_name : (intptr_t)"unknown";
                    cx->insts[idx].dst = alloc_reg(cx, inst->dst.reg_id);
                    break;
                }

                case IR_RET: {
                    int idx;
                    if (inst->operands[0].kind != IR_VAL_NONE) {
                        int ret_r = alloc_reg(cx, inst->operands[0].reg_id);
                        // Move return value to RAX
                        idx = x64_add_inst(cx, X64_MOV);
                        cx->insts[idx].dst = REG_RAX;
                        cx->insts[idx].src = ret_r;
                        free_reg(cx, inst->operands[0].reg_id);
                    }
                    idx = x64_add_inst(cx, X64_RET);
                    break;
                }

                case IR_BR: {
                    if (inst->operands[0].kind == IR_VAL_BLOCK) {
                        int idx = x64_add_inst(cx, X64_JMP);
                        cx->insts[idx].label = inst->operands[0].block_id;
                    }
                    break;
                }

                case IR_BR_COND: {
                    int cond_r = alloc_reg(cx, inst->operands[0].reg_id);
                    int idx = x64_add_inst(cx, X64_CMP);
                    cx->insts[idx].dst = cond_r;
                    cx->insts[idx].imm = 0;
                    cx->insts[idx].has_imm = true;

                    // JNE to true branch (non-zero = true)
                    idx = x64_add_inst(cx, X64_Jcc);
                    cx->insts[idx].cond = 0x85;  // JNE
                    cx->insts[idx].label = inst->operands[1].block_id;

                    // JMP to false branch
                    idx = x64_add_inst(cx, X64_JMP);
                    cx->insts[idx].label = inst->operands[2].block_id;

                    free_reg(cx, inst->operands[0].reg_id);
                    break;
                }

                default:
                    break;
            }
            inst = inst->next;
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ASSEMBLY BACKEND — Intel-syntax text output (NASM-compatible)
// ═══════════════════════════════════════════════════════════════════════════════

static void asm_emit_inst(FILE *out, X64Context *cx, X64Inst *inst, int idx) {
    switch (inst->op) {
        case X64_LABEL:
            fprintf(out, ".L%d:\n", inst->label);
            break;
        case X64_NOP:
            break;
        case X64_MOV:
            fprintf(out, "    mov %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_MOV_IMM:
            fprintf(out, "    mov %s, %lld\n", reg_name(inst->dst), (long long)inst->imm);
            break;
        case X64_MOVZX:
            fprintf(out, "    movzx %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_ADD:
            fprintf(out, "    add %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_SUB:
            fprintf(out, "    sub %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_IMUL:
            fprintf(out, "    imul %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_IDIV:
            fprintf(out, "    idiv %s\n", reg_name(inst->src));
            break;
        case X64_AND:
            fprintf(out, "    and %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_OR:
            fprintf(out, "    or %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_XOR:
            fprintf(out, "    xor %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_NEG:
            fprintf(out, "    neg %s\n", reg_name(inst->dst));
            break;
        case X64_NOT:
            fprintf(out, "    not %s\n", reg_name(inst->dst));
            break;
        case X64_SHL:
            fprintf(out, "    shl %s, cl\n", reg_name(inst->dst));
            break;
        case X64_SHR:
            fprintf(out, "    shr %s, cl\n", reg_name(inst->dst));
            break;
        case X64_CMP:
            if (inst->has_imm)
                fprintf(out, "    cmp %s, %lld\n", reg_name(inst->dst), (long long)inst->imm);
            else
                fprintf(out, "    cmp %s, %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_SETcc: {
            static const char *setcc_names[] = {
                "sete", "setne", "setl", "setg", "setle", "setge"
            };
            int cc_idx = -1;
            for (int i = 0; i < 6; i++) {
                static const unsigned char cc[] = {0x94,0x95,0x9C,0x9F,0x9E,0x9D};
                if (inst->cond == cc[i]) { cc_idx = i; break; }
            }
            if (cc_idx >= 0)
                fprintf(out, "    %s %s\n", setcc_names[cc_idx], reg_name(inst->dst));
            break;
        }
        case X64_PUSH:
            fprintf(out, "    push %s\n", reg_name(inst->dst));
            break;
        case X64_POP:
            fprintf(out, "    pop %s\n", reg_name(inst->dst));
            break;
        case X64_JMP:
            fprintf(out, "    jmp .L%d\n", inst->label);
            break;
        case X64_Jcc: {
            static const char *jcc_names[] = { "je","jne","jl","jg","jle","jge" };
            if (inst->cond == 0x85) { fprintf(out, "    jne .L%d\n", inst->label); break; }
            int cc_idx = -1;
            for (int i = 0; i < 6; i++) {
                static const unsigned char cc[] = {0x84,0x85,0x8C,0x8F,0x8E,0x8D};
                if (inst->cond == cc[i]) { cc_idx = i; break; }
            }
            if (inst->cond == 0x85) break; // already handled
            if (cc_idx >= 0)
                fprintf(out, "    %s .L%d\n", jcc_names[cc_idx], inst->label);
            break;
        }
        case X64_CALL:
            if (inst->has_imm && inst->imm == (intptr_t)"printf") {
                fprintf(out, "    sub rsp, 32\n");
                fprintf(out, "    lea rcx, [rel .fmt_int]\n");
                // Arg is already in rcx/rdi from earlier mov
                fprintf(out, "    call printf\n");
                fprintf(out, "    add rsp, 32\n");
            } else {
                fprintf(out, "    call %s\n", (const char*)inst->imm);
            }
            break;
        case X64_RET:
            fprintf(out, "    ret\n");
            break;
        case X64_LEA:
            fprintf(out, "    lea %s, [rbp - %d]\n", reg_name(inst->dst), inst->stack_off + 8);
            break;
        case X64_ALLOCA:
            fprintf(out, "    lea %s, [rbp - %d]\n", reg_name(inst->dst), inst->stack_off + 8);
            break;
        case X64_LOAD:
            fprintf(out, "    mov %s, [%s]\n", reg_name(inst->dst), reg_name(inst->src));
            break;
        case X64_STORE:
            fprintf(out, "    mov [%s], %s\n", reg_name(inst->dst), reg_name(inst->src));
            break;
    }
}

int codegen_asm_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot open output: %s\n", path); return 1; }

    fprintf(f, "; Ketamine Language v%d.%d.%d — x86-64 Assembly (Intel/NASM syntax)\n",
            KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
    fprintf(f, "; Auto-generated. Assemble with: nasm -f win64 \"%s\" (Windows)\n", path);
    fprintf(f, ";                            nasm -f elf64 \"%s\" (Linux)\n", path);
#ifdef _WIN32
    fprintf(f, "default rel\n");
    fprintf(f, "extern printf\n");
    fprintf(f, "section .text\n\n");
#else
    fprintf(f, "default rel\n");
    fprintf(f, "extern printf\n");
    fprintf(f, "section .text\n\n");
#endif

    // Emit format string for printf
    fprintf(f, "section .data\n");
    fprintf(f, "  .fmt_int: db \"%%ld\", 10, 0\n");
    fprintf(f, "section .text\n\n");

    for (int f_idx = 0; f_idx < mod->func_count; f_idx++) {
        IRFunction *fn = mod->functions[f_idx];

        X64Context cx;
        memset(&cx, 0, sizeof(cx));
        cx.verbose = opts->verbose;
        select_instructions(&cx, fn);

        const char *fn_name = fn->name ? fn->name : "anon";

        // Function prologue
        fprintf(f, "global %s\n", fn_name);
        fprintf(f, "%s:\n", fn_name);
        fprintf(f, "    push rbp\n");
        fprintf(f, "    mov rbp, rsp\n");
        if (cx.stack_size > 0)
            fprintf(f, "    sub rsp, %d\n", cx.stack_size + 32);

        // Emit instructions
        for (int i = 0; i < cx.count; i++) {
            asm_emit_inst(f, &cx, &cx.insts[i], i);
        }

        // Function epilogue (if not already returned)
        // Check if last instruction is RET
        if (cx.count == 0 || cx.insts[cx.count-1].op != X64_RET) {
            fprintf(f, "    mov rsp, rbp\n");
            fprintf(f, "    pop rbp\n");
            fprintf(f, "    ret\n");
        }
        fprintf(f, "\n");
    }

    fclose(f);

    if (opts->verbose)
        fprintf(stderr, "ketc: Assembly -> %s (%d functions)\n", path, mod->func_count);

    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// NATIVE x86-64 BACKEND — Raw machine code emitter
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef _WIN32
    #pragma pack(push,1)
    typedef struct { uint16_t sig; uint16_t lastsec; uint16_t nsects; uint16_t opts; uint16_t flags; } COFF_HDR;
    typedef struct { char name[8]; uint32_t vsize; uint32_t vaddr; uint32_t datasize; uint32_t datapos; uint32_t relpos; uint32_t linepos; uint16_t nrelocs; uint16_t nlines; uint32_t flags; } COFF_SECT;
    #pragma pack(pop)
#endif

typedef struct {
    unsigned char *buf;
    int            len;
    int            cap;
    int            pos;      // write cursor for data section
    unsigned char *data_buf;
    int            data_len;
    int            data_cap;
    int            data_pos;
} ByteBuf;

static void byte_init(ByteBuf *bb) {
    bb->cap = 65536;
    bb->buf = (unsigned char*)malloc(bb->cap);
    bb->len = 0;
    bb->pos = 0;
    bb->data_cap = 65536;
    bb->data_buf = (unsigned char*)malloc(bb->data_cap);
    bb->data_len = 0;
}

static void byte_write(ByteBuf *bb, unsigned char b) {
    if (bb->len >= bb->cap) {
        bb->cap *= 2;
        bb->buf = (unsigned char*)realloc(bb->buf, bb->cap);
    }
    bb->buf[bb->len++] = b;
}

static void byte_write32(ByteBuf *bb, uint32_t v) {
    byte_write(bb, v & 0xFF);
    byte_write(bb, (v >> 8) & 0xFF);
    byte_write(bb, (v >> 16) & 0xFF);
    byte_write(bb, (v >> 24) & 0xFF);
}

static void byte_write64(ByteBuf *bb, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        byte_write(bb, (unsigned char)(v & 0xFF));
        v >>= 8;
    }
}

static void byte_patch32(ByteBuf *bb, int offset, uint32_t v) {
    if (offset + 4 > bb->len) return;
    bb->buf[offset]   = v & 0xFF;
    bb->buf[offset+1] = (v >> 8) & 0xFF;
    bb->buf[offset+2] = (v >> 16) & 0xFF;
    bb->buf[offset+3] = (v >> 24) & 0xFF;
}

// Encode ModRM byte: mod=0..3, reg=0..7, rm=0..7
static unsigned char modrm(int mod, int reg, int rm) {
    return (unsigned char)(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

// Encode REX prefix
static unsigned char rex(bool w, bool r, bool x, bool b) {
    return (unsigned char)(0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0));
}

static void emit_rex_w(ByteBuf *bb, int dst_reg, int src_reg) {
    bool need_rex = (dst_reg > 7 || src_reg > 7);
    if (need_rex)
        byte_write(bb, rex(true, dst_reg > 7, false, src_reg > 7));
}

static void emit_mov_reg_reg(ByteBuf *bb, int dst, int src) {
    // MOV r64, r/m64: 48 89 /r (dst = ModRM.reg, src = ModRM.rm)
    emit_rex_w(bb, dst, src);
    byte_write(bb, 0x89);
    byte_write(bb, modrm(3, dst & 7, src & 7));
}

static void emit_mov_reg_imm64(ByteBuf *bb, int reg, int64_t imm) {
    // MOV r64, imm64: 48 B8+reg
    if (reg > 7) byte_write(bb, rex(true, false, false, true));
    else         byte_write(bb, rex(true, false, false, false));
    byte_write(bb, (unsigned char)(0xB8 | (reg & 7)));
    byte_write64(bb, (uint64_t)imm);
}

static void emit_movzx_reg_reg(ByteBuf *bb, int dst, int src) {
    // MOVZX r64, r/m8: 48 0F B6 /r
    emit_rex_w(bb, dst, src);
    byte_write(bb, 0x0F);
    byte_write(bb, 0xB6);
    byte_write(bb, modrm(3, dst & 7, src & 7));
}

static void emit_arith(ByteBuf *bb, unsigned char opcode, int dst, int src) {
    // opcode r/m64, r64: 48 01-03 /r
    emit_rex_w(bb, dst, src);
    byte_write(bb, opcode);
    byte_write(bb, modrm(3, dst & 7, src & 7));
}

static void emit_neg(ByteBuf *bb, int reg) {
    emit_rex_w(bb, reg, 0);
    byte_write(bb, 0xF7);
    byte_write(bb, modrm(3, 3, reg & 7));  // /3
}

static void emit_not(ByteBuf *bb, int reg) {
    emit_rex_w(bb, reg, 0);
    byte_write(bb, 0xF7);
    byte_write(bb, modrm(3, 2, reg & 7));  // /2
}

static void emit_idiv(ByteBuf *bb, int reg) {
    emit_rex_w(bb, reg, 0);
    byte_write(bb, 0xF7);
    byte_write(bb, modrm(3, 7, reg & 7));  // /7
}

static void emit_push(ByteBuf *bb, int reg) {
    if (reg > 7) { byte_write(bb, 0x41); byte_write(bb, (unsigned char)(0x50 + (reg & 7))); }
    else         { byte_write(bb, (unsigned char)(0x50 + reg)); }
}

static void emit_pop(ByteBuf *bb, int reg) {
    if (reg > 7) { byte_write(bb, 0x41); byte_write(bb, (unsigned char)(0x58 + (reg & 7))); }
    else         { byte_write(bb, (unsigned char)(0x58 + reg)); }
}

static void emit_cmp_reg_reg(ByteBuf *bb, int dst, int src) {
    // CMP r64, r/m64: 48 39 /r
    emit_rex_w(bb, dst, src);
    byte_write(bb, 0x39);
    byte_write(bb, modrm(3, dst & 7, src & 7));
}

static void emit_cmp_reg_imm(ByteBuf *bb, int reg, int64_t imm) {
    // CMP r/m64, imm32: 48 83 /7 imm8  OR  48 81 /7 imm32
    if (imm >= -128 && imm <= 127) {
        emit_rex_w(bb, reg, 0);
        byte_write(bb, 0x83);
        byte_write(bb, modrm(3, 7, reg & 7));
        byte_write(bb, (unsigned char)(imm & 0xFF));
    } else {
        emit_rex_w(bb, reg, 0);
        byte_write(bb, 0x81);
        byte_write(bb, modrm(3, 7, reg & 7));
        byte_write32(bb, (uint32_t)(imm & 0xFFFFFFFF));
    }
}

static void emit_setcc(ByteBuf *bb, int reg, unsigned char cc) {
    // SETcc r/m8: 0F 9x /0
    if (reg > 7) byte_write(bb, rex(false, false, false, true));
    byte_write(bb, 0x0F);
    byte_write(bb, cc);
    byte_write(bb, modrm(3, 0, reg & 7));
}

static void emit_jmp_rel32(ByteBuf *bb, int *patch_offset) {
    // JMP rel32: E9 cd
    byte_write(bb, 0xE9);
    *patch_offset = bb->len;
    byte_write32(bb, 0);  // placeholder
}

static void emit_jcc_rel32(ByteBuf *bb, unsigned char cc, int *patch_offset) {
    // Jcc rel32: 0F 8x cd
    byte_write(bb, 0x0F);
    byte_write(bb, cc);
    *patch_offset = bb->len;
    byte_write32(bb, 0);  // placeholder
}

static void emit_call_rel32(ByteBuf *bb, int *patch_offset) {
    // CALL rel32: E8 cd
    byte_write(bb, 0xE8);
    *patch_offset = bb->len;
    byte_write32(bb, 0);  // placeholder
}

static void emit_ret(ByteBuf *bb) {
    byte_write(bb, 0xC3);
}

static void emit_lea_rbp_offset(ByteBuf *bb, int reg, int offset) {
    // LEA r64, [RBP + disp8/disp32]
    // REX.W + 8D /r
    emit_rex_w(bb, reg, 0);
    byte_write(bb, 0x8D);
    if (offset >= -128 && offset <= 127) {
        byte_write(bb, modrm(1, reg & 7, 5));  // [rbp + disp8]
        byte_write(bb, (unsigned char)(offset & 0xFF));
    } else {
        byte_write(bb, modrm(2, reg & 7, 5));  // [rbp + disp32]
        byte_write32(bb, (uint32_t)offset);
    }
}

static void emit_load(ByteBuf *bb, int dst, int src) {
    // MOV r64, [r64]: 48 8B /r
    emit_rex_w(bb, dst, src);
    byte_write(bb, 0x8B);
    byte_write(bb, modrm(0, dst & 7, src & 7));
}

static void emit_store(ByteBuf *bb, int dst, int src) {
    // MOV [r64], r64: 48 89 /r
    emit_rex_w(bb, src, dst);
    byte_write(bb, 0x89);
    byte_write(bb, modrm(0, src & 7, dst & 7));
}

// ─── Emit an entire function as raw x86-64 bytes ────────────────────────────
typedef struct {
    ByteBuf code;
    int     *label_offsets;   // label -> byte offset
    int     label_count;
    int     *patch_list;      // pairs of (byte_offset, label_id)
    int     patch_count;
    int     max_patches;
} NativeEmitter;

static void native_init(NativeEmitter *ne) {
    byte_init(&ne->code);
    ne->label_offsets = NULL;
    ne->label_count = 0;
    ne->patch_list = NULL;
    ne->patch_count = 0;
    ne->max_patches = 1024;
    ne->patch_list = (int*)malloc(ne->max_patches * 2 * sizeof(int));
}

static void native_set_label(NativeEmitter *ne, int label) {
    if (label >= ne->label_count) {
        int old = ne->label_count;
        ne->label_count = label + 1;
        ne->label_offsets = (int*)realloc(ne->label_offsets, ne->label_count * sizeof(int));
        for (int i = old; i < ne->label_count; i++) ne->label_offsets[i] = -1;
    }
    ne->label_offsets[label] = ne->code.len;
}

static void native_patch(NativeEmitter *ne, int offset, int label) {
    if (ne->patch_count >= ne->max_patches) {
        ne->max_patches *= 2;
        ne->patch_list = (int*)realloc(ne->patch_list, ne->max_patches * 2 * sizeof(int));
    }
    ne->patch_list[ne->patch_count * 2] = offset;
    ne->patch_list[ne->patch_count * 2 + 1] = label;
    ne->patch_count++;
}

static void native_resolve_patches(NativeEmitter *ne) {
    for (int i = 0; i < ne->patch_count; i++) {
        int offset = ne->patch_list[i * 2];
        int label = ne->patch_list[i * 2 + 1];
        if (label < ne->label_count && ne->label_offsets[label] >= 0) {
            int target = ne->label_offsets[label];
            int rel = target - (offset + 4);
            byte_patch32(&ne->code, offset, (uint32_t)(int32_t)rel);
        }
    }
}

static void native_free(NativeEmitter *ne) {
    free(ne->code.buf);
    free(ne->code.data_buf);
    free(ne->label_offsets);
    free(ne->patch_list);
}

static void native_emit_func(NativeEmitter *ne, X64Context *cx, IRFunction *fn) {
    // Prologue
    emit_push(&ne->code, REG_RBP);
    emit_mov_reg_reg(&ne->code, REG_RBP, REG_RSP);
    if (cx->stack_size > 0) {
        int stack_alloc = cx->stack_size + 32;
        if (stack_alloc > 127) {
            // sub rsp, imm32
            unsigned char rex_byte = rex(true, false, false, false);
            byte_write(&ne->code, rex_byte);
            byte_write(&ne->code, 0x81);
            byte_write(&ne->code, modrm(3, 5, 4));  // sub /5, rm=RSP
            byte_write32(&ne->code, (uint32_t)stack_alloc);
        } else {
            byte_write(&ne->code, rex(true, false, false, false));
            byte_write(&ne->code, 0x83);
            byte_write(&ne->code, modrm(3, 5, 4));
            byte_write(&ne->code, (unsigned char)stack_alloc);
        }
    }

    // Pre-scan for labels
    for (int i = 0; i < cx->count; i++) {
        if (cx->insts[i].op == X64_LABEL)
            native_set_label(ne, cx->insts[i].label);
    }

    // Emit instructions
    for (int i = 0; i < cx->count; i++) {
        X64Inst *inst = &cx->insts[i];
        switch (inst->op) {
            case X64_NOP:
                break;
            case X64_MOV:
                emit_mov_reg_reg(&ne->code, inst->dst, inst->src);
                break;
            case X64_MOV_IMM:
                emit_mov_reg_imm64(&ne->code, inst->dst, inst->imm);
                break;
            case X64_MOVZX:
                emit_movzx_reg_reg(&ne->code, inst->dst, inst->src);
                break;
            case X64_ADD:  emit_arith(&ne->code, 0x01, inst->dst, inst->src); break;
            case X64_SUB:  emit_arith(&ne->code, 0x29, inst->dst, inst->src); break;
            case X64_IMUL: {
                // IMUL r64, r/m64: 48 0F AF /r
                emit_rex_w(&ne->code, inst->dst, inst->src);
                byte_write(&ne->code, 0x0F);
                byte_write(&ne->code, 0xAF);
                byte_write(&ne->code, modrm(3, inst->dst & 7, inst->src & 7));
                break;
            }
            case X64_IDIV:
                emit_idiv(&ne->code, inst->src);
                break;
            case X64_AND:  emit_arith(&ne->code, 0x21, inst->dst, inst->src); break;
            case X64_OR:   emit_arith(&ne->code, 0x09, inst->dst, inst->src); break;
            case X64_XOR:  emit_arith(&ne->code, 0x31, inst->dst, inst->src); break;
            case X64_NEG:  emit_neg(&ne->code, inst->dst); break;
            case X64_NOT:  emit_not(&ne->code, inst->dst); break;
            case X64_CMP:
                if (inst->has_imm) emit_cmp_reg_imm(&ne->code, inst->dst, inst->imm);
                else emit_cmp_reg_reg(&ne->code, inst->dst, inst->src);
                break;
            case X64_SETcc:
                emit_setcc(&ne->code, inst->dst, (unsigned char)inst->cond);
                break;
            case X64_PUSH:
                emit_push(&ne->code, inst->dst);
                break;
            case X64_POP:
                emit_pop(&ne->code, inst->dst);
                break;
            case X64_JMP: {
                int patch;
                emit_jmp_rel32(&ne->code, &patch);
                native_patch(ne, patch, inst->label);
                break;
            }
            case X64_Jcc: {
                int patch;
                emit_jcc_rel32(&ne->code, (unsigned char)inst->cond, &patch);
                native_patch(ne, patch, inst->label);
                break;
            }
            case X64_CALL: {
                if (inst->has_imm && inst->imm == (intptr_t)"printf") {
                    // Emit call to printf (needs external relocation)
                    // For now, emit as a relative call that needs to be patched
                    int patch;
                    emit_call_rel32(&ne->code, &patch);
                    native_patch(ne, patch, -1);  // external call
                } else {
                    // Call by name — relative call
                    int patch;
                    emit_call_rel32(&ne->code, &patch);
                    native_patch(ne, patch, -2);  // function call by name
                }
                break;
            }
            case X64_RET:
                emit_ret(&ne->code);
                break;
            case X64_ALLOCA:
                emit_lea_rbp_offset(&ne->code, inst->dst, -(inst->stack_off + 8));
                break;
            case X64_LOAD:
                emit_load(&ne->code, inst->dst, inst->src);
                break;
            case X64_STORE:
                emit_store(&ne->code, inst->dst, inst->src);
                break;
            default:
                break;
        }
    }

    // Epilogue (if no ret at end)
    if (cx->count == 0 || cx->insts[cx->count-1].op != X64_RET) {
        emit_mov_reg_reg(&ne->code, REG_RSP, REG_RBP);
        emit_pop(&ne->code, REG_RBP);
        emit_ret(&ne->code);
    }

    native_resolve_patches(ne);
}

#ifdef _WIN32
// Write a minimal COFF object file
static int write_coff(const char *path, NativeEmitter *ne, const char *fn_name) {
    FILE *f = fopen(path, "wb");
    if (!f) return 1;

    // COFF header
    unsigned char hdr[20] = {0};
    // Machine: x86-64 (0x8664)
    hdr[0] = 0x64; hdr[1] = 0x86;
    // Number of sections
    hdr[2] = 1; hdr[3] = 0;
    // Time stamp
    // ... (simplified)
    fwrite(hdr, 20, 1, f);

    // Section table
    fclose(f);
    return 0;
}
#endif

int codegen_native_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "Cannot open output: %s\n", path); return 1; }

    int num_funcs = 0;
    for (int i = 0; i < mod->func_count; i++) {
        if (mod->functions[i]->name) num_funcs++;
    }

    // Header: magic + num_funcs
    fwrite("KEXE", 1, 4, f);
    uint32_t nf = (uint32_t)num_funcs;
    fwrite(&nf, 4, 1, f);

    // Function table offset placeholder
    uint32_t table_off = 0;
    fwrite(&table_off, 4, 1, f);

    // Reserve space for function table (we'll fill it after emitting code)
    long table_start = ftell(f);
    // Each entry: name_len(4) + name(name_len) + code_off(4) + code_len(4)
    long *entry_pos = (long*)calloc(num_funcs, sizeof(long));
    int func_idx = 0;

    for (int i = 0; i < mod->func_count; i++) {
        IRFunction *fn = mod->functions[i];
        if (!fn->name) continue;

        // Write placeholder entry
        entry_pos[func_idx] = ftell(f);
        uint32_t name_len = (uint32_t)strlen(fn->name);
        fwrite(&name_len, 4, 1, f);
        fwrite(fn->name, 1, name_len, f);
        uint32_t code_off = 0, code_len = 0;
        fwrite(&code_off, 4, 1, f);
        fwrite(&code_len, 4, 1, f);
        func_idx++;
    }

    // Emit function code
    func_idx = 0;
    for (int i = 0; i < mod->func_count; i++) {
        IRFunction *fn = mod->functions[i];
        if (!fn->name) continue;

        X64Context cx;
        memset(&cx, 0, sizeof(cx));
        cx.verbose = opts->verbose;

        int param_idx = 0;
        for (int j = 0; param_idx < fn->var_arg_count && j < num_arg_regs; j++) {
            cx.reg_used[arg_regs[j]] = true;
            cx.reg_map[param_idx] = arg_regs[j];
            cx.reg_rev[arg_regs[j]] = param_idx;
            param_idx++;
        }

        select_instructions(&cx, fn);

        NativeEmitter ne;
        native_init(&ne);
        native_emit_func(&ne, &cx, fn);

        // Record code position and size
        uint32_t code_off = (uint32_t)ftell(f);
        uint32_t code_len = (uint32_t)ne.code.len;

        fwrite(ne.code.buf, 1, ne.code.len, f);

        // Patch entry
        long pos = entry_pos[func_idx] + 4 + name_len;  // skip name_len + name
        fseek(f, pos, SEEK_SET);
        fwrite(&code_off, 4, 1, f);
        fwrite(&code_len, 4, 1, f);
        fseek(f, 0, SEEK_END);

        native_free(&ne);
        func_idx++;
    }

    // Patch table offset
    fseek(f, 8, SEEK_SET);
    uint32_t tbl_off = (uint32_t)table_start;
    fwrite(&tbl_off, 4, 1, f);

    free(entry_pos);
    fclose(f);

    if (opts->verbose)
        fprintf(stderr, "ketc: Native x86-64 -> %s (%d functions)\n", path, mod->func_count);

    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// JIT COMPILATION — Compile IR to executable memory at runtime
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#define MAX_JIT_FUNCS 256

typedef struct {
    const char *name;
    void       *code;
    int         size;
} JITEntry;

static JITEntry jit_table[MAX_JIT_FUNCS];
static int jit_count = 0;

// Allocate executable memory
static void *jit_alloc_exec(int size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}

// Make memory executable
static bool jit_make_exec(void *ptr, int size) {
#ifdef _WIN32
    DWORD old;
    return VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &old) != 0;
#else
    return mprotect(ptr, size, PROT_READ | PROT_EXEC) == 0;
#endif
}

static void jit_free_exec(void *ptr, int size) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

int jit_compile_module(IRModule *mod, CompileOptions *opts) {
    jit_count = 0;

    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];
        if (!fn->name) continue;
        if (jit_count >= MAX_JIT_FUNCS) break;

        // Select x86-64 instructions
        X64Context cx;
        memset(&cx, 0, sizeof(cx));
        cx.verbose = opts->verbose;

        int param_idx = 0;
        for (int i = 0; param_idx < fn->var_arg_count && i < num_arg_regs; i++) {
            cx.reg_used[arg_regs[i]] = true;
            cx.reg_map[param_idx] = arg_regs[i];
            cx.reg_rev[arg_regs[i]] = param_idx;
            param_idx++;
        }

        select_instructions(&cx, fn);

        // Emit machine code
        NativeEmitter ne;
        native_init(&ne);
        native_emit_func(&ne, &cx, fn);

        // Allocate executable memory
        void *exec = jit_alloc_exec(ne.code.len + 64);
        if (!exec) {
            native_free(&ne);
            continue;
        }

        // Copy code
        memcpy(exec, ne.code.buf, ne.code.len);
        // Fill rest with INT3 (0xCC)
        memset((char*)exec + ne.code.len, 0xCC, 64);

        // Make executable
        if (!jit_make_exec(exec, ne.code.len + 64)) {
            jit_free_exec(exec, ne.code.len + 64);
            native_free(&ne);
            continue;
        }

        // Add to table
        jit_table[jit_count].name = fn->name;
        jit_table[jit_count].code = exec;
        jit_table[jit_count].size = ne.code.len;
        jit_count++;

        if (opts->verbose)
            fprintf(stderr, "ketc: JIT compiled '%s' -> %p (%d bytes)\n",
                    fn->name, exec, ne.code.len);

        native_free(&ne);
    }

    return 0;
}

JITFunc jit_get_function(const char *name) {
    for (int i = 0; i < jit_count; i++) {
        if (strcmp(jit_table[i].name, name) == 0)
            return (JITFunc)jit_table[i].code;
    }
    return NULL;
}

void jit_free(void) {
    for (int i = 0; i < jit_count; i++) {
        jit_free_exec(jit_table[i].code, jit_table[i].size);
        jit_table[i].name = NULL;
        jit_table[i].code = NULL;
        jit_table[i].size = 0;
    }
    jit_count = 0;
}
