#ifndef REGS_H
#define REGS_H

#include <stdint.h>

uint64_t _ds(void);
uint64_t _ss(void);
uint64_t _cs(void);
uint64_t _es(void);
uint64_t _fs(void);
uint64_t _gs(void);

#endif//REGS_H