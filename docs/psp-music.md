# The PSP's music: `pPMS` to the cache's song form

**Status:** IN PROGRESS (2026-10-12), group MS of the PSP-only round
([`psp-only-round.md`](psp-only-round.md)), both stages built. Stage A:
`tools/ppms.py` reads a PSP disc's songs and writes them in the form
`base/bgm/` stores; it reproduces the 2026-10-10 measurement in code and adds
a render A/B (sections 1-4). Stage B: the importer writes `base/bgm/` from a
PSP disc alone, paired with MB's banks, `verify` checks it against
`fixtures/bgm-psp.tsv`, and the Jukebox lists it as the PSP disc's (sections
6-7). Nothing has been heard. No game bytes appear here, only counts, hashes,
offsets and rules (rule 1). `psp_ms/` below
is the scratch folder of session `3d54deba` (its probes, the render script and
`measure_jp.json` / `ab_180.json`).

## 0. The answer

- **The format is fully read.** Each of the 81 `BGM/*.EMI` on a PSP disc keeps
  the PSX's three music sections, and its SEP (type 10) is a `pPMS`: a table
  of four Standard MIDI Files. The PSP's own song table and file-path table
  are in `BOOT.BIN`. So a PSP disc alone gives all 166 songs, with no PSX disc
  and no copied table.
- **The inversion reproduces the 2026-10-10 numbers exactly.** 73 songs are
  byte-identical to the PSX-built file. The other 93: 4 differ in the rhythm
  field only, 5 in the order of events within a tick, 56 in pitch-bend events
  only, 11 in controllers, 1 in where its setup sits (song 31), and 16 in
  notes. Two tempos differ (22: 128 to 126 bpm; 157: 79 to 82 bpm).
  PSP-JP and PSP-EU give the same 166 files.
- **The bend question is settled from the code: the PSX driver does not reset
  the bend at key-on, and does not remember it either.** `_SsVmKeyOn` takes
  its pitch from `note2pitch`, which reads only the note, the tone's centre
  and its shift. A bend event retunes the voices of its program that are
  sounding at that moment, and nothing else. Almost all of the PSP's extra
  bends (870 of them in the 56 songs) are resets to centre, and 728 of those
  follow a bend that was not centre. Each one therefore snaps any
  still-sounding voice of that program back to pitch. **Rendered through our
  sequencer, 50 of the 56 differ from the PSX render (section 4.2).** The songs
  are not an encoding difference; they are the PSP's own music.
