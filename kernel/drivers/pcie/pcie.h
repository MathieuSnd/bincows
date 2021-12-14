#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../lib/assert.h"

void pcie_init(void);


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
    struct resource* resources;
    struct dev_info info;
    const char* name;
    
    void* config_space;

    pcie_path_t path;

    struct driver* driver;

};

struct pcie_driver {
    const struct pcie_device* dev;
};

struct PCIE_config_space {
    volatile uint16_t vendorID;
    volatile uint16_t deviceID;
             uint16_t unused0;
             uint16_t unused1;
    volatile uint8_t  revID;
    volatile uint8_t  progIF;
    volatile uint8_t  subclasscode;
    volatile uint8_t  classcode;
             uint16_t reserved3;
    volatile uint16_t header_type;
    volatile uint8_t  BIST;
    volatile uint32_t bar[6];
    volatile uint32_t cardbud_cis_ptr;
    volatile uint16_t subsystemID;
    volatile uint16_t subsystem_vendorID;
    volatile uint32_t expansion_base;
    volatile uint8_t  capabilities;
             uint8_t  reserved0[3];
             uint32_t reserved1;
    volatile uint8_t  interrupt_line;
    volatile uint8_t  interrupt_pin;
             uint16_t reserved5[2];
} __packed;



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

