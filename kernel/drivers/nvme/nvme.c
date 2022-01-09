#include "nvme.h"
#include "queue.h"

#include "../pcie/pcie.h"
#include "../driver.h"

#include "../../lib/logging.h"
#include "../../lib/sprintf.h"
#include "../../lib/registers.h"
#include "../../lib/dump.h"
#include "../../memory/physical_allocator.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../int/idt.h"

static void remove(driver_t* this);

struct data {
    unsigned doorbell_stride;
    struct queue_pair admin_queues;
};

#define NSSRS_SUPPORT(CAP) ((CAP & (1llu << 36)) != 0)


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
    volatile uint32_t asq_low;  // admin submission queue
    volatile uint32_t asq_high; // base address

    volatile uint32_t acq_low;  // admin completion queue
    volatile uint32_t acq_high; // base address
} __attribute__((packed));


struct subqueuee {
    uint32_t cmd;
    uint32_t nsid;
    uint32_t cdw2; // cmd specific
    uint32_t cdw3; // cmd specific
    uint64_t metadata_ptr;
    uint64_t data_ptr[2];
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));


static inline
uint32_t make_cmd(
            uint8_t opcode,
            unsigned fused, // 2 bits
            unsigned cmdid  // 16 bits
) {
    return ((unsigned)opcode) 
         | (fused << 8)
         | (0 << 14)   // 0: PRP; 1: SGL
         | (cmdid << 16);
}


struct compqueuee {
    uint32_t cmd_specific;
    uint32_t reserved;
    uint16_t sq_head_pointer;
    uint16_t iq_id;
    uint16_t cmd_id;
    unsigned phase: 1;
    unsigned status: 15;
} __attribute__((packed));


static_assert_equals(sizeof(struct subqueuee),  64);
static_assert_equals(sizeof(struct compqueuee), 16);


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


static void enable(struct regs* regs) {
    regs->config = 
            (0 << 24) // Controller Ready Independent of 
                      // Media Enable: I have no idea what
                      // it is.
          | (4 << 20) // I/O completion queue entry size
                      // should be 16, so 2**4
          | (6 << 16) // I/O submition queue entry size
                      // should be 64, so 2**6
          | (0   << 14) // shutdown notification
          | (0 << 11) // Arbitration Mechanism Selected 
                      // 0: round robin
                      // 1: Weighted Round Robin with 
                      //    Urgent Priority Class
          | (0 << 7)  // Map page size: (2 ^ (12 + MPS)) 
                      // 4K paging: MPS = 0
          | (0 << 4)  // NVM command set
          | 1;    // busy wait for the ready bit


    // wait for the controller to be ready
    while((regs->status & 1) == 0)
        asm("pause");
}

static 
void reset(struct regs* regs) {
    regs->subsystem_reset = 0x4E564D65;
    //regs->subsystem_reset = 0;
}

static 
int is_shutdown(struct regs* regs) {
    return ((regs->status >> 2) & 3) == 0b10;
}



static
void print_status(driver_t* this) {
    struct pcie_dev* dev = 
            (struct pcie_dev*)this->device;
        
    struct regs* regs = dev->bars[0].base;

    log_debug("cap: %lx\n"
              "CCS:        %x\n"
              "version:    %x\n"
              "mask set:   %x\n"
              "mask clear: %x\n"
              "config:     %x\n"
              "status:     %x\n"
              "queue_attr: %x\n",

              regs->cap,
              (regs->cap>>37)&0xff,
              regs->version,
              regs->intmask_set,
              regs->intmask_clear,
              regs->config,
              regs->status,
              regs->queue_attr
              );
}


// alloc a page, and return the paddr 
static uint64_t createPRP(void) {
    uint64_t paddr = physalloc_single();
    uint64_t vaddr = (uint64_t)translate_address((void*)paddr);
    unmap_pages(vaddr, 1);
    
    map_pages(paddr, 
              vaddr, 
              1,
              PRESENT_ENTRY | PL_XD | PCD              
              ); 
    
    return paddr;
}


static void freePRP(uint64_t paddr) {
    
    unmap_pages(
        (uint64_t)translate_address((void*)paddr), 
        1
    );
    physfree((void*)paddr);
}


