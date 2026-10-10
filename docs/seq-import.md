# Music from a disc: the songs and banks the cache holds (`base/bgm/`)

**Status:** STABLE (2026-10-08, verified 2026-10-10: `verify` on `base/bgm/`, 4.1; a cloud session, group IMP of
[`sequenced-music-plan.md`](sequenced-music-plan.md) section 9; the JP disc
measured)

[`sequenced-music-plan.md`](sequenced-music-plan.md) section 3. `tools/seq.py`
writes the disc's music into the cache in the format of
[`seq-format.md`](seq-format.md). Each song becomes `base/bgm/NNN.DAT`: its
sub-song's events as a flat table. Each `BGM*.EMI`'s VAB becomes
`base/bgm/bank/NAME.DAT`: programs, tones and the SPU's ADPCM bodies. Nothing
here plays anything. The player is group SEQ's, and the engine seam is ENG's.

```
python tools/seq.py build  --disc "<JP>.cue" --out CACHE      (CACHE/base/bgm/, 2 s)
python tools/seq.py dump   CACHE/base/bgm/021.DAT             (a song or a bank as text)
python tools/seq.py verify --disc "<JP>.cue" --cache CACHE    (against inventory.py and vag.py)
python tools/seq.py check                                     (synthetic round trip, no game data)
```

## 1. What it reads

- **The EMI.** Every `BIN/BGM/*.EMI` holds three sections:
  - one type 6, the VAB header;
  - one type 10, the SEP;
  - one type 7, the VAB body.

  In 3 of the 81 there is also a type 8 cue table, which nothing here reads.
  `songs` refuses an EMI without exactly one each of 6, 10 and 7.
- **The VAB header** (`parse_vab`) is Sony's layout:
  - a 32-byte header: `pBAV`, version 7, `fsize`, `ps`, `ts`, `vs`, `mvol`,
    `pan`, `attr1`, `attr2`;
  - `ProgAtr[128]` of 16 bytes;
  - `VagAtr[ps * 16]` of 32 bytes;
  - 256 u16 sample sizes, in 8-byte units. Entry 0 is unused, and sample i is
    entry i.

  These are checked on all 81 VABs, and each check is an error if it fails,
  never a skip:
  - the header's length is exactly that;
  - the size table's sum is the body's length;
  - entries 0 and past `vs` are 0;
  - every sample is a whole number of 16-byte blocks;
  - `fsize` is header plus body;
  - the programs' `tones` sum to `ts`;
  - `ps` is the count of programs with tones.
- **The tone blocks follow the programs that have tones, not the program
  numbers.** Block k holds the tones of the k-th program with `tones > 0`.
  Every tone in it carries `prog` = that program, in all 81 VABs. 27 of the
  81 have gaps: `BGM053`'s one program is 10, and `BGM032`'s are 2..7. Version
  1 of the bank format indexed by program number, which would have played the
  wrong tones in those 27. Version 2 records the block in the program record
  ([`seq-format.md`](seq-format.md) section 2).
- **The SEP** (`read_sep`, `read_track`) is `pQES` version 0, a list of
  sub-songs. Each is `{u16 id, u16 resolution, u24 tempo, 2 rhythm bytes, u32
  length}` followed by events:
  - a MIDI delta time, then the status, with running status for channel
    messages;
  - meta `FF 51 tt tt tt` (tempo, with no length byte) and `FF 2F 00` (end).

  The walk is `tools/bgm/inventory.py`'s `parse_track`, made to return every
  event. Any other status or meta event is an error. The writer cannot import
  `inventory.py`: it imports the sibling checkout at load time, which a
  player's machine does not have. `verify` does import it, as the independent
  parse.
