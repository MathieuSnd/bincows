bits 64

global _ss
global _cs
global _ds
global _es
global _fs
global _gs

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
    