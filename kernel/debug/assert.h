#pragma once


#ifndef NDEBUG
void __assert(const char* __restrict__ expression, 
              const char* __restrict__ file,
              const int                line);
             
#define assert(EX) (void)((EX) || (__assert (#EX, __FILE__, __LINE__),0))
#else 
#define assert(EX)
#endif


#define static_assert(EX) _Static_assert(EX, \
                    "static assert failed")

