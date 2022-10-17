#pragma once

#ifndef BLIB_STDLIB_H
    #error "Include <stdlib.h> instead of this file directly."
#endif

#include <stddef.h>

/**
 * this file includes alloc functions:
 * - malloc
 * - realloc
 * - free
 * 
 */

void* malloc(size_t size);
void  free(void* p);
void* realloc(void* ptr, size_t size);

#define alloca(size) __builtin_alloca(size)
