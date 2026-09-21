#!/usr/bin/env python
"""Launch the game fresh and record its attract sequence from the first frame.

    python tools/attract_run.py --out analysis/attract/a.tsv
    python tools/attract_run.py --out analysis/attract/b.tsv --original "*"
    python tools/attract_diff.py analysis/attract/a.tsv analysis/attract/b.tsv

No input is ever sent. Left alone, the port plays both logo videos, and about a
minute after launch starts the attract sequence by itself
(docs/attract-mode.md) — and input timing is the one thing that could
legitimately make two runs differ, so the script's job is to keep its hands off.

What it does: ends any running BOF3.exe (pass --no-kill to refuse instead),
starts build/bof3x-launcher.exe, brings the game window to the foreground (the
port freezes when it is not — docs/windowed-mode.md), records with
attract_watch for --minutes, then ends the game.

--original sets BOF3X_ORIGINAL for the run: "*" is Capcom's code throughout,
the configuration a regression oracle compares against (docs/SCAFFOLDING.md).

--lang and --filter pin BOF3X_LANG and BOF3X_FILTER, default "original" and
"linear": the launcher fills an unset variable in from its settings file, so
without them the run inherits whatever the owner last chose in the dialog -
2026-09-21, an English attract run compared against a Chinese reference
looked like a regression in LoadDatFile for an afternoon.
"""
import argparse, ctypes, ctypes.wintypes as w, os, subprocess, sys, threading, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
u = ctypes.WinDLL('user32')


def game_pid():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq BOF3.exe', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    return int(out.split(',')[1].strip('"')) if 'BOF3' in out else None


def kill_game():
    subprocess.run(['taskkill', '/F', '/IM', 'BOF3.exe'], capture_output=True)
    t0 = time.time()
    while game_pid() and time.time() - t0 < 10:
        time.sleep(0.1)


def keep_in_front(stop):
    """Foreground the game's window whenever it is not. Sends no input."""
    while not stop.is_set():
        pid = game_pid()
        if pid:
            fg = w.DWORD()
            u.GetWindowThreadProcessId(u.GetForegroundWindow(), ctypes.byref(fg))
            if fg.value != pid:
                wins = []

                @ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
                def cb(hwnd, _):
                    p = w.DWORD()
                    u.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
                    if p.value == pid and u.IsWindowVisible(hwnd):
                        wins.append(hwnd)
                    return True
                u.EnumWindows(cb, 0)
                if wins:
                    u.SetForegroundWindow(wins[0])
        stop.wait(0.5)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--minutes', type=float, default=5.0,
                    help='default 5: launch, ~1 min to the first scene, one 5,522-frame cycle')
    ap.add_argument('--game', default=os.path.join(ROOT, 'bof3'))
    ap.add_argument('--original', default=None, metavar='LIST',
                    help='value for BOF3X_ORIGINAL, e.g. "*" or "File_Read"')
    ap.add_argument('--lang', default='original',
                    help='value for BOF3X_LANG: "original" (default) or "en"')
    ap.add_argument('--filter', default='linear',
                    help="value for BOF3X_FILTER: \"linear\" (default, the port's own) or \"point\"")
    ap.add_argument('--no-kill', action='store_true')
    a = ap.parse_args()

    if game_pid():
        if a.no_kill:
            sys.exit('BOF3.exe is already running')
        kill_game()

    env = dict(os.environ)
    env.pop('BOF3X_ORIGINAL', None)
    if a.original:
        env['BOF3X_ORIGINAL'] = a.original
    env['BOF3X_LANG'] = a.lang
    env['BOF3X_FILTER'] = a.filter
    launcher = os.path.join(ROOT, 'build', 'bof3x-launcher.exe')
    # --no-config: an oracle run must not stop on the settings dialog, and must
    # take the settings file's values without a human touching them
    # (docs/launcher-settings.md section 4).
    if subprocess.run([launcher, '--game', a.game, '--no-config'], env=env).returncode != 0:
        sys.exit('launcher failed')

    stop = threading.Event()
    front = threading.Thread(target=keep_in_front, args=(stop,), daemon=True)
    front.start()
    try:
        subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'attract_watch.py'),
                        '--minutes', str(a.minutes), '--wait', '30', '--out', a.out], check=True)
    finally:
        stop.set()
        kill_game()
    with open(a.out, 'a', encoding='utf-8', newline='\n') as f:
        f.write(f'# BOF3X_ORIGINAL={a.original or ""}\n')
        f.write(f'# BOF3X_LANG={a.lang} BOF3X_FILTER={a.filter}\n')


if __name__ == '__main__':
    main()
