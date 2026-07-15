#include "include/runtime.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// RUNTIME — Async event loop, Future<T>, spawn/block_on
// ═══════════════════════════════════════════════════════════════════════════════

static Runtime *g_runtime = NULL;

// ─── Event Loop ───────────────────────────────────────────────────────────────
EventLoop *event_loop_new(Arena *arena) {
    EventLoop *loop = (EventLoop*)arena_alloc_zero(arena, sizeof(EventLoop));
    loop->arena = arena;
    loop->running = false;
    return loop;
}

void event_loop_add_task(EventLoop *loop, Task *task) {
    task->next = loop->tasks;
    loop->tasks = task;
    loop->task_count++;
}

void event_loop_remove_task(EventLoop *loop, int task_id) {
    Task *prev = NULL;
    for (Task *t = loop->tasks; t; prev = t, t = t->next) {
        if (t->id == task_id) {
            if (prev) prev->next = t->next;
            else loop->tasks = t->next;
            loop->task_count--;
            return;
        }
    }
}

// ─── Waker ────────────────────────────────────────────────────────────────────
static void task_waker_wake(Waker *self) {
    Task *task = (Task*)self->data;
    task->future->state = FUTURE_PENDING; // reschedule
}

static void noop_waker_wake(Waker *self) {
    (void)self;
}

Waker *waker_task(Task *task, Arena *arena) {
    Waker *w = (Waker*)arena_alloc_zero(arena, sizeof(Waker));
    w->wake = task_waker_wake;
    w->data = task;
    return w;
}

Waker *waker_noop(Arena *arena) {
    Waker *w = (Waker*)arena_alloc_zero(arena, sizeof(Waker));
    w->wake = noop_waker_wake;
    w->data = NULL;
    return w;
}

// ─── Runtime ──────────────────────────────────────────────────────────────────
Runtime *runtime_init(Arena *arena) {
    Runtime *rt = (Runtime*)arena_alloc_zero(arena, sizeof(Runtime));
    rt->loop = event_loop_new(arena);
    rt->arena = arena;
    rt->initialized = true;
    g_runtime = rt;
    return rt;
}

int runtime_spawn(Runtime *rt, Future *future) {
    Task *task = (Task*)arena_alloc_zero(rt->arena, sizeof(Task));
    task->id = rt->next_task_id++;
    task->future = future;
    task->waker = waker_task(task, rt->arena);
    task->is_completed = false;
    event_loop_add_task(rt->loop, task);
    return task->id;
}

void *runtime_block_on(Runtime *rt, int task_id) {
    while (1) {
        // Find the task
        Task *target = NULL;
        for (Task *t = rt->loop->tasks; t; t = t->next) {
            if (t->id == task_id) { target = t; break; }
        }
        if (!target) return NULL;

        // Poll it
        if (target->future->state != FUTURE_READY) {
            target->future->poll(target->future, target->waker);
        }

        if (target->future->state == FUTURE_READY) {
            target->is_completed = true;
            return target->future->output;
        }

        // Tick other tasks while waiting
        runtime_tick(rt);
    }
}

int runtime_tick(Runtime *rt) {
    int polled = 0;
    for (Task *t = rt->loop->tasks; t; t = t->next) {
        if (t->is_completed) continue;
        if (t->future->state == FUTURE_READY) {
            t->is_completed = true;
            polled++;
            continue;
        }
        t->future->poll(t->future, t->waker);
        if (t->future->state == FUTURE_READY) {
            t->is_completed = true;
        }
        polled++;
    }
    return polled;
}

void runtime_run(Runtime *rt) {
    rt->loop->running = true;
    while (rt->loop->task_count > 0) {
        runtime_tick(rt);
        // Remove completed tasks
        Task *prev = NULL;
        for (Task *t = rt->loop->tasks; t; ) {
            Task *next = t->next;
            if (t->is_completed) {
                if (prev) prev->next = next;
                else rt->loop->tasks = next;
                rt->loop->task_count--;
            } else {
                prev = t;
            }
            t = next;
        }
    }
    rt->loop->running = false;
}

void *runtime_get_output(Runtime *rt, int task_id) {
    for (Task *t = rt->loop->tasks; t; t = t->next) {
        if (t->id == task_id && t->is_completed) {
            return t->future->output;
        }
    }
    return NULL;
}

Runtime *runtime_global(void) {
    return g_runtime;
}

// ─── Future operations ────────────────────────────────────────────────────────
static void ready_future_poll(Future *self, Waker *waker) {
    (void)waker;
    self->state = FUTURE_READY;
}

static void ready_future_drop(Future *self) {
    if (self->output) free(self->output);
}

Future *future_ready(void *val, size_t size, struct Type *type, Arena *arena) {
    Future *f = (Future*)arena_alloc_zero(arena, sizeof(Future));
    f->state = FUTURE_READY;
    f->poll = ready_future_poll;
    f->drop = ready_future_drop;
    f->output = malloc(size);
    if (f->output && val) memcpy(f->output, val, size);
    f->output_size = size;
    f->output_type = type;
    return f;
}

static void pending_future_poll(Future *self, Waker *waker) {
    (void)waker;
    // Never resolves
    self->state = FUTURE_PENDING;
}

static void pending_future_drop(Future *self) {
    (void)self;
}

Future *future_pending(Arena *arena) {
    Future *f = (Future*)arena_alloc_zero(arena, sizeof(Future));
    f->state = FUTURE_PENDING;
    f->poll = pending_future_poll;
    f->drop = pending_future_drop;
    return f;
}

bool future_poll(Future *f, Waker *waker) {
    if (f->state == FUTURE_READY) return true;
    f->poll(f, waker);
    return f->state == FUTURE_READY;
}

void future_free(Future *f, Arena *arena) {
    (void)arena;
    if (f->drop) f->drop(f);
}
