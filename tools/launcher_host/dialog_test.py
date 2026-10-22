"""Drive the launcher's settings dialog (src/launcher/config_dialog.cpp) without starting the game.

Three fake caches - PSP-only, PSX-only, and PC + PSX + PSP - each a manifest.toml as
tools/importer.py writes it, with the [build] keys the PSP-only round's contract fixes
(docs/psp-only-round.md section 4: base_policy, exe_tables_from), and empty opt\\<layer>\\
dat\\ files naming the layers built. Builds, names and made-up hashes only: no game data.

The PC + PSX + PSP game directory holds a copy of the player's BOF3.exe (the launcher checks
its hash; the copy is deleted at the end); the disc-built ones hold none, as a disc-only
install. Beside the launcher, bof3x.dll is a dummy and bof3x.exe a copy of Windows'
hostname.exe, so Play starts nothing of the game (the launcher starts bof3x.exe by default
since state 3; until 2026-10-12 this test predated that and found no dialog). The ini is
read after each run. Needs a desktop session (the dialog is shown).
docs/launcher-settings.md section 3, docs/launcher-suites.md.

    python tools/launcher_host/dialog_test.py build/bof3x-launcher.exe <the install>/BOF3.exe [--shots DIR]

Since the reading-row round (docs/mods.md): the Mods tab and the Settings page's Row above the
text box, against a home (BOF3X_HOME) whose runtime\tools holds this checkout's tools/mods.py,
run on this Python (BOF3X_PYTHON), and mod zips mods.py's own self-test makes - no game data.
--shots writes the Settings page and the Mods tab as the player sees them (PrintWindow).
"""
import ctypes, os, shutil, subprocess, sys, tempfile, time
from ctypes import wintypes as W

u = ctypes.WinDLL("user32", use_last_error=True)
u.FindWindowW.restype = W.HWND
u.FindWindowW.argtypes = [W.LPCWSTR, W.LPCWSTR]
u.GetDlgItem.restype = W.HWND
u.GetDlgItem.argtypes = [W.HWND, ctypes.c_int]
u.FindWindowExW.restype = W.HWND
u.FindWindowExW.argtypes = [W.HWND, W.HWND, W.LPCWSTR, W.LPCWSTR]
u.GetWindowLongW.argtypes = [W.HWND, ctypes.c_int]
u.SendMessageW.restype = ctypes.c_ssize_t
u.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
u.IsWindowVisible.argtypes = [W.HWND]
u.IsWindowEnabled.argtypes = [W.HWND]
u.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]

WM_COMMAND, WM_CLOSE, CB_GETCURSEL, CB_SETCURSEL, BM_GETCHECK, BM_CLICK = 0x111, 0x10, 0x147, 0x14E, 0xF0, 0xF5
CBN_SELCHANGE = 1
IDOK = 1
IDC_LANGUAGE, IDC_MUSIC, IDC_CACHE, IDC_CACHENOTE, IDC_SUITENOTE = 1001, 1014, 1015, 1017, 1018
IDC_WIDE = 1009   # DIV-0041: off, 16:9, 21:9 (wide= 0, 1, 2; docs/ultrawide-frame.md)
CB_GETCOUNT = 0x146
IDC_SUITE0, STRIDE, PSX, PSP, MEMBER0, WHY = 1040, 10, 1, 2, 3, 7
SUITES = ["graphics", "audio", "data"]

