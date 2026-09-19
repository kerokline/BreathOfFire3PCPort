#!/usr/bin/env python
"""Convert Breath of Fire III saves between a PlayStation memory card and the
PC port's BISLPS0?.DAT files (docs/save-interchange.md, IDEAS I1).

    python tools/save_convert.py list    CARD.mcr
    python tools/save_convert.py info    bof3/BISLPS00.DAT
    python tools/save_convert.py psx2pc  CARD.mcr SLOT --names-from bof3/BISLPS00.DAT --out bof3/BISLPS05.DAT
    python tools/save_convert.py pc2psx  bof3/BISLPS00.DAT CARD.mcd SLOT --names-from CARD.mcd:1

What the two formats are, measured 2026-09-19:

  PSX  one 8 KiB card block: 0x200 bytes of title and icon frames, then the
       0x10B0-byte game block.
  PC   the game block alone from file offset 0, zero-padded to 0x12B0.

The game block is the same in both - same fields at the same offsets, same
u16 byte-sum at +0x70 - with ONE layout difference: in each of the eight
0xA4-byte character records at +0x90 the name field is 5 bytes on PSX and 9 on
PC, and every later field sits 4 bytes further in. The stride is unchanged; the
PC record gives up four bytes of tail, which no PSX save on hand uses.

Names do not convert: each release stores them in its own text encoding (JP
kana bytes, US ASCII, PC two-byte Chinese). So names are TAKEN FROM A DONOR save
of the destination format, record for record - any save of that format will do,
since all eight records carry their default names from a new game on. Without
--names-from the source bytes are kept, truncated to fit, and will most likely
display as garbage.

SLOT is the card's directory slot, 1-15, as `list` prints it. pc2psx REPLACES
the game block of an existing BoF3 save in that slot (the card is backed up to
CARD.bak first); it does not create directory entries, and the card-manager
title text of that slot is left as it was - the in-game load screen reads the
summary inside the game block, not that text.

Save files are game-derived data: never commit them (CLAUDE.md rule 1).
"""
import argparse
import os
import shutil
import struct
import sys

CARD_SIZE = 0x20000
BLOCK = 0x2000
DIR_FRAME = 0x80
GAME_OFF = 0x200          # game block inside a PSX save file
GAME_LEN = 0x10B0
PC_LEN = 0x12B0
CKSUM = 0x70              # u16 byte-sum of the game block, itself excluded
REC_BASE = 0x90
REC_STRIDE = 0xA4
REC_COUNT = 8
PSX_NAME = 5
PC_NAME = 9
SHIFT = PC_NAME - PSX_NAME
PSX_REC_USED = REC_STRIDE - SHIFT   # PSX bytes that have a home in a PC record
SUMMARY = 0xCA0           # slot summary: 5-byte leader name, then char id at +5
SUMMARY_NAME = 5


def die(msg):
    sys.exit(f'save_convert: {msg}')


def checksum(game):
    return (sum(game[:GAME_LEN]) - game[CKSUM] - game[CKSUM + 1]) & 0xFFFF


def stored_checksum(game):
    return struct.unpack_from('<H', game, CKSUM)[0]


def seal(game):
    struct.pack_into('<H', game, CKSUM, 0)
    struct.pack_into('<H', game, CKSUM, sum(game[:GAME_LEN]) & 0xFFFF)


# --- containers --------------------------------------------------------------

def read_card(path):
    data = bytearray(open(path, 'rb').read())
    if len(data) != CARD_SIZE or data[:2] != b'MC':
        die(f'{path}: not a raw 128 KiB memory-card image')
    return data


def card_slots(card):
    """(slot, filename) for every first-block directory entry."""
    for slot in range(1, 16):
        frame = card[slot * DIR_FRAME:(slot + 1) * DIR_FRAME]
        if frame[0] == 0x51:
            yield slot, frame[0x0A:0x1E].split(b'\0')[0].decode('ascii', 'replace')


def card_game(card, slot, path):
    names = dict(card_slots(card))
    if slot not in names:
        die(f'{path}: directory slot {slot} does not start a save')
    if 'BOF3' not in names[slot]:
        die(f'{path}: slot {slot} is {names[slot]}, not a Breath of Fire III save')
    off = slot * BLOCK + GAME_OFF
    game = bytearray(card[off:off + GAME_LEN])
    if stored_checksum(game) != checksum(game):
        die(f'{path}: slot {slot} fails its checksum - refusing to convert it')
    return game


def read_pc(path):
    data = open(path, 'rb').read()
    if len(data) != PC_LEN:
        die(f'{path}: {len(data)} bytes, a PC save is {PC_LEN}')
    game = bytearray(data[:GAME_LEN])
    if any(data[GAME_LEN:]):
        die(f'{path}: data past 0x{GAME_LEN:X} - not a layout this tool knows')
    if stored_checksum(game) != checksum(game):
        die(f'{path}: fails its checksum - refusing to convert it')
    return game


