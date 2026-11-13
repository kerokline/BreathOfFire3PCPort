#!/usr/bin/env python
"""Names for the save's "records and finds" bits, read from the player's install.

    python tools/bank_names.py --game <folder>                # the five tables
    python tools/bank_names.py --game <folder> --tsv OUT.tsv  # the same, to a file
    python tools/bank_names.py --game <folder> --save F.DAT   # one save's set bits, named

    import bank_names; names = bank_names.load(game_dir)      # for a watcher

The five banks of the save block (docs/save-block/range-c-party.md, docs/
story-flags.md "Other flag banks"); bit n of a bank is bit n & 7 of byte n >> 3:

    +0x688  enemy kinds defeated    bit = the enemy data's kind byte +0xC
    +0x6A8  abilities learnt        bit = the ability id's low byte
    +0x6C8  enemy kinds identified  bit = the enemy kind, as +0x688
    +0x6EC  chests and field objects  bit = the content object's +5
    +0x72C  passage finds           bit = the passage entry's byte +4

Where each name comes from (layouts only here; every value is read from the
install at run time, CLAUDE.md rule 1, docs/save-editor.md "Records and finds"):

  - An enemy kind: each DAT\\AREAnnn.DAT's kind-0 chunk of tag 0xC2000 (0x4A8
    bytes, loaded at 0x8C5580): eight enemy data records of 0x8C bytes from
    +0x48, the name the first 12 bytes, the kind the byte +0xC
    (Battle_CopyEnemyData 0x4946C0, Battle_SetupEnemy 0x494320). The file is
    read as the game reads it: the cache's base/ and loc/zh-CN/ (or the
    install's DAT\\), then the language layer (the cache's loc/<tag>/dat/ or
    the install's DAT\\<tag>.AREAnnn.DAT), whose kind-0 chunks of tag
    0xC2000 + 0x48 + k * 0x8C replace record k's name (tools/loc_build.py
    convert_enemy_names, DIV-0053).
  - An ability: the language layer's FIRST.DAT name chunk (kind 5) whose tag
    is NameTable_Abilities + 8 (0x65C4E0): 16-byte names, the first ability
    1's (src/launcher/exe_image.cpp ExeNameTableTags, DIV-0008).
  - A chest or field object: op 9x of an area's placement script
    (Area_Descriptors 0x667590 -> descriptor +0, walked by tools/event_scan.py),
    13 bytes (EventOp_9x 0x57B530): op[5] / op[7] the cell x / z (op[4] /
    op[6] the half cell), op[8] the object's +5 - the bit -, op[9]:op[0xA]
    the dword +0x18, so +0x18 (the category, 0xFF zenny) is op[0xA] and +0x19
    (the item id, or the zenny / 40) is op[9] (Field_ContentTake 0x52FBD0).
    Also the two tables of 9x records area code hands EventOp_9x:
    Area42_Placements 0x5F62E0 (three, area 42) and Area135_SpawnOps 0x62DA50
    (four, area 135).
  - A passage find: the descriptor's +0x2C list of 8-byte entries, +0x31 the
    last index (Area_PassageAhead 0x530600): +0 x, +1 z, +2 bit 7 the run is
    along z, +3 the run's length in cells, +4 the bit (0xFF none), +5 the item
    id or the zenny / 40, +6 the category (0xFF zenny). An entry with the word
    +4's bit 15 set and +6 not 0xFF opens a message or a script instead
    (Field_PassageOpen 0x52F600) and is no find (Field_PassageTake 0x52F760).

The item, ability and enemy names are shown only for a Latin language layer
(en-US, en-150, fr-FR, de-DE), as the launcher's save editor shows them: the
other layers' glyph codes are not letters (src/launcher/save/names.cpp). The
places and the zenny amounts do not need a language.

load() never raises for a missing source: it returns what it could read and
says on stderr what it could not. Its output is game data: print it, or write
it with --tsv outside this repository (the tool refuses a path inside it).
"""
import argparse
import os
import struct
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(TOOLS, '..'))

