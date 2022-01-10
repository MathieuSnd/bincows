
[extern irq_common_handler]

; macro argument: irq number
%macro create_irq 1
[extern _irq_handler%1]
_irq_handler%1:
    iret
    ; enter stack frame
    ;push %1
    ; push rbp
    ; mov rbp, rsp

    ;mov dil, byte %1
    
    ; call common_stub
    
    ; mov rsp, rbp
    ; pop rbp
    ;add rsp, 8

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
