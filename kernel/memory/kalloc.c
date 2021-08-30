#include <stdint.h>
#include "kalloc.h"
#include "../common.h"

static void* brk = (void *) 0x00800000;


void* kmalloc(size_t size) {
    void* ptr = brk;
    brk = mallign16(brk+size);
    return ptr;
}

void kfree(void* ptr) {

}