# The save block's banks (docs/save-block/range-c-party.md).
BANKS = [('enemy', 'Enemies defeated', 0x688), ('skill', 'Skills learned', 0x6A8),
         ('enemy', 'Enemies identified', 0x6C8), ('chest', 'Chests and field objects', 0x6EC),
         ('passage', 'Passage finds', 0x72C)]

AREA_TABLE, AREAS = 0x667590, 200          # Area_Descriptors
OP_LENGTHS = 0x663B0C                      # EventScript_OpLengths
FIELD_MOVE_SPEEDS = 0x6697F0               # read by tools/event_scan.py's walk
OP9_SIZE = 13
OP9_TABLES = [(42, 0x5F62E0, 3), (135, 0x62DA50, 4)]   # Area42_Placements, Area135_SpawnOps
PASSAGE_LIST, PASSAGE_LAST, PASSAGE_SIZE = 0x2C, 0x31, 8

ENEMY_TAG, ENEMY_SIZE, ENEMY_HEAD, ENEMY_STRIDE, ENEMY_COUNT = 0xC2000, 0x4A8, 0x48, 0x8C, 8
ENEMY_NAME, ENEMY_KIND = 12, 0xC

ABILITY_TAG = 0x65C4D8 + 8                 # NameTable_Abilities + 8: ability 1's name
ITEM_TAGS = {0: 0x656B28, 1: 0x657450, 2: 0x657D68, 3: 0x658450, 4: 0x657310}  # Item_NamePtr's categories
CATEGORY_WORD = {0: 'item', 1: 'weapon', 2: 'armor', 3: 'accessory', 4: 'key item'}
NAME_BYTES = 16

LATIN = ('en', 'fr', 'de')


def say(msg):
    sys.stderr.write('bank_names: %s\n' % msg)


# ---- the install ------------------------------------------------------------

def find_install(game_dir=None, cache=None, language=None):
    """(exe folder, cache folder or None, language tag or None)."""
    if game_dir is None:
        try:
            sys.path.insert(0, TOOLS)
            import bof3x_home
            game_dir = bof3x_home.default_game(os.path.join(REPO, 'bof3'))
        except ImportError:
            game_dir = os.path.join(REPO, 'bof3')
    game_dir = os.path.abspath(game_dir)
    exe_dir, homes = game_dir, [game_dir, os.path.dirname(game_dir)]
    if not os.path.isfile(os.path.join(game_dir, 'BOF3.exe')) and \
            os.path.isfile(os.path.join(game_dir, 'game', 'BOF3.exe')):
        exe_dir, homes = os.path.join(game_dir, 'game'), [game_dir]
    ini_cache = None
    for h in homes:
        ini = os.path.join(h, 'bof3x.ini')
        if not os.path.isfile(ini):
            continue
        try:
            for line in open(ini, encoding='utf-8', errors='replace'):
                k, _, v = line.strip().partition('=')
                if k == 'language' and language is None and v:
                    language = v
                elif k == 'cache' and v:
                    ini_cache = v
        except OSError:
            pass
        break
    if cache is None:
        cache = os.environ.get('BOF3X_CACHE') or ini_cache
        if not cache:
            for h in homes:
                if os.path.isdir(os.path.join(h, 'cache', 'base')) or os.path.isdir(os.path.join(h, 'cache', 'loc')):
                    cache = os.path.join(h, 'cache')
                    break
    if cache and not os.path.isdir(cache):
        cache = None
    return exe_dir, cache, language


def latin(language):
    return bool(language) and language[:2] in LATIN


def chunks(path):
    """[(kind, tag, payload)] of a .DAT, or None if it is not there. A
    container cut short ends the list where it breaks."""
    try:
        blob = open(path, 'rb').read()
    except OSError:
        return None
    out, p = [], 0
    while p + 16 <= len(blob):
        kind, tag, size = struct.unpack_from('<III', blob, p)
        if size > len(blob) - p - 16:
            break
        out.append((kind, tag, blob[p + 16:p + 16 + size]))
        p += 16 + size
    return out


