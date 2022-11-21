#include "../lib/assert.h"
#include "../lib/string.h"
#include "pmm.h"

#include "paging.h"
#include "vmap.h"
#include "../lib/sprintf.h"
#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../lib/registers.h"
#include "../smp/smp.h"
#include "../int/idt.h"
#include "../boot/boot_interface.h"


#define CR0_WP            (1lu << 16)
#define CR0_PG_BIT        (1lu << 31)
#define CR4_PAE_BIT       (1lu << 5)
#define CR4_PCIDE         (1lu << 17)
#define IA32_EFER_NXE_BIT (1lu << 11)

// size of bulks of allocation
// the page buffer is 16 long
// in the worst case scenario,
// 8192 page allocs 
// -> 1 pdpt + 1 pd + 9 pt 
// = 11 newpages
#define MAX_ALLOC 4096


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


static_assert_equals(sizeof(pml4),     0x1000);


// the alloc_page_table function  has the right
// to realloc on the fly
// unset it to avoid nasty recursions
static int alloc_page_table_realloc = 1;
static void fill_page_table_allocator_buffer(size_t n);
static void* alloc_page_table(void);


// buffer the allocation requests
// ask for 16 pages per call is the
// optimal thing as its the granularity
// of the highest level bitmap
#define PTAAB_SIZE 64
#define PTAAB_REFILL 16
static void* page_table_allocator_buffer[PTAAB_SIZE];
static size_t page_table_allocator_buffer_size = 0;


static void internal_map_pages(uint64_t physical_addr, 
                               uint64_t virtual_addr, 
                               size_t  count,
                               uint64_t flags);

// extract the offset of the page table 
// from a virtual address
__attribute__((pure))
static uint32_t pt_offset(uint64_t virt) {
    return (virt >> 12) & 0x1ff;
}
// extract the offset of the page directory 
// from a virtual address
__attribute__((pure))
static uint32_t pd_offset(uint64_t virt) {
    return (virt >> 21) & 0x1ff;
}
// extract the offset of the pdp 
// from a virtual address
__attribute__((pure))
static uint32_t pdpt_offset(uint64_t virt) {
    return (virt >> 30) & 0x1ff;
}
// extract the offset of the page directory 
// from a virtual address
__attribute__((pure))
static uint32_t pml4_offset(uint64_t virt) {
    return (virt >> 39) & 0x1ff;
}


static inline int is_master_aligned(uint64_t virt) {
    return (virt & (PAGE_MASTER_SIZE - 1)) == 0;
}

