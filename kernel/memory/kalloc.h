#pragma once

#include <stddef.h>

// provides 8byte-aligned 
// free memory
void* kmalloc(size_t size);
void  kfree(void* p);
void  kheap_init(void);

#ifndef NDEBUG
void  kmalloc_test(void);
#endif
