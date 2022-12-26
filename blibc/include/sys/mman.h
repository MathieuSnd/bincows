#ifndef MMAN_H
#define MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "stat.h"
#include "types.h"


int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);


// only supported for shms
void *mmap(void *addr, size_t length, int prot, int flags,
            int fd, off_t offset);

// never supported
int munmap(void *addr, size_t length);



#ifdef __cplusplus
}
#endif

#endif//MMAN_H