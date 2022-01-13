#pragma once

#include <stdint.h>
#include <stddef.h>


struct driver;

int nvme_install(struct driver*);


/**
 * @brief read contiguous memory region from 
 * NVME drive. On error, a kernel panic occurs.
 * 
 * @param this  the driver structure
 * @param lba   the lba address
 * @param buf   the destination. It better be 
 *              block size-aligned
 * @param count the number of blocks to read
 */

void nvme_sync_read(struct driver* this,
                    uint64_t lba,
                    void*    buf,
                    size_t   count);



/**
 * @brief write contiguous memory region to 
 * NVME drive. On error, a kernel panic occurs.
 * 
 * @param this  the driver structure
 * @param lba   the lba address
 * @param buf   the destination. It better be 
 *              block size-aligned
 * @param count the number of blocks to read
 */
void nvme_sync_write(struct driver* this,
                     uint64_t lba,
                     void*    buf,
                     size_t   count);
 
#define NVME_INVALID_IO_QUEUE (~0u)
