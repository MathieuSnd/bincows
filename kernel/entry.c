#include <stddef.h>
#include <stdint.h>
#include <stivale2.h>

#include "memory/gdt.h"
#include "video/video.h"
#include "video/terminal.h"
#include "acpi/acpi.h"
#include "int/apic.h"
#include "int/idt.h"

#include "drivers/hpet.h"
#include "drivers/pcie.h"

#include "memory/physical_allocator.h"
#include "memory/paging.h"
#include "memory/vmap.h"
#include "memory/kalloc.h"

#include "lib/sprintf.h"
#include "lib/string.h"
#include "lib/logging.h"
#include "lib/common.h"
#include "lib/registers.h"
 

#define KERNEL_STACK_SIZE 8192

 // 8K stack
static uint8_t stack_base[KERNEL_STACK_SIZE] __attribute__((section(".stack"))) __align(16);

#define INITIAL_STACK_PTR ((uintptr_t)(stack_base + KERNEL_STACK_SIZE))

// initial stack pointer 
const uintptr_t kernel_stack = INITIAL_STACK_PTR;

static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
        .next = 0
    },
    .flags = 0
};

static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag = {
    // Same as above.
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        .next = (uint64_t)&terminal_hdr_tag
    },
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 32
};
 
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr = {
    .entry_point = 0,
    .stack = INITIAL_STACK_PTR,
    .flags = (1 << 1), // Bit 1: stivale2 gives us high half pointers 

    .tags = (uintptr_t)&framebuffer_hdr_tag // next tag in the linked list
};
 
static const void *stivale2_get_tag(const struct stivale2_struct *stivale2_struct, 
                                    uint64_t id) {
    struct stivale2_tag *current_tag = (void *)stivale2_struct->tags;
    // travel the linked list
    for (;;) {
        // We assert that these are not NULL afterwards, when the terminal
        // is successfully initialized
        if(current_tag == NULL)
            return NULL;

        if (current_tag->identifier == id) {
            return current_tag;
        }
 
         current_tag = (void *)current_tag->next;
    }    
}


#define PRINT_VAL(v) kprintf(#v "=%ld\n", (uint64_t)v);
#define PRINT_HEX(v) kprintf(#v "=%lx\n", (uint64_t)v);

// const char but represents a big string
extern const char _binary_bootmessage_txt;

// print all chars

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static void debug_terminal() {
    char buff[256];
    for(int i = 0; i < 256; i++)
        buff[i] = i+1;
    kputs(buff);
}

static void print_fb_infos(struct stivale2_struct_tag_framebuffer* fbtag) {
    PRINT_VAL(fbtag->framebuffer_width);
    PRINT_VAL(fbtag->framebuffer_height);
    PRINT_VAL(fbtag->framebuffer_pitch);
    PRINT_VAL(fbtag->framebuffer_bpp);
    PRINT_HEX(fbtag->framebuffer_addr);
}

#pragma GCC diagnostic pop


static void init_memory(const struct stivale2_struct_tag_memmap* memmap_tag,
                        const struct stivale2_struct_tag_framebuffer* fbtag) {
    klog_debug("init memory...");
    init_physical_allocator(memmap_tag);


    init_paging(memmap_tag);

// map MMIOs
    map_pages(
        early_virtual_to_physical((void *)fbtag->framebuffer_addr), 
        MMIO_BEGIN,
        (fbtag->framebuffer_height * fbtag->framebuffer_pitch+0x0fff) / 0x1000,
        PRESENT_ENTRY
    );

// map lapic & hpet registers
    map_acpi_mmios();

// init kernel heap
    kheap_init();
}


// Registers %rbp, %rbx and %r12 through %r15 “belong” to the calling functio
// The following will be our kernel's entry point.
void _start(struct stivale2_struct *stivale2_struct) {


    // Let's get the terminal structure tag from the bootloader.
    const struct stivale2_struct_tag_terminal*    term_str_tag;
    const struct stivale2_struct_tag_memmap*      memmap_tag;
    const struct stivale2_struct_tag_framebuffer* fbtag;
    const struct stivale2_struct_tag_rsdp*        rsdp_tag_ptr;

    term_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
    memmap_tag   = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);
    fbtag        = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
    rsdp_tag_ptr = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_RSDP_ID);


    // term_str_tag == NULL is not a blocking
    // errror: stivale2 terminal is only used 
    // to print potential errors occuring when
    // initializing bincows' terminal
    if (term_str_tag != NULL) {
        void *term_write_ptr = (void *)term_str_tag->term_write;
    // the default terminal handler does nothing        
        set_terminal_handler(term_write_ptr);
    }
        
 
 // print all logging messages
    set_logging_level(LOG_LEVEL_DEBUG);

    setup_isrs();
    read_acpi_tables((void*)rsdp_tag_ptr->rsdp);

    init_memory(memmap_tag, fbtag);


 // first initialize our terminal 
    initVideo(fbtag, (void *)MMIO_BEGIN);
    init_gdt_table();
// we cannot use stivale2 terminal
// after loading our gdt
// so we need to load our gdt after our
// terminal is successfully installed 
    
    setup_terminal();
    append_paging_initialization();
    terminal_set_colors(0xfff0a0, 0x212121);
    terminal_clear();

        
    kputs(&_binary_bootmessage_txt);
    
    kprintf("boot logs:\n");
    kputs(klog_get());
    klog_flush();

    pcie_init();


    hpet_init();
    apic_setup_clock();


    for(;;) {
        asm volatile("hlt");
        kprintf("%lu\r", clock());
    }

    __builtin_unreachable();
}