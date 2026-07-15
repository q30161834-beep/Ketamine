#include "include/codegen.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// ═══════════════════════════════════════════════════════════════════════════════
// CODEGEN — LLVM IR and Go backend code generation
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Shared: type name utilities ──────────────────────────────────────────────

const char *codegen_type_str(struct Type *type) {
    if (!type) return "void";
    switch (type->kind) {
        case TY_NEVER: return "never";
        case TY_VOID:  return "void";
        case TY_BOOL:  return "bool";
        case TY_BYTE:  return "i8";
        case TY_CHAR:  return "i32";
        case TY_I8:    return "i8";
        case TY_I16:   return "i16";
        case TY_I32:   return "i32";
        case TY_I64:   return "i64";
        case TY_I128:  return "i128";
        case TY_U8:    return "i8";
        case TY_U16:   return "i16";
        case TY_U32:   return "i32";
        case TY_U64:   return "i64";
        case TY_U128:  return "i128";
        case TY_F32:   return "float";
        case TY_F64:   return "double";
        case TY_STR:   return "i8*";
        case TY_PTR:   return "i8*";
        case TY_REF:   return "i8*";
        case TY_ARRAY: return "array";
        case TY_STRUCT: return type->struct_type.name ? type->struct_type.name : "struct";
        case TY_ENUM:   return type->enum_type.name ? type->enum_type.name : "enum";
        case TY_FN:    return "ptr";
        default:       return "unknown";
    }
}

const char *codegen_llvm_type_str(struct Type *type) {
    if (!type) return "void";
    switch (type->kind) {
        case TY_VOID:  return "void";
        case TY_BOOL:  return "i1";
        case TY_BYTE:  return "i8";
        case TY_CHAR:  return "i32";
        case TY_I8:    return "i8";
        case TY_I16:   return "i16";
        case TY_I32:   return "i32";
        case TY_I64:   return "i64";
        case TY_U8:    return "i8";
        case TY_U16:   return "i16";
        case TY_U32:   return "i32";
        case TY_U64:   return "i64";
        case TY_F32:   return "float";
        case TY_F64:   return "double";
        case TY_STR:   return "i8*";
        case TY_PTR:
        case TY_REF:   return "i8*";
        case TY_ARRAY: {
            if (type->array.elem && type->array.count > 0) {
                static char buf[64];
                snprintf(buf, sizeof(buf), "[%lld x %s]",
                         (long long)type->array.count,
                         codegen_llvm_type_str(type->array.elem));
                return buf;
            }
            return "i8*";
        }
        case TY_FN:    return "i8*";
        case TY_STRUCT: return type->struct_type.name ? type->struct_type.name : "%struct";
        default:       return "i64";
    }
}

const char *codegen_go_type_str(struct Type *type) {
    if (!type) return "void";
    switch (type->kind) {
        case TY_VOID:  return "";
        case TY_BOOL:  return "bool";
        case TY_BYTE:  return "byte";
        case TY_CHAR:  return "rune";
        case TY_I8:    return "int8";
        case TY_I16:   return "int16";
        case TY_I32:   return "int";
        case TY_I64:   return "int64";
        case TY_U8:    return "uint8";
        case TY_U16:   return "uint16";
        case TY_U32:   return "uint";
        case TY_U64:   return "uint64";
        case TY_F32:   return "float32";
        case TY_F64:   return "float64";
        case TY_STR:   return "string";
        case TY_PTR:   return "unsafe.Pointer";
        case TY_REF:   return "*" + (type->ptr.pointee ? codegen_go_type_str(type->ptr.pointee) : "byte");
        case TY_ARRAY: {
            if (type->array.elem && type->array.count > 0) {
                static char buf[64];
                snprintf(buf, sizeof(buf), "[%lld]%s",
                         (long long)type->array.count,
                         codegen_go_type_str(type->array.elem));
                return buf;
            }
            return "[]byte";
        }
        case TY_FN:    return "func";
        case TY_STRUCT: return type->struct_type.name ? type->struct_type.name : "struct{}";
        case TY_ENUM:   return type->enum_type.name ? type->enum_type.name : "interface{}";
        default:       return "int";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// LLVM IR CODEGEN
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
    FILE    *out;
    int      tmp;           // SSA temp counter
    int      str_count;     // global string constants
    int      label_count;   // basic block labels
    bool     had_error;
    IRModule *mod;
} LLVMGen;

static int llvm_tmp(LLVMGen *g) { return g->tmp++; }
static int llvm_label(LLVMGen *g) { return g->label_count++; }

static void llvm_emit(LLVMGen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->out, fmt, ap);
    va_end(ap);
}

