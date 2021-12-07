#include <stddef.h>
#include <stdint.h>

#include "../lib/common.h"
#include "idt.h"



typedef struct {
    uint16_t    size;
    const void* offset;
} __packed IDTD;


typedef struct {
    uint8_t gate_type: 4;
    uint8_t z        : 1;
    uint8_t dpl      : 2;
    uint8_t p        : 1;
} __packed type_attr_t;


static_assert(sizeof(type_attr_t) == 1);


typedef struct {
   uint16_t    offset_1;  // offset bits 0..15
   uint16_t    selector;  // a code segment selector in GDT or LDT
   uint8_t     ist;       // bits 0..2 holds Interrupt
                          // Stack Table offset, rest of bits zero.

   type_attr_t type_attr; // type and attributes
   uint16_t    offset_2;  // offset bits 16..31
   uint32_t    offset_3;  // offset bits 32..63
   uint32_t    zero;      // reserved
} __packed IDTE;



IDTE idt[256] = {0};

void _lidt(IDTD* idtd);

void setup_idt(void) {
    IDTD idt_descriptor = {
        .size   = 255 * sizeof(IDTE),
        .offset = idt,
    };

    _lidt(&idt_descriptor);
}


static type_attr_t make_type_attr_t(uint8_t gate_type) {
    
    assert((gate_type & 0xf0) == 0);
    
    return (type_attr_t) {
        .gate_type = gate_type,
        .z         = 0,
        .dpl       = 0,
        .p         = 1,
    };
}


static IDTE make_idte(void* handler, type_attr_t type_attr) {
    uint64_t h = (uint64_t) handler;

    return (IDTE) {
        .offset_1  = h & 0xffff,
        .selector  = 0x08, // kernel code segment
        .ist       = 0,
        .type_attr = type_attr,
        .offset_2  = (h >> 16) & 0xffff,
        .offset_3  = (h >> 32) & 0xffffffff,
        .zero      = 0,
    };
}

static IDTE make_isr(void* handler) {
    return make_idte(handler, make_type_attr_t(ATTR_64_GATE));
}
static IDTE make_irq(void* handler) {
    return make_idte(handler, make_type_attr_t(ATTR_64_TRAP));
}


void set_rflags(uint64_t RFLAGS);
uint64_t get_rflags(void);


void set_irs_handler(uint16_t number, void* handler) {
// make sure to disable interrupts
// while updating the idt
// then save IF to its old state
    uint64_t rflags = get_rflags();
    _cli();

    idt[number] = make_isr(handler);

    set_rflags(rflags);
}


void set_irq_handler(uint16_t number, void* handler) {
// same as above
    uint64_t rflags = get_rflags();
    _cli();

    idt[number] = make_irq(handler);

    set_rflags(rflags);
}


void _cli(void) {
    asm volatile("cli");
}


void _sti(void) {
    asm volatile("sti");
}
