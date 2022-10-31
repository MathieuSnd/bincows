
/**
 * This pmm unsures mutual exclusion 
 * for allocations.
 * 
 * allocating and freeing can be done in an
 * IRQ context. 
 * 
 * 1 cpu: mutual exclusion = cli/sti
 * @todo add spinlocks for SMP
 */
#pragma once

#include <stddef.h>
#include <stdint.h>



struct boot_interface;

// init the physical allocator
// first phase of initialization:
// suppose that all the addressable memory is identity mapped
void init_physical_allocator(const struct boot_interface *);

//linked list element representing a 64 MB memory region
struct physical_allocator_data_page_entry {
    void* physical_address;

    uint64_t reserved[2];
};


// the physalloc / physfree functions require these  
// pages of every range to be mapped accordingly to the 
// vmap.h specifications:
// 
// physical_address should be mapped to physical_address | 0x
//
// size: the number of entries in the returned array 
const struct physical_allocator_data_page_entry* 
                     physical_allocator_data_pages(size_t* size);


typedef void (*PHYSALLOC_CALLBACK)(
    uint64_t physical_address, 
    uint64_t virtual_address, 
    size_t size);


void physalloc(size_t size, void* virtual_addr, PHYSALLOC_CALLBACK callback);

// alloc a single physical page
// without mapping it or anything
uint64_t physalloc_single(void);

void physfree(uint64_t physical_page_addr);

// return the number of available pages in the system
size_t available_pages(void);

// return the pages in the system
size_t total_pages(void);
