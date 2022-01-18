#include <stddef.h>
#include <stdint.h>
#include <stivale2.h>

#include "acpi/acpi.h"
#include "acpi/power.h"

#include "int/apic.h"
#include "int/idt.h"
#include "int/pic.h"

#include "drivers/terminal/video.h"
#include "drivers/terminal/terminal.h"
#include "drivers/hpet.h"
#include "drivers/ps2kb.h"
#include "drivers/pcie/pcie.h"
#include "drivers/pcie/scan.h"

#include "fs/gpt.h"

#include "memory/gdt.h"
#include "memory/physical_allocator.h"
#include "memory/paging.h"
#include "memory/vmap.h"
#include "memory/heap.h"

#include "lib/sprintf.h"
#include "lib/string.h"
#include "lib/logging.h"
#include "lib/common.h"
#include "lib/registers.h"
#include "lib/dump.h"
#include "lib/stacktrace.h"

#include "early_video.h"
 

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


// const char but represents a big string
extern const char _binary_bootmessage_txt;


static void read_modules(unsigned module_count, 
                        const struct stivale2_module* modules) {
    for(unsigned i = 0; i < module_count; i++) {
        const struct stivale2_module* module = &modules[i];
        if(!strcmp(module->string, "kernel.symbols")) {
            stacktrace_file((void*)module->begin);
        }
    }
}

 
static void init_memory(
    const struct stivale2_struct_tag_memmap* memmap_tag,
    const struct stivale2_struct_tag_framebuffer* fb
) {
    log_debug("init memory...");
    init_physical_allocator(memmap_tag);
    init_paging            (memmap_tag);

// map MMIOs
    map_pages(
        early_virtual_to_physical((void *)fb->framebuffer_addr), 
        MMIO_BEGIN,
        (fb->framebuffer_height * fb->framebuffer_pitch+0x0fff) / 0x1000,
        PRESENT_ENTRY
    );

// map lapic & hpet registers
    map_acpi_mmios();
}

static void empty_terminal_handler(const char* s, size_t l) {
(void) (s + l);
// empty handler by default,
// make sure not to execute the address 0 :)
}

void kbhandler(const struct kbevent* ev) {
    if(ev->type == KEYRELEASED && ev->scancode == PS2KB_ESCAPE) {
        shutdown();
        __builtin_unreachable();
    }
    if(ev->type == KEYPRESSED && ev->keycode != 0) {
        printf("%c", ev->keycode);
    }
};


static void print_fun(const char* s, size_t len) {
    driver_t* terminal = get_active_terminal();
    if(terminal != NULL)
        write_string(terminal, s, len);
}


// Registers %rbp, %rbx and %r12 through %r15 “belong” to the calling functio
// The following will be our kernel's entry point.
void _start(struct stivale2_struct *stivale2_struct) {


    // Let's get the terminal structure tag from the bootloader.
    const struct stivale2_struct_tag_terminal*    term_str_tag;
    const struct stivale2_struct_tag_memmap*      memmap_tag;
    const struct stivale2_struct_tag_framebuffer* framebuffer_tag;
    const struct stivale2_struct_tag_rsdp*        rsdp_tag_ptr;
    const struct stivale2_struct_tag_modules*     modules_tag;
    const struct stivale2_struct_tag_boot_volume* boot_volume_tag;

    term_str_tag    = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
    memmap_tag      = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);
    framebuffer_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
    rsdp_tag_ptr    = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_RSDP_ID);
    modules_tag     = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MODULES_ID);
    boot_volume_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_BOOT_VOLUME_ID);
    

    // term_str_tag == NULL is not a blocking
    // errror: stivale2 terminal is only used 
    // to print potential errors occuring when
    // initializing bincows' terminal
    if (term_str_tag != NULL) {
        void *term_write_ptr = (void *)term_str_tag->term_write;

        set_backend_print_fun(term_write_ptr);
    }
    if(modules_tag != NULL) {
        read_modules(modules_tag->module_count, modules_tag->modules);
    }
    else 
        log_warn("no stivale2 modules found");
        
 // print all logging messages
    set_logging_level(LOG_LEVEL_DEBUG);

    setup_isrs();

    read_acpi_tables((void*)rsdp_tag_ptr->rsdp);

    init_memory(memmap_tag, framebuffer_tag);

    set_backend_print_fun(empty_terminal_handler);
    append_paging_initialization();


    //set_terminal_handler(empty_terminal_handler);

// init kernel heap
    heap_init();

// drivers
    atshutdown(remove_all_drivers);
    atshutdown(free_all_devices);
    atshutdown(gpt_cleanup);


    video_init(framebuffer_tag);


    set_backend_print_fun(print_fun);

// so we need to load our gdt after our
// terminal is successfully installed 
    init_gdt_table();
    

    puts(&_binary_bootmessage_txt);

    printf("boot logs:\n");
    puts(log_get());
    log_flush();

    hpet_init();
    apic_setup_clock();

    pcie_init();


    pic_init();
    ps2kb_init();

    ps2kb_set_event_callback(kbhandler);

    disk_part_t* part;// = find_partition(*(GUID*)&boot_volume_tag->part_guid);
    if(part) {

    }
    else {
        log_warn(
            "cannot find main partition! (boot volume GUID: {%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x})",
            boot_volume_tag->part_guid.a,
            boot_volume_tag->part_guid.b,
            boot_volume_tag->part_guid.c,
            boot_volume_tag->part_guid.d[0],
            boot_volume_tag->part_guid.d[1],
            boot_volume_tag->part_guid.d[2],
            boot_volume_tag->part_guid.d[3],
            boot_volume_tag->part_guid.d[4],
            boot_volume_tag->part_guid.d[5],
            boot_volume_tag->part_guid.d[6],
            boot_volume_tag->part_guid.d[7]
        );

        part = search_partition("Bincows");
        
        if(part) {
            log_warn(
                "Bincows partition found on device %s (partition %u). "
                "This partition will be mounted as the main partition.", 
            
                part->interface->driver->device->name,
                part->id);
        }
        else {
            panic("no Bincows partition found.\n");
        }
    }




    //printf("issou");
    //log_info("%x allocated heap blocks", get_n_allocation());

    for(;;) {
        asm volatile("hlt");
        //log_info("clock()=%lu", clock());
    }

    __builtin_unreachable();
}