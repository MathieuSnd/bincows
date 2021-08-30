[bits 64]


section .text
global _lgdt
global _cr3
extern gdt_descriptor
; argument in RDI
_lgdt:
    call _lgdt2
    ;pop rax
    
    push qword 1234
    pop rax
    ;cmp rax, 1234
    ;a: je a
    ret

_lgdt2:
    cli 
    ;sgdt [gdt_descriptor]

    ;sub dword [gdt_descriptor],   4*8
    ;add qword [gdt_descriptor+2], 4*8
    
    ;mov rax, qword [gdt_descriptor+2]
    
    ;mov qword [rax], 0

    lgdt [gdt_descriptor]
    ;ret

    mov rax, rsp
	push qword 0x30 
    ;0x30
	push qword rax
	pushf
	push qword 0x28
	push qword far_ret
	iretq

far_ret:
    ;mov  ax, ds
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

global test
test:
    mov rax, 0xff00ff
    ret
    dq 0
endtest: TIMES 64 dq 0

section .data
global test_size
test_size: dq endtest - test