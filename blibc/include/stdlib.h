#pragma once

#define BLIB_STDLIB_H

extern int atexit(void (*func)(void));

extern void __attribute__((noreturn)) exit(int status);


extern int putenv(char *string);
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);
extern char* getenv(const char *name);

extern int system(const char *command);

// malloc, free, realloc, alloca
#include <alloc.h>

#undef BLIB_STDLIB_H