MANIFEST = {
    "psp-only": """[meta]
target = "pc-zh"

[build]
base_policy = "psp"
standins_sha256 = "66"
exe_tables_from = "psp-jp"

[[source]]
build = "psp-jp"
path = "Breath of Fire III (PSP) (Japan).iso"

[cache]
assets = [
  ["AREA000.DAT", 0, "base", "psp-jp:own:WORLD00/AREA000.EMI#7", "00"],
]
bgm = [
  ["base/bgm/000.DAT", "psp-jp:ppms:SOUND/BGM.BIN#0", "11"],
]

[snd]
build = "psp-jp"
how = "psp-at3"
files = [
  ["MAGIC/000_00.WAV", "22"],
]

[exe]
build = "psp-jp"
file = "base/exe/data.bin"
""",
    "psx-only": """[meta]
target = "pc-zh"

[build]
base_policy = "psx"
exe_tables_from = "psx-us"

[[source]]
build = "psx-us"
path = "Breath of Fire III (USA).cue"

[cache]
assets = [
  ["AREA000.DAT", 0, "base", "psx-us:copy", "00"],
]
bgm = [
  ["base/bgm/000.DAT", "psx-us:seq:BGM000.EMI#1", "11"],
]

[exe]
build = "psx-us"
file = "base/exe/data.bin"
""",
    "all": """[meta]
target = "pc-zh"

[build]
base_policy = "psx"
exe_tables_from = ["pc-zh"]

[[source]]
build = "pc-zh"
path = "DAT"

[[source]]
build = "pc-zh"
path = "BOF3.exe"

[[source]]
build = "psx-us"
path = "Breath of Fire III (USA).cue"

[[source]]
build = "psp-eu"
path = "Breath of Fire III (PSP) (Europe).iso"

[[source]]
build = "psp-jp"
path = "Breath of Fire III (PSP) (Japan).iso"

[cache]
assets = [
  ["AREA000.DAT", 0, "base", "pc-zh:chunk", "00"],
]
opt = [
  ["AREA067.DAT", "opt/psp-art", 0, "psp-eu:copy:WORLD01/AREA067.EMI#6@192", "55"],
  ["AREA067.DAT", "opt/psp-tiles", 0, "psp-eu:copy:WORLD01/AREA067.EMI#6@192", "55"],
  ["AREA067.DAT", "opt/psp-names-en-150", 0, "psp-eu:copy:WORLD01/AREA067.EMI#6@192", "55"],
  ["AREA067.DAT", "opt/area4-walls", 0, "psx-us:copy:WORLD00/AREA004.EMI#8@0", "55"],
]
bgm = [
  ["base/bgm/000.DAT", "psx-us:seq:BGM000.EMI#1", "11"],
]

[exe]
build = "pc-zh"
file = "base/exe/data.bin"
""",
}
# The layers each cache was built with (opt\<layer>\dat\<file>), and the game's DAT\ files. Each
# is listed in its manifest's [cache] opt rows above, as importer.py writes it: since the code
# review of 2026-10-12 a layer tree the manifest does not list is not offered.
CACHE_LAYERS = {"psp-only": [], "psx-only": [], "all": ["psp-art", "psp-tiles", "psp-names-en-150", "area4-walls"]}
# The language layers each cache holds (loc\<tag>\dat\). A disc-only install needs one or the
# launcher stops before the window (DIV-0102); until 2026-10-22 these caches had none and the
# test stopped there (the reading-row round's ML found it so on main too).
CACHE_LOC = {"psp-only": ["ja-JP"], "psx-only": ["en-US"], "all": []}
# The reading-row round (docs/mods.md): a PC + PSX cache holding ja-JP, en-US and de-DE, and the
# layers ruby-furigana-ja-JP (a mod's) and ruby-en-US-ja-JP (a second language's) built - each
# listed in the manifest as tools/mods.py lists one. ruby-de-DE-ja-JP is not built.
MANIFEST["ruby"] = MANIFEST["all"].replace("opt = [\n", "opt = [\n"
    '  ["AREA014.DAT", "opt/ruby-furigana-ja-JP", 0, "mod:furigana@0.1.0:ja-JP", "77"],\n'
    '  ["AREA014.DAT", "opt/ruby-en-US-ja-JP", 0, "second:en-US:ja-JP", "77"],\n')
CACHE_LAYERS["ruby"] = CACHE_LAYERS["all"] + ["ruby-furigana-ja-JP", "ruby-en-US-ja-JP"]
CACHE_LOC["ruby"] = ["ja-JP", "en-US", "de-DE"]
TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IDC_RUBY, IDC_TAB_MODS, IDC_TAB_SETTINGS = 1012, 1026, 1022
IDC_MOD_LIST, IDC_MOD_REMOVE, IDC_MOD_PATH, IDC_MOD_ADD, IDC_MOD_STATUS, IDC_MOD_LOG = 1700, 1701, 1702, 1704, 1705, 1706

SCR = tempfile.mkdtemp(prefix="bof3x_dialog_")
L = os.path.join(SCR, "dlg", "launcher")
G = os.path.join(SCR, "dlg", "game")


def cache_dir(name):
    return os.path.join(SCR, "c_" + name)


def setup(launcher, exe, dat, caches=()):
    """A fresh launcher folder and game folder: `exe` copied in when given, `dat` empty
    DAT\\ files; and each named fake cache."""
    shutil.rmtree(os.path.join(SCR, "dlg"), ignore_errors=True)
    os.makedirs(L); os.makedirs(os.path.join(G, "DAT"))
    shutil.copy(launcher, L)
    open(os.path.join(L, "bof3x.dll"), "wb").write(b"not a dll")
    shutil.copy(os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "hostname.exe"),
                os.path.join(L, "bof3x.exe"))
    if exe:
        shutil.copy(exe, G)
    for name in dat:
        open(os.path.join(G, "DAT", name), "wb").close()
    for c in caches:
        d = cache_dir(c)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        open(os.path.join(d, "manifest.toml"), "w", newline="\n").write(MANIFEST[c])
        for layer in CACHE_LAYERS[c]:
            os.makedirs(os.path.join(d, "opt", layer, "dat"))
            open(os.path.join(d, "opt", layer, "dat", "AREA067.DAT"), "wb").close()
        for tag in CACHE_LOC[c]:
            os.makedirs(os.path.join(d, "loc", tag, "dat"))
            open(os.path.join(d, "loc", tag, "dat", "AREA000.DAT"), "wb").close()


