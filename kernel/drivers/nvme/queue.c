#include "../../memory/physical_allocator.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../lib/logging.h"

#include "queue.h"

struct queue create_queue(
    unsigned size, 
    unsigned element_size
) {
    uint64_t paddr = physalloc_single();
    void*    vaddr = translate_address((void*)paddr);


    // we know that all the memory is mapped
    // to TRANSLATED_PHYSICAL_MEMORY_BEGIN + addr
    // so no need to map it again

    unmap_pages((uint64_t)vaddr, 1);

    map_pages(
        paddr, 
        (uint64_t)vaddr,
        1,
        PRESENT_ENTRY | PL_XD | PCD
    );
  

    return (struct queue) {
        .base = vaddr,
        .element_size = element_size,
        .size         = size,
        .head = 0,
        .tail = 0,
    };
}

void free_queue(struct queue* q) {
    unmap_pages((uint64_t)q->base, 1);

    // vaddr -> paddr 
    physfree((uint64_t)q->base & ~TRANSLATED_PHYSICAL_MEMORY_BEGIN);
}
