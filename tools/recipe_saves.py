#!/usr/bin/env python
"""Recipe saves: the save a recipe loads lives in tools/recipe_saves/, not in
the game's slots. For the load it is put into slot 0, and the owner's slot 0
comes back as soon as the game has read it - so every recipe starts by
opening slot 0, and the game's sixteen slots stay the owner's to play with.

A recipe names its save in a header comment, which the game's recipe parser
ignores (src/hook/input_script.cpp; docs/input-script.md section 1a):

    # save adult_ryu            -> tools/recipe_saves/adult_ryu.DAT

input_run.py does the swap (`Slot0`); this file is also a small command line:

    python tools/recipe_saves.py list                # the saves and who uses them
    python tools/recipe_saves.py import 5 adult_ryu  # bof3/BISLPS05.DAT -> tools/recipe_saves/adult_ryu.DAT
    python tools/recipe_saves.py restore             # put slot 0 back after a run that died

The swap: the owner's BISLPS00.DAT is renamed into <game>/.recipe_slot0/ (a
subdirectory - a file beside the saves whose 8.3 alias matched BISLPS??.DAT
would reach Save_ListFiles, and that table is unbounded, docs/save-files.md
section 2), the recipe save is copied in its place, and the reverse happens
when the run says so - input_run.py does it on the DLL's `save loaded` log
line (LoadMenu_Read, src/game/save_menu.cpp), since a loaded game reads the
file no more - or at the latest when the game has exited. A lock file beside
the backup names the run that holds slot 0; another run waits for it, and a
run that finds a lock whose process is gone puts the backup back first. The
lock is beside the game, not the checkout, so worktrees sharing one game
directory share it. Whatever the recipe wrote to slot 0 before the hand-back
is discarded with the copy: the recipe save is never changed by a run.

The player's game is not swapped under: any BOF3.exe that does not carry the
unattended marker (an event `Local\\bof3x_unattended_<pid>`, made by
src/hook/dllmain.cpp when BOF3X_INPUT or BOF3X_SELFTEST_ONLY is set) refuses
the swap. Scripted games and headless self-tests never wait on each other.

Save files are game data (CLAUDE.md rule 1): tools/recipe_saves/*.DAT is
gitignored, and this tool never prints their contents.
"""
import argparse, ctypes, json, os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, 'tools', 'recipe_saves')
RECIPES = os.path.join(ROOT, 'tools', 'recipes')
SLOT0 = 'BISLPS00.DAT'
LOCK_DIR = '.recipe_slot0'                          # under the game directory
HEADER = re.compile(r'^#\s*save\s+(\S+)\s*$')     # exactly `# save NAME`; `# Save 5 (adult Ryu)` is prose
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenEventW.restype = ctypes.c_void_p
k32.OpenEventW.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_wchar_p]
k32.OpenProcess.restype = ctypes.c_void_p
k32.CloseHandle.argtypes = [ctypes.c_void_p]
k32.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]


def recipe_save(recipe, game=None):
    """The save a recipe's `# save NAME` header names, or None when the recipe
    loads no save (title-only recipes, NEW GAME). The extension may be given
    or not."""
    with open(recipe) as f:
        for line in f:
            m = HEADER.match(line)
            if m:
                return save_path(m[1], game)
    return None


def save_dirs(game=None):
    """Where recipe saves are looked for: BOF3X_RECIPE_SAVES, this checkout's
    tools/recipe_saves, and - for a worktree run against the main checkout's
    game directory - the tools/recipe_saves beside that game directory."""
    out = []
    if os.environ.get('BOF3X_RECIPE_SAVES'):
        out.append(os.environ['BOF3X_RECIPE_SAVES'])
    out.append(DIR)
    if game:
        out.append(os.path.join(os.path.dirname(os.path.abspath(game)), 'tools', 'recipe_saves'))
    return out


def save_path(name, game=None):
    """The first existing NAME.DAT among save_dirs, else the path it would
    have in this checkout (so the error names it)."""
    if not name.upper().endswith('.DAT'):
        name += '.DAT'
    for d in save_dirs(game):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    return os.path.join(DIR, name)


def pid_alive(pid):
    """True while the process exists. Not os.kill(pid, 0): on Windows that
    terminates the process."""
    h = k32.OpenProcess(0x1000, False, int(pid))         # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return False
    code = ctypes.c_ulong()
    ok = k32.GetExitCodeProcess(h, ctypes.byref(code))
    k32.CloseHandle(h)
    return bool(ok) and code.value == 259                # STILL_ACTIVE


