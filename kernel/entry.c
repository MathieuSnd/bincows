#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/power.h"

#include "int/apic.h"
#include "int/idt.h"
#include "int/pic.h"
#include "int/syscall.h"

#include "drivers/terminal/terminal.h"
#include "drivers/video/video.h"
#include "drivers/hpet.h"
#include "drivers/ps2.h"
#include "drivers/pcie/pcie.h"
#include "drivers/pcie/scan.h"

#include "fs/gpt.h"
#include "fs/vfs.h"

#include "memory/gdt.h"
#include "memory/pmm.h"
#include "memory/paging.h"
#include "memory/vmap.h"

#include "lib/sprintf.h"
#include "lib/string.h"
#include "lib/logging.h"
#include "lib/simd.h"
#include "lib/registers.h"
#include "lib/dump.h"
#include "lib/stacktrace.h"
#include "lib/panic.h"
#include "boot/boot_interface.h"

#include "sched/sched.h"
#include "sync/spinlock.h"


// accessible by other compilation units
// like panic.c
const size_t stack_size = KERNEL_THREAD_STACK_SIZE;
uint8_t stack_base[KERNEL_THREAD_STACK_SIZE] 
        __attribute__((section(".stack"))) 
        __attribute__((aligned(16)));




static void init_memory(const struct boot_interface* bi) {
    log_debug("init memory...");
    init_pmm(bi);
    init_paging            (bi);

// map framebuffer
    map_pages(
        bi->framebuffer_paddr, 
        FB_VADDR,
        // round to one page
        (bi->framebuffer_size + 0x0fff) / 0x1000,
        PRESENT_ENTRY | PL_RW  | PL_XD 
      | PL_US // user can access the framebuffer
              // if it is mapped inside its
              // shared memory
    );
}

static void empty_terminal_handler(const char* s, size_t l) {
(void) (s + l);
// empty handler by default,
// make sure not to execute the address 0 :)
}


static void print_fun(const char* s, size_t len) {
    driver_t* terminal = get_active_terminal();
    if(terminal != NULL)
        write_string(terminal, s, len);
}


/**
 * @brief find main Bincows partition
 * in detected GPT partitions
 * 
 * choose the partition with the given GUID.
 * If not found, take any partition entitiled 
 * "Bincows"
 * 
 * @return disk_part_t* NULL if not found
 */
static 
disk_part_t* find_main_part(const struct boot_interface* bi) {

    GUID part_guid = bi->boot_volume_guid;


    disk_part_t* part = NULL;

    int valid_part_guid = (part_guid.low | part_guid.high) != 0llu;

    if(valid_part_guid) {
        log_info(
            "booted from partition with guid %lx-%lx",
            part_guid.high,
            part_guid.low
        );
        part = find_partition(part_guid);

        if(part)
            log_info("main partition found");
    }
    if(!part) {
        if(!valid_part_guid) {
            log_warn(
                "cannot find boot partition."
            );
        }
        else {
            log_warn(
                "cannot find boot partition. (boot volume GUID: "
                "{%8lx-%8lx})", part_guid.high, part_guid.low
            );
        }

        part = search_partition("Bincows");

        if(part) {
            log_warn(
                "Bincows partition found on device %s. "
                "This partition will be mounted as the main partition.", 
            
                part->sysname
            );
        }
        else {
            panic("no Bincows partition found.\n");
        }
    }

    return part;
}



void launch_init(void) {
    // argv and envp for the init program
    char argv[] = "/bin/init\0";
    char envp[] = "\0";


    void* elf_file;
    // open load and run elf file
    file_handle_t* f = vfs_open_file(argv, VFS_READ);

    assert(f);

    vfs_seek_file(f, 0, SEEK_END);
    size_t file_size = vfs_tell_file(f);

    assert(file_size != 0);

    elf_file = malloc(file_size); 

    vfs_seek_file(f, 0, SEEK_SET);

    size_t rd = vfs_read_file(elf_file, file_size, f);
    //sleep(10);

    assert(rd == file_size);

    vfs_close_file(f);
    

    _cli();

    pid_t pid= sched_create_process(KERNEL_PID, elf_file, file_size, 0);
    free(elf_file);

    assert(pid != -1);

    // proc lock is taked by sched_create_process
    process_t* proc = sched_get_process(pid);

    assert(proc);

    process_map(proc);

    set_process_entry_arguments(
        proc, 
        argv, sizeof(argv), envp, sizeof(envp)
    );



    // release process lock
    spinlock_release(&proc->lock);

    sched_unblock(pid, 1);

    process_unmap();
    _sti();

    assert(pid);


}