def find_item(dlg, i):
    """A control on the window or on one of Configure's sub-pages (child dialogs,
    docs/launcher-settings.md section 2a), as the launcher's own Item finds it."""
    h = u.GetDlgItem(dlg, i)
    page = u.FindWindowExW(dlg, None, "#32770", None)
    while not h and page:
        h = u.GetDlgItem(page, i)
        page = u.FindWindowExW(dlg, page, "#32770", None)
    return h


def text(h):
    # WM_GETTEXT, which the system marshals across processes (GetWindowText reads
    # another process's edit control as empty)
    b = ctypes.create_unicode_buffer(1024)
    u.SendMessageW(h, 0x000D, 1024, ctypes.cast(b, ctypes.c_void_p).value)
    return b.value


def settext(h, s):
    b = ctypes.create_unicode_buffer(s)
    u.SendMessageW(h, 0x000C, 0, ctypes.cast(b, ctypes.c_void_p).value)  # WM_SETTEXT, sends EN_CHANGE


def shown(h):
    """The launcher left the line shown (WS_VISIBLE on the line itself): its sub-page may be
    hidden behind the title page, which IsWindowVisible would count."""
    return bool(u.GetWindowLongW(h, -16) & 0x10000000)   # GWL_STYLE, WS_VISIBLE


def suites(item):
    """Each suite's radios (enabled, checked, label), its member lines (text, enabled; the
    shown ones), and its reason line."""
    out = {}
    for i, s in enumerate(SUITES):
        b = IDC_SUITE0 + STRIDE * i
        radio = lambda k: (bool(u.IsWindowEnabled(item(b + k))), u.SendMessageW(item(b + k), BM_GETCHECK, 0, 0),
                           text(item(b + k)))
        members = [(text(item(b + MEMBER0 + k)), bool(u.IsWindowEnabled(item(b + MEMBER0 + k))))
                   for k in range(4) if shown(item(b + MEMBER0 + k))]
        out[s] = dict(psx=radio(PSX), psp=radio(PSP), members=members, why=text(item(b + WHY)))
    return out


def click(item, suite, side):
    u.SendMessageW(item(IDC_SUITE0 + STRIDE * SUITES.index(suite) + side), BM_CLICK, 0, 0)


# ---- The list box's and the combo box's strings: LB_GETTEXT and CB_GETLBTEXT are system
# messages, marshalled across processes as WM_GETTEXT is (both boxes keep their strings,
# LBS_HASSTRINGS / CBS_HASSTRINGS), so a local buffer is read.
LB_GETTEXT, LB_GETCOUNT, LB_GETITEMDATA, LB_SETCURSEL = 0x189, 0x18B, 0x199, 0x186
CB_GETLBTEXT, CB_GETITEMDATA = 0x148, 0x150


def items(h, combo):
    """[(text, itemdata)] of a list box or combo box in the launcher."""
    out = []
    for i in range(max(u.SendMessageW(h, CB_GETCOUNT if combo else LB_GETCOUNT, 0, 0), 0)):
        b = ctypes.create_unicode_buffer(2048)
        u.SendMessageW(h, CB_GETLBTEXT if combo else LB_GETTEXT, i, ctypes.cast(b, ctypes.c_void_p).value)
        out.append((b.value, u.SendMessageW(h, CB_GETITEMDATA if combo else LB_GETITEMDATA, i, 0)))
    return out


def wait_idle(item, timeout=60):
    """The Mods page's child finished: Add enabled again and the status no longer says it is working."""
    t0 = time.time()
    time.sleep(0.3)
    while time.time() - t0 < timeout:
        st = text(item(IDC_MOD_STATUS))
        if u.IsWindowEnabled(item(IDC_MOD_ADD)) and not st.startswith(("Checking", "Building")):
            time.sleep(0.3)
            if u.IsWindowEnabled(item(IDC_MOD_ADD)):
                return True
        time.sleep(0.1)
    return False


SHOTS = None


def shot(dlg, name):
    if SHOTS:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import capture_page
        os.makedirs(SHOTS, exist_ok=True)
        time.sleep(0.5)   # the page drawn
        capture_page.grab(dlg, os.path.join(SHOTS, name + ".png"))


