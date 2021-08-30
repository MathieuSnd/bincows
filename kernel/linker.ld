/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)
 
SECTIONS
{
    . = 0x00100000;
 
    /* We place the .stivalehdr section containing the header in its own section, */
    /* and we use the KEEP directive on it to make sure it doesn't get discarded. */
    .stivalehdr : {
        KEEP(*(.stivalehdr))
    }
 
    /* Then let's place all the other traditional executable sections afterwards. */
    .text : ALIGN(0x1000) {
        *(.text*)
    }
 
    .rodata : ALIGN(0x1000) {
        *(.rodata*)
    }
 
    .data : ALIGN(0x1000) {
        *(.data*)
    }
 
    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
    }
}