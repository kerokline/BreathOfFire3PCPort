# Group R4A: the battle's targeting helpers, Field_RunSlot, the community's simulation

**Status:** MEASURED (2026-10-05) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave four, from
the round branch's tip `35ec19ef`. **48 functions ours**
(`src/game/rest_4a.cpp`, shadow name `rest_4a`): the cut table's 48 rows for
R4A (`analysis/round14_cut.tsv`, the band `0x452DD0..0x456D4F`), none added,
none dropped (no start is a case, a shared tail or data; the band tool found
no code no list has). Each read to its last instruction with capstone and
fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7) without edits to it:
194,000 rounds, 0 mismatches; 74 controls planted, 72 refused (two by
ours' abort, their in-range variants refused), one equivalent and one not
refused whose near variant is (section 6). `'*'` exit 0 narrow and with
`BOF3X_WIDE=1`; `ledger_check` 0 errors. **Fuzz only**: no
recorded route enters the faerie village or reaches a row of this wave
(section 9). The 26 `hypothesis` rows are all functions. No full-frame fill
is built by any of the 48. No byte of the 48 is patched (DIVERGENCE.md,
`cheats.cpp`, `widescreen.cpp`: none of the addresses). **One function wants
a ledger entry** (section 7, L1): `Battle_RandomLiveEnemy` reads up to six
bytes of its own stack frame it never wrote when five or more enemies are
standing; ours takes them as 0.

The cut called the band "field core below the community rows" with 20 rows
under "table 0x6529E4 read by 0x456D50" (that reader is R4B's, and none of
these rows is in its table). By the code it is three things:

| Rows | What | Names |
|--:|---|---|
| 6 | the battle's targeting helpers: the auto-target check, a random standing member / enemy / other member, a member's action test, a member's fixed auto-target | `Battle_AutoTargetCheck`, `Battle_RandomLiveMember`, `Battle_RandomLiveEnemy`, `Battle_RandomOtherMember`, `Battle_MemberActionIs0E`, `Battle_MemberAutoFixed` |
| 2 | a `Field_Slots` record's step and the CLUT-row copy it makes | `Field_RunSlot` (named before, now ours), `Field_SlotClutCopy` |
| 28 | the community's simulation (areas 175..185, Area179_Init's callee): the entry, the queue of new areas, population, mood, the two building levels, the ticks of building kinds 5 / 9 / 0xB / 0xD, a record's spawn and removal, two sums, the offer words, and the village's objects placed and posed by building kind | `CommuSim_*` (17), `CommuPose_Kind0`, `_Kind4` .. `_KindD` (11) |
| 12 | entries 1..11 and 61 of `Field_ObjectTriggers` (`0x662E20`) | `FieldTrigger01` .. `FieldTrigger11`, `FieldTrigger61` |

What any of it is in play (what a building kind is, what the records are, what
the village shows) is not stated here: the descriptions are what the code
reads, writes and calls. The area numbers 175..185 are the community's by the
sibling's `names/places.toml` (dev label "kyoudoutai"); `Area179_Init` (ours,
`area_w4d`) calls `CommuSim_AreaEnter` when the chapter byte is above 7.

## 1. What each function does

The community's cells (named in `rest_4a_callees.h`; none has a `symbols.toml`
name):

- the clock `0x904134` (a dword `GameMode_Field` counts on `Field_Request` 3)
  and six stamps `0x9046B4..0x9046C4` it is compared against;
- 60 records of 8 bytes at `0x9046D0`: +0 in use, +1 the building (1..8) or a
  loose kind (0, 9, 0xA, 0xB), +2 and +3 states, +4 a clock stamp; 60 trait
  records of 0x14 bytes at `0x653200` (image .data) by record, read at +0..+4
  and +0x10..+0x13;
- 8 buildings of 8 bytes at `0x9048B0`: +0 the kind, +1, +2, +3 a level, +4 a
  stamp;
- the mood `0x9046CA` (s8, 0..99), the lit level `0x9046CC` (below 7), the dark
  level `0x9046CB` (below 10), the area seen `0x9046C8` (u16);
- an event queue of 2-byte entries at `0x904CA0` counted by `0x937F80`, a
  spawned list at `0x904F00` counted by `0x904A90`, a removed list at
  `0x9039C0` counted by `0x9039A0` (none bounded);
