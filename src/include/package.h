#ifndef KETAMINE_PACKAGE_H
#define KETAMINE_PACKAGE_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// PACKAGE MANAGER — Manifest, dependency resolution, registry client
// ═══════════════════════════════════════════════════════════════════════════════

#define KET_MANIFEST_FILE     "ket.json"
#define KET_LOCK_FILE         "ket.lock"
#define KET_PKG_DIR           "ket_packages"
#define KET_REGISTRY_URL      "https://registry.ketamine-lang.org"
#define KET_VERSION_ANY       "*"
#define KET_VERSION_LATEST    "latest"

// ─── Version ──────────────────────────────────────────────────────────────────
typedef struct {
    int major;
    int minor;
    int patch;
    const char *pre_release;  // e.g., "alpha.1", NULL for stable
} PkgVersion;

// ─── Version Constraint ───────────────────────────────────────────────────────
typedef enum {
    VC_EXACT,        // =1.0.0
    VC_CARET,        // ^1.0.0 (compatible)
    VC_TILDE,        // ~1.0.0 (patch-level)
    VC_ANY,          // *
    VC_GREATER,      // >1.0.0
    VC_LESS,         // <1.0.0
    VC_RANGE,        // >=1.0.0 <2.0.0
} VerConstraintKind;

typedef struct {
    VerConstraintKind kind;
    PkgVersion        version;
    PkgVersion        version_high;   // for range
} VerConstraint;

// ─── Dependency ───────────────────────────────────────────────────────────────
typedef struct {
    const char    *name;
    VerConstraint  constraint;
    const char    *source;       // "registry", "git", "path"
    const char    *source_url;   // git URL or local path
    const char    *features;     // feature flags
    bool           optional;
} Dependency;

// ─── Package ──────────────────────────────────────────────────────────────────
typedef struct {
    // From manifest
    PkgVersion    version;
    const char    *name;
    const char    *description;
    const char    *license;
    const char    *entry_point;   // main module file
    const char    **authors;
    int            author_count;
    Dependency    *dependencies;
    int            dep_count;

    // Resolved
    const char    *install_path;

    // Metadata
    Context       *ctx;
} Package;

// ─── Manifest ─────────────────────────────────────────────────────────────────
typedef struct {
    Package        pkg;
    // Raw parsed data
    const char    *raw_json;
} Manifest;

// ─── Registry ─────────────────────────────────────────────────────────────────
typedef struct {
    const char    *url;
    const char    *cache_path;
    bool           use_cache;
    Context       *ctx;
} Registry;

// ─── Resolved Dependency Graph ────────────────────────────────────────────────
typedef struct DepNode {
    const char        *name;
    PkgVersion         version;
    struct DepNode   **deps;
    int                dep_count;
    const char        *path;
    bool               resolved;
} DepNode;

// ═══════════════════════════════════════════════════════════════════════════════
// API
// ═══════════════════════════════════════════════════════════════════════════════

/// Parse a manifest file from JSON
Manifest *manifest_parse(const char *path, Context *ctx);

/// Create a new manifest with default values
Manifest *manifest_create(const char *name, const char *version_str, Context *ctx);

/// Write manifest to file
bool manifest_write(Manifest *m, const char *path);

/// Resolve a dependency from the registry
bool registry_resolve(Registry *reg, const char *name, VerConstraint *constraint,
                      Package *out);

/// Download a package from the registry
bool registry_download(Registry *reg, const char *name, PkgVersion *version,
                       const char *dest_path);

/// Resolve all dependencies in a dependency graph
DepNode *dep_graph_resolve(Package *pkg, Registry *reg);

/// Install a package and all its dependencies (recursive)
bool package_install(Package *pkg, Registry *reg);

/// Build a package (compile all source files)
bool package_build(Package *pkg, CompileOptions *opts);

/// Create a new package project
bool package_init(const char *path, const char *name, const char *version);

/// Add a dependency to the manifest
bool package_add_dep(Manifest *m, const char *name, const char *version_constraint);

/// Run a package (compile + execute)
bool package_run(Package *pkg, CompileOptions *opts);

/// Version parsing utilities
bool version_parse(const char *str, PkgVersion *out);
int  version_compare(PkgVersion *a, PkgVersion *b);
bool version_satisfies(PkgVersion *v, VerConstraint *c);
const char *version_to_string(PkgVersion *v, Arena *arena);

/// Check if a string is a valid package name
bool package_name_valid(const char *name);

/// Initialize registry client
Registry *registry_new(Context *ctx);

#endif // KETAMINE_PACKAGE_H
