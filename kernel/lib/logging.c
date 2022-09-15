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
#include "../lib/registers.h"
#include "../sched/sched.h"

#include "logging.h"


#define TEXT_COLOR 0xfff0a0

#define BUFFER_SIZE 4096


static const char* logfile_path = NULL;

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
        log_flush(); // buffer full, flush it.

    memcpy(logs_buffer+i, str, len);
    i += len;
}

static const char* get_level_names_and_set_terminal_color(unsigned level) {
    //driver_t* terminal = get_active_terminal();
    switch(level) {
        case LOG_LEVEL_DEBUG:
            //if(terminal)
            //    terminal_set_fgcolor(terminal,LOG_DEBUG_COLOR);
            return "\x1b[90m[DEBUG]\x1b[0m   ";
        case LOG_LEVEL_INFO:
            //if(terminal)
            //    terminal_set_fgcolor(terminal,LOG_INFO_COLOR);
            return "\x1b[32m[INFO]\x1b[0m    ";
        default:// level > warning -> warning.
        case LOG_LEVEL_WARN:
            //if(terminal)
            //    terminal_set_fgcolor(terminal,LOG_WARNIN_COLOR);
            return "\x1b[33m[WARNING]\x1b[0m ";
    }
}


void log(unsigned level, const char* string) {
    // @todo protect this with a spinlock

    static char buffer[2048];
    if(level < current_level)
        return; // avoid overflows

    const char* level_name = get_level_names_and_set_terminal_color(level);

    
    sprintf(buffer, "%s%s\n", level_name, string);

    if(level >= current_level) {

        puts(buffer);

        //driver_t* terminal = get_active_terminal();

    // print on the screen
    // with fancy colors
        //printf(level_name);

        //if(terminal)
        //    terminal_set_fgcolor(terminal, TEXT_COLOR);
        
        //printf(string);
        //printf("\n");
    }

// append to the buffer
    append_string(buffer);

    static int i = 0;
    if(logfile_path && i++ % 10 == 0)
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
    i = 0;
    return;

    // if a log is done during a flush, it shouldn't flush recursively
    static int flushing = 0;

    

    if(logfile_path && !flushing) {


        if(!interrupt_enable() || !sched_is_running() || sched_current_pid() != 0) {
            // we cannot use the write 
            // function.
            i = 0;
            //log_warn("cannot flush log file");
            return;
        }

        flushing = 1;

        file_handle_t* file = vfs_open_file(
                        logfile_path, VFS_WRITE | VFS_APPEND);

        assert(file);


        vfs_write_file(logs_buffer, i, 1, file);

        vfs_close_file(file);

        flushing = 0;
    }
    i = 0;
}



void log_cleanup(void) {
    log_flush();
    logfile_path = NULL;
}

void log_init_file(const char* filename) {
    logfile_path = filename;

    log_flush();
}


