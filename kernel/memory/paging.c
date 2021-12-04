#include <stivale2.h>
#include "../debug/assert.h"
#include "../klib/string.h"
#include "physical_allocator.h"

#include "paging.h"
#include "vmap.h"
#include "../klib/sprintf.h"
#include "../debug/panic.h"
#include "../debug/dump.h"

extern uint64_t get_cr0(void);
extern void set_cr0(uint64_t cr0);

extern void _cr3(uint64_t cr3);

extern uint64_t get_cr4(void);
extern void set_cr4(uint64_t cr4);

#define CR0_PG_BIT  (1lu << 31)
#define CR4_PAE_BIT (1lu << 5)
#define CR4_PCIDE   (1lu << 17)


// size of bulks of allocation
// the page buffer is 64 long
// so we suppose that 4096 pages
// won't make more than 64 page
// tables
#define MAX_ALLOC 1024


/**
 * 4th level table (pde) entry
 */
typedef void* pte;

/**
 * 3rd level table (pde) entry
 */
typedef pte* pde;

/**
 * 2nd level table (pdpt) entry
 * 
 */
typedef pde* pdpte;

/**
 * 1st level table (pml4) entry
 */
typedef pdpte* pml4e;

#define __page __attribute__((aligned(4096)))


static pml4e pml4[512]   __page = {0};

// 1st entry 0 -> 0x0000007fffffffff
static pdpte pdpt_low[512] __page = {0}; 
// 256th          0xffff800000000000 -> 0xffff807fffffffff
static pdpte pdpt_mid[512] __page = {0};
// 511st entry 0xffffff8000000000 -> 0xffffffffffffffff
static pdpte pdpt_high[512] __page = {0};

static_assert_equals(sizeof(pml4),     0x1000);
static_assert_equals(sizeof(pdpt_high), 0x1000);
static_assert_equals(sizeof(pdpt_mid),  0x1000);
static_assert_equals(sizeof(pdpt_low),  0x1000);



// the alloc_page_table function  has the right
// to realloc on the fly
// unset it to avoid nasty recursions
static int alloc_page_table_realloc = 1;
static void fill_page_table_allocator_buffer(size_t n);


// extract the offset of the page table 
// from a virtual address
static uint32_t pt_offset(uint64_t virt) {
    return (virt >> 12) & 0x1ff;
}
// extract the offset of the page directory 
// from a virtual address
static uint32_t pd_offset(uint64_t virt) {
    return (virt >> 21) & 0x1ff;
}
// extract the offset of the pdp 
// from a virtual address
static uint32_t pdpt_offset(uint64_t virt) {
    return (virt >> 30) & 0x1ff;
}
// extract the offset of the page directory 
// from a virtual address
static uint32_t pml4_offset(uint64_t virt) {
    return (virt >> 39) & 0x1ff;
}

// PWT: page level write-through
// PCD: page level cache disable
static void* create_table_entry(void* entry, unsigned flags) {
    assert_aligned(entry, 0x1000);

    return  (void*)(flags |
                    (uint64_t)entry);
}

// extract the pointer from an entry of 
// a table structure
static void* extract_pointer(void* c)  {
    return (void*)(0x000ffffffffff000llu & (uint64_t)c);
}


// the current page flags 
//static unsigned current_page_flags;



/*
static void physical_allocator_callback_kernel(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   size) {
    assert(is_kernel_data(virtual_address));

    (void)(physical_address + virtual_address + size);
}

void physical_allocator_callback_user(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   size) {

    assert(is_user(virtual_address));
    
    
    (void)(physical_address + virtual_address + size);
}

void physical_allocator_callback_mmio(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   size) {

    assert(is_mmio(virtual_address));

    (void)(physical_address + virtual_address + size);
}
*/

