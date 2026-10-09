#!/bin/sh
# Run in a Linux Amiga GCC toolchain, from any working directory.
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:-"$src/../../.context/amiga/zztemperature"}
mkdir -p "$out"
cc=${AMIGA_CC:-m68k-amigaos-gcc}
"$cc" -noixemul -m68020 -O2 -Wall -Wextra -Werror \
    "$src/zztemperature.c" -o "$out/ZZTemperature" -lamiga
"$cc" -noixemul -m68020 -O2 -Wall -Wextra -Werror \
    "$src/make-icon.c" -o "$out/MakeZZTemperatureIcon" -lamiga
