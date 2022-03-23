#pragma once

#include "../assert.h"

#define EI_NIDENT 16

typedef struct ehdr {
    uint32_t magic;                    // 0-3 Magic number - 0x7F, then 'ELF' in ASCII
    uint8_t  arch;                     //1 = 32 bit, 2 = 64 bit
    uint8_t  endianess;                //1 = little endian, 2 = big endian
    uint8_t  header_version;           //ELF header version
    uint8_t  abi;                      // usually 0 for System V
    uint8_t  unused[8];                // 
    uint16_t type;                     //1 = relocatable, 2 = executable, 3 = shared, 4 = core
    uint16_t isa;                      //Instruction set - see table below
    uint32_t version;                  //ELF Version
    uint64_t program_entry;            //Program entry position
    uint64_t phdr_offset;              //Program header table position
    uint64_t section_hdr_offset;       //Section header table position
    uint32_t flags;                    //Flags - architecture dependent; see note below
    uint16_t ehdr_size;                //Header size
    uint16_t phdr_entry_size;          //Size of an entry in the program header table
    uint16_t phdr_table_size;          //Number of entries in the program header table
    uint16_t section_table_entry_size; //Size of an entry in the section header table
    uint16_t section_table_size;       //Number of entries in the section header table
    uint16_t section_names_index;      //Index in section header table with the section names
} __attribute__((packed)) ehdr_t;


typedef 
struct phdr {
    uint32_t p_type;  //Type of segment
    uint32_t flags;     //Flags
    uint64_t p_offset;  //The offset in the file that the data for this segment can be found
    uint64_t p_addr;    //Where you should start to put this segment in virtual memory
    uint64_t undefined; //Undefined for the System V ABI
    uint64_t p_filesz;  //Size of the segment in the file
    uint64_t p_memsz;   //Size of the segment in memory
    uint64_t p_align;   //The required alignment for this section (must be a power of 2)
} __attribute__((packed)) phdr_t;


#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff
/*
egment types: 0 = null - ignore the entry; 
1 = load - clear p_memsz bytes at p_vaddr to 0, then copy p_filesz bytes from p_offset to p_vaddr;
2 = dynamic - requires dynamic linking; 
3 = interp - contains a file path to an executable to use as an interpreter for the following segment; 
4 = note section. There are more values, but mostly contain architecture/environment specific information, which is probably not required for the majority of ELF files.

Flags: 1 = executable, 2 = writable, 4 = readable.
*/


static_assert_equals(sizeof(ehdr_t), 64);
static_assert_equals(sizeof(phdr_t), 56);


