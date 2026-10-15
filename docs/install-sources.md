# The player's sources: identify, list, set up - the launcher's contract

**Status:** IN PROGRESS (2026-10-15, branch `install/importer-side`: the
importer half built and run on every kind of source held; the launcher's
Sources page is being written against it on another branch; **nothing here
played** - the installs it makes are the disc-only / PSP-only / PC installs
the earlier rounds measured)

The streamlined install the owner asked for on 2026-10-15: the launcher gets
a **Sources** page; the player picks a path, it is checked against the known
releases, the first source creates the install and later ones are added to
the list, and the install is rebuilt from the list. A release zip carries a
bundled Python so the player installs nothing
([`releases/README.md`](releases/README.md)). This document is the importer's
half: the commands the launcher runs, the folder they write, how a source is
identified, and what was measured.

The owner's decisions (2026-10-15): bundle python.org's embeddable Python now
and port the importer to C++ later (IDEAS I39); one home folder beside the
launcher; ffmpeg is the player's (on `PATH`, or a located `ffmpeg.exe`), with
a link to <https://ffmpeg.org/download.html#build-windows>.

## 1. Where it lives

| File | What |
|---|---|
| `tools/player_sources.py` | `identify` for the launcher, `sources.toml`, `setup` |
| `tools/importer.py` | the commands' argument parsing (`identify --json`, `sources`, `setup`); `build` and `install` do the work, unchanged |
| `tools/bof3x_home.py` | the one home resolver for the tools (section 2) |
| `tools/make_release.py` | the release zip (section 5) |

## 2. The home folder

```
<home>\bof3x-launcher.exe  bof3x.dll  bof3x.exe  music-names.tsv  bof3x.ini
<home>\runtime\python\python.exe       the bundled interpreter
<home>\runtime\tools\importer.py       and the 24 other scripts it reaches
<home>\runtime\fixtures.toml, fixtures\, recipes\, symbols.toml, tables.toml, exe_maps\
<home>\sources.toml                     the player's sources, in the build's order
<home>\cache\                           the data cache (importer.py build)
<home>\game\                            the game folder the launcher starts
<home>\setup.log                        the last setup's own lines
```