static void setup_admin_queues(
        driver_t*    this, 
        struct regs* regs
) {

    struct data* data = this->data;

    data->admin_queues = (struct queue_pair) {
        .sq=create_queue(64, 64), // 64x64=4K
        .cq=create_queue(16, 64), // 16x64<4K
    };
// let the controller know about those
    regs->asq_low  = trv2p(data->admin_queues.sq.base) & 0xffffffff;
    regs->asq_high = trv2p(data->admin_queues.sq.base) >> 32;
    regs->acq_low  = trv2p(data->admin_queues.cq.base) & 0xffffffff;
    regs->acq_high = trv2p(data->admin_queues.cq.base) >> 32;
}


static void free_admin_queues(driver_t* this) {
    struct data* data = this->data;
    free_queue(&data->admin_queues.sq);
    free_queue(&data->admin_queues.cq);
}


static
void doorbell_submission(
                struct data* data,
                struct regs* regs, 
                unsigned queue_id, 
                unsigned tail
) {
    uint32_t* doorbell = 
            ((void*)regs)
             + 0x1000 
             + data->doorbell_stride * 2 * queue_id;

    *doorbell = tail;
}


static
void doorbell_completion(driver_t* this,
                         struct regs* regs, 
                         unsigned queue_id, 
                         unsigned head
) {
    struct data* data = this->data;
    
    uint32_t* doorbell = 
            ((void*)regs)
             + 0x1000 
             + data->doorbell_stride * (2 * queue_id + 1);

    *doorbell = head;
}

// the caller should first
// update the head
static void insert_command(
                struct data*      data,
                struct regs*      regs,
                struct queue*     sq, 
                struct subqueuee* sqe
) {
    assert(!queue_full(sq));

    struct subqueuee* tail = queue_tail(sq);


    *tail = *sqe;

    // doorbell
    doorbell_submission(
        data,       
        regs,       
        0,          // admin queue
        ++sq->tail  // new tail value 
    );
}


static void createIOqueue(
    driver_t* this,
    struct regs* regs
) {

    struct data* data = this->data;

    // physical address
    uint64_t pprp = createPRP();
    
    struct subqueuee identifye = {
        .cmd = make_cmd(
            0x01,               // opcode
            0,                  // fused
            0                   // cmdid
        ),                      //
        .nsid     = 0,       // unused
        .cdw2     = 0,          // unused
        .cdw3     = 0,          // unused
        .metadata_ptr = 0,      // unused
        .data_ptr = {
            pprp,               // prp0
            0                   // prp1:
                                // none
        },  
        .cdw10    = (64 << 16) | 1,
        .cdw11    = (0 << 16)   // IVT
                  | 2           // interrupt enable
                  | 1,          // physically
                                // contiguous
    };
    
    insert_command(
        data, 
        regs,
        &data->admin_queues.sq, 
        &identifye);


    for(int i = 0; i < 100; i++)
            outb(0x80, 0);

}

// cns:  0 to idenfity a ns
//       1 for the controller
//       2 for the active ns list
// if not indentifying a namespace,
// put nsid = 0
static void identify(
    driver_t* this,
    uint32_t  nsid,     
    uint8_t   cns,
    struct regs* regs
) {
    assert(cns < 2);

    struct data* data = this->data;

    // physical address
    uint64_t pprp = createPRP();
    
    struct subqueuee identifye = {
        .cmd = make_cmd(
            0x06,               // opcode
            0,                  // fused
            0                   // cmdid
        ),                      //
        .nsid     = nsid,       // unused
        .cdw2     = 0,          // unused
        .cdw3     = 0,          // unused
        .metadata_ptr = 0,      // unused
        .data_ptr = {
            pprp,               // prp0
            0                   // prp1:
                                // none
        },  
        .cdw10    = cns,
        .cdw11    = 0           // CNTID
    };
    
    insert_command(
        data, 
        regs,
        &data->admin_queues.sq, 
        &identifye);


    for(int i = 0; i < 100; i++)
            outb(0x80, 0);


    print_status(this);
    
    dump(
        translate_address((void*)pprp),
        64,
        20,
        DUMP_HEX8
    );
}


// return whether the device is 
// supported or not
// cap: capabilities register
int is_supported(struct pcie_dev* dev,
                 uint64_t cap) { 
    if(dev->dev.type != DEVICE_ID_PCIE)
        return 0;

    struct regs* bar0 = (struct regs*)dev->bars[0].base;

    if(bar0 == 0)
        return 0;
   
    // check that the NVM command set
    // is supported
    if((cap & (1llu << 37)) == 0)
        return 0;
    

    // check that the I/O command set
    // is supported
    //if((cap & (1llu << 43)) == 0)
    //    return 0;

    // check that the 4k pages are 
    // supported: MPSMIN = 0
    // min page size is 2^(12 + MPSMIN)
    if((cap & (0xfllu << 48)) != 0)
        return 0;

    // check if the device is resetable
    if(! NSSRS_SUPPORT(cap)) {
        log_warn("NSSRS is not supported on %s.", dev->dev.name.ptr);
        //return 0;        
    }

    return 1;
}

