#include "include/parallel.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
  #include <windows.h>
  #define THREAD_FN DWORD WINAPI
  #define THREAD_RET 0
#else
  #include <pthread.h>
  #define THREAD_FN void*
  #define THREAD_RET NULL
#endif

// ─── Thread Pool ───────────────────────────────────────────────────────────

typedef struct {
    ThreadPool *pool;
    int         id;
} ThreadArg;

static volatile bool g_pool_running = false;

ThreadPool *pool_init(int thread_count) {
    ThreadPool *pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    pool->thread_count = thread_count < 1 ? 1 : thread_count;
    pool->running = true;
    g_pool_running = true;
    pool->threads = calloc((size_t)pool->thread_count, sizeof(void*));

#ifdef _WIN32
    for (int i = 0; i < pool->thread_count; i++) {
        ThreadArg *arg = (ThreadArg*)malloc(sizeof(ThreadArg));
        arg->pool = pool;
        arg->id = i;
        pool->threads[i] = CreateThread(NULL, 0, pool_worker, arg, 0, NULL);
    }
#else
    // pthread implementation would go here
    (void)pool;
#endif

    return pool;
}

void pool_submit(ThreadPool *pool, void (*func)(void*), void *arg) {
    WorkItem *item = (WorkItem*)malloc(sizeof(WorkItem));
    item->func = func;
    item->arg = arg;
    item->next = NULL;

    // Lock-free push (simplified)
    if (pool->queue.tail) {
        pool->queue.tail->next = item;
    } else {
        pool->queue.head = item;
    }
    pool->queue.tail = item;
    pool->queue.count++;
}

void pool_wait(ThreadPool *pool) {
    while (pool->active_count > 0 || pool->queue.count > 0) {
        // Spin-wait (simplified)
#ifdef _WIN32
        Sleep(1);
#endif
    }
}

void pool_destroy(ThreadPool *pool) {
    pool->running = false;
    g_pool_running = false;
#ifdef _WIN32
    for (int i = 0; i < pool->thread_count; i++) {
        WaitForSingleObject(pool->threads[i], INFINITE);
        CloseHandle(pool->threads[i]);
    }
#endif
    // Clean remaining work items
    WorkItem *item = pool->queue.head;
    while (item) {
        WorkItem *next = item->next;
        free(item);
        item = next;
    }
    free(pool->threads);
    free(pool);
}

// ─── Parallel Compilation Phases ───────────────────────────────────────────

ParallelResult *parallel_parse_files(Context *ctx, const char **files, int file_count) {
    ParallelResult *result = (ParallelResult*)calloc(1, sizeof(ParallelResult));
    result->modules = (ASTNode**)calloc((size_t)file_count, sizeof(ASTNode*));
    result->module_count = file_count;

    // Sequential parse for now (threading would go here)
    for (int i = 0; i < file_count; i++) {
        int len;
        char *source = NULL;
        FILE *f = fopen(files[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            len = (int)ftell(f);
            fseek(f, 0, SEEK_SET);
            source = (char*)malloc((size_t)len + 1);
            if (source) {
                fread(source, 1, (size_t)len, f);
                source[len] = '\0';
            }
            fclose(f);
        }
        // Parse would happen here using parser
        (void)source;
        (void)ctx;
    }

    return result;
}

bool parallel_type_check(Context *ctx, ASTNode **modules, int count) {
    (void)ctx;
    (void)modules;
    (void)count;
    return true;
}

int parallel_codegen(IRModule *mod, const char *path, CompileOptions *opts) {
    (void)mod;
    (void)path;
    (void)opts;
    return 0;
}

// ─── Worker thread (Windows) ───────────────────────────────────────────────
#ifdef _WIN32
static DWORD WINAPI pool_worker(LPVOID arg) {
    ThreadPool *pool = ((ThreadArg*)arg)->pool;
    free(arg);

    while (g_pool_running) {
        // Try to dequeue
        WorkItem *item = NULL;
        if (pool->queue.head) {
            item = pool->queue.head;
            pool->queue.head = item->next;
            if (!pool->queue.head) pool->queue.tail = NULL;
            pool->queue.count--;
        }

        if (item) {
            InterlockedIncrement(&pool->active_count);
            item->func(item->arg);
            InterlockedDecrement(&pool->active_count);
            free(item);
        } else {
            Sleep(1); // yield
        }
    }

    return THREAD_RET;
}
#endif
