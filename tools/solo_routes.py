"""The battle routes with a team of one (docs/solo-character-proof.md 3; round doc
docs/solo-character-round.md 4 "RT"): each route run once plain (no solo
behaviour) and once per champion with `BOF3X_ON=SoloCharacter` and
`BOF3X_SOLO=<name>`, and a verdict per run - ran to its end in step with the
plain run / desynced / hung / crashed / failed - with the party lists, the
member count, the champion's mark and the champion's record (level, joined bits,
EXP, HP, AP) before and after, read from watches put into a copy of the route
(watches take no frame, so the route plays frame for frame as recorded).

A recorded route replays pad words by frame. Once one member fights where the
recording had three, the turn order and the menus change, so a route that
picks commands in battle is expected to desync; `robustness` measures each
route's battle inputs (from the plain run's battle windows) and says which are
input-robust (confirm only) and which want recording anew (the owner's
BOF3X_RECORD; docs/solo-character-proof.md 5 lists them).

    python tools/solo_routes.py prepare --root R --build C            # launcher copy, game folder, saves
    python tools/solo_routes.py run --root R --champions ryu nina     # plain once, then each champion
    python tools/solo_routes.py robustness --root R                   # needs the plain runs
    python tools/solo_routes.py report --root R

Routes (default): combat, combatGroup, combat_exp, dragonTransform, gameover,
quick_combat (tools/recipes/). `--routes` takes any recipe name. Output under
<root>/routes: the watched copies, each run's .runlog and .log, frames;
<root>/routes_results.tsv and routes_results.md (the latest run of each).
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import solo_harness as H  # noqa: E402

DEFAULT_ROUTES = ('combat', 'combatGroup', 'combat_exp', 'dragonTransform', 'gameover', 'quick_combat')
CONFIRMS = {'circle', 'r2', 'l2', 'cross'}   # the buttons that confirm in these routes' battles (battle_commands.txt: L2 / R2 confirm like circle)
COLUMNS = ('route', 'side', 'verdict', 'status', 'end_frame', 'battles', 'plain_battles', 'final_mode',
           'party_before', 'party_after', 'count_before', 'count_after', 'mark_before', 'mark_after',
           'champion_before', 'champion_after', 'solo_log', 'notes')


def routes_dir(root):
    d = os.path.join(root, 'routes')
    os.makedirs(d, exist_ok=True)
    return d


def watched_copy(root, route):
    src = os.path.join(H.TOOLS, 'recipes', route + '.txt')
    if not os.path.exists(src):
        sys.exit(f'no recipe {src}')
    with open(src, encoding='utf-8') as f:
        lines = f.read().splitlines()
    out = H.insert_after_header(lines, ['# solo_routes.py: the party watched (no frame of their own)'] + H.party_watches())
    dst = os.path.join(routes_dir(root), route + '.txt')
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')
    return dst


def run_one(a, route, side, env):
    d = routes_dir(a.root)
    recipe = watched_copy(a.root, route)
    full = {'BOF3X_OPT': 'none', 'BOF3X_LAYERING': '0',
            'BOF3X_RECIPE_SAVES': H.winpath(os.path.join(a.root, 'saves'))}
    if a.speed != 1:
        full['BOF3X_SPEED'] = str(a.speed)
    full.update(env)
    extra = ['--lang', a.lang]
    if route == 'quick_combat_cross':
        extra.append('--no-save')
    rc, status, lg, secs = H.run_recipe(a.root, recipe, os.path.join(d, f'{route}_{side}'),
                                        os.path.join(a.root, 'Lcand'), full,
                                        os.path.join(d, f'{route}_{side}.runlog'), minutes=a.minutes, extra=extra)
    print(f'[{time.strftime("%H:%M:%S")}] {route} {side}: status {status or "?"} rc {rc} {secs:.0f} s', flush=True)
    return status, lg


def summarise(route, side, status, lg, champion, plain):
    w = H.read_watches(lg)
    f = H.log_facts(lg)
    b = H.battles(w)
    mode_last = H.timeline(w, 'sl_mode')
    final_mode = H.le(mode_last[-1][1], 0, 2) if mode_last else None
    # "before" is the value at the first battle's first frame (the save loaded, the party as it fights);
    # "after" the last value of the run
    t0 = b[0][0] if b else None
    p0, p1 = H.at_or_before(w, 'sl_party', t0), H.at_or_before(w, 'sl_party', None)
    c0, c1 = H.at_or_before(w, 'sl_count', t0), H.at_or_before(w, 'sl_count', None)
    m0, m1 = H.at_or_before(w, 'sl_mark', t0), H.at_or_before(w, 'sl_mark', None)
    row = {'route': route, 'side': side, 'status': status, 'end_frame': f['end_frame'],
           'battles': ' '.join(f'{s}-{e}' for s, e in b) or 'none', 'final_mode': final_mode,
           'party_before': H.party_text(p0) if p0 else '', 'party_after': H.party_text(p1) if p1 else '',
           'count_before': int(c0, 16) if c0 else '', 'count_after': int(c1, 16) if c1 else '',
           'mark_before': int(m0, 16) if m0 else '', 'mark_after': int(m1, 16) if m1 else '',
           'solo_log': ' / '.join(s for s in f['solo'] if 'self-test' not in s)[:400]}
    if champion is not None:
        name = f'sl_rec{H.CHAMPIONS[champion]}'
        r0, r1 = H.at_or_before(w, name, t0), H.at_or_before(w, name, None)
        row['champion_before'] = H.record_text(r0) if r0 else ''
        row['champion_after'] = H.record_text(r1) if r1 else ''
    notes = []
    if f['fatal'] or f['crash'] or status == 'FATAL':
        verdict = 'crashed'
        notes.append(f['fatal'] or f['crash'])
    elif status == 'timed out':
        verdict = 'hung'
    elif status == 'game exited':
        verdict = 'crashed (the game exited before the recipe ended)'
    elif status == 'FAILED':
        verdict = 'failed (a recipe check gave up)'
    elif status != 'done':
        verdict = f'not finished ({status or "no status"})'
    else:
        verdict = 'ran to its end'
    if plain is not None and verdict == 'ran to its end':
        pb = plain.get('battles')
        row['plain_battles'] = pb
        same_shape = row['battles'] == pb and final_mode == plain.get('final_mode')
        if same_shape:
            verdict += ', in step with the plain run'
        else:
            verdict = 'desynced (ran to the recipe\'s end, the battles or the end mode differ from the plain run)'
    row['verdict'] = verdict
    row['notes'] = '; '.join(n for n in notes if n)
    return row


def results_path(root):
    return os.path.join(root, 'routes_results.tsv')


def load_results(root):
    rows = {}
    p = results_path(root)
    if os.path.exists(p):
        with open(p, encoding='utf-8') as f:
            lines = [l.rstrip('\n').split('\t') for l in f if not l.startswith('#')]
        if lines:
            head = lines[0]
            for l in lines[1:]:
                r = dict(zip(head, l))
                rows[(r['route'], r['side'])] = r
    return rows


def save_results(root, rows):
    H.write_tsv(results_path(root), COLUMNS, list(rows.values()),
                'tools/solo_routes.py: the latest run of each route and side (docs/solo-character-proof.md 3)')
    with open(os.path.join(root, 'routes_results.md'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(H.md_table(('route', 'side', 'verdict', 'battles', 'party_before', 'party_after', 'count_after',
                            'mark_after', 'champion_before', 'champion_after'), list(rows.values())) + '\n')


def cmd_prepare(a):
    os.makedirs(a.root, exist_ok=True)
    H.prepare_launcher(a.root, 'cand', a.build)
    H.prepare_game(a.root, a.game_src)
    _, n = H.prepare_saves(a.root, a.saves_src)
    print(f'prepared {a.root}: Lcand from {a.build}, {n} saves')


def cmd_run(a):
    rows = load_results(a.root)
    for route in a.routes:
        if H.stopped(a.root):
            print('STOP seen')
            break
        if a.again or (route, 'plain') not in rows or rows[(route, 'plain')].get('status') != 'done':
            status, lg = run_one(a, route, 'plain', {})
            rows[(route, 'plain')] = summarise(route, 'plain', status, lg, None, None)
            save_results(a.root, rows)
        plain = rows[(route, 'plain')]
        for champ in a.champions:
            if H.stopped(a.root):
                break
            env = {'BOF3X_ON': 'SoloCharacter', 'BOF3X_SOLO': champ}
            if a.if_present:
                env['BOF3X_ON'] = 'SoloCharacter,SoloChampionIfPresent'
            side = 'solo_' + champ + ('_ifp' if a.if_present else '')
            status, lg = run_one(a, route, side, env)
            row = summarise(route, side, status, lg, champ, plain)
            rows[(route, side)] = row
            save_results(a.root, rows)
            print(f'    {row["verdict"]}; party {row["party_before"]} -> {row["party_after"]}; '
                  f'champion {row.get("champion_before")} -> {row.get("champion_after")}', flush=True)
    print(open(os.path.join(a.root, 'routes_results.md'), encoding='utf-8').read())
    return 0


PAD = {'up', 'down', 'left', 'right', 'circle', 'cross', 'square', 'triangle', 'l1', 'l2', 'r1', 'r2', 'start',
       'select'}
NO_FRAME = {'watch', 'peek', 'poke', 'mark', 'every', 'quit', 'links', 'door', 'goto', 'battle', 'place', 'view', 'end'}


def expand(path):
    """A recipe as one entry a frame: the buttons held that frame ('' none), keyboard keys as `key:NAME`.
    The steps of src/hook/input_script.cpp that take frames (wait, hold, press with `set hold` / `set gap`,
    shot, key); `until` / `seek` cannot be expanded without the game, so a recipe holding one is refused."""
    frames, hold_n, gap_n = [], 4, 8

    def name(b):
        return '+'.join(x if x in PAD else 'key:' + x for x in b.split('+'))
    with open(path, encoding='utf-8') as f:
        for line in f:
            t = line.split('#')[0].split()
            if not t:
                continue
            op = t[0]
            if op == 'set' and len(t) == 3:
                if t[1] == 'hold':
                    hold_n = int(t[2])
                elif t[1] == 'gap':
                    gap_n = int(t[2])
            elif op == 'wait':
                frames += [''] * int(t[1])
            elif op == 'hold':
                frames += [name(t[1])] * int(t[2])
            elif op == 'press':
                k = int(t[2][1:]) if len(t) > 2 and t[2].startswith('x') else 1
                frames += ([name(t[1])] * hold_n + [''] * gap_n) * k
            elif op == 'shot':
                n = int(t[2]) if len(t) > 2 else 30
                frames += [name(t[3]) if len(t) > 3 else ''] * n
            elif op == 'key':
                n = int(t[2]) if len(t) > 2 else 1
                if n == 0 and frames is not None:
                    frames.append(None)   # a key with no frame: marked on the next frame below
                else:
                    frames += ['key:' + t[1]] * n
            elif op in ('until', 'seek'):
                raise ValueError(f'`{op}` takes a number of frames only the game knows')
            elif op in NO_FRAME:
                continue
            else:
                raise ValueError(f'unknown step `{op}`')
    out, pending = [], False
    for x in frames:
        if x is None:
            pending = True
            continue
        out.append(('key:0+' + x if x else 'key:0') if pending else x)
        pending = False
    return out


def cmd_robustness(a):
    """Per route: the presses that land inside the plain run's battles, confirm-only or not."""
    rows = load_results(a.root)
    out = []
    for route in a.routes:
        plain = rows.get((route, 'plain'))
        try:
            frames = expand(os.path.join(H.TOOLS, 'recipes', route + '.txt'))
        except ValueError as e:
            out.append({'route': route, 'verdict': f'not expanded: {e}'})
            continue
        if not plain or not plain.get('battles') or plain['battles'] == 'none':
            out.append({'route': route, 'verdict': 'no plain run with a battle yet (run `run` first)'})
            continue
        wins = []
        for s in plain['battles'].split():
            a0, _, b0 = s.partition('-')
            wins.append((int(a0), int(b0) if b0 and b0 != 'None' else len(frames)))
        confirm = other = 0
        kinds = set()
        prev = ''
        for i, btn in enumerate(frames):
            if btn and btn != prev and any(s <= i < e for s, e in wins):
                parts = set(btn.split('+'))
                if parts <= CONFIRMS:
                    confirm += 1
                else:
                    other += 1
                    kinds |= parts - CONFIRMS
            prev = btn
        robust = other == 0
        out.append({'route': route, 'frames': len(frames), 'battles': plain['battles'], 'confirm_presses': confirm,
                    'other_presses': other, 'other_buttons': ' '.join(sorted(kinds)),
                    'verdict': 'input-robust (confirm only in battle)' if robust else
                    'not input-robust: moves a cursor or presses other buttons in battle'})
    print(H.md_table(('route', 'frames', 'battles', 'confirm_presses', 'other_presses', 'other_buttons', 'verdict'),
                     out))
    return 0


