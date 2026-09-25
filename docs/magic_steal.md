# Steal's overlay: the spell harness's proof

**Status:** IN PROGRESS (2026-09-25) - three functions ours
(`src/game/magic_steal.cpp`, shadow name `magic_steal`), fuzzed headless
through the shared harness with DIV-0046 on and off; 28 negative controls,
every one refused by a count (exit 3) in the one function it touches. No
recorded route casts Steal: fuzz only until the owner's eye.

Group SH of round nine ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)).
The proof that the harness ([`magic_harness.md`](magic_harness.md)) takes
one overlay end to end: `Magic_Rows` row 87, file `0x2AD` = the PSX's
`MAGIC216.EMI`, loaded by ability id `0xD8`, which is **Steal** read one id
down ([`cut-content.md`](cut-content.md) §2). That Steal is a skill the
player uses is the reading of [`cheats.md`](cheats.md) (DIV-0046 was made
for it and the owner watched it work on Pilfer's copy), not a measurement
of Steal's.

## 1. The extent

`tools/magic_rows.py --unit MAGIC216`: `0x4F50B0..0x4F52E5`, four
functions, between MAGIC213's last (`0x4F5050`) and MAGIC218's entry
(`0x4F52F0`); the PSX overlay's catalogue gives one function root
(`overlay_catalog.json`, a count that misses stack-table phases). The
fourth is `0x4F52D0`, `MagicFx_EndWhenIdle`, round eight's (CJ): Pilfer's
overlay reaches it too, one body the linker kept here.

| Function | Entry | Bytes | Reached as | Does |
|---|---|--:|---|---|
| `SkillSteal_Task` | `0x4F50B0` | 0x2E | `Magic_Rows` row 87 | phase `+1` through a three-entry stack table |
| `SkillSteal_Start` | `0x4F50E0` | 0x56 | its entry 0 | the owner's actor byte and position to the task; `+0xB` 0; on |
| `SkillSteal_Roll` | `0x4F5140` | 0x18A | its entry 1 | the roll, the item, the message; on |
| `MagicFx_EndWhenIdle` | `0x4F52D0` | 0x16 | its entry 2 | (CJ's) the done flag and free once the message window is down |

`SkillSteal_RateTable` (`0x65C204`, 8 signed bytes) is named: the roll's
rate by the enemy's `+0xAA`, the same values as Pilfer's `Steal_RateTable`
`0x65AC20`. The names say what the functions do; the `Steal_` prefix was
taken by round eight for Pilfer's copy.

## 2. The roll

The same routine as Pilfer's (`Steal_Start`, [`magic_fx_reached.md`](magic_fx_reached.md)
§4) in an older shape: no thief's double, no animation, and the message
queued at once rather than by a later phase. The enemy is the target byte
`0x904B44` less 3, unchecked; the tier from the thief's speed (party
`+0xA8`, by `0x904B34`) less the enemy's `+0xB8`: 12, then one less below
each of 49, 29, 19, 9, -10, -20, -30, -50 (signed). Success when
`Rand() & 0xFF` is below the table's rate times the tier (signed, so a
negative rate never passes); then:

- no item (`+0xA8` word 0): message `0x3A`;
- `Inventory_Add(item >> 8, item, 1)` answering 0: `0x39`;
- else `0x4B58F0(item, category)` (the item's name into `Text_Records`),
  `0x38`, and the enemy's item and rate row cleared.

A failed roll gives `0x39`, or `0x3A` when the enemy's rate row is 0. Every
end queues `BattleQueue_Push(1, 0xFF, Msg_SystemPtr(id))` and steps the
phase. The target byte is read again after `Rand` and after the queue, as
the original reads it. The original pushes a fourth argument (0) to
`Inventory_Add`, which takes three; ours passes three (cdecl: the caller
pops, the callee never reads it). That these ids are "stole", "could not"
and "nothing to steal" is by the branches, as in CJ's reading.

**DIV-0046.** `BOF3X_STEAL=1` patches the mask's immediate at `0x4F51EF`
to 0 (`src/game/cheats.cpp`). Ours never runs that body, so it reads the
byte back after the patch - `Cheats_StealRollMask()`, new beside
`Cheats_PilferRollMask()` - and masks with it. The patch is still made, so
`BOF3X_ORIGINAL=SkillSteal_Roll` keeps the cheat and the fuzz's copy
carries it. Control R18 is the proof that the fuzz sees the difference.

No divergence and no ledger entry: each function is a faithful
replacement, except that a phase past the task's table aborts, as every
stack-table dispatcher the project has taken does.

## 3. The fuzz, and the controls

`BOF3X_SHADOW=magic_steal`, through `magic_harness::Run`: three copies (the
task's three immediates re-aimed at handler recorders, the roll's seven
calls at the standard recorders), the harness's standard regions plus the
rate table; 2,000 rounds a function. The seed (`magic_steal_fuzz.cpp`):
the phase 0..2; for the roll a target that is an enemy two rounds in three
(else a party slot, below the records), the rate row 0..7 two in three,
the exe's own table two in three (read at start-up, not written in the
source), the item 0 or not, the speed difference at each threshold and one
below, and `Rand` aimed at rate x tier, landing on the compare or one below
half the time.

