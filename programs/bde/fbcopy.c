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
        __m512i xor = _mm512_xor_si512(A, B);
        
        if(_mm512_test_epi64_mask(xor, xor)) {
            // A != B. NT store A to dest
            _mm512_stream_si512(dest_512, A);

            // update buf1
            _mm512_store_si512(buf1_512, A);
        }

        count -= 64;
        buf0_512++;
        buf1_512++;
        dest_512++;
    }
}

static
void avx256(const void* buf0, 
            const void* buf1, 
                  void* hw_buf, 
                  size_t count) 
{
    assert((count % 32) == 0);
    __m256i* buf0_256 = __builtin_assume_aligned(buf0, 32);
    __m256i* buf1_256 = __builtin_assume_aligned(buf1, 32);
    __m256i* dest_256 = __builtin_assume_aligned(hw_buf, 32);


    __m256i A, B, C;

    while(count) {
        // load src buffs
        A = _mm256_loadu_si256(buf0_256);
        B = _mm256_loadu_si256(buf1_256);

        // compare
        __m256i xor = _mm256_xor_si256(A, B);
        
        if(_mm256_test_epi64_mask(xor, xor)) {
            // A != B. NT store A to dest
            _mm256_stream_si256(dest_256, A);

            // update buf1
            _mm256_store_si256(buf1_256, A);
        }

        count -= 32;
        buf0_256++;
        buf1_256++;
        dest_256++;
    }
}


static
void sse128(const void* buf0, 
            const void* buf1, 
                  void* hw_buf, 
                  size_t count) 
{
    assert((count % 16) == 0);
    __m128i* buf0_128 = __builtin_assume_aligned(buf0,   16);
    __m128i* buf1_128 = __builtin_assume_aligned(buf1,   16);
    __m128i* dest_128 = __builtin_assume_aligned(hw_buf, 16);


    __m128i A, B, C;

    while(count) {
        // load src buffs
        A = _mm_load_si128(buf0_128);
        B = _mm_loadu_si128(buf1_128);

        // compare
        __m128i xor = _mm_xor_si128(A, B);
        
        if(_mm_testz_si128(xor, xor)) {
            // A != B. NT store A to dest
            _mm_stream_si128(dest_128, A);

            // update buf1
            _mm_store_si128(buf1_128, A);
        }

        count -= 16;
        buf0_128++;
        buf1_128++;
        dest_128++;
    }
}




fbcopy_f get_fbcopy_f(void) {
    if(SDL_HasAVX512F())
        return avx512;
    else if(SDL_HasAVX2())
        return avx256;
    else if(SDL_HasSSE2())
        return sse128;
    else
        assert(0);
}
