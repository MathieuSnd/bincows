#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include "../lib/string_t.h"

typedef atomic_bool lock_t;

#define DRIVER_STATE_UNINITIALIZED 0
#define DRIVER_STATE_OK 1
#define DRIVER_STATE_FAULT 2
#define DRIVER_STATE_SHUTDOWN 3


typedef struct {
    void*    addr;
    unsigned size;
    unsigned type;
} resource_t;


typedef struct driver {

    // returns 0 if everything is fine
    int (*install)(struct driver*);

    void (*remove)(struct driver*);

    string_t name;

    uint32_t status;

    lock_t lock;

    struct dev* device;
    
    const struct driver* parent;

    void* __restrict__ data;
    size_t data_len;

} driver_t;



driver_t* driver_register_and_install(
            int (*install)(struct driver*), 
            struct dev* dev
);

/**
 * drivers must be removed BEFORE
 * devices
 */
void remove_all_drivers(void);
