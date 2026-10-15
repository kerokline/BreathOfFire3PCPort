# Breath of Fire III — PC Port

<!-- game-art -->
<p align="center">
<img width="560" height="240" alt="Ending Ultrawide Frame" src="https://github.com/user-attachments/assets/6a5c29a9-35da-4573-9fe9-06ac0ce0a806" />
</p>
<p align ="center">
<img width="320" height="240" alt="Japanese Diary" src="https://github.com/user-attachments/assets/3859fc66-48c5-4f79-94c4-1b928aa66195" />
<img width="424" height="240" alt="English Widescreen Start Screen" src="https://github.com/user-attachments/assets/09cf6a77-eb15-44b1-bdfd-733f2033c50d" />
<img width="424" height="240" alt="French Widescreen Steal" src="https://github.com/user-attachments/assets/4ea0bba0-5c57-4886-b783-79b79dc2e4d3" />
<img width="320" height="240" alt="Chinese Whelp Fight" src="https://github.com/user-attachments/assets/c0051efb-5307-4a5b-8bb2-e76dafddc19f" /> 
</p>
<!-- /game-art -->

A renovation / overhaul of the 2001 Chinese PC port of *Breath of Fire III*:
fix its bugs, modernise its platform, and consolidate QOL features that
people have been adding over the years (simple cheats, difficulty mods, etc.)

This is a sibling to [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp),
which is an *archival* project — a static recompilation of the PlayStation
release whose correctness criterion is "matches original hardware".

> **Status: Decompilations efforts are effectively completed. The original 
> port's BOF3.exe has been replaced by our own `bof3x.exe`, which loads the 
> game with no code of Capcom's, only art and data assets. All that remains is testing.
> (state 3, [`docs/state3-build.md`](docs/state3-build.md)). 
>
> The game can be installed from the 2001 PC port, from a PlayStation disc alone, 
> or from a PSP disc alone, and is playable in English, French, German and Japanese.
>
> This doc is mostly hand-written and edited now to keep it readable - the AI written 
> status is kept in [`docs/STATUS.md`](docs/STATUS.md); every document
> is indexed in [`docs/README.md`](docs/README.md); every deliberate change to
> the game is in [`docs/DIVERGENCE.md`](docs/DIVERGENCE.md). 

## Highlights

Here's a non-exhaustive list of the things that have changed ->

- **Install from what you own**: the importer builds one game asset cache from any
  mix of the PC port, the PlayStation discs, and the PSP discs. A PlayStation disc 
  alone or a PSP disc alone is a complete install, with no PC files at all.
- **Localizations**: the game uses the scripts of the copy you install it from. Most
  of my time has been spent on English testing, a little on Japanese, and lots of
  automated testing on the Chinese. If you have multiple installs, the localization
  choice is a dropdown in the launcher.
- **Music**: when available, the game uses the PlayStation's own sequenced music
  played by a rebuilt synthesizer. You can also choose the PSP's audio or the PC's
  audio by dropdowns in the launcher. There's also a...
- **A Jukebox** in the launcher! I matched the songs to their names in the OST
  for easy searching.
- **Version Change Choice**: if you installed from both a PSX and PSP copy of the
  game, you can choose which version changes to run with. For instance, you could 
  run with the original Stallion colors, but still steal Ivory Dice from Myria.
- **Display**: choose between a PSX 4:3, PSP 16:9 (widened fully, not vertically
  cropped like the original), or a comically large 21:9 Ultrawide.
- **Looks**: the port originally used a smooth filter, so the launcher now offers
  a choice of that, a sharp point filter, or the SatPixie shader.
- **Controls**: modern pads through SDL3
- **Fixes and QOL Changes**: Oh so many! Take a look at the divergence doc if you'd 
  like to see an exhaustive list of changes that have been made. 

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

## Decompilation Target

The shipped `BOF3.exe` (2,584,576 bytes, 2001-04-18), measured:

- 32-bit x86, MSVC 6.0, no packing, fixed image base `0x400000` — **native code,
  there is no CPU to emulate**
- ~590,900 instructions in `.text`; ~2,950 functions found by static
  discovery, 10,246 starts in all once functions reached only through a
  pointer are counted.
- **101 imported symbols across 7 DLLs** — DirectDraw, DirectSound, DirectInput,
  MCI, user32/gdi32, and the MSVC6 CRT. The entire host platform to reimplement
  fits on one page.
- It is a compilation of the same C source tree as the PlayStation release,
  which is what lets names and findings transfer from the sibling project
  ([`docs/SHARED_SOURCE.md`](docs/SHARED_SOURCE.md)).

## The approach

**Incremental decompilation into a hybrid binary**, the OpenRCT2 /
OpenLoco model: load the original executable in-process, replace one function at
a time with readable C++, delete original code paths as subsystems complete. The 
last of the game's own functions was replaced in October 2026, and since then our 
`bof3x.exe` runs the game without Capcom's executable. It reads only the data tables, 
from the player's `BOF3.exe` or from a disc ([`docs/platform-layers-plan.md`](docs/platform-layers-plan.md)).

## Shoutouts

- [**bof3ext**](https://github.com/TheRealBiggs/bof3ext) and [**bof3ext_resources**](https://github.com/TheRealBiggs/bof3ext_resources) by TheRealBiggs — 
  used a replacement `ddraw.dll` that translates most of the game to English, fixes
  bugs, and replaces the renderer with OpenGL. Big inspiration for me starting
  my project [`docs/PLAN.md`](docs/PLAN.md) §4.

- [**BreathOfFire3Recomp**](https://github.com/kerokline/BreathOfFire3Recomp) —
  This is less a shoutout to myself and more to the psxrecomp team - their AI 
  tooling is what opened up the ability to go back and renovate the PC port. The
  various documentation on PSX functionality and decomp examples remains
  invaluable when pieces of the PC port were unrecoverable / shipped broken.

- [**OpenRCT2**](https://github.com/OpenRCT2/OpenRCT2/) — absolutely the coolest
  project there is, going strong since 2014. Their pattern / development plan 
  served as the guide for how this project could begin.

## Legal

**No game data is distributed here and none may be committed** — see [`.gitignore`](.gitignore).
You need a legally owned copy of the game: the Chinese PC port, or the PlayStation or PSP release.
Everything built from game data is built on the player's machine from the player's own copies.

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
