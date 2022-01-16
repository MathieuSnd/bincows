#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "terminal.h"
#include "video.h"

#include "../../lib/string.h"
#include "../../lib/assert.h"
#include "../../lib/logging.h"
#include "../../memory/heap.h"
#include "../../int/apic.h"
#include "../../int/idt.h"


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
    terminal_handler_t terminal_handler;
    struct Char* char_buffer;

    uint16_t ncols, nlines;
    uint16_t term_nlines;
    uint16_t first_line;
    uint16_t cur_col, cur_line;
    
    bool need_refresh;

    uint32_t current_fgcolor;
    uint32_t current_bgcolor;
    unsigned margin_left, margin_top;
    unsigned timerID;
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
    //static unsigned update_cur_line = 0;
    



    //if(need_refresh) {
    //    need_refresh = 0;
    //    flush_screen();
    //}
    // flush the buffer
}

// number of currently installed
// terminals: when it reaches 0 we have to free the charmap

static int terminals = 0;


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
    unsigned console_w = (dev->width * 9 ) / 10,
             console_h = (dev->height * 95 ) / 100;
#ifdef BIGGER_FONT
    d->ncols       = console_w / TERMINAL_FONTWIDTH / 2 - 1;
    d->term_nlines = console_h / TERMINAL_LINE_HEIGHT / 2 - 1;
#else
    d->ncols       = console_w / TERMINAL_FONTWIDTH - 1;
    d->term_nlines = console_h / TERMINAL_LINE_HEIGHT - 1;
#endif

    // align width on 16 pixels
    d->ncols &= ~0x0f;

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


    // allocate the terminal buffer
    // to minimize the number of heap allocations,
    // we realloc our data buffer
    d = this->data = realloc(d, 
                        sizeof(struct data) 
                     + d->ncols * d->nlines * sizeof(struct Char));

    d->char_buffer = ((void*) d) + sizeof(struct data);


    //d->timerID = apic_create_timer(
    //                    (timer_callback_t)terminal_update, 
    //                    UPDATE_PERIOD_MS,
    //                    this);
//
    if(terminals++ == 0)
        loadBMP_24b_1b(&_binary_charmap_bmp, &charmap);
    

    //set_terminal_handler(this, write_string);

    terminal_set_colors(this, 0xfff0a0, 0x212121);
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
    
    if(d->timerID != INVALID_TIMER_ID)
        apic_delete_timer(d->timerID);

    // the allocation of data also 
    // contains the char buffer
    free(d);
    //if(active_terminal == this)   
    //    active_terminal = NULL;
}


