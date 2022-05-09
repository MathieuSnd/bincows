/**
 * paging manager for the kernel
 * 
 * coarse grained mutual exclusion
 * 
 */

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

// asserts that the vaddr is in
// a mapped page
uint64_t get_paddr(const void* vaddr);

// page table flags

// the entry is present
#define PRESENT_ENTRY 1llu
// read only
#define PL_RW 2llu
// supervisor only
#define PL_US 4llu
// page level write through
#define PWT 8llu
// page level cache disable
#define PCD 16llu
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
               uint64_t flags);

void unmap_pages(uint64_t virtual_addr, size_t count);

void alloc_pages(void* virtual_addr, 
               size_t   count,
               uint64_t flags);

/**
 * @brief change page level attributes: PL_RW PL_US PWT PCD
 * of already mapped pages
 * 
 */
void remap_pages(void* virtual_addr, 
                 size_t count,
                 uint64_t new_flags);



/**
 * @brief allocate a user page map
 * and return the paddr of
 * the top level table physical
 * base address.
 * 
 * This page table should eventually
 * be freed with alloc_user_page_map
 * 
 */
uint64_t alloc_user_page_map(void);


/**
 * @brief free a user page map
 * returned by alloc_user_page_map
 * 
 */
void free_user_page_map(uint64_t);



/**
 * @brief return the top level 
 * map table physical base address
 */
uint64_t get_user_page_map(void);


/**
 * @brief set the highest level 
 * map table physical base address
 */
void set_user_page_map(uint64_t paddr);


void unmap_user(void);
