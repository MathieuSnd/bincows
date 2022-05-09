#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void init_gdt_table();
void set_tss_rsp(uint64_t rsp);

#endif // GDT_H