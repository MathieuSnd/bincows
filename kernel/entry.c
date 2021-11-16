#include <stddef.h>
#include <stdint.h>
#include <stivale2.h>

#include "memory/gdt.h"
#include "video/video.h"
#include "klib/sprintf.h"
#include "klib/string.h"
#include "video/terminal.h"
#include "acpi/acpi.h"
#include "common.h"
#include "regs.h"
#include "int/apic.h"

#include "int/idt.h"
#include "memory/physical_allocator.h"
 

#define KERNEL_STACK_SIZE 8192
 // 8K stack
static uint8_t stack_base[KERNEL_STACK_SIZE] __align(16);

#define INITIAL_STACK_PTR ((uintptr_t)(stack_base + KERNEL_STACK_SIZE))

// initial stack pointer 
const uintptr_t kernel_stack = INITIAL_STACK_PTR;

static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    // All tags need to begin with an identifier and a pointer to the next tag.
    .tag = {
        // Identification constant defined in stivale2.h and the specification.
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
        // If next is 0, it marks the end of the linked list of header tags.
        .next = 0
    },
    // The terminal header tag possesses a flags field, leave it as 0 for now
    // as it is unused.
    .flags = 0
};


// We are now going to define a framebuffer header tag, which is mandatory when
// using the stivale2 terminal.
// This tag tells the bootloader that we want a graphical framebuffer instead
// of a CGA-compatible text mode. Omitting this tag will make the bootloader
// default to text mode, if available.
static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag = {
    // Same as above.
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        // Instead of 0, we now point to the previous header tag. The order in
        // which header tags are linked does not matter.
        .next = (uint64_t)&terminal_hdr_tag
    },
    // We set all the framebuffer specifics to 0 as we want the bootloader
    // to pick the best it can.
    .framebuffer_width  = 800,
    .framebuffer_height = 600,
    .framebuffer_bpp    = 32
};
 
// The stivale2 specification says we need to define a "header structure".
// This structure needs to reside in the .stivale2hdr ELF section in order
// for the bootloader to find it. We use this __attribute__ directive to
// tell the compiler to put the following structure in said section.
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr = {
    // The entry_point member is used to specify an alternative entry
    // point that the bootloader should jump to instead of the executable's
    // ELF entry point. We do not care about that so we leave it zeroed.
    .entry_point = 0,
    // Let's tell the bootloader where our stack is.
    // We need to add the sizeof(stack) since in x86(_64) the stack grows
    // downwards.
    .stack = INITIAL_STACK_PTR,
    // Bit 1, if set, causes the bootloader to return to us pointers in the
    // higher half, which we likely want.
    .flags = (1 << 1),
    // This header structure is the root of the linked list of header tags and
    // points to the first one in the linked list.
    .tags = (uintptr_t)&framebuffer_hdr_tag
};
 
// We will now write a helper function which will allow us to scan for tags
// that we want FROM the bootloader (structure tags).
void *stivale2_get_tag(struct stivale2_struct *stivale2_struct, uint64_t id) {
    struct stivale2_tag *current_tag = (void *)stivale2_struct->tags;
    for (;;) {
        // If the tag pointer is NULL (end of linked list), we did not find
        // the tag. Return NULL to signal this.
        if (current_tag == NULL) {
            return NULL;
        }
 
        // Check whether the identifier matches. If it does, return a pointer
        // to the matching tag.
        if (current_tag->identifier == id) {
            return current_tag;
        }
 
        // Get a pointer to the next tag in the linked list and repeat.
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


// Registers %rbp, %rbx and %r12 through %r15 “belong” to the calling functio
// The following will be our kernel's entry point.
void _start(struct stivale2_struct *stivale2_struct) {


    // Let's get the terminal structure tag from the bootloader.
    struct stivale2_struct_tag_terminal* term_str_tag;
    struct stivale2_struct_tag_memmap*   memmap_tag;
    term_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
    memmap_tag   = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);

    // Check if the tag was actually found.
    if (term_str_tag == NULL) {
        // It wasn't found, just hang...
        for (;;) {
            asm volatile("hlt");
        }
        __builtin_unreachable();
    }
 
    // Let's get the address of the terminal write function.
    void *term_write_ptr = (void *)term_str_tag->term_write;
    
    
 
    set_terminal_handler(term_write_ptr);

    struct stivale2_struct_tag_framebuffer fbtag;
    struct stivale2_struct_tag_framebuffer* _fbtag = stivale2_get_tag(stivale2_struct,0x506461d2950408fa);
    memcpy(&fbtag,    _fbtag,       sizeof(fbtag));

    struct stivale2_struct_tag_rsdp* rsdp_tag_ptr = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_RSDP_ID);

    assert(rsdp_tag_ptr != NULL);

    uint64_t rsdp_location = rsdp_tag_ptr->rsdp;




    Image sc = {.w    =        fbtag.framebuffer_width,
                .h    =        fbtag.framebuffer_height,
                .pitch=        fbtag.framebuffer_pitch,
                .bpp  =        fbtag.framebuffer_bpp,
                .pix  = (void*)fbtag.framebuffer_addr};
    
    init_gdt_table();

    initVideo(&sc);


    setup_terminal();
 
    setup_isr();
    terminal_set_colors(0xf0f0f0, 0x007000);
    terminal_clear();
    kputs(&_binary_bootmessage_txt);
    read_acpi_tables((void*)rsdp_location);
    kputs("DONE\n");

    init_physical_allocator(memmap_tag);


    apic_setup_clock();


    


    for(;;) {
        asm volatile("hlt");
        kprintf("%lu\r", clock());
    }


   for(;;) {
       asm volatile ("hlt");
    }
    __builtin_unreachable();


    //*ptr = 0xfffa24821;
    asm volatile ("sti");
    asm volatile ("hlt");

    for(size_t i = 0; i < fbtag.framebuffer_height; i++) {
        uint8_t* base = (uint8_t*)fbtag.framebuffer_addr + fbtag.framebuffer_pitch*i;
        for(size_t j = 0; j < fbtag.framebuffer_width; j++) {
            uint32_t* px = (uint32_t*)&base[4*j];
            *px = 0xffffff00;
        }
    }
    //*(uint64_t*)(term_write_ptr) = 0xfeac;
    //print_fun(buf, 2);

//    kprintf("Bincows beta");
 
    // We're done, just hang...
    for (;;) {
        asm ("hlt");
    }
}