- **Stage B: a PSP disc alone now gives `base/bgm/`.** 166 songs and 81 banks
  (MB's inverse), 247 files, identical from PSP-JP and PSP-EU, `verify` 0
  problems against `fixtures/bgm-psp.tsv`. A PSP-only build writes 741 `base`
  containers. Every song renders on the PSP banks without an abort, and the
  Jukebox lights its PSP source on such a cache. The PC and PSX caches are
  unchanged.

## 1. What was built

`tools/ppms.py` (stage A):

| Function / command | What it does |
|---|---|
| `read_ppms(section)` | The `pPMS` table and its SMFs; anything outside the measured layout is refused |
| `invert(sub)` | One SMF as `seq.read_sep` gives a PSX sub-song. `seq.song_bytes` then writes it unchanged (`seq-format.md` 1) |
| `songs(disc)`, `file_paths(disc)` | The PSP's song table (`tables.toml` `songs`, the `psp-jp` / `psp-eu` rows in `BOOT.BIN`) and `BOOT.BIN`'s path table, which the song table's file ids index |
| `song_files(iso)` | The 166 song files, `(song, bank name, bytes, the pPMS's section index)`; `build` writes them under `base/bgm/` |
| `measure --disc ISO --psx CUE [--json]` | Each PSP-built song against the PSX-built one, with its class and the inversion's rule counts |
| `dump --disc ISO --song N` | One inverted song, printed the way `seq.py dump` prints it |
| `check` | A synthetic round trip with no game data: `seq.py`'s synthetic SEP is put into the PSP's form at divisions 480 and 48 and inverted back, and the refusals are tried. 0 errors |

Stage B's additions (`build`, `importer_bgm`, the fixture) are in section 6.

## 2. The format, as measured

`read_ppms` refuses anything outside this layout, and `measure` reads all 81
files (the 166 songs name all 81 `BGM` EMIs, as `fixtures/bgm.tsv`'s 81 banks
per build show). So the layout holds on both PSPs. `check` holds it as a
synthetic test. The counts come from the census script `psp_ms/probe3.py`
(PSP-JP against PSX-JP, all 324 sub-songs) in the session scratchpad:

- **`pPMS` header:** the magic, then the section's size (81 of 81), then the
  end of the table, which is `0x10 + 16 x` the sub-song count (81 of 81),
  then `0xFFFFFFFF`.
- **Each entry, 16 bytes:** `u16` id, equal to its index (324 of 324); `u16`
  `0xFFFF`; `u32` offset; `u32` size; `u32 0xFFFFFFFF`. The SMFs follow each
  other from the table's end to the section's end.
- **Each SMF:** format 0, one track (324 of 324). The division is 48 in 255
  sub-songs and 480 in 69. Every PSX sub-song has resolution 48.
- **Metas, all at tick 0:** a tempo in every sub-song (324). A time signature
  in 85, a track name in 85, a port in 84, a key signature in 84. The PSX
  SEPs carry no tempo event at all, only the header's tempo.
- **Note-offs:** the PSP has 336 `8n` events. The PSX has none: every note
  ends as a `9n` with velocity 0.
- **Loops:** the PSX's NRPN pair 99 = 20 / 99 = 30 (317 / 315) is 99 = 0 /
  99 = 1 on the PSP. Its 172 data entries 6 = 127 are 6 = 0.

**The file ids.** The song table's 165 records equal the PSX's
(`tables.toml`'s cite). Their file ids index a table of 64-byte path records
in `BOOT.BIN`, which takes the place of the PSX boot EXE's table of LBAs.
`file_paths` finds it by shape: the longest run of 64-byte records that each
name a file under the disc's data folder. That gives 888 records on both
PSPs. Records 0..882 equal the PSX-JP file table's paths with `BIN/` dropped
(883 of 887; the four after them are the PSP's `ETC/` files where the PSX
has `LOGO/` and others). Every song's id lands on a `BGM/` path.

## 3. The inversion, and the measurement reproduced

The rules are in `ppms.py`'s docstring, and `invert` counts each as it
applies it. Over the 166 songs of PSP-JP:

| Rule | Sub-songs or events |
|---|---:|
| Division 48 as it is / 480 divided by 10 (every tick whole) | 98 / 68 sub-songs |
| Tempo at tick 0 into the header | 166 |
| Time signature into the rhythm bytes / absent, so 04 02 | 80 / 86 |
| Track name, port, key signature dropped | 80, 79, 79 |
| `8n` to `9n` with velocity 0 | 336 events |
| Loop markers mapped / their data entries mapped | 316 / 159 events |
| End of track moved to the last channel event / already there | 79 / 87 |

The classes, from `python tools/ppms.py measure --disc "<PSP-JP iso>" --psx
"<PSX-JP cue>"`. `classify` compares the song files' events as multisets, so
a class names what kind of event one side has and the other lacks:

| Class | Songs | Which |
|---|---:|---|
| Byte-identical | **73** | |
| Rhythm field only | 4 | 18 34 38 77 |
| Order within a tick only | 5 | 75 80 98 145 152 |
| Pitch bend only | 56 | 3 8 11 15 19 22 23 27 28 30 33 35 37 48 51 52 54 55 56 57 63 64 65 66 68 70 71 73 74 76 81 82 90 91 97 99 102 103 107 110 111 114 117 131 132 134 139 140 142 150 151 153 155 156 164 165 |
| Controllers (with bends in some) | 11 | 0 29 40 41 60 72 88 122 125 148 154 |
| Setup moved (programs) | 1 | 31 |
| Notes | 16 | 45 50 89 93 112 119 120 121 128 135 136 138 143 144 147 157 |
| Tempo differs (across classes) | 2 | 22 (468,750 to 476,190 us a beat), 157 (759,493 to 731,707) |

These are the 2026-10-10 numbers (`psp-only-build.md` 2.1), reached the same
way. The "4 more where only the rhythm field differs" are the four above.
**PSP-EU:** all 166 song files are identical to PSP-JP's
(`psp_ms/probe4.py`).

What the classes hold, from the same scratch scripts (`probe6.py`) and the
`--json` output:

- **The rhythm field.** The PSX's two header bytes are not always 04 02
  (0302 x 4, 0202 x 2, 8005 x 3 among the sub-songs). The PSP writes 4/4
  where it has a time signature, so 9 songs differ in the field: 5 with a PSP
  4/4 and 4 with none. **The player never reads it** (`src/audio/song.cpp`
  stores it; nothing in `seq.cpp` uses it), so none of the 9 can be heard.
- **Controllers:** the PSP's songs use only 6, 7, 10 and 99, the ones the
  player implements (`seq.cpp` `ControlChange`; 0, 91, 98, 100, 101 and 121
  abort, and none of them appears). The differences are channel volumes
  (29, 72, 122: 127 becomes 60 or 80), added pans (0, 40, 41, 60, 88, 125,
  148, 154), and a few added bends.
- **Notes:** song 45 moves its 780-note channel-0 part to channel 9.
  157 starts earlier and changes its tempo. The others gain or lose a note or
  two, or end one 18 to 28 ticks earlier (`bgm-comparison.md` 8.3 lists them).
- **Song 31:** the same programs, controllers and notes, on other ticks or
  channels, plus 3 bends.

## 4. Does the PSX driver reset the pitch bend at key-on?

### 4.1 The code

**No, and it does not remember the bend either.** Three readings agree:

- **Our sequencer** (`src/audio/seq.cpp`, the reading of
  [`libsnd-reading.md`](libsnd-reading.md) 3.8). `VmKeyOn` keys every voice
  at `NoteToPitch()`. `VmPitchBend` re-pitches every voice of the sequence,
  VAB and program that is sounding, releasing ones included, through
  `NoteToPitch2(note + bend)`, and stores nothing per channel.
- **The sibling's names.** `_SsVmKeyOn` `0x801719A4`, `note2pitch`
  `0x801723D4`, `_SsVmPitchBend` `0x80173258` and `_SsVmPBVoice` `0x80173058`
  are whole-object Psy-Q signature matches (`symbols.toml`, `LIBSND.LIB`
  `VM_KEY` / `VM_N2P` / `VM_PB`): the stock library.
- **The JP boot EXE, disassembled** (`psp_ms/probe_keyon.py`, capstone over
  those extents). `_SsVmKeyOn` calls `note2pitch`, never `note2pitch2`, and
  `vmNoiseOn`, `_SsVmAlloc`, `_SsVmDoAllocate`, `_SsVmKeyOnNow`,
  `_SsVmSelectToneAndVag`, `_SsVmVSetUp` and `_SsVmKeyOff`, never the bend
  code. `note2pitch`'s only global loads are the pitch table `0x80184DD4` and
  three fields of `_svm_cur` (`0x8018F152` the note, `0x8018F160` the centre,
  `0x8018F161` the shift). `_SsSetPitchBend` stores no global, and
  `_SsVmPitchBend` stores only `0x8018F166`, a `_svm_cur` field.

So a PSX note always starts unbent. A bend changes only what is sounding when
it arrives, and that is per program, not per channel.

**What the PSP added.** In the 56 bend-only songs the PSP has 870 bend events
the PSX lacks, and the PSX has 60 the PSP lacks (`psp_ms/probe5.py`, which
tracks each program's last bend as libsnd applies it):

| PSP-only bends | Count |
|---|---:|
| To centre, after a bend that was not centre, at a key on's tick | 719 |
| To centre, after a bend that was not centre, elsewhere | 9 |
| To centre, the program already at centre, at a key on's tick | 97 |
| To centre, the program already at centre, elsewhere | 45 |

59 of the 60 PSX-only bends are to centre with the program already there,
and one is a bend.

A centre bend on a program already at centre changes nothing:
`note2pitch2(note, 0)` equals `note2pitch` whenever the tone's shift is below
128. The 728 that follow a real bend are another matter. They pull back to
pitch any voice of that program still sounding, held or in release, at a
moment where the PSX leaves it bent. *A guess, not measured: the PSP's own
player probably keeps the bend per channel, as General MIDI does, and its
converter added a reset before each note so that new notes start unbent as
on the PSX. On the PSX that reset also reaches the old notes' tails.*

### 4.2 The render

`psp_ms/render_ab.py` renders each of the 93 songs that are not
byte-identical through the host `synth_render` (`tools/bgm/host`, built from
this worktree's `src/audio` with `clang++`, Release). It renders the
PSX-built file from `analysis/cache/pc-plus-us` and the PSP-built file in a
scratch cache whose banks are the same PSX banks, 180 s each, and compares
the two WAVs sample for sample. Both sides use the same bank, so only the
sequence differs.

`diff` is the energy of the sample difference relative to the PSX render's,
in dB. No song aborted (all 186 renders exited 0).

| Class | Songs | Identical for 180 s | Differ | `diff`, min / median / max | First difference |
|---|---:|---:|---:|---|---|
| Rhythm field only | 4 | **4** (18 34 38 77) | 0 | | |
| Order within a tick | 5 | **3** (75 80 98) | 2 | 145 -55.3, 152 -60.6 dB | 34.5 s, 60.6 s |
| Pitch bend only | 56 | **6** (8 28 37 102 107 165) | **50** | -52.7 / -29.8 / -16.2 dB | 0.1 to 110.9 s |
| Controllers | 11 | 0 | 11 | -25.9 / -20.5 / -15.7 dB | 0.3 to 111.0 s |
| Setup moved (31) | 1 | 0 | 1 | -19.0 dB | 0.2 s |
| Notes | 16 | 0 | 16 | -47.1 / -23.5 / +3.4 dB | 0.02 to 57.7 s |

**So the 56 bend-only songs are not an encoding difference on the PSX
driver's model: 50 of them render differently.** The level of the difference
runs from -16 dB (65: a quarter of the signal's amplitude) to -53 dB (90).
Only 6 are identical for the first three minutes, and so are 7 of the other
37 songs. Song 157 differs at +3.4 dB from the first note, because its tempo
and start moved and the two renders drift apart. Song 121 is at -6.8 dB from
57.7 s. Song 45's channel move costs nothing until 43.3 s and is -35 dB
after that: libsnd keys a voice by program, not channel, so the move matters
only where the channels' volume and pan differ.

The A/B is exact, not perceptual. Whether -40 dB of retuned release tail is
heard is the owner's ear. The renders are scratch (game data) and are
deleted after each comparison. `ab_180.json` keeps the numbers.

## 5. Owner's calls this meets

- **Call 2, 2026-10-10** (`psp-only-build.md` 7): a PSP-only install plays the
  PSP's music as it is. `song_files` gives the PSP's own songs. The inversion
  changes format only (ticks, metas, note-offs, the loop pair, the end tick)
  and keeps every note, bend and controller the PSP has. With a PSX disc among
  the sources, the PSX's songs stay the base (`importer_bgm` returns nothing).
- **Game data in code, 2026-10-10:** nothing is copied. The song table is
  read from `BOOT.BIN` at `tables.toml`'s recorded place, and the path table
  is found by shape. The only constants are the format's own (divisions,
  magic, loop values) and `seq.py`'s song 165 rule. The fixture holds hashes
  and sizes only.
- **Every option listed, greyed by source** (2026-10-10): the Jukebox's PSP
  source is lit for a PSP-only cache and greyed, with the reason, otherwise
  (section 6.3).
- **The Jukebox plays the PSP's volume merge "as the PSP has it"** (the owner,
  2026-10-11, `music-player.md` 3). **Partly met, an owner's question.** The
  cache's PSP banks are MB's inverse (`psp-music-banks.md` 3): each tone's
  merged volume is kept, so the loudness is the PSP's, but the pan is
  un-squeezed (`3p - 128`), so the stereo image is the PSX's width, not the
  PSP's. The Jukebox plays what the cache holds. Playing the PSP's squeezed
  pan would need a second bank form; it is not built.

## 6. Stage B: what was built

### 6.1 `ppms.py`

| Function / command | What it does |
|---|---|
| `build(iso, out)`, `build --disc ISO --out CACHE` | `base/bgm/`: the 166 songs and the 81 banks they name. A bank is `seq.bank_bytes(name, seq.parse_vab(*vag.psp_music_vab(pphd, pbvc)))`, MB's contract; a `ValueError` from it ends the build naming the EMI. Returns `seq.build`'s shape |
| `importer_bgm(sources, out)` | The importer's hook: the first PSP disc, only when no PSX disc is among the sources |
| `fixture --disc ISO [--disc ISO]` | `fixtures/bgm-psp.tsv`: per PSP build, each file's path, size, section and hash (`fixtures/bgm.tsv`'s format) |
| `check_fixture(builds)` | Its shape, no game data: PSP builds of `fixtures.toml`, 166 songs each, exactly the banks they name, the format versions |

Manifest rows: `["base/bgm/000.DAT", "psp-jp:ppms:BGM000.EMI#<i>", <sha256>]`
for a song (the `pPMS` section) and `"psp-jp:psp-vab:NAME.EMI#<i>,<j>"` for a
bank (`PPHD`, `pBVC`). The build id opens the source, so LT's launcher reads
it from the first row, as the coordinator's contract says.

### 6.2 `importer.py` (shared; the lines added)

- `build`: one call site, `bgm = seq.importer_bgm(...) or
  ppms.importer_bgm(...)`. The manifest's `bgm` rows and the build's summary
  line take either (the summary names `ppms.py` for a PSP build).
- `verify_bgm`: `fix.update(ppms.load_fixture())`, so a PSP build's rows are
  checked against `fixtures/bgm-psp.tsv` as a PSX build's are against
  `fixtures/bgm.tsv`. (Its printed line still says `fixtures/bgm.tsv`.)
- `check`: `ppms.check()` and `ppms.check_fixture(builds)` beside `seq.check()`.
- The manifest writer is unchanged, so a PC or PSX cache's manifest is too.

### 6.3 The Jukebox (`src/launcher/music_player.cpp`)

`CacheBgmBuild` reads the build id that opens the source of the manifest's
first `bgm` row. A cache holds one `base\bgm`:
- a PSP build: the PSP source lists the cache's songs and plays them through
  the same `SeqStream` (the same form, the same sequencer); the PSX source is
  greyed, "The cache's songs are the PSP disc's";
- anything else (or no manifest): as before for the PSX source; the PSP
  source is greyed, "a cache built from a PSP disc alone holds them".

The old `Fatal` for a PSP `Play` is gone with the reason for it. A PC + PSX +
PSP install still shows only the PSX's songs: a PSP column beside a PSX one
needs a second tree (an `opt/` layer, TG's half).

## 7. Stage B: what was measured

All from this branch's tools; the scripts are in `psp_ms/`.

- **PSP-only builds** (`stageb_psp.sh`: `importer.py build --source <PSP iso>
  --out <scratch>`, then `verify`), both exit 0:

  | | PSP-JP | PSP-EU |
  |---|---|---|
  | `base` containers written | **741** | **741** |
  | `base/bgm` | 247 files (166 songs, 81 banks), `ppms.py` | the same |
  | `verify` | 741 held, 0 problems; `base/bgm` 247 of 247 as the fixture, 0 problems | the same |

  `--lang ja-JP` / `en-150` refuse on a PSP source (no PSX disc carries it):
  the PSP's language layers are LG's.
- **PSP-JP and PSP-EU:** the 247 files identical (the fixture's hashes, and
  the two caches' `base/bgm` compared). 73 of the 166 songs equal the PSX
  fixture's, as section 3 found; 0 of the 81 banks do (MB's inverse, by
  design).
- **Every PSP song plays on the PSP banks:** `render_all_psp.py`, the host
  `synth_render` on the PSP-only cache, 30 s each: 166 of 166 rendered, no
  abort.
- **The PC and PSX paths unchanged** (`stageb_builds.sh`, `cmp_refs.py`,
  against `psp_coord/ref_*`): JP + PC 2,126 files, US 1,873, EU 1,865, every
  file byte-identical. The manifests differ only in the build time and the
  `[build] base_policy = "psx"` table SI's merge added (on the round tip
  before this branch, not this change). `recipes/` untouched.
- `importer.py check` exit 0 (0 errors with `ppms` added); `ledger_check` 0
  errors.
- **i686 build** (llvm-mingw, `cmake --preset i686 && cmake --build build`):
  0 warnings in `music_player.cpp` (the warnings in the log are SDL's and
  other files', none new).
- **Launcher host suite** (`tools/launcher_host`, `config_tests`): 48 of 50;
  the two failures are the code-page tests of `config.cpp` (`cache=` widened
  through the ANSI code page), a file this branch does not touch.
- **`dialog_test`:** the repo's script finds no dialog. Since state 3, the
  launcher refuses to start without `bof3x.exe` beside it, and the script's
  scratch install has only a dummy DLL. A scratch copy that also writes a
  dummy `bof3x.exe` (`dialog_test_exe.py`): **all 19 checks ok**. The repo's
  script needs that one line; it is not this group's file.
- **The Jukebox, by script** (`jukebox_test.py`, the built launcher in a
  scratch install, `cache=` in its ini): on the PSP-JP cache, PSX greyed, PSP
  enabled, 166 songs listed (no catalogue beside the scratch launcher, so no
  hidden rows), Play on 000 "Playing 000 from the PSP disc: looping as the
  disc's song does", Stop, Close. On the US PSX cache, PSX enabled and PSP
  greyed with its reason. Not heard.
- **Headless self-test** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, the
  launcher, `bof3x.exe` and the DLL in an ini-less scratch folder, `--game`
  the PC install, `--no-config`): the hosted build (a shadow switch selects
  it), game process exit 0, launcher exit 0, about 15 minutes. The exe host's
  self-test on a PSX disc-only install was not run: this branch changes the
  launcher's Jukebox only, nothing the DLL or `bof3x.exe` holds.

## 8. Not verified

- **No song has been heard**, on either bank form. The render A/B (section
  4.2) is exact, not perceptual. The owner's ear on the 16 note songs, the 2
  tempo songs, song 31 and song 22 (MB's program-pan case) is V4's
  (`psp-only-round.md` 6).
- **The PSP's own player was not read.** Section 4.1's guess about why the
  PSP added the resets stays a guess.
- **The A/B covers each song's first 180 s**, and used the PSX's banks. On
  the PSP banks only "renders without an abort" was checked here; MB's
  `psp-music-banks.md` 3 has the SNR of the inverse.
- **The game was not run on a PSP-only cache.** The engine's start-up check
  (`music-seq-engine.md` 3) reads the same files the host render does, so it
  should pass; V2 / V3 will show it.
- **The Jukebox's catalogue** (`music-names.tsv`) was not beside the scratch
  launcher; the hidden rows apply to the PSP list as to the others (the same
  filter).

## 9. For the other files

**Folded in 2026-10-12** (the coordinator's pass, branch `psp/ledger`): DIV C is **DIV-0105**, MB's text folded in; the README row, STATUS and owner-review (the ear) are in; the `dialog_test.py` note is HANDOFF's trap.

**DIV-0105 (drafted as DIV C, numbered 2026-10-12), "A PSP-only install plays the
PSP's music: its sequences and its banks".** MB's draft (`psp-music-banks.md`
9) is folded in here; the dropped tone stays MB's separate bullet or entry.

> - **Original behaviour.** The PC port plays its MP3s. With a cache
>   (DIV-0087), the songs are the PSX disc's sequences on the PSX's banks,
>   through `psx::MusicSynth`. The PSP re-authored all 166 songs as Standard
>   MIDI Files (`pPMS`): 73 carry the PSX's events exactly; the others differ
>   in notes (16), controllers (11), pitch-bend resets (56), the setup of song
>   31, event order within a tick (5), the rhythm field (4) and two tempos
>   (22, 157). Its music banks merge each tone's volume with its program's and
>   the VAB master's (`(tv * pm * bm * 32) // 127**2`), and its pan is
>   `(tp + mp + 64) // 3`, a third of the PSX's spread.
> - **New behaviour.** When the player gives a PSP disc and no PSX disc, the
>   importer writes `base/bgm/` from the PSP: the songs inverted to the
>   cache's song form (`tools/ppms.py`, [`psp-music.md`](psp-music.md); format
>   only - every note, bend and controller is the PSP's), and the banks with
>   the merge inverted (`vag.psp_music_vab`,
>   [`psp-music-banks.md`](psp-music-banks.md)): master and program volumes
>   127, each tone's volume the merged one rounded, program pans 64, each
>   tone's pan `3p - 128`; samples, pitch, range, ADSR and mode the PSP's,
>   which are the PSX's. The sequencer, the PSX driver's model, plays them
>   unchanged. The Jukebox lists them as the PSP disc's.
> - **Rationale.** The owner's call (`psp-only-build.md` 7, call 2): a
>   PSP-only install uses the PSP's audio as it is; there is no PSX original
>   to fall back on. Of the merge, the inverse undoes what the conversion
>   lost, where playing the squeezed pan as it is would narrow the image.
> - **Also in the PSX version?** No: the PSX plays its own sequences and the
>   unmerged bank fields. The PSX driver neither resets nor remembers a bend
>   at key-on (`_SsVmKeyOn` -> `note2pitch`, disassembled), so the PSP's 728
>   centre resets after a real bend retune voices still sounding.
> - **Verification.** `ppms.py check`, `vag.py check`; `importer.py verify`
>   against `fixtures/bgm-psp.tsv` (247 of 247, both PSPs); the song A/B on
>   the PSX banks: 80 of the 93 songs that are not byte-identical differ
>   within 180 s, 50 of them the bend-only songs (`psp-music.md` 4.2); the
>   banks against the PSX-built: 166 songs, SNR median 41.9 dB, one song at
>   16.8 dB (22), level within half a decibel (`psp-music-banks.md` 3); all
>   166 render on the PSP banks with no abort. Not heard.
> - **Reversible?** A PSX disc among the sources builds the PSX's songs and
>   banks. With both discs, the PSP's music as an option is TG's `opt/` half.

**`docs/README.md` row:** "[`psp-music.md`](psp-music.md) | The PSP's songs
(`pPMS`, a table of SMFs) inverted to the cache's song form by
`tools/ppms.py`; the song and path tables from `BOOT.BIN`. 73 of 166
byte-identical to the PSX-built, the rest classed (rhythm 4, order 5, bend
56, controllers 11, setup 1, notes 16; tempos 22, 157); PSP-JP = PSP-EU. The
PSX driver neither resets nor remembers the bend at key-on (`_SsVmKeyOn` ->
`note2pitch`, disassembled): 50 of the 56 bend-only songs render differently
through our sequencer. Stage B: the importer writes `base/bgm/` from a PSP
disc alone with MB's banks (247 files, `fixtures/bgm-psp.tsv`, verify 0
problems), and the Jukebox lists them as the PSP disc's. MEASURED; not
heard."

**STATUS.md:** "MS (2026-10-12): a PSP-only build writes `base/bgm/` - the
PSP's 166 songs (`tools/ppms.py`) on its 81 banks (MB's inverse) - and the
Jukebox lights its PSP source on such a cache. The bend-only songs are real
differences on the PSX driver's model. [`psp-music.md`](psp-music.md)."

**HANDOFF.md / owner-review.md:** the owner's ear (V4) on the PSP's music from
a PSP-only cache through the Jukebox: the 16 note songs, 22 and 157 (tempo),
31; and the question of section 5, whether the Jukebox should play the PSP's
squeezed pan "as the PSP has it" rather than the inverse.

**`tools/launcher_host/dialog_test.py`** (whoever owns it): `setup` should also
write a dummy `bof3x.exe` beside the launcher, or the dialog never opens (the
state-3 launcher's check).
