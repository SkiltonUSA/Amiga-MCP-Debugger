#!/usr/bin/env python3
"""Validate and summarize a completed physical ZZFrameBench log."""
import argparse
import json
import re
import statistics
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path)
p.add_argument('--output',type=Path)
a=p.parse_args();text=a.log.read_text()
assert 'MAPPING PASS' in text and 'FRAMEBUFFER_BENCH PASS' in text
assert 'EXIT epilogue=52455431' in text and 'LAUNCHER exit=0;' in text
checks=re.findall(r'^VERIFY .*mismatches=(\d+)$',text,re.M)
assert len(checks)==8 and all(x=='0' for x in checks)
rows=[dict(re.findall(r'(\w+)=([^ ]+)',line)) for line in text.splitlines() if line.startswith('FRAME ')]
assert len(rows)==240
groups=[]
for path in ['ARM-direct','68k-Z3']:
    for width,height in [(320,240),(640,480)]:
        values=[r for r in rows if r['path']==path and int(r['w'])==width and int(r['h'])==height]
        assert len(values)==60 and {int(r['n']) for r in values}==set(range(1,61))
        batches=[]
        for batch in [1,2,3]:
            v=[r for r in values if int(r['batch'])==batch];assert len(v)==20
            batches.append({key:statistics.median(int(r[key]) for r in v) for key in ['arm_us','wall_us']})
        arm=statistics.median(int(r['arm_us']) for r in values)
        wall=statistics.median(int(r['wall_us']) for r in values)
        duration=arm if path=='ARM-direct' else wall
        assert duration>0
        groups.append(dict(path=path,width=width,height=height,bytes_per_frame=width*height*4,
            samples=60,arm_copy_median_ms=arm/1000,host_wait_or_copy_median_ms=wall/1000,
            payload_MB_per_second=width*height*4/duration,
            copy_only_equivalent_fps=1000000/duration,batch_medians_us=batches))
record=dict(passed=True,timed_copies=240,full_frame_checks=8,pixel_mismatches=0,
            pixel_bytes=4,display='640x480 P96 screen; 320x240 rectangle and 640x480 full frame',
            arm_timer_hz_estimate=int(re.search(r'estimated_hz=(\d+)',text)[1]),
            groups=groups,limits=[
                'No video decoding, colour conversion, audio or SDL.',
                'ARM MMU, instruction cache and data cache remain off.',
                'ARM time uses a two-second global-timer frequency estimate.',
                'No vsync, page flipping or tear-free presentation test.',
                'Host wait timing includes 20 ms polling but excludes bitmap locking, mapping sentinels, logging and deliberate inter-frame yielding.',
                'Verification reads pixels across Zorro outside timed sections.',
                'Payload throughput is not a measurement of peak Zorro or DDR bandwidth.'])
result=json.dumps(record,indent=2)+'\n'
if a.output:a.output.write_text(result)
else:print(result,end='')
