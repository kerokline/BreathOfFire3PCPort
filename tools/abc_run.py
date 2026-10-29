#!/usr/bin/env python
"""The A/B/C install harness: the same route on two or more installs (the PC
install, a PSX disc alone, a PSP disc alone), the game writing its own
frames, compared picture by picture (docs/abc-harness.md).

    python tools/abc_run.py prepare [--set eu]              # caches, installs, launcher (idempotent, offline)
    python tools/abc_run.py run --set eu --route attract    # the attract sequence on each install, one at a time
    python tools/abc_run.py run --set eu --route tools/recipes/combat.txt
    python tools/abc_run.py run --set eu --route fairyVillage --save fairyVillage   # a recipe with no `# save` header
    python tools/abc_run.py compare --set eu --route combat # again, from the frames on disk
    python tools/abc_run.py all --set eu --route attract    # prepare, run, compare

The state hash and call traces check that our code computes the same thing
on every install; they miss the places where the DATA differs while the code
runs the same - a zeroed table that shifts the combatants, the wrong icon
page, a text offset. Those show in the picture, so the picture is what this
compares: a frame every --every recipe frames (BOF3X_SHOT_EVERY, the game's
own render target) plus the recipe's own `shot`s, matched by tick, with a
difference measure, a heat map, a contact sheet A | B | C | diff A-B | diff
A-C | diff B-C, and a report that lists what no known difference explains
first (recipes/abc-known.toml). --statehash adds the state hash per run and
`statehash.py diff` per pair as context; off by default.

Runs are sequential, never two games at once, each through tools/input_run.py
(--no-front: the game writes its own frames and runs unfocused). Every run
gets BOF3X_OPT=none and BOF3X_LAYERING=0 unless its install says otherwise,
and the same BOF3.CFG. Kill only what we start (input_run ends its own game
by pid).
"""
import argparse, glob, hashlib, json, os, re, shutil, subprocess, sys, time, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abc_compare  # noqa: E402
import abc_known  # noqa: E402
import bof3x_home  # noqa: E402  (a release's home; a checkout keeps the repo defaults)

INSTALLS = os.path.join(ROOT, 'recipes', 'abc-installs.toml')
KNOWN = os.path.join(ROOT, 'recipes', 'abc-known.toml')
LAUNCHER_FILES = ('bof3x-launcher.exe', 'bof3x-launcher-dev.exe', 'bof3x.dll', 'bof3x.exe')   # -dev: the hosted runs (launcher.cpp ForwardToDevLauncher)
EVERY = 30            # one frame a second of game time (30 logic frames a second)
ATTRACT_FRAMES = 11000  # the attract sequence: about one cycle and the title before it


