# State by name, group B1: the quicksave's six participants

**Status:** STABLE (2026-11-25, verified the same day on the Linux x86-64
ELF of `sbn/B1`; section 6 has the measurements). The state-by-name round's group B1
([`state-by-name-round.md`](state-by-name-round.md) 3.6, 4). Built: the
participants' snapshot functions in `src/render/gfx_snapshot.cpp`,
`src/game/field/field_7.cpp`, `src/game/platform/sound_ours.cpp`,
`src/game/solo/solo_battle.cpp` and `ST` in `src/hook/snapshot.cpp`;
`src/game/core/new_game.cpp` read and left as it is. Not played by the owner.
Ledger: draft E below (section 7), numbered at the merge.

The quicksave carries one record per participant, each with a version of its
own ([`quicksave.md`](quicksave.md) 5). This round asks two things of each
(round doc 3.6): a record this build does not read is a refusal from the
participant's check, before a byte of the span is written - never a `Fatal`
after it; and the record says whether its bytes depend on the build's width
or system, and if they do it is written field by field at its next version.

## 1. The six, measured against A1's hand-over

A1's notes (`state-by-name-format.md` 9, "B1") said `ST`, `SOUND`, `SOLOCB`
and `NEWGAME` refuse in their check, and `GFX` and `BEHAVIOURS` check
nothing. Read again at the round tip (`4132fc47`):

| Participant | File | Version | Check before this change | Another version was |
|---|---|--:|---|---|
| `GFX` | `src/render/gfx_snapshot.cpp` | 3 | none registered | a `Fatal` in `Restore`, after the span |
| `SOUND` | `src/game/platform/sound_ours.cpp` | 1 | the music source only; `if (... != kSnapVersion) return true` | **a `Fatal` in `SnapRestore`, after the span** |
| `BEHAVIOURS` | `src/game/field/field_7.cpp` | 4 | none registered | a `Fatal` in `BehavioursRestore`, after the span |
| `SOLOCB` | `src/game/solo/solo_battle.cpp` | 3 | the version | a refusal ("solo battle record") |
| `NEWGAME` | `src/game/core/new_game.cpp` | 1 | the size and the version, and the name screen's switch and grid | a refusal ("New Game record") |
| `ST` | `src/hook/snapshot.cpp` | 1 | the language only; `if (... != kStVersion) return true` | **a `Fatal` in `StRestore`, after the span** |

**A premise that did not hold:** `ST` and `SOUND` had a check, but both
passed a record of another version (and any record they could not walk) on
to their restore's `Fatal` - their comments said so ("StRestore's Fatals say
what is wrong", "let through to SnapRestore's own Fatals"). So four of the six
reached a `Fatal` after the span on another version, not two. All four now
refuse in their check. `SOLOCB` and `NEWGAME` refused already; `SOLOCB`'s
check now also refuses a record whose bytes disagree (section 3).

## 2. What each record holds, and whether it is the width's or the system's

Read, not run: this machine builds only the 64-bit Linux ELF. Where a
question is one of layout, a `static_assert` settles it for every build that
compiles (it fails the 32-bit or the Windows build if it is wrong there). Our
targets are x86 only, little-endian, with `unsigned` and `int` 32 bits in
both data models (LP64 on Linux, LLP64 on Windows) and `bool` one byte; a
`long` would differ (8 bytes on Linux x86-64, 4 elsewhere) and none is
written. "The same" below means the same bytes for the same state at 32 and
64 bits and on Windows and Linux.

### GFX (version 4, this change)

