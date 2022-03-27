
[extern irq_common_handler]


; macro argument: irq number
%macro create_irq 1
[extern _irq_handler%1]
_irq_handler%1:
; already pushed stuf:
;    uint64_t RIP;
;    uint64_t CS;
;    uint64_t RFLAGS;
;    uint64_t RSP;
;    uint64_t SS;

; enter stack frame
    push rbp
    ; save stack pointer
    
    ; save frame pointer and enter stack frame
;    mov rbp, rsp
    
    ; save rax
    push rax
    mov al, byte %1

    jmp common_stub
    

%endmacro


create_irq 32
create_irq 33
create_irq 34
create_irq 35
create_irq 36
create_irq 37
create_irq 38
create_irq 39
create_irq 40
create_irq 41
create_irq 42
create_irq 43
create_irq 44
create_irq 45
create_irq 46
create_irq 47


common_stub:

; clear DF flag
;  DF need to be clear on function 
; entry and exit (System-V ABI)
    cld

    ; save context
    ; we already saved rsp, rbp, rax
    ; here, al = the irq handler number

    push rcx
    push rdx
    push rbx
    push rsi
    push rdi

    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; load kernel data segment
    mov di, 0x10
    mov ss, di
    mov ds, di
    mov es, di


    ; put the irq handler number in dil: the 
    ; argument for the irq_common_handler function
    mov dil, al

    ; beginning of the interrupt stack = beginning of
    ; the context structure 
    mov rsi, rsp



    call irq_common_handler

    ; restore the context
    ; this function only returns
    ; if no rescheduling is done



    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8

    pop rdi
    pop rsi
    pop rbx
    pop rdx
    pop rcx

    ; load target ss into rax
    mov rax, [rsp + 40]
    mov ds, ax
    mov es, ax

    
    pop rax
    pop rbp


    iretq