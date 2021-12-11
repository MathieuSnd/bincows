[section .rodata]
[global _binary_charmap_bmp]
[global _binary_bootmessage_txt]
[global __image_pix]

_binary_charmap_bmp:
    incbin "../resources/bmp/charmap.bmp"
_binary_charmap_bmp_end:


_binary_bootmessage_txt:
    incbin "../resources/ascii/boot_message.txt"
    db 0
