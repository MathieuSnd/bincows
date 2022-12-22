#include "simd.h"
#include "registers.h"
#include "../int/idt.h"
#include "../memory/heap.h" 
#include "logging.h" 

#include <x86intrin.h>
#include <cpuid.h>


static uint64_t xstate_support_mask;
static size_t   state_size;

static void scan_xstate_support(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx = 0;
    uint32_t edx;

    __get_cpuid_count(0x0D,0, &eax,&ebx,&ecx,&edx);
    xstate_support_mask = eax | ((uint64_t)edx << 32); 
    state_size = ecx;

    log_debug("xstate support bitmask: %lx", xstate_support_mask);
}

int x86_support_xsave(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    __get_cpuid(1,&eax,&ebx,&ecx,&edx);

    uint32_t support = ecx & bit_XSAVE;

    if(support)
        scan_xstate_support();

    return support;
}

int x86_support_avx(void) {
    return (xstate_support_mask & 0x004) == 0x004;
}

int x86_support_avx512(void) {
    return (xstate_support_mask & 0x0e0) == 0x0e0;
}



#define CR0_MP (1 << 1)
#define CR0_EM (1 << 2)

#define CR4_OSFXSR (1 << 9)
#define CR4_OSXMMEXCPT (1 << 10)

#define CR4_OSXSAVE (1 << 18) // cr4

int xstate_enable(void) {
    uint64_t rf = get_rflags();
    _cli();
    
    uint64_t cr4 = get_cr4();
    cr4 = cr4 | CR4_OSXSAVE | CR4_OSXMMEXCPT;
    set_cr4(cr4);

    set_rflags(rf);
    return 0;   
}

int sse_enable(void) {
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
    return 0;
}


/*
xcr0:
0. x87 (always 1) 
1. sse 
2. avx
3. mpx
4. mpx
5. avx512
6. avx512
7. avx512
8. PT trace 
9. PKRU
10-63. reserved.
*/
static uint64_t xcr0;

int avx_enable(void) {
    uint64_t rf = get_rflags();
    _cli();

    xcr0 = 0x007; // x87, sse, avx
    xsetbv(0, xcr0);

    set_rflags(rf);
    return 0;
}

int avx512_enable(void) {
    uint64_t rf = get_rflags();
    _cli();

    xcr0 = 0x0e7; // x87, sse, avx, avx512
    xsetbv(0, xcr0);

    assert(xgetbv(0) == xcr0);


    set_rflags(rf);
    return 0;
}



static uint64_t xsave_bitmask(int what) {
    uint64_t bitmask = 0;
    if(what & XSAVE_X87)
        bitmask &= 0x01;
    if(what & XSAVE_SSE)
        bitmask &= 0x02;
    if(what & XSAVE_AVX)
        bitmask &= 0x04;
    if(what & XSAVE_AVX512)
        bitmask &= 0xe0;

    return bitmask & xstate_support_mask;
}



void xsave(int what, struct xstate_buffer* to) {
    uint64_t mask = xsave_bitmask(what);
    xsaves((uint8_t*)to->begin, mask);
}


void xrstor(int what, struct xstate_buffer* from) {
    uint64_t mask = xsave_bitmask(what);
    xsaves((uint8_t*)from->begin, mask);
}



#include <x86intrin.h>


struct xstate_buffer* xstate_create(void) {
    assert(state_size > 0);
    
    struct xstate_buffer* buf = 
        malloc(sizeof(buf) + state_size + 63);

    uint64_t aligned = ((uint64_t)buf->buffer + 63) & ~0x3fllu;

    buf->begin = (void*)aligned;


    // now fill it with reference xstate
    // @todo use another reference
    // for now juste use the current xstate which 
    // should be valid
    xsave(XSAVE_ALL, buf);

    return buf;
}

void xstate_free(struct xstate_buffer* buf) {
    free(buf);
}



