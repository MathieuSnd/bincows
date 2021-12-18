#include "pic.h"
#include "idt.h"

#include "../lib/registers.h"
#include "../lib/assert.h"
#include "../lib/logging.h"


#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */
 
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */
#define PIC_EOI		0x20		/* End-of-interrupt command code */
#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
 
void pic_eoi(unsigned char irq)
{
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
 
	outb(PIC1_COMMAND,PIC_EOI);
} 

static void io_wait(void) {
    outb(0x80,0);
}

static uint16_t mask = 0xff;

void pic_init(void) {
	log_debug("init PIC...");

	_cli();

	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  
    // starts the initialization sequence (in cascade mode)
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	outb(PIC1_DATA, 32);                 
    // ICW2: Master PIC vector offset
	outb(PIC2_DATA, 40);                 
    // ICW2: Slave PIC vector offset
	outb(PIC1_DATA, 4);                       
    // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)

	outb(PIC2_DATA, 2);                       
    // ICW3: tell Slave PIC its cascade identity (0000 0010)
 
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);
    // mask all interrupts
	outb(PIC1_DATA, 0xff); 
	outb(PIC2_DATA, 0xff); 


	mask = 0xff;
	_sti();

}

void pic_mask_irq(unsigned number, int do_mask) {
    assert(number < 16);

    if(do_mask) 
        mask |= 1 << number;
    else
        mask &= ~(1 << number);


    outb(PIC1_DATA, mask & 0xff);
    outb(PIC2_DATA, mask >> 8);
}
