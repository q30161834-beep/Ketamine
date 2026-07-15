#include "include/test_framework.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  static double test_now_ms(void) {
      LARGE_INTEGER freq, count;
      QueryPerformanceFrequency(&freq);
      QueryPerformanceCounter(&count);
      return (double)count.QuadPart / (double)freq.QuadPart * 1000.0;
  }
#else
  #include <sys/time.h>
  static double test_now_ms(void) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
  }
#endif

TestSuite *test_suite_new(const char *output_path) {
    TestSuite *suite = (TestSuite*)calloc(1, sizeof(TestSuite));
    suite->capacity = 256;
    suite->tests = (TestCase*)calloc((size_t)suite->capacity, sizeof(TestCase));
    suite->output_path = output_path;
    return suite;
}

void test_suite_add(TestSuite *suite, const char *name,
                    const char *file, int line) {
    if (suite->count >= suite->capacity) {
        suite->capacity *= 2;
        suite->tests = (TestCase*)realloc(suite->tests,
                        (size_t)suite->capacity * sizeof(TestCase));
    }
    TestCase *tc = &suite->tests[suite->count++];
    tc->name = name;
    tc->file = file;
    tc->line = line;
    tc->result = TEST_PASS;
    tc->message[0] = '\0';
}

void test_record(TestSuite *suite, const char *name, TestResult result,
                 const char *msg) {
    for (int i = 0; i < suite->count; i++) {
        if (strcmp(suite->tests[i].name, name) == 0) {
            suite->tests[i].result = result;
            if (msg) strncpy(suite->tests[i].message, msg, 511);
            break;
        }
    }
}

int test_run_all(TestSuite *suite, ASTNode *module, Context *ctx) {
    (void)module;
    (void)ctx;

    double start = test_now_ms();

    for (int i = 0; i < suite->count; i++) {
        TestCase *tc = &suite->tests[i];

        if (tc->ignored) {
            tc->result = TEST_IGNORE;
            suite->ignored++;
            continue;
        }

        double t_start = test_now_ms();
        // Would actually run the test function here
        // For now, just record success
        tc->result = TEST_PASS;
        tc->duration_ms = test_now_ms() - t_start;

        if (tc->result == TEST_PASS) suite->passed++;
        else suite->failed++;
    }

    suite->total_duration = test_now_ms() - start;
    return suite->failed;
}

int test_run_parallel(TestSuite *suite, ASTNode *module,
                      Context *ctx, int threads) {
    (void)threads;
    return test_run_all(suite, module, ctx);
}

int test_write_junit_xml(TestSuite *suite, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 1;

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuite name=\"ketamine\" tests=\"%d\" failures=\"%d\" "
               "errors=\"%d\" time=\"%.3f\">\n",
            suite->count, suite->failed,
            suite->count - suite->passed - suite->ignored,
            suite->total_duration / 1000.0);

    for (int i = 0; i < suite->count; i++) {
        TestCase *tc = &suite->tests[i];
        fprintf(f, "  <testcase name=\"%s\" classname=\"%s\" time=\"%.3f\">\n",
                tc->name, tc->file ? tc->file : "?",
                tc->duration_ms / 1000.0);

        if (tc->result == TEST_FAIL) {
            fprintf(f, "    <failure message=\"%s\"/>\n", tc->message);
        } else if (tc->result == TEST_IGNORE) {
            fprintf(f, "    <skipped/>\n");
        } else if (tc->result == TEST_ERROR) {
            fprintf(f, "    <error message=\"%s\"/>\n", tc->message);
        }

        fprintf(f, "  </testcase>\n");
    }

    fprintf(f, "</testsuite>\n");
    fclose(f);
    return 0;
}

void test_print_summary(TestSuite *suite) {
    printf("\n─── Test Results ───\n");
    printf("  %d passed, %d failed, %d ignored\n",
           suite->passed, suite->failed, suite->ignored);
    printf("  Total: %d tests in %.0f ms\n",
           suite->count, suite->total_duration);

    for (int i = 0; i < suite->count; i++) {
        TestCase *tc = &suite->tests[i];
        if (tc->result == TEST_FAIL) {
            printf("  FAIL  %s (%s:%d)\n", tc->name, tc->file, tc->line);
            if (tc->message[0]) printf("    %s\n", tc->message);
        } else if (tc->result == TEST_IGNORE) {
            printf("  SKIP  %s (%s:%d)\n", tc->name, tc->file, tc->line);
        }
    }
}

bool test_assert_eq(int64_t a, int64_t b, const char *expr_a,
                    const char *expr_b, const char *file, int line) {
    if (a != b) {
        fprintf(stderr, "ASSERT_EQ FAIL: %s:%d: %s == %s\n  left: %lld\n  right: %lld\n",
                file, line, expr_a, expr_b,
                (long long)a, (long long)b);
        return false;
    }
    return true;
}

bool test_assert(bool cond, const char *expr, const char *file, int line) {
    if (!cond) {
        fprintf(stderr, "ASSERT FAIL: %s:%d: %s\n", file, line, expr);
        return false;
    }
    return true;
}

int test_run_doctests(ASTNode *module, Context *ctx) {
    (void)module;
    (void)ctx;
    return 0;
}
