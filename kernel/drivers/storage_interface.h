#pragma once


#include <stdint.h>
#include <stddef.h>

struct driver;

struct storage_interface {
    struct driver* driver;

    void (*read)(struct driver *,
                 uint64_t lba,
                 void*    buf,
                 size_t   count);

    void (*write)(struct driver *,
                 uint64_t    lba,
                 const void* buf,
                 size_t      count);

    // can have wait timings,
    // but far less than read
    // call sync when needing every
    // operation to terminate
    void (*async_read)(struct driver *,
                 uint64_t lba,
                 void*    buf,
                 size_t   count);

    void (*sync)(struct driver *);
    
    unsigned capacity;
    
    unsigned lbashift;
};