void terminal_clear(driver_t* this) {
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


void terminal_set_fgcolor(driver_t* this, uint32_t c) {
    struct data* restrict d = this->data;
    d->current_fgcolor = c;
}
void terminal_set_bgcolor(driver_t* this, uint32_t c) {
    struct data* restrict d = this->data;
    d->current_bgcolor = c;
}

/*
// return a ptr to the actual char at pos 
static struct Char* get_Char_at(int l, int c) {
    return & buffer [ c + l * ncols ];
}
*/

static void move_buffer(driver_t* this, int lines) {
    struct data* restrict d = this->data;

    d->need_refresh = true;

    if(lines > 0) {// scroll backward
        size_t bytes = d->ncols * lines;
        size_t buff_size = d->nlines * d->ncols;

        memmove(
            d->char_buffer, 
            d->char_buffer + bytes,
            sizeof(struct Char)*(buff_size - bytes)
        );

        // cannot touch the first one: it is already written
        for(unsigned i = 1; i < bytes; i++) {
            d->char_buffer[i+buff_size-bytes] = make_Char(this,0);
        }
    }
}

static void next_line(driver_t* this) {
    struct data* restrict d = this->data;
    d->cur_col = 0;
    d->cur_line++;
    if(d->cur_line >= d->nlines) {
        d->cur_line = d->nlines-4;

        move_buffer(this, 4);
    }  
    else if(d->cur_line >= d->first_line + d->term_nlines) {
        d->first_line++;
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
    
    if(d->cur_col >= d->ncols) {
        next_line(this);
    }
    
    d->char_buffer[d->ncols * d->cur_line + d->cur_col] = make_Char(this, c);
    
    struct Char* ch = &d->char_buffer[d->ncols * d->cur_line + d->cur_col];
    
    if(!d->need_refresh)
        print_char(this, ch, d->cur_line - d->first_line, d->cur_col);
    
    d->cur_col += 1;
}


// emplace the char in the buffer, and maybe draw 
static void emplace_char(driver_t* this, char c) {
    struct data* restrict d = this->data;

    switch(c) {
    default:
        // any character
        emplace_normal_char(this, c);
        break;
    
    case '\n':
    {
        for(unsigned i=d->cur_col;i < d->ncols; i++) 
        {
            //print_char(ch, cur_line - first_line, cur_col);
            emplace_normal_char(this, ' ');
        }
    }
        break;
    
    case '\t':
    {
        unsigned new_col = ((d->cur_col + TAB_SPACE) / TAB_SPACE) * TAB_SPACE;
        while(d->cur_col < new_col)
            emplace_char(this, ' ');

    }
        break;
    case '\r':
        d->cur_col = 0;
        break;
    }
}


static void print_char(driver_t* this, 
                       const struct Char* restrict c, 
                       int line, int col) {

    struct data* restrict d = this->data;
/*
    Pos srcpos = {
        FONTWIDTH  * c_x,
        FONTHEIGHT * c_y,
    };
    Rect dstrect = {
        .x = col  * FONTWIDTH, 
        .y = line * LINE_HEIGHT,

        .w = FONTWIDTH,
        .h = FONTHEIGHT,
    };
*/
    //imageDrawMask(charmap, &srcpos, &dstrect, c->fg_color, c->bg_color);
    //imageLower_draw(charmap, &srcpos, &dstrect);
    //imageLower_blitBinaryMask(
    //    charmap,
    //    FONTWIDTH * c_x,  FONTHEIGHT * c_y,
    //    col  * FONTWIDTH, line * LINE_HEIGHT,
    //    FONTWIDTH, FONTHEIGHT,
    //    c->bg_color, c->fg_color);
    //
    /*
    Rect interlineRect = {
        .x = col  * FONTWIDTH, 
        .y = line * LINE_HEIGHT + FONTHEIGHT,
        .w = FONTWIDTH,
        .h = INTERLINE,
    };
*/
#ifdef BIGGER_FONT
    blitcharX2((struct framebuffer_dev *)this->device, 
                &charmap, c->c, c->fg_color, c->bg_color,
                d->margin_left + 2 * col  * TERMINAL_FONTWIDTH, 
                d->margin_top  + 2 * line * TERMINAL_LINE_HEIGHT);
    

#else

    blitchar((struct framebuffer_dev *)this->device, 
             &charmap, c->c, c->fg_color, c->bg_color,
             d->margin_left + col  * TERMINAL_FONTWIDTH, 
             d->margin_top  + line * TERMINAL_LINE_HEIGHT);
#endif

}

static void flush_screen(driver_t* this) {
    struct data* d = this->data;
        // begins at the terminal's first line
    const struct Char* curr = d->char_buffer + d->first_line * d->ncols;

    for(size_t l = 0; l < d->term_nlines; l++) {
        for(size_t c = 0; c < d->ncols; c++) {
            print_char(this, curr++, l, c);
        }
    }
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


void set_terminal_handler(driver_t* this, terminal_handler_t h) {
    struct data* restrict d = this->data;
    d->terminal_handler = h;
}

void terminal_set_colors(driver_t* this, 
                         uint32_t foreground, 
                         uint32_t background) 
{
    struct data* restrict d = this->data;

    d->current_fgcolor = foreground;
    d->current_bgcolor = background;
}
