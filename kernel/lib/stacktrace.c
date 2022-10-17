#include <stdint.h>

#include "../memory/vmap.h"
#include "../lib/assert.h"
#include "../lib/dump.h"
#include "../lib/logging.h"
#include "sprintf.h"
#include "panic.h"

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


static
int check_address(uint64_t addr) {
    return
             // check cannonical form
        !(
            (addr & 0xfffff80000000000) == 0xfffff80000000000
         || (addr & 0xfffff80000000000) == 0x0000000000000000 
        )
            // check null pointer (1st page)
         || (addr & 0xfffffffffffff000) == 0x0000000000000000;
}

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

    if(((uint64_t)(file) & 0xffff000000000000) != 0xffff000000000000) {
        panic("broken up memory");
    }

    // binary search
    uint64_t A = 0;
    uint64_t B = file->n_symbols - 1;

    uint64_t addrA = file->symbols[0].addr;
    uint64_t addrB = file->symbols[file->n_symbols - 1].addr;
    
    if(addr < addrA)
        return NULL;
    if(addr >= addrB) {
        *offset = addr - file->symbols[file->n_symbols - 1].addr;

        return (void*)file->symbols[file->n_symbols - 1].addr;
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
        
        if(check_address((uint64_t)ptr)) {
            printf("corrupted stack\n");
            break;
        }


        if(*ptr == 0) // reached the top 
        {
            printf("   trace end\n");
            break;
        }

        
        int interrupt_routine = 0;

        if(check_address((uint64_t)(ptr + 1))) {
            printf("   invalid address\n");
            return;
        }

        
        uint64_t rip = (uint64_t) *(ptr+1);

        if(check_address(rip)) {
            printf("   %16llx BAD RIP\n", rip);
            return;
        }


        printf("   %16llx    ", rip);

        if(is_user((void*)rip)) {
            printf("userspace\n");
            break;
        }

        unsigned offset;
        const char* symbol  = get_symbol_name(rip-1, &offset);

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

