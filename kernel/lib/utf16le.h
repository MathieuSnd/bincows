#pragma once


#include <stddef.h>
#include <stdint.h>

/**
 * @brief convert a utf16le encoded 
 * null-terminated string into a ascii string
 * 
 * @param ascii   destination pointer
 * @param utf16le source pointer
 * @param len     maximum length to convert
 */
void utf16le2ascii(
          char*     __restrict__ ascii, 
    const uint16_t* __restrict__ utf16le,
          size_t len
);


/**
 * @brief convert a ascii encoded 
 * null-terminated string into a utf16le
 * null-terminated string
 * 
 * @param utf16le destination pointer
 * @param ascii   source pointer
 * @param len     maximum length to convert
 */
void ascii2utf16le(
    uint16_t*   __restrict__ utf16le,
    const char* __restrict__ ascii, 
    size_t len
);
