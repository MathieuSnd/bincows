#ifndef MEMFS_H
#define MEMFS_H

#include "../fs.h"
#include "../../sched/shm.h"

/**
 * memfs: 
 * files in this fs represent
 * shared memory objects. 
 * reading from a file always return
 * the mem_desc structure.
 * 
 * a process cannot open the same file
 * twice at the same time.
 * 
 * 
 */

struct mem_desc {
    // begin process virtual address of 
    // the memory range
    void* vaddr;
};



fs_t* memfs_mount(void);

void memfs_register_file(char* name, shmid_t id);

#endif// MEMFS_H