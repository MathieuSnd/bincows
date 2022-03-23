#pragma once





// size of elf_program = sizeof(elf_program_t) + sizeof
typedef
struct elf_program {

    struct segment {
        void* base;
        size_t length;

        // page flags
        // as defined in /memory/paging.h
        unsigned flags;
    };

    // program entry
    void* main;

    size_t n_segs;
    struct segment segs[0];

} elf_program_t;

elf_program_t* elf_load(const void* file, size_t file_size);


