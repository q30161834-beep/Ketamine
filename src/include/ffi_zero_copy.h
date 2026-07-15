#ifndef KETAMINE_FFI_ZERO_COPY_H
#define KETAMINE_FFI_ZERO_COPY_H

#include "types.h"
#include "arena.h"

// ─── FFI Type Conversion Cache ──────────────────────────────────────────────
typedef struct FFITypeEntry {
    struct FFITypeEntry *next;
    struct Type         *ket_type;       // Ketamine type
    const char          *target_name;    // e.g. "PyObject*", "numpy.ndarray"
    size_t               target_size;
    bool                 zero_copy;      // can be passed directly
    bool                 no_copy;        // #[ffi(no_copy)]
    void               (*to_ffi)(void *ket, void *ffi);    // converter
    void               (*from_ffi)(void *ffi, void *ket);  // converter
} FFITypeEntry;

typedef struct {
    FFITypeEntry **buckets;
    int            bucket_count;
    int            count;
    Arena         *arena;
} FFITypeCache;

// ─── FFI Borrow/Pin/Transfer State ──────────────────────────────────────────
typedef struct {
    void   *data;
    size_t  size;
    bool    is_borrowed;    // Ownership::Borrow
    bool    is_pinned;      // Ownership::Pin
    bool    is_transferred; // Ownership::Transfer
    int     refcount;       // Ownership::RefCount
} FFIBuffer;

// ─── API ──────────────────────────────────────────────────────────────────

/// Initialize type cache
FFITypeCache *ffi_type_cache_new(Arena *arena);

/// Register a type conversion
void ffi_type_cache_add(FFITypeCache *cache, struct Type *ket,
                        const char *target_name, bool zero_copy);

/// Look up a type conversion
FFITypeEntry *ffi_type_cache_get(FFITypeCache *cache, struct Type *ket);

/// Borrow a buffer (zero-copy, ownership stays in Ketamine)
FFIBuffer *ffi_borrow(void *data, size_t size);

/// Pin a buffer (memory locked, never freed by Ketamine)
FFIBuffer *ffi_pin(void *data, size_t size);

/// Transfer a buffer (ownership moves to FFI target)
FFIBuffer *ffi_transfer(void *data, size_t size);

/// Increment refcount
void ffi_refcount_inc(FFIBuffer *buf);

/// Decrement refcount, free if zero
void ffi_refcount_dec(FFIBuffer *buf);

/// Check if a type supports zero-copy FFI
bool ffi_can_zero_copy(struct Type *ty);

/// Pass a buffer directly without copying (zero-copy path)
void *ffi_zero_copy_ptr(void *data, FFIBuffer *buf);

#endif
