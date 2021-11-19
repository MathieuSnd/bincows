#include <stivale2.h>
#include "../debug/assert.h"
#include "physical_allocator.h"

#include "paging.h"

extern uint64_t get_cr0(void);
extern void set_cr0(uint64_t cr0);

extern void _cr3(uint64_t cr3);

extern uint64_t get_cr4(void);
extern void set_cr4(uint64_t cr4);

#define CR0_PG_BIT (1lu << 31)
#define CR4_PAE_BIT (1lu << 5)


/**
 * 4th level table (pde) entry
 */
typedef const void* pte;

/**
 * 3rd level table (pde) entry
 */
typedef const pte* pde;

/**
 * 2nd level table (pdpt) entry
 * 
 */
typedef const pde* pdpte;

/**
 * 1st level table (pml4) entry
 */
typedef const pdpte* pml4e;



pml4e pml4_table[512] __attribute__((aligned(4096))) = {0};

/*
struct stivale2_struct_tag_memmap {
    struct stivale2_tag tag;      // Identifier: 0x2187f79e8612de07
    uint64_t entries;             // Count of memory map entries
    struct stivale2_mmap_entry memmap[];  // Array of memory map entries
};
struct stivale2_mmap_entry {
    uint64_t base;      // Physical address of base of the memory section
    uint64_t length;    // Length of the section
    uint32_t type;      // Type (described below)
    uint32_t unused;
};
enum stivale2_mmap_type : uint32_t {
    USABLE                 = 1,
    RESERVED               = 2,
    ACPI_RECLAIMABLE       = 3,
    ACPI_NVS               = 4,
    BAD_MEMORY             = 5,
    BOOTLOADER_RECLAIMABLE = 0x1000,
    KERNEL_AND_MODULES     = 0x1001,
    FRAMEBUFFER            = 0x1002
};

*/




void physical_allocator_callback(uint64_t physical_address, 
                                 uint64_t virtual_address,
                                 size_t   size) {
}

void init_paging(void) {    


// get the physical address of the pml4 table
    uint64_t lower_half_ptr = ~0xffffffff80000000llu | (uint64_t)&pml4_table;

while(1);


// Page level Write Through (PWT) 0
// Page level Cache Disable (PCD) 0
// [63:MAXPHYADDR] must be 0!!! as 'lower_half_ptr' is supposed to
// be a physical address already, it should be the case 
    _cr3(lower_half_ptr);
// enable  PAE in cr4 
    set_cr4(get_cr4() | CR0_PG_BIT);

// enable the PG bit
    set_cr0(get_cr0() | CR0_PG_BIT);
}

void map_pages(uint64_t physical_addr, uint64_t virtual_addr, size_t count) {
    (void) (physical_addr + virtual_addr + count);
}
