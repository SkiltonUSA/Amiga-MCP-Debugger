#!/bin/sh
set -eu
cd /work
out=.context/amiga/ffmpeg-bench
src=$out/ffmpeg-9.0.2
flags="-mcpu=cortex-a9 -marm -mfpu=neon -mfloat-abi=softfp -include /work/amiga/ffmpeg_bench/libc_config.h -D__GLOBAL_ERRNO -O3 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -ffreestanding -fno-stack-protector -fno-tree-vectorize -DZZ_VIDEO -DZZ_VIDEO_ICACHE -DZZ_VIDEO_NEON -DZZ_CONTROL=0x180000 -DZZ_BLOCK_SIZE=0x1000000 -Iamiga/ffmpeg_bench -Iamiga/arm_debug -Iamiga/arm_debug/zz9000 -I$out/arm-build -I$src"
for file in worker heap runtime; do
 arm-none-eabi-gcc $flags -c amiga/ffmpeg_bench/$file.c -o $out/$file.o
done
arm-none-eabi-gcc $flags -c amiga/arm_debug/zz9000/entry.S -o $out/entry.o
arm-none-eabi-ld -shared -Bsymbolic --exclude-libs=ALL --gc-sections --no-undefined --defsym=ZZ_IMAGE_LIMIT=0x180000 -T amiga/arm_debug/zz9000/payload.ld -Map=$out/link.map $out/entry.o $out/worker.o $out/heap.o $out/runtime.o $out/arm-build/libavcodec/libavcodec.a $out/arm-build/libavutil/libavutil.a --start-group $out/libc-pic.a /usr/lib/gcc/arm-none-eabi/15/thumb/v7-a+simd/softfp/libgcc.a --end-group -o $out/ffmpeg.elf
