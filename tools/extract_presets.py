#!/usr/bin/env python3
"""Regenerate src/Presets.h and notes/presets.csv from the Windows DJIReframe.ofx.

Usage: tools/extract_presets.py /path/to/DJIReframe.ofx

The table lives at RVA 0x180022580 as double[5][12][5]; see the README.
"""
import struct, sys, pathlib

TABLE_VA, RDATA_VA, RDATA_FO = 0x180022580, 0x180022000, 0x20a00
PROJ = ['Wide', 'CrystalBall', 'TinyPlanet', 'UltraWide', 'Dewarp']
LABEL = {'Wide': 'Wide', 'CrystalBall': 'Crystal Ball', 'TinyPlanet': 'Tiny Planet',
         'UltraWide': 'Ultra Wide', 'Dewarp': 'Dewarp'}
RES = ['1080p 16:9(1920*1080)', '1080p 9:16(1080*1920)', '1080p 1:1 (1080*1080)',
       '1080p 2.35:1(2538*1080)', '1080p 4:3 (1440*1080)', '1080p 3:4(1080*1440)',
       '4k 16:9(3840*2160)', '4k 9:16(2160*3840)', '4k 1:1(2160*2160)',
       '4k 2.35:1(3840*1634)', 'Source Image', 'Resolution Custom']


def main(dll_path):
    data = pathlib.Path(dll_path).read_bytes()
    vals = struct.unpack_from('<300d', data, TABLE_VA - RDATA_VA + RDATA_FO)
    root = pathlib.Path(__file__).resolve().parent.parent

    with (root / 'notes/presets.csv').open('w') as f:
        f.write('projection_index,projection,resolution_index,resolution,'
                'pan,tilt,roll,fov,distortion\n')
        for p in range(5):
            for r in range(12):
                i = 5 * (r + 12 * p)
                f.write(f'{p},{PROJ[p]},{r},"{RES[r]}",'
                        + ','.join(f'{x:g}' for x in vals[i:i + 5]) + '\n')
    print('wrote notes/presets.csv')
    print('(src/Presets.h is checked in; re-emit by hand if the table ever changes)')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
