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
# The rest of boss-sweep.md 7.3's routed rows (D:\bof3x-runs' bsw1027 fights.sh) and section 8's fight 35, each
# staged as its row says; run them with --mark-at-cut (the routes fight earlier battles on the way)
FIGHTS[2] = ('Nue', 'nue', 4233, 1, dict(route='cutsceneAndNue'))
FIGHTS[7] = ('Worker, Operator', 'whelp_boss', 711, 2,
             dict(route='whelpBoss', play=0, kill=False, to_end=True, return_presses=400))
FIGHTS[8] = ('kind 8', 'bossAndFlash', 1747, 4, dict(route='bossAndFlash', kill_enemies='0,1,2,3'))
FIGHTS[12] = ('Amalgam', 'ropeAndAmalgam', 300, 1,
              dict(route='ropeAndAmalgam', field_cut=300, goto=(28, 0x270000, 0x568000, 3),
                   camera=(0x190000, 0x5B0000), place=(0x1C0000, 0x5B0000), pre_poke=['0x7DF3A0 u8 1']))
FIGHTS[13] = ('Balio, Sunder', 'balioAndSunder_1', 2259, 2,
              dict(route='balioAndSunder_1', kill=False, mortal=True, to_end=True, return_presses=500))
FIGHTS[16] = ('Balio, Sunder, Nina', 'balioAndSunder_2', 17194, 3,
              dict(route='balioAndSunder_2', kill_enemies='0,1,2', kill_rounds=3, minutes=25))
FIGHTS[23] = ('Bully 1..3', 'stallion', 1939, 3, dict(route='stallion', kill_enemies='0,1,2'))
FIGHTS[24] = ('Stallion', 'stallion', 10574, 3, dict(route='stallion', minutes=25))
FIGHTS[29] = ('Dolphin', 'dolphinBoss', 9955, 1, dict(route='dolphinBoss', minutes=20))
FIGHTS[35] = ('Gaist, Torch', 'geist', 1468, 3, dict(route='geist', scene=True, after=1200, kill_rounds=3,
                                                   return_presses=600))
# Crafts of this sweep's own (tools/chapter_jump.py's form, put into its table at `prepare`; chapter_jump.py is
# not edited): the two one-member cases the round doc names that no save reached (docs/solo-character-proof.md 6b)
SOLO_CRAFTS = {
    # chapter 6 run 8 (Scena06_Run08, scena_06_3.cpp): step 0 SortParty (the second list to one member, the count 1),
    # step 1 PartyPlace(0x190000, 0x410000, 0x19), step 2 Battle(0x19) - fight 25, set-up 25 (Boss25_Exit puts the
    # list back). The area is boss-fights.md's inferred 92, at Run09Enter's arrival (0x180000, 0x3D0000, 0x89).
    'bfs25': dict(donor='preBeyd', chapter=6, state=2, run=8, step=0, area=92, flags=0x89, x=0x180000, z=0x3D0000,
                  why='fight 25: Scena06_Run08 from step 0 (SortParty, PartyPlace, Battle(0x19))'),
    # chapter 8 run 0xB (Scena08_Scene11, scena_08_2.cpp): step 7 Scena08_SetUpRecord4, the record of id 4 at HP 0,
    # status | 0x60, must-stay on it and on record 7; step 9 DropIn(2), PlaceParty(0x150000, 0x190000, 0x22);
    # step 0xA waits counter 0 = 0x1A, then Field_StartEventBattle(0x22) - fight 34 (boss-fights.md: Mikba, area 43)
    'bfs34': dict(donor='minecartToZombieDragon', chapter=8, state=2, run=0xB, step=7, area=43, flags=1,
                  x=0x150000, z=0x190000, cset=[0x14], cclr=[0x15], counters=[0x1A, 0, 0, 0],
                  why='fight 34: Scena08_Scene11 from step 7 (record 4 to HP 0) with counter 0 at 0x1A'),
    # chapter 5 run 0x12 (scena_05_2.cpp, 0x549A80): step 6 ChangeArea(0x50, 0xC60000, 0xF8000, 0x82); steps 8, 9
    # wait counter 0 = 1, 7; step 0xA Battle(0x16) at counter 0 = 0xB - fight 22, Garr (boss-fights.md, area 80)
    'bfs22': dict(donor='PreEmitai', chapter=5, state=2, run=0x12, step=0xA, area=0x50, flags=0x82,
                  x=0xC60000, z=0xF8000, counters=[0xB, 0, 0, 0],
                  why='fight 22: chapter 5 run 0x12 at its battle step 0xA with counter 0 at 0xB'),
    # chapter 7 run 6 (Scena07_Scene6, scena_07_2.cpp; Scena07_Cell0 starts it at step 0xA): step 0x12 at counter
    # 0 = 0x13 PlaceParty at the leader and Field_StartEventBattle(0x1F) - fight 31, Garr, area 0x55 (85). Flag 8
    # set so the area's entry does not restart run 6 at step 0; flag 9 clear (set at step 0x14, after the fight)
    'bfs31': dict(donor='dolphinBoss', chapter=7, state=2, run=6, step=0x12, area=0x55, flags=0x82,
                  x=0x258000, z=0x410000, cset=[8], cclr=[9], counters=[0x13, 0, 0, 0],
                  why='fight 31: Scena07_Scene6 at its battle step 0x12 with counter 0 at 0x13'),
}
FIGHTS[22] = ('Garr (chapter 5)', 'bfs22', 390, 3, dict(scene=True))
# fight 31 from the owner's recorded route (2026-11-12): the party's dismissal, the fight called at recipe
# frame 4064 (the fight byte), the return at 6764, chapter 8 at 9428; cut three frames before the call. The
# craft bfs31 crashed at the area's load (docs/solo-character-proof.md 6b) and is kept for the record only.
# After the return, confirms for 7,000 frames carry the scene on toward the chapter change.
FIGHTS[31] = ('Garr (Angel Tower)', 'preGarrFight', 4061, 3,
              dict(route='preGarrFight', after=7000, after_confirm=True, minutes=30))
