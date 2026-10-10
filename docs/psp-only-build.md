# A build from a PSP disc alone: how close we are, and the PSP toggles

**Status:** MEASURED (2026-10-10). Read-only: the importer run on each disc
alone, plus two measurement agents (banks and music, sound effects), with
scripts in the session scratchpad. Nothing in `src/` or `tools/` changed.

The owner asked, 2026-10-10: how close are we to producing our build from a
PSP disc analysis alone? Two goals:

1. **A working build from the player's PSP disc**, with no PC install and no PSX disc.
2. **A toggle per PSP difference** (items, battle code, effects, maybe audio)
   in the **launcher's** config dialog. Every option is listed. An option
   whose source disc the install lacks is greyed out, so the player sees both
   the defaults in force and what another disc would add (section 6).

No game bytes appear here: counts, hashes and offsets only (rule 1).

## Short answer

- **Data: about two thirds of the way.** A PSP disc carries JP's game data
  and code byte for byte, in PSP containers ([`region-diff.md`](region-diff.md)
  5). Today the importer builds **11 of 741** `base/` containers from a PSP
  disc alone, against **727** from the US PSX disc alone. The difference is
  almost all audio, which nobody had read yet. With a PPHD reader
  (half a day, measured below) a PSP disc gives 885 of the 901 banks
  byte-identical. Most of what is left is the PSP's own content changes
  (section 2.2). For a PSP-only player those changes *are* the base: there is
  no PlayStation original to fall back on.
- **Engine: the same distance as from any disc, and that is the long pole.**
  The game still runs inside the player's `BOF3.exe`. A disc-only build of
  any kind needs our own executable (state 3,
  [`platform-layers-plan.md`](platform-layers-plan.md) section 3). It also
  needs a language layer that does not borrow the PC's font and tables, and
  the 14 battle arenas the port edited, read. None of these is PSP-specific,
  and none is done.
- **Toggles: the data half exists, the code half is unmeasured.** Five PSP
  `opt/` layers are built and verified (DIV-0086). Only the boss module and
  the encounter path have been compared as code, and they matched. The rest
  of the shared battle engine, the menus and the items code has not been read
  against the PSP's ELF.
- **Audio: all there, but not exact.** Banks: 885 of 901 byte-identical. Songs:
  the PSP's own versions (73 of 166 reproduce the PSX exactly). Effects,
  voice and jingles: 891 of 891 names present, but ATRAC3plus at about 25 dB
  SNR, and no permissive decoder exists.

## 1. What was run

`tools/importer.py build --source <one disc> --out <scratch>` for each disc:

| Source alone | `base/` chunks from the disc | `base/` containers written | `base/snd` | `base/bgm` | `base/exe` |
|---|---:|---:|---|---|---|
| PSX US | 3,252 + 41 stand-ins | **727** of 741 | 880 (`wave-from-xa`) | 247 files (`seq.py`) | from the boot EXE |
| PSP JP | 2,151 | **11** | none | none | from `BOOT.BIN` |
| PSP EU | 2,129 | **11** | none | none | from `BOOT.BIN` |

What either PSP disc lacks, as the importer reports it (PSP-JP; PSP-EU in brackets):

| Class | Chunks | Why |
|---|---:|---|
| `bank` | 901 | No reader for the PSP's `PPHD` / `pBVC` (section 2.1) |
| `disc` | 206 (228) | The PSP's section differs from the PC's chunk: its content changes (section 2.2) |
| `art` | 16 | The port's world-map dial page. A PSX disc's own page stands in; the PSP's is not offered |
| `type1` | 14 | Battle arenas where the PSP changed Ryu's form data (P8) |
| `pc-edit` | 14 | The port's own arena edits. **Missing from every disc** (section 3) |
| `pc-byte` | 4 | `RYUD00..03`. The `ryud` rule applies to a PSX section; the PSP's differs (P8) |
| `pc-icons` | 1 | `FIRST`'s item-type icons. The `icons` rule reads a PSX section |

