
#include "stivale2.h"
#include "boot_interface.h"

// for KERNEL_STACK_SIZE
#include "../sched/thread.h"


extern uint8_t stack_base[];

void* const bs_stack_end = stack_base + KERNEL_STACK_SIZE;


#define INITIAL_STACK_PTR ((uintptr_t)(stack_base + KERNEL_STACK_SIZE))



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
        if(current_tag == NULL) {
            // construct panic string: "stivale2: tag not found %s", id
            log_warn("stivale2 bootloader: tag not found %lx", id);
            return NULL;
        }

        if (current_tag->identifier == id) {
            return current_tag;
        }
 
         current_tag = (void *)current_tag->next;
    }    
}


// read modules to fetch kernel symbols file
static void read_modules(
        const struct stivale2_struct_tag_modules* mtag,
              struct boot_interface* bi) {

    unsigned module_count = mtag->module_count;
    const struct stivale2_module* modules = mtag->modules;

    for(unsigned i = 0; i < module_count; i++) {
        const struct stivale2_module* module = &modules[i];
        if(!strcmp(module->string, "kernel.symbols")) {
        }
    }
}


// early memory lifetime
static const struct stivale2_struct_tag_memmap* memmap_tag;


static struct mmape* stivale2_mmap_get_next(void) {
    static int i = 0;
    static struct mmape mmape;

    for(;;) {
        if(i >= memmap_tag->entries)
            return NULL;


        struct stivale2_mmap_entry* st2e = &memmap_tag->memmap[i++];

        mmape.pbase  = st2e->base;
        mmape.length = st2e->length;
        
        switch (st2e->type)
        {
        case STIVALE2_MMAP_RESERVED:
        case STIVALE2_MMAP_BAD_MEMORY:
            continue;


        case STIVALE2_MMAP_USABLE:
            mmape.type = USABLE;
            break;

        case STIVALE2_MMAP_ACPI_RECLAIMABLE:
            mmape.type = USABLE;
            break;
        case STIVALE2_MMAP_ACPI_NVS:
            mmape.type = ACPI_NVS;
            break;
        case STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE:
            mmape.type = BOOTLOADER_RECLAIMABLE;
            break;
        case STIVALE2_MMAP_KERNEL_AND_MODULES:
            mmape.type = KERNEL;
            break;
        case STIVALE2_MMAP_FRAMEBUFFER:
            mmape.type = MMIO;
            break;
        default:
            panic("stivale2 mmap: unexpected value");
        }
        return &mmape;
    }
}


// entry point
static void _start(struct stivale2_struct *stivale2_struct) {

    log_info("starting boot sequence");

    // bootleader stivale2 structures
    const struct stivale2_struct_tag_terminal*    term_str_tag;
    const struct stivale2_struct_tag_framebuffer* framebuffer_tag;
    const struct stivale2_struct_tag_rsdp*        rsdp_tag_ptr;
 // const struct stivale2_struct_tag_memmap*      memmap_tag; global struct
    const struct stivale2_struct_tag_modules*     modules_tag;
    const struct stivale2_struct_tag_boot_volume* boot_volume_tag;

    term_str_tag    = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
    memmap_tag      = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);
    framebuffer_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);
    rsdp_tag_ptr    = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_RSDP_ID);
    modules_tag     = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MODULES_ID);
    boot_volume_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_BOOT_VOLUME_ID);

    // system information structure to fill using stivale2 structures
    struct boot_interface bi;

    

    // term_str_tag == NULL is not a blocking
    // errror: stivale2 terminal is only used 
    // to print potential errors occuring when
    // initializing bincows' terminal
    if (term_str_tag != NULL) {
        bi.console_write = (void *)term_str_tag->term_write;
    }
    if(modules_tag != NULL) {
        read_modules(modules_tag, &bi);
    }
    else
        log_warn("no stivale2 modules found");

    
    bi.mmap_get_next = stivale2_mmap_get_next;    
        
}
