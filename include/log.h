// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#pragma once

#include <stdio.h> // IWYU pragma: keep
#include <time.h>

/* Log levels */
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO
} log_level_t;

/* Global log level */
extern log_level_t CURRENT_LOG_LEVEL;

static inline void log_set_level(log_level_t level) {
    CURRENT_LOG_LEVEL = level;
}

/* Terminal colors */
#define LOG_COLOR_RED    "\033[0;31m"
#define LOG_COLOR_YELLOW "\033[0;33m"
#define LOG_COLOR_CYAN   "\033[0;36m"
#define LOG_COLOR_GRAY   "\033[0;37m"
#define LOG_COLOR_RESET  "\033[0m"

static inline const char *log_timestamp(void) {
    static char buf[20];
    time_t now = time(NULL);
    struct tm tm_buf;
    if (localtime_r(&now, &tm_buf) == NULL)
        return "XXXX-XX-XX ??:??:??";
    if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf) == 0)
        return "XXXX-XX-XX ??:??:??";
    return buf;
}

#define LOG_BASE_LOCATED(level, color, label, fmt, ...)                        \
    do {                                                                       \
        if (CURRENT_LOG_LEVEL >= (level)) {                                    \
            fprintf(stderr, "%s[%s] [%s] %s:%d: " fmt LOG_COLOR_RESET "\n",    \
                    color, log_timestamp(), label, __FILE__,                   \
                    __LINE__ __VA_OPT__(, ) __VA_ARGS__);                      \
        }                                                                      \
    } while (0)

#define LOG_BASE(level, color, label, fmt, ...)                                \
    do {                                                                       \
        if (CURRENT_LOG_LEVEL >= (level)) {                                    \
            fprintf(stderr, "%s[%s] [%s] " fmt LOG_COLOR_RESET "\n", color,    \
                    log_timestamp(), label __VA_OPT__(, ) __VA_ARGS__);        \
        }                                                                      \
    } while (0)

/* Public macros */
#define LOG_ERROR(fmt, ...)                                                    \
    LOG_BASE_LOCATED(LOG_LEVEL_ERROR, LOG_COLOR_RED, "ERROR",                  \
                     fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARNING(fmt, ...)                                                  \
    LOG_BASE_LOCATED(LOG_LEVEL_WARNING, LOG_COLOR_YELLOW, "WARN",              \
                     fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
    LOG_BASE(LOG_LEVEL_INFO, LOG_COLOR_CYAN, "INFO",                           \
             fmt __VA_OPT__(, ) __VA_ARGS__)
