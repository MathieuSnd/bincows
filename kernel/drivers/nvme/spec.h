#pragma once

#define NSSRS_SUPPORT(CAP) ((CAP & (1llu << 36)) != 0)


struct regs {
             uint64_t cap;
             uint32_t version;
             uint32_t intmask_set;
             uint32_t intmask_clear;
    volatile uint32_t config;
    volatile uint32_t subsystem_reset;
    volatile uint32_t status;
             uint32_t reserved1;
    volatile uint32_t queue_attr;
    volatile uint32_t asq_low;  // admin submission queue
    volatile uint32_t asq_high; // base address

    volatile uint32_t acq_low;  // admin completion queue
    volatile uint32_t acq_high; // base address
} __attribute__((packed));

struct lbaf {
    uint16_t ms;
    uint8_t  ds; // data size: 1 << ds shift 
    uint8_t  rp;
};

static_assert_equals(sizeof(struct lbaf), 4);


struct subqueuee {
    uint32_t cmd;
    uint32_t nsid;
    uint32_t cdw2; // cmd specific
    uint32_t cdw3; // cmd specific
    uint64_t metadata_ptr;
    uint64_t data_ptr[2];
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));


static inline
uint32_t make_cmd(
            uint8_t opcode,
            unsigned fused, // 2 bits
            unsigned cmdid  // 16 bits
) {
    return ((unsigned)opcode) 
         | (fused << 8)
         | (0 << 14)   // 0: PRP; 1: SGL
         | (cmdid << 16);
}


struct compqueuee {
    uint32_t cmd_specific;
    uint32_t reserved;
    uint16_t sq_head_pointer;
    uint16_t iq_id;
    uint16_t cmd_id;
    unsigned phase: 1;
    unsigned status: 15;
} __attribute__((packed));


static_assert_equals(sizeof(struct subqueuee),  64);
static_assert_equals(sizeof(struct compqueuee), 16);


static inline 
void reset(struct regs* regs) {
    regs->subsystem_reset = 0x4E564D65;
}

static inline 
int is_shutdown(struct regs* regs) {
    return ((regs->status >> 2) & 3) == 0b10;
}

