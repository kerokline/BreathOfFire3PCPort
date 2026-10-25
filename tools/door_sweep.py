#!/usr/bin/env python
"""The every-door sweep: where each link of each area leads, and what music
plays on arrival, on two or more installs, compared (docs/door-sweep.md).

    python tools/door_sweep.py run --set us-disc --root C:/Users/kerok/bof3x-runs/everydoor [--areas 0-199]
    python tools/door_sweep.py compare --set us-disc --root ...      # again, from the logs on disk
    python tools/door_sweep.py all --set us-disc --root ...          # abc_run prepare, run, compare

What it drives - the DIRECT form, not a walk. Every area's exits are the
descriptor's link list (Area_LinkAt, docs/area-entry.md 1). The game is
started on a crafted save that stands the party in the first area of the plan
(chapter_jump.py's method: a donor save, its scenario bytes set to state 2,
run 0, step 0 - the field, no run - its area word, entry flags and 16.16 cells
set, the +0x70 checksum recomputed). A recipe then takes, for each link:

    goto AREA X Z FLAGS   Field_ChangeArea through the original's entry - the
                          funnel the save's own load goes through - to stand
                          the party in the link's area (the place an incoming
                          link of the reference install puts it)
    door CX CZ            Area_LinkAt(CX, CZ) and Field_Request 5: the two
                          calls Field_LeaderStepLands makes when the leader
                          steps on a link cell (0x52E580)

and peeks, once the field has settled, the area the party arrived in, the
mode, the track playing (Music_Track 0x904131) and the one Area_PickMusic
chose (0x904CD0), and the leader's cells. Both steps are recipe steps
(src/hook/input_script.cpp), harness-only; nothing changes when no recipe
names them. The cell for link i is the first one of its run, searched as
Area_LinkAt searches (from the far end), that no later link also holds - so
the link taken is the one aimed at. A link no cell of which is its own
(shadowed) is listed, not driven.

Not seen this way: whether the cells are reachable on foot, the map's cell
codes (0xA7 / 0xC0) that decide whether stepping there asks for a link at all,
and scripted exits that are not links (event scripts' own Field_ChangeArea).

Per install, under ROOT/door/<set>/<install>/: the recipe chunks, each run's
bof3x.log, links.json (the image's link lists, from the `links` step) and
steps.json (one row a driven link). A chunk that fails (an until timed out, a
crash) is resumed from the next link with a save crafted for that link's area.
Compare: a link whose arrival area or music differs between the reference
(the set's first install) and another is listed, first the ones no entry of
recipes/door-known.toml explains; then the link lists themselves, field by
field (a difference there that no driven link shows is listed too).

One game at a time (input_run.py by way of abc_run.run_one); BOF3X_OPT=none
and BOF3X_LAYERING=0 as every harness run.
"""
import argparse, glob, json, os, re, struct, sys, time, tomllib
from types import SimpleNamespace

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abc_run  # noqa: E402
import chapter_jump  # noqa: E402

KNOWN = os.path.join(ROOT, 'recipes', 'door-known.toml')
DONOR = 'deisCave'        # chapter 6, before Deis's temple (issue 139's route); any recipe save will do
FALLBACK = (0x120000, 0x180000, 1)   # an area no link leads into: the crafts' "the field" cells (chapter_jump cs153)

# The addresses the recipe peeks (symbols.toml).
GAME_AREA = 0x904EFC      # Game_AreaNumber, u16
GAME_MODE = 0x66C7E8      # Game_Mode, u16
FIELD_REQUEST = 0x66C7D8  # Field_Request, u8: 5 asks for the area change, 0 once GameMode_Enter is done
MUSIC_TRACK = 0x904131    # Music_Track, u8: the track playing, 0xFF none
MUSIC_PICKED = 0x904CD0   # AreaW1f_MusicCurrent, u8: the track Area_PickMusic chose for the area
LEADER_X = 0x802D74       # ObjTrio[0] +0x34, s32 16.16
LEADER_Z = 0x802D78       # ObjTrio[0] +0x38

