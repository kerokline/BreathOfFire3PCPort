# Breath of Fire III — PC Port

<!-- game-art -->
<p align="center">
<img width="320" height="240" alt="Japanese Diary" src="https://github.com/user-attachments/assets/3859fc66-48c5-4f79-94c4-1b928aa66195" />
<img width="424" height="240" alt="English Widescreen Start Screen" src="https://github.com/user-attachments/assets/09cf6a77-eb15-44b1-bdfd-733f2033c50d" />
<img width="424" height="240" alt="French Widescreen Steal" src="https://github.com/user-attachments/assets/4ea0bba0-5c57-4886-b783-79b79dc2e4d3" />
<img width="320" height="240" alt="Chinese Whelp Fight" src="https://github.com/user-attachments/assets/c0051efb-5307-4a5b-8bb2-e76dafddc19f" /> 
</p>
<!-- /game-art -->

A **living-game** renovation of the 2001 Chinese PC port of *Breath of Fire III*:
fix its bugs, modernise its platform, and get to source you can actually change.

This is a sibling to [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp),
which is the *archival* project — a static recompilation of the PlayStation
release whose correctness criterion is "matches original hardware". The two
projects have deliberately opposite invariants, and that is what makes them good
collaborators rather than competitors. See [`docs/PLAN.md`](docs/PLAN.md) §6.

> **Status: every function of the game's own code is ours, and it no longer
> needs Capcom's executable to run.** All 10,088 functions the engine replaces
> run as our C++, each checked against Capcom's original before it was switched
> on. Our own `bof3x.exe` loads the game with no code of Capcom's mapped at all
> (state 3, [`docs/state3-build.md`](docs/state3-build.md)). The game can be
> installed from the 2001 PC port, from a PlayStation disc alone, or from a PSP
> disc alone. It is playable in English, French, German and Japanese.
>
> The disc-only and PSP-only installs are new (October 2026) and have been
> run by scripts and watched, not yet played through by hand.
>
> What is true today is in [`docs/STATUS.md`](docs/STATUS.md); every document
> is indexed in [`docs/README.md`](docs/README.md); every deliberate change to
> the game is in [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md).

## What it does for a player

Every item below is a ledgered change (`DIV-nnnn`, in
[`docs/DIVERGENCE.md`](docs/DIVERGENCE.md)) or an unmodified original feature
made reachable. Most are switches in the launcher; the originals stay available.

- **English**, built on your machine from your own US PlayStation disc:
  dialogue, menus, item, ability and enemy names, the Config screen, the title
  menu, the battle commands and banners, the world map's place plates
  (DIV-0005..0009, 0013..0020, 0052, 0053, 0055). **French, German and
  Japanese** the same way from those discs (DIV-0054, DIV-0056, DIV-0057),
  less tested so far; the faerie village's board and lists in English too
  (DIV-0064, DIV-0088). A few labels and some text painted into artwork are
  still Chinese on the PC install.
- **Install from what you own**: the importer builds one cache from any mix
  of the PC port, the US, European, French, German or Japanese PlayStation
  disc, and the European or Japanese PSP disc. A PlayStation disc alone or a
  PSP disc alone is a complete install, with no PC files at all (DIV-0096..0106,
  [`docs/disc-only-install.md`](docs/disc-only-install.md)).
- **Music**: the PlayStation's own sequenced music played from your disc by our
  synthesiser (DIV-0087), or the PC's MP3s, which now loop where the
  PlayStation's songs loop instead of rewinding to the start (DIV-0081). After
  a random battle the field song resumes where it left off, as the PlayStation
  does (DIV-0092).
- **The PSP's version, piece by piece**: the launcher's suites choose the
  PlayStation's or the PSP's graphics, audio and data, each separately, and
  the PSP's two battle-rule changes are switches (DIV-0107..0113).
- **A Jukebox** in the launcher: every song from each source you have, named
  from the official soundtrack box set.
- **Display**: a Direct3D 11 renderer in place of DirectDraw (DIV-0031); a
  resizable window or borderless fullscreen with no display-mode change
  (DIV-0032, F8 toggles); integer scaling, snapped to whole multiples or
  fitted to the height (DIV-0036, DIV-0042); **widescreen 426 x 240**, the view
  widened the way Capcom's PSP release did it (DIV-0041); FMVs in the window
  (DIV-0035).
