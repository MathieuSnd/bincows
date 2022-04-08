#include "ps2kb.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../int/irq.h"
#include "../int/pic.h"
#include "../lib/registers.h"

#include "../fs/devfs/devfs.h"


static char lshift_state, rshift_state, altgr_state; 

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
#define BUFFER_SIZE 64
static char file_buffer[BUFFER_SIZE];

static volatile unsigned buff_tail = 0, buff_head = 0;


static void append_event(const struct kbevent* ev) {
    if(ev->type == KEYRELEASED && ev->scancode == PS2KB_ESCAPE) {
        shutdown();
        __builtin_unreachable();
    }
    
    if(ev->type == KEYPRESSED && ev->keycode != 0) {
        if((buff_head + 1) % BUFFER_SIZE == buff_tail) {
            // buffer is full
            return;
        }
        else {
            file_buffer[buff_head] = ev->keycode;
            buff_head = (buff_head + 1) % BUFFER_SIZE;
        }
    }
};

static int block_pop_chars(char* c, size_t n) {
    size_t i;

    for(i = 0; i < n; i++) {
        while(buff_tail == buff_head) {
            // buffer is empty
            sleep(20);
        }
    
        c[i] = file_buffer[buff_tail];
        buff_tail = (buff_tail + 1) % BUFFER_SIZE;
    }
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
        leds_state = 0xff;
        command_byte(0xED);
        
        // io wait
        outb(0x80, 0);
        
        command_byte(leds_state);
        return 1;
    }

    return 0;
}

static void process_byte(uint8_t b) {

    if(b == 0xFA) // ACK
        return;
    else if(b == 0xFE) { // resend
        // well shit
        log_warn("something went wrong with the keyboard");
        return;
    } 
    else if(process_leds(b))
        return;
    struct kbevent ev = {
        .type     = (b&0x80) ? KEYRELEASED : KEYPRESSED,
        .scancode  = b&0x7f,
    };

    if(ev.keycode == 0x2A) {
        lshift_state = ev.type;
        return;
    }
    else if(ev.keycode == 0x36) {
        rshift_state = ev.type;
        return;
    }
    else if(ev.keycode == 56) {
        altgr_state = ev.type;
    }

    if(altgr_state)
        ev.keycode = ps2_azerty_table_altgr    [ev.scancode];
    else if(is_caps())
        ev.keycode = ps2_azerty_table_uppercase[ev.scancode];
    else
        ev.keycode = ps2_azerty_table_lowercase[ev.scancode];
    
    append_event(&ev);
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




static int devfs_read(void* arg, void* buf, size_t begin, size_t count) {
    (void) arg;

    return block_pop_chars(buf, count);
}

static int devfs_write(void* arg, const void* buf, size_t begin, size_t count) {
    (void) buf;
    (void) count;
    panic("bordel");
    // unwritable
    return 0;
}


void ps2kb_register_dev_file(const char* filename) {


    int r = devfs_map_device((devfs_file_interface_t){
        .arg   = NULL,
        .read  = (void*)devfs_read,
        .write = (void*)devfs_write,
        .file_size = ~0llu,
    }, filename);

    // r = 0 on success
    assert(!r);
}