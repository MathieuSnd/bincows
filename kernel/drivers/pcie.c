#include "pcie.h"
#include "../lib/logging.h"
#include "../lib/dump.h"
#include "../lib/assert.h"
#include "../memory/heap.h"
#include "../memory/paging.h"
#include "../lib/string.h"


struct PCIE_configuration_space {
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

// higher bus number: could be 0xff
// if there is only one bus group
static size_t max_bus = 0;

struct PCIE_Descriptor pcie_descriptor = {0};
// 0   | group | bus | 0 | slot  | 0 | func |
//     |   2   | 8   | 1 |  3    | 1 |  3   |
//             16    8              

static struct PCIE_segment_group_descriptor* get_segment_group(unsigned bus) {
    struct PCIE_segment_group_descriptor* group = &pcie_descriptor.array[0];

    unsigned group_number = 0;
    unsigned start_bus, end_bus;

// go through all segment groups
// if the bus is not in the range,
// it is not the right segment

    while((start_bus = group->start_bus) > bus 
        || (end_bus  = group->end_bus)   < bus) {

        assert(group_number != pcie_descriptor.size);

        group_number++;
        group++;
    }
    return group;
}

static struct PCIE_configuration_space* __attribute__((pure)) get_config_space_base(
                unsigned bus, 
                unsigned device, 
                unsigned func
                ) {
    struct PCIE_segment_group_descriptor* group_desc = get_segment_group(bus);

    return group_desc->address + (
        (bus - group_desc->start_bus) << 20 
      | device                        << 15 
      | func                          << 12);
}


static int __attribute__((pure)) get_vendorID(unsigned bus, unsigned device, unsigned func) {
    struct PCIE_configuration_space* config_space = get_config_space_base(bus,device,func);
    
    return config_space->vendorID;
}


struct pcie_device_descriptor {
    uint16_t bus, device, function;
    struct PCIE_configuration_space* config_space;
};

// array of all installed devices' functions
static struct pcie_device_descriptor* found_devices = NULL;
static size_t n_found_devices = 0;


/**
 * constructs the found_devices array
 */
static void scan_devices(void) {

    // first create a linked list
    // as we dont know how many devices are present
    struct device_desc_node {
        struct pcie_device_descriptor e;
        struct device_desc_node* next;
    };

// head of the list 
    struct device_desc_node ghost_node = {.next = NULL};
    struct device_desc_node* current = &ghost_node; 
// count the number of devices

    void insert(
        unsigned bus, 
        unsigned device, 
        unsigned func, 
        struct PCIE_configuration_space* config_space
        ) 
    {            
        current->next = malloc(sizeof(struct device_desc_node));

        current = current->next;
        current->next           = NULL;

        // fill the new node
        current->e.bus          = bus;
        current->e.device       = device;
        current->e.function     = func;
        current->e.config_space = config_space;

        n_found_devices++;
    }


    for(unsigned bus = 0; bus < max_bus; bus++) {
        for(unsigned device = 0; device < 32; device++) {
            
            // if the first function doesn't exist, so does 
            // the device
            uint16_t vendorID = get_vendorID(bus,device, 0);
            if(vendorID == 0xFFFF)
                continue;

            insert(bus, 
                   device, 
                   0, 
                   get_config_space_base(bus,device,0)
            );
            
            for(unsigned func = 1; func < 8; func++) {

                if(get_vendorID(bus,device, func) != vendorID)
                    continue;
                
                insert(bus, 
                       device, 
                       func, 
                       get_config_space_base(bus,device,func)
                );
            }
        }
    }
    

    // now create the final array
    found_devices = malloc(
                    n_found_devices 
                    * sizeof(struct pcie_device_descriptor));
    

    // now fill it and free the devices structure
    struct pcie_device_descriptor* ptr = found_devices;

    for(struct device_desc_node* device = ghost_node.next;
                                 device != NULL;
                                 ) {

        memcpy(ptr++, &device->e, sizeof(struct pcie_device_descriptor));
        

        struct device_desc_node* next = device->next;
        free(device);
        device = next;
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
        unmap_pages((uint64_t)pcie_descriptor.array[i].address, 256 * 32 * 8);
}

/**
 * during the init,
 * we identity map the pcie configuration spaces
 * 
 */
void pcie_init(void) {

    log_debug("init pcie...");

    // calculate the highest bus number

    for(unsigned i = 0; i < pcie_descriptor.size; i++) {
        // map the corresponding pages 
        if(pcie_descriptor.array[i].end_bus > max_bus)
            max_bus = pcie_descriptor.array[i].end_bus;
    }

    log_debug("%u PCIE bus groups found", pcie_descriptor.size);

    identity_map_possible_config_spaces();
    scan_devices();


    for(unsigned i = 0; i < n_found_devices; i++) {
        
        log_warn("%2x:%2x:%2x: %2x:%2x:%2x, rev %2x, "
                 "vendor 0x%4x", 
            
            found_devices[i].bus,
            found_devices[i].device,
            found_devices[i].function,

            found_devices[i].config_space->classcode,
            found_devices[i].config_space->subclasscode,
            found_devices[i].config_space->progIF,
            found_devices[i].config_space->revID,
            
            found_devices[i].config_space->vendorID
        );
    }

    identity_unmap_possible_config_spaces();
    log_info("found %u PCI Express devices", n_found_devices);
}

void pcie_init_devices(void) {
}