// map the data of the allocator
static void map_physical_memory(const struct stivale2_struct_tag_memmap* memmap) {
// as we are not in a callback function,
// we can realloc page tables on the fly 
    alloc_page_table_realloc = 1;


    for(unsigned i = 0; i < memmap->entries; i++) {
        const struct stivale2_mmap_entry* e = &memmap->memmap[i];

        if(e->type ==  STIVALE2_MMAP_USABLE || e->type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE
          || e->type == STIVALE2_MMAP_ACPI_RECLAIMABLE || 
          e->type == STIVALE2_MMAP_ACPI_NVS) {
            // be inclusive!
            uint64_t phys_addr = (uint64_t) (e->base / 0x1000) * 0x1000;

            size_t   size = (e->length + (e->base - phys_addr) + 0x0fff) / 0x1000;
            

            if(size == 0)
                continue;

            
            void* virtual_addr = translate_address((void *)phys_addr);


            map_pages(phys_addr, (uint64_t)virtual_addr, size, PRESENT_ENTRY);
            // use the allocator to allocate page tables
            // to map its own data
        }

    }
}
/*
static void map_allocator_data(void) {
    const struct physical_allocator_data_page_entry* entries;
    size_t size = 0;

// as we are not in a callback function,
// we can realloc page tables on the fly 
    alloc_page_table_realloc = 1;
    
    entries = physical_allocator_data_pages(&size);

    for(unsigned i = 0; i < size; i++) {
        uint64_t phys_addr = (uint64_t) entries[i].physical_address;
        uint64_t virtual_addr = TRANSLATED_PHYSICAL_MEMORY_BEGIN | phys_addr;

        map_pages(phys_addr, virtual_addr, 1, PRESENT_ENTRY);
        // use the allocator to allocate page tables
        // to map its own data
    }
}
*/

// map the kernel to KERNEL_DATA_BEGIN | kernel_phys_base
static void map_kernel(const struct stivale2_struct_tag_memmap* memmap) {

// count the number of kernel entries
// suppose the first one is the .text section,
// the second one is rodata
// and the third is data+bss
    int n = 0;


    for(unsigned i = 0; i < memmap->entries; i++) {
        const struct stivale2_mmap_entry* e = &memmap->memmap[i];

        if(e->type == STIVALE2_MMAP_KERNEL_AND_MODULES) {
            // floor the base to one page
            uint64_t base = e->base & ~0x0fff;

            uint64_t virtual_addr = base | KERNEL_DATA_BEGIN;
            
            // ceil the size
            size_t   size = e->length + (e->base - base);
            size = (size+0x0fff) / 0x1000;
   

            unsigned flags = PRESENT_ENTRY;

            switch (n++)
            {
            case 0:
                /* .text */
                flags |=  PL_XD;
                break;
            case 1:
                /* rodata */
                flags |= PL_RW;
                break;
            
            default:
                break;
            }
            //alloc the page table pages
            // before doing any allocation
            fill_page_table_allocator_buffer(64);
            map_pages(base, virtual_addr, size, flags);
        }
    }
}


// return non 0 value iif the entry is
// present
static int present_entry(void* entry) {
    return (uint64_t)entry & PRESENT_ENTRY;
} 


 //function for debug purposes

static inline void print_struct(int level, void** table, uint64_t virt) {
    void** addr = translate_address(table);

    //if(level > 1)
      //  return ;

    for(int i = 0; i < 512; i++) {
        if(present_entry(addr[i])) {
            uint64_t v = (virt << 9) | i;

            for(int i = 0; i < level; i++)
                kputs("-");

            if(level == 2) {
                kprintf(" %lx -> %lx\n", v << 12, extract_pointer(addr[i]));
            }
            else {
                kputs("\n");
                print_struct(level+1, extract_pointer(addr[i]), v);
            }
        }
    }
}





void init_paging(const struct stivale2_struct_tag_memmap* memmap) {
// init the highest paging structures
// no need to use the translate_address macro
// as we are still in the early memory configuration
// so memory is both identity mapped and transtated 
// so *x = *translate_address(x)

    //pml4[0]   = create_table_entry(
    //                    (void*)early_virtual_to_physical(pdpt_low), 
    //                    PRESENT_ENTRY
    //                );
    //pml4[256] = create_table_entry(
    //                    (void*)early_virtual_to_physical(pdpt_mid), 
    //                    PRESENT_ENTRY | PL_US
    //                );
                    

    // the high half memory is supervisor only
    // so that no user can access it eventhough the entry
    // stays in the pml4!  
    //pml4[511] = create_table_entry(
    //                    (void*)early_virtual_to_physical(pdpt_high), 
    //                    PRESENT_ENTRY | PL_US
    //                );


// map all the memory to 0xffff800000000000
    map_physical_memory(memmap);


// everytime we allocate a page table
// while allocating some memory,
// we need to put this to 0
// in order to avoid awful recursion bugs 
    alloc_page_table_realloc = 0;



// map the kernel 
    map_kernel(memmap);
}

void append_paging_initialization(void) {

// enable  PAE in cr4 
// disable PCIDE
    set_cr4((get_cr4() | CR4_PAE_BIT) & ~CR4_PCIDE);

// enable the PG bit
    set_cr0(get_cr0() | CR0_PG_BIT);
// get the physical address of the pml4 table
    uint64_t pml4_lower_half_ptr = early_virtual_to_physical(pml4);

// finaly set the pml4 to cr3
    _cr3(pml4_lower_half_ptr);

}


