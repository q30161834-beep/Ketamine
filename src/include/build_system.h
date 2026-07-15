#ifndef KETAMINE_BUILD_SYSTEM_H
#define KETAMINE_BUILD_SYSTEM_H

#include "types.h"

// ─── Advanced Build System ─────────────────────────────────────────────────
// Supports: incremental builds, cross-compilation, release/profile builds.

typedef enum {
    BUILD_DEBUG,
    BUILD_RELEASE,
    BUILD_PROFILE,
    BUILD_COVERAGE,
    BUILD_SIZE      // optimize for size
} BuildProfile;

typedef struct {
    const char     *target_triple;    // e.g., x86_64-unknown-linux-gnu
    BuildProfile    profile;
    const char     *output_dir;
    const char    **source_dirs;
    int             source_dir_count;
    const char    **include_dirs;
    int             include_dir_count;
    const char    **link_libs;
    int             link_lib_count;
    bool            incremental;
    bool            emit_llvm;
    bool            emit_asm;
    bool            strip_symbols;
    int             jobs;
    TargetBackend   target_backend;
    OptLevel        opt_level;
} BuildConfig;

typedef struct {
    const char    *source_path;
    const char    *object_path;
    const char    *dep_path;      // dependency file for incremental build
    uint64_t       source_mtime;  // last modified time
    bool           needs_rebuild;
} BuildUnit;

typedef struct {
    BuildUnit     *units;
    int            count;
    int            capacity;
    BuildConfig    config;
    int            errors;
} BuildProject;

// ─── API ──────────────────────────────────────────────────────────────────

/// Create a build project from a config
BuildProject *build_project_new(BuildConfig *config);

/// Scan source directories and create build units
int build_scan_sources(BuildProject *proj);

/// Check which files need rebuilding (incremental)
void build_check_incremental(BuildProject *proj);

/// Build all units (compile + link)
int build_all(BuildProject *proj);

/// Clean build artifacts
int build_clean(BuildProject *proj);

/// Cross-compile for a different target
int build_cross_compile(BuildProject *proj, const char *target);

/// Run a single compilation unit
int build_compile_unit(BuildProject *proj, int unit_idx);

/// Link all objects into final binary
int build_link(BuildProject *proj);

/// Write dependency file for incremental builds
void build_write_depfile(BuildProject *proj, int unit_idx);

#endif