Result (2026-09-25):

    shadow      magic_steal self-test: 6000 rounds over 3 functions (2000 each), 9278 calls to the stand-ins,
                0 MISMATCHES; 11368 bytes of state (9 regions) and the stand-ins' log compared
    shadow      magic_steal coverage (calls the originals made): Inventory_Add 763, Msg_SystemPtr 2000,
                BattleQueue_Push 2000, Rand 2000, 0x4B58F0 515, phase 0x4F50E0 645, phase 0x4F5140 720,
                phase 0x4F52D0 635

That is 515 thefts, 248 full bags and 1,237 failed rolls or empty enemies
in the roll's 2,000. With `BOF3X_STEAL=1`: 0 mismatches, 746 thefts.
`BOF3X_SHADOW='*'`: exit 0.

**Twenty-eight negative controls**, planted one at a time by a script (not
committed: apply, build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_steal`,
restore). All 28 are refused by a count (exit 3), each only in the
function it touches:

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Task: table index phase + 1 | SkillSteal_Task 2,000 |
| T2 | Task: entries 0 and 1 swapped | SkillSteal_Task 1,365 |
| S1 | Start: `+8` from the owner's `+9` | SkillSteal_Start 1,992 |
| S2 | Start: `+0x38` from the owner's `+0x3C` | SkillSteal_Start 2,000 |
| S3 | Start: `+0xB` = 1 | SkillSteal_Start 2,000 |
| S4 | Start: `+2` on, not `+1` | SkillSteal_Start 2,000 |
| R1 | Roll: threshold 48 | SkillSteal_Roll 7 |
| R2 | Roll: threshold -51 | SkillSteal_Roll 22 |
| R3 | Roll: top tier 13 | SkillSteal_Roll 22 |
| R4 | Roll: rate unsigned | SkillSteal_Roll 318 |
| R5 | Roll: `>` not `>=` | SkillSteal_Roll 239 |
| R6 | Roll: target not re-read after `Rand` | SkillSteal_Roll 43 |
| R7 | Roll: failed roll 0x39 whatever the rate | SkillSteal_Roll 67 |
| R8 | Roll: no item 0x39 | SkillSteal_Roll 241 |
| R9 | Roll: bag full 0x3A | SkillSteal_Roll 248 |
| R10 | Roll: category from the low byte | SkillSteal_Roll 761 |
| R11 | Roll: `Inventory_Add` count 2 | SkillSteal_Roll 763 |
| R12 | Roll: the name's arguments swapped | SkillSteal_Roll 515 |
| R13 | Roll: stolen message 0x37 | SkillSteal_Roll 515 |
| R14 | Roll: stolen queue (1, 0xFE) | SkillSteal_Roll 515 |
| R15 | Roll: target not re-read after the queue | SkillSteal_Roll 94 |
| R16 | Roll: rate row kept | SkillSteal_Roll 511 |
| R17 | Roll: item kept | SkillSteal_Roll 514 |
| R18 | Roll: DIV-0046 mask not read back (run with `BOF3X_STEAL=1`) | SkillSteal_Roll 468 |
| R19 | Roll: the ends' queue (2, 0xFF) | SkillSteal_Roll 1,485 |
| R20 | Roll: the thief is the next actor | SkillSteal_Roll 330 |
| R21 | Roll: the stolen phase not on | SkillSteal_Roll 515 |
| R22 | Roll: the enemy's speed at `+0xBA` | SkillSteal_Roll 303 |

The thinnest is R1 (7 rounds): a tier threshold moved by one shows only on
a roll at the compare with the difference at that threshold. R6 and R15
(43, 94) are the re-reads, shown only by a recorder moving the target byte
across the one call between the two reads.

Not planted, because no fuzz can see it: `SkillSteal_Start` reading the
owner pointer once instead of four times - nothing runs between the reads,
so the two are the same function unless the task's own writes alias the
pointer cell. Ours reads it four times, as the original.

## 4. For `analysis/calltrace/entries_logic.txt`

The three lines (`004F50B0 2E`, `004F50E0 56`, `004F5140 18A`) are appended
to the main checkout's copy under a `group SH` comment. The host line
`004F5050 280` still runs over them; its body is `57` (CJ's reading).

## 5. What the route reaches

Nothing: no recorded route casts Steal. The live check is the owner's, with
DIV-0045's cheat putting the skill in a list or a save that has it; the
thing to see is the message after the roll and, on a theft, the item in
the list (the same check DIV-0046's was, [`cheats.md`](cheats.md)).

## 6. Defects (Capcom's, latent, kept)

The same three CJ found in Pilfer's copy (known-defects D59, D64, D67):
the task's stack table is unbounded (ours aborts); the rate row is the
enemy's `+0xAA` unbounded (a row past 7 reads the bytes after the table);
the target is not checked to be an enemy (a target of 0..2 reads, and on a
theft clears, bytes below the enemy records). Nothing new.
