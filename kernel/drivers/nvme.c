#include "nvme.h"
#include "pcie/pcie.h"
#include "driver.h"

#include "../lib/logging.h"

static void remove(driver_t* this);

struct data {
    int foo;
};


struct regs {
             uint64_t cap;
             uint32_t version;
             uint32_t intmask_set;
             uint32_t intmask_clear;
    volatile uint32_t config;
    volatile uint32_t subsystem_reset;
    volatile uint32_t status;
             uint32_t reserved1;
    volatile uint32_t queue_attr;
    volatile uint64_t submission_queue;
    volatile uint64_t completion_queue;
    volatile uint64_t sq_tail;
    volatile uint64_t cq_head;
};


static void rename_device(struct pcie_dev* dev) {
    string_free(&dev->dev.name);
    char* buff = malloc(32);


    struct regs* regs = dev->bars[0].base;
    uint32_t version = regs->version;

    sprintf(buff, 
            "NVMe %u.%u controller", 
            version >> 16,
            (version >> 8) & 0xff);

    dev->dev.name = (string_t){buff, 1};
}


static 
void reset(struct regs* regs) {
    //regs->subsystem_reset = 0x4E564D65;
    //regs->subsystem_reset = 0;
}

static 
int is_shutdown(struct regs* regs) {
    return (regs->status >> 2) & 3 == 0x10;
}


static
void print_status(driver_t* this) {
    struct pcie_dev* dev = 
            (struct pcie_dev*)this->device;
        
    struct regs* regs = dev->bars[0].base;

    log_debug("CCS:        %x\n"
              "version:    %x\n"
              "mask set:   %x\n"
              "mask clear: %x\n"
              "config:     %x\n"
              "status:     %x\n",
              (regs->cap>>37)&0xff,
              regs->version,
              regs->intmask_set,
              regs->intmask_clear,
              regs->config,
              regs->status
              );
}


int nvme_install(driver_t* this) {
    if(this->device->type != DEVICE_ID_PCIE)
        return 0;

    struct pcie_dev* dev = (struct pcie_dev*)this->device;
    struct regs* bar0 = (struct regs*)dev->bars[0].base;

    assert(bar0 != 0);

    // capabilities register
    uint64_t cap = bar0->cap;

    // check that the NVM command set
    // is supported
    if((cap & (1llu << 37)) == 0)
        return 0;

    // check that the I/O command set
    // is supported
    if((cap & (1llu << 43)) == 0)
        return 0;

    // check that the 4k pages are 
    // supported: MPSMIN = 0
    // min page size is 2^(12 + MPSMIN)
    if((cap & (0xfllu << 48)) != 0)
        return 0;

    // check if the device is resetable
    if(cap & (1llu << 36) == 0) {
        log_warn("NSSRS is not supported on %s.", dev->dev.name.ptr);
        return 0;        
    }

    print_status(this);


    reset(bar0);

    bar0->config = 
            (0 << 24) // Controller Ready Independent of 
                      // Media Enable: I have no idea what
                      // it is.
          | (4 << 20) // I/O completion queue entry size
                      // should be 16, so 2**4
          | (6 << 16) // I/O submition queue entry size
                      // should be 64, so 2**6*
          | (0   << 14) // shutdown notification
          | (0 << 11) // Arbitration Mechanism Selected 
                      // 0: round robin
                      // 1: Weighted Round Robin with 
                      //    Urgent Priority Class
          | (0 << 7)  // Map page size: (2 ^ (12 + MPS)) 
                      // 4K paging: MPS = 0
    ;

    rename_device(dev);

// init struct
    this->remove = remove;
    this->name = (string_t) {"Bincows NVMe driver", 0};
    this->data = malloc(sizeof(struct data));
    this->data_len = sizeof(struct data);
    


    print_status(this);




    struct PCIE_config_space* cs = dev->config_space;


    //pcie_map_bars(dev);

    volatile uint64_t* bar_reg = (uint64_t*)&cs->bar[0];
    assert(init_msix(dev));

    set_msix(dev, 0, 0, 50, 0, 0, 0);
    set_msix(dev, 1, 0, 51, 0, 0, 0);
    set_msix(dev, 2, 0, 52, 0, 0, 0);
    set_msix(dev, 3, 0, 53, 0, 0, 0);

    this->status = DRIVER_STATE_OK; 
// installed
    return 1;
}


static void remove(driver_t* this) {

    free(this->data);
}

static void shutdown(struct driver* this) {
    assert(this);

    this->status = DRIVER_STATE_SHUTDOWN;

    struct pcie_dev* dev = (struct pcie_dev*)this->device;
    struct regs* bar0    = (struct regs*)dev->bars[0].base;

    bar0->config = (bar0->config & ~0xc000) | (0b10 << 14);
}

