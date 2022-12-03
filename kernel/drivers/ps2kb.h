#pragma once

#include <stdint.h>

void ps2kb_init(void);

// keys
#define PS2KB_ESCAPE 1

enum ps2_event_type {
    KEYPRESSED  = 1,
    KEYRELEASED = 2,
    KEYREPEAT   = 3,

    MOUSE_MOVE  = 5,
};


struct ps2_event {
    enum ps2_event_type type;

    int keycode;
    int scancode;

    // 0-terminated sequence
    char unix_sequence[8];
};



void ps2_trigger_CPU_reset(void);

// poll untill the specified key is pressed
void ps2kb_poll_wait_for_key(uint8_t key);


void ps2kb_register_dev_file(const char* filename);

