[bits 64]
section .text
    [global atomic_lock]
    [global atomic_unlock]

    atomic_lock:
        lock bts qword [rdi], 0
        jc spin
        ret

    spin:
        pause
        test qword [rdi], 1
        jnz spin
        jmp atomic_lock

    atomic_unlock:
        lock btr qword [rdi], 0
        ret