#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "acpi/power.h"

#include "int/apic.h"
#include "int/idt.h"
#include "int/pic.h"
#include "int/syscall.h"

#include "drivers/terminal/video.h"
#include "drivers/terminal/terminal.h"
#include "drivers/hpet.h"
#include "drivers/ps2kb.h"
#include "drivers/pcie/pcie.h"
#include "drivers/pcie/scan.h"

#include "fs/gpt.h"
#include "fs/vfs.h"

#include "memory/gdt.h"
#include "memory/physical_allocator.h"
#include "memory/paging.h"
#include "memory/vmap.h"
#include "memory/heap.h"

#include "lib/sprintf.h"
#include "lib/string.h"
#include "lib/logging.h"
#include "lib/registers.h"
#include "lib/dump.h"
#include "lib/stacktrace.h"
#include "lib/panic.h"

#include "sched/sched.h"

#include "sync/spinlock.h"

#include "early_video.h"

#include "boot/boot_interface.h"
 

// accessible by other compilation units
// like panic.c
const size_t stack_size = KERNEL_STACK_SIZE;
uint8_t stack_base[KERNEL_STACK_SIZE] 
        __attribute__((section(".stack"))) 
        __attribute__((aligned(16)));




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
        PRESENT_ENTRY | PL_RW  | PL_XD
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
disk_part_t* find_main_part(const struct stivale2_struct_tag_boot_volume* boot_volume_tag) {

    const struct stivale2_guid* part_guid = 
        (boot_volume_tag != NULL) ? &boot_volume_tag->part_guid: NULL;


    disk_part_t* part = NULL;
    if(part_guid)
        part = find_partition(*(GUID*)part_guid);
        
    if(part)
        log_info("main partition found");
    if(!part) {
        if(!part_guid) {
            log_warn(
                "cannot find boot partition."
            );
        }
        else {
            log_warn(
                "cannot find boot partition. (boot volume GUID: "
                "{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x})",
                part_guid->a,    part_guid->b,    part_guid->c,
                part_guid->d[0], part_guid->d[1],
                part_guid->d[2], part_guid->d[3], part_guid->d[4], 
                part_guid->d[5], part_guid->d[6], part_guid->d[7]
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


static inline
void test_disk_write(void) {
    file_handle_t* f = vfs_open_file("//////home//elyo//", VFS_WRITE);

    const char* s = "Hello, world!";

    vfs_write_file(s, 1, strlen(s), f);
// check
    //read
    vfs_close_file(f);
    
    f = vfs_open_file("//////home//elyo//", VFS_READ);

    // read and print file
    char buf[2048];
    size_t read = vfs_read_file(buf, 2048, f);
    log_info("read %d bytes", read);
    printf("%s", buf);

    vfs_close_file(f);
}



static inline
void test_disk_overflow(void) {
    file_handle_t* f = vfs_open_file("//////home//elyo//", VFS_READ);

    const int bsize = 1024 * 1024 * 8;

    const size_t size = 1024*1024*64;

    uint8_t* buf = malloc(bsize);
    for(int i = 0; i < bsize; i++)
        buf[i] = i;

    uint64_t time = clock_ns();

    for(unsigned i = 0; i < size / bsize; i++) {
        log_info("write %u (%u)", i * bsize, (clock_ns() - time) / 1000000);
        time = clock_ns();
        
        size_t r = vfs_write_file(buf, bsize, 1, f);
        assert(r == 1); 
    }

// check
    //read
    vfs_close_file(f);
    
    f = vfs_open_file("//////home//elyo//", VFS_READ);

    time = clock_ns();
    int rsize = (int) bsize;
    int i = 0;
    while(vfs_read_file(buf, rsize, f) == (size_t)rsize) {
        int begin = i++ * rsize;
        log_info("read %u (%u)", begin, (clock_ns() - time) / 1000000);
        time = clock_ns();

        for(int j = begin; j < begin + rsize; j++)
            assert(buf[j - begin] == (j & 0xff));
            
    }
    vfs_close_file(f);
    free(buf);
}





static inline 
void test_disk_read(void) {
    file_handle_t* fd = vfs_open_file("//////bin/sh//", VFS_READ);
    
    const int size = vfs_seek_file(fd, 0, SEEK_END);

    assert(size > 0);
    vfs_seek_file(fd, 0, SEEK_SET);


    vfs_close_file(fd);

    _cli();

    for(int i = 0; i < 10000; i++) {

        // alloc user memory
        uint64_t pm = alloc_user_page_map();

        // map user memory
        set_user_page_map(pm);

        uint8_t* buf = malloc(size);
        memset(buf, 0, size);        

        _sti();
        fd = vfs_open_file("/////bin/sh//", VFS_READ);
        int r = vfs_read_file(buf, size, fd);

        for(int i = 0; i < 10; i++)
            asm("hlt");


        vfs_close_file(fd);


        assert(r == size);

        // load elf
        //elf_program_t* prog = elf_load(buf, size);


        log_info("checksum: %u", memsum(buf, size));

        //assert(prog != NULL);

        //elf_free(prog);

        free_user_page_map(pm);
        free(buf);

    }



}



void launch_shell(void) {
    // argv and envp for the initial shell
    char argv[] = "/bin/sh\0";
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
    

    //terminal_clear(get_active_terminal());

    _cli();

    pid_t pid= sched_create_process(KERNEL_PID, elf_file, file_size, 0);
    free(elf_file);

    assert(pid != -1);

    // proc lock is taked by sched_create_process
    process_t* proc = sched_get_process(pid);

    assert(proc);

    set_process_entry_arguments(
        proc, 
        argv, sizeof(argv), envp, sizeof(envp)
    );



    // release process lock
    spinlock_release(&proc->lock);

    sched_unblock(pid, 1);
    _sti();

    assert(pid);


}


void kernel_main(struct boot_interface* bi) {
    
    if(bi->kernel_symbols)
        stacktrace_file(bi->kernel_symbols);
    if(bi->console_write)
        set_backend_print_fun(bi->console_write);


    // start system initialization

 // print all logging messages
    set_logging_level(LOG_LEVEL_DEBUG);

    setup_isrs();

    init_memory(memmap_tag, framebuffer_tag);

    read_acpi_tables(translate_address((void*)(bi->rsdp_paddr)));
    
// map lapic & hpet registers
    map_acpi_mmios();


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


    driver_t* terminal = video_init(framebuffer_tag);


    set_backend_print_fun(print_fun);

// so we need to load our gdt after our
// terminal is successfully installed 
    init_gdt_table();

    puts("\x1b[0m\x0c");
    
    puts(log_get());

    log_info("screen resolution: %ux%u", 
            framebuffer_tag->framebuffer_width,
            framebuffer_tag->framebuffer_height
    );

    hpet_init();
    apic_setup_clock();



    vfs_init();
    pcie_init();


    pic_init();
    ps2kb_init();

    //ps2kb_set_event_callback(early_kbhandler);
    

    disk_part_t* part = find_main_part(boot_volume_tag);
    
    assert(part);
    int r = vfs_mount(part, "/");
    assert(r != 0);

    
    // init log file
    log_init_file("/var/log/sys.log");


    r = vfs_mount_devfs();      assert(r);
    r = vfs_mount_pipefs();      assert(r);

    // /dev/term
    terminal_register_dev_file("term", terminal);

    // /dev/ps2kb
    ps2kb_register_dev_file("ps2kb");


    log_debug("init sched");
    sched_init();


    log_debug("init syscalls");
    syscall_init();

    //log_warn("test disk read");
    //test_disk_read();

    //log_warn("test disk write");
    //test_disk_overflow();
    

    log_debug("launch shell");
    // everything should be correctly initialized
    // now we can start the shell
    launch_shell();

    log_debug("start scheduling");

    // clear the terminal
    //printf("\x0c");

    
    // start the scheduler
    sched_start();
    
    panic("unreachable");
}
