#include <stdint.h>
#include <stddef.h>

#include "../klib/sprintf.h"
#include "kalloc.h"
#include "../debug/assert.h"
#include "../common.h"


#define HEAP_SIZE_KB 16 * 1024
#define HEAP_BEGIN 0x01000000
#define HEAP_SIZE 1024 * HEAP_SIZE_KB

//(0x00EFFFFF - HEAP_BEGIN)

//static const uint8_t* heap = (uint8_t*)0x00100000; //[HEAP_SIZE] __attribute__((section(".bss")));

static void* brk = (void *) HEAP_BEGIN;


void* kmalloc(size_t size) {
    void* ptr = brk;
    brk = mallign16(brk+size);

    kprintf("kmalloc(%lu); heap use: %u ko / %u ko\n", 
                size, ((size_t)brk - (size_t)HEAP_BEGIN) / 1024, HEAP_SIZE / 1024);
    
    assert((size_t)brk - (size_t)HEAP_BEGIN < HEAP_SIZE);

    return ptr;
}

void kfree(void* ptr) {
    (void) ptr;
}