static
void init_struct(driver_t* this) {
// init struct
    this->remove = remove;
    this->name = (string_t) {"Bincows NVMe driver", 0};
    this->data = malloc(sizeof(struct data));
    this->data_len = sizeof(struct data);
    rename_device((struct pcie_dev*)this->device);
}


static void set_pci_cmd(driver_t* this) {
    struct pcie_dev* dev = (struct pcie_dev*)this->device;
    struct PCIE_config_space* cs = dev->config_space;

    // interrupt disable   (bit 10),
    // bus master enable   (bit 02),
    // Memory Space Enable (bit 01),
    // I/O Space Enable    (bit 0)
    cs->command = 7; // ou bien 6 jsp....

    log_warn("PCICMD=%x\n"
             "PCISTS=%x", 
             cs->command,
             cs->status);
}


static __attribute__((interrupt))
void nvme_irq_handler(void* ifr) {
    (void) ifr;

    log_warn("AYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
}

void setup_irqs(
            struct pcie_dev* dev,
            struct regs*     regs
) {
    int using_msix = init_msix(dev);

    assert(using_msix);
    
    // this register shall not be accessed
    // when using msix
    if(! using_msix)
        regs->intmask_clear = 0xff;

    set_irq_handler(50, nvme_irq_handler);

    set_msix(dev, 0, 0, 50, 0, 0, 0);
    set_msix(dev, 1, 0, 19, 0, 0, 0);
    set_msix(dev, 2, 0, 19, 0, 0, 0);
    set_msix(dev, 3, 0, 19, 0, 0, 0);
    _sti();
}


int nvme_install(driver_t* this) {

    struct pcie_dev* dev = (struct pcie_dev*)this->device;
    struct regs* bar0 = (struct regs*)dev->bars[0].base;

    // capabilities register
    const uint64_t cap = bar0->cap;


    if(!is_supported(dev, cap)) {
        log_warn("NVMe: %s is not supported!", dev->dev.name.ptr);
        return 0;
    }
    
    set_pci_cmd(this);
    
    init_struct(this);

    if(! NSSRS_SUPPORT(cap)) {
        // no reset support ...
        // I'm not sure what to do 
        // here ...
        // let's just unset the enable bit
        // maybe the computer won't explode
        bar0->config = 0x460000;
        log_warn("NO NSSRS");
    }
    else // reset properly
    {
        reset(bar0);
        bar0->config = 0x00;
    }

    log_debug("reseted");


    bar0->queue_attr = 
            (4 << 16) // 4 entries for 
          | (4 << 0); // admin queues
    

    struct data* data = this->data;

    // CAP.DSTRD
    data->doorbell_stride = 4 << ((cap>>32) & 0xf);
    // start the controller
    
    // setup interrupts
//
    setup_admin_queues(this, bar0);

    print_status(this);

//    createIOqueue(this, bar0);


    setup_irqs(dev, bar0);

    enable(bar0);
    



    _sti();
    set_pci_cmd(this);

    identify( // identify controller
        this, 
        0,    // unused for this cns
        1,     // controller: cns=1
        bar0
    );
    asm("hlt");
    //dump(data->admin_queues.cq.base, 16, 8, DUMP_HEX8);

    set_pci_cmd(this);


    struct PCIE_config_space* cs = dev->config_space;


    //pcie_map_bars(dev);

    volatile uint64_t* bar_reg = (uint64_t*)&cs->bar[0];

    

    //set_msix(dev, 1, 0, 51, 0, 0, 0);
    //set_msix(dev, 2, 0, 52, 0, 0, 0);
    //set_msix(dev, 3, 0, 53, 0, 0, 0);

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


// don't forget to delete it afterward!
unsigned nvme_create_ioqueue(struct driver* this) {
    (void) (this);
    return NVME_INVALID_IO_QUEUE;
}

void nvme_delete_ioqueue(struct driver* this, 
                         unsigned queueid) {
    (void) (this + queueid);
}

