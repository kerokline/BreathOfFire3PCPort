# Launcher settings — the dialog, `bof3x.ini`, and what `START.EXE` actually is

**Status:** STABLE (built and verified 2026-09-20; extended through 2026-09-24 -
the Look box's CRT entries, snap, widescreen, cheats, controls and the pad;
2026-10-10 the Music and Cache folder boxes and the PSP extras, verified by
`tools/launcher_host/`, §3.1. §6 is the record of 2026-09-20's verification,
not re-run since. **2026-10-12: the PSP extras boxes are replaced by three
suites** - graphics, audio, data, each the PlayStation's or the PSP's -
[`launcher-suites.md`](launcher-suites.md), DIV-0107; §3.1's PSP extras item
is the record of what they replaced. **2026-10-15: the dialog is a window
with a title page and Configure's tabs** - section 2a, from the owner's
mock-ups)

`bof3x-launcher` now opens a settings dialog before it starts the game. This
document says what the shipped `START.EXE` / `SETUP.EXE` are (neither is a
settings program), why the dialog is a bare Win32 resource, where each setting
goes, and which settings could not be offered yet.

## 1. What ships on the disc, and why none of it is reusable

Measured 2026-09-20: own PE section/import/string parse of `START.EXE`, string
sweep of `BOF3.exe`, file timestamps.

**`START.EXE` (360 KB, 2001-04-18) is the CD autorun shell, not a configurator.**
Its imports are `KERNEL32` / `USER32` / `GDI32` / `ADVAPI32` only — no
DirectDraw, no DirectSound, nothing of the game's stack. It paints a `MENUBMP`
bitmap resource and `DrawTextA`s five items in 宋体, resolving the install
through the registry:

| Menu item | What it runs |
|---|---|
| 进行龙战士Ⅲ (Play) | `RegQueryValueExA` on `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\BOF3.EXE`, value `Path` → `SetCurrentDirectoryA` → `WinExec "<path>\BOF3.EXE"` |
| 安装龙战士Ⅲ (Install) | `WinExec "SETUP.EXE"` |
| 移除龙战士Ⅲ (Uninstall) | `UNINST -f"%s\UNINST.ISU"` |
| 安装DirectX7 | `Directx7\dxsetup.exe` |
| 结束 (Exit) | quit |

The two entries the owner sees greyed out are Install and Uninstall: both need
things a copied-off-disc install does not have. **It never reads or writes
`BOF3.CFG`**, and nothing in the game refers to it.

**`SETUP.EXE` and friends are InstallShield 5.** `SETUP.EXE` (1997-10-22),
`Setup.Ins`, `_INST32I.EX_`, `_ISDEL.EXE`, `_SETUP.DLL`, `lang.dat`, `os.dat`,
`setup.lid`, `DATA.TAG`, `layout.bin`, `*.cab` are that kit's standard files.
`SETUP.INI` is InstallShield's own (`AppName`, `FreeDiskSpace=470`,
`EnableLangDlg=Y`); `setup.lid` is its language picker (`0804`, zh-CN). None of
it is read by the game.

**`BOF3.exe` names exactly one configuration file in its entire string table:
`BOF3.CFG`** — the two-line file `Cfg_Load` `0x4FD030` reads
([`windowed-mode.md`](windowed-mode.md)). So there is no existing settings path
to extend, and the installer's files are not one.

Consequence for the project: our launcher replaces `START.EXE`'s role entirely.
The player's copy is left alone (`CLAUDE.md` rule 1); it is simply not used.

## 2. Why a Win32 dialog resource and not a toolkit

The dialog is a `DIALOGEX` in [`src/launcher/launcher.rc`](../src/launcher/launcher.rc),
shown with `DialogBoxParamW`, plus a manifest asking for Common Controls 6 and
DPI awareness. Nothing is vendored; until 2026-09-24 nothing was linked but
`comctl32`, and since then SDL3 is built static into it as into the DLL
([`THIRD_PARTY.md`](THIRD_PARTY.md) §2) - the Controls dialog's capture reads
the pad through the same code the game does (`src/input/pad_sdl.cpp`), and
the dialogs take the pad as navigation. Still one 32-bit executable with no
DLL beside it; 9 MB where it was 1.

