#pragma once

#include <stddef.h>

// provides 8byte-aligned 
// free memory
void* malloc(size_t size);
void  free(void* p);
void* realloc(void* ptr, size_t size);
void  heap_init(void);

size_t heap_get_n_allocation(void);
size_t heap_get_size(void);
size_t heap_get_brk(void);

#ifndef NDEBUG
void  malloc_test(void);
#endif
