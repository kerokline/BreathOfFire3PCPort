"""The bindings end to end: bof3x.ini -> the launcher -> BOF3X_KEYS / BOF3X_PAD
-> the DLL's key table and pad map (docs/controls.md section 4.2, DIV-0050).
    python tools/ini_probe.py
Writes a test ini beside the launcher with four keys swapped, a diagonal and
a Nintendo pad layout, launches with --no-config, presses the keys at the
window and reads Input_Held; the log's `pad:` lines show what the DLL took.
Backs up and restores build/bof3x.ini. Pulls the game to the front; leave the
machine alone for a minute. Exit 0 on ALL OK.
"""
import ctypes, ctypes.wintypes as w, os, shutil, struct, sys, threading, time
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(root, 'tools'))
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
ini = root + r'\build\bof3x.ini'
bak = ini + '.probe-bak'
had = os.path.exists(ini)
if had: shutil.copyfile(ini, bak)
try:
    text = open(ini, encoding='utf-8').read() if had else ''
    lines = [l for l in text.splitlines() if not l.startswith('key.') and not l.startswith('pad.')]
    lines += ['key.Z=circle', 'key.Up=down', 'key.Enter=select', 'key.Q=r2', 'key.Numpad9=up+right',
              'pad.layout=nintendo', 'pad.south=triangle', 'pad.ls_up=up']
    open(ini, 'w', encoding='utf-8').write('\r\n'.join(lines) + '\r\n')
    env = dict(os.environ, BOF3X_LANG='en', BOF3X_PRESENT='clean')
    for k in ('BOF3X_KEYS', 'BOF3X_PAD', 'BOF3X_PAD_LAYOUT'): env.pop(k, None)
    ar.kill_stale(root + r'\build\bof3x-launcher.exe')
    pid = ar.launch(root + r'\build\bof3x-launcher.exe', root + r'\bof3', env)
    stop = threading.Event(); threading.Thread(target=ar.keep_in_front, args=(stop,), daemon=True).start()
    g = ts.Game(pid)
    held = lambda: struct.unpack('<H', g.read(0x7E1BE8, 2))[0]
    frame = lambda: struct.unpack('<I', g.read(0x937F94, 4))[0]
    t0 = time.time()
    while frame() < 60 and time.time() - t0 < 60: time.sleep(0.2)
    res = []
    for name, scan, ext, expect in (('Z circle', 0x2C, False, 0x20), ('Up down', 0x48, True, 0x4000),
                                    ('Enter select', 0x1C, False, 0x100), ('Q R2', 0x10, False, 0x2),
                                    ('Numpad9 up+right', 0x49, False, 0x3000), ('X unbound', 0x2D, False, 0)):
        key(scan, True, ext); time.sleep(0.4); a = held()
        key(scan, False, ext); time.sleep(0.4); b = held()
        ok = a == expect and b == 0
        res.append(ok); print(f'{name:18s} held {a:#06x} (want {expect:#06x}) released {b:#06x} {"OK" if ok else "FAIL"}')
    stop.set(); ar.kill_game()
    log = open(root + r'\build\bof3x.log', encoding='utf-8', errors='replace').read().splitlines()
    print('\n'.join(l for l in log if l.startswith('pad:')))
    print('ALL OK' if all(res) else 'SOME FAILED')
    ok = all(res)
finally:
    if had: shutil.move(bak, ini)
    else: os.remove(ini)
sys.exit(0 if ok else 1)
