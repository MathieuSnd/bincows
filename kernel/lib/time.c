#include "time.h"
#include "../int/apic.h"

void sleep(unsigned ms) {
    uint64_t begin = clock_ns();

    while (clock_ns() < begin + (uint64_t)ms * 1000000)
        asm volatile("hlt");
}