def name_text(raw):
    """One name as the save editor shows it (src/launcher/save/names.cpp
    NameText): ASCII bytes as letters, 0x2A a quotation mark, a two-byte glyph
    code a question mark. None for a name with no letter at all - an unset
    record, or a name still in another script's glyph codes."""
    out, i, letters = [], 0, 0
    while i < len(raw) and raw[i]:
        c = raw[i]
        if c & 0x80:
            out.append('?')
            i += 2
            continue
        out.append('"' if c == 0x2A else chr(c))
        letters += chr(c).isalnum()
        i += 1
    text = ''.join(out).strip()
    return text if letters else None


class Exe:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        if d[:2] != b'MZ':
            raise ValueError('not a PE file')
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        if d[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', d, pe + 24)[0] != 0x10B:
            raise ValueError('not a 32-bit PE')
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        opt = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 52)[0]
        self.secs = [struct.unpack_from('<IIII', d, pe + 24 + opt + 40 * i + 8) for i in range(nsec)]

    def read(self, va, n):
        """n bytes at va from a section's initialised part, or None."""
        for _vs, v, rs, ro in self.secs:
            if self.base + v <= va and va + n <= self.base + v + rs:
                o = ro + va - self.base - v
                return self.data[o:o + n]
        return None

    def u32(self, va):
        b = self.read(va, 4)
        return None if b is None else struct.unpack('<I', b)[0]


# ---- the finds: placement scripts and passage lists -------------------------

def op9_records(exe):
    """[(area, op bytes)] for every op 9x the areas' placement scripts hold on
    any path (the structural walk of tools/event_scan.py), and the 9x records
    of the two area tables."""
    sys.path.insert(0, TOOLS)
    import event_scan
    img = event_scan.Image.__new__(event_scan.Image)
    img.data, img.secs = exe.data, event_scan.sections(exe.data)
    lengths = img.bytes(OP_LENGTHS, 16)
    speeds = img.bytes(FIELD_MOVE_SPEEDS, 256)
    out = []
    for area in range(AREAS):
        d = exe.u32(AREA_TABLE + 4 * area)
        start = exe.u32(d) if d else None
        if not start or exe.read(start, 1) is None:
            continue
        walk = event_scan.Walk(img, start, lengths, speeds).run()
        for a, _depth, text in walk.ops:
            if text.startswith('9x '):
                op = exe.read(a, OP9_SIZE)
                if op:
                    out.append((area, op))
    for area, va, count in OP9_TABLES:
        raw = exe.read(va, OP9_SIZE * count)
        if raw is None:
            continue
        for i in range(count):
            op = raw[OP9_SIZE * i:OP9_SIZE * (i + 1)]
            if op[0] >> 4 == 9:
                out.append((area, op))
    return out


def passage_entries(exe):
    """[(area, 8-byte entry)] for every area's passage list."""
    out = []
    for area in range(AREAS):
        d = exe.u32(AREA_TABLE + 4 * area)
        desc = exe.read(d, 0x34) if d else None
        if desc is None:
            continue
        lst = struct.unpack_from('<I', desc, PASSAGE_LIST)[0]
        if not lst:
            continue      # DIV-0197: 105 areas have none
        last = desc[PASSAGE_LAST]
        raw = exe.read(lst, PASSAGE_SIZE * (last + 1))
        if raw is None:
            continue
        out += [(area, raw[PASSAGE_SIZE * i:PASSAGE_SIZE * (i + 1)]) for i in range(last + 1)]
    return out


def holds(category, value, items):
    """What a find gives: an item's name, or the zenny."""
    if category == 0xFF:
        return '%d zenny' % (value * 40)
    table = items.get(category if category in CATEGORY_WORD else 0, [])
    if value < len(table) and table[value]:
        return table[value]
    return '%s %d' % (CATEGORY_WORD.get(category, 'item'), value)


def add(table, bit, text):
    have = table.setdefault(bit, [])
    if text not in have:
        have.append(text)


