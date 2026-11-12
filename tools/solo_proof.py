"""The solo-character round's off-proof (docs/solo-character-proof.md 2; round doc
docs/solo-character-round.md 4 "RT" and 5): the ten state-hash routes on a
candidate build with the solo behaviours off, against references recorded fresh
from the round's base build - the `orig4` pairs are stale (docs/HANDOFF.md 14b).

The method is the 2026-11-12 proof chains' (D:\\bof3x-runs\\de1112\\proof\\chain.sh;
docs/state-hash.md 6): the base build run twice per route (the reference pair),
the candidate once, `tools/statehash.py check REF REFB NEW`; a route that differs
gets a second candidate run, checked against the same pair, so run-to-run noise
(the sound page 0x7DE000, the VRAM shadow, heap pointers on 0x6C3000) is told
from a difference the candidate makes. Every run: the exe host, `BOF3X_OPT=none`,
`BOF3X_LAYERING=0`, the skip list of this tree, a launcher copy with no ini, a
game folder of the root's own, `--slot0-hold` on the shop route. One game at a
time; a file <root>/STOP ends a batch before its next run.

    python tools/solo_proof.py prepare --root R --base-build B --cand-build C
    python tools/solo_proof.py refs    --root R [--routes ...]
    python tools/solo_proof.py check   --root R --variant off       [--routes ...]
    python tools/solo_proof.py check   --root R --variant on-none   [--routes ...]
    python tools/solo_proof.py check   --root R --variant on-champ:ryu [--routes ...]
    python tools/solo_proof.py all     --root R [--champion ryu]     # refs, off, on-none, on-champ
    python tools/solo_proof.py chapters --root R                      # which routes stay in chapter 0
    python tools/solo_proof.py report  --root R

Variants (what the candidate is run with):
  off            no BOF3X_ON, no BOF3X_SOLO: the behaviours' default, off. Must be identical.
  on-none        BOF3X_ON=SoloCharacter, BOF3X_SOLO unset: the mode with no champion stamps nothing and
                 stands down. Must be identical.
  on-champ:NAME  BOF3X_ON=SoloCharacter, BOF3X_SOLO=NAME, only on routes that stay in chapter 0 (the mode
                 stands down through the intro, round doc 6a item 3; `chapters` says which, from the route
                 save's chapter byte, and the run's log is checked for a stamp line). Must be identical.
                 `--all-routes` runs it on every route: there the mark is stamped and the mode is active, so
                 a difference is expected and is reported, not judged.

Output under <root>/sh: the hashes cn_<route>_<side>.sh, each run's input_run output (.runlog) and the
game's log (.log), the checks; <root>/results.tsv one row a route and variant, appended.
"""
import argparse
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import solo_harness as H  # noqa: E402

SKIP = os.path.join(H.TOOLS, 'statehash_skip.txt')
RESULT_COLUMNS = ('when', 'route', 'variant', 'verdict', 'first_difference', 'runs', 'stamp', 'notes')


def log(root, msg):
    line = f'[{time.strftime("%Y-%m-%d %H:%M:%S")}] {msg}'
    print(line, flush=True)
    with open(os.path.join(root, 'proof.log'), 'a', encoding='utf-8') as f:
        f.write(line + '\n')


def sh_dir(root):
    d = os.path.join(root, 'sh')
    os.makedirs(os.path.join(d, 'out'), exist_ok=True)
    return d


def hash_path(root, route, side):
    return os.path.join(sh_dir(root), f'cn_{route}_{side}.sh')


