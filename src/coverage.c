#include "include/coverage.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

CoverageData *coverage_init(const char *output_path) {
    CoverageData *cd = (CoverageData*)calloc(1, sizeof(CoverageData));
    cd->capacity = 1024;
    cd->entries = (CoverageEntry*)calloc((size_t)cd->capacity, sizeof(CoverageEntry));
    cd->output_path = output_path;
    return cd;
}

void coverage_hit(CoverageData *cd, const char *file, int line) {
    for (int i = 0; i < cd->count; i++) {
        if (cd->entries[i].line == line &&
            strcmp(cd->entries[i].file, file) == 0) {
            cd->entries[i].count++;
            return;
        }
    }

    if (cd->count >= cd->capacity) {
        cd->capacity *= 2;
        cd->entries = (CoverageEntry*)realloc(cd->entries,
                        (size_t)cd->capacity * sizeof(CoverageEntry));
    }

    CoverageEntry *e = &cd->entries[cd->count++];
    e->file = file;
    e->line = line;
    e->count = 1;
    e->is_function = false;
}

void coverage_register_function(CoverageData *cd, const char *file,
                                const char *name, int line) {
    if (cd->count >= cd->capacity) {
        cd->capacity *= 2;
        cd->entries = (CoverageEntry*)realloc(cd->entries,
                        (size_t)cd->capacity * sizeof(CoverageEntry));
    }

    CoverageEntry *e = &cd->entries[cd->count++];
    e->file = file;
    e->line = line;
    e->count = 0;
    e->is_function = true;
    e->function_name = name;
}

int coverage_write_lcov(CoverageData *cd, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 1;

    const char *current_file = NULL;

    for (int i = 0; i < cd->count; i++) {
        CoverageEntry *e = &cd->entries[i];

        if (!current_file || strcmp(current_file, e->file) != 0) {
            if (current_file) fprintf(f, "end_of_record\n");
            current_file = e->file;
            fprintf(f, "SF:%s\n", e->file);
        }

        if (e->is_function) {
            fprintf(f, "FN:%d,%s\n", e->line, e->function_name);
            fprintf(f, "FNF:1\nFNH:%d\n", e->count > 0 ? 1 : 0);
        } else {
            fprintf(f, "DA:%d,%d\n", e->line, e->count);
        }
    }

    if (current_file) fprintf(f, "end_of_record\n");
    fclose(f);
    return 0;
}

int coverage_write_html(CoverageData *cd, const char *dir) {
    (void)dir;
    // genhtml would consume the .lcov file
    coverage_write_lcov(cd, "coverage.lcov");
    return 0;
}

void coverage_summary(CoverageData *cd) {
    int total_lines = 0, hit_lines = 0;
    int total_fns = 0, hit_fns = 0;

    for (int i = 0; i < cd->count; i++) {
        CoverageEntry *e = &cd->entries[i];
        if (e->is_function) {
            total_fns++;
            if (e->count > 0) hit_fns++;
        } else {
            total_lines++;
            if (e->count > 0) hit_lines++;
        }
    }

    printf("\n─── Coverage Summary ───\n");
    printf("  Lines:     %d / %d (%.1f%%)\n",
           hit_lines, total_lines,
           total_lines > 0 ? (double)hit_lines / total_lines * 100.0 : 0);
    printf("  Functions: %d / %d (%.1f%%)\n",
           hit_fns, total_fns,
           total_fns > 0 ? (double)hit_fns / total_fns * 100.0 : 0);
}

void coverage_free(CoverageData *cd) {
    if (cd) {
        free(cd->entries);
        free(cd);
    }
}
