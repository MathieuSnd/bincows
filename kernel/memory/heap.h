#pragma once

#include <stddef.h>

// provides 8byte-aligned 
// free memory
void* malloc(size_t size);
void  free(void* p);
void  heap_init(void);

#ifndef NDEBUG
void  malloc_test(void);
#endif
