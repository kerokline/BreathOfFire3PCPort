#!/usr/bin/env python
"""Launch the game with an input recipe and capture the window at its `shot`s.

    python tools/input_run.py recipes/config_screen.txt --out analysis/shots/config
    python tools/input_run.py recipes/config_screen.txt --out X --lang en-US --original "*"
    python tools/input_run.py recipes/combat.txt --out X --no-front --wav analysis/wav   (the audio mix too)

The recipe is played inside the game by bof3x.dll (BOF3X_INPUT,
src/hook/input_script.cpp; the language is in docs/input-script.md), counted
in the game's own frames, so a run is repeatable from launch the way the
attract sequence is. This script only launches, keeps the window in the
foreground (the port freezes when it is not - docs/windowed-mode.md), watches
build/bof3x.log for `input       shot NAME` lines, and saves the game's client
area as OUT/NAME.png a short moment after each. It sets BOF3X_SHOT_WAIT, so
the game freezes at each shot - clock and all - until the grab is done and
this script releases it: a shot is one exact frame, the same on every run
(docs/input-script.md section 3). It ends the game when the recipe says done
or FAILED, or after --minutes.

While a recipe plays, the real keyboard is ignored - but this script, like
attract_run.py, pulls the game to the front every half second, so leave the
machine alone for the length of the run.

A recipe that loads a save names it in a `# save NAME` header line, and the
file lives in tools/recipe_saves/NAME.DAT, not in a game slot: for the run it
is put into slot 0 and the owner's slot 0 is backed up, then put back as soon
as the game logs the load - the file is not read again - or when the game has
exited (recipe_saves.py). So every recipe opens slot 0, and the game's slots
stay free to play in. Another run waits for the hand-back; a game that is not
a scripted run (no BOF3X_INPUT) refuses the swap.

Needs the game windowed (BOF3.CFG first line 0, or the launcher dialog).

New Game's name and options screens (the New Game round) are turned off for
every run - NewGameNameEntry and NewGameOptions added to BOF3X_OFF, a
caller's names or the launcher ini's off= kept - so a route that presses New
Game plays the port's New Game. A recipe about the screens says so with a
`# screens new-game` header line (or --new-game-screens). A build that does
not know the names gets nothing added (it would stop at start on them).

A `shot NAME 1 [BUTTONS]` line IS one frame of the route: it stands in place
of a frame, holding that frame's buttons. To add shots to a recorded recipe
use tools/recipe_shots.py, which keeps the total. Never insert shot lines by
hand or with an ad-hoc splitter without taking a frame out of the run they
split: every later input then lands a frame late per shot, the replay drifts
(a cast misses its fish, a walk misses its ledge), and it looks like the game
being random. 2026-09-30 cost a day and a withdrawn ledger entry (DIV-0067).
One game at a time, too: two runs share build/bof3x.log and build/bof3x.dll.
Exit status 0 when the recipe finished, 1 when it FAILED or timed out.

--host linux --elf PATH runs the Linux build instead (tools/linux_run.py),
directly on Linux or through `wsl -e` from Windows, with the same variables
and the same output files, every Windows path among them written as the WSL
sees it. No window is found, raised or grabbed: the game writes its frames
(BOF3X_SHOT_DIR, kept as BMPs where Python has no PIL), a frozen shot is
released through the semaphore /bof3x_shot_<pid>, the log is the ELF's path
with .log, and the launcher's and the game's stdout is OUT/_game_output.txt.

    python3 tools/input_run.py tools/recipes/combat.txt --host linux --elf L/bof3x \\
        --out OUT --game ~/bof3x-runs/lcB/bof3 --env BOF3X_STATEHASH=OUT/combat.sh

- **Through the launcher** (the default): bof3x-launcher beside the ELF (or
  --launcher) starts it with --game and --no-config, as the Windows runs'
  launcher does, so the run has the launcher's layer: its defaults with no
  ini, BOF3.CFG, the preset and language lines, BOF3X_LAUNCHER. It returns
  once the game is up; the game is followed by the pid it prints.
  --direct starts the ELF itself instead (no launcher: only the variables set
  here reach the game, and BOF3X_LAUNCHER=1 is set as the launcher would).
- **A game folder made for the run** (--run-game DIR, default OUT/game on
  Linux): links into --game for what the game only reads, real copies of
  the slot files and BOF3.CFG, and the recipe save (`# save NAME`) as slot 0
  (linux_run.make_run_game). --game is never written, and no slot swap or
  lock is needed: the folder is the run's own. The other slots are --game's,
  as they were beside the swapped slot 0 of a Windows run on that install
  (a route that opens the load menu reads every slot's header into .data).
  --run-game none runs on --game itself (then a recipe save needs --no-save).
- **Headless** (the default; --window keeps the caller's display): SDL's
  offscreen video driver and dummy audio driver unless the caller set them,
  and no DISPLAY or WAYLAND_DISPLAY for the game, so a Fatal's message box
  has no screen to open on and the run ends with the game.
- **BOF3X_EXEFILE=1** is set, and said, when the install holds BOF3.exe, no
  cache is named and the caller did not set it: the Windows runners' rule
  (attract_run.developer_exefile).
"""
import argparse, os, re, subprocess, sys, threading, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import language_tags  # noqa: E402  (the retired bare codes, DIV-0005)
import bof3x_home  # noqa: E402  (a release's home; a checkout keeps the repo defaults)
import linux_run  # noqa: E402  (--host linux)

