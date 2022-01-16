#pragma once

#include <stdint.h>
#include <stddef.h>

struct driver;

int nvme_install(struct driver*);


// the block size is 1 << lbashift
int nvme_get_lbashift(struct driver*);


/**
 * @brief read contiguous memory region from 
 * NVME drive. On error, a kernel panic occurs.
 * 
 * @param this  the driver structure
 * @param lba   the lba address
 * @param buf   the destination.
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
 * @param buf   the source buffer base virtual
 *              address.
 * @param count the number of blocks to read
 */
void nvme_sync_write(struct driver* this,
                     uint64_t lba,
                     void*    buf,
                     size_t   count);

 
#define NVME_INVALID_IO_QUEUE (~0u)
