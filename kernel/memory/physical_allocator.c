#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>
#include "../debug/assert.h"

#include "../klib/sprintf.h"
/**
 * memory modeling:
 * | MR1 | MR2 | //// | MR3 | .... 
 * MR = 64 MB memory range
 * 
 * MR linked list entry: {
 *      MR_base,   // base address of the MR
 *      MR_length, // length, in pages  
 *      MR_next,   // the next MR in the list, or null
 * }
 * 
 * 4 global linked lists:
 *      memory_list[i] = MR linked list entry *
 *  ML0: at least 1 64K contiguous available range (very fast allocation)
 *  ML1: at least 1 32K contiguous available range (fast allocation)
 *  ML1: at least 1 16K contiguous available range (medium allocation)
 *  ML1: at least 1 4K  contiguous available range (slow allocation)
 * 
 *
 *  MR layout;
 *  | ----------------------------- 4K HEADER --------------------------- |      BLOCKS      |
 *  | 1152 header | 128 bitmap3 |  256 bitmap2 | 512 bitmap1 | 2K bitmap0 | 64 MB - 4K pages |
 *  
 *  header: {
 *      size,     // of the range, in pages
 *      rem0,     // number of free       pages (4k)
 *      rem1,     // number of free super pages (16k)
 *      rem2,     // number of free mega  pages (32k) 
 *      rem3,     // number of free ultra pages (64k) 
 *  }
 * 
 * 
*/

// 64 MB range = 16384 pages
#define MAX_SIZE 16384

// drop the segments that are too small
#define MIN_SIZE 10

//linked list element representing a 64 MB memory region
struct memory_range {
    void* base;
    size_t length;

    struct memory_range* next;
};

struct MR_header {
// number of available blocks for each block size bitmap
    uint32_t available[4];

// padding
    uint8_t padding[1152 - 16];

    uint8_t bitmap_level3[128];
    uint8_t bitmap_level2[256];
    uint8_t bitmap_level1[512];
    uint8_t bitmap_level0[2048];
};


static_assert_equals(sizeof(struct MR_header), 4096);

// 512 x 64MB regions: up to 32 Go of RAM
struct memory_range buffer[512];


// lists of memory ranges: sorted by biggest free range
struct memory_range* mr_lists[4] = {0};


// init memory range as a free range, ready to be allocated
static void init_memory_range(struct memory_range* range, uint64_t addr, size_t length) {
// init the linked list's structure
    range->base = addr;
    range->length = length;

// init the MR header
    struct MR_header* header = (struct MR_header *)addr;
    
    // we use one page per region for the header
    header->available[0] = (length-1);
    header->available[1] = (length-1) >> 2;
    header->available[2] = (length-1) >> 3;
    header->available[3] = (length-1) >> 4;

// choose the right linked list to insert the element in
// then insert it at the beginning: so the insertion is O(1)
    for(int i = 3; i >= 0; i--) {
        if(header->available[3] == 0)
            continue;
        
        range->next = mr_lists[i];
        mr_lists[i] = range;
        break;
    }
    
    
    // zero all the bit maps
    memset(((uint8_t*)header)+1152, 0, 2048);
}


void init_physical_allocator(const struct stivale2_struct_tag_memmap* memmap) {
// counter of the number of detected pages
    unsigned total_pages = 0;

    unsigned j = 0; // index of the current created memory range struct 
    for(unsigned i = 0; i < memmap->entries; i++) {

        const struct stivale2_mmap_entry e = memmap->memmap[i];
        
    // dont take kernel & modules or acpi reclaimable
    // memory ranges in account
        if(e.type == STIVALE2_MMAP_USABLE || 
           e.type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE ||
           e.type == STIVALE2_MMAP_FRAMEBUFFER) {

        // ceil the size to the number of pages
            size_t size = (e.length + 4096 - 1) / 4096;

            uint64_t base = e.base;

            while(size > 0) {

                size_t s;// the actual size of the current 

                if(size > MAX_SIZE)
                    s = MAX_SIZE;
                else if(size < MIN_SIZE)
                    break;
                else
                    s = size;

                struct memory_range* range = &buffer[j++];
                init_memory_range(range, base, s);
                

                kprintf("%l16x: %d pages\n", range->base, range->length);

                total_pages += s;
                size -= s;
                base += 0x1000 * s;
                
            }
        }
    }
    kprintf("detected %u MB of memory, %u usable\n", total_pages / 256, (total_pages - j) / 256);
}


// return a MR that fits best the requested size
// it can however not fit it if no MR does
static void getMR(size_t requested_size) {
}

/**
 * @brief allocate 
 * 
 * @param size in 4K pages
 * @return void* the physical allocated address
 */
void* physalloc(size_t size) {
}

/**
 * size is in pages 
 * 
 */
void physfree(void* virtual_addr, size_t size) {
    (void) (virtual_addr + size);
}