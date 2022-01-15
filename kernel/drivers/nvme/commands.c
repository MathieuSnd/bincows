#include "commands.h"
#include "../../lib/logging.h"
#include "../../lib/string.h"
#include "../../lib/time.h"

static
void doorbell_submission(
        unsigned     doorbell_stride,
        struct regs* regs, 
        unsigned     queue_id, 
        unsigned     tail
) {
    uint32_t* doorbell = 
            ((void*)regs)
             + 0x1000 
             + doorbell_stride * 2 * queue_id;

    *doorbell = tail;
}


void async_command(
    unsigned doorbell_stride,
    struct regs* regs,
    struct queue* sq,

    uint8_t  opcode, 
    uint16_t cmdid,
    uint32_t nsid,
    uint64_t prp0,
    uint64_t prp1,
    uint32_t cdw10,
    uint32_t cdw11,
    uint32_t cdw12
) {
    assert(!queue_full(sq));

    volatile
    struct subqueuee* tail = queue_tail_ptr(sq);



    *tail = (struct subqueuee) {
        .cmd = make_cmd(
            opcode,             // opcode
            0,                  // fused
            cmdid               
        ),                      
        .nsid     = nsid,       
        .cdw2     = 0,          // unused
        .cdw3     = 0,          // unused
        .metadata_ptr = 0,      // unused
        .data_ptr = {
            prp0,               // prp0
            prp1                // prp1
        },  
        .cdw10    = cdw10,
        .cdw11    = cdw11,
        .cdw12    = cdw12,
    };
    
    uint16_t tail_idx = queue_produce(sq);

    // doorbell
    doorbell_submission(
        doorbell_stride,       
        regs,       
        sq->id,          // admin queue
        tail_idx         // new tail value 
    );
}


void sync_command(
    unsigned doorbell_stride,
    struct regs* regs,
    struct queue* sq,

    uint8_t  opcode, 
    uint16_t cmdid,
    uint32_t nsid,
    uint64_t prp0,
    uint64_t prp1,
    uint32_t cdw10,
    uint32_t cdw11,
    uint32_t cdw12
) {

    async_command(
        doorbell_stride,
        regs,
        sq,
        opcode, 
        cmdid,
        nsid,
        prp0,
        prp1,
        cdw10,
        cdw11,
        cdw12
    );

    // blocking mode:
    // wait for the controller to process
    // the command
    while(!queue_empty(sq))
        sleep(1);
}
