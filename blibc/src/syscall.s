; implement function syscall:
; long int syscall (long int __sysno, void* args, size_t args_sz)

[global syscall]

syscall:
    push rbp
    mov rbp, rsp

    syscall

    ; if rdx != 0,
    ; the system wants us to
    ; trigger the YIELD(47) software interrupt
    test rdx, rdx
    jne _ret
    int 47

_ret:
    leave
    ret