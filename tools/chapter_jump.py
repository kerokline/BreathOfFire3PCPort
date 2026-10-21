#!/usr/bin/env python
"""Chapter-jump saves: a recipe save rewritten to start in a chosen scenario
chapter, run and step, in a chosen area at a chosen position, with chosen
chapter and story flags - so a scene no recorded route reaches can be loaded
and captured (memory note chapter-jump-save; docs/ending-backdrop.md 1; the
wide-ring round's group CS, docs/narrow-pillarbox.md 10, docs/wide-ring.md 8).

    python tools/chapter_jump.py list                 # the named crafts
    python tools/chapter_jump.py make cs194 [cs98 ..] # -> <out>/NAME.DAT
    python tools/chapter_jump.py make all --out DIR

The donor is a save in tools/recipe_saves (game data, gitignored; CLAUDE.md
rule 1). The output is game data too: it goes to `--out` (default
BOF3X_RECIPE_SAVES, else tools/recipe_saves), never into the tree. This tool
prints only the bytes it sets, never a donor's contents.

The game block (docs/save-interchange.md; the PC file is the 0x10B0-byte
block from offset 0, the live copy at 0x9039E0):

| Block | RAM | What | Read by |
|---|---|---|---|
| +0x00..+0x0F | 0x8034E0.. | byte 0 the chapter (`Cond_ByteFA`), 1 `Field_StatusBits`, 2 the chapter's state, 4 its run (`MoveScript_Var7`), 5 the run's step | `LoadMenu_Apply` copies the 16 bytes |
| +0x20..+0x23 | 0x903A00 | the four scenario counters (`0x903848`) | `LoadMenu_Apply` |
| +0x24, +0x27, +0x28, +0x2C | 0x903A04.. | the area to enter: u16 area, u8 entry flags, s32 x, s32 z (16.16, a cell is 0x10000) | `GameMode_Start` -> `Field_ChangeArea` |
| +0x70 | | u16 checksum: the byte sum of +0..+0x10AF with this word zeroed | the load menu ("file is damaged" when wrong) |
| +0x5B0 + 8 c | `Cond_Flags` + 8 c | chapter c's flag row (`Chapter_FlagRow`), bit n at byte n >> 3, bit n & 7 | `Flags_Test` |
| +0x650 | `Cond_Flags` + 0xA0 | the story flags the area inits test (`Area167_Init`'s 0x8F / 0x90) | `Flags_Test` |

The rest of the first 16 bytes is zeroed (as `craft.py` of group SW and
`ending17` did). On the load `Area_Enter` stores state 1 and runs the
chapter's EnterArea for the area, then state 2 runs the run at its step - so
a craft with state 2 resumes a run at the step after its own
`Field_ChangeArea`, the way the original arrives there.
"""
import argparse
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAVES = os.path.join(ROOT, 'tools', 'recipe_saves')
BLOCK = 0x10B0
SUM_AT = 0x70
CHAPTER_ROWS = 0x5B0
STORY = 0x650

