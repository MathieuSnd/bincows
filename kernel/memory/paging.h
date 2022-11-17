/**
 * paging manager for the kernel
 * 
 * coarse grained mutual exclusion
 * 
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

struct boot_interface;

#define PAGE_MAP_LEVEL 4
#define PAGE_MASTER_SIZE (512*1024*1024*1024llu)

/**
 * enable PML4 4K paging
 * identity maps all addressable the memory except kernel 
 * executable
 * 
 * map the kernel executable to high half: 0xffffffff80000000 + phys
 */
void init_paging(const struct boot_interface* bi);
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


// free: if non0, free the physical pages using physfree
void unmap_pages(uint64_t virtual_addr, size_t count, int free);

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
 * @brief set the highest level 
 * map table physical base address
 */
void set_user_page_map(uint64_t paddr);


/**
 * @brief return physical address pointed by a
 * given virtual address
 * 
 * @param vaddr the virtual address
 * @return the physical address pointed by vaddr if
 * it is mapped, 0 otherwise
 */
uint64_t get_phys_addr(const void* vaddr);



// a master region is a range controlled
// by one highest level paging structure entry.
// 
// this function unmaps the master region
// with given base address.
// The base address must be aligned on
// PAGE_MASTER_SIZE bytes.
// This function does not free any memory and
// should be used with care.
// This function does flush the core's TLB
void unmap_master_region(void* base);


// map a master page direcory to a given
// PAGE_MASTER_SIZE aligned base address.
// This function does flush the core's TLB
void map_master_region(void* base, uint64_t pd, uint64_t flags);


// return the master page direcory currently mapped
// on a given PAGE_MASTER_SIZE aligned base address.
uint64_t get_master_pd(void* base);


// create an empty page directory and return its
// physical address. The result must be freed
// using physfree(). It can be used by
// map_master_region(). 
//
// The point of using this function is to 
// This
uint64_t create_empty_pd(void);




/**
 * @brief free everything that is mapped within the 
 * given virtual address range: unmap and call physfree
 * vaddr and count must be 512 GB aligned
 * 
 * @param begin_vaddr the beginning of the range. 512 GB aligned
 * @param count number bytes to unmap and free.  512 GB aligned
 * 
 */
void free_page_range(uint64_t begin_vaddr, size_t count);


