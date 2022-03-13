#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stddef.h>
#include "../lib/assert.h"

#define ATTR_64_GATE 0b1110
#define ATTR_64_TRAP 0b1110



struct IFrame {
    uint64_t RIP;
    uint64_t CS;
    uint64_t RFLAGS;
    uint64_t RSP;
    uint64_t SS;
} __attribute__((packed));

static_assert(sizeof(struct IFrame) == 40);



void setup_idt(void);

void set_irs_handler(uint16_t number, void* handler);
void set_irq_handler(uint16_t number, void* handler);
void _cli(void);
void _sti(void);


void setup_isrs(void);


#endif // IDT_H