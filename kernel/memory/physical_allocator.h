#pragma once

#include <stddef.h>

struct stivale2_struct_tag_memmap;

// init the physical allocator with the stivale2 memory map
void init_physical_allocator(const struct stivale2_struct_tag_memmap* memmap);

void* physalloc(size_t size);
void physfree(void* virtual_addr, size_t size);