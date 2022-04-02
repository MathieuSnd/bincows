#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../../lib/assert.h"
#include "../../lib/string_t.h"

#include "../dev.h"
#include "../driver.h"


#define TERMINAL_CHARMAP_W 6
#define TERMINAL_CHARMAP_H 2048
// must be divisible by 4 !!!

static_assert(TERMINAL_CHARMAP_H % 4 == 0);

#define TERMINAL_FONTWIDTH TERMINAL_CHARMAP_W
#define TERMINAL_FONTHEIGHT (TERMINAL_CHARMAP_H / 256)
#define TERMINAL_INTERLINE 0
#define TERMINAL_LINE_HEIGHT (TERMINAL_FONTHEIGHT + TERMINAL_INTERLINE)

#define TERMINAL_N_PAGES 2


#define DEVICE_ID_FRAMEBUFFER (0xfa30bffe)

// virtual device
struct framebuffer_dev {
    struct dev dev;

    void*    pix;
    unsigned width, height;
    unsigned pitch;
    unsigned bpp;
};

struct stivale2_struct_tag_framebuffer;
typedef void (*terminal_handler_t)(driver_t* dr, const char *string, size_t length);

// the default terminal handler 

char terminal_install(driver_t* this);
void terminal_remove (driver_t* this);
void terminal_update (driver_t* this);

void terminal_clear  (driver_t* this);

// change the default terminal handler,
// which is an empty function
// h = NULL will make a safe empty handler
//void set_terminal_handler(driver_t* this, terminal_handler_t h);

void write_string(driver_t* this, const char *string, size_t length);


driver_t* get_active_terminal(void);

/**
 * the arguments of the following functions are expected
 * to be on the little endian RGB format
 */
void terminal_set_bgcolor(driver_t* this,uint32_t c);
void terminal_set_fgcolor(driver_t* this,uint32_t c);

void terminal_set_colors(driver_t* this,
                         uint32_t foreground, 
                         uint32_t background);

void terminal_register_dev_file(const char* filename, driver_t* this);

