#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../debug/assert.h"


#define TERMINAL_CHARMAP_W 6
#define TERMINAL_CHARMAP_H 2048
// must be divisible by 4 !!!

static_assert(TERMINAL_CHARMAP_H % 4 == 0);

#define TERMINAL_FONTWIDTH TERMINAL_CHARMAP_W
#define TERMINAL_FONTHEIGHT (TERMINAL_CHARMAP_H / 256)
#define TERMINAL_INTERLINE 0
#define TERMINAL_LINE_HEIGHT (TERMINAL_FONTHEIGHT + TERMINAL_INTERLINE)

#define TERMINAL_N_PAGES 2


struct stivale2_struct_tag_framebuffer;
typedef void (*terminal_handler_t)(const char *string, size_t length);

// the default terminal handler 
terminal_handler_t get_terminal_handler(void);


void setup_terminal(void);
void terminal_clear(void);

// change the default terminal handler,
// which is an empty function
void set_terminal_handler(terminal_handler_t h);


/**
 * the arguments of the following functions are expected
 * to be on the little endian RGB format
 */
void set_terminal_bgcolor(uint32_t c);
void set_terminal_fgcolor(uint32_t c);

void terminal_set_colors(uint32_t foreground, uint32_t background);