#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void (*terminal_handler_t)(const char *string, size_t length);

/**
 * return NULL if no terminal output is installed
 **/
terminal_handler_t get_terminal_handler(void);


void set_terminal_handler(terminal_handler_t h);

void termina_set_colors(uint32_t foreground, uint32_t background);