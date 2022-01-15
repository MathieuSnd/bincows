/**
 * @autor Mathieu Serandour
 * 
 * At installation, this driver first configures the 
 * administration queues. When this is done, it creates
 * one pair of IO queues. As long as the initialization
 * process is not finished, all of the operations
 * (identify, IO queue creation) are blocking. This might 
 * slow the system initialization down.
 */

#include "nvme.h"

// inclundes the other headers
#include "commands.h"

#include "../pcie/pcie.h"
#include "../driver.h"

#include "../../lib/logging.h"
#include "../../lib/panic.h"
#include "../../lib/sprintf.h"
#include "../../lib/registers.h"
#include "../../lib/dump.h"
#include "../../lib/time.h"
#include "../../lib/string.h"
#include "../../memory/physical_allocator.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../int/irq.h"

// for apic_eoi()
#include "../../int/apic.h"

static void remove(driver_t* this);

#define HANDLED_NAMESPACES 1
#define ADMIN_QUEUE_SIZE 2
#define IO_QUEUE_SIZE    64

struct namespace {
    uint32_t id;

    // block size: 1 << block_size_shift
    uint32_t block_size_shift;
    uint64_t capacity;
    uint32_t pref_granularity;
    uint32_t pref_write_alignment;
    uint32_t pref_write_size;
    int      flags; // bit 0: optperf
};

/**
 * the whole structure is zeroed when
 *  created
 */
struct data {
    unsigned          doorbell_stride;
    struct queue_pair admin_queues;
    struct queue_pair io_queues;

    unsigned transfert_max_size;

    // alias for this->dev->bars[0]
    struct regs*      registers;

    // number of namespaces 
    unsigned          nns;

    // namespace ids array
    struct namespace  namespaces[HANDLED_NAMESPACES];
};

static
void perform_read_command(
    struct data* data,
    struct regs* regs,
    uint64_t lba,
    void*    _buf,
    size_t   count
);

static
void perform_write_command(
    struct data* data,
    struct regs* regs,
    uint64_t lba,
    void*    _buf,
    size_t   count
);


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


    log_debug("wait for the controller to be ready");

    // wait for the controller to be ready
    while((regs->status & 1) == 0)
        asm("pause");
    log_debug("OK");

}


static inline
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
              "aqattr: %x\n",

              regs->cap,
              (regs->cap>>37)&0xff,
              regs->version,
              regs->intmask_set,
              regs->intmask_clear,
              regs->config,
              regs->status,
              regs->aqattr
              );
}


// alloc a page, and return the paddr 
static uint64_t createPRP(void) {
    uint64_t paddr = physalloc_single();
    uint64_t vaddr = (uint64_t)translate_address((void*)paddr);

    // disable cache for this page:
    // unmap it, the remap it as not cachable
    unmap_pages(vaddr, 1);
    
    map_pages(
        paddr, 
        vaddr, 
        1,
        PRESENT_ENTRY | PL_XD | PCD              
    ); 
    
    return paddr;
}


static void freePRP(uint64_t paddr) {
    uint64_t vaddr = (uint64_t)translate_address((void*)paddr);
    

    // reset for this page:
    // unmap it, the remap it as cachable

    unmap_pages(
        (uint64_t)translate_address((void*)paddr), 
        1
    );
    map_pages(
        paddr, 
        vaddr, 
        1,
        PRESENT_ENTRY | PL_XD // cache enable
    );
    physfree(paddr);
}


static void setup_admin_queues(
        driver_t*    this, 
        struct regs* regs
) {

    struct data* data = this->data;

    data->admin_queues = (struct queue_pair) {
        .sq=create_queue(ADMIN_QUEUE_SIZE, sizeof(struct subqueuee),0),
        .cq=create_queue(ADMIN_QUEUE_SIZE, sizeof(struct compqueuee),0),
    };
// let the controller know about those
    regs->asq_low  = trv2p((void*)data->admin_queues.sq.base) & 0xffffffff;
    regs->asq_high = trv2p((void*)data->admin_queues.sq.base) >> 32;
    regs->acq_low  = trv2p((void*)data->admin_queues.cq.base) & 0xffffffff;
    regs->acq_high = trv2p((void*)data->admin_queues.cq.base) >> 32;
}


static
void doorbell_completion(driver_t* this,
                         struct regs* regs, 
                         struct queue* cq
) {
    struct data* data = this->data;
    
    uint32_t* doorbell = 
            ((void*)regs)
             + 0x1000 
             + data->doorbell_stride * (2 * cq->id + 1);

    *doorbell = cq->head;
}

