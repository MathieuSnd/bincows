#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>
#include <stdbool.h>

#include "../lib/string.h"
#include "../lib/sprintf.h"
#include "../lib/assert.h"
#include "../lib/panic.h"
#include "../lib/registers.h"
#include "../int/idt.h"

#include "physical_allocator.h"
#include "../lib/logging.h"
#include "vmap.h"

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

    union {
        struct {
            uint8_t bitmap_level3[128];
            uint8_t bitmap_level2[256];
            uint8_t bitmap_level1[512];
            uint8_t bitmap_level0[2048];
        };
        uint8_t bitmaps[128+256+512+2048];
    };
};



static_assert_equals(sizeof(struct MR_header), 4096);

// 512 x 64MB regions: up to 32 Go of RAM
// this buffer is sorted by base address
static struct memory_range memory_ranges_buffer[512];

// number of ranges considered 
static unsigned n_ranges = 0;

static unsigned total_available_pages = 0;
static unsigned total_memory_pages = 0;

// lists of memory ranges: sorted by biggest free range
static struct memory_range* mr_lists[4] = {0};


// init memory range as a free range, ready to be allocated
// it should only be called with early memory configuration:
// identity mapped stuf
static void init_memory_range(struct memory_range* range, uint64_t addr, size_t length) {

    assert_aligned(addr, 0x1000);

// init the linked list's structure
    range->base = (void *)addr;
    range->length = length-1;

// init the MR header
    struct MR_header* header = (struct MR_header *)addr;


    // zero all the bit maps
    memset(header->bitmaps, 0, 128+256+512+2048);

    // we use one page per region for the header
    header->available[0] = (length-1);

    // we have to ceil to take the last few pages in account
    header->available[1] = (length-1 + 4-1) >> 2;
    header->available[2] = (length-1 + 8-1) >> 3;
    header->available[3] = (length-1 + 16-1) >> 4;

// choose the right linked list to insert the element in
// then insert it at the beginning: so the insertion is O(1)
    for(int i = 3; i >= 0; i--) {
        if(header->available[i] == 0)
            continue;
        
        range->next = mr_lists[i];
        mr_lists[i] = range;
        break;
    }
}


void init_physical_allocator(const struct stivale2_struct_tag_memmap* memmap) {
// counter of the number of detected pages
    unsigned total_pages = 0;

    unsigned j = 0; // index of the current created memory range struct 
    for(unsigned i = 0; i < memmap->entries; i++) {

        const struct stivale2_mmap_entry e = memmap->memmap[i];
    // memory ranges in account
        
    // dont take kernel & modules or acpi reclaimable
        if(e.type == STIVALE2_MMAP_USABLE) {

        // ceil the size to the number of pages

            uint64_t base = e.base;
            size_t size = e.length;

            if(base & 0x0fff) {
                uint64_t new_base = (base & 0x0fff) + 0x1000;
                size = new_base - base;
                base = new_base;
            }

            // floor the size 
            size /= 4096;


            while(size > 0) {


                size_t s;// the actual size of the current 

                if(size > MAX_SIZE)
                    s = MAX_SIZE;
                else if(size < MIN_SIZE)
                    break;
                else
                    s = size;


                struct memory_range* range = &memory_ranges_buffer[j++];
                init_memory_range(range, base, s);

                total_pages += s;
                size -= s;
                base += 0x1000 * s;
                
            }
        }
    }

    n_ranges = j;
    total_memory_pages = total_available_pages = total_pages - n_ranges;
    log_info(
        "found %u MB of usable memory in the system (%u pages, %u physical ranges)",
        total_memory_pages * 4 / 1024,
        total_memory_pages,
        n_ranges
    );
}


// return a MR that fits best the requested size
// it can however not fit it if no MR does
// max_block_size_id: index of the linked list in which we found the MR
// the returned page is always the first element of the max_block_size_id th list
static struct memory_range* getMR(size_t requested_size, unsigned* max_block_size_id) {

// we don't use the size yet
    (void) requested_size;


// worst fit: always return the memory range for which we know that  
// a big free contiguous memory range is available 
    for(int i = 3; i >= 0; i--) {
        struct memory_range* range = mr_lists[i];

        if(range == NULL)
            continue;

        *max_block_size_id = i;


// return the first element, as it is in one of the memory range lists,
// we are sure that it is not full.        
        return range;
    }

// it shouldn't happen
    panic("internal physical allocator error");
    __builtin_unreachable();
}



