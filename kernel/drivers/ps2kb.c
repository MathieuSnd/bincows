#include "ps2kb.h"
#include "../lib/logging.h"
#include "../lib/registers.h"
#include "../lib/sprintf.h"
#include "../int/idt.h"
#include "../int/pic.h"
#include "../lib/registers.h"

static void kbevent_default_handler(const struct kbevent* event) {
    (void) event;
}

static kbevent_handler handler = kbevent_default_handler;

static char lshift_state, rshift_state; 

static uint8_t get_byte(void) {
    // wait for the output bufer
    // to be filled
    while((inb(0x64) & 1) == 0) {
        asm volatile("pause");
    }

    return inb(0x60);
}

// outputs a command to the controller
static void command_byte(uint8_t b) {

    // wait for the input bufer
    // to be empty
    while((inb(0x64) & 2) == 1) {
        asm volatile("pause");
    }

    outb(0x64, b);
}



extern const char ps2_azerty_table_lowercase[];
extern const char ps2_azerty_table_uppercase[];



static uint8_t leds_state = 0; 
// by default, leds are disabled 


int is_caps(void) {
    // 3rd bit = 3rd led = caps
    int s = (leds_state / 4) +  (lshift_state | rshift_state);
    return s % 2;
}


static int process_leds(uint8_t b) {
    if(b == 0xba) {// caps lock
        // flush data buffer
        inb(0x60);
        command_byte(0xED);
        leds_state = leds_state ^ 4;

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
        .keycode  = b&0x7f,
    };

    if(ev.keycode == 0x2A) {
        lshift_state = ev.type;
        return;
    }
    else if(ev.keycode == 0x36) {
        rshift_state = ev.type;
        return;
    }


    if(is_caps())
        ev.scancode = ps2_azerty_table_uppercase[ev.keycode];
    else
        ev.scancode = ps2_azerty_table_lowercase[ev.keycode];
    
    printf("%c", ev.scancode);
    
    handler(&ev);
}


static void __attribute__((interrupt)) irq_handler(void* r) {

    process_byte(inb(0x60));

    pic_eoi(1);
}

void ps2kb_init(void) {
    
    log_debug("init ps/2 keyboard...");
    
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
    log_debug("zdf");

    command_byte(0x6);
    command_byte(config | 1);


    set_irq_handler(17, irq_handler);
    pic_mask_irq(1, 0);

    for(;;)
    asm("hlt");
    //log_debug("dzdvf");
}


void ps2kb_set_event_callback(kbevent_handler h) {
    handler = h;
}