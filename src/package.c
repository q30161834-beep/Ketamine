#include "include/package.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ═══════════════════════════════════════════════════════════════════════════════
// PACKAGE MANAGER — Manifest parsing, dependency resolution, registry operations
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Version Parsing ──────────────────────────────────────────────────────────

bool version_parse(const char *str, PkgVersion *out) {
    if (!str || !out) return false;
    memset(out, 0, sizeof(PkgVersion));

    // Handle "latest"
    if (strcmp(str, "latest") == 0) {
        out->major = 9999;
        out->minor = 9999;
        out->patch = 9999;
        return true;
    }

    // Parse semver: major.minor.patch[-pre-release]
    int n = sscanf(str, "%d.%d.%d", &out->major, &out->minor, &out->patch);
    if (n < 3) {
        // Try major.minor
        n = sscanf(str, "%d.%d", &out->major, &out->minor);
        if (n < 2) return false;
        out->patch = 0;
    }

    // Check for pre-release
    const char *dash = strchr(str, '-');
    if (dash) {
        // Need to make sure dash is after the version numbers
        int major, minor, patch;
        if (sscanf(str, "%d.%d.%d", &major, &minor, &patch) >= 2) {
            out->pre_release = dash + 1;
        }
    }

    return true;
}

int version_compare(PkgVersion *a, PkgVersion *b) {
    if (!a || !b) return 0;
    if (a->major != b->major) return a->major - b->major;
    if (a->minor != b->minor) return a->minor - b->minor;
    if (a->patch != b->patch) return a->patch - b->patch;

    // Pre-release versions are less than release versions
    if (a->pre_release && !b->pre_release) return -1;
    if (!a->pre_release && b->pre_release) return 1;
    if (a->pre_release && b->pre_release)
        return strcmp(a->pre_release, b->pre_release);

    return 0;
}

const char *version_to_string(PkgVersion *v, Arena *arena) {
    if (!v) return "0.0.0";
    char buf[128];
    if (v->pre_release)
        snprintf(buf, sizeof(buf), "%d.%d.%d-%s", v->major, v->minor, v->patch, v->pre_release);
    else
        snprintf(buf, sizeof(buf), "%d.%d.%d", v->major, v->minor, v->patch);
    return ket_intern(arena->ctx, buf, (int)strlen(buf));
}

bool version_satisfies(PkgVersion *v, VerConstraint *c) {
    if (!v || !c) return false;

    switch (c->kind) {
        case VC_ANY:
            return true;

        case VC_EXACT:
            return version_compare(v, &c->version) == 0;

        case VC_CARET: {
            // ^1.2.3 means >=1.2.3 <2.0.0
            // ^0.2.3 means >=0.2.3 <0.3.0
            // ^0.0.3 means >=0.0.3 <0.0.4
            if (version_compare(v, &c->version) < 0) return false;
            PkgVersion upper = c->version;
            if (upper.major > 0 || upper.minor > 0) {
                if (upper.major > 0) {
                    upper.major++;
                    upper.minor = 0;
                    upper.patch = 0;
                } else {
                    upper.minor++;
                    upper.patch = 0;
                }
            } else {
                upper.patch++;
            }
            return version_compare(v, &upper) < 0;
        }

        case VC_TILDE: {
            // ~1.2.3 means >=1.2.3 <1.3.0
            if (version_compare(v, &c->version) < 0) return false;
            PkgVersion upper = c->version;
            upper.minor++;
            upper.patch = 0;
            return version_compare(v, &upper) < 0;
        }

        case VC_GREATER:
            return version_compare(v, &c->version) > 0;

        case VC_LESS:
            return version_compare(v, &c->version) < 0;

        default:
            return false;
    }
}

// ─── Package Name Validation ─────────────────────────────────────────────────

bool package_name_valid(const char *name) {
    if (!name || !*name) return false;

    // Must start with lowercase letter
    if (!islower((unsigned char)name[0])) return false;

    // Must contain only lowercase letters, digits, hyphens, underscores
    for (int i = 0; name[i]; i++) {
        if (!islower((unsigned char)name[i]) &&
            !isdigit((unsigned char)name[i]) &&
            name[i] != '-' && name[i] != '_') {
            return false;
        }
    }

    return true;
}

