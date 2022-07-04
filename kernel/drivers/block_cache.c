#include "block_cache.h"
#include "../memory/heap.h"
#include "../sync/spinlock.h"
#include "../lib/panic.h"
#include "../lib/string.h"
#include "../int/idt.h"



// page cache


struct page_cache_entry {
    uint64_t lba;

    uint8_t ready_bitmap;
    uint8_t dirty_bitmap;

    void* vaddr;
};



typedef struct block_page_cache {
    struct driver* driver;

    void* vaddr_base;
    uint64_t virt_size;

    struct storage_interface* cache_interface;
    struct storage_interface* target_interface;
    
    struct page_cache_entry* entries;
} block_page_cache_t;


static size_t n_block_caches = 0;

/**
 * block caches structures
 * one per block device
 * 
 * it should remain sorted by virtual base address 
 */

static block_page_cache_t** block_caches = NULL;

// lock for the block_caches structure
// this lock is coarse grained
static fast_spinlock_t block_caches_lock = {0};



// the lock should be taken
static void* vmap_device(uint64_t vsize) {
    // we have a range of virtual memory:
    // BLOCK_CACHE_BEGIN -> BLOCK_CACHE_END.
    // 
    // find a free block of virtual memory with size at least vsize
    // and return the virtual address

    // the lock should be taken
    assert(!interrupt_enable());

    assert(vsize <= BLOCK_CACHE_END - BLOCK_CACHE_BEGIN);

    uint64_t vaddr = BLOCK_CACHE_BEGIN;
    uint64_t end = vaddr + vsize;


    for(int i = 0; i < n_block_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        
        uint64_t cache_begin = cache->vaddr_base;
        uint64_t cache_end = cache_begin + cache->virt_size;

        if(cache_begin >= vaddr && cache_begin < end) {
            // current block overlaps
            vaddr = cache_end;
        }
    }


    if(vaddr + vsize > BLOCK_CACHE_END) {
        log_warn("could not map block cache: not enough virtual memory");

        return NULL;
    }
    else
        return (void*)vaddr;
}



///////////////////////////////
///// INTERFACE FUNCTIONS /////
///////////////////////////////

static
void block_cache_async_read(struct driver* driver,
                uint64_t lba,
                void*    buf,
                size_t   count
) {
    // find the block cache with given driver

    for(int i = 0; i < n_block_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        if(cache->driver == driver) {
            // found the block cache
            // call the cache interface
            cache->target_interface->async_read(driver, lba, buf, count);
            return;
        }
    }

    panic("could not find block cache for driver");
}



static
void block_cache_write(struct driver* driver,
                uint64_t    lba,
                const void* buf,
                size_t      count
) {
    // find the block cache with given driver

    for(int i = 0; i < n_block_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        if(cache->driver == driver) {
            // found the block cache
            // call the cache interface
            cache->target_interface->write(driver, lba, buf, count);
            return;
        }
    }

    panic("could not find block cache for driver");
} 



/////////////////////////////////
//////// GLOBAL FUNCTIONS ///////
/////////////////////////////////

void block_cache_setup(
        const struct storage_interface* input, 
        struct storage_interface* output,
        const struct block_cache_params* params
) {   
    _cli();
    spinlock_acquire(&block_caches_lock);

    // create the cache
    block_page_cache_t* cache = malloc(sizeof(block_page_cache_t));

    
    cache->vaddr_base = vmap_device(params->virt_size);
    cache->virt_size = params->virt_size;
    cache->cache_interface = output;
    cache->target_interface = input;
    cache->entries = NULL;
    cache->driver = input->driver;
    
    
    // insert the cache into the list
    block_caches = realloc(block_caches, sizeof(block_page_cache_t) * (n_block_caches + 1));
    block_caches[n_block_caches] = cache;
    n_block_caches++;


    spinlock_release(&block_caches_lock);
    _sti();


    // create output storage interface
    output->driver = input->driver;
    output->async_read = block_cache_async_read;
    output->write = block_cache_write;
    output->sync = input->sync;
    output->capacity = input->capacity;
    output->lbashift = input->lbashift;
}


void block_cache_free(struct storage_interface* cache_interface) {

    _cli();
    spinlock_acquire(&block_caches_lock);

    // find the cache structure with the given cache_interface

    // block page structure to free
    block_page_cache_t* bc = NULL;

    // index of bc in the list
    unsigned i;

    for(i = 0; i < n_block_caches; i++) {
        if(block_caches[i]->cache_interface == cache_interface) {
            // found the cache
            bc = block_caches[i];
            break;
        }
    }

    if(bc == NULL) {
        _sti();
        spinlock_release(&block_caches_lock);

        panic("tried to free a block cache that was not found");
        return;
    }


    free(bc->entries);
    free(bc);
    // remove the cache from the list

    memmove(
        block_caches + i, 
        block_caches + i + 1, 
        sizeof(block_page_cache_t) * (n_block_caches - i - 1)
    );

    n_block_caches--;


    _sti();
    spinlock_release(&block_caches_lock);
}



/*
// can have wait timings,
// but far less than read
// call sync when needing every
// operation to terminate
void (*async_read)(struct driver *,
                uint64_t lba,
                void*    buf,
                size_t   count);

void (*sync)(struct driver *);


void block_setup_cache(
        const struct storage_interface*  input, 
              struct storage_interface*  output,
        const struct block_cache_params* params
) {

    *output = (struct storage_interface) {
        .read     = block_cache_read,
        .write    = block_cache_write,
        .sync     = block_cache_sync,
        .remove   = block_cache_remove,
        .driver   = (struct driver*) input,
        .lbashift = input->lbashift,
        .capacity = input->capacity,
    };


}
*/





void block_cache_free(struct storage_interface* interface);
