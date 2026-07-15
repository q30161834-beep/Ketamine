#ifndef KETAMINE_REGISTRY_H
#define KETAMINE_REGISTRY_H

#include "types.h"

// ─── Package Registry System ───────────────────────────────────────────────
// Central package registry for Ketamine.
// `ket add <pkg>` / `ket publish` / lock file support.

#define MAX_PKG_NAME 128
#define MAX_PKG_VERSION 32
#define MAX_PKG_DEPENDENCIES 64
#define MAX_REGISTRY_URL 512

// ─── Version Constraint ───────────────────────────────────────────────────
typedef enum {
    CONSTRAINT_EXACT,   // =1.0.0
    CONSTRAINT_CARET,   // ^1.0.0 (>=1.0.0, <2.0.0)
    CONSTRAINT_TILDE,   // ~1.0.0 (>=1.0.0, <1.1.0)
    CONSTRAINT_GE,      // >=1.0.0
    CONSTRAINT_GT,      // >1.0.0
    CONSTRAINT_LE,      // <=1.0.0
    CONSTRAINT_LT,      // <1.0.0
    CONSTRAINT_WILDCARD // *
} VersionConstraint;

typedef struct {
    char               name[MAX_PKG_NAME];
    char               version[MAX_PKG_VERSION];
    VersionConstraint  constraint;
} PkgDependency;

// ─── Package Metadata ─────────────────────────────────────────────────────
typedef struct {
    char             name[MAX_PKG_NAME];
    char             version[MAX_PKG_VERSION];
    char             description[256];
    char             author[128];
    char             license[64];
    char             repo_url[256];
    char             documentation[256];
    PkgDependency    dependencies[MAX_PKG_DEPENDENCIES];
    int              dep_count;
    char             checksum[65];        // SHA-256 hex
    char             download_url[512];
    bool             is_git_dep;
    char             git_url[512];
} PackageMeta;

// ─── Lock File Entry ──────────────────────────────────────────────────────
typedef struct {
    char    name[MAX_PKG_NAME];
    char    version[MAX_PKG_VERSION];
    char    checksum[65];
    char    resolved_url[512];
} LockEntry;

typedef struct {
    LockEntry   entries[256];
    int         count;
} LockFile;

// ─── API ──────────────────────────────────────────────────────────────────

/// Parse a version string with optional constraint prefix
bool registry_parse_version(const char *s, char *out_version, VersionConstraint *out_constraint);

/// Check if a version satisfies a constraint
bool registry_version_satisfies(const char *version, const char *required, VersionConstraint constraint);

/// Compare two semver versions (returns -1, 0, +1)
int registry_version_compare(const char *a, const char *b);

/// Download a package from the registry
int registry_download(const char *name, const char *version, const char *output_dir);

/// Publish a package to the registry
int registry_publish(const char *pkg_dir, const char *registry_url);

/// Resolve dependencies for a package (recursive)
int registry_resolve_deps(PackageMeta *meta, const char *cache_dir);

/// Generate a lock file from resolved dependencies
bool registry_write_lock(const char *path, LockEntry *entries, int count);

/// Read a lock file
bool registry_read_lock(const char *path, LockFile *lock);

/// Install dependencies from lock file
int registry_install_from_lock(const char *lock_path, const char *output_dir);

/// Fetch registry index from URL
int registry_fetch_index(const char *registry_url, PackageMeta **out_pkgs, int *out_count);

#endif