def unattended(pid):
    """True for a BOF3.exe that marked itself scripted or headless (dllmain.cpp)."""
    h = k32.OpenEventW(0x100000, False, 'Local\\bof3x_unattended_%d' % pid)   # SYNCHRONIZE
    if h:
        k32.CloseHandle(h)
    return bool(h)


def games_running():
    """Pids of every BOF3.exe on the machine, whoever started it."""
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq BOF3.exe', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    return [int(row.split('","')[1]) for row in out.splitlines() if row.startswith('"BOF3.exe"')]


def players_games():
    """The BOF3.exe processes that are somebody's game, not a scripted run."""
    return [p for p in games_running() if not unattended(p)]


class Paths:
    def __init__(self, game):
        self.game = os.path.abspath(game)
        self.slot0 = os.path.join(self.game, SLOT0)
        self.dir = os.path.join(self.game, LOCK_DIR)
        self.backup = os.path.join(self.dir, SLOT0)
        self.lock = os.path.join(self.dir, 'lock.json')

    def read_lock(self):
        try:
            with open(self.lock) as f:
                return json.load(f)
        except (OSError, ValueError):
            return None


def restore(game, quiet=False):
    """Undo a swap: remove the recipe copy from slot 0 and put the backup back.
    Returns True when there was anything to undo."""
    P = Paths(game)
    lock = P.read_lock()
    if lock is None and not os.path.exists(P.backup):
        if not quiet:
            print('slot 0: nothing to restore')
        return False
    if os.path.exists(P.slot0):
        os.remove(P.slot0)                               # the recipe's copy, whatever the run did to it
    if os.path.exists(P.backup):
        shutil.move(P.backup, P.slot0)
        what = 'put back'
    else:
        what = 'left empty, as it was' if lock and not lock.get('had_slot0') else 'left empty'
    if os.path.exists(P.lock):
        os.remove(P.lock)
    if not quiet:
        print(f'slot 0: {what} ({P.slot0})')
    return True


class Slot0:
    """`with Slot0(game_dir, save_path) as s:` - the recipe save is slot 0
    from the block's start until `s.release()` or, failing that, the block's
    end. Waits up to `wait` seconds for another run's lock; refuses while a
    game that is not a scripted run is up (it read its slot list at start-up
    and may open a save menu on the file) unless `shared` is set."""

    def __init__(self, game, save, shared=False, wait=300, recipe=None):
        self.P, self.save, self.shared, self.wait = Paths(game), os.path.abspath(save), shared, wait
        self.recipe, self.held = recipe, False

    def __enter__(self):
        P = self.P
        if not os.path.isfile(self.save):
            sys.exit(f'no recipe save {self.save}\n'
                     f'  make one: python tools/recipe_saves.py import SLOT {os.path.basename(self.save)[:-4]}')
        t0, told = time.time(), False
        while True:
            lock = P.read_lock()
            if lock and lock.get('pid') and lock['pid'] != os.getpid() and pid_alive(lock['pid']):
                if time.time() - t0 > self.wait:
                    sys.exit(f'slot 0: still held by pid {lock["pid"]} ({lock.get("recipe", "?")}) after '
                             f'{self.wait:.0f} s; giving up')
                if not told:
                    print(f'slot 0: held by another run (pid {lock["pid"]}, {lock.get("recipe", "?")}); '
                          f'waiting for its load')
                    told = True
                time.sleep(1)
                continue
            if lock:
                print(f'slot 0: a run (pid {lock.get("pid")}) died holding it; putting the backup back first')
                restore(P.game)
            elif os.path.exists(P.backup):
                print(f'slot 0: a backup with no lock at {P.backup}; putting it back first')
                restore(P.game)
            break
        players = players_games()
        if players and not self.shared:
            sys.exit(f'slot 0: a game that is not a scripted run is up (BOF3.exe pid '
                     f'{", ".join(map(str, players))}); slot 0 is not swapped under a player. Close it, or '
                     f'--slot0-shared if that game never opens a save or load menu')
        os.makedirs(P.dir, exist_ok=True)
        had = os.path.exists(P.slot0)
        with open(P.lock, 'w') as f:
            json.dump({'pid': os.getpid(), 'game': P.game, 'had_slot0': had, 'save': self.save,
                       'recipe': self.recipe and os.path.basename(self.recipe),
                       'time': time.strftime('%Y-%m-%d %H:%M:%S')}, f, indent=1)
        if had:
            shutil.move(P.slot0, P.backup)
        shutil.copyfile(self.save, P.slot0)
        self.held = True
        print(f'slot 0: {os.path.relpath(self.save, ROOT)} in, the owner\'s save '
              f'{"backed up" if had else "was absent"}')
        return self

    def release(self, why=''):
        """Hand slot 0 back now - the game has loaded the save and will not
        read the file again. Idempotent."""
        if not self.held:
            return
        lock = self.P.read_lock()
        if lock and lock.get('pid') not in (None, os.getpid()):
            print(f'slot 0: the lock is pid {lock["pid"]}\'s now, not ours; leaving it')
            self.held = False
            return
        restore(self.P.game, quiet=True)
        self.held = False
        print(f'slot 0: the owner\'s save is back{" - " + why if why else ""}')

    def __exit__(self, *exc):
        self.release('at the end of the run')
        return False


