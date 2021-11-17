#pragma once

#include <stddef.h>
#include <stdint.h>

struct stivale2_struct_tag_memmap;

// init the physical allocator with the stivale2 memory map
void init_physical_allocator(const struct stivale2_struct_tag_memmap* memmap);


typedef void (*PHYSALLOC_CALLBACK)(uint64_t physical_address, uint64_t virtual_address, size_t size);


void physalloc(size_t size, void* virtual_addr, PHYSALLOC_CALLBACK callback);

void physfree(void* physical_page_addr);