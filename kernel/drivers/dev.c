#include <stddef.h>
#include "dev.h"

static struct dev** devices = NULL;
static unsigned n_devices = 0;


void realloc_dev(void) {
    static unsigned buffsize = 0;
    
    if(n_devices == 0)
        buffsize = 0;
    else if(buffsize == 0)
        buffsize = n_devices;
        
    else if(n_devices > buffsize)
        buffsize *= 2;
    else if(n_devices < buffsize / 2)
        buffsize /= 2;

    devices = realloc(devices, 
                      buffsize * sizeof(struct dev*));
}



void register_dev(struct dev* dev) {
    unsigned i = n_devices++;
    realloc_dev();
    
    devices[i] = dev;
}


void free_all_devices(void) {

    for(unsigned i = 0; i < n_devices; i++) {
        string_free(&devices[i]->name);
        free(devices[i]);
    }

    n_devices = 0;
    realloc_dev();
}