# The load: ending17.txt's / the cs_*.txt crafts' title-to-slot-0 presses (the PC's
# buttons; a Western install presses its release's own, input-script.md 2a).
PREAMBLE = """wait 53
hold start 4
wait 67
hold start 6
wait 47
hold circle 4
wait 54
wait 5
wait 4
wait 4
wait 5
wait 6
wait 5
wait 5
wait 27
hold circle 5
wait 85
hold circle 2
until 0x66C7E8 u16 == 2 timeout 1800
until 0x66C7D8 u8 == 0 timeout 1800
wait 30
"""

LINE = re.compile(r'^input\s+(.*)$')


# --- the links dump --------------------------------------------------------------

LINKS_AREA = re.compile(r'links area (\d+): (\d+) links$')
LINKS_NONE = re.compile(r'links area (\d+): (no descriptor|no list|list .* not readable)')
LINK_DIRECT = re.compile(r'link (\d+)\.(\d+) cells (\d+),(\d+) ([xz])(\d+) -> area (\d+) x 0x([0-9A-F]+) z 0x([0-9A-F]+) flags 0x([0-9A-F]+)$')
LINK_LIST = re.compile(r'link (\d+)\.(\d+) cells (\d+),(\d+) ([xz])(\d+) -> list (\d+)(.*)$')
ALT = re.compile(r'alt (\d+)\.(\d+)\.(\d+) area (\d+) x 0x([0-9A-F]+) z 0x([0-9A-F]+) row (\d+) bit (\d+) flags 0x([0-9A-F]+)')
ALT_BAD = re.compile(r'alt (\d+)\.(\d+): list .* not readable')


def input_lines(log):
    with open(log, encoding='utf-8', errors='replace') as fh:
        for raw in fh:
            m = LINE.match(raw.rstrip('\n'))
            if m:
                yield m[1]


def parse_links(log):
    """{area: [link, ...]} from a log's `links` dump; None when it has none."""
    areas, seen = {}, False
    for t in input_lines(log):
        if t.startswith('links: '):
            seen = True
            break
        m = LINKS_AREA.match(t)
        if m:
            areas[int(m[1])] = []
            continue
        m = LINKS_NONE.match(t)
        if m:
            areas[int(m[1])] = None if m[2] != 'no list' else []
            continue
        m = LINK_DIRECT.match(t)
        if m:
            a, i = int(m[1]), int(m[2])
            areas[a].append({'i': i, 'x': int(m[3]), 'z': int(m[4]), 'along': m[5], 'cells': int(m[6]),
                             'to': int(m[7]), 'tx': int(m[8], 16), 'tz': int(m[9], 16), 'flags': int(m[10], 16)})
            continue
        m = LINK_LIST.match(t)
        if m:
            a, i = int(m[1]), int(m[2])
            areas[a].append({'i': i, 'x': int(m[3]), 'z': int(m[4]), 'along': m[5], 'cells': int(m[6]),
                             'list': int(m[7]), 'alts': [] if 'not readable' not in m[8] else None})
            continue
        m = ALT.match(t)
        if m:
            a, i = int(m[1]), int(m[2])
            areas[a][i]['alts'].append({'to': int(m[4]), 'tx': int(m[5], 16), 'tz': int(m[6], 16), 'row': int(m[7]),
                                        'bit': int(m[8]), 'flags': int(m[9], 16)})
            continue
        m = ALT_BAD.match(t)
        if m:
            areas[int(m[1])][int(m[2])]['alts'] = None
    return areas if seen else None


def link_cells(l):
    """The cells of a link's run in Area_LinkAt's order (the far end first); a
    cell past 0xFF never matches there (the run's start plus its index is an int)."""
    out = []
    for j in range(l['cells'] - 1, -1, -1):
        x, z = (l['x'], l['z'] + j) if l['along'] == 'z' else (l['x'] + j, l['z'])
        if x <= 0xFF and z <= 0xFF:
            out.append((x, z))
    return out


def door_cell(links, i):
    """A cell of link i that no later link holds (Area_LinkAt takes the last
    link holding a cell), or None: the link is shadowed."""
    later = set()
    for l in links[i + 1:]:
        later.update(link_cells(l))
    for c in link_cells(links[i]):
        if c not in later:
            return c
    return None


