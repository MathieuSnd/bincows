#pragma once

#include <stdint.h>

void ps2kb_init(void);

// keys
#define PS2KB_ESCAPE 1


struct kbevent {
    enum {
        KEYRELEASED = 0, 
        KEYPRESSED = 1
    } type;
    uint32_t keycode;
    uint32_t scancode;
};


void ps2_trigger_CPU_reset(void);

// poll untill the specified key is pressed
void ps2kb_poll_wait_for_key(uint8_t key);


void ps2kb_register_dev_file(const char* filename);

