r"""The drawn launcher's proof against the Win32 one (docs/launcher-portable.md, stage P2): the
settings file written from the same choices by both launchers is the same, byte for byte.

    python tools/launcher_host/ui_compare.py --ui <bof3x-launcher-ui.exe> --win32 <launcher copy>
        --home <BOF3X_HOME> --game <game folder> --ini <starting bof3x.ini> --out <dir>
        [--case NAME ...] [--list]

For each case of CASES it makes the case's choices in both launchers, from the same starting ini,
then Back and Exit - which saves the settings and starts nothing - and compares the two files:
"equal", or the lines that differ. A difference is a finding: the case is not changed to hide it.

- The drawn side needs no window: bof3x-launcher-ui.exe --check --ini <start> --game <game>
  --save <out> --script "<open the page>; set <widget>=<value> ...; press Back; press Exit".
- The Win32 side: the starting ini is written as bof3x.ini into the launcher copy (--win32: an
  ini-less folder holding bof3x-launcher.exe, bof3x-launcher-dev.exe, bof3x.dll and bof3x.exe,
  docs/HANDOFF.md), since that launcher reads and writes bof3x.ini beside itself whatever
  BOF3X_HOME says (launcher.cpp: OwnDirectory() + "\\bof3x.ini"; BOF3X_HOME only says where the
  home's cache\, sources.toml and mods are, common/home.cpp). It is started with --game and
  --config, so it shows its window; the window is found among the windows of the process this
  tool started (by its PID, capture_page.windows_of), the choices are made by control id
  (src/launcher/resource.h, read by name), then Back (IDC_BACK) and Exit (IDCANCEL on the title
  page). The window shows on the screen for a moment in each case. Play (IDOK on the main
  window) is never sent: this tool refuses to send IDOK to any window but an options window it
  opened, and after each case it looks for a bof3x.exe started by the launcher and reports and
  ends one if found (none should be). A launcher that does not end is ended by its own handle.

The method of the Win32 side is dialog_test.py's (find_item, the WM_GETTEXT read, the windows of
one PID), copied here rather than imported: importing dialog_test.py makes a temporary folder at
import time. Needs a desktop session. Reads the install (--home, --game) and never writes there:
it writes only the launcher copy's bof3x.ini (removed at the end) and --out.
"""
import sys
if sys.platform != 'win32':
    sys.exit("ui_compare.py is a Windows tool: it drives the Win32 launcher's window through user32, so it does not run here.")
import argparse, ctypes, difflib, os, re, shutil, subprocess, time
from ctypes import wintypes as W

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capture_page import windows_of   # noqa: E402  the shown windows of one process, by its PID

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

u = ctypes.WinDLL("user32", use_last_error=True)
k = ctypes.WinDLL("kernel32", use_last_error=True)
u.GetDlgItem.restype = W.HWND
u.GetDlgItem.argtypes = [W.HWND, ctypes.c_int]
u.GetParent.restype = W.HWND
u.GetParent.argtypes = [W.HWND]
u.FindWindowExW.restype = W.HWND
u.FindWindowExW.argtypes = [W.HWND, W.HWND, W.LPCWSTR, W.LPCWSTR]
u.GetWindowLongW.argtypes = [W.HWND, ctypes.c_int]
u.SendMessageW.restype = ctypes.c_ssize_t
u.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
u.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
u.IsWindow.argtypes = [W.HWND]
u.IsWindowEnabled.argtypes = [W.HWND]
u.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
u.GetDlgCtrlID.argtypes = [W.HWND]

WM_SETTEXT, WM_GETTEXT, WM_COMMAND, WM_HSCROLL, WM_CLOSE = 0x000C, 0x000D, 0x0111, 0x0114, 0x0010
BM_GETCHECK, BM_CLICK, CB_SETCURSEL, CB_GETCURSEL, CB_GETCOUNT = 0xF0, 0xF5, 0x14E, 0x147, 0x146
TBM_GETPOS, TBM_SETPOS, TBM_GETRANGEMAX = 0x400, 0x405, 0x402
BN_CLICKED, CBN_SELCHANGE, SB_ENDSCROLL = 0, 1, 8
GWL_STYLE, WS_VISIBLE, BS_TYPEMASK = -16, 0x10000000, 0xF
BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_CHECKBOX, BS_AUTOCHECKBOX, BS_RADIO, BS_AUTORADIO = 0, 1, 2, 3, 4, 9

