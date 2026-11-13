"""The boss sweep with a team of one (docs/solo-character-proof.md 4; round doc
docs/solo-character-round.md 4 "RT"): the boss sweep's pre-fight saves
(docs/boss-sweep.md 6, tools/chapter_jump.py's bf* crafts and the owner's
midEyeEncounter) staged as tools/spell_sweep.py's fight mode stages them
(docs/boss-sweep.md 7.1), run once plain and once per champion with
`BOF3X_ON=SoloCharacter` and `BOF3X_SOLO=<name>`, and a verdict per fight:
started / ended (win, loss or by the set-up's script) / hung / crashed / did
not start. A fight that does not start, hangs or crashes with one member is a
finding, reported in the table, never skipped.

The crafted saves hold no champion mark; the game stamps the launcher's champion
at the first area entry (DIV-0228, `Solo_Stamp` in `Area_Enter`), so the sweep
runs from the saves as they are. The party is watched (both lists, the count,
the formation, the party set, the mark, the eight records' id, level, joined
bits, EXP, HP, AP) beside the fight's own watches.

    python tools/solo_boss_sweep.py prepare --root R --build C   # launcher copy, game folder, saves, the crafts
    python tools/solo_boss_sweep.py list
    python tools/solo_boss_sweep.py run --root R --champions ryu [nina ...] [--fights 28 38 ...] [--no-plain]
    python tools/solo_boss_sweep.py report --root R

The fights (`list`): 28 Gazer, 38 Ammonite, 50 Chimera, 54 Shroom, 55 Myria
(the scene-started fights of boss-sweep.md 6.2 / 7.3, staged as 7.3 did) and
39..48, chapter 15's sample fights (6.3; staged with `--scene` at the entry's own
start - not played by the 2026-10-28 sweep, so their plain row is new too).
Output under <root>/boss: the recipes, each run's frames, .runlog and .log;
<root>/boss_results.tsv and boss_results.md.
"""
import argparse
import os
import subprocess
import sys
import time
import types

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import solo_harness as H  # noqa: E402
import spell_sweep as S  # noqa: E402

PROBE = os.path.join(H.TOOLS, 'recipes', 'bossfight_probe.txt')

# fight: (boss, save, cut, enemies, extra spell_sweep settings) - boss-sweep.md 7.3's rows (fights2.sh,
# fights3.sh) and 6.3's table; the sample fights' donor is adult_ryu, a US conversion that confirms with cross
FIGHTS = {
    28: ('Gazer', 'midEyeEncounter', 437, 3, dict(kill_enemies='0,1,2')),
    38: ('Ammonite', 'bf38c', 800, 4, dict(scene=True, kill_enemies='0,1,2,3')),
    50: ('Chimera', 'bf50c', 569, 2, dict(play=0, confirm='cross')),
    54: ('Shroom', 'bf54b', 437, 3, {}),
    55: ('Myria', 'bf55b', 476, 3, {}),
}
for _f in range(39, 49):
    FIGHTS[_f] = (f'sample {_f}', f'bf{_f}', 570, 3, dict(scene=True, confirm='cross', play=0, kill_enemies='0,1,2'))
# Route-reached fights (boss-sweep.md 7.3's routed rows): the owner's recorded route kept to its cut, then
# `battle ID` as spell_sweep.py stages it; `route` names tools/recipes/<route>.txt and its own `# save`.
# Fight 32: a two-member party in the route's save (the owner, 2026-11-12: "a 2 man team/fight").
FIGHTS[32] = ('D.Zombie', 'minecartToZombieDragon', 5230, 3,
              dict(route='minecartToZombieDragon', after=1200, after_confirm=True))
CRAFTS = sorted({v[1] for v in FIGHTS.values() if v[1].startswith('bf')})
COLUMNS = ('fight', 'boss', 'side', 'verdict', 'called', 'start', 'win', 'lost', 'back', 'joined', 'party_before',
           'party_in_battle', 'count_in_battle', 'party_after_return', 'mark', 'solo_battle', 'solo_restore',
           'champion_before', 'champion_after', 'status', 'notes')


def boss_dir(root):
    d = os.path.join(root, 'boss')
    os.makedirs(d, exist_ok=True)
    return d


