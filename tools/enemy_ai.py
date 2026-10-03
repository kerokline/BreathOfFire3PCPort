#!/usr/bin/env python
"""Print the enemies' AI rows out of the player's own AREAnnn.DAT files, in the
words of our own engine code, for reading against an outside description.

Where the bytes are (all read in our code, cited per field):

    An area's kind-0 chunk of tag 0xC2000, 0x4A8 bytes, loads at 0x8C5580:
        0x00  8 encounter rows of 9 bytes (symbols.toml Encounter_Rows)
        0x48  8 enemy data records of 0x8C bytes (0x8C55C8 + id * 0x8C)
    A record (Battle_CopyEnemyData, battle_sprites.cpp; EnemyAI_*, battle_e5.cpp,
    battle_misc.cpp, battle_damage.cpp; BattleEnemy_PickAction, battle_e2.cpp):
        +0x00 name[12]     +0x0E action-odds index  +0x12 u16 flags
        +0x18 level        +0x1C..+0x23 eight abilities (one picked by Rand & 7)
        +0x24 HP  +0x26 AP  +0x28..+0x2E four stats
        +0x38 four AI rows of 16: +0 condition, +1 change kind, +2 mask,
              +3 value, +4 action-odds index, +6 u16 message, +8..+15 abilities
    The action-odds table 0x65563C in BOF3.exe: a byte per index, four 2-bit
    kinds, one picked by Rand & 3 (0 attack, 1 defend, 2 escape, 3 ability).

The stat names (strength, defence, agility, intelligence, in that order at
+0x28..) and the element names of conditions 0..8 are hypotheses from use,
not read from a name in the binary.

    python tools/enemy_ai.py --game bof3 --area 51
    python tools/enemy_ai.py --game bof3 --name "Tar Man"
    python tools/enemy_ai.py --game bof3 --conditions      # a census of every row's condition and kind

Names come from the sibling checkout's names/enemies.toml and abilities.toml
when it is there (--names), by area and slot; without it the ids are printed.
Output is game data: to the terminal or analysis/, never committed
(CLAUDE.md rule 1).
"""
import argparse
import collections
import glob
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat  # noqa: E402

CHUNK_TAG = 0xC2000
CHUNK_SIZE = 0x4A8
RECORDS_AT = 0x48
STRIDE = 0x8C
ROWS_AT = 0x38
ODDS_TABLE = 0x65563C

# EnemyAI_ChooseActions (before the turn) and EnemyAI_TurnCheck (after a hit).
CONDITIONS = {
    0x00: "hit: element mask 0x001 (fire?)", 0x01: "hit: element mask 0x002 (ice?)",
    0x02: "hit: element mask 0x004 (thunder?)", 0x03: "hit: element mask 0x008",
    0x04: "hit: element mask 0x010", 0x05: "hit: element mask 0x020",
    0x06: "hit: element mask 0x040", 0x07: "hit: element mask 0x100 (abilities only)",
    0x08: "hit: element mask 0x080",
    0x09: "hit: by an ability, amount not 0", 0x0A: "hit: by an attack, amount not 0",
    0x0B: "turn: HP <= max / 4", 0x0C: "turn: HP <= max / 2", 0x0D: "turn: AP <= max / 5",
    0x0E: "turn: any enemy has status 0x2000", 0x0F: "turn: enemies at start > enemies left",
    0x10: "turn: one enemy left", 0x11: "turn: HP > max / 2", 0x12: "turn: AP > max / 2",
    0x13: "turn: counter == 2", 0x14: "turn: counter == 10",
    0x15: "turn: member 0's level - level > 5",
    0x16: "hit: status bit 3", 0x17: "hit: status bit 7", 0x18: "hit: byte +0xAA is 0",
    0x19: "turn: counter even", 0x1A: "turn: counter odd",
    0x1B: "turn: counter % 3 == 0", 0x1C: "turn: counter % 3 != 0",
    0x1D: "turn: one member left", 0x1E: "turn: more than one member",
    0x1F: "turn: no enemy lost", 0x20: "turn: more than one enemy left",
    0x21: "hit: element mask 0x001, every time", 0x22: "hit: element mask 0x002, every time",
    0x23: "hit: element mask 0x004, every time", 0x24: "hit: by an attack, every time",
    0x25: "hit: HP <= the amount",
    0x26: "turn: every other row has fired", 0x27: "turn: the flag 0x904B97 is 1",
    0x63: "off",
}
STATS = ["HP", "AP", "strength", "defence", "agility", "intelligence", "level (factor squared)", "bit 0"]
ACTIONS = ["attack", "defend", "escape", "ability"]