def run_side(root, route, side, launcher, extra_env, minutes):
    """One state-hashed run of a route; returns (ok, summary)."""
    d = sh_dir(root)
    recipe = os.path.join(H.TOOLS, 'recipes', route + '.txt')
    env = {'BOF3X_OPT': 'none', 'BOF3X_LAYERING': '0', 'BOF3X_STATEHASH_SKIP': H.winpath(SKIP),
           'BOF3X_RECIPE_SAVES': H.winpath(os.path.join(root, 'saves')),
           'BOF3X_STATEHASH': H.winpath(hash_path(root, route, side))}
    cache = os.path.join(root, 'cache_' + ('base' if launcher.endswith('Lbase') else 'cand'))
    if os.path.isdir(cache):
        env['BOF3X_CACHE'] = H.winpath(cache)
    env.update(extra_env)
    if os.path.exists(hash_path(root, route, side)):
        os.remove(hash_path(root, route, side))
    extra = ['--slot0-hold'] if route == 'shop' else []
    rc, status, lg, secs = H.run_recipe(root, recipe, os.path.join(d, 'out', f'{route}_{side}'), launcher, env,
                                        os.path.join(d, f'cn_{route}_{side}.runlog'), minutes=minutes, extra=extra)
    f = H.log_facts(lg)
    summary = (f'{route} {side} rc={rc} status={status or "?"} {secs:.0f}s end={f["end"]}@{f["end_frame"]} '
               f'fatal={"yes" if f["fatal"] else "no"} {f["behaviours"]}')
    log(root, summary + (f' | {f["fatal"]}' if f['fatal'] else ''))
    return status == 'done' and os.path.exists(hash_path(root, route, side)), f


def check(root, route, ref, refb, new, name):
    """tools/statehash.py check; returns (exit code, first line, first difference text)."""
    out = os.path.join(sh_dir(root), name)
    with open(out, 'w', encoding='utf-8') as f:
        rc = subprocess.run([sys.executable, os.path.join(H.TOOLS, 'statehash.py'), 'check',
                             hash_path(root, route, ref), hash_path(root, route, refb), hash_path(root, route, new)],
                            stdout=f, stderr=subprocess.STDOUT, cwd=H.ROOT).returncode
    text = open(out, encoding='utf-8', errors='replace').read()
    first = ''
    m = re.search(r'(\d+) pages differ over (\d+) compared ticks; the first at tick (\d+)', text)
    if m:
        rows = re.findall(r'^0x([0-9A-F]{6})\s+(\d+)\s+(\d+)\s+(\d+)\s+(.*)$', text, re.M)
        rows.sort(key=lambda r: int(r[1]))
        pages = ', '.join(f'0x{p} from tick {a} ({c} ticks; {s.strip()[:60]})' for p, a, b, c, s in rows[:3])
        first = f'{m.group(1)} pages, first tick {m.group(3)}: {pages}'
    elif rc != 0:
        first = ' / '.join(text.strip().splitlines()[-2:])[:300]
    log(root, f'check {name} exit={rc} | ' + ' '.join(text.strip().splitlines()[:2])[:300])
    return rc, first


def append_result(root, row):
    path = os.path.join(root, 'results.tsv')
    new = not os.path.exists(path)
    with open(path, 'a', encoding='utf-8', newline='\n') as f:
        if new:
            f.write('# tools/solo_proof.py: one row a route and variant (docs/solo-character-proof.md 2)\n')
            f.write('\t'.join(RESULT_COLUMNS) + '\n')
        f.write('\t'.join(str(row.get(c, '')).replace('\t', ' ') for c in RESULT_COLUMNS) + '\n')


def variant_env(variant):
    if variant == 'off':
        return {}, 'off'
    if variant == 'on-none':
        return {'BOF3X_ON': 'SoloCharacter'}, 'onnone'
    m = re.fullmatch(r'on-champ:(\w+)', variant)
    if m and m.group(1) in H.CHAMPIONS:
        return {'BOF3X_ON': 'SoloCharacter', 'BOF3X_SOLO': m.group(1)}, 'on' + m.group(1)
    sys.exit(f'--variant {variant}: off, on-none or on-champ:<{"|".join(H.CHAMPIONS)}>')


def route_chapter(root, route):
    name = H.recipe_save_name(os.path.join(H.TOOLS, 'recipes', route + '.txt'))
    if not name:
        return None, None, None
    p = os.path.join(root, 'saves', name + '.DAT')
    if not os.path.exists(p):
        return name, None, None
    ch, mark = H.save_bytes(p)
    return name, ch, mark


def cmd_prepare(a):
    os.makedirs(a.root, exist_ok=True)
    H.prepare_launcher(a.root, 'base', a.base_build)
    H.prepare_launcher(a.root, 'cand', a.cand_build)
    H.prepare_game(a.root, a.game_src)
    s, n = H.prepare_saves(a.root, a.saves_src)
    with open(os.path.join(a.root, 'builds.txt'), 'w', encoding='utf-8') as f:
        for side, b in (('base', a.base_build), ('cand', a.cand_build)):
            head = subprocess.run(['git', '-C', os.path.dirname(os.path.abspath(b)), 'log', '--oneline', '-1'],
                                  capture_output=True, text=True).stdout.strip()
            f.write(f'{side}\t{os.path.abspath(b)}\t{head}\n')
    log(a.root, f'prepared: Lbase from {a.base_build}, Lcand from {a.cand_build}, {n} saves, game from {a.game_src}')