#include <cpuid.h>
#define EFLAGS_ID (1 << 21)

static int x86_support_cpuid(void) {
    uint64_t rf0 = get_rflags();
    set_rflags(rf0 ^ EFLAGS_ID);
    uint64_t rf1 = get_rflags();

    if((rf0 ^ rf1) & EFLAGS_ID)
        return 1; // supported
    return 0;   // unsupported
}


#include <x86intrin.h>

static void check_system_requirements(void) {
    if(! x86_support_cpuid())
        panic("system CPU does not support CPUID");

    if(! x86_support_xsave())
        panic("system CPU does not support XSAVE instruction"); 

    int avx = x86_support_avx();
    int avx512 = x86_support_avx512();
    
    log_info("avx support: %s", avx ? "true" : "false");
    log_info("avx512 support: %s", avx512 ? "true" : "false");
}


extern void run_tests(void);

void kernel_main(struct boot_interface* bi) {


 // print all logging messages
    set_logging_level(LOG_LEVEL_DEBUG);
    
    if(bi->kernel_symbols)
        stacktrace_file(bi->kernel_symbols);
    else
        log_warn("no stacktrace found");
    if(bi->console_write)
        set_backend_print_fun(bi->console_write);
    else
        log_warn("no early console found");

    assert(bi->mmap_get_next);
    assert(bi->rsdp_paddr);


    // start system initialization

    setup_isrs();

    check_system_requirements();

    init_memory(bi);

    acpi_early_scan(translate_address((void*)(bi->rsdp_paddr)));
    acpi_map_mmio();

    set_backend_print_fun(empty_terminal_handler);
    append_paging_initialization();


// init kernel heap
    heap_init();



// shutdown procedure
    atshutdown(sched_cleanup);
    atshutdown(log_cleanup);
    atshutdown(vfs_cleanup);
    atshutdown(gpt_cleanup);
    atshutdown(remove_all_drivers);
    atshutdown(free_all_devices);

    


    driver_t* terminal = video_init(bi);


    set_backend_print_fun(print_fun);

// so we need to load our gdt after the
// terminal is successfully installed 
    init_gdt_table();



    // clear screen to append
    // the background color
    puts("\x1b[0m\x0c");

    puts(log_get());

    hpet_init();
    apic_setup_clock();

    
    // scan PCIe bus
    pcie_init();
    
//  now that errors/panic can be printed, 
//  finalize acpi initialization 
    acpi_init();


    // @todo reclaim bootloader / ACPI memory


    vfs_init();



    pic_init();
    ps2_init();


    disk_part_t* part = find_main_part(bi);
    
    assert(part);
    int r = vfs_mount(part, "/");
    assert(r != 0);

    // init log file
    log_init_file("/var/log/sys.log");


    r = vfs_mount_devfs();      assert(r);
    r = vfs_mount_pipefs();     assert(r);
    r = vfs_mount_memfs();      assert(r);

    // /dev/term
    terminal_register_dev_file("term", terminal);


    // /dev/ps2
    ps2_register_dev_file("ps2");

    // /mem/video
    video_create_file("video");

    
    xstate_enable();
    sse_enable();
    avx_enable();
    avx512_enable();

    log_debug("init sched");
    sched_init();


    log_debug("init syscalls");
    syscall_init();



#ifdef KERNEL_RUN_TESTS
    run_tests();
#endif

    log_debug("launch shell");
    // everything should be correctly initialized
    // now we can start the shell
    launch_init();

    log_debug("start scheduling");

    
    // start the scheduler
    sched_start();
    
    panic("unreachable");
}
