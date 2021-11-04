#include "../klib/sprintf.h"
#include "../debug/assert.h"
#include "../debug/panic.h"
#include "idt.h"





static void print_frame(char* buff, struct IFrame* interrupt_frame) {
    sprintf(buff,
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


static void panic_handler(const char* name,struct IFrame* interrupt_frame) {
    char buff1[256], buff2[512];
    print_frame(buff1, interrupt_frame);
    sprintf(buff2, "%s: \n%s", name, buff1);
    
    panic(buff2);
    __builtin_unreachable();
}

__attribute__((interrupt)) void ISR_general_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_general_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_error_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_error_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_div_by_zero_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_div_by_zero_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_debug_handler(struct IFrame* interrupt_frame) {
    
    panic_handler("ISR_debug_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_NMI_handler(struct IFrame* interrupt_frame) {
    
    panic_handler("ISR_NMI_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_breakpoint_handler(struct IFrame* interrupt_frame) {
    
    panic_handler("ISR_breakpoint_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_overflow_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_overflow_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_bound_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_bound_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_invalid_opcode_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_invalid_opcode_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_device_not_available_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_device_not_available_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_double_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_double_fault_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_coproc_segment_overrun_handler(struct IFrame* interrupt_frame) {
    panic_handler("ISR_coproc_segment_overrun_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_invalid_TSS_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_invalid_TSS_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_segment_not_present_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_segment_not_present_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_stack_segment_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_stack_segment_fault_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_general_protection_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_general_protection_fault_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_page_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    (void) error_code;
    panic_handler("ISR_page_fault_handler", interrupt_frame);
}
__attribute__((interrupt)) void ISR_spurious(struct IFrame* interrupt_frame) {
    panic_handler("ISR_spurious", interrupt_frame);
}

extern uint64_t idt[256];


void setup_isr(void) {
    _cli();

    set_irs_handler(0,  (void *) ISR_div_by_zero_handler);
    set_irs_handler(1,  (void *) ISR_debug_handler);
    set_irs_handler(2,  (void *) ISR_NMI_handler);
    set_irs_handler(3,  (void *) ISR_breakpoint_handler);
    set_irs_handler(4,  (void *) ISR_overflow_handler);
    set_irs_handler(5,  (void *) ISR_bound_handler);
    set_irs_handler(6,  (void *) ISR_invalid_opcode_handler);
    set_irs_handler(7,  (void *) ISR_device_not_available_handler);
    set_irs_handler(8,  (void *) ISR_double_fault_handler);
    set_irs_handler(9,  (void *) ISR_coproc_segment_overrun_handler);
    set_irs_handler(10, (void *) ISR_invalid_TSS_handler);
    set_irs_handler(11, (void *) ISR_segment_not_present_handler);
    set_irs_handler(12, (void *) ISR_stack_segment_fault_handler);
    set_irs_handler(13, (void *) ISR_general_protection_fault_handler);
    set_irs_handler(14, (void *) ISR_page_fault_handler);
// >= 32 &&  < 38

    for(int i = 15; i <= 0xff; i++)
        set_irs_handler(i, ISR_spurious);

    set_irs_handler(32, ISR_coproc_segment_overrun_handler);
    //for(int i = d; i <= 0xff; i++)
    //    set_irs_handler(i, ISR_coproc_segment_overrun_handler);

    setup_idt();
    _sti();
}