def probe_for(root, save):
    """bossfight_probe.txt with its `# save` line naming this save (the probe is generated; its frames
    are unchanged)."""
    with open(PROBE, encoding='utf-8') as f:
        lines = f.read().splitlines()
    if not lines[0].startswith('# save '):
        sys.exit(f'{PROBE}: the first line is not a `# save` header')
    lines[0] = f'# save {save}'
    p = os.path.join(boss_dir(root), f'probe_{save}.txt')
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    return p


def fight_args(root, fight, members):
    boss, save, cut, enemies, extra = FIGHTS[fight]
    extra = dict(extra)
    route = extra.pop('route', None)
    a = types.SimpleNamespace(
        route=os.path.join(H.TOOLS, 'recipes', route + '.txt') if route else probe_for(root, save), cut=cut, goto=(), field_cut=371, settle=150, pre_poke=[], camera=(), place=(),
        fight=fight, scene=False, enemies=enemies, mortal=False, members=members, confirm='circle', ids='',
        sections=set(), play=3, to_end=False, kill=True, kill_enemies='0', kill_rounds=1, every=3, kill_hp=1,
        round_presses=60, return_presses=250, after=300, after_confirm=False, poke=[], peek=[], shots=6,
        spacing=30, enemy_flags=0, enemy_odds=1)
    for k, v in extra.items():
        setattr(a, k, v)
    return a


def write_recipe(root, fight, members, tag, wait_cross=False, mark=None):
    a = fight_args(root, fight, members)
    if mark is not None:
        # --mark-at-cut: the route replays with the mode on and no champion (the mark 0: inactive), and the
        # champion's mark (record + 1) is poked at the cut, before the fight is called, so the boss is the
        # first battle fought alone (the owner's way past a route's earlier fight, 2026-11-12)
        a.pre_poke = list(a.pre_poke) + [f'0x{H.CHAMPION_MARK:X} u8 {mark}']
    head = S.fight_head(a)
    i = next(n for n, l in enumerate(head) if l.startswith('mark fight'))
    head = head[:i] + H.party_watches() + head[i:]
    watches = sum(1 for l in head if l.startswith('watch '))
    if watches > 64:
        sys.exit(f'fight {fight}: {watches} watches, the recipe op allows 64')
    body = S.fight_body(a, None)
    if wait_cross:
        # A team of one: the cross is one member's, so a confirm pressed by `seek ... == 1` as the cross opens
        # enters the command and phase 1 is over two frames later, between the seek's checks; the seek then
        # gives up while the battle runs on (fight 32 with ryu, 2026-11-12). Wait for the cross instead.
        pre = f'seek {a.confirm} 0x{S.PHASE:X} u8 == 1 max '
        body = [f'until 0x{S.PHASE:X} u8 == 1 timeout 1800' if l.startswith(pre) else l for l in body]
    p = os.path.join(boss_dir(root), f'{tag}.txt')
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join([f'# solo_boss_sweep.py: fight {fight} staged as tools/spell_sweep.py --fight stages it, '
                           'the party watched - a harness recipe (pokes), not a route'] + head + body) + '\n')
    return p


