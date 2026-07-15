#ifndef KETAMINE_PARALLEL_H
#define KETAMINE_PARALLEL_H

#include "types.h"

// ─── Parallel Compilation Engine ───────────────────────────────────────────
// Work-stealing thread pool for parallel compilation.
// Supports: parallel parsing, type checking, codegen.

typedef struct WorkItem {
    void (*func)(void *arg);
    void  *arg;
    struct WorkItem *next;
} WorkItem;

typedef struct {
    WorkItem   *head;
    WorkItem   *tail;
    int         count;
} WorkQueue;

typedef struct {
    WorkQueue   queue;
    int         thread_count;
    void      **threads;     // thread handles
    bool        running;
    int         active_count;
} ThreadPool;

// ─── Parallel Phase Results ────────────────────────────────────────────────
typedef struct {
    ASTNode   **modules;     // parsed modules (parallel parsing)
    int         module_count;
    int         errors;
    double      phase_time;
} ParallelResult;

// ─── API ──────────────────────────────────────────────────────────────────

/// Initialize thread pool with N workers
ThreadPool *pool_init(int thread_count);

/// Submit work to the pool
void pool_submit(ThreadPool *pool, void (*func)(void*), void *arg);

/// Wait for all work to complete
void pool_wait(ThreadPool *pool);

/// Destroy thread pool
void pool_destroy(ThreadPool *pool);

/// Parse multiple files in parallel
ParallelResult *parallel_parse_files(Context *ctx, const char **files, int file_count);

/// Type-check modules in parallel
bool parallel_type_check(Context *ctx, ASTNode **modules, int count);

/// Generate code for functions in parallel
int parallel_codegen(IRModule *mod, const char *path, CompileOptions *opts);

#endif
