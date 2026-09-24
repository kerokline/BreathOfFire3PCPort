"""The keyboard path end to end: launch the game, press scancodes with SendInput
at its window, and sample Input_Held 0x7E1BE8 (docs/controls.md, DIV-0050).
    python tools/key_probe.py
Four keys of the default table (Z, up, Enter, Q) held 0.4 s each: the word
must show that key's pad bit and clear on release. Pulls the game to the
front for about half a minute; leave the machine alone. Exit 0 on ALL OK.
"""
import ctypes, ctypes.wintypes as w, os, struct, sys, threading, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import attract_run as ar, task_stacks as ts
u = ctypes.WinDLL('user32')
class KI(ctypes.Structure):
    _fields_ = [('wVk', w.WORD), ('wScan', w.WORD), ('dwFlags', w.DWORD), ('time', w.DWORD), ('dwExtraInfo', ctypes.c_void_p)]
class INPUT(ctypes.Structure):
    class U(ctypes.Union):
        _fields_ = [('ki', KI), ('pad', ctypes.c_byte * 32)]
    _anonymous_ = ('u',); _fields_ = [('type', w.DWORD), ('u', U)]
def key(scan, down, ext=False):
    fl = 0x8 | (0 if down else 0x2) | (0x1 if ext else 0)
    i = INPUT(type=1); i.ki = KI(0, scan, fl, 0, None)
    u.SendInput(1, ctypes.byref(i), ctypes.sizeof(INPUT))
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
env = dict(os.environ, BOF3X_LANG='en', BOF3X_PRESENT='clean')
ar.kill_stale(root + r'\build\bof3x-launcher.exe')
pid = ar.launch(root + r'\build\bof3x-launcher.exe', root + r'\bof3', env)
stop = threading.Event(); threading.Thread(target=ar.keep_in_front, args=(stop,), daemon=True).start()
g = ts.Game(pid)
held = lambda: struct.unpack('<H', g.read(0x7E1BE8, 2))[0]
frame = lambda: struct.unpack('<I', g.read(0x937F94, 4))[0]
t0 = time.time()
while frame() < 60 and time.time() - t0 < 60: time.sleep(0.2)
print('loop running, frame', frame())
res = []
for name, scan, ext, expect in (('Z triangle', 0x2C, False, 0x10), ('up arrow', 0x48, True, 0x1000), ('Enter start', 0x1C, False, 0x800), ('Q L2', 0x10, False, 0x1)):
    key(scan, True, ext); time.sleep(0.4); a = held()
    key(scan, False, ext); time.sleep(0.4); b = held()
    ok = a == expect and b == 0
    res.append(ok); print(f'{name:12s} held {a:#06x} (want {expect:#06x}) released {b:#06x} {"OK" if ok else "FAIL"}')
stop.set(); ar.kill_game()
print('ALL OK' if all(res) else 'SOME FAILED'); sys.exit(0 if all(res) else 1)