## 2. What is specific to the PSP

### 2.1 Audio

| Asset | From a PSX disc | From a PSP disc |
|---|---|---|
| The 901 bank chunks (`vag.py`) | 901 / 901 byte-identical | **885 / 901** byte-identical, both PSPs. The `PPHD` layout is read: `PPPG` programs, `PPTN` tones with centre, fine, min / max, mode and ADSR, `PPVA` sample offsets into `pBVC`. A synthetic VH and VB fed to the unchanged `vag.bank` reproduce the PC's chunks. **About 60 lines and half a day** with the importer hook |
| The other 16 | | One bank, shared by 16 battle containers (`BPLD/BPLU 015, 025, 056, 257, 457, 567, 578`, `BRTD/BRTU457`). A chord plays two tones of program 1. The PSP's converter declared only one tone and dropped the second slot (VAG 5), whose samples are still in `pBVC`. Those four fields are not on the PSP disc. **Source: the PC or a PSX disc, or a hand rule**. The PSP may lose that sound in play (unchecked) |
| The 166 songs (`base/bgm`) | 166 | By inverting `pPMS` (ticks / 10 where 480, tempo from the tick-0 metas, the loop controllers remapped, note-offs back to velocity 0): **73 byte-identical**, 4 more where only the rhythm field differs. The rest: 5 reorder within a tick, 56 pitch-bend only, 11 controllers, 1 setup moved, **16 notes differ, 2 tempos differ** (22: 128 to 126 BPM, unlisted; 157: 79 to 82) |
| The 81 music banks | 81 | **0 can be byte-identical.** The PSP merges each tone's volume and pan with its program's and squeezes pan about 3:1. Program mvol, priority, mode and the VAB master volume are gone, as are 11,802 tone slots past each program's count. Samples are 1,195 / 1,195 verbatim, and pitch, ADSR and range are exact |
| The 880 effects and 11 jingles | 880 / 880 (`wave-from-xa`) | **Section 2.1.1** |

So for music, a PSP-only install takes the **PSP's own music**. That means a
PSP-native bank format the sequencer can play (2-3 days; the volume-merge rule
is a call to make). The PSX-identical route stops at 73 songs. Whether the 56
bend-only songs are an encoding difference or audible depends on whether the
PSX driver resets pitch bend at key-on, which has not been measured. The 16
note songs, the 2 tempo songs and song 31 are real differences: a `psp-music`
toggle (2-3 days after the reader).

#### 2.1.1 Sound effects, voice and jingles

**All present, none byte-identical, and the decoder is the cost.**

- **Format.** `SCE_XA/` holds 892 files: RIFF `WAVE_FORMAT_EXTENSIBLE` with
  the **ATRAC3plus** subtype, 44.1 kHz, 128 kbit/s. All are stereo, from a
  mono source, except `WHISTLE`. There are no loop chunks. JP and EU are byte-identical,
  892 of 892.
- **Mapping: 891 of the PC's 891 sound names.** 875 `MAGIC/PL00..08` files
  equal the PC's 875 distinct `NNN_KK` names (`019_02` is absent on both, and
  never opened). `VOICE/NA01..05` are `VOICE00..04`, the lengths exact. The 11
  `MUSIC/` files are the 11 jingles. `OTHER/WHISTLE` is PSP-only, named in
  `BOOT.BIN` after the music entries, and matches nothing in any PSX XA stream.
- **Timing.** The PSP cut each effect on a 0.25 s grid. 875 of 875 onsets sit
  at the PC start sector's time mod 0.25 s, or 0.25 s before it (94 clips),
  within 7 ms. So an importer must shift each clip by up to ±250 ms,
  computable from the recipe's start sectors plus one bit per clip. These are
  numbers, not data.