TITLE = "Breath of Fire III - bof3x"   # the Win32 launcher's window (launcher.rc IDD_CONFIG)


def resource_ids():
    """The control ids by name, from src/launcher/resource.h, with the system's IDOK and IDCANCEL."""
    ids = {"IDOK": 1, "IDCANCEL": 2}
    with open(os.path.join(REPO, "src", "launcher", "resource.h"), encoding="utf-8") as f:
        for line in f:
            m = re.match(r"\s*#define\s+(ID[A-Z0-9_]+)\s+(\d+)\b", line)
            if m:
                ids[m.group(1)] = int(m.group(2))
    return ids


ID = resource_ids()

# ---- The pages: how each launcher reaches a page, and leaves it -------------------------------
#
# drawn: the drawn launcher's steps that show the page (main.cpp --script).
# win32: the Win32 window's steps that show it, in the same form as a case's choices: (control,
#        value), value None for a press. The window starts on its title page; Configure first.
# A page that is a window of its own in the Win32 launcher (SatPixie's options) is opened by a
# press of a button in OPENS, and its choices are made in that window until its OK or Cancel.
NAV_SETTINGS = [("IDC_CONFIGURE", None), ("IDC_TAB0", None), ("IDC_SUB0", None)]
PAGES = {
    "settings": dict(drawn="open settings 0", win32=NAV_SETTINGS),
    "preset": dict(drawn="open settings 1", win32=[("IDC_CONFIGURE", None), ("IDC_TAB0", None), (ID["IDC_SUB0"] + 1, None)]),
    "music": dict(drawn="open music", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 2, None)]),
    "graphics": dict(drawn="open game 0", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 3, None), ("IDC_SUB0", None)]),
    "versions-data": dict(drawn="open game 1", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 3, None), (ID["IDC_SUB0"] + 1, None)]),
    "cheats1": dict(drawn="open game 2 0", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 3, None), (ID["IDC_SUB0"] + 2, None), ("IDC_SHEET0", None)]),
    "cheats2": dict(drawn="open game 2 1", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 3, None), (ID["IDC_SUB0"] + 2, None), (ID["IDC_SHEET0"] + 1, None)]),
    "solo": dict(drawn="open game 3", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 3, None), (ID["IDC_SUB0"] + 3, None)]),
    "extras": dict(drawn="open mods 1 0", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 4, None), (ID["IDC_SUB0"] + 1, None), ("IDC_SHEET0", None)]),
    "newgame": dict(drawn="open mods 1 1", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 4, None), (ID["IDC_SUB0"] + 1, None), (ID["IDC_SHEET0"] + 1, None)]),
    "data": dict(drawn="open data", win32=[("IDC_CONFIGURE", None), (ID["IDC_TAB0"] + 5, None)]),
    # SatPixie's options: the CRT look chosen and Options... pressed; a case ends with OK or Cancel.
    "satpixie": dict(drawn="open settings 0; set look=2; press Options...",
                     win32=NAV_SETTINGS + [("IDC_FILTER", 2), ("IDC_LOOKOPTIONS", None)]),
}
# The Win32 buttons that open a window of their own, and that window's title (launcher.rc).
OPENS = {ID["IDC_LOOKOPTIONS"]: "CRT options (SatPixie)"}


def C(drawn, drawn_value, win32, win32_value):
    """One choice in both vocabularies: the drawn launcher's widget and value (None: press it),
    and the Win32 control (a resource.h name or a number) and value (None: press it)."""
    return (drawn, drawn_value, win32, win32_value)


def P(page):
    """In a case's choices: go to another page, in both launchers, before the next choice."""
    return page


