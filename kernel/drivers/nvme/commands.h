#pragma once

#include "spec.h"
#include "queue.h"

// defines all of the admin commands

#define OPCODE_IDENTIFY 0x06
#define OPCODE_CREATE_SQ 0x01
#define OPCODE_CREATE_CQ 0x05

#define OPCODE_IO_READ 0x02
#define OPCODE_IO_WRITE 0x01

/**
 * @brief insert the described command in the
 * given submission queue. If the submission
 * queue is full, a kernel panic occurs.
 * the function immediatly returns after the 
 * command is enqueued
 * 
 * @param doorbell_stride the NVM doorbell stride 
 * @param regs the virtual address of the NVME 
 *           register space (bar0 base)
 * @param sq a pointer to the submission queue 
 *           structure in which the command is to 
 *           be enqueued
 * @param opcode the opcode of the command
 * @param cmdid the command id
 * @param nsid the namespace id
 * @param prp0 the command prp0 field
 * @param prp1 the command prp1 field
 * @param cdw10 the command cdw10 field
 * @param cdw11 the command cdw11 field
 * @param cdw12 the command cdw12 field
 */
void async_command(
    unsigned doorbell_stride,
    struct regs* regs,
    struct queue* sq,

    uint8_t  opcode, 
    uint16_t cmdid,
    uint32_t nsid,
    uint64_t prp0,
    uint64_t prp1,
    uint32_t cdw10,
    uint32_t cdw11,
    uint32_t cdw12
);


/**
 * @brief insert the described command in the
 * given submission queue. If the submission
 * queue is full, a kernel panic occurs.
 * the function waits until the controller
 * submitted a completion entry before returning.
 * It uses the sleep(ms) so the caller must make 
 * sure it is available.
 * 
 * @param doorbell_stride the NVM doorbell stride 
 * @param regs the virtual address of the NVME 
 *           register space (bar0 base)
 * @param sq a pointer to the submission queue 
 *           structure in which the command is to 
 *           be enqueued
 * @param opcode the opcode of the command
 * @param cmdid the command id
 * @param nsid the namespace id
 * @param prp0 the command prp0 field
 * @param prp1 the command prp1 field
 * @param cdw10 the command cdw10 field
 * @param cdw11 the command cdw11 field
 * @param cdw12 the command cdw12 field
 */
void sync_command(
    unsigned doorbell_stride,
    struct regs* regs,
    struct queue* sq,

    uint8_t  opcode, 
    uint16_t cmdid,
    uint32_t nsid,
    uint64_t prp0,
    uint64_t prp1,
    uint32_t cdw10,
    uint32_t cdw11,
    uint32_t cdw12
);
