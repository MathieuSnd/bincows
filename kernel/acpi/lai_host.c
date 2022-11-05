#ifdef USE_LAI
#include <lai/host.h>
#include "acpitables.h"
#include "acpi.h"

#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../lib/registers.h" 
#include "../memory/heap.h"
#include "../memory/vmap.h"

// this file impelments the functions needed 
// by the LAI library

 
// OS-specific functions.
void *laihost_malloc(size_t s) {
    return malloc(s);
}
void *laihost_realloc(void* p, size_t s, size_t o) {
    (void) o;
    return realloc(p, s);
}
void laihost_free(void* p, size_t s) {
    (void)s;
    free(p);
}

void laihost_log(int level, const char * msg) {
    if(level == LAI_DEBUG_LOG)
        log_debug("LAI: %s", msg);
    else
        log_warn ("LAI: %s", msg);
}
void laihost_panic(const char* msg) {
    static char buff[1024];
    sprintf(buff, "LAI: %s", msg);
    panic(buff);
}


// only called early: 
void *laihost_scan(const char * name, size_t c) {
    return (void *)acpi_get_table(name, c);
}

void *laihost_map(size_t, size_t);
void laihost_outb(uint16_t, uint8_t);
void laihost_outw(uint16_t, uint16_t);
void laihost_outd(uint16_t, uint32_t);
uint8_t laihost_inb(uint16_t port) {
    return inb(port);
}
uint16_t laihost_inw(uint16_t port) {
    return inw(port);
}
uint32_t laihost_ind(uint16_t port) {
    return ind(port);
}

void laihost_pci_writeb(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint8_t);
uint8_t laihost_pci_readb(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t);
void laihost_pci_writew(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint16_t);
uint16_t laihost_pci_readw(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t);
void laihost_pci_writed(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint32_t);
uint32_t laihost_pci_readd(uint16_t, uint8_t, uint8_t, uint8_t, uint16_t);

void laihost_sleep(uint64_t);

void laihost_handle_amldebug(lai_variable_t *);


#endif

