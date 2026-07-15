#include "include/jit_layout.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// Platform Information
// ═══════════════════════════════════════════════════════════════════════════════

PlatformInfo platform_info(PlatformABI abi) {
    PlatformInfo info;
    memset(&info, 0, sizeof(info));
    info.abi = abi;

    switch (abi) {
        case ABI_SYSTEM_V:
            info.ptr_size      = 8;
            info.ptr_align     = 8;
            info.stack_align   = 16;
            info.reg_args[0]   = 0; // RDI
            info.reg_args[1]   = 1; // RSI
            info.reg_args[2]   = 2; // RDX
            info.reg_args[3]   = 3; // RCX
            info.reg_args[4]   = 8; // R8
            info.reg_args[5]   = 9; // R9
            info.reg_arg_count = 6;
            info.callee_cleanup = false;
            break;

        case ABI_WIN64:
            info.ptr_size      = 8;
            info.ptr_align     = 8;
            info.stack_align   = 16;
            info.reg_args[0]   = 1; // RCX
            info.reg_args[1]   = 2; // RDX
            info.reg_args[2]   = 8; // R8
            info.reg_args[3]   = 9; // R9
            info.reg_arg_count = 4;
            info.callee_cleanup = false;
            break;

        case ABI_AARCH64:
            info.ptr_size      = 8;
            info.ptr_align     = 8;
            info.stack_align   = 16;
            for (int i = 0; i < 8; i++) info.reg_args[i] = i;
            info.reg_arg_count = 8;
            info.callee_cleanup = false;
            break;

        case ABI_RISCV64:
            info.ptr_size      = 8;
            info.ptr_align     = 8;
            info.stack_align   = 16;
            for (int i = 0; i < 8; i++) info.reg_args[i] = i + 10; // a0-a7
            info.reg_arg_count = 8;
            info.callee_cleanup = false;
            break;
    }
    return info;
}