def places(found):
    """{bit: [(area, x, z, holds)]} -> {bit: text}. The same cell and find in
    several areas (one place in a town's chapter variants) is one entry,
    `area005, area010 - (50, 8): <find>`; different ones are joined by '; '."""
    out = {}
    for bit, rows in found.items():
        groups = []
        for area, x, z, what in rows:
            for g in groups:
                if g[1:] == (x, z, what):
                    if area not in g[0]:
                        g[0].append(area)
                    break
            else:
                groups.append(([area], x, z, what))
        out[bit] = '; '.join('%s - (%d, %d): %s' % (', '.join('area%03d' % a for a in g[0]), g[1], g[2], g[3])
                             for g in groups)
    return out


# ---- the names -------------------------------------------------------------

def name_chunks(paths):
    """{tag: [16-byte names]} from the first FIRST.DAT of `paths` there."""
    for p in paths:
        cs = chunks(p)
        if cs is None:
            continue
        out = {}
        for kind, tag, payload in cs:
            if kind == 5 and len(payload) % NAME_BYTES == 0:
                out[tag] = [name_text(payload[i:i + NAME_BYTES]) for i in range(0, len(payload), NAME_BYTES)]
        return out, p
    return None, None


def enemy_names(exe_dir, cache, language):
    """{kind: [names]} over every area's enemy records, the language layer
    applied; and how many areas were read."""
    dat = os.path.join(exe_dir, 'DAT')
    out, read = {}, 0
    for area in range(AREAS):
        name = 'AREA%03d.DAT' % area
        shipped = []
        if cache and os.path.isfile(os.path.join(cache, 'base', 'dat', name)):
            shipped = [os.path.join(cache, 'base', 'dat', name), os.path.join(cache, 'loc', 'zh-CN', 'dat', name)]
        else:
            shipped = [os.path.join(dat, name)]
        layer = []
        if language and language != 'original':
            cached = os.path.join(cache, 'loc', language, 'dat', name) if cache else None
            if cached and os.path.isdir(os.path.join(cache, 'loc', language)):
                layer = [cached]
            else:
                layer = [os.path.join(dat, '%s.%s' % (language, name))]
        table, patches = None, []
        for p in shipped + layer:
            for kind, tag, payload in chunks(p) or []:
                if kind != 0:
                    continue
                if tag == ENEMY_TAG and len(payload) >= ENEMY_SIZE:
                    table = bytearray(payload[:ENEMY_SIZE])
                elif ENEMY_TAG < tag < ENEMY_TAG + ENEMY_SIZE:
                    patches.append((tag - ENEMY_TAG, payload))
        if table is None:
            continue
        read += 1
        for at, payload in patches:   # the layer's names after the whole table, in file order
            table[at:at + len(payload)] = payload[:ENEMY_SIZE - at]
        for k in range(ENEMY_COUNT):
            rec = table[ENEMY_HEAD + ENEMY_STRIDE * k:ENEMY_HEAD + ENEMY_STRIDE * (k + 1)]
            if not any(rec):
                continue
            text = name_text(bytes(rec[:ENEMY_NAME]))
            if text:
                add(out, rec[ENEMY_KIND], text)
    return out, read


