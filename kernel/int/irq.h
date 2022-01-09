#pragma once

#include "idt.h"


typedef void(* irq_handler_t)(void);

unsigned install_irq(irq_handler_t* handler);
void     release_irq(unsigned n);