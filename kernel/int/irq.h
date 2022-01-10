#pragma once

#include "idt.h"

struct driver;

#define IRQ_BEGIN 32
#define IRQ_END   47

typedef void(* irq_handler_t)(struct driver *);

// prefere using install_irq
void     register_irq(unsigned irq_number,
                      irq_handler_t  handler, 
                      struct driver* driver);


unsigned install_irq(irq_handler_t  handler, 
                     struct driver* driver);
void     release_irq(unsigned n);