def main_checkout():
    """The checkout holding bof3/ and CDImage/: this one, or the main one of a worktree."""
    try:
        common = subprocess.run(['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=ROOT,
                                capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(os.path.normpath(common))
    except (OSError, subprocess.CalledProcessError):
        return ROOT


class Config:
    def __init__(self, path, root):
        with open(path, 'rb') as fh:
            self.doc = tomllib.load(fh)
        self.root = os.path.abspath(root)
        self.vars = {'root': self.root, 'main': main_checkout()}
        paths = self.doc.get('paths', {})
        self.vars['pc'] = self.sub(paths.get('pc_game', '{main}/bof3'))
        self.vars['discs'] = self.sub(paths.get('discs', '{main}/CDImage'))
        d = self.doc.get('defaults', {})
        self.cfg = self.sub(d['cfg']) if d.get('cfg') else None
        self.recipe_saves = self.sub(d.get('recipe_saves', '{main}/tools/recipe_saves'))
        self.installs = {}
        for i in self.doc.get('install', []):
            i = dict(i)
            for k in ('game', 'cache', 'launcher'):
                if k in i:
                    i[k] = self.sub(i[k])
            i['build'] = [self.sub(s) for s in i.get('build', [])]
            i.setdefault('launcher', os.path.join(self.root, 'L'))
            i.setdefault('traits', [])
            i.setdefault('env', {})
            self.installs[i['name']] = i
        self.sets = self.doc.get('set', {})

    def sub(self, s):
        return os.path.normpath(s.format(**self.vars))

    def members(self, name, only=None):
        if only:
            names = only.split(',')
        else:
            if name not in self.sets:
                sys.exit(f'no set {name!r} in the installs file (sets: {", ".join(self.sets)})')
            names = self.sets[name]['installs']
        for n in names:
            if n not in self.installs:
                sys.exit(f'no install {n!r} in the installs file')
        return [self.installs[n] for n in names]


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for b in iter(lambda: fh.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def stamp_read(path):
    try:
        with open(path, encoding='utf-8') as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return None


def stamp_write(path, data):
    with open(path, 'w', encoding='utf-8') as fh:
        json.dump(data, fh, indent=1)


def run_tool(cmd, log):
    print('  $ ' + ' '.join(f'"{c}"' if ' ' in c else c for c in cmd))
    t0 = time.time()
    with open(log, 'w', encoding='utf-8') as fh:
        r = subprocess.run(cmd, cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT)
    print(f'    exit {r.returncode}, {time.time() - t0:.0f} s, log {log}')
    return r.returncode


# --- prepare ------------------------------------------------------------------

def tools_hash():
    """A hash of the importer's tools and recipes (this harness's own files
    left out): a cache built by other tools is rebuilt, since what a build
    writes (the exe image's places and gaps, the stand-ins) moves with them."""
    h = hashlib.sha256()
    for d, skip in (('tools', 'abc_'), ('recipes', 'abc-')):
        base = os.path.join(ROOT, d)
        for f in sorted(os.listdir(base)):
            p = os.path.join(base, f)
            if f.startswith(skip) or not os.path.isfile(p):
                continue
            if d == 'tools' and not f.endswith('.py'):
                continue
            h.update(f.encode())
            with open(p, 'rb') as fh:
                h.update(fh.read().replace(b'\r\n', b'\n'))
    return h.hexdigest()[:16]


def prepare_cache(i, logs):
    cache = i['cache']
    if not i['build']:
        if not os.path.exists(os.path.join(cache, 'manifest.toml')):
            sys.exit(f'{i["name"]}: cache {cache} has no manifest.toml and the install names no build')
        print(f'  cache {cache}: used as it is')
        return
    want = {'sources': i['build'], 'lang': i.get('build_lang', []), 'opt': i.get('build_opt', []),
            'tools': tools_hash()}
    st = os.path.join(cache, '.abc_build.json')
    have = stamp_read(st)
    if have and have.get('want') == want and os.path.exists(os.path.join(cache, 'manifest.toml')):
        print(f'  cache {cache}: built ({have.get("built")})')
        return
    for s in i['build']:
        if not os.path.exists(s):
            sys.exit(f'{i["name"]}: source {s} not found')
    if os.path.exists(cache):
        if not have:
            sys.exit(f'{cache} exists and was not built by this harness: move it, or name it without `build`')
        shutil.rmtree(cache)        # ours (the stamp says so), built from other sources
    cmd = [sys.executable, 'tools/importer.py', 'build', '--out', cache]
    for s in i['build']:
        cmd += ['--source', s]
    for t in want['lang']:
        cmd += ['--lang', t]
    for o in want['opt']:
        cmd += ['--opt', o]
    log = os.path.join(logs, f'build_{os.path.basename(cache)}.log')
    if run_tool(cmd, log) != 0:
        sys.exit(f'{i["name"]}: the cache build failed; see {log}')
    stamp_write(st, {'want': want, 'built': time.strftime('%Y-%m-%d %H:%M:%S')})


def copy_tree_if_newer(src, dst):
    n = 0
    for dp, _dn, fns in os.walk(src):
        rel = os.path.relpath(dp, src)
        od = os.path.join(dst, rel) if rel != '.' else dst
        os.makedirs(od, exist_ok=True)
        for f in fns:
            s, d = os.path.join(dp, f), os.path.join(od, f)
            ss = os.stat(s)
            if os.path.exists(d):
                ds = os.stat(d)
                if ds.st_size == ss.st_size and int(ds.st_mtime) == int(ss.st_mtime):
                    continue
            shutil.copy2(s, d)
            n += 1
    return n


PC_SKIP = re.compile(r'^(BISLPS.*|backup-.*|.*\.(zip|cab|ins|lid|ini)|SETUP\..*|START\.EXE|_.*|BOF3\.CFG|'
                     r'bof3x.*|BOF3\.exe\..*)$', re.I)


def prepare_pc(i, cfg):
    """A copy of the PC install: BOF3.exe, DAT\\, SND\\, BGM\\, the movies and the
    small root files - never a save, never the owner's ini."""
    src, game = cfg.vars['pc'], i['game']
    if not os.path.exists(os.path.join(src, 'BOF3.exe')):
        sys.exit(f'{i["name"]}: no BOF3.exe in {src}')
    os.makedirs(game, exist_ok=True)
    n = 0
    for f in os.listdir(src):
        p = os.path.join(src, f)
        if os.path.isdir(p):
            if f.upper() in ('DAT', 'SND', 'BGM'):
                n += copy_tree_if_newer(p, os.path.join(game, f))
            continue
        if PC_SKIP.match(f) and f.lower() != 'bof3.exe':
            continue
        if f.lower() != 'bof3.exe' and not f.lower().endswith('.avi') and os.path.getsize(p) > (1 << 20):
            continue
        d = os.path.join(game, f)
        if os.path.exists(d) and os.path.getsize(d) == os.path.getsize(p) and \
                int(os.path.getmtime(d)) == int(os.path.getmtime(p)):
            continue
        shutil.copy2(p, d)
        n += 1
    print(f'  game {game}: the PC install copied ({n} files new or changed)')


def prepare_base(i, logs):
    game, cache = i['game'], i['cache']
    man = os.path.join(cache, 'manifest.toml')
    built = sha(man)
    st = os.path.join(game, '.abc_install.json')
    want = {'cache': cache, 'manifest': built, 'lang': i.get('lang'), 'opt': i.get('install_opt', [])}
    have = stamp_read(st)
    if have and have.get('want') == want:
        print(f'  game {game}: installed ({have.get("installed")})')
        return
    if os.path.exists(game):
        if not have:
            sys.exit(f'{game} exists and was not installed by this harness: move it, or use kind "use"')
        shutil.rmtree(game)
    cmd = [sys.executable, 'tools/importer.py', 'install', '--base', '--cache', cache, '--game', game]
    if i.get('lang'):
        cmd += ['--lang', i['lang']]
    for o in want['opt']:
        cmd += ['--opt', o]
    log = os.path.join(logs, f'install_{os.path.basename(game)}.log')
    if run_tool(cmd, log) != 0:
        sys.exit(f'{i["name"]}: install --base failed; see {log}')
    stamp_write(st, {'want': want, 'installed': time.strftime('%Y-%m-%d %H:%M:%S')})


def users_of(folder):
    """The processes running from a launcher folder: a launcher or game exe
    under it, or a game whose bof3x.dll is the folder's. [(pid, path)]."""
    want = os.path.join(os.path.normcase(os.path.abspath(folder)), '')
    ps = ("Get-Process -Name BOF3,bof3x,bof3x-* -ErrorAction SilentlyContinue | ForEach-Object { "
          "$m = $_.Modules | Where-Object { $_.ModuleName -eq 'bof3x.dll' } | Select-Object -First 1; "
          "'{0}|{1}|{2}' -f $_.Id, $_.Path, $m.FileName }")
    out = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True, text=True).stdout
    found = []
    for line in out.splitlines():
        pid, _, rest = line.strip().partition('|')
        for path in rest.split('|'):
            if pid.isdigit() and path and os.path.normcase(os.path.abspath(path)).startswith(want):
                found.append((int(pid), path))
                break
    return found


def prepare_launcher(folder, build):
    os.makedirs(folder, exist_ok=True)
    busy = users_of(folder)
    if busy:
        # copying over a file a running game maps failed with WinError 32 part
        # way through the set (2026-10-13): name the holder before touching any
        sys.exit(f'{folder} is in use: ' + '; '.join(f'pid {p} ({path})' for p, path in busy) +
                 '. prepare refreshes this launcher copy and cannot while a game runs from it - let that run '
                 'end (another harness on this root?) or end that pid, then prepare again')
    if os.path.exists(os.path.join(folder, 'bof3x.ini')):
        sys.exit(f'{folder} holds a bof3x.ini: the harness wants an ini-less launcher (the play ini\'s '
                 f'cheat.exp=10 and opt= would reach the runs)')
    n = 0
    for f in LAUNCHER_FILES:
        s, d = os.path.join(build, f), os.path.join(folder, f)
        if not os.path.exists(s):
            sys.exit(f'no {s}: build first (cmake --preset i686 && cmake --build build)')
        if not os.path.exists(d) or sha(s) != sha(d):
            shutil.copy2(s, d)
            n += 1
    print(f'  launcher {folder}: {n} of {len(LAUNCHER_FILES)} files refreshed from {build}')


def prepare(cfg, installs, a):
    logs = os.path.join(cfg.root, 'logs')
    os.makedirs(logs, exist_ok=True)
    launchers = sorted({i['launcher'] for i in installs})
    for l in launchers:
        if os.path.normcase(l).startswith(os.path.normcase(cfg.root)):
            prepare_launcher(l, a.build)
        else:
            for f in LAUNCHER_FILES:
                if not os.path.exists(os.path.join(l, f)):
                    sys.exit(f'launcher folder {l} lacks {f}')
    for i in installs:
        print(f'{i["name"]}: {i.get("what", "")}')
        kind = i.get('kind', 'base')
        if kind != 'use':
            prepare_cache(i, logs)
        if kind == 'pc':
            prepare_pc(i, cfg)
        elif kind == 'base':
            prepare_base(i, logs)
        elif kind == 'use':
            if not os.path.isdir(i['game']):
                sys.exit(f'{i["name"]}: no game folder {i["game"]}')
        else:
            sys.exit(f'{i["name"]}: kind {kind!r} is not pc, base or use')
        if cfg.cfg and os.path.exists(cfg.cfg):
            d = os.path.join(i['game'], 'BOF3.CFG')
            if not os.path.exists(d) or sha(d) != sha(cfg.cfg):
                shutil.copy2(cfg.cfg, d)
                print(f'  BOF3.CFG copied from {cfg.cfg}')
        slots = glob.glob(os.path.join(i['game'], 'BISLPS*.DAT'))
        if slots and kind != 'use':
            print(f'  note: {len(slots)} save slot file(s) in {i["game"]}: a save menu shows them; '
                  f'the other installs may not have the same')
    print('prepare: done')


# --- run ------------------------------------------------------------------------

def route_name(route):
    return 'attract' if route == 'attract' else os.path.splitext(os.path.basename(route))[0]


def recipe_for(route, rdir, frames):
    if route != 'attract':
        p = route if os.path.isabs(route) else os.path.join(ROOT, route)
        if not os.path.exists(p):
            p2 = os.path.join(ROOT, 'tools', 'recipes', route + ('' if route.endswith('.txt') else '.txt'))
            if os.path.exists(p2):
                p = p2
            else:
                sys.exit(f'no recipe {route}')
        return os.path.abspath(p)
    p = os.path.join(rdir, f'attract_{frames}.txt')
    with open(p, 'w', newline='\n') as fh:
        fh.write('# Written by tools/abc_run.py: no input - the attract sequence from recipe frame 0,\n'
                 f'# {frames} frames; the frames come from BOF3X_SHOT_EVERY.\n'
                 f'wait {frames}\n')
    return p


def run_one(cfg, i, recipe, out, a):
    """One install, one route: input_run.py with the install's environment.
    Returns the meta dictionary written beside the frames."""
    if os.path.exists(out):
        shutil.rmtree(out)       # this harness's own run folder
    os.makedirs(out)
    env = dict(os.environ)
    for k in list(env):
        if k.startswith('BOF3X_'):
            del env[k]           # nothing of the caller's shell reaches a run unasked
    env['BOF3X_OPT'] = 'none'
    env['BOF3X_LAYERING'] = '0'
    env['BOF3X_CACHE'] = i['cache']
    if i.get('exeimage'):
        env['BOF3X_EXEIMAGE'] = i['cache']
    env['BOF3X_SHOT_EVERY'] = str(a.every)
    env['BOF3X_RECIPE_SAVES'] = cfg.recipe_saves
    for k, v in i['env'].items():
        env[k] = str(v)
    if a.statehash:
        env['BOF3X_STATEHASH'] = os.path.join(out, 'run.sh')
        if a.statehash_skip:
            env['BOF3X_STATEHASH_SKIP'] = os.path.abspath(a.statehash_skip)
    launcher = os.path.join(i['launcher'], 'bof3x-launcher.exe')
    cmd = [sys.executable, os.path.join(ROOT, 'tools', 'input_run.py'), recipe, '--out', out, '--no-front',
           '--launcher', launcher, '--game', i['game'], '--minutes', str(a.minutes), '--slot0-hold'] + a.save_args
    if i.get('lang'):
        cmd += ['--lang', i['lang']]
    if a.speed != 1:
        cmd += ['--speed', str(a.speed)]
    if a.wide is not None:
        cmd += ['--wide', a.wide]
        env['BOF3X_WIDE'] = a.wide   # input_run sets it too; here so the run's meta records it
    print(f'{i["name"]}: {route_name(a.route)} -> {out}')
    runlog = os.path.join(out, '_run.txt')
    for attempt in (1, 2):
        t0 = time.time()
        with open(runlog, 'w', encoding='utf-8') as fh:
            r = subprocess.run(cmd, cwd=ROOT, env=env, stdout=fh, stderr=subprocess.STDOUT)
        wall = time.time() - t0
        # input_run asks tasklist whether the game lives; when process
        # creation fails (2026-10-12, two harnesses' games at once: the
        # runner itself exited 0xC0000142) it reads "exited" and leaves the
        # game up. Whatever the runner said, no game of this launcher folder
        # outlives its run - ended by pid (attract_run.kill_stale).
        stale = end_stale(launcher)
        text = open(runlog, encoding='utf-8', errors='replace').read()
        if (r.returncode & 0xFFFFFFFF) == 0xC0000142 or (stale and 'end: game exited' in text):
            print(f'  attempt {attempt}: the runner could not start or follow the game '
                  f'(exit 0x{r.returncode & 0xFFFFFFFF:X}{", a game left running and ended" if stale else ""})')
            if attempt == 1:
                restore_slot0(i['game'])
                for f in glob.glob(os.path.join(out, '*')):
                    if os.path.basename(f) != '_run.txt':
                        os.remove(f)
                time.sleep(10)
                continue
        break
    log = os.path.join(i['launcher'], 'bof3x.log')
    if os.path.exists(log):
        shutil.copy2(log, os.path.join(out, 'bof3x.log'))
    text = open(runlog, encoding='utf-8', errors='replace').read()
    end = re.findall(r'^end: (.*)$', text, re.M)
    ended = re.findall(r'recipe (done|FAILED) at recipe frame (\d+)', text)
    # the timed frames are BMPs the game wrote; PNGs from here on
    n = 0
    for b in sorted(glob.glob(os.path.join(out, 't*.bmp'))):
        from PIL import Image
        with Image.open(b) as im:
            im.convert('RGB').save(os.path.splitext(b)[0] + '.png')
        os.remove(b)
        n += 1
    shots = len(glob.glob(os.path.join(out, '*.png'))) - n
    meta = {'install': i['name'], 'what': i.get('what', ''), 'traits': i['traits'], 'cache': i['cache'],
            'game': i['game'], 'lang': i.get('lang'), 'route': route_name(a.route), 'recipe': recipe,
            'every': a.every, 'exit': r.returncode, 'status': end[-1] if end else 'no end line',
            'end_frame': int(ended[-1][1]) if ended else None, 'wall_s': round(wall, 1),
            'timed_frames': n, 'shots': shots,
            'env': {k: v for k, v in env.items() if k.startswith('BOF3X_')},
            'when': time.strftime('%Y-%m-%d %H:%M:%S'), 'head': git_head()}
    with open(os.path.join(out, 'meta.json'), 'w', encoding='utf-8') as fh:
        json.dump(meta, fh, indent=1)
    print(f'  {meta["status"]}, exit {r.returncode}, {wall:.0f} s, {n} timed frames, {shots} shots')
    if 'FATAL' in text or r.returncode != 0:
        for line in text.splitlines():
            if 'FATAL' in line or line.startswith('end:') or 'launcher failed' in line:
                print('  ' + line)
    return meta


def end_stale(launcher):
    """End any game whose bof3x.dll is this launcher folder's, by pid; True
    when there was one."""
    import attract_run
    want = os.path.normcase(os.path.dirname(os.path.abspath(launcher)))
    ps = ("Get-Process -Name BOF3,bof3x,bof3x-* -ErrorAction SilentlyContinue | ForEach-Object { "
          "$m = $_.Modules | Where-Object { $_.ModuleName -eq 'bof3x.dll' } | Select-Object -First 1; "
          "if ($m) { '{0}|{1}' -f $_.Id, $m.FileName } }")
    out = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True, text=True).stdout
    found = any(os.path.normcase(os.path.dirname(l.partition('|')[2].strip())) == want
                for l in out.splitlines() if '|' in l)
    if found:
        attract_run.kill_stale(launcher)
    return found


def restore_slot0(game):
    """A run cut short leaves the recipe save in slot 0 and a lock: put back
    what was there (recipe_saves.py restore)."""
    if os.path.isdir(os.path.join(game, '.recipe_slot0')):
        subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'recipe_saves.py'), 'restore', '--game', game],
                       cwd=ROOT, capture_output=True)


