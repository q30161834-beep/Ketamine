// ═══════════════════════════════════════════════════════════════════════════════
// PACKAGE MANAGER TESTS — Manifest parsing, version resolution
// ═══════════════════════════════════════════════════════════════════════════════

#include "../src/include/package.h"
#include "../src/include/arena.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define TEST(name) do { printf("  TEST: %s ... ", name); fflush(stdout);
#define END_TEST(pass) do { \
    if (pass) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL\n"); tests_failed++; } \
} while(0)

static int tests_passed = 0, tests_failed = 0;

static int ctx_count = 0;
static Context *make_ctx(void) {
    Arena *a = malloc(sizeof(Arena));
    *a = arena_new_size(1024 * 1024);
    Context *ctx = arena_alloc_zero(a, sizeof(Context));
    ctx->arena = a;
    ctx->intern_capacity = 256;
    ctx->intern_table = arena_alloc_zero(a, sizeof(InternStr*) * 256);
    ctx_count++;
    return ctx;
}

static void free_ctx(Context *ctx) {
    arena_free(ctx->arena);
    free(ctx->arena);
    free(ctx);
}

static void test_version_parse(void) {
    TEST("version parse — semver");
    PkgVersion v;
    bool ok = version_parse("1.2.3", &v);
    END_TEST(ok && v.major == 1 && v.minor == 2 && v.patch == 3);
}

static void test_version_parse_major_minor(void) {
    TEST("version parse — major.minor");
    PkgVersion v;
    bool ok = version_parse("2.0", &v);
    END_TEST(ok && v.major == 2 && v.minor == 0 && v.patch == 0);
}

static void test_version_compare_equal(void) {
    TEST("version compare — equal");
    PkgVersion a, b;
    version_parse("1.0.0", &a);
    version_parse("1.0.0", &b);
    END_TEST(version_compare(&a, &b) == 0);
}

static void test_version_compare_greater(void) {
    TEST("version compare — greater");
    PkgVersion a, b;
    version_parse("2.0.0", &a);
    version_parse("1.0.0", &b);
    END_TEST(version_compare(&a, &b) > 0);
}

static void test_version_satisfies_exact(void) {
    TEST("version satisfies — exact");
    PkgVersion v; version_parse("1.0.0", &v);
    VerConstraint c = { .kind = VC_EXACT }; version_parse("1.0.0", &c.version);
    END_TEST(version_satisfies(&v, &c));
}

static void test_version_satisfies_caret(void) {
    TEST("version satisfies — caret ^");
    PkgVersion v; version_parse("1.5.0", &v);
    VerConstraint c = { .kind = VC_CARET }; version_parse("1.2.0", &c.version);
    END_TEST(version_satisfies(&v, &c));
}

static void test_version_satisfies_tilde(void) {
    TEST("version satisfies — tilde ~");
    PkgVersion v; version_parse("1.2.5", &v);
    VerConstraint c = { .kind = VC_TILDE }; version_parse("1.2.0", &c.version);
    END_TEST(version_satisfies(&v, &c));
}

static void test_version_any(void) {
    TEST("version satisfies — any *");
    PkgVersion v; version_parse("99.99.99", &v);
    VerConstraint c = { .kind = VC_ANY };
    END_TEST(version_satisfies(&v, &c));
}

static void test_package_name_valid(void) {
    TEST("package name validation");
    END_TEST(package_name_valid("my-pkg") && package_name_valid("core_utils") && !package_name_valid("Invalid") && !package_name_valid("") && !package_name_valid("has spaces"));
}

static void test_registry_create(void) {
    TEST("registry creation");
    Context *ctx = make_ctx();
    Registry *reg = registry_new(ctx);
    END_TEST(reg != NULL);
    free_ctx(ctx);
}

int main(void) {
    printf("─── Package Manager Tests ───\n\n");
    test_version_parse();
    test_version_parse_major_minor();
    test_version_compare_equal();
    test_version_compare_greater();
    test_version_satisfies_exact();
    test_version_satisfies_caret();
    test_version_satisfies_tilde();
    test_version_any();
    test_package_name_valid();
    test_registry_create();
    printf("\n─── Results: %d passed, %d failed ───\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