/**
 * @brief select the bitmap to use to allocate a block
 * this function is to decode the max_block_size_id value
 * to select the right (ith) bitmap in the structure
 * 
 * the function also gives the granularity of the returned bitmap
 * 
 * @param header                the header of the memory range structure
 * @param max_block_size_id     the id of the linked list in which the memory 
 *                                  range has been found    
 * @param granularity           [output] granularity of the selected bitmap 
 * @return void* 
 */
static void* select_bitmap(struct MR_header* header, 
                           unsigned  max_block_size_id, 
                           unsigned* granularity) {
    switch (max_block_size_id)
        {
        case 0:
            *granularity = 1;
            return header->bitmap_level0;
            break;
        case 1:
            *granularity = 4;
            return header->bitmap_level1;
            break;
        case 2:
            *granularity = 8;
            return header->bitmap_level2;
            break;
        case 3:
            *granularity = 16;
            return header->bitmap_level3;
            break;
        
        default:
            assert(0);
            return NULL;
        }
}

// return a pointer to the MR header
static struct MR_header* get_header_base(const struct memory_range* range) {
    return (void *)((uint64_t)range->base | TRANSLATED_PHYSICAL_MEMORY_BEGIN);
}

// modifies the bitmaps to allocate
// the ith page in the 64 MB segment
static void alloc_page_bitmaps(struct MR_header* header, unsigned page) {
// process on bytes: calculate the right masks
    uint8_t mask_level0 = 1 <<  page       % 8,
            mask_level1 = 1 << (page /  4) % 8,
            mask_level2 = 1 << (page /  8) % 8,
            mask_level3 = 1 << (page / 16) % 8;

// calculate the new number of free blocks for all the sizes

    assert((header->bitmap_level0[page/8] & mask_level0) == 0);

    header->available[0]--;
    if((header->bitmap_level1[page/8/4] & mask_level1) == 0)
        header->available[1]--;
    if((header->bitmap_level2[page/8/8] & mask_level2) == 0)
        header->available[2]--;
    if((header->bitmap_level3[page/8/16] & mask_level3) == 0)
        header->available[3]--;

    assert(header->available[3] != 0xffffffff);
    assert(header->available[2] != 0xffffffff);
    assert(header->available[1] != 0xffffffff);
    assert(header->available[0] != 0xffffffff);



// actually set the bits
    header->bitmap_level0[page/8]    |= mask_level0;
    header->bitmap_level1[page/8/4]  |= mask_level1;
    header->bitmap_level2[page/8/8]  |= mask_level2;
    header->bitmap_level3[page/8/16] |= mask_level3;
}

// modifies the bitmaps to free
// the ith page in the 64 MB segment
static void free_page_bitmaps(struct MR_header* header, unsigned page) {
// process on bytes: calculate the right masks
    uint8_t mask_level0 = 1 <<  page       % 8,
            mask_level1 = 1 << (page /  4) % 8,
            mask_level2 = 1 << (page /  8) % 8,
            mask_level3 = 1 << (page / 16) % 8;


// assert that the page was previously allocated: the bit is set 
// in mask level 0 bit
    assert((header->bitmap_level0[page/8] & mask_level0) != 0);

// unset the corresponding bits
    header->bitmap_level0[page/8]    &= ~mask_level0;
    header->bitmap_level1[page/8/4]  &= ~mask_level1;
    header->bitmap_level2[page/8/8]  &= ~mask_level2;
    header->bitmap_level3[page/8/16] &= ~mask_level3;

// calculate the new number of free blocks for all the sizes

    //if((header->bitmap_level0[page/8] & mask_level0) == 0)
        header->available[0]++;
    if((header->bitmap_level1[page/8/4] & mask_level1) == 0)
        header->available[1]++;
    if((header->bitmap_level2[page/8/8] & mask_level2) == 0)
        header->available[2]++;
    if((header->bitmap_level3[page/8/16] & mask_level3) == 0)
        header->available[3]++;

}


