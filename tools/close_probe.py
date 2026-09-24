"""Closing the window: launch, wait for a frame count, post WM_CLOSE to the
game's window, and report whether the process ended (docs/controls.md §6).
    python tools/close_probe.py 700            # at the title
    python tools/close_probe.py 700 build_x    # another build directory
Found 2026-09-24: the present read a destroyed window's client rectangle
(ResizeBuffers E_INVALIDARG), and SDL's HIDAPI discovery took the WM_QUIT.
Both fixed; a pass prints "alive after close: False" and the four teardown
lines. Pulls the game to the front; leave the machine alone for a minute.
"""
import ctypes, ctypes.wintypes as w, os, struct, sys, threading, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import attract_run as ar, task_stacks as ts
u = ctypes.WinDLL('user32')
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
bdir = sys.argv[2] if len(sys.argv) > 2 else root + r'\build'
env = dict(os.environ, BOF3X_LANG='en', BOF3X_PRESENT='clean')
ar.kill_stale(bdir + r'\bof3x-launcher.exe')
pid = ar.launch(bdir + r'\bof3x-launcher.exe', root + r'\bof3', env)
stop = threading.Event(); threading.Thread(target=ar.keep_in_front, args=(stop,), daemon=True).start()
g = ts.Game(pid)
frame = lambda: struct.unpack('<I', g.read(0x937F94, 4))[0]
t0 = time.time()
while frame() < int(sys.argv[1]) and time.time() - t0 < 90: time.sleep(0.2)
print('frame', frame())
wins = []
@ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
def cb(hwnd, _):
    p = w.DWORD(); u.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
    if p.value == pid and u.IsWindowVisible(hwnd): wins.append(hwnd)
    return True
u.EnumWindows(cb, 0)
stop.set()
for h in wins:
    buf = ctypes.create_unicode_buffer(64); u.GetClassNameW(h, buf, 64)
    print('WM_CLOSE to', hex(h), 'class', buf.value)
    u.PostMessageW(h, 0x0010, 0, 0)
t0 = time.time()
while ar.pid_alive(pid) and time.time() - t0 < 15: time.sleep(0.1)
print('alive after close:', ar.pid_alive(pid), 'frame now', frame() if ar.pid_alive(pid) else '-')
ar.kill_game()
log = open(bdir + r'\bof3x.log', encoding='utf-8', errors='replace').read().splitlines()
print('\n'.join(l[:150] for l in log[-4:]))