if sys.platform == 'win32':
    import ctypes, ctypes.wintypes as w

    from PIL import ImageGrab

    from attract_run import ROOT, game_pid, kill_game, kill_stale, launch, keep_in_front, log_run_time  # noqa: E402
    import recipe_saves  # noqa: E402

    u = ctypes.WinDLL('user32')
    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    k32.OpenEventW.restype = w.HANDLE
    k32.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
    try:
        ctypes.WinDLL('shcore').SetProcessDpiAwareness(2)   # physical pixels for the grab
    except OSError:
        u.SetProcessDPIAware()
else:
    # attract_run.py and recipe_saves.py reach user32 and kernel32 as they are
    # imported: on another system only --host linux runs, without them.
    ROOT = linux_run.ROOT
    recipe_saves = None
    log_run_time = None

LOG = os.path.join(ROOT, 'build', 'bof3x.log')
SHOT = re.compile(r'input\s+shot (\S+) recipe frame (\d+)( frozen)?')
END = re.compile(r'input\s+(done|FAILED) at recipe frame (\d+)')
LOADED = re.compile(r'save\s+loaded slot ([0-9A-F]+)')     # LoadMenu_Read, src/game/save_menu.cpp

# The New Game round (docs/new-game-round.md M8, section 7 trap 1): New Game
# runs the name screen and the options screen, on by default, so every route
# that presses New Game would change. Every run gets these code names in
# BOF3X_OFF - the port's New Game, frame for frame - unless the recipe is
# about the screens: a `# screens new-game` header line, or --new-game-screens.
NEW_GAME_SCREENS = ('NewGameNameEntry', 'NewGameOptions')
SCREENS_HEADER = re.compile(r'^#\s*screens\s+new-game\s*$')


def recipe_wants_screens(recipe):
    """True when the recipe carries the `# screens new-game` header."""
    with open(recipe, encoding='utf-8', errors='replace') as f:
        return any(SCREENS_HEADER.match(line) for line in f)


def ini_off(launcher):
    """The off= of the launcher folder's bof3x.ini, '' when there is none: an
    environment BOF3X_OFF replaces it whole (src/launcher/config.cpp), so the
    names added here keep it. A gate's launcher folder has no ini."""
    try:
        with open(os.path.join(os.path.dirname(os.path.abspath(launcher)), 'bof3x.ini'),
                  encoding='utf-8', errors='replace') as f:
            for line in f:
                key, eq, value = line.strip().partition('=')
                if eq and key.strip() == 'off':
                    return value.strip()
    except OSError:
        pass
    return ''


def names_known(launcher, names, files=('bof3x.dll', 'bof3x.exe')):
    """The names the launcher folder's game knows as code names: a build from
    before the round has none of them, and BOF3X_OFF stops the game at start
    on a name that is not a code name (src/hook/detour.cpp). Read as the
    NUL-ended string in bof3x.dll or bof3x.exe (code_names.h's table) - or,
    for --host linux, in the ELF (`files` is then its name)."""
    folder = os.path.dirname(os.path.abspath(launcher))
    blobs = []
    for fn in files:
        try:
            with open(os.path.join(folder, fn), 'rb') as f:
                blobs.append(f.read())
        except OSError:
            pass
    return [n for n in names if any(n.encode() + b'\0' in b for b in blobs)]


