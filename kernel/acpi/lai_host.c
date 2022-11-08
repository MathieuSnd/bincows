#ifdef USE_LAI
#include <lai/host.h>
#include "acpitables.h"
#include "../memory/paging.h"
#include "acpi.h"
#include "../drivers/pcie/pcie.h"

#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../lib/registers.h" 
#include "../lib/time.h" 
#include "../memory/heap.h"
#include "../memory/vmap.h"

// this file impelments the functions needed 
// by the LAI library
 
// OS-specific functions.
void *laihost_malloc(size_t s) {
    void* p = malloc(s);
    assert(p);
    assert(is_in_heap(p));
    heap_defragment();
    return p;
}
void *laihost_realloc(void* p, size_t s, size_t o) {
    (void) o;
    
    assert(!p || is_in_heap(p));

    heap_defragment();
    
    p = realloc(p, s);
    
    assert(p);
    assert(is_in_heap(p));
    heap_defragment();
    
    return p;
}
void laihost_free(void* p, size_t s) {
    (void)s;
    assert(p);
    assert(is_in_heap(p));
    heap_defragment();
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

void *laihost_map(size_t paddr, size_t bytes) {
    // @todo map to private LAI virtual memory range
    // to ensure no overlap and to unmap easily 

    // map in the transalted memory range
    void*    vaddr    = translate_address((void*)paddr);
    uint64_t page_off = paddr & 0xfff;

    uint64_t pbase    = paddr & ~0xfff;
    uint64_t vbase    = (uint64_t)vaddr & ~0xfff;
    
    // first check if it is mapped already
    if(get_phys_addr(vaddr) != 0) {
        assert(get_phys_addr(vaddr) == paddr);
    }
    else {
        uint64_t end_off = page_off + bytes;

        assert(pbase);
        assert(vbase);

        int count = 1 + (end_off - 1) / 0x1000;
        // unmapped page
        map_pages(
            pbase,
            vbase,
            count,
            PRESENT_ENTRY | PL_RW | PL_XD
        );
    }
    
    return vaddr;
}

void laihost_outb(uint16_t p, uint8_t b) {
    outb(p,b);
}
void laihost_outw(uint16_t p, uint16_t w) {
    outw(p,w);
}

void laihost_outd(uint16_t p, uint32_t d) {
    outd(p,d);
}

uint8_t laihost_inb(uint16_t port) {
    return inb(port);
}
uint16_t laihost_inw(uint16_t port) {
    return inw(port);
}
uint32_t laihost_ind(uint16_t port) {
    return ind(port);
}



void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val) {
    uint8_t* base = (uint8_t*)pcie_config_space_base(seg, bus, slot, fun);

    *(uint8_t*)(base + offset) = val;
}

void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val) {
    uint8_t* base = (uint8_t*)pcie_config_space_base(seg, bus, slot, fun);

    *(uint16_t*)(base + offset) = val;
}

void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val) {
    uint8_t* base = (uint8_t*)pcie_config_space_base(seg, bus, slot, fun);

    *(uint32_t*)(base + offset) = val;
}

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset)  {
    const uint8_t* base = (uint8_t*)pcie_config_space_base(seg, bus, slot, fun);

    return *(uint8_t*)(base + offset);
}
uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset)  {
    const uint8_t* base = (uint8_t*)pcie_config_space_base(seg, bus, slot, fun);

    return *(uint16_t*)(base + offset);
}
uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset)  {
    const uint8_t* base = (uint8_t*)pcie_config_space_base(seg, bus, slot, fun);

    return *(uint32_t*)(base + offset);
}

void laihost_sleep(uint64_t ms) {
    sleep(ms);
}

void laihost_handle_amldebug(lai_variable_t *);


#endif

