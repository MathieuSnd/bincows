#include "terminal.h"

terminal_handler_t terminal_handler = NULL;
uint32_t terminal_foreground = 0xa0a0a0;
uint32_t terminal_background = 0x00;

terminal_handler_t get_terminal_handler(void) {
    return terminal_handler;
}


void set_terminal_handler(terminal_handler_t h) {
    terminal_handler = h;
}

void termina_set_colors(uint32_t foreground, uint32_t background) {
    terminal_foreground = foreground;
    terminal_background = background;
}

