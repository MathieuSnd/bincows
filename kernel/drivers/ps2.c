#include "ps2.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../int/irq.h"
#include "../int/pic.h"
#include "../lib/registers.h"
#include "../lib/panic.h"
#include "../lib/string.h"
#include "../lib/time.h"
#include "../fs/devfs/devfs.h"

static
int tolower(int c) {
    if(c >= 'A' && c <= 'Z')
        return c + 32;
    return c;
}



static char lshift_state, rshift_state, altgr_state; 
static char ctrl_state;

static uint8_t get_byte(void) {
    // wait for the output bufer
    // to be filled
    while((inb(0x64) & 1) == 0) {
        asm volatile("pause");
    }

    return inb(0x60);
}

static void flush_output_buffer(void) {
    // wait for the output bufer
    // to be empty
    while((inb(0x64) & 1) != 0) {
        inb(0x60);
    }
}


// input values circular buffer
#define BUFFER_SIZE 1000
static char file_buffer[BUFFER_SIZE];

static volatile unsigned buff_tail = 0, buff_head = 0;

// shutdown signal to the kernel process
extern int lazy_shutdown;


static int append_event(struct ps2_event* ev) {
    int len = sizeof(*ev);
    void* buf = ev;
    
    // this way the process is easier
    assert(len < BUFFER_SIZE);

    int wrap_end = (int) buff_head + len - BUFFER_SIZE;


    if((buff_head < buff_tail && buff_head + len >= buff_tail)
    ||(buff_tail < buff_head && wrap_end >= (int)buff_tail)) {
        log_warn("ps2 buffer overrun");
        return -1;
    }
    else {

        int end = buff_head + len;
        if(end >=  BUFFER_SIZE) {
            int first_part = BUFFER_SIZE - buff_head;

            // must cut in two parts
            memcpy(
                file_buffer + buff_head,
                buf,
                first_part
            );


            buf += first_part;
            buff_head = 0;
            len -= first_part;
        }

        memcpy(
            file_buffer + buff_head,
            buf,
            len
        );
        buff_head += len;

        assert(buff_head <= BUFFER_SIZE);
    }
    return 0;
}


static int block_pop_chars(char* c, size_t n) {
    size_t i;
    

    assert(interrupt_enable());
    _cli();

    for(i = 0; i < n; i++) {
        while(buff_tail == buff_head) {
            // buffer is empty
            
            _sti();
            int cause = sleep(20);

            if(cause) {
                // a signal interupted the sleep
                return i;
            }
            _cli();
        }

    
        c[i] = file_buffer[buff_tail];
        //printf("%u ", c[i]);
        buff_tail = (buff_tail + 1) % BUFFER_SIZE;
    }
    _sti();

    return i;
}


// outputs a command to the controller
static void command_byte(uint8_t b) {

    // wait for the input bufer
    // to be empty
    while((inb(0x64) & 2) != 0) {
        asm volatile("pause");
    }

    outb(0x64, b);
}

static int mouse_command(uint8_t b) {
    // wait for the input bufer
    // to be empty
    while((inb(0x64) & 2) != 0)
        asm volatile("pause");

    outb(0x64, 0xD4);

    while((inb(0x64) & 2) != 0)
        asm volatile("pause");

    outb(0x60, b);



    while((inb(0x64) & 1) == 0)
        asm volatile("pause");

    uint8_t ack = inb(0x60);

    // only the first byte matters, drop
    // the rest
    while(inb(0x64) & 1)
        inb(0x60);

    if(ack != 0xFA) // didn't acknowledge the command
        return -1;
    else // all good
        return 0;
}



extern const char ps2_azerty_table_lowercase[];
extern const char ps2_azerty_table_uppercase[];
extern const char ps2_azerty_table_altgr    [];



static uint8_t leds_state = 0; 
// by default, leds are disabled 


int is_caps(void) {
    // 3rd bit = 3rd led = caps
    int s = (leds_state / 4) +  (lshift_state | rshift_state);
    return s % 2;
}


