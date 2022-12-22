#ifndef SIMD_H
#define SIMD_H

#include <stdint.h>

int xstate_enable(void);

/*
sse_enable, avx_enable, avx512_enable must be called in order
*/
int sse_enable(void);
int avx_enable(void);
int avx512_enable(void);

/*
x86_support_xsave, x86_support_avx, x86_support_avx512 
must be called in order
*/
int x86_support_xsave(void);
int x86_support_avx(void);
int x86_support_avx512(void);


// xstate region structure
struct xstate {
    uint8_t reserved[0];
} __attribute__((aligned((64)))); 

struct xstate_buffer {
    struct xstate* begin;
    uint8_t buffer[0];
};



#define XSAVE_X87 1
#define XSAVE_SSE 2
#define XSAVE_AVX 4
#define XSAVE_AVX512 8
#define XSAVE_ALL 15

void xsave (int what, struct xstate_buffer* to);
void xrstor(int what, struct xstate_buffer* from);

struct xstate_buffer* xstate_create(void);
void xstate_free(struct xstate_buffer* buf);

#endif