def load(game_dir=None, language=None, cache=None):
    """{'enemy', 'skill', 'chest', 'passage': {bit: text}}; a bit with no name
    is absent, a bit several places share has them all, joined with '; '."""
    result = {'enemy': {}, 'skill': {}, 'chest': {}, 'passage': {}}
    exe_dir, cache, language = find_install(game_dir, cache, language)
    names_ok = latin(language)
    if not names_ok:
        say('language %s: no Latin language layer, so no enemy, skill or item names (places and zenny only)'
            % (language or 'not set'))

    items, ability = {}, []
    if names_ok:
        paths = [os.path.join(exe_dir, 'DAT', language + '.FIRST.DAT')]
        if cache:
            paths.append(os.path.join(cache, 'loc', language, 'dat', 'FIRST.DAT'))
        tables, where = name_chunks(paths)
        if tables is None:
            say('no FIRST.DAT for %s (looked at %s): no skill or item names' % (language, ', '.join(paths)))
        else:
            ability = [None] + tables.get(ABILITY_TAG, [])
            items = {c: tables.get(t, []) for c, t in ITEM_TAGS.items()}
            if len(ability) == 1:
                say('%s holds no ability names' % where)
    for bit in range(256):
        if bit < len(ability) and ability[bit]:
            result['skill'][bit] = ability[bit]

    if names_ok:
        kinds, read = enemy_names(exe_dir, cache, language)
        if not read:
            say('no AREAnnn.DAT with an enemy table under %s%s: no enemy names'
                % (os.path.join(exe_dir, 'DAT'), ' or ' + cache if cache else ''))
        for k, ns in kinds.items():
            result['enemy'][k] = '; '.join(ns)

    exe_path = os.path.join(exe_dir, 'BOF3.exe')
    try:
        exe = Exe(exe_path)
    except (OSError, ValueError) as e:
        say('cannot read %s (%s): no chest or passage places' % (exe_path, e))
        return result
    if exe.u32(AREA_TABLE) is None:
        say('%s has no Area_Descriptors at 0x%X: no chest or passage places' % (exe_path, AREA_TABLE))
        return result

    chest = {}
    try:
        for area, op in op9_records(exe):
            if op[8] != 0xFF:
                chest.setdefault(op[8], []).append((area, op[5], op[7], holds(op[0xA], op[9], items)))
    except Exception as e:     # a walk that cannot read a script: keep what came before
        say('the placement scripts could not be walked (%s)' % e)
    passage = {}
    for area, e in passage_entries(exe):
        if e[4] == 0xFF or (e[5] & 0x80 and e[6] != 0xFF):
            continue
        passage.setdefault(e[4], []).append((area, e[0], e[1], holds(e[6], e[5], items)))
    result['chest'] = places(chest)
    result['passage'] = places(passage)
    return result


# ---- the command line ------------------------------------------------------

def save_block(path):
    """The game block of a PC save (from offset 0) or of a PSX card save file
    (from 0x200; tools/save_convert.py)."""
    b = open(path, 'rb').read()
    return b[0x200:] if len(b) >= 0x2000 and b[:2] == b'SC' else b


def rows(names):
    for key, label, at in BANKS:
        for bit in range(256):
            if bit in names[key]:
                yield label, at, bit, names[key][bit]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--game', help="the install: the folder with BOF3.exe, or the home holding game\\ and cache\\")
    ap.add_argument('--language', help='the language layer (default: bof3x.ini language=)')
    ap.add_argument('--cache', help='the data cache (default: bof3x.ini cache=, else <home>\\cache)')
    ap.add_argument('--tsv', help='write the tables to this path, outside the repository')
    ap.add_argument('--save', nargs='+', help='print the set bits of these saves, named')
    a = ap.parse_args()
    names = load(a.game, a.language, a.cache)
    if a.save:
        for path in a.save:
            block = save_block(path)
            area = struct.unpack_from('<H', block, 0x24)[0]
            print('%s (area %d)' % (path, area))
            for key, label, at in BANKS:
                bits = [n for n in range(256) if block[at + (n >> 3)] >> (n & 7) & 1]
                unnamed = [n for n in bits if n not in names[key]]
                print('  %s: %d set, %d unnamed%s' % (label, len(bits), len(unnamed),
                                                      (' (' + ' '.join(map(str, unnamed)) + ')') if unnamed else ''))
                for n in bits:
                    if n in names[key]:
                        print('    %3d  %s' % (n, names[key][n]))
        return
    if a.tsv:
        out = os.path.normpath(os.path.abspath(a.tsv))
        if os.path.commonpath([out, REPO]) == REPO:
            sys.exit('bank_names: %s is inside this repository: the tables are game data, write them somewhere else' % out)
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# bank\tblock offset\tbit\tname\n')
            for label, at, bit, text in rows(names):
                f.write('%s\t0x%X\t%d\t%s\n' % (label, at, bit, text))
        print('wrote %s' % out)
        return
    for key, label, at in BANKS:
        print('%s (+0x%X): %d of 256 bits named' % (label, at, len(names[key])))
        for bit in range(256):
            if bit in names[key]:
                print('  %3d  %s' % (bit, names[key][bit]))


if __name__ == '__main__':
    main()
