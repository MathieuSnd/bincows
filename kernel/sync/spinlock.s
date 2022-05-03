[global _spinlock_init]
[global _spinlock_acquire]
[global _spinlock_release]

; stolen from   https://wiki.osdev.org/Spinlock#Improved_Lock
;
spin_wait:
    pause
    test dword [rdi],1      ;Is the lock free?
    jnz spin_wait           ;no, wait
 
_spinlock_acquire:
    push rbp
    mov rbp, rsp
    lock bts dword [rdi],0  ;Attempt to acquire the lock (in case lock is uncontended)
    jc spin_wait            ;Spin if locked ( organize code such that conditional jumps are typically not taken ) 
    leave
    ret                     ;Lock obtained


_spinlock_release:
    push rbp
    mov rbp, rsp
    mov dword [rdi], 0
    leave
    ret