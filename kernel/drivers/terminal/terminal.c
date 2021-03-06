#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "terminal.h"
#include "video.h"

#include "../../lib/string.h"
#include "../../lib/registers.h"
#include "../../lib/assert.h"
#include "../../lib/logging.h"
#include "../../memory/heap.h"
#include "../../int/apic.h"
#include "../../int/idt.h"
#include "../../fs/devfs/devfs.h"


#define TAB_SPACE 6


struct Char {
    uint32_t fg_color: 24;
    char c;
    uint32_t bg_color;
} __attribute__((packed));

static_assert_equals(sizeof(struct Char), 8);

static struct Char make_Char(driver_t* this, char c);
static void print_char      (driver_t* this, const struct Char* restrict c, int line, int col);
static void flush_screen    (driver_t* this);


struct data {
    // single char buffering,
    struct Char* char_buffer;

    // double pixel buffering
    void* px_buffers[2];
    // the buffer swiches when flushing it:
    // generally when scrolling
    unsigned cur_px_buffer;

    size_t fb_size;

    uint16_t ncols, nlines;
    uint16_t term_nlines;
    uint16_t first_line;
    uint16_t cur_col, cur_line;
    
    bool need_refresh;

    uint32_t current_fgcolor;
    uint32_t current_bgcolor;
    unsigned margin_left, margin_top;

    union
    {
        uint32_t escape_flags;
        struct {
            unsigned seq: 1;
            unsigned color: 1;

            uint8_t idx;
        } esc;
    };

    uint8_t esc_seq[4];

    // this buffer keeps a copy of the framebuffer
};


static driver_t* active_terminal = NULL;
// global functions


driver_t* get_active_terminal(void) {
    return active_terminal;
}

#define NCOLS_MAX 133
#define TERMINAL_LINES_MAX 75


// common resource
static Image charmap;


#define UPDATE_PERIOD_MS (1000 / 60)


// "uint8_t" but designates a binary file 
extern uint8_t _binary_charmap_bmp;



// should be called every UPDATE_PERIOD ms
// should execute in an IRQ
void terminal_update(driver_t* this) {
    (void) this;
    return;
    



    //if(need_refresh) {
    //    need_refresh = 0;
    //    flush_screen();
    //}
    // flush the buffer
}

// number of currently installed
// terminals: when it reaches 0 we have to free the charmap

static int terminals = 0;

static void terminal_clear(driver_t* this);



char terminal_install(driver_t* this) {
    log_info("installing terminal...");
    struct framebuffer_dev* dev = 
            (struct framebuffer_dev *)this->device;

    if(!dev ||
        dev->dev.type != DEVICE_ID_FRAMEBUFFER ||
        dev->bpp != 32
       )
        return false;

    this->remove = terminal_remove;
    this->name = (string_t){"Terminal",0};

// alloc the data
// don't put it in this->data yet,
// as we will realloc it
    struct data* restrict d = this->data = malloc(sizeof(struct data));


    d->first_line = 0;
    
// dynamicly create the terminal
// with right size
    unsigned console_w = (dev->width  ),//* 9 ) / 10,
             console_h = (dev->height );//* 95 ) / 100;
#ifdef BIGGER_FONT
    d->ncols       = console_w / TERMINAL_FONTWIDTH / 2;
    d->term_nlines = console_h / TERMINAL_LINE_HEIGHT / 2;
#else
    d->ncols       = console_w / TERMINAL_FONTWIDTH  ;//- 1;
    d->term_nlines = console_h / TERMINAL_LINE_HEIGHT;// - 1;
#endif

    // align width on 16 pixels
    //d->ncols &= ~0x0f;

    d->nlines      = TERMINAL_N_PAGES * d->term_nlines;


    // calculate the margins 
#ifdef BIGGER_FONT
    d->margin_left = (dev->width - d->ncols * 2 * TERMINAL_FONTWIDTH ) / 2;
    d->margin_top  = 0;
#else
    d->margin_left = (dev->width - d->ncols * TERMINAL_FONTWIDTH ) / 2;
    d->margin_top  = (dev->height - d->term_nlines * TERMINAL_FONTHEIGHT) / 2;
#endif

    // align margin
    d->margin_left = d->margin_left & ~0xf;


    // allocate a framebuffer copy 
    //data->framebuffer = malloc(framebuffer);


    // allocate the terminal buffer
    // to minimize the number of heap allocations,
    // we realloc our data buffer
    size_t charbuffer_size = d->ncols * d->nlines * sizeof(struct Char);
    size_t pxbuffer_size   = dev->height * dev->pitch;

    d = this->data = realloc(d, 
                        sizeof(struct data) 
                     + charbuffer_size
                     + 2 * pxbuffer_size);

    d->fb_size = pxbuffer_size;
    d->char_buffer = ((void*) d) + sizeof(struct data);
    d->px_buffers[0] = ((void*) d->char_buffer)   + charbuffer_size;
    d->px_buffers[1] = ((void*) d->px_buffers[0]) + pxbuffer_size;
    d->escape_flags = 0;
    
    memset(d->px_buffers[0], 0, 2 * pxbuffer_size);

    d->cur_px_buffer = 0;

    if(terminals++ == 0)
        loadBMP_24b_1b(&_binary_charmap_bmp, &charmap);
    

    terminal_clear(this);

    active_terminal = this;

    return 1;
}