def git_head():
    try:
        return subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, capture_output=True,
                              text=True).stdout.strip()
    except OSError:
        return ''


def save_args(cfg, recipe, a):
    """The input_run.py arguments that decide what slot 0 holds, the same on
    every side. A recipe with no `# save NAME` header loads whatever each
    install's own slot 0 holds, so the sides can start from different saves
    (2026-10-13, fairyVillage): refused unless --save names one or --no-save
    says the recipe loads none."""
    import recipe_saves
    if a.save and a.no_save:
        sys.exit('--save and --no-save together: pick one')
    if a.no_save:
        print("slot 0: --no-save, each install's own slot 0 stays (the recipe must load no save)")
        return ['--no-save']
    name = a.save
    if not name:
        with open(recipe, encoding='utf-8', errors='replace') as fh:
            heads = [m for m in map(recipe_saves.HEADER.match, fh) if m]
        if heads:
            name = heads[0][1]
        elif a.route == 'attract':
            return []                       # the harness's own attract recipe: no input, no load
        else:
            sys.exit(f'{recipe} has no `# save NAME` header: each install would load its own slot 0, so the '
                     f'sides could start from different saves. Fix: add `# save NAME` to the recipe (the save '
                     f'is tools/recipe_saves/NAME.DAT; `python tools/recipe_saves.py list`), or pass '
                     f'--save NAME, or --no-save if the recipe loads no save (title-only, NEW GAME)')
    file = name if name.upper().endswith('.DAT') else name + '.DAT'
    dirs = [cfg.recipe_saves, recipe_saves.DIR]
    if not any(os.path.isfile(os.path.join(d, file)) for d in dirs):
        sys.exit(f'no recipe save {file} in ' + ' or '.join(dirs) +
                 ' (`python tools/recipe_saves.py import SLOT NAME` makes one)')
    print(f'slot 0: {file} on every side' + (' (--save)' if a.save else " (the recipe's header)"))
    return ['--save', name] if a.save else []


