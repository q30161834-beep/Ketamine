#include "include/registry.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ─── Version parsing ──────────────────────────────────────────────────────
bool registry_parse_version(const char *s, char *out_version, VersionConstraint *out_constraint) {
    if (!s || !*s) return false;

    VersionConstraint constraint = CONSTRAINT_EXACT;

    switch (s[0]) {
        case '^': constraint = CONSTRAINT_CARET; s++; break;
        case '~': constraint = CONSTRAINT_TILDE; s++; break;
        case '>':
            if (s[1] == '=') { constraint = CONSTRAINT_GE; s += 2; }
            else { constraint = CONSTRAINT_GT; s++; }
            break;
        case '<':
            if (s[1] == '=') { constraint = CONSTRAINT_LE; s += 2; }
            else { constraint = CONSTRAINT_LT; s++; }
            break;
        case '=': constraint = CONSTRAINT_EXACT; s++; break;
        case '*': constraint = CONSTRAINT_WILDCARD; break;
    }

    if (out_version) {
        int i = 0;
        while (*s && i < MAX_PKG_VERSION - 1) {
            out_version[i++] = *s++;
        }
        out_version[i] = '\0';
    }
    if (out_constraint) *out_constraint = constraint;
    return true;
}

int registry_version_compare(const char *a, const char *b) {
    while (*a && *b) {
        int na = 0, nb = 0;
        while (*a && *a != '.') { na = na * 10 + (*a - '0'); a++; }
        while (*b && *b != '.') { nb = nb * 10 + (*b - '0'); b++; }
        if (na != nb) return na < nb ? -1 : 1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    if (*a == *b) return 0;
    return *a ? 1 : -1;
}

bool registry_version_satisfies(const char *version, const char *required, VersionConstraint constraint) {
    int cmp = registry_version_compare(version, required);

    switch (constraint) {
        case CONSTRAINT_EXACT:   return cmp == 0;
        case CONSTRAINT_CARET:   return cmp >= 0 && version[0] == required[0];
        case CONSTRAINT_TILDE:   return cmp >= 0 && version[0] == required[0] && version[2] == required[2];
        case CONSTRAINT_GE:      return cmp >= 0;
        case CONSTRAINT_GT:      return cmp > 0;
        case CONSTRAINT_LE:      return cmp <= 0;
        case CONSTRAINT_LT:      return cmp < 0;
        case CONSTRAINT_WILDCARD: return true;
    }
    return false;
}

// ─── Download / Publish / Resolve ─────────────────────────────────────────
int registry_download(const char *name, const char *version, const char *output_dir) {
    (void)name;
    (void)version;
    (void)output_dir;
    // Would download from registry API
    fprintf(stderr, "registry: would download %s@%s to %s\n", name, version, output_dir);
    return 0;
}

int registry_publish(const char *pkg_dir, const char *registry_url) {
    (void)pkg_dir;
    (void)registry_url;
    fprintf(stderr, "registry: would publish from %s to %s\n", pkg_dir, registry_url);
    return 0;
}

int registry_resolve_deps(PackageMeta *meta, const char *cache_dir) {
    (void)meta;
    (void)cache_dir;
    return 0;
}

bool registry_write_lock(const char *path, LockEntry *entries, int count) {
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "# Ketamine lock file\n");
    fprintf(f, "# This file ensures reproducible builds\n\n");

    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %s %s %s\n",
                entries[i].name,
                entries[i].version,
                entries[i].checksum,
                entries[i].resolved_url);
    }

    fclose(f);
    return true;
}

bool registry_read_lock(const char *path, LockFile *lock) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    lock->count = 0;

    while (fgets(line, sizeof(line), f)) {
        // Skip comments
        if (line[0] == '#' || line[0] == '\n') continue;

        char name[128], version[64], checksum[65], url[256];
        if (sscanf(line, "%127s %63s %64s %255s", name, version, checksum, url) >= 3) {
            LockEntry *e = &lock->entries[lock->count++];
            strncpy(e->name, name, MAX_PKG_NAME - 1);
            strncpy(e->version, version, MAX_PKG_VERSION - 1);
            strncpy(e->checksum, checksum, 64);
            strncpy(e->resolved_url, url, 511);
        }
    }

    fclose(f);
    return true;
}

int registry_install_from_lock(const char *lock_path, const char *output_dir) {
    LockFile lock;
    if (!registry_read_lock(lock_path, &lock)) {
        fprintf(stderr, "Cannot read lock file: %s\n", lock_path);
        return 1;
    }

    for (int i = 0; i < lock.count; i++) {
        int r = registry_download(lock.entries[i].name,
                                   lock.entries[i].version,
                                   output_dir);
        if (r) return r;
    }

    return 0;
}

int registry_fetch_index(const char *registry_url, PackageMeta **out_pkgs, int *out_count) {
    (void)registry_url;
    (void)out_pkgs;
    (void)out_count;
    return 0;
}
