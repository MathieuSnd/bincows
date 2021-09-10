#include "dump.h"
#include "../klib/sprintf.h"

/**
 * dump a memory chunk in kprintf
 * addr: address of the beginning of the chunk
 * size:      number of bytes to dump
 * line_size: number of words per line
 * 
 * mode:     either
 *      DUMP_HEX8  : hexadecimal integers of size 8  bits
 *      DUMP_HEX32 : hexadecimal integers of size 32 bits
 *      DUMP_HEX64 : hexadecimal integers of size 64 bits
 *      DUMP_DEC8  :     decimal integers of size 8  bits
 *      DUMP_DEC32 :     decimal integers of size 32 bits
 *      DUMP_DEC64 :     decimal integers of size 64 bits
 * 
 **/
void dump(const void* addr, size_t size, size_t line_size, uint8_t mode) {
    
    char row_fmt[8], last_row_fmt[8];

    size_t pitch; // sizeof word
    
    int i = 0;
    row_fmt     [i] = '%';
    last_row_fmt[i] = '%';
    i++;

    
    // create fmts for printf

//  word width
    if((mode & DUMP_8) != 0) {
        // byte mode

        row_fmt     [i] = '2';
        last_row_fmt[i] = '2';
        i++;

        pitch = 1;
    }
    else if((mode & DUMP_64) == 0) {
        // normal 32bit mode

        row_fmt     [i] = '8';
        last_row_fmt[i] = '8';
        i++;
        
        pitch = 4;
    }
    else {
        // long mode
        row_fmt     [i] = 'l';
        last_row_fmt[i] = 'l';
        i++;
        row_fmt     [i] = '1';
        last_row_fmt[i] = '1';
        i++;
        row_fmt     [i] = '6';
        last_row_fmt[i] = '6';
        i++;

        pitch = 8;
    }

// base
    char base_id;


    if((mode & DUMP_HEX) == 0) // hex
        base_id = 'x';
    else               // dec
        base_id = 'u';
    
    row_fmt     [i] = base_id;
    last_row_fmt[i] = base_id;
    i++;

    row_fmt     [i] =  ' ';
    last_row_fmt[i] = '\n'; 
    i++;

    row_fmt     [i] = '\0';
    last_row_fmt[i] = '\0'; 
    i++;

    

    size /= pitch;
    size_t lines = (size + line_size - 1) / line_size;
    
    // iterator ptr
    const uint8_t* ptr = addr;

// create mask : create the complementary with 0xff...ff << n
// then complement it
    const uint64_t mask = ~(~0llu << 8*pitch);

    for(size_t i = 0; i < lines; i++) {
        // the last line might not be full
        // therefore we have to check each one
        
        for(size_t j = 0; j < line_size-1; j++) {
            if(size-- <= 1)
                break;
            else {
                kprintf(row_fmt, *(uint64_t *)ptr & mask);
                ptr+=pitch;
            }
            
        }
        kprintf(last_row_fmt, *(uint64_t *)ptr & mask);
        ptr+=pitch;
    }
}
