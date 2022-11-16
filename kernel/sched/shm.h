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

    // list of processes that share this shm
    pid_t*   processes;
    unsigned n_processes;

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
struct shm_handler {
    struct shm* target;
    void* vaddr;
};

/**
 * @brief create and initialize an shm
 * object, given its initial size
 * 
 * @param initial_size byte size
 * @return struct shm* NULL if it couldn't 
 * be created
 */
struct shm* create_shm(size_t initial_size);

/**
 * @brief remove an shm object
 * @return 0 on success, non-0 on failure
 */
int remove_shm(struct shm* shm);

/**
 * open an shm object given its id.
 *  
 * On success, the current process is marked as 
 * having the given shm opened and returns a
 * shm handler structure
 * 
 * On failure, NULL is returned
 *
 */
struct shm_handler* shm_open(shmid_t id);


#endif