"""Drive the launcher's settings dialog (src/launcher/config_dialog.cpp) without starting the game.

A temporary game directory holds a copy of the player's BOF3.exe (the launcher checks its
hash; the copy is deleted at the end) and empty DAT\\ files naming layers and a language;
the launcher's bof3x.dll there is a dummy, so after Play the game process - created
suspended - fails to load it and the launcher ends it (Die) before any of the game's code
runs. The ini is read after each run. Needs a desktop session (the dialog is shown).
docs/launcher-settings.md section 3.

    python tools/launcher_host/dialog_test.py build/bof3x-launcher.exe <the install>/BOF3.exe
"""
import ctypes, os, shutil, subprocess, sys, tempfile, time
from ctypes import wintypes as W

u = ctypes.WinDLL("user32", use_last_error=True)
u.FindWindowW.restype = W.HWND
u.FindWindowW.argtypes = [W.LPCWSTR, W.LPCWSTR]
u.GetDlgItem.restype = W.HWND
u.GetDlgItem.argtypes = [W.HWND, ctypes.c_int]
u.SendMessageW.restype = ctypes.c_ssize_t
u.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
u.IsWindowVisible.argtypes = [W.HWND]
u.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
u.SetWindowTextW.argtypes = [W.HWND, W.LPCWSTR]
u.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]

WM_COMMAND, WM_CLOSE, CB_GETCURSEL, CB_SETCURSEL, BM_GETCHECK, BM_SETCHECK = 0x111, 0x10, 0x147, 0x14E, 0xF0, 0xF1
CBN_SELCHANGE, EN_CHANGE = 1, 0x300
IDOK = 1
IDC_LANGUAGE, IDC_MUSIC, IDC_CACHE, IDC_CACHENOTE, IDC_OPTNOTE, IDC_OPT0 = 1001, 1014, 1015, 1017, 1018, 1020
PSP = ["psp-art", "psp-tiles", "psp-maps", "psp-names-en-150", "psp-names-ja-JP"]

SCR = tempfile.mkdtemp(prefix="bof3x_dialog_")
L = os.path.join(SCR, "dlg", "launcher")
G = os.path.join(SCR, "dlg", "game")


def setup(launcher, exe, layers):
    shutil.rmtree(os.path.join(SCR, "dlg"), ignore_errors=True)
    os.makedirs(L); os.makedirs(os.path.join(G, "DAT"))
    shutil.copy(launcher, L)
    open(os.path.join(L, "bof3x.dll"), "wb").write(b"not a dll")
    shutil.copy(exe, G)
    for name in layers:
        open(os.path.join(G, "DAT", name + ".AREA067.DAT"), "wb").close()


def text(h):
    # WM_GETTEXT, which the system marshals across processes (GetWindowText reads
    # another process's edit control as empty)
    b = ctypes.create_unicode_buffer(512)
    u.SendMessageW(h, 0x000D, 512, ctypes.cast(b, ctypes.c_void_p).value)
    return b.value


def settext(h, s):
    b = ctypes.create_unicode_buffer(s)
    u.SendMessageW(h, 0x000C, 0, ctypes.cast(b, ctypes.c_void_p).value)  # WM_SETTEXT, sends EN_CHANGE


