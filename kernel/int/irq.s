
[extern irq_common_handler]

; macro argument: irq number
%macro create_irq 1
[extern _irq_handler%1]
_irq_handler%1:
    ; enter stack frame
    ; push rbp
    ; mov rbp, rsp

; enter stack frame
    push rbp
    mov rbp, rsp
    push rdi
    mov dil, byte %1
    
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

    push rax
    push rcx
    push rdx
    push rsi
    push r8
    push r9
    push r10
    push r11

; clear DF flag
;  DF need to be clear on function 
; entry and exit (System-V ABI)
    cld

    call irq_common_handler


    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdx
    pop rcx
    pop rax
    pop rdi
    ; mov rsp, rbp
    ; pop rbp
    leave
    iretq