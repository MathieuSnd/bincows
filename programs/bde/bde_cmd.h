#ifndef BDE_CMD_H
#define BDE_CMD_H

#include <stddef.h>
#include <stdint.h>

typedef size_t bde_mem_off_t;
#define BDE_DRAW
#define BDE_CREATE_WINDOW
#define BDE_EXECUTE


struct bde_cmd {
    int type;
    uint64_t cmdid;
    int cmd_size;

    int window_id;
    bde_mem_off_t buff_begin;
    int x, y;
    int w, h;
};

struct bde_resp {
    uint64_t cmdid;
    int window_id;
    int error;
};


struct bde_shared_space {
    uint64_t signature;             // 0x0BDEBDEBDEBDEBDE
    size_t space_len;               // written by bde
    uint64_t resp_tail;             // written by bde
    uint64_t resp_head;             // written by app
    struct bde_resp resp_buffer[8]; // written by bde
    uint8_t buffer[0];              // written by app
};


#endif//BDE_CMD_H