static void llvm_emit_type(LLVMGen *g, Type *type) {
    fprintf(g->out, "%s", codegen_llvm_type_str(type));
}

static void llvm_emit_value(LLVMGen *g, IRValue *v) {
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            fprintf(g->out, "%lld", (long long)v->const_int);
            break;
        case IR_VAL_CONST_FLOAT:
            fprintf(g->out, "%f", v->const_float);
            break;
        case IR_VAL_CONST_BOOL:
            fprintf(g->out, "%s", v->const_bool ? "true" : "false");
            break;
        case IR_VAL_REG:
            fprintf(g->out, "%%t%d", v->reg_id);
            break;
        case IR_VAL_GLOBAL:
            fprintf(g->out, "@%s", v->global_name);
            break;
        case IR_VAL_FUNCTION:
            fprintf(g->out, "@%s", v->func_name);
            break;
        case IR_VAL_BLOCK:
            fprintf(g->out, "%%b%d", v->block_id);
            break;
        default:
            fprintf(g->out, "undef");
    }
}

static void llvm_gen_inst(LLVMGen *g, IRBlock *block, IRInst *inst) {
    const char *ty = codegen_llvm_type_str(inst->type);

    switch (inst->op) {
        case IR_NONE: {
            // Constant or already folded — emit as constant
            if (inst->dst.kind == IR_VAL_CONST_INT) {
                llvm_emit(g, "  %%t%d = add i64 0, %lld\n",
                          inst->dst.reg_id, (long long)inst->dst.const_int);
            }
            break;
        }
        case IR_ADD:
            llvm_emit(g, "  %%t%d = add %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_SUB:
            llvm_emit(g, "  %%t%d = sub %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_MUL:
            llvm_emit(g, "  %%t%d = mul %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_DIV:
            llvm_emit(g, "  %%t%d = sdiv %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_MOD:
            llvm_emit(g, "  %%t%d = srem %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_AND:
            llvm_emit(g, "  %%t%d = and %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_OR:
            llvm_emit(g, "  %%t%d = or %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_XOR:
            llvm_emit(g, "  %%t%d = xor %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_SHL:
            llvm_emit(g, "  %%t%d = shl %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_SHR:
            llvm_emit(g, "  %%t%d = ashr %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_NEG:
            llvm_emit(g, "  %%t%d = sub %s 0, ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, "\n");
            break;
        case IR_NOT:
            llvm_emit(g, "  %%t%d = xor %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", 1\n");
            break;
        case IR_EQ:
            llvm_emit(g, "  %%t%d = icmp eq %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_NE:
            llvm_emit(g, "  %%t%d = icmp ne %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_LT:
            llvm_emit(g, "  %%t%d = icmp slt %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_GT:
            llvm_emit(g, "  %%t%d = icmp sgt %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_LE:
            llvm_emit(g, "  %%t%d = icmp sle %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_GE:
            llvm_emit(g, "  %%t%d = icmp sge %s ", inst->dst.reg_id, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", ");
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, "\n");
            break;
        case IR_ALLOCA:
            llvm_emit(g, "  %%t%d = alloca %s\n", inst->dst.reg_id, ty);
            break;
        case IR_LOAD:
            llvm_emit(g, "  %%t%d = load %s, %s* ", inst->dst.reg_id, ty, ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, "\n");
            break;
        case IR_STORE:
            llvm_emit(g, "  store %s ", ty);
            llvm_emit_value(g, &inst->operands[1]);
            llvm_emit(g, ", %s* ", ty);
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, "\n");
            break;
        case IR_CALL: {
            bool is_print = inst->operands[0].func_name &&
                strcmp(inst->operands[0].func_name, "print") == 0;

            if (is_print) {
                // Check argument type
                IRValue *arg = &inst->operands[1];
                if (arg->kind == IR_VAL_CONST_INT || arg->kind == IR_VAL_REG) {
                    llvm_emit(g, "  call i32 (i8*, ...) @printf(i8* @.fmt_int, i64 ");
                    llvm_emit_value(g, arg);
                    llvm_emit(g, ")\n");
                } else if (arg->kind == IR_VAL_GLOBAL) {
                    llvm_emit(g, "  call i32 @puts(i8* ");
                    llvm_emit_value(g, arg);
                    llvm_emit(g, ")\n");
                } else {
                    llvm_emit(g, "  call i32 @puts(i8* ");
                    llvm_emit_value(g, arg);
                    llvm_emit(g, ")\n");
                }
            } else {
                const char *fn_name = inst->operands[0].func_name ?
                    inst->operands[0].func_name : "unknown";
                llvm_emit(g, "  %%t%d = call %s @%s(", inst->dst.reg_id, ty, fn_name);
                for (int i = 1; i < 4; i++) {
                    if (inst->operands[i].kind == IR_VAL_NONE) break;
                    if (i > 1) llvm_emit(g, ", ");
                    llvm_emit(g, "i64 ");
                    llvm_emit_value(g, &inst->operands[i]);
                }
                llvm_emit(g, ")\n");
            }
            break;
        }
        case IR_RET: {
            if (inst->operands[0].kind != IR_VAL_NONE) {
                llvm_emit(g, "  ret %s ", ty);
                llvm_emit_value(g, &inst->operands[0]);
                llvm_emit(g, "\n");
            } else {
                llvm_emit(g, "  ret void\n");
            }
            break;
        }
        case IR_BR: {
            if (inst->operands[0].kind == IR_VAL_BLOCK) {
                llvm_emit(g, "  br label %%b%d\n", inst->operands[0].block_id);
            }
            break;
        }
        case IR_BR_COND: {
            llvm_emit(g, "  br i1 ");
            llvm_emit_value(g, &inst->operands[0]);
            llvm_emit(g, ", label %%b%d, label %%b%d\n",
                      inst->operands[1].block_id,
                      inst->operands[2].block_id);
            break;
        }
        case IR_UNREACHABLE:
            llvm_emit(g, "  unreachable\n");
            break;
        case IR_PHI:
            // Simplified: emit placeholder
            llvm_emit(g, "  %%t%d = phi %s\n", inst->dst.reg_id, ty);
            break;
        default:
            llvm_emit(g, "  ; unhandled op: %d\n", inst->op);
            break;
    }
}

static void llvm_gen_function(LLVMGen *g, IRFunction *fn) {
    const char *ret_ty = fn->type ? codegen_llvm_type_str(fn->type) : "i64";
    const char *fn_name = fn->name ? fn->name : "anon";

    llvm_emit(g, "\ndefine %s @%s(", ret_ty, fn_name);
    for (int i = 0; i < fn->var_arg_count; i++) {
        if (i > 0) llvm_emit(g, ", ");
        llvm_emit(g, "i64");
    }
    llvm_emit(g, ") {\n");

    // Emit each block
    g->tmp = 0;
    for (int b = 0; b < fn->block_count; b++) {
        IRBlock *block = fn->blocks[b];
        llvm_emit(g, "b%d:\n", block->id);

        IRInst *inst = block->first;
        while (inst) {
            llvm_gen_inst(g, block, inst);
            inst = inst->next;
        }
    }

    llvm_emit(g, "}\n");
}

int codegen_llvm_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Cannot open output: %s\n", path);
        return 1;
    }

    LLVMGen g;
    g.out = f;
    g.tmp = 0;
    g.str_count = 0;
    g.label_count = 0;
    g.had_error = false;
    g.mod = mod;

    // Header
    llvm_emit(&g, "; Ketamine Language v%d.%d.%d — LLVM IR\n",
              KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
    llvm_emit(&g, "target triple = \"x86_64-pc-linux-gnu\"\n");
    llvm_emit(&g, "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n\n");

    // Declare runtime functions
    llvm_emit(&g, "declare i32 @puts(i8*)\n");
    llvm_emit(&g, "declare i32 @printf(i8*, ...)\n");
    llvm_emit(&g, "declare i8* @malloc(i64)\n");
    llvm_emit(&g, "declare void @free(i8*)\n");
    llvm_emit(&g, "declare i8* @memcpy(i8*, i8*, i64)\n");
    llvm_emit(&g, "declare i8* @memset(i8*, i32, i64)\n");
    llvm_emit(&g, "@.fmt_int = private constant [4 x i8] c\"%%ld\\0A\\00\"\n");
    llvm_emit(&g, "@.fmt_str = private constant [4 x i8] c\"%%s\\0A\\00\"\n\n");

    // Emit all functions
    for (int f = 0; f < mod->func_count; f++) {
        llvm_gen_function(&g, mod->functions[f]);
    }

    fclose(f);
    return g.had_error ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GO CODEGEN
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
    FILE    *out;
    int      indent;
    bool     had_error;
    IRModule *mod;
} GoGen;

static void go_indent(GoGen *g) {
    for (int i = 0; i < g->indent; i++) fputs("    ", g->out);
}

static void go_emit(GoGen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->out, fmt, ap);
    va_end(ap);
}

static void go_block(GoGen *g, IRBlock *block);

static void go_gen_inst(GoGen *g, IRInst *inst) {
    switch (inst->op) {
        case IR_RET: {
            go_indent(g);
            if (inst->operands[0].kind != IR_VAL_NONE) {
                go_emit(g, "return\n");
            } else {
                go_emit(g, "return\n");
            }
            break;
        }
        case IR_CALL: {
            const char *fn = inst->operands[0].func_name ? inst->operands[0].func_name : "";
            if (strcmp(fn, "print") == 0) {
                go_indent(g);
                go_emit(g, "fmt.Println(");
                // Print argument
                IRValue *arg = &inst->operands[1];
                switch (arg->kind) {
                    case IR_VAL_CONST_INT:
                        go_emit(g, "%lld", (long long)arg->const_int);
                        break;
                    case IR_VAL_GLOBAL:
                        go_emit(g, "\"%s\"", arg->global_name);
                        break;
                    default:
                        go_emit(g, "val");
                }
                go_emit(g, ")\n");
            }
            break;
        }
        case IR_ALLOCA:
            // Variable declaration
            go_indent(g);
            go_emit(g, "var x%d %s\n", inst->dst.reg_id, "int");
            break;
        default:
            break;
    }
}

static void go_block(GoGen *g, IRBlock *block) {
    IRInst *inst = block->first;
    while (inst) {
        go_gen_inst(g, inst);
        inst = inst->next;
    }
}

int codegen_go_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Cannot open output: %s\n", path);
        return 1;
    }

    GoGen g;
    g.out = f;
    g.indent = 0;
    g.had_error = false;
    g.mod = mod;

    // Package declaration
    go_emit(&g, "package main\n\n");
    go_emit(&g, "import (\n");
    go_emit(&g, "    \"fmt\"\n");
    go_emit(&g, "    \"os\"\n");
    go_emit(&g, "    \"sync\"\n");
    go_emit(&g, "    \"time\"\n");
    go_emit(&g, "    \"log\"\n");
    go_emit(&g, "    \"math\"\n");
    go_emit(&g, "    \"crypto/rand\"\n");
    go_emit(&g, "    \"encoding/json\"\n");
    go_emit(&g, "    \"net/http\"\n");
    go_emit(&g, "    \"golang.org/x/net/websocket\"\n");
    go_emit(&g, ")\n\n");

    // Emit functions
    for (int f = 0; f < mod->func_count; f++) {
        IRFunction *fn = mod->functions[f];
        const char *fn_name = fn->name ? fn->name : "anon";

        go_emit(&g, "func %s(", fn_name);
        for (int i = 0; i < fn->var_arg_count; i++) {
            if (i > 0) go_emit(&g, ", ");
            go_emit(&g, "arg%d int", i);
        }
        go_emit(&g, ") {\n");
        g.indent = 1;

        go_block(&g, fn->entry);

        g.indent = 0;
        go_emit(&g, "}\n\n");
    }

    fclose(f);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// WASM CODEGEN — WebAssembly Text Format (WAT)
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
    FILE    *out;
    int      tmp;
    int      local_count;
    int      func_idx;
    bool     had_error;
    IRModule *mod;
} WasmGen;

static int wasm_tmp(WasmGen *g) { return g->tmp++; }

static void wasm_emit(WasmGen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->out, fmt, ap);
    va_end(ap);
}

static const char *wasm_type_str(struct Type *type) {
    if (!type) return "i64";
    switch (type->kind) {
        case TY_VOID:  return "void";  // No return
        case TY_BOOL:  return "i32";
        case TY_BYTE:  return "i32";
        case TY_CHAR:  return "i32";
        case TY_I8:    return "i32";
        case TY_I16:   return "i32";
        case TY_I32:   return "i32";
        case TY_I64:   return "i64";
        case TY_U8:    return "i32";
        case TY_U16:   return "i32";
        case TY_U32:   return "i32";
        case TY_U64:   return "i64";
        case TY_F32:   return "f32";
        case TY_F64:   return "f64";
        case TY_STR:   return "i32";    // pointer
        case TY_PTR:
        case TY_REF:   return "i32";    // pointer
        case TY_FN:    return "i32";
        default:       return "i64";
    }
}

static void wasm_emit_value(WasmGen *g, IRValue *v) {
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            if (v->const_int >= 0 && v->const_int <= 0x7FFFFFFF) {
                wasm_emit(g, "i64.const %lld", (long long)v->const_int);
            } else {
                wasm_emit(g, "i64.const %lld", (long long)v->const_int);
            }
            break;
        case IR_VAL_CONST_FLOAT:
            wasm_emit(g, "f64.const %f", v->const_float);
            break;
        case IR_VAL_CONST_BOOL:
            wasm_emit(g, "i32.const %d", v->const_bool ? 1 : 0);
            break;
        case IR_VAL_REG:
            wasm_emit(g, "local.get $t%d", v->reg_id);
            break;
        case IR_VAL_GLOBAL:
        case IR_VAL_FUNCTION:
            wasm_emit(g, "global.get $%s", v->func_name ? v->func_name : v->global_name);
            break;
        default:
            wasm_emit(g, "i64.const 0");
    }
}

static void wasm_gen_inst(WasmGen *g, IRBlock *block, IRInst *inst) {
    const char *ws = wasm_type_str(inst->type);
    int is_float = (inst->type && (inst->type->kind == TY_F32 || inst->type->kind == TY_F64));

    (void)block;
    (void)ws;

    // Determine WASM op prefix
    const char *p = "i64";
    if (is_float) p = (inst->type->kind == TY_F32) ? "f32" : "f64";
    else if (inst->type) {
        switch (inst->type->kind) {
            case TY_BOOL: case TY_BYTE: case TY_CHAR:
            case TY_I8: case TY_I16: case TY_I32:
            case TY_U8: case TY_U16: case TY_U32:
                p = "i32";
                break;
            case TY_I64: case TY_U64: p = "i64"; break;
            case TY_F32: p = "f32"; break;
            case TY_F64: p = "f64"; break;
            default: break;
        }
    }

    switch (inst->op) {
        case IR_NONE:
            break;

        case IR_ADD:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.add\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_SUB:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.sub\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_MUL:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.mul\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_DIV:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            if (is_float)
                wasm_emit(g, "    %s.div\n", p);
            else if (p[0] == 'i')
                wasm_emit(g, "    %s.div_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_MOD:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.rem_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_AND:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.and\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_OR:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.or\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_XOR:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.xor\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_SHL:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.shl\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_SHR:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.shr_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_NEG:
            if (is_float) {
                wasm_emit(g, "    f64.const -0.0\n");
                wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
                wasm_emit(g, "    f64.neg\n");
            } else {
                wasm_emit(g, "    i64.const 0\n");
                wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
                wasm_emit(g, "    %s.sub\n", p);
            }
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_NOT:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    i32.eqz\n");
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        // Comparisons
        case IR_EQ:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.eq\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_NE:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.ne\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_LT:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            if (is_float)
                wasm_emit(g, "    %s.lt\n", p);
            else
                wasm_emit(g, "    %s.lt_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_GT:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            if (is_float)
                wasm_emit(g, "    %s.gt\n", p);
            else
                wasm_emit(g, "    %s.gt_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_LE:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            if (is_float)
                wasm_emit(g, "    %s.le\n", p);
            else
                wasm_emit(g, "    %s.le_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_GE:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            if (is_float)
                wasm_emit(g, "    %s.ge\n", p);
            else
                wasm_emit(g, "    %s.ge_s\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_ALLOCA:
            // Allocate on local stack
            wasm_emit(g, "    i64.const 0\n");
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_LOAD:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    %s.load\n", p);
            wasm_emit(g, "    local.set $t%d\n", inst->dst.reg_id);
            break;

        case IR_STORE:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    local.get $t%d\n", inst->operands[1].reg_id);
            wasm_emit(g, "    %s.store\n", p);
            break;

        case IR_CALL: {
            const char *fn = inst->operands[0].func_name ?
                inst->operands[0].func_name : "unknown";

            if (strcmp(fn, "print") == 0) {
                // Call import: env.print_i32 / env.print_f64
                IRValue *arg = &inst->operands[1];
                wasm_emit(g, "    local.get $t%d\n", arg->reg_id);
                wasm_emit(g, "    call $print_i64\n");
            } else {
                wasm_emit(g, "    call $%s\n", fn);
            }
            break;
        }

        case IR_RET: {
            if (inst->operands[0].kind != IR_VAL_NONE) {
                wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
                wasm_emit(g, "    return\n");
            } else {
                wasm_emit(g, "    return\n");
            }
            break;
        }

        case IR_BR:
            if (inst->operands[0].kind == IR_VAL_BLOCK) {
                wasm_emit(g, "    br $b%d\n", inst->operands[0].block_id);
            }
            break;

        case IR_BR_COND:
            wasm_emit(g, "    local.get $t%d\n", inst->operands[0].reg_id);
            wasm_emit(g, "    if\n");
            wasm_emit(g, "      br $b%d\n", inst->operands[1].block_id);
            wasm_emit(g, "    else\n");
            wasm_emit(g, "      br $b%d\n", inst->operands[2].block_id);
            wasm_emit(g, "    end\n");
            break;

        case IR_UNREACHABLE:
            wasm_emit(g, "    unreachable\n");
            break;

        default:
            wasm_emit(g, "    ;; unhandled op: %d\n", inst->op);
            break;
    }
}

static void wasm_gen_function(WasmGen *g, IRFunction *fn) {
    const char *fn_name = fn->name ? fn->name : "anon";
    const char *ret_ty = fn->type ? wasm_type_str(fn->type) : "i64";
    int is_void = (ret_ty && strcmp(ret_ty, "void") == 0);

    wasm_emit(g, "  (func $%s", fn_name);

    // Parameters
    for (int i = 0; i < fn->var_arg_count; i++) {
        wasm_emit(g, " (param $arg%d i64)", i);
    }

    // Return type
    if (!is_void) {
        wasm_emit(g, " (result %s)", ret_ty);
    }

    wasm_emit(g, "\n");

    // Find max register used to declare locals
    int max_reg = 0;
    for (int b = 0; b < fn->block_count; b++) {
        IRInst *inst = fn->blocks[b]->first;
        while (inst) {
            if (inst->dst.reg_id > max_reg) max_reg = inst->dst.reg_id;
            inst = inst->next;
        }
    }

    // Declare locals
    for (int i = 0; i <= max_reg; i++) {
        wasm_emit(g, "    (local $t%d i64)\n", i);
    }

    // Emit blocks
    for (int b = 0; b < fn->block_count; b++) {
        IRBlock *block = fn->blocks[b];

        // Use block/loop structure for control flow
        if (b < fn->block_count - 1) {
            wasm_emit(g, "    (block $b%d\n", block->id);
        }

        IRInst *inst = block->first;
        while (inst) {
            wasm_gen_inst(g, block, inst);
            inst = inst->next;
        }

        if (b < fn->block_count - 1) {
            wasm_emit(g, "    )\n");  // end block
        }
    }

    wasm_emit(g, "  )\n");
    g->func_idx++;
}

int codegen_wasm_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Cannot open output: %s\n", path);
        return 1;
    }

    WasmGen g;
    g.out = f;
    g.tmp = 0;
    g.local_count = 0;
    g.func_idx = 0;
    g.had_error = false;
    g.mod = mod;

    // WAT Header
    wasm_emit(&g, ";; Ketamine Language v%d.%d.%d — WebAssembly Text Format\n",
              KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
    wasm_emit(&g, "(module\n");

    // Import environment functions
    wasm_emit(&g, "  ;; Runtime imports\n");
    wasm_emit(&g, "  (import \"env\" \"print_i64\" (func $print_i64 (param i64)))\n");
    wasm_emit(&g, "  (import \"env\" \"print_f64\" (func $print_f64 (param f64)))\n");
    wasm_emit(&g, "  (import \"env\" \"print_i32\" (func $print_i32 (param i32)))\n");
    wasm_emit(&g, "  (import \"env\" \"print_str\" (func $print_str (param i32 i32)))\n");
    wasm_emit(&g, "  (import \"env\" \"mem_alloc\" (func $mem_alloc (param i32) (result i32)))\n");
    wasm_emit(&g, "  (import \"env\" \"mem_free\" (func $mem_free (param i32)))\n");
    wasm_emit(&g, "\n");

    // Memory
    wasm_emit(&g, "  ;; Linear memory\n");
    wasm_emit(&g, "  (memory (export \"memory\") 1)\n");
    wasm_emit(&g, "\n");

    // Table for function pointers
    wasm_emit(&g, "  ;; Indirect call table\n");
    wasm_emit(&g, "  (table 0 funcref)\n");
    wasm_emit(&g, "\n");

    // Global variables
    wasm_emit(&g, "  ;; Global variables\n");
    wasm_emit(&g, "  (global $heap_ptr (mut i32) (i32.const 0))\n");
    wasm_emit(&g, "\n");

    // Emit functions
    wasm_emit(&g, "  ;; Functions\n");
    for (int f = 0; f < mod->func_count; f++) {
        wasm_gen_function(&g, mod->functions[f]);
    }

    // Export main
    wasm_emit(&g, "\n  ;; Exports\n");
    for (int f = 0; f < mod->func_count; f++) {
        const char *fn_name = mod->functions[f]->name;
        if (fn_name && strcmp(fn_name, "main") == 0) {
            wasm_emit(&g, "  (export \"main\" (func $main))\n");
        }
    }

    wasm_emit(&g, ")\n");

    fclose(f);

    if (opts->verbose) {
        fprintf(stderr, "ketc: WASM text -> %s (%d functions)\n",
                path, mod->func_count);
    }

    return g.had_error ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PYTHON CODEGEN — Translates IR to Python 3 source code
// ═══════════════════════════════════════════════════════════════════════════════

const char *codegen_python_type_str(struct Type *type) {
    if (!type) return "None";
    switch (type->kind) {
        case TY_VOID:  return "None";
        case TY_BOOL:  return "bool";
        case TY_BYTE:  return "int";
        case TY_CHAR:  return "str";
        case TY_I8:    return "int";
        case TY_I16:   return "int";
        case TY_I32:   return "int";
        case TY_I64:   return "int";
        case TY_U8:    return "int";
        case TY_U16:   return "int";
        case TY_U32:   return "int";
        case TY_U64:   return "int";
        case TY_F32:   return "float";
        case TY_F64:   return "float";
        case TY_STR:   return "str";
        case TY_PTR:   return "int";  // pointer as int
        case TY_REF:   return "object";
        case TY_ARRAY: return "list";
        case TY_TUPLE: return "tuple";
        case TY_STRUCT: return type->struct_type.name ? type->struct_type.name : "object";
        case TY_ENUM:   return type->enum_type.name ? type->enum_type.name : "object";
        case TY_FN:    return "callable";
        default:       return "object";
    }
}

typedef struct {
    FILE    *out;
    int      indent;
    int      tmp;
    bool     had_error;
    IRModule *mod;
} PyGen;

static void py_indent(PyGen *g) {
    for (int i = 0; i < g->indent; i++) fputs("    ", g->out);
}

static void py_emit(PyGen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->out, fmt, ap);
    va_end(ap);
}

static int py_tmp(PyGen *g) { return g->tmp++; }

static void py_emit_value(PyGen *g, IRValue *v) {
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            py_emit(g, "%lld", (long long)v->const_int);
            break;
        case IR_VAL_CONST_FLOAT:
            py_emit(g, "%f", v->const_float);
            break;
        case IR_VAL_CONST_BOOL:
            py_emit(g, "%s", v->const_bool ? "True" : "False");
            break;
        case IR_VAL_REG:
            py_emit(g, "_t%d", v->reg_id);
            break;
        case IR_VAL_GLOBAL:
        case IR_VAL_FUNCTION:
            py_emit(g, "%s", v->func_name ? v->func_name :
                    v->global_name ? v->global_name : "unknown");
            break;
        default:
            py_emit(g, "None");
    }
}

static void py_gen_inst(PyGen *g, IRBlock *block, IRInst *inst) {
    (void)block;

    switch (inst->op) {
        case IR_NONE:
            break;

        case IR_ADD: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " + ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_SUB: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " - ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_MUL: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " * ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_DIV: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " // ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_MOD: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " %% ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_AND: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " & ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_OR: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " | ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_XOR: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " ^ ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_SHL: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " << ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_SHR: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " >> ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_NEG: {
            py_indent(g);
            py_emit(g, "_t%d = -(", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, ")\n");
            break;
        }
        case IR_NOT: {
            py_indent(g);
            py_emit(g, "_t%d = not (", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, ")\n");
            break;
        }

        // Comparisons
        case IR_EQ: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " == ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_NE: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " != ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_LT: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " < ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_GT: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " > ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_LE: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " <= ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }
        case IR_GE: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, " >= ");
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }

        case IR_ALLOCA: {
            py_indent(g);
            py_emit(g, "_t%d = 0  # alloca\n", inst->dst.reg_id);
            break;
        }
        case IR_LOAD: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->dst.reg_id);
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, "\n");
            break;
        }
        case IR_STORE: {
            py_indent(g);
            py_emit(g, "_t%d = ", inst->operands[0].reg_id);
            py_emit_value(g, &inst->operands[1]);
            py_emit(g, "\n");
            break;
        }

        case IR_CALL: {
            const char *fn = inst->operands[0].func_name ?
                inst->operands[0].func_name : "unknown";

            if (strcmp(fn, "print") == 0) {
                py_indent(g);
                py_emit(g, "print(");
                py_emit_value(g, &inst->operands[1]);
                py_emit(g, ")\n");
            } else if (strcmp(fn, "main") == 0) {
                // main is called by the Python runtime
            } else {
                py_indent(g);
                py_emit(g, "_t%d = ", inst->dst.reg_id);
                py_emit(g, "%s(", fn);
                for (int i = 1; i < 4; i++) {
                    if (inst->operands[i].kind == IR_VAL_NONE) break;
                    if (i > 1) py_emit(g, ", ");
                    py_emit_value(g, &inst->operands[i]);
                }
                py_emit(g, ")\n");
            }
            break;
        }

        case IR_RET: {
            if (inst->operands[0].kind != IR_VAL_NONE) {
                py_indent(g);
                py_emit(g, "return ");
                py_emit_value(g, &inst->operands[0]);
                py_emit(g, "\n");
            } else {
                py_indent(g);
                py_emit(g, "return\n");
            }
            break;
        }

        case IR_BR: {
            if (inst->operands[0].kind == IR_VAL_BLOCK) {
                py_indent(g);
                py_emit(g, "    # goto block %d\n", inst->operands[0].block_id);
            }
            break;
        }

        case IR_BR_COND: {
            py_indent(g);
            py_emit(g, "if ");
            py_emit_value(g, &inst->operands[0]);
            py_emit(g, ":\n");
            g->indent++;
            py_indent(g);
            py_emit(g, "# goto block %d\n", inst->operands[1].block_id);
            g->indent--;
            py_indent(g);
            py_emit(g, "else:\n");
            g->indent++;
            py_indent(g);
            py_emit(g, "# goto block %d\n", inst->operands[2].block_id);
            g->indent--;
            break;
        }

        case IR_UNREACHABLE:
            py_indent(g);
            py_emit(g, "raise Exception(\"unreachable\")\n");
            break;

        default:
            py_indent(g);
            py_emit(g, "# unhandled op: %d\n", inst->op);
            break;
    }
}

static void py_gen_function(PyGen *g, IRFunction *fn) {
    const char *fn_name = fn->name ? fn->name : "anon";

    // Skip main — it will be called at module level
    if (strcmp(fn_name, "main") == 0) {
        // Generate as regular function
        py_emit(g, "def main():\n");
        g->indent = 1;

        // Generate all blocks
        for (int b = 0; b < fn->block_count; b++) {
            IRBlock *block = fn->blocks[b];
            IRInst *inst = block->first;
            while (inst) {
                py_gen_inst(g, block, inst);
                inst = inst->next;
            }
        }

        g->indent = 1;
        py_indent(g);
        py_emit(g, "pass\n");

        g->indent = 0;
        py_emit(g, "\n\n");
        py_emit(g, "if __name__ == \"__main__\":\n");
        py_emit(g, "    main()\n");
        return;
    }

    py_emit(g, "def %s(");
    // Emit parameters — use the first N operands of the entry block
    int param_idx = 0;
    py_emit(g, "):\n");

    g->indent = 1;

    // Emit docstring
    py_indent(g);
    py_emit(g, "\"\"\"Auto-generated from Ketamine IR.\"\"\"\n");

    // Generate all blocks
    for (int b = 0; b < fn->block_count; b++) {
        IRBlock *block = fn->blocks[b];
        IRInst *inst = block->first;
        while (inst) {
            py_gen_inst(g, block, inst);
            inst = inst->next;
        }
    }

    g->indent = 1;
    py_indent(g);
    py_emit(g, "pass\n");
    g->indent = 0;
    py_emit(g, "\n");
}

int codegen_python_module(IRModule *mod, const char *path, CompileOptions *opts) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Cannot open output: %s\n", path);
        return 1;
    }

    PyGen g;
    g.out = f;
    g.indent = 0;
    g.tmp = 0;
    g.had_error = false;
    g.mod = mod;

    // Header
    py_emit(&g, "#!/usr/bin/env python3\n");
    py_emit(&g, "# Ketamine Language v%d.%d.%d — Python backend\n",
            KET_VERSION_MAJOR, KET_VERSION_MINOR, KET_VERSION_PATCH);
    py_emit(&g, "# Auto-generated, do not edit.\n\n");

    // Imports
    py_emit(&g, "import sys\n");
    py_emit(&g, "import math\n");
    py_emit(&g, "import json\n");
    py_emit(&g, "import threading\n");
    py_emit(&g, "import time\n");
    py_emit(&g, "import os\n");
    py_emit(&g, "from typing import List, Dict, Tuple, Optional, Any\n\n");

    // Emit functions
    for (int f = 0; f < mod->func_count; f++) {
        py_gen_function(&g, mod->functions[f]);
    }

    // Entry point
    bool has_main = false;
    for (int f = 0; f < mod->func_count; f++) {
        if (strcmp(mod->functions[f]->name, "main") == 0) {
            has_main = true;
            break;
        }
    }

    if (has_main) {
        py_emit(&g, "\nif __name__ == \"__main__\":\n");
        py_emit(&g, "    main()\n");
    }

    fclose(f);

    if (opts->verbose) {
        fprintf(stderr, "ketc: Python source -> %s (%d functions)\n",
                path, mod->func_count);
    }

    return g.had_error ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// JAVASCRIPT CODEGEN (stub)
// ═══════════════════════════════════════════════════════════════════════════════

int codegen_js_module(IRModule *mod, const char *path, CompileOptions *opts) {
    (void)mod; (void)path; (void)opts;
    fprintf(stderr, "JavaScript backend not yet implemented\n");
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// NATIVE CODEGEN (stub)
// ═══════════════════════════════════════════════════════════════════════════════

int codegen_native_module(IRModule *mod, const char *path, CompileOptions *opts) {
    (void)mod; (void)path; (void)opts;
    fprintf(stderr, "Native backend not yet implemented\n");
    return 1;
}
