#pragma once

#include <stddef.h>
#include <stdint.h>



#define CHARMAP_W 12
#define CHARMAP_H 4096
// must be divisible by 4 !!!

#define FONTWIDTH CHARMAP_W
#define FONTHEIGHT (CHARMAP_H / 256)
#define INTERLINE 0
#define LINE_HEIGHT (FONTHEIGHT + INTERLINE)

#define N_PAGES 4


struct stivale2_struct_tag_framebuffer;
typedef void (*terminal_handler_t)(const char *string, size_t length);

/**
 * return NULL if no terminal output is installed
 **/
terminal_handler_t get_terminal_handler(void);

void set_terminal_bgcolor(uint32_t c);
void set_terminal_fgcolor(uint32_t c);


void setup_terminal(void);
void clear_terminal(void);

// no use another terminal handler
void set_terminal_handler(terminal_handler_t h);

void terminal_set_colors(uint32_t foreground, uint32_t background);