#pragma once



#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))


#define LOG_LEVEL_DEBUG   1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_WARNING 3

#define LOG_DEBUG_COLOR   0x909090
#define LOG_INFO_COLOR    0xc0c0c0
#define LOG_WARNIN_COLOR  0xffff00


#ifndef NDEBUG
#define klog_debug(fmt, ...) klogf(LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)
#else
#define klog_debug(fmt, ...)
#endif

#define klog_info(...) klogf(LOG_LEVEL_INFO, __VA_ARGS__) 
#define klog_warn(...) klogf(LOG_LEVEL_WARN, __VA_ARGS__)

void klog(unsigned level, const char* string);

// behaves like kprintf
void klogf(unsigned level, const char* fmt, ...);

void set_logging_level(unsigned level);

const char* klog_get(void);

void klog_flush(void);