- **Fidelity**, against the PSX XA decoded and resampled:

  | Set | Correlation | SNR |
  |---|---|---|
  | 875 effects | min 0.990, median 0.998 | 16.9 / 24.9 / 30.0 dB (min / median / max) |
  | 5 voices | 0.994-0.9985 | 19-25 dB |
  | 11 jingles | 0.995-0.9996 | 20-31 dB |

  This is about MP3-128 waveform accuracy. Trimmed energy is at most 0.37 %,
  quiet tails only. Taking these is a ledger entry: a lossy re-encode where
  a PSX disc gives the PC's exact files.
- **The decoder.** No permissive ATRAC3plus decoder is known. FFmpeg's and
  PPSSPP's (taken from FFmpeg) are LGPL, and Sony's is proprietary. By
  [`LICENSING.md`](LICENSING.md) section 4 neither can be vendored. That
  leaves two options:
  - **(a)** The importer calls an ffmpeg the player already has, at import
    time. Nothing is vendored, the engine is untouched, and the output is
    22.05 kHz WAVs with the onset shift.
  - **(b)** A clean-room decoder. Estimated, not measured: 3-5 k lines of C,
    with no public specification, so a written description has to come
    first. The patents are probably expired, which is unverified.

  The measurement used an ffmpeg 9.0.1 already on this machine's `PATH`.

### 2.2 The PSP's content changes are its base

Classified from `recipes/pc-zh.toml`, `base/` chunks a PSX disc supplies and
the PSP does not, banks aside:

| What | Chunks | I32 lead |
|---|---:|---|
| Area texture pages (`0x0A081000`): 653 tiles blanked, 301 redrawn | 65 | unlisted |
| The battle / menu UI page (`0x1A080200`): the shoulder-button labels | 59 | unlisted (the PSP's buttons) |
| Area palettes (`0x8002BE00`) | 53 | unlisted |
| Map bands (`0x80104000`): 11 PSP texture edits, plus `AREA004` (the Western walls, without the placement half) | 12 | P1 unresolved |
| The port's dial page (`art`) | 16 | the port's, not the PSP's |
| Ryu's form data (`RYUD`, and the arenas carrying it) | 8 + 14 | P8 |
| Stallion's palettes (`0x8002D800`) | 2 | P6 |
| `DEMO`, `FIRST`, `SCENA17`, `COMMU02` pages and palettes (the title logo, a language page, a font tile) | 13 | P13 and others |
| `pc-icons` | 1 | the port's |

For these about 270 chunks, an importer given only a PSP disc has two
choices. It can stand the PSP's own section in (the `own` class that step 3
uses for PSX discs), or refuse. Standing in means the PSP-only player plays
the PSP's look by default, with the blanked tiles and the PSP's buttons, and
the PlayStation look becomes the greyed option in the launcher. That inverts
the plan's data policy (a later build's content is off by default,
[`unified-data-plan.md`](unified-data-plan.md) 7), so it is a ledger entry
for PSP-only installs. The 16 dial pages and the icons are the port's own
art, so the PSP's page cannot stand in for them. They need the `icons` and
dial rules extended to read the PSP's section, which is small.

### 2.3 The exe's tables

From [`exe-import.md`](exe-import.md) 3-5. A PSP-built `base/exe/` matches the
PC in 86.6 % of `.data` (JP disc: 87.0 %). It carries 25 of the 29
catalogued tables, 23 of them identical. The other two are the PSP's content:
`exp_table` (roster index 6 rebalanced, 249 bytes) and consumable 87 (one
flag bit). It **lacks `sin_table`**, a math table; a rule-built one would
need to be proved byte-identical. Its data pointers rebuild for **only about
59 %** (3,841 of about 6,540), against 93 % from a PSX disc, because the ELF's
layout is not the EMIs'. Either the engine holds the rest as layout (as it
must for code pointers anyway), or the transform learns the ELF.

### 2.4 Text

A disc-only language layer does not exist for any disc yet: `loc_build.py`
borrows the PC's font and the exe's name, verb and config tables
([`importer-transforms.md`](importer-transforms.md) 8). For the PSP there is
more. Its names live in `BOOT.BIN`, not `GAME.EMI`, and `loc_build all` has
never been validated against a PSP disc. PSP-EU's English is `en-150` with
the PSP's edits (P9 and others, region-diff 5.3), and PSP-JP's is `ja-JP`.