def exe_byte_reader(path):
    blob = open(path, "rb").read()
    pe = struct.unpack_from("<I", blob, 0x3C)[0]
    nsec = struct.unpack_from("<H", blob, pe + 6)[0]
    opt = struct.unpack_from("<H", blob, pe + 20)[0]
    base = struct.unpack_from("<I", blob, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + 40 * i
        vsize, va, rsize, raw = struct.unpack_from("<IIII", blob, o + 8)
        secs.append((base + va, rsize, raw))

    def read(addr):
        for va, rsize, raw in secs:
            if va <= addr < va + rsize:
                return blob[raw + addr - va]
        raise SystemExit(f"0x{addr:X} is in no section's file bytes")
    return read


def odds(read, index):
    if read is None:
        return f"odds index {index}"
    b = read(ODDS_TABLE + index)
    count = collections.Counter((b >> (2 * k)) & 3 for k in range(4))
    return ", ".join(f"{n}/4 {ACTIONS[k]}" for k, n in sorted(count.items()))


def load_names(root):
    """(area, slot) -> name and ability id -> name, from the sibling's generated TOML."""
    enemies, abilities = {}, {}
    try:
        text = open(os.path.join(root, "enemies.toml"), encoding="utf-8").read()
        for block in text.split("[[enemy]]")[1:]:
            a = re.search(r'area = "AREA(\d+)"', block)
            s = re.search(r"slot = (\d+)", block)
            n = re.search(r'us = "([^"]*)"', block) or re.search(r'en = "([^"]*)"', block)
            if a and s and n:
                enemies[(int(a.group(1)), int(s.group(1)))] = n.group(1)
        text = open(os.path.join(root, "abilities.toml"), encoding="utf-8").read()
        for block in text.split("[[ability]]")[1:]:
            i = re.search(r"id = (\d+)", block)
            n = re.search(r'us = "([^"]*)"', block)
            if i and n:
                abilities[int(i.group(1))] = n.group(1)
    except OSError:
        pass
    return enemies, abilities


def area_records(path):
    blob, chunks = dat.load(path)
    for c in chunks:
        if c.kind == 0 and c.tag == CHUNK_TAG and c.size == CHUNK_SIZE:
            body = blob[c.offset:c.offset + c.size]
            return [body[RECORDS_AT + STRIDE * i:RECORDS_AT + STRIDE * (i + 1)] for i in range(8)]
    return []


def ability_list(raw, abilities):
    # The sibling's names are keyed one below the list's byte, and 0 is an
    # empty place: measured on seven enemies (docs/enemy_ai_data.md section 3).
    count = collections.Counter(raw)
    return ", ".join(f"{abilities.get(i - 1, f'#{i - 1}') if i else 'none'} ({n}/8)" for i, n in count.items())


def change(row):
    kind, mask, value = row[1], row[2], row[3]
    if kind == 1:
        return f"flags word = 0x{mask:02X}"
    if kind == 2:
        return f"status cleared, then set to old | 0x{mask:02X}"
    if kind == 3:
        which = [STATS[i] for i in range(8) if mask & (0x80 >> i)]
        return f"scale {', '.join(which)} by {value}/10"
    if kind == 4:
        which = [str(i) for i in range(8) if mask & (0x80 >> i)]
        return f"attribute bytes {', '.join(which)} = {value}"
    if kind == 5:
        which = [n for b, n in ((2, "+0xAA (steal rate?)"), (1, "+0xAE (drop rate?)")) if mask & b]
        return f"{', '.join(which)} = {value}"
    if kind == 6:
        which = [n for b, n in ((2, "+0x96 (experience?)"), (1, "+0x94 (zenny?)")) if mask & b]
        return f"scale {', '.join(which)} by {value}/10"
    if kind == 7:
        return f"the flag 0x904B97 = {mask}"
    return "none" if kind == 0 else f"kind {kind} (no case)"


def show(area, slot, rec, name, read, abilities):
    w = lambda o: struct.unpack_from("<H", rec, o)[0]
    print(f"AREA{area:03d} slot {slot}: {name}")
    print(f"  level {w(0x18)}  HP {w(0x24)}  AP {w(0x26)}  stats {w(0x28)} {w(0x2A)} {w(0x2C)} {w(0x2E)}"
          f"  flags 0x{w(0x12):04X}")
    print(f"  base   : {odds(read, rec[0xE])}; abilities {ability_list(rec[0x1C:0x24], abilities)}")
    for r in range(4):
        row = rec[ROWS_AT + 16 * r:ROWS_AT + 16 * r + 16]
        cond = CONDITIONS.get(row[0], f"0x{row[0]:02X} (no case)")
        if row[0] == 0x63:
            continue
        msg = struct.unpack_from("<H", row, 6)[0]
        print(f"  row {r}  : when {cond}")
        print(f"           change: {change(row)}" + (f"; message 0x{msg:04X}" if msg else ""))
        print(f"           then {odds(read, row[4])}; abilities {ability_list(row[8:16], abilities)}")
    print()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", default=os.environ.get("BOF3_GAME_DIR", "bof3"))
    ap.add_argument("--names", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                    "..", "..", "BreathOfFire3Recomp", "names"))
    ap.add_argument("--area", type=int)
    ap.add_argument("--name", help="only enemies whose name contains this")
    ap.add_argument("--conditions", action="store_true", help="count every row's condition and change kind")
    args = ap.parse_args()

    exe = os.path.join(args.game, "BOF3.exe")
    read = exe_byte_reader(exe) if os.path.exists(exe) else None
    enemies, abilities = load_names(args.names)
    census = collections.Counter()
    seen = set()
    for path in sorted(glob.glob(os.path.join(args.game, "DAT", "AREA[0-9][0-9][0-9].DAT"))):
        area = int(os.path.basename(path)[4:7])
        if args.area is not None and area != args.area:
            continue
        for slot, rec in enumerate(area_records(path)):
            if not any(rec):
                continue
            name = enemies.get((area, slot), f"(record {slot})")
            if args.name and args.name.lower() not in name.lower():
                continue
            if args.conditions:
                for r in range(4):
                    row = rec[ROWS_AT + 16 * r:ROWS_AT + 16 * r + 16]
                    census[(row[0], row[1])] += 1
                continue
            if args.area is None and bytes(rec) in seen:
                continue
            seen.add(bytes(rec))
            show(area, slot, rec, name, read, abilities)
    for (cond, kind), n in sorted(census.items()):
        print(f"{n:5d}  condition 0x{cond:02X} {CONDITIONS.get(cond, '(no case)'):45s} change kind {kind}")


if __name__ == "__main__":
    main()
