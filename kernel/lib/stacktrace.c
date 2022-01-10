#include <stdint.h>

#include "../memory/vmap.h"
#include "../lib/assert.h"
#include "../lib/dump.h"
#include "../lib/logging.h"
#include "sprintf.h"

#define MAX_STACK_TRACE 40

extern uint64_t _rbp(void);

typedef struct {
    uint64_t addr;
    char     name[56];
} sym_t;

static const struct  {
    uint64_t n_symbols; 
    sym_t    symbols[];
}* file = NULL;

void stacktrace_file(const void* f) {
    file = f;
} 

// offset: output offset
// addr = addr(symbol) + offset
// not found -> return NULL
static const char* get_symbol_name(
                        uint64_t  addr, 
                        unsigned* offset
) {
    if(!file)
        return NULL;

    // dichotomy
    uint64_t A = 0;
    uint64_t B = file->n_symbols - 1;

    uint64_t addrA = file->symbols[0].addr;
    uint64_t addrB = file->symbols[file->n_symbols - 1].addr;
    
    if(addr < addrA)
        return NULL;
    if(addr >= addrB) {
        *offset = addr - file->symbols[file->n_symbols - 1].addr;

        return file->symbols[file->n_symbols - 1].addr;
    }

    while(A+1 != B) {
        uint64_t C     = (A+B) >> 1;
        uint64_t addrC = file->symbols[C].addr;

        if(addr < addrC) {
            B     = C;
            addrB = addrC;
        }
        else {
            A     = C;
            addrA = addrC;
        }       
    }


    *offset = addr - addrA;
    return file->symbols[A].name;
}


void stacktrace_print(void) {
    void** ptr = (void**)_rbp();
    puts("backtrace:\n");
    
    for(unsigned i = 0; i < MAX_STACK_TRACE; i++) {
        
        if(*ptr == 0) // reached the top
            break;
        puts("   ");
        
        int interrupt_routine = 0;
        
        void* rip = *(ptr+1);
        if(!is_kernel_memory((uint64_t)rip)) {
            //maybe it is an exception error code
            interrupt_routine = 1;
            rip = *(ptr+2);
            
        }
        printf("%llx    ", rip);

        unsigned* offset;
        const char* symbol  = get_symbol_name(rip, &offset);

        if(symbol)
            printf("<%s + %x>", symbol, offset);
        else
            printf("<??>");

        
        if(!interrupt_routine)
            puts("\n");
        else
            puts(" - INTERRUPT ROUTINE\n");

        ptr = *ptr;
    }
}