def summarise(fight, side, status, lg, champion):
    boss = FIGHTS[fight][0]
    row = {'fight': fight, 'boss': boss, 'side': side, 'status': status}
    if not lg:
        row['verdict'] = 'no log (the launch failed)'
        return row
    s = S.fight_summary(lg, fight)
    f = H.log_facts(lg)
    w = H.read_watches(lg)
    row.update({k: s[k] for k in ('called', 'start', 'win', 'lost', 'back')})
    # before: the frame the fight was called (or mode 5's first, for a --scene cut); in battle: 60 frames
    # into mode 5, after the set-up's step 3 has built the members (CB writes the party before it)
    t_before = s['called'] if s['called'] is not None else s['start']
    t_in = s['start'] + 60 if s['start'] is not None else None
    p0 = H.at_or_before(w, 'sl_party', t_before)
    c0 = H.at_or_before(w, 'sl_count', t_before)
    row['party_before'] = (H.party_text(p0) + f' (count {int(c0, 16)})') if p0 and c0 else ''
    joined = []
    for r in range(8):
        v = H.at_or_before(w, f'sl_rec{r}', t_before)
        if v and bytes.fromhex(v)[H.REC_FLAGS - H.REC_WATCH_FROM] & 1:
            joined.append(str(r))
    row['joined'] = ' '.join(joined)
    p = H.at_or_before(w, 'sl_party', t_in)
    c = H.at_or_before(w, 'sl_count', t_in)
    m = H.at_or_before(w, 'sl_mark', None)
    row['party_in_battle'] = H.party_text(p) if p else ''
    row['count_in_battle'] = int(c, 16) if c else ''
    row['mark'] = int(m, 16) if m else ''
    if champion:
        name = f'sl_rec{H.CHAMPIONS[champion]}'
        r0, r1 = H.at_or_before(w, name, s['start']), H.at_or_before(w, name, None)
        row['champion_before'] = H.record_text(r0) if r0 else ''
        row['champion_after'] = H.record_text(r1) if r1 else ''
    notes = []
    if f['fatal'] or f['crash'] or status in ('FATAL', 'game exited'):
        verdict = 'crashed'
        notes.append(f['fatal'] or f['crash'] or status)
    elif s['start'] is None:
        verdict = 'did not start'
        if s['facts']['fail']:
            notes.append(s['facts']['fail'])
    elif s['back'] is not None:
        how = 'win' if s['win'] is not None else 'loss' if s['lost'] is not None else 'by the set-up\'s script'
        verdict = f'started, ended ({how})'
    else:
        verdict = 'started, hung (no return to the field)'
        notes.append(s['facts']['fail'] or status)
    fought = [x for x in f['solo'] if 'fought by' in x or 'already has the champion' in x or 'has 0 HP' in x]
    back = [x for x in f['solo'] if 'put back' in x]
    row['solo_battle'] = ' / '.join(x.split('SoloCharacter: ', 1)[-1] for x in fought)[:300]
    row['solo_restore'] = ' / '.join(x.split('SoloCharacter: ', 1)[-1] for x in back)[:300]
    pb = H.at_or_before(w, 'sl_party', s['back']) if s['back'] is not None else None
    cb = H.at_or_before(w, 'sl_count', s['back']) if s['back'] is not None else None
    sb = H.at_or_before(w, 'sl_set', s['back']) if s['back'] is not None else None
    last = H.at_or_before(w, 'sl_party', None)
    row['party_after_return'] = (H.party_text(last) if last else '') + (f' (count {int(cb, 16)}, set {sb})' if cb else '')
    row['party_at_return'] = H.party_text(pb) if pb else ''
    row['verdict'] = verdict
    row['notes'] = '; '.join(n for n in notes if n)[:300]
    return row


def results(root):
    rows = {}
    p = os.path.join(root, 'boss_results.tsv')
    if os.path.exists(p):
        with open(p, encoding='utf-8') as f:
            lines = [l.rstrip('\n').split('\t') for l in f if not l.startswith('#')]
        if lines:
            for l in lines[1:]:
                r = dict(zip(lines[0], l))
                rows[(r['fight'], r['side'])] = r
    return rows