def screens_off(env, launcher, files=('bof3x.dll', 'bof3x.exe')):
    """Merge NEW_GAME_SCREENS into env['BOF3X_OFF'], keeping a caller's names
    or else the ini's off=. A name the caller already lists, as off or as
    `-Name`, is left as the caller wrote it. Returns what was added."""
    have = env.get('BOF3X_OFF')
    if have is None:
        have = ini_off(launcher)
    listed = [n.strip() for n in have.replace(' ', ',').split(',') if n.strip()]
    bare = {n.lstrip('-') for n in listed}
    add = [n for n in names_known(launcher, NEW_GAME_SCREENS, files) if n not in bare]
    if add:
        env['BOF3X_OFF'] = ','.join(listed + add)
    return add


def game_window(pid):
    found = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
    def cb(hwnd, _):
        p = w.DWORD()
        u.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and u.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True
    u.EnumWindows(cb, 0)
    return found[0] if found else None


def shown(path):
    """path for a log line: relative to the checkout when on its drive (relpath raises across drives)."""
    try:
        return os.path.relpath(path, ROOT)
    except ValueError:
        return path


def grab(path):
    # The game writes the frame itself when BOF3X_SHOT_DIR is set (input_script
    # SaveShot -> render::SaveFrame): the target as drawn, whatever covers the
    # window. Convert that; fall back to the screen only when it is missing.
    bmp = os.path.splitext(path)[0] + '.bmp'
    for _ in range(40):
        if os.path.exists(bmp):
            break
        time.sleep(0.05)
    if os.path.exists(bmp):
        from PIL import Image
        with Image.open(bmp) as im:
            im.convert('RGB').save(path)
            size = im.size
        os.remove(bmp)
        print(f'  saved {shown(path)} ({size[0]}x{size[1]}, from the game)')
        return
    pid = game_pid()
    hwnd = pid and game_window(pid)
    if not hwnd:
        print(f'  no game window for {path}')
        return
    r = w.RECT()
    u.GetClientRect(hwnd, ctypes.byref(r))
    pt = w.POINT(0, 0)
    u.ClientToScreen(hwnd, ctypes.byref(pt))
    ImageGrab.grab(bbox=(pt.x, pt.y, pt.x + r.right, pt.y + r.bottom), all_screens=True).save(path)
    print(f'  saved {shown(path)} ({r.right}x{r.bottom})')


