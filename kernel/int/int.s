
[extern int_common_handler]


; macro argument: int number
%macro create_int 1
[extern _int_handler%1]
_int_handler%1:
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
    mov rbp, rsp
    
    ; save rax and rcx
    push rax
    push rcx

    mov cl, byte %1

    jmp common_stub
%endmacro


; same as above but for exceptions with an 
; error code: #PF, #GP, ...
%macro create_int_code 1
[extern _int_handler%1]
_int_handler%1:
; enter stack frame, and get the error code
    xchg rbp, [rsp]

    ; rbp contains the error code.
    ; save rax then use it to store the error code
    push rax
    push rcx
    mov eax, ebp
    
    ; save frame pointer and enter stack frame
    lea rbp, [rsp + 16]

    ; put the error code in upper 
    mov cl, byte %1


    jmp common_stub
%endmacro


;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; machine exceptions ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;
create_int 0
create_int 1
create_int 2
create_int 3
create_int 4
create_int 5
create_int 6
create_int 7
create_int_code 8
create_int 9
create_int_code 10
create_int_code 11
create_int_code 12
create_int_code 13
create_int_code 14
create_int 15 ; reserved
create_int 16
create_int_code 17
create_int 18
create_int 19
create_int 20
create_int_code 21
create_int 22 ; reserved
create_int 23 ; reserved
create_int 24 ; reserved
create_int 25 ; reserved
create_int 26 ; reserved
create_int 27 ; reserved
create_int 28
create_int_code 29
create_int_code 30
create_int 31 ; reserved


;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;; irq handlers ;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;
create_int 32
create_int 33
create_int 34
create_int 35
create_int 36
create_int 37
create_int 38
create_int 39
create_int 40
create_int 41
create_int 42
create_int 43
create_int 44
create_int 45
create_int 46
create_int 47
create_int 48


common_stub:

; clear DF flag
;  DF need to be clear on function 
; entry and exit (System-V ABI)
    cld

    ; save context
    ; we already saved rsp, rbp, rax, rcx
    ; here, al = the int handler number

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

    ; 1st argument
    ; put the int handler number in dil: the 
    ; argument for the int_common_handler function
    mov dil, cl

    ; 2nd argument
    ; beginning of the interrupt stack = beginning of
    ; the context structure
    mov rsi, rsp

    ; 3rd argument
    ; put the error code in rdx: the argument for the int_common_handler function
    mov rdx, rax


    ; void int_common_handler(uint8_t irq_n, gp_regs_t* context, uint32_t error_code)
    call int_common_handler

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

    pop rax
    pop rbp


    iretq