// PWT: page level write-through
// PCD: page level cache disable
static void* create_table_entry(void* entry, uint64_t flags) {
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



/*
static void pmm_callback_kernel(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   size) {
    assert(is_kernel_data(virtual_address));

    (void)(physical_address + virtual_address + size);
}

void pmm_callback_user(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   size) {

    assert(is_user(virtual_address));
    
    
    (void)(physical_address + virtual_address + size);
}

void pmm_callback_mmio(
            uint64_t physical_address, 
            uint64_t virtual_address,
            size_t   size) {

    assert(is_mmio(virtual_address));

    (void)(physical_address + virtual_address + size);
}
*/

// map the physical memory, using the map given by the bootloader
// to the translated memory region. also map the kernel according
// to vmap.h.
static void map_physical_memory(const struct boot_interface* bi) {
// as we are not in a callback function,
// we can realloc page tables on the fly 
    alloc_page_table_realloc = 1;

    struct mmape* e;


// count the number of kernel entries
// suppose the first one is the .text section,
// the second one is rodata
// and the third is data+bss
// @todo check ELF header maybe
    int kernel_entry_i = 0;

    while((e = bi->mmap_get_next()) != NULL) {

        if(e->type == USABLE 
        || e->type == BOOTLOADER_RECLAIMABLE
        || e->type == ACPI_RECLAIMABLE
        || e->type == ACPI_NVS) {
            // be inclusive!
            uint64_t phys_addr = (uint64_t) (e->pbase / 0x1000) * 0x1000;

            size_t   size = (e->length + (e->pbase - phys_addr) + 0x0fff) / 0x1000;
            

            if(size == 0)
                continue;

            
            void* virtual_addr = translate_address((void *)phys_addr);


            internal_map_pages(
                phys_addr, 
                (uint64_t)virtual_addr, 
                size, 
                PRESENT_ENTRY | PL_XD | PL_RW
            );
            // use the allocator to allocate page tables
            // to map its own data
        }

        if(e->type == KERNEL) {
            // floor the base to one page
            uint64_t base = e->pbase & ~0x0fff;

            
            // ceil the size
            size_t   size = e->length + (e->pbase - base);
            size = (size+0x0fff) / 0x1000;
   
            uint64_t virtual_addr = base | KERNEL_DATA_BEGIN;

            uint64_t flags = PRESENT_ENTRY;

            switch (kernel_entry_i++)
            {
            case 0:
                /* .text */
                break;
            case 1:
                /* rodata */
                flags |= PL_XD;
                break;
            case 2:
                flags |= PL_RW;
                /* data+bss */
                break;
            default:
                //modules: do not map in higher half!
                flags |= PL_RW;
                virtual_addr = base | TRANSLATED_PHYSICAL_MEMORY_BEGIN;
                break;
            }
            // fill_page_table_allocator_buffer(PTAAB_SIZE);
            internal_map_pages(base, virtual_addr, size, flags);
        }
    }
}
/*
static void map_allocator_data(void) {
    const struct pmm_data_page_entry* entries;
    size_t size = 0;

// as we are not in a callback function,
// we can realloc page tables on the fly 
    alloc_page_table_realloc = 1;
    
    entries = pmm_data_pages(&size);

    for(unsigned i = 0; i < size; i++) {
        uint64_t phys_addr = (uint64_t) entries[i].physical_address;
        uint64_t virtual_addr = TRANSLATED_PHYSICAL_MEMORY_BEGIN | phys_addr;

        internal_map_pages(phys_addr, virtual_addr, 1, PRESENT_ENTRY);
        // use the allocator to allocate page tables
        // to map its own data
    }
}
*/

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
                puts("-");

            if(level == 2) {
                printf(" %lx -> %lx\n", v << 12, extract_pointer(addr[i]));
            }
            else {
                puts("\n");
                print_struct(level+1, extract_pointer(addr[i]), v);
            }
        }
    }
}





void init_paging(const struct boot_interface* bi) {
// init the highest paging structures
// no need to use the translate_address macro
// as we are still in the early memory configuration
// so memory is both identity mapped and transtated 
// so *x = *translate_address(x)

// initialization: we map some memory regions
// statically: we don't use the pmm in order to
// find what is to map.
// we therefore can allocate page tables on
// the fly
    alloc_page_table_realloc = 1;


// @todo add missing regions (see vmap.h)
// first, let's create the main memory regions:
// 1st   user:       0x0000000000000000 -> 0x0000007fffffffff
// 256th supervisor: 0xffff800000000000 -> 0xffff807fffffffff
// 511st supervisor: 0xffffff8000000000 -> 0xffffffffffffffff

// we don't map the user space yet.
// for every process, we need to allocate new page map
// structures, meaning allocating the pml4's entries entry
// and mapping it using the map_master_region() function

// the two high half memory regions are supervisor only
// so that no user can access it eventhough the entry
// stays in the pml4 table  
    pml4[256] = create_table_entry(
            alloc_page_table(),   // once again use the pmm
            PRESENT_ENTRY | PL_RW // supervisor only
    );
                    
// same as above
    pml4[511] = create_table_entry(
            alloc_page_table(),   // once again use the pmm
            PRESENT_ENTRY | PL_RW // supervisor only  
    );


// map all the memory to 0xffff800000000000
    map_physical_memory(bi);


// everytime we allocate a page table
// while allocating some memory,
// we need to put this to 0
// in order to avoid awful recursion bugs 
    alloc_page_table_realloc = 0;

}



void append_paging_initialization(void) {

// enable  PAE in cr4 
// disable PCIDE
    set_cr4((get_cr4() | CR4_PAE_BIT) & ~CR4_PCIDE);

// enable the PG bit
    set_cr0(get_cr0() | CR0_PG_BIT | CR0_WP);

// enable NXE bit
    write_msr(IA32_EFER_MSR, read_msr(IA32_EFER_MSR) | IA32_EFER_NXE_BIT);

// get the physical address of the pml4 table
    uint64_t pml4_lower_half_ptr = kernel_data_to_physical(pml4);

// finaly set the pml4 to cr3
    _cr3(pml4_lower_half_ptr);

}



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


    uint8_t* v = translate_address(physical_address);

    for(int i = 0; i < 0x1000; i++) {
        assert(v[i] == 0);
    }
}



