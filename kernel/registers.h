#ifndef  REGISTERS_H
#define  REGISTERS_H

#include "common.h"

u16 _cs(void);
u16 _ds(void);
u16 _ss(void);
u16 _es(void);
u16 _fs(void);
u16 _gs(void);

uint64_t get_rflags(void);
uint64_t get_cr0(void);
uint64_t _cr2(void);
uint64_t _cr3(void);
uint64_t get_cr4(void);


u16 set_cs(void);
u16 set_ds(void);
u16 set_ss(void);
u16 set_es(void);
u16 set_fs(void);
u16 set_gs(void);

void set_rflags(void);
void set_cr0(void);
void set_cr4(void);


uint64_t read_msr(uint32_t addr);
uint64_t write_msr(uint32_t addr, uint64_t value);


#endif// REGISTERS_H