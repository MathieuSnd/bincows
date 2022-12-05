#include "ps2kb.h"
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


static int append_buffer_event(struct ps2_event* ev) {
    int len = sizeof(*ev);
    void* buf = ev;
    
    // this way the process is easier
    assert(len < BUFFER_SIZE);

    int wrap_end = (int) buff_head + len - BUFFER_SIZE;


    if((buff_head < buff_tail && buff_head + len >= buff_tail)
    ||(buff_tail < buff_head && wrap_end >= (int)buff_tail)) {
        log_warn("ps2 keyboard overrun");
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

void ps2kb_poll_wait_for_key(uint8_t key) {
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
    if(ev.type == KEYRELEASED && ev.scancode == PS2KB_ESCAPE)
        lazy_shutdown = 1;

    
    append_buffer_event(&ev);
}


static void irq_handler(struct driver* dr) {
    // no driver context: unused 
    // there is not point in having a driver
    // context as it could only be one PS/2 controller
    (void) dr;

    uint8_t status = inb(0x64);
    
    // sometimes, interrupts
    // are triggered eventhough no
    // key is pressed or released
    if(status & 1)
        process_byte(inb(0x60));

    pic_eoi(1);
}

void ps2kb_init(void) {
    
    log_debug("init ps/2 keyboard...");

    register_irq(41, irq_handler, NULL);
    
    unsigned status = inb(0x64);
    if(status & 0xc0) {
        // error
        log_warn("ps/2 controller error: %u", status);
        return;
    }

// enable ps2 through config byte
    uint8_t config;
    command_byte(0x20); // read config byte command
    config = get_byte();

    command_byte(0x6);
    command_byte(config | 1);
    // flush output buffer
    flush_output_buffer();

    pic_mask_irq(1, 0);

}


static int ps2kb_read(void* arg, void* buf, size_t begin, size_t count) {
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

static int ps2kb_write(void* arg, const void* buf, size_t begin, size_t count) {
    (void) arg;
    (void) buf;
    (void) begin;
    (void) count;
    log_warn("tried to write in read-only dev file /dev/ps2kb");
    // unwritable
    return -1;
}


void ps2kb_register_dev_file(const char* filename) {


    int r = devfs_map_device((devfs_file_interface_t){
        .arg   = NULL,
        .read  = (void*)ps2kb_read,
        .write = (void*)ps2kb_write,
        .rights = {.read = 1, .write = 0, .seekable = 0},
        .file_size = ~0llu,
    }, filename);

    // r = 0 on success
    assert(!r);
}