- the resident counts `0x675F58` (8 bytes) and nine offer words `0x675F60`
  (written .data).

| Function | Address | What |
|---|---|---|
| `Battle_AutoTargetCheck` | `0x452DD0` | (actor) -> al: 1 without battle flag bit 14, when `0x904B8B` is out, or when it is the actor; a party actor stores `0x904B8B` as the target and answers 0; an enemy with +0xBA above 1 and two status bits clear answers 1; else with `0x904B35` 4 its ability's flags byte picks the target (0xC0 / 0x80 / 0x40, or the actor) and 1, otherwise the target `0x904B8B` and 0 |
| `Battle_RandomLiveMember` | `0x452EB0` | of members 0..2 not out, the one at `Rand() % count` |
| `Battle_RandomLiveEnemy` | `0x452F10` | of enemies 3..10 not out, the one at `Rand() % count` - a four-byte list on the stack (L1) |
| `Battle_MemberActionIs0E` | `0x454260` | (member) -> al: its action word 0xE or 0x128 |
| `Battle_MemberAutoFixed` | `0x454290` | (member): odds +0x142 (at most 5) against `Rand() % 20`: under, and `0x904AB1` not 1, `Battle_RandomOtherMember`; else `Battle_DefaultTarget((Rand() & 7) + 3)`; +0x125 1, `0x904B35` 1, the target the answer |
| `Battle_RandomOtherMember` | `0x454310` | (member) -> al: of members 0..2 not out and not the argument, the one at `Rand() % count` |
| `Field_RunSlot` | `0x455300` | (index): the record's object pose against +2 picks a script from the table +4; at +1 0 the script steps (an 0xFF step leads back); `Field_SlotClutCopy`; +1 down |
| `Field_SlotClutCopy` | `0x455380` | (slot): the frame +3 split by the object kind's divisor into a CLUT row and an offset; script byte 2 colours copied along the row; `Gfx_ClutStripDirty` |
| `CommuSim_AreaEnter` | `0x455450` | same area as `0x802290`: place the objects only; else count the records (R4D's `0x45E6B0`), roll the offers, clear three counts; no records: three spawns and the clocks, mood 10, levels set; some: queue areas, mood, levels, population, ticks 9 / 0xD / 5 / 0xB, the sums; then place the objects |
| `CommuSim_QueueAreas` | `0x455540` | area numbers past the area seen whose byte `0x652850[n]` is 4 or 5 queue that kind once |
| `CommuSim_Population` | `0x4555D0` | (count): by mood against 4 x count, spawns (up to 20 records) or removals (down to 1) by the elapsed clock / 10 or / 20 |
| `CommuSim_Mood` | `0x455700` | (count): the kind-9 records' trait sum less 2 x count, times the elapsed fifths, added to the mood, clamped 0..99 |
| `CommuSim_LevelLit` / `_LevelDark` | `0x4557A0` / `0x455870` | a level up when the elapsed clock reaches a cost less the residents' trait sum (events 7 / 6) |
| `CommuSim_TickKind5` / `_TickKindB` | `0x455950` / `0x455DF0` | a building's level +3 raised by the elapsed clock over a divisor (events 0 / 1 with the first resident) |
| `CommuSim_TickKind9` | `0x455AA0` | each resident of a kind-9 building: two rolls set its +2 to 1, 2 or 3 or remove it (event 2) |
| `CommuSim_TickKindD` | `0x455BE0` | each resident of a kind-0xD building with +3 0x1n: its item's price tier and two rolls set +3 / +2 (event 3) |
| `CommuSim_AddRecord` / `_RemoveRecord` | `0x455F40` / `0x455FF0` | a record from a random start: the first free one taken (traits copied), or the first in use (kind 9 last) dropped |
| `CommuSim_SumKindsAB` | `0x456080` | `0x9046CD` / `0x9046CE` the trait +0x11 sums of kinds 0xA / 0xB |
| `CommuSim_RollOffers` | `0x4560D0` | the nine words `0x675F60`: three groups of three distinct `0x9B + n` |
| `CommuSim_PlaceObjects` | `0x4561A0` | each record in use takes the next sprite object (by kind), the rest up to 20 cleared |
| `CommuSim_PlaceLoose` / `_PlaceResident` | `0x4562B0` / `0x4563B0` | one object: Sprite_Current, place, elevation, bank, facing; a resident's pose by its building's kind (a 14-entry jump table inside the function) |
| `CommuPose_Kind0`, `_Kind4` .. `_KindD` | `0x456680`..`0x456AF0` | the object's pose word +0x88 (and +6, +0x18, +0x1C) by the resident count, the levels, a flag or a roll |
| `FieldTrigger01`, `04`..`08`, `10`, `11`, `61` | `0x456B20`, `0x456C50`..`0x456D30` | tail kind `0x9039F3` (0xE, 0x15..0x19, 0x3C), the object to `0x939A38`, a sub-kind `0x9039F5` (the object's +5, or 2 / 3), `ScriptFlags_Set40`; al 0 |
| `FieldTrigger02` / `03` | `0x456B70` / `0x456BF0` | four / three numbers into `Text_Records` rows by `Area08_MessageFormat`, `Msg_OpenScript(0x55 / 0x56)`, `Field_Request` 2; al 0 |
| `FieldTrigger09` | `0x456B40` | `Shop_Records[object +0x1C]` +0 = the level of the building of the record the object's +5 names; al 0 |

## 2. Divergence, and the patched sites read back

None of the 48 is patched by a ledger entry, `cheats.cpp`, `widescreen.cpp`,
`labels.cpp` or `yes_no_layout.cpp` (grepped 2026-10-05). No full-frame fill.
One function is not a plain copy: `Battle_RandomLiveEnemy` (L1, section 7) -
**wants a ledger entry**; the coordinator's and the owner's.

## 3. The tables

- **`Field_ObjectTriggers` `0x662E20`** (named before, ART's): the twelve
  triggers are its entries 1..11 and 61. No `[[data]]` added: it is named, and
  its reader `0x56E020` is not this group's.
- **`CommuSim_PlaceResident`'s jump table `0x45663C`** (14 entries, bounded by
  `cmp eax, 0xD; ja`) is inside the function's extent: the clone moves it.
- The image tables the simulation reads (`0x652850`, `0x65290C`, `0x65291B`,
  `0x652928`, `0x652958`, `0x652960`, `0x65299C`, `0x6528E4`, `0x6528EC`,
  `0x6528F4`, `0x653200`) are not named: they hold no code pointers, and
  wave four's other groups read the same records - a name is for the
  round's end, once (the addendum's rule on an address entered twice).

## 4. The fuzz (`rest_4a_fuzz.cpp`)

Two groups, field mode, every clone `kCall`:

- **`rest_4a`**: the 47, 4,000 rounds a function (188,000). ret_mask 0xFF on
  the five battle helpers that answer in al and the twelve triggers.
- **`rest_4a enemy`**: `Battle_RandomLiveEnemy` alone, 6,000 rounds, with its
  own `Battle_ActorIsOut` stand-in (`LevelledIsOut`, a custom stand-in): on
  the original's side, at the first call (actor 3), it writes 0 to the six
  frame bytes the original never writes (the count dword's upper three and
  the loop dword's), which ours holds as 0 - L1's levelling. Standing three
  times in four, so five or more standing enemies (the list's overflow) is
  the common case.

Stand-ins beyond the standard sets: the group's own callees (every
`CommuSim_*` / `CommuPose_*` a sibling calls, `Field_SlotClutCopy`,
`Battle_RandomOtherMember`), R4D's `0x45E6B0` (`kByte` 0..2), and three re-listed:
`Battle_ActorIsOut` (mask 0xFF, `kFlag`, an effect that keeps one actor of the
picker's range standing - the last not excluded, set per round in `Args` - so
the count is never 0: the original faults there and ours aborts, neither
comparable), `Battle_DefaultTarget` (mask 0xFF) and `Rand` (`kRand` masked to
0x7FFF, the CRT's range: the harness's garbage answers would take the pickers'
`idiv` below their lists, where ours aborts).

Regions beyond field mode's: the community and battle bytes
`0x904700..0x904B90`, the eight enemy records, `Field_Slots`,
`Gfx_ClutStrip` (0x4000), the event queue with `Text_Records`' rows, the
spawned list's head, `0x9039A8..0x903A04` (the removed list, the tail kind
and sub-kind), the event count, the resident counts and offer words, the
trigger's object cell, `Shop_Records`' first 50, the word `0x802290`.

Seeds: the community as the simulation leaves it (up to 24 records in use and
two always, each +1 a building or a loose kind, stamps at and about the
ticks' boundaries from the clock, building kinds 4 / 5 / 9 / 0xB / 0xD
mostly, levels 0..2, no kind-0xB building with six residents, the counts
small, the area words among 0xAF..0xBA); a `Field_Slots` record with its
object, its four-script table and a script ending in an 0xFF step back
(frames below 15, object kinds 0..4, so the CLUT row stays in the strip);
the battle bytes with the flag bit 14 mostly set, `0x904B35` 4 often, enemy
records about the +0xBA > 1 boundary, members' actions 0xE / 0x128 and their
neighbours, odds about 5. The triggers' object is a sprite record with +5 a
record and +0x1C a shop below 50.

The group's disturbance (`Disturb`, ten cases by `sh::DisturbCase(h, 10)`,
values from `h` only): a slot's +1, the clock, `Game_AreaNumber`, a record's
+2 / +3, a building's level (0..2), the event or removed count, `0x904B8B`
and `0x904AB1`, a resident count, an offer word, a record out of use. Each
case's cell is re-read after a call by some function of the group, and a
control misses each re-read (section 6). Five stand-ins are louder than the
standard rows for it (`Rand`, `Battle_ActorIsOut`, `Field_SlotClutCopy`,
`0x45E6B0`, `AreaMap_Elevation` and the poses: section 6), each moving its
cell from its own answer.

