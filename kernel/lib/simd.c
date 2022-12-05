#include "simd.h"
#include "registers.h"
#include "../int/idt.h"

#define CR0_MP (1 << 1)
#define CR0_EM (1 << 2)

#define CR4_OSFXSR (1 << 9)
#define CR4_OSXMMEXCPT (1 << 10)

void enable_sse(void) {
    uint64_t rf = get_rflags();
    _cli();

    // CR0: EM = 0, MP = 1

    uint64_t cr0 = get_cr0();
    cr0 = (cr0 & ~CR0_EM) | CR0_MP; 
    set_cr0(cr0);

    // CR4: CR4_OSFXSR = 1, CR4_OSXMMEXCPT = 1

    uint64_t cr4 = get_cr4();
    cr4 = cr4 | CR4_OSFXSR | CR4_OSXMMEXCPT;
    set_cr4(cr4);

    
    set_rflags(rf);


    
    uint8_t base[512] __attribute__((aligned(16)));
    uint8_t* ahi = base;
    asm volatile("fxsave %0 "::"m"(*ahi));

}