- **The song table** (`songs`) has 165 entries of `{u16 file id, u8 seq, u8
  sub}`. The address is `tables.toml`'s `songs` row for the disc's build
  (`tables.disc_build` by `SYSTEM.CNF`): JP `0x80182830`, US `0x80181EB8`,
  PAL `0x80182384`. So any catalogued PSX disc works.
  - A file id indexes the boot EXE's table of disc-file LBAs (the sibling's
    `file_ids.py`, JP `0x80182DBC`). That table is not catalogued, so `seq.py`
    finds it by shape: the longest run of words that are each a file's LBA.
    On JP the run is 887 words at `0x80182DBC`, all 887 files on the disc.
  - Each row must have `seq` = 0 and `sub` ≤ 3, and must name a `BIN/BGM/`
    file. The sub-song's own `id` must equal `sub`, and its events must end
    exactly at its length. All 165 pass.
  - **Song 165** is added: `BGMBAT00.EMI` sub 1
    ([`bgm-comparison.md`](bgm-comparison.md) 6.3).
  - The table read equals `inventory.json`'s table in 165 of 165 (file and
    sub).

## 2. What it writes (JP disc)

| | Count | Bytes |
|---|---:|---:|
| Songs `base/bgm/NNN.DAT` | **166**: 000..165, 021 included, 166 absent | 3,400,244 |
| Events in them | 282,579 (11 in song 49 .. 7,163 in song 157) | |
| Banks `base/bgm/bank/NAME.DAT` | **81**: every `BGM*.EMI`, all named by some song | 18,584,364 |
| Samples in them | 1,195 | |
| Total | 247 files | 21,984,608 |

- The table names all 81 `BIN/BGM/*.EMI`, and each song's bank file exists.
  `BGMBAT00` is named by table songs too, not only by 165.
- **Looping.** 157 songs loop and 9 play once: 4, 9, 28, 42, 58, 96, 105,
  141 and 150, the PC's `N` files. Every song has resolution 48.
- **Which events occur.** Across all 166 songs there are five event types:
  - note-on (`0x9n`): 256,278, half of them velocity 0. **No `0x8n`
    note-off occurs**;
  - controller (`0xBn`): 4,476, numbers 7, 10, 99 and 6 only. There is no
    11, 64 or 98, though the plan expected them (section 2 item 2);
  - pitch bend (`0xEn`): 20,584;
  - program (`0xCn`): 1,075;
  - end of track: 166.

  **There is no tempo meta event** in any song: the header's tempo is the
  only tempo. Nothing falls outside the known set.

## 3. The irregulars

- **Song 21** (`BGM019` sub 0) is written: 399 events, loops at 18..1554.
  The PC has no file for it (D24).
- **`166`** has no sequence and gets no file.
- **No table song is an empty placeholder.** Every one of the 166 has at
  least one sounding note-on. The thinnest are drones: song 49 (`BGM041`
  sub 0) is two held notes, released by the note-ons with velocity 0 that
  follow the loop-end marker at the same tick. Order within a tick is kept
  for that reason ([`seq-format.md`](seq-format.md) section 1).
- **Two loop starts.** Songs 37 (`BGM028` sub 1: starts at ticks 18 and 402,
  end 1170) and 80 (`BGM082` sub 0: 24 and 1560, end 6936) each set the loop
  start twice before their one end. The header records the first. Both
  markers are in the events, and which start libsnd jumps back to is the SEQ
  reading's question. Neither song's loop was measured from its render
  (`loops.json`: correlation under 0.8, excluded).
- **No VAB disagrees with its body.** The size table equals the body's length
  in 81 of 81.
- **Reserved bytes.** Every tone record's reserved bytes are the same fill,
  `B1 B2` and `C0 00 C1 00 C2 00 C3 00`, in all 14,496 records. Program
  records' are `FF` (and byte 5 is 0 or `FF`). None is carried, and nothing
  is lost.
