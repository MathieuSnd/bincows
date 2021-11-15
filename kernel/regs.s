[global _ss]
[global _cs]
[global _ds]
[global _es]
[global _fs]
[global _gs]
[global inb]
[global outb]
[global get_rflags]
[global set_rflags]
[global _ltr]
[global read_msr]
[global write_msr]

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

    
; read_msr(uint32_t addr, uint64_t value)
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