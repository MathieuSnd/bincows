#pragma once

#include <stddef.h>
#include <stdint.h>



#define DUMP_HEX 0
#define DUMP_DEC 16

#define DUMP_8  32
#define DUMP_32 0
#define DUMP_64 1

#define DUMP_HEX8  (DUMP_HEX | DUMP_8)
#define DUMP_HEX32 (DUMP_HEX | DUMP_32)
#define DUMP_HEX64 (DUMP_HEX | DUMP_64)
#define DUMP_DEC8  (DUMP_DEC | DUMP_8)
#define DUMP_DEC32 (DUMP_DEC | DUMP_32)
#define DUMP_DEC64 (DUMP_DEC | DUMP_64)


void dump(const void* addr, size_t size, size_t line_size, uint8_t mode);