# name: donor, chapter, state, run, step, area, entry flags, x, z, chapter flags
# set / cleared, story flags set / cleared, counters; and why, with the code
# that arrives there.
CRAFTS = {
    'cs53': dict(donor='ninaWalkBehindBlock', chapter=6, state=2, run=3, step=0xB, area=0x35, flags=0x80,
                 x=0x230000, z=0x290000, cset=[0xA], cclr=[0xB],
                 why='chapter 6 run 3 after its step 0xA (scena_sc6.cpp Scena06_Run03): the camera '
                     'turned to pitch 0 by kind 0x13 and back over 0x78 frames. The donor is the chapter-6 '
                     'save from before the scene (its flag 0xA clear); preLighthouse (after it) crashed in '
                     "Scena06_EnterArea's party join (Sprite_LoadPalette)"),
    'cs98': dict(donor='identify', chapter=10, state=2, run=0, step=0, area=0x62, flags=1,
                 x=0x380000, z=0x240000, cclr=[4],
                 why="area 98 at Scena10_Run2's entry position, no run (the field)"),
    'cs149': dict(donor='adult_ryu', chapter=15, state=2, run=6, step=0x32, area=0x95, flags=0x82,
                  x=0x80000, z=0xC0000, cset=[0x16],
                  why='chapter 15 run 6 after its step 0x31 (Scena15_Run6): Scena15_EnterArea starts the '
                      'camera at distance 0xD00 and zooms it in; step 0x33 shifts the view'),
    'cs153': dict(donor='adult_ryu', chapter=14, state=2, run=0, step=0, area=153, flags=1,
                  x=0x120000, z=0x180000, why='area 153, no scene names it (the field)'),
    'cs154': dict(donor='adult_ryu', chapter=14, state=2, run=0, step=0, area=154, flags=1,
                  x=0x120000, z=0x180000, why='area 154, no scene names it (the field)'),
    'cs186': dict(donor='adult_ryu', chapter=14, state=2, run=0, step=0, area=0xBA, flags=0x80,
                  x=0x48000, z=0x640000,
                  why="area 186 at Area173_Tail38's entry position (area_w4c.cpp): the corridor"),
    'cs192': dict(donor='adult_ryu', chapter=14, state=2, run=0, step=0, area=192, flags=1,
                  x=0x120000, z=0x180000, why='area 192, no scene places it (the field)'),
    'cs194': dict(donor='adult_ryu', chapter=13, state=2, run=2, step=1, area=0xC2, flags=1,
                  x=0x130000, z=0x190000, cclr=[2, 3, 4],
                  why='chapter 13 run 2 after its step 0 (Scena13_Run2): the first visit, '
                      'Camera_Distance 0xF200 pulled back over 0x1E frames (Scena13_EnterArea). CRASHES '
                      'on this donor (a chapter-14 save rewound): the frame after Scena13_SaveParty7 '
                      "swaps the party, Sprite_DrawPass's sort reads past Sprite_DrawList "
                      '(docs/narrow-pillarbox.md 10)'),
    'cs194b': dict(donor='adult_ryu', chapter=13, state=2, run=0, step=0, area=0xC2, flags=1,
                   x=0x130000, z=0x190000, cset=[2, 3, 4],
                   why='area 194 after its first visit (chapter 13 flags 2..4 set): the field, no scene'),
    'cs198': dict(donor='adult_ryu', chapter=15, state=2, run=6, step=0, area=0xC6, flags=0x80,
                  x=0x160000, z=0x120000, cclr=[0x13, 0x14, 0x15, 0x16],
                  why="chapter 15 run 6 from step 0, as Scena15_Run5's step 0x24 leaves it in area 198"),
    'cs167': dict(donor='adult_ryu', chapter=14, state=2, run=0, step=0, area=0xA7, flags=0x0A,
                  x=0x280000, z=0x630000, sset=[0x8F], sclr=[0x90],
                  why='Area167_Init (area_w4a.cpp): Cond_ByteFD 0, story flag 0x8F set and 0x90 clear '
                      '-> Camera_Distance 0x980'),
    'cs19s3': dict(donor='yraallRoad', chapter=1, state=2, run=0, step=0, area=0x13, flags=3,
                   x=0x1B0000, z=0x1B8000, cset=[0x1B], cclr=[2, 3],
                   why="area 19 inside Scenario chapter 1's step-hook box (mode_states.cpp): a step "
                       'there arms run 3 (Scena01_Scene03): the camera out to 0xA00 and back'),
}


def checksum(b):
    s = sum(b[:SUM_AT]) + sum(b[SUM_AT + 2:BLOCK])
    return s & 0xFFFF


def flag(b, base, n, on):
    i, m = base + (n >> 3), 1 << (n & 7)
    b[i] = (b[i] | m) if on else (b[i] & ~m & 0xFF)


def make(name, out_dir, donor_dir):
    c = CRAFTS[name]
    src = os.path.join(donor_dir, c['donor'] + '.DAT')
    b = bytearray(open(src, 'rb').read())
    if len(b) < BLOCK:
        sys.exit(f'{src}: {len(b)} bytes, not a game block')
    got = struct.unpack_from('<H', b, SUM_AT)[0]
    if got != checksum(b):
        sys.exit(f'{src}: its checksum does not follow the rule (+0x70 {got:#06x}); refusing to craft from it')
    b[0:16] = bytes(16)
    b[0], b[2], b[4], b[5] = c['chapter'], c['state'], c['run'] & 0xFF, c['step']
    b[0x20:0x24] = bytes(c.get('counters', [0, 0, 0, 0]))
    struct.pack_into('<H', b, 0x24, c['area'])
    b[0x27] = c['flags']
    struct.pack_into('<ii', b, 0x28, c['x'], c['z'])
    row = CHAPTER_ROWS + 8 * c['chapter']
    for n in c.get('cset', []):
        flag(b, row, n, True)
    for n in c.get('cclr', []):
        flag(b, row, n, False)
    for n in c.get('sset', []):
        flag(b, STORY, n, True)
    for n in c.get('sclr', []):
        flag(b, STORY, n, False)
    struct.pack_into('<H', b, SUM_AT, checksum(b))
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, name + '.DAT')
    open(out, 'wb').write(b)
    print(f"{name}: from {c['donor']}, chapter {c['chapter']} state {c['state']} run {c['run']} step {c['step']:#x}, "
          f"area {c['area']} flags {c['flags']:#x} at ({c['x']:#x}, {c['z']:#x}) -> {out}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('verb', choices=('list', 'make'))
    ap.add_argument('names', nargs='*')
    ap.add_argument('--out', default=os.environ.get('BOF3X_RECIPE_SAVES') or SAVES)
    ap.add_argument('--donors', default=SAVES, help='where the donor saves are (default tools/recipe_saves)')
    a = ap.parse_args()
    if a.verb == 'list':
        for n, c in CRAFTS.items():
            print(f"{n:8} {c['donor']:14} {c['why']}")
        return
    names = list(CRAFTS) if a.names == ['all'] else a.names
    for n in names:
        if n not in CRAFTS:
            sys.exit(f'{n}: no such craft (list them with `list`)')
        make(n, a.out, a.donors)


if __name__ == '__main__':
    main()