`bof3x_home.home()` is `BOF3X_HOME` when set, else the release layout (the
script in `<home>\runtime\tools\`, so home is two folders up), else `None` -
a repository checkout, where every tool keeps the default it had (`build\`,
`bof3\`, `CDImage\`, `~\abc`); a flag or an older variable still wins.
`importer.py`'s `ROOT` (the folder above `tools\`) is `runtime\` in a release,
which is why the release copies the data the tools read there: no code path
changes between a checkout and a release. `input_run.py`, `attract_run.py`
(`--game`, `--launcher`) and `abc_run.py` (`--build`) take their repo-path
defaults through it; in a checkout they resolve to exactly the old paths.

## 3. `sources.toml` and the order

```toml
[[source]]
path = "C:\\Games\\BOF3"
build = "pc-zh"
kind = "pc-folder"
label = "PC, the 2001 Chinese release"
added = "2026-10-15T10:00:27"
```

Written only by `sources add` / `remove`. **The importer orders it by role**:
the PC game first (its chunks are the recipe's target, and a language layer
is built against its `DAT\` and `BOF3.exe`), PlayStation discs next, PSP discs
last (a PSP disc fills only what the others leave, as `resolve_preset` already
rules); within a role, the order added - so the first PlayStation disc added
is a disc-only install's base (its `base/exe/`, its stand-ins). One source per
build: a second copy of the same release is refused.

## 4. The contract (what the launcher runs)

The launcher runs `<home>\runtime\python\python.exe <home>\runtime\tools\importer.py ...`
with `PYTHONUTF8=1`, `PYTHONIOENCODING=utf-8`, and `BOF3X_FFMPEG` when the
player located `ffmpeg.exe`; it reads stdout and stderr merged. In these modes
stdout is UTF-8 and carries only the lines below; anything else (a Python
warning on stderr) is shown in a log box.

| Command | Output | Exit |
|---|---|---|
| `identify --json PATH...` | one JSON object a line per path | 0 all usable, 1 otherwise |
| `sources --home H list --json` | one identify-shaped object a line, in list order; `ok` false and a `problem` when the path is gone | 0 |
| `sources --home H add PATH --json` | one identify-shaped object (the stored, absolute path when added) | 0 added, 1 refused |
| `sources --home H remove PATH` | `removed: PATH` or `not in the list: PATH` | 0 / 1 |
| `setup --home H --json-progress [--ffmpeg EXE]` | `progress <done> <total> <text>` lines, then `done game=<path> cache=<path>`, or `error <sentence>` | 0 / 1 |

The object: `{"path", "ok", "build", "kind", "label", "problem"}` - `build` a
`fixtures.toml` id or null, `kind` one of `pc-exe`, `pc-folder`, `psx`, `psp`
or null, `label` short text ("PlayStation, USA"), `problem` null or one
sentence for the player. Without `--json`, `identify` prints what it always
printed. `identify --home H` (optional) unpacks a `.zip` under
`H\cache\tmp`, else under `BOF3X_HOME`'s, else the system's temp.

`add` of a `BOF3.exe` adds the folder that holds it when that folder is a
complete PC game (a file dialog's natural pick), and refuses otherwise.

`setup` reruns several `progress` lines with the same `<done>` while the
cache builds (each language layer, the movie's conversion) and warnings as
`progress` lines that do not advance (a long path; no ffmpeg, naming the
link). Every line is flushed.

### 4.1 What is a source, and the answers

| Given | Identified by | Usable when |
|---|---|---|
| A folder with `BOF3.exe` and `DAT\` (`pc-folder`) | `BOF3.exe`'s sha256 and the `DAT\` tree's per-file manifest (`[A-Z0-9_]+\.DAT`, so our overlays beside it do not count) against `pc-zh` | both match; otherwise the sentence says which part does not (and how many of the 742 DAT files) |
| `BOF3.exe` (`pc-exe`) | whole-file sha256 | identify: matches; add: its folder is a complete `pc-folder` |
| A `.cue` / `.bin` (`psx`) | the ISO 9660 tree file by file against the five PSX manifests | the tree matches and the image is raw (2,352-byte sectors) |
| A PSP `.iso` (`psp`) | the tree against the two PSP manifests | the tree matches |
| A `.zip` of any of these | first, without unpacking: a member whose size and sha256 are a catalogued data track or PSP image; else the `.cue` and the data track it names (or the lone `.iso`/`.bin`, or the whole folder holding `BOF3.exe`) unpacked to the temp folder, identified, deleted | as above |
| `.chd`, `.cso` / `.zso` (by extension or magic) | - | never yet: "not supported yet", convert back (IDEAS I40) |
| `.7z`, `.rar`, ... ; `.ecm`, `.pbp`, `.mdf`, `.nrg` ... | - | never: "only .zip archives are read" / "this image format is not read" |

An unmatched disc says what it is when it can: its serial from `SYSTEM.CNF`
(PSX) or `UMD_DATA.BIN` (PSP); a serial of a catalogued release with files
that differ is "looks like the USA PlayStation disc (SLUS-00422), but 1 of its
887 files differ ... (a patched or damaged image?)"; an uncatalogued serial
(the Japanese demo, `SLPM-80115`) is "not a catalogued release ... (a demo or
another game?)".

**The 2,048-byte PlayStation `.iso`, measured.** Made from the US `.bin` by
keeping each 2,352-byte sector's 2,048 bytes at offset 24 (what a ripper's
"ISO" mode writes): its tree **matches** `psx-us` file by file, because
`fixtures/*.files.tsv` hashes every file, `.STR` and `.XA` included, as
`psx_disc.py` reads them - 2,048 bytes a sector (`fixtures.toml`'s note). The
lost bytes are the XA audio sectors' last 276 (form 2 carries 2,324) and every
sector's subheader, which `xa.sectors` needs (`XA needs a raw (2,352-byte
sector) image`) and the movie needs too (`fmv_build` skips a cooked image). So
`identify` says it honestly - the files match, the format has lost the audio
and movie sectors, use the `.bin`/`.cue` - and returns `ok: false`, and `add`
refuses it.

## 5. `setup`

From `sources.toml`, into `H\cache.new` and `H\game.new`, swapped into place
only when everything succeeded:

1. Unpack each `.zip` source into `H\cache\tmp` (the `.cue` and its data
   track; nothing else).
2. `importer.py build` with every source in the list's order (a PC folder
   gives two: its `DAT\` and its `BOF3.exe`), the language layers below, and
   the default `opt` layers (`area4-walls` whenever a Western PSX disc is
   there, DIV-0080) - no others: the PSP's layers are the launcher's suites'
   to choose ([`launcher-suites.md`](launcher-suites.md)). Its own lines go
   to `H\setup.log`.
3. With a PC folder: copy `BOF3.exe`, the 742 shipped `DAT\` containers,
   `BGM\`, `SND\`, `logos.avi` and `capcom.avi` into the new game folder; the
   player's folder is only read (saves `BISLPS*.DAT` and `BOF3.CFG` are not
   copied). A `capcom.avi` that is the shipped Indeo 5 file (`fixtures.toml`
   `capcom_avi`) is re-encoded by DIV-0001's own command when ffmpeg is there,
   else copied as it is (the game's start test needs the file; without the
   repair it shows Windows' "video not available" field) and the progress says
   so.
4. `importer.py install`: over the copied PC folder, the language and default
   layers; with no PC folder, `install --base` (the disc-only / PSP-only
   install, [`disc-only-install.md`](disc-only-install.md),
   [`psp-install.md`](psp-install.md)) and the layers.
5. Swap: each current folder to `<name>.old`, the new one into place, the old
   ones deleted. If a rename fails (the game still running, a file open) the
   moved folders are put back and `error` says so; a failure at any earlier
   step deletes the new folders. Either way the previous install is left
   working.

**The languages chosen.** Every PlayStation disc's language (its
`fixtures.toml` tag: `en-US`, `en-150`, `fr-FR`, `de-DE`, `ja-JP`); without a
PC folder, every PSP disc's too (a disc-only build reads a PSP disc's text,
[`loc-build-psp.md`](loc-build-psp.md); beside the PC's `DAT\` `loc_build.py`
reads PSX discs only, so a PC + PSP install gets no PSP language). Each tag
once, in the list's order. The PC's own Chinese is the PC's `DAT\`.

**ffmpeg.** `--ffmpeg`, else `BOF3X_FFMPEG`, else the `PATH`'s (`at3.find_ffmpeg`,
as `build` resolves it). A given path that is not there is an `error`. With
none, `build` already skips only what needs it - the disc's logo movie
(`fmv_build`), the PSP's ATRAC3plus sounds (`at3.importer_snd`) - and setup
names them in one `progress` line with the link.

**The path.** MCI fails to open a movie at about 160 characters
([`disc-only-install.md`](disc-only-install.md) section 1): when
`H\game\CAPCOM.AVI` is longer than 150 characters, a `progress` warning says
so before anything is built.

## 6. Measured (2026-10-15, this machine)

Every run below on the owner's images in the main checkout's `CDImage\`
and `bof3\`, read in place; the homes, caches and installs in the session's
scratch folder (game data, deleted afterwards - rule 1); ffmpeg 9.0.1 on the
`PATH` unless stated. Times are wall clock on this machine, one run each
unless two are given.

**`identify --json`** (the JSON lines exactly as section 4 shows them):

| Source | Answer | Time |
|---|---|---|
| US `.cue`, JP `.cue`, EU `.cue`, DE Track 1 `.bin` | `psx-us`, `psx-jp`, `psx-eu-en`, `psx-de`, ok | 16-21 s the first read of an image, 1-3 s with it in the file cache |
| US `.zip`, FR `.zip`, PSP-EU `.zip` | ok, by the member's sha256 without unpacking | 1.6, 1.6, 3.1 s |
| PSP-JP `.iso` | `psp-jp`, ok | 2.4 s |
| `bof3\` (the PC folder), `bof3\BOF3.exe` | `pc-folder` / `pc-exe`, ok | 2.2 / 1.2 s |
| `bof3\DAT` | refused: "This is the game's DAT folder; choose the folder above it" | 1.7 s |
| `bof3\logos.avi`, `capcom.avi` | refused: a part of a release, not a source | 1 s |
| The US `.bin` with one byte flipped inside `BIN/BATTLE/BATL_SE.EMI` | refused: "looks like the USA PlayStation disc (SLUS-00422), but 1 of its 887 files differ" | 3.2 s |
| The Japanese demo, `.cue` and `.zip` | refused: `SLPM-80115` "is not a catalogued release" | 2.8 s |
| A 2,048-byte `.iso` of the US disc (section 4.1) | refused: its files match `psx-us`, the audio and movie sectors are lost | 2.9 s |
| `Breath of Fire III (EMU).zophar.zip` (PSF music rips) | refused: no disc image and no game folder in it | under 1 s |
| EU Track 2 `.bin` (CD audio) | refused: no readable file system | 1.5 s |
| `CDImage\` (a folder of images) | refused: no BOF3.exe and DAT; choose the image file itself | under 1 s |

**`sources`**: adding the PSP-JP `.iso`, the US `.cue` and `bof3\BOF3.exe`
in that order listed them PC, PSX, PSP (the exe stored as its folder); the US
`.zip` after the US `.cue` was refused as a duplicate of `psx-us`; the
2,048-byte `.iso` refused with its sentence; `remove` of a listed path exit 0,
of an unlisted one exit 1.

**`setup --json-progress`**:

| Home's sources | Time | Result |
|---|---|---|
| US `.cue` | 243 s; 227 s again over the first install | `game\` 400 MB: 742 + 254 `en-US.*` + `area4-walls.AREA004.DAT`, `SND\` 880, `BGM\` empty, `CAPCOM.AVI` (the disc's, by ffmpeg); `cache\` 423 MB |
| US `.zip` | 239 s | the same `game\` file for file, the same cache but the manifest's `built` time; `cache\tmp` gone |
| `bof3\` + US `.cue` | 177 s; 171 s again | `game\` 539 MB: `BOF3.exe`, the 742 shipped containers byte-identical to the player's, 247 `en-US.*` overlays **byte-identical to the owner's own install's 247**, `area4-walls`, `BGM\` 166, `SND\` 893, both movies as the folder has them; `cache\` 428 MB |
| PSP-JP `.iso` | 249 s; 238 s again | `game\` 410 MB: 742 + 254 `ja-JP.*`, no `area4-walls` (a Western PSX disc's), `SND\` 891 (ATRAC3plus by ffmpeg), the PSP's `CAPCOM.AVI`; `cache\` 435 MB |
| PSP-JP `.iso`, **no ffmpeg on the PATH** | 22 s | the same `DAT\` names; `SND\` empty and no movie, and before the build: "No ffmpeg found: skipping the PSP's sound effects and voices, the disc's logo movie. Get ffmpeg from <link> and run setup again to add them." |
| US `.cue`, its path in `sources.toml` edited by hand to the 2,048-byte `.iso` | 12 s, exit 1 | `error XA needs a raw (2,352-byte sector) image (details in ...setup.log)`; `game\` and `cache\` unchanged (a listing of every file, size and mtime hashed before and after: equal); no `.new` folder left |
| US `.cue` with `game\DAT\FIRST.DAT` held open by another process | 224 s, exit 1 | `error The previous install could not be replaced ... Access is denied`; the cache's swap undone, both folders as before (the same listing hash) |

Every home's path here was 165-173 characters to `CAPCOM.AVI`, so each setup
showed the long-path warning first.

The re-encode of an Indeo `capcom.avi` (section 5 step 3), run through
`copy_pc` on its own from the shipped file (`fixtures.toml` `capcom_avi`,
sha256 `65ca9b15...`): 80.7 s, and the output's sha256 is `460d25ce...`,
**`capcom_avi_cinepak`'s - the DIV-0001 file byte for byte** with the same
ffmpeg 9.0.1.

**The bundled interpreter** (the release zip unzipped, run with `PATH` holding
only `C:\Windows` and `System32` - no Python and no ffmpeg on it;
`BOF3X_FFMPEG` set the way the launcher sets it where stated): `identify
--json` on the US `.cue` and the PSP-EU `.zip`, both ok, 3 s; `setup` from the
US `.cue` with no ffmpeg 98 s, `game\` the repo interpreter's but for
`CAPCOM.AVI` (skipped, and said so); `bof3\` + the US `.zip` 184 s and the
PSP-EU `.zip` 247 s with `BOF3X_FFMPEG`, the former's `game\` and cache
identical to the repo interpreter's. The first bundled setup stopped at
`psp_exe: src/hook/exe_own.cpp has no single Dat_FileNames name pool` - the
one file the data list lacked, found by the trace below and now shipped.

**What the tools read under `ROOT`** (an audit hook on `open` in every Python
process of three setups - US, `bof3\` + US, PSP-JP - through a
`sitecustomize` on `PYTHONPATH`): `fixtures.toml`, seven `fixtures/*.tsv`,
sixteen `recipes/` files, `symbols.toml`, `tables.toml`,
`exe_maps/<build>.tsv` and `src/hook/exe_own.cpp`; 22 of the 25 scripts the
import walk finds were loaded (`cache_walk` and `suites` are `check`'s, and
`player_sources` the commands' own).

### 6.1 The release zip

`python tools/make_release.py --version v0.1.0-alpha.1` with the main
checkout's `build\` (2026-10-15): **58.7 MB zip (58,704,310 bytes), 1,190
files, 171 MB unzipped** (`bof3x.dll` 63 MB of it), in 19-21 s with the
downloads cached.

- **Python:** python.org's `python-3.13.16-embed-amd64.zip`, sha256
  `97dae527...`, equal to the checksum in python.org's own `.spdx.json`; its
  `python313._pth` gains `../tools` and `Lib/site-packages`, so the importer
  and the wheels are found with no environment variable. `import site` stays
  off; in `._pth` mode `PYTHONPATH` and the user's site-packages are ignored.
- **Wheels:** numpy 2.2.6, Pillow 12.0.0, capstone 5.0.9 for `win_amd64` /
  cp313, each sha256 equal to PyPI's digest, installed with `pip install
  --target --platform win_amd64 --python-version 3.13 --only-binary=:all:`.
  numpy is used by `at3.py` (the PSP's sounds), Pillow by the language
  build's fonts, capstone by `exe_tables.py`. The walk also finds
  `ahocorasick` (`exe_twins.py`, the recipe generators only) and `inventory`
  (`seq.py verify`, the sibling's) - not bundled, listed in the script.
- **numpy's licences:** its wheel bundles OpenBLAS (BSD), the GCC runtime
  library (GPL-3.0 with the GCC Runtime Library Exception) and Microsoft's
  `msvcp140`; `THIRD_PARTY_RUNTIME.txt` says so. Redistributing the runtime
  is what the exception permits; whether that sits with
  [`LICENSING.md`](LICENSING.md) section 4 is **the owner's call** - numpy's
  only use is `at3.py`'s, so it could be replaced if it does not.
- **The tools:** 25 `.py` files, found by walking every `import` from
  `importer.py` and `player_sources.py` at any depth, plus string literals
  naming a `tools/*.py` (how `importer.py` runs `loc_build.py`); the data under
  `ROOT` as measured above. An import the release neither bundles nor lists
  stops the script.
- **No game data:** the script refuses a zip holding a file of a game-data
  kind (`.dat`, `.bin`, `.iso`, `.cue`, `.avi`, `.mp3`, `.str`, `.emi`, ...;
  `BOF3.exe`) or any file whose sha256 `fixtures.toml` catalogues. Clean.
- **The launcher's paths:** `<launcher dir>\runtime\python\python.exe` and
  `<launcher dir>\runtime\tools\importer.py`.

## 7. Not done

- The C++ importer (IDEAS I39) and CHD / CSO readers (I40).
- A `.zip` of a PC game folder: written (`unpack_zip` unpacks the folder
  holding `BOF3.exe` whole) but not run - no such zip is held.
- The Indeo re-encode was measured through `copy_pc` alone, not inside a
  whole setup: the PC folder held here already carries DIV-0001's file.
- Nothing set up here was launched or played; the installs are the ones the
  disc-only, PSP-only and PC rounds measured, and the PC + US overlays equal
  the owner's own.
- No release tag pushed; `release.yml` has not run on GitHub.


## 8. Ledger

None of this changes game behaviour: it chooses which existing layers an
install gets (each already ledgered - DIV-0080's default walls, the language
layers DIV-0005 / DIV-0090, the disc-only install DIV-0096..0102) and copies
the player's files. The one data repair it applies, the logo movie, is
DIV-0001's own command. Precedent: the launcher's settings dialog and the
Jukebox got no entry of their own; the suites did (DIV-0107) because they
change what the game plays. So no new entry.
