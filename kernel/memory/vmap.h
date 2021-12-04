#pragma once

#include <stdint.h>

/**
 * general virtual memory map for Bincows:
 *  
 * 0                   ------------------
 *                     |      USER      |
 *                     |     MEMORY     | 
 * 0x0000007fffffffff  |----------------| 
 *                     |                | 
 * 0xffff800000000000  |----------------|
 *                     |   TRANSLATED   |
 *                     |     MEMORY     |
 * 0xffff807fffffffff  |----------------|
 *                     |                | 
 * 0xffffffff00000000  |----------------|
 *                     |      MMIO      | 
 * 0xffffffff80000000  |----------------|
 *                     |     KERNEL     |
 *                     |      DATA      |
 * 0xffffffff80300000  |----------------|
 *                     |  KERNEL HEAP   |
 * 0xffffffffffffffff  ------------------
 * 
 * 
 * the allocator and the page table manager
 * share the same virtual space, in a translated
 * physical memory space
 * 
 *  
 * 
 * detailed MMIO address space:
 * 
 * 0xffffffff00000000  | ---- PCIE ---- |
 *                     |   framebuffer  |
 *                     |       ...      |
 *                     | -------------- |
 * 0xffffffff1fffe000  |      HPET      |
 * 0xffffffff1ffff000  |      LAPIC     |
 * 0xffffffff20000000  |----------------|
 * 
 * 
 */

#define HPET_VIRTUAL_ADDRESS 0xffffffff1fffe000llu
#define APIC_VIRTUAL_ADDRESS 0xffffffff1ffff000llu



/**
 * early virtual memory map for Bincows:
 * 
 * 
 * 0                   ------------------
 *                     |                | 
 *                     |                | 
 * 0xffff800000000000  |----------------|
 *                     |   TRANSLATED   | 
 *                     |     MEMORY     | 
 * 0xffff8c0000000000  |----------------|
 *                     |                | 
 * 0xffffffff80000000  |----------------|
 *                     |     KERNEL     |
 *                     |      DATA      |
 * 0xffffffffffffffff  ------------------
 * 
 * 
 */


#define TRANSLATED_PHYSICAL_MEMORY_BEGIN 0xffff800000000000llu
#define MMIO_BEGIN          0xffffffff00000000llu
#define KERNEL_DATA_BEGIN   0xffffffff80000000llu
#define KERNEL_HEAP_BEGIN   0xffffffff80300000llu

// return non 0 value iif the given address
// resides in kernel memory
static inline int is_user(uint64_t virtual_address) {
    // user is in the lower half
    return ! (virtual_address & (1llu << 63));
}
static inline int is_allocator_memory(uint64_t virtual_address) {
    // user is in the lower half
    return (virtual_address & 0xff80000000000000) == 0xff00000000000000;
}
static inline int is_kernel_memory(uint64_t virtual_address) {
    // kernel is in the 2 higher GB
    return (virtual_address & KERNEL_DATA_BEGIN) == KERNEL_DATA_BEGIN;
}
static inline int is_mmio(uint64_t virtual_address) {
    // between -4 GB and -2 GB
    return (virtual_address & KERNEL_DATA_BEGIN) == MMIO_BEGIN;
}

// return the physical address of a
// stivale2 high half pointer
static inline uint64_t early_virtual_to_physical(
        const void* virtual_address) {
    
    if((0xfffff00000000000llu & (uint64_t)virtual_address) ==
                         TRANSLATED_PHYSICAL_MEMORY_BEGIN)
        return ~TRANSLATED_PHYSICAL_MEMORY_BEGIN & (uint64_t)virtual_address;
    
    else
        return ~KERNEL_DATA_BEGIN & (uint64_t)virtual_address;
    
}


// translate a physical memory address
// to access it where it is mapped
static inline void* translate_address(void* phys_addr) {
    return (void*)((uint64_t)phys_addr | TRANSLATED_PHYSICAL_MEMORY_BEGIN);
}
