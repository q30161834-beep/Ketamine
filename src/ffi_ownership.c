#include "include/ffi_ownership.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ─── Recommendation engine ─────────────────────────────────────────────────
FFIOwnership ffi_ownership_recommend(size_t data_size, bool is_mutable, bool is_static) {
    if (is_static) {
        return OWNERSHIP_PIN;
    }
    if (data_size < 1024) {
        return OWNERSHIP_COPY;
    }
    if (is_mutable) {
        return OWNERSHIP_TRANSFER;
    }
    return OWNERSHIP_REFCOUNT;
}

// ─── Core dispatch ─────────────────────────────────────────────────────────
void *ffi_pass_to_target(void *data, size_t size, size_t align,
                         FFIOwnership policy, const char *target_name) {
    (void)target_name;
    switch (policy) {
        case OWNERSHIP_COPY: {
            void *copy = malloc(size);
            if (copy) memcpy(copy, data, size);
            return copy;
        }
        case OWNERSHIP_PIN:
            // Memory stays in place; caller registers via ffi_pin_memory
            return data;
        case OWNERSHIP_TRANSFER:
            // Return the pointer — caller must not free it
            return data;
        case OWNERSHIP_REFCOUNT:
            // Memory stays, caller manages refcount
            return data;
    }
    return NULL;
}

// ─── Pin Registry ──────────────────────────────────────────────────────────
PinRegistry *ffi_pin_registry_new(Arena *arena) {
    PinRegistry *reg = (PinRegistry*)arena_alloc_zero(arena, sizeof(PinRegistry));
    reg->arena = arena;
    return reg;
}

void *ffi_pin_memory(PinRegistry *reg, void *data, size_t size, const char *owner) {
    PinEntry *e = (PinEntry*)arena_alloc_zero(reg->arena, sizeof(PinEntry));
    e->data = data;
    e->size = size;
    e->owner = owner;
    e->released = false;
    e->next = reg->entries;
    reg->entries = e;
    reg->count++;
    return data;
}

void ffi_unpin_memory(PinRegistry *reg, void *data) {
    for (PinEntry *e = reg->entries; e; e = e->next) {
        if (e->data == data) {
            e->released = true;
            return;
        }
    }
}

// ─── Transfer ──────────────────────────────────────────────────────────────
void *ffi_transfer_ownership(void *data, size_t size) {
    (void)size;
    // In a real implementation, we'd tell the borrow checker to forget this ptr
    return data;
}

// ─── Refcount Registry ─────────────────────────────────────────────────────
RefCountRegistry *ffi_refcount_registry_new(Arena *arena) {
    RefCountRegistry *reg = (RefCountRegistry*)arena_alloc_zero(arena, sizeof(RefCountRegistry));
    reg->arena = arena;
    return reg;
}

void ffi_refcount_inc(RefCountRegistry *reg, void *data) {
    for (RefCountEntry *e = reg->entries; e; e = e->next) {
        if (e->data == data) {
            e->count++;
            return;
        }
    }
    // Create new entry
    RefCountEntry *e = (RefCountEntry*)arena_alloc_zero(reg->arena, sizeof(RefCountEntry));
    e->data = data;
    e->count = 1;
    e->free_fn = free;
    e->next = reg->entries;
    reg->entries = e;
    reg->count++;
}

void ffi_refcount_dec(RefCountRegistry *reg, void *data) {
    RefCountEntry *prev = NULL;
    for (RefCountEntry *e = reg->entries; e; prev = e, e = e->next) {
        if (e->data == data) {
            e->count--;
            if (e->count <= 0) {
                if (e->free_fn) e->free_fn(e->data);
                // Remove from list (unlink, but arena doesn't free individual entries)
                if (prev) prev->next = e->next;
                else reg->entries = e->next;
                reg->count--;
            }
            return;
        }
    }
}

// ─── Clone ─────────────────────────────────────────────────────────────────
void *ffi_clone_data(void *data, size_t size, Arena *arena) {
    if (arena) {
        return arena_alloc_copy(arena, data, size);
    }
    void *copy = malloc(size);
    if (copy) memcpy(copy, data, size);
    return copy;
}

// ─── Cleanup ───────────────────────────────────────────────────────────────
void ffi_ownership_cleanup(PinRegistry *pins, RefCountRegistry *refs) {
    // Release all pinned entries (forget them)
    for (PinEntry *e = pins->entries; e; e = e->next) {
        e->released = true;
    }
    pins->count = 0;

    // Release all refcounted entries still alive
    RefCountEntry *e = refs->entries;
    while (e) {
        RefCountEntry *next = e->next;
        if (e->free_fn) e->free_fn(e->data);
        e = next;
    }
    refs->entries = NULL;
    refs->count = 0;
}