def need_prepared(root):
    for d in ('Lbase', 'Lcand', 'g', 'saves'):
        if not os.path.isdir(os.path.join(root, d)):
            sys.exit(f'{root}: no {d} - run `solo_proof.py prepare` first')


def cmd_refs(a):
    need_prepared(a.root)
    for route in a.routes:
        for side in ('ref', 'refb'):
            if H.stopped(a.root):
                log(a.root, 'STOP seen')
                return 1
            if os.path.exists(hash_path(a.root, route, side)) and not a.again:
                log(a.root, f'{route} {side}: kept (--again records it anew)')
                continue
            run_side(a.root, route, side, os.path.join(a.root, 'Lbase'), {}, a.minutes)
        # the pair against itself: how much the base disagrees with itself on this machine now
        if all(os.path.exists(hash_path(a.root, route, s)) for s in ('ref', 'refb')):
            out = os.path.join(sh_dir(a.root), f'diff_{route}_ref_refb.txt')
            with open(out, 'w', encoding='utf-8') as f:
                rc = subprocess.run([sys.executable, os.path.join(H.TOOLS, 'statehash.py'), 'diff',
                                     hash_path(a.root, route, 'ref'), hash_path(a.root, route, 'refb')],
                                    stdout=f, stderr=subprocess.STDOUT, cwd=H.ROOT).returncode
            log(a.root, f'{route} ref/refb diff exit={rc}: ' + open(out, encoding='utf-8').readline().strip()[:200])
    return 0


def cmd_check(a):
    need_prepared(a.root)
    env, side = variant_env(a.variant)
    champ = a.variant.startswith('on-champ:')
    bad = 0
    for route in a.routes:
        if H.stopped(a.root):
            log(a.root, 'STOP seen')
            return 1
        if not all(os.path.exists(hash_path(a.root, route, s)) for s in ('ref', 'refb')):
            log(a.root, f'{route}: no reference pair - run `refs` first')
            append_result(a.root, {'when': time.strftime('%F %T'), 'route': route, 'variant': a.variant,
                                   'verdict': 'no reference'})
            bad += 1
            continue
        save, ch, mark = route_chapter(a.root, route)
        if champ and not a.all_routes and ch != 0:
            log(a.root, f'{route}: save {save} is chapter {ch}, not 0 - not a route for {a.variant} (--all-routes)')
            append_result(a.root, {'when': time.strftime('%F %T'), 'route': route, 'variant': a.variant,
                                   'verdict': 'not run (save chapter %s)' % ch})
            continue
        ok, facts = run_side(a.root, route, side, os.path.join(a.root, 'Lcand'), env, a.minutes)
        stamp = next((s for s in facts['solo'] if 'stamped' in s), '')
        notes = []
        if not ok:
            notes.append(f'run did not finish: {facts["fatal"] or facts["end"] or "no hash"}')
        rc, first = check(a.root, route, 'ref', 'refb', side, f'check_{route}_{side}.txt') if ok else (1, '')
        runs = side
        verdict = 'identical' if rc == 0 else 'differs'
        if rc != 0 and ok and not a.no_repeat:
            ok2, facts2 = run_side(a.root, route, side + '2', os.path.join(a.root, 'Lcand'), env, a.minutes)
            runs += ',' + side + '2'
            if ok2:
                rc2, first2 = check(a.root, route, 'ref', 'refb', side + '2', f'check_{route}_{side}2.txt')
                if rc2 == 0:
                    verdict = 'identical on the repeat (first run noise)'
                    notes.append(f'first run: {first}')
                    first = ''
                else:
                    # is the base within the candidate's own spread? (chain.sh's recheck)
                    rc3, first3 = check(a.root, route, side, side + '2', 'ref', f'recheck_{route}_{side}.txt')
                    verdict = 'differs (both runs)' if rc3 != 0 else 'differs (both runs; base within the candidate pair\'s spread)'
                    notes.append(f'repeat: {first2}')
            else:
                notes.append(f'repeat did not finish: {facts2["fatal"] or facts2["end"]}')
        if champ:
            if stamp:
                notes.append('the mark was stamped: the mode was active on this route')
                if not a.all_routes:
                    verdict += ' (stamped: route does not qualify)'
        if not ok:
            verdict = 'run failed'
        if rc != 0 or not ok:
            bad += 1
        append_result(a.root, {'when': time.strftime('%F %T'), 'route': route, 'variant': a.variant,
                               'verdict': verdict, 'first_difference': first, 'runs': runs,
                               'stamp': stamp[:120], 'notes': '; '.join(notes)})
        log(a.root, f'RESULT {route} {a.variant}: {verdict}' + (f' | {first}' if first else ''))
    return 1 if bad else 0


