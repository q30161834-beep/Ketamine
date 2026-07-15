#ifndef KETAMINE_JIT_LAYOUT_H
#define KETAMINE_JIT_LAYOUT_H

#include "types.h"

// ─── Platform ABI ──────────────────────────────────────────────────────────
typedef enum {
    ABI_SYSTEM_V,       // Linux, macOS, FreeBSD (x86-64)
    ABI_WIN64,          // Windows x64
    ABI_AARCH64,        // ARM64
    ABI_RISCV64         // RISC-V 64
} PlatformABI;

// ─── Struct Field Layout ───────────────────────────────────────────────────
typedef struct {
    size_t offset;      // byte offset from struct start
    size_t size;        // field size in bytes
    size_t align;       // field alignment in bytes
    struct Type *type;  // type reference
    const char *name;   // field name (NULL for padding)
} FieldLayout;

// ─── Struct Layout ─────────────────────────────────────────────────────────
typedef struct {
    size_t       size;          // total struct size (including trailing padding)
    size_t       align;         // struct alignment
    FieldLayout *fields;        // array of field layouts
    int          field_count;   // number of fields
    size_t       padding_total; // total bytes lost to padding
    bool         packed;        // packed struct (no padding)
    PlatformABI  abi;           // which ABI this layout targets
} StructLayout;

// ─── Platform info ─────────────────────────────────────────────────────────
typedef struct {
    PlatformABI abi;
    size_t      ptr_size;       // 4 or 8
    size_t      ptr_align;      // 4 or 8
    size_t      stack_align;    // 16 on x86-64
    int         reg_args[6];    // register file for argument passing
    int         reg_arg_count;  // number of register-passed args
    bool        callee_cleanup; // true for stdcall/thiscall
} PlatformInfo;

// ─── API ──────────────────────────────────────────────────────────────────

/// Get platform info for a given ABI
PlatformInfo platform_info(PlatformABI abi);

/// Detect host platform ABI automatically
PlatformABI platform_detect_host(void);

/// Calculate layout for a struct type
StructLayout jit_calculate_struct_layout(struct Type *ty, PlatformABI abi);

/// Calculate layout for an enum type (discriminant + payload)
StructLayout jit_calculate_enum_layout(struct Type *ty, PlatformABI abi);

/// Calculate size and alignment of any type
size_t jit_type_size(struct Type *ty, PlatformABI abi);
size_t jit_type_align(struct Type *ty, PlatformABI abi);

/// Check if a type is a valid return-in-register type
bool jit_is_return_in_reg(struct Type *ty, PlatformABI abi);

/// Validate that runtime layout matches JIT layout (for FFI safety)
bool jit_validate_layout(struct Type *ty, size_t expected_size, StructLayout *layout);

/// Free a struct layout
void jit_layout_free(StructLayout *layout);

/// Print layout for debugging
void jit_layout_dump(StructLayout *layout);

#endif
