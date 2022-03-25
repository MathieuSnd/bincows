

[global _restore_context]

; [[noreturn]] restore_context(struct gp_regs*)
;typedef struct gp_regs {
;    uint64_t rip;
;    uint64_t rbp;
;    uint64_t rsp;
;    uint64_t rax;
;    uint64_t rcx;
;    uint64_t rdx;
;    uint64_t rbx;
;    uint64_t rsi;
;    uint64_t rdi;
;    uint64_t rflags;
;
;// r8 -> r15
;    uint64_t ext[8];
;
;} __attribute__((packed)) gp_regs_t;
_restore_context:
    mov rsp, rdi


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

