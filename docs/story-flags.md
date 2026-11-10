# Story flags: what each chapter's flags mean, and what sets them

**Status:** IN PROGRESS (2026-11-09). Every chapter row, 0..19, has a flag map in
[`story-flags/`](story-flags/), read from our source. The maps of chapters 0..15
are checked against 38 real saves, one of them made after the ending. The event flags are mapped too (section 9). One
thing is checked in play: granting a gene early (section 1).

Reference material for the save editor's story points
([`save-editor-spec.md`](save-editor-spec.md) section 12.2) and for the wiki:
flag numbers and their meanings. It holds no save and no dump of one.

## 1. Where the flags are

- **Chapter flags.** `Cond_Flags` `0x903F90`, save block `+0x5B0`: one 8-byte row
  of 64 flags per chapter, chapters 0..19. Chapter `c`'s row is at block
  `+0x5B0 + 8c`. `Chapter_FlagRow` `0x929ED0` points at the current chapter's row.
- **Numbering.** Flag `n` is bit `n & 7` of byte `n >> 3` of the row, low bit
  first (`Flags_Set`, `event_script.cpp`). MEASURED on `start_normal.DAT`.
- **Event flags** at block `+0x650` (`Cond_Flags + 0xA0`) are a separate array
  with their own setters: section 9. Some chapter flags wait on them.

### Other flag banks: finds and genes are not chapter flags

READ 2026-11-09. Opening a chest or taking a gene does not touch a chapter row.
Each has its own bank in the save block:

| Bank | Block | Set by | Flag number from |
|---|---|---|---|
| Field object contents (chests, pick-ups) | `+0x6EC`, 32 bytes | `Field_ContentTake` (`field_e1.cpp`), bank `0x9040CC` | The object's byte `+5` in the area data |
| A second find bank, with a counter at `+0x75C` | `+0x72C`, 32 bytes | `Field_PassageSteps` entry 1, bank `0x90410C` | The passage entry's byte `+4` |
| Dragon genes | `+0xC70`, 18 bits | `Gene_Bits` `0x904650`, below | The gene's number |

MEASURED: between `start.DAT` and `start_normal.DAT` the one item picked up changed
exactly one byte at `+0x6EC`, beside its inventory slot.

**Genes are set in five places.** A gene found in the field goes through one generic
handler, `FieldTail_FlagMessage` (`field_e2.cpp`, original `0x56DE50`), which sets
the gene's bit, shows message `0xFA` and plays a stream. It sets no chapter flag.
Four genes are given by story scenes instead, each beside a chapter flag:

| Gene | Scene | Chapter flag set at the same step |
|---:|---|---|
| 0 | `Scena03_Scene1` step 4, with item event `0x97` | chapter 3 flag 1 |
| 6 | `Scena03_Scene8` step `0x14` | chapter 3 flag 30 |
| 3 and 16 | `Scena08_Scene3` step `0x12`, the run that holds event battle `0x20` | chapter 8 flag 4 |

By the order in Red Herring's editor (cited, not checked by us) those are Flame,
Defender, Shadow and Fusion.

**Granting a gene early.** Set the gene's bit and leave the chapter flag alone.
Read for genes 3 and 16 ([`chapter-08.md`](story-flags/chapter-08.md)):

- No code on the path to `Scena08_Scene3` reads `Gene_Bits`. With bit 16 already
  set the scene still plays and ORs both bits in again, which changes nothing.
- The scene is not gated by chapter 8's flag 4. `Area108_TailPlace` state 9 starts
  run 3 (`area_w2f.cpp`, checked), on a path that tests the leader, the place and
  event flag 27 (section 9). No code of ours tests chapter 8's flag 4.
- **Played by the owner, 2026-11-09**, from `tools/recipe_saves/fusionEarly.DAT`
  (their save before that scene with gene bit 16 set and nothing else changed):
  the gene could be used in battle before the scene's fight, the scene that gives
  the genes played with no fault seen, gene 3 arrived as normal, and the story
  went on as normal afterwards. So setting the bit alone grants the gene, and the
  game's data scripts did not object on this path.

## 2. How a chapter starts and ends

READ and checked 2026-11-09 (`Scenario_Start` in `title_states.cpp`, `Area_Enter`
in `mode_flow.cpp`).