// update queue structure and doorbells
// and panics if an error occured
static void handle_queue(
    struct driver* this,
    struct regs* regs,
    struct queue_pair* queues
) {
    update_queues(queues);
    struct queue* cq = &queues->cq;


    // handler every new entry
    while(!queue_empty(cq)) {
        volatile
        struct compqueuee* entry = queue_head_ptr(cq);

        if(entry->status) {
            // error 
            dump((void*)entry, 16, 8, DUMP_HEX8);
            panic(
                "NVMe: a problem occured "
                "(see the above dump for any nerdish debug)"
            );
        }

        queue_consume(cq);
    }


    doorbell_completion(
        this,
        regs,
        &queues->cq
    );
}

static void irq_handler(driver_t* this) {
    // check if an admin command is completed
    struct data* data = this->data;
    log_warn("irq!");

    handle_queue(
            this, 
            data->registers,  
            &data->admin_queues);
    
    // if io queues are well initialized..
    if(data->io_queues.cq.base)
        handle_queue(
            this, 
            data->registers,  
            &data->io_queues);


    // acknowledge the irq to the apic
    apic_eoi();
}


// create IO submission and completion queues
static void createIOqueues(
    struct data* data,
    struct regs* regs
) {


    
    data->io_queues = (struct queue_pair) {
        .sq=create_queue(IO_QUEUE_SIZE, sizeof(struct subqueuee ),1),
        .cq=create_queue(IO_QUEUE_SIZE, sizeof(struct compqueuee),1),
    };


    // physical addresses
    uint64_t p_compque_buff = trv2p((void*)data->io_queues.cq.base);
    uint64_t p_subque_buff  = trv2p((void*)data->io_queues.sq.base);

    /* synchronously enqueue a IO completion queue creation
       command in the admin queue  
    */
    sync_command( // create IO completion queue
        data->doorbell_stride,
        regs,                    // NVMe register space
        &data->admin_queues.sq,  // admin submission queue
        OPCODE_CREATE_CQ,        // opcode: create completion 
                                 // queue
        1,                       // cmdid
        0,                       // nsid - unused
        p_compque_buff,          // prp0
        0,                       // prp1
        ((IO_QUEUE_SIZE-1) << 16)// cdw10 high word: queue 
                                 // size - 1
        | 1,                     // low word:  queue id
        (0 << 16)                // cdw11: IVT (upper part)
       | 2                       // interrupt enable
       | 1,                      // physically contiguous
       0                         // cdw12: unused for this 
    );                           // command
    

    /* synchronously enqueue a IO submission queue creation
       command in the admin queue  
    */
    sync_command( // create IO submission queue
        data->doorbell_stride,
        regs,                    // NVMe register space
        &data->admin_queues.sq,  // admin submission queue
        OPCODE_CREATE_SQ,        // opcode: create submission 
                                 // queue
        2,                       // cmdid
        0,                       // nsid - unused
        p_subque_buff,           // prp0
        0,                       // prp1
        ((IO_QUEUE_SIZE-1) << 16)// cdw10 high word: queue 
                                 // size - 1
        | 1,                     // low word:  queue id
        (1 << 16)                // cdw11: completion queue id
       | 1,                      // physically contiguous
        0                        // cdw12: unused for this 
    );                           // command
    log_warn("SQ=%lx", p_subque_buff);
}


// cns:  0 to idenfity a ns
//       1 for the controller
//       2 for the active ns list
// if not indentifying a namespace,
// put nsid = 0
static void identify(
    struct data* data,
    struct regs* regs,

    uint16_t  cmdid,
    uint32_t  nsid,     
    uint8_t   cns,
    uint64_t  dest_paddr
) {
    assert(cns <= 2);

    sync_command(
        data->doorbell_stride,
        regs,
        &data->admin_queues.sq,
        OPCODE_IDENTIFY,       // opcode
        cmdid,
        nsid,
        dest_paddr, // PRP0
        0,          // PRP1 - unused
        cns,
        0,          // CNTID
        0          // cdw12 - unused
    );
}


