#include "utf16le.h"



void utf16le2ascii(
        char* __restrict__ ascii, 
        const uint16_t* __restrict__ utf16le,
        size_t len
) {
    char c;

    for(;len>0;--len) {
        *ascii++ = c = (*utf16le++) & 0xff;

        if(!c) {
            *ascii = '\0';
            break;
        }
    }
}