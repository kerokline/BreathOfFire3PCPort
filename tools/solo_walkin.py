"""Walk-in saves for the owner (docs/solo-character-proof.md 6b): a tools/chapter_jump.py craft set at a
state-0 field point a little before a fight, with the party the owner asks for, so the owner plays the
rest by hand and the game's own scenes build the fight's party. Game data: the saves are written into the
--out folder (a harness root), never into the tree.

    python tools/solo_walkin.py make mikba43 --donors <root>\\saves --out <root>\\walkin
    python tools/solo_walkin.py show <file>       # the bytes this tool sets, read back

A craft here is chapter_jump.py's (donor, chapter, state, run, step, area, flags, x, z, chapter flags,
`party` ids with `party_set`) plus `joined`: records whose +0xB bit 0 is set (the joined bit,
solo-character-reads.md M6), applied after chapter_jump.make and the checksum (+0x70) written again.
The solo mark (block +0x1041) is written 0, so the launcher's champion is stamped at the first area entry.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chapter_jump as CJ  # noqa: E402

RECORDS = 0x90        # CharacterRecords (0x903A70) in the block (0x9039E0)
STRIDE = 0xA4
MARK = 0x1041         # Solo_ChampionMark

WALKINS = {
    # Chapter 8, area 0x2B (43), on the field with no run (state 2, run 0, step 0, as the owner's own saves hold it), flags 0x14 set and 0x15 clear: walking onto the arrive
    # hook's cells (z 0xF0000 or more, x cells 0x1A..0x26; Scena08_ArriveHook, scena_08_2.cpp) starts run 0xB,
    # Scena08_Scene11, from step 0 - its DropIn(0) and PartySet_LoadFirst(7, 8, 4), step 7's party prompt
    # (Request(6)), Scena08_SetUpRecord4 and record 4 at HP 0, step 9's placement and step 0xA's fight 0x22.
    # The place is the one Scene11 itself puts the party at in area 0x2B after the fight (step 0xE,
    # ChangeArea(0x2B, 0x110000, 0x170000, 0x84)): x cell 0x11, outside the hook's x cells. Flags 0x0F, 0x10,
    # 0x11, 0x13 set as the runs before it leave them (run 9 sets 0xF and 0x10, area 0x15's entry 0x11, run 10
    # 0x14; 0x13 is tested by area 0x15's entry). The party: ids 7, 8, 4 (records 0, 1, 4) in both lists, the
    # party set row 0x0F (ids 4, 7, 8; M1's table), records 1 and 4 joined (the owner's ask: Nina and Rei on
    # the team); the rest of the records as the donor has them.
    'mikba43': dict(donor='minecartToZombieDragon', chapter=8, state=2, run=0, step=0, area=0x2B, flags=0x84,
                    x=0x110000, z=0x170000, cset=[0xF, 0x10, 0x11, 0x13, 0x14], cclr=[0x15],
                    party=[7, 8, 4], party_set=0x0F, joined=[0, 1, 4],
                    why='chapter 8, area 43 before the arrive hook that starts Scena08_Scene11 (fight 34)'),
}


def make(name, donors, out):
    c = dict(WALKINS[name])
    joined = c.pop('joined', [])
    CJ.CRAFTS[name] = c
    CJ.make(name, out, donors)
    path = os.path.join(out, name + '.DAT')
    b = bytearray(open(path, 'rb').read())
    for r in joined:
        b[RECORDS + r * STRIDE + 0xB] |= 1
    b[MARK] = 0
    struct.pack_into('<H', b, CJ.SUM_AT, CJ.checksum(b))
    open(path, 'wb').write(b)
    print(f'{name}: joined records {joined} set, mark 0, checksum {CJ.checksum(b):#06x} -> {path}')
    show(path)


def show(path):
    b = open(path, 'rb').read()
    print(f'chapter {b[0]} state {b[2]} run {b[4]} step {b[5]:#x}; area {struct.unpack_from("<H", b, 0x24)[0]} '
          f'flags {b[0x27]:#x} at ({struct.unpack_from("<i", b, 0x28)[0]:#x}, {struct.unpack_from("<i", b, 0x2C)[0]:#x})')
    print(f'lists {b[CJ.PARTY:CJ.PARTY + 3].hex(" ")} / {b[CJ.PARTY + 3:CJ.PARTY + 6].hex(" ")}; '
          f'party set {b[CJ.PARTY_SET]:#04x}; mark {b[MARK]}')
    for r in range(8):
        o = RECORDS + r * STRIDE
        print(f'  record {r}: id {b[o + 9]:#x} level {b[o + 0xA]} joined {b[o + 0xB] & 1} must-stay {(b[o + 0xB] >> 1) & 1} '
              f'HP {struct.unpack_from("<H", b, o + 0x18)[0]} status {struct.unpack_from("<H", b, o + 0x10)[0]:#06x}')
    row = CJ.CHAPTER_ROWS + 8 * b[0]
    on = [n for n in range(64) if b[row + (n >> 3)] >> (n & 7) & 1]
    print(f'chapter {b[0]} flags set: {" ".join(f"{n:#x}" for n in on)}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('make')
    p.add_argument('name', choices=sorted(WALKINS))
    p.add_argument('--donors', required=True)
    p.add_argument('--out', required=True)
    p = sub.add_parser('show')
    p.add_argument('file')
    a = ap.parse_args()
    if a.cmd == 'make':
        make(a.name, a.donors, a.out)
    else:
        show(a.file)


if __name__ == '__main__':
    main()
