#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>


typedef atomic_bool lock_t;

#define DRIVER_STATE_UNINITIALIZED 0
#define DRIVER_STATE_OK 1
#define DRIVER_STATE_FAULT 2


typedef struct {
    void*  addr;
    size_t size;
} resource_t;

typedef struct {
    void(* install)(driver_t*);
    void(* remove) (void);

    const char* name;

    uint32_t status;

    lock_t lock;

    // things like pci BARs,
    // hpet registers space, ...
    resource_t* iores;

    // can be NULL
    struct pcie_device* device;
    
    const driver_t* parent;

    void* data;
    size_t data_len;

} driver_t;


void register_driver(driver_t* dr);

void remove_all_drivers(void);