- **A chapter ends** when its last scene sets bit 7 of `Field_StatusBits`
  (`Field_SetStatus80`, `StatusBit80`), usually beside a final flag and an area
  change. Nothing else happens until the party next enters an area.
- **The next chapter starts on that area entry.** `Area_Enter` adds one to the
  chapter byte and calls `Scenario_Start`. Chapter 16 is skipped: 15 goes to 17.
- **`Scenario_Start(c)`** writes the chapter byte, zeroes the scenario's working
  bytes (state, run, step, timer), zeroes **the first dword of chapter `c`'s row**,
  points `Chapter_FlagRow` at the row and loads the chapter's data file
  (`0x306 + c`).
- **Only flags 0..31 are cleared at a chapter start.** Flags 32..63 are not. In a
  normal game they are still zero, since nothing has set them. A created save
  must leave them zero.

So for the save editor:

- **The start of chapter `c`:** chapter byte `c`, working bytes zero, row `c`
  empty, every earlier row as that chapter left it.
- **The end of a chapter is not "every flag set".** It is the row as the last
  scene leaves it. Each map's "End" section gives that row: the flags always set,
  the optional ones, the alternatives and the ones cleared on the way.

## 3. Kinds of flag

A flag left clear at the end of a chapter is not always a thing the player missed.
The maps give each flag one of these kinds:

| Kind | Meaning | Example |
|---|---|---|
| progress | Set on every route through the chapter | Chapter 0 flags 4..7 |
| once-only set-up | Marks a first-entry scene or set-up as done | Chapter 0 flags 0..3 |
| optional | Set only if the player does a thing | Chapter 0 flags 8, 9: two optional battles |
| choice | One of a set of alternatives | Chapter 1 flags 60, 61; chapter 2 flags 40, 41 |
| party pick | One flag of a group, chosen by the party at that time | Chapter 12 flags 26..35 and 50..59; chapter 13 flags 24, 25, 27 |
| transient | Set, then cleared before the chapter ends | Chapter 1 flags 16, 63 |
| state | Follows a thing in the world and can change back | Chapter 4 flags 8..11, the levers of area 40, reset on most entries |
| unused | No code names it, or code only tests it | Chapter 1 flags 8, 14, 17, 19, 23 |

## 4. The chapters

Each row links to the chapter's map. "End" is the scene that sets the advance bit.
The flag counts and ends are the reading passes' (one pass per chapter file,
2026-11-09). Section 5 says what was checked.

| Chapter | Flags named | End | Notes |
|---:|---:|---|---|
| [0](story-flags/chapter-00.md) | 11 | `Scena00_Run11` step 9, to area 9 | The opening. Starts from New Game |
| [1](story-flags/chapter-01.md) | 33 | Run `0xE` step 9, to area 15 | Needs flag 60, one outcome of a choice |
| [2](story-flags/chapter-02.md) | 52 | `Scena02_Scene15` step 7, flag 32 | Six flags set by area 26's code |
| [3](story-flags/chapter-03.md) | 32 | `Scena03_Scene8` step `0x14`, flag 30 | Gives genes 0 and 6 |
| [4](story-flags/chapter-04.md) | 13 | Run 2 step `0x16`, flag 12 | Flags 8..11 are lever states |
| [5](story-flags/chapter-05.md) | 53 | Entering area `0x2D` with flag 50, sets flag 62 | Event battles 18..20 set five flags |
| [6](story-flags/chapter-06.md) | 45 | Entering area `0x4C` with flag 44, sets flag 63 | Chapter 9 also writes flag 62 |
| [7](story-flags/chapter-07.md) | 15 | `Scena07_Scene6` step `0x27`, to area `0x20` | |
| [8](story-flags/chapter-08.md) | 26 | `Scena08_Scene11` step `0x15` | Gives genes 3 and 16 |
| [9](story-flags/chapter-09.md) | 56 | Area `0x52` step hook, flag 33, then run 11 | Effect code reads this row in any chapter |
| [10](story-flags/chapter-10.md) | 55 | `Run7Body` step `0xF` | Flags 32..47 are pick-ups |
| [11](story-flags/chapter-11.md) | 18 | `Scena11_Scene9` step 9, to area `0x79` | Nothing clears a flag |
| [12](story-flags/chapter-12.md) | 39 | `Scena12_Run9` step `0x2F`, flag 63 | Two party-pick groups. Flag 2 chooses a boss's enemy data |
| [13](story-flags/chapter-13.md) | 35 | `Scena13_Run8` step `0xB`, flag 30 | |
| [14](story-flags/chapter-14.md) | 35 | `Scena14_Run7` step `0x20`, flag 49 | Mostly touched from area files. Chapter 15 writes flags 1..3 |
| [15](story-flags/chapter-15.md) | 39 | `Scena15_Run6` step `0x36`, to area `0xC7` | Two outcomes: flag 18 restarts from boot, flag 17 goes to the ending |
| [16](story-flags/chapter-16.md) | 4 | Back to the title | The title demo. Never reached from 15 |
| [17](story-flags/chapter-17.md) | 0 | `Scena17_EndReturn` | The ending. Returns to chapter 15, below |
| [18](story-flags/chapter-18.md) | 0 | - | Nothing starts it |
| [19](story-flags/chapter-19.md) | 0 | - | Nothing starts it |

