#ifndef KETAMINE_ARENA_H
#define KETAMINE_ARENA_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// ARENA ALLOCATOR — Bump allocator with fallback regions
// ═══════════════════════════════════════════════════════════════════════════════

#define ARENA_DEFAULT_SIZE (64 * 1024 * 1024)   // 64 MB
#define ARENA_LARGE_THRESH (1024 * 1024)         // 1 MB — use mmap directly
#define ARENA_ALIGNMENT    16
#define ARENA_MAX_BLOCKS   256

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t             size;
    size_t             used;
    uint8_t           *data;
    bool               is_mmap;
} ArenaBlock;

typedef struct Arena {
    ArenaBlock  *head;
    ArenaBlock  *current;
    size_t       block_size;
    size_t       total_allocated;
    size_t       total_used;
    int          block_count;
} Arena;

// ─── API ──────────────────────────────────────────────────────────────────────

// Initialize arena with default block size
Arena  arena_new(void);
Arena  arena_new_size(size_t block_size);

// Allocate memory (aligned)
void  *arena_alloc(Arena *arena, size_t size);
void  *arena_alloc_zero(Arena *arena, size_t size);
void  *arena_alloc_aligned(Arena *arena, size_t size, int align);

// Allocate and copy memory
void  *arena_alloc_copy(Arena *arena, const void *data, size_t size);

// Duplicate a string
char  *arena_strdup(Arena *arena, const char *str);
char  *arena_strndup(Arena *arena, const char *str, int len);

// Format a string into arena
char  *arena_strdup_fmt(Arena *arena, const char *fmt, ...);

// Reset arena (free all blocks except possibly the first)
void   arena_reset(Arena *arena);

// Free all memory
void   arena_free(Arena *arena);

// Current usage statistics
size_t arena_total_used(Arena *arena);
size_t arena_total_allocated(Arena *arena);

// Temporary save/restore (for speculative parsing)
typedef struct {
    ArenaBlock *block;
    size_t      used;
} ArenaMark;

ArenaMark arena_mark(Arena *arena);
void      arena_restore(Arena *arena, ArenaMark mark);

#endif // KETAMINE_ARENA_H