# the same fight with the route's own presses after the return (its return at recipe frame 6764), to the
# recording's end: the scene after the fight walked as the owner walked it (131 is this sweep's label)
FIGHTS[131] = ('Garr (Angel Tower), the route after', 'preGarrFight', 4061, 3,
               dict(route='preGarrFight', after=0, tail_from=6798, minutes=30, fight=31,
                    # the return's area reload (field mode 1) comes 34 frames after the return on both sides:
                    # the tail starts there, on the frame, rather than where the sweep's return seek ends
                    tail_align=['until 0x66C7E8 u16 == 1 timeout 600']))
FIGHTS[25] = ('Beyd, Zig (set-up 25)', 'bfs25', 390, 3, dict(scene=True))
FIGHTS[34] = ('Mikba (record 4 at 0 HP)', 'bfs34', 390, 3, dict(scene=True))
# a member alone hits one enemy a round: the sample fights with several enemies want more kill rounds and a
# longer return window than three members do (fight 46 with garr, 2026-11-13)
for _f in range(39, 49):
    FIGHTS[_f][4].update(kill_rounds=3, return_presses=600)
CRAFTS = sorted({v[1] for v in FIGHTS.values() if v[1].startswith('bf')})
COLUMNS = ('fight', 'boss', 'side', 'verdict', 'called', 'start', 'win', 'lost', 'back', 'joined', 'party_before',
           'party_in_battle', 'count_in_battle', 'party_after_return', 'ending', 'after', 'vs_plain', 'mark',
           'solo_battle', 'solo_restore', 'champion_before', 'champion_after', 'no_pieces', 'status', 'notes')


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
    extra.pop('minutes', None)
    extra.pop('tail_from', None)
    extra.pop('tail_align', None)
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
        # A team of one: the cross is one member's, so a confirm pressed by `seek ... == 1` (4 frames held, 8
        # released) as the cross opens enters the command and phase 1 is over two frames later, between the
        # seek's checks; the seek gives up while the battle runs on (fight 32 with ryu). An `until` with no
        # presses stalls where the battle's messages want a confirm in phase 3 (fight 35). So the same seek
        # with one-frame presses and one-frame gaps: a check every second frame sees the two frames phase 1
        # lasts even when a press enters the command, and the messages still get their confirms. Six times
        # the presses keep the seek's window in frames.
        pre = f'seek {a.confirm} 0x{S.PHASE:X} u8 == 1 max '
        out = []
        for l in body:
            if l.startswith(pre):
                k = int(l.split(' max ')[1])
                out += ['set hold 1', 'set gap 1', f'{pre}{k * 6}', 'set hold 4', 'set gap 8']
            else:
                out.append(l)
        body = out
    tail_from = FIGHTS[fight][4].get('tail_from')
    if tail_from:
        # After the return: the route's own frames from its own return to the field (field mode 5 -> 2 at
        # recipe frame tail_from in the straight replay), aligned on the sweep's return, so the scene after the
        # fight is played with the owner's presses, walking included, instead of confirms alone
        _, frames = S.frames_of(a.route)
        tail = S.emit_frames(frames[tail_from:])
        k = body.index('watch off')
        align = FIGHTS[fight][4].get('tail_align', [])
        body = body[:k] + align + [f'mark the route from its frame {tail_from}'] + tail + body[k:]
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
    s = S.fight_summary(lg, FIGHTS[fight][4].get('fight', fight))
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
    how = ''
    if f['fatal'] or f['crash'] or status in ('FATAL', 'game exited'):
        verdict = 'crashed'
        notes.append(f['fatal'] or f['crash'] or status)
    elif s['start'] is None:
        verdict = 'did not start'
        if s['facts']['fail']:
            notes.append(s['facts']['fail'])
    elif s['back'] is not None:
        if s['win'] is not None:
            # the win bit with no frame where the enemies left reached 0: an enemy's Escape or the set-up's end
            how = 'win, enemies defeated' if s['none_left'] is not None else 'win, no enemy defeated (an escape)'
        else:
            how = 'loss' if s['lost'] is not None else "by the set-up's script"
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
    nopieces = 0
    with open(lg, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            if 'frame of no pieces' in line:
                nopieces += 1
    row['no_pieces'] = nopieces
    if s['start'] is not None and s['back'] is None and verdict.startswith('started, hung'):
        # the phase watches at the end: still turning (a change in the last 120 frames) or stopped
        ph = [f for f, n, o, v in w if n == 'phase_step' and o]
        last = max((f for f, n, o, v in w), default=0)
        verdict += ' - the battle still turning at the end' if ph and last - ph[-1] < 120 else ' - the battle stopped'
    row['verdict'] = verdict
    row['ending'] = how if s['start'] is not None and s['back'] is not None else ''
    # after the return: the field modes and areas the run went through to its end (the scene going on)
    if s['back'] is not None:
        modes = [H.le(v, 0, 2) for fr, n, o, v in w if n == 'sl_mode' and fr >= s['back']]
        areas = [H.le(v, 0, 2) for fr, n, o, v in w if n == 'area' and fr >= s['back']]
        ch = H.at_or_before(w, 'sl_chapter', None)
        row['after'] = (f'modes {" ".join(str(m) for m in modes) or "-"}; areas {" ".join(str(x) for x in areas) or "-"}; '
                        f'chapter {int(ch, 16) if ch else "?"}; recipe {f["end"] or status}')
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
                            'count_in_battle', 'party_after_return', 'ending', 'after', 'vs_plain', 'mark',
                            'solo_battle', 'solo_restore', 'champion_before', 'champion_after', 'notes'), order) + '\n')


