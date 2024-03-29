#include "../../memory/pmm.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../lib/dump.h"
#include "../../lib/logging.h"

#include "queue.h"
#include "spec.h"
#include "../../lib/string.h"

struct queue create_queue(
    unsigned size, 
    unsigned element_size,
    unsigned id
) {
    uint64_t paddr = physalloc_single();
    void*    vaddr = translate_address((void*)paddr);


    // we know that all the memory is mapped
    // to TRANSLATED_PHYSICAL_MEMORY_BEGIN + addr
    // so no need to map it again

    memset(vaddr, 0, size * element_size);

    unmap_pages((uint64_t)vaddr, 1, 0);

    map_pages(
        paddr, 
        (uint64_t)vaddr,
        1,
        PRESENT_ENTRY | PL_XD | PCD | PL_RW
    );
  
    return (struct queue) {
        .base = vaddr,
        .element_size = element_size,
        .size         = size,
        .id   = id, 
        .head = 0,
        .tail = 0
    };
}

void free_queue(struct queue* q) {
    uint64_t vaddr = (uint64_t)q->base;
    uint64_t paddr = vaddr & ~TRANSLATED_PHYSICAL_MEMORY_BEGIN;

    // change page flags: 
    // unmap then remap it as cachable

    unmap_pages((uint64_t)vaddr, 1, 0);

    map_pages(
        paddr, 
        vaddr,
        1,
        PRESENT_ENTRY | PL_XD | PL_RW
    );
  
    physfree(paddr);
}

void update_queues(struct queue_pair* queues) {
    // check all phase bits that could 
    // have changed

    while(1) {
        volatile
        struct compqueuee* entry = 
                    queue_tail_ptr(&queues->cq);
        
        int old_phase = cq_tail_phase(queues);

        // end of the input
        if(entry->phase == old_phase)
            break;

        // the controller produced an entry
        // in the completion queue
        queue_produce(&queues->cq);

        // which means that the controller
        // consumed one entry in the submission 
        // queue
        queue_consume(&queues->sq);



        // increase the counter to keep predicting 
        // the old phase bit state
        queues->completion_counter++;
    }
}
