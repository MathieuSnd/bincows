#pragma once



#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))


#define LOG_LEVEL_DEBUG   1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_WARNING 3

#define LOG_DEBUG_COLOR   0x909090
#define LOG_INFO_COLOR    0xc0c0c0
#define LOG_WARNIN_COLOR  0xffff00


#ifndef NDEBUG
#define log_debug(...) logf(LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define log_debug(...)
#endif

#define log_info(...) logf(LOG_LEVEL_INFO, __VA_ARGS__) 
#define log_warn(...) logf(LOG_LEVEL_WARN, __VA_ARGS__)

void log(unsigned level, const char* string);

// behaves like printf
void logf(unsigned level, const char* fmt, ...);

void set_logging_level(unsigned level);

const char* log_get(void);

void log_flush(void);
