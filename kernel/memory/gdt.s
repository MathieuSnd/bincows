[section .text]
[global _lgdt]
[global _cr3]

; argument in RDI
_lgdt:
    lgdt [rdi]

    mov rax, rsp
	push qword 0x30 
    ;0x30
	push qword rax
	pushf
	push qword 0x28
	push qword far_ret
	iretq

far_ret:
    mov  ax, 0x30
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    ret


_cr3:
    mov rax, rdi
    mov cr3, rax
    ret