- **The 38 bundles** (`BATTLE*`, `BOSS*`, `DEMO`) are whole copies: VAB
  header, SEP and body hash equal to one `BGM` file's. The copies are of
  `BGMBAT04` 22, `BGMBAT02` 6, `BGMBAT01` 5, `BGMBAT03` 2, and `BGMBAT05`,
  `BGMBAT06`, `BGMOPN` 1 each. `BGMBAT02` is itself byte-identical to
  `BGMBAT00`. This bears on 165, the fanfare (plan section 6):
  - its sequence is sub 1 of `BGMBAT00` .. `04`, identical in all five;
  - `BGMBAT05` and `06` have an empty sub 1. `BGMBAT05`'s sub 3 is song 159
    (760 notes);
  - the banks of `BGMBAT00` (`= 02`), `01`, `03` and `04` are four different
    banks.

  So on the PSX the fanfare after a fight sounds through that fight's bank,
  one of four. The cache's `165` names `BGMBAT00`'s bank, which 6 bundles
  share. Playing it through the fight's bank instead is a choice for ENG and
  the owner. Since every bundle is a byte copy of a `BGM` file, nothing more
  is imported from the bundles.

## 4. The hook

`tools/importer.py` (17 lines added, 4 changed):
- `import seq`;
- in `build`, after `xa.importer_snd`, `seq.importer_bgm(sources, out)`. It
  writes `base/bgm/` from the first PSX disc among the sources, or nothing;
- `write_manifest` gains a `bgm = [[file, source, sha256], ...]` array under
  `[cache]`. The source is `psx-jp:seq:BGM019.EMI#1` for a song and
  `psx-jp:vab:BGM019.EMI#0,2` for a bank (the VAB header's and body's
  sections);
- `check` runs `seq.check()` and counts its errors with the rest.

Nothing else in the importer changed. `importer.py build --source <JP>.cue`
gives the same containers as before, plus `base/bgm  from psx-jp: 247 files
(seq.py)`. Its `base/bgm/` is identical to `seq.py build`'s, and all 247
manifest hashes match the files. `importer.py verify` on that cache is
unchanged: 727 containers held, 0 problems, `base/exe/` as recorded.

### 4.1 `verify` and `check` on `base/bgm/` (2026-10-10)

`build` already recorded each file's hash in the manifest's `bgm` rows; now
`importer.py verify` (`verify_bgm`) reads them back:

- every row's file is there and hashes to its row;
- every file under `base/bgm/` is named by a row (no strays);
- every song and bank parses with `seq.read_song` / `seq.read_bank`, and
  every song's bank is in the cache;
- the rows against **`fixtures/bgm.tsv`**, the files `seq.py build` writes
  from each PSX disc (build, path, size, the EMI sections, sha256; 247 rows a
  build, written by `python tools/seq.py fixture --disc <cue> ...`). This
  catches a file changed together with its manifest row, and a cache written
  by an older `seq.py`.

The fixture was written from the five PSX discs on 2026-10-10 (`psx-jp`,
`psx-us`, `psx-eu-en`, `psx-fr`, `psx-de`): **all 247 files are identical in
all five builds** - every song and every bank. Its header names the song and
bank format versions (1 and 2); `importer.py check` runs
`seq.check_fixture` (CI): the versions equal what `seq.py` writes, every build
is a PSX build of `fixtures.toml`, each holds songs `000..165` and the bank of
each song's EMI and nothing else, every row well formed. A format bump fails
`check` until the fixture is regenerated.

Measured on the owner's cache (`analysis/cache/pc-plus-us`, built from the US
disc): `base/bgm/ (psx-us): 247 files, 166 songs, 81 banks; psx-us: 247 of
247 as fixtures/bgm.tsv; 0 problem(s)`. Negative controls on scratch copies
(each exit 1, the line naming the file): one bit of `003.DAT` flipped; a
stray `999.DAT`; bank `BGM000` deleted (and its two songs' bank reported
missing); bank `BGM002` truncated by 8 bytes; `010.DAT` changed with its
manifest row rehashed (caught by the fixture alone). On `check_fixture`: the
song format set to 2, a song row dropped, a bank row dropped, an unknown
build - each refused.

