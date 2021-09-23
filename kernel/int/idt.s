
[section .text]
[global _lidt]

_lidt:
    lidt [rdi]
    ret