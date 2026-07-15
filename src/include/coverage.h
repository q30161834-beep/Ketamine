#ifndef KETAMINE_COVERAGE_H
#define KETAMINE_COVERAGE_H

#include "types.h"

// ─── Code Coverage Instrumentation ─────────────────────────────────────────
// Instruments compiled code to track which lines/functions are executed.
// Generates LCOV-compatible coverage reports.

typedef struct {
    const char *file;
    int         line;
    int         count;      // how many times this line was hit
    bool        is_function;
    const char *function_name;
} CoverageEntry;

typedef struct {
    CoverageEntry *entries;
    int            count;
    int            capacity;
    const char    *output_path;
} CoverageData;

// ─── API ──────────────────────────────────────────────────────────────────

/// Initialize coverage tracking
CoverageData *coverage_init(const char *output_path);

/// Register a line/function as executed
void coverage_hit(CoverageData *cd, const char *file, int line);

/// Register a function entry
void coverage_register_function(CoverageData *cd, const char *file,
                                const char *name, int line);

/// Write coverage report in LCOV format
int coverage_write_lcov(CoverageData *cd, const char *path);

/// Write coverage report as HTML (via genhtml-compatible output)
int coverage_write_html(CoverageData *cd, const char *dir);

/// Generate coverage summary
void coverage_summary(CoverageData *cd);

/// Free coverage data
void coverage_free(CoverageData *cd);

#endif