def save(root, rows):
    order = sorted(rows.values(), key=lambda r: (int(r['fight']), r['side'] != 'plain', r['side']))
    H.write_tsv(os.path.join(root, 'boss_results.tsv'), COLUMNS, order,
                'tools/solo_boss_sweep.py: the latest run of each fight and side (docs/solo-character-proof.md 4)')
    with open(os.path.join(root, 'boss_results.md'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(H.md_table(('fight', 'boss', 'side', 'verdict', 'start', 'back', 'joined', 'party_before', 'party_in_battle',
                            'count_in_battle', 'party_after_return', 'mark', 'solo_battle', 'solo_restore',
                            'champion_before', 'champion_after', 'notes'), order) + '\n')


def cmd_prepare(a):
    os.makedirs(a.root, exist_ok=True)
    H.prepare_launcher(a.root, 'cand', a.build)
    H.prepare_game(a.root, a.game_src)
    sv, n = H.prepare_saves(a.root, a.saves_src)
    missing = [c for c in CRAFTS if not os.path.exists(os.path.join(sv, c + '.DAT'))]
    if missing:
        r = subprocess.run([sys.executable, os.path.join(H.TOOLS, 'chapter_jump.py'), 'make', '--donors', sv,
                            '--out', sv] + missing, cwd=H.ROOT, capture_output=True, text=True)
        with open(os.path.join(a.root, 'crafts.log'), 'w', encoding='utf-8') as f:
            f.write(r.stdout + r.stderr)
        if r.returncode:
            sys.exit(f'chapter_jump.py make failed ({a.root}\\crafts.log)')
    still = [c for c in CRAFTS if not os.path.exists(os.path.join(sv, c + '.DAT'))]
    print(f'prepared {a.root}: {n} saves, crafts made {len(missing) - len(still)}, missing {still or "none"}')


def cmd_list(a):
    for fight, (boss, save_, cut, enemies, extra) in sorted(FIGHTS.items()):
        print(f'{fight:3d}  {boss:12s} save {save_:16s} cut {cut:4d} enemies {enemies} {extra}')
    return 0


def run_one(a, fight, side, env, champion):
    tag = f'f{fight:02d}_{side}'
    mark = H.CHAMPIONS[champion] + 1 if champion and side.startswith('mark_') else None
    recipe = write_recipe(a.root, fight, a.members, tag, wait_cross=bool(champion) and not a.seek_cross, mark=mark)
    full = {'BOF3X_OPT': 'none', 'BOF3X_LAYERING': '0', 'BOF3X_RECIPE_SAVES': H.winpath(os.path.join(a.root, 'saves'))}
    if a.speed != 1:
        full['BOF3X_SPEED'] = str(a.speed)
    full.update(env)
    d = boss_dir(a.root)
    rc, status, lg, secs = H.run_recipe(a.root, recipe, os.path.join(d, tag), os.path.join(a.root, 'Lcand'), full,
                                        os.path.join(d, f'{tag}.runlog'), minutes=a.minutes,
                                        extra=['--lang', a.lang])
    S.to_png(os.path.join(d, tag))
    row = summarise(fight, side, status, lg, champion)
    print(f'[{time.strftime("%H:%M:%S")}] fight {fight} {side}: {row["verdict"]} ({secs:.0f} s; '
          f'party {row.get("party_in_battle")}, count {row.get("count_in_battle")}, mark {row.get("mark")})', flush=True)
    return row


def cmd_run(a):
    rows = results(a.root)
    for fight in a.fights:
        if fight not in FIGHTS:
            sys.exit(f'fight {fight}: not in the table (`list`)')
        if a.mark_at_cut:
            solo = [('mark_' + c, {'BOF3X_ON': 'SoloCharacter'}, c) for c in a.champions]
        else:
            solo = [('solo_' + c, {'BOF3X_ON': 'SoloCharacter', 'BOF3X_SOLO': c}, c) for c in a.champions]
        sides = ([] if a.no_plain else [('plain', {}, None)]) + solo
        for side, env, champ in sides:
            if H.stopped(a.root):
                print('STOP seen')
                save(a.root, rows)
                return 1
            rows[(str(fight), side)] = run_one(a, fight, side, env, champ)
            save(a.root, rows)
    print(open(os.path.join(a.root, 'boss_results.md'), encoding='utf-8').read())
    return 0


def cmd_report(a):
    print(open(os.path.join(a.root, 'boss_results.md'), encoding='utf-8').read())
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('prepare')
    p.add_argument('--root', required=True)
    p.add_argument('--build', required=True, help='the build directory to run (the round\'s merged tip)')
    H.add_prepare_args(p)
    p.set_defaults(fn=cmd_prepare)
    p = sub.add_parser('list')
    p.set_defaults(fn=cmd_list, root='.')
    p = sub.add_parser('run')
    p.add_argument('--root', required=True)
    p.add_argument('--fights', nargs='+', type=int, default=sorted(FIGHTS))
    p.add_argument('--champions', nargs='+', default=['ryu'], choices=sorted(H.CHAMPIONS))
    p.add_argument('--no-plain', action='store_true', help='skip the plain (mode off) run of each fight')
    p.add_argument('--mark-at-cut', action='store_true',
                   help='SoloCharacter on with no BOF3X_SOLO through the route, the mark of the champion poked at the '
                        'cut (sides mark_<champion>): a route-reached fight reached in step past earlier battles')
    p.add_argument('--seek-cross', action='store_true',
                   help='solo sides: keep the seek of spell_sweep for the cross (default: `until`, see write_recipe)')
    p.add_argument('--members', type=int, default=3,
                   help='working records the party pokes cover (HP / DEF 999; spell_sweep\'s default 3)')
    p.add_argument('--speed', type=int, default=4)
    p.add_argument('--lang', default='en-US')
    p.add_argument('--minutes', type=float, default=15)
    p.set_defaults(fn=cmd_run)
    p = sub.add_parser('report')
    p.add_argument('--root', required=True)
    p.set_defaults(fn=cmd_report)
    a = ap.parse_args()
    a.root = os.path.abspath(a.root)
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
