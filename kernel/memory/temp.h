#ifndef TEMP_H
#define TEMP_H

/**
 * this file declares functions
 * to use the KERNEL TEMP mechanism.
 * 
 * the KERNEL TEMP memory region is used
 * for temporary mapping. Examples are
 * - mapping the new process when forking
 * - mapping a SHM (to allocate it, memset it, free it)
 *
 * The rules to use this region are quite strict:
 * - one CPU core can only access the 512 GB with
 *   base address:
 *   KERNEL_TEMP_BEGIN | (LAPIC_ID * 512 GB)
 * - every operation (from mapping to unmapping)
 *   must be done with the interrupts disabled.
 *
 * This way, no collision can happen, and no need
 * any inter-core locking.
 * This however restricts the number of cores in the
 * system to KERNEL_TEMP_SIZE / 512 GB
 * Which is currently 32.
 * In order to increase this limit, an allocation
 * system involving spinlocking could be used,
 * or the KERNEL TEMP range size could be increased.
 */

#define TEMP_SIZE (512*1024*1024*1024)

#include <stdint.h>

/**
 * lock a temp virtual memory range.
 * interrupts must be disabled before
 * calling temp_lock and should remain
 * disabled until temp_release is called.
 * 
 * the range base address is returned.
 * the byte size of the range is TEMP_SIZE
 */
void* temp_lock(void);

/**
 * return the physical address of the page 
 * directory of the current temp owned by 
 * this CPU core.
 * This function should only be called between
 * a temp_lock and a temp_release call.
 */
uint64_t temp_pd(void);

/**
 * release the virtual memory range locked
 * by the current CPU core
 * no memory should be mapped within this
 * range
 */
void temp_release(void);



#endif TEMP_H