Considered and rejected: **Dear ImGui** (MIT, so vendorable — but it needs its
own window and render backend, and looks non-native); **Qt / wxWidgets** (LGPL
and a large redistributable — friction on the commercial path,
[`LICENSING.md`](LICENSING.md) §4, for no gain on a twelve-control panel -
twelve when this was written; since 2026-09-23/24 the main dialog opens four
more, SatPixie's Options, Cheats, Controls and its Bind capture, all
`DIALOGEX` resources in the same `launcher.rc`, and the reasoning holds);
**WPF / WinUI** (a .NET runtime dependency); **WebView2** (an Edge runtime
dependency). A dialog resource is what a 2001-era game launcher would have
used, and here it is also the choice that vendors nothing (`CLAUDE.md` rule 5).

**Two launchers (2026-10-15).** Windows Defender quarantined the launcher as
`Behavior:Win32/DefenseEvasion.A!ml` when started from Explorer, the day it
became a windowed program. A windowed program that can write into another
process (the hosted build's DLL injection: `VirtualAllocEx`,
`WriteProcessMemory`, `CreateRemoteThread`) is what such models flag. So the
build makes two from the same sources (`CMakeLists.txt` `bof3x_add_launcher`):
**`bof3x-launcher.exe`**, the player's, without that code - its imports hold
none of those functions - and **`bof3x-launcher-dev.exe`** (`BOF3X_HOSTED=1`),
with it. Given `--host bof3` or a hosted-only switch (`BOF3X_ORIGINAL`,
`BOF3X_SHADOW`, `BOF3X_CALLTRACE`, `BOF3X_MP3=original`), the player's launcher
starts the developer's beside it with the same arguments, environment and
redirected output, and ends with its exit code (`ForwardToDevLauncher`), so
every documented harness command keeps its meaning; without one beside it -
a release ships only the player's - it stops with that sentence. Checked: a
hosted self-test (`BOF3X_SHADOW=EffectKind5B_Fade`) through the player's
launcher, output to a file and to a pipe - the developer launcher's lines,
exit 0; with no developer launcher, the refusal and exit 1;
`dialog_test.py` on the player's, its 27 checks.

## 2a. The window: a title page and Configure (2026-10-15)

From the owner's two mock-ups. One `DIALOGEX` (`IDD_CONFIG`) with two faces:

- **The title page**: the party on the dune
  ([`images/launcherBackground.png`](images/launcherBackground.png), built
  into the exe as `IDR_BACKGROUND`, a screenshot on the terms of
  [`LICENSING.md`](LICENSING.md) section 3) with the banner in its top black
  band and **Play / Configure / Jukebox / Exit** in its bottom one. The banner
  is our own lettering after the North American logo's arrangement - BREATH,
  a small OF, FIRE, a taller III - in Palatino Linotype, which every Windows
  install has (the owner's choice over Georgia and the other stock serifs; a
  bundled font was offered and not taken). Not the logo's art.
