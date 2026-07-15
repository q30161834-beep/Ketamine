#ifndef KETAMINE_TEST_FRAMEWORK_H
#define KETAMINE_TEST_FRAMEWORK_H

#include "types.h"

// ─── Test Framework ────────────────────────────────────────────────────────
// Integrated test runner: `ket test`
// Supports: #[test], assert_eq!, assert_ne!, assert!, parallel test execution.

typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_IGNORE,
    TEST_ERROR,
} TestResult;

typedef struct {
    const char   *name;
    const char   *file;
    int           line;
    TestResult    result;
    char          message[512];
    double        duration_ms;
    bool          ignored;
} TestCase;

typedef struct {
    TestCase     *tests;
    int           count;
    int           capacity;
    int           passed;
    int           failed;
    int           ignored;
    double        total_duration;
    const char   *output_path;    // for JUnit XML
} TestSuite;

// ─── API ──────────────────────────────────────────────────────────────────

/// Initialize a test suite
TestSuite *test_suite_new(const char *output_path);

/// Add a test case
void test_suite_add(TestSuite *suite, const char *name,
                    const char *file, int line);

/// Record a test result
void test_record(TestSuite *suite, const char *name, TestResult result,
                 const char *msg);

/// Run all discovered tests (sequential)
int test_run_all(TestSuite *suite, ASTNode *module, Context *ctx);

/// Run tests in parallel using thread pool
int test_run_parallel(TestSuite *suite, ASTNode *module,
                      Context *ctx, int threads);

/// Generate JUnit XML report
int test_write_junit_xml(TestSuite *suite, const char *path);

/// Print test summary to stdout
void test_print_summary(TestSuite *suite);

/// Assert two values are equal (returns false on mismatch)
bool test_assert_eq(int64_t a, int64_t b, const char *expr_a,
                    const char *expr_b, const char *file, int line);

/// Assert a condition is true
bool test_assert(bool cond, const char *expr, const char *file, int line);

/// Run doctests (tests embedded in doc comments)
int test_run_doctests(ASTNode *module, Context *ctx);

#endif