static void identify_controller(
    struct data* data,
    struct regs* regs
) {

    // physical address
    uint64_t pdata = createPRP();
    
    identify( // identify controller
        data,
        regs,
        0,    // CMDID
        0,    // NSID: unused for this cns
        1,    // CNS, controller: 1
        pdata
    );

    uint8_t* vdata = translate_address((void*)pdata);
    
    data->transfert_max_size = 1 << vdata[77];
    log_warn("MAX: %u pages", vdata[77]);

    // no need to keep this.
    // very dumb to copy 4K to just keep 1 byte...
    freePRP(pdata);
}


// we identify the ith namespace
// in the structure 
static void identify_namespace(
    struct data* data,
    struct regs* regs,
    uint32_t     i
) {
    // physical base address
    // of the destination NS 
    // data structure
    uint64_t pdata = createPRP();

    uint32_t nsid = data->namespaces[i].id;

    log_info("nsid=%x", nsid);
    
    identify(    // identify namespaces
        data,
        regs,
        nsid + 3,// CMDID: begins at 4,
                 // we can pick nsid+3 as 
                 // NSIDs are distincs and
                 // >= 1.
        nsid,    // NSID
        0,       // CNS, NSID list: 2
        pdata
    );

    void* vdata = translate_address((void*)pdata);


    struct namespace* ns = &data->namespaces[i];
    
    ns->capacity             = *(uint64_t*)(vdata +  8);

    // OPTPERF bit: validity of fields:
    // pref_granularity, 
    // pref_write_alignment, 
    // pref_write_size
    ns->flags                    = *(uint64_t *)(vdata + 24)&16>>4;
    if(ns->flags) { 
        ns->pref_granularity     = *(uint16_t *)(vdata + 64);
        ns->pref_write_alignment = *(uint16_t *)(vdata + 66);
        ns->pref_write_size      = *(uint16_t *)(vdata + 72);
    }
    uint8_t flbas                = *(uint8_t  *)(vdata + 26);

    uint8_t lbaid = flbas & 0xf;
    struct lbaf* lbafs = vdata + 128;

    ns->block_size_shift = lbafs[lbaid].ds;
    if(!ns->block_size_shift)
        ns->block_size_shift = 9;
    

    log_info("namespace:\n"
    "    ns->id                   : %lx\n"
    "    ns->capacity             : %u K, %u M\n"
    "    ns->flags                : %lx\n"
    "    ns->block_size_shift     : 1 << %lx = %lu bytes\n"
    "    ns->pref_granularity     : %lx\n"
    "    ns->pref_write_alignment : %lx\n"
    "    ns->pref_write_size      : %lx",
    ns->id,
    ns->capacity >> 10,
    ns->capacity >> 20,
    ns->flags,
    ns->block_size_shift,
    1<< ns->block_size_shift,
    ns->pref_granularity,
    ns->pref_write_alignment,
    ns->pref_write_size
    );

    // no need to keep this.
    // very dumb to copy 4K to just keep 16 byte...
    freePRP(pdata);
}



static 
void identify_active_namespaces(
    struct driver* this,
    struct data*   data,
    struct regs*   regs
) {
    // physical base address
    // of the destination data
    // containing the active Namespaces
    uint64_t pdata = createPRP();
    
    identify( // identify namespaces
        data,
        regs,
        3,    // CMDID
        0,    // NSID: the returned NSIDs are
              // higher than this field. We want
              // them all
        2,    // CNS, NSID list: 2
        pdata
    );

    uint32_t* vdata = translate_address((void*)pdata);
    
    // keep it outside the loop to keep track of the 
    // end iteration
    int i;

    for(i = 0; i < HANDLED_NAMESPACES; i++) {
        uint32_t nsid = *vdata++;
        if(nsid == 0) // end
            break;
        data->namespaces[i].id = nsid;
    }

    data->nns = i;
    
    if(data->nns == 0)
        log_warn("NVMe: no namespace found");
    else if(data->nns > 1)
        log_warn("NVMe: Bincows currently only handles"
                 "one NVMe namespace per controller. "
                 "%u namespaces are ignored for device %s.",
                 
                 data->nns-1,
                 this->device->name
        );

    log_warn("%u found namespaces: %x", data->nns, data->namespaces[0].id);

    // no need to keep this.
    // very dumb to copy 4K to just keep 16 byte...
    freePRP(pdata);
}


// return whether the device is 
// supported or not
// cap: capabilities register
static
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


static void set_pci_cmd(driver_t* this) {
    struct pcie_dev* dev = (struct pcie_dev*)this->device;
    struct PCIE_config_space* cs = dev->config_space;

    // interrupt disable   (bit 10),
    // bus master enable   (bit 02),
    // Memory Space Enable (bit 01),
    // I/O Space Enable    (bit 0)
    cs->command = 7;
}


