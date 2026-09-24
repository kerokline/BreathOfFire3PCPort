# Launcher settings — the dialog, `bof3x.ini`, and what `START.EXE` actually is

**Status:** STABLE (built and verified 2026-09-20; extended through 2026-09-24 -
the Look box's CRT entries, snap, widescreen, cheats, controls and the pad.
§6 is the record of 2026-09-20's verification, not re-run since)

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

## 3. Three destinations, because there are three mechanisms

| Setting | Where it goes | Mechanism |
|---|---|---|
| Language | `BOF3X_LANG` in the child's environment | ours — DIV-0005, [`dialogue-localisation.md`](dialogue-localisation.md) |
| Look (was "Texture filter"): Smooth, Sharp, CRT, SatPixie - four entries (`config_dialog.cpp`) | `BOF3X_FILTER=point` for Sharp and both CRT looks; `screen=` in the ini | ours — DIV-0012 |
| Look: CRT - scanlines and glow (2026-09-23) | `BOF3X_PRESENT=crt` (`screen=crt`) | ours — DIV-0037; [`crt-look.md`](crt-look.md) |
| Display (fullscreen/windowed) | line 1 of `<game>\BOF3.CFG` | **the original's own input**, `Cfg_Load` `0x4FD030` |
| Renderer | line 2 of `BOF3.CFG` | the original's, same reader |
| Look: SatPixie, and its Options... dialog (2026-09-23) | `BOF3X_PRESENT=satpixie`, `BOF3X_SATPIXIE=name=value,...` | ours — DIV-0043; `crt-look.md` §5 |
| Snap (2026-09-23) | `BOF3X_SNAP=0` when off | ours — DIV-0042: whole multiples, or the picture fitted to the window's height; the window is resized instead of sized here |
| Widescreen (2026-09-23) | `BOF3X_WIDE=1` when on | ours — DIV-0041 |
| Cheats... (2026-09-24): EXP and zenny sliders, steal switch | `BOF3X_EXP` / `BOF3X_ZENNY` when not 1, `BOF3X_STEAL=1` when on | ours — DIV-0045, DIV-0046; [`cheats.md`](cheats.md) |
| (Window size, removed 2026-09-23 evening) | `BOF3X_SCALE`, from `scale=` in the ini, when not 2 | the first window's size only, until the game saves `bof3x.window` (DIV-0042) |
| Keep running unfocused (2026-09-23) | `BOF3X_BACKGROUND=0` when off | ours — DIV-0033 |
| Controls... (2026-09-24): fourteen rows, two key cells and two pad cells each - click a cell and press the key or pad input, Escape cancels, Clear empties - the face-button layout | `key.NAME=action` and `pad.NAME=action` lines in the ini; `BOF3X_KEYS` / `BOF3X_PAD` when they differ from the default | ours — DIV-0050; [`controls.md`](controls.md) §4.2. Unset, the DLL leaves the game's own key table (`BOF3.CFG` lines 3+ or its default) and uses the default pad map |

The game process inherits the launcher's environment, so the first two needed
no new channel and **no change to the DLL at all**.

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
  developer invocations in [`HANDOFF.md`](HANDOFF.md) (`BOF3X_LANG=en
  build/bof3x-launcher.exe`) keep overriding it.
- A setting at its default sets **no** variable, so a default run is identical
  to one launched with no settings file at all.

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

`bof3x.ini` sits **next to the launcher**, not in the game directory — the
game directory stays the player's, `BOF3.CFG` aside. It is plain text with
comments and can be hand-edited:

As `ConfigSave` (`src/launcher/config.cpp`) writes it at the defaults,
2026-09-24 - each key has a `#` comment line above it in the real file,
left out here, and the key and pad lists are cut short:

```ini
[bof3x]
language=original          # original | en
filter=linear              # linear | point
screen=clean               # clean | crt (DIV-0037) | satpixie (DIV-0043)
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
dialog, or pressing Exit, starts nothing and saves nothing.

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

The English entry is offered only when `DAT\en.*` exists; otherwise the dialog
says to build the overlays with `tools/loc_build.py` rather than offering an
option that cannot work.

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
