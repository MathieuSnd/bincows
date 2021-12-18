#pragma once

#include "../lib/string_t.h"

typedef struct driver driver_t;
/*
    generic device:
    every device should derive from this
    
*/
typedef struct dev {
    unsigned type;
    string_t name;
    driver_t* driver;
} dev_t;


void register_dev(struct dev*);

// this function does not
// remove the drivers!
// it only free the memory
void free_all_devices(void);

