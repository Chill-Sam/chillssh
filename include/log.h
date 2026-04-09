#pragma once

#include <stdio.h> // IWYU pragma: keep

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

/* Internal macro */
#define LOG_BASE(level, label, fmt, ...)                                       \
    do {                                                                       \
        if (CURRENT_LOG_LEVEL >= level) {                                      \
            fprintf(stderr, "[%s] %s:%d: " fmt "\n", label, __FILE__,          \
                    __LINE__ __VA_OPT__(, ) __VA_ARGS__);                      \
        }                                                                      \
    } while (0)

/* Public macros */
#define LOG_ERROR(fmt, ...)                                                    \
    LOG_BASE(LOG_LEVEL_ERROR, "ERROR", fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)                                                  \
    LOG_BASE(LOG_LEVEL_WARNING, "WARN", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_BASE(LOG_LEVEL_INFO, "INFO", fmt, ##__VA_ARGS__)