def release(pid):
    """Let a frozen shot go on: the event input_script.cpp made for this pid."""
    h = k32.OpenEventW(0x0002, False, 'Local\\bof3x_shot_%d' % pid)   # EVENT_MODIFY_STATE
    if not h:
        print(f'  no shot event for pid {pid} (error {ctypes.get_last_error()}); the game waits out its timeout')
        return
    k32.SetEvent(h)
    k32.CloseHandle(h)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('recipe')
    ap.add_argument('--out', required=True, help='directory for NAME.png')
    ap.add_argument('--minutes', type=float, default=10.0)
    ap.add_argument('--delay', type=float, default=0.4,
                    help='seconds between a shot line and the grab, for the frame to reach the screen; '
                         'the game is frozen meanwhile, so this only has to outlast the compositor')
    ap.add_argument('--game', default=bof3x_home.default_game(os.path.join(ROOT, 'bof3')))
    ap.add_argument('--lang', default=None, type=lambda v: language_tags.refuse_retired(v, '--lang'),
                    help='BOF3X_LANG, an overlay tag, e.g. en-US (the bare en/fr/de/ja are retired, DIV-0005; '
                         'a recipe recorded under the bare `en` plays under en-US)')
    ap.add_argument('--original', default=None, metavar='LIST', help='BOF3X_ORIGINAL')
    ap.add_argument('--host', choices=('exe', 'bof3', 'linux'), default=None,
                    help='bof3x.exe (exe) or the hosted BOF3.exe (bof3); default bof3 with --original, else exe. '
                         'linux: the Linux build named by --elf, directly on Linux, through wsl -e on Windows '
                         '(the only mode on Linux)')
    ap.add_argument('--elf', default=None, metavar='PATH',
                    help='--host linux: the Linux build to start (its log is PATH with .log); a Windows path or, '
                         'from Windows, a path inside the WSL (/home/...)')
    ap.add_argument('--env', action='append', default=[], metavar='K=V', help='any other variable')
    ap.add_argument('--wide', choices=('0', '1', '2', '3'), default=None,
                    help="BOF3X_WIDE for the run: 0 the original's 320 x 240, 1 the 16:9 picture (426 x 240), "
                         "2 the 21:9 picture (560 x 240), 3 the 16:10 picture (384 x 240; DIV-0041, "
                         "docs/ultrawide-frame.md). Unset, the "
                         "launcher's bof3x.ini decides (wide=), as before")
    ap.add_argument('--speed', type=int, default=1, metavar='N',
                    help='BOF3X_SPEED: N logic frames per frame of wall time (1..64). The recipe is frame-indexed, '
                         'so it replays the same; shots are still drawn. Late frames are not drawn, so do not '
                         'watch it')
    ap.add_argument('--launcher', default=None,
                    help='another copy of the launcher, with its own bof3x.ini, bof3x.dll and bof3x.log beside it; '
                         '--host linux: bof3x-launcher beside the ELF unless named')
    ap.add_argument('--direct', action='store_true',
                    help='--host linux: start the ELF itself, not through bof3x-launcher (no ini, no defaults)')
    ap.add_argument('--run-game', default=None, metavar='DIR',
                    help='--host linux: the game folder made for the run from --game (default OUT/game on Linux; '
                         '"none": run on --game itself)')
    ap.add_argument('--window', action='store_true',
                    help='--host linux: keep the display (DISPLAY, WAYLAND_DISPLAY, SDL\'s drivers) as the caller '
                         'has it; by default the game runs headless (offscreen, dummy audio, no display)')
    ap.add_argument('--no-front', action='store_true',
                    help='leave the window where it is: the game writes its own frames (BOF3X_SHOT_DIR) and keeps '
                         'running unfocused (DIV-0033), so nothing needs it on top')
    ap.add_argument('--wav', default=None, metavar='DIR',
                    help='BOF3X_WAV_DIR: the game writes its audio mix to DIR/<n>_<label>.wav (docs/wav-dump.md)')
    ap.add_argument('--wav-label', default=None, metavar='LABEL',
                    help="BOF3X_WAV_LABEL; default the recipe's name, with _orig when --original is given")
    ap.add_argument('--save', default=None, metavar='NAME',
                    help='the recipe save to put in slot 0 (tools/recipe_saves/NAME.DAT), instead of the one '
                         'the recipe\'s `# save NAME` header names')
    ap.add_argument('--no-save', action='store_true', help='leave slot 0 alone whatever the recipe says')
    ap.add_argument('--new-game-screens', action='store_true',
                    help="leave New Game's name and options screens as the build has them; by default every run "
                         "turns them off (BOF3X_OFF), so a route that presses New Game plays as before the round, "
                         "unless the recipe has the `# screens new-game` header")
    ap.add_argument('--only-slot0', action='store_true',
                    help='--host linux: the run\'s game folder holds slot 0 alone (the recipe save) and the two-line '
                         'BOF3.CFG of a state-hash run (windowed, renderer 1; linux_run.RUN_CFG), not --game\'s other '
                         'slots and BOF3.CFG: the load menu reads every slot\'s header into .data and BOF3.CFG decides '
                         'Cfg_Fullscreen. With BOF3X_SCALE=2, a reference on any machine (docs/state-hash.md 6)')
    ap.add_argument('--slot0-shared', action='store_true',
                    help='swap slot 0 although a game that is not a scripted run is up (one that never opens a '
                         'save or load menu)')
    ap.add_argument('--slot0-hold', action='store_true',
                    help='keep the recipe save in slot 0 until the run ends, not only until the game logs its '
                         'load: a route that opens a save or load menu again reads the file again, and an '
                         'all-original side never logs the load (so it holds to the end anyway) - an A/B of '
                         'such a route wants this on both sides')
    ap.add_argument('--slot0-wait', type=float, default=300, metavar='S',
                    help='seconds to wait for another run to hand slot 0 back (it does on its load)')
    a = ap.parse_args()
    global LOG
    linux = a.host == 'linux'
    if not linux and sys.platform != 'win32':
        sys.exit('input_run.py runs only --host linux on this system: the Windows hosts need Windows.')
    if linux and not a.elf:
        sys.exit('--host linux needs --elf, the Linux build to start')
    if linux:
        a.elf_here = linux_run.local_path(a.elf)
        LOG = linux_run.log_of(a.elf)
        if a.direct:
            a.launcher = None
        elif a.launcher is None:   # beside the ELF, as the Windows launcher is beside bof3x.exe
            a.launcher = (a.elf.rsplit('/', 1)[0] + '/bof3x-launcher' if a.elf.startswith('/') else
                          os.path.join(os.path.dirname(os.path.abspath(a.elf)), 'bof3x-launcher'))
        a.game_for_run = a.game
    else:
        if a.launcher is None:
            a.launcher = os.path.join(bof3x_home.launcher_dir(), 'bof3x-launcher.exe')
        LOG = os.path.join(os.path.dirname(os.path.abspath(a.launcher)), 'bof3x.log')

    recipe = os.path.abspath(a.recipe)
    if not os.path.isfile(recipe):
        sys.exit(f'no recipe {recipe}')
    os.makedirs(a.out, exist_ok=True)
    if linux:
        stale = linux_game(a, {})
        stale.end_stale()   # as kill_stale: a leftover game of this ELF, by pid
        stale.close()
    else:
        kill_stale(a.launcher)

    env = dict(os.environ)
    for k in ('BOF3X_ORIGINAL', 'BOF3X_LANG', 'BOF3X_INPUT', 'BOF3X_WAV_DIR', 'BOF3X_WAV_LABEL'):
        env.pop(k, None)
    if a.wide is not None:
        env['BOF3X_WIDE'] = a.wide   # set, it wins over the ini's wide= (the launcher fills only an unset one)
    env['BOF3X_INPUT'] = recipe
    env['BOF3X_SHOT_WAIT'] = '1'
    env['BOF3X_SHOT_DIR'] = os.path.abspath(a.out)
    # the owner's screen=satpixie (DIV-0043) must not reach an A/B's captures;
    # --env BOF3X_PRESENT=satpixie asks for it
    env['BOF3X_PRESENT'] = 'clean'
    if a.lang:
        env['BOF3X_LANG'] = a.lang
    if a.original:
        env['BOF3X_ORIGINAL'] = a.original
    if a.speed != 1:
        env['BOF3X_SPEED'] = str(a.speed)
    if a.wav:
        os.makedirs(a.wav, exist_ok=True)
        env['BOF3X_WAV_DIR'] = os.path.abspath(a.wav)
        env['BOF3X_WAV_LABEL'] = a.wav_label or (os.path.splitext(os.path.basename(recipe))[0]
                                                 + ('_orig' if a.original else ''))
    for kv in a.env:
        k, _, v = kv.partition('=')
        env[k] = v
    if a.new_game_screens or recipe_wants_screens(recipe):
        print('new game: the name and options screens as the build has them (the recipe is about them)')
    else:
        added = (screens_off(env, a.elf_here, (os.path.basename(a.elf_here),)) if linux else
                 screens_off(env, a.launcher))
        print(f'new game: BOF3X_OFF={env.get("BOF3X_OFF", "")}' if added else
              'new game: nothing added to BOF3X_OFF (listed already, or a build without the screens)')
    run_game = run_linux if linux else run
    if linux:
        if a.direct:
            env['BOF3X_LAUNCHER'] = '1'   # as the launcher sets it for bof3x.exe (src/launcher/launcher.cpp)
        a.env_keys = tuple(kv.partition('=')[0] for kv in a.env)
        linux_exefile(env, linux_run.local_path(a.game) if a.game.startswith('/') else a.game)
        save = None if a.no_save else linux_save(a.save, recipe, a.game)
        run_dir = a.run_game or (None if linux_run.ON_WINDOWS else os.path.join(os.path.abspath(a.out), 'game'))
        if a.only_slot0 and not (run_dir and run_dir != 'none'):
            sys.exit('--only-slot0 shapes the game folder made for the run: not with --run-game none')
        if run_dir and run_dir != 'none':
            a.game_for_run = make_game(a, run_dir, save)
        elif save:
            sys.exit('a recipe save on --host linux goes into a game folder made for the run: --run-game DIR '
                     '(or --no-save when --game\'s slot 0 already holds it)')
        else:
            print('slot 0: left alone (no `# save NAME` header)' if not a.no_save else 'slot 0: left alone (--no-save)')
        status = run_game(a, env, None)
        print(f'end: {status}')
        sys.exit(0 if status == 'done' else 1)

    if a.only_slot0:
        sys.exit('--only-slot0 is --host linux\'s: the Windows hosts swap slot 0 in the install itself')
    # The save the recipe loads goes into slot 0 for the run and the owner's
    # slot 0 comes back after it (recipe_saves.py; docs/input-script.md 1a).
    if recipe_saves is None:
        sys.exit('the Windows hosts need Windows')
    else:
        save = None if a.no_save else (recipe_saves.save_path(a.save, a.game) if a.save
                                       else recipe_saves.recipe_save(recipe, a.game))
    if save is None:
        print('slot 0: left alone (no `# save NAME` header)' if not a.no_save else 'slot 0: left alone (--no-save)')
        status = run_game(a, env, None)
    else:
        with recipe_saves.Slot0(a.game, save, shared=a.slot0_shared, wait=a.slot0_wait, recipe=recipe) as slot0:
            status = run_game(a, env, slot0)
    print(f'end: {status}')
    sys.exit(0 if status == 'done' else 1)


