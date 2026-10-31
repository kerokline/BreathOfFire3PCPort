#!/usr/bin/env python
"""Print the area-map cell bytes around the leader in a running BOF3.exe.

    python tools/cell_peek.py              # one snapshot
    python tools/cell_peek.py --radius 8   # a wider square
    python tools/cell_peek.py --watch 2    # a snapshot every 2 seconds

Read-only, as tools/mem_watch.py (the process access of tools/task_stacks.py):
nothing is written to the game and no input is sent. Prints the area number
(Game_AreaNumber), the map's width and height (AreaMap_Header +0, +1), the
leader's cell and facing (ObjTrio[0] +0x36 / +0x3A / +8), the view focus
(Field_Kind2X / Z high words) and the byte plane AreaMap_Bytes around the
leader, the leader's own cell in brackets. Rows are z, columns x; '..' is 0.

What the bytes mean to the leader's step on a world map (Field_CellKind,
src/game/member_sprites.cpp; Field_LeaderStepLands, event_leader.cpp): 0
passes; 0x10, 0x40 and 0xF5 refuse the step; 0xA0 / 0xA1 / 0xAE are place
cells, passable, and 0xA1 to 0xA1 passes; 0xC0 passes and arms the area
link on arrival (Area_LinkAt), and a step from 0xC0 onto 0xA1 or 0xAF is
what loads the area.
"""
import argparse, os, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import task_stacks as ts

AREA = 0x904EFC        # Game_AreaNumber (u16)
HEADER = 0x8CB580      # AreaMap_Header (width, height bytes)
BYTES_PTR = 0x905D94   # AreaMap_Bytes (pointer)
TRIO = 0x802D40        # ObjTrio[0], a records::PartyMember
KIND2Z, KIND2X = 0x905E60, 0x905E64


def u16(b, o=0):
    return struct.unpack_from('<H', b, o)[0]


def snapshot(game, radius):
    area = u16(game.read(AREA, 2))
    w, h = game.read(HEADER, 2)
    plane = struct.unpack('<I', game.read(BYTES_PTR, 4))[0]
    trio = game.read(TRIO, 0x40)
    lx, lz, face = u16(trio, 0x36), u16(trio, 0x3A), trio[8]
    kx = struct.unpack('<i', game.read(KIND2X, 4))[0] >> 16
    kz = struct.unpack('<i', game.read(KIND2Z, 4))[0] >> 16
    print(f'area {area}  map {w}x{h}  leader cell ({lx}, {lz}) = (0x{lx:X}, 0x{lz:X}) facing {face}  '
          f'view focus ({kx}, {kz})  plane 0x{plane:X}')
    if not plane or w == 0:
        print('no byte plane loaded')
        return
    x0, x1 = max(0, lx - radius), min(w - 1, lx + radius)
    z0, z1 = max(0, lz - radius), min(h - 1, lz + radius)
    print('      ' + ' '.join(f'{x:02X}' for x in range(x0, x1 + 1)))
    for z in range(z0, z1 + 1):
        row = game.read(plane + z * w + x0, x1 - x0 + 1)
        cells = []
        for i, b in enumerate(row):
            s = '..' if b == 0 else f'{b:02X}'
            cells.append(f'[{s}]' if (x0 + i, z) == (lx, lz) else f' {s} ')
        print(f'z {z:02X} ' + ''.join(cells).replace('  ', ' ').rstrip())


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--radius', type=int, default=6)
    ap.add_argument('--watch', type=float, help='repeat every N seconds')
    a = ap.parse_args()
    game = ts.Game(ts.find_game())
    while True:
        snapshot(game, a.radius)
        if not a.watch:
            break
        print()
        time.sleep(a.watch)


if __name__ == '__main__':
    main()
