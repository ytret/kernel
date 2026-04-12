#pragma once

#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_FLOW  4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_FLOW
#endif

#define _LOG_HELPER(LEVEL, ...)                                                \
    log_printf(__FILE_NAME__, __func__, __LINE__, (LEVEL), __VA_ARGS__)

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(...) _LOG_HELPER(LOG_LEVEL_ERROR, __VA_ARGS__)
#else
#define LOG_ERROR(...) (void)0
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(...) _LOG_HELPER(LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define LOG_INFO(...) (void)0
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(...) _LOG_HELPER(LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define LOG_DEBUG(...) (void)0
#endif

#if LOG_LEVEL >= LOG_LEVEL_FLOW
#define LOG_FLOW(...) _LOG_HELPER(LOG_LEVEL_FLOW, __VA_ARGS__)
#else
#define LOG_FLOW(...) (void)0
#endif

void log_printf(const char *file, const char *func, int line, int level,
                const char *fmt, ...);
