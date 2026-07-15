#include "include/monomorph_dispatch.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>

// ─── Strategy selection per backend ────────────────────────────────────────
MonoStrategy mono_strategy_for_target(TargetBackend target) {
    switch (target) {
        case TARGET_LLVM_IR:
        case TARGET_X86_64:
        case TARGET_ASM:
        case TARGET_AARCH64:
        case TARGET_RISCV64:
        case TARGET_WASM:
            return MONO_FULL;
        case TARGET_GO:
            return MONO_PARTIAL;
        case TARGET_PYTHON:
        case TARGET_JS:
        case TARGET_FORTH:
            return MONO_DYNAMIC;
        case TARGET_JIT:
            // JIT compiles x86-64, so it's full monomorphization
            return MONO_FULL;
    }
    return MONO_FULL;
}

// ─── Type category classification ──────────────────────────────────────────
TypeCategory mono_type_category(struct Type *type) {
    if (!type) return TCAT_OTHER;

    switch (type->kind) {
        case TY_NEVER:
        case TY_VOID:
            return TCAT_OTHER;
        case TY_BOOL:
            return TCAT_BOOL;
        case TY_BYTE:
        case TY_CHAR:
        case TY_I8:
        case TY_I16:
        case TY_I32:
        case TY_I64:
        case TY_I128:
        case TY_U8:
        case TY_U16:
        case TY_U32:
        case TY_U64:
        case TY_U128:
            return TCAT_INT;
        case TY_F16:
        case TY_F32:
        case TY_F64:
        case TY_F128:
            return TCAT_FLOAT;
        case TY_STR:
            return TCAT_STR;
        case TY_PTR:
        case TY_REF:
        case TY_MUT_REF:
            return TCAT_PTR;
        case TY_ARRAY:
        case TY_SLICE:
        case TY_TUPLE:
        case TY_STRUCT:
        case TY_ENUM:
            return TCAT_STRUCT;
        default:
            return TCAT_OTHER;
    }
}

// ─── Canonical types for each category ─────────────────────────────────────
struct Type *mono_canonical_type(TypeTable *tt, TypeCategory cat) {
    switch (cat) {
        case TCAT_INT:    return ket_type_get(tt, TY_I64);
        case TCAT_FLOAT:  return ket_type_get(tt, TY_F64);
        case TCAT_PTR:    return ket_type_ptr(tt, ket_type_get(tt, TY_VOID));
        case TCAT_BOOL:   return ket_type_get(tt, TY_BOOL);
        case TCAT_STR:    return ket_type_get(tt, TY_STR);
        case TCAT_STRUCT:
        case TCAT_OTHER:
        default:
            return NULL;
    }
}

// ─── Category comparison ───────────────────────────────────────────────────
bool mono_same_category(struct Type *a, struct Type *b) {
    return mono_type_category(a) == mono_type_category(b);
}

// ─── Names ─────────────────────────────────────────────────────────────────
const char *mono_strategy_name(MonoStrategy s) {
    switch (s) {
        case MONO_FULL:    return "full";
        case MONO_PARTIAL: return "partial";
        case MONO_DYNAMIC: return "dynamic";
    }
    return "unknown";
}

const char *mono_category_name(TypeCategory cat) {
    switch (cat) {
        case TCAT_INT:    return "int";
        case TCAT_FLOAT:  return "float";
        case TCAT_PTR:    return "ptr";
        case TCAT_BOOL:   return "bool";
        case TCAT_STR:    return "string";
        case TCAT_STRUCT: return "struct";
        case TCAT_OTHER:  return "dynamic";
    }
    return "?";
}