# ---- The cases ---------------------------------------------------------------------------------
#
# (name, page, choices). A choice is C(...) or P(page). The values are each launcher's own: a
# combo box's entry number, a check box's 0 or 1, an edit field's text, a slider's step. The other
# pages' rows (the suites, the music source, the cheats, the solo character, New Game) are added
# here as their pages are written.
CASES = [
    ("untouched", "settings", []),
    # Settings (launcher.rc IDD_PAGE_SETTINGS).
    ("language-0", "settings", [C("language", "0", "IDC_LANGUAGE", 0)]),
    ("language-2", "settings", [C("language", "2", "IDC_LANGUAGE", 2)]),
    ("look-smooth", "settings", [C("look", "0", "IDC_FILTER", 0)]),
    ("look-sharp", "settings", [C("look", "1", "IDC_FILTER", 1)]),
    ("display-full", "settings", [C("display", "0", "IDC_DISPLAY", 0)]),
    ("wide-off", "settings", [C("wide", "0", "IDC_WIDE", 0)]),
    ("wide-16-9", "settings", [C("wide", "1", "IDC_WIDE", 1)]),
    ("snap-off", "settings", [C("Integer scaling (2x, 3x, 4x ...)", "0", "IDC_SNAP", 0)]),
    ("interp-on", "settings", [C("Smooth motion: 60 frames each second (experimental)", "1", "IDC_INTERP", 1)]),
    ("background-off", "settings", [C("Keep the game running when its window is not active", "0", "IDC_BACKGROUND", 0)]),
    ("show-off", "settings", [C("Show this window at start", "0", "IDC_SHOW", 0)]),
    # The row above the text: shown on Settings only with Extras' toggle on. Its entries are the
    # install's (None, the reading-row mods, the other held languages): an entry the install cannot
    # play is grey, and both launchers keep ruby= as read when one is picked.
    ("ruby-row-1", "extras", [C("Show the Row above the text box on Settings (furigana or a second language)", "1", "IDC_EXTRA_RUBY", 1),
                              P("settings"), C("ruby", "1", "IDC_RUBY", 1)]),
    ("ruby-row-2", "extras", [C("Show the Row above the text box on Settings (furigana or a second language)", "1", "IDC_EXTRA_RUBY", 1),
                              P("settings"), C("ruby", "2", "IDC_RUBY", 2)]),
    # Preset (IDD_PAGE_PRESET), and a preset that greys Display: another Settings choice under it.
    ("preset-repaired", "preset", [C("preset", "2", "IDC_PRESET", 2)]),
    ("preset-shipped-wide", "preset", [C("preset", "1", "IDC_PRESET", 1), P("settings"), C("wide", "0", "IDC_WIDE", 0)]),
    # Mods, Extras, Page 1 (IDD_PAGE_EXTRA).
    ("extras-ruby-ui", "extras", [C("Show the Row above the text box on Settings (furigana or a second language)", "1", "IDC_EXTRA_RUBY", 1)]),
    ("extras-text", "extras", [C("Faster text: Msg Speed Med. runs at Fast speed, and Fast shows each page at once", "1", "IDC_EXTRA_TEXTSPEED", 1)]),
    ("extras-blue", "extras", [C("Show the name of a skill that you can learn in blue", "0", "IDC_EXTRA_LEARNBLUE", 0)]),
    ("extras-faerie", "extras", [C("Add a faerie village entrance on the desert map, near Caer Xhan", "0", "IDC_EXTRA_FAERIE", 0)]),
    # Data (IDD_PAGE_DATA): the cache folder, a folder that is not there (only its text is saved).
    ("data-cache", "data", [C("cache", r"C:\bof3x-no-such-cache", "IDC_CACHE", r"C:\bof3x-no-such-cache")]),
    # SatPixie's options (IDD_SATPIXIE): each kind of box, Defaults, Cancel, an untouched OK.
    ("satpixie-untouched-ok", "satpixie", [C("OK", None, "IDOK", None)]),
    ("satpixie-sliders", "satpixie", [C("modulate", "40", "IDC_SP_MODULATE", 40), C("gamma", "8", "IDC_SP_GAMMA", 8),
                                      C("chroma", "23", "IDC_SP_CHROMA", 23), C("blurx", "3", "IDC_SP_BLURX", 3),
                                      C("blury", "20", "IDC_SP_BLURY", 20), C("OK", None, "IDOK", None)]),
    ("satpixie-toggles", "satpixie", [C("Natural vision (neutral colours, gamma above)", "0", "IDC_SP_NATURAL", 0),
                                      C("Ghosting", "1", "IDC_SP_GHOSTING", 1),
                                      C("Chromatic aberration", "0", "IDC_SP_CHROMAON", 0),
                                      C("Vignette", "0", "IDC_SP_VIGNETTE", 0),
                                      C("Vignette shaped for 4:3 only", "1", "IDC_SP_VIG43", 1),
                                      C("Interference wiggle", "1", "IDC_SP_WIGGLE", 1),
                                      C("Rolling scanlines", "0", "IDC_SP_SCANROLL", 0),
                                      C("Overscan crop (trims the picture's edges)", "1", "IDC_SP_OVERSCAN", 1),
                                      C("mask", "2", "IDC_SP_MASK", 2), C("OK", None, "IDOK", None)]),
    ("satpixie-defaults", "satpixie", [C("modulate", "10", "IDC_SP_MODULATE", 10), C("Defaults", None, "IDC_SP_DEFAULTS", None),
                                       C("blurx", "1", "IDC_SP_BLURX", 1), C("OK", None, "IDOK", None)]),
    ("satpixie-cancel", "satpixie", [C("modulate", "10", "IDC_SP_MODULATE", 10), C("Ghosting", "1", "IDC_SP_GHOSTING", 1),
                                     C("Cancel", None, "IDCANCEL", None)]),
]


