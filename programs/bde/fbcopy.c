/*
    this file contains different functions to copy
    changes of the double buf to the framebuffer
*/
#include "fbcopy.h"
#include <assert.h>
#include <x86intrin.h>
#include <SDL.h>

static
void avx512(const void* buf0, 
            const void* buf1, 
                  void* hw_buf, 
                  size_t count) 
{
    assert((count % 64) == 0);
    __m512i* buf0_512 = __builtin_assume_aligned(buf0, 64);
    __m512i* buf1_512 = __builtin_assume_aligned(buf1, 64);
    __m512i* dest_512 = __builtin_assume_aligned(hw_buf, 64);


    __m512i A, B, C;

    while(count) {
        // load src buffs
        A = _mm512_loadu_si512(buf0_512);
        B = _mm512_loadu_si512(buf1_512);

        // compare
        //if(!_mm512_cmpeq_epi64_mask(A, B)) {
            // A != B. NT store A to dest
            _mm512_stream_si512(dest_512, A);

            // update buf1
            _mm512_store_si512(buf1_512, A);
        //}

        count -= 64;
        buf0_512++;
        buf1_512++;
    }
}


fbcopy_f get_fbcopy_f(void) {
    if(SDL_HasAVX512F())
        return avx512;
    else
        assert(0);
}
