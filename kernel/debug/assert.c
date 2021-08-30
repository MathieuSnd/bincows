#include "../klib/sprintf.h"
#include "assert.h"
#include "panic.h"


void __assert(const char* __restrict__ expression, 
              const char* __restrict__ file,
              const int                line) {
    char buffer[1024];

    sprintf(buffer, "%s:%d: assertion '%s' failed", file, line, expression);
    
    panic(buffer);
}