#include <stdarg.h>

#include "../klib/sprintf.h"
#include "logging.h"
#include "../video/terminal.h"

#define TEXT_COLOR 0xfff0a0

const char* get_level_names_and_set_terminal_color(unsigned level) {
    switch(level) {
        case LOG_LEVEL_DEBUG:
            set_terminal_fgcolor(LOG_DEBUG_COLOR);
            return "[DEBUG] ";
        case LOG_LEVEL_INFO:
            set_terminal_fgcolor(LOG_INFO_COLOR);
            return "[INFO] ";

        default:// level > warning -> warning.
        case LOG_LEVEL_WARNING:
            set_terminal_fgcolor(LOG_WARNIN_COLOR);
            return "[WARNING] ";
    }
}

void klog(int level, const char* string) {
    kputs(get_level_names_and_set_terminal_color(level));
    
    set_terminal_fgcolor(TEXT_COLOR);
    
    kputs(string);
}
void klogf(int level, const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);


    kputs(get_level_names_and_set_terminal_color(level));
    
    set_terminal_fgcolor(TEXT_COLOR);
    
    vkprintf(fmt, ap);    

    va_end(ap);
}