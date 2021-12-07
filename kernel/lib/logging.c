#include <stdarg.h>

#include "../lib/sprintf.h"
#include "logging.h"
#include "../video/terminal.h"

#include "../lib/string.h"

#define TEXT_COLOR 0xfff0a0

#define BUFFER_SIZE 4096

static char logs_buffer[BUFFER_SIZE];
static unsigned current_level;
// current position in the buffer
static int i = 0;

// appends a string to the buffer
// if there is enough space remaining.
// else, leave it as is
static inline void append_string(const char* str) {

    unsigned len = strlen(str);
    if(i+len >= BUFFER_SIZE)
        klog_flush();        
    memcpy(logs_buffer+i, str, len);
    i += len;
}

static const char* get_level_names_and_set_terminal_color(unsigned level) {
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

void klog(unsigned level, const char* string) {
    if(level < current_level)
        return; // avoid overflows

    const char* level_name = get_level_names_and_set_terminal_color(level);


// print on the screen
// with fancy colors
    kputs(level_name);
    set_terminal_fgcolor(TEXT_COLOR);
    
    kputs(string);
    kputs("\n");

// append to the buffer
    append_string(level_name);
    append_string(string);
    append_string("\n");
}


    
void klogf(unsigned level, const char* fmt, ...) {
    if(level < current_level)
        return;

    char string[1024];
    
// render string buffer 
    va_list ap;
    va_start(ap, fmt);
    vsprintf(string, fmt, ap);
    va_end(ap);

    klog(level, string);

}

void set_logging_level(unsigned level) {
    current_level = level;
}


const char* klog_get(void) {
    logs_buffer[i] = 0;
    return logs_buffer;
}

void klog_flush(void) {
    i = 0;
}


