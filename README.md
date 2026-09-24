# Breath of Fire III — PC Port

<!-- game-art -->
<p align="center">
</p>
<!-- /game-art -->

A **living-game** renovation of the 2001 Chinese PC port of *Breath of Fire III*:
fix its bugs, modernise its platform, and get to source you can actually change.

This is a sibling to [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp),
which is the *archival* project — a static recompilation of the PlayStation
release whose correctness criterion is "matches original hardware". The two
projects have deliberately opposite invariants, and that is what makes them good
collaborators rather than competitors. See [`docs/PLAN.md`](docs/PLAN.md) §6.

> **Status: playable in English, and being replaced one function at a time.**
> A launcher starts your own copy of the game with our DLL inside
> it, and **1,025 of the port's functions now run as our C++** — everything
> the attract sequence reaches, the field, menus, text, the world map and the
> battle engine a recorded fight reaches — each checked against Capcom's
> original before it was switched on. That is roughly a tenth of the ~10,200
> functions in the binary; the rest still runs as Capcom shipped it.
>
> What is true today is in [`docs/STATUS.md`](docs/STATUS.md); every document
> is indexed in [`docs/README.md`](docs/README.md); every deliberate change to
> the game is in [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md).

## What it does for a player

Every item below is a ledgered change (`DIV-nnnn`, in
[`docs/DIVERGENCE.md`](docs/DIVERGENCE.md)) or an unmodified original feature
made reachable. Most are switches in the launcher; the originals stay available.

- **English**, built on your machine from your own US PlayStation disc:
  dialogue, menus, item and ability names, the Config screen, the title menu,
  the battle commands (DIV-0005..0009, 0013..0020). Enemy and place names and
  text painted into artwork are still Chinese.
- **Display**: a Direct3D 11 renderer in place of DirectDraw (DIV-0031); a
  resizable window or borderless fullscreen with no display-mode change
  (DIV-0032, F8 toggles); integer scaling, snapped to whole multiples or
  fitted to the height (DIV-0036, DIV-0042); **widescreen 426 x 240**, the view
  widened the way Capcom's PSP release did it (DIV-0041); FMVs in the window
  (DIV-0035).
- **Looks**: the port's smooth filter, a sharp point filter (DIV-0012), or two
  CRT looks — our own scanlines and halation (DIV-0037), and the SatPixie
  shader with an options dialog (DIV-0043).
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
  multipliers and steal-always (DIV-0045, DIV-0046).

In game: **F1** double speed · **F8** window / fullscreen · **F9** pause (twice
to return to the title, as the original) · **F11** save a screenshot beside
the DLL · **F12** Capcom's own quicksave to slot 0.

## Building and running

You need a legally owned copy of the Chinese PC port (`BOF3.exe`, 2001-04-18).
The launcher checks its SHA-256 and refuses any other build; the game files are
never modified.

**Toolchain:** [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) targeting
i686 (the only supported compiler — no MSVC, no MSYS2), `cmake` ≥ 3.25,
`ninja`, and `python` ≥ 3.11. The first configure needs the network: SDL3 is
fetched at a pinned tag and built static.

```bash
cmake --preset i686
```

```bash
cmake --build build
```

This produces `build/bof3x-launcher.exe` and `build/bof3x.dll`, which must stay
side by side. Run the launcher and point it at your install:

```bash
build/bof3x-launcher.exe --game "C:/Games/BOF3"
```

The game directory can also come from `BOF3_GAME_DIR`, or the current
directory. The launcher shows its settings dialog first (`--config` forces it,
`--no-config` skips it) and keeps its settings in `bof3x.ini` beside itself;
the game's log is `bof3x.log`, in the same place. Details:
[`docs/SCAFFOLDING.md`](docs/SCAFFOLDING.md) §4 and
[`docs/launcher-settings.md`](docs/launcher-settings.md).

**English** needs one more step, run once, reading your own US PlayStation disc
image and writing overlay files into the game's `DAT\` folder:

```bash
python tools/loc_build.py all --disc "path/to/your US disc.cue" --game "C:/Games/BOF3"
```

Then pick English in the launcher's Language box. See
[`docs/dialogue-localisation.md`](docs/dialogue-localisation.md).

**The Capcom logo video** shipped as Indeo 5, which Windows no longer decodes.
Re-encoding it to Cinepak fixes it; the one-line `ffmpeg` command is in
[`docs/DIVERGENCE.md`](docs/DIVERGENCE.md) DIV-0001.

## What the target is

The shipped `BOF3.exe` (2,584,576 bytes, 2001-04-18), measured:

- 32-bit x86, MSVC 6.0, no packing, fixed image base `0x400000` — **native code,
  there is no CPU to emulate**
- ~590,900 instructions in `.text`; ~2,950 functions found by static
  discovery, roughly 10,200 in all once functions reached only through a
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
game stays playable at every commit; when the last function is replaced, the
original binary is no longer needed.

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
  Its documented findings are cited and independently verified, the same way 
  this project treats any other source. See [`docs/PLAN.md`](docs/PLAN.md) §4.
- [**bof3ext_resources**](https://github.com/TheRealBiggs/bof3ext_resources) —
  360 files of translated text, fonts, and HD textures.
- [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp) —
  the archival sibling, built from the *PlayStation* release. Source of the
  reverse-engineering corpus this project transfers names from (~677 named
  functions), and the regression oracle the divergence ledger depends on.

All three are independent derivatives of Capcom's work — two of the 2001 PC
port, one of the 1997 PlayStation release. None is built on another.

## Legal

You need a legally owned copy of the Chinese PC port, and for English, of the
US PlayStation release. **No game data is distributed here and none may be
committed** — see [`.gitignore`](.gitignore). Everything built from game data
is built on the player's machine from the player's own copies.

This repository is licensed under [PolyForm Noncommercial 1.0.0](LICENSE).
It is an independent interoperability and preservation effort, not affiliated
with or endorsed by Capcom. Third-party code it carries (SDL3, the SatPixie
shader) is listed with its notices in
[`docs/THIRD_PARTY.md`](docs/THIRD_PARTY.md).

[`docs/LICENSING.md`](docs/LICENSING.md) explains why that licence and what
follows from it — including why no copyleft source is vendored here.

## Contributing

[`CONTRIBUTING.md`](CONTRIBUTING.md). Three rules matter most: never commit game
data, ledger every intentional behavioural change, and cite the measurement
behind every claim. Commits must be signed off (`git commit -s`); CI checks it.
