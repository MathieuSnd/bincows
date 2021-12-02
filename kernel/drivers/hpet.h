#pragma once

void hpet_init(void);
void hpet_prepare_wait_ms(unsigned);
void hpet_wait(void);
void hpet_disable(void);