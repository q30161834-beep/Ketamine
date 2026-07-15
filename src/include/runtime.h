#ifndef KETAMINE_RUNTIME_H
#define KETAMINE_RUNTIME_H

#include "types.h"
#include "arena.h"

// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE RUNTIME — Async runtime with event loop, Future, spawn, block_on
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Future State ─────────────────────────────────────────────────────────────
typedef enum {
    FUTURE_PENDING,
    FUTURE_READY,
    FUTURE_POLLED,
    FUTURE_DROPPED
} FutureState;

// ─── Waker ────────────────────────────────────────────────────────────────────
typedef struct Waker {
    void         (*wake)(struct Waker *self);
    void         *data;
} Waker;

// ─── Future (generic boxed async computation) ─────────────────────────────────
typedef struct Future {
    FutureState  state;
    void         (*poll)(struct Future *self, Waker *waker);
    void         (*drop)(struct Future *self);
    void         *output;       // pointer to result (type-erased)
    size_t       output_size;
    struct Type *output_type;
    void         *data;         // per-future internal state
} Future;

// ─── Task (a running async computation) ───────────────────────────────────────
typedef struct Task {
    struct Task  *next;
    Future       *future;
    Waker        *waker;
    bool          is_completed;
    int           id;
} Task;

// ─── Event Loop ───────────────────────────────────────────────────────────────
typedef struct EventLoop {
    Task         *tasks;         // linked list of active tasks
    int           task_count;
    Task         *completed_queue; // tasks ready for result collection
    int           completed_count;
    bool          running;
    void         *timer_heap;    // min-heap of timer events (simplified)
    Arena        *arena;
} EventLoop;

// ─── Runtime (global async executor) ──────────────────────────────────────────
typedef struct Runtime {
    EventLoop    *loop;
    int           next_task_id;
    bool          initialized;
    Arena        *arena;
} Runtime;

// ─── API ──────────────────────────────────────────────────────────────────────

/// Initialize the global runtime
Runtime *runtime_init(Arena *arena);

/// Spawn a future onto the event loop, returns a task ID
int      runtime_spawn(Runtime *rt, Future *future);

/// Block the current thread until a specific task completes
void    *runtime_block_on(Runtime *rt, int task_id);

/// Run the event loop for one iteration (poll ready tasks)
int      runtime_tick(Runtime *rt);

/// Run the event loop until all tasks complete
void     runtime_run(Runtime *rt);

/// Get the output of a completed task
void    *runtime_get_output(Runtime *rt, int task_id);

/// Get global singleton runtime
Runtime *runtime_global(void);

/// Create a future that resolves immediately with a value
Future  *future_ready(void *val, size_t size, struct Type *type, Arena *arena);

/// Create a future that never resolves (for testing)
Future  *future_pending(Arena *arena);

/// Map a future's output through a function
Future  *future_map(Future *f, void *(*fn)(void*), Arena *arena);

/// Chain two futures: first.then(second)
Future  *future_then(Future *first, Future *(*fn)(void*), Arena *arena);

/// Join multiple futures: wait for all to complete
Future  *future_join_all(Future **futures, int count, Arena *arena);

/// Poll a future manually
bool     future_poll(Future *f, Waker *waker);

/// Free a future and its output
void     future_free(Future *f, Arena *arena);

// ─── Waker constructors ──────────────────────────────────────────────────────

/// Create a waker that schedules the task for re-polling
Waker   *waker_task(Task *task, Arena *arena);

/// Create a waker that does nothing (for blocking contexts)
Waker   *waker_noop(Arena *arena);

// ─── Event Loop helpers ──────────────────────────────────────────────────────

/// Create an event loop
EventLoop *event_loop_new(Arena *arena);

/// Add a task to the event loop
void       event_loop_add_task(EventLoop *loop, Task *task);

/// Remove a completed task
void       event_loop_remove_task(EventLoop *loop, int task_id);

#endif
