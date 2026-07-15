#ifdef _WIN32
  #include <windows.h>
#endif
#include "include/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
  #define MAP_FAILED NULL
  static void *mmap_alloc(size_t sz) {
      return VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  }
  static void mmap_free(void *p, size_t sz) {
      VirtualFree(p, 0, MEM_RELEASE);
  }
#else
  #include <sys/mman.h>
  #include <unistd.h>
  static void *mmap_alloc(size_t sz) {
      return mmap(NULL, sz, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }
  static void mmap_free(void *p, size_t sz) {
      munmap(p, sz);
  }
#endif

static ArenaBlock *block_alloc(size_t size) {
    bool use_mmap = size >= ARENA_LARGE_THRESH;
    ArenaBlock *b = (ArenaBlock*)malloc(sizeof(ArenaBlock));
    if (!b) { fprintf(stderr, "Arena: out of memory\n"); exit(1); }

    if (use_mmap) {
        b->data = (uint8_t*)mmap_alloc(size);
        b->is_mmap = true;
    } else {
        b->data = (uint8_t*)malloc(size);
        b->is_mmap = false;
    }

    if (!b->data) {
        fprintf(stderr, "Arena: out of memory (%zu bytes)\n", size);
        free(b);
        exit(1);
    }

    b->size = size;
    b->used = 0;
    b->next = NULL;
    return b;
}

Arena arena_new(void) {
    return arena_new_size(ARENA_DEFAULT_SIZE);
}

Arena arena_new_size(size_t block_size) {
    Arena a;
    a.block_size = block_size;
    a.total_allocated = 0;
    a.total_used = 0;
    a.block_count = 0;
    a.current = NULL;
    a.head = NULL;

    a.current = block_alloc(block_size);
    a.head = a.current;
    a.block_count = 1;
    a.total_allocated = block_size;

    return a;
}

void *arena_alloc_aligned(Arena *a, size_t size, int align) {
    if (size == 0) size = 1;

    // Align current position
    size_t mask = (size_t)(align - 1);
    size_t pos = (a->current->used + mask) & ~mask;

    if (pos + size > a->current->size) {
        // Need a new block
        size_t new_size = a->block_size;
        if (size > new_size) new_size = size + a->block_size;
        if (size + sizeof(ArenaBlock) > new_size) new_size = size + a->block_size;

        ArenaBlock *b = block_alloc(new_size);
        b->next = a->current->next;
        a->current->next = b;
        a->current = b;
        a->block_count++;
        a->total_allocated += new_size;

        pos = 0;
    }

    void *ptr = a->current->data + pos;
    a->current->used = pos + size;
    a->total_used += size;
    return ptr;
}

void *arena_alloc(Arena *a, size_t size) {
    return arena_alloc_aligned(a, size, ARENA_ALIGNMENT);
}

void *arena_alloc_zero(Arena *a, size_t size) {
    void *p = arena_alloc(a, size);
    memset(p, 0, size);
    return p;
}

void *arena_alloc_copy(Arena *a, const void *data, size_t size) {
    void *p = arena_alloc(a, size);
    if (data) memcpy(p, data, size);
    return p;
}

char *arena_strdup(Arena *a, const char *str) {
    if (!str) return NULL;
    int len = (int)strlen(str);
    return arena_strndup(a, str, len);
}

char *arena_strndup(Arena *a, const char *str, int len) {
    if (!str || len == 0) return NULL;
    char *p = (char*)arena_alloc(a, len + 1);
    memcpy(p, str, len);
    p[len] = '\0';
    return p;
}

char *arena_strdup_fmt(Arena *a, const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof(buf)) return NULL;
    return arena_strndup(a, buf, n);
}

void arena_reset(Arena *a) {
    if (!a->head) return;
    // Keep the first block, free the rest
    ArenaBlock *b = a->head->next;
    while (b) {
        ArenaBlock *next = b->next;
        if (b->is_mmap) {
            mmap_free(b->data, b->size);
        } else {
            free(b->data);
        }
        free(b);
        b = next;
    }

    a->head->used = 0;
    a->head->next = NULL;
    a->current = a->head;
    a->block_count = 1;
    a->total_used = 0;
    a->total_allocated = a->head->size;
}

void arena_free(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        if (b->is_mmap) {
            mmap_free(b->data, b->size);
        } else {
            free(b->data);
        }
        free(b);
        b = next;
    }
    a->head = NULL;
    a->current = NULL;
    a->block_count = 0;
    a->total_allocated = 0;
    a->total_used = 0;
}

size_t arena_total_used(Arena *a) {
    return a->total_used;
}

size_t arena_total_allocated(Arena *a) {
    return a->total_allocated;
}

ArenaMark arena_mark(Arena *a) {
    ArenaMark m;
    m.block = a->current;
    m.used = a->current ? a->current->used : 0;
    return m;
}

void arena_restore(Arena *a, ArenaMark m) {
    if (!m.block) return;

    // Free blocks after the mark block
    ArenaBlock *b = m.block->next;
    while (b) {
        ArenaBlock *next = b->next;
        if (b->is_mmap) mmap_free(b->data, b->size);
        else free(b->data);
        free(b);
        b = next;
    }

    m.block->used = m.used;
    m.block->next = NULL;
    a->current = m.block;

    // Recalculate total
    a->total_used = 0;
    a->total_allocated = 0;
    b = a->head;
    while (b) {
        a->total_used += b->used;
        a->total_allocated += b->size;
        b = b->next;
    }
}