def recipe_has_save(recipe):
    """True when the recipe has a `# save NAME` header (recipe_saves.HEADER's
    rule, for the system where that module cannot be imported)."""
    with open(recipe, encoding='utf-8', errors='replace') as f:
        return any(re.match(r'^#\s*save\s+(\S+)\s*$', line) for line in f)


def linux_game(a, env):
    return linux_run.LinuxGame(a.elf, getattr(a, 'game_for_run', a.game), env, os.path.join(a.out, '_game_output.txt'),
                               keys=getattr(a, 'env_keys', ()), launcher=a.launcher, headless=not a.window)


def linux_save(name, recipe, game):
    """--host linux: the recipe save's file (recipe_saves.save_path's rule,
    which cannot be imported off Windows), None when the recipe names none.
    Looked for in BOF3X_RECIPE_SAVES, this checkout's tools/recipe_saves,
    the one beside --game, and - from a worktree - the main checkout's."""
    if name is None:
        with open(recipe, encoding='utf-8', errors='replace') as f:
            for line in f:
                m = re.match(r'^#\s*save\s+(\S+)\s*$', line)
                if m:
                    name = m[1]
                    break
    if name is None:
        return None
    if not name.upper().endswith('.DAT'):
        name += '.DAT'
    dirs = [os.environ.get('BOF3X_RECIPE_SAVES'), os.path.join(ROOT, 'tools', 'recipe_saves'),
            os.path.join(os.path.dirname(os.path.abspath(game)), 'tools', 'recipe_saves')]
    parts = os.path.abspath(ROOT).replace('\\', '/').split('/.claude/worktrees/')
    if len(parts) == 2:
        dirs.append(os.path.join(parts[0], 'tools', 'recipe_saves'))
    for d in dirs:
        if d and os.path.isfile(os.path.join(d, name)):
            return os.path.join(d, name)
    sys.exit(f'no recipe save {name} in {", ".join(d for d in dirs if d)}')