def cmd_prepare(a):
    os.makedirs(a.root, exist_ok=True)
    H.prepare_launcher(a.root, 'cand', a.build)
    H.prepare_game(a.root, a.game_src)
    sv, n = H.prepare_saves(a.root, a.saves_src)
    for name in SOLO_CRAFTS:
        if not os.path.exists(os.path.join(sv, name + '.DAT')):
            import chapter_jump  # noqa: E402  (its make(), with this sweep's crafts put into its table)
            chapter_jump.CRAFTS[name] = SOLO_CRAFTS[name]
            chapter_jump.make(name, sv, sv)
    missing = [c for c in CRAFTS if c not in SOLO_CRAFTS and not os.path.exists(os.path.join(sv, c + '.DAT'))]
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
                                        os.path.join(d, f'{tag}.runlog'), minutes=max(a.minutes, FIGHTS[fight][4].get("minutes", 0)),
                                        extra=['--lang', a.lang])
    S.to_png(os.path.join(d, tag))
    row = summarise(fight, side, status, lg, champion)
    print(f'[{time.strftime("%H:%M:%S")}] fight {fight} {side}: {row["verdict"]} ({secs:.0f} s; '
          f'party {row.get("party_in_battle")}, count {row.get("count_in_battle")}, mark {row.get("mark")})', flush=True)
    return row


def compare(row, plain):
    def one(key, label):
        return f'{label} ' + ('same' if row.get(key) == plain.get(key) else f'DIFFERS (plain: {plain.get(key)})')
    return '; '.join([one('ending', 'ending'), one('party_after_return', 'party after'),
                      one('after', 'after the return')])


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
            row = run_one(a, fight, side, env, champ)
            plain = rows.get((str(fight), 'plain'))
            if plain and side != 'plain':
                row['vs_plain'] = compare(row, plain)
            rows[(str(fight), side)] = row
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