// fill the page table allocator buffer
static void fill_page_table_allocator_buffer(size_t n) {
    assert(n <= PTAAB_SIZE);
    
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
    uint64_t rf = get_rflags();
    _cli();

    if(! page_table_allocator_buffer_size) {
        if(!alloc_page_table_realloc)
            panic(
                "alloc_page_table(): out of buffered pages, unable to allocate "
                "page tables"
            );

        physalloc(PTAAB_REFILL, 0, page_table_allocator_callback);

        page_table_allocator_buffer_size = PTAAB_REFILL;
        
        for(int i = 0; i < PTAAB_REFILL; i++)
            zero_page_table_page(page_table_allocator_buffer[i]);

    }

    void* ptable = page_table_allocator_buffer[--page_table_allocator_buffer_size];

    set_rflags(rf);

    return ptable;
}


static void free_page_table(uint64_t pt) {
    
    // try to emplace pt in the allocator buffer

    // if the buffer is full already, we cannot store it
    // we need to free it
    if(page_table_allocator_buffer_size == PTAAB_SIZE) {
        physfree(pt);
    }

    // if the buffer is not full, we can store it
    else {
        zero_page_table_page((void*)pt);
        page_table_allocator_buffer[page_table_allocator_buffer_size++] = (void*)pt;
    }
}


static void* get_entry_or_allocate(void** restrict table, unsigned index)  {
    assert(index < 512);

    void** virtual_addr_table =  translate_address(table);

    void* entry = virtual_addr_table[index];

    if(!present_entry(entry)) {

        void* e = create_table_entry(
            alloc_page_table(), 
            PRESENT_ENTRY | PL_US | PL_RW);
        return virtual_addr_table[index] = e;
    }
    else
        return entry;
}

// kernel panic if the entrty is not present
static void* get_entry_or_panic(void** restrict table, unsigned index) {
    assert(index < 512);

    void** virtual_addr_table =  translate_address(table);

    void* entry = virtual_addr_table[index];

    if(!present_entry(entry)) {
        log_warn("trying to access non-present entry %u", index);
        panic("get_entry_or_panic(): entry not present");
    }
    else
        return entry; 
}

/**
 * this function cannot be called in a callback
 * because it would lead to recursion.
 * therefore we put it static internal
 * and wrap it in a fresh public function
 */
static void internal_map_pages(uint64_t physical_addr, 
                               uint64_t virtual_addr, 
                               size_t   count,
                               uint64_t flags) {
    
    while(count > 0) {
        
        // fetch table indexes
        unsigned pml4i = pml4_offset(virtual_addr),
                pdpti = pdpt_offset(virtual_addr),
                pdi   = pd_offset(virtual_addr),
                pti   = pt_offset(virtual_addr);
        
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
                    "internal_map_pages(...,flags=%lu):\n"
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


void remap_pages(void*    vaddr_ptr, 
                 size_t   count,
                 uint64_t flags) {
    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();

    uint64_t virtual_addr = (uint64_t)vaddr_ptr;
    
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
            
            void** entry_ptr = (void**)translate_address(pdentry) + pti;
            

            if(!present_entry(*entry_ptr)) {

                char buff[256];

                sprintf(buff, 
                    "remap_pages(...,flags=%lu):\n"
                    " tried to remap virtual memory 0x%lx, nothing was mapped there",
                    flags, virtual_addr
                );
                
                panic(buff);
            }

            // extract physical page and remake an entry with 
            // the given flags
            void*  e = create_table_entry(0,flags);

            *entry_ptr = e;
            
            pti++;
            count--;
            virtual_addr  += 0x1000;               
        }
    }

    // end of mutual exclusion
    set_rflags(rf);
}




// @todo do better
// private to alloc_pages
static uint64_t cpu_private_flags[64];


void alloc_pages_callback(
        uint64_t physical_address, 
        uint64_t virtual_address,
        size_t   c) {
    internal_map_pages(physical_address,
                virtual_address,
                c,
                cpu_private_flags[get_smp_count()]);
};