void ps2_trigger_CPU_reset(void) {
    command_byte(0xFE);
    // reset command
}

void ps2_poll_wait_for_key(uint8_t key) {
    while(get_byte() != key)
        ;
}

static int process_leds(uint8_t b) {
    if(b == 0xba) {// caps lock
        // flush data buffer
        inb(0x60);
        leds_state ^= 4;
        command_byte(0xED);
        
        // io wait
        outb(0x80, 0);
        
        command_byte(leds_state);
        return 1;
    }

    return 0;
}




static void process_byte(uint8_t b) {

    // if non 0, a sequence is running
    static int seq = 0;

    if(b == 0xFA) // ACK
        return;
    else if(b == 0xFE) { // error
        log_warn("something went wrong with the keyboard");
        return;
    } 
    else if(b == 0xE0) {
        // sequence
        seq = 1;
        return;
    }
    else if(process_leds(b))
        return;

    struct ps2_event ev = {
        .type     = (b&0x80) ? KEYRELEASED : KEYPRESSED,
        
        .scancode  = b&0x7f,
        .keycode   = 0,
        .unix_sequence = {0}
    };


    // process keycode
    // and unix sequence

    int pressed = ev.type == KEYPRESSED;

    // non 0 if the key is a modifier (ctrl, alt)
    // and should not have a unix sequence
    int modifier_key = 0;

    // @todo remove ?
    if(ev.scancode == 0x2A) {
        lshift_state = pressed;
        modifier_key = 1;
    }
    else if(ev.scancode == 0x36) {
        rshift_state = pressed;
        modifier_key = 1;
    }


    else if(ev.scancode == 56) {
        altgr_state =  pressed;
        modifier_key = 1;
    }

    else if(ev.scancode == 0x1d) {
        ctrl_state = pressed;
        modifier_key = 1;
    }

    if(seq) {
        switch(ev.scancode) {
                break;
            case 0x48: // up cursor
                strcpy(ev.unix_sequence, "\x1b[A");
                break;
            case 0x50: // down cursor
                strcpy(ev.unix_sequence, "\x1b[B");
                break;
            case 0x4d: // right cursor
                strcpy(ev.unix_sequence, "\x1b[C");
                break;
            case 0x4b: // left cursor
                strcpy(ev.unix_sequence, "\x1b[D");
                break;
            default:
                ev.keycode = 0;
                break;
        }
        ev.scancode |= (0xE0 << 8);

        seq = 0;
    }
    else if(!modifier_key) {
        if(altgr_state)
            ev.keycode = ps2_azerty_table_altgr    [ev.scancode];
        else if(is_caps())
            ev.keycode = ps2_azerty_table_uppercase[ev.scancode];
        else
            ev.keycode = ps2_azerty_table_lowercase[ev.scancode];


        if(ctrl_state) {
            char c = tolower(ev.keycode);
            if(c > 'a' && c < 'z') {
                c = c - 'a' + 1;
                ev.unix_sequence[0] = c;
            }
        }
        else ev.unix_sequence[0] =ev.keycode;
    }


    // @todo remove
    if(ev.type == KEYRELEASED && ev.scancode == ps2_ESCAPE)
        lazy_shutdown = 1;

    
    append_event(&ev);
}


static void irq_handler(struct driver* dr) {
    // no driver context: unused 
    // there is not point in having a driver
    // context as it could only be one PS/2 controller
    (void) dr;

    uint8_t status = inb(0x64);

    //if(status &)
    
    // sometimes, interrupts
    // are triggered eventhough no
    // key is pressed or released
    while(status & 1) {
        process_byte(inb(0x60));
        
        status = inb(0x64);
    }

    pic_eoi(1);
}