void terminal_remove(driver_t* this) {
    struct data* restrict d = this->data;
    
// the charmap is shared amoung
// all terminals
    if(--terminals == 0)
        bmp_free(&charmap);

    //free(d->char_buffer);
    
    // the allocation of data also 
    // contains the char buffer
    free(d);
    //if(active_terminal == this)   
    //    active_terminal = NULL;
}


static void terminal_clear(driver_t* this) {
    struct data* restrict d = this->data;

    d->cur_col    = 0;
    d->cur_line   = 0;
    d->first_line = 0;
    
    size_t buffer_len = d->nlines * d->ncols;

    struct Char* ptr = d->char_buffer;

    for(;buffer_len > 0; --buffer_len)
        *(ptr++) = make_Char(this, 0);
    
    flush_screen(this);
}


static void move_buffer(driver_t* this, int lines) {
    struct data* restrict d = this->data;

    d->need_refresh = true;

    if(lines > 0) {// scroll backward
        size_t px = d->ncols * lines;
        size_t buff_px = d->nlines * d->ncols;

        memmove(
            d->char_buffer, 
            d->char_buffer + px,
            sizeof(struct Char)*(buff_px - px)
        );


        // cannot touch the first one: it is already written
        for(unsigned i = 1; i < px; i++)
            d->char_buffer[i+buff_px - px] = make_Char(this,0);
    }
}

static void next_line(driver_t* this) {
    struct data* restrict d = this->data;
    d->cur_col = 0;
    d->cur_line++;
    if(d->cur_line >= d->nlines) {
        d->first_line = d->nlines - d->term_nlines;
        d->cur_line = d->nlines-4;
        move_buffer(this, 4);
    }  
    else if(d->cur_line >= d->first_line + d->term_nlines) {
        d->first_line += 4;
        d->need_refresh = true;
    }
}

// create the char struct
static struct Char make_Char(driver_t* this, char c) {
    struct data* restrict d = this->data;
    
    return (struct Char) {
        .fg_color = d->current_fgcolor,
        .c        = c,
        .bg_color = d->current_bgcolor
    };
}


static void emplace_normal_char(driver_t* this, char c) {
    struct data* restrict d = this->data;
    
    d->char_buffer[d->ncols * d->cur_line + d->cur_col] = make_Char(this, c);
    
    struct Char* ch = &d->char_buffer[d->ncols * d->cur_line + d->cur_col];
    
    if(!d->need_refresh)
        print_char(this, ch, d->cur_line - d->first_line, d->cur_col);
    
    d->cur_col += 1;

    
    if(d->cur_col >= d->ncols) {
        next_line(this);
    }
    
}


static uint32_t colors[] = {
    0x0c0c1c, 0x400000, 0x13a10e, 0xc19c00,
    0x0037da, 0x881798, 0x3a96dd, 0xecec9d,
    0x767676, 0xe74856, 0x16c60c, 0xf9f1a5,
    0x3b78ff, 0xb4009e, 0x61d6d6, 0xf2f2f2,
};

// escape terminal colors

static void set_color(struct data* restrict d, unsigned tenth, unsigned unit) {
    (void) d;
    uint32_t color;

    if(unit > 7)
        unit = 7;

    uint32_t* c;

    switch(tenth) {
    case 3:
        c = &d->current_fgcolor;
        color = colors[unit];
        break;
    case 4:
        c = &d->current_bgcolor;
        color = colors[unit];
        break;
    case 9:
        c = &d->current_fgcolor;
        color = colors[unit + 8];
        break;
    case 10:
        c = &d->current_bgcolor;
        color = colors[unit + 8];
        break;
    default:
        // invalid escape sequence
        return;
    }

    *c = color;   
}