def entry_for(areas, a):
    """Where `goto` stands the party in area a: the first direct link into it
    (by area and index) of the reference install, its cells as Area_LinkAt
    makes them; else FALLBACK."""
    for src in sorted(areas):
        for l in areas[src] or []:
            if l.get('to') == a:
                return ((l['tx'] & 0xFF80) << 8, (l['tz'] & 0xFF80) << 8, l['flags'] & 0x8F, f'{src}.{l["i"]}')
    for src in sorted(areas):        # else an indirect link's alternative
        for l in areas[src] or []:
            for k, x in enumerate(l.get('alts') or []):
                if x['to'] == a:
                    return ((x['tx'] & 0xFF80) << 8, (x['tz'] & 0xFF80) << 8, x['flags'] & 0x8F, f'{src}.{l["i"]}.{k}')
    return FALLBACK + ('fallback',)


def area_list(spec):
    out = []
    for part in spec.split(','):
        lo, _, hi = part.partition('-')
        out += range(int(lo, 0), int(hi or lo, 0) + 1)
    return out


def make_plan(areas, wanted):
    plan, shadowed = [], []
    for a in wanted:
        links = areas.get(a)
        if not links:
            continue
        ex, ez, ef, via = entry_for(areas, a)
        for l in links:
            c = door_cell(links, l['i'])
            if c is None:
                shadowed.append(f'{a}.{l["i"]}')
                continue
            plan.append({'key': f'{a}.{l["i"]}', 'area': a, 'link': l['i'], 'cx': c[0], 'cz': c[1],
                         'entry': [ex, ez, ef], 'via': via})
    return plan, shadowed


# --- the save and the recipe -----------------------------------------------------

def craft_save(donor_dir, donor, out_dir, area, x, z, flags):
    """chapter_jump's craft: the donor's chapter kept, state 2, run 0, step 0
    (the field, no run), the area word, entry flags and cells set, the
    checksum recomputed. Game data: written under the harness root only."""
    src = os.path.join(donor_dir, donor + '.DAT')
    b = bytearray(open(src, 'rb').read())
    if len(b) < chapter_jump.BLOCK:
        sys.exit(f'{src}: {len(b)} bytes, not a game block')
    if struct.unpack_from('<H', b, chapter_jump.SUM_AT)[0] != chapter_jump.checksum(b):
        sys.exit(f'{src}: its checksum does not follow the rule; refusing to craft from it')
    chapter = b[0]
    if area is None:                 # the donor's own place
        area, flags = struct.unpack_from('<H', b, 0x24)[0], b[0x27]
        x, z = struct.unpack_from('<ii', b, 0x28)
    b[0:16] = bytes(16)
    b[0], b[2], b[4], b[5] = chapter, 2, 0, 0
    struct.pack_into('<H', b, 0x24, area)
    b[0x27] = flags
    struct.pack_into('<ii', b, 0x28, x, z)
    struct.pack_into('<H', b, chapter_jump.SUM_AT, chapter_jump.checksum(b))
    os.makedirs(out_dir, exist_ok=True)
    name = f'door_{donor}_{area}'
    with open(os.path.join(out_dir, name + '.DAT'), 'wb') as fh:
        fh.write(b)
    return name, chapter