| Field | Bytes | From | The same? | Why |
|---|--:|---|---|---|
| version | 4 | `kVersion` (`gfx_snapshot.cpp:50`) | yes | `uint32_t` |
| generations | 512 x 4 | `Gfx_ClutRows[y].generation` (`:262`) | yes | `U` = `uint32_t` |
| rows with a block | 4 | count (`:265`) | yes | |
| each row | 2 + `0x1000` | the row's index as `uint16_t`, the block's bytes (`:269-270`) | yes | the converted pixels, not the block's pointer |
| the pipe state | 27 | `render::PipeState` field by field (`:274-286`) | yes, at version 4 | **version 3 wrote the struct by `sizeof` (`gfx_snapshot.cpp:195-196` at the round tip): a `TexVersion*` first and two runs of `bool` with padding - 84 bytes on i686, 88 on x86-64** (`static_assert` at `:204`); its padding was whatever `g_state`'s copy held. Version 4 writes the twelve fields the restore uses as `uint32_t` and `uint8_t`; the texture and the grabs are never written (`PipeForSnapshot` clears them, `RestorePipe` keeps them clear) |
| the clear colour | 4 | `render::ClearColor()` (`:287`) | yes | `U` |
| layering's sub-record | 4 + n | `layering::SnapshotSave` (`layering.cpp:819`) | yes | `uint32_t` counts, `uint16_t` index, the run's `at` (`uint32_t`, `Address()` of a cell record or sprite in the span: the same place in every build at `0x400000`), two `uint8_t`, `Box` (four `float`), the quads' `float` |
| map_layers' | 4 + n | `map_layers::QuickSaveSides` (`map_layers.cpp:144`) | yes | bytes |
| widescreen's | 4 + 616 | `Widescreen_QuickSave` (`widescreen.cpp:849`) | yes, by layout | `Split[22]` by `sizeof`: 4 one-byte fields then six `int`, 28 bytes, no padding at any width (`widescreen.cpp:178`); its first word is its size, compared by the check |
| wide_rand's | 13 | `wide_rand::QuickSave` (`wide_rand.cpp:72`) | yes | a `uint8_t`, three `uint32_t` |
| map_rim's | 8, or 8 + the rim | `map_rim::QuickSave` (`map_rim.cpp:275`) | the bytes' sizes yes; **two kinds of word no** | `uint8_t` cells, `uint16_t` runs, `uint32_t` words. The extra layer (`g_extra`, 12 words) holds, for an empty list, the head's own address (`map_rim.cpp:208-216`, `Low32(head)`): a place in our module's `.bss`, which another build or system links elsewhere. The other words are 0 or draw items in the pool (the span). `g_table_item` holds item numbers, not addresses |
| the extra layer's place | 4 | `Va(map_rim::ExtraLayer())` (`:294`) | no, by design | new at version 4: so the restore can rebase the heads (below) |

Each sub-record is framed by its byte count at version 4 (`:289-293`), so the
check can walk the record to its end without knowing their layouts.

**The extra layer's heads.** At an exact load the place is this process's and
nothing moves. Elsewhere (a portable load, when one exists) the restore
rebases each of the 12 words that names a place inside the take's layer to
the same place in this process's (`RebaseWords`, `Restore` after map_rim's
sub-record), only when map_rim put the layer back (taken wide, wide now).
A pool item's address cannot fall in that range: the layer is in our module,
the pool in the span.

### SOUND (version 1, unchanged)

| Field | Bytes | The same? | Why |
|---|--:|---|---|
| version | 4 | yes | |
| 6 banks: data word, bytes, samples | 8 + n each | yes | the data word is a heap address in the span's own form (`uint32_t`, the narrow image's arena), compared with the span's word at the restore; the samples are bytes |
| 23 channels: bank, entry, playing, frequency, position | 11 each | yes | three `uint8_t`, two `uint32_t` |
| the last stream's file | 4 + n | yes | |
| the voice stream | 6 | yes | |
| `Music_File` | 13 | yes | `uint8_t`, three `int32_t` |
| the music stream | 20 + n | yes | four `uint8_t`, `int32_t`, three `uint32_t`, bytes |
| `sound::g_resume` | 24 | yes, by layout | a struct of ours by `sizeof`: `bool, unsigned, uint32_t, bool, unsigned, uint32_t`, 24 bytes with 3 + 3 of padding on every x86 target (`static_assert` in `sound_ours.cpp`, beside `SoundBank`). Its padding bytes are written as the static holds them and never read |
| fade frame, unseen stop, unducked, stream missing | 13 | yes | `uint32_t`, `uint32_t`, `float`, `uint8_t` |

Not the width's or the system's: left at version 1.

### BEHAVIOURS (version 4, unchanged)

`uint8_t` and `uint16_t` statics, two `uint32_t` counts, the master's saved
text (`g_master_saved`, bytes of the text arena) and the town warp's ids and
page (bytes) - `field_7.cpp:779-798`. No struct, no pointer: **the same**, so
the version stays 4.

