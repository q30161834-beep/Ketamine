#include "include/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE LOGGING SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_NONE
} LogLevel;

static LogLevel current_level = LOG_INFO;
static FILE *log_file = NULL;

void log_set_level(LogLevel level) {
    current_level = level;
}

void log_set_file(const char *path) {
    if (log_file) fclose(log_file);
    log_file = fopen(path, "a");
}

static const char *log_level_name(LogLevel level) {
    switch (level) {
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARN";
        case LOG_ERROR:   return "ERROR";
        default:          return "NONE";
    }
}

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (level < current_level) return;

    // Timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm);

    // Format message
    char msg[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    // Build line
    char line_buf[4224];
    snprintf(line_buf, sizeof(line_buf),
             "[%s] %-5s %s:%d — %s\n",
             timestamp, log_level_name(level), file, line, msg);

    // Write to stderr
    fprintf(stderr, "%s", line_buf);

    // Write to log file
    if (log_file) {
        fputs(line_buf, log_file);
        fflush(log_file);
    }
}

#define log_debug(...)    log_write(LOG_DEBUG,    __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)     log_write(LOG_INFO,     __FILE__, __LINE__, __VA_ARGS__)
#define log_warning(...)  log_write(LOG_WARNING,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...)    log_write(LOG_ERROR,    __FILE__, __LINE__, __VA_ARGS__)
