#ifndef KETAMINE_MONOMORPH_DISPATCH_H
#define KETAMINE_MONOMORPH_DISPATCH_H

#include "types.h"

// ─── Monomorphization Strategy per Backend ─────────────────────────────────
typedef enum {
    MONO_FULL,         // Generate code per concrete type (LLVM, Native, ASM, WASM)
    MONO_PARTIAL,      // Generate code per category (int, float, ptr, interface{})
    MONO_DYNAMIC       // Single generic code, no monomorphization (Python, Forth)
} MonoStrategy;

// ─── Type Category (for PARTIAL strategy) ──────────────────────────────────
typedef enum {
    TCAT_INT,       // all integer types
    TCAT_FLOAT,     // all float types
    TCAT_PTR,       // all pointer/reference types
    TCAT_BOOL,      // bool
    TCAT_STR,       // string
    TCAT_STRUCT,    // struct/enum (passed by pointer)
    TCAT_OTHER      // interface{} / dynamic
} TypeCategory;

// ─── API ──────────────────────────────────────────────────────────────────

/// Get monomorphization strategy for a given backend
MonoStrategy mono_strategy_for_target(TargetBackend target);

/// Map a concrete type to its type category (for PARTIAL strategy)
TypeCategory mono_type_category(struct Type *type);

/// Get a canonical type for a category (e.g., TCAT_INT -> i64)
struct Type *mono_canonical_type(TypeTable *tt, TypeCategory cat);

/// Check if two types share the same category
bool mono_same_category(struct Type *a, struct Type *b);

/// Get human-readable strategy name
const char *mono_strategy_name(MonoStrategy s);

/// Get human-readable category name
const char *mono_category_name(TypeCategory cat);

#endif