PlatformABI platform_detect_host(void) {
#ifdef _WIN64
    return ABI_WIN64;
#elif defined(__x86_64__)
    return ABI_SYSTEM_V;
#elif defined(__aarch64__)
    return ABI_AARCH64;
#elif defined(__riscv) && __riscv_xlen == 64
    return ABI_RISCV64;
#else
    // Default to System V for unknown platforms
    return ABI_SYSTEM_V;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// Type Size & Alignment
// ═══════════════════════════════════════════════════════════════════════════════

static size_t align_up(size_t val, size_t align) {
    return (val + align - 1) & ~(align - 1);
}

size_t jit_type_size(struct Type *ty, PlatformABI abi) {
    if (!ty) return 0;
    (void)abi;

    switch (ty->kind) {
        case TY_NEVER:
        case TY_VOID:    return 0;
        case TY_BOOL:    return 1;
        case TY_BYTE:    return 1;
        case TY_CHAR:    return 4;
        case TY_I8:
        case TY_U8:      return 1;
        case TY_I16:
        case TY_U16:     return 2;
        case TY_I32:
        case TY_U32:
        case TY_F32:     return 4;
        case TY_I64:
        case TY_U64:
        case TY_F64:     return 8;
        case TY_I128:
        case TY_U128:
        case TY_F128:    return 16;
        case TY_STR:
        case TY_PTR:
        case TY_REF:
        case TY_MUT_REF:
        case TY_FN:      return 8;
        case TY_ARRAY:
            if (ty->array.count > 0)
                return jit_type_size(ty->array.elem, abi) * (size_t)ty->array.count;
            return 8; // slice header: {ptr, len}
        case TY_SLICE:   return 16; // {ptr, len}
        case TY_TUPLE: {
            size_t sz = 0;
            for (int i = 0; i < ty->tuple.elem_count; i++)
                sz += jit_type_size(ty->tuple.elems[i], abi);
            return sz;
        }
        case TY_STRUCT: {
            StructLayout layout = jit_calculate_struct_layout(ty, abi);
            size_t sz = layout.size;
            free(layout.fields);
            return sz;
        }
        case TY_ENUM: {
            StructLayout layout = jit_calculate_enum_layout(ty, abi);
            size_t sz = layout.size;
            free(layout.fields);
            return sz;
        }
        default: return 8;
    }
}

size_t jit_type_align(struct Type *ty, PlatformABI abi) {
    if (!ty) return 1;
    (void)abi;

    switch (ty->kind) {
        case TY_NEVER:
        case TY_VOID:    return 1;
        case TY_BOOL:    return 1;
        case TY_BYTE:    return 1;
        case TY_CHAR:    return 4;
        case TY_I8:
        case TY_U8:      return 1;
        case TY_I16:
        case TY_U16:     return 2;
        case TY_I32:
        case TY_U32:
        case TY_F32:     return 4;
        case TY_I64:
        case TY_U64:
        case TY_F64:     return 8;
        case TY_I128:
        case TY_U128:
        case TY_F128:    return 16;
        case TY_STR:
        case TY_PTR:
        case TY_REF:
        case TY_MUT_REF:
        case TY_FN:      return 8;
        case TY_ARRAY:
            return jit_type_align(ty->array.elem, abi);
        case TY_SLICE:   return 8;
        case TY_TUPLE: {
            size_t max_align = 1;
            for (int i = 0; i < ty->tuple.elem_count; i++) {
                size_t a = jit_type_align(ty->tuple.elems[i], abi);
                if (a > max_align) max_align = a;
            }
            return max_align;
        }
        case TY_STRUCT:
        case TY_ENUM: {
            StructLayout layout;
            if (ty->kind == TY_STRUCT)
                layout = jit_calculate_struct_layout(ty, abi);
            else
                layout = jit_calculate_enum_layout(ty, abi);
            size_t al = layout.align;
            free(layout.fields);
            return al;
        }
        default: return 8;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Struct Layout Calculation
// ═══════════════════════════════════════════════════════════════════════════════

StructLayout jit_calculate_struct_layout(struct Type *ty, PlatformABI abi) {
    StructLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.abi = abi;

    if (!ty || ty->kind != TY_STRUCT) {
        layout.size = 0;
        layout.align = 1;
        return layout;
    }

    int fcount = ty->struct_type.field_count;
    if (fcount <= 0) {
        layout.size = 0;
        layout.align = 1;
        return layout;
    }

    layout.fields = (FieldLayout*)calloc((size_t)fcount, sizeof(FieldLayout));
    if (!layout.fields) {
        layout.size = 0;
        layout.align = 1;
        return layout;
    }
    layout.field_count = fcount;
    layout.packed = (ty->flags & TF_PACKED) != 0;

    size_t offset = 0;
    size_t max_align = 1;

    for (int i = 0; i < fcount; i++) {
        struct Type *ftype = ty->struct_type.fields[i].type;
        const char *fname = ty->struct_type.fields[i].name;

        size_t fsize = jit_type_size(ftype, abi);
        size_t falign = jit_type_align(ftype, abi);

        layout.fields[i].name = fname;
        layout.fields[i].type = ftype;
        layout.fields[i].size = fsize;
        layout.fields[i].align = falign;

        if (!layout.packed) {
            // Align offset to field's alignment
            size_t aligned = align_up(offset, falign);
            size_t pad = aligned - offset;
            layout.padding_total += pad;
            offset = aligned;
        }

        layout.fields[i].offset = offset;
        offset += fsize;

        if (falign > max_align) max_align = falign;
    }

    // Struct alignment = max field alignment
    layout.align = layout.packed ? 1 : max_align;

    // Trailing padding
    size_t final_size = align_up(offset, layout.align);
    layout.padding_total += final_size - offset;
    layout.size = final_size;

    return layout;
}

StructLayout jit_calculate_enum_layout(struct Type *ty, PlatformABI abi) {
    StructLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.abi = abi;

    if (!ty || ty->kind != TY_ENUM) {
        layout.size = 0;
        layout.align = 1;
        return layout;
    }

    // Enums layout: discriminant (int) + largest variant payload
    size_t disc_size = 8; // int64 discriminant
    size_t disc_align = 8;

    size_t max_payload_size = 0;
    size_t max_payload_align = 1;

    for (int i = 0; i < ty->enum_type.variant_count; i++) {
        size_t var_size = 0;
        size_t var_align = 1;
        for (int j = 0; j < ty->enum_type.variants[i].payload_count; j++) {
            size_t ps = jit_type_size(ty->enum_type.variants[i].payload_types[j], abi);
            size_t pa = jit_type_align(ty->enum_type.variants[i].payload_types[j], abi);
            var_size += ps;
            if (pa > var_align) var_align = pa;
        }
        if (var_size > max_payload_size) max_payload_size = var_size;
        if (var_align > max_payload_align) max_payload_align = var_align;
    }

    // Layout: [discriminant] [padding] [payload]
    size_t offset = 0;
    offset += disc_size;
    offset = align_up(offset, max_payload_align);
    size_t payload_offset = offset;
    offset += max_payload_size;

    layout.align = disc_align > max_payload_align ? disc_align : max_payload_align;
    layout.size = align_up(offset, layout.align);
    layout.field_count = ty->enum_type.variant_count;
    layout.padding_total = (payload_offset - disc_size) + (layout.size - offset);

    return layout;
}

// ─── Return-in-register check ───────────────────────────────────────────────
bool jit_is_return_in_reg(struct Type *ty, PlatformABI abi) {
    size_t sz = jit_type_size(ty, abi);

    switch (abi) {
        case ABI_SYSTEM_V:
            // System V: <= 16 bytes in two registers
            return sz <= 16;
        case ABI_WIN64:
            // Win64: <= 8 bytes in RAX, larger via hidden sret pointer
            return sz <= 8;
        case ABI_AARCH64:
        case ABI_RISCV64:
            return sz <= 16;
    }
    return sz <= 8;
}

// ─── Layout validation ──────────────────────────────────────────────────────
bool jit_validate_layout(struct Type *ty, size_t expected_size, StructLayout *layout) {
    if (layout->size != expected_size) {
        fprintf(stderr, "Layout mismatch for %s: computed %zu, expected %zu\n",
                ty->struct_type.name ? ty->struct_type.name : "?",
                layout->size, expected_size);
        return false;
    }
    return true;
}

// ─── Free ───────────────────────────────────────────────────────────────────
void jit_layout_free(StructLayout *layout) {
    if (layout && layout->fields) {
        free(layout->fields);
        layout->fields = NULL;
    }
}

// ─── Debug dump ─────────────────────────────────────────────────────────────
void jit_layout_dump(StructLayout *layout) {
    if (!layout) { printf("  (null layout)\n"); return; }

    const char *abi_names[] = {
        [ABI_SYSTEM_V]  = "System V",
        [ABI_WIN64]     = "Win64",
        [ABI_AARCH64]   = "AArch64",
        [ABI_RISCV64]   = "RISC-V 64",
    };

    printf("  Struct Layout (ABI: %s):\n",
           layout->abi < 4 ? abi_names[layout->abi] : "?");
    printf("    Size:  %zu bytes\n", layout->size);
    printf("    Align: %zu bytes\n", layout->align);
    printf("    Padding total: %zu bytes\n", layout->padding_total);
    if (layout->packed) printf("    Packed: yes\n");

    for (int i = 0; i < layout->field_count; i++) {
        printf("    [%d] %s: offset=%zu size=%zu align=%zu\n",
               i,
               layout->fields[i].name ? layout->fields[i].name : "?",
               layout->fields[i].offset,
               layout->fields[i].size,
               layout->fields[i].align);
    }
}