// emplace the char in the buffer, and maybe draw 
static void emplace_char(driver_t* this, char c) {
    struct data* restrict d = this->data;


    // color escape
    if(d->esc.color) {
        if(c <= '9' && c >= '0') {
            if(d->esc.idx > sizeof(d->esc_seq)) {
                // invalid escape sequence
                d->esc.color = 0;
                return;
            }
                
            d->esc_seq[d->esc.idx] = c - '0';
            d->esc.idx++;
        }
        else {
            switch(c) {
                case 'm':
                    if(d->esc.idx == 2) {
                        // invalid escape sequence
                        set_color(d, d->esc_seq[0], d->esc_seq[1]);
                    }
                    else if(d->esc.idx == 4) {
                        set_color(d, d->esc_seq[0], d->esc_seq[1]);
                        set_color(d, d->esc_seq[2], d->esc_seq[3]);
                    }
                    else if(d->esc.idx == 1 && d->esc_seq[0] == 0) {
                        set_color(d, 3,7); // fg: white
                        set_color(d, 4,0); // bg: black
                    }

                        // reset colors
                    // else, the sequence is invalid

                    d->esc.color = 0;
                    d->esc.idx = 0;
                    break;
                case ';':
                    break;
                default:
                    // invalid escape sequence
                    d->esc.color = 0;
                    break;
            }
        }
        return;
    }


    if(d->esc.seq && c == '[') {
        d->esc.color = 1;
        d->esc.seq = 0;
        return;
    }

    d->esc.seq = 0;


    switch(c) {
    default:
        // any character
        emplace_normal_char(this, c);
        break;
    // escape color

    case '\x1b':
        d->esc.seq = 1;
        break;
    case '\n':
        for(unsigned i=d->cur_col;i < d->ncols; i++) 
            emplace_normal_char(this, ' ');
        break;
    case '\t':
    {
        unsigned new_col = ((d->cur_col + TAB_SPACE) / TAB_SPACE) * TAB_SPACE;
        if(new_col > d->ncols)
            new_col = 0;
        while(d->cur_col < new_col)
            emplace_char(this, ' ');
    }
        break;
    case '\r':
        d->cur_col = 0;
        break;
    case '\b':
        if(d->cur_col > 0)
            d->cur_col--;
        else if(d->cur_line > 0) {
            d->cur_line--;
            d->cur_col = d->ncols-1;
        }
        break;
    case '\x0c':
        // clear the screen
        terminal_clear(this);
        break;
    }
}



static void print_char(driver_t* this, 
                       const struct Char* restrict c, 
                       int line, int col) {

    struct data* restrict d = this->data;

    struct framebuffer_dev * dev = (struct framebuffer_dev *)this->device;

    unsigned pitch = dev->pitch;
    void* px = dev->pix;

    void* px_buffer = d->px_buffers[d->cur_px_buffer];
#ifdef BIGGER_FONT
    blitcharX2(px, pitch,
                &charmap, c->c, c->fg_color, c->bg_color,
                d->margin_left + 2 * col  * TERMINAL_FONTWIDTH, 
                d->margin_top  + 2 * line * TERMINAL_LINE_HEIGHT);
    
#else
    blitchar(px, pitch, 
             &charmap, c->c, c->fg_color, c->bg_color,
             d->margin_left + col  * TERMINAL_FONTWIDTH, 
             d->margin_top  + line * TERMINAL_LINE_HEIGHT);
             
    blitchar(px_buffer, pitch, 
             &charmap, c->c, c->fg_color, c->bg_color,
             d->margin_left + col  * TERMINAL_FONTWIDTH, 
             d->margin_top  + line * TERMINAL_LINE_HEIGHT);
#endif

}

// print in buffer exclusively
static void buff_print_char(driver_t* this, 
        const struct Char* restrict c, 
        int line, int col
) {
    struct framebuffer_dev * dev = (struct framebuffer_dev *)this->device;
   
    struct data* restrict d = this->data;
    
    void* px = d->px_buffers[d->cur_px_buffer];
    unsigned pitch = dev->pitch;

#ifdef BIGGER_FONT
    blitcharX2(px, pitch, 
                &charmap, c->c, c->fg_color, c->bg_color,
                d->margin_left + 2 * col  * TERMINAL_FONTWIDTH, 
                d->margin_top  + 2 * line * TERMINAL_LINE_HEIGHT);
    
#else
    blitchar(px, pitch, 
             &charmap, c->c, c->fg_color, c->bg_color,
             d->margin_left + col  * TERMINAL_FONTWIDTH, 
             d->margin_top  + line * TERMINAL_LINE_HEIGHT);
#endif
}

