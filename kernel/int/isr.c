#include "../klib/sprintf.h"
#include "../debug/assert.h"
#include "idt.h"






static void print_frame(struct IFrame* interrupt_frame) {
    kprintf(
        "RIP: %16lx\n"
        "RSP: %16lx\n"
        "CS:  %16lx   SS:  %16lx\n"
        "\n"
        "RFLAGS: %8lx\n",
        interrupt_frame->RIP,    
        interrupt_frame->RSP,    
        interrupt_frame->CS,    
        interrupt_frame->SS,
        interrupt_frame->RFLAGS);
}


__attribute__((interrupt)) void ISR_general_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_general_handler()\n");
}
__attribute__((interrupt)) void ISR_error_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_error_handler()\n");
}


__attribute__((interrupt)) void ISR_div_by_zero_handler(struct IFrame* interrupt_frame) {
    kprintf("ISR_div_by_zero_handler():\n");
    print_frame(interrupt_frame);
}



__attribute__((interrupt)) void ISR_debug_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_debug_handler()\n");
}
__attribute__((interrupt)) void ISR_NMI_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_NMI_handler()\n");
}
__attribute__((interrupt)) void ISR_breakpoint_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_breakpoint_handler()\n");
}
__attribute__((interrupt)) void ISR_overflow_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_overflow_handler()\n");
}
__attribute__((interrupt)) void ISR_bound_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_bound_handler()\n");
}
__attribute__((interrupt)) void ISR_invalid_opcode_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_invalid_opcode_handler()\n");
}
__attribute__((interrupt)) void ISR_device_not_available_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_device_not_available_handler()\n");
}
__attribute__((interrupt)) void ISR_double_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_double_fault_handler()\n");
}
__attribute__((interrupt)) void ISR_coproc_segment_overrun_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_coproc_segment_overrun_handler()\n");
}
__attribute__((interrupt)) void ISR_invalid_TSS_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_invalid_TSS_handler()\n");
}
__attribute__((interrupt)) void ISR_segment_not_present_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_segment_not_present_handler()\n");
}
__attribute__((interrupt)) void ISR_stack_segment_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_stack_segment_fault_handler()\n");
}
__attribute__((interrupt)) void ISR_general_protection_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_general_protection_fault_handler()\n");
}
__attribute__((interrupt)) void ISR_page_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) interrupt_frame;
    kprintf("ISR_page_fault_handler()\n");
}
__attribute__((interrupt)) void ISR_spurious(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    kprintf("ISR_spurious()\n");
}

extern uint64_t idt[256];


void setup_isr(void) {
    _cli();

    set_interrupt_handler(0,  (void *) ISR_div_by_zero_handler);
    set_interrupt_handler(1,  (void *) ISR_debug_handler);
    set_interrupt_handler(2,  (void *) ISR_NMI_handler);
    set_interrupt_handler(3,  (void *) ISR_breakpoint_handler);
    set_interrupt_handler(4,  (void *) ISR_overflow_handler);
    set_interrupt_handler(5,  (void *) ISR_bound_handler);
    set_interrupt_handler(6,  (void *) ISR_invalid_opcode_handler);
    set_interrupt_handler(7,  (void *) ISR_device_not_available_handler);
    set_interrupt_handler(8,  (void *) ISR_double_fault_handler);
    set_interrupt_handler(9,  (void *) ISR_coproc_segment_overrun_handler);
    set_interrupt_handler(10, (void *) ISR_invalid_TSS_handler);
    set_interrupt_handler(11, (void *) ISR_segment_not_present_handler);
    set_interrupt_handler(12, (void *) ISR_stack_segment_fault_handler);
    set_interrupt_handler(13, (void *) ISR_general_protection_fault_handler);
    set_interrupt_handler(14, (void *) ISR_page_fault_handler);

    for(int i = 15; i <= 255; i++)
        set_interrupt_handler(i, ISR_spurious);

    for(size_t i = 0; i < 16; i++) {
        kprintf("entry %d:\t%16lx %16lx\n", i, idt[2*i], idt[2*i+1]);
    }

    setup_idt();
    _sti();
}