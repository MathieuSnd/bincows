#include "../lib/sprintf.h"
#include "../lib/assert.h"
#include "../lib/panic.h"
#include "idt.h"
#include "irq.h"

#include "../sched/sched.h"
#include "../sched/thread.h"
#include "../sched/signal/signal.h"

// for block cache page faults
#include "../drivers/block_cache.h"

// return the value of cr2
extern uint64_t _cr2(void);


static void print_frame(char* buff, gp_regs_t* regs) {
    sprintf(buff,
        "RIP: %16lx\n"
        "RSP: %16lx\n"
        "CS:  %4lx   SS:  %4lx\n"
        "\n"
        "RFLAGS: %8lx\n",
        regs->rip,    
        regs->rsp,    
        regs->cs,    
        regs->ss,
        regs->rflags);
}


static void __attribute__((noreturn)) panic_handler(const char* name) {

    gp_regs_t* regs = sched_saved_context();
    char buff1[256], buff2[512];

    print_frame(buff1, regs);
    sprintf(buff2, "%s: \n%s", name, buff1);
    
    panic(buff2);
}


static void panic_handler_code(
        const char* name,
        uint64_t code        
) {
    char buff1[256], buff2[512];

    gp_regs_t* regs = sched_saved_context();
    print_frame(buff1, regs);

    sprintf(buff2, 
        "%s, code=%lx: \n%s", 
        name, 
        code,
        buff1);
    
    panic(buff2);
    __builtin_unreachable();
}