What the harness lacks, for the coordinator's fold: nothing that blocked; the
custom stand-in's frame write is this group's own (it knows the clone's frame
layout).

## 5. What the cut and the tool said, settled

- **Extents**: the tool's read and the cut agree but for padding on nine rows
  (`0x4563B0` 708 + table = 0x2C4 against 713; `0x456AF0` 35 against 48; seven
  triggers); the code's are right.
- **`0x456AF0` as a host**: the cut and `entries_logic.txt` (0x34E) give it
  the twelve triggers after it. It is `CommuPose_KindD` (0x23 bytes, its own
  `ret`); each trigger is a function of its own reached through
  `Field_ObjectTriggers` (its address in a cell). No start is a case.
- **"table 0x6529E4 read by 0x456D50"** (20 rows): `0x456D50` is R4B's
  dispatcher on `0x9039F4`; none of R4A's rows is in its table. The rows are
  the simulation's, reached by direct calls.
- **PSX twins** (`analysis/pairs_propagated.json`): `0x452DD0` 0x800A626C,
  `0x452EB0` 0x800A64BC, `0x452F10` 0x800A6408 (call-anchored), `0x455450`
  0x801EECA4 (call); the sibling names none of them (`names/*.toml`,
  `symbols.toml` grepped). Not read; no name transferred. `Field_RunSlot`'s
  0x80197D84 is the 2026-09-22 entry's.