// move the region from one list to another
// return 1 if the region is full
/// !!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!
/// !!!   THE RANGE MUST BE ON THE   !!!
/// !!!         TOP OF ITS LIST      !!!
/// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
static int move_region_if_full(struct memory_range* range, 
                                const struct MR_header* restrict header, 
                                unsigned list_id) {

// search for the number of available blocks 
    assert(list_id < 4);

    int return_value = 0;

// we can move down multiple times
    while(1) {
        unsigned available = header->available[list_id];

        if(available)
            return return_value;
        // the region isn't full
        else {
            // remove the range from the list
            mr_lists[list_id] = range->next;

            return_value = 1;

            if(list_id != 0) {
                list_id--;
                // if it isn't already 
                // on the weakest list,
                // put it in the weaker list
                range->next = mr_lists[list_id];
                mr_lists[list_id] = range;
            }
            else
                return 1;
            // else, just remove it.
            // it won't be accessible
            // and won't slow down the allocation
            // of other segments
        }
    }


}


/**
 * @brief allocate 
 * 
 * @param size in 4K pages
 * @return void* the physical allocated address
 */
void physalloc(size_t size, void* virtual_addr, PHYSALLOC_CALLBACK callback) {
    // mutual exclusion
    uint64_t rf = get_rflags();

    _cli();

    assert_aligned(virtual_addr, 0x1000);

    if(total_available_pages < size) {
        char buff[128];
        sprintf(buff, "OUT OF MEMORY: "
                      "physalloc(size=%u,virtuall_addr=0x%lx,...)",
                size, virtual_addr);
        panic(buff);
    }
    total_available_pages -= size;

    // loop through the MRs
    while(size > 0) {
        unsigned max_block_size_id = 0;
        struct memory_range* range = getMR(size, &max_block_size_id);
        

        const unsigned memory_range_length = range->length;
        
        struct MR_header* header = get_header_base(range);

// use the bitmap that maximizes the granularity,
// in accordance with the getMR() result
// maximizing the granularity is essential to reduce the traveling time
// through a bitmap
        unsigned granularity = 0;
        uint64_t* bitmap_ptr = select_bitmap(header, max_block_size_id, &granularity);

    // extract bitmap chunks, 64 per 64
        uint64_t bitmap_chunk;
        
        // iterate over at least enough bits 
        
        unsigned current_block = 0; 
    // there is a last block that handles the very last few pages
        const unsigned total_blocks = (memory_range_length+granularity-1) / granularity;

        int found_block = 0;

    // loop through the selected level bitmap
    // to find big regions
        while(current_block < total_blocks) {


            bitmap_chunk = *(bitmap_ptr++);

            uint64_t free_chunks_bitmap = ~bitmap_chunk;
            // keep traveling through the bitmap as long as no bit of the bitmap_chunk is full
            if(free_chunks_bitmap == 0) // the whole chunk is full
                current_block += 64;
            else {
                
                // loop up to 64 iterations
                int i = 0;
                
                // current block of the following loop
                unsigned local_current_block = current_block;                


                // extract the 0 bit positions from the bitmap chunk
                // we know for sure that at least one bit is unset
                for(;;i++) {

                    unsigned ffs = __builtin_ffsll(free_chunks_bitmap);


                    // if none of the bits are set,
                    // lets check the 64 next ones
                    if(ffs == 0) {
                        current_block += 64;
                        break;
                    }

                    // see https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
                    unsigned bit_pos = ffs - 1;

                    local_current_block = current_block + bit_pos;

                    // this 1 is not part of the bitmap
                    if(local_current_block >= total_blocks) {
                        current_block = local_current_block;
                        break;
                    }


                // clear the bit instead of redownloading the chunk
                    free_chunks_bitmap &= ~(1llu << bit_pos);
                
                // as the granularity isn't always 1, lets find the 
                // index the level0 bitmap of the found free region
                    unsigned curr_page = (local_current_block) * granularity;
                    

                    // if we are done (size == 0 or curr_page == range->length)
                    // then exit the whole region travelling loop
                    bool done_travelling = false;

                // loop through the region inside the lvl0 map 
                    for(unsigned j = 0; j < granularity; j++) {
                        void* target_address = range->base + (curr_page+1) * 0x1000;

                        callback((uint64_t)target_address, (uint64_t)virtual_addr, 1);
                        
                        alloc_page_bitmaps(header, curr_page);

                        found_block = 1;
                        
                        size--;
                        curr_page++;

                        virtual_addr += 0x1000;

                        if(curr_page >= range->length || size == 0) {
                            done_travelling = true;
                            break;
                            // break the main traveling loop
                        }
                    }
                    

                    if(done_travelling) {
                        current_block = local_current_block;
                        // update the current_block
                        // variable before breaking
                        break;
                    }
                }

                // now that we allocated stuff on the segment,
                // check if it became full
                if(move_region_if_full(range, header, max_block_size_id)) {
                    break;
                }

                if(size == 0)
                    break;
            }
        }
        assert(found_block);
    }

    // end of mutal exclusion
    set_rflags(rf);
}


