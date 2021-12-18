#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../lib/assert.h"
#include "../driver.h"
#include "../dev.h"

// pcie devices structures
// should have this type 
#define DEVICE_ID_PCIE (0x2c1e)

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

typedef struct {
    uint64_t base;
    uint32_t size;
    unsigned io: 1;
    unsigned type: 2;
    unsigned prefetchable: 1;
} bar_t;

struct pcie_dev {
    struct dev dev;
    
    void* config_space; // vaddr
    
    // if 64 bit bar:
    // bars[2*i] contains the whole
    // address along with flags
    // and bars[2*i+1] is unused

    bar_t bars[6];

    pcie_path_t path;
    struct dev_info info;

// 8-aligned 
// NULL if msix is not supported
// or not initialized
    void*    msix_table;
    unsigned msix_table_size;
};

static_assert_equals(sizeof(pcie_path_t), 8);
void pcie_init(void);


// return 0 if MSIs cannot be enabled
// 1 instead
int enable_msi(struct pcie_dev* dev, 
               unsigned vector, 
               uint32_t processor,
               uint32_t edge_trigger,
               uint8_t  deassert);


// enable msix and masks all interrups
// return 0 if MSIXs cannot be enabled
// 1 instead
int init_msix(struct pcie_dev* dev);

// index:        the index of MSI-X entry 
// mask:         0: unmasked, 1: masked
// vector:       idt entry number
// processor:    target processor lapic id
// edge_trigger: 0: edge, 1: level
// deassert:     0: deassert, 1: assert
void set_msix(struct pcie_dev* dev, 
              unsigned index,
              unsigned mask,
              unsigned vector, 
              uint32_t processor,
              uint32_t edge_trigger,
              uint8_t  deassert);


//__attribute__((pure))
unsigned pcie_bar_size(void* config_space, unsigned i);


struct PCIE_config_space {
    volatile uint16_t vendorID;
    volatile uint16_t deviceID;
    volatile uint16_t command;
    volatile uint16_t status;
    volatile uint8_t  revID;
    volatile uint8_t  progIF;
    volatile uint8_t  subclasscode;
    volatile uint8_t  classcode;
             uint16_t reserved3;
    volatile uint8_t  header_type;
    volatile uint8_t  BIST;
    volatile uint32_t bar[6];
    volatile uint32_t cardbud_cis_ptr;
    volatile uint16_t subsystem_vendorID;
    volatile uint16_t subsystemID;
    volatile uint32_t expansion_base;
    volatile uint8_t  capabilities;
             uint8_t  reserved0[3];
             uint32_t reserved1;
    volatile uint8_t  interrupt_line;
    volatile uint8_t  interrupt_pin;
             uint8_t reserved5[2];
} __packed;

static_assert_equals(sizeof(struct PCIE_config_space), 0x40);