def cmd_summarise(a):
    """The rows again from the logs already under <root>/routes (after a change of the reader)."""
    rows = load_results(a.root)
    d = routes_dir(a.root)
    for (route, side), r in sorted(rows.items(), key=lambda kv: kv[0][1] != 'plain'):
        lg = os.path.join(d, f'{route}_{side}.log')
        champ = side.split('_')[1] if side.startswith('solo_') else None
        plain = rows.get((route, 'plain')) if side != 'plain' else None
        rows[(route, side)] = summarise(route, side, r.get('status', ''), lg if os.path.exists(lg) else None,
                                        champ, plain)
    save_results(a.root, rows)
    return cmd_report(a)


def cmd_report(a):
    print(open(os.path.join(a.root, 'routes_results.md'), encoding='utf-8').read())
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('prepare')
    p.add_argument('--root', required=True)
    p.add_argument('--build', required=True, help='the build directory to run (the round\'s merged tip)')
    H.add_prepare_args(p)
    p.set_defaults(fn=cmd_prepare)
    for name, fn in (('run', cmd_run), ('robustness', cmd_robustness), ('summarise', cmd_summarise),
                     ('report', cmd_report)):
        p = sub.add_parser(name)
        p.add_argument('--root', required=True)
        p.add_argument('--routes', nargs='+', default=list(DEFAULT_ROUTES))
        p.add_argument('--champions', nargs='+', default=['ryu'], choices=sorted(H.CHAMPIONS))
        p.add_argument('--if-present', action='store_true', help='also turn on SoloChampionIfPresent (DIV-0229)')
        p.add_argument('--again', action='store_true', help='run the plain side again even if it has a result')
        p.add_argument('--speed', type=int, default=4, help='BOF3X_SPEED (the recipe is frame-indexed; default 4)')
        p.add_argument('--lang', default='en-US', help='the routes were recorded under en-US')
        p.add_argument('--minutes', type=float, default=20)
        p.set_defaults(fn=fn)
    a = ap.parse_args()
    a.root = os.path.abspath(a.root)
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
