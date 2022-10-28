#include <lai/host.h>
#include "acpitables.h"
#include "acpi.h"

#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../memory/heap.h"

// this file impelments the functions needed 
// by the LAI library

#ifdef USE_LAI


// OS-specific functions.
void *laihost_malloc(size_t s) {
    return malloc(s);
}
void *laihost_realloc(void* p, size_t s) {
    return realloc(p, s);
}
void laihost_free(void* p) {
    free(p);
}

void laihost_log(int level, const char * msg) {
    if(level == LAI_DEBUG_LOG)
        log_debug("LAI: %s", msg);
    else
        log_warn ("LAI: %s", msg);
}
void laihost_panic(const char* msg) {
    char buff[1024];
    sprintf(buff, "LAI: %s", msg);
    panic(buff);
}


// only called early: 
void *laihost_scan(char * name, size_t c) {
    struct XSDT* xsdt = get_xsdt_location();

    size_t n_entries = (xsdt->header.length - sizeof(xsdt->header)) / sizeof(void*);

    uint32_t* raw = (uint32_t*)name;
    
    for(size_t i = 0; i < n_entries; i++) {
        struct ACPISDTHeader* table = xsdt->entries[i];

        if(*raw == table->signature.raw)
            if(c-- == 0)
                return table;
    }

// no such table
    return NULL;
}

void *laihost_map(size_t, size_t);
void laihost_outb(uint16_t, uint8_t);
void laihost_outw(uint16_t, uint16_t);
void laihost_outd(uint16_t, uint32_t);
uint8_t laihost_inb(uint16_t);
uint16_t laihost_inw(uint16_t);
uint32_t laihost_ind(uint16_t);

void laihost_pci_writeb(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint8_t);
uint8_t laihost_pci_readb(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t);
void laihost_pci_writew(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint16_t);
uint16_t laihost_pci_readw(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t);
void laihost_pci_writed(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint32_t);
uint32_t laihost_pci_readd(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t);

void laihost_sleep(uint64_t);

void laihost_handle_amldebug(lai_variable_t *);


#endif