static void update_framebuffer(driver_t* this) {
    struct data* restrict d = this->data;
    struct framebuffer_dev * dev = (struct framebuffer_dev *)this->device;
    
    // 64 bytes = 8 qwords
    const unsigned cache_line = 8;

    uint64_t* devfb = dev->pix;
    uint64_t* buff = d->px_buffers[d->cur_px_buffer];
    uint64_t* otherbuff = d->px_buffers[!d->cur_px_buffer];

    for(unsigned i = 0; i < d->fb_size / 8; i += cache_line) {
        uint64_t diff = 0;

        uint64_t* ptr = buff;

        uint64_t* otherptr = otherbuff;


        // check if something is modified
        for(unsigned i = 0; i < cache_line; i++) {
            diff = *(ptr++) ^ *(otherptr++);
            if(diff)
                break;
        }

        if(diff) {
            // have to append changes

            ptr = buff;
            uint64_t* devptr = devfb;
            for(unsigned j = 0; j < cache_line; j++) {
                asm volatile(
                    "movnti %1, %0"
                    : "=m" (*(devptr++))
                    : "r"  (*ptr++)
                );
            }
       }
       else {

       }


        buff      += cache_line;
        otherbuff += cache_line;
        devfb     += cache_line;
    }

}


static void flush_screen(driver_t* this) {
    struct data* d = this->data;
        // begins at the terminal's first line
    const struct Char* curr = d->char_buffer + d->first_line * d->ncols;


    d->cur_px_buffer = !d->cur_px_buffer;
    

    for(size_t l = 0; l < d->term_nlines; l++) {
        /*
        for(size_t c = 0; c < d->ncols; c += 2) {
            nt_print_chars(this, curr, l, c);
            curr += 2;
        }
        */
        for(size_t c = 0; c < d->ncols; c++) {
            buff_print_char(this, curr, l, c);
            curr++;
        }
    }
    update_framebuffer(this);
}


/*
// add the string to the buffer
static void append_string(const char *string, size_t length) {
// atomic operation
    _cli();
    
    memcpy(stream_buffer + stream_buffer_content_size, 
           string, 
           length);

    stream_buffer_content_size += length;

    _sti();
}
*/

void write_string(driver_t* this, const char *string, size_t length) {
    struct data* restrict d = this->data;
    d->need_refresh = false;

    for(;length>0;--length) {
        char c = *string++;

        if(!c)
            break;

        emplace_char(this, c);
    }
    if(d->need_refresh)
        flush_screen(this);
}



///////////////////////////////////////////////////////////////////////////////
/////////////      /dev/term0 driver      ////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "../../../blib/include/terminfo.h"
#include "../../lib/math.h"

static int terminal_devfile_read(
                driver_t* this,
                void*     buffer,
                size_t    begin,
                size_t    count
) { 

    (void)begin;


    struct data* restrict d = this->data;

    
    terminfo_t terminfo = {
        .cols = d->ncols,
        .lines = d->term_nlines,
        .stored_lines = d->nlines,
        .cursor_x = d->cur_col,
        .cursor_y = d->cur_line,
        .first_line = d->first_line,
    };

    log_info("terminfo rd %d", count);

    int rd = MIN(sizeof(terminfo_t), count);

    memcpy(buffer, &terminfo, rd);

    
    // unreadable
    return rd;
}


static int terminal_devfile_write(
                driver_t* this,
                const void* buffer,
                size_t begin,
                size_t count
) {
    // unseekable
    (void)begin;

    assert(this);
    struct data* restrict d = this->data;
    assert(d);

    write_string(this, buffer, count);
    return count;
}


void terminal_register_dev_file(const char* filename, driver_t* this) {


    int r = devfs_map_device((devfs_file_interface_t){
        .arg   = this,
        .read  = (void*) terminal_devfile_read,
        .write = (void*) terminal_devfile_write,
        .rights = {.read = 1, .write = 1, .seekable = 0},
        .file_size = ~0llu,
    }, filename);

    // r = 0 on success
    assert(!r);
}