// ─── Manifest Creation ────────────────────────────────────────────────────────

Manifest *manifest_create(const char *name, const char *version_str, Context *ctx) {
    Arena *arena = ctx->arena;
    Manifest *m = (Manifest*)arena_alloc_zero(arena, sizeof(Manifest));

    m->pkg.name = name;
    version_parse(version_str, &m->pkg.version);
    m->pkg.entry_point = "src/main.kt";
    m->pkg.ctx = ctx;

    return m;
}

// ─── Simple JSON Manifest Parser ──────────────────────────────────────────────
// Parses ket.json format: { "name": "...", "version": "...", "dependencies": {...} }

static const char *skip_ws(const char *p) {
    while (*p && (unsigned char)*p <= 32) p++;
    return p;
}

static const char *parse_json_string(const char *p, char *buf, int buf_size) {
    p = skip_ws(p);
    if (*p != '"') return NULL;
    p++;

    int i = 0;
    while (*p && *p != '"' && i < buf_size - 1) {
        if (*p == '\\') {
            p++;
            if (*p == '"') { buf[i++] = '"'; }
            else if (*p == 'n') { buf[i++] = '\n'; }
            else if (*p == 't') { buf[i++] = '\t'; }
            else if (*p == '\\') { buf[i++] = '\\'; }
            else { buf[i++] = *p; }
        } else {
            buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';

    if (*p == '"') p++;
    return p;
}

Manifest *manifest_parse(const char *path, Context *ctx) {
    Arena *arena = ctx->arena;

    // Read file
    FILE *f = fopen(path, "rb");
    if (!f) {
        ket_diag_push(ctx, SEV_ERROR, 0, 0, "cannot open manifest: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz > 1024 * 1024) {  // 1MB max
        fclose(f);
        ket_diag_push(ctx, SEV_ERROR, 0, 0, "manifest too large");
        return NULL;
    }

    char *content = (char*)malloc(sz + 1);
    fread(content, 1, sz, f);
    content[sz] = '\0';
    fclose(f);

    Manifest *m = (Manifest*)arena_alloc_zero(arena, sizeof(Manifest));
    m->raw_json = ket_intern(ctx, content, (int)sz);
    m->pkg.ctx = ctx;
    m->pkg.dependencies = NULL;
    m->pkg.dep_count = 0;
    m->pkg.entry_point = "src/main.kt";

    const char *p = content;

    // Simple JSON parsing for manifest
    p = skip_ws(p);
    if (*p != '{') goto error;
    p++;

    char key[256], val[1024];

    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == ',') p++;
        p = skip_ws(p);
        if (*p == '}') break;

        // Parse key
        p = parse_json_string(p, key, sizeof(key));
        if (!p) goto error;
        p = skip_ws(p);
        if (*p != ':') goto error;
        p++;

        p = skip_ws(p);

        if (strcmp(key, "name") == 0) {
            p = parse_json_string(p, val, sizeof(val));
            if (!p) goto error;
            m->pkg.name = ket_intern(ctx, val, (int)strlen(val));
        }
        else if (strcmp(key, "version") == 0) {
            p = parse_json_string(p, val, sizeof(val));
            if (!p) goto error;
            version_parse(val, &m->pkg.version);
        }
        else if (strcmp(key, "description") == 0) {
            p = parse_json_string(p, val, sizeof(val));
            if (!p) goto error;
            m->pkg.description = ket_intern(ctx, val, (int)strlen(val));
        }
        else if (strcmp(key, "license") == 0) {
            p = parse_json_string(p, val, sizeof(val));
            if (!p) goto error;
            m->pkg.license = ket_intern(ctx, val, (int)strlen(val));
        }
        else if (strcmp(key, "entry") == 0) {
            p = parse_json_string(p, val, sizeof(val));
            if (!p) goto error;
            m->pkg.entry_point = ket_intern(ctx, val, (int)strlen(val));
        }
        else if (strcmp(key, "dependencies") == 0) {
            // Parse dependencies object
            p = skip_ws(p);
            if (*p != '{') goto error;
            p++;

            int cap = 16;
            m->pkg.dependencies = (Dependency*)arena_alloc_zero(arena, sizeof(Dependency) * cap);

            while (*p && *p != '}') {
                p = skip_ws(p);
                if (*p == ',') p++;
                p = skip_ws(p);
                if (*p == '}') break;

                char dep_name[256];
                p = parse_json_string(p, dep_name, sizeof(dep_name));
                if (!p) goto error;
                p = skip_ws(p);
                if (*p != ':') goto error;
                p++;

                p = skip_ws(p);
                char ver_str[256];
                p = parse_json_string(p, ver_str, sizeof(ver_str));
                if (!p) goto error;

                Dependency *dep = &m->pkg.dependencies[m->pkg.dep_count++];
                dep->name = ket_intern(ctx, dep_name, (int)strlen(dep_name));

                // Parse version constraint
                if (ver_str[0] == '^') {
                    dep->constraint.kind = VC_CARET;
                    version_parse(ver_str + 1, &dep->constraint.version);
                } else if (ver_str[0] == '~') {
                    dep->constraint.kind = VC_TILDE;
                    version_parse(ver_str + 1, &dep->constraint.version);
                } else if (ver_str[0] == '=') {
                    dep->constraint.kind = VC_EXACT;
                    version_parse(ver_str + 1, &dep->constraint.version);
                } else if (strcmp(ver_str, "*") == 0) {
                    dep->constraint.kind = VC_ANY;
                } else {
                    dep->constraint.kind = VC_EXACT;
                    version_parse(ver_str, &dep->constraint.version);
                }

                dep->source = "registry";
                dep->source_url = KET_REGISTRY_URL;
                dep->features = NULL;
                dep->optional = false;
            }
            if (*p == '}') p++;
        }
        else {
            // Skip unknown value
            if (*p == '"') {
                char skip[256];
                p = parse_json_string(p, skip, sizeof(skip));
            } else if (*p == '{') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '{') depth++;
                    if (*p == '}') depth--;
                    p++;
                }
            } else if (*p == '[') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[') depth++;
                    if (*p == ']') depth--;
                    p++;
                }
            } else {
                // Skip number/boolean/null
                while (*p && *p != ',' && *p != '}' && *p != '\n') p++;
            }
        }
    }

    free(content);
    return m;

