#pragma once

#include "storage_interface.h"
#include "../memory/vmap.h"


/**
 * cache for block devices.
 * 
 */


struct block_cache_params {
    // cannonical size:
    // higher bits are zero
    // lower bits are 1
    // in from 0x000000ffffff
    // or 0x0008ffffff
    // it should be 512 GB aligned
    uint64_t virt_size;
};

/**
 * @brief given an input storage interface, 
 * create an output storage interface that uses the input one,
 * through a paramertrized cache.
 * 
 * @param input 
 * @param output 
 * @param the parameters of the cache.
 */
void block_cache_setup(
        const struct storage_interface* input, 
        struct storage_interface* output,
        const struct block_cache_params* params);

void block_cache_free(struct storage_interface* cache_interface);


/**
 * @brief reclaim pages from the cache
 * 
 * @param pages number of pages to reclaim
 * 
 */
void block_cache_reclaim_pages(unsigned pages);



uint64_t block_cache_used_pages(void);


/**
 * @brief page fault handler:
 * it should be called when a pf occurs
 * in kernel space (for a write or a read)
 * in the block cache range.
 * 
 * @param pf_address 
 */
void block_cache_page_fault(uint64_t pf_address);