- **Looks**: the port's smooth filter, a sharp point filter (DIV-0012), or
  a CRT look — our own scanlines and glow (DIV-0037), or the SatPixie shader
  with an options dialog (DIV-0043).
- **Controls**: modern pads through SDL3, with a Nintendo-layout toggle
  (DIV-0050); keyboard and pad bindings set in the launcher by pressing the
  input; the Config panel's controller icons as the PlayStation drew them
  (DIV-0051).
- **Fixes**: game speed no longer depends on how long Windows has been up —
  it ran at half speed after about six days — and now runs at the
  PlayStation's 29.97 (DIV-0022, DIV-0047); a crash from queued image uploads
  (DIV-0004); saves vanishing from the save menu (DIV-0002); the menu's
  clipped numerals (DIV-0010); the world map's missing compass needle
  (DIV-0044).
- **Conveniences**: keep running while the window is not in front
  (DIV-0033); F1 toggles double speed (DIV-0048); optional EXP / zenny
  multipliers and steal-always (DIV-0045, DIV-0046); the walls of area 4 taken
  from a Western PlayStation disc (DIV-0080); a fix for a sprite's feet drawn
  under a raised floor, on by default (DIV-0071; `BOF3X_LAYERING=0` restores the
  original order).

In game: **F1** double speed · **F8** window / fullscreen · **F9** pause (twice
to return to the title, as the original) · **F11** save a screenshot beside
the DLL · **F12** Capcom's own quicksave to slot 0.

## Building and running

You need a legally owned copy of the game: the Chinese PC port (`BOF3.exe`,
2001-04-18 - the launcher checks its SHA-256 and refuses any other build), or
a PlayStation or PSP disc image of *Breath of Fire III*. Nothing you own is
modified; everything the game reads is built on your machine from your own
copies.

**Toolchain:** [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) targeting
i686 (the only supported compiler - no MSVC, no MSYS2), `cmake` >= 3.25,
`ninja`, and `python` >= 3.11. The first configure needs the network: SDL3 is
fetched at a pinned tag and built static. `ffmpeg` on `PATH` is optional; the
importer uses it for the opening movie and for the PSP's sound.

```bash
cmake --preset i686
```

```bash
cmake --build build
```

This produces `build/bof3x-launcher.exe`, `build/bof3x.dll` and
`build/bof3x.exe`, which must stay side by side.

**From the PC port.** Run the launcher and point it at your install:

```bash
build/bof3x-launcher.exe --game "C:/Games/BOF3"
```

The game directory can also come from `BOF3_GAME_DIR`, or the current
directory. The launcher shows its settings dialog first (`--config` forces it,
`--no-config` skips it) and keeps its settings in `bof3x.ini` beside itself;
the game's log is `bof3x.log`, in the same place. It starts our `bof3x.exe`;
`--host bof3` starts your `BOF3.exe` with our DLL injected instead, the way
the project ran before October 2026. Details:
[`docs/SCAFFOLDING.md`](docs/SCAFFOLDING.md) §4 and
[`docs/launcher-settings.md`](docs/launcher-settings.md).

**English and the other languages** come from a PlayStation disc. The importer
builds a cache from your PC install and the disc, and the launcher reads it
(the Cache folder box, or `cache=` in `bof3x.ini`). The preset for the US disc
is below; `pc-plus-eu-en-text`, `pc-plus-fr-text`, `pc-plus-de-text` and
`pc-plus-jp-text` take the other discs:

```bash
python tools/importer.py build --preset pc-plus-us-text --source "C:/Games/BOF3/DAT" --source "C:/Games/BOF3/BOF3.exe" --source "path/to/your US disc.cue" --out "C:/bof3-cache"
```

Then pick the language in the launcher's Language box. Add a PSP disc as one
more `--source` for the PSP's art, music and data as options. See
[`docs/importer.md`](docs/importer.md) and
[`docs/launcher-suites.md`](docs/launcher-suites.md).

**From a PlayStation or PSP disc alone**, with no PC files: build the cache
from the disc, lay a game directory from it, and point the launcher at both.
Full steps and what such an install lacks are in
[`docs/disc-only-install.md`](docs/disc-only-install.md):

```bash
python tools/importer.py build --source "path/to/your disc.cue" --lang en-US --out "C:/bof3-cache"
```