#define DECLARE_EXCEPTION_HANDLER(IRQN, NAME) \
{\
    void NAME(uint32_t code) {\
        (void) code;\
        panic_handler(#NAME);\
        __builtin_unreachable();\
    };\
    register_exception_handler(IRQN, NAME);\
}

#define DECLARE_CODE_EXCEPTION_HANDLER(IRQN, NAME) \
{\
    void NAME(uint32_t code) {\
        panic_handler_code(#NAME, code);\
        __builtin_unreachable();\
    };\
    register_exception_handler(IRQN, NAME);\
}


#define DECLARE_int_HANDLER(H) extern void H(void*);



// those handlers are declared in irq.s
DECLARE_int_HANDLER(_int_handler0)
DECLARE_int_HANDLER(_int_handler1)
DECLARE_int_HANDLER(_int_handler2)
DECLARE_int_HANDLER(_int_handler3)
DECLARE_int_HANDLER(_int_handler4)
DECLARE_int_HANDLER(_int_handler5)
DECLARE_int_HANDLER(_int_handler6)
DECLARE_int_HANDLER(_int_handler7)
DECLARE_int_HANDLER(_int_handler8)
DECLARE_int_HANDLER(_int_handler9)
DECLARE_int_HANDLER(_int_handler10)
DECLARE_int_HANDLER(_int_handler11)
DECLARE_int_HANDLER(_int_handler12)
DECLARE_int_HANDLER(_int_handler13)
DECLARE_int_HANDLER(_int_handler14)
DECLARE_int_HANDLER(_int_handler15)
DECLARE_int_HANDLER(_int_handler16)
DECLARE_int_HANDLER(_int_handler17)
DECLARE_int_HANDLER(_int_handler18)
DECLARE_int_HANDLER(_int_handler19)
DECLARE_int_HANDLER(_int_handler20)
DECLARE_int_HANDLER(_int_handler21)
DECLARE_int_HANDLER(_int_handler22)
DECLARE_int_HANDLER(_int_handler23)
DECLARE_int_HANDLER(_int_handler24)
DECLARE_int_HANDLER(_int_handler25)
DECLARE_int_HANDLER(_int_handler26)
DECLARE_int_HANDLER(_int_handler27)
DECLARE_int_HANDLER(_int_handler28)
DECLARE_int_HANDLER(_int_handler29)
DECLARE_int_HANDLER(_int_handler30)
DECLARE_int_HANDLER(_int_handler31)
DECLARE_int_HANDLER(_int_handler32)
DECLARE_int_HANDLER(_int_handler33)
DECLARE_int_HANDLER(_int_handler34)
DECLARE_int_HANDLER(_int_handler35)
DECLARE_int_HANDLER(_int_handler36)
DECLARE_int_HANDLER(_int_handler37)
DECLARE_int_HANDLER(_int_handler38)
DECLARE_int_HANDLER(_int_handler39)
DECLARE_int_HANDLER(_int_handler40)
DECLARE_int_HANDLER(_int_handler41)
DECLARE_int_HANDLER(_int_handler42)
DECLARE_int_HANDLER(_int_handler43)
DECLARE_int_HANDLER(_int_handler44)
DECLARE_int_HANDLER(_int_handler45)
DECLARE_int_HANDLER(_int_handler46)
DECLARE_int_HANDLER(_int_handler47)
DECLARE_int_HANDLER(_int_handler48)

#include "../lib/logging.h"
static
void ISR_page_fault_handler(uint32_t error_code) {

    uint64_t pf_address = _cr2();

    if((error_code & 0x17) == 0x00) {
        // if the page is not present,
        // the page fault happened in kernel space
        // and not because of an instruction fetch

        if(is_block_cache(pf_address)) {
            // let the block cache handle the page fault
            block_cache_page_fault(pf_address);
            return;
        }
    }
    // this is an error, panic
    char buff[128];

    // the content of cr2 is the illegal address

    // user mode page fault
    if(error_code & 0x4) {
        log_warn("page fault in user mode at address %lx, error code %lx. PID = %u:%u",
                pf_address, error_code, sched_current_pid(), sched_current_tid());
        stacktrace_print(); 
        process_trigger_signal(sched_current_pid(), SIGSEGV);

        schedule();

    }
    else {
        sprintf(buff, "PAGE FAULT. illegal address: %16lx, error code %x\n", pf_address, error_code);
        panic_handler(buff);
        __builtin_unreachable();
    }


}


// not static
__attribute__((interrupt)) 
void IRQ_dummy_handler(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    panic("Uregistered irq number fired\n");
    __builtin_unreachable();
}


// not static
__attribute__((interrupt)) 
void IRQ_LAPIC_spurious(struct IFrame* interrupt_frame) {
    (void) interrupt_frame;
    panic("LAPIC spurious irq\n");
    __builtin_unreachable();
}


void setup_isrs(void) {
// cli just in case the idt was already 
// initialized
    _cli();

    set_irq_handler(1,  _int_handler1);
    set_irq_handler(2,  _int_handler2);
    set_irq_handler(3,  _int_handler3);
    set_irq_handler(4,  _int_handler4);
    set_irq_handler(5,  _int_handler5);
    set_irq_handler(6,  _int_handler6);
    set_irq_handler(7,  _int_handler7);
    set_irq_handler(8,  _int_handler8);
    set_irq_handler(9,  _int_handler9);
    set_irq_handler(10, _int_handler10);
    set_irq_handler(11, _int_handler11);
    set_irq_handler(12, _int_handler12);
    set_irq_handler(13, _int_handler13);
    set_irq_handler(14, _int_handler14);
    set_irq_handler(15, _int_handler15);
    set_irq_handler(16, _int_handler16);
    set_irq_handler(17, _int_handler17);
    set_irq_handler(18, _int_handler18);
    set_irq_handler(19, _int_handler19);
    set_irq_handler(20, _int_handler20);
    set_irq_handler(21, _int_handler21);
    set_irq_handler(22, _int_handler22);
    set_irq_handler(23, _int_handler23);
    set_irq_handler(24, _int_handler24);
    set_irq_handler(25, _int_handler25);
    set_irq_handler(26, _int_handler26);
    set_irq_handler(27, _int_handler27);
    set_irq_handler(28, _int_handler28);
    set_irq_handler(29, _int_handler29);
    set_irq_handler(30, _int_handler30);
    set_irq_handler(31, _int_handler31);

    DECLARE_EXCEPTION_HANDLER(0,       ISR_div_by_zero_handler);
    DECLARE_EXCEPTION_HANDLER(1,       ISR_debug_handler);
    DECLARE_EXCEPTION_HANDLER(2,       ISR_NMI_handler);
    DECLARE_EXCEPTION_HANDLER(3,       ISR_breakpoint_handler);
    DECLARE_EXCEPTION_HANDLER(4,       ISR_overflow_handler);
    DECLARE_EXCEPTION_HANDLER(5,       ISR_bound_handler);
    DECLARE_EXCEPTION_HANDLER(6,       ISR_invalid_opcode_handler);
    DECLARE_EXCEPTION_HANDLER(7,       ISR_device_not_available_handler);
    DECLARE_EXCEPTION_HANDLER(9,       ISR_coproc_segment_overrun_handler);
    DECLARE_EXCEPTION_HANDLER(16,      ISR_x87_floating_point_exception_handler);
    DECLARE_EXCEPTION_HANDLER(18,      ISR_machine_check_handler);
    DECLARE_EXCEPTION_HANDLER(19,      ISR_simd_floating_point_exception_handler);
    DECLARE_EXCEPTION_HANDLER(28,      ISR_hypervisor_injection_exception_handler);
    DECLARE_EXCEPTION_HANDLER(20,      ISR_virtualization_exception_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(8,  ISR_double_fault_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(10, ISR_invalid_TSS_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(11, ISR_segment_not_present_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(12, ISR_stack_segment_fault_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(13, ISR_general_protection_fault_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(17, ISR_alignment_check_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(21, ISR_control_protection_exception_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(29, ISR_vmm_communication_exception_handler);
    DECLARE_CODE_EXCEPTION_HANDLER(30, ISR_security_exception_handler);
    register_exception_handler(14, ISR_page_fault_handler);

// IRQs 
    set_irq_handler(32, _int_handler32);
    set_irq_handler(33, _int_handler33);
    set_irq_handler(34, _int_handler34);
    set_irq_handler(35, _int_handler35);
    set_irq_handler(36, _int_handler36);
    set_irq_handler(37, _int_handler37);
    set_irq_handler(38, _int_handler38);
    set_irq_handler(39, _int_handler39);
    set_irq_handler(40, _int_handler40);
    set_irq_handler(41, _int_handler41);
    set_irq_handler(42, _int_handler42);
    set_irq_handler(43, _int_handler43);
    set_irq_handler(44, _int_handler44);
    set_irq_handler(45, _int_handler45);
    set_irq_handler(46, _int_handler46);
    
    // yield interrupt: available from userspace
    set_irs_handler(47, _int_handler47, 3);
    
    // timer IRQ
    set_irq_handler(48, _int_handler48);

    for(int i = 49; i <= 254; i++)
        set_irq_handler(i, IRQ_dummy_handler);

    set_irq_handler(255, IRQ_LAPIC_spurious);


    setup_idt();
    _sti();
}