def run(ini, act, env=None):
    """Write the ini, open the dialog, read its state, apply act(dlg, item), press Play; the ini after."""
    p = os.path.join(L, "bof3x.ini")
    if ini is None:
        if os.path.exists(p): os.remove(p)
    else:
        open(p, "w", newline="").write(ini)
    e = dict(os.environ)
    for k in ("BOF3X_CACHE", "BOF3X_CACHE_DATA", "BOF3X_OPT", "BOF3X_LANG", "BOF3X_EXEIMAGE"):
        e.pop(k, None)
    e.update(env or {})
    proc = subprocess.Popen([os.path.join(L, "bof3x-launcher.exe"), "--game", G, "--config"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=e)
    try:
        return drive(proc, p, act)
    except BaseException:
        if proc.poll() is None:
            proc.kill()   # our own launcher, by its handle: never left open on a failed step
        raise


def drive(proc, p, act):
    dlg = None
    for _ in range(200):
        dlg = u.FindWindowW(None, "Breath of Fire III - bof3x")
        if dlg or proc.poll() is not None: break
        time.sleep(0.05)
    if not dlg:
        proc.kill()   # our own process, by its handle
        raise AssertionError("no dialog: %s" % proc.stderr.read().decode(errors="replace").strip())
    time.sleep(0.3)
    item = lambda i: find_item(dlg, i)
    state = dict(music=u.SendMessageW(item(IDC_MUSIC), CB_GETCURSEL, 0, 0), cache=text(item(IDC_CACHE)),
                 cachenote=text(item(IDC_CACHENOTE)), note=text(item(IDC_SUITENOTE)), suites=suites(item))
    if act:
        act(dlg, item)
        time.sleep(0.2)
        state["after"] = dict(cachenote=text(item(IDC_CACHENOTE)), note=text(item(IDC_SUITENOTE)), suites=suites(item))
    u.PostMessageW(dlg, WM_COMMAND, IDOK, 0)
    for _ in range(200):   # a message box, if the launcher stops: close it
        if proc.poll() is not None: break
        box = u.FindWindowW(None, "bof3x-launcher")
        if box: u.PostMessageW(box, WM_CLOSE, 0, 0)
        time.sleep(0.05)
    if proc.poll() is None:
        proc.kill()
    proc.wait(10)
    err = proc.stderr.read().decode(errors="replace").strip().splitlines()
    return state, open(p).read() if os.path.exists(p) else None, err


def lines(ini):
    return {k: v for k, _, v in (l.partition("=") for l in ini.splitlines() if l and l[0] not in "#[;")}


def mods_checks(launcher, exe, report):
    """The reading-row round (docs/mods.md): the Mods tab and the Row above the text box."""
    sys.path.insert(0, TOOLS)
    import mods
    zips = os.path.join(SCR, "zips")
    with contextlib_quiet():
        mods.selftest(zips)            # makes the zips (and checks them) in SCR\zips
    setup(launcher, exe, [], ["ruby"])
    # A home beside the launcher's own (BOF3X_HOME): runtime\tools holds mods.py and what it
    # imports; importer.py there is only the marker the launcher finds the runtime by.
    home = os.path.join(SCR, "home")
    shutil.rmtree(home, ignore_errors=True)
    rt = os.path.join(L, "runtime", "tools")
    os.makedirs(rt)
    for f in ("mods.py", "language_tags.py"):
        shutil.copy(os.path.join(TOOLS, f), rt)
    with open(os.path.join(rt, "importer.py"), "w") as f:
        f.write("# the launcher's marker for this runtime (sources_dialog.cpp FindRuntime); never run by this test\n"
                "raise SystemExit('dialog_test: importer.py is not part of this test')\n")
    # The home's own cache, what mods.py builds into: the ruby cache less the mod's layer.
    hc = os.path.join(home, "cache")
    shutil.copytree(cache_dir("ruby"), hc)
    shutil.rmtree(os.path.join(hc, "opt", "ruby-furigana-ja-JP"))
    mods._manifest_rows(hc, "ruby-furigana-ja-JP", [])
    env = {"BOF3X_HOME": home, "BOF3X_PYTHON": sys.executable}
    seen = {}

    # 1. The Mods tab: empty; a good zip added (the builder absent: refused loudly); a .DAT zip refused.
    def act_mods(dlg, item):
        u.SendMessageW(item(IDC_TAB_MODS), BM_CLICK, 0, 0)
        seen["listed"] = wait_idle(item)
        seen["empty"] = text(item(IDC_MOD_STATUS))
        settext(item(IDC_MOD_PATH), os.path.join(zips, "good.zip"))
        u.SendMessageW(item(IDC_MOD_ADD), BM_CLICK, 0, 0)
        seen["added"] = wait_idle(item)
        seen["status_add"] = text(item(IDC_MOD_STATUS))
        settext(item(IDC_MOD_PATH), os.path.join(zips, "holds_dat.zip"))
        u.SendMessageW(item(IDC_MOD_ADD), BM_CLICK, 0, 0)
        wait_idle(item)
        seen["log"] = text(item(IDC_MOD_LOG))
        seen["rows"] = items(item(IDC_MOD_LIST), False)
        shot(dlg, "mods_tab_not_built")
        u.SendMessageW(item(IDC_TAB_SETTINGS), BM_CLICK, 0, 0)
        seen["ruby_original"] = items(item(IDC_RUBY), True)
    run("[bof3x]\nopt=\ncache=\nlanguage=original\n", act_mods, env)
    report("Mods tab, no mods: the status says so", seen.get("listed") and seen.get("empty") == "No mods installed.",
           repr(seen.get("empty")))
    report("  a good zip added; its build refused loudly, naming the builder",
           "Added: Japanese (Furigana) 0.1.0" in seen.get("log", "") and "ruby_build.py" in seen.get("log", "")
           and "not built" in seen.get("status_add", ""), "%r | %r" % (seen.get("status_add"), seen.get("log")))
    report("  a zip holding a .DAT refused with the reason", "Not added: AREA014.DAT: game data" in seen.get("log", ""))
    rows = seen.get("rows", [])
    report("  listed: name, version, kind, not built - greyed",
           len(rows) == 1 and rows[0][0].startswith("Japanese (Furigana)\t0.1.0\treading-row\tnot built") and rows[0][1] == 1,
           repr(rows))
    with open(os.path.join(home, "mods.toml"), encoding="utf-8") as f:
        report("  mods.toml lists it; mods\\furigana holds its two files",
               'id = "furigana"' in f.read() and sorted(os.listdir(os.path.join(home, "mods", "furigana")))
               == ["mod.toml", "readings.toml"])
    report("  the language played the Chinese: every row entry greyed",
           all(d == 0 for _, d in seen.get("ruby_original", [])[1:]) and seen.get("ruby_original", [("", 0)])[0] == ("None", 1),
           repr(seen.get("ruby_original")))

    # The builder FB writes is not on this branch: mods.py's self-test's stand-in (one kind-18
    # chunk) builds the layer into the home's cache, through the same step setup runs.
    built, _ = mods.setup_step(home, hc, lambda t, quiet=False: None,
                               builder=os.path.join(zips, "fake_builder", "ruby_build.py"))
    report("the mod's step with a stand-in builder: ruby-furigana-ja-JP built into the home's cache",
           built == ["ruby-furigana-ja-JP"], repr(built))
    # 2. The box under ja-JP: None; the mod (its layer built in the cache named); English (built);
    #    German (not built, greyed). Each choice's ruby= written.
    langs = None
    for pick, want in ((1, "furigana"), (2, "en-US"), (3, None), (0, "")):
        got = {}

        def act_box(dlg, item, pick=pick, got=got):
            u.SendMessageW(item(IDC_TAB_SETTINGS), BM_CLICK, 0, 0)
            got["entries"] = items(item(IDC_RUBY), True)
            u.SendMessageW(item(IDC_RUBY), CB_SETCURSEL, pick, 0)
            u.SendMessageW(dlg, WM_COMMAND, (CBN_SELCHANGE << 16) | IDC_RUBY, item(IDC_RUBY))
            got["sel"] = u.SendMessageW(item(IDC_RUBY), CB_GETCURSEL, 0, 0)
            if pick == 1:
                shot(dlg, "settings_row_above_the_text")
                u.SendMessageW(item(IDC_TAB_MODS), BM_CLICK, 0, 0)
                wait_idle(item)
                got["rows"] = items(item(IDC_MOD_LIST), False)
                shot(dlg, "mods_tab_built")
        st, ini, err = run("[bof3x]\nopt=\ncache=\nlanguage=ja-JP\nruby=\n", act_box, env)
        e = got.get("entries", [])
        if langs is None:
            langs = e
            report("ja-JP: the box lists None, the mod, English, German",
                   [t.split(" - ")[0] for t, _ in e] == ["None", "Japanese (Furigana) (mod 0.1.0)",
                                                         "English (US PlayStation script)", "German (PlayStation script)"]
                   and [d for _, d in e] == [1, 1, 1, 0] and "not built" in e[3][0], repr(e))
        line = lines(ini).get("ruby")
        if want is None:
            report("  German picked (greyed): not taken, ruby= stays empty", line == "" and got.get("sel") == 0,
                   "%r %r" % (line, got.get("sel")))
        else:
            report("  entry %d picked: ruby=%s" % (pick, want), line == want, repr(line))
        if pick == 1:
            r = got.get("rows", [])
            report("  the Mods tab with the layer built under ja-JP: built, not greyed",
                   len(r) == 1 and "\tbuilt: choose it in Settings" in r[0][0] and r[0][1] == 0, repr(r))
    # 3. Under en-US the mod's row is greyed with its base; the box offers it greyed too.
    got = {}

    def act_en(dlg, item):
        u.SendMessageW(item(IDC_TAB_MODS), BM_CLICK, 0, 0)
        wait_idle(item)
        got["rows"] = items(item(IDC_MOD_LIST), False)
        shot(dlg, "mods_tab_other_language")
        got["entries"] = items(item(IDC_RUBY), True)
    st, ini, err = run("[bof3x]\nopt=\ncache=\nlanguage=en-US\nruby=furigana\n", act_en, env)
    r, e = got.get("rows", []), got.get("entries", [])
    report("en-US played: the mod's row greyed, its base named", len(r) == 1 and "over ja-JP text only" in r[0][0]
           and r[0][1] == 1, repr(r))
    report("  the box: the mod greyed (over ja-JP only), Japanese and German not built",
           len(e) == 4 and e[1][1] == 0 and "over ja-JP text only" in e[1][0] and e[2][1] == 0 and e[3][1] == 0, repr(e))
    report("  ruby=furigana untouched: written back as read", lines(ini).get("ruby") == "furigana", lines(ini).get("ruby"))
    report("  stderr: the layer not there for en-US, no row", any("ruby=furigana, but layer ruby-furigana-en-US is not there"
                                                                  in x for x in err), " | ".join(err[-3:]))
    # 4. Remove from the tab: the row, the folder and the layer built from it go.
    got = {}

    def act_rm(dlg, item):
        u.SendMessageW(item(IDC_TAB_MODS), BM_CLICK, 0, 0)
        wait_idle(item)
        u.SendMessageW(item(IDC_MOD_LIST), LB_SETCURSEL, 0, 0)
        u.SendMessageW(dlg, WM_COMMAND, (1 << 16) | IDC_MOD_LIST, item(IDC_MOD_LIST))   # LBN_SELCHANGE
        u.SendMessageW(item(IDC_MOD_REMOVE), BM_CLICK, 0, 0)
        wait_idle(item)
        got["rows"] = items(item(IDC_MOD_LIST), False)
        got["entries"] = items(item(IDC_RUBY), True)
    run("[bof3x]\nopt=\ncache=\nlanguage=ja-JP\n", act_rm, env)
    report("Remove: the list empty, mods\\furigana and the home cache's layer gone, the box without it",
           got.get("rows") == [] and not os.path.exists(os.path.join(home, "mods", "furigana"))
           and not os.path.exists(os.path.join(home, "cache", "opt", "ruby-furigana-ja-JP"))
           and not any("Furigana" in t for t, _ in got.get("entries", [])), repr(got))
    return True


class contextlib_quiet:
    """stdout swallowed (mods.py's self-test prints a line a check)."""
    def __enter__(self):
        import io
        self.old, sys.stdout = sys.stdout, io.StringIO()

    def __exit__(self, *a):
        out, sys.stdout = sys.stdout.getvalue(), self.old
        if "0 failure(s)" not in out:
            print(out)


def main():
    global SHOTS
    if "--shots" in sys.argv:
        i = sys.argv.index("--shots")
        SHOTS = os.path.abspath(sys.argv[i + 1])
        del sys.argv[i:i + 2]
    launcher, exe = sys.argv[1], sys.argv[2]
    ok = True

    def report(name, cond, detail=""):
        nonlocal ok
        ok &= bool(cond)
        print("%-66s %s %s" % (name, "ok" if cond else "FAIL", detail))

    # ---- PC + PSX + PSP: the PC's BOF3.exe, a cache from all three, three layers built.
    setup(launcher, exe, ["area4-walls.AREA004.DAT", "en-US.AREA000.DAT"], ["all", "psx-only", "psp-only"])
    C = cache_dir("all")
    # 1. A fresh ini as the launcher writes it, then an untouched dialog: byte-identical.
    _, first, _ = run("[bof3x]\nopt=\ncache=%s\nmusic=seq\n" % C, None)
    st, again, err = run(first, None)
    report("PC+PSX+PSP untouched: the ini byte-identical", again == first)
    report("  music=seq shown as the default entry", st["music"] == 0, str(st["music"]))
    report("  the note: the PlayStation's base, the cache's builds",
           "base is the PlayStation's" in st["note"] and "psp-eu, psp-jp" in st["note"], st["note"])
    g, a, d = (st["suites"][s] for s in SUITES)
    report("  graphics: the PlayStation's checked, the PSP's offered",
           g["psx"][:2] == (True, 1) and g["psp"][:2] == (True, 0) and "the base" in g["psx"][2], str(g["psx"]) + str(g["psp"]))
    report("  graphics: psp-maps (not built) greyed, art and tiles not",
           g["members"] == [("Stallion's palettes", True), ("area tiles and palettes", True), ("map bands", False)]
           and "--opt psp-maps" in g["why"], str(g["members"]) + " " + g["why"])
    report("  audio: the PSP's greyed, its layers not built (TG's, --opt psp-snd; the music is the Music box's)",
           a["psp"][:2] == (False, 0) and "--opt psp-snd" in a["why"] and not any(m[1] for m in a["members"]), a["why"])
    report("  data: the PSP's offered (the renames), tables and P8 greyed",
           d["psp"][0] and [m[1] for m in d["members"]] == [False, True, False, False]
           and "plays under English" in d["why"], str(d["members"]) + " " + d["why"])
    # 2. The PSP's graphics over an empty opt=: its two layers, the walls kept.
    st, ini, _ = run("[bof3x]\nopt=\ncache=%s\n" % C, lambda dlg, item: click(item, "graphics", PSP))
    report("the PSP's graphics: opt=psp-art,psp-tiles,area4-walls", lines(ini)["opt"] == "psp-art,psp-tiles,area4-walls",
           lines(ini)["opt"])
    # 3. Read back as the PSP's; the PlayStation's chosen again: the walls alone.
    st, ini, _ = run(ini, lambda dlg, item: click(item, "graphics", PSX))
    report("  shown as the PSP's when opt= has both", st["suites"]["graphics"]["psp"][:2] == (True, 1),
           str(st["suites"]["graphics"]["psp"]))
    report("the PlayStation's again: opt=area4-walls", lines(ini)["opt"] == "area4-walls", lines(ini)["opt"])
    # 4. An ini from the PSP extras boxes: psp-art alone is a partial suite, kept as written.
    st, ini, _ = run("[bof3x]\nopt=psp-art,area4-walls\ncache=%s\n" % C, None)
    gs = st["suites"]["graphics"]
    report("opt=psp-art alone: neither radio, the reason says kept",
           gs["psx"][1] == 0 and gs["psp"][1] == 0 and "kept as written" in gs["why"], gs["why"])
    report("  untouched: opt= byte for byte", lines(ini)["opt"] == "psp-art,area4-walls", lines(ini)["opt"])
    # 5. English chosen and the PSP's data: the renames, and their note gone.
    def act5(dlg, item):
        u.SendMessageW(item(IDC_LANGUAGE), CB_SETCURSEL, 1, 0)
        u.SendMessageW(dlg, WM_COMMAND, (CBN_SELCHANGE << 16) | IDC_LANGUAGE, item(IDC_LANGUAGE))
        click(item, "data", PSP)
        click(item, "audio", PSP)   # greyed: a click does nothing
    st, ini, _ = run("[bof3x]\nopt=none\ncache=%s\n" % C, act5)
    l = lines(ini)
    report("en-US and the PSP's data over opt=none: psp-names-en-150", l["opt"] == "psp-names-en-150", l["opt"])
    report("  the renames' language note gone under en-US", "plays under" not in st["after"]["suites"]["data"]["why"],
           st["after"]["suites"]["data"]["why"])
    report("  language=en-US written", l["language"] == "en-US", l["language"])
    # 6. The music and cache boxes, as before; a cache typed in redraws the suites.
    def act6(dlg, item):
        u.SendMessageW(item(IDC_MUSIC), CB_SETCURSEL, 2, 0)   # DIV-0150: psx, psp, pc
        settext(item(IDC_CACHE), "  C:\\nowhere at all  ")
    st, ini, _ = run("[bof3x]\nopt=\ncache=%s\nmusic=\n" % C, act6)
    l = lines(ini)
    report("music moved to the PC's MP3s: music=pc (DIV-0150)", l["music"] == "pc", l["music"])
    report("cache typed with spaces: trimmed as a line is", l["cache"] == "C:\\nowhere at all", l["cache"])
    report("  its note: not a folder", "Not a folder" in st["after"]["cachenote"], st["after"]["cachenote"])
    report("  the suites redrawn: no manifest there", "No manifest.toml" in st["after"]["note"]
           and not st["after"]["suites"]["graphics"]["psp"][0], st["after"]["note"])
    report("  opt= untouched", l["opt"] == "", l["opt"])
    # 6b. The Widescreen box (DIV-0041): three entries, wide= shown and written back; 21:9 chosen.
    for w in ("0", "1", "2"):
        seen = {}
        def look(dlg, item, seen=seen):
            seen["n"] = u.SendMessageW(item(IDC_WIDE), CB_GETCOUNT, 0, 0)
            seen["sel"] = u.SendMessageW(item(IDC_WIDE), CB_GETCURSEL, 0, 0)
        st, ini, _ = run("[bof3x]\nopt=\ncache=%s\nwide=%s\n" % (C, w), look)
        report("wide=%s: three entries, entry %s shown, written back" % (w, w),
               seen.get("n") == 3 and seen.get("sel") == int(w) and lines(ini).get("wide") == w,
               "%s %s %s" % (seen.get("n"), seen.get("sel"), lines(ini).get("wide")))
    def pick21(dlg, item):
        u.SendMessageW(item(IDC_WIDE), CB_SETCURSEL, 2, 0)
    st, ini, _ = run("[bof3x]\nopt=\ncache=%s\nwide=1\n" % C, pick21)
    report("21:9 chosen over wide=1: wide=2", lines(ini).get("wide") == "2", lines(ini).get("wide"))
    # 7. The same game, the PSX-only cache named: every PSP side greyed, naming a PSP disc.
    st, ini, _ = run("[bof3x]\nopt=\ncache=%s\n" % cache_dir("psx-only"), lambda dlg, item: click(item, "graphics", PSP))
    report("PSX-only cache: every PSP side greyed, PlayStation checked",
           all(s["psp"][:2] == (False, 0) and s["psx"][:2] == (True, 1) for s in st["suites"].values()))
    report("  each names the PSP discs", all("needs a PSP disc (psp-jp or psp-eu)" in s["why"] for s in st["suites"].values()),
           st["suites"]["audio"]["why"])
    report("  a click on a greyed side writes nothing", lines(ini)["opt"] == "", lines(ini)["opt"])

    # ---- Disc-built installs, no BOF3.exe: PSX-only and PSP-only.
    setup(launcher, None, [], ["psx-only", "psp-only"])
    st, ini, err = run("[bof3x]\nopt=\ncache=%s\n" % cache_dir("psx-only"), None)
    report("PSX-only install: the dialog opens without BOF3.exe", st is not None)
    report("  every PSP side greyed, the PlayStation's the base",
           all(s["psp"][:2] == (False, 0) and s["psx"][:2] == (True, 1) and "the base" in s["psx"][2]
               for s in st["suites"].values()))
    report("  the launcher took .data from the cache", any("a disc-built install" in e for e in err), " | ".join(err[-2:]))
    st, ini, err = run("[bof3x]\nopt=\ncache=%s\n" % cache_dir("psp-only"),
                       lambda dlg, item: [click(item, s, PSX) for s in SUITES] + [click(item, "data", PSP)])
    report("PSP-only install: the note says the PSP's base", "base is the PSP's" in st["note"], st["note"])
    report("  every suite the PSP's, checked and enabled, the PlayStation's greyed",
           all(s["psp"][:2] == (True, 1) and "the base" in s["psp"][2] and s["psx"][:2] == (False, 0)
               for s in st["suites"].values()))
    report("  the reason names the PlayStation discs",
           all("needs a PlayStation disc (psx-jp, psx-us, psx-fr, psx-de or psx-eu-en)" in s["why"]
               for s in st["suites"].values()), st["suites"]["graphics"]["why"])
    report("  every member the base's but the English renames (psp-eu's, the cache psp-jp's)",
           [m for s in st["suites"].values() for m in s["members"] if not m[1]] == [("English renames", False)]
           and "English renames: needs psp-eu" in st["suites"]["data"]["why"], st["suites"]["data"]["why"])
    report("  clicks write nothing: opt= stays empty", lines(ini)["opt"] == "", lines(ini)["opt"])

    # ---- A PC install with no cache, psp-art installed in DAT\ (the PSP extras' path).
    setup(launcher, exe, ["psp-art.AREA067.DAT", "area4-walls.AREA004.DAT"])
    st, ini, _ = run("[bof3x]\nopt=\n", lambda dlg, item: click(item, "graphics", PSP))
    report("no cache: the note says so", "No cache named" in st["note"], st["note"])
    report("  graphics offered for the installed psp-art", st["suites"]["graphics"]["psp"][0]
           and st["suites"]["graphics"]["members"][0] == ("Stallion's palettes", True))
    report("  the PSP's graphics: opt=psp-art,area4-walls", lines(ini)["opt"] == "psp-art,area4-walls", lines(ini)["opt"])
    st, ini, _ = run("[bof3x]\nopt=\n", None)
    report("  audio's PSP side names the discs and a cache",
           "in a cache named in Cache folder" in st["suites"]["audio"]["why"], st["suites"]["audio"]["why"])

    mods_checks(launcher, exe, report)   # its reports count in ok

    shutil.rmtree(SCR, ignore_errors=True)
    print("all ok" if ok else "FAILURES")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
