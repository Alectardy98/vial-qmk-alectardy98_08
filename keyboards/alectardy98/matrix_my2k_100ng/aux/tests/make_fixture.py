# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic assets only: superfloppy FAT12 or observed-geometry MBR/FAT16."""
import struct, sys
partitioned = '--mbr' in sys.argv[2:]
large = '--large' in sys.argv[2:]
start, count, disk_count = (238, 246290, 247807) if partitioned else (0, 2880, 2880)
spc, fat_sectors = (4, 241) if partitioned else (1, 9)
root_sector = 1 + 2 * fat_sectors
data_sector = root_sector + 14
boot = bytearray(512)
boot[:3] = b'\xeb\x3c\x90'
boot[3:11] = b'MY2KTEST'
struct.pack_into('<HBHBHHBHHHII', boot, 11, 512, spc, 1, 2, 224,
                 0 if partitioned else count, 0xf8 if partitioned else 0xf0,
                 fat_sectors, 18, 2, start, count if partitioned else 0)
boot[510:512] = b'\x55\xaa'
fat = bytearray(fat_sectors * 512)
fat[:8 if partitioned else 6] = bytes.fromhex('f8ffffff ffffffff' if partitioned else 'f0ffffffffff')

def asset(sig, width, height, frames, delays, payload):
    offset = 20 + 2 * frames
    size = offset + len(payload)
    return struct.pack('<4sHHIHHHH', sig, 20, offset, size, width, height, 2, frames) + struct.pack('<'+'H'*frames, *delays) + payload

mono = asset(b'ABIT', 8, 1, 1, [1000], b'\x01')
width,height = (129,2) if large else (2,1)
color = asset(b'ANIM', width, height, 2, [10,20], bytes.fromhex('f80007e0')*(width*height//2)+bytes.fromhex('001fffff')*(width*height//2))
def fat_entry(cluster,value):
    if partitioned:
        struct.pack_into('<H',fat,cluster*2,value)
    else:
        off=cluster*3//2
        previous=struct.unpack_from('<H',fat,off)[0]
        value=((previous&0x000f)|(value<<4)) if cluster&1 else ((previous&0xf000)|value)
        struct.pack_into('<H',fat,off,value)
fat_entry(2,0xffff if partitioned else 0xfff)
clusters=(len(color)+spc*512-1)//(spc*512)
for c in range(3,3+clusters):
    fat_entry(c,c+1 if c<2+clusters else (0xffff if partitioned else 0xfff))
next_color=asset(b'ANIM',width,height,2,[30,40],bytes.fromhex('001fffff')*(width*height//2)+bytes.fromhex('f80007e0')*(width*height//2))
next_cluster=3+clusters
for c in range(next_cluster,next_cluster+clusters):
    fat_entry(c,c+1 if c<next_cluster+clusters-1 else (0xffff if partitioned else 0xfff))
with open(sys.argv[1], 'wb') as image:
    image.truncate(disk_count * 512)  # Sparse synthetic image, no device access.
    if partitioned:
        mbr = bytearray(512)
        mbr[446+4] = 0x06  # FAT16, LBA fields authoritative.
        struct.pack_into('<II', mbr, 446+8, start, count)
        mbr[510:512] = b'\x55\xaa'
        image.write(mbr)
    image.seek(start * 512)
    image.write(boot)
    for sector in (1, 1 + fat_sectors):
        image.seek((start + sector) * 512)
        image.write(fat)
    for entry, (name, data, cluster) in enumerate([(b'BW_TEXT ABW', mono, 2), (b'LIVE    CRS', color, 3), (b'NEXT    CRS',next_color,next_cluster)]):
        root = bytearray(32)
        root[:11] = name
        root[11] = 0x20
        struct.pack_into('<H', root, 26, cluster)
        struct.pack_into('<I', root, 28, len(data))
        image.seek((start + root_sector) * 512 + entry * 32)
        image.write(root)
        image.seek((start + data_sector + (cluster-2)*spc) * 512)
        image.write(data)