static void init_struct(driver_t* this) {
// init struct
    this->remove = remove;
    this->name = (string_t) {"Bincows NVMe driver", 0};
    this->data = malloc(sizeof(struct data));
    // zero it
    memset(this->data, 0, sizeof(struct data));

    this->data_len = sizeof(struct data);
    rename_device((struct pcie_dev*)this->device);
}


void setup_irqs(
            driver_t* this,
            struct pcie_dev* dev,
            struct regs*     regs
) {
    int using_msix = init_msix(dev);

    
    // this register shall not be accessed
    // when using msix
    if(! using_msix)
        regs->intmask_clear = 0xff;

    
    assert(using_msix);

    unsigned admin_irq = install_irq(irq_handler, this);

    set_msix(dev, 0, 0, admin_irq, 0, 0, 0);
    _sti();
}


int nvme_install(driver_t* this) {
    log_info("NVME: installing...");

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

    bar0->aqattr = 
            ((ADMIN_QUEUE_SIZE-1) << 16) // admin queues
          | ((ADMIN_QUEUE_SIZE-1) << 0);
    

    struct data* data = this->data;
    data->registers = bar0;

    // CAP.DSTRD
    data->doorbell_stride = 4 << ((cap>>32) & 0xf);
    // start the controller
    
    // setup interrupts

    setup_admin_queues(this, bar0);
    setup_irqs(this, dev, bar0);    
    log_warn("enable(bar0);");
    enable(bar0);

// actually it holds no usefull information
    identify_controller(data, bar0);

    createIOqueues(data, bar0);

    identify_active_namespaces(this, data, bar0);

    // iterate over identified namespaces
    //for(unsigned i = 0; i < data->nns; i++)
    identify_namespace(data, bar0, 0);
    //sleep(3000);

    this->status = DRIVER_STATE_OK;

    void* buff = malloc(0x2000);

    nvme_sync_read(
        this,
        0x1fca00 >> 9,
        buff,
        16
    );

    dump(
        buff,
        8192,
        32,
        DUMP_HEX8
    );


// installed
    return 1;
}


static void shutdown(struct driver* this) {

    struct data* data = this->data;

    // wait for every operations to finish
    if(data->admin_queues.cq.base) {
        // if initialized...
        while(data->admin_queues.cq.head 
           != data->admin_queues.sq.tail)
            sleep(1); 
        log_debug("done");

    }
    if(data->io_queues.cq.base) {
        // if initialized...
        while(data->io_queues.cq.head
           != data->io_queues.sq.tail)
            sleep(1); 
    }
    // all done! we can safely send a shutdown request
    // without the risk of losing data

    struct pcie_dev* dev = (struct pcie_dev*)this->device;
    struct regs* bar0    = (struct regs*)dev->bars[0].base;

    //  controller shutdown request
    bar0->config = (bar0->config & ~0xc000) | (0b10 << 14);
    
    
    this->status = DRIVER_STATE_SHUTDOWN;
}


static void remove(driver_t* this) {
    shutdown(this);
    struct data* data = this->data;
    
    // free all the initialized queues
    if(data->admin_queues.cq.base) {
        free_queue(&data->admin_queues.sq);
        free_queue(&data->admin_queues.cq);
    }
    if(data->io_queues.cq.base) {
        free_queue(&data->io_queues.sq);
        free_queue(&data->io_queues.cq);
    }

    // finally free the data structure
    free(this->data);
}


int nvme_get_lbashift(struct driver* driver) {
    struct data* data = driver->data;
    return data->namespaces[0].block_size_shift;
}


// find the biggest size for the access 
// that does not cross a page border (in 
// our shitty architecture)
// and advance the vaddr ptr
static 
uint16_t get_max_count_and_advance(uint64_t* vaddr, unsigned shift) {
    uint16_t c = 0;

    uint64_t page_base = (*vaddr) & ~0x0fff;


    while(1) {
        c++;

        uint64_t next = *vaddr + (1 << shift);
        
        // crossed a 4K page border
        if((next & ~0x0fff) != page_base)
            break; 

        *vaddr = next;
    }

    return c;
}

/** _buf constraints:
 *      - if the transfert crosses a page
 *        border, _buf must be LBA size aligned
 *      
 *      - else it only must be 4 aligned
 */
