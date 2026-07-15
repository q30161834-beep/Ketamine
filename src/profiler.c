#include "include/profiler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
  #include <windows.h>
  static uint64_t prof_cycles(void) {
      LARGE_INTEGER freq, count;
      QueryPerformanceFrequency(&freq);
      QueryPerformanceCounter(&count);
      return count.QuadPart;
  }
#else
  #include <time.h>
  static uint64_t prof_cycles(void) {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
  }
#endif

#define PROF_MAX_DEPTH 256
static ProfilerData *g_prof = NULL;
static const char *g_stack[PROF_MAX_DEPTH];
static uint64_t g_stack_start[PROF_MAX_DEPTH];
static int g_stack_depth = 0;

ProfilerData *profiler_init(int sample_interval_us) {
    ProfilerData *pd = (ProfilerData*)calloc(1, sizeof(ProfilerData));
    pd->capacity = 1024;
    pd->entries = (ProfileEntry*)calloc((size_t)pd->capacity, sizeof(ProfileEntry));
    pd->sample_interval = sample_interval_us > 0 ? sample_interval_us : 100;
    pd->enabled = 1;
    g_prof = pd;
    return pd;
}

void profiler_enter(const char *name, const char *file, int line) {
    if (!g_prof || !g_prof->enabled) return;
    if (g_stack_depth >= PROF_MAX_DEPTH) return;

    // Find or create entry
    ProfileEntry *entry = NULL;
    for (int i = 0; i < g_prof->count; i++) {
        if (strcmp(g_prof->entries[i].name, name) == 0) {
            entry = &g_prof->entries[i];
            break;
        }
    }

    if (!entry) {
        if (g_prof->count >= g_prof->capacity) {
            g_prof->capacity *= 2;
            g_prof->entries = (ProfileEntry*)realloc(g_prof->entries,
                            (size_t)g_prof->capacity * sizeof(ProfileEntry));
        }
        entry = &g_prof->entries[g_prof->count++];
        entry->name = name;
        entry->file = file;
        entry->line = line;
    }

    g_stack[g_stack_depth] = name;
    g_stack_start[g_stack_depth] = prof_cycles();
    g_stack_depth++;
    entry->calls++;
}

void profiler_exit(void) {
    if (!g_prof || !g_prof->enabled || g_stack_depth <= 0) return;

    g_stack_depth--;
    const char *name = g_stack[g_stack_depth];
    uint64_t elapsed = prof_cycles() - g_stack_start[g_stack_depth];

    for (int i = 0; i < g_prof->count; i++) {
        if (strcmp(g_prof->entries[i].name, name) == 0) {
            g_prof->entries[i].total_cycles += elapsed;
            g_prof->entries[i].self_cycles += elapsed;
            // Subtract child time
            if (g_stack_depth > 0) {
                for (int j = 0; j < g_prof->count; j++) {
                    if (strcmp(g_prof->entries[j].name, g_stack[g_stack_depth - 1]) == 0) {
                        g_prof->entries[j].self_cycles -= elapsed;
                        break;
                    }
                }
            }
            break;
        }
    }
}

int profiler_write_flamegraph(ProfilerData *pd, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 1;

    // Write SVG flamegraph header
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" "
               "viewBox=\"0 0 1200 800\">\n");
    fprintf(f, "<rect width=\"1200\" height=\"800\" fill=\"#eee\"/>\n");
    fprintf(f, "<text x=\"10\" y=\"20\" font-family=\"mono\" font-size=\"14\">");
    fprintf(f, "Ketamine Profile</text>\n");

    // Simple flamegraph visualization
    int y = 40;
    uint64_t max_time = 0;
    for (int i = 0; i < pd->count; i++) {
        if (pd->entries[i].total_cycles > max_time)
            max_time = pd->entries[i].total_cycles;
    }

    for (int i = 0; i < pd->count && y < 780; i++) {
        ProfileEntry *e = &pd->entries[i];
        if (max_time == 0) continue;

        int width = (int)((double)e->total_cycles / max_time * 1180);
        if (width < 2) width = 2;

        uint8_t r = (uint8_t)((double)i / pd->count * 200 + 55);
        uint8_t g = (uint8_t)(100 - (double)i / pd->count * 50);
        uint8_t b = 80;

        fprintf(f, "<rect x=\"10\" y=\"%d\" width=\"%d\" height=\"20\" "
                   "fill=\"#%02x%02x%02x\" rx=\"3\"/>\n",
                y, width, r, g, b);
        fprintf(f, "<text x=\"15\" y=\"%d\" font-family=\"mono\" "
                   "font-size=\"12\" fill=\"white\">%s</text>\n",
                y + 14, e->name);
        y += 24;
    }

    fprintf(f, "</svg>\n");
    fclose(f);

    // Also write collapsed format for flamegraph.pl
    char collapsed_path[512];
    snprintf(collapsed_path, sizeof(collapsed_path), "%.*s.collapsed",
             (int)(strrchr(path, '.') ? strrchr(path, '.') - path : (int)strlen(path)),
             path);
    profiler_write_collapsed(pd, collapsed_path);

    return 0;
}

int profiler_write_collapsed(ProfilerData *pd, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 1;

    for (int i = 0; i < pd->count; i++) {
        ProfileEntry *e = &pd->entries[i];
        fprintf(f, "%s;%s %llu\n",
                e->file ? e->file : "?",
                e->name,
                (unsigned long long)(e->self_cycles / 1000));
    }

    fclose(f);
    return 0;
}

int profiler_write_perf(ProfilerData *pd, const char *path) {
    (void)pd;
    (void)path;
    return 0;
}

void profiler_summary(ProfilerData *pd) {
    printf("\n─── Profile Summary ───\n");
    printf("%-30s %12s %12s %8s\n", "Function", "Calls", "Total (us)", "Self (us)");
    printf("%-30s %12s %12s %8s\n", "────────", "─────", "─────────", "────────");

    for (int i = 0; i < pd->count; i++) {
        ProfileEntry *e = &pd->entries[i];
        printf("%-30s %12llu %12llu %8llu\n",
               e->name,
               (unsigned long long)e->calls,
               (unsigned long long)(e->total_cycles / 1000),
               (unsigned long long)(e->self_cycles / 1000));
    }
}

void profiler_free(ProfilerData *pd) {
    if (pd) {
        free(pd->entries);
        free(pd);
        g_prof = NULL;
    }
}
