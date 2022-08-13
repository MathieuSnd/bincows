#pragma once

#include "idt.h"

struct driver;

#define IRQ_BEGIN 32
#define IRQ_END   48


typedef void(* irq_handler_t)(struct driver *);

// the error_code field is reserved for 
// exceptions without error codes
typedef void(* exc_handler_t)(uint32_t erorr_code);


// prefere using install_irq
void     register_irq(unsigned irq_number,
                      irq_handler_t  handler, 
                      struct driver* driver);


// register an exception handler
unsigned register_exception_handler(
    unsigned exception_number,
    exc_handler_t handler
);


unsigned install_irq(irq_handler_t  handler, 
                     struct driver* driver);
void     release_irq(unsigned n);