void alloc_pages(void*  virtual_addr_begin, 
               size_t   count,
               uint64_t flags) {
    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();
    
    // don't allow recusion
    alloc_page_table_realloc = 0;


    cpu_private_flags[get_smp_count()] = flags;

    while(count > 0) {
        unsigned size = count;
        if(size > MAX_ALLOC)
            size = MAX_ALLOC;

        fill_page_table_allocator_buffer(PTAAB_REFILL);

        physalloc(size, virtual_addr_begin, alloc_pages_callback);

        count -= size;
        virtual_addr_begin += size * 0x1000;
    }

    // end of mutual exclusion
    set_rflags(rf);
}

void map_pages(uint64_t physical_addr, 
               uint64_t virtual_addr, 
               size_t   count,
               uint64_t flags) {
    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();


    assert(physical_addr);
    assert(virtual_addr);

    while(count > PTAAB_SIZE) {
        fill_page_table_allocator_buffer(PTAAB_SIZE);
        internal_map_pages(physical_addr, virtual_addr, 64, flags);
        count -= 64;

        physical_addr += 0x1000 * 64;
        virtual_addr  += 0x1000 * 64;
    }

    // count <= 64
    fill_page_table_allocator_buffer(PTAAB_SIZE);
    internal_map_pages(physical_addr, virtual_addr, count, flags);

    // end of mutual exclusion
    set_rflags(rf);
}

// return 1 iif the no entry is present in the range
static inline int is_table_empty(pte* page_table, unsigned begin, unsigned end) {
    assert(begin <= end);
    assert(end <= 512);

    pte* translated = translate_address(page_table);

    for(unsigned i = begin; i < end; i++) {
        if(present_entry(translated[i]))
            return 0;
    }
    return 1;
}

static pte* get_page_table_or_panic(uint64_t vaddr) {
    unsigned pml4i = pml4_offset(vaddr),
             pdpti = pdpt_offset(vaddr),
             pdi   = pd_offset(vaddr);

    assert(pml4i == 0 || pml4i == 511 || pml4i == 256);
    // those entries should exist

    pml4e restrict pml4entry = extract_pointer(
                    get_entry_or_panic((void**)pml4,      pml4i));
    
    pdpte restrict pdptentry = extract_pointer(
                    get_entry_or_panic((void**)pml4entry, pdpti));
    

    return extract_pointer(get_entry_or_panic((void**)pdptentry, pdi));
}

void unmap_pages(uint64_t virtual_addr, size_t count, int free) {
    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();

    while(count > 0) {
        // fetch table indexes
        void** pt = translate_address(
                        get_page_table_or_panic(virtual_addr));
        

        // unmap multiple pages in the page table
        // without recalculating it
        unsigned pti   = pt_offset(virtual_addr);
        
        // keep track of the first & last element
        // to unmap, we are sure that in this range
        // everything is unmapped
        unsigned begin = pti;

        while(count > 0 && pti < 512) {
            void** entry_ptr = pt + pti;
            

            if(!present_entry(*entry_ptr)) {

                char buff[256];

                sprintf(buff, 
                    "unmap_pages(...):\n"
                    " tried to unmap not mapped virtual memory 0x%lx",
                    virtual_addr);
                
                panic(buff);
            }

            if(free) {
                uint64_t paddr = (uint64_t)extract_pointer(*entry_ptr);
                physfree(paddr);
            }

            // actually erase the entry
            *entry_ptr = 0;

            pti++;
            count--;
            virtual_addr  += 0x1000;      
        }

        (void) begin;
        /** @TODO unmap pages if empty
         * 
         */
        /*
        unsigned end = pti;
        // unmap the page map if empty
        if(is_table_empty(pdentry, 0, begin) &&
           is_table_empty(pdentry, end, 512))
        {
            // the page table contains no entry
            // let's free it
//            physfree(pdentry);
        }
        */
    }

    // end of mutual exclusion
    set_rflags(rf);
}

uint64_t get_paddr(const void* vaddr) {
    unsigned pti = pt_offset((uint64_t)vaddr);

    pte entry = get_page_table_or_panic((uint64_t)vaddr)[pti];
    
    assert(present_entry(entry));

    uint64_t page_paddr = (uint64_t)extract_pointer(entry);

    return page_paddr | ((uint64_t)vaddr & 0x0fff);
}





static void flush_tlb(void) {
    _cr3(trk2p(pml4));
}


uint64_t create_empty_pd(void) {
    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();

    uint64_t p = (uint64_t)alloc_page_table();

    // end of mutual exclusion
    set_rflags(rf);


    uint8_t* v = translate_address((void*)p);

    for(int i = 0; i < 0x1000; i++) {
        assert(v[i] == 0);
    }


    return (uint64_t)p;
}


