#include "block_cache.h"
#include "../memory/heap.h"
#include "../memory/paging.h"
#include "../memory/physical_allocator.h"
#include "../sync/spinlock.h"
#include "../lib/panic.h"
#include "../lib/assert.h"
#include "../lib/string.h"
#include "../int/idt.h"

#include "../sched/sched.h"


/**
 * @brief to avoid using coarse grained locking
 * for cache accesses (which would be slow in case
 * of a lot of small reads / writes),
 * the fetched pages are recorded in an array:
 * cache->page_fetches. 
 * 
 * When a cache miss occurs, the page needs to be 
 * fetched. This is done without holding the cache lock.
 * The page is fetched in translated physical memory space.
 * It should then be maped to the block cache memory space.
 * 
 * When a cache miss occurs, the system checks if the page
 * is being fetched on the storage device. If so, the system
 * waits until the page is available. Else, the page is added
 * in the fetched pages list cache->page_fetches. 

**/

/** 
 * NVMe IO submission queue size
 * should be enough for all the requests
 */
#define MAX_PAGE_FETCHES 128


typedef struct page_fetch {
    uint64_t lba;   // page first lba
                    //
    void* paddr;    // the page physical address. It should
                    // be mapped to the block cache memory 
                    // space once fetched
                    //
    uint64_t pf_id; // page fetch identifier
} page_fetch_t;

/**
 * this structure should be used
 * in mutual exclusion
 */