def cmd_chapters(a):
    print('route\tsave\tchapter\tmark\tqualifies for on-champ')
    for route in a.routes:
        save, ch, mark = route_chapter(a.root, route)
        print(f'{route}\t{save}\t{ch}\t{mark}\t{"yes" if ch == 0 else "no"}')
    print('(the chapter byte of the route\'s save at load; a route can still change chapter as it plays - the '
          'on-champ run\'s log is checked for a stamp line)')
    return 0


def cmd_report(a):
    path = os.path.join(a.root, 'results.tsv')
    if not os.path.exists(path):
        sys.exit('no results.tsv yet')
    rows = {}
    with open(path, encoding='utf-8') as f:
        lines = [l.rstrip('\n').split('\t') for l in f if not l.startswith('#')]
    head = lines[0]
    for l in lines[1:]:
        r = dict(zip(head, l))
        rows[(r['route'], r['variant'])] = r   # the latest row of each wins
    print(H.md_table(('route', 'variant', 'verdict', 'first_difference', 'notes'), list(rows.values())))
    return 0


def cmd_all(a):
    rc = cmd_refs(a)
    if rc:
        return rc
    worst = 0
    for v in ('off', 'on-none', f'on-champ:{a.champion}'):
        a.variant = v
        worst |= cmd_check(a)
        if H.stopped(a.root):
            break
    cmd_report(a)
    return worst


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    def common(p, routes=True):
        p.add_argument('--root', required=True, help='the harness root (under D:\\bof3x-runs, an excluded folder)')
        if routes:
            p.add_argument('--routes', nargs='+', default=list(H.TEN_ROUTES), help='default the ten state-hash routes')
            p.add_argument('--minutes', type=float, default=40, help='input_run.py --minutes per run')
    p = sub.add_parser('prepare', help='the root: launcher copies of both builds, the game folder, the saves')
    common(p, routes=False)
    p.add_argument('--base-build', required=True, help='the base build directory (main at the merge-base): .../build')
    p.add_argument('--cand-build', required=True, help='the candidate build directory (the round\'s merged tip)')
    H.add_prepare_args(p)
    p.set_defaults(fn=cmd_prepare)
    p = sub.add_parser('refs', help='the reference pair per route, from the base build (ref, refb)')
    common(p)
    p.add_argument('--again', action='store_true', help='record a pair that already exists anew')
    p.set_defaults(fn=cmd_refs)
    p = sub.add_parser('check', help='the candidate per route against the pair')
    common(p)
    p.add_argument('--variant', default='off', help='off | on-none | on-champ:<champion>')
    p.add_argument('--all-routes', action='store_true', help='on-champ: also routes past chapter 0 (reported, not judged)')
    p.add_argument('--no-repeat', action='store_true', help='no second candidate run on a difference')
    p.set_defaults(fn=cmd_check)
    p = sub.add_parser('all', help='refs, then off, on-none and on-champ')
    common(p)
    p.add_argument('--again', action='store_true')
    p.add_argument('--champion', default='ryu', choices=sorted(H.CHAMPIONS))
    p.add_argument('--all-routes', action='store_true')
    p.add_argument('--no-repeat', action='store_true')
    p.set_defaults(fn=cmd_all)
    p = sub.add_parser('chapters', help='the chapter byte of each route\'s save (which routes stay in chapter 0)')
    common(p)
    p.set_defaults(fn=cmd_chapters)
    p = sub.add_parser('report', help='the latest verdict per route and variant, as a table')
    common(p, routes=False)
    p.set_defaults(fn=cmd_report)
    a = ap.parse_args()
    a.root = os.path.abspath(a.root)
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
