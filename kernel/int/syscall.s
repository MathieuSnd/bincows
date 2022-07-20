
[extern syscall_main]
[extern sched_task_kernel_stack]
[global syscall_entry]

[extern apic_config]

[extern syscall_stacks]

[section .text]
syscall_entry:

    ; here, the stack pointer is the user one.
    ; irqs are disabled, so it's not a problem.
    ; we shouldn't forget to enable them when
    ; we successfully loaded the kernel stack.

    ; switch stacks
    ; to do so, we need to load the cpu private 
    ; syscall stack pointer

    ; 1. get the lapic id
    ; 2. switch to the cpu private stack pointer

    ; we suppose that when the thread was switched to,
    ; the kernel set syscall_stacks[lapic_id]
    ; to the right thread kernel stack pointer.
    
    ; 1.
    mov rax, [rel apic_config]

    mov eax, dword [rax + 0x20]; LAPIC ID Register

    ; 2.
    lea r9, [rel syscall_stacks]    
    mov rax, [r9 + rax * 8]

    ; save the current stack pointer
    ; in r9
    mov r9, rsp

    ; load the new one
    mov rsp, [rax]

    ; should be the right stack!

    ; set up frame pointer
    push rcx
    push rbp
    mov rbp, rsp



    ; save the userstack pointer
    push r9



    push rcx
    push r11


    ; give the user stack pointer as the 4rth argument
    ; to the syscall_main function
    mov rcx, r9


    ; clear direction flag    
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

    pop r9

    leave


    ; disable interrupts again
    ; when switching to the user 
    ; stack pointer
    cli

    ; switch to user stack pointer
    mov rsp, r9

    ; x86_64 version of sysret 
    ; without the o64 prefix, it would enter
    ; x86 32bit compatibility mode
    o64 sysret
