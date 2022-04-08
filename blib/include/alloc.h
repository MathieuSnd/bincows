#pragma once

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
