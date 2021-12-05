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
void     _cr3(uint64_t cr3);
uint64_t get_cr4(void);


void set_rflags(uint64_t rf);
void set_cr0(uint64_t cr0);
void set_cr4(uint64_t cr4);


#define IA32_EFER_MSR 0xC0000080

uint64_t read_msr(uint32_t addr);
uint64_t write_msr(uint32_t addr, uint64_t value);


#endif// REGISTERS_H