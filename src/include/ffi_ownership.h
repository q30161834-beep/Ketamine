#ifndef KETAMINE_FFI_OWNERSHIP_H
#define KETAMINE_FFI_OWNERSHIP_H

#include "types.h"
#include "arena.h"
#include "borrow.h"

// ─── FFI Ownership Policy enum ─────────────────────────────────────────────
typedef enum {
    OWNERSHIP_COPY,       // Clone data before passing (safe, slow for large data)
    OWNERSHIP_PIN,        // Pin in Ketamine memory, never free (for static data)
    OWNERSHIP_TRANSFER,   // Transfer ownership to FFI target, Ketamine forgets
    OWNERSHIP_REFCOUNT    // Reference counting for shared objects
} FFIOwnership;

// ─── FFI Memory Descriptor ─────────────────────────────────────────────────
typedef struct {
    void          *data;
    size_t         size;
    size_t         align;
    FFIOwnership   policy;
    int            refcount;     // used with OWNERSHIP_REFCOUNT
    void          *ffi_handle;   // pointer in target language heap
    const char    *type_name;    // for debugging
} FFIMemory;

// ─── Pin Registry ─────────────────────────────────────────────────────────
// Tracks all pinned memory regions (OWNERSHIP_PIN)
typedef struct PinEntry {
    struct PinEntry *next;
    void            *data;
    size_t           size;
    const char      *owner;          // which FFI module pinned it
    bool             released;
} PinEntry;

typedef struct {
    PinEntry *entries;
    int       count;
    Arena    *arena;
} PinRegistry;

// ─── Refcount Registry ─────────────────────────────────────────────────────
typedef struct RefCountEntry {
    struct RefCountEntry *next;
    void                 *data;
    int                  count;
    void               (*free_fn)(void*);
} RefCountEntry;

typedef struct {
    RefCountEntry *entries;
    int            count;
    Arena         *arena;
} RefCountRegistry;

// ─── API ──────────────────────────────────────────────────────────────────

/// Determine best policy based on data characteristics
FFIOwnership ffi_ownership_recommend(size_t data_size, bool is_mutable, bool is_static);

/// Pass data to FFI target with specified ownership policy
void        *ffi_pass_to_target(void *data, size_t size, size_t align,
                                FFIOwnership policy, const char *target_name);

/// Pin memory (OWNERSHIP_PIN) — register with borrow checker
void        *ffi_pin_memory(PinRegistry *reg, void *data, size_t size,
                            const char *owner);

/// Unpin memory (release pin, borrow checker can free)
void         ffi_unpin_memory(PinRegistry *reg, void *data);

/// Transfer ownership — forget in Ketamine, give to FFI
void        *ffi_transfer_ownership(void *data, size_t size);

/// Increment refcount (OWNERSHIP_REFCOUNT)
void         ffi_refcount_inc(RefCountRegistry *reg, void *data);

/// Decrement refcount, free if zero
void         ffi_refcount_dec(RefCountRegistry *reg, void *data);

/// Clone data into a new allocation (OWNERSHIP_COPY)
void        *ffi_clone_data(void *data, size_t size, Arena *arena);

/// Cleanup all pinned memory (at program exit)
void         ffi_ownership_cleanup(PinRegistry *pins, RefCountRegistry *refs);

/// Initialize registries
PinRegistry       *ffi_pin_registry_new(Arena *arena);
RefCountRegistry  *ffi_refcount_registry_new(Arena *arena);

#endif
