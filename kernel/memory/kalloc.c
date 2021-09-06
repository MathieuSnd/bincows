#include <stdint.h>
#include <stddef.h>

#include "kalloc.h"
#include "../debug/assert.h"
#include "../common.h"


#define HEAP_SIZE_MB 16
#define HEAP_SIZE HEAP_SIZE_MB * 1024 * 1024

static uint8_t heap[HEAP_SIZE] __attribute__((section(".bss")));

static void* brk = (void *) heap;


void* kmalloc(size_t size) {
    void* ptr = brk;
    brk = mallign16(brk+size);

    assert((size_t)brk - (size_t)heap < HEAP_SIZE);

    return ptr;
}

void kfree(void* ptr) {
    (void) ptr;
}
