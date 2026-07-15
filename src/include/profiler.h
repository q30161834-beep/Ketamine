#ifndef KETAMINE_PROFILER_H
#define KETAMINE_PROFILER_H

#include "types.h"

// ─── Built-in Profiler ────────────────────────────────────────────────────
// Instruments compiled programs to measure per-function CPU/memory usage.
// Generates flamegraphs (SVG) and perf-compatible output.

typedef struct {
    const char *name;
    const char *file;
    int         line;
    uint64_t    calls;
    uint64_t    total_cycles;      // CPU cycles spent
    uint64_t    self_cycles;       // excluding children
    uint64_t    alloc_bytes;       // memory allocated
    int         alloc_count;
    uint64_t    max_stack;         // max stack depth
} ProfileEntry;

typedef struct {
    ProfileEntry *entries;
    int           count;
    int           capacity;
    int           enabled;
    int           sample_interval; // microseconds
} ProfilerData;

// ─── API ──────────────────────────────────────────────────────────────────

/// Initialize profiler
ProfilerData *profiler_init(int sample_interval_us);

/// Start profiling a function
void profiler_enter(const char *name, const char *file, int line);

/// Stop profiling current function
void profiler_exit(void);

/// Write flamegraph data (stack sample format)
int profiler_write_flamegraph(ProfilerData *pd, const char *path);

/// Write collapsed stack format (for flamegraph.pl)
int profiler_write_collapsed(ProfilerData *pd, const char *path);

/// Write perf-compatible profile data
int profiler_write_perf(ProfilerData *pd, const char *path);

/// Print profile summary
void profiler_summary(ProfilerData *pd);

/// Free profiler data
void profiler_free(ProfilerData *pd);

#endif