# ---- The drawn side ----------------------------------------------------------------------------

def drawn_script(page, choices):
    steps = [PAGES[page]["drawn"]]
    for ch in choices:
        if isinstance(ch, str):
            steps.append(PAGES[ch]["drawn"])
            continue
        widget, value = ch[0], ch[1]
        steps.append("press %s" % widget if value is None else "set %s=%s" % (widget, value))
    return "; ".join(steps + ["press Back", "press Exit"])


def run_drawn(args, env, page, choices, save):
    if os.path.exists(save):
        os.remove(save)
    script = drawn_script(page, choices)
    r = subprocess.run([args.ui, "--check", "--ini", args.ini, "--game", args.game, "--save", save, "--script", script],
                       capture_output=True, text=True, env=env, timeout=120)
    out = (r.stdout + r.stderr).strip().splitlines()
    if r.returncode != 0:
        raise RuntimeError("the drawn launcher stopped (exit %d): %s" % (r.returncode, " | ".join(out)))
    return script, out


# ---- The Win32 side (dialog_test.py's method) --------------------------------------------------

def control_id(name):
    return ID[name] if isinstance(name, str) else name


def find_item(dlg, i):
    """A control on the window or on one of Configure's sub-pages (child dialogs), as the
    launcher's own Item finds it (dialog_test.py)."""
    h = u.GetDlgItem(dlg, i)
    page = u.FindWindowExW(dlg, None, "#32770", None)
    while not h and page:
        h = u.GetDlgItem(page, i)
        page = u.FindWindowExW(dlg, page, "#32770", None)
    return h


def class_of(h):
    b = ctypes.create_unicode_buffer(64)
    u.GetClassNameW(h, b, 64)
    return b.value


def own_window(pid, title):
    for h, t in windows_of(pid):
        if t == title:
            return h
    return None