## 5. The checks

| Check | Result |
|---|---|
| `seq.py check`: a synthetic SEP and VAB, made in memory, written and read back. The SEP has two sub-songs, running status across a velocity-0 note-on, two tempos, the loop markers, program, bend and pressure. The VAB has programs 0 and 3 (a gap) and two samples. Also refused: a wrong tone count, a body longer than the size table, a wrong file version, an unknown meta event. Two injected faults (a shifted marker, a wrong block) were each caught | **0 errors**; runs in `importer.py check` (CI) |
| `seq.py verify`: each song file read back, against `inventory.py`'s `parse_sep` of the same SEP. Compared: note-ons, controllers by number, programs, channels, loop markers (tick and kind), tempo changes, end tick, resolution, initial tempo, and the header's loop start / end | **166 of 166 agree** |
| `seq.py verify`: each bank read back, against `vag.py`'s reading of the VAB (programs at `0x12`, samples at `0x16`, `vag._vags`' bodies) and against the header's raw tone records field by field. Also checked: program / tone counts against `ps` / `ts` | **81 of 81 agree** |
| The song table read against `inventory.json`'s (the owner's run) | 165 of 165 |
| `importer.py check` | 0 errors, unchanged output plus the `seq check` line |
| `importer.py verify`'s `base/bgm/` check (4.1, 2026-10-10) | 247 of 247 on the US-disc cache; six negative controls refused |

## 6. For the other files

- **`importer.md`**:
  - the status line: "step 9's `base/bgm/` in [`seq-import.md`](seq-import.md)";
  - section 2 item 4: the manifest's `bgm` array;
  - section 2 item 5: `check` runs `seq.py check`.
- **`unified-data-plan.md`**: step 9's importer half is done. `base/bgm/`
  comes from any PSX disc in the sources (166 songs, 81 banks), and the
  player and engine seam remain (`sequenced-music-plan.md` groups SEQ / HOST
  / ENG). Section 6's `BGM/` row: from a disc, sequences in the cache. The
  MP3s stay for a PC-only install and for `166`.
- **`STATUS.md`**:
  - `tools/seq.py` writes `base/bgm/` (166 songs, 81 banks, 22.0 MB from JP)
    with `dump`, `verify` (166 / 166 against `inventory.py`, 81 / 81 against
    `vag.py`) and a CI round trip;
  - the bank format is at version 2, because of the tone-block finding.
- **`HANDOFF.md`**:
  - the engine reads nothing from `base/bgm/` yet (ENG);
  - ~~the importer's `verify` does not yet hash `base/bgm/`~~ (done 2026-10-10, 4.1);
  - SEQ's reading should settle the two-loop-start songs (37, 80) and the
    order of the loop end against the note-offs at the same tick (song 49).
- **`sequenced-music-plan.md`**:
  - section 0: "38 battle / boss / demo bundles" are whole copies of seven
    `BGM` files;
  - section 2 item 2: the songs use controllers 6, 7, 10 and 99 only, with
    no note-off status and no tempo event;
  - section 6, "the 38 bundles' sub 1": checked by hash, there are four
    distinct banks under the fanfare, and two bundles' (`BGMBAT05` / `06`
    copies) have no fanfare at sub 1;
  - section 9: IMP done.
- **`bgm-comparison.md`** 6.2: "Each `BOSS` / `BATTLE` / `DEMO` SEP is a `BGM`
  file's ... sub 1 the battle bundles' own" holds for the copies of
  `BGMBAT01`..`04` only. The `BGMBAT05` / `06` copies have sub 1 empty
  (`BGMBAT05`'s sub 3 is song 159), and `DEMO` (`BGMOPN`) has only sub 0.
- **`README.md`** (docs index): a row for `seq-import.md`, and for
  `seq-format.md` if it has none.
- **Ledger**: nothing yet. Writing the cache changes no behaviour until the
  engine reads it (plan section 5).
