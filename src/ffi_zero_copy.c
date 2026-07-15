#include "include/ffi_zero_copy.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FFI_CACHE_BUCKETS 64

static uint32_t ffi_hash(struct Type *ty) {
    return (uint32_t)(uintptr_t)ty;
}

FFITypeCache *ffi_type_cache_new(Arena *arena) {
    FFITypeCache *cache = (FFITypeCache*)arena_alloc_zero(arena, sizeof(FFITypeCache));
    cache->bucket_count = FFI_CACHE_BUCKETS;
    cache->buckets = (FFITypeEntry**)arena_alloc_zero(arena,
                        sizeof(FFITypeEntry*) * FFI_CACHE_BUCKETS);
    cache->arena = arena;
    return cache;
}

void ffi_type_cache_add(FFITypeCache *cache, struct Type *ket,
                        const char *target_name, bool zero_copy) {
    uint32_t h = ffi_hash(ket) % cache->bucket_count;
    FFITypeEntry *e = (FFITypeEntry*)arena_alloc_zero(cache->arena, sizeof(FFITypeEntry));
    e->ket_type = ket;
    e->target_name = target_name;
    e->zero_copy = zero_copy;
    e->no_copy = zero_copy;
    e->next = cache->buckets[h];
    cache->buckets[h] = e;
    cache->count++;
}

FFITypeEntry *ffi_type_cache_get(FFITypeCache *cache, struct Type *ket) {
    uint32_t h = ffi_hash(ket) % cache->bucket_count;
    for (FFITypeEntry *e = cache->buckets[h]; e; e = e->next) {
        if (e->ket_type == ket) return e;
    }
    return NULL;
}

// ─── Borrow / Pin / Transfer ───────────────────────────────────────────────
static FFIBuffer *ffi_buf_alloc(void *data, size_t size) {
    FFIBuffer *buf = (FFIBuffer*)calloc(1, sizeof(FFIBuffer));
    if (buf) {
        buf->data = data;
        buf->size = size;
    }
    return buf;
}

FFIBuffer *ffi_borrow(void *data, size_t size) {
    FFIBuffer *buf = ffi_buf_alloc(data, size);
    if (buf) buf->is_borrowed = true;
    return buf;
}

FFIBuffer *ffi_pin(void *data, size_t size) {
    FFIBuffer *buf = ffi_buf_alloc(data, size);
    if (buf) buf->is_pinned = true;
    return buf;
}

FFIBuffer *ffi_transfer(void *data, size_t size) {
    FFIBuffer *buf = ffi_buf_alloc(data, size);
    if (buf) buf->is_transferred = true;
    return buf;
}

void ffi_refcount_inc(FFIBuffer *buf) {
    if (buf) buf->refcount++;
}

void ffi_refcount_dec(FFIBuffer *buf) {
    if (!buf) return;
    buf->refcount--;
    if (buf->refcount <= 0 && !buf->is_borrowed && !buf->is_pinned) {
        free(buf->data);
        free(buf);
    }
}

bool ffi_can_zero_copy(struct Type *ty) {
    if (!ty) return false;
    // Arrays and slices of primitive types can be zero-copy
    switch (ty->kind) {
        case TY_ARRAY:
        case TY_SLICE:
            return true;
        case TY_STRUCT:
            // Large structs with #[ffi(no_copy)]
            return ty->size > 1024;
        case TY_PTR:
        case TY_REF:
            return true;
        default:
            return ty->size <= 8; // small types fit in register
    }
}

void *ffi_zero_copy_ptr(void *data, FFIBuffer *buf) {
    (void)buf;
    return data; // direct pointer, no copy
}
