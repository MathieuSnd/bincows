#include "../lib/sprintf.h"
#include "../lib/assert.h"
#include "../lib/panic.h"
#include "idt.h"

// return the value of cr2
extern uint64_t _cr2(void);


static void print_frame(char* buff, struct IFrame* interrupt_frame) {
    sprintf(buff,
        "RIP: %16lx\n"
        "RSP: %16lx\n"
        "CS:  %4lx   SS:  %4lx\n"
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


static void panic_handler_code(
        const char* name,
        struct IFrame* interrupt_frame,
        uint64_t code        
) {
    char buff1[256], buff2[512];
    print_frame(buff1, interrupt_frame);
    sprintf(buff2, 
        "%s, code=%lx: \n%s", 
        name, 
        code,
        buff1);
    
    panic(buff2);
    __builtin_unreachable();
}


#define DECLARE_EXCEPTION_HANDLER(NAME) \
__attribute__((interrupt))\
void NAME(struct IFrame* ifr) {\
    panic_handler(#NAME, ifr);\
}


#define DECLARE_CODE_EXCEPTION_HANDLER(NAME) \
__attribute__((interrupt))\
void NAME(struct IFrame* ifr, uint64_t code) {\
    panic_handler_code(#NAME, ifr, code);\
}


DECLARE_EXCEPTION_HANDLER(ISR_general_handler)
DECLARE_EXCEPTION_HANDLER(ISR_div_by_zero_handler)
DECLARE_EXCEPTION_HANDLER(ISR_debug_handler)
DECLARE_EXCEPTION_HANDLER(ISR_NMI_handler)
DECLARE_EXCEPTION_HANDLER(ISR_breakpoint_handler)
DECLARE_EXCEPTION_HANDLER(ISR_overflow_handler)
DECLARE_EXCEPTION_HANDLER(ISR_bound_handler)
DECLARE_EXCEPTION_HANDLER(ISR_invalid_opcode_handler)
DECLARE_EXCEPTION_HANDLER(ISR_device_not_available_handler)
DECLARE_EXCEPTION_HANDLER(ISR_coproc_segment_overrun_handler)
DECLARE_EXCEPTION_HANDLER(ISR_LAPIC_spurious)


DECLARE_CODE_EXCEPTION_HANDLER(ISR_error_handler)
DECLARE_CODE_EXCEPTION_HANDLER(ISR_double_fault_handler)
DECLARE_CODE_EXCEPTION_HANDLER(ISR_invalid_TSS_handler)
DECLARE_CODE_EXCEPTION_HANDLER(ISR_segment_not_present_handler)
DECLARE_CODE_EXCEPTION_HANDLER(ISR_stack_segment_fault_handler)
DECLARE_CODE_EXCEPTION_HANDLER(ISR_general_protection_fault_handler)


__attribute__((interrupt)) void ISR_page_fault_handler(struct IFrame* interrupt_frame, uint64_t error_code) {
    char buff[128];
    //for(;;);
    // the content of cr2 is the illegal address
    sprintf(buff, "PAGE FAULT. illegal address: %16lx, error code %x\n", _cr2(), error_code);
    panic_handler(buff, interrupt_frame);
}





void setup_isrs(void) {
// cli just in case the idt was already 
// initialized
    _cli();

    set_irs_handler(0,  ISR_div_by_zero_handler);
    set_irs_handler(1,  ISR_debug_handler);
    set_irs_handler(2,  ISR_NMI_handler);
    set_irs_handler(3,  ISR_breakpoint_handler);
    set_irs_handler(4,  ISR_overflow_handler);
    set_irs_handler(5,  ISR_bound_handler);
    set_irs_handler(6,  ISR_invalid_opcode_handler);
    set_irs_handler(7,  ISR_device_not_available_handler);
    set_irs_handler(8,  ISR_double_fault_handler);
    set_irs_handler(9,  ISR_coproc_segment_overrun_handler);
    set_irs_handler(10, ISR_invalid_TSS_handler);
    set_irs_handler(11, ISR_segment_not_present_handler);
    set_irs_handler(12, ISR_stack_segment_fault_handler);
    set_irs_handler(13, ISR_general_protection_fault_handler);
    set_irs_handler(14, ISR_page_fault_handler);


    for(int i = 15; i < 256; i++)
        set_irs_handler(i,ISR_LAPIC_spurious);


    set_irs_handler(32, ISR_coproc_segment_overrun_handler);
    setup_idt();
    _sti();
}