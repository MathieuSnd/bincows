
[section .text]
[global _lidt]

lidt:
    lidt [rdi]
    ret