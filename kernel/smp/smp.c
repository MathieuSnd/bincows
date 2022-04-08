#include "../memory/physical_allocator.h"
#include "../lib/logging.h"
#include "../lib/assert.h"
#include "smp.h"


static void ap_start(struct stivale2_smp_info* info) {

    log_warn("ap_start (procid = %u, lapic_id = %u)", info->processor_id, info->lapic_id);

    for(;;) {
        asm("hlt");
    }
}

static size_t cpus_count = 1;



void init_smp(struct stivale2_struct_tag_smp* tag) {
    cpus_count = tag->cpu_count;

    assert(tag->bsp_lapic_id == 0);

    // init APs
    for(unsigned i = 1; i < cpus_count; i++) {
        // set AP bootstrap stacks
        tag->smp_info[i].target_stack = physalloc_single() + 0x1000;

        // set AP bootstrap entry
        tag->smp_info[i].goto_address = (uint64_t)ap_start;
    }
}


size_t get_smp_count(void) {
    return cpus_count;
}