## 6. Controls

The script (`controls.py` in the session scratchpad's `r4a/`) plants each
mutant in `rest_4a.cpp` on a unique anchor, rebuilds, runs `rest_4a` on the
named clones (`BOF3X_R4A_ONLY`), restores and, at the end, rebuilds clean.
Counts are in this worktree, at the committed fuzz (the second-to-last
column is the clones run). **74 planted: 70 refused by a count, 2 by ours'
abort (their in-range variants refused), 1 equivalent, 1 not refused (its
near variant refused).**

| # | Mutant | Clones | Mismatched rounds |
|---|---|---|--:|
| C1 | flag bit 14 -> 15 | `Battle_AutoTargetCheck` | 1,980 / 4,000 |
| C2 | `0x904B8B` read before `Battle_ActorIsOut` (case 6) | same | 467 |
| C3 | `forced >= 3` -> `> 3` | same | 3 (thin: the boundary) |
| C4 | members 0..1 only | `Battle_RandomLiveMember` | ours' abort (count 0) |
| C4b | member 1 never listed | same | 747 |
| C5 | the list's overflow not written (`f[c]` only below 4) | `Battle_RandomLiveEnemy` | 4,358 / 6,000 |
| C6 | one never-written frame byte 1, not 0 (L1's levelling matters) | same | 600 / 6,000 |
| C7 | 0x128 -> 0x129 | `Battle_MemberActionIs0E` | 694 |
| C8 | odds cap 5 -> 6 | `Battle_MemberAutoFixed` | 23 |
| C9 | `0x904AB1` read before `Rand` (case 6) | same | 23 |
| C10 | exclusion of `m + 1` | `Battle_RandomOtherMember` | ours' abort |
| C10b | member 0 never listed | same | 505 |
| C11 | the slot's +1 read before the copy (case 0) | `Field_RunSlot` | 1,978 |
| C12 | the 0xFF step back one entry short | same | 118 |
| C13 | +0x24 bit 2 -> 3 | `Field_SlotClutCopy` | 1,630 |
| C14 | the destination column + 1 | same | 3,331 |
| C15 | the mood stamp + 1 | `CommuSim_AreaEnter` | 619 |
| C16 | the area read before the spawns (case 2) | same | 9 |
| C17 | the clock read before the spawns (case 1) | same | 7 |
| C18 | area kind 5 -> 6 | `CommuSim_QueueAreas` | 1,740 |
| C19 | the spawn cap + 1 | `CommuSim_Population` | 95 |
| C20 | the grow stamp the clock before the calls (case 1) | same | 13 |
| C21 | the shrink stamp likewise | same | 4 (thin) |
| C22 | the mood cap 99 -> 100 | `CommuSim_Mood` | 497 |
| C23 | trait + 3 -> + 2 | same | 878 |
| C24 | event 7 -> 8 | `CommuSim_LevelLit` | 763 |
| C25 | `<` -> `<=` on the lit level needed | `CommuSim_LevelDark` | 95 |
| C26 | the divisor floor 5 -> 4 | `CommuSim_TickKind5` | 12 |
| C27 | event 0 -> 1 | same | 554 |
| C28 | the age x 6 -> x 5 | `CommuSim_TickKind9` | 390 |
| C29 | the level read before the roll (case 4) | same | 4 (thin) |
| C30 | the removed count read before 0x45E6B0 (case 5) | same | 73 |
| C31 | +3 not read again after the roll (case 3) | `CommuSim_TickKindD` | 29 |
| C32 | the roll's fold 0x1C -> 0x1B | same | 4 (thin) |
| C32b | the fold 0x1C -> 0x10 | same | 6 |
| C33 | the tier walk's end `>=` -> `>` | same | 0: **equivalent** (the eighth bound is 0xFFFF, no u16 price is above it) |
| C33b | the walk stopped a tier early | same | 0: no seeded item prices above the seventh bound (30,000) |
| C33c | the walk stopped three tiers early | same | 40 |
| C34 | `6 - count` -> `7 - count` | `CommuSim_TickKindB` | 1,077 |
| C35 | the cap + 1 | same | 67 |
| C36 | a spawn's +2 = 1 | `CommuSim_AddRecord` | 4,000 |
| C37 | kind 9 -> 8 passed over | `CommuSim_RemoveRecord` | 636 |
| C38 | kind 0xB -> 0xC | `CommuSim_SumKindsAB` | 2,960 |
| C39 | 0xA0 -> 0xA1 excluded | `CommuSim_RollOffers` | 1,908 |
| C40 | only the words written compared (case 8) | same | 233 |
| C41 | area 0xB9 -> 0xBA | `CommuSim_PlaceObjects` | 671 |
| C42 | the area read once before the loop (case 2) | same | 241 |
| C43 | the clear to 19, not 20 | same | 1,918 |
| C67 | the records' +0 read once before the loop (case 9) | same | 142 |
| C44 | +0x84 2 -> 3 | `CommuSim_PlaceLoose` | 3,572 |
| C45 | +5 the record + 1 | same | 4,000 |
| C46 | the facing read before the bank call (Sprite_Current moved) | `PlaceLoose`, `PlaceResident` | 312 / 12,000 |
| C47 | the count read before the elevation call (case 7) | `CommuSim_PlaceResident` | 14 |
| C48 | the count read before the pose call (case 7) | same | 175 |
| C49 | kind 0xC's pose `KindA`'s | same | 232 |
| C50..C60 | each pose's constant or cell one off | `CommuPose_*` (one each) | 547 .. 4,000 each |
| C61..C66 | the tail kind, a row's number, the message, the sub-kind's byte, the shop's building, kind 0x3C | the triggers | 3,983 .. 4,000 each |
| C68 | tail kind 0x18 -> 0x19 in the shared helper | all twelve triggers | 4,000 (`FieldTrigger10`) |
| C69 | the event count read before the rolls (case 5) | `CommuSim_TickKindD` | 164 |

Every case of the group's disturbance moves a cell some function re-reads
after a call, and a control misses each: case 0 C11, 1 C17 / C20 / C21, 2 C16
/ C42, 3 C31, 4 C29, 5 C30 / C69, 6 C2 / C9, 7 C47 / C48, 8 C40, 9 C67.
**The fuzz was made louder for them** (C11, C30, C31, C47, C48 were 0 at
first, C2 / C9 at 4 and 1): `Rand`'s stand-in runs the group's disturbance one
call in four, `Battle_ActorIsOut`'s moves `0x904B8B`, `Field_SlotClutCopy`'s
the slot's +1, R4D's `0x45E6B0`'s the removed count, `AreaMap_Elevation`'s and
every pose stand-in's a resident count; case 3 hits the record `TickKindD` is
on half the time. Thin (under 10): C3, C21, C29, C32 - each a boundary or a
re-read under one call; named for the debt list.

## 7. Latent defects (Capcom's, described, not fixed)

- **L1 - `Battle_RandomLiveEnemy` `0x452F10`: a four-byte list for eight
  enemies. Wants a ledger entry.** The list is at `esp + 4` of a 12-byte
  frame, the count byte in the dword after it, the loop byte in the dword
  after that. With five or more of enemies 3..10 standing, the fifth write
  lands on the count (the count becomes that enemy's number, 7..10), the next
  ones on the count dword's upper bytes and the loop dword (the loop runs on
  `bl`, so it is not disturbed; no write reaches the return address - the
  last index written is at most 9). `Rand() % count` then picks any of frame
  bytes 0..9: the list, the count byte, written bytes, the loop byte (11 at
  the end), or the count dword's upper bytes (frame bytes 5..7) and the loop
  dword's (9) where nothing was written - **whatever the stack held**. The
  answer goes to `BattleAction_PickRandomAbility` (`0x42F9D0`, ours, BE1) as
  a target. Ours keeps the same twelve bytes and writes them as the original
  does, so every pick the original makes from written bytes is ours too; the
  six never-written bytes are 0 (the nearest the function computes: it stores
  the count's and the loop's low bytes into zeroed dwords). With four or fewer
  standing nothing differs. Whether a battle reaches it with five standing
  enemies and that caller is the owner's to say; a target 0 (member 0) or a
  number outside 0..10 then is what the original can produce too.
- **L2 - divides by zero** (ours aborts with a message, before the fault):
  `Battle_RandomLiveMember` (all members out), `Battle_RandomLiveEnemy` (all
  enemies out), `Battle_RandomOtherMember` (no other member standing - a
  member alone, picking another: `Battle_MemberAutoTarget` calls it on a roll
  of 3 or more unless `0x904AB1` is 1, so play may reach it when one member
  stands; not established), `Field_SlotClutCopy` (object kinds 5..7 of the
  divisor table hold 0), `CommuSim_TickKindB` (a kind-0xB building with six
  residents: `6 - count`). The pickers' `Rand` below 0 (not the CRT's) would
  index below the list: ours aborts.
- **L3 - walks without an end**: `CommuSim_AddRecord` with all 60 records in
  use, `CommuSim_RemoveRecord` with none: the original loops forever; ours
  aborts before the walk. `CommuSim_RollOffers` loops until three distinct
  words, which `Rand` always gives in time.
- **L4 - unbounded indexes copied as they are** (reads and writes of the
  process's memory, no fault): `CommuSim_TickKind9` reads the odds
  `{0x41, 0x37, 0xF}` on its stack by a building's +1 - **above 2 the original
  reads its frame (a stale byte, then the return address)**; ours aborts
  there (no play reach shown). `CommuSim_PlaceObjects` takes one sprite object
  a record and does not bound them (60 records reach past the 30
  `Sprite_Objects`); `CommuSim_PlaceResident`'s count indexes the next
  building's places past three residents; the three lists and the event
  queue are unbounded; `FieldTrigger09` writes `Shop_Records` by the object's
  dword; `Battle_AutoTargetCheck` indexes enemy records by the actor byte.
  All copied.

## 8. Calls across groups

- **Out**: R4D's `0x45E6B0` (the records in use) from `CommuSim_AreaEnter` and
  `CommuSim_TickKind9` - by address (`rest_4a_callees.h` `kCommuCount`), 2
  sites; R4D's own name for it, at the round's rebinding.
- **In, from this round's wave four**: R4B's `0x456E20` calls
  `CommuSim_SumKindsAB` and jumps to `CommuSim_TickKind5` (by address in
  Capcom's code; R4B's own file names them at the rebinding).
- **In, from code already ours** (rebound, section 10): `Area179_Init`
  (`area_w4d`) -> `CommuSim_AreaEnter`; `Shop_Close` (`mode_states`) and
  the call at `0x495D06` (after a DAT load, in code the catalog counts in
  `GameMode_Field`'s extent; nothing of ours names it) ->
  `CommuSim_RollOffers`; `Field_RunSlots` (`frame_callees`) ->
  `Field_RunSlot`; `BattleEnemy_PickAction` (`battle_e2`) and
  `Battle_MemberAutoTarget` (`battle_sprites`) -> `Battle_AutoTargetCheck`;
  `BattleAction_PickRandomAbility` (`battle_e1`) -> `Battle_RandomLiveMember`
  / `_RandomLiveEnemy`; `Battle_MemberAutoTarget` -> `_MemberAutoFixed`,
  `_MemberActionIs0E`, `_RandomOtherMember`. The twelve triggers are reached
  through `Field_ObjectTriggers` from `0x56E020` (ours).
- **Harness rows** that list three of the 48 as Capcom's by address:
  `boss_harness.cpp` lines 814..816 (`0x452DD0` `kFlag` reading the low byte,
  `0x452EB0`, `0x452F10` `kFlag`, no argument). They match what was read (al
  answers; `0x452DD0` reads its argument's low byte). `'*'` passes with them
  as they are; left for the coordinator's `_OURS` fold.

## 9. The live route

None: no recorded route enters the faerie village (areas 175..185), and the
catalog shows no row of this wave reached. The battle helpers are called in
battles, but no trace of a recorded route enters them either (the catalog's
reach column is empty for all 48). **Fuzz only.** The owner records a route
once the code is ours.

## 10. The rebinding

Rebound (the value unchanged, so every fuzz key stands; each change kept to
its line):

- `battle_e2_callees.h` `kActorMayAct` -> `bof3::addr::Battle_AutoTargetCheck`
  (and its comment);
- `battle_e1_callees.h` `kSideTargetA` / `kSideTargetB` ->
  `Battle_RandomLiveMember` / `Battle_RandomLiveEnemy` (and the comment);
- `battle_sprites.cpp`'s table of originals: four `Raw<...>(0x...)` ->
  `Raw<...>(bof3::addr::Name)`; `battle_sprites_callees.h` comments ("nobody's"
  -> the name);
- `frame_callees.cpp`'s `kOriginals`: the bare `Field_RunSlot` (which became
  our function) -> a pointer from `bof3::addr::Field_RunSlot`, so
  `BOF3X_ORIGINAL` still restores Capcom's for that caller (review item 4);
- `area_w4d_callees.h` `kFieldReset` -> `bof3::addr::CommuSim_AreaEnter`
  (the header now includes `symbols.gen.h`; its comment updated);
- `mode_states_callees.h` `kAfterShop` -> `bof3::addr::CommuSim_RollOffers`
  (the include added); `mode_states.cpp`'s comment "the unread 0x4560D0".

Left raw: the call-site targets in other groups' clone tables
(`battle_e1_fuzz`, `battle_e2_fuzz`, `battle_sprites_fuzz`, `area_w4d_fuzz`,
`mode_states_fuzz` - disassembly facts the harnesses check) and their
stand-in rows keyed by address (`battle_e2_fuzz` `{"0x452DD0", ...}`,
`battle_sprites_fuzz`'s masks); `boss_harness.cpp` 814..816 (section 8);
comments that cite an address beside what it does (`battle_e2.cpp` 868-869,
`battle_sprites.cpp` 413-455, `area_w4d.cpp` 235).

## 11. For `analysis/calltrace/entries_logic.txt`

The 48 extents were present for 34 starts already; appended 14 lines
(2026-10-05): the twelve triggers, `0x4563B0 2C4` beside the existing `2C9`
(padding), and `0x456AF0 23` beside the host line `0x456AF0 34E`, which
covers the twelve triggers: **the coordinator splits it** at the round's end.
