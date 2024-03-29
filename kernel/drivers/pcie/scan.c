#include "scan.h"
#include "pcie.h"

#include "../../lib/logging.h"
#include "../../lib/dump.h"
#include "../../lib/assert.h"
#include "../../memory/heap.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../lib/string.h"
#include "../../lib/sprintf.h"



struct PCIE_Descriptor pcie_descriptor = {0};


__attribute__((pure)) 
static int get_vendorID(unsigned bus_group,
                        unsigned bus, 
                        unsigned device, 
                        unsigned func
) {
    return pcie_config_space_base(
            bus_group, 
            bus,
            device,
            func
        )->vendorID;
}


static void map_bar(uint64_t paddr, 
                    void*    vaddr, 
                    uint32_t size
) {
    map_pages(
        paddr & ~0xfff, // align paddr and vaddr
        (uint64_t)vaddr & ~0xfff, // on a page 
        (size 
        + ((uint64_t)vaddr & 0xfff) // take account
        + 0xfff) / 0x1000,          // of missalignment
        PRESENT_ENTRY | PCD | PL_RW
    );
}

// return 1 if the bar is 64 bit,
// 0 if it is 32
// fill the dev->bars[index] structure
static int scan_bar(
    struct pcie_dev* dev,
    struct PCIE_config_space* cs, 
    unsigned index
) {
    // var to return
    int is64 = 0;

    uint32_t bar_reg = cs->bar[index];
    uint64_t paddr    = bar_reg & ~0xf;

    unsigned type = (bar_reg>>1) & 3;

// check if the bar is 32 or 64 bit
    switch(type) {
        case 0: // 32bit physical address
            break;
        case 2: // 64bit physical address
            assert(index != 5);
            paddr |= (uint64_t)cs->bar[index+1] << 32;
            is64 = 1;
            break;
        default: // illegal value for PCI 3.0
            log_warn("%s: illegal bar%u register",
                        dev->dev.name.ptr, index);
    }

    unsigned io = bar_reg&1;

    uint32_t size = 0;

    void* vaddr = (void*)paddr;

// check if the bar is used
    if(paddr != 0 && !io) {// NULL: unused
        // the bar is used
        vaddr = translate_address((void *)paddr);
        size  = pcie_bar_size(cs, index);

        // now make it accessible
        map_bar(paddr, vaddr, size);
    }

    dev->bars[index] = (bar_t){
        .base         = vaddr,
        .size         = size,
        .io           = io,
        .type         = type,
        .prefetchable = (bar_reg >> 3) & 1,
    };

    return is64;
}


static void scan_bars(struct pcie_dev* dev) {
    struct PCIE_config_space* cs = dev->config_space;
    
    // BARs scan
    for(unsigned i = 0; i < 6;) {
        // 64 bit bars take 2 bar registers
        if(scan_bar(dev, cs, i))
            i += 2;
        else
            i += 1;
    }
}


static void new_dev(
        void (*callback)(struct pcie_dev*),
        unsigned bus_group, 
        unsigned bus, 
        unsigned device, 
        unsigned func
) { 
    
    struct PCIE_config_space* cs = 
               pcie_config_space_base(bus_group,
                                     bus,
                                     device,
                                     func);

    if((cs->header_type & 0x7f) != 0 ||
        cs->classcode == 06)
        return;
    // the device is a bridge,
    // we won't do anything with it anyway

    // give a unique name to each device
    static unsigned id = 0;

    

// we hate mallocs.
// we definitly want to do as few mallocs
// as we possible.
// therefore we will trickely make one malloc 
// call to allocate both the struct and the name
// string
// the device manager will invoke a free call
// with the device's address, so freeing
// the dev should free the name.
    struct alloc {
        struct pcie_dev dev;
        char name[16];
        // 16 should always be enough
    };
    struct alloc* alloc = malloc(sizeof(struct alloc));

    struct pcie_dev* dev      = &alloc->dev;
    char*            name_buf = alloc->name;

    sprintf(name_buf, "pcie%u", id++);


    
    // fill the struct 
    *dev = (struct pcie_dev) {
        .dev = {
            .type     = DEVICE_ID_PCIE,
            .name     = {name_buf, 0},
            .driver   = NULL,
        },
        
        .config_space = cs,

        .info = (struct dev_info) {
            .vendorID     = cs->vendorID,
            .deviceID     = cs->deviceID,
            .classcode    = cs->classcode,
            .subclasscode = cs->subclasscode,
            .progIF       = cs->progIF,
            .revID        = cs->revID,
        },

        .path = (pcie_path_t) {
            .domain = bus_group,
            .bus    = bus,  
            .device = device,
            .func   = func,
        }, 
        .msix_table=NULL,
        .msix_table_size=0,
    };

    // now scan bars
    scan_bars(dev);

    // our struct inherits from this one
    // don't worry

    callback(dev);
}






// scan a single device
// adds its functions in the linked list
// if it finds some
static void scan_device(void (*callback)(struct pcie_dev*),
                        unsigned bus_group, 
                        unsigned bus, 
                        unsigned device)
{

    // if the first function doesn't exist, so 
    // does the device
    uint16_t vendorID = get_vendorID(bus_group, 
                                     bus,
                                     device, 
                                     0);
    // no device!
    if(vendorID == 0xFFFF)
        return;
    // a device is there!
    new_dev(callback,
            bus_group,
            bus, 
            device, 
            0
    );
    
    
    // check for additionnal functions for this
    // device
    for(unsigned func = 1; func < 8; func++) {

        if(get_vendorID(bus_group,
                        bus,
                        device, 
                        func) != vendorID)
            continue;
        
        new_dev(callback,
                bus_group,
                bus, 
                device, 
                func
        );
    }
}

/**
 * constructs the found_devices array
 */
static void scan_devices(void (*callback)(struct pcie_dev*)) {

    // first create a linked list
    // as we dont know how many devices are present


// count the number of devices

    for(unsigned bus_group_i = 0; 
                 bus_group_i < pcie_descriptor.size; 
                 bus_group_i++) 
    {    
        struct PCIE_busgroup* group = 
                    &pcie_descriptor.array[bus_group_i];

        unsigned bus_group = group->group;
        
        for(unsigned bus = group->start_bus; 
                     bus < group->end_bus; 
                     bus++) 
        {
            for(unsigned device = 0; device < 32; device++) {
                
                scan_device(callback, bus_group,bus,device);
            }
        }
    }

}


// map every page that might be a config space 
// to PHYSICAL
static void map_possible_config_spaces(void) {
    for(unsigned i = 0; i < pcie_descriptor.size; i++) {
        void* phys = pcie_descriptor.array[i].address;

        // the corresponding pages
        // @todo find a better solution,
        // as it burns > 512 ko of paging
        // structures
        map_pages(
            (uint64_t)phys, // phys
            (uint64_t)translate_address(phys), // virt
            256 * // busses
            32  * // devices
            8,    // functions
            PRESENT_ENTRY | PCD | PL_XD | PL_RW
            // no cache, execute disable, writable
        );
    }
}


/**
 * during the init,
 * we identity map the pcie configuration spaces
 * 
 */
void pcie_scan(void (*callback)(struct pcie_dev*)) {
    map_possible_config_spaces();

    scan_devices(callback);
}
