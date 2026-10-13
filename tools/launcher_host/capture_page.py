"""Capture the launcher's settings dialog, and its Jukebox, as the player would see them.

    python tools/launcher_host/capture_page.py <folder with bof3x-launcher.exe> --game <install>
        --cache <cache> --out <dir> [--name NAME] [--jukebox] [--via-env] [--lang TAG]

Starts the launcher (no --no-config, so the settings dialog opens) on the cache, as a player
has it: a bof3x.ini of two lines written into the launcher folder (`cache=`, and `language=`
with --lang), removed afterwards - or, with --via-env, BOF3X_CACHE set instead (it wins at
start-up; the page's suites read it, the Cache folder box stays empty). So the page's suites
and greyed reasons are that cache's (docs/launcher-suites.md). It waits for the dialog, writes it
with PrintWindow (PW_RENDERFULLCONTENT) as <out>/<name>.png, with --jukebox clicks Jukebox...
and writes that dialog as <out>/<name>_jukebox.png, then cancels: the dialog's Cancel writes
no ini and starts no game. The launcher folder must be a scratch copy (an ini there would be
read). Needs a desktop session. Writes only --out; reads nothing of the game itself.
"""
import argparse, ctypes, os, subprocess, sys, time
from ctypes import wintypes as W

from PIL import Image

u = ctypes.WinDLL('user32', use_last_error=True)
g = ctypes.WinDLL('gdi32')
WNDENUMPROC = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
u.EnumWindows.argtypes = [WNDENUMPROC, W.LPARAM]
u.GetWindowThreadProcessId.argtypes = [W.HWND, ctypes.POINTER(W.DWORD)]
u.IsWindowVisible.argtypes = [W.HWND]
u.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
u.GetWindowRect.argtypes = [W.HWND, ctypes.POINTER(W.RECT)]
u.GetDlgItem.restype = W.HWND
u.GetDlgItem.argtypes = [W.HWND, ctypes.c_int]
u.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
u.PrintWindow.argtypes = [W.HWND, W.HDC, W.UINT]
u.GetDC.restype = W.HDC
u.GetDC.argtypes = [W.HWND]
u.ReleaseDC.argtypes = [W.HWND, W.HDC]
g.CreateCompatibleDC.restype = W.HDC
g.CreateCompatibleDC.argtypes = [W.HDC]
g.CreateCompatibleBitmap.restype = W.HBITMAP
g.CreateCompatibleBitmap.argtypes = [W.HDC, ctypes.c_int, ctypes.c_int]
g.SelectObject.argtypes = [W.HDC, W.HGDIOBJ]
g.GetDIBits.argtypes = [W.HDC, W.HBITMAP, W.UINT, W.UINT, ctypes.c_void_p, ctypes.c_void_p, W.UINT]
g.DeleteObject.argtypes = [W.HGDIOBJ]
g.DeleteDC.argtypes = [W.HDC]

WM_COMMAND, IDCANCEL, WM_CLOSE, BM_CLICK = 0x111, 2, 0x10, 0xF5
IDC_MUSICPLAYER = 1030          # src/launcher/resource.h
PW_RENDERFULLCONTENT = 2


def windows_of(pid):
    found = []

    def cb(h, _):
        p = W.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u.IsWindowVisible(h):
            t = ctypes.create_unicode_buffer(256)
            u.GetWindowTextW(h, t, 256)
            found.append((h, t.value))
        return True
    u.EnumWindows(WNDENUMPROC(cb), 0)
    return found


def wait_window(pid, want=None, skip=(), timeout=20):
    t0 = time.time()
    while time.time() - t0 < timeout:
        for h, t in windows_of(pid):
            if h in skip:
                continue
            if want is None or want in t:
                return h, t
        time.sleep(0.2)
    return None, None


def grab(h, path):
    r = W.RECT()
    u.GetWindowRect(h, ctypes.byref(r))
    w, ht = r.right - r.left, r.bottom - r.top
    sdc = u.GetDC(None)
    mdc = g.CreateCompatibleDC(sdc)
    bmp = g.CreateCompatibleBitmap(sdc, w, ht)
    g.SelectObject(mdc, bmp)
    u.PrintWindow(h, mdc, PW_RENDERFULLCONTENT)

    class BIH(ctypes.Structure):
        _fields_ = [('biSize', W.DWORD), ('biWidth', W.LONG), ('biHeight', W.LONG), ('biPlanes', W.WORD),
                    ('biBitCount', W.WORD), ('biCompression', W.DWORD), ('biSizeImage', W.DWORD),
                    ('biXPelsPerMeter', W.LONG), ('biYPelsPerMeter', W.LONG), ('biClrUsed', W.DWORD),
                    ('biClrImportant', W.DWORD)]
    bi = BIH(ctypes.sizeof(BIH), w, -ht, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * ht * 4)
    g.GetDIBits(mdc, bmp, 0, ht, buf, ctypes.byref(bi), 0)
    Image.frombuffer('RGB', (w, ht), buf.raw, 'raw', 'BGRX', 0, 1).save(path)
    g.DeleteObject(bmp)
    g.DeleteDC(mdc)
    u.ReleaseDC(None, sdc)
    print(f'  {path}: {w} x {ht}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('launcher_dir')
    ap.add_argument('--game', required=True)
    ap.add_argument('--cache', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--name', default='launcher')
    ap.add_argument('--lang', help='BOF3X_LANG for the launcher (the Language box\'s start)')
    ap.add_argument('--jukebox', action='store_true')
    ap.add_argument('--via-env', action='store_true', help='BOF3X_CACHE / BOF3X_LANG instead of an ini')
    a = ap.parse_args()
    ini = os.path.join(a.launcher_dir, 'bof3x.ini')
    if os.path.exists(ini):
        sys.exit(f'{a.launcher_dir} holds a bof3x.ini: use an ini-less scratch copy')
    os.makedirs(a.out, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith('BOF3X_')}
    if a.via_env:
        env['BOF3X_CACHE'] = os.path.abspath(a.cache)
        if a.lang:
            env['BOF3X_LANG'] = a.lang
    else:
        with open(ini, 'w', newline='\r\n') as fh:
            fh.write('[bof3x]\ncache=' + os.path.abspath(a.cache) + '\n' + (f'language={a.lang}\n' if a.lang else ''))
    p = subprocess.Popen([os.path.join(a.launcher_dir, 'bof3x-launcher.exe'), '--game', a.game], env=env,
                         cwd=a.launcher_dir)
    try:
        h, t = wait_window(p.pid)
        if not h:
            sys.exit('no dialog appeared')
        time.sleep(1.0)
        grab(h, os.path.join(a.out, a.name + '.png'))
        if a.jukebox:
            u.PostMessageW(u.GetDlgItem(h, IDC_MUSICPLAYER), BM_CLICK, 0, 0)
            j, _ = wait_window(p.pid, 'Jukebox', skip=(h,))
            if j:
                time.sleep(1.0)
                grab(j, os.path.join(a.out, a.name + '_jukebox.png'))
                u.PostMessageW(j, WM_CLOSE, 0, 0)
                time.sleep(0.5)
            else:
                print('  no Jukebox dialog appeared')
        u.PostMessageW(h, WM_COMMAND, IDCANCEL, 0)
        p.wait(10)
    finally:
        if p.poll() is None:
            p.kill()
    if os.path.exists(ini):
        os.remove(ini)


if __name__ == '__main__':
    main()