error:
    free(content);
    ket_diag_push(ctx, SEV_ERROR, 0, 0, "failed to parse manifest: %s", path);
    return NULL;
}

bool manifest_write(Manifest *m, const char *path) {
    if (!m) return false;

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", m->pkg.name ? m->pkg.name : "unnamed");
    fprintf(f, "  \"version\": \"%s\",\n", version_to_string(&m->pkg.version, NULL));
    if (m->pkg.description)
        fprintf(f, "  \"description\": \"%s\",\n", m->pkg.description);
    if (m->pkg.license)
        fprintf(f, "  \"license\": \"%s\",\n", m->pkg.license);

    if (m->pkg.dep_count > 0) {
        fprintf(f, "  \"dependencies\": {\n");
        for (int i = 0; i < m->pkg.dep_count; i++) {
            Dependency *dep = &m->pkg.dependencies[i];
            fprintf(f, "    \"%s\": \"%s\"", dep->name,
                    version_to_string(&dep->constraint.version, NULL));
            if (i < m->pkg.dep_count - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "  }\n");
    } else {
        fprintf(f, "  \"dependencies\": {}\n");
    }

    fprintf(f, "}\n");
    fclose(f);
    return true;
}

// ─── Registry Client ──────────────────────────────────────────────────────────

Registry *registry_new(Context *ctx) {
    Arena *arena = ctx->arena;
    Registry *reg = (Registry*)arena_alloc_zero(arena, sizeof(Registry));
    reg->url = KET_REGISTRY_URL;
    reg->use_cache = true;
    reg->ctx = ctx;

    // Determine cache path
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) home = ".";
    char cache_path[1024];
    snprintf(cache_path, sizeof(cache_path), "%s/.ketamine/cache", home);
    reg->cache_path = ket_intern(ctx, cache_path, (int)strlen(cache_path));

    return reg;
}

