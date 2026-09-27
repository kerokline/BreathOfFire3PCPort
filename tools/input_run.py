#!/usr/bin/env python
"""Launch the game with an input recipe and capture the window at its `shot`s.

    python tools/input_run.py recipes/config_screen.txt --out analysis/shots/config
    python tools/input_run.py recipes/config_screen.txt --out X --lang en --original "*"

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
Exit status 0 when the recipe finished, 1 when it FAILED or timed out.
"""
import argparse, ctypes, ctypes.wintypes as w, os, re, subprocess, sys, threading, time

from PIL import ImageGrab

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from attract_run import ROOT, game_pid, kill_game, kill_stale, launch, keep_in_front  # noqa: E402
import recipe_saves  # noqa: E402

u = ctypes.WinDLL('user32')
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenEventW.restype = w.HANDLE
k32.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
try:
    ctypes.WinDLL('shcore').SetProcessDpiAwareness(2)   # physical pixels for the grab
except OSError:
    u.SetProcessDPIAware()

LOG = os.path.join(ROOT, 'build', 'bof3x.log')
SHOT = re.compile(r'input\s+shot (\S+) recipe frame (\d+)( frozen)?')
END = re.compile(r'input\s+(done|FAILED) at recipe frame (\d+)')
LOADED = re.compile(r'save\s+loaded slot ([0-9A-F]+)')     # LoadMenu_Read, src/game/save_menu.cpp


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
        print(f'  saved {os.path.relpath(path, ROOT)} ({size[0]}x{size[1]}, from the game)')
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
    print(f'  saved {os.path.relpath(path, ROOT)} ({r.right}x{r.bottom})')


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
    ap.add_argument('--game', default=os.path.join(ROOT, 'bof3'))
    ap.add_argument('--lang', default=None, help='BOF3X_LANG, e.g. en')
    ap.add_argument('--original', default=None, metavar='LIST', help='BOF3X_ORIGINAL')
    ap.add_argument('--env', action='append', default=[], metavar='K=V', help='any other variable')
    ap.add_argument('--launcher', default=os.path.join(ROOT, 'build', 'bof3x-launcher.exe'),
                    help='another copy of the launcher, with its own bof3x.ini, bof3x.dll and bof3x.log beside it')
    ap.add_argument('--no-front', action='store_true',
                    help='leave the window where it is: the game writes its own frames (BOF3X_SHOT_DIR) and keeps '
                         'running unfocused (DIV-0033), so nothing needs it on top')
    ap.add_argument('--save', default=None, metavar='NAME',
                    help='the recipe save to put in slot 0 (tools/recipe_saves/NAME.DAT), instead of the one '
                         'the recipe\'s `# save NAME` header names')
    ap.add_argument('--no-save', action='store_true', help='leave slot 0 alone whatever the recipe says')
    ap.add_argument('--slot0-shared', action='store_true',
                    help='swap slot 0 although a game that is not a scripted run is up (one that never opens a '
                         'save or load menu)')
    ap.add_argument('--slot0-wait', type=float, default=300, metavar='S',
                    help='seconds to wait for another run to hand slot 0 back (it does on its load)')
    a = ap.parse_args()
    global LOG
    LOG = os.path.join(os.path.dirname(os.path.abspath(a.launcher)), 'bof3x.log')

    recipe = os.path.abspath(a.recipe)
    if not os.path.isfile(recipe):
        sys.exit(f'no recipe {recipe}')
    os.makedirs(a.out, exist_ok=True)
    kill_stale(a.launcher)

    env = dict(os.environ)
    for k in ('BOF3X_ORIGINAL', 'BOF3X_LANG', 'BOF3X_INPUT'):
        env.pop(k, None)
    env['BOF3X_INPUT'] = recipe
    env['BOF3X_SHOT_WAIT'] = '1'
    env['BOF3X_SHOT_DIR'] = os.path.abspath(a.out)
    # the owner's screen=crt (DIV-0037) must not reach an A/B's captures;
    # --env BOF3X_PRESENT=crt asks for it
    env['BOF3X_PRESENT'] = 'clean'
    if a.lang:
        env['BOF3X_LANG'] = a.lang
    if a.original:
        env['BOF3X_ORIGINAL'] = a.original
    for kv in a.env:
        k, _, v = kv.partition('=')
        env[k] = v

    # The save the recipe loads goes into slot 0 for the run and the owner's
    # slot 0 comes back after it (recipe_saves.py; docs/input-script.md 1a).
    save = None if a.no_save else (recipe_saves.save_path(a.save, a.game) if a.save
                                   else recipe_saves.recipe_save(recipe, a.game))
    if save is None:
        print('slot 0: left alone (no `# save NAME` header)' if not a.no_save else 'slot 0: left alone (--no-save)')
        status = run(a, env, None)
    else:
        with recipe_saves.Slot0(a.game, save, shared=a.slot0_shared, wait=a.slot0_wait, recipe=recipe) as slot0:
            status = run(a, env, slot0)
    print(f'end: {status}')
    sys.exit(0 if status == 'done' else 1)


def run(a, env, slot0):
    """Launch, follow the log to the recipe's end, end the game. The status
    word. `slot0` (recipe_saves.Slot0 or None) is handed back when the DLL
    logs the save loaded: the file is not read again after that."""
    launch(a.launcher, a.game, env)

    stop = threading.Event()
    if not a.no_front:
        threading.Thread(target=keep_in_front, args=(stop,), daemon=True).start()
    status, pos, deadline = None, 0, time.time() + a.minutes * 60
    try:
        while status is None and time.time() < deadline:
            time.sleep(0.05)
            if not game_pid():
                status = 'game exited'
                break
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
                    grab(os.path.join(a.out, m[1] + '.png'))
                    if m[3]:
                        release(game_pid())
                elif m := END.search(line):
                    print(f'recipe {m[1]} at recipe frame {m[2]}')
                    status = m[1]
                elif re.search(r'input\s+(mark|peek|line) ', line):
                    print(line.strip())
                elif m := LOADED.search(line):
                    print(f'save loaded: slot {m[1]}')
                    if slot0:
                        slot0.release('the game has loaded it')
        if status is None:
            status = 'timed out'
    finally:
        stop.set()
        kill_game()
    return status


if __name__ == '__main__':
    main()