def write_recipe(path, steps, save, first, settle, settle_src, dump):
    with open(path, 'w', newline='\n') as fh:
        fh.write('# Written by tools/door_sweep.py (docs/door-sweep.md): a crafted save in the first\n'
                 '# link\'s area, then goto / door / peek per link. Harness steps; not a player\'s route.\n')
        fh.write(f'# save {save}\n')
        fh.write(PREAMBLE)
        if dump:
            fh.write('links\n')
        here = first
        for s in steps:
            fh.write(f'mark door {s["key"]}\n')
            if here != s['area']:
                ex, ez, ef = s['entry']
                fh.write(f'goto {s["area"]} {ex:#x} {ez:#x} {ef:#x}\n')
                fh.write(f'until {FIELD_REQUEST:#x} u8 == 0 timeout 1800\n')
                fh.write(f'wait {settle_src}\n')
            fh.write(f'peek {GAME_AREA:#x} u16 src_area\n')
            fh.write(f'door {s["cx"]} {s["cz"]}\n')
            fh.write(f'until {FIELD_REQUEST:#x} u8 == 0 timeout 1800\n')
            fh.write(f'wait {settle}\n')
            fh.write(f'peek {GAME_AREA:#x} u16 arr_area\n')
            fh.write(f'peek {GAME_MODE:#x} u16 arr_mode\n')
            fh.write(f'peek {MUSIC_TRACK:#x} u8 arr_music\n')
            fh.write(f'peek {MUSIC_PICKED:#x} u8 arr_picked\n')
            fh.write(f'peek {LEADER_X:#x} u32 arr_x\n')
            fh.write(f'peek {LEADER_Z:#x} u32 arr_z\n')
            fh.write(f'mark done {s["key"]}\n')
            here = None          # after a door: always go back
        fh.write('end\n')


# --- one install -------------------------------------------------------------------

MARK = re.compile(r'mark (door|done) (\d+\.\d+) recipe frame')
PEEK = re.compile(r'peek (\w+) = 0x([0-9A-F]+) ')
DOOR = re.compile(r'door (\d+),(\d+) in area (\d+): (?:to area (\d+) x 0x([0-9A-F]+) z 0x([0-9A-F]+) flags 0x([0-9A-F]+) track (\d+)|no link)')
ENDED = re.compile(r'(done|FAILED) at recipe frame')


def parse_steps(log):
    """{key: row} for the doors a log reached; and the key it stopped inside, if any."""
    rows, cur, row = {}, None, None
    for t in input_lines(log):
        m = MARK.match(t)
        if m:
            if m[1] == 'door':
                cur, row = m[2], {'key': m[2]}
            elif cur == m[2]:
                row['complete'] = True
                rows[cur] = row
                cur, row = None, None
            continue
        if row is None:
            continue
        m = PEEK.match(t)
        if m:
            row[m[1]] = int(m[2], 16)
            continue
        m = DOOR.match(t)
        if m:
            if m[4] is None:
                row['door'] = None
            else:
                row['door'] = {'to': int(m[4]), 'x': int(m[5], 16), 'z': int(m[6], 16), 'flags': int(m[7], 16),
                               'track': int(m[8])}
    return rows, cur


def run_install(cfg, i, plan, a, odir):
    os.makedirs(odir, exist_ok=True)
    done_path = os.path.join(odir, 'steps.json')
    results = json.load(open(done_path)) if os.path.exists(done_path) and not a.fresh else {}
    links = None
    lp = os.path.join(odir, 'links.json')
    if os.path.exists(lp) and not a.fresh:
        links = {int(k): v for k, v in json.load(open(lp)).items()}
    chunk = 0
    while True:
        todo = [s for s in plan if s['key'] not in results]
        if not todo and links is not None:
            break
        steps = todo[:a.chunk] if todo else []
        if steps:
            first = steps[0]
            ex, ez, ef = first['entry']
            save, chapter = craft_save(a.donors, a.donor, cfg.recipe_saves, first['area'], ex, ez, ef)
        else:                        # the links alone: the donor where it stands
            first = {'key': '-', 'area': None}
            save, chapter = craft_save(a.donors, a.donor, cfg.recipe_saves, None, 0, 0, 0)
        while os.path.exists(os.path.join(odir, f'chunk{chunk:03}')):
            chunk += 1
        cdir = os.path.join(odir, f'chunk{chunk:03}')
        os.makedirs(cdir)
        recipe = os.path.join(cdir, 'doors.txt')
        write_recipe(recipe, steps, save, first['area'], a.settle, a.settle_src, links is None)
        print(f'{i["name"]}: chunk {chunk}, {len(steps)} doors from {first["key"]} (save {save}, chapter {chapter})')
        ra = SimpleNamespace(every=1000000, statehash=False, statehash_skip=None, speed=a.speed, wide='0',
                             minutes=a.minutes, save_args=['--save', save], route=recipe)
        meta = abc_run.run_one(cfg, i, recipe, os.path.join(cdir, 'run'), ra)
        log = os.path.join(cdir, 'run', 'bof3x.log')
        if not os.path.exists(log):
            sys.exit(f'{i["name"]}: no log from chunk {chunk}')
        if links is None:
            links = parse_links(log)
            if links is None:
                sys.exit(f'{i["name"]}: chunk {chunk} ended before its `links` step ({meta["status"]}); see {log}')
            json.dump({str(k): v for k, v in links.items()}, open(lp, 'w'), indent=0)
        rows, stuck = parse_steps(log)
        for k, r in rows.items():
            r['chunk'] = chunk
            results[k] = r
        if stuck is not None and stuck not in results:
            text = open(os.path.join(cdir, 'run', '_run.txt'), encoding='utf-8', errors='replace').read()
            why = 'FATAL' if 'FATAL' in text else ('timed out' if 'timed out' in open(log, encoding='utf-8',
                                                                                      errors='replace').read() else meta['status'])
            results[stuck] = {'key': stuck, 'failed': why, 'chunk': chunk}
            print(f'  {stuck}: did not finish ({why}); resumed after it')
        elif steps and not rows:
            # nothing reached: the load itself failed
            sys.exit(f'{i["name"]}: chunk {chunk} reached no door ({meta["status"]}); see {log}')
        json.dump(results, open(done_path, 'w'), indent=0)
    return links, results


