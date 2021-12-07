#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

typedef uint16_t u64;
typedef  int64_t i64;
typedef uint32_t u32;
typedef  int32_t i32;

typedef uint16_t u16;
typedef  int16_t i16;

typedef uint8_t u8;
typedef  int8_t i8;


#define __packed    __attribute__((packed))
#define __align(N)  __attribute__ ((aligned(N)))
#define __noreturn  __attribute__((noreturn))

#ifndef NDEBUG
#define PDEBUG printf("'%s':%d - %s()\n", __FILE__, __LINE__,__func__);
#endif 

inline uint64_t allign16(uint64_t p) {
    uint64_t offset = p & 0x0f;
    if(offset)
        p = (p+16) & (~0xf);

    return p;
}

inline void* mallign16(void* ptr) {
    return (void*)allign16((uint64_t)ptr);
}
// 8 = ring 0 && id 1

#endif //COMMON_H 