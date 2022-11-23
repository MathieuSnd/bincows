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
#define PAGE_R1_SIZE     (    1024*1024*1024llu)
#define PAGE_R2_SIZE     (       2*1024*1024llu)

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


/**
 * Deep free a master region memory along 
 * with paging structures.
 */
void free_master_region(uint64_t pd);


/**
 * Deep free a level 1 range memory 
 * along with paging structures given 
 * the level 1 page directory.
 * 
 * if pfree == 0, the underlying physical 
 * memory is not freed. Paging structures
 * are freed anyway. 
 */
void free_r1_region(uint64_t pd, int pfree);


// return the master page direcory currently mapped
// on a given PAGE_MASTER_SIZE aligned base address.
// corresponds to get_pd(base, 0)
uint64_t get_master_pd(void* base);


// return the physical address of the page direcory 
// currently mapped at a given well aligned base address.
// level 0 corresponds to the master page directory.
// level 1 corresponds to an entry in the master
// page directory, and so on.
// 
// base alignment constraint:
//    level = 0: PAGE_MASTER_SIZE
//    level = 1: PAGE_R1_SIZE
//    level = 2: PAGE_R2_SIZE
//    level > 3: undefined
// 
// 0 is returned if no pd with the given level is 
// mapped at the given base address, or in case the level 
// is beyond the paging system level: 
//        >= 2 if PML4 is used,
//        >= 3 if PML5 is used.
uint64_t get_pd(void* base, int level);



// map the page direcory with the given physical
// address and given directory level to a given
// well aligned virt base address.
// level 0 corresponds to the master page directory.
// level 1 corresponds to an entry in the master
// page directory, and so on.
// 
// base alignment constraint:
//    level = 0: PAGE_MASTER_SIZE
//    level = 1: PAGE_R1_SIZE
//    level = 2: PAGE_R2_SIZE
//    level > 3: undefined
//
// No memory should be mapped at the virtual address
// when callong map_pd(...).
// 
// On success, 0 is returned. 
// Failure can happend if some memory is already mapped
// at the given virtual address or if no pd with the 
// given level is mapped at the given base address,
// or in case the level is beyond the paging system level: 
//        >= 2 if PML4 is used,
//        >= 3 if PML5 is used.
//
// if level > 0, the page table with levels < level
// should be mapped already.
int map_pd(uint64_t pd, void* base, int level, uint64_t flags);


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