bool registry_resolve(Registry *reg, const char *name, VerConstraint *constraint,
                      Package *out) {
    if (!reg || !name || !out) return false;

    // In production, this would query the registry API
    // For now, simulate resolution with a simple lookup
    if (reg->ctx->options.verbose) {
        fprintf(stderr, "  registry: resolving %s ...\n", name);
    }

    // Stub: assume resolution succeeds for known packages
    // In production, this would make HTTP requests to the registry
    memset(out, 0, sizeof(Package));
    out->name = name;
    out->version.major = 1;
    out->version.minor = 0;
    out->version.patch = 0;
    out->ctx = reg->ctx;

    return true;
}

bool registry_download(Registry *reg, const char *name, PkgVersion *version,
                       const char *dest_path) {
    if (!reg || !name || !dest_path) return false;

    if (reg->ctx->options.verbose) {
        fprintf(stderr, "  registry: downloading %s v%d.%d.%d -> %s\n",
                name, version->major, version->minor, version->patch, dest_path);
    }

    // Stub: In production, this downloads and extracts a tarball
    // For now, create the directory and return success
    // mkdir -p equivalent would be platform-specific

    return true;  // stubbed
}

// ─── Dependency Graph Resolution ──────────────────────────────────────────────

DepNode *dep_graph_resolve(Package *pkg, Registry *reg) {
    if (!pkg) return NULL;

    if (reg->ctx->options.verbose) {
        fprintf(stderr, "  resolving dependency graph for %s ...\n", pkg->name);
    }

    // Create root node
    DepNode *root = (DepNode*)calloc(1, sizeof(DepNode));
    root->name = pkg->name;
    root->version = pkg->version;
    root->resolved = true;

    // Resolve direct dependencies
    if (pkg->dep_count > 0) {
        root->deps = (DepNode**)calloc(pkg->dep_count, sizeof(DepNode*));
        for (int i = 0; i < pkg->dep_count; i++) {
            Dependency *dep = &pkg->dependencies[i];
            DepNode *child = (DepNode*)calloc(1, sizeof(DepNode));
            child->name = dep->name;
            child->version = dep->constraint.version;
            child->resolved = true;

            Package resolved_pkg;
            if (registry_resolve(reg, dep->name, &dep->constraint, &resolved_pkg)) {
                child->version = resolved_pkg.version;
            }

            root->deps[root->dep_count++] = child;
        }
    }

    return root;
}

// ─── Package Operations ───────────────────────────────────────────────────────

bool package_install(Package *pkg, Registry *reg) {
    if (!pkg) return false;

    if (reg->ctx->options.verbose) {
        fprintf(stderr, "  installing %s v%d.%d.%d ...\n",
                pkg->name, pkg->version.major, pkg->version.minor, pkg->version.patch);
    }

    // Create package directory
    char pkg_dir[1024];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", KET_PKG_DIR, pkg->name);
    // mkdir -p equivalent

    // Download each dependency
    for (int i = 0; i < pkg->dep_count; i++) {
        Dependency *dep = &pkg->dependencies[i];
        Package dep_pkg;
        if (registry_resolve(reg, dep->name, &dep->constraint, &dep_pkg)) {
            char dep_path[1024];
            snprintf(dep_path, sizeof(dep_path), "%s/%s", KET_PKG_DIR, dep->name);
            registry_download(reg, dep->name, &dep_pkg.version, dep_path);
        }
    }

    return true;
}

