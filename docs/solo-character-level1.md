# The solo-character round: M10, taking a champion to level 1

**Status:** DONE (2026-11-12). Read-only: the source at `82a26a9d`,
`symbols.toml`, the docs named below, and `bof3/BOF3.exe` read with short
capstone scripts in `D:\bof3x-runs\solo1112\M10\scratch` (`lv.py`, `lv2.py`,
`sweep.py`, `sweep2.py`; `rd.py` and `fn.py` copied from group M's scratch).
Nothing under `src/` or `tools/` changed; the game was not run.

The question is [`solo-character-round.md`](solo-character-round.md) 6a item 4:
the owner wants the champion to start at level 1, and records 1, 2, 4 and 5
start higher (5, 13, 5, 10). Each section gives the answer first, then the
evidence. An inference is marked as one.

**Method for the binary scans.** `python sweep.py` disassembles `.text` with
capstone (skipdata on) and prints every instruction whose first operand is a
memory write with an absolute displacement inside `CharacterRecords`
(`0x903A70`..`0x903F8F`) at a record offset of `+0xA`, `+0xC..+0xF`,
`+0x40..+0x4B`, `+0x4E` or the ability lists, mapped to its function by
`fn.py` (the greatest `symbols.toml` `[[func]]` pc at or below the site).
`python sweep2.py` then looks, in the 208 functions that hold any such
address, for writes through a register at displacement `+0xA` (byte), `+0xC`
(dword) or `+0x40..+0x4A` (word). Neither sees a write through a pointer that
a helper receives as an argument (`Stat_AddCap999(ptr)` and the like); for
those this note relies on the READ pass of
[`save-block/range-b-characters.md`](save-block/range-b-characters.md) 1.

## 1. What `Char_LevelUp` does per level

**Answer.** `Char_LevelUp` (`0x498DE0`, `src/game/field/char.cpp:53-95`) is a
pure function of the record and one table. Its inputs are:

- the record index (the argument's low byte, unchecked);
- the record's EXP `+0xC` (signed) and level `+0xA`;
- the record's six signed growth-bias bytes `+0x89..+0x8E`;
- `Char_ExpTable` (`0x658F48`): 99 rows of 8 bytes per record index, indexed
  by **record index x 99 + level**.

Row L describes the step from level L to L + 1:

- `+0..+1` (u16): the EXP for that step. The new level is the first L whose
  sum of these words over rows 1..L is above the EXP; 99 at most.
- `+2`, `+3`: the gain to base max HP `+0x40` and base max AP `+0x42`.
- `+4` high and low nibble: base power `+0x44` and defence `+0x46`.
- `+5` high and low nibble: base agility `+0x48` and intelligence `+0x4A`.
- `+6`, `+7`: two ability ids, each given to `AbilityList_Add` (0 is skipped).

For each level gained it adds `max(0, gain + bias)` to each stat word, capped
at 999, and files the two abilities. Then it stores the level and calls
`Char_RecalcStats`, which rebuilds the effective block `+0x20..+0x3F` from the
base block, the equipment `+0x12..+0x17`, the master `+0x1F` and the fatigue
`+0x1E` ([`save-block/range-b-characters.md`](save-block/range-b-characters.md)
1.1). Row 0 is never applied (the loop starts at the old level, which is at
least 1).

What it does **not** change: willpower (`+0x4E`, `+0x1C`, `+0x2E`), current HP
and AP, the resistances and percentages, equipment. It has no randomness.

**Outside influence**: only the master, and only through the record. A master
(`MasterJoin_Step5Apply`) copies its six bias bytes into `+0x89..+0x8E` and the
level into `+0x88`; quitting writes them to 0 (range-b 1, rows `+0x88` and
`+0x89..+0x8E`). Equipment does not enter the growth; it enters only the
recompute. The table itself can differ by install: a PSP-only install lays
the PSP's rows for record index 6 (DIV-0104), and a mod can lay its own
`exp_table` (the image layer, [`image-layer-round.md`](image-layer-round.md)
table row `exp_table`; DIV-0224's mod).

**Evidence.**

- `symbols.toml` `Char_LevelUp` (disasm 2026-09-28, group SX) and
  `Char_ExpTable` (`0x318` bytes a member = 99 x 8), and the body at
  `char.cpp:63-95`.
- Re-checked against the exe 2026-11-12: a capstone listing of
  `0x498DE0..0x498FD2` shows the only calls are `0x590C90`
  (`AbilityList_Add`, twice) and `0x590660` (`Char_RecalcStats`); the reads
  are `[esi + 0x903A7C]` (EXP), `[ecx*8 + 0x658F48]` (the EXP word),
  `[esi + 0x903A7A]` (level), `[esi + 0x903AF9..0x903AFE]` (the bias, `movsx`);
  the writes are the six words `0x903AB0..0x903ABA` and the level byte.
  No `Rand` call.
- A whole-`.text` scan for `call` / `jmp` rel32 to `0x498DE0` finds two
  callers: `BattleResult_Setup` (`0x431DE9`) and `Scena08_SetUpRecord4`
  (`0x553641`). No pointer to it is stored anywhere in the file (a byte
  search for the address finds none).
- `AbilityList_Add` (`0x590C90`, `src/game/menu/char.cpp:421-445`) puts the
  id in the **first 0 slot** of the type's ten-slot list. It does not test
  for an id already held. This matters for section 5.

## 2. Can a record be taken back to level 1?

**Answer: yes, from the exe's own data, for a record still at its New Game
state.** With no master the bias is 0 and nothing is near the 999 cap, so
each level adds exactly the row's gains. Taking them off is an exact inverse:

> level-1 base = default base - the sum of the gains of rows 1..N-1,

where N is the default level. Levelling the result from 1 back to N with
`Char_LevelUp` gives the default record's base block again, word for word
(the addition is exact when no clamp or cap acts; inference from the code in
section 1, not run).

The check (`python lv.py`, 2026-11-12), all four records:

| Record | Default level | Derived level-1 HP, AP, power, defence, agility, intelligence |
|---|---|---|
| 1 | 5 | 17, 18, 9, 8, 11, 16 |
| 2 | 13 | 30, 0, 20, 16, 11, 5 |
| 4 | 5 | 26, 6, 15, 11, 16, 14 |
| 5 | 10 | 22, 15, 12, 10, 9, 20 |

All values are non-negative and of the same size as the three records that
start at level 1 (records 0, 3, 6: HP 20 to 40, power 12 to 18, by the same
read). Three more facts say the default records were built by the same table
from a level-1 base, and are not independent numbers:

- **The EXP matches the level.** Each of the seven default records' EXP lies
  in its level's window (the sum of rows 1..N-1 at or below it, the sum of
  rows 1..N above it). Records 0, 3, 6 hold 0.
- **The skills match.** Every non-zero ability id in rows 1..N-1 is in the
  default record's lists: two for record 1, one for record 2, five for record
  5; record 4's rows 1..4 have none.
- **The masters are off.** All seven default records have master `0xFF`,
  `+0x88` 0 and the six bias bytes 0, so the growth they would have had is the
  bare table's.

One value sits on the boundary: record 2's AP derives to exactly 0 (its rows
1..12 add all of its default AP). That is consistent but not proved to be
Capcom's intent; nothing in the code reads it. Open; nothing in the exe can
settle it further.

**Not derivable for a record that has moved on.** Once a record has had a
master, a stat item, or a level gained under a bias, its base block is no
longer "defaults plus the table". The record keeps only the current master
and `+0x88`, not the history. So the subtraction is valid only from the
default record, not from the live one.

## 3. What else goes with the level

**Answer.**

- **EXP** `+0xC` (s32) is the stored total. The level is a function of it
  (section 1). **EXP to next is not stored**: `Char_ExpForLevel` (`0x574A60`,
  `menu_windows.cpp`; `symbols.toml` `Char_ExpTable`) and `Char_LevelUpGain`
  (`0x432170`) compute it from the same rows. So a level-1 record must hold an
  EXP in level 1's window; 0 is what records 0, 3 and 6 hold.
- **Skills by level**: the default lists of records 1, 2 and 5 hold the row
  skills of their levels 2..N (section 2). A level-1 record that keeps them
  would get them **again** when it re-levels, because `AbilityList_Add` files
  an id without a held test: a duplicate in the list, in the next free slot
  (inference from `char.cpp:439-444`; the effect of a duplicate in a list is
  not read). So they must be taken out; re-levelling gives them back at the
  same levels. The rest of each list (the row-0 skill and any other) stays.
- **Willpower** is made from the level once, at New Game:
  `NewGame_InitCharacters` (`0x437820`, `src/game/battle/battle.cpp:111-136`)
  stores `(level << 4) / div + add` (the pair at `0x64B8B0 + 2n`) into `+0x4E`,
  `+0x1C` and `+0x2E`. `Char_LevelUp` never changes it. At level 1 the formula
  gives (`python lv2.py`): record 1 2 -> 0, record 2 11 -> 3, record 4 3 -> 1,
  record 5 6 -> 1. In battle, a kill whose overkill is below the current willpower
  sets the member's flag bit 2 (range-b 1, row `+0x1C`).
- **HP and AP**: in the default records current HP and AP equal the
  effective maxima. `TitleFlow_EnterGame` cuts HP to the new max after a load
  but does not cut AP (range-b 1.1), so a rewrite must set both.
- **Not tied to level**: equipment, resistances, percentages, the `+0x4C`
  word, name, id, flags.

## 4. Who else writes a record's level, EXP or stats

**Answer.** Outside New Game, the level is written only by `Char_LevelUp`,
and EXP only by `BattleResult_AddExp` and `Scena08_SetUpRecord4`. Only one of
these can undo part of a level-1 start, and it does so the same way the
original does: **chapter 8 gives record 4 10,000 EXP and levels it**.

| Writer | Record | When | Fields | Effect on a level-1 champion |
|---|---|---|---|---|
| `Char_LevelUp` `0x498DE0` | the argument | after a won battle (`BattleResult_Setup`); chapter 8 | level, base stats, abilities | normal growth from the champion's own EXP |
| `BattleResult_AddExp` `0x4468B0` | battle members | the win's EXP tick (`BattleResult_ExpTick` `0x431AE4`, `0x431B06`) | EXP += , cap 9,999,999 | normal |
| `Scena08_SetUpRecord4` `0x553630` (`src/game/scenario/scena_08_2.cpp:1420-1434`) | 4 | chapter 8, `Scena08_Scene11` step 7 | EXP += 10,000, `Char_LevelUp(4)`, weapon and three armour set to fixed ids, recompute, HP and AP filled | record 4 goes from EXP 0 to **level 19** (`python lv2.py`; 19 from its default EXP too). Not an overwrite: the champion keeps its own gains and gets the same 10,000 as in the original. Its equipment is replaced (see below) |
| `Scena06_GuestRecord` `0x54E700` | 7 | chapter 6 guest | HP, stats and base stats, two equipment bytes | none: record 7 is not a champion (6a item 7) |
| `BattleExtra_ApplyTally` | 7 | the battle extra | base stats | none, record 7 |
| `ItemUse_*Up` stat items (range-b 1 rows `+0x40..+0x4A`) | the target | the player's use | one base stat, added | normal, the player's choice |
| `Area192_RestoreCharacters` `0x42C2D0`, `RestoreRecords` (`src/game/scenario/scena_14_2.cpp:423-446`), `Party_HealJoined` | joined records | story heals | HP = max, AP = max, status 0, current willpower in some (range-b 1, row `+0x1C`) | none: no level, EXP or base stat |
| `Battle_WriteBackMember` | battle members | battle end | status, HP, AP, current willpower | none |
| The dragon form's backup and restore (`Accession_ApplyB` `0x4EB350`, `BattleFx_RestoreParty` `0x433DA0`) | - | in battle | the battle's working copies (`0x14C`-byte party records), not `CharacterRecords` | none |
| `NewGame_InitCharacters` `0x437820` | 0..7 | New Game | the whole record | before any stamp: the stamp runs after it |
| A save's load (`TitleFlow_EnterGame` `0x588800`) | all | load | the saved record, then the recompute | the save's own state: a stamped save keeps its level-1 champion |

**Evidence.**

- `python sweep.py` (absolute writes, 2026-11-12): the only hits on level,
  EXP or base stats are `BattleResult_AddExp` (`0x446937`, `0x44695B`),
  `Char_LevelUp` (the 13 writes listed in section 1), `Scena06_GuestRecord`
  (`0x54E72D..0x54E77D`, record 7's base words) and `Scena08_SetUpRecord4`
  (`0x55363C`, record 4's EXP).
- `python sweep2.py` (register-relative writes in the 208 functions that
  name a record address): eight hits, all stack locals (`[esp + 0xc]`,
  `[esp + 0xa]`), a sprite object (`Field_LeaderStand`), or data decoded as
  code (`rcl` in `Scena06_Run12`). None is a record write.
- The range-b READ pass lists the pointer-based writers: `+0x0A` only
  `Char_LevelUp`; `+0x0C` only `BattleResult_AddExp` (it misses
  `Scena08_SetUpRecord4`, which the absolute scan found); `+0x40..+0x4A` the
  default records, `Char_LevelUp`, the stat items and record 7's two writers.
  The bulk copies to working copies ("no function copies a working copy back
  whole") are its section header.
- **Found on the way (chapter 8 equipment):** `Scena08_SetUpRecord4` stores
  the weapon and three armour ids directly (`scena_08_2.cpp:1427-1430`); it
  does not return the old items to the inventory (inference: no inventory
  call in the function). With "champion always" a champion record 4 fights
  from chapter 1 (6a, "What this opens"), so whatever the player has put on
  it by chapter 8 is replaced and, by that inference, lost. Whether the
  original can reach this point with player-chosen gear on record 4 is not
  read here. CB or LV should decide on it.

## 5. The proposal

**Make record R level 1 from the exe's own data, once, at the stamp, and
only if record R is still at its New Game state.** No table of ours is
needed.

**When.** At the moment SC writes the champion byte on a save that has none
(6a item 6), for R in 1, 2, 4, 5. Records 0 and 6 start at level 1 and are
left alone. (Record 0 may have gained EXP in the intro, where the mode stands
down; that is the original's.)

**The test.** Record R's level `+0xA` and EXP `+0xC` equal the default
record's, its master `+0x1F` is `0xFF` and `+0x88` is 0. If the test fails,
the record has moved on (section 2's last paragraph): leave it and log one
line. A save stamped late in the game then keeps its champion's level. That
is the owner's call (below).

**The fields**, from `Char_DefaultRecords` (`0x64B390 + 0xA4 R`) and
`Char_ExpTable` (rows `99 R + 1 .. 99 R + N - 1`) **as the image has them**
(the image layer's `char_defaults` and `exp_table`, so a PSP or mod layer is
followed):

1. `+0xA` = 1, `+0xC` = 0.
2. `+0x40..+0x4A`: the default base minus the sum of the row gains (section
   2). If any result is negative (a layer whose table does not fit the
   default records), do not rewrite; log it.
3. The ability lists: for each non-zero id in bytes `+6`, `+7` of those rows,
   remove one copy from its type's list (`AbilityList_ForType`), and move
   the rest forward so the list stays packed from the front in its old order.
4. Willpower: `+0x4E` from New Game's formula at level 1. This is the one
   choice in the list (below).
5. `Char_RecalcStats(record)`, then `+0x18` = `+0x20`, `+0x1A` = `+0x22`,
   `+0x1C` = `+0x2E`.

Leave the rest as it is: name, id, flags, status, equipment, resistances,
percentages, the `+0x4C` word, the master bytes.

**What undoes it later.** Nothing except the game's own growth. The level
rises only from the champion's EXP; chapter 8 adds 10,000 EXP to record 4
as it does in the original (level 19 from 0 EXP). A load of a save made
before the stamp has no champion byte, so the stamp runs again on it. A save
editor that writes level or stats is the player's own act.

**The owner's calls this leaves.**

- **Willpower.** Recompute it at level 1 (the game's own rule: it is made
  from the starting level) or keep the default level's value. Recomputing
  gives record 1 a willpower of 0 (its default is 2). Recommended: recompute;
  a level-1 start should be level 1 all through.
- **A late stamp.** A save already past New Game, stamped the first time the
  mode meets it: keep the champion's level (recommended: the rewrite is only
  defined from the default record), or take the champion to level 1 anyway,
  which loses its masters' and items' gains and needs rules this note does
  not have.
- **Record 4's chapter 8 equipment** (section 4).

## What the round doc must change

- 6a item 4: M10 is read. "Whether a script sets a level at a join is not
  read" is answered: none does, except chapter 8's 10,000 EXP to record 4
  (`Scena08_SetUpRecord4`), which adds and does not overwrite.
- LV's scope is section 5: a rewrite at the stamp from `char_defaults` and
  `exp_table` as laid, for records 1, 2, 4 and 5, guarded by the New-Game-state
  test; no table of ours. LV owns the duplicate-skill removal and the
  willpower recompute.
- SC: the stamp must run after `NewGame_InitCharacters` and before the save's
  first write, and LV's rewrite must run at the same moment so the save never
  holds a champion byte with a default-level champion.
- CB (or LV): record 4's equipment at chapter 8 when it is the champion.
- Section 6 gains the two owner's calls above (willpower, late stamp).
