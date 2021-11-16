[section .text]
[global _lgdt]
[global _cr3]

%define KERNEL_CODE_SEGMENT 0x08
%define KERNEL_DATA_SEGMENT 0x10

; argument in RDI
_lgdt:
    lgdt [rdi]

    mov rax, rsp
	push qword KERNEL_DATA_SEGMENT
    ;0x30
	push qword rax
	pushfq ; push rflags
	push qword KERNEL_CODE_SEGMENT
	push qword far_ret
	iretq

far_ret:
    mov  ax, 0x10
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    ret