def run(cfg, installs, a):
    name = route_name(a.route)
    rdir = os.path.join(cfg.root, 'runs', f'{a.tag or a.set}_{name}')
    os.makedirs(rdir, exist_ok=True)
    recipe = recipe_for(a.route, rdir, a.frames)
    if getattr(a, 'save_args', None) is None:
        a.save_args = save_args(cfg, recipe, a)
    metas = []
    for i in installs:
        if not os.path.exists(os.path.join(i['launcher'], 'bof3x-launcher.exe')):
            sys.exit(f'{i["launcher"]}: no launcher (Defender may have quarantined it: rebuild, run prepare)')
        metas.append(run_one(cfg, i, recipe, os.path.join(rdir, i['name']), a))
    return metas


# --- compare and report ------------------------------------------------------------

def statehash_pairs(sides, out, known):
    """statehash.py diff per pair (--align recipe), its page lines tagged by
    the known rules' `pages`. Context only."""
    res = {}
    for x in range(len(sides)):
        for y in range(x + 1, len(sides)):
            p, q = sides[x], sides[y]
            a, b = os.path.join(p.path, 'run.sh'), os.path.join(q.path, 'run.sh')
            if not (os.path.exists(a) and os.path.exists(b)):
                continue
            r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'statehash.py'), 'diff', a, b,
                                '--align', 'recipe'], cwd=ROOT, capture_output=True, text=True)
            lines = []
            for line in (r.stdout + r.stderr).splitlines():
                m = re.match(r'(0x[0-9A-F]+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(.*)$', line)
                if m:
                    syms = [re.sub(r'\+.*$', '', s.strip('()')) for s in m[5].split() if not s.startswith('+')]
                    tag = abc_known.match_page(known.rules if known else [], p, q, syms)
                    lines.append({'page': m[1], 'first': int(m[2]), 'last': int(m[3]), 'ticks': int(m[4]),
                                  'symbols': m[5], 'known': tag})
                else:
                    lines.append({'text': line})
            pn = f'{p.key}-{q.key}'
            with open(os.path.join(out, f'statehash_{pn}.txt'), 'w', encoding='utf-8') as fh:
                fh.write(r.stdout + r.stderr)
            res[pn] = lines
    return res