**After the ending** (checked in `Scena17_EndReturn`, `scena_sc15.cpp`): one answer
to the last prompt sets event flag `0x92`, clears chapter 15's flags 9..22, and
writes chapter byte 15 directly, without `Scenario_Start`. Any other answer
restarts from boot and writes nothing. `Save_BuildBlock` copies event flag `0x92`
into the slot summary, so it is the mark of a cleared game.

**MEASURED on the owner's cleared save** (save 37, `game\game\BISLPS0F.DAT`,
2026-11-09): chapter byte 15 with the working bytes zero, event flag 146 set,
chapter 15's row holding 0..6 and 32..47 with 9..22 clear, as the code clears
them. The summary byte at block `+0xCB4` is 1 in this save and 0 in every other
save of the play folder, so that byte is the summary's copy of flag 146.

## 5. What was checked

- **Against saves, mechanically.** Every flag set in any real save
  ([`observed-rows.md`](story-flags/observed-rows.md), 38 saves) has a row in its
  chapter's table, chapters 0..15, with one exception: **chapter 6's flag 6** is
  set in the owner's PC playthrough from save 31 on, and the map found no code
  that names it. Its setter is not found. Each map's "End" section compares
  its reading with the rows of saves past that chapter, and reports agreement.
- **In the code, by a second read:** `Scenario_Start` and the advance in
  `Area_Enter`; `Scena17_EndReturn`; `Area108_TailPlace` starting chapter 8's
  run 3; chapter 0's eleven flags.
- **Not checked:** everything else in the maps is one reading pass. Chapters
  16..19 have no save evidence, and none is expected: 16 is the demo, 17 is never
  saved in, 18 and 19 never start. One thing has been played: the early gene
  (section 1).
- **The saves** grew on 2026-11-09 from 21 to 38: the owner's play folder and its
  backups hold one PC playthrough, 17 saves from the opening to after the ending,
  with at least one save in chapters 0..6, 8, 9, 12, 14 and 15. The chapter maps'
  own "End" sections were written before these and compare with saves 1..22
  only.

## 6. Things that cross chapters

- **A finished row is usually frozen, with known exceptions.** MEASURED on the
  authentic psxrecomp JP saves: chapter 0's row is identical at 0:28, 5:19 and
  12:48, and chapter 1's at 5:19 and 12:48. The exceptions read from code:
  chapter 9 sets and clears chapter 6's flag 62; chapter 15 writes chapter 14's
  flags 1..3; the ending clears chapter 15's flags 9..22.
- **Fixed-row code runs in any chapter.** The effect files read chapter 9's row at
  its fixed address, and battle code tests chapter 1's flag 22.
- **Chapter flags can wait on event flags.** Chapter 14's flags 25 and 26 wait on
  event flag 122 ([`chapter-14.md`](story-flags/chapter-14.md)).
- **`Area21_ChoiceTrade` sets flag 18 of whichever row is current.** It is in the
  saves' chapter 8 row.

## 7. Open

- **Scripts in the game data.** Event conditions can test a chapter flag or a gene
  bit, and some steps have no starter in our code: chapter 10's run 7, chapter 14's
  `Scena14_Run1` step `0x19` (flags 2 and 3), the second branch of
  `Scena15_Run5`. A data read or a run would settle each.