static void mouse_button_event(int buttons) {
    static int button_state;

    int pressed  = (~button_state &  buttons) & 0x03;
    int released = ( button_state & ~buttons) & 0x03;

    for(int i = 0; i < 3; i++) {
        struct ps2_event ev = {
            .mouse = {
                .button = 1 << i,
                .xrel = 0,
                .yrel = 0
            },
        };
        if(pressed & (1 << i)) {
            ev.type = MOUSE_PRESSED;
            append_event(&ev);
        }
        else if(released & (1 << i)) {
            ev.type = MOUSE_RELEASED;
            append_event(&ev);
        } 
    }

    button_state = buttons;
}


static void mouse_wait(void) {
    while((inb(0x64) & 1) == 0)
        asm("pause");
}

static void mouse_read(void) {
    mouse_wait();
    uint8_t b0    = inb(0x60);  mouse_wait();
    int16_t x_mov = inb(0x60);  mouse_wait();
    int16_t y_mov = inb(0x60);


    if((b0 & 8) == 0) {
        
        log_info("ps/2 mouse packet error %u", b0);
        return;
    }

    // pressed / unpressed event
    mouse_button_event(b0);

    int x_sign = b0 & 0x10 ? 1 : 0;
    int y_sign = b0 & 0x20 ? 1 : 0;


    int x_overflow = b0 & 0x40;
    int y_overflow = b0 & 0x80;

    int16_t x_rel = x_mov - (x_sign << 8);
    int16_t y_rel = y_mov - (y_sign << 8);

    if(x_overflow)
        x_rel *= 2;
    if(y_overflow)
        y_rel *= 2;


    if(x_rel | y_rel) {
        // cursor move 

        struct ps2_event ev = {
            .type = MOUSE_MOVE,
            .mouse = {
                .xrel = x_rel,
                .yrel = -y_rel,
                .button = 0,
            } 
        };
        append_event(&ev);
    }
} 


static void mouse_irq_handler(struct driver* unused) {
    (void) unused;
    uint8_t status = inb(0x64);

    while(status & 1) {
        mouse_read();
        status = inb(0x64);
    }

    pic_eoi(12);
}

void ps2_init(void) {
    
    log_debug("init ps/2 keyboard...");

    register_irq(41, irq_handler, NULL);
    register_irq(52, mouse_irq_handler, NULL);
    
    unsigned status = inb(0x64);
    if(status & 0xc0) {
        // error
        log_warn("ps/2 controller error: %u", status);
        return;
    }

    command_byte(0x20); // read config byte command
    uint8_t config = get_byte();
    config |= 0x03;
    config &= ~0x30; 

    command_byte(0x60);
    while((inb(0x64) & 2) != 0)
        asm volatile("pause");
    
    outb(0x60, config);



    command_byte(0xAE); // enable 1st PS/2 port (keyboard)
    command_byte(0xA8); // enable 2nd PS/2 port (mouse)


    mouse_command(0xF6); // load defaults
    mouse_command(0xF4); // enable ps2 mouse


    // flush output buffer
    flush_output_buffer();

    // unmask IRQs
    pic_mask_irq(1, 0);
    pic_mask_irq(12, 0);
}


static int ps2_read(void* arg, void* buf, size_t begin, size_t count) {
    (void) arg;
    (void) begin;

    if(!count)
        return 0;

    int rd = block_pop_chars(buf, count);
    if(!rd) {
        // call interrupted by signal
        // EINTR
        return -1;
    }
    return rd;
}

static int ps2_write(void* arg, const void* buf, size_t begin, size_t count) {
    (void) arg;
    (void) buf;
    (void) begin;
    (void) count;
    log_warn("tried to write in read-only dev file /dev/ps2");
    // unwritable
    return -1;
}


void ps2_register_dev_file(const char* filename) {


    int r = devfs_map_device((devfs_file_interface_t){
        .arg   = NULL,
        .read  = (void*)ps2_read,
        .write = (void*)ps2_write,
        .rights = {.read = 1, .write = 0, .seekable = 0},
        .file_size = ~0llu,
    }, filename);

    // r = 0 on success
    assert(!r);
}
