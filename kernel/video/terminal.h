#pragma once

#include <stddef.h>
#include <stdint.h>

struct stivale2_struct_tag_framebuffer;
typedef void (*terminal_handler_t)(const char *string, size_t length);

/**
 * return NULL if no terminal output is installed
 **/
terminal_handler_t get_terminal_handler(void);


void setup_terminal(void);

// no use another terminal handler
void set_terminal_handler(terminal_handler_t h);

void terminal_set_colors(uint32_t foreground, uint32_t background);