- **Two possible defects, Capcom's if real, seen in the read only:**
  `Scena07_Scene7` records its rewards in chapter 7's row while `ObjectReward`
  tests chapter 6's, so a reward could be given twice; and nothing in the code
  stops chapter 8's run 3 from starting again. Neither is in `known-defects.md`
  until one is seen in play.
- **The event flags' missing setters** (section 9) and the table-driven sites.
- **Chapter 0:** the area files are not searched for tests of its row.

## 8. The extractor

`tools/story_flags.py` lists flag sites: `census`, `summary`, `chapter N`,
`file NAME.cpp`. It is a first pass, and the maps are the record. Known gaps,
found by the reading passes: calls through the `g.flags_*` table (chapter 16),
one-line object handlers, wrappers that fix the flag number (`SetAndEnd0`,
`CellRun8`), `mode_states.cpp`'s chapter 1 functions, `area_w4e.cpp`'s `Row14()`,
and a few sites given to the wrong function.

## 9. The event flags

The bank at `0x904030` (`Cond_Flags + 0xA0`), save block `+0x650`. Mapped
2026-11-09 by nine reading passes, one per group of source files, in
[`story-flags/event/`](story-flags/event/). A script merged their tables into
[`story-flags/event-flags.md`](story-flags/event-flags.md): **454 sites naming 132
flags**, by flag number, each with the saves it is set in. 53 more sites take
their flag number from the game data.

### Size

`Flags_Set`, `Flags_Clear` and `Flags_Test` mask the flag number to 8 bits, so
they reach flags 0..255, block `+0x650..+0x66F`. Flags 245..255 are written by
loops in the engine and overlap the bytes at `+0x66E` that Red Herring's editor
calls the faerie village stage. The eight bytes after flag 255, up to the zenny
at `+0x678`, are not reached through this bank.

### How they start (checked)

- `TitleFlow_NewGame` does not clear the event bank. It clears block `+0x12`
  (`0x9039F2`).
- On the first field frame `Field_ModeTail` runs its state 0, `Scenario_SubInit`
  (`title_states.cpp`), because that byte is 0. It copies a 16-byte table,
  `Scenario_SubInitTable` `0x662CD8`, over flags 0..127 and sets the byte to 1.
- MEASURED: the event bytes 0..15 of `start.DAT` equal that table in the owner's
  `BOF3.exe`, bytes 16..39 are zero, and block `+0x12` is 1 in every PC save held.
  Flags 3, 25 and 27 are the table's bits, and they are set in every real save.
- **Flags 128..255 are never cleared at a new game**, by the code read. After a
  load they hold what the save holds.

For a created save: seed flags 0..127 from the table, read from the player's
install, leave 128..255 zero, and **write block `+0x12` as 1**. With 0 there the
game would seed flags 0..127 again on the first field frame and lose the save's.

### Scripts in the game data can only test them

READ by the engine pass ([`event/engine.md`](story-flags/event/engine.md)), the two
script conditions checked by a second read. No event-script op and no
movement-script command sets or clears an event flag: every set and clear names
its flag in our code. Data reaches the bank by these paths, all tests:

| Path | Function | Flag number from |
|---|---|---|
| Event-script condition 12 | `EventCond_StoryFlag` (`field_o.cpp`) | The operand byte |
| Event-script condition 2, after op `F9 14` | `EventCond_Flag` (`event_script.cpp`): the row is the current chapter's by default, any row after `F9` | The operand byte |
| Map-data condition codes, kinds `0x14` and `0x34` | `Area_TestCondition` (`map_cells.cpp`), from about 13 cell drawers | The code's low byte |
| Music zones | `Area_ZoneTrack` (`window_task.cpp`) | A table entry |
| Movement-script variable 12 | `MoveScript_Variable` (`move_cmds.cpp`) | Flags 0..31 as one dword |

`Field_ObjectTriggerByKind` (`field_e2.cpp`) passes the bank to a handler chosen
by an object's byte; the handlers are code and are in the area passes.

So the missing setters below are not data scripts. They are code the passes did
not resolve: table-driven sites, or helpers that take the number as an argument.

### Kinds

Most event flags are world state, not story progress:

| Kind | Examples |
|---|---|
| Story progress across chapters | 67 (set at the start of chapters 8 and 13 and in chapter 10); 143 then 144 (a once-only sequence over areas 168 and 167); 146 (the cleared game) |
| A choice that picks a destination | 147 and 148: the choice of area 100, read to pick one of four areas |
| A set to complete | 109..112, one object trigger each in four areas; 113 when all four are set |
| Switches, gates, levers | 79, 81..83 (area 106); 69 (the gate cells of area 100); 122..124 (area 141) |
| Once-only scenes and gifts | 91, 107, 104, 121, 127, 149; 108 (one master's grant) |
| Per-visit state | 116 (healed in areas 169 and 171, cleared on entry); 88 (areas 173 and 186) |
| Puzzle and minigame work flags | 11, 17..23, 36..39, 65: set in no real save |
| Signals between code | 28, a 15-frame latch that stops switches in four areas firing twice; flags that effects set or clear as they act |

### Checked against the saves

By the merge script, over the 38 real saves:

- **Set in every save:** 3, 25, 27, the seed.
- **Named in code and set in no save:** 50 flags, most of them puzzle, signal and
  per-visit flags, as their kinds predict.
- **Set in a save, with sites but no setter found:** 5, 6, 24, 26, 56..63, 92, 93.
- **Set in a save, with no site at all:** 14, 15, 71, 96..101, 131..135. (246..255
  are the engine's loops.)
- **Flag 146** is set in the cleared save and in no other.

The last two lists were the gap, and the tables close it. Run on the owner's
`BOF3.exe`, `tools/flag_tables.py` finds every flag of both lists among the
values of the switch, cell and trigger tables, so their setters are the
table-driven sites (2026-11-09). Chapter 6's flag 6 (section 5) is still without
a setter.

### The tables in the exe

Some sites take their flag number from a table in the executable. **This repo
does not quote those values** (the owner, 2026-11-09): it reads them from the
player's exe when they are wanted.

- [`../recipes/flag-tables.tsv`](../recipes/flag-tables.tsv): the layout of each
  table, 33 tables, with no values. Compiled from the source by one reading pass;
  [`story-flags/flag-tables.md`](story-flags/flag-tables.md) gives the evidence
  for each layout.
- `python tools/flag_tables.py --exe <BOF3.exe>` prints the flag numbers. Checked
  on the owner's exe: the new-game seed reads back as flags 3, 25 and 27, as the
  saves show, and no table gives a number outside its bank.
- One table is not read: `Area_MusicSets` holds pointers, which the recipe cannot
  follow.
- The tool can write the values as a page for use outside this repo, and refuses
  a path inside it.

### Other things the passes found

- **Chapter flags and event flags depend on each other both ways.** Chapter 14's
  flags 25 and 26 wait on event flag 122. Event flag 51, set by the choice of
  area 112, is kept only if chapter 13's flag 16 is set.
- **A fourth operation, toggle** (`Flags_Toggle`): eight sites.
- **More small banks:** `0x904654`, block `+0xC74`, written by
  `Area98_ChoiceFlag9`; and bits of `0x904061`, block `+0x681`, where three
  masters keep their one-time grant. The save editor spec lists those two places
  under Red Herring's names (masters, unlocked formations), so those names need a
  second look.
- **`Cond_ByteFD`**, the byte many area set-ups switch on, is still unnamed. Event
  condition 8 compares it with an operand.
- **The path into chapter 8's run 3**, read more closely by the area pass
  ([`event/areas-w2f-w3a.md`](story-flags/event/areas-w2f-w3a.md)): the leader's id
  must be 2 and the place 4; event flag 27 picks script 9 over script 8; state 5
  sets flag 29, which nothing tests. This corrects
  [`chapter-08.md`](story-flags/chapter-08.md), which called flags `0x1B` and
  `0x1D` gates.

### Corrections to the chapter maps, not yet applied

From [`event/chapters.md`](story-flags/event/chapters.md): chapter 6 leaves out the
branch of `Cell3` on chapter flag `0x2C`; chapter 9 misses the chapter 6 flag 62
set at the same step; chapter 10 misses that the arrive hook starts run 12, which
clears event flag 79; chapter 17 cites line 2682 for flag 146, and the set is on
2684. Chapter 0 uses no event flag.
