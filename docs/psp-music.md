# The PSP's music: `pPMS` to the cache's song form

**Status:** IN PROGRESS (2026-10-12), group MS of the PSP-only round
([`psp-only-round.md`](psp-only-round.md)). Stage A is done: `tools/ppms.py`
reads a PSP disc's songs and writes them in the form `base/bgm/` stores. It
reproduces the 2026-10-10 measurement in code and adds a render A/B. Stage B
is still to come: the importer writing `base/bgm/` from a PSP source, paired
with MB's banks. Nothing in `src/` or the importer changed. No game bytes
appear here, only counts, hashes, offsets and rules (rule 1).

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
  sequencer, most of the 56 do sound different (section 4.2):** the songs are
  not an encoding difference, they are the PSP's own music.

## 1. What was built

`tools/ppms.py` (stage A):

| Function / command | What it does |
|---|---|
| `read_ppms(section)` | The `pPMS` table and its SMFs; anything outside the measured layout is refused |
| `invert(sub)` | One SMF as `seq.read_sep` gives a PSX sub-song. `seq.song_bytes` then writes it unchanged (`seq-format.md` 1) |
| `songs(disc)`, `file_paths(disc)` | The PSP's song table (`tables.toml` `songs`, the `psp-jp` / `psp-eu` rows in `BOOT.BIN`) and `BOOT.BIN`'s path table, which the song table's file ids index |
| `song_files(iso)` | The 166 song files, `(song, bank name, bytes)`. This is what stage B writes under `base/bgm/` |
| `measure --disc ISO --psx CUE [--json]` | Each PSP-built song against the PSX-built one, with its class and the inversion's rule counts |
| `dump --disc ISO --song N` | One inverted song, printed the way `seq.py dump` prints it |
| `check` | A synthetic round trip with no game data: `seq.py`'s synthetic SEP is put into the PSP's form at divisions 480 and 48 and inverted back, and the refusals are tried. 0 errors |

`check` is not yet called from `importer.py check`, because the importer
belongs to stage B (section 6).

## 2. The format, as measured

`python tools/ppms.py check` holds the layout as a test. The counts below
come from scratch census scripts over all 81 files of both PSP discs
(`psp_ms/probe3.py` and the inline probes in the session scratchpad):

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

RENDER_TABLE

## 5. Owner's calls this meets

- **Call 2, 2026-10-10** (`psp-only-build.md` 7): a PSP-only install plays the
  PSP's music as it is. `song_files` gives the PSP's own songs. The inversion
  changes format only (ticks, metas, note-offs, the loop pair, the end tick)
  and keeps every note, bend and controller the PSP has.
- **Game data in code, 2026-10-10:** nothing is copied. The song table is
  read from `BOOT.BIN` at `tables.toml`'s recorded place, and the path table
  is found by shape. The only constants are the format's own (divisions,
  magic, loop values) and `seq.py`'s song 165 rule.
- **The jukebox's PSP column** (`music-player.md` 3) stays greyed until stage
  B lands, as before.

## 6. What stage B needs

- **MB's merge:** `vag.psp_music_vab(pphd, pbvc) -> (vh, vb)`, the contract
  in `psp-only-round.md` 4. Each song's bank is its EMI's (`song_files` gives
  the name), and `seq.parse_vab` / `bank_bytes` write it as `seq.build` does.
- **The importer:** one call site in `build`. When no PSX disc is given and a
  PSP disc is, call `ppms.importer_bgm(sources, out)` (to be added, mirroring
  `seq.importer_bgm`). The manifest's `[bgm]` build record names `psp-jp` /
  `psp-eu` and the method. `ppms.check()` goes beside `seq.check()` in
  `importer.py check`.
- **`verify`:** a fixture of the PSP build's song and bank hashes, as
  `fixtures/bgm.tsv` has per PSX build. JP and EU are identical (section 3),
  so one set of rows serves both. A second check could read each song back
  through `seq.read_song` and the `pPMS` independently.
- **The jukebox:** `ListPsp` can list a cache whose `[bgm]` record is a PSP
  build. A cache holds one `base/bgm`, so a PC + PSX + PSP install still has
  only the PSX's songs there. A PSP column beside a PSX one needs a second
  tree (an `opt/` layer, TG's half), which is not stage B's job.
- **Not decided here:** whether a PSP-only cache's `base/bgm` takes the PSP's
  setup and loop bytes as they are. That is call 2's reading, and this doc
  takes it so.

## 7. Not verified

- **No song has been heard.** The render A/B shows where the samples differ,
  not whether an ear can tell. The owner's ear on the 16 note songs, the 2
  tempo songs and song 31 is V4's (`psp-only-round.md` 6).
- **The PSP's own player was not read.** Section 4.1's guess about why the
  PSP added the resets stays a guess. What the PSP itself sounds like is not
  what our sequencer plays from these files. Ours is the PSX driver's model.
- **The renders use the PSX's banks.** With MB's PSP banks (the volume and pan
  merge) every song will differ again. That is stage B's and V1.4's
  measurement.
- **The sequencer's slow-mode abort** (a tempo change into libsnd's slow mode)
  cannot trigger: the PSP has no tempo past tick 0. The render ran every
  song without an abort (section 4.2).
- `importer.py check` and `ledger_check.py` pass (0 errors). The importer and
  `src/` are untouched, so the PC and PSX path checks are moot for stage A.

## 8. For the other files

**DIV C (draft; the coordinator numbers it), "A PSP-only install plays the
PSP's music":**

> **What the original did.** The PC port plays its MP3s. With a cache
> (DIV-0087), the songs are the PSX disc's sequences through `psx::MusicSynth`.
> The PSP re-authored all 166 songs as Standard MIDI Files (`pPMS`). 73 of
> them carry the PSX's events exactly. The others differ in notes (16),
> controllers (11), pitch-bend resets (56), the setup of song 31, event order
> within a tick (5) and two tempos.
> **What this does.** When the player gives a PSP disc and no PSX disc, the
> importer writes `base/bgm/` from the PSP's songs (`tools/ppms.py`,
> [`psp-music.md`](psp-music.md)), inverted to the cache's song form, with the
> PSP's music banks (`vag.psp_music_vab`, MB). The sequencer, the PSX driver's
> model, plays them unchanged. With a PSX disc present, the PSX's songs stay
> the base.
> **Why.** The owner's call of 2026-10-10 (`psp-only-build.md` 7, call 2): a
> PSP-only install uses the PSP's audio as it is. There is no PSX original to
> fall back on.
> **Audible.** Rendered through our sequencer, RENDER_AUDIBLE. Unheard.

**`docs/README.md` row:** "[`psp-music.md`](psp-music.md) | The PSP's songs
(`pPMS`, a table of SMFs) inverted to the cache's song form by
`tools/ppms.py`. The song and path tables come from `BOOT.BIN`. 73 of 166
byte-identical to the PSX-built, the rest classed (rhythm 4, order 5, bend
56, controllers 11, setup 1, notes 16; tempos 22, 157). PSP-JP = PSP-EU. The
PSX driver neither resets nor remembers the bend at key-on (`_SsVmKeyOn` ->
`note2pitch`, disassembled), so the PSP's 728 resets after a real bend retune
sounding voices. The render A/B result. Stage B (the importer, with MB's
banks) to come. IN PROGRESS."

**STATUS.md:** "MS stage A (2026-10-12): `tools/ppms.py` reads the PSP's
songs from the disc alone and reproduces the 73-identical measurement. The
bend-only songs are audible differences on the PSX driver's model, not an
encoding. [`psp-music.md`](psp-music.md)."