def run(ini, act):
    """Write the ini, open the dialog, read its state, apply act(dlg), press Play; the ini after."""
    p = os.path.join(L, "bof3x.ini")
    if ini is None:
        if os.path.exists(p): os.remove(p)
    else:
        open(p, "w", newline="").write(ini)
    proc = subprocess.Popen([os.path.join(L, "bof3x-launcher.exe"), "--game", G, "--config"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    dlg = None
    for _ in range(200):
        dlg = u.FindWindowW(None, "Breath of Fire III - bof3x")
        if dlg: break
        time.sleep(0.05)
    assert dlg, "no dialog"
    time.sleep(0.3)
    item = lambda i: u.GetDlgItem(dlg, i)
    state = dict(music=u.SendMessageW(item(IDC_MUSIC), CB_GETCURSEL, 0, 0), cache=text(item(IDC_CACHE)),
                 cachenote=text(item(IDC_CACHENOTE)),
                 boxes={PSP[i]: (bool(u.IsWindowVisible(item(IDC_OPT0 + i))),
                                 u.SendMessageW(item(IDC_OPT0 + i), BM_GETCHECK, 0, 0)) for i in range(5)},
                 optnote=text(item(IDC_OPTNOTE)) if u.IsWindowVisible(item(IDC_OPTNOTE)) else "")
    if act:
        act(dlg, item)
        time.sleep(0.2)
        state["after"] = dict(cachenote=text(item(IDC_CACHENOTE)),
                              visible={PSP[i]: bool(u.IsWindowVisible(item(IDC_OPT0 + i))) for i in range(5)})
    u.PostMessageW(dlg, WM_COMMAND, IDOK, 0)
    # Die's message box after the dummy dll: close it
    for _ in range(200):
        if proc.poll() is not None: break
        box = u.FindWindowW(None, "bof3x-launcher")
        if box: u.PostMessageW(box, WM_CLOSE, 0, 0)
        time.sleep(0.05)
    proc.wait(10)
    err = proc.stderr.read().decode(errors="replace").strip().splitlines()
    return state, open(p).read() if os.path.exists(p) else None, err


def lines(ini):
    return {k: v for k, _, v in (l.partition("=") for l in ini.splitlines() if l and l[0] not in "#[;")}


def main():
    launcher, exe = sys.argv[1], sys.argv[2]
    setup(launcher, exe, ["psp-art", "psp-maps", "psp-names-en-150", "area4-walls", "en-US"])
    ok = True

    def report(name, cond, detail=""):
        nonlocal ok
        ok &= bool(cond)
        print("%-62s %s %s" % (name, "ok" if cond else "FAIL", detail))

    # 1. A fresh ini as the launcher writes it, then an untouched dialog: byte-identical.
    _, first, _ = run("[bof3x]\nopt=\ncache=%s\nmusic=seq\n" % SCR, None)
    st, again, err = run(first, None)
    report("untouched dialog: the ini byte-identical", again == first)
    report("  music=seq shown as the default entry", st["music"] == 0, str(st["music"]))
    report("  cache shown as the ini has it", st["cache"] == SCR, st["cache"])
    report("  installed PSP layers boxed, en-150 names hidden (original)",
           st["boxes"] == {"psp-art": (True, 0), "psp-tiles": (False, 0), "psp-maps": (True, 0),
                           "psp-names-en-150": (False, 0), "psp-names-ja-JP": (False, 0)}, str(st["boxes"]))
    report("  the cache note: a folder without base\\bgm", "No base\\bgm" in st["cachenote"], st["cachenote"])
    # 2. Untouched with empty lines: still empty.
    _, base, _ = run("[bof3x]\nopt=\ncache=\nmusic=\n", None)
    _, again, _ = run(base, None)
    l = lines(again)
    report("untouched: empty opt= / cache= / music= stay empty",
           again == base and l["opt"] == "" and l["cache"] == "" and l["music"] == "", str({k: l[k] for k in ("opt", "cache", "music")}))

    # 3. Tick psp-art over an empty opt=: the walls stay on; music to mp3; a cache typed in.
    def act3(dlg, item):
        u.SendMessageW(item(IDC_OPT0), BM_SETCHECK, 1, 0)
        u.SendMessageW(item(IDC_MUSIC), CB_SETCURSEL, 1, 0)
        settext(item(IDC_CACHE), "  C:\\nowhere at all  ")
    st, ini, _ = run(base, act3)
    l = lines(ini)
    report("psp-art ticked over opt= empty: psp-art,area4-walls", l["opt"] == "psp-art,area4-walls", l["opt"])
    report("music moved to the MP3s: music=mp3", l["music"] == "mp3", l["music"])
    report("cache typed with spaces: trimmed as a line is", l["cache"] == "C:\\nowhere at all", l["cache"])
    report("  its note: not a folder", "Not a folder" in st["after"]["cachenote"], st["after"]["cachenote"])
    # 4. Back: music to the default writes empty; psp-art off leaves the walls listed.
    def act4(dlg, item):
        u.SendMessageW(item(IDC_OPT0), BM_SETCHECK, 0, 0)
        u.SendMessageW(item(IDC_MUSIC), CB_SETCURSEL, 0, 0)
    st, ini, _ = run(ini, act4)
    l = lines(ini)
    report("psp-art unticked: area4-walls stays listed", l["opt"] == "area4-walls", l["opt"])
    report("music back to the default: music= empty", l["music"] == "", l["music"])
    report("  the boxes showed psp-art ticked", st["boxes"]["psp-art"] == (True, 1), str(st["boxes"]["psp-art"]))
    # 5. The names layer: shown when en-US is chosen; ticked, written after the art.
    def act5(dlg, item):
        u.SendMessageW(item(IDC_LANGUAGE), CB_SETCURSEL, 1, 0)
        u.SendMessageW(dlg, WM_COMMAND, (CBN_SELCHANGE << 16) | IDC_LANGUAGE, item(IDC_LANGUAGE))
        u.SendMessageW(item(IDC_OPT0 + 3), BM_SETCHECK, 1, 0)
        u.SendMessageW(item(IDC_OPT0 + 2), BM_SETCHECK, 1, 0)
    st, ini, err = run("[bof3x]\nopt=none\n", act5)
    l = lines(ini)
    report("en-US chosen: the en-150 names box shown", st["after"]["visible"]["psp-names-en-150"], str(st["after"]["visible"]))
    report("opt=none, maps and names ticked: psp-maps,psp-names-en-150", l["opt"] == "psp-maps,psp-names-en-150", l["opt"])
    report("language=en-US written", l["language"] == "en-US", l["language"])
    # 6. A layer the dialog does not offer (not installed) survives an edit.
    def act6(dlg, item):
        u.SendMessageW(item(IDC_OPT0), BM_SETCHECK, 1, 0)
    st, ini, _ = run("[bof3x]\nopt=psp-tiles,area4-walls\n", act6)
    l = lines(ini)
    report("psp-tiles (not installed) kept when psp-art is ticked", l["opt"] == "psp-art,psp-tiles,area4-walls", l["opt"])
    # 7. No PSP layer installed: the note instead of boxes.
    setup(launcher, exe, ["area4-walls"])
    st, ini, _ = run("[bof3x]\nopt=\n", None)
    report("no PSP layer installed: the note shown, no box",
           "No PSP layer is installed" in st["optnote"] and not any(v[0] for v in st["boxes"].values()), st["optnote"])
    report("  and opt= untouched", lines(ini)["opt"] == "", lines(ini)["opt"])
    shutil.rmtree(SCR, ignore_errors=True)
    print("all ok" if ok else "FAILURES")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
