#include "nvme.h"
#include "pcie/pcie.h"
#include "driver.h"

#include "../lib/logging.h"

static void remove(driver_t* this);

struct data {
    int foo;
};


int nvme_install(driver_t* this) {
    if(this->device->type != DEVICE_ID_PCIE)
        return 0;

    this->remove = remove;
    this->name = (string_t) {"NVMe driver", 0};
    this->data = malloc(sizeof(struct data));
    this->data_len = sizeof(struct data);
    

    struct pcie_dev* dev = (struct pcie_dev*)this->device;

    log_warn("bar0 base=%lx,type=%x,prefetchable=%x", 
            dev->bars[0].base, 
            dev->bars[0].type, 
            dev->bars[0].prefetchable
    );

    struct PCIE_config_space* cs = dev->config_space;


    //pcie_map_bars(dev);

    volatile uint64_t* bar_reg = (uint64_t*)&cs->bar[0];
    assert(init_msix(dev));

    set_msix(dev, 0, 0, 50, 0, 0, 0);
    set_msix(dev, 1, 0, 51, 0, 0, 0);
    set_msix(dev, 2, 0, 52, 0, 0, 0);
    set_msix(dev, 3, 0, 53, 0, 0, 0);


// installed
    return 1;
}


static void remove(driver_t* this) {

    free(this->data);
}