def compare(cfg, installs, a):
    name = route_name(a.route)
    rdir = os.path.join(cfg.root, 'runs', f'{a.tag or a.set}_{name}')
    keys = 'ABCDEFGH'
    sides = []
    for k, i in zip(keys, installs):
        d = os.path.join(rdir, i['name'])
        if not os.path.isdir(d):
            sys.exit(f'no run folder {d}: run first')
        sides.append(abc_compare.Side(k, d))
    known = abc_known.Known(a.known) if os.path.exists(a.known) else None
    out = os.path.join(rdir, 'compare')
    if os.path.exists(out):
        shutil.rmtree(out)
    result = abc_compare.compare(sides, out, known, name, a.tol, a.min_pixels, a.sheet_rows)
    sh = statehash_pairs(sides, out, known) if any(os.path.exists(os.path.join(s.path, 'run.sh'))
                                                   for s in sides) else {}
    report = os.path.join(cfg.root, f'report_{a.tag or a.set}_{name}.md')
    write_report(report, result, sides, known, sh, rdir, a)
    print(f'report: {report}')
    return report


def spans(result, pn, pick):
    """Runs of consecutive frames (in tick order) for which pick(row, pair) is
    true and the pair's scene stays the same: lists of rows."""
    out, cur = [], []
    keys = pn.split('-')
    for row in result['rows']:
        r = row['pairs'].get(pn, {})
        if pick(row, r):
            sc = tuple((k, tuple(row['scene'][k]) if row['scene'].get(k) else None) for k in keys)
            if cur and cur[-1][1] == sc:
                cur.append((row, sc))
                continue
            if cur:
                out.append([x[0] for x in cur])
            cur = [(row, sc)]
        elif cur:
            out.append([x[0] for x in cur])
            cur = []
    if cur:
        out.append([x[0] for x in cur])
    return out