def linux_exefile(env, game):
    """attract_run.developer_exefile's rule for --host linux: a PC folder with
    no cache named reads .data from its BOF3.exe under BOF3X_EXEFILE=1 (the
    launcher refuses such a folder otherwise), as the Windows references ran."""
    if 'BOF3X_EXEFILE' in env or env.get('BOF3X_CACHE') or env.get('BOF3X_EXEIMAGE'):
        return
    try:
        held = any(n.upper() == 'BOF3.EXE' for n in os.listdir(game))
    except OSError:
        return
    if held:
        env['BOF3X_EXEFILE'] = '1'
        print(f'  BOF3X_EXEFILE=1: {game} holds BOF3.exe and no cache is named, so the game reads .data from it '
              f'(a developer\'s run, docs/exe-retire-round.md)')


def make_game(a, run_dir, save):
    """The game folder made for the run (linux_run.make_run_game), here or
    through the WSL's helper; its path as the game is given it."""
    if linux_run.ON_WINDOWS:
        helper = linux_run.LinuxGame(a.elf, a.game, {}, os.devnull)
        got = helper._ask('mkgame', linux_run.to_linux(a.game), linux_run.to_linux(run_dir),
                          linux_run.to_linux(save) if save else None, a.only_slot0)
        helper.close()
        if not got:
            sys.exit(f'the WSL could not make the run\'s game folder {run_dir}')
        links, copies = got
        where = linux_run.to_linux(run_dir)
    else:
        try:
            links, copies = linux_run.make_run_game(a.game, run_dir, save, a.only_slot0)
        except (OSError, ValueError) as e:
            sys.exit(f'the run\'s game folder: {e}')
        where = os.path.abspath(run_dir)
    print(f'game folder for the run: {where}, from {a.game} - links: {", ".join(links)}; copies: {", ".join(copies)}')
    return where