typedef struct block_page_cache {
    struct driver* driver;

    void* vaddr_base;
    uint64_t virt_size;

          struct storage_interface* cache_interface;
    const struct storage_interface* target_interface;


    int   n_page_fetches;
    page_fetch_t page_fetches[MAX_PAGE_FETCHES];

    uint64_t n_used_page;


    struct {
        pid_t locked;    // if non -1, contains the pid that currently uses the structure
        uint8_t padding[128 - sizeof(pid_t)];
    } __attribute__((packed)) __attribute__((aligned(128)));
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



uint64_t block_cache_used_pages(void) {
    // read only, no need to lock
    // count the n_used_page of all the block caches
    
    // it can differ from the number of pages used by the block caches
    // but not that mutch. It avoid taking every lock

    uint64_t rf = get_rflags();

    _cli();

    spinlock_acquire(&block_caches_lock);

    uint64_t n_used_pages = 0;
    for (size_t i = 0; i < n_block_caches; i++) {
        n_used_pages += block_caches[i]->n_used_page;
    }

    spinlock_release(&block_caches_lock);

    set_rflags(rf);

    return n_used_pages;
}




///////////////////////////
//// PAGE FAULT HANDLER ///
///////////////////////////

void block_cache_page_fault(uint64_t pf_address) {
    panic("block cache page fault");
    (void) pf_address;
}


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


    for(size_t i = 0; i < n_block_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        
        uint64_t cache_begin = (uint64_t) cache->vaddr_base;
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


/**
 * @brief check if the given entry is present in cache.
 * It means that the page is ready and mapped.
 * 
 * @param cache 
 * @param vaddr 
 * @return int 
 */
static 
int check_present_page(void* vaddr) {
    return get_phys_addr(vaddr) != 0;
}


static void lock_cache(block_page_cache_t* restrict cache) {
    // lock the cache
    
    uint64_t rf = get_rflags();

    while(1) {
        _cli();
        if(cache->locked == -1) {
            // @todo atomic lock
            // maybe switch back to fast spinlock
            cache->locked = sched_current_pid();
            break;
        }
        
        set_rflags(rf);
        assert(cache->locked != sched_current_pid());
        
        sched_yield();
    }    

    set_rflags(rf);
}

static void unlock_cache(block_page_cache_t* restrict cache) {
    // unlock the cache
    cache->locked = -1;
}




static
void flush_cache(block_page_cache_t* cache) {
    // flush the cache

    // attempt to terminate the current operations
    lock_cache(cache);
    while(cache->n_page_fetches > 0) {
        unlock_cache(cache);
        sched_yield();
        lock_cache(cache);
    }

    assert(cache->n_page_fetches == 0);
    free_page_range((uint64_t)cache->vaddr_base, cache->virt_size);
    // WT: nothing to write back
    
    cache->n_used_page = 0;


    unlock_cache(cache);
}


void block_cache_reclaim_pages(unsigned pages) {
    // this function actually flushes all caches
    // so that no memory is used anymore
    // @todo this may be a bit too aggressive

    (void) pages;

    assert(interrupt_enable());

    _cli();
    spinlock_acquire(&block_caches_lock);

    volatile unsigned n_caches = n_block_caches;

    for(unsigned i = 0; i < n_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        lock_cache(cache);
    }
    spinlock_release(&block_caches_lock);
    _sti();


    for(unsigned i = 0; i < n_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        unlock_cache(cache);
        flush_cache(cache);
    }



}



/**
 * @brief returns true if the given page is being fetched
 * from the cache. This is used to avoid multiple fetches of the same page.
 * The idea is not to lock the cache while fetching a page.
 * 
 * the lock should be taken
 * 
 * @param cache 
 * @param lba 
 * @return int 
 */
static int is_fetching(block_page_cache_t* cache, uint64_t lba) {

    assert(cache->locked == sched_current_pid());

    unsigned sectors_per_page = 0x1000 >> cache->cache_interface->lbashift;
    
    // first lba in the page: page identifier
    uint64_t lba_begin = lba & ~(sectors_per_page - 1);


    for(int i = 0; i < cache->n_page_fetches; i++) {
        if(cache->page_fetches[i].lba == lba_begin) {
            return 1;
        }
    }
    return 0;
}



// the cache sould be locked
void fetch_page(block_page_cache_t* restrict cache,
                struct driver* driver,
                uint64_t lba
) {
    assert(cache->locked == sched_current_pid());



    unsigned lbashift = cache->cache_interface->lbashift;
    unsigned sectors_per_page = 0x1000 >> lbashift;
    
    assert(lbashift < 12);

    lba = lba & ~(sectors_per_page - 1);
    
    if(is_fetching(cache, lba)) {
        // we are already fetching this page
        return;
    }
    

    while(cache->n_page_fetches >= MAX_PAGE_FETCHES) {
        log_warn("block cache: too many page fetches");
        unlock_cache(cache);
        sched_yield();
        lock_cache(cache);
    }

    void* paddr = (void *) physalloc_single();


    cache->page_fetches[cache->n_page_fetches++] = (struct page_fetch) {
        .lba = lba,
        .paddr = paddr,
    };

    cache->target_interface->async_read(driver, lba, translate_address(paddr), sectors_per_page); 
}



static void target_sync(block_page_cache_t* restrict cache) {
    lock_cache(cache);

    if(cache->n_page_fetches > 0) {
        // get the id of the last page fetch in the list
        int last_page_fetch = cache->n_page_fetches - 1;
        uint64_t last_id = cache->page_fetches[last_page_fetch].pf_id;
        
        unlock_cache(cache);        
        
        // unlock the cache while synchronizing:
        // we don'map_pages want to block the cache while synchronizing

        cache->target_interface->sync(cache->driver);


        lock_cache(cache);

        if(cache->n_page_fetches > 0) {
            // remove the page fetches with id lower than last_id

            // find the first page fetch with id greater than last_id
            int i = 0;
            while(i < cache->n_page_fetches 
               && cache->page_fetches[i].pf_id <= last_id) {
                i++;
            }

            if(i > 0) {
                for(int j = 0; j < i; j++) {
                    unsigned lbashift = cache->cache_interface->lbashift;

                    // map the page fetched
                    void* paddr = cache->page_fetches[j].paddr;
                    void* vaddr = (void*)cache->vaddr_base 
                            + ((cache->page_fetches[j].lba << lbashift) & ~0xfff);

                    map_pages(
                        (uint64_t) paddr, 
                        (uint64_t) vaddr, 
                        1, 
                        PRESENT_ENTRY | PL_XD | PL_RW
                    );

                    cache->n_used_page++;

                }
                // remove the i first page fetches in the list 
                // (as we are sure that they are ready)
                memmove(
                    cache->page_fetches, 
                    cache->page_fetches + i, 
                    (cache->n_page_fetches - i) * sizeof(struct page_fetch)
                );
                
                cache->n_page_fetches = cache->n_page_fetches - i;
            }

        }

    }
    
    unlock_cache(cache);
}




static 
unsigned sectors_in_iteration(
            uint64_t lba, 
            size_t count, 
            unsigned sectors_per_page
) {
    unsigned page_offset = lba & (sectors_per_page - 1);

    if(count > sectors_per_page - page_offset) {
        return sectors_per_page - page_offset;
    }
    else {
        return count;
    }
}


static void prefetch(block_page_cache_t* restrict cache,
                 struct driver* driver,
                 uint64_t lba,
                 size_t   count
) {
    unsigned lbashift = cache->cache_interface->lbashift;

    unsigned sectors_per_page = 0x1000 >> lbashift;
    
    assert(lbashift < 12);

    void* base = (void*)cache->vaddr_base + (lba << lbashift);


    while(count > 0) {

        lock_cache(cache);

        unsigned present = check_present_page(base);

        if(!present)
            fetch_page(cache, driver, lba);

        unlock_cache(cache);

        unsigned it_rd = sectors_in_iteration(lba, count, sectors_per_page);


        lba   += it_rd;
        base  += it_rd << lbashift;
        count -= it_rd;
    }

    target_sync(cache);
}



/**
 * @brief try to read the cache. If the cache hits, the function returns
 * and the page is ready to be read. Else, the functions launches a page
 * fetch, and blocks until the page is fetched.
 * 
 * the cache should be locked
 * 
 */
static
void blocking_read_cache(block_page_cache_t* restrict cache,
                        struct driver* driver,
                        uint64_t lba,
                        void*    base
) {

    assert(cache->locked == sched_current_pid());

    int present_page = check_present_page(base);

    if(!present_page) {
        // page is not present in cache
        // launch a page fetch
        if(!is_fetching(cache, lba)) {
            fetch_page(cache, driver, lba);
        }
        // wait for the page to be fetched
        unlock_cache(cache);

        target_sync(cache);
        
        lock_cache(cache);
    }
}



/**
 * @brief read sectors from a cached block device.
 * behaves like storage_interface.read.
 * 
 */
static void read(block_page_cache_t* restrict cache,
                 struct driver* driver,
                 uint64_t lba,
                 void*    buf,
                 size_t   count
) {

    prefetch(cache, driver, lba, count);

    unsigned lbashift = cache->cache_interface->lbashift;

    unsigned sectors_per_page = 0x1000 >> lbashift;
    
    assert(lbashift < 12);

    void* base = (void*)cache->vaddr_base + (lba << lbashift);


    lock_cache(cache);



    // try to read one page at a time
    while(count > 0) {


        assert(lba < cache->cache_interface->capacity);
        assert(base >= cache->vaddr_base);
        assert(base < cache->vaddr_base + cache->virt_size);

        blocking_read_cache(cache, driver, lba,  base);

        unsigned it_rd = sectors_in_iteration(lba, count, sectors_per_page);


        memcpy(buf, base, it_rd << lbashift);

        count -= it_rd;
        lba   += it_rd;
        base  += it_rd << lbashift;
        buf   += it_rd << lbashift;
    }

    unlock_cache(cache);
}



static void write_cache(block_page_cache_t* restrict cache,
                  struct driver* driver,
                  uint64_t lba,
                  const void* buf,
                  size_t   count 
) {
    unsigned lbashift = cache->cache_interface->lbashift;
    
    unsigned sectors_per_page = 0x1000 >> lbashift;
    
    assert(lbashift < 12);
    
    void* base = (void*)cache->vaddr_base + (lba << lbashift);


    lock_cache(cache);


    
    // try to write one page at a time
    while(count > 0) {

        // sectors to write in this iteration
        unsigned it_wr = sectors_in_iteration(lba, count, sectors_per_page);

        assert(base >= cache->vaddr_base);
        assert(base < cache->vaddr_base + cache->virt_size);
        assert(lba < cache->cache_interface->capacity);


        if(it_wr < sectors_per_page)
            // need to fetch the entier page.
            blocking_read_cache(cache, driver, lba,  base);
        else {
            // juste allocate a blank page
            if(! check_present_page(base)) {
                alloc_pages(base, 1, PRESENT_ENTRY | PL_XD | PL_RW);
                cache->n_used_page++;
            }
        }
        
        memcpy(base, buf, it_wr << lbashift);
        
        count -= it_wr;
        lba   += it_wr;
        base  += it_wr << lbashift;
        buf   += it_wr << lbashift;
        
        unlock_cache(cache);
    }

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

    for(size_t i = 0; i < n_block_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        if(cache->driver == driver) {
            // found the block cache
            read(cache, driver, lba, buf, count);
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

    for(size_t i = 0; i < n_block_caches; i++) {
        block_page_cache_t* cache = block_caches[i];
        if(cache->driver == driver) {
            // found the block cache
            write_cache(cache, driver, lba, buf, count);

            // this is to make sure that two writes to the same page
            // are not executed in parallel. 
            // it is terrible for performance, so @todo find a better way
            cache->target_interface->sync(driver);

            // write through
            // @todo maybe merge this with write_cache
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
    assert(params != NULL);
    assert(input  != NULL);
    assert(output != NULL);

    // @tood maybe reduce vaddr granularity
    // 512 GB aligned size
    // this granularity is needed to easily free
    // recursivelly the pages
    assert(params->virt_size % (1llu << 39) == 0);
    assert(params->virt_size > 0llu);


    _cli();
    spinlock_acquire(&block_caches_lock);

    // create the cache
    block_page_cache_t* cache = malloc(sizeof(block_page_cache_t));

    
    cache->vaddr_base = vmap_device(params->virt_size);
    cache->virt_size = params->virt_size;
    cache->cache_interface = output;
    cache->target_interface = input;
    cache->driver = input->driver;
    cache->locked = -1; // initially locked by no process
    cache->n_used_page = 0;

    cache->n_page_fetches = 0;
    
    
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


    free(bc);
    // remove the cache from the list

    memmove(
        block_caches + i, 
        block_caches + i + 1, 
        sizeof(block_page_cache_t) * (n_block_caches - i - 1)
    );


    // realloc
    block_caches = realloc(
        block_caches, 
        sizeof(block_page_cache_t) * (n_block_caches - 1)
    );

    n_block_caches--;


    spinlock_release(&block_caches_lock);
    _sti();
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
