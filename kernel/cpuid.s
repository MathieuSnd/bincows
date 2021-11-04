[section .text]
[global cpuid]

;struct cpuid_regs {
;    uint32_t eax, ebx, ecx, edx;
;}


; void cpuid(uint32_t eax, struct cpuid_regs* out_regs);
cpuid:
    push rbp; no stack alloc
    push rbx

    mov eax, edi
    cpuid

    mov [rdi + 0],  eax
    mov [rdi + 4],  ebx
    mov [rdi + 8],  ecx
    mov [rdi + 12], edx


    pop rbx
    pop rbp
    ret