### 2.5 Video

None to speak of. The game has no story FMV. The PC ships two logo AVIs
(DIV-0001), the PSX `LOGO/CAPCOM30.STR`, and the PSP one `PSMF/CAPLOGO.PMF`.
A PSP-only install can skip the logos or play the PMF; neither blocks a build.

## 3. What every disc-only build still needs (not PSP-specific)

1. **Our own executable** (state 3). Capcom's runtime start-up, allocator,
   per-thread data, the MP3 decoder and the software renderer's converters
   still run ([`HANDOFF.md`](HANDOFF.md)). State 2's proof waits on the
   decoder decision ([`platform-next.md`](platform-next.md) D). State 3 needs
   a design doc first. Its engine half is in
   [`exe-import.md`](exe-import.md) section 6: 9,142 `.data` words point into
   Capcom's `.text`, `.rdata`'s 65 addresses, and 22.7 KiB that no disc
   carries. A round of its own, with the biggest live check yet.
2. **The 14 port-edited arenas** (`pc-edit`,
   [`type1-compression.md`](type1-compression.md) 3). Eight `PL` arenas are
   352 bytes shorter in member 2's block, plus three `BPLD01x` with 1,949
   coordinate-like words and the `27A` pair. Unread. If the PC's code depends
   on the edited layout, a disc's unedited arena is a crash, not a stand-in.
   Reading them is a day or two, and decides between a rule and a recipe.
3. **A disc-only language layer**: a font from the disc's glyph atlas, and
   `loc_build` reading `base/` and `base/exe/` instead of the PC.
4. **The engine reading the cache** instead of `DAT/` (`install` copies
   files into `DAT/` meanwhile, [`importer.md`](importer.md) 6).

## 4. Goal 2: what can be toggled, by category

| Category | What is known | Toggle form | State |
|---|---|---|---|
| **Art** | P6 (Stallion), the 840-tile and 515-row PSP repaint, 11 map bands | `psp-art`, `psp-tiles`, `psp-maps` `opt/` layers | **built and verified** offline (DIV-0086); not built with llvm-mingw, not seen |
| **Names / text** | P7, 8 English renames, 5 PSP-JP renames, P9 | `psp-names-en-150`, `psp-names-ja-JP` | **built**; PSP-EU's other text edits have no layer |
| **Items** | Consumable 87 loses a flag bit; the item records are otherwise equal | an exe-table layer | needs the engine half of `base/exe/`, or a chunk kind for it |
| **Balance** | Roster index 6's level table rebalanced (249 bytes) | an exe-table layer | the same; a balance change, Extension tier |
| **Battle code** | Stallion's boss module: identical. Encounter path: one load guard removed, which the PC already never trips (P4 refuted) | a code branch under an option | **the shared battle engine, menus and item code unread**: the main unknown |
| **Effects** | P8: Ryu's form data and 15 arenas changed by 13,770 bytes, not rendered | a data layer (`RYUD`, `BPLD`/`BRTD`) | measurable; render first |
| **Audio** | 16 songs' notes, 2 tempos, song 31, maybe pitch bends; one bank's dropped tone | `psp-music` (section 2.1) | needs the PSP-native bank and `pPMS` reader |
| **UI** | Shoulder-button labels, title logo (P13), title menu (P14), storage wording (P15) | layers | the button page means the PSP's controls; probably not wanted |

**The code half is the big unknown for goal 2.** I32 asks for the PSP ELF to
be read against the PSX code function by function. So far only the boss module
and the encounter path have been, and every function matched. The method
exists ([`psp-stallion.md`](psp-stallion.md) 3.1: address-masked instruction
sequences, the PSX names from the sibling). The PSP ELF's `.text` is about
2.9 MB. A first pass is a reading round of 2-4 agents: pair every PSX function
the sibling names with its PSP twin, then diff the pairs, setting widescreen
and platform changes aside. That gives the count of real code changes, which
could be small.