def wait_for(pred, timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        v = pred()
        if v:
            return v
        time.sleep(0.05)
    return None


class Win32Run:
    """One run of the Win32 launcher: its window, and the window its choices go to now."""

    def __init__(self, proc, dlg):
        self.proc, self.dlg, self.target, self.target_title = proc, dlg, dlg, TITLE

    def item(self, i):
        return find_item(self.dlg, i) if self.target == self.dlg else u.GetDlgItem(self.target, i)

    def command(self, h, code, post=False):
        """The control's notification to its parent, as the control itself sends it."""
        wp = (code << 16) | (u.GetDlgCtrlID(h) & 0xFFFF)
        parent = u.GetParent(h)
        (u.PostMessageW if post else u.SendMessageW)(parent, WM_COMMAND, wp, h)

    def press(self, i):
        if i == ID["IDOK"] and self.target == self.dlg:
            raise RuntimeError("refused: IDOK on the launcher's main window is Play")
        h = self.item(i)
        if not h:
            raise RuntimeError("the window %r has no control %d" % (self.target_title, i))
        if not u.IsWindowEnabled(h):
            raise RuntimeError("the control %d is grey: the press did nothing" % i)
        style = u.GetWindowLongW(h, GWL_STYLE) & BS_TYPEMASK
        if i in OPENS:
            # A window of its own, shown modal: posted, so this tool is not held in its loop.
            self.command(h, BN_CLICKED, post=True)
            title = OPENS[i]
            w = wait_for(lambda: own_window(self.proc.pid, title))
            if not w:
                raise RuntimeError("the window %r did not open" % title)
            time.sleep(0.2)
            self.target, self.target_title = w, title
        elif self.target != self.dlg and i in (ID["IDOK"], ID["IDCANCEL"]):
            # The options window's OK or Cancel: it closes, and the choices go to the main window.
            w = self.target
            self.command(h, BN_CLICKED, post=True)
            if not wait_for(lambda: not u.IsWindow(w)):
                raise RuntimeError("the window %r did not close" % self.target_title)
            self.target, self.target_title = self.dlg, TITLE
            time.sleep(0.1)
        elif style in (BS_PUSHBUTTON, BS_DEFPUSHBUTTON):
            self.command(h, BN_CLICKED)
        else:
            u.SendMessageW(h, BM_CLICK, 0, 0)   # a tab or a radio button: pushed as the player pushes it

    def choose(self, i, value):
        h = self.item(i)
        if not h:
            raise RuntimeError("the window %r has no control %d" % (self.target_title, i))
        if not u.IsWindowEnabled(h):
            raise RuntimeError("the control %d is grey: the choice did nothing" % i)
        kind = class_of(h)
        if kind == "ComboBox":
            if not 0 <= value < u.SendMessageW(h, CB_GETCOUNT, 0, 0):
                raise RuntimeError("the combo box %d has no entry %d" % (i, value))
            u.SendMessageW(h, CB_SETCURSEL, value, 0)
            self.command(h, CBN_SELCHANGE)
        elif kind == "Button":
            if bool(u.SendMessageW(h, BM_GETCHECK, 0, 0)) != bool(value):
                u.SendMessageW(h, BM_CLICK, 0, 0)   # an auto check box: toggled, BN_CLICKED sent
        elif kind == "Edit":
            b = ctypes.create_unicode_buffer(value)
            u.SendMessageW(h, WM_SETTEXT, 0, ctypes.cast(b, ctypes.c_void_p).value)   # sends EN_CHANGE
        elif kind == "msctls_trackbar32":
            if not 0 <= value <= u.SendMessageW(h, TBM_GETRANGEMAX, 0, 0):
                raise RuntimeError("the slider %d has no step %d" % (i, value))
            u.SendMessageW(h, TBM_SETPOS, 1, value)
            u.SendMessageW(u.GetParent(h), WM_HSCROLL, SB_ENDSCROLL, h)   # its value text follows
        else:
            raise RuntimeError("the control %d is a %s: this tool does not set one" % (i, kind))

    def step(self, control, value):
        i = control_id(control)
        if value is None:
            self.press(i)
        else:
            self.choose(i, value)


def children_named(pid, name):
    """The PIDs of the processes called `name` whose parent is `pid` (Toolhelp)."""
    class PE(ctypes.Structure):
        _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD), ("th32ProcessID", W.DWORD),
                    ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", W.DWORD), ("cntThreads", W.DWORD),
                    ("th32ParentProcessID", W.DWORD), ("pcPriClassBase", ctypes.c_long), ("dwFlags", W.DWORD),
                    ("szExeFile", ctypes.c_wchar * 260)]
    k.CreateToolhelp32Snapshot.restype = W.HANDLE
    snap = k.CreateToolhelp32Snapshot(2, 0)   # TH32CS_SNAPPROCESS
    found = []
    try:
        e = PE()
        e.dwSize = ctypes.sizeof(PE)
        ok = k.Process32FirstW(snap, ctypes.byref(e))
        while ok:
            if e.th32ParentProcessID == pid and e.szExeFile.lower() == name.lower():
                found.append(e.th32ProcessID)
            ok = k.Process32NextW(snap, ctypes.byref(e))
    finally:
        k.CloseHandle(snap)
    return found


