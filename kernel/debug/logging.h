#pragma once



#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))


#define LOG_LEVEL_DEBUG   1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_WARNING 3

#define LOG_DEBUG_COLOR   0x909090
#define LOG_INFO_COLOR    0xc0c0c0
#define LOG_WARNIN_COLOR  0xffff00


#ifndef NDEBUG
#define log_debug(fmt, ...) klogf(LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

#define log_info(fmt, ...) \
    if(NUMARGS(__VA_ARGS__) == 0) klog(LOG_LEVEL_DEBUG, fmt) \
    else klogf(LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)

#define log_warn(fmt, ...) \
    if(NUMARGS(__VA_ARGS__) == 0) klog(LOG_LEVEL_DEBUG, fmt) \
    else klogf(LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)


void klog(int level, const char* string);

// behaves like kprintf
void klogf(int level, const char* fmt, ...);