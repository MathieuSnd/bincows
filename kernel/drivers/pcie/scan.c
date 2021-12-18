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



//__attribute__((pure))
static struct PCIE_config_space*  
                get_config_space_base(unsigned bus_group,
                                      unsigned bus, 
                                      unsigned device, 
                                      unsigned func
                                      ) 
{
    assert(bus_group < pcie_descriptor.size);
    assert(bus_group == 0);
    struct PCIE_busgroup* group_desc = &pcie_descriptor.array[bus_group];
    
    
    return translate_address(group_desc->address) + (
        (bus - group_desc->start_bus) << 20 
      | device                        << 15 
      | func                          << 12);    
}


static int __attribute__((pure)) 
                get_vendorID(unsigned bus_group,
                             unsigned bus, 
                             unsigned device, 
                             unsigned func) 
{
    struct PCIE_config_space* config_space = 
                get_config_space_base(bus_group, bus,device,func);
    return config_space->vendorID;
}


static void new_dev(
        void (*callback)(struct pcie_dev*),
        unsigned bus_group, 
        unsigned bus, 
        unsigned device, 
        unsigned func
) { 
    
    struct PCIE_config_space* cs = 
               get_config_space_base(bus_group,
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
    };

    // BARs scan
    for(unsigned i = 0; i < 6;) {
        // next iteration index
        unsigned next_i = i+1;

        uint32_t bar_reg = cs->bar[i];

        uint64_t addr = bar_reg & ~0xf;
        unsigned type = (bar_reg>>1) & 3;


        switch(type) {
            case 0: // 32bit address
                break;
            case 2: // 64bit address
                addr |= (uint64_t)cs->bar[i+1] << 32;
                next_i++; // bar[i+1] is skiped
                break;
            default: // illegal value for PCI 3.0
                log_warn("%s: illega bar%u register",
                         dev->dev.name.ptr, i);
        }
        
        log_debug("BAR%u: base=%5lx, type%x",i, addr, type);
        if(addr != 0) {// NULL: unused
            addr = translate_address(addr);
        }

        dev->bars[i] = (bar_t){
            .base         = addr,
            .io           = bar_reg&1,
            .type         = type,
            .prefetchable = (bar_reg >> 3) & 1,
        };

        i = next_i;
    }

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
        map_pages(
            (uint64_t)phys, // phys
            (uint64_t)translate_address(phys), // virt
            256 * // busses
            32  * // devices
            8,    // functions
            PRESENT_ENTRY | PCD | PL_XD
            // no cache, execute disable
        );
    }
}


static void identity_unmap_possible_config_spaces(void) {
    for(unsigned i = 0; i < pcie_descriptor.size; i++) {
        void* phys = pcie_descriptor.array[i].address;

        unmap_pages(
            (uint64_t)translate_address(phys), // vaddr
            256 * 32 * 8
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
