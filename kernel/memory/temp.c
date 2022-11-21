#include "temp.h"
#include "paging.h"
#include "vmap.h"

#include "../int/apic.h"
#include "../lib/registers.h"
#include "../lib/assert.h"

static void* cpu_temp_addr(void) {
    uint64_t addr = KERNEL_TEMP_BEGIN | (get_lapic_id() * TEMP_SIZE);
    assert(is_kernel_temp(addr));

    return (void*)addr;
}


void* temp_lock(void) {
    assert(!interrupt_enable());
    return cpu_temp_addr();
}

uint64_t temp_pd(void) {
    assert(!interrupt_enable());
    return get_master_pd(cpu_temp_addr);
}

void temp_release(void) {
    assert(!interrupt_enable());
}
