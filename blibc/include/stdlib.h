#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#define BLIB_STDLIB_H

int atexit(void (*func)(void));

void __attribute__((noreturn)) exit(int status);


int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
extern char* getenv(const char *name);

int system(const char *command);

// malloc, free, realloc, alloca
#include <alloc.h>


#ifdef __cplusplus
}
#endif

#undef BLIB_STDLIB_H
#endif//_STDLIB_H