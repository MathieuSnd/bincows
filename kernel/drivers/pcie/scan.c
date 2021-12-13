#include "scan.h"
#include "pcie.h"
#include "../../lib/logging.h"
#include "../../lib/dump.h"
#include "../../lib/assert.h"
#include "../../memory/heap.h"
#include "../../memory/paging.h"
#include "../../lib/string.h"


struct PCIE_config_space {
    volatile uint16_t vendorID;
    volatile uint16_t deviceID;
             uint16_t unused0;
             uint16_t unused1;
    volatile uint8_t  revID;
    volatile uint8_t  progIF;
    volatile uint8_t  subclasscode;
    volatile uint8_t  classcode;
    volatile uint32_t infos;
    volatile uint64_t bar[3];
    volatile uint32_t cardbud_cis_ptr;
    volatile uint16_t subsystemID;
    volatile uint16_t subsystem_vendorID;
    volatile uint32_t expansion_base;
    volatile uint8_t  capabilities;
             uint8_t  reserved0[3];
             uint32_t reserved1;
    volatile uint8_t  interrupt_line;
    volatile uint8_t  interrupt_pin;
             uint16_t reserved2[2];
};



struct PCIE_Descriptor pcie_descriptor = {0};


// array of all installed devices' functions
static struct pcie_dev* found_devices = NULL;
static size_t n_found_devices = 0;

// we construct a linked list first
// so we need a node struct
struct early_device_desc {
    uint16_t bus, device, function, domain;
    struct PCIE_config_space* config_space;

    struct early_device_desc* next;
};
// used to iterate over devices when scanning
struct early_device_desc head_node = {.next = NULL};
struct early_device_desc* current = &head_node; 


////__attribute__((pure))
static struct resource get_bar_resource(
            struct PCIE_config_space* cs, 
            unsigned i) 
{
    volatile uint64_t* bar_reg = (uint64_t*)&cs->bar[i];
    uint64_t val = *bar_reg;

    *bar_reg = ~0llu;
    
    unsigned size = ~ *bar_reg + 1;

    *bar_reg = val;


    return (struct resource) {
        .addr = (void *)val,
        .size = size,
    };
}


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
    return 0;
    /*
    return group_desc->address + (
        (bus - group_desc->start_bus) << 20 
      | device                        << 15 
      | func                          << 12);
      */
}


static int __attribute__((pure)) 
                get_vendorID(unsigned bus_group,
                             unsigned bus, 
                             unsigned device, 
                             unsigned func) 
{
//    struct PCIE_config_space* config_space = 
//                get_config_space_base(bus_group, bus,device,func);
    return 8086;
    //return config_space->vendorID;
}


static void insert(
        unsigned bus_group, 
        unsigned bus, 
        unsigned device, 
        unsigned func)
{ 
    /*
    struct PCIE_config_space* config_space = 
               get_config_space_base(bus_group,
                                     bus,
                                     device,
                                     func)
    /*
        ->next = malloc(sizeof(struct early_device_desc));

    current = current->next;
    current->next           = NULL;
    // fill the new node
    current->domain       = bus_group;
    current->bus          = bus;
    current->device       = device;
    current->function     = func;
    current->config_space = config_space;
*/
    asm  volatile("nop");
    //printf("%2x:%2x:%2x.%2x - %lx", bus_group, bus, device, func);
    //n_found_devices++;
}


// scan a single device
// adds its functions in the linked list
// if it finds some
static void scan_device(unsigned bus_group, 
                        unsigned bus, 
                        unsigned device)
{

    // if the first function doesn't exist, so 
    // does the device
    uint16_t vendorID = get_vendorID(bus_group, 
                                     bus,
                                     device, 
                                     0);
    //return;

    // no device!
    if(vendorID == 0xFFFF)
        return;
    // a device is there!
    insert(bus_group,
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
        
        insert(bus_group,
               bus, 
               device, 
               func
        );
    }
}

/**
 * constructs the found_devices array
 */
static void scan_devices(void) {

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
                
                scan_device(bus_group,bus,device);
            }
        }
    }

}


// create the output array
// with the 
static void create_array(void) {

    // now create the final array
    found_devices = malloc(
                    n_found_devices 
                    * sizeof(struct pcie_dev));

    // now fill it and free the devices structure
    struct pcie_dev* ptr = found_devices;

    unsigned i = 0;

    for(struct early_device_desc* device = head_node.next;
                                  device != NULL;
                                  device = device->next)
    {
        struct PCIE_config_space* cs = device->config_space;

        assert(i++ < n_found_devices);        

        *ptr++ = (struct pcie_dev) {
                .bars = {
                    0,//get_bar_resource(cs, 0),
                    0,//get_bar_resource(cs, 1),
                    0,//get_bar_resource(cs, 2),
                },
                .config_space = cs,
                .driver = NULL,
                .info = (struct dev_info) {
                    .vendorID     = cs->vendorID,
                    .deviceID     = cs->deviceID,
                    .classcode    = cs->classcode,
                    .subclasscode = cs->subclasscode,
                    .progIF       = cs->progIF,
                    .revID        = cs->revID,
                },
                .path = (pcie_path_t) {
                    .domain = device->domain,
                    .bus    = device->bus,
                    .device = device->device,
                    .func   = device->function,
                },
        };
        


        free(device);
    }
}


// identity map every page that might be a config space 
static void identity_map_possible_config_spaces(void) {
    for(unsigned i = 0; i < pcie_descriptor.size; i++) {

        // identity map the corresponding pages
        map_pages(
            (uint64_t)pcie_descriptor.array[i].address, // phys
            (uint64_t)pcie_descriptor.array[i].address, // virt
            256 * // busses
            32  * // devices
            8,    // functions
            PRESENT_ENTRY | PCD | PL_XD
            // no cache, execute disable
        );
    }
}


static void identity_unmap_possible_config_spaces(void) {
    for(unsigned i = 0; i < pcie_descriptor.size; i++)
        unmap_pages(
            (uint64_t)pcie_descriptor.array[i].address,
            256 * 32 * 8
        );
}

/**
 * during the init,
 * we identity map the pcie configuration spaces
 * 
 */
struct pcie_dev* pcie_scan(unsigned* size) {

    log_debug("scanning pcie devices...");


    log_debug("%u PCIE bus groups found", pcie_descriptor.size);


    identity_map_possible_config_spaces();
    log_debug("identity_map_possible_config_spaces()", pcie_descriptor.size);

    //scan_devices();
    log_debug("devices scanned", pcie_descriptor.size);


    //create_array();
    //log_debug("memory shit done", pcie_descriptor.size);


    for(unsigned i = 0; i < n_found_devices; i++) {
        
        log_warn("%2x:%2x:%2x: %2x:%2x:%2x, rev %2x, "
                 "vendor 0x%4x", 
            
            found_devices[i].path.bus,
            found_devices[i].path.device,
            found_devices[i].path.func,

            found_devices[i].info.classcode,
            found_devices[i].info.subclasscode,
            found_devices[i].info.progIF,
            found_devices[i].info.revID,

            found_devices[i].info.vendorID
        );
    }
    

//    identity_unmap_possible_config_spaces();
    log_info("found %u PCI Express devices", n_found_devices);

    *size = n_found_devices;
    return found_devices;

}

