#!/usr/bin/env python
"""The labelling tool: a subsystem proposed for every start the catalogue
cannot label, with the evidence for it and a tier (docs/labelling-pass.md).

    python tools/label_runs.py --catalog <remaining_catalog.tsv> --out <dir>
    python tools/label_runs.py ... --function 0x4F1230      -> one start, every vote
    python tools/label_runs.py ... --validate               -> leave-one-out on ours

tools/remaining_catalog.py labels a start from ranges, names, PSX pairs,
tables, hosts and neighbours; what none of those reaches is its part 7,
"Unlabelled", and its part 2 is the boot-resident code labelled only by the
table or host that holds it. This tool reads the exe for both sets and for
every other start, and votes.

What a start is. The universe is the catalogue's (pc_funcs.json and
pc_hidden.json) plus every symbols.toml function; a start's span runs to the
next start. Its extent is band_rows.read_extent's recursive descent inside the
span (jump tables bounded as magic_rows.py bounds them). Every instruction the
descent reached is decoded again for the absolute addresses it names: a
memory operand's displacement or an immediate in .rdata / .data / .bss
(0x5C4000..0x93D6EC, so the zero-fill globals count, which band_rows' own
drefs leave out), a code address in an immediate, a call through the import
table.

What is known. A start has a class when one of these gives it, in order:
its symbols.toml `impl` file (ours; IMPL_CLASS), its catalogue label
(LABEL_CLASS), its symbols.toml name (NAME_CLASS). Classes are coarse on
purpose - the round-sized subsystems of CLASSES - with the finer source (the
impl file's stem, the catalogue label) kept as the detail.

The votes, each family a distribution over classes, weighted by FAMILY:

  callers    every other start whose code names this one: a direct call,
             jmp or jcc (a linear sweep of .text, band_rows.sweep_refs), or
             an immediate (a pushed or stored function pointer); one vote per
             distinct caller with a class.
  tables     every aligned dword of .rdata / .data that holds the start. The
             table is the one the code indexes: the nearest address at or
             below the cell some start touches (table_readers; a run of code
             pointers is often several tables back to back). It votes the
             class of a symbols.toml data item named at that base, else the
             class of the code that indexes it (a kind's entry and its state
             table), else the classes of its other slots.
  dispatches the slots of every table this start indexes: a dispatcher is
             part of what it dispatches to (BATE's root 0x42D710, whose only
             caller is a game-mode frame).
  callees    for each start this one calls: the classes of that callee's
             other callers, each class's count divided by its size and the
             whole weighted by 1 / the classes sharing it (spread()), so a
             helper everyone calls votes for no one.
  data       the same, for each absolute data address it touches (named ones
             are written into the evidence by their symbols.toml name).
  neighbours the nearest classed start below and above, within 0x3000 bytes
             (the linker kept the source's file order, SHARED_SOURCE.md s2);
             one that is ours counts double one the catalogue placed.
  psx        a pairs_propagated.json pair, never the call-disputed tier: its
             overlay's class.
  weak       a caller, reader or slot whose own class is only the
             catalogue's address guess (WEAK_SOURCES: range, neighbour,
             host) votes here instead of as structure.

The class with the most weight is proposed. The tier, in symbols.toml's
vocabulary: `evidence` when the structural families present (callers,
tables, dispatches, psx) are unanimous for the proposed class and one other
family agrees - evidence of membership in a subsystem, not of a name;
`hypothesis` for any other proposal (neighbours alone is always this);
`unnamed` when nothing votes. Proposals at `evidence` are fed back as known
for a second and third pass (a helper called only from an unlabelled caller
the first pass placed). The passes also cover the boot-resident starts whose
catalogue label maps to no class ('Boot: unnamed').

Suspects (column `suspect`), independent of the label:

  case       a .text dword holds the start (a jump table's case), or the
             descent of the start before it reaches it and nothing outside
             that start names it - a switch case or a continuation, not a
             function (round twelve's eight, battle_e5's 0x44B8D0).
  falls-in   the start before it runs into it without a ret or jmp.
  padding    the start is nop / int3 or does not decode: an alignment
             artefact of the scan.
  orphan     no caller, no table, no immediate and no pair names it: dead
             code or reached some way the scan does not see.
  excluded   inside the statically linked CRT or MP3 decoder (EXCLUDE),
             out of takeover scope by the owner's choice.

--validate runs the same vote for every start that is ours with its own class
hidden - by default with its whole impl file hidden, as an unlabelled run's
neighbours are - and prints the hit rate per tier, the measurement that says
what a tier is worth. Writes only under --out (a scratch directory: the rows are derived
from copyrighted game code and are never committed, CLAUDE.md rule 1).
Addresses are load-bearing constants (rule 3).
"""
import argparse, bisect, collections, csv, json, os, re, struct, sys, tomllib

from capstone import x86

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import magic_rows as mr  # noqa: E402
import band_rows as br   # noqa: E402

DATA_LO, DATA_HI = 0x5C4000, 0x93D6EC      # .rdata, .data and its zero fill (section headers)
NEIGHBOUR_GAP = 0x3000
FAMILY = {'callers': 4.0, 'tables': 4.0, 'psx': 5.0, 'callees': 2.0, 'data': 2.0, 'neighbours': 2.0,
          'weak': 1.0, 'dispatches': 4.0}
