[global _ss]
[global _cs]
[global _ds]
[global _es]
[global _fs]
[global _gs]
;[global inb]
;[global outb]
[global get_rflags]
[global set_rflags]
[global set_cr0]
[global get_cr0]
[global _cr2]
[global _cr3]
[global get_cr3]
[global set_cr4]
[global get_cr4]
[global _ltr]
[global read_msr]
[global write_msr]
[global _rbp]
[global _change_stack]

[section .text]

_ds:
    xor rax,rax
    mov ax, ds
    ret

_ss:
    xor rax,rax
    mov ax, ss
    ret

_cs:
    xor rax,rax
    mov ax, cs
    ret

_es:
    xor rax,rax
    mov ax, es
    ret
  
_gs:
    xor rax,rax
    mov ax, gs
    ret
      
_fs:
    xor rax,rax
    mov ax, fs
    ret

_rbp:
    mov rax, rbp
    ret


; void get_cr0(uint64_t cr3)
get_cr0:
    mov rax, cr0
    ret
; uint64_t set_cr0(void)
set_cr0:
    mov cr0, rdi
    ret

; uint64_t get_cr2(void)
_cr2:
    mov rax, cr2
    ret

; void cr3(uint64_t cr3)
_cr3:
    mov cr3, rdi
    ret

; set_cr3(uint64_t cr3)
get_cr3:
    mov rax, cr3
    ret
    
; void get_cr4(uint64_t cr3)
get_cr4:
    mov rax, cr4
    ret
; uint64_t set_cr4(void)
set_cr4:
    mov cr4, rdi
    ret


get_rflags:
    pushfq
    pop rax
    ret

; set_rflags(uint64_t rflags)  
set_rflags:
    push rdi
    popfq
    ret

; void _ltr(uint16_t tss_selector)
_ltr:
    ltr di
    ret

; uint8_t inb(port)
inb:
    push rdx
    mov  rdx, rdi
    xor  rax, rax

    in   al, dx
    
    pop  rdx
    ret
    
; void outb(uint16_t port, uint8_t val)
outb:
    push rdx
    mov  rdx, rdi
    mov  rax, rsi

    out  dx,  al
    
    pop  rdx
    ret


; read_msr(uint32_t addr)
read_msr:
    push rbp
    mov rbp, rsp
    
    push rcx
    push rdx

    mov ecx, edi
    rdmsr

    shl rdx, 32
    or rax, rdx

    pop rcx
    pop rdx
    
    mov rsp, rbp
    pop rbp
    ret

    
; write_msr(uint32_t addr, uint64_t value)
write_msr:
    push rbp
    mov rbp, rsp
    
    push rcx
    push rdx

    mov   ecx, edi
    mov   eax, esi
    mov   rdx, rsi
    shr   rdx, 32

    wrmsr

    pop rcx
    pop rdx
    
    mov rsp, rbp
    pop rbp
    ret

; void [[noreturn]] change_stack(uint64_t stack_addr, void (*no_return)(void))
_change_stack:
    mov rsp, rdi
    push rbp
    mov rbp, rsp

    mov rdi, rsi
    jmp rdi
