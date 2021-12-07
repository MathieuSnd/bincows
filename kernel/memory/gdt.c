#include "gdt.h"
#include "../lib/common.h"
#include "../lib/sprintf.h"
#include "../lib/assert.h"

extern void _ltr(uint16_t tss_selector);


// kernel stack
extern const uint8_t kernel_stack[];

struct TSS {
    uint32_t reserved0;
    uint64_t RSP[3]; // The full 64-bit canonical forms of the stack pointers (RSP) for privilege levels 0-2
    uint64_t reserved1;
    uint64_t IST[7]; // The full 64-bit canonical forms of the interrupt stack table (IST) pointers.
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t IOPB_offset; // just put the size of the structure
} __attribute__((packed));

static_assert_equals(sizeof(struct TSS), 104);

const struct TSS tss = {0, {(uint64_t)kernel_stack,0,0}, 0, {0}, 0,0,sizeof(struct TSS)};

struct GDTentry {
    u16 limit1;             // x86_64: ignrored
    u16 base1;              // x86_64: ignrored
    u8  base2;              // x86_64: ignrored
// access byte
//union {
  //  struct {
    u8  accessed: 1;        // should be 0 (the CPU sets it to 1 when accessed)
    u8  read_write: 1;      // for code selectors: readable bit
                            // for data selectors: writable bit
    u8  dir_conforming: 1;  // for code selectors: should be 0 in longmode 
                            //                     (code can be executed by lower privilege level)
                            // for data selectors: 0: segment grows upward, downward otherwise 
    u8  executable: 1;      // for code selectors
    u8  descriptor: 1;      // should be 1 for data/code segments, 0 for system (tss)
    u8  ring: 2;            // privilege ring
    u8  present: 1;         // should always be 1
    //} __packed;
  //  u8 access_byte;
//} __packed;


    u8  limit2: 4;          // x86_64: ignrored
//union {
    //struct {
    u8  nullbits: 1;        // must be 0
    u8  longcode: 1;        // x86_64: 1
    u8  size: 1;            // x86_64: must be 0
    u8  granularity: 1;     // x86_64: ignrored
    //} __packed;
//    u8 flags: 4;
//} __packed;
    u8  base3;           // x86_64: ignrored
} __packed;

struct GDTDescriptor {
    uint16_t size;    // size of the GDT - 1
    uint64_t offset;  // address of the table
} __packed;



// GDT is in .bss section
static struct GDTentry gdt[] = {
    {0},//  null descriptor
    {0,0,0,  0,0,0,1,1,0,1,   0,  0,1,0,1,  0}, // kernel code segment
    {0,0,0,  0,1,0,0,1,0,1,   0,  0,0,1,1,  0}, // kernel data segment
    
    {0,0,0,  0,0,0,1,1,3,1,   0,  0,1,0,1,  0}, // user code segment
    {0,0,0,  0,1,0,0,1,3,1,   0,  0,0,1,1,  0}, // user data segment

// gcc is SO dumb we have to fill the struct by software
    {0,0,0,  1,0,0,1,0,0,1,   0,  1,0,0,0,  0}, // TSS segment1
    {0,0,0,  0,0,0,0,0,0,0,   0,  0,0,0,0,  0}, // TSS segment2
};

static_assert(sizeof(gdt) == 7 * sizeof(struct GDTentry));

// from gdt.s
void _lgdt(void* gdt_desc_ptr);


struct GDTDescriptor gdt_descriptor = {
    .size=sizeof(gdt)-1,
    .offset=(uint64_t)gdt
};

#define PRINT_STRUCT(TARGET) \
    printf("&" #TARGET "=0x%8lx\tsizeof(" #TARGET ")=%ld (0x%lx)\n", \
    &TARGET, sizeof(TARGET),sizeof(TARGET))

    volatile int y = 5421;




void init_gdt_table() {
    // init GDT TSS entry 
    
    gdt[5].base1 = (uint64_t)&tss       & 0xfffflu;
    gdt[5].base2 = (uint64_t)&tss >> 16 & 0x00ffllu;
    gdt[5].base3 = (uint64_t)&tss >> 24 & 0x00ffllu;
    *(uint64_t *)(&gdt[6]) = (uint64_t)(&tss) >> 32;
    
    _lgdt(&gdt_descriptor);


    // the TSS offset in the GDT table << 3 (the first 3 bits are DPL)
    _ltr(0x28);
}