STRUCTURAL = ('callers', 'tables', 'psx', 'dispatches')
# A caller or a table reader whose own class is only the catalogue's address
# guess (a range, a neighbour, a host) votes in the weak family, not as
# structure: the game-mode frames at 0x517330 / 0x517340 are "Field objects"
# by range and would otherwise make BATE's root 0x42D710 field code (the
# spot check, docs/labelling-pass.md section 5).
WEAK_SOURCES = ('catalog:range', 'catalog:neighbour', 'catalog:host')

# The statically linked libraries, out of takeover scope (owner's choice). The
# extents are measured (docs/labelling-pass.md section 4): the decoder's first
# function and the CRT's first, each closed under its own calls.
EXCLUDE = [(0x5ADEA0, 0x5B9380, 'MP3 decoder'), (0x5B9380, 0x5C4000, 'MSVC CRT')]

# Tables whose slots are units laid out in address order: a start inside one
# unit's address range gets the unit as its detail (the effect kinds' entries,
# each followed by its state table's states - docs/labelling-pass.md s3).
UNIT_TABLES = {'Effect_KindHandlers': 'effect kind'}

CLASSES = ['battle engine', 'battle effects', 'boss scripts', 'effect objects', 'field core',
           'event script', 'text, windows, menus', 'shop, inn, save point', 'world map',
           'area overlays', 'scenario banks', 'party state, items', 'minigames, master',
           'top-level, platform', 'renderer, PSX library', 'sound',
           'MP3 decoder', 'MSVC CRT']

IMPL_CLASS = [
    (r'magic_|battle_fx_tasks', 'battle effects'),
    (r'boss_', 'boss scripts'),
    (r'battle_|enemy_ai', 'battle engine'),
    (r'area_w\d|area_011', 'area overlays'),
    (r'worldmap_area|world_map', 'world map'),
    (r'scena_', 'scenario banks'),
    (r'event_|move_|field_event', 'event script'),
    (r'msgbox|menu_|window_|save_menu|text_|glyph_|msg_pool|config_text', 'text, windows, menus'),
    (r'shop_', 'shop, inn, save point'),
    (r'item_use|inventory_ops|char_stats', 'party state, items'),
    (r'psx_gte|psx_gpu|d3d_|gfx_|tex_|display_|draw_|prim|sprt_draw', 'renderer, PSX library'),
    (r'sound', 'sound'),
    (r'mode_|title_states|task_sched|win_main|file_io|dat_load|save_io|pad_read|fmv_play|frame_callees',
     'top-level, platform'),
    (r'field_|map_|kind2_object|object_kinds|member_sprites|sprite_|area_', 'field core'),
]

LABEL_CLASS = [
    (r'^Table (Effect_KindHandlers|EffectKind)', 'effect objects'),
    (r'^Table (PartyAction|Member_States)', 'field core'),
    (r'^Table (Effect_Handlers|Battle)', 'battle engine'),
    (r'^Table (Shop|Inn|FieldSave)', 'shop, inn, save point'),
    (r'^Table (MenuList|FieldMenu|Window_)', 'text, windows, menus'),
    (r'^Table WorldMap', 'world map'),
    (r'^Table Area_', 'area overlays'),
    (r'^Table Field', 'field core'),
    (r'^Battle magic', 'battle effects'),
    (r'^Battle|^Boot: battle', 'battle engine'),
    (r'^Boss', 'boss scripts'),
    (r'^Area overlays', 'area overlays'),
    (r'^Scenario|^Boot: Scena', 'scenario banks'),
    (r'^Communication|^Master', 'minigames, master'),
    # the PSX's PLP overlays are the party sets' field actions, which the PC
    # holds with the rest of the field code (field_hidden.cpp's PartyAction*)
    (r'^Party character', 'field core'),
    (r'^Event script', 'event script'),
    (r'^Shop', 'shop, inn, save point'),
    (r'^Field menu|^Text and windows|^Boot: text', 'text, windows, menus'),
    (r'^Field|^Sprite draw|^Map and draw|^Boot: field', 'field core'),
    (r'^Boot: party', 'party state, items'),
    (r'^Top-level|^Boot: top-level|^Task system|^Platform|^Windows shell|^Boot: platform|^Not functions',
     'top-level, platform'),
    (r'^Renderer|^PSX library', 'renderer, PSX library'),
    (r'^Sound', 'sound'),
    (r'^MP3', 'MP3 decoder'),
    (r'^MSVC', 'MSVC CRT'),
]

NAME_CLASS = [
    (r'^(Magic\d|MagicFx|BattleFx|Spell|Fx\d)', 'battle effects'),
    (r'^Boss', 'boss scripts'),
    (r'^(EffectKind|Effect_Run|Effect_Objects|Effect_Camera)', 'effect objects'),
    (r'^(Battle|Enemy|Effect_|Formation|Encounter|DragonCmd|DragonForm|GeneWin|Cmd_|Escape|Damage)', 'battle engine'),
    (r'^Area\d', 'area overlays'),
    (r'^WorldMap', 'world map'),
    (r'^Scena', 'scenario banks'),
    (r'^(Event|Move)', 'event script'),
    (r'^(PartyAction|Member_)', 'field core'),
    (r'^(Msg|Text|Font|Glyph|Window|Menu|Choice|TitleMenu)', 'text, windows, menus'),
    (r'^(Shop|Inn)', 'shop, inn, save point'),
    (r'^(Inventory|Item|Equip|Char_|Ability|Zenny|Party)', 'party state, items'),
    (r'^(Gfx|D3d|Gte|Gpu|Ot|Tex|Clut|Display|Vram|Fmv|Draw|Prim)', 'renderer, PSX library'),
    (r'^(Sound|Snd|Music|Bgm|Spu)', 'sound'),
    (r'^Crt_', 'MSVC CRT'),
    (r'^(Task|Boot|Game_|Title|Mode|Pad|Input|Cfg|Config|File|Dat|LoadDat|Rand$|Win)', 'top-level, platform'),
    (r'^(Field|Sprite|Map|Kind2|Object|Camera|Leader|Member|Anim|Cell)', 'field core'),
]