**But a word of the span names our module** while the town warp's menu is
up: `WarpBuildPage` sets `Msgbox_Base` and `MenuWindows_ListText` to
`g_warp_message`, a buffer of ours (`field_7.cpp:548-549`). A1's words
written by their names do not cover it (a run-time value, not one of
Capcom's slots before the injects, `span_words.h` `g_slots`), so a portable
load would write the take's address raw. It is not BEHAVIOURS' record; it is
in the hand-back for A1's file (section 8).

### SOLOCB (version 3, unchanged), NEWGAME (version 1, unchanged), ST (version 1, unchanged)

- `SOLOCB`: three `uint8_t`, a `uint32_t`, `Bytes` twice and `StepSeven` by
  `sizeof` (six `uint8_t` each: no padding), a `uint8_t`, three bytes - 33
  bytes with the version (`static_assert` in `solo_battle.cpp`). The same.
- `NEWGAME`: `Record` by `sizeof`: four `uint8_t`, `name_grid::Cells` (ten
  `unsigned char`), a `uint8_t` - 15 bytes, no padding. The same.
- `ST`: `uint32_t` version, seed (`crt_rest::RandSeedCell()` is a
  `uint32_t*`), the ring's count, the ring as `uint16_t`, the pool's bytes and
  the pool, a `uint8_t` (`snapshot.cpp` `StSave`). The same.

So of the six, **`GFX` alone was the width's** (its pipe state), and `GFX`
alone holds a word that is the build's (the extra layer's heads). Both are
version 4's.

## 3. The checks, and the Fatals that remain

A check runs on its record while `Parse` reads the file (`ParseRecords`),
before the discard step and before a byte of the span is written; a refusal
there is `Refuse`, which offers the save half when it is whole
(`SnapshotAnswer::RefusedSaveOffered`, tier 0). **`Parse` itself did not
change**: `GFX` and `BEHAVIOURS` now pass their check to `Snapshot_Register`,
as the other four did.

The reasons a player can meet (the overlay's "Quickload refused: ..." and F5's
question):

| Reason | When |
|---|---|
| `another graphics record` | `GFX` of another version (3 and before, or a later one); map_rim's sub-record of another version, widescreen's or wide_rand's of another size |
| `another behaviours record` | `BEHAVIOURS` of another version |
| `another sound record` | `SOUND` of another version |
| `another state record` | `ST` of another version |
| `other settings (CampMasters)`, `other settings (TownWarp)` | a master's talk or the town warp taken under the behaviour, loaded with it off |
| `other settings (DrawPool)` | `ST`'s draw-item ring of another size (`BOF3X_ORIGINAL=DrawPool`) |
| `damaged` | a record its check cannot walk to its exact end, or whose own words disagree |

The settings' fingerprint (A1) holds `BOF3X_ON`, `BOF3X_OFF` and
`BOF3X_ORIGINAL`, so in a format-2 file the three "other settings" above are
caught by it first; the checks catch them in a format-1 file of the same
build, which has no fingerprint.

**The Fatals that remain in each restore**, and why a record that passed its
check cannot reach them:

- `GFX` `Restore`: no discard step before it (the core's order, not the
  file); another version (the check); a row out of range or order (the
  check); out of memory for a row; a glyph that cannot be built again (the
  device, not the file); a sub-record restored from fewer bytes than its
  frame (a sub-record's layout changed without GFX's version - the comment at
  `kVersion` says a change raises it - or a file damaged inside a frame).
  Inside the sub-records: `layering`'s run and box counts and indexes,
  `map_layers`' size, `map_rim`'s table count - each bounded by a constant of
  the writer's layout, so reached only by a writer of another layout under
  the same version, or by damage; `map_rim`'s version and `widescreen`'s size
  are compared by the check.
- `BEHAVIOURS` `BehavioursRestore`: every condition is the check's (version,
  text and page within their rooms, CampMasters and TownWarp on), and the
  check requires the record's exact length.
- `SOUND` `SnapRestore`: the version, a bank with data and no bytes, a
  channel naming no bank, a track stream with no file, the record's length:
  the check. What remains is the span agreeing with the record (a bank's data
  word, a voice inside its bank's data: both from the same file, so only
  damage), memory, and `Music_File` read again differing in its loops or
  track where its size agreed (the check compares the size).