# --- compare -----------------------------------------------------------------------

def load_known(path):
    if not os.path.exists(path):
        return []
    with open(path, 'rb') as fh:
        return tomllib.load(fh).get('known', [])


def known_for(known, key, field, ref, other):
    a, _, i = key.partition('.')
    for k in known:
        if k.get('installs') and other['name'] not in k['installs']:
            continue
        if k.get('pair_trait') and not ((k['pair_trait'] in ref['traits']) != (k['pair_trait'] in other['traits'])):
            continue
        if k.get('areas') and int(a) not in k['areas']:
            continue
        if k.get('links') and key not in k['links']:
            continue
        if k.get('fields') and field not in k['fields']:
            continue
        return k
    return None


def describe(r):
    if r is None:
        return 'not run'
    if 'failed' in r:
        return f'failed ({r["failed"]})'
    d = r.get('door')
    door = 'no link' if d is None else f'to {d["to"]} ({d["x"] >> 16},{d["z"] >> 16}) flags {d["flags"]:#x} picks {d["track"]}'
    return (f'{door}; arrived area {r.get("arr_area")} mode {r.get("arr_mode")} at '
            f'({r.get("arr_x", 0) >> 16},{r.get("arr_z", 0) >> 16}), music {r.get("arr_music")} (picked {r.get("arr_picked")})')


def link_text(l):
    if l is None:
        return 'none'
    run = f'cells {l["x"]},{l["z"]} {l["along"]}{l["cells"]}'
    if 'to' in l:
        return f'{run} -> area {l["to"]} ({l["tx"] >> 8},{l["tz"] >> 8}) flags {l["flags"]:#x}'
    alts = 'unreadable' if l['alts'] is None else ', '.join(
        f'{x["to"]}({x["tx"] >> 8},{x["tz"] >> 8}) if {x["row"]}.{x["bit"]}' for x in l['alts'])
    return f'{run} -> list {l["list"]}: {alts}'


