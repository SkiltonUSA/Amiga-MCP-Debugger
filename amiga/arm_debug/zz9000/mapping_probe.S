/* Short, position-independent Core1 mapping probe. Only writes its locked
 * P96 bitmap. Candidate Exec RAM is READ ONLY until translation is proven.
 * ZZ9K_ENV argv[0]=bitmap ARM base, argv[1]=candidate ARM address.
 * Expected reset entry: MMU, I-cache and D-cache disabled. No CP15 writes. */
.syntax unified
.arm
.section .text.entry,"ax"
.global _start
_start:
    mrc p15, 0, r1, c1, c0, 0
    movw r2, #0x1005
    tst r1, r2
    bxne lr
    push {r4, lr}
    ldr r2, [r0, #4]
    add r2, r2, #1024
    rev r1, r1
    str r1, [r2, #4]
    mrc p15, 0, r1, c0, c0, 0
    rev r1, r1
    str r1, [r2, #8]
    mrc p15, 0, r1, c0, c0, 5
    rev r1, r1
    str r1, [r2, #12]
    ldr r3, [r0, #8]
    add r2, r2, #16
    mov r4, #8
1:  ldr r1, [r3], #4
    str r1, [r2], #4
    subs r4, r4, #1
    bne 1b
    dsb sy
    sub r2, r2, #48
    movw r1, #0x414d
    movt r1, #0x3150
    str r1, [r2] /* big endian MAP1 bytes */
    dsb sy
    pop {r4, pc}
