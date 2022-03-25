#include <stdint.h>
#include <stddef.h>

#include "elf.h"
#include "spec.h"


#include "../string.h"
#include "../logging.h"
#include "../dump.h"
#include "../../memory/paging.h"
#include "../../memory/heap.h"

#include "../../sched/thread.h"


/**
 * @brief return 0 if invalid header
 * 
 */
static int check_elf_header(const ehdr_t* ehdr, size_t file_size) {

    // file is too short
    if(file_size < sizeof(ehdr_t))
        return 0;
    
    log_warn("checked %x", ehdr->magic);

    dump(
        ehdr,
        sizeof(ehdr_t),
        16,
        DUMP_8
    );


    if(
           ehdr->magic     != 0x464c457f
        || ehdr->arch      != 2
        || ehdr->endianess != 1
        || ehdr->header_version  != 1
        || ehdr->abi       != 0
        || ehdr->version   != 1
        || ehdr->isa       != 0x3E // x86-64
        || ehdr->ehdr_size != sizeof(ehdr_t)
        || ehdr->phdr_entry_size != sizeof(phdr_t)
    )
        return 0;

    return 1;
}


static
unsigned convert_flags(unsigned p_flags) {
    unsigned flags = PRESENT_ENTRY;

    if((p_flags & 1) == 0) {
        // no execute
        flags |= PL_XD;
    } 
    if((p_flags & 2) == 0) {
        // no write
        flags |= PL_RW;
    }
    else if((p_flags & 4) == 0) {
        // no read
        log_info("ignoring elf not present readable flag");
    }

    return flags;
}


elf_program_t* elf_load(const void* file, size_t file_size) {
    
    const ehdr_t* ehdr = file;

    if(!check_elf_header(ehdr, file_size))
        return NULL;
    elf_program_t* prog = malloc(
        sizeof(elf_program_t) +
        sizeof(prog->segs[0]) * ehdr->phdr_table_size
    );


    prog->entry   = (void *)ehdr->program_entry;

    unsigned j = 0;
    assert(ehdr->phdr_entry_size == 56);
    for(unsigned i = 0; i < ehdr->phdr_table_size; i++, j++) {
        const phdr_t* phdr = file + ehdr->phdr_offset 
                                  +i * ehdr->phdr_entry_size;

        log_warn("issou phdr->p_type=%u",phdr->p_type);


        prog->segs[j].flags  = convert_flags(phdr->flags);
        prog->segs[j].length = phdr->p_memsz;
        prog->segs[j].base   = (void*)phdr->p_addr;



        log_warn(
            "i = %u\n"
            "prog->segs[j].base=%lx \n"
            "prog->segs[j].length >> 12=%lu \n"
            "prog->segs[j].flag=%lu \n",
            i,
            prog->segs[j].base,
            prog->segs[j].length >> 12,
            prog->segs[j].flags
        );

        if(phdr->p_type != PT_LOAD) {
            --j;
            continue;
        }

        if(
            phdr->p_offset + phdr->p_filesz > file_size
         || phdr->p_filesz > phdr->p_memsz
         || ((uint64_t)prog->segs[j].base & 0x0fff)
        ) {
            // bad file
            log_info("bad elf file");

            free(prog);
            return NULL;
        }
    
    
        size_t page_count = (prog->segs[j].length+0xfff) >> 12;
        // map segments without paging attributes
        // until we copied the content
        alloc_pages(
            prog->segs[j].base,
            page_count,
            PRESENT_ENTRY
        );
        

        memset(
            prog->segs[j].base, 
            0, 
            phdr->p_memsz
        );

        memcpy(           
            prog->segs[j].base, 
            file + phdr->p_offset,
            phdr->p_filesz
        );

        remap_pages(
            prog->segs[j].base,
            page_count,
            0
        );

    }

    prog->n_segs = j;

    prog = realloc(
        prog,
        sizeof(elf_program_t) +
        sizeof(prog->segs[0]) * prog->n_segs
    );

    return prog;
}
