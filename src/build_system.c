#include "include/build_system.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <windows.h>
  static uint64_t file_mtime(const char *path) {
      WIN32_FILE_ATTRIBUTE_DATA info;
      if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info))
          return 0;
      ULARGE_INTEGER li;
      li.LowPart = info.ftLastWriteTime.dwLowDateTime;
      li.HighPart = info.ftLastWriteTime.dwHighDateTime;
      return li.QuadPart;
  }
#else
  #include <sys/stat.h>
  static uint64_t file_mtime(const char *path) {
      struct stat st;
      if (stat(path, &st) != 0) return 0;
      return (uint64_t)st.st_mtime;
  }
#endif

BuildProject *build_project_new(BuildConfig *config) {
    BuildProject *proj = (BuildProject*)calloc(1, sizeof(BuildProject));
    proj->config = *config;
    proj->capacity = 128;
    proj->units = (BuildUnit*)calloc((size_t)proj->capacity, sizeof(BuildUnit));
    return proj;
}

int build_scan_sources(BuildProject *proj) {
    // In a real implementation, this would recursively scan directories
    // for .kt files. For now, just return the sources passed in.
    proj->count = 0;
    for (int i = 0; i < proj->config.source_dir_count; i++) {
        printf("build: scanning source dir: %s\n", proj->config.source_dirs[i]);
    }
    return 0;
}

void build_check_incremental(BuildProject *proj) {
    for (int i = 0; i < proj->count; i++) {
        BuildUnit *u = &proj->units[i];
        u->source_mtime = file_mtime(u->source_path);
        uint64_t obj_mtime = file_mtime(u->object_path);
        u->needs_rebuild = (obj_mtime == 0) || (u->source_mtime > obj_mtime);

        if (u->needs_rebuild) {
            printf("build: need to rebuild %s\n", u->source_path);
        }
    }
}

int build_all(BuildProject *proj) {
    build_scan_sources(proj);
    build_check_incremental(proj);

    for (int i = 0; i < proj->count; i++) {
        if (proj->units[i].needs_rebuild) {
            int r = build_compile_unit(proj, i);
            if (r) return r;
        }
    }

    return build_link(proj);
}

int build_clean(BuildProject *proj) {
    printf("build: cleaning...\n");
    for (int i = 0; i < proj->count; i++) {
        BuildUnit *u = &proj->units[i];
        if (u->object_path) {
            remove(u->object_path);
            printf("  rm %s\n", u->object_path);
        }
    }
    return 0;
}

int build_cross_compile(BuildProject *proj, const char *target) {
    printf("build: cross-compiling for %s\n", target);
    proj->config.target_triple = target;
    return build_all(proj);
}

int build_compile_unit(BuildProject *proj, int unit_idx) {
    (void)proj;
    (void)unit_idx;
    // Would invoke the compiler on a single source file
    return 0;
}

int build_link(BuildProject *proj) {
    (void)proj;
    // Would link all .o files into final executable
    return 0;
}

void build_write_depfile(BuildProject *proj, int unit_idx) {
    (void)proj;
    (void)unit_idx;
}
