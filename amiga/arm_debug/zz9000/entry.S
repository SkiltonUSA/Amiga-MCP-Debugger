#include "layout.h"
.syntax unified
.arm
.section .text.entry,"ax"
.global _start
.hidden _start
_start:
    /* Refuse an unexpected MMU/cache mode before using the private stack. */
    mrc p15, 0, r1, c1, c0, 0
    movw r2, #0x1005
    tst r1, r2
    bxne lr
    /* Retain firmware callee-saved state and its unusually aligned stack. */
    push {r4-r11, lr}
    mov r4, sp
    ldr r5, [r0, #4] /* argv[0], verified Exec allocation ARM address */
    ldr r1, =ZZ_STACK_TOP
    add sp, r5, r1
    bic sp, sp, #7
    mov r0, r5
    bl zz_worker
    mov sp, r4
    /* Last write after C has finished and the private stack is retired.
     * Host still quiesces Core1 before freeing the allocation. */
    ldr r1, =ZZ_DIAG + 24
    movw r0, #0x4552
    movt r0, #0x3154
    str r0, [r5, r1] /* big endian RET1 bytes */
    dsb sy
    pop {r4-r11, pc}
