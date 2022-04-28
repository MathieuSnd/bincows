; ps2 -> azerty
; translation table

[section .rodata]
[global ps2_azerty_table_lowercase]
[global ps2_azerty_table_uppercase]
[global ps2_azerty_table_altgr]
ps2_azerty_table_lowercase:
    db     0,0, "&e", '"', "'(-e_ca)=", 8  
    db     9, "azertyuiop^$", 10
    db     0, "qsdfghjklmu", 0, 0, "*"
    db     "wxcvbn,;:!", 0, 0
    db     0, ' ',
    TIMES 80-52 db 0
    db "<"

ps2_azerty_table_uppercase:
    db     0,0, "1234567890)+", 8  
    db     9, "AZERTYUIOP^$", 10
    db     0, "QSDFGHJKLM%", 0, 0, "*"
    db     "WXCVBN?./!", 0,0
    db     0, ' '
    TIMES 80-53 db 0
    db ">"


ps2_azerty_table_altgr:
    db     0,0,0, "~#{[|`\^@]}", 8  
    db     
    TIMES 90-15 db 0

