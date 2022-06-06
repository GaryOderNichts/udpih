OUTPUT_ARCH(arm)

INCLUDE "imports.ld"

SECTIONS {
    . = (0x101312d0);

    .text : {
        crt0.o(.init)
        *(.text*);
    }
    .rodata : {
        *(.rodata*)
    }
    .data : {
        *(.data*)
    }
    /* .bss : {
        *(.bss*)
    } */

    /DISCARD/ : {
        *(*);
    }
}