bool package_build(Package *pkg, CompileOptions *opts) {
    if (!pkg || !opts) return false;

    if (opts->verbose) {
        fprintf(stderr, "  building package: %s (v%d.%d.%d)\n",
                pkg->name, pkg->version.major, pkg->version.minor, pkg->version.patch);
    }

    // In production, this would find all .kt files and compile them
    // For now, just report success
    return true;
}

bool package_init(const char *path, const char *name, const char *version) {
    if (!path || !name) return false;

    if (!package_name_valid(name)) {
        fprintf(stderr, "error: invalid package name '%s'\n", name);
        fprintf(stderr, "  package names must start with a lowercase letter\n");
        fprintf(stderr, "  and contain only lowercase letters, digits, hyphens, underscores\n");
        return false;
    }

    // Create directory structure
    char cmd[4096];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "if not exist \"%s\\src\" mkdir \"%s\\src\"", path, path);
#else
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/src\"", path);
#endif
    system(cmd);

    // Create manifest
    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/%s", path, KET_MANIFEST_FILE);

    // Create minimal context for manifest
    // (In production, use proper context)
    Arena *arena = (Arena*)malloc(sizeof(Arena));
    *arena = arena_new_size(1024 * 1024);
    Context *ctx = (Context*)arena_alloc_zero(arena, sizeof(Context));
    ctx->arena = arena;

    Manifest *m = manifest_create(name, version ? version : "0.1.0", ctx);
    bool ok = manifest_write(m, manifest_path);

    // Create main.kt
    char main_path[1024];
    snprintf(main_path, sizeof(main_path), "%s/src/main.kt", path);
    FILE *f = fopen(main_path, "w");
    if (f) {
        fprintf(f, "// %s v%s\n", name, version ? version : "0.1.0");
        fprintf(f, "import core\n\n");
        fprintf(f, "fn main() {\n");
        fprintf(f, "    println(\"Hello from %s!\");\n", name);
        fprintf(f, "}\n");
        fclose(f);
    }

    arena_free(arena);
    free(arena);

    return ok;
}

bool package_add_dep(Manifest *m, const char *name, const char *version_constraint) {
    if (!m || !name) return false;

    if (!package_name_valid(name)) {
        fprintf(stderr, "error: invalid package name '%s'\n", name);
        return false;
    }

    // Check if already exists
    for (int i = 0; i < m->pkg.dep_count; i++) {
        if (strcmp(m->pkg.dependencies[i].name, name) == 0) {
            fprintf(stderr, "dependency '%s' already exists\n", name);
            return false;
        }
    }

    Arena *a = m->pkg.ctx->arena;
    // Resize dependencies array
    int new_count = m->pkg.dep_count + 1;
    Dependency *new_deps = (Dependency*)arena_alloc_zero(a, sizeof(Dependency) * new_count);
    memcpy(new_deps, m->pkg.dependencies, sizeof(Dependency) * m->pkg.dep_count);

    Dependency *dep = &new_deps[m->pkg.dep_count];
    dep->name = name;
    dep->source = "registry";
    dep->source_url = KET_REGISTRY_URL;

    // Parse version constraint
    const char *vc = version_constraint ? version_constraint : "*";
    if (vc[0] == '^') {
        dep->constraint.kind = VC_CARET;
        version_parse(vc + 1, &dep->constraint.version);
    } else if (vc[0] == '~') {
        dep->constraint.kind = VC_TILDE;
        version_parse(vc + 1, &dep->constraint.version);
    } else if (strcmp(vc, "*") == 0) {
        dep->constraint.kind = VC_ANY;
    } else {
        dep->constraint.kind = VC_EXACT;
        version_parse(vc, &dep->constraint.version);
    }

    m->pkg.dependencies = new_deps;
    m->pkg.dep_count = new_count;

    return true;
}

bool package_run(Package *pkg, CompileOptions *opts) {
    if (!pkg || !opts) return false;

    if (opts->verbose) {
        fprintf(stderr, "  running package: %s\n", pkg->name);
    }

    // Stub: In production, this would:
    // 1. Compile all source files
    // 2. Link with dependencies
    // 3. Execute the resulting binary
    return package_build(pkg, opts);
}