```bash
python tools/importer.py install --base --cache "C:/bof3-cache" --game "C:/bof3-game" --lang en-US
```

**The Capcom logo video** on the PC port shipped as Indeo 5, which Windows no
longer decodes. Re-encoding it to Cinepak fixes it; the one-line `ffmpeg`
command is in [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md) DIV-0001. A
disc-only install converts the disc's own movie instead (DIV-0099).

## What the target is

The shipped `BOF3.exe` (2,584,576 bytes, 2001-04-18), measured:

- 32-bit x86, MSVC 6.0, no packing, fixed image base `0x400000` — **native code,
  there is no CPU to emulate**
- ~590,900 instructions in `.text`; ~2,950 functions found by static
  discovery, 10,246 starts in all once functions reached only through a
  pointer are counted ([`docs/attract-remaining.md`](docs/attract-remaining.md) §3)
- **101 imported symbols across 7 DLLs** — DirectDraw, DirectSound, DirectInput,
  MCI, user32/gdi32, and the MSVC6 CRT. The entire host platform to reimplement
  fits on one page.
- It is a compilation of the same C source tree as the PlayStation release,
  which is what lets names and findings transfer from the sibling project
  ([`docs/SHARED_SOURCE.md`](docs/SHARED_SOURCE.md)).

## The approach

Not static recompilation, despite the sibling project's name. Static recomp
preserves behaviour *by construction* — it is an archival technique, and this
project's deliverable is deliberate divergence.

Instead, **incremental decompilation into a hybrid binary**, the OpenRCT2 /
OpenLoco model: load the original executable in-process, replace one function at
a time with readable C++, delete original code paths as subsystems complete. The
game stayed playable at every commit. The last of the game's own functions was
replaced in October 2026, and since then our `bof3x.exe` runs the game without
Capcom's executable. It reads only the data tables, from the player's
`BOF3.exe` or from a disc
([`docs/platform-layers-plan.md`](docs/platform-layers-plan.md)).

Every takeover is checked before it counts: a differential fuzz against a
byte-copy of Capcom's function with negative controls, then live runs against
all-original code — a deterministic attract sequence and recorded input routes
compared frame by frame, a memory dump, and a per-frame call hash. Any
function can be switched back to Capcom's with `BOF3X_ORIGINAL`
([`docs/SCAFFOLDING.md`](docs/SCAFFOLDING.md),
[`docs/call-trace.md`](docs/call-trace.md)).

## Related work

- [**bof3ext**](https://github.com/TheRealBiggs/bof3ext) and [**bof3ext_resources**](https://github.com/TheRealBiggs/bof3ext_resources) by TheRealBiggs — a
  replacement `ddraw.dll` that translates most of the game to English, fixes
  bugs, and replaces the renderer with OpenGL. A **peer project, not a base**:
  its documented findings are cited and independently verified, the same way
  this project treats any other source. See [`docs/PLAN.md`](docs/PLAN.md) §4.
- [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp) —
  the archival sibling, built from the *PlayStation* release. Source of the
  reverse-engineering corpus this project transfers names from (~677 named
  functions), and the regression oracle the divergence ledger depends on.

All three are independent derivatives of Capcom's work — two of the 2001 PC
port, one of the 1997 PlayStation release. None is built on another.

## Legal

You need a legally owned copy of the game: the Chinese PC port, or the
PlayStation or PSP release, and for a language the PC port lacks, that
release's disc. **No game data is distributed here and none may be
committed** — see [`.gitignore`](.gitignore). Everything built from game data
is built on the player's machine from the player's own copies.

This repository is licensed under [PolyForm Noncommercial 1.0.0](LICENSE).
It is an independent interoperability and preservation effort, not affiliated
with or endorsed by Capcom. Third-party code it carries (SDL3, the SatPixie
shader, minimp3) is listed with its notices in
[`docs/THIRD_PARTY.md`](docs/THIRD_PARTY.md).

[`docs/LICENSING.md`](docs/LICENSING.md) explains why that licence and what
follows from it — including why no copyleft source is vendored here.

## Contributing

[`CONTRIBUTING.md`](CONTRIBUTING.md). Three rules matter most: never commit game
data, ledger every intentional behavioural change, and cite the measurement
behind every claim. Commits must be signed off (`git commit -s`); CI checks it.