// buffer the allocation requests
// ask for 16 pages per call is the
// optimal thing as its the granularity
// of the highest level bitmap
static void* page_table_allocator_buffer[64];
static size_t page_table_allocator_buffer_size = 0;


// callback for the page allocator
static void page_table_allocator_callback(uint64_t phys_addr, 
                              uint64_t virt_addr,
                              size_t    size) {
    (void)(size+virt_addr); // the size is always one whatsoever...
    page_table_allocator_buffer[page_table_allocator_buffer_size++] = (void*)phys_addr;
}


static void zero_page_table_page(void* physical_address) {
    assert_aligned(physical_address, 0x1000);
    memset(translate_address(physical_address), 0, 0x1000);
}



// fill the page table allocator buffer
static void fill_page_table_allocator_buffer(size_t n) {
    assert(n <= 64);
    
    int to_alloc = n - page_table_allocator_buffer_size;
    
    if(to_alloc < 0)
        return;

    int old_size = page_table_allocator_buffer_size;

    physalloc(to_alloc, 0, page_table_allocator_callback);
    for(unsigned i = old_size; i < n; i++)
        zero_page_table_page(page_table_allocator_buffer[i]);
    page_table_allocator_buffer_size = n;
}


// return a newly allocated zeroed page
static void* alloc_page_table(void) {
    if(! page_table_allocator_buffer_size) {
        if(!alloc_page_table_realloc)
            panic(
                "alloc_page_table(): out of buffered pages, unable to allocate "
                "page tables"
            );

        physalloc(16, 0, page_table_allocator_callback);
        page_table_allocator_buffer_size = 16;
        
        for(int i = 0; i < 16; i++)
            zero_page_table_page(page_table_allocator_buffer[i]);
    }

    return page_table_allocator_buffer[--page_table_allocator_buffer_size];
}


static void* get_entry_or_allocate(void** restrict table, unsigned index)  {
    assert(index < 512);

    void** virtual_addr_table =  translate_address(table);

    void* entry = virtual_addr_table[index];

    if(!present_entry(entry)) {
        void* e = create_table_entry(
            alloc_page_table(), 
            PRESENT_ENTRY);
        return virtual_addr_table[index] = e;
    }
    else
        return entry;
}


void map_pages(uint64_t physical_addr, 
               uint64_t virtual_addr, 
               size_t count,
               unsigned flags) {
    
    while(count > 0) {
        // fetch table indexes
        unsigned pml4i = pml4_offset(virtual_addr),
                pdpti = pdpt_offset(virtual_addr),
                pdi   = pd_offset(virtual_addr),
                pti   = pt_offset(virtual_addr);

        assert(pml4i == 0 || pml4i == 511 || pml4i == 256);
        // those entries should exist
        pml4e restrict pml4entry = extract_pointer(get_entry_or_allocate((void**)pml4,      pml4i));
        pdpte restrict pdptentry = extract_pointer(get_entry_or_allocate((void**)pml4entry, pdpti));
        pde   restrict pdentry   = extract_pointer(get_entry_or_allocate((void**)pdptentry, pdi));

        while(count > 0 && pti < 512) {
            // create a new entry
            void*  e = create_table_entry((void*)physical_addr,flags);
            
            void** entry_ptr = (void**)translate_address(pdentry) + pti;
            

            if(present_entry(*entry_ptr)) {
                char buff[256];
                sprintf(buff, 
                    "map_pages(...,flags=%u):\n"
                    " tried to map physical memory 0x%lx to 0x%lx, but physical memory 0x%lx"
                    " was already mapped here",
                    flags, physical_addr, virtual_addr, extract_pointer(*entry_ptr));
                
                panic(buff);
            }

            
            *entry_ptr = e;
            
            pti++;
            count--;
            physical_addr += 0x1000;
            virtual_addr  += 0x1000;
        }
    }
}


void alloc_pages(void*  virtual_addr_begin, 
               size_t   count,
               unsigned flags) {
    // don't allow recusion
    alloc_page_table_realloc = 0;


    void callback(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   c) {
            
        
        map_pages(physical_address,
                  virtual_address,
                  c,
                  flags);
    };

    while(count > 0) {
        unsigned size = count;
        if(size > MAX_ALLOC)
            size = MAX_ALLOC;

        fill_page_table_allocator_buffer(16);

        physalloc(size, virtual_addr_begin, callback);

        count -= size;
        virtual_addr_begin += size * 0x1000;
    }
}