def read_donor(spec, want):
    """A donor save of format `want` ('pc' or 'psx'): PATH, or CARD:SLOT."""
    if spec is None:
        return None
    path, _, slot = spec.rpartition(':')
    if want == 'psx':
        if not path or not slot.isdigit():
            die('--names-from for pc2psx is CARD:SLOT')
        return card_game(read_card(path), int(slot), path)
    return read_pc(spec)


# --- the one layout difference -----------------------------------------------

def records(game):
    return [game[REC_BASE + i * REC_STRIDE:REC_BASE + (i + 1) * REC_STRIDE]
            for i in range(REC_COUNT)]


def fit_name(name, width):
    name = bytes(name).split(b'\0')[0][:width - 1]
    return name + bytes(width - len(name))


def convert(game, to_pc, donor):
    out = bytearray(game)
    donor_recs = records(donor) if donor is not None else None
    src_w, dst_w = (PSX_NAME, PC_NAME) if to_pc else (PC_NAME, PSX_NAME)
    for i, rec in enumerate(records(game)):
        body = rec[src_w:]
        if to_pc:
            if any(body[PSX_REC_USED - PSX_NAME:]):
                die(f'record {i} uses its last {SHIFT} bytes, which have no place '
                    f'in a PC record - not seen in any save before, stop and look')
            body = body[:PSX_REC_USED - PSX_NAME]
        else:
            body = body + bytes(SHIFT)
        name = donor_recs[i][:dst_w] if donor_recs else fit_name(rec[:src_w], dst_w)
        new = bytes(name) + bytes(body)
        assert len(new) == REC_STRIDE
        out[REC_BASE + i * REC_STRIDE:REC_BASE + (i + 1) * REC_STRIDE] = new
    if donor_recs:
        # The summary names the lead character; find that character's record.
        lead = game[SUMMARY + SUMMARY_NAME]
        id_at = (PC_NAME if to_pc else PSX_NAME)
        for rec in records(out):
            if rec[id_at] == lead:
                out[SUMMARY:SUMMARY + SUMMARY_NAME] = fit_name(rec[:dst_w], SUMMARY_NAME)
                break
    seal(out)
    return out


# --- reporting ---------------------------------------------------------------

def describe(game, pc):
    t = game[0x6E8:0x6EB]
    zenny = struct.unpack_from('<I', game, 0x678)[0]
    area = struct.unpack_from('<H', game, 0x24)[0]
    w = PC_NAME if pc else PSX_NAME
    levels = ' '.join(f'{r[w]}:{r[w + 1]}' for r in records(game))
    ok = 'OK' if stored_checksum(game) == checksum(game) else 'BAD'
    return (f'cksum {ok}  time {t[0]:02d}:{t[1]:02d}:{t[2]:02d}  zenny {zenny}  '
            f'area {area}  party {list(game[0x682:0x685])}  id:level {levels}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('list'); p.add_argument('card')
    p = sub.add_parser('info'); p.add_argument('pcsave')
    p = sub.add_parser('psx2pc')
    p.add_argument('card'); p.add_argument('slot', type=int)
    p.add_argument('--names-from'); p.add_argument('--out', required=True)
    p.add_argument('--force', action='store_true')
    p = sub.add_parser('pc2psx')
    p.add_argument('pcsave'); p.add_argument('card'); p.add_argument('slot', type=int)
    p.add_argument('--names-from')
    a = ap.parse_args()

    if a.cmd == 'list':
        card = read_card(a.card)
        for slot, name in card_slots(card):
            if 'BOF3' in name:
                off = slot * BLOCK + GAME_OFF
                print(f'{slot:2d}  {name:20s} {describe(card[off:off + GAME_LEN], False)}')
            else:
                print(f'{slot:2d}  {name:20s} (another game)')
    elif a.cmd == 'info':
        print(describe(read_pc(a.pcsave), True))
    elif a.cmd == 'psx2pc':
        game = card_game(read_card(a.card), a.slot, a.card)
        out = convert(game, True, read_donor(a.names_from, 'pc'))
        if os.path.exists(a.out) and not a.force:
            die(f'{a.out} exists; --force to overwrite')
        with open(a.out, 'wb') as f:
            f.write(out + bytes(PC_LEN - GAME_LEN))
        print(f'wrote {a.out}: {describe(out, True)}')
    elif a.cmd == 'pc2psx':
        out = convert(read_pc(a.pcsave), False, read_donor(a.names_from, 'psx'))
        card = read_card(a.card)
        card_game(card, a.slot, a.card)          # must already hold a BoF3 save
        shutil.copyfile(a.card, a.card + '.bak')
        off = a.slot * BLOCK + GAME_OFF
        card[off:off + GAME_LEN] = out
        with open(a.card, 'wb') as f:
            f.write(card)
        print(f'replaced slot {a.slot} of {a.card} (backup {a.card}.bak): {describe(out, False)}')


if __name__ == '__main__':
    main()
