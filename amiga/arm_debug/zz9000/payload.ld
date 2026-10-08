ENTRY(_start)
PHDRS { image PT_LOAD FLAGS(7); dynamic PT_DYNAMIC FLAGS(6); }
SECTIONS {
  . = 0;
  .text : { KEEP(*(.text.entry)) *(.text*) } :image
  .rodata : { *(.rodata*) } :image
  .data : { *(.data*) *(.got*) } :image
  .dynamic : { *(.dynamic) } :image :dynamic
  .dynsym : { *(.dynsym) } :image
  .dynstr : { *(.dynstr) } :image
  .hash : { *(.hash) } :image
  .gnu.hash : { *(.gnu.hash) } :image
  .rel.dyn : { *(.rel.dyn) } :image
  .bss : { *(.bss*) *(COMMON) } :image
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
  ASSERT(. < 0x8000, "ARM image overlaps private control block")
}
