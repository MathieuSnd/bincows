#include "time.h"
#include "../int/apic.h"

void sleep(unsigned ms) {
    uint64_t begin = clock();

    while (clock() < begin + ms)
        asm volatile("hlt");
}