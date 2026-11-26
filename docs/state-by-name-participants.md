# State by name, group B1: the quicksave's six participants

**Status:** STABLE (2026-11-25, verified the same day on the Linux x86-64
ELF of `sbn/B1`; sections 6 and 10.4 have the measurements). The state-by-name round's group B1
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

- **Closed by B1b (section 10):** the item below was measured and its
  classes named.
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

## 10. B1b: words of the span that name our module (2026-11-25)

Section 8's first item, measured and closed by what the measurement
supports. Built: `src/hook/snapshot.cpp` (`NameWord`, the WORDS list's
placing in `Parse`, the not-portable codes, a probe) and
`src/hook/span_words.h` (`kTextWords`). The captures are outside the tree, in
`~/bof3x-runs/sbn1125/B1/root/` on the laptop: `qw/` and `qd/` the
measurement, `qp/` the forced portable loads, `sh/b1b/` the state hashes.

### 10.1 The question, and why a range test cannot answer it

A take writes every word of `.data` / `.bss` raw unless a rule names it
(A1: a code place, a fold of the moved image, one of the 17 run-time message
slots, a text slot of Capcom's). A word that holds an address in our module
outside those rules is the hole: a portable load writes the take's address,
which names another build's object.

**The probe** (`BOF3X_QSWORDS=<file>`, off by default: each pass of
`Snapshot_Pending`, the words a take would write raw whose value lies in one
of our module's sections, written as changes) answers the naive question.
On `bridgeWalk` (en-US, `--direct`): **28,797 distinct words, on every one of
2,099 passes**; 26,904 of them fall on a symbol of ours. In this ELF our module
is `0x10000000..0x1266xxxx`, 38 MB, most of it `.bss` (the harnesses' and the
log's buffers), so a value in it is mostly game data: script words, pixel
pairs, coordinates (`0x10FFFF08`, `0x12000101`). This is the state hash's
finding (`state-hash.md` 7, "why not every word that falls in our image":
3,635 to 6,187 a tick on Windows) at the take's scale. A "not portable" on
such a word would mark every take not portable.

**The build differential** separates the two. Each route ran as a state-hash
run (orig6's conditions, en-US) on this build and on the round base
(`round/wt`, `1832d217`), with `.data` dumped raw at the same ticks
(`qd/run.sh`, `qd/diff.py`). Our statics moved by about `0x8000` between the two
links (`g_warp_message` `0x115C78D0` against `0x115BF8C0`, the message pool
`0x114F2560` against `0x114EA530`), and the game's state is the same: no word
outside the skip list differs whose values are not ours, on any route. So a
word that differs and names our module is a pointer of ours, and a word that
is equal is a number:

| Route | Ticks | Numbers in our range, a tick | Pointers of ours a take named | Written raw (the probe) |
|---|--:|--:|--:|--:|
| `bridgeWalk` | 8 | 3,620..12,434 | 109 | 539 |
| `combat` | 9 | 3,620..12,362 | 114 | 50 |
| `shop` | 10 | 3,620..12,575 | 110 | 1,007 |
| `cutsceneAndNue` | 14 | 9,684..16,567 | 112 | 1,895 |
| `menu_screens` | 7 | (identical hashes) | - | - |
| `fairyVillage` | 11 | 3,620..11,845 | 109 | 255 |
| `masterAndManillo` | 12 | 3,620..15,790 | 109 | 44 |

### 10.2 What the raw pointers are

| Class | Words | Where seen | Its rule |
|---|---|---|---|
| a participant's own | `Sound_Banks`' voice buffers, `Sound_Channels`, `Snd_Device`, `Snd_Primary`, `Music_Buffer`, `Music_Notify`, `Music_Events`, `SndStream_Buffer` (our audio layer's objects), and `Music_Staging` (sample bytes, in range on one tick: a number); `PsxRest_Direct3D`, `D3d_Viewport`, `D3d_BackMaterial` (our renderer's) | every route; all in the state hash's skip list | **rebuilt or kept by the participant** after the span: `SOUND`'s `SnapRestore` writes the device words, clears the stream words and remakes every buffer; `GFX` keeps `0x7CC348` and `0x7CC354` live (section 2). Already right |
| message box text into the pool | `Msgbox_Base`, `MenuWindows_ListText`, `Msgbox_Resume` (the box state's `+0xC`, `+0x10`, `+0x14`) | `shop`: 3, 3 and 7 of 10 ticks; a named word in the pool on 2,593 of 3,157 passes | **re-aimed by name**: written as its offset in DIV-0007's moved message pool (`kWordPool`, the 17 run-time slots' rule), placed at this process's pool |
| the battle block's text word | `0x93B8FC` (`BattleFlow_TaskCurrent + 0x38`, unnamed in symbols.toml) | `cutsceneAndNue`: pool text on 2 of 14 ticks; at other passes `g_messages+0x5B` / `+0x82`, the banner slots' text | the same, and the next class's |
| a copy of a text slot's pointer, or a cursor into its text | six words of `Effect_Objects` (`0x7E15B0`, `B4`, `1630`, `34`, `16B0`, `B4`) | `masterAndManillo`: 7 of 12 ticks; about 4,600 passes; `g_lines+0x140` (slot `0x653BC0`'s value) and `g_lines+0x1`, `+0x2`, ... `+0xC1` (DIV-0064's lines, drawn letter by letter) | **re-aimed by its slot**: written as the slot and how far into the slot's text (`kWordTextCopy`, new), placed from the slot as a text slot is placed (the process's own text at a portable load), the offset checked against this process's text |
| the town warp's page | `Msgbox_Base`, `MenuWindows_ListText` on `g_warp_message` (`field_7.cpp:548-549`) | no route: DIV-0225 wants the Definitive Edition mod's data | **not portable** (`kNpTextOurs`, new): a text word that names our module outside the pool and the slots' texts. The same answer for the reading row's composed copy (`MsgBox_ReadingRowSystem`), which no route reached either |

The state hash's own cross-build differences on these routes are exactly
the second, third and fourth classes: `shop` page `0x7DE000` from tick 565
(until now read as the sound page's noise), `cutsceneAndNue` page `0x93B000`
from 5,506, `masterAndManillo` page `0x7E1000` from 715. The hash reads the
words raw, as before, so no hash moves; `kTextWords` and the copy rule are
its next candidates (its section 7's "the place to look when a route
reports another page of this class").

### 10.3 The false positives, measured

- **The copy rule** names a word only when its value is a text slot's
  pointer or lies inside that slot's text up to its NUL (`TextExtent`, at
  most 255 bytes). In the 71 dump pairs, **no number** (a word equal in both
  builds) equals one of the 83 text starts or lies within 64 bytes past one;
  within a blind 256-byte window, 7 to 11 do on four routes, which the NUL
  bound excludes for these short strings. A false positive would rewrite a
  number at a portable load; none was met.
- **A value test over the message pool** (any word holding a pool address
  marks the take not portable) was measured and **not taken**: one word that
  holds the same value in both builds, `0x8160C8` (past `Gfx_ClutStrip`), lies in
  this build's pool range on **2,432 of 7,420** `cutsceneAndNue` passes, and
  `0x7A2370` (inside `Gfx_VramShadow`) on 73 `masterAndManillo` passes. As a
  refusal it would mark a third of that route's takes not portable for a
  number; as a re-aim it would corrupt a pixel. Such a word is written raw
  and **logged at the take** (`take: N word(s) no rule names hold an address
  in the message pool`), so it is seen, not hidden.
- **What stays unmeasured**: a pointer of ours that no route stores, outside
  the named words, the slots' texts and the participants' words. It cannot
  be told from a number at a take; the probe and the differential are the way
  to find one.

### 10.4 Proved (the build of `e15d1f3a`)

- **The self-test** (`BOF3X_SELFTEST_ONLY=1`, under the original text and
  under `en-US`): exit 0, 10,424 and 10,481 log lines, no FAIL. New cases:
  the box's text word and the battle block's into the pool written as their
  offsets; a word holding a text slot's pointer, and a cursor 5 bytes in and
  at the NUL, written as the slot and how far in, one past the NUL not; the
  box's word on a buffer of ours (DIV-0225's page) makes a take not
  portable (`kNpTextOurs`), another build refusing it "another build (not
  portable)" with the save offered and its own build restoring it exactly; a
  word no rule names holding a pool address written raw, not refused; and end
  to end, a take with a cursor 3 bytes into the slot's text restored
  portably (the cursor in this process's text) and exactly (in the take's).
- **What a take writes now** (`quicksave_check.py make` takes, read back
  from the files): `shop` at 2,500 (`qp/shopbox`) lists the box's three
  words as pool offsets (`0xEA2`, `0xEBA`, `0x29A`); `masterAndManillo` at
  1,600 (`qp/masterbox`) lists the six effect words as cursors (slot
  `0x653BC0` +13 and +12, `0x653BB8` +16 and +28, `0x653BB0` +0 and +11); both
  takes portable.
- **Forced portable loads** (`BOF3X_QSPORTABLE=1`, A1's cross form with
  `--run-game none`, `quicksave_check.py cross`, orig6's conditions): `combat`
  (A1's take at 900) **equal, 1,680 ticks** from the restore; `masterbox`
  **equal, 3,904 ticks**; `shopbox` (on `d8c9917f`, before the cursor rule)
  **equal, 640 ticks**. Each loaded run logged `restored PORTABLE`. These load
  in the same build, so a pool or a slot's text sits where it was: they show
  the names placed back without harm; the re-aim to another place is the
  self-test's.
- **No state hash moved**: `bridgeWalk` and `combat`, this build against
  `round/wt` (`1832d217`), `statehash.py diff`: **identical on all 2,074 and
  2,602 ticks** (`sh/b1b/`; and the same on `d8c9917f`, `sh/b1b_v1/`).
- **The re-measure**: the differential again on this build (`qd/run_b1.sh`,
  the base's dumps reused): the box's words and `0x93B8FC` left `shop`'s and
  `cutsceneAndNue`'s raw lists (on `d8c9917f`); on `masterAndManillo` (on `e15d1f3a`) the six effect words joined the named (115 named, against 109) and none is raw. What a take still writes raw that names our module, on the three routes, is the participants' own words alone.
- **The no-data checks** pass; `width_census`, `data_census` and
  `reloc_census` rose by this change's sites (span VAs spelled as the
  run-time slots are, the self-test's narrowings, `TextExtent`'s bound) and
  are regenerated.

### 10.5 Ledger

No draft F. A player meets no new reason: a take with the box on text of
ours outside the pool is refused in another build as "another build (not
portable)", and a cursor this build's text cannot hold as "another build (a
word this build cannot place)", both A1's reasons, with the save offered
(DIV-0145 as amended). What changes is which quicksaves load portably, and
that a portable load writes these words by their names.

### 10.6 Not checked

- A load in another build: every portable load here is forced in the build
  that wrote it, so the re-aim to another pool or another text is the
  self-test's only.
- The town warp and the reading row's copy in a live game: no recipe reaches
  them (the first wants the Definitive Edition mod's data).
- Routes other than the seven, the player's defaults (the differential ran
  under orig6's conditions), the original Chinese text (no moved pool, the
  slots hold BOF3.exe's text), and the 32-bit and Windows builds.
- A pointer of ours stored where no rule looks (10.3's last point).

### 10.7 B1c: corrected - a word is named by where it is (2026-11-25)

**What was wrong.** B1b's copy rule (10.2's fourth row, as built in
`d8c9917f` / `e15d1f3a`) tried every word of `.data` that is not a slot:
a value at a text slot's pointer, or inside that slot's text up to its NUL,
was written as a cursor into the text (`kWordTextCopy`). The proof found the
fault across builds ([`state-by-name-proof.md`](state-by-name-proof.md) 6,
finding 2): in B3's `-O1` build U a word of area data, `0x8262EC`, holding
the number `0x1149015B` in every run of both builds, lay inside U's `g_lines`.
So U's take named it, and the tip's load wrote the tip's `g_lines+0xCB`,
`0x114FF14B`, into the game's data: a wrong restore. 10.3's measurement of
the rule's false positives compared two builds whose statics were `0x8000`
apart. A number clear of the text in that pair fell inside it in U's layout,
and some layout always will: a test of what a value looks like, over the
whole span, meets some number in some build.

**The principle**: a word is renamed only by where it is, never by what its
value looks like. A value test on its own may only make a take less trusted
(a not-portable code, a line in the log), never rewrite a word.

**The rules as they stand** (`snapshot.cpp` `NameWord`, `Parse`):

| Rule | Owner | Named by | The principle |
|---|---|---|---|
| Capcom's text slots (`kWordText`) | A1 | place: the slot list, read before the injects (`span_words::g_slots`) | holds |
| the 17 run-time message slots into the pool (`kWordPool`) | A1 | place: the named list | holds |
| `kTextWords` into the pool (`kWordPool`) | B1b | place: four named words | holds (question (b) below) |
| `kTextWords` into a slot's text (`kWordTextCopy`) | B1b, B1c | place: four named words, a pointer by their type; matched against every slot's text | holds |
| a kind-0xF effect object's `+0x50` / `+0x54` into a fishing line (`kWordTextCopy`) | B1c | place: `Effect_Objects` `0x7E11E0`, 20 x `0x80`, the record of kind 0xF (`+5`) as the take's bytes hold it, live or ended; matched against the thirteen line records of `0x653B98` only | holds |
| any other word whose value lies in a slot's text | B1c | - | **written raw** (it was B1b's rule; now nothing renames it) |
| a text word or a cursor field naming our module elsewhere (`kNpTextOurs`); a run-time slot doing so (`kNpRuntimeSlot`); a slot holding our code (`kNpSlotCode`) | A1, B1b, B1c | a value test, making the take not portable | holds: less trusted only |
| a word no rule names holding a pool address | B1b | a value test, logged at the take | holds: logged only |
| a code place (`kWordCode`) | A1 | **value**: `Bind_VaOf(w)`, any word equal to a bound place | **does not hold in principle**: a number equal to `(top << 24) \| VA` of a bound function is written as the VA and placed with this process's top byte. That is the identity while the two processes' top bytes agree, which every measured run shows (A1: a differing top byte "has not been seen"). For A1's file: name a place by where it is (the census of load-time code words), or make a process with another top byte refuse a portable take |
| an image pointer or link (`kWordFold`, `kWordFold24`) | A1 | **value**: the fold | does not hold in principle, but harmless as built: the take writes them only when its image is moved, and a load at another base is refused (A1's "another span"), so the placing is the identity |

`Parse` also checks the place on the way in. A file whose WORDS list names a
copy at any word that is neither a text word nor a kind-0xF record's
cursor field (by the record's bytes in the file), into a fishing line, is
refused as "another build (a word this build cannot place)", with its save
offered. (The file's record bytes give the kind: a kind-0xF record.) Every file a B1b build wrote with a misnamed number is refused that
way rather than having its word rewritten.

**Question (a), which fields of an effect record.** Only `+0x50` and `+0x54`,
and only in a record of kind 0xF. `effect_kind_0f.cpp`:
- `+0x50` is the typing cursor, set to the text of line record `+0x4B` and
  stepped one character a frame (`:1615`, `:1634`, `:1672`);
- `+0x54` is the scrolling cursor, copied from `+0x50` (`:1770`).

Measured on `masterAndManillo`'s dumps (ticks 1,200 to 2,000), the kinds
0x2, 0x3, 0x5, 0xA, 0xC, 0xD and 0x1A hold span addresses in the same
fields (`0x8B....`), or zero, and are left alone. Inside a kind-0xF record,
`+0x54` need not lie in `+0x4B`'s line: record 7 at tick 1,200 had `+0x4B`
5 and `+0x54` at `g_lines+0xCB`, inside line 3, the previous text. So the
cursor is matched against all thirteen lines, not against `+0x4B`'s alone.
A number in another field of a record is never tried.

**Live or ended.** The first cut (`f8bb996f`) asked for a live record (`+0`
not 0). The probe re-run on `masterAndManillo` then showed the six cursor
fields written raw on 247 to 3,599 passes: records 8 and 9 from tick 1,700
have `+0` clear (their effect ended) and keep kind 0xF and their cursors.
The kind byte is its last owner's until `Effect_Spawn` reuses the record
and writes `+5`, so the fields are still that owner's cursors, and
`fab53dad` names them whether the effect is live or ended. A record reused
by another kind is that kind's. Record 7 from tick 4,000 is kind 0x16
(`EffectKind16_WorldMap`, a record handler that never reads `+0x50` /
`+0x54`) and holds a stale `g_lines+4` from its kind-0xF life: it is written
raw, and nothing reads it before the next owner writes it.

**Question (b), the pool rule.** `kWordPool` is tried only at the 17
run-time slots and the four `kTextWords` (the `runtime` test in `NameWord`).
It was bound to named words from the start, so it does not share the fault.
The value test over the pool is the log line, which changes no word.

**Question (c), the principle**: stated above; the rules that hold it and
the two of A1's that do not, in the table.

**What this gives up.** A copy of a text pointer stored somewhere other than
these places is written raw. On the seven routes no such word was measured
(10.8), and the town warp's and the reading row's case is a not-portable
take, as before.

### 10.8 B1c: proved

The build of `fab53dad` (N, `3532ce44...`) and the same source at `-O1 -g
-DNDEBUG` in `build-u` (L, `65e438ff...`, configured as B3 configured U),
each run from a copy with its libraries. Between them `g_lines` lies at
`0x11500080` (N) and `0x11491090` (L). Captures in
`~/bof3x-runs/sbn1125/B1/root/c/` (the matrix rows `m/`, `chain.log`,
`chain2.log`), `qd/` (the probe) and `sh/b1c/`. B3's tool, takes and
conditions: `quicksave_check.py matrix` under the proof's skip rows.

| Take | L takes, N loads | N takes, L loads |
|---|---|---|
| `f297`, `f605`, `f4640` (`fairyVillage`) | portable, **equal** 472, 484, 59 ticks | portable, **equal** 472, 484, 59 |
| `s2500` (`shop`, the pool words) | portable, **equal** 497 | portable, differs from k=348 (`Frame_Counter` 2848): the shop's jingle, B3's finding 5, as its U and T loads did |
| `mm1600` (`masterAndManillo`) | portable; one page, `0x7E1000`, every tick | the same |
| `cf1500` (`caughFish`) | portable; one page, `0x7E1000`, every tick | the same |
| `mm3320` (a record reused by another kind, below) | portable, **equal** 479 | portable, **equal** 479 |

- **`0x8262EC` unchanged by the load.** `f297` by L: the file holds
  `0x1149015B`, and raw dumps at ticks 1, 100 and 400 of L's forced load and
  of N's load hold `0x1149015B` in all six.
- **The cursors re-aimed, by dump.** `mm1600` and `cf1500`, both directions,
  the taker's forced load against the other build's load at ticks 1, 100 and
  400: the four to six cursor fields that differ are each at the **same
  offset into its own build's `g_lines`** (`mm1600` tick 1: record 7
  `+0x14D` / `+0x14C`, record 8 `+0x10F` / `+0x11C`, record 9 `+0xC0` /
  `+0xCB`), and every other differing word outside the skip rows (103 a
  tick) is a slot holding each build's own text. The one page the hash
  reports is the hash reading those cursors raw (finding 3 of the proof).
- **A B1b file is refused, not renamed.** `f297` taken by B3's U (the B1b
  rule), loaded by N: `REFUSED another build (a word this build cannot
  place) ... the file names the word at 0x008262EC as a copy of a slot's
  text, and that word holds none (the save half is whole ...)`.
- **The self-test**, original text and `en-US`: exit 0, 10,424 and 10,481
  lines, no FAIL.
- **No state hash moved**: `bridgeWalk` and `combat` against T (`round/wt`,
  `2bf9bde3...`): identical on all 2,074 and 2,602 ticks.
- **The no-data checks** pass; the width and reloc censuses regenerated.

**The probe again** (`BOF3X_QSWORDS`, the seven routes, against B1b's
`e15d1f3a`; the differential's dumps class each word): what is written raw
now and was named before, outside the skip list -

| Word | Routes, passes | What it is |
|---|---|---|
| `0x760E60`, `0x77AE1C`, `0x797E1C` (the VRAM shadow), `0x82D1DC`, `0x851E94`, `0x867D38` (past `Gfx_ClutStrip`) | every route; 108 to 7,328 passes | **numbers**: equal in both builds at every dumped tick. B1b's rule was renaming them (each lay in some slot's text in that layout): finding 2's fault, six more times, now gone |
| `0x7E15B0` (record 7's `+0x50`) | `masterAndManillo`, 2,213 passes from 3,307 | **a stale pointer**: `g_lines+4` left from the record's kind-0xF life, the record now kind 0x16. Written raw, by the rule |
| `0x7E1630`, `34`, `16B0`, `B4` | `masterAndManillo`, the one pass 3,307 | the same, on the frame the fishing spot is left: the kind bytes already another's, the cursors not yet cleared |

The stale pointer is the one pointer of ours now left raw. `mm3320` is a
take inside its window (`Frame_Counter` 3,320): it loads portably in the
other build and runs **equal for 479 ticks both ways**, the word holding
the taker's address in every dump of both loads: nothing reads it. So it is
left as a number, and the take stays portable.

### 10.9 B1c: not checked

- That no code ever reads a kind-0x16 (or other) record's stale `+0x50`:
  shown by `mm3320`'s 479 ticks and by the record living and dying as kind
  0x16 with the word unchanged, not by reading every effect kind.
- The stale cursor is not logged at the take.
- A1's two value-named rules (the code place, the fold) are described in
  10.7's table, not changed.
- A copy of a text pointer stored outside the named places, on a route not
  measured; the 32-bit and Windows builds.
