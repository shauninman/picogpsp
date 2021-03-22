
#ifndef _WIN32
#define _open_gba_bios_rom open_gba_bios_rom
#endif

.globl _open_gba_bios_rom

.data
_open_gba_bios_rom:
  .incbin "bios/open_gba_bios.bin"

