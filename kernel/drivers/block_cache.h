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
    uint64_t virt_size;
};

/**
 * @brief given an input storage interface, 
 * create an output storage interface that uses the input one,
 * through a paramertrized cache.
 * 
 * @param input 
 * @param output 
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


