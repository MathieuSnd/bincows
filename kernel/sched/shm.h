#ifndef SHM_H
#define SHM_H

#include <stdint.h>
#include "process.h"


/**
 * every SHM has a size between 1 byte and
 * 1 GB. It is 1 GB-aligned.
 * 
 * One process can use at most 512 SHMs.
 * 
 * the virtual memory at which is placed 
 * the SHM is within the range 
 * 
 * A per process allocator will assign
 * 
 */


// = 1 GB
#define SHM_SIZE_MAX (1024 * 1024 * 1024)

typedef int shmid_t;

/**
 * global shm structure:
 * one of this entry represents a
 * shared memory object
 */
struct shm {
    shmid_t id;

    // number of SHM instances
    int n_insts;

    // physical address of the 1 GB
    // page directory (level 3 page structure)
    uint64_t pd_paddr;

    // byte size of the region
    uint32_t size;
};


/**
 * process local shared memory structure
 * one of this entry represents a shared
 * memory view from a process
 * 
 */
struct shm_instance {
    shmid_t target;
};

/**
 * @brief create and initialize an shm
 * object, given its initial size
 * 
 * @param initial_size byte size
 * @return struct_instance* NULL if it couldn't 
 * be created
 */
struct shm_instance* shm_create(size_t initial_size);


// base: 1 GB aligned virtual base address of a
// currently mapped memory range. The level 2 page
// directory (that controls 1 GB of memory) is
// extracted so users can access the range.
//
// this function is intended to allow the kernel to 
// share MMIO or higher half memory.
// The kernel should use this function with care
// so that only the desired memory is shared: 
// The whole 1 GB range with given base is shared.
// The initial_size field is just a hint
//
struct shm_instance* shm_create_from(size_t size, void* base);



/**
 * 
 * resize the shm size
 * 
 * @param shm shm to truncate
 * @return 0 on success, non 0 on failure
 */
int shm_truncate(shmid_t shm, size_t new_size);



/**
 * open an shm object given its id.
 *  
 * On success, the current process is marked as 
 * having the given shm opened and returns a
 * shm handler structure
 * 
 * On failure, NULL is returned
 * 0xffff80000bfa5000
 * 0xffff80000bf98000
 *
 */
struct shm_instance* shm_open(shmid_t id);

/**
 * 
 * close and unmap the SHM instance
 * 
 */
int shm_close(const struct shm_instance* instance);


void shm_cleanup(void);

#endif