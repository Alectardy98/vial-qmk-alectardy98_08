#!/usr/bin/env python3
"""Generate four original ANIM/RGB565 frames into a new regular file, offline."""
import argparse, os, pathlib, struct
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
assert not str(a.output.resolve()).startswith(('/dev/','/sys/','/proc/')), 'regular build artifact only'
# No manufacturer image/ROM is embedded. Simple gradients and a checker sprite.
frames=[]
for frame in range(4):
    pixels=[]
    for y in range(128):
        for x in range(128):
            color=((x//4)<<11)|((y//2)<<5)|((x+y+frame*8)//8&31)
            if 8+frame*16<=x<24+frame*16 and 32<=y<48:
                color=0xf800 if (x^y)&1 else 0xffe0
            pixels.append(color)
    frames.append(struct.pack('>'+str(len(pixels))+'H',*pixels))
offset=28;payload=b''.join(frames)
blob=struct.pack('<4sHHIHHHH4H',b'ANIM',20,offset,offset+len(payload),128,128,2,4,17,17,16,17)+payload
# O_EXCL preserves any existing capture/asset accidentally supplied as output.
with a.output.open('xb')as f:f.write(blob)
print(a.output)
