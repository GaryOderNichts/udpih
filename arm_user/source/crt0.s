.section ".init"
.arm
.align 4

.extern _main
.type _main, %function

_start:
    bl _main

    // load the threadQuitRoutine into lr
    ldr lr, =0x08134000

    // load the original stack pointer
    ldr sp, =0x1016ae30
    
    // jump back into the __uhsBackgroundThread
    ldr pc, =0x10111164
