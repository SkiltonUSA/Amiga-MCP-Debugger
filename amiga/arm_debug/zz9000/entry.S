#include "layout.h"
#if (defined(ZZ_VIDEO_ICACHE) || defined(ZZ_VIDEO_NEON)) && !defined(ZZ_VIDEO)
#error Video execution experiments are restricted to the video worker
#endif
.syntax unified
.arm
#ifdef ZZ_VIDEO_NEON
.fpu neon
.macro neon_put reg, offset
    rev r2, \reg
    str r2, [r11, #\offset]
.endm
#endif
.section .text.entry,"ax"
.global _start
.hidden _start
_start:
    /* Refuse an unexpected MMU/cache mode before using the private stack. */
    mrc p15, 0, r1, c1, c0, 0
    movw r2, #0x1005
    tst r1, r2
    bxne lr
#if defined(ZZ_VIDEO_ICACHE) || defined(ZZ_VIDEO_NEON)
    mrc p15, 0, r1, c0, c0, 5
    and r1, r1, #255
    cmp r1, #1
    bxne lr
#endif
    /* Retain firmware callee-saved state and its unusually aligned stack. */
    push {r4-r11, lr}
    mov r4, sp
    ldr r5, [r0, #4] /* argv[0], verified Exec allocation ARM address */
#ifdef ZZ_VIDEO
    mrc p15, 0, r6, c1, c0, 0
    ldr r2, =ZZ_DIAG + 28
    rev r1, r6
    str r1, [r5, r2] /* original SCTLR, big endian */
#endif
#ifdef ZZ_VIDEO_ICACHE
    /* Code is immutable after host relocation/cache sync and firmware GO.
     * Local ICIALLU/BPIALL only: no data or shared PL310 maintenance. */
    mov r0, #0
    dsb sy
    mcr p15, 0, r0, c7, c5, 0
    mcr p15, 0, r0, c7, c5, 6
    dsb sy
    isb
    orr r0, r6, #0x1000
    mcr p15, 0, r0, c1, c0, 0
    isb
#endif
    ldr r1, =ZZ_STACK_TOP
    add sp, r5, r1
    bic sp, sp, #7
#ifdef ZZ_VIDEO_NEON
    /* Refuse missing access rather than changing firmware coprocessor policy.
     * No compiled NEON C may execute before these checks and snapshots. */
    ldr r11, =ZZ_NEON_STATE
    add r11, r5, r11
    mrc p15, 0, r9, c1, c0, 2
    neon_put r9, 512
    tst r9, #0xc0000000 /* ASEDIS/D32DIS must be clear */
    bne .Lworker_done
    and r0, r9, #0x00f00000
    cmp r0, #0x00f00000
    bne .Lworker_done
    isb
    vmrs r8, fpexc
    neon_put r8, 516
    tst r8, #0x40000000
    beq .Lworker_done
    vmrs r0, mvfr0
    neon_put r0, 524
    and r0, r0, #15
    cmp r0, #2 /* 32 double registers */
    bne .Lworker_done
    vmrs r0, mvfr1
    neon_put r0, 528
    ubfx r0, r0, #8, #12
    movw r1, #0x111 /* SIMD loads/stores, integers and single precision */
    cmp r0, r1
    bne .Lworker_done
    vmrs r7, fpscr
    neon_put r7, 520
    mov r1, r11
    vstmia r1!, {d0-d15}
    vstmia r1!, {d16-d31}
    /* Runtime SIMD witness, outside every frame timing. */
    vmov.i32 q0, #21
    vadd.i32 q0, q0, q0
    vmov r0, s0
    neon_put r0, 532
#endif
    mov r0, r5
    bl zz_worker
#ifdef ZZ_VIDEO_NEON
    mov r1, r11
    vldmia r1!, {d0-d15}
    vldmia r1!, {d16-d31}
    vmsr fpscr, r7
    /* Store restored registers into the second snapshot for host comparison. */
    vstmia r1!, {d0-d15}
    vstmia r1!, {d16-d31}
    mrc p15, 0, r0, c1, c0, 2
    neon_put r0, 536
    vmrs r0, fpexc
    neon_put r0, 540
    vmrs r0, fpscr
    neon_put r0, 544
#endif
.Lworker_done:
#ifdef ZZ_VIDEO_ICACHE
    dsb sy
    mcr p15, 0, r6, c1, c0, 0
    isb
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0
    mcr p15, 0, r0, c7, c5, 6
    dsb sy
    isb
#endif
#ifdef ZZ_VIDEO
    mrc p15, 0, r0, c1, c0, 0
    rev r0, r0
    ldr r1, =ZZ_DIAG + 32
    str r0, [r5, r1] /* read-back restored SCTLR before RET1 */
#endif
    mov sp, r4
    /* Last write after C has finished and the private stack is retired.
     * Host still quiesces Core1 before freeing the allocation. */
    ldr r1, =ZZ_DIAG + 24
    movw r0, #0x4552
    movt r0, #0x3154
    str r0, [r5, r1] /* big endian RET1 bytes */
    dsb sy
    pop {r4-r11, pc}
