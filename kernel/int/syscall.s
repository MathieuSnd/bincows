
[extern syscall_main]
[extern sched_task_kernel_stack]


[global syscall_entry]

syscall_entry:

    push rcx
    push r11

    cld

    ; load the right kernel stack

    ;call sched_task_kernel_stack

    ;mov rsp, rax

    ; load segments:
    ; syscall did set cs and ss
    ; mov ax, 0x23

    call syscall_main

    ; restore user segments
    ; mov cx, 0x1b
    ; mov ds, cx
    ; mov es, cx

    pop r11
    pop rcx


    jmp $
    ; x86_64 version of sysret 
    ; without the p64 it would enter
    ; x86 32bit compatibility mode
    o64 sysret
