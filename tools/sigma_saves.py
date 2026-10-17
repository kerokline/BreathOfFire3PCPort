#!/usr/bin/env python
"""The saves the button routes load (docs/button-routes.md): the swapped-Config
twin of a recipe save, and the chapter-jump saves. Every file this writes is
game data: it goes to a folder outside the tree (the harness root's `saves`),
never into the checkout (CLAUDE.md rule 1).

    python tools/sigma_saves.py words SAVE...                 # the nine pad words of each
    python tools/sigma_saves.py swap SRC DST                  # Speak <-> Action, View <-> Change
    python tools/sigma_saves.py jump SRC DST --chapter N [--state S] [--area A] [--pos X,Z] [--facing F]
    python tools/sigma_saves.py place SRC DST [--area A] [--pos X,Z] [--facing F]   # the scenario bytes kept
    python tools/sigma_saves.py stock DIR                    # copy the main checkout's recipe saves into DIR
    python tools/sigma_saves.py check SAVE...                 # the +0x70 checksum

The save file is the 0x10B0-byte game block from offset 0 and the summary
after it (docs/save-files.md). Inside the block:

- +0x00..+0x0F: loaded straight into 0x8034E0.. by LoadMenu_Apply (byte 0 the
  scenario chapter, byte 2 its state; docs/ending-backdrop.md 1);
- +0x24: the area word; +0x27 the facing; +0x28 / +0x2C the position's x and z
  (s32, 16.16), read off the stock saves (each holds its area and the spot it
  was saved on: F12's save "loads in the field where the party stood",
  save-files.md);
- +0x70: the checksum, u16, the byte sum of +0x00..+0x10AF with that word
  taken as zero (LoadMenu_Read; a wrong one is "file is damaged");
- +0xCBC: the nine pad words LoadMenu_Apply copies to 0x903580.. (word 0
  Speak, 1 Move, 2 Menu, 3 View, 4 Change, 5 select, 6 Action, 7 confirm,
  8 cancel; docs/controls.md 2.1).

`swap` applies the Config panel's own rebind (ConfigScreen_Controller,
original 0x4613B0, src/game/rest_4f.cpp) twice: cross pressed on the Speak
line, then L1 on the View line - the two presses tools/recipes/sigma_swap.txt
makes in the game. So the file it writes is the one the game would write;
sigma_swap.txt's own save is the check (docs/button-routes.md 2).
"""
import argparse, os, shutil, struct, subprocess, sys

BLOCK = 0x10B0
CK = 0x70
WORDS = 0xCBC
AREA, FACING, POS_X, POS_Z = 0x24, 0x27, 0x28, 0x2C
NAMES = ('Speak', 'Move', 'Menu', 'View', 'Change', 'select', 'Action', 'confirm', 'cancel')
PAD_ROWS = (0, 1, 6, 2, 3, 4)      # 0x6536F0: the panel's line -> word (Speak, Move, Action, Menu, View, Change)
BITS = {'l2': 0x1, 'r2': 0x2, 'l1': 0x4, 'r1': 0x8, 'triangle': 0x10, 'circle': 0x20, 'cross': 0x40,
        'square': 0x80, 'select': 0x100, 'start': 0x800}


def read(path):
    with open(path, 'rb') as fh:
        b = bytearray(fh.read())
    if len(b) < BLOCK:
        sys.exit(f'{path}: {len(b)} bytes, not a save (the block alone is 0x{BLOCK:X})')
    return b


def checksum(b):
    return (sum(b[:BLOCK]) - b[CK] - b[CK + 1]) & 0xFFFF


def write(path, b):
    struct.pack_into('<H', b, CK, checksum(b))
    with open(path, 'wb') as fh:
        fh.write(b)


def words(b):
    return list(struct.unpack_from('<9H', b, WORDS))


def rebind(w, line, bit):
    """One press of `bit` on the panel's `line` (0..5), as ConfigScreen_Controller
    does it: bits 0 and 1 off word 0 and the confirm word, the line holding the
    bit takes the cursor line's word, the cursor line's word becomes the bit,
    bits 0 and 1 back on."""
    w = list(w)
    bits = bit & 0xFC
    if not bits:
        return w
    bits &= -bits                      # the lowest bit alone
    w[0] &= 0xFFFC
    w[7] &= 0xFFFC
    cur = PAD_ROWS[line]
    for r in PAD_ROWS:
        if w[r] & bits:
            w[r] = w[cur]
            break
    w[cur] = bits
    w[0] |= 3
    w[7] |= 3
    return w