def end_pid(pid):
    """Ends one process by its exact PID."""
    h = k.OpenProcess(0x0001, False, pid)   # PROCESS_TERMINATE
    if h:
        k.TerminateProcess(h, 1)
        k.CloseHandle(h)


def run_win32(args, env, page, choices, save):
    ini = os.path.join(args.win32, "bof3x.ini")
    shutil.copyfile(args.ini, ini)
    notes = []
    proc = subprocess.Popen([os.path.join(args.win32, "bof3x-launcher.exe"), "--game", args.game, "--config"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, cwd=args.win32)
    try:
        dlg = wait_for(lambda: own_window(proc.pid, TITLE) or (proc.poll() is not None and -1), timeout=15)
        if not dlg or dlg == -1:
            raise RuntimeError("no window: %s" % proc.stderr.read().decode(errors="replace").strip())
        time.sleep(0.3)
        run = Win32Run(proc, dlg)
        steps = list(PAGES[page]["win32"])
        for ch in choices:
            steps += PAGES[ch]["win32"][1:] if isinstance(ch, str) else [(ch[2], ch[3])]   # [1:]: Configure is open
        for control, value in steps:
            run.step(control, value)
            time.sleep(0.05)
        if run.target != dlg:
            raise RuntimeError("the window %r is still open: a case on it ends with its OK or Cancel" % run.target_title)
        u.SendMessageW(dlg, WM_COMMAND, ID["IDC_BACK"], 0)   # Back: the pages applied
        configure = find_item(dlg, ID["IDC_CONFIGURE"])
        if not (u.GetWindowLongW(configure, GWL_STYLE) & WS_VISIBLE):
            raise RuntimeError("Back did not return to the title page")
        u.PostMessageW(dlg, WM_COMMAND, ID["IDCANCEL"], 0)   # Exit on the title page: saves, starts nothing
        t0 = time.time()
        while proc.poll() is None and time.time() - t0 < 15:
            for h, t in windows_of(proc.pid):   # a message box: a finding, then closed
                if t != TITLE:
                    notes.append("a window %r was shown; closed" % t)
                    u.PostMessageW(h, WM_CLOSE, 0, 0)
            time.sleep(0.05)
    finally:
        for pid in children_named(proc.pid, "bof3x.exe"):
            notes.append("a bof3x.exe (PID %d) was started by the launcher; ended" % pid)
            end_pid(pid)
        if proc.poll() is None:
            notes.append("the launcher did not end; ended by its handle")
            proc.kill()
        proc.wait(10)
    err = proc.stderr.read().decode(errors="replace").strip().splitlines()
    if os.path.exists(ini):
        shutil.move(ini, save)
    elif os.path.exists(save):
        os.remove(save)
    return notes + err


# ---- The comparison ----------------------------------------------------------------------------

def snapshot(paths):
    """Each file's size and time under the folders given, one level deep: the install is read only."""
    out = {}
    for d in paths:
        for e in os.scandir(d):
            if e.is_file():
                st = e.stat()
                out[e.path] = (st.st_size, st.st_mtime_ns)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--ui", help="bof3x-launcher-ui.exe")
    ap.add_argument("--win32", help="the ini-less launcher copy holding bof3x-launcher.exe")
    ap.add_argument("--home", help="BOF3X_HOME for both launchers (read only)")
    ap.add_argument("--game", help="the game folder (read only)")
    ap.add_argument("--ini", help="the starting bof3x.ini: a copy, never the player's own")
    ap.add_argument("--out", help="the folder for each case's two files")
    ap.add_argument("--case", action="append", help="run only this case (more than one: give it again)")
    ap.add_argument("--list", action="store_true", help="list the cases and their scripts, and run nothing")
    args = ap.parse_args()
    cases = [c for c in CASES if not args.case or c[0] in args.case]
    if args.case and len(cases) != len(set(args.case)):
        sys.exit("ui_compare: no case %s" % ", ".join(sorted(set(args.case) - {c[0] for c in CASES})))
    if args.list:
        for name, page, choices in cases:
            print("%-24s %s" % (name, drawn_script(page, choices)))
        return 0
    for need in ("ui", "win32", "home", "game", "ini", "out"):
        if not getattr(args, need):
            sys.exit("ui_compare: --%s is needed" % need)
    home = os.path.abspath(args.home)
    for p in (args.ini, args.out, args.win32):
        if os.path.abspath(p).lower().startswith(home.lower() + os.sep):
            sys.exit("ui_compare: %s is under the install (--home), which is read only" % p)
    for f in ("bof3x-launcher.exe", "bof3x.exe", "bof3x.dll"):
        if not os.path.isfile(os.path.join(args.win32, f)):
            sys.exit("ui_compare: %s is not in %s (without bof3x.exe the launcher stops on a dialog)" % (f, args.win32))
    os.makedirs(args.out, exist_ok=True)

    env = dict(os.environ)
    for key in ("BOF3X_CACHE", "BOF3X_CACHE_DATA", "BOF3X_OPT", "BOF3X_LANG", "BOF3X_EXEIMAGE", "BOF3X_IMAGEBASE"):
        env.pop(key, None)
    env["BOF3X_HOME"] = home
    watched = [d for d in (home, args.game, os.path.join(home, "cache")) if os.path.isdir(d)]
    before = snapshot(watched)

    failed = 0
    try:
        for name, page, choices in cases:
            drawn_ini = os.path.join(args.out, name + ".drawn.ini")
            win32_ini = os.path.join(args.out, name + ".win32.ini")
            try:
                script, _ = run_drawn(args, env, page, choices, drawn_ini)
                notes = run_win32(args, env, page, choices, win32_ini)
            except RuntimeError as e:
                print("%-24s NOT RUN: %s" % (name, e))
                failed += 1
                continue
            a = open(drawn_ini, "rb").read() if os.path.exists(drawn_ini) else None
            b = open(win32_ini, "rb").read() if os.path.exists(win32_ini) else None
            if a is not None and a == b:
                print("%-24s equal (%d bytes)" % (name, len(a)))
            else:
                failed += 1
                print("%-24s DIFFERENT" % name)
                if a is None or b is None:
                    print("    %s saved no file" % ("the drawn launcher" if a is None else "the Win32 launcher"))
                else:
                    for line in difflib.unified_diff(b.decode("utf-8", "replace").splitlines(),
                                                     a.decode("utf-8", "replace").splitlines(),
                                                     "win32", "drawn", lineterm="", n=0):
                        print("    " + line)
            for n in notes:
                print("    win32: " + n)
    finally:
        ini = os.path.join(args.win32, "bof3x.ini")
        if os.path.exists(ini):
            os.remove(ini)   # the copy stays ini-less
        after = snapshot(watched)
        if after != before:
            changed = sorted(set(before) ^ set(after) | {p for p in before if p in after and before[p] != after[p]})
            print("ui_compare: THE INSTALL CHANGED: %s" % ", ".join(changed))
            failed += 1
    print("ui_compare: %d case(s), %d not equal or not run" % (len(cases), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