def scene_text(row, keys):
    sc = {k: row['scene'].get(k) for k in keys}
    vals = {tuple(v) if v else None for v in sc.values()}
    if len(vals) == 1:
        v = vals.pop()
        return f'mode {v[0]} area {v[1]}' if v else 'scene not logged'
    return ', '.join(f'{k}: mode {v[0]} area {v[1]}' if v else f'{k}: -' for k, v in sc.items())


def tick_of(row):
    return row['tick'] if row['tick'] is not None else row['frame']


def write_report(path, result, sides, known, sh, rdir, a):
    L = []
    w = L.append
    name = result['route']
    w(f'# A/B/C: {name}, set `{a.set}`')
    w('')
    w(f'Written by `tools/abc_run.py` ({time.strftime("%Y-%m-%d %H:%M")}); the method is '
      f'`docs/abc-harness.md`. Frames: `{os.path.relpath(rdir, os.path.dirname(path)).replace(os.sep, "/")}/`; '
      f'the index: `{os.path.relpath(os.path.join(rdir, "compare", "index.html"), os.path.dirname(path)).replace(os.sep, "/")}`.')
    w('')
    w('| Side | Install | Frames | Run | Wall |')
    w('|---|---|--:|---|--:|')
    for s in sides:
        m = s.meta
        w(f'| {s.key} | `{s.name}` - {m.get("what", "")} | {len(s.frames)} | {m.get("status", "?")}'
          f'{" at frame " + str(m["end_frame"]) if m.get("end_frame") is not None else ""} | {m.get("wall_s", "?")} s |')
    w('')
    w(f'A pixel differs when a channel moves by more than {result["tol"]} (of 255); a frame differs past '
      f'{result["min_pixels"]} such pixels. Regions are in game pixels (320 x 240). The comparison took '
      f'{result["seconds"]} s.')
    w('')
    w('| Pair | Compared | Differ | Unexplained | Missing | First differs | First unexplained | Scene parts |')
    w('|---|--:|--:|--:|--:|--:|--:|--:|')
    for pn, s in result['summary'].items():
        w(f'| {pn} | {s["compared"]} | {s["differ"]} | {s["unexplained"]} | {s["missing"]} | '
          f'{s["first_diff"] if s["first_diff"] is not None else "-"} | '
          f'{s["first_unexplained"] if s["first_unexplained"] is not None else "-"} | '
          f'{s["first_scene_split"] if s["first_scene_split"] is not None else "-"} |')
    w('')
    sheet_of = {}
    for n, row in enumerate(result['rows']):
        sheet_of[row['frame']] = result['sheets'][n // a.sheet_rows] if result['sheets'] else ''

    w('## 1. Unexplained differences')
    w('')
    w('Spans of consecutive frames in one scene where some region matched no known difference. '
      'The largest regions of the span\'s worst frame are listed (x0,y0-x1,y1 in game pixels, '
      'differing pixels at the frame\'s own size).')
    w('')
    keys = [s.key for s in sides]
    any_un = False
    for pn in result['pairs']:
        sp = spans(result, pn, lambda row, r: bool(r.get('unexplained')))
        if not sp:
            continue
        any_un = True
        w(f'### {pn}: {len(sp)} span(s)')
        w('')
        for rows in sp:
            worst = max(rows, key=lambda row: row['pairs'][pn]['changed'])
            r = worst['pairs'][pn]
            regs = ', '.join(f'{x["box"][0]},{x["box"][1]}-{x["box"][2]},{x["box"][3]} ({x["pixels"]} px'
                             + (f', **moved {x["moved"][0]:+d},{x["moved"][1]:+d}**' if x.get('moved') else '')
                             + ')' for x in r['unexplained'][:4])
            more = f' and {len(r["unexplained"]) - 4} more' if len(r['unexplained']) > 4 else ''
            span = f'{tick_of(rows[0])}' + (f'..{tick_of(rows[-1])}' if len(rows) > 1 else '')
            size = f' **sizes differ** {r["size"][0]} / {r["size"][1]};' if 'size' in r else ''
            w(f'- **{span}** ({len(rows)} frame{"s" if len(rows) > 1 else ""}, {scene_text(worst, [pn[0], pn[2]])}):'
              f'{size} worst `{worst["frame"]}` {100 * r["changed"]:.2f}% of pixels, mean |d| {r["mad"]:.2f}; '
              f'regions {regs}{more}; heat `compare/{r["heat"].replace(os.sep, "/")}`, sheet `compare/{sheet_of.get(worst["frame"], "")}`')
        w('')
    if not any_un:
        w('None.')
        w('')

    w('## 2. Where the runs part')
    w('')
    lines = []
    for pn, s in result['summary'].items():
        if s['first_scene_split'] is not None:
            row = next(r for r in result['rows'] if r['tick'] == s['first_scene_split'])
            lines.append(f'- {pn}: the scene differs from tick {s["first_scene_split"]} '
                         f'({scene_text(row, [pn[0], pn[2]])}); frames after it are of different moments, so '
                         f'their differences are the parting, not the data')
        if s['missing']:
            lines.append(f'- {pn}: {s["missing"]} frame(s) on one side only (a run that ended early or '
                         f'took a shot the other did not)')
    for s in sides:
        m = s.meta
        if m and (m.get('exit') != 0 or m.get('status') != 'done'):
            lines.append(f'- {s.key} `{s.name}`: the run ended `{m.get("status")}` (exit {m.get("exit")}), '
                         f'recipe frame {m.get("end_frame")}')
    L += lines or ['None: every pair stayed in the same scene, and every run finished.']
    w('')

    w('## 3. Explained differences')
    w('')
    by = {}
    for row in result['rows']:
        for pn, r in row['pairs'].items():
            for x in r.get('regions', []):
                if x.get('known'):
                    by.setdefault((x['known'], pn), []).append(tick_of(row))
    rules = {r['id']: r for r in (known.rules if known else [])}
    if by:
        w('| Known | Ledger | Pair | Frames | Ticks |')
        w('|---|---|---|--:|---|')
        for (kid, pn), ts in sorted(by.items()):
            ts = sorted(set(ts), key=lambda t: (isinstance(t, str), t))
            w(f'| {kid} | {rules.get(kid, {}).get("ledger", "")} | {pn} | {len(ts)} | {ts[0]}..{ts[-1]} |')
    else:
        w('None matched.')
    w('')

    w('## 4. Equal')
    w('')
    for pn, s in result['summary'].items():
        w(f'- {pn}: {s["compared"] - s["differ"]} of {s["compared"]} frames equal')
    w('')

    if sh:
        w('## 5. State hash (context; `--statehash`)')
        w('')
        w('`statehash.py diff --align recipe` per pair; pages a known rule names are tagged. The full '
          'output is `compare/statehash_<pair>.txt`.')
        w('')
        for pn, lines in sh.items():
            pages = [x for x in lines if 'page' in x]
            un = [x for x in pages if not x['known']]
            w(f'### {pn}: {len(pages)} pages differ, {len(un)} untagged')
            w('')
            for x in lines:
                if 'text' in x and x['text'].startswith('diff:'):
                    w(x['text'])
                    w('')
            for x in (un + [x for x in pages if x['known']])[:40]:
                w(f'- `{x["page"]}` ticks {x["first"]}..{x["last"]} ({x["ticks"]}): {x["symbols"]}'
                  + (f' - known `{x["known"]}`' if x['known'] else ''))
            w('')

    w('## What this cannot see')
    w('')
    w('- Sound and music (no audio is captured); the logo movies, which play before recipe frame 0.')
    w('- Frames between the timed ones (every ' + str(a.every) + ' recipe frames) and the recipe\'s shots: '
      'a fault shown for less than that may fall between two.')
    w('- What a route never reaches; text the route never opens.')
    w('- A difference inside a region a known rule matched: rules match by scene and box, coarsely, not '
      'by what is drawn there.')
    w('- Two sides that are both wrong the same way: the pair agrees.')
    w('')
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(L) + '\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('cmd', choices=('prepare', 'run', 'compare', 'all'))
    ap.add_argument('--root', default=os.environ.get('BOF3X_ABC_ROOT', os.path.join(os.path.expanduser('~'), 'abc')),
                    help='where caches, installs, runs and reports go (default ~/abc, or BOF3X_ABC_ROOT); keep it short')
    ap.add_argument('--installs', default=INSTALLS)
    ap.add_argument('--known', default=KNOWN)
    ap.add_argument('--set', default='eu')
    ap.add_argument('--only', help='comma-separated install names instead of the set\'s')
    ap.add_argument('--tag', help='name the run folder and report by this instead of the set (a repeat, a subset)')
    ap.add_argument('--route', default='attract', help='"attract", or a recipe (a path, or a name in tools/recipes)')
    ap.add_argument('--save', metavar='NAME', help="the recipe save every side loads (tools/recipe_saves/NAME.DAT), "
                    "in place of the recipe's `# save NAME` header; a recipe with neither is refused")
    ap.add_argument('--no-save', action='store_true', help="the recipe loads no save (title-only, NEW GAME): run it "
                    "with no header, each install's slot 0 left as it is")
    ap.add_argument('--every', type=int, default=EVERY, help=f'a timed frame every N recipe frames (default {EVERY})')
    ap.add_argument('--frames', type=int, default=ATTRACT_FRAMES, help='the attract route\'s length in recipe frames')
    ap.add_argument('--minutes', type=float, default=15.0, help='per run, before input_run gives up')
    ap.add_argument('--speed', type=int, default=1, help='BOF3X_SPEED (frames are still drawn where taken); default 1')
    ap.add_argument('--wide', choices=('0', '1', '2'), default=None,
                    help='BOF3X_WIDE for every run (input_run.py --wide): 0 320 x 240, 1 16:9, 2 21:9; unset, each '
                         'install\'s launcher ini decides, as before')
    ap.add_argument('--statehash', action='store_true', help='also record the state hash and diff it per pair (context)')
    ap.add_argument('--statehash-skip', help='BOF3X_STATEHASH_SKIP for every run (e.g. a union of exe_tables.py gate-skip lists)')
    ap.add_argument('--build', default=bof3x_home.launcher_dir(), help='where prepare takes the launcher, DLL and exe from')
    ap.add_argument('--tol', type=int, default=abc_compare.TOL)
    ap.add_argument('--min-pixels', type=int, default=abc_compare.MIN_PIXELS)
    ap.add_argument('--sheet-rows', type=int, default=16)
    a = ap.parse_args()
    cfg = Config(a.installs, a.root)
    installs = cfg.members(a.set, a.only)
    os.makedirs(cfg.root, exist_ok=True)
    t0 = time.time()
    if a.cmd in ('run', 'all') and a.route != 'attract':
        a.save_args = save_args(cfg, recipe_for(a.route, None, a.frames), a)   # before `all` prepares
    if a.cmd in ('prepare', 'all'):
        prepare(cfg, installs, a)
    if a.cmd in ('run', 'all'):
        run(cfg, installs, a)
    if a.cmd in ('compare', 'run', 'all'):
        compare(cfg, installs, a)
    print(f'{a.cmd}: {time.time() - t0:.0f} s')


if __name__ == '__main__':
    main()