def frame_png(path):
    """The frame the game wrote for a shot (BOF3X_SHOT_DIR/NAME.bmp, before its
    log line) as path, a PNG; left a BMP where this Python has no PIL."""
    bmp = os.path.splitext(path)[0] + '.bmp'
    for _ in range(40):
        if os.path.exists(bmp):
            break
        time.sleep(0.05)
    if not os.path.exists(bmp):
        print(f'  no frame {shown(bmp)} from the game')
        return
    try:
        from PIL import Image
    except ImportError:
        print(f'  saved {shown(bmp)} (from the game; no PIL here to make a PNG of it)')
        return
    with Image.open(bmp) as im:
        im.convert('RGB').save(path)
        size = im.size
    os.remove(bmp)
    print(f'  saved {shown(path)} ({size[0]}x{size[1]}, from the game)')


def run_linux(a, env, slot0):
    """--host linux: start the ELF (linux_run.LinuxGame), follow its log to
    the recipe's end, end it - run()'s steps with no window. The status word."""
    started, frames = time.time(), ''
    for _ in range(100):           # the old log first, as run() does
        try:
            os.remove(LOG)
            break
        except FileNotFoundError:
            break
        except OSError:
            time.sleep(0.1)
    else:
        sys.exit(f'{LOG} cannot be removed: a game of this build is still running')
    language_tags.refuse_retired_run(a.elf_here, env)
    game = linux_game(a, env)
    print(f'  host: linux ({"the WSL, through wsl -e" if game.wsl else "directly"}), {a.elf}'
          f'{f", through {a.launcher}" if a.launcher else ", the ELF itself (--direct)"}'
          f'{"" if a.window else ", headless (" + ", ".join(f"{k}={game.env[k]}" for k in linux_run.HEADLESS_SET) + ", no DISPLAY or WAYLAND_DISPLAY)"}')
    game.start()
    print(f'  started {a.elf} (pid {game.pid}{" in the WSL" if game.wsl else ""}); its output: '
          f'{shown(game.output)}')
    status, pos, deadline, quits = None, 0, time.time() + a.minutes * 60, False
    try:
        exited = False
        while status is None and time.time() < deadline:
            time.sleep(0.05)
            if exited:
                status = 'game exited'
                break
            exited = not game.alive()     # the log is read once more after the exit, as in run()
            try:
                with open(LOG, 'rb') as f:
                    f.seek(pos)
                    chunk = f.read()
            except OSError:
                continue
            cut = chunk.rfind(b'\n') + 1
            pos += cut
            for line in chunk[:cut].decode('utf-8', errors='replace').splitlines():
                if 'FATAL' in line:
                    print(line.strip())
                    status = 'FATAL'
                elif m := SHOT.search(line):
                    print(f'shot {m[1]} at recipe frame {m[2]}{" (frozen)" if m[3] else ""}')
                    frames = m[2]
                    frame_png(os.path.join(a.out, m[1] + '.png'))
                    if m[3] and not game.release(f'bof3x_shot_{game.pid}'):
                        print(f'  no shot semaphore for pid {game.pid}; the game waits out its timeout')
                elif m := END.search(line):
                    print(f'recipe {m[1]} at recipe frame {m[2]}')
                    status, frames = m[1], m[2]
                    quits = 'quit step' in line
                elif re.search(r'input\s+(mark|peek|line) ', line):
                    print(line.strip())
                elif m := LOADED.search(line):
                    print(f'save loaded: slot {m[1]}')
                    if slot0 and not a.slot0_hold:
                        slot0.release('the game has loaded it')
        if status is None:
            status = 'timed out'
    finally:
        if status == 'done' and quits:
            game.wait(10)   # the recipe's quit: the game is exiting, and its exit code is the run's to report
        game.end()
        if game.ended_by_us and env.get('BOF3X_SHOT_WAIT'):
            game.unlink(f'bof3x_shot_{game.pid}')    # an ended game leaves its semaphore
        game.close()
        code = game.exit_code()
        print(f'game: {"ended by this run" if game.ended_by_us else "exited (the launcher started it: its code is not seen here)" if game.launcher else f"exit code {code}"}')
        if log_run_time:
            log_run_time('input_run', os.path.splitext(os.path.basename(a.recipe))[0], started,
                         status or 'interrupted', env, a.elf, frames)
    return status


