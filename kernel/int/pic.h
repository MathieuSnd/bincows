#pragma once

void pic_init(void);

// mask = 1 => irq masked
// mask = 0 => irq unmasked
void pic_mask_irq(unsigned number, int mask);
void pic_eoi(unsigned char irq);