- `ST` `StRestore`: the version, the ring's size, the pool's size and the
  length: the check.
- `SOLOCB` `ReadRecord`: the version, the length and every agreement test
  (one function, `ReadBody`, shared by the check and the restore).
- `NEWGAME`: the version and the size: the check.

A file has no checksum over its snapshot half, so damage inside a record
that keeps its own words consistent is not a refusal anywhere; it was not
before.

## 4. The state revision

A1 (`state-by-name-format.md` 2): the revision is raised by a change that
gives a byte of saved state a new meaning. Version 4 of `GFX` changes how one
participant writes its own record - a change of the quicksave's contents,
which the participant's version names - and no byte of the span, the arena or
the TASKS record means anything new; the game's state is restored as before
(section 6: no state hash moved, the cross test exact). **The state revision
stays 1.** Every older `GFX` record is refused by its version, its save
offered.

## 5. The self-test

`BOF3X_SELFTEST_ONLY=1` in the exe host:

- `gfx_snapshot`: as before (a take, the live moment changed, the restore,
  a second take equal to the first), then version 4: the check passes the
  record; the record is its parts; refused before anything is written -
  version 3 and version 5 ("another graphics record"), a byte short and a
  byte long, more than 512 rows, a row out of order ("damaged"), map_rim's
  version word, widescreen's size word and wide_rand's size changed ("another
  graphics record"), map_layers' count not its bytes ("damaged"); the extra
  layer's heads rebased to a new place and nothing else moved.
- `behaviours` (new): a record of set statics read back the same and to its
  end; version 3 and 5 refused, a short and a long record, text past its
  room; the CampMasters and TownWarp gates refused when the behaviour is off
  in the run (passed when it is on).
- `SOUND`: as before, plus another version, a byte short, a byte long and a
  channel naming no bank refused by the check.
- `snapshot` (`ST`): as before, plus another version, a ring of another size,
  a byte short and a byte long.
- `solo battle`: as before, plus a byte short and a record whose bytes
  disagree refused by the check.

## 6. Measured (2026-11-25, the laptop, the Linux x86-64 ELF of `sbn/B1`)

Captures in `~/bof3x-runs/sbn1125/B1/root/` (outside the tree).

- **Build:** clean, no warning outside the third-party `_deps`
  (`build1.log`, `build2.log`).
- **Self-test:** exit 0, 10,424 log lines, every case of section 5 passed;
  the refusals are logged one a line ("refused as the self-test means:
  ..."), e.g. `GFX's record holds map_rim's rim record of another layout (8
  bytes, first word 3; this build's 8, 2)`. `selftest2.log`.
- **State hash**, orig6 conditions (`--only-slot0 --env BOF3X_SCALE=2`,
  `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the skip list), this ELF against the
  comparison base (`A4/wt`'s build of the round tip), `tools/statehash.py
  diff`: `bridgeWalk` (en-US) **identical on all 2,074 ticks**; `combat`
  **identical on all 2,602 ticks** (`sh/run.sh`, `sh/run.log`). Run on the
  first commit's build; the second commit changed one self-test constant.
- **Quicksave cross test** (`qs/run.sh`: the live proof form takes
  `BISLPS10.DAT`, the cross form loads it at frame 0 in the live run's
  folder with `--run-game none`, then `tools/quicksave_check.py cross`):
  - `combat` (a battle on screen; GFX put back 19 palette rows and rebuilt
    29 glyphs): **equal, 1,680 ticks from the restore**, tick for tick;
  - `cutsceneAndNue` (a scene mid-message; 17 rows, 20 glyphs): **equal,
    5,215 ticks**.
- **The refusal, driven:** A4's `tools/quicksave_check.py tier0`, unchanged,
  through a wrapper (`root/tier0_rec.py TAG`, not in the tree) that sets the
  mode's doctor byte (`STAMP_AT`) to the low byte of record TAG's version
  word once the take is written, so the doctor turns version 4 into 36. Route
  `combat`, take at 420 (the take's peeks: mode 5, area 29; the GFX record
  75,331 bytes):
  - `GFX`: the load's log `F7 BISLPS10.DAT refused: another graphics record -
    record 'GFX': GFX's record is version 36; this build reads 4 only (...)
    (the save half is whole: it can be loaded as a save)`, no restore line
    after it; then the question, the restart, the title's load of the save
    half; against the save half loaded through the title's own menu: area,
    position, came-from words and the last picture **equal**
    (`t0_GFX.out`, `t0_GFX/`);
  - `BEHAVIOURS`: the same road, `refused: another behaviours record - ...
    version 36; this build reads 4 only`, and the same equal comparison
    (`t0_BEHAVIOURS.out`, `t0_BEHAVIOURS/`).
- **No-data checks:** `ledger_check` (246 entries, 0 errors), `data_census
  --check` and `--strict`, `reloc_census --check` (686, as committed),
  `region_table --check`, `os_census` (three modes), `reloc_bare`,
  `retire_shells --strict`, `tu_census --strict`, `tu_renames`,
  `ledger_registry`, `ledger_switches`, `exe_own check`, `tables check`: pass.
  `width_census --check` reported version 4's five deliberate sites (the
  `PipeState` size assert, the extra layer's place narrowed through `Va` /
  `Low32` twice and logged twice); regenerated, and it passes. Not run: the
  slow `linux_tree.py` and `fnptr_census.py` (CI's).

## 7. Ledger: draft E (an amendment of DIV-0145) - taken into DIV-0145, 2026-11-25

- **What the original did:** as DIV-0145 has it (F6 nothing; F7 the next
  renderer device).
- **What this does (amended):** a quicksave whose record of ours - the
  graphics, the sound, the behaviours, our other statics - is one this build
  does not read, or is damaged, is refused before anything is written, with
  its reason ("another graphics record", "another sound record", "another
  behaviours record", "another state record", "damaged"), and its save is
  offered when its save half is whole. Before, each of the first four ended
  the game in a `Fatal` part-way through the load. A quicksave taken during
  a master's talk from the master list or with the town warp open, loaded
  with CampMasters or TownWarp off, and one taken with the original's 1,024
  draw items loaded with 2,048 or the reverse, are refused as "other
  settings" with the setting named (in a format-2 file the settings'
  fingerprint refuses them first). The graphics record is written at
  version 4: the same bytes at 32 and 64 bits and on Windows and Linux; every
  quicksave written before this change is refused for it, its save offered.
- **Why:** the state-by-name round (IDEAS I54; round doc 3.6): a record
  another build cannot read is a refusal, never a load that stops the game.
- **Saved state:** unchanged (revision 1): a participant's record is the
  quicksave's contents under its own version, not a byte of the game's state.
- **Reversible?** The keys are ours. A build without this change stops at a
  version-4 graphics record in its `Restore`, as before.

## 8. For other groups

- **A1's file (`snapshot.cpp` `NameWord`), not mine:** a word of the span
  that names our module outside Capcom's slots is written raw and makes the
  take no less portable. Measured by reading: `Msgbox_Base` and
  `MenuWindows_ListText` name `g_warp_message` while DIV-0225's town warp is
  up (`field_7.cpp:548-549`); the message box's own read position may follow
  them (`msg_box.cpp`, not traced). Proposed: a word of `.data` / `.bss` whose
  value `span_words::g_ours.Named` places, outside the slots and the kept
  ranges, marks the take not portable (a new `kNp` code), as
  `kNpRuntimeSlot` does for the message slots.
- **The five GFX sub-records' files** (`layering.cpp`, `map_layers.cpp`,
  `widescreen.cpp`, `wide_rand.cpp`, `map_rim.cpp`) were not edited. Their
  comments could say that a change of their record's layout raises `GFX`'s
  version (or, for map_rim, its own, which the check compares).

## 9. Not checked

- The 32-bit and the Windows builds were not compiled here: the
  `static_assert`s on `PipeState`, `Resume` and SOLOCB's layout are written
  for them and run only on x86-64 Linux.
- A portable load in play does not exist yet (A1, A2): the extra layer's
  rebase is run by the self-test on a stand-in layer, not in a load. And a
  wide-mode quicksave (the rim taken) was not run through the cross test.
- Version 3 records are refused, not read: a version-3 record can only come
  from another build (the same build writes 4), and no portable load in play
  exists for it to have been restored by.