def classify(table, s):
    for rx, c in table:
        if re.search(rx, s):
            return c
    return None


def excluded(e):
    for lo, hi, name in EXCLUDE:
        if lo <= e < hi:
            return name
    return None


def imports(img):
    """{IAT cell: 'dll!name'} from the import directory."""
    d = img.data
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    opt = pe + 24
    rva, _ = struct.unpack_from('<II', d, opt + 96 + 8)
    out = {}
    if not rva:
        return out
    p = img.off(img.base + rva)
    while True:
        oft, _, _, name_rva, ft = struct.unpack_from('<IIIII', d, p)
        if not name_rva:
            break
        o = img.off(img.base + name_rva)
        dll = d[o:d.index(b'\0', o)].decode('latin1').lower()
        th = oft or ft
        i = 0
        while True:
            w = img.u32(img.base + th + 4 * i)
            if not w:
                break
            if w & 0x80000000:
                nm = '#%d' % (w & 0xFFFF)
            else:
                q = img.off(img.base + w) + 2
                nm = d[q:d.index(b'\0', q)].decode('latin1')
            out[img.base + ft + 4 * i] = dll + '!' + nm
            i += 1
        p += 20
    return out


class World:
    def __init__(self, a):
        self.a = a
        self.img = mr.Image(a.exe)
        sym = br.load_toml(a.symbols)
        self.sym_funcs = {f['pc']: f for f in sym['func']}
        pf = json.load(open(os.path.join(a.analysis, 'pc_funcs.json')))
        self.recorded = {f['entry'] for f in pf['functions']}
        self.hidden = {h['entry']: h for h in json.load(open(os.path.join(a.analysis, 'pc_hidden.json')))}
        img = self.img
        self.starts = sorted({e for e in self.recorded | set(self.hidden) | set(self.sym_funcs)
                              if img.text_lo <= e < img.text_hi})
        self.iat = imports(img)

        # catalogue rows (not ours)
        self.cat = {}
        with open(a.catalog, encoding='utf-8') as fh:
            for r in csv.DictReader(fh, delimiter='\t'):
                self.cat[int(r['entry'], 16)] = r

        # data names: items (sized by ctype and count) and blocks
        size_of = {'char': 1, 'unsigned char': 1, 'signed char': 1, 'short': 2, 'unsigned short': 2}
        items = []
        for d in sym['data']:
            n = size_of.get(d.get('ctype', ''), 4) * max(1, d.get('count', 1))
            items.append((d['pc'], d['pc'] + n, d['name']))
        items.sort()
        self.items = items
        self.item_lo = [x[0] for x in items]
        self.blocks = sorted((b['pc'], b['pc'] + (b.get('pc_stride', 0) * 8 or 0x400), b['name'])
                             for b in sym['block'])
        self.units = []           # (slot target, table name, index), by address
        for d in sym['data']:
            if d['name'] in UNIT_TABLES and d.get('count'):
                for i in range(d['count']):
                    v = self.img.u32(d['pc'] + 4 * i)
                    if v and self.img.in_text(v):
                        self.units.append((v, d['name'], i))
        self.units.sort()
        self.unit_lo = [u[0] for u in self.units]

        # PSX pairs at an acceptable tier, and the sibling's overlay families
        self.pairs = {}
        for p in json.load(open(os.path.join(a.analysis, 'pairs_propagated.json'))):
            if p['how'] == 'call-disputed':
                continue
            self.pairs.setdefault(p['pc'], p)
        self.overlays = {}
        ov = os.path.join(a.sibling, 'names', 'overlays.toml')
        if os.path.exists(ov):
            self.overlays = {o['md5']: o for o in br.load_toml(ov)['overlay']}

        self.known = {}      # start -> (class, detail, source)
        for e in self.starts:
            k = self.base_class(e)
            if k:
                self.known[e] = k
        self.class_size = collections.Counter(k[0] for k in self.known.values())
        self.read_all()

    # ---- classes ----------------------------------------------------------
    def base_class(self, e):
        f = self.sym_funcs.get(e)
        if f and 'impl' in f:
            c = classify(IMPL_CLASS, f['impl'].split('/')[-1])
            if c:
                return c, os.path.splitext(f['impl'].split('/')[-1])[0], 'ours'
        x = excluded(e)
        if x:
            return x, x, 'range'
        r = self.cat.get(e)
        if r and r['part'] != '7 Unlabelled' and r['how'] != 'pair:call-disputed':
            # (the catalogue's first-pair-wins lets seven call-disputed pairs
            # label a row; that tier is never used here)
            c = classify(LABEL_CLASS, r['label'])
            if c:
                return c, r['label'], 'catalog:' + r['how'].split(':')[0]
        if f:
            c = classify(NAME_CLASS, f['name'])
            if c:
                return c, f['name'], 'name'
        return None

    def psx_class(self, e):
        p = self.pairs.get(e)
        if not p:
            return None
        if p['section'] == 'boot':
            return None, 'psx boot %08X (%s)' % (p['psx'], p['how'])
        o = self.overlays.get(p['section'])
        nm = o['name'] if o else p['section'][:8]
        fam = o['family'] if o else ''
        lab = {'GAME': 'Field core', 'SHOP': 'Shop', 'START': 'Field menu', 'BATTLE': 'Battle engine',
               'BATL_END': 'Battle result', 'BATE': 'Battle extra', 'BATL_OVR': 'Battle overlay'}.get(nm)
        if not lab:
            lab = {'BMAGIC': 'Battle magic', 'BOSS': 'Boss', 'SCENARIO': 'Scenario', 'PLCHAR': 'Party character',
                   'COMMU': 'Communication'}.get(fam, 'Area overlays' if fam.startswith('WORLD') else fam)
        return classify(LABEL_CLASS, lab or ''), 'psx %s %08X (%s)' % (nm, p['psx'], p['how'])

    def data_name(self, addr):
        i = bisect.bisect_right(self.item_lo, addr) - 1
        if i >= 0 and self.items[i][0] <= addr < self.items[i][1]:
            lo, _, n = self.items[i]
            return n if addr == lo else '%s+%X' % (n, addr - lo)
        for lo, hi, n in self.blocks:
            if lo <= addr < hi:
                return '%s+%X' % (n, addr - lo)
        return None

    def sname(self, e):
        f = self.sym_funcs.get(e)
        return f['name'] if f else '0x%06X' % e

    def limit(self, s):
        i = bisect.bisect_right(self.starts, s)
        return self.starts[i] if i < len(self.starts) else self.img.text_hi

    def owner(self, addr):
        i = bisect.bisect_right(self.starts, addr) - 1
        return self.starts[i] if i >= 0 else None

    # ---- reading ----------------------------------------------------------
    def read_all(self):
        img = self.img
        self.ext = {}
        self.fwd = {}           # start -> dict(calls, imms, data, iat)
        for s in self.starts:
            d = br.read_extent(img, s, self.limit(s))
            data, iat, imms = collections.Counter(), set(), set()
            for pc in d['seen']:
                ins = mr.decode(img, pc)
                for op in ins.operands:
                    if op.type == x86.X86_OP_MEM:
                        v = op.mem.disp & 0xFFFFFFFF
                        if v in self.iat:
                            iat.add(self.iat[v])
                        elif DATA_LO <= v < DATA_HI:
                            data[v] += 1
                    elif op.type == x86.X86_OP_IMM and ins.mnemonic != 'call' and not ins.mnemonic.startswith('j'):
                        v = op.imm & 0xFFFFFFFF
                        if DATA_LO <= v < DATA_HI and ins.mnemonic not in ('cmp', 'test', 'and', 'or', 'xor'):
                            data[v] += 1
                        elif img.in_text(v):
                            imms.add(v)
            calls = {t for _, k, t in d['outs'] if k in ('call', 'jmp')}
            self.ext[s] = d
            self.fwd[s] = dict(calls=calls, imms=imms, data=data, iat=iat)
        # reverse: direct transfers and immediates, from the linear sweep
        refs, _ = br.sweep_refs(img, self.starts, set(self.starts))
        self.rev = collections.defaultdict(set)      # target -> {(from start, kind)}
        self.rev_sites = collections.defaultdict(list)
        for t, sites in refs.items():
            for site, kind in sites:
                o = self.owner(site)
                if o == t and kind in ('jcc', 'jmp'):
                    continue
                self.rev[t].add((o, kind))
                self.rev_sites[t].append((site, kind))
        # immediates the descent read (a stack table built in code, a push)
        for s, f in self.fwd.items():
            for v in f['imms']:
                if v in self.ext:
                    self.rev[v].add((s, 'imm'))
        # cells: .rdata / .data dwords, and .text ones (jump tables)
        cells = br.cell_refs(img, set(self.starts))
        self.tables = collections.defaultdict(list)   # target -> [(table start, cell)]
        self.text_cells = collections.defaultdict(list)
        self.table_slots = collections.defaultdict(list)
        for t, lst in cells.items():
            for cell, sec in lst:
                if sec == '.text':
                    self.text_cells[t].append(cell)
                    continue
                ts = br.run_start(img, cell, limit=4096)
                self.tables[t].append((ts, cell))
        for t, lst in self.tables.items():
            for ts, cell in lst:
                self.table_slots[ts].append((cell, t))
        # who touches each data address
        self.touchers = collections.defaultdict(set)
        for s, f in self.fwd.items():
            for v in f['data']:
                self.touchers[v].add(s)
        self.callers_of = collections.defaultdict(set)
        for t, lst in self.rev.items():
            for o, k in lst:
                self.callers_of[t].add(o)
        # the slots of every table, by the base its code indexes
        self.slots_of_base = collections.defaultdict(set)
        for t, lst in self.tables.items():
            for ts, cell in lst:
                base, _ = self.table_readers(ts, cell)
                self.slots_of_base[base].add(t)

    def table_readers(self, ts, cell):
        """The code that indexes the table holding `cell`: the starts touching
        the highest address at or below the cell (down to the run's start less
        two dwords - a table read as base - 4 * 1, Field_ObjectTriggers' shape).
        A run of code pointers can be several tables back to back; the nearest
        touched base below the cell is its own table's."""
        a = cell
        while a >= ts - 8:
            if a in self.touchers:
                return a, self.touchers[a]
            a -= 4
        return ts, set()

    def table_bounds(self, ts, cell):
        """[base, end) of the one table a cell belongs to: from the touched base
        at or below it (table_readers) to the next touched cell above it or
        the run's end - a run of code pointers can be several tables."""
        base, _ = self.table_readers(ts, cell)
        p = cell + 4
        while True:
            v = self.img.u32(p)
            if v is None or not self.img.in_text(v) or (p in self.touchers and p > base + 4):
                return base, p
            p += 4

    def unit_of(self, e):
        i = bisect.bisect_right(self.unit_lo, e) - 1
        if i < 0 or e - self.units[i][0] > NEIGHBOUR_GAP:
            return None
        v, t, k = self.units[i]
        return '%s %s %d (0x%06X)' % (t, UNIT_TABLES[t], k, v)

    def table_name(self, ts, cell):
        n = self.data_name(cell)
        if n:
            return n.split('+')[0]
        n = self.data_name(ts)
        return n.split('+')[0] if n else None

    # ---- votes ------------------------------------------------------------
    def spread(self, acc, dist):
        """Add one shared callee's or data address's class distribution to
        acc: each class's count divided by its size (a class of 1,500 spell
        functions touches everything more often than one of 60), normalised,
        and weighted by 1 / the number of classes sharing it - Sprite_Current,
        which fourteen classes touch, says almost nothing. Returns the weight."""
        if not dist:
            return 0.0
        p = {c: n / max(1, self.class_size[c]) for c, n in dist.items()}
        tot = sum(p.values())
        wgt = 1.0 / len(dist)
        for c, v in p.items():
            acc[c] += wgt * v / tot
        return wgt

    def votes(self, e, known, hide_self=True):
        """{family: Counter(class -> weight)}, and the evidence strings."""
        fam = {k: collections.Counter() for k in FAMILY}
        detail = collections.Counter()
        ev = collections.defaultdict(list)

        def kc(x):
            if x == e and hide_self:
                return None
            return known.get(x)

        # callers
        for o in sorted(self.callers_of.get(e, ())):
            if o == e:
                continue
            k = kc(o)
            if k:
                fam['weak' if k[2] in WEAK_SOURCES else 'callers'][k[0]] += 1
                detail[(k[0], k[1])] += 1
            ev['callers'].append('%s%s' % (self.sname(o), '(%s%s)' % (
                '~' if k[2] in WEAK_SOURCES else '', k[0]) if k else '(?)'))
        # tables
        for ts, cell in sorted(set(self.tables.get(e, ()))):
            slots = self.table_slots.get(ts, [])
            base, readers = self.table_readers(ts, cell)
            # the table's own name: a data item starting at its base (or at
            # base + 4, a table read as base - 4); a name that only covers
            # the cell may be a neighbour's count run long (EffectKind18_States)
            tn = next((self.data_name(b) for b in (base, base + 4, base + 8)
                       if self.data_name(b) and '+' not in self.data_name(b)), None)
            c = (classify(LABEL_CLASS, 'Table ' + tn) or classify(NAME_CLASS, tn)) if tn else None
            weak = False
            if not c and readers:
                # the code that indexes the table: a kind's dispatcher and its
                # state table, a mode and its steps
                rk = [kc(o) for o in readers if kc(o)]
                rc = collections.Counter(k[0] for k in rk)
                if rc:
                    c = rc.most_common(1)[0][0]
                    weak = all(k[2] in WEAK_SOURCES for k in rk)
                    tn = 'table 0x%06X read by %s' % (base, self.sname(sorted(readers)[0]))
            if not c:
                tn = tn or self.table_name(ts, cell)
                c = (classify(LABEL_CLASS, 'Table ' + tn) or classify(NAME_CLASS, tn)) if tn else None
            ts = base
            if not c:
                sc = collections.Counter()
                wk = []
                for _, t in slots:
                    if t != e:
                        k = kc(t)
                        if k:
                            sc[k[0]] += 1
                            wk.append(k[2] in WEAK_SOURCES)
                if sc:
                    c = sc.most_common(1)[0][0]
                    weak = all(wk)
            if c:
                fam['weak' if weak else 'tables'][c] += 1
                detail[(c, tn or 'table 0x%06X' % ts)] += 1
            ev['tables'].append('%s[%d]%s' % (tn or 'table 0x%06X' % ts, (cell - ts) // 4, '(%s)' % c if c else '(?)'))
        # psx
        pc = self.psx_class(e)
        if pc:
            if pc[0]:
                fam['psx'][pc[0]] += 1
            ev['psx'].append(pc[1])
        f = self.fwd.get(e, {})
        # dispatches: the slots of the tables this start indexes (a kind's
        # entry and its states, a mode's root and its steps)
        for v in sorted(f.get('data', ())):
            for t in sorted(self.slots_of_base.get(v, ())):
                if t == e:
                    continue
                k = kc(t)
                if k:
                    fam['weak' if k[2] in WEAK_SOURCES else 'dispatches'][k[0]] += 1
                    detail[(k[0], k[1])] += 1
                ev['dispatches'].append('%s%s' % (self.sname(t), '(%s)' % k[0] if k else '(?)'))
        # callees
        n = 0
        for g in sorted(f.get('calls', ())):
            if g == e or g not in self.ext:
                continue
            dist = collections.Counter()
            for o in self.callers_of.get(g, ()):
                if o != e:
                    k = kc(o)
                    if k:
                        dist[k[0]] += 1
            k = kc(g)
            ev['callees'].append(self.sname(g) + ('(%s)' % k[0] if k else ''))
            n += self.spread(fam['callees'], dist)
        if n:
            for c in fam['callees']:
                fam['callees'][c] /= n
        for i in sorted(f.get('iat', ())):
            ev['imports'].append(i)
        # data
        n = 0
        named = collections.Counter()
        for v, cnt in f.get('data', {}).items():
            nm = self.data_name(v)
            if nm:
                named[nm.split('+')[0]] += cnt
            dist = collections.Counter()
            for o in self.touchers.get(v, ()):
                if o != e:
                    k = kc(o)
                    if k:
                        dist[k[0]] += 1
            n += self.spread(fam['data'], dist)
        if n:
            for c in fam['data']:
                fam['data'][c] /= n
        ev['data'] = ['%s x%d' % kv for kv in named.most_common(6)]
        if f.get('data') and not named:
            ev['data'] = ['%d unnamed addresses' % len(f['data'])]
        # neighbours
        i = bisect.bisect_left(self.starts, e)
        lo = hi = None
        for j in range(i - 1, -1, -1):
            x = self.starts[j]
            if e - x > NEIGHBOUR_GAP:
                break
            if kc(x):
                lo = x
                break
        for j in range(i + 1, len(self.starts)):
            x = self.starts[j]
            if x - e > NEIGHBOUR_GAP:
                break
            if kc(x):
                hi = x
                break
        for x in (lo, hi):
            if x is not None:
                k = kc(x)
                # a neighbour that is ours (read and written) counts double
                # one the catalogue placed by address
                wn = 1.0 if k[2] == 'ours' else 0.5
                fam['neighbours'][k[0]] += wn
                detail[(k[0], k[1])] += wn
        ev['neighbours'] = ['%s(%s)' % (self.sname(x), kc(x)[0]) if x is not None else '-' for x in (lo, hi)]
        return fam, detail, ev

    def propose(self, e, known, hide_self=True):
        fam, detail, ev = self.votes(e, known, hide_self)
        score = collections.Counter()
        for k, dist in fam.items():
            tot = sum(dist.values())
            if tot:
                for c, w in dist.items():
                    score[c] += FAMILY[k] * w / tot
        if not score:
            return None, '', 'unnamed', fam, ev
        best = score.most_common(1)[0][0]
        det = [d for (c, d), _ in detail.most_common() if c == best]
        if best == 'effect objects' and self.unit_of(e):
            det = [self.unit_of(e)]
        present = [k for k, d in fam.items() if sum(d.values())]
        struct_ok = [k for k in STRUCTURAL if sum(fam[k].values())]
        unanimous = struct_ok and all(set(fam[k]) == {best} for k in struct_ok)
        others = [k for k in present if k not in STRUCTURAL and fam[k].most_common(1)[0][0] == best]
        if unanimous and others:
            tier = 'evidence'
        else:
            tier = 'hypothesis'
        return best, det[0] if det else '', tier, fam, ev

    # ---- suspects ---------------------------------------------------------
    def suspects(self, e):
        out = []
        x = excluded(e)
        if x:
            out.append('excluded:' + x)
        ins = mr.decode(self.img, e)
        if ins is None or ins.mnemonic in ('nop', 'int3') or self.img.u8(e) == 0:
            out.append('padding')
        # a .text dword holding the start counts only inside a run of two or
        # more code pointers (a jump table), not a lone immediate's bytes
        jt = [c for c in self.text_cells.get(e, ())
              if any(self.img.in_text(self.img.u32(c + d) or 0) for d in (-4, 4))]
        if jt:
            out.append('case(text table 0x%06X)' % jt[0])
        i = bisect.bisect_left(self.starts, e)
        if i > 0:
            p = self.starts[i - 1]
            dp = self.ext[p]
            if dp['falls'] and dp['bad'] is None:
                out.append('falls-in(from 0x%06X)' % p)
            else:
                # the start before, read on past this one: does its code reach it?
                d2 = br.read_extent(self.img, p, self.limit(e))
                if e in d2['seen']:
                    outside = [o for o, k in self.rev.get(e, ()) if o != p] or self.tables.get(e)
                    if not outside:
                        out.append('case(reached inside 0x%06X)' % p)
        if not self.rev.get(e) and not self.tables.get(e) and not self.text_cells.get(e) \
                and e not in self.pairs:
            out.append('orphan')
        return out


def fmt_ev(ev):
    parts = []
    for k in ('callers', 'tables', 'dispatches', 'psx', 'callees', 'imports', 'data', 'neighbours'):
        v = ev.get(k)
        if v:
            more = '' if len(v) <= 6 else ' +%d' % (len(v) - 6)
            parts.append('%s: %s%s' % (k, ', '.join(v[:6]), more))
    return ' | '.join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--analysis', default='analysis')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--sibling', default='../BreathOfFire3Recomp')
    ap.add_argument('--catalog', required=True, help="tools/remaining_catalog.py's --tsv at the same symbols.toml")
    ap.add_argument('--out', help='directory for labels.tsv, runs.tsv, boot_tables.tsv')
    ap.add_argument('--function', help='print one start with every vote')
    ap.add_argument('--validate', action='store_true', help='leave-one-out over the starts that are ours')
    ap.add_argument('--holdout', choices=('start', 'file'), default='file',
                    help="--validate hides one start, or its whole impl file (the default: the harder test)")
    ap.add_argument('--passes', type=int, default=3)
    a = ap.parse_args()
    w = World(a)

    if a.function:
        for tok in a.function.split(','):
            e = int(tok, 16)
            c, det, tier, fam, ev = w.propose(e, w.known)
            print('0x%06X size %d  catalog %s  proposed %s (%s) %s' % (
                e, w.ext[e]['end'] - e, w.cat[e]['label'] if e in w.cat else 'ours', c, det, tier))
            for k, d in fam.items():
                if d:
                    print('  %-10s %s' % (k, ', '.join('%s %.2f' % kv for kv in d.most_common())))
            print('  ' + fmt_ev(ev).replace(' | ', '\n  '))
            print('  suspects:', ', '.join(w.suspects(e)) or '-')
        return

    if a.validate:
        ours = [e for e, k in w.known.items() if k[2] == 'ours']
        tally = collections.defaultdict(lambda: [0, 0])
        conf = collections.Counter()
        fam_hit = collections.defaultdict(lambda: [0, 0])
        by_file = collections.defaultdict(list)
        for e in ours:
            by_file[w.known[e][1]].append(e)
        for e in ours:
            if a.holdout == 'file':
                # the whole impl file hidden at once: the start's own group is
                # unlabelled, as an unlabelled run's neighbours are
                known = w.known
                hid = {x: known.pop(x) for x in by_file[w.known[e][1]] if x != e}
                truth = w.known[e][0]
                c, _, tier, fam, _ = w.propose(e, known)
                known.update(hid)
            else:
                c, _, tier, fam, _ = w.propose(e, w.known)
            truth = w.known[e][0]
            tally[tier][1] += 1
            tally[tier][0] += c == truth
            only = [k for k, d in fam.items() if sum(d.values())]
            key = 'neighbours only' if only == ['neighbours'] else ('no structural' if not any(k in STRUCTURAL for k in only) else 'structural')
            fam_hit[key][1] += 1
            fam_hit[key][0] += c == truth
            if c != truth:
                conf[(truth, c)] += 1
        print('leave-one-out over %d starts that are ours' % len(ours))
        for t, (h, n) in sorted(tally.items()):
            print('  %-11s %5d of %5d  %.1f%%' % (t, h, n, 100.0 * h / max(n, 1)))
        for t, (h, n) in sorted(fam_hit.items()):
            print('  %-16s %5d of %5d  %.1f%%' % (t, h, n, 100.0 * h / max(n, 1)))
        print('  commonest misses (truth -> proposed):')
        for (t, g), n in conf.most_common(12):
            print('    %4d  %s -> %s' % (n, t, g))
        return

    # targets: the catalogue's parts 7 and 2
    targets = {e: r['part'][0] for e, r in w.cat.items() if r['part'][0] in '27'}
    known = dict(w.known)
    for e in targets:
        if targets[e] == '7':
            known.pop(e, None)
    res = {}
    # the passes run over the unlabelled starts and the boot-resident ones the
    # catalogue's label gives no class ('Boot: unnamed', an unknown name prefix)
    passing = {e for e in targets if targets[e] == '7' or e not in known}
    for p in range(1, a.passes + 1):
        added = 0
        for e in sorted(passing):
            if e in res and res[e][2] == 'evidence':
                continue
            c, det, tier, fam, ev = w.propose(e, known)
            res[e] = (c, det, tier, ev, p)
        for e, (c, det, tier, ev, pp) in res.items():
            if tier == 'evidence' and e not in known:
                known[e] = (c, det, 'label pass %d' % pp)
                added += 1
        print('pass %d: %d evidence proposals fed back' % (p, added))
    for e in targets:
        if targets[e] == '2' and e not in passing:
            c, det, tier, fam, ev = w.propose(e, known)
            res[e] = (c, det, tier, ev, 0)

    # runs of unlabelled starts: adjacent in the start list, no known start between,
    # gap at most 0x400 bytes
    run_of, runs = {}, []
    cur = []
    for e in w.starts:
        if targets.get(e) == '7':
            if cur and e - w.ext[cur[-1]]['end'] > 0x400:
                runs.append(cur); cur = []
            cur.append(e)
        elif cur:
            runs.append(cur); cur = []
    if cur:
        runs.append(cur)
    for i, r in enumerate(runs):
        for e in r:
            run_of[e] = 'R%04d' % (i + 1)

    if not a.out:
        return
    os.makedirs(a.out, exist_ok=True)
    rows = []
    with open(os.path.join(a.out, 'labels.tsv'), 'w', encoding='utf-8', newline='') as fh:
        wr = csv.writer(fh, delimiter='\t')
        wr.writerow(['entry', 'size', 'run', 'set', 'hidden', 'proposed', 'detail', 'tier', 'pass',
                     'suspect', 'catalog_label', 'evidence'])
        for e in sorted(targets):
            c, det, tier, ev, p = res[e]
            sus = w.suspects(e)
            size = w.ext[e]['end'] - e
            row = ['0x%06X' % e, size, run_of.get(e, ''), 'unlabelled' if targets[e] == '7' else 'boot',
                   int(e in w.hidden), c or '', det, tier, p, ';'.join(sus), w.cat[e]['label'], fmt_ev(ev)]
            wr.writerow(row)
            rows.append(row)
    with open(os.path.join(a.out, 'runs.tsv'), 'w', encoding='utf-8', newline='') as fh:
        wr = csv.writer(fh, delimiter='\t')
        wr.writerow(['run', 'from', 'to', 'functions', 'bytes', 'proposed', 'share', 'evidence', 'hypothesis',
                     'unnamed', 'suspects', 'below', 'above'])
        for i, r in enumerate(runs):
            cc = collections.Counter(res[e][0] for e in r if res[e][0])
            tiers = collections.Counter(res[e][2] for e in r)
            top = cc.most_common(1)[0] if cc else ('', 0)
            j = bisect.bisect_left(w.starts, r[0])
            below = w.starts[j - 1] if j else None
            k = bisect.bisect_right(w.starts, r[-1])
            above = w.starts[k] if k < len(w.starts) else None
            nb = lambda x: '%s(%s)' % (w.sname(x), known[x][0] if x in known else '?') if x else '-'
            wr.writerow(['R%04d' % (i + 1), '0x%06X' % r[0], '0x%06X' % w.ext[r[-1]]['end'], len(r),
                         sum(w.ext[e]['end'] - e for e in r), top[0], '%.2f' % (top[1] / len(r)),
                         tiers['evidence'], tiers['hypothesis'], tiers['unnamed'],
                         sum(1 for e in r if w.suspects(e)), nb(below), nb(above)])
    # the boot-resident set by the table that holds each: every .rdata / .data
    # table (table_bounds) holding one, its slots, the bare-ret slots (the
    # engine's "empty" handler), slots repeated inside it, and slots whose
    # target another table also holds (shared)
    boot = {e for e in targets if targets[e] == '2'}
    bare = {e for e in w.starts if (mr.decode(w.img, e) or type('x', (), {'mnemonic': ''})).mnemonic == 'ret'}
    tabs = {}
    member_tabs = collections.defaultdict(list)
    for e in sorted(boot):
        for ts, cell in w.tables.get(e, ()):
            base, end = w.table_bounds(ts, cell)
            tabs[base] = end
            member_tabs[e].append((base, (cell - base) // 4))
    holders = collections.defaultdict(set)        # target -> table bases holding it
    tvals = {}
    for base, end in tabs.items():
        tvals[base] = [w.img.u32(p) for p in range(base, end, 4)]
        for v in tvals[base]:
            holders[v].add(base)
    for t, lst in w.tables.items():            # tables no boot function is in, for the shared count
        for ts, cell in lst:
            b, _ = w.table_bounds(ts, cell)
            holders[t].add(b)
    with open(os.path.join(a.out, 'boot_tables.tsv'), 'w', encoding='utf-8', newline='') as fh:
        wr = csv.writer(fh, delimiter='\t')
        wr.writerow(['table', 'name', 'slots', 'distinct', 'bare_ret_slots', 'repeated_slots', 'shared_targets',
                     'boot_members', 'ours', 'other_not_ours', 'readers'])
        for base in sorted(tabs):
            vals = tvals[base]
            cnt = collections.Counter(vals)
            _, readers = w.table_readers(base, base)
            wr.writerow(['0x%06X' % base, w.table_name(base, base) or '', len(vals), len(cnt),
                         sum(cnt[b] for b in bare if b in cnt), sum(c - 1 for c in cnt.values() if c > 1),
                         sum(1 for v in cnt if len(holders[v]) > 1), sum(1 for v in cnt if v in boot),
                         sum(1 for v in cnt if w.known.get(v, ('', '', ''))[2] == 'ours'),
                         sum(1 for v in cnt if v in w.cat and v not in boot),
                         ' '.join(w.sname(r) for r in sorted(readers)[:3])])
    with open(os.path.join(a.out, 'boot_members.tsv'), 'w', encoding='utf-8', newline='') as fh:
        wr = csv.writer(fh, delimiter='\t')
        wr.writerow(['entry', 'catalog_label', 'how', 'tables', 'callers', 'proposed', 'tier', 'suspect'])
        for e in sorted(boot):
            c, det, tier, ev, p = res[e]
            wr.writerow(['0x%06X' % e, w.cat[e]['label'], w.cat[e]['how'],
                         ' '.join('0x%06X[%d]' % bt for bt in member_tabs[e]),
                         len({o for o, k in w.rev.get(e, ())}), c or '', tier, ';'.join(w.suspects(e))])
    tc = collections.Counter((r[3], r[7]) for r in rows)
    print('rows:', len(rows), dict(tc))
    print('runs:', len(runs))


if __name__ == '__main__':
    main()