// deep free the page map:
// recursively free all the page tables 
// that it references
// page_table: physical address of the page structure.
// the page structure is, in funciton of the level 
// argument:
// level: 4: pml4, 3: pdpt, 2: pd, 1: pt
// level: 0: page
static void deep_free_map(uint64_t page_table, int level) {
    if(!level) {
        physfree(page_table);
        return;
    }

    void** translated = (void**)translate_address((void*)page_table);

    for(unsigned i = 0; i < 512; i++) {
        if(present_entry(translated[i])) {
            uint64_t page_table_addr = (uint64_t)extract_pointer(translated[i]);

            if(level == 1) {
                // shortcut the last level
                physfree(page_table_addr);
            }
            else
                deep_free_map(page_table_addr, level - 1);
            
            translated[i] = 0;
        }
    }

    free_page_table(page_table);
}


void free_master_region(uint64_t pd) {
    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();

    // deep free the page map
    deep_free_map(pd, 3);

    // end of mutual exclusion
    set_rflags(rf);
}



static void* get_entry(void* table, unsigned index) {    
    assert(index < 512);

    void** virtual_addr_table =  translate_address(table);

    assert(virtual_addr_table);

    return (void *)virtual_addr_table[index];
}



void free_page_range(uint64_t vaddr, size_t count) {
    // 512 GB aligned
    assert(vaddr % (1llu << 39) == 0);
    assert(count % (1llu << 39) == 0);

    // mutual exclusion
    uint64_t rf = get_rflags();
    _cli();


    while(count) {
        unsigned pml4i = pml4_offset(vaddr);
        void* pml4entry = get_entry((void**)pml4, pml4i);

        if(!present_entry(pml4entry)) {
            // not present pml4 entry:
            // increase vaddr to the next 512 GB range
            uint64_t _1_39 = 1llu << 39;

            vaddr = (vaddr & ~(_1_39 - 1)) + _1_39;
            continue;
        }

        uint64_t pdpt_paddr = (uint64_t)extract_pointer(pml4entry);

        assert(vaddr % (1llu << 39) == 0);
        // vaddr is aligned on 512 GB
        // let's free 512 GB
        deep_free_map(pdpt_paddr, 3);
        // unmap
        pml4[pml4i] = 0;
        vaddr += 1llu << 39;

        count -= 1llu << (39);
    }


    set_rflags(rf);
}



void unmap_master_region(void* base) {
    // flags 0: unmaped 
    map_master_region(base, 0, 0);
}


void map_master_region(void* base, uint64_t pd, uint64_t flags) {
    assert(is_master_aligned((uint64_t)base));

    unsigned pml4i = pml4_offset((uint64_t)base);

    pml4e old = pml4[pml4i];

    pml4[pml4i] = create_table_entry(
            (void*)pd,
            flags
    );

    if(pml4[0] != old)
        flush_tlb();
}


uint64_t get_master_pd(void* base) {
    assert(is_master_aligned((uint64_t)base));

    unsigned pml4i = pml4_offset((uint64_t)base);


    return (uint64_t)extract_pointer(pml4[pml4i]);
}




uint64_t get_phys_addr(const void* vaddr) {
    uint64_t  page_offset = (uint64_t)vaddr &  0xfffllu;

    unsigned pml4i = pml4_offset((uint64_t)vaddr);
    unsigned pdpti = pdpt_offset((uint64_t)vaddr);
    unsigned pdi   = pd_offset  ((uint64_t)vaddr);
    unsigned pti   = pt_offset  ((uint64_t)vaddr);

    pml4e restrict pml4entry = get_entry((void**)pml4,      pml4i);
    if(!present_entry(pml4entry))
        return 0;

    pdpte restrict pdptentry = get_entry((void**)extract_pointer(pml4entry), pdpti);
    if(!present_entry(pdptentry))
        return 0;

    pde restrict pdentry    = get_entry((void**)extract_pointer(pdptentry), pdi);
    if(!present_entry(pdentry))
        return 0;

    pte restrict ptentry    = get_entry((void**)extract_pointer(pdentry), pti);
    if(!present_entry(ptentry))
        return 0;


    uint64_t ppage_base = (uint64_t) extract_pointer(ptentry);

    assert((ppage_base & 0xfff) == 0);
    assert(page_offset < 0x1000);

    return ppage_base | page_offset;
}