// @todo this doesn't work in an smp system
static uint64_t physalloc_single_paddr;

void physalloc_single_callback(
    uint64_t physical_address, 
    uint64_t virtual_address, 
    size_t size
) {
    (void)(virtual_address + size);

    physalloc_single_paddr = physical_address;
}

uint64_t physalloc_single(void) {

    _cli();
    physalloc(1, NULL, physalloc_single_callback);
    volatile uint64_t paddr_copy = physalloc_single_paddr;
    _sti();


    // check that it has actually
    // allocated something
    assert(paddr_copy != 0);
    return paddr_copy;
}


// return the memory range in which the given page is
// contained
static const struct memory_range* get_memory_range(const void* addr) {
    // dichotomy
    unsigned int a = 0,
                 b = n_ranges-1;
    const struct memory_range* restrict A = &memory_ranges_buffer[0],
                             * restrict B = &memory_ranges_buffer[n_ranges-1];

// check the inital bounds
    if(addr > B->base) {
            // length is in pages 
        assert(addr < B->base+B->length * 0x1000);
        return B;
    }
    assert(addr > A->base);



        // A.base < addr < B.base
    while(addr > A->base + A->length * 0x1000) {
        unsigned c = (a+b) >> 1;

    // start fetching for the next iteration
        __builtin_prefetch(memory_ranges_buffer + ((a + c)>>1));
        __builtin_prefetch(memory_ranges_buffer + ((c + b)>>1));

        const struct memory_range* restrict C = &memory_ranges_buffer[c];

        if(addr < C->base) {
            b = c;
            B = C;
        }
        else {
            a = c;
            A = C;
        }
    }

// the address shouldn't be A->base (which is the header of the segment)
    assert(addr != A->base);
    return A;
}

/**
 * size is in pages 
 * 
 */
void physfree(uint64_t physical_page_addr) {
    // mutual exclusion
    uint64_t rf = get_rflags();

    _cli();

    assert_aligned(physical_page_addr, 0x1000);

    const struct memory_range* range = get_memory_range((void*)physical_page_addr);

    unsigned position = (physical_page_addr 
                       - (uint64_t)range->base) / 0x1000 - 1;


    free_page_bitmaps(get_header_base(range), position);
    memset(translate_address((void*)physical_page_addr), 0, 0x1000);
    
    total_available_pages++;

    // end of mutual exclusion
    set_rflags(rf);
}


static_assert_equals(
    sizeof(struct physical_allocator_data_page_entry),
    sizeof(struct memory_range));

const struct physical_allocator_data_page_entry* 
       physical_allocator_data_pages(size_t* size) {
    *size = n_ranges;
    return (struct physical_allocator_data_page_entry *)memory_ranges_buffer;
}




// return the number of available pages in the system
size_t available_pages(void) {
    return total_available_pages;
}

// return the pages in the system
size_t total_pages(void)  {
    return total_memory_pages;
}

