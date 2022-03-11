/**
 * @file tb.c
 * 
 * this file contains testbench implementations
 * for kernel functions
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <drivers/storage_interface.h>


unsigned heap_get_n_allocation(void) {
    return 0;
}

void ps2_trigger_CPU_reset() {
    printf("CPU RESET");
    exit(0);
}

void* get_active_terminal() {
    return NULL;
}

void terminal_set_fgcolor(void) {
    
}
void terminal_set_bgcolor(void) {
    
}
void terminal_set_colors(void) {

}

size_t heap_get_size() {
    return 0;
}

size_t heap_get_brk(void) {
    return 0;
}
void heap_defragment(void) {

}
void heap_print(void) {

}

size_t stack_size = 4096;
void* stack_base = NULL;

void* _rbp(void) {
    return __builtin_return_address(0);
}

void ps2kb_poll_wait_for_key(uint8_t key) {
    (void) key;

    getchar();
}



