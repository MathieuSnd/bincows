; implement function syscall:
; long int syscall (long int __sysno, void* args, size_t args_sz)

[global syscall]

syscall:
    push rbp
    mov rbp, rsp

    syscall

    leave
    ret