def run(a, env, slot0):
    """Launch, follow the log to the recipe's end, end the game. The status
    word. `slot0` (recipe_saves.Slot0 or None) is handed back when the DLL
    logs the save loaded: the file is not read again after that."""
    started, frames = time.time(), ''
    # The DLL truncates bof3x.log when it opens it, which is after the launch:
    # read before that, the previous run's `recipe done` line ended this run at
    # once with no frames (2026-10-13, the jp sweep's combat on both disc-only
    # installs; also docs/abc-sweep-1012-fixes.md 3.5, abc-sweep-1013-village.md 4,
    # wav-dump.md 5). The old log goes first, so every line read is this run's.
    for _ in range(100):
        try:
            os.remove(LOG)
            break
        except FileNotFoundError:
            break
        except OSError:
            time.sleep(0.1)
    else:
        sys.exit(f'{LOG} cannot be removed: a game of this launcher folder is still running')
    launch(a.launcher, a.game, env, a.host)

    stop = threading.Event()
    if not a.no_front:
        threading.Thread(target=keep_in_front, args=(stop,), daemon=True).start()
    status, pos, deadline = None, 0, time.time() + a.minutes * 60
    try:
        exited = False
        while status is None and time.time() < deadline:
            time.sleep(0.05)
            if exited:
                status = 'game exited'
                break
            # A recipe's `quit` step ends with the game exiting: the DLL logs
            # `done` from its detach, so the log is read once more after the
            # process is gone before the exit counts against the recipe.
            exited = not game_pid()
            try:
                with open(LOG, 'rb') as f:
                    f.seek(pos)
                    chunk = f.read()
            except OSError:
                continue
            cut = chunk.rfind(b'\n') + 1         # whole lines only
            pos += cut
            for line in chunk[:cut].decode('utf-8', errors='replace').splitlines():
                if 'FATAL' in line:
                    print(line.strip())
                    status = 'FATAL'
                elif m := SHOT.search(line):
                    time.sleep(a.delay)
                    print(f'shot {m[1]} at recipe frame {m[2]}{" (frozen)" if m[3] else ""}')
                    frames = m[2]
                    grab(os.path.join(a.out, m[1] + '.png'))
                    if m[3]:
                        release(game_pid())
                elif m := END.search(line):
                    print(f'recipe {m[1]} at recipe frame {m[2]}')
                    status, frames = m[1], m[2]
                elif re.search(r'input\s+(mark|peek|line) ', line):
                    print(line.strip())
                elif m := LOADED.search(line):
                    print(f'save loaded: slot {m[1]}')
                    if slot0 and not a.slot0_hold:
                        slot0.release('the game has loaded it')
        if status is None:
            status = 'timed out'
    finally:
        stop.set()
        kill_game()
        log_run_time('input_run', os.path.splitext(os.path.basename(a.recipe))[0], started, status or 'interrupted',
                     env, a.launcher, frames)
    return status


if __name__ == '__main__':
    main()
