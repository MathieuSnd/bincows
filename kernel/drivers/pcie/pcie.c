#include "pcie.h"
#include "scan.h"

#include "../nvme.h"

#include "../../lib/logging.h"
#include "../../lib/assert.h"
#include "../../memory/paging.h"
#include "../../lib/string.h"


static struct pcie_dev* devices = NULL;
static unsigned n_devices = 0;

// array of drivers
// should be the same size as the 
// device array. 
// device[i] corresponds to drivers[i]
// if drivers[i] = NULL, device[i] doesn't 
// have an installed driver 
static struct driver** drivers = NULL;

// at shutdown
static void pcie_shutdown(void);


////__attribute__((pure))
static inline struct resource get_bar_resource(
            struct PCIE_config_space* cs, 
            unsigned i) 
{
    volatile uint64_t* bar_reg = (uint64_t*)&cs->bar[i];
    uint64_t val = *bar_reg;

    *bar_reg = ~0llu;
    
    unsigned size = (unsigned) ~*bar_reg + 1;

    *bar_reg = val;


    return (struct resource) {
        .addr = (void *)val,
        .size = size,
    };
}


// alloc the structure and fill with
// basic info
static struct driver* create_driver_data(
                        struct device* dev) 
{
    struct driver* dr = malloc(sizeof(struct driver));

    dr->dev = dev;
    dr->status = DRIVER_STATE_UNINITIALIZED;

    return dr;
}


// recognize compatible drivers
// and set them up
static void setup_drivers(void) {
    drivers = malloc(n_devices * sizeof(void *));

    for(int i = 0; i < n_devices; i++) {
        struct pcie_dev* dev = &devices[i];
        // only one driver is supported right now....

        if(dev->info.classcode == 1) {
            // mass storage drive
            if(dev->info.subclasscode == 8) {
                // yey! we found an nvme device!
                struct driver* dr = create_driver_data(dev);
                drivers[i] = dr;
                
                if(vme_install(drivers[i])) {
                    dev->driver = dr;

                    log_info("installed driver %u", 
                             dr->driver_name);
                }
                else {
                    dr->status = DRIVER_STATE_FAULT;
                    panic("couldn't install NVMe driver!!");
                }

            }
        }
    }
}


void pcie_init(void) {
    log_debug("init pcie...");
    devices = pcie_scan(&n_devices);
    
    atshutdown(pcie_shutdown);
}

static void free_device(struct pcie_dev* dev) {
    assert(dev);
    assert(dev->name);

    free(dev->name);
}

static void pcie_shutdown(void) {
    for(int i = 0; i < n_devices; i++) {
        free_device(devices+i);
    }
    n_devices = 0;

    free(devices);
}
