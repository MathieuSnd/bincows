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

static void write_string(const char *string, size_t length);
static struct Char make_Char(char c);
static void print_char(const struct Char* restrict c, int line, int col);
static void flush_screen(void);

static terminal_handler_t terminal_handler = NULL;

// need to redraw the entire terminal
static bool need_refresh = false;


static void empty_terminal_handler(const char* s, size_t l) {
    (void) (s + l);
    // empty handler by default,
    // make sure not to execute the address 0 :)
}

#define NCOLS_MAX 133
#define TERMINAL_LINES_MAX 75


static void append_string(const char *string, size_t length);



static Image* charmap = NULL;
static struct Char* char_buffer = NULL;

static uint16_t ncols, nlines;
static uint16_t term_nlines;
static uint16_t first_line = 0;
static uint16_t cur_col, cur_line;

static uint32_t current_fgcolor = 0xa0a0a0;
static uint32_t current_bgcolor = 0;

static unsigned margin_left, margin_top;

#define UPDATE_PERIOD_MS (1000 / 60)


// "uint8_t" but designates a binary file 
extern uint8_t _binary_charmap_bmp;


static unsigned timerID = INVALID_TIMER_ID;



static char*    stream_buffer = NULL;

//static unsigned stream_buffer_size = 0;
//static volatile unsigned stream_buffer_content_size = 0;


// should be called every UPDATE_PERIOD ms
// should execute in an IRQ
void terminal_update(void) {
    return;
    static unsigned update_cur_line = 0;
    



    if(need_refresh) {
        need_refresh = 0;
        flush_screen();
    }
    // flush the buffer
}



void terminal_remove(void) {
    apic_delete_timer(timerID);
}

void terminal_install_early(void) {
    set_terminal_handler(empty_terminal_handler);

//    log_debug("install the terminal...");
//
//    assert(charmap == NULL);
//    assert(char_buffer == NULL);


    
    const Image* screenImage = getScreenImage();
    
// dynamicly create the terminal
// with right size
    unsigned console_w = (screenImage->w * 9 ) / 10,
             console_h = (screenImage->h * 95 ) / 100;
#ifdef BIGGER_FONT
    ncols       = console_w / TERMINAL_FONTWIDTH / 2 - 1;
    term_nlines = console_h / TERMINAL_LINE_HEIGHT / 2 - 1;
#else
    ncols       = console_w / TERMINAL_FONTWIDTH - 1;
    term_nlines = console_h / TERMINAL_LINE_HEIGHT - 1;
#endif

    nlines      = TERMINAL_N_PAGES * term_nlines;


    // calculate the margins 
#ifdef BIGGER_FONT
    margin_left = (screenImage->w - ncols       * 2 * TERMINAL_FONTWIDTH ) / 2;
    margin_top  = 0;//(screenImage->h - term_nlines * 2 * TERMINAL_FONTHEIGHT) / 2;
#else
    margin_left = (screenImage->w - ncols       * TERMINAL_FONTWIDTH ) / 2;
    margin_top  = (screenImage->h - term_nlines * TERMINAL_FONTHEIGHT) / 2;
#endif


    // allocate the terminal buffer
    char_buffer   = malloc(ncols * nlines * sizeof(struct Char) * 10);
    stream_buffer = malloc(ncols * term_nlines);


}

// finish intallation when memory
// is well initialized
void terminal_install_late(void) {
    //timerID = apic_create_timer(terminal_update, UPDATE_PERIOD_MS);

    

    charmap = loadBMP_24b_1b(&_binary_charmap_bmp);
    
    
    set_terminal_handler(write_string);

    terminal_clear();
}


void terminal_clear(void) {

    cur_col    = 0;
    cur_line   = 0;
    first_line = 0;
    
    size_t buffer_len = nlines * ncols;

    struct Char* ptr = char_buffer;
    for(;buffer_len > 0; --buffer_len)
        *(ptr++) = make_Char(0);
    
    flush_screen();

}


void set_terminal_fgcolor(uint32_t c) {
    current_fgcolor = c;
}
void set_terminal_bgcolor(uint32_t c) {
    current_bgcolor = c;
}

/*
// return a ptr to the actual char at pos 
static struct Char* get_Char_at(int l, int c) {
    return & buffer [ c + l * ncols ];
}
*/

static void move_buffer(int lines) {
    need_refresh = true;

    if(lines > 0) {// scroll backward
        size_t bytes = ncols * lines;
        size_t buff_size = nlines * ncols;

        memmove(char_buffer, char_buffer + bytes, sizeof(struct Char)*(buff_size - bytes));        

        // cannot touch the first one: it is already written
        for(unsigned i = 1; i < bytes; i++) {
            char_buffer[i+buff_size-bytes] = make_Char(0);
        }
    }
}

static void next_line(void) {
    cur_col = 0;
    cur_line++;
    if(cur_line >= nlines) {
        cur_line = nlines-4;

        move_buffer(4);
    }  
    else if(cur_line >= first_line + term_nlines) {
        first_line++;
        need_refresh = true;
    }
}

// create the char struct
static struct Char make_Char(char c) {
    return (struct Char) {
        .fg_color = current_fgcolor,
        .c        = c,
        .bg_color = current_bgcolor
    };
}


static void emplace_normal_char(char c) {
    if(cur_col >= ncols) {
        next_line();
    }
    
    char_buffer[ncols * cur_line + cur_col] = make_Char(c);
    
    struct Char* ch = &char_buffer[ncols * cur_line + cur_col];
    
    if(!need_refresh)
        print_char(ch, cur_line - first_line, cur_col);
    
    cur_col += 1;
}


// emplace the char in the buffer, and maybe draw 
static void emplace_char(char c) {
    switch(c) {
    default:
        // any character
        emplace_normal_char(c);
        break;
    
    case '\n':
    {
        for(unsigned i=cur_col;i < ncols; i++) 
        {
            //print_char(ch, cur_line - first_line, cur_col);
            emplace_normal_char(' ');
        }
    }
        break;
    
    case '\t':
    {
        unsigned new_col = ((cur_col + TAB_SPACE) / TAB_SPACE) * TAB_SPACE;
        while(cur_col < new_col)
            emplace_char(' ');

    }
        break;
    case '\r':
        cur_col = 0;
        break;
    }
}


static void print_char(const struct Char* restrict c, int line, int col) {
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
    blitcharX2(charmap, c->c, c->fg_color, c->bg_color,
                margin_left + 2 * col  * TERMINAL_FONTWIDTH, 
                margin_top  + 2 * line * TERMINAL_LINE_HEIGHT);
    

#else

    blitchar(charmap, c->c, c->fg_color, c->bg_color,
                margin_left + col  * TERMINAL_FONTWIDTH, 
                margin_top  + line * TERMINAL_LINE_HEIGHT);
#endif

}

static void flush_screen(void) {
        // begins at the terminal's first line
    const struct Char* curr = char_buffer + first_line * ncols;

    for(size_t l = 0; l < term_nlines; l++) {
        for(size_t c = 0; c < ncols; c++) {
            print_char(curr++, l, c);
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


static void write_string(const char *string, size_t length) {
    need_refresh = false;

    for(;length>0;--length) {
        char c = *string++;
        
        if(!c)
            break;
        emplace_char(c);

    }
    if(need_refresh)
        flush_screen();
}


terminal_handler_t get_terminal_handler(void) {
    return terminal_handler;
}



void set_terminal_handler(terminal_handler_t h) {
    terminal_handler = h;
}

void terminal_set_colors(uint32_t foreground, uint32_t background) {
    current_fgcolor = foreground;
    current_bgcolor = background;
}
