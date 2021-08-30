#include "../klib/sprintf.h"
#include "assert.h"
#include "panic.h"


void __assert(const char* __restrict__ expression, 
              const char* __restrict__ file,
              const int                line) {
    char buffer[1024];

    sprintf("assertion '%s' failed at: %s:%d", expression, file, line);
    
    panic(buffer);
}