def compare(cfg, installs, a, rdir):
    known = load_known(a.known)
    data = {}
    for i in installs:
        odir = os.path.join(rdir, i['name'])
        lp, sp = os.path.join(odir, 'links.json'), os.path.join(odir, 'steps.json')
        if not (os.path.exists(lp) and os.path.exists(sp)):
            sys.exit(f'{i["name"]}: no results in {odir}: run first')
        data[i['name']] = ({int(k): v for k, v in json.load(open(lp)).items()}, json.load(open(sp)))
    ref = installs[0]
    rlinks, rsteps = data[ref['name']]
    diffs, static = [], []
    for o in installs[1:]:
        olinks, osteps = data[o['name']]
        for key in sorted(set(rsteps) | set(osteps), key=lambda k: tuple(map(int, k.split('.')))):
            r, s = rsteps.get(key), osteps.get(key)
            fields = []
            if r is None or s is None or 'failed' in r or 'failed' in s:
                if (r or {}).get('failed') != (s or {}).get('failed') or r is None or s is None:
                    fields.append('run')
            else:
                if r.get('src_area') != s.get('src_area'):
                    fields.append('source')
                if r.get('door') != s.get('door'):
                    fields.append('link')
                if r.get('arr_area') != s.get('arr_area') or (r.get('arr_x', 0) >> 16, r.get('arr_z', 0) >> 16) != \
                        (s.get('arr_x', 0) >> 16, s.get('arr_z', 0) >> 16):
                    fields.append('area')
                if r.get('arr_music') != s.get('arr_music') or r.get('arr_picked') != s.get('arr_picked'):
                    fields.append('music')
            if not fields:
                continue
            k = None
            for f in fields:
                k = known_for(known, key, f, ref, o)
                if k is None:
                    break
            diffs.append({'pair': f'{ref["name"]}-{o["name"]}', 'key': key, 'fields': fields, 'ref': describe(r),
                          'other': describe(s), 'known': k and k['id'], 'ledger': k and k.get('ledger')})
        for area in sorted(set(rlinks) | set(olinks)):
            rl, ol = rlinks.get(area), olinks.get(area)
            if rl == ol:
                continue
            n = max(len(rl or []), len(ol or []))
            for j in range(n):
                x = (rl or [None] * n)[j] if rl and j < len(rl) else None
                y = (ol or [None] * n)[j] if ol and j < len(ol) else None
                if x != y:
                    static.append({'pair': f'{ref["name"]}-{o["name"]}', 'key': f'{area}.{j}', 'ref': link_text(x),
                                   'other': link_text(y), 'driven': f'{area}.{j}' in rsteps})
    return diffs, static, data


def write_report(path, cfg, installs, a, diffs, static, data, plan, shadowed):
    ref = installs[0]
    lines = [f'# Door sweep: set {a.set}', '',
             f'Written by `tools/door_sweep.py` on {time.strftime("%Y-%m-%d %H:%M")} (docs/door-sweep.md). '
             f'Direct form: `goto` the area, `door` = Area_LinkAt + Field_Request 5, peeks after {a.settle} frames. '
             f'Donor save `{a.donor}`. Reference: `{ref["name"]}`.', '',
             '| Install | links in the image | doors driven | failed |', '|---|---|---|---|']
    for i in installs:
        links, steps = data[i['name']]
        nl = sum(len(v or []) for v in links.values())
        lines.append(f'| `{i["name"]}` | {nl} in {sum(1 for v in links.values() if v)} areas | '
                     f'{sum(1 for r in steps.values() if "failed" not in r)} | '
                     f'{sum(1 for r in steps.values() if "failed" in r)} |')
    lines += ['', f'Plan: {len(plan)} links in {len({p["area"] for p in plan})} areas; '
              f'{len(shadowed)} shadowed (no cell of their own): {", ".join(shadowed) or "none"}.', '']
    un = [d for d in diffs if not d['known']]
    kn = [d for d in diffs if d['known']]
    lines += ['## 1. Doors that differ, no known entry', '']
    if not un:
        lines.append('None.')
    for d in un:
        lines.append(f'- **{d["key"]}** ({d["pair"]}; {", ".join(d["fields"])}): {d["ref"]} | **{d["other"]}**')
    lines += ['', '## 2. Doors that differ, explained by `recipes/door-known.toml`', '']
    if not kn:
        lines.append('None.')
    for d in kn:
        lines.append(f'- {d["key"]} ({d["pair"]}; {", ".join(d["fields"])}) - `{d["known"]}` ({d["ledger"]}): '
                     f'{d["ref"]} | {d["other"]}')
    lines += ['', '## 3. Link lists that differ in the image', '']
    if not static:
        lines.append('None: every area\'s link list and alternatives read the same.')
    for s in static:
        lines.append(f'- {s["key"]} ({s["pair"]}{"" if s["driven"] else ", not driven"}): {s["ref"]} | **{s["other"]}**')
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(lines) + '\n')


