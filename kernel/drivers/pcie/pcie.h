#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../lib/assert.h"

void pcie_init(void);
void pcie_init_devices(void);



typedef void (*driver_init_fun)(void* config_space); 
typedef void (*driver_callback)(void); 

struct resource {
    void*  addr;
    size_t size;
};

typedef union {
    struct {
        unsigned domain: 32;
        
        unsigned unused: 8;

        unsigned bus:    8;
        unsigned device: 8;
        unsigned func:   8;
    };
    uint64_t value;

} pcie_path_t;

struct dev_info {
    uint16_t vendorID;
    uint16_t deviceID;

    uint8_t  classcode;
    uint8_t  subclasscode;
    uint8_t  progIF;
    uint8_t  revID;
};

static_assert_equals(sizeof(pcie_path_t), 8);


struct pcie_dev {
    struct resource bars[3];
    struct dev_info info;
    
    void* config_space;

    pcie_path_t path;

    struct driver* driver;

};

struct driver {
    driver_init_fun install;
    driver_callback remove;
    
    const char* driver_name;
    uint32_t status;


    struct resource data;
    const struct pcie_device* dev;
};


/**
 * PCIE drivers interfaces:
 * 
 * provide void init(void* config_space_base)
 * 
 * functions to call:
 *      register_irq(unsigned)
 *      unregister_irq(unsigned)
 * 
 *      int  register_timer(void callback(void), unsigned period)
 *      void unregister_timer(int timer_id)
 *     
 */

