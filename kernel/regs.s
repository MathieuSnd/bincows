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

section .text

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

outb:
    push rdx
    mov  al, dil
    
    out  dx, al
    
    pop  rdx
    ret
