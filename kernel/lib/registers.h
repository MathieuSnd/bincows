#ifndef  REGISTERS_H
#define  REGISTERS_H


uint16_t _cs(void);
uint16_t _ds(void);
uint16_t _ss(void);
uint16_t _es(void);
uint16_t _fs(void);
uint16_t _gs(void);

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



static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}


uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t val);


#endif// REGISTERS_H