static
void perform_read_command(
    struct data* data,
    struct regs* regs,
    uint64_t lba,
    void*    _buf,
    size_t   count
) {
    // general assert protection
    assert(lba         < data->namespaces[0].capacity);
    assert(lba + count < data->namespaces[0].capacity);
    assert(data->namespaces[0].id == 1);
    assert(data->nns >= 1);

    // NVME PRP: must be dword aligned
    assert_aligned(_buf, 4);

    uint64_t buf_vaddr = (uint64_t)_buf;

    
    unsigned shift = data->namespaces[0].block_size_shift;


    // must not cross a 4K page boudary
    assert(
        ((buf_vaddr + (1 << shift)) & ~0x0fff)
      ==( buf_vaddr                 & ~0x0fff)
    );


    while(count != 0) {

        uint64_t paddr = trv2p((void*)buf_vaddr);
        

        uint16_t c = get_max_count_and_advance(&buf_vaddr, shift);
        
        if(c > count)
            c = count;


        // c == 0 if the buffer is not well aligned
        assert(c != 0);



        async_command( // read command
            data->doorbell_stride,
            regs,                    // NVMe register space
            &data->io_queues.sq,  // admin submission queue
            OPCODE_IO_READ,
            0,                       // cmdid - unused
            1,                       // nsid
            paddr,                   // prp0
            0,                       // prp1
            lba&0xffffffff,          // cdw10: low lba part
            lba>>32,                 // cdw11: high lba part
            c - 1                    // cdw12 low part: count-1
        );

        lba += c;
        count -= c;
    }
}


/** _buf constraints:
 *      - if the transfert crosses a page
 *        border, _buf must be LBA size aligned
 *      
 *      - else it only must be 4 aligned
 */
static
void perform_write_command(
    struct data* data,
    struct regs* regs,
    uint64_t lba,
    void*    _buf,
    size_t   count
) {
    // general assert protection
    assert(lba         < data->namespaces[0].capacity);
    assert(lba + count < data->namespaces[0].capacity);
    assert(data->namespaces[0].id == 1);
    assert(data->nns >= 1);

    // NVME PRP: must be dword aligned
    assert_aligned(_buf, 4);

    uint64_t buf_vaddr = (uint64_t)_buf;

    
    unsigned shift = data->namespaces[0].block_size_shift;


    // must not cross a 4K page boudary
    assert(
        ((buf_vaddr + (1 << shift)) & ~0x0fff)
      ==( buf_vaddr                 & ~0x0fff)
    );


    while(count != 0) {
        uint64_t paddr = trv2p((void*)buf_vaddr);

        uint16_t c = get_max_count_and_advance(&buf_vaddr, shift);

        if(c > count)
            c = count;

        // c == 0 if the buffer is not well aligned
        assert(c != 0);


        async_command( // read command
            data->doorbell_stride,
            regs,                    // NVMe register space
            &data->io_queues.sq,     // admin submission queue
            OPCODE_IO_WRITE,
            0,                       // cmdid - unused
            1,                       // nsid
            paddr,                   // prp0
            0,                       // prp1
            lba&0xffffffff,          // cdw10: low lba part
            lba>>32,                 // cdw11: high lba part
            c - 1                    // cdw12 low part: count-1
        );
        count -= c;
        lba   += c;
    }
}


void nvme_sync_read(struct driver* this,
                    uint64_t lba,
                    void*    buf,
                    size_t   count
) {
    assert(this->status == DRIVER_STATE_OK);
    struct data* data = this->data;
    assert(data->nns);



    unsigned shift = data->namespaces[0].block_size_shift;

    unsigned max_count = 0x1000 >> shift;

    // we only use one prp.
    // this is slow.    
    uint64_t* prp_paddr = createPRP();    


    while(queue_full(&data->io_queues.sq))
        sleep(1);

    while(count != 0) {
        // busy wait for a submission entry to be 
        // available

        unsigned c;

        if(count < max_count)
            c = count;
        else
            c = max_count;
        

        perform_read_command(
            data, 
            data->registers,
            lba,
            (void*)translate_address((void*)prp_paddr),
            c
        );

        while(!queue_empty(&data->io_queues.sq))
            sleep(1);
        
        memcpy(buf, translate_address(prp_paddr), c << shift);
        buf += c << shift;

        lba   += c;
        count -= c;
    }


    freePRP(prp_paddr);
}


void nvme_sync_write(struct driver* this,
                     uint64_t lba,
                     void*    buf,
                     size_t   count
) {
    panic("nvme_sync_write: unimplented");
}




