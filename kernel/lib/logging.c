#include <stdarg.h>

#ifndef TEST
#include "../drivers/terminal/terminal.h"
#else
#define get_active_terminal() NULL
#define terminal_set_fgcolor(A,B) {}
#endif


#include "../drivers/driver.h"
#include "../lib/sprintf.h"
#include "../lib/string.h"
#include "../fs/vfs.h"
#include "../acpi/power.h"

#include "logging.h"


#define TEXT_COLOR 0xfff0a0

#define BUFFER_SIZE 4096

static file_handle_t* logfile = NULL;

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
        log_flush();        
    memcpy(logs_buffer+i, str, len);
    i += len;
}

static const char* get_level_names_and_set_terminal_color(unsigned level) {
    driver_t* terminal = get_active_terminal();
    switch(level) {
        case LOG_LEVEL_DEBUG:
            if(terminal)
                terminal_set_fgcolor(terminal,LOG_DEBUG_COLOR);
            return "[DEBUG]   ";
        case LOG_LEVEL_INFO:
            if(terminal)
                terminal_set_fgcolor(terminal,LOG_INFO_COLOR);
            return "[INFO]    ";

        default:// level > warning -> warning.
        case LOG_LEVEL_WARN:
            if(terminal)
                terminal_set_fgcolor(terminal,LOG_WARNIN_COLOR);
            return "[WARNING] ";
    }
}

void log(unsigned level, const char* string) {
    if(level < current_level)
        return; // avoid overflows

    const char* level_name = get_level_names_and_set_terminal_color(level);

    if(level >= current_level) {
        driver_t* terminal = get_active_terminal();

    // print on the screen
    // with fancy colors
        printf(level_name);

        if(terminal)
            terminal_set_fgcolor(terminal, TEXT_COLOR);
        
        printf(string);
        printf("\n");
    }

// append to the buffer
    append_string(level_name);
    append_string(string);
    append_string("\n");

    if(logfile)
        log_flush();
}


    
void logf(unsigned level, const char* fmt, ...) {

    char string[1024];
    
// render string buffer 
    va_list ap;
    va_start(ap, fmt);
    vsprintf(string, fmt, ap);
    va_end(ap);

    log(level, string);

}

void set_logging_level(unsigned level) {
    current_level = level;
}


const char* log_get(void) {
    logs_buffer[i] = 0;
    return logs_buffer;
}

void log_flush(void) {

    if(logfile)
        vfs_write_file(logs_buffer, i, 1, logfile);

    i = 0;
}


void log_cleanup(void) {
    if(logfile) {
        vfs_close_file(logfile);
        logfile = NULL;
    }
}

void log_init_file(const char* filename) {
    logfile = vfs_open_file(filename, VFS_WRITE | VFS_APPEND);

    assert(logfile);

    log_flush();

    atshutdown(log_cleanup);
}