def swapped(w):
    w = rebind(w, 0, BITS['cross'])    # the Speak line, cross: Speak <-> Action
    return rebind(w, 4, BITS['l1'])    # the View line, L1: View <-> Change


def show(w):
    return ' '.join(f'{n} 0x{v:X}' for n, v in zip(NAMES, w))


def main_checkout():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        common = subprocess.run(['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=here,
                                capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(os.path.normpath(common))
    except (OSError, subprocess.CalledProcessError):
        return here


def inside_tree(path):
    root = os.path.normcase(main_checkout())
    p = os.path.normcase(os.path.abspath(path))
    return p.startswith(os.path.join(root, ''))


def guard(dst):
    if inside_tree(dst):
        sys.exit(f'{dst}: inside the checkout. These saves are game data; write them outside the tree (rule 1)')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('words'); p.add_argument('saves', nargs='+')
    p = sub.add_parser('check'); p.add_argument('saves', nargs='+')
    p = sub.add_parser('swap'); p.add_argument('src'); p.add_argument('dst')
    p = sub.add_parser('jump'); p.add_argument('src'); p.add_argument('dst')
    p.add_argument('--chapter', type=int, required=True)
    p.add_argument('--state', type=int, default=0)
    p.add_argument('--area', type=int)
    p.add_argument('--pos', help='X,Z as s32 16.16 (hex or decimal)')
    p.add_argument('--facing', type=int)
    p = sub.add_parser('place'); p.add_argument('src'); p.add_argument('dst')
    p.add_argument('--area', type=int)
    p.add_argument('--pos', help='X,Z as s32 16.16 (hex or decimal)')
    p.add_argument('--facing', type=int)
    p = sub.add_parser('stock'); p.add_argument('dir')
    a = ap.parse_args()

    if a.cmd == 'words':
        for s in a.saves:
            print(f'{os.path.basename(s)}: {show(words(read(s)))}')
    elif a.cmd == 'check':
        for s in a.saves:
            b = read(s)
            have = struct.unpack_from('<H', b, CK)[0]
            print(f'{os.path.basename(s)}: chapter {b[0]} state {b[2]} area {struct.unpack_from("<H", b, AREA)[0]} '
                  f'checksum {"ok" if have == checksum(b) else "BAD"}')
    elif a.cmd == 'swap':
        guard(a.dst)
        b = read(a.src)
        w0 = words(b)
        w1 = swapped(w0)
        struct.pack_into('<9H', b, WORDS, *w1)
        write(a.dst, b)
        print(f'{os.path.basename(a.src)}: {show(w0)}\n{os.path.basename(a.dst)}: {show(w1)}')
    elif a.cmd in ('jump', 'place'):
        guard(a.dst)
        b = read(a.src)
        if a.cmd == 'jump':
            b[0:16] = bytes(16)
            b[0] = a.chapter & 0xFF
            b[2] = a.state & 0xFF
        if a.area is not None:
            struct.pack_into('<H', b, AREA, a.area)
        if a.facing is not None:
            b[FACING] = a.facing
        if a.pos:
            x, z = (int(v, 0) for v in a.pos.split(','))
            struct.pack_into('<ii', b, POS_X, x, z)
        write(a.dst, b)
        print(f'{a.dst}: chapter {b[0]} state {b[2]} area {struct.unpack_from("<H", b, AREA)[0]} '
              f'at ({struct.unpack_from("<i", b, POS_X)[0]:#x}, {struct.unpack_from("<i", b, POS_Z)[0]:#x}) facing {b[FACING]}')
    elif a.cmd == 'stock':
        guard(a.dir)
        src = os.path.join(main_checkout(), 'tools', 'recipe_saves')
        os.makedirs(a.dir, exist_ok=True)
        n = 0
        for f in sorted(os.listdir(src)):
            if f.upper().endswith('.DAT'):
                shutil.copy2(os.path.join(src, f), os.path.join(a.dir, f))
                n += 1
        print(f'{n} saves copied from {src} to {a.dir}')


if __name__ == '__main__':
    main()