## 5. Suggested order

| Step | What | Size | Gives |
|---|---|---|---|
| 1 | `psp-unwrap` for banks: the PPHD reader into `vag.bank` | half a day | 885 / 901 banks from a PSP disc |
| 2 | The `icons` and dial rules reading the PSP's sections; the PSP's `own` stand-ins behind a PSP-only flag (a DIV) | a day | `base/` complete from a PSP disc except the 16 + 14 |
| 3 | The PSP ELF code diff (I32's code half) | a reading round | the list of code toggles, or proof there are few |
| 4 | The launcher's options page: every layer and option listed from a catalogue, each with its source builds; greyed when the manifest has none | 1-2 days | goal 2's UI for the data layers that exist |
| 5 | The PSP music: a PSP-native bank and `pPMS` reader, then `psp-music` | 4-6 days | PSP-only music, and the music toggle |
| 6 | Effects by route (a), the player's ffmpeg at import, with the onset shift (section 2.1.1; a DIV); `sin_table` by rule; the PSP data pointers | 2-3 days | PSP-only sound and `base/exe/` |
| 7 | The shared items in section 3: `pc-edit` read, a disc-only language layer, state 3 | rounds | **any** disc-only build, the PSP's included |

Steps 1, 2 and 4 are useful now even with a PC install present. Step 4
delivers the owner's "see what another disc would add" on the five layers
that already exist.

## 6. The launcher's options page (the owner's design, 2026-10-10)

- **Three toggles: audio, graphics, data** (the owner's call, section 7),
  each choosing between the PSP's version and the PlayStation's.
- **Every option is listed**, whether or not its source is present. An option
  whose source the install lacks is **greyed out and not selectable**, with
  the discs that would enable it named. The player sees both the defaults in
  force and what another disc would add.
- **The source of truth is the importer's manifest**: which builds were
  identified, against `fixtures.toml`. Each option carries its source builds,
  as `recipes/opt.toml` already does per layer (`psp-*`: either PSP disc;
  `area4-walls`: a Western PSX disc). Today the launcher's `opt=` only sees
  layers already installed in `DAT/` and has no dialog box for them
  (DIV-0086, [`opt-layers.md`](opt-layers.md) 5); this page replaces that
  with the full catalogue.
- **For a PSP-only install it works in reverse.** The PSP's look is the
  default (section 2.2), and the PlayStation original appears greyed, naming
  a PSX disc.
- In the launcher, not the in-game Config screen (the owner: the in-game
  version would need graphics work to look right). A launcher choice takes
  effect at the next start, which fits layers that load with each area.

## 7. The owner's calls (2026-10-10)

1. **A PSP-only install uses the PSP's art and data** as its base: the
   section 2.2 stand-ins, the PSP's `exp_table` and consumable 87. This is a
   ledger entry when the importer does it.
2. **A PSP-only install uses the PSP's audio as it is** when no PSX disc is
   present: the 885 banks, the PSP's music, and the ATRAC3plus effects. Where
   both are present the PSX's stays the default, and the PSP's audio
   eventually becomes a toggle.
3. **Three toggles, not one per difference:** **audio**, **graphics** and
   **data**. Each picks the PSP's version or the PSX's, and is greyed when
   its source is missing (section 6). Finer grain only if testing the install
   route shows a need. The same path may later carry **mods**, if side-loading
   a layer is easy enough. The `opt/` layer mechanism (DIV-0086) is already
   that shape: a named layer of whole chunks over the base, from a manifested
   source.

Still open:

- **The decoder for the effects:** the player's own ffmpeg at import time (a),
  or a clean-room ATRAC3plus decoder (b) (section 2.1.1). (a) is recommended
  to start: nothing vendored, and (b) can replace it later.
- **Whether the PSP code diff (step 3) runs before the launcher page.** With
  three toggles decided, the page no longer waits on it: battle-code
  differences, if any are found, fall under **data** or get a fourth toggle.