def users(name):
    """The recipes whose header names this save."""
    stem = os.path.splitext(name)[0].lower()
    out = []
    for fn in sorted(os.listdir(RECIPES)):
        if fn.endswith('.txt'):
            p = recipe_save(os.path.join(RECIPES, fn))
            if p and os.path.splitext(os.path.basename(p))[0].lower() == stem:
                out.append(fn)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('list', help='the recipe saves and the recipes that load them')
    p.add_argument('--game', default=os.path.join(ROOT, 'bof3'))
    p = sub.add_parser('import', help="copy one of the game's slots into tools/recipe_saves/NAME.DAT")
    p.add_argument('slot', help='hex digit 0-F')
    p.add_argument('name')
    p.add_argument('--game', default=os.path.join(ROOT, 'bof3'))
    p.add_argument('--force', action='store_true', help='overwrite an existing recipe save')
    p = sub.add_parser('restore', help='put the owner\'s slot 0 back after a run that died holding it')
    p.add_argument('--game', default=os.path.join(ROOT, 'bof3'))
    a = ap.parse_args()

    if a.cmd == 'list':
        saves = sorted(f for f in os.listdir(DIR) if f.upper().endswith('.DAT')) if os.path.isdir(DIR) else []
        if not saves:
            print(f'no saves in {DIR}')
        for s in saves:
            st = os.stat(os.path.join(DIR, s))
            print(f'{s:24} {st.st_size:6} bytes  {time.strftime("%Y-%m-%d %H:%M", time.localtime(st.st_mtime))}  '
                  f'<- {", ".join(users(s)) or "no recipe"}')
        for fn in sorted(os.listdir(RECIPES)):
            if fn.endswith('.txt'):
                p = recipe_save(os.path.join(RECIPES, fn), a.game)
                if p and not os.path.exists(p):
                    print(f'MISSING {os.path.relpath(p, ROOT)} <- {fn}')
        lock = Paths(a.game).read_lock()
        if lock:
            alive = lock.get('pid') and pid_alive(lock['pid'])
            print(f'slot 0 swapped by pid {lock.get("pid")} ({lock.get("recipe", "?")}; '
                  f'{"running" if alive else "gone - `restore`"}) since {lock.get("time")}')
        games = games_running()
        if games:
            print('BOF3.exe running: ' + ', '.join(f'{p} ({"scripted" if unattended(p) else "a player"})'
                                                  for p in games))
    elif a.cmd == 'import':
        try:
            slot = int(a.slot, 16)
            assert 0 <= slot <= 15
        except (ValueError, AssertionError):
            sys.exit(f'slot must be one hex digit, not {a.slot}')
        src = os.path.join(a.game, f'BISLPS{slot:02X}.DAT')
        if not os.path.isfile(src):
            sys.exit(f'no save in slot {slot:X}: {src}')
        if slot == 0 and Paths(a.game).read_lock():
            sys.exit('slot 0 holds a recipe save right now (a run is on, or died - `restore`); not importing it')
        dst = os.path.join(DIR, a.name if a.name.upper().endswith('.DAT') else a.name + '.DAT')
        if os.path.exists(dst) and not a.force:
            sys.exit(f'{os.path.relpath(dst, ROOT)} exists; --force to overwrite')
        os.makedirs(DIR, exist_ok=True)
        shutil.copyfile(src, dst)
        print(f'slot {slot:X} -> {os.path.relpath(dst, ROOT)} ({os.path.getsize(dst)} bytes)')
    elif a.cmd == 'restore':
        restore(a.game)


if __name__ == '__main__':
    main()
