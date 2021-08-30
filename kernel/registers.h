#ifndef  REGISTERS_H
#define  REGISTERS_H

#include "common.h"

u16 _cs(void);
u16 _ds(void);
u16 _ss(void);
u16 _es(void);
u16 _fs(void);
u16 _gs(void);


u16 set_cs(void);
u16 set_ds(void);
u16 set_ss(void);
u16 set_es(void);
u16 set_fs(void);
u16 set_gs(void);

#endif// REGISTERS_H