#ifndef AD_ZZ_LAYOUT_H
#define AD_ZZ_LAYOUT_H
/* Offsets inside an Exec-owned 128 KiB allocation, NOT global DDR addresses. */
#define ZZ_BLOCK_SIZE 0x20000
#define ZZ_CONTROL 0x8000
#define ZZ_DIAG (ZZ_CONTROL + 64)
#define ZZ_STOP (ZZ_CONTROL + 128)
#define ZZ_CHALLENGE (ZZ_CONTROL + 132)
#define ZZ_PAGE (ZZ_CONTROL + 256)
#define ZZ_STACK_TOP (ZZ_BLOCK_SIZE - 16)
#define ZZ_READY 0x41524d31
#define ZZ_RETURNED 0x52455431
#define ZZ_XOR 0x6d637031
#endif
