#include "pcie.h"
#include "scan.h"
#include "../driver.h"

#include "../nvme.h"

#include "../../lib/logging.h"
#include "../../lib/assert.h"
#include "../../lib/panic.h"
#include "../../lib/math.h"
#include "../../memory/heap.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../lib/string.h"

////__attribute__((pure))
unsigned pcie_bar_size(void* config_space, 
                       unsigned i) 
{   
    struct PCIE_config_space* cs = config_space;

    volatile uint32_t* bar_reg = (uint32_t*)
                &cs->bar[i];
    

// we only need the 32 bit value
// even if it is 64 bit, the bar sizes are 
// inferior to 4 GB
    uint32_t val32 = *bar_reg;

    
    *bar_reg = ~0x0fllu | (val32 & 0x0f);
    unsigned read = (unsigned) ~*bar_reg;

    

    *bar_reg = val32;

    return (read & ~0x0f) + 0x10;
}

// recognize compatible drivers
// and set them up
static void scan_drivers_callback(struct pcie_dev* dev) {
    register_dev((struct dev *)dev);

    log_info("pcie device: %s %2x:%2x:%2x.%2x, "
             "%2x.%2x.%2x, rev%x",
                dev->dev.name.ptr,
                dev->path.domain,
                dev->path.bus,
                dev->path.device,
                dev->path.func,

                dev->info.classcode,
                dev->info.subclasscode,
                dev->info.progIF,
                dev->info.revID
                );
// 3.0: vga controller
// 3.1: xga
// 3.2: 3d
// 3.80: other
    if(dev->info.classcode == 1 && 
        dev->info.subclasscode == 8) {
        //1.8: NVMe controller
        driver_register_and_install(nvme_install, (dev_t*)dev);                
    }
    if(dev->info.classcode == 03 &&
        dev->info.subclasscode == 00) {
            // vga
            //setup_msi(dev, 48, 0, 0, 0);
        }
// register it even if there is no driver
// to treat it
}


void pcie_init(void) {
    log_debug("init pcie...");
    pcie_scan(scan_drivers_callback);    
}


__attribute__((pure))
static void* get_capability(struct PCIE_config_space* cs, 
                            uint8_t capabilityID
) {

    // check that the device has a
    // pointer to the capabilities list 
    assert((cs->status & 0x10) == 0x10);

    // traverse the capabilities list
// see https://wiki.osdev.org/PCI#IRQ_Handling
    for(uint32_t*
            reg = (uint32_t*)(cs->capabilities | (uint64_t)cs);
            reg != NULL;
    ) {

        log_warn("issou %x", (*reg & 0xff));
        if(capabilityID == (*reg & 0xff))
            return reg;

        unsigned index = (*reg>>8)&0xff;

        if(index == 0)
            break;
        
        else
            reg = (uint32_t*)(index | (uint64_t)cs);

    }
    
    return NULL;
}


int enable_msi(struct pcie_dev* dev, 
               unsigned vector, 
               uint32_t processor,
               uint32_t edge_trigger,
               uint8_t  deassert
) {
    assert(dev != NULL);
    assert(vector < 256);
    struct PCIE_config_space* cs = dev->config_space;


    uint32_t* cap = get_capability(cs, 0x05);
    
    if(!cap) {
        // no MSI support on the device
        return 0;
    }

    // message address 
    cap[1] = 0xFEE00000 | (processor << 12);
    cap[2] = 0; // high is reserved as NULL

    // message data

	*(uint16_t*)(cap+3) = vector
          | (edge_trigger == 1 ? 0 : (1 << 15)) 
          | (deassert     == 1 ? 0 : (1 << 14));


    // enable MSIs
    ((uint16_t*)cap)[1] = (((uint16_t*)cap)[1] & 0xff7e) | 1;
    // keep reserved bits and capabilities
    // per-vector unmask    (bit 8)
    // and enable           (bit 0)

    return 1;   
}


struct msix_entry {
    uint64_t msg_addr;
    uint32_t msg_data;
    uint32_t vector_ctrl;
};


int init_msix(struct pcie_dev* dev) {
    assert(dev != NULL);
    struct PCIE_config_space* cs = dev->config_space;


    uint32_t* restrict cap = get_capability(cs, 0x11);
    
    if(!cap) {
        // no MSI-X support on the device
        return 0;
    }
    unsigned table_reg = cap[2];


// bar index
    unsigned bir          = table_reg &  0x7u;

// offset in bar[bir]
    unsigned table_offset = table_reg & ~0x7u;

    unsigned table_size   = ((cap[0] >> 16) & 0x3ff);

//  bar0 ... bar5
    assert(bir < 6);
    assert(dev->bars[bir].base != 0);
    assert(table_offset + table_size <= dev->bars[bir].size);


    struct msix_entry* table = (void*)(
        dev->bars[bir].base
        + table_offset);


    dev->msix_table = table; 
    dev->msix_table_size = table_size;

    // mask every entry
    for(unsigned i = 0; i < table_size; i++)
        table[i].vector_ctrl = 1;



// global enable
    ((uint16_t*)cap)[1] = (((uint16_t*)cap)[1] & 0x3fff) | 0x8000;
    // set   'enable'        bit 15
    // unset 'function mask' bit 14
    // keep other bits

    return 1;
}


void set_msix(struct pcie_dev* dev, 
              unsigned index,
              unsigned mask,
              unsigned vector, 
              uint32_t processor,
              uint32_t edge_trigger,
              uint8_t  deassert
) {
    assert(dev != NULL);
    assert(vector < 0x100);
    assert(mask <= 1);
    assert(dev->msix_table != NULL);

    assert(sizeof(struct msix_entry) * (index+1) 
                <= dev->msix_table_size);


    struct msix_entry* table = dev->msix_table;
    struct msix_entry* e = table + index;


    e->msg_addr = 0xFEE00000 | (processor << 12);
    e->msg_data = (vector & 0xFF) 
          | (edge_trigger == 1 ? 0 : (1 << 15)) 
          | (deassert     == 1 ? 0 : (1 << 14));
    e->vector_ctrl = mask;
}
