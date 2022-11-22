#ifndef VMAP_H
#define VMAP_H

/**
 * general virtual memory map for Bincows:
 *
 *                         LOWER HALF
 * 0x0000000000000000  +----------------+
 *       512 GB        |      USER      |
 *                     |     PRIVATE    |
 * 0x0000008000000000  |----------------|
 *       512 GB        |                |
 * 0x0000010000000000  |----------------|
 *       512 GB        |      USER      |
 *                     |     SHARED     |
 * 0x0000018000000000  +----------------+
 *
 *
 *                         HIGHER HALF
 * 0xffff800000000000  +----------------+
 *       512 GB        |   TRANSLATED   |
 *                     |     MEMORY     |
 * 0xffff808000000000  |----------------|
 *   16 TB - 512 GB    |                |
 * 0xffff900000000000  |----------------|
 *       64 TB         |  BLOCK  CACHE  |
 * 0xffffd00000000000  |----------------|
 *       16 TB         |                |
 * 0xffffe00000000000  |----------------|
 *       16 TB         |   KERNEL TEMP  |
 * 0xfffff00000000000  |----------------|
 *    16 TB - 4 GB     |                |
 * 0xffffffff00000000  |----------------|
 *       512 MB        |      MMIO      |
 * 0xffffffff20000000  |----------------|
 *                     |                |
 * 0xffffffff80000000  |----------------|
 *        3 MB         |     KERNEL     |
 *                     |      DATA      |
 * 0xffffffff80300000  |----------------|
 *        2 GB         |  KERNEL HEAP   |
 * 0xffffffffffffffff  +----------------+
 *
 *
 * one pml4 entry is 512 GB: 0x0000008000000000.
 * The block cache is exaclty 128 pml4 entries long,
 * it is important to detect when a page has been
 * modified.
 *
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
 * 0xffffffff00000000  |----- PCIE -----|
 *        256 MB       |       ...      |
 * 0xffffffff10000000  |----------------|
 *        128 MB       |      empty     |
 * 0xffffffff18000000  |----------------|
 *         32 MB       |   framebuffer  |
 * 0xffffffff1a000000  |----------------|
 *     96 MB - 8KB     |      empty     |
 * 0xffffffff1fffe000  |----------------|
 *                     |      HPET      |
 * 0xffffffff1ffff000  |----------------|
 *                     |      LAPIC     |
 * 0xffffffff20000000  |----------------|
 *
 *
 *
 */


#define HPET_VADDR 0xffffffff1fffe000llu
#define APIC_VADDR 0xffffffff1ffff000llu
#define FB_VADDR   0xffffffff18000000llu


/**
 * early virtual memory map when calling
 * kernel_main:
 *
 * 0x000000000000000   |----------------|
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
 * 0xffffffffffffffff  |----------------|
 *
 *
 */


////////////////////////////////
/// user space memory ranges ///
////////////////////////////////
#define USER_PRIVATE_BEGIN 0x0000000000000000
#define USER_PRIVATE_END   0x0000008000000000

#define USER_SHARED_BEGIN  0x0000010000000000
#define USER_SHARED_END    0x0000018000000000


////////////////////////////////
// kernel space memory ranges //
////////////////////////////////
#define HIGHER_HALF_BEGIN 0xffff800000000000llu

#define TRANSLATED_PHYSICAL_MEMORY_BEGIN 0xffff800000000000llu
#define TRANSLATED_PHYSICAL_MEMORY_END   0xffff800000000000llu

#define BLOCK_CACHE_BEGIN 0xffff900000000000llu
#define BLOCK_CACHE_END   0xffffd00000000000llu

#define KERNEL_TEMP_BEGIN 0xffffe00000000000llu
#define KERNEL_TEMP_END   0xfffff00000000000llu

#define MMIO_BEGIN        0xffffffff00000000llu
#define MMEO_END          0xffffffff20000000llu

#define KERNEL_DATA_BEGIN 0xffffffff80000000llu
#define KERNEL_HEAP_BEGIN 0xffffffff80300000llu



//////////////////
// segmentation //
//////////////////

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10

#define USER_CS 0x23
#define USER_DS 0x1b
#define USER_RF 0x202


#include <stdint.h>
#include "../lib/assert.h"


/////////////////////////////////
// memory range test functions //
/////////////////////////////////

static inline int is_user_private(void* vaddr) {
    // GCC generates warning as >= USER_PRIVATE_BEGIN 
    // is always true since USER_PRIVATE_BEGIN = 0
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    return (uint64_t)vaddr >= USER_PRIVATE_BEGIN
        && (uint64_t)vaddr <  USER_PRIVATE_END;
#pragma GCC diagnostic pop
}

static inline int is_user_shared(void* vaddr) {
    return (uint64_t)vaddr >= USER_SHARED_BEGIN
        && (uint64_t)vaddr <  USER_SHARED_END;
}

// return non 0 value iif the given address
// resides in user memory
static inline int is_user(void* vaddr) {
    // user is in the lower half
    return is_user_private(vaddr) || is_user_shared(vaddr);
}

static inline int is_kernel_memory(uint64_t vaddr) {
    // kernel is in the 2 higher GB
    return (vaddr & KERNEL_DATA_BEGIN) == KERNEL_DATA_BEGIN;
}

static inline int is_block_cache(uint64_t vaddr) {
    return vaddr >= BLOCK_CACHE_BEGIN && vaddr < BLOCK_CACHE_END;
}

static inline int is_mmio(uint64_t vaddr) {
    return vaddr >= MMIO_BEGIN && vaddr < MMEO_END;
}

static inline int is_kernel_temp(uint64_t vaddr) {
    return vaddr >= KERNEL_TEMP_BEGIN && vaddr < KERNEL_TEMP_END;
}

static inline int is_higher_half(uint64_t vaddr) {
    return vaddr >= HIGHER_HALF_BEGIN;
}




/////////////////////////////////
///// translation functions /////
/////////////////////////////////


// return the physical address of a
// early high half pointer
static inline uint64_t kernel_data_to_physical(
    const void* vaddr) {
    assert((uint64_t) vaddr > KERNEL_DATA_BEGIN);
    return ~KERNEL_DATA_BEGIN & (uint64_t)vaddr;
}

// translated virtual address that is in
// the TRANSLATED region to a physical address:
// (phys + TRANSLATED_PHYSICAL_MEMORY_BEGIN)
static inline uint64_t trv2p(void* trvaddr) {
    return ((uint64_t)trvaddr & ~TRANSLATED_PHYSICAL_MEMORY_BEGIN);
}

static inline uint64_t trk2p(void* trkaddr) {
    return ((uint64_t)trkaddr & ~KERNEL_DATA_BEGIN);
}

// translate a physical memory address
// to access it where it is mapped
static inline void* __attribute__((pure)) translate_address(void* phys_addr) {
    return (void *)((uint64_t)phys_addr | TRANSLATED_PHYSICAL_MEMORY_BEGIN);
}


#endif // VMAP_H