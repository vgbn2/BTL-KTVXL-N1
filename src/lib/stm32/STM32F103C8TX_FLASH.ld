ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } >FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    *(.ARM.exidx*)
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  _sidata = LOADADDR(.data);

  .data : AT(_etext)
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } >RAM

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } >RAM

  . = ALIGN(4);
  _end = .;
  PROVIDE(end = .);
}