- **Configure**: the same banner over six push-like tabs - Settings,
  Controls, Music, Game Options, Mods, Data - and Back. Each tab is one or
  two child dialogs (`IDD_PAGE_*`, `kSubPages` in `config_dialog.cpp`); a tab
  with two shows a sub-tab button for each. Settings holds what the old
  dialog's Settings group did, plus "Show this window every time"; Controls
  is the old Controls dialog's grid, inline, as "Directions and buttons" and
  "Shoulders, Start and Select"; Music the song source; Game Options the
  three suites ([`launcher-suites.md`](launcher-suites.md)) as "Graphics and
  audio" and "Data", and the old Cheats dialog as "Cheats"; Data the cache
  folder and Sources.... **Mods is kept for side-loaded files** - a
  player's own DAT and EMI files over the game's, pixel art, script,
  scenario (the owner, 2026-10-15; [`IDEAS.md`](IDEAS.md) I33, "mods as
  cache layers") - and says so; nothing loads them yet. (The suites sat
  under Mods for an hour first; the owner moved them, since they choose
  between releases rather than load a player's files.) The SatPixie options, the capture,
  the Jukebox and the Sources page are still their own dialogs. Control ids
  are unique across the pages, so the code finds any of them through the
  window (`Item`, `config_dialog.cpp`), and the pages forward what their
  controls say to it.

**Size.** The picture's 640 x 480 times the display's scale (the owner,
2026-10-15): 640 x 480 at 100 %, 1280 x 960 at 200 %. Drawn without
smoothing at a whole scale and smoothed at a fractional one (125 %, 150 %),
where unsmoothed pixels would come out uneven. The pages are dialog units,
which Windows scales by the same factor, so they are laid out to fit 640 x
480 at any scale - the reason Controls and Game Options have sub-pages; a
page that did not fit would make the window larger (the picture centred)
and say so on stderr. The first build sized the window to the largest whole
multiple the monitor holds (1280 x 960 on a 1080p monitor at 100 %); the
owner asked for it to start at 640 x 480 instead.

**What is saved.** Play applies every page and saves, as before. **Back**
(or Escape, or the pad's circle) applies the pages and returns to the title
page; an Exit from there saves what they said and starts nothing
(`kDialogSave`). Exit or the close box without a Back saves nothing. The
Controls and Game Options pages are read back only once touched, so an
untouched window still writes the bindings and cheats as they were read (an
ini holding a multiplier over the cap of 10 is shown as 10 and kept as
written). Start, and Enter, still mean Play from either face.

Checked 2026-10-15, no game run, at 100 % only (this machine's scale; 200 %
is the arithmetic, not yet seen): every tab and sub-tab captured by a scratch
script (PrintWindow, as `tools/launcher_host/capture_page.py` does), each
page inside the 640 x 480 window, the Data suite's reason no longer cut off.
`dialog_test.py` finds controls through the pages now, and reads a member
line as shown from its own `WS_VISIBLE` (its page may be hidden); 27 checks
pass, the same 27 as the launcher from before this change, and both stop at
its PSX-only cache step on the disc-only refusal (DIV-0102) before the
window opens - not the window's doing. Back then Exit wrote `wide=2`; the
close box after the same change left the ini byte for byte.

## 3. Three destinations, because there are three mechanisms

| Setting | Where it goes | Mechanism |
|---|---|---|
| Language: "Chinese (PC script)", then every overlay language whose `DAT\<tag>.*` files exist - `en-US`, `en-150`, `fr-FR`, `de-DE`, `ja-JP` in that order (BCP 47 tags since 2026-10-08, `fixtures.toml`'s per build; the bare `en`, `fr`, `de`, `ja` from 2026-09-27 before, English only before that) | `BOF3X_LANG=<tag>` in the child's environment; `language=<tag>` in the ini | ours — DIV-0005, DIV-0054, DIV-0056, [`dialogue-localisation.md`](dialogue-localisation.md) |
| Look (was "Texture filter"): Smooth, Sharp, SatPixie - three entries (`config_dialog.cpp`; four until 2026-09-27, when our own CRT look, DIV-0037, was withdrawn) | `BOF3X_FILTER=point` for Sharp and SatPixie; `screen=` in the ini (`screen=crt` from an older ini reads as `satpixie`) | ours — DIV-0012 |
| Display (fullscreen/windowed) | line 1 of `<game>\BOF3.CFG` | **the original's own input**, `Cfg_Load` `0x4FD030` |
| Renderer | line 2 of `BOF3.CFG` | the original's, same reader |
| Look: SatPixie, and its Options... dialog (2026-09-23) | `BOF3X_PRESENT=satpixie`, `BOF3X_SATPIXIE=name=value,...` | ours — DIV-0043; `crt-look.md` §5 |
| Snap (2026-09-23) | `BOF3X_SNAP=0` when off | ours — DIV-0042: whole multiples, or the picture fitted to the window's height; the window is resized instead of sized here |
| Widescreen (2026-09-23) | `BOF3X_WIDE=1` when on | ours — DIV-0041 |
| Optional layers (2026-10-08): `opt=` in the ini - empty (the default), `none`, or a comma-separated list of `opt/` layer names; since 2026-10-10 the dialog's PSP extras group (§3.1), since 2026-10-12 the dialog's three suites ([`launcher-suites.md`](launcher-suites.md); up to 16 layers) | `BOF3X_OPT=<list>`, only the layers whose `DAT\<layer>.*` are installed and whose language (a `-<tag>` suffix) is the one played. **Empty** names the default layers that are installed - since 2026-10-10 `area4-walls` (DIV-0080) when `DAT\area4-walls.*.DAT` is there; **`none`** names none; a list is exactly that list (so one without `area4-walls` turns the walls off). A value it cannot honour (a character other than a letter, digit, `-` or `,`, an empty name, `none` in a list, a name twice, more than 8, one over 23 characters) **stops the launcher** with a message rather than falling back to the default | ours — DIV-0086, DIV-0080, [`opt-layers.md`](opt-layers.md) section 5 |
| Cheats... (2026-09-24): EXP and zenny sliders, steal switch | `BOF3X_EXP` / `BOF3X_ZENNY` when not 1, `BOF3X_STEAL=1` when on | ours — DIV-0045, DIV-0046; [`cheats.md`](cheats.md) |
| (Window size, removed 2026-09-23 evening) | `BOF3X_SCALE`, from `scale=` in the ini, when not 2 | the first window's size only, until the game saves `bof3x.window` (DIV-0042) |
| Keep running unfocused (2026-09-23) | `BOF3X_BACKGROUND=0` when off | ours — DIV-0033 |
| Controls... (2026-09-24): fourteen rows, two key cells and two pad cells each - click a cell and press the key or pad input, Escape cancels, Clear empties - the face-button layout | `key.NAME=action` and `pad.NAME=action` lines in the ini; `BOF3X_KEYS` / `BOF3X_PAD` when they differ from the default | ours — DIV-0050; [`controls.md`](controls.md) §4.2. Unset, the DLL leaves the game's own key table (`BOF3.CFG` lines 3+ or its default) and uses the default pad map |

| Music and Cache folder (2026-10-10, §3.1): the disc's songs or the PC's MP3s; the importer's cache, typed or Browse... | `BOF3X_MUSIC` / `BOF3X_CACHE` when not empty; `music=` / `cache=` in the ini | ours - DIV-0087, [`music-seq-engine.md`](music-seq-engine.md) 2 |

The game process inherits the launcher's environment, so the first two needed
no new channel and **no change to the DLL at all**.

**A game directory with no `BOF3.exe`** (the disc-only round, 2026-10-12):
accepted when `cache=` / `BOF3X_CACHE` names a cache (and `BOF3X_CACHE_DATA`
is not `0`) or the environment already sets `BOF3X_EXEIMAGE` - a disc-built
install. The hash check and the key table's comparison are skipped (the
default keys are the engine's rule, [`controls.md`](controls.md) section 1),
`BOF3X_EXEIMAGE` is set to the cache unless the environment set it, and
stderr says so: `no BOF3.exe in <dir>: a disc-built install - no hash check,
the default keys by the engine's rule, .data from <cache>`. Without a cache
the refusal is the old one (`no BOF3.exe in <dir>. Point --game or
BOF3_GAME_DIR at your own install.`), and the hosted build (`--host bof3`,
or a hosted-only switch) still needs the file and says why. With the file
there, everything is checked as before
([`exe-import-engine.md`](exe-import-engine.md) 2.2).

### 3.1 The Music, Cache folder and PSP extras boxes (2026-10-10)

The ini's `music=`, `cache=` and `opt=` lines existed before the boxes did
(DIV-0087, DIV-0086); the boxes write them exactly as a hand edit would, and
**an untouched dialog writes each line back as it was read**:

- **Music**: "The disc's songs where the cache has them (default)" or "The
  PC's MP3s always". The first entry is what an empty `music=` and
  `music=seq` both mean: it keeps whichever the ini said; moving the box from
  the MP3s back to it writes the default, an empty `music=`. A value that is
  neither (`music=MP3`) was already dropped to the default by `ConfigLoad`.
- **Cache folder**: the `cache=` path, typed or picked with Browse...
  (`SHBrowseForFolderW`; the launcher links `shell32` and `ole32` for it).
  Trimmed as `ConfigLoad` trims a line; the ini's bytes in the ANSI code page,
  as the launcher hands them to the game (`SetEnvironmentVariableA`) and the
  DLL reads them - a path the code page cannot hold keeps the dialog open
  with a message. A note under the box says what the game will make of it,
  by the DLL's own tests (`music_seq.cpp` `Arm`): no cache; not a folder (the
  game stops at start-up); over 226 characters (stops); no `base\bgm` (the
  MP3s play); or "Holds base\bgm: the disc's songs play where it has them".
  The dialog refuses nothing the hand edit would not: a bad path still
  reaches the DLL, which stops as before.
- **PSP extras** (the owner's call 3, [`opt-layers.md`](opt-layers.md)): a
  box for each of `psp-art`, `psp-tiles`, `psp-maps`, `psp-names-en-150`,
  `psp-names-ja-JP` (`kPspLayers`, `config.h`) whose `DAT\<layer>.*.DAT` is
  installed, packed from the group's top; a names layer's box only while the
  Language box holds its language (`LayerLanguage` / `SamePrimaryLanguage`,
  the DLL's test), and a hidden box keeps its tick. With none installed, a
  note says how to make them. A box is ticked when `opt=` plays its layer.
  On Play, `ConfigOptEdit`: no box changed - `opt=` byte for byte; else the
  layers the line played (an empty `opt=` contributing the default layers
  installed, so `area4-walls` stays on) with each change made, written in
  `kPspLayers`' order, then `kOptDefault`'s, then any other name the line had;
  `none` when nothing is left. A layer the boxes do not offer (not installed,
  another language's) stays in the line. `area4-walls` has no box: a repair
  on by default (DIV-0080), turned off by `opt=none` as before.

Verified 2026-10-10, no game run:

- `tools/launcher_host/config_tests.cpp` (`cmake -S tools/launcher_host -B
  <scratch> -G Ninja -DCMAKE_CXX_COMPILER=clang++`, then `config_tests.exe`):
  **38 checks, 0 failures** - twelve `ConfigOptEdit` cases, each result
  passing `ConfigOptValid`, and a hand-edited ini (`cache=` with spaces
  round it and a trailing `\`, `music=seq`, an `opt=` list) through
  `ConfigLoad` / `ConfigSave` twice, the second save byte-identical.
- `python tools/launcher_host/dialog_test.py build/bof3x-launcher.exe
  <install>\BOF3.exe` drives the real dialog: a temporary game directory with
  a copy of `BOF3.exe` and empty `DAT\` files naming `psp-art`, `psp-maps`,
  `psp-names-en-150`, `area4-walls` and `en-US`, a dummy `bof3x.dll` so that
  after Play the suspended game process cannot load it and the launcher ends
  it before any game code runs. **All 19 checks ok**: an untouched dialog
  leaves the ini byte-identical (with `music=seq` and a cache path, and with
  all three lines empty); the installed boxes shown and the `en-150` names
  hidden under the Chinese; `psp-art` ticked over an empty `opt=` writes
  `psp-art,area4-walls`, unticked again `area4-walls`; the Music box to the
  MP3s writes `music=mp3`, back `music=`; a cache typed with spaces is
  trimmed and noted "Not a folder"; under `en-US` the names box appears and
  `opt=none` with maps and names ticked writes `psp-maps,psp-names-en-150`;
  `psp-tiles`, listed but not installed, survives an edit; with no PSP layer
  installed the note shows and `opt=` is untouched. The owner's cache
  (`analysis\cache\pc-plus-us\`) is noted "Holds base\bgm" and written back
  with its trailing `\`.

**Writing `BOF3.CFG` is not a divergence** and has no
[`DIVERGENCE.md`](DIVERGENCE.md) entry. It is the file the 2001 program already
reads, holding values it already accepts; the launcher is doing by hand what
the owner was doing by hand. No code is patched and no behaviour is changed
that the original would not have produced from the same file.

Rules the writer follows:

- **Lines 3+ are preserved.** They are the integer pairs `0x5A9860` consumes and
  are unread by us ([`windowed-mode.md`](windowed-mode.md), Open) - *read
  since: the keyboard table, [`controls.md`](controls.md) §1*. No third line
  is ever added — the line *count* selects between `0x5A9860` and `0x5A9880`.
- **Nothing is written when there is nothing to say.** If the file is absent and
  the settings are `1`/`1`, the absent file already means that, so it stays
  absent. If the rewritten content would equal the existing content, no write
  happens.
- **The first run adopts, it does not impose.** With no `bof3x.ini` yet, display
  and renderer are seeded *from* the existing `BOF3.CFG`. This was found the
  hard way: the first build silently turned the owner's hand-written windowed
  `0` back to `1` on its first launch.
- **An environment variable already set wins** over the settings file, so the
  developer invocations in [`HANDOFF.md`](HANDOFF.md) (`BOF3X_LANG=en-US
  build/bof3x-launcher.exe`) keep overriding it.
- A setting at its default sets **no** variable, so a default run is identical
  to one launched with no settings file at all. The one default that sets a
  variable is `opt=`'s: empty, it names the default layers installed in the
  game's `DAT\` (DIV-0080's `area4-walls`), as it does with no settings file.
  `opt=` is written back as it was read (there is no dialog box for it), so a
  save never turns an empty one into a list that pins the default off. A
  harness that must not get the layer sets `BOF3X_OPT=none`.

### The pad in the launcher (2026-09-24)

With the settings dialog open the launcher holds the pad through SDL, and
stops it before the game starts. Two uses:

- **The Controls capture.** A cell is a button showing its binding; clicking
  it opens "Press a key or a pad input for Up". A keyboard hook on the
  launcher's thread takes the next key whatever has the focus - the
  scancode from the message, with the extended flag in bit 7, is
  DirectInput's code - and a timer takes the next pad input that was not
  already held when the dialog opened, in the map's positional terms with
  the face-button layout applied, so what is captured is what the game
  reads. The device decides the kind of cell: a pad press on a key cell
  goes to that row's pad cell. A key or input already in another cell moves.
  Escape cancels, Clear empties the cell. The guide button cannot be
  captured; Windows keeps it.
- **Navigation from the couch** (the owner's ask): in every dialog but the
  capture, the pad's up / down move the focus, left / right change a combo
  box or slider (else move the focus), cross clicks the focused button,
  toggles its box or cycles a combo, circle is Cancel and start is OK,
  through the launcher's own bindings. Edges only: nothing repeats while
  held. The keyboard navigates the dialogs as Windows always has.

Both confirmed by the owner with an Xbox Series X pad, 2026-09-24.

## 4. Using it

```
build/bof3x-launcher.exe --game bof3              # dialog, unless hidden
build/bof3x-launcher.exe --game bof3 --config     # dialog, always
build/bof3x-launcher.exe --game bof3 --no-config  # never; for scripted runs
```

**Double-clicking `bof3x.exe`** (2026-10-15, the owner) starts the game as
the launcher would: the game is not where the install's folders and settings
are resolved, so `bof3x.exe` started by anything but the launcher (which sets
`BOF3X_LAUNCHER`) starts the `bof3x-launcher.exe` beside it with
`--no-config` and ends (`exe_host.cpp` `HandOffToLauncher`). The game folder,
the cache, the language and the layers are then the launcher's, from that
folder's `bof3x.ini` and its `game\` and `cache\`; no settings window. That
launcher's environment carries `BOF3X_HANDOFF`, so the game never hands over
twice; with no launcher beside `bof3x.exe`, it starts as before and says what
it lacks. Checked: `bof3x.exe` started directly in a disc-built install's
home with `BOF3X_SELFTEST_ONLY=1` - handed over, the launcher's game found
`game\` and the cache (`.data from BOF3X_EXEIMAGE's cache`), self-test done.

`bof3x.ini` sits **next to the launcher**, not in the game directory — the
game directory stays the player's, `BOF3.CFG` aside. It is plain text with
comments and can be hand-edited:

As `ConfigSave` (`src/launcher/config.cpp`) writes it at the defaults,
2026-09-24 - each key has a `#` comment line above it in the real file,
left out here, and the key and pad lists are cut short:

```ini
[bof3x]
language=original          # original | en-US | en-150 | fr-FR | de-DE | ja-JP (whichever loc_build.py has built)
filter=linear              # linear | point
screen=clean               # clean | satpixie (DIV-0043); crt (DIV-0037, withdrawn) reads as satpixie
satpixie.acc_modulate=0.65 # ... fourteen satpixie.* lines, the preset's names
display=fullscreen         # fullscreen (a borderless window) | windowed -> BOF3.CFG line 1
snap=1                     # DIV-0042
scale=2                    # 2..8, the first window only, until bof3x.window exists
wide=0                     # DIV-0041
background=1               # DIV-0033
renderer=1                 # 1 | 0                  -> BOF3.CFG line 2
cheat.exp=1                # DIV-0045
cheat.zenny=1
cheat.steal=0              # DIV-0046
key.Up=up                  # ... the 24 lines of the game's default table
key.Z=triangle
pad.layout=positional      # positional | nintendo | auto
pad.south=cross            # ... eighteen pad.* lines
show_launcher=1            # 0 starts the game straight away
```

(The five-key example this replaced was the 2026-09-20 file.)

The dialog's **"Show this window every time"** box clears `show_launcher`;
`--config` is the way back, and the dialog says so on its face. Closing the
window, or pressing Exit, starts nothing and saves nothing - unless Back has
applied Configure's pages first (section 2a).

**Scripted runs inherit the settings file.** The launcher fills in
`BOF3X_LANG` and `BOF3X_FILTER` from `bof3x.ini` only when the variable is
unset or empty; a value already in the environment wins. (That was the whole
list on 2026-09-21. Since then `ConfigApplyEnvironment` fills thirteen the
same way, each only when unset and only when the setting is not at its
default: also `BOF3X_BACKGROUND`, `BOF3X_PRESENT`, `BOF3X_SATPIXIE`,
`BOF3X_SNAP`, `BOF3X_WIDE`, `BOF3X_SCALE`, `BOF3X_EXP`, `BOF3X_ZENNY`,
`BOF3X_STEAL`, `BOF3X_KEYS` and `BOF3X_PAD` - `src/launcher/config.cpp`.) So a harness that
must run a particular configuration sets both variables explicitly. The DLL
reads `BOF3X_LANG=original` as no overlay, and `BOF3X_FILTER=linear` is the
port's own filter. `tools/attract_run.py` pins both by default (`--lang`,
`--filter` to change them) and writes them into the recording's header.
Found 2026-09-21: with the owner's `language=en`, every all-ours oracle run
played the attract sequence in English, and the longer messages made the
message index lag the Chinese reference by up to 52 frames. The bisection
blamed `LoadDatFile`, because that is where the overlay walk lives.

An entry is offered only when its `DAT\<tag>.*` exists; otherwise the dialog
says to build the overlays with `tools/loc_build.py` rather than offering an
option that cannot work. An ini carried over from before 2026-10-08 says
`language=en`. Until 2026-10-10 no entry had that tag and it read, silently,
as the original until the dialog was used again; for part of 2026-10-10 the
launcher mapped the four bare codes to their tags. Since the owner's call
the same day there is **no mapping again, and the codes are retired with an
error**: an ini saying `language=en` (`fr`, `de`, `ja`), or an environment
saying `BOF3X_LANG=en`, stops the launcher before the dialog with a message
box, the same line on stderr and exit 1 - `bof3x.ini's language=en is
retired; use en-US (en-150 for the European English) (DIV-0005)` - and a
`BOF3X_LANG=en` that reaches the DLL any other way is a Fatal at injection.
Edit the line to the tag (`en-US`, `en-150`, `fr-FR`, `de-DE`, `ja-JP`) and
delete any `DAT\en.*.DAT` (`fr.`, `de.`, `ja.`) overlays: nothing reads them.
Silently dropping the player's choice was the worse surprise, and guessing
it (which English?) the next; saying so once, loudly, is neither.

**An install without the Chinese text (DIV-0102, 2026-10-12).** The
port's Chinese is in the PC's `DAT\` alone, so `original` plays only from a
PC install (`BOF3.exe` in the game directory) or with a cache holding
`loc\zh-CN` (built from the PC's `DAT\`); a disc-only install
([`disc-only-install.md`](disc-only-install.md)) has neither. There the
Language box does not offer "Chinese (PC script)" - it lists the languages
held (`DAT\<tag>.*` or the cache's `loc\<tag>\`) and its note says Chinese
is the PC install's only - and the launcher, before the dialog as for a
retired code, stops on `BOF3X_LANG=original`, on `language=original` in the
ini, and on a tag (from either) that is not held, with a message box naming
the languages held. With neither naming a language the default is the first
held, in the box's order (`en-US` first; from a US disc it is the only one),
said on stderr; the dialog's choice is checked again after it closes, under
the cache folder it may have changed. A PC install is unchanged.
`ConfigOriginalAvailable` / `ConfigResolveLanguage`
(`src/launcher/config.cpp`); the DLL's Fatal is the same rule.

## 5. What is not offered, and why

- **Resolution** was a disabled box showing `640 x 480` until the Direct3D 11
  backend. **Since 2026-09-23 it is "Window size"**: 640 x 480 (2x) to
  2560 x 1920 (8x), `scale=` in `bof3x.ini`, `BOF3X_SCALE` for the game - the
  render target of a *window*. A borderless window ignores it and takes the
  largest multiple that fits the monitor (DIV-0036, the owner's rule).
  **Removed the same evening (DIV-0042):** the window resizes freely and the
  picture follows it; `scale=` only seeds the first window, and after that
  `bof3x.window` beside the dll keeps the last placement.
- **Renderer.** `0x5A5160` holds the only reference to the `Software Render`
  string; until 2026-09-23 which value was which was not established, and the
  entries read "Default" and "Alternate". **Now traced**
  ([`window-modes.md`](window-modes.md) §4a): `0` is the synthetic software
  record - Capcom's set-up takes `0x5A60E0`'s software branch through
  `0x5AA671` and the MMX probe `0x5A9A30` - and `1` the Direct3D HAL. The
  entries read "Direct3D (default)" and "Software". Only Capcom's set-up
  reads the line (`BOF3X_ORIGINAL=Display_Setup`); ours always draws with
  Direct3D 11 (DIV-0031). The owner's `bof3x.ini` said `renderer=0` on
  2026-09-23, so any all-original run since it was set has drawn with
  Capcom's software renderer.
- **Audio, input, key bindings.** No mechanism yet. Lines 3+ of `BOF3.CFG` are
  the obvious candidate for bindings and are still unread. *Since 2026-09-24
  input and key bindings are offered - the Controls dialog, §3 (DIV-0050);
  lines 3+ turned out to be the keyboard table
  ([`controls.md`](controls.md) §1). Audio is still not offered.*

## 6. Verified 2026-09-20 (the record of that day)

On this machine, llvm-mingw 32-bit build, against the owner's install:

- Dialog renders themed and DPI-correct; all labels fit (one was clipped on the
  first build and the string was shortened).
- No `bof3x.ini`, `BOF3.CFG` = `0`,`1` → dialog opens with Display = **Windowed**:
  the seed path. Before that fix the same run rewrote the file to `1`,`1`.
- Play → `bof3x.ini` written, `BOF3.CFG` unchanged (`0`,`1`), game starts.
- `display=fullscreen` over a four-line `BOF3.CFG` → line 1 becomes `1`, lines 3
  and 4 (`12 34`, `56 78`) pass through untouched.
- `show_launcher=0`, no flags → no dialog, game starts.
- `show_launcher=0` with `--config` → dialog opens, checkbox correctly
  unchecked; language combo holds 2 entries (overlays present); Exit closes it
  without starting the game and without writing.