# --- main ------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('cmd', choices=('run', 'compare', 'all', 'plan'))
    ap.add_argument('--root', default=os.environ.get('BOF3X_ABC_ROOT', os.path.join(os.path.expanduser('~'), 'abc')))
    ap.add_argument('--installs', default=abc_run.INSTALLS)
    ap.add_argument('--known', default=KNOWN)
    ap.add_argument('--set', default='us-disc')
    ap.add_argument('--only', help='comma-separated install names instead of the set\'s (the first is the reference)')
    ap.add_argument('--tag', help='name the result folder by this instead of the set')
    ap.add_argument('--areas', default='0-199', help='the areas whose links are driven, e.g. 0-40,62,97-98')
    ap.add_argument('--donor', default=DONOR, help=f'the recipe save the crafted saves start from (default {DONOR})')
    ap.add_argument('--donors', default=None, help='where the donor is (default the main checkout\'s tools/recipe_saves)')
    ap.add_argument('--settle', type=int, default=60, help='frames after an arrival before the peeks')
    ap.add_argument('--settle-src', type=int, default=20, help='frames after a goto before the door')
    ap.add_argument('--chunk', type=int, default=400, help='doors per launch at most')
    ap.add_argument('--speed', type=int, default=4)
    ap.add_argument('--minutes', type=float, default=60.0, help='per launch, before input_run gives up')
    ap.add_argument('--fresh', action='store_true', help='forget earlier results in the result folder')
    ap.add_argument('--build', default=None, help='abc_run prepare\'s --build (all only)')
    a = ap.parse_args()
    cfg = abc_run.Config(a.installs, a.root)
    installs = cfg.members(a.set, a.only)
    a.donors = a.donors or cfg.recipe_saves
    cfg.recipe_saves = os.path.join(cfg.root, 'door_saves')   # the crafted saves: game data, under the root
    rdir = os.path.join(cfg.root, 'door', a.tag or a.set)
    os.makedirs(rdir, exist_ok=True)
    if a.cmd == 'all':
        pa = SimpleNamespace(build=a.build or os.path.join(ROOT, 'build'))
        abc_run.prepare(cfg, installs, pa)
    plan_path = os.path.join(rdir, 'plan.json')
    if a.cmd in ('run', 'all', 'plan'):
        ref = installs[0]
        rdir_ref = os.path.join(rdir, ref['name'])
        lp = os.path.join(rdir_ref, 'links.json')
        if not os.path.exists(lp) or a.fresh:
            # the reference's map first: a launch that only dumps the links
            print(f'{ref["name"]}: the link map')
            run_install(cfg, ref, [], a, rdir_ref)
        areas = {int(k): v for k, v in json.load(open(lp)).items()}
        plan, shadowed = make_plan(areas, area_list(a.areas))
        json.dump({'plan': plan, 'shadowed': shadowed, 'areas': a.areas}, open(plan_path, 'w'), indent=0)
        print(f'plan: {len(plan)} doors in {len({p["area"] for p in plan})} areas, {len(shadowed)} shadowed')
        if a.cmd == 'plan':
            return
        for i in installs:
            run_install(cfg, i, plan, a, os.path.join(rdir, i['name']))
    p = json.load(open(plan_path))
    diffs, static, data = compare(cfg, installs, a, rdir)
    rep = os.path.join(cfg.root, f'report_door_{a.tag or a.set}.md')
    write_report(rep, cfg, installs, a, diffs, static, data, p['plan'], p['shadowed'])
    with open(os.path.join(rdir, 'result.json'), 'w') as fh:
        json.dump({'diffs': diffs, 'static': static}, fh, indent=1)
    print(f'{len([d for d in diffs if not d["known"]])} unexplained, {len([d for d in diffs if d["known"]])} known, '
          f'{len(static)} link-list rows differ -> {rep}')


if __name__ == '__main__':
    main()
