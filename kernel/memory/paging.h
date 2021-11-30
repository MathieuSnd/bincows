#pragma once

#include <stddef.h>
#include <stdint.h>

struct stivale2_struct_tag_memmap;

/**
 * enable PML4 4K paging
 * identity maps all addressable the memory except kernel 
 * executable
 * 
 * map the kernel executable to high half: 0xffffffff80000000 + phys
 */
void init_paging(const struct stivale2_struct_tag_memmap* memmap);
void append_paging_initialization(void);


// page table flags

// the entry is present
#define PRESENT_ENTRY 1
// read only
#define PL_RW 2
// supervisor only
#define PL_US 4
// page level write through
#define PWT 8
// page level cache disable
#define PCD 16
#define PL_XD (1llu << 63)

/**
 * map pages from a given physical address to a given virtual address
 * map 'count' continuous pages 
 * 
 * flags can be one of the following:
 *      - PRESENT_ENTRY
 *      - PL_RW: read only
 *      - US:    supervisor (should not be used here)
 *      - PWT:   page level write through
 *      - PCD:   page level cache disable
 */
void map_pages(uint64_t physical_addr,
               uint64_t virtual_addr, 
               size_t   count,
               unsigned flags);

void alloc_pages(uint64_t virtual_addr, 
               size_t   count,
               unsigned flags);
