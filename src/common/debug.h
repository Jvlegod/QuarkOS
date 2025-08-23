#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "kprintf.h"

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE  4

/* default level: -DLOG_LEVEL=LOG_LEVEL_WARN */
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define __LOG_PRINT(level_str, fmt, ...) \
    do { \
        kprintf("[%s] %s:%d: " fmt "\r\n", level_str, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
# define LOG_DEBUG(fmt, ...) __LOG_PRINT("DEBUG", fmt, ##__VA_ARGS__)
#else
# define LOG_DEBUG(fmt, ...) do {} while(0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
# define LOG_INFO(fmt, ...)  __LOG_PRINT("INFO", fmt, ##__VA_ARGS__)
#else
# define LOG_INFO(fmt, ...)  do {} while(0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
# define LOG_WARN(fmt, ...)  __LOG_PRINT("WARN", fmt, ##__VA_ARGS__)
#else
# define LOG_WARN(fmt, ...)  do {} while(0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
# define LOG_ERROR(fmt, ...) __LOG_PRINT("ERROR", fmt, ##__VA_ARGS__)
#else
# define LOG_ERROR(fmt, ...) do {} while(0)
#endif

#endif /* __DEBUG_H__ */
