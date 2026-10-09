#include <assert.h>
#include <stdio.h>
#include "wire.h"
int main(void)
{
    assert(fb_valid(0x200000,1280,320,240));
    assert(fb_valid(0x200000,2560,640,480));
    assert(!fb_valid(0x1ffffc,1280,320,240));
    assert(!fb_valid(0x200001,1280,320,240));
    assert(!fb_valid(0x200000,1276,320,240));
    assert(!fb_valid(0x200000,1281,320,240));
    assert(!fb_valid(0x200000,0xffffffffu,320,240));
    assert(!fb_valid(0x200000,1280,0,240));
    assert(!fb_valid(0x200000,2560,648,480));
    assert(!fb_valid(0x200000,2560,640,481));
    assert(!fb_valid(0x200000,1280,319,240));
    assert(!fb_valid(0xffffffffu,1280,320,240));
    assert(fb_valid(0x41f0000u-640*480*4,2560,640,480));
    assert(!fb_valid(0x41f0000u-640*480*4+4,2560,640,480));
    assert(FB_SOURCE+FB_MAX_WIDTH*FB_MAX_HEIGHT*4<ZZ_STACK_TOP-16384);
    puts("PASS: framebuffer bounds, overflow rejection and allocation separation");
    return 0;
}
