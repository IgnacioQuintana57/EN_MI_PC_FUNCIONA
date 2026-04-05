    .text
    .globl sumar_uno

sumar_uno:
    movq %rdi, %rax
    addq $1, %rax
    ret

.section .note.GNU-stack,"",@progbits
