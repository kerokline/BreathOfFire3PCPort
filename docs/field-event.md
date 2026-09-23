# The event script's field side

**Status:** IN PROGRESS (2026-09-22) - 31 functions ours: the party leader's
frame and its first two states, the area set-up that places the party, a
member joining, the party-set loader and the small helpers they share. Each
read whole by capstone against its true extent and fuzzed against a byte copy
of Capcom's with every call out re-aimed at a recorder; 48 negative controls,
46 refused by a count of mismatching rounds, one refused by a fault (and the
comparison it was meant to test covered by another control), one not refused
and shown below to be a change that changes nothing. **Not yet through the
live batch check** - that runs centrally after the merge (section 7).

[`attract-remaining.md`](attract-remaining.md) section 4.6 catalogued this
block as "the `0x52D8F0`..`0x536AC0` block the area set-up and the field frame
call into", unread. This document reads the half of it the attract sequence
reaches. The event script's *interpreter* (`0x579740`..`0x57C810`) is a
separate group; the field's mode handlers (`0x56B2A0`..`0x56D920`) another.

## 1. Who calls what

Two entry points reach this file every field frame
([`field-frame.md`](field-frame.md)):

- `Field_MembersFrame` `0x517350` calls `Field_PendingJump` `0x533760`, then
  `Field_LeaderFrame` `0x52D8F0` for ObjTrio's member 0 and `Field_MemberFrame`
  `0x51AC50` for members 1 and 2.
- The area set-up `0x594E60` calls `Field_PartySetUp` `0x533110` and then
  `Field_PartyFirstFrame` `0x5334A0`, once per area.

Three dispatch tables decide the rest. All three are indexed by a **whole
byte, unchecked**; the entries past the end are other data.

| Table | Entries | Indexed by | What the entries are |
|---|--:|---|---|
| `0x660918` | 15 | `Sprite_Current +1` (the leader's state) | 0 `Field_LeaderStart`, 1 `Field_LeaderControl`, then `0x525370`, `0x52E9D0`, `0x52F390`, `0x52F400`, `0x52F4F0`, `0x52F5E0`, `0x52F950`, `0x528880`, `0x52FB60`, `0x52FBB0`, `0x52FE90`, `0x42A8B0`, `0x534480` - all unread |
| `0x660954` | 3 | `Sprite_Current +2` (its sub-state) | 0 `Field_LeaderStand`, 1 `0x52DB90`, 2 `0x52E110` - the last two unread |
| `0x660B60` | 4 | the byte `0x904EF1` | `0x533780`, `0x5338B0`, `0x5338F0`, `0x533900` - unread, none reached by the attract cycle |

The call tree inside the file:

```
Field_LeaderFrame -> Sprite_RestoreClut, Field_CopyInput, [0x660918]
  Field_LeaderStart   -> Sprite_ClearSteps, MapView_GroundAt, Field_LeaderAnimation
  Field_LeaderControl -> [0x660954], Sprite_ScriptTick
    Field_LeaderStand -> 0x535120, 0x535240, 0x5301F0, seven tests, 0x530480,
                         Field_LeaderAnimation
Field_PartySetUp  -> Flags_Test, Party_Count, Sprite_SetAnimationBank,
                     Field_MemberSprite, Field_PartyLoad, Sprite_ReleaseTint,
                     Sprite_LoadPalette, Field_PartyPosition, 0x533690
Field_PartyFirstFrame -> Party_ClearActive, Field_LeaderFrame, Field_MemberFrame
Field_PartyLoad   -> Party_ClearAll, Field_MemberSprite, Sprite_SetTint, Field_MemberTimers
Party_Join        -> Party_JoinReset, Member_ClearState, Field_MemberSprite
Field_MemberSprite-> Sprite_ReleaseTint, Sprite_LoadPalette
PartySet_Load     -> PartySet_LoadFirst / _LoadSecond -> PartySet_Find -> 0x536A60
                                                      -> PartySet_Select -> LoadDatFile
                     File_LoadDone, Task_Sleep
MapView_GroundAt  -> MapView_CheckHeightScale, AreaMap_Elevation
Sprite_ReleaseTint-> Tint_Release
Field_ZoneCounterRoll -> Area_ZoneAt, Rand, Party_Count, 0x535310
```

Callees that stay Capcom's and are given stand-ins in the fuzz: `0x5301F0`,
`0x535120`, `0x535240`, `0x535310` (equipment count: kind 3 is the two
accessory bytes +0x16 / +0x17 of the actor record), `0x531950`, `0x530920`,
`0x5302C0`, `0x530380`, `0x530800`, `0x5303E0`, `0x530430`, `0x530480`,
`0x533690`, `0x536A60`, `0x589330`, `Sprite_SetTint`, `Tint_Release`,
`Field_MemberFrame`, and the two frame callees `Sprite_RestoreClut` /
`Field_CopyInput` (ours, in other files).

**`0x536A60` never returns.** It pushes the three ids into a formatted string
and loops `0x517090` / `Crt_sprintf` / `Task_Sleep(1)` for ever - an on-screen
error for "this party has no set". `PartySet_Find`'s "not found" path there is
a dead end in the real game; its stand-in in the fuzz returns so that the rest
of the function can be compared.

## 2. The addresses this layer touches

`symbols.toml` names `ObjTrio`, `Field_Members`, `Field_State`,
`Sprite_Current`, `Field_ActorStates`, `MoveScript_EffectState`,
`Field_MemberCount`, `Field_InputFlags`, `Field_InputHeld`,
`Field_ScriptFlags` / `2`, `Field_Request`, `Field_Kind2X` / `Z`,
`Game_AreaNumber`, `Draw_OtSlot`, `MapView_HeightScale`, the clut strip and
`MoveScript_TintRecords`. The rest are kept as named constants in
`src/game/field_event_callees.h` rather than as `[[data]]` entries, so that no
other group of the parallel round could bind one of them twice:

| Address | What it is |
|---|---|
| `0x802D40` `ObjTrio` | the three party objects, stride `0x14C`; `+0x80..+0x123` is a copy of an actor record, `+0x124` / `+0x125` two timers, `+0x134` the zone counter, `+0x138` flags, `+0x148` the actor index (`Field_Members` is that byte of member 0) |
| `0x903A70` | the actor records, `0xA4` bytes each; `Field_ActorStates` is `+0x10` of the first. `+9` is which member the record is, `+0xB` bit 0 "joined", `+0x12`..`+0x17` the equipment `0x535310` counts |
| `0x904062` | two 3-byte party lists of member ids, `0xFF`-ended (`Party_Count`, `Field_PartyLoad`, `Party_Join`) |
| `0x90412C` | the loaded party set: a row of `0x669750`, bit 7 set when `PartySet_Select` loaded it with mode 0 |
| `0x669750` | 19 rows of three member ids, sorted inside each row and down each column - what `PartySet_Find` searches |
| `0x669738` | a byte per member id, to the sprite's `+0x2B` |
| `0x813580` | dword offsets from itself, indexed by the member's column, to the sprite bank `+0x4C` |
| `0x80D380` | 64 bytes of palette per party slot, inside `Gfx_ClutStripSource` |
| `0x660A24` | two bytes a zone: the counter's base and the most a roll adds. Zones 1..6 are 120/60, 96/48, 72/36, 24/12, 16/8, 8/4; zone 0 and 7 are 0 |
| `0x668D80` | a list of 8-byte zone records per area (`Area_ZoneAt`) |
| `0x6608D8` | two bytes per animation, `Field_LeaderAnimation`'s remap |
| `0x660B40` | 8 bytes (dx, dz) per direction / 2, the members' formation offsets |
| `0x903850` | `PartySet_Find`'s three ids; `+4` its sort's swap byte |
| `0x904EF0` | the pending jump: a flag and an index into `0x660B60` |
| `0x9045FC` | two dwords and a byte `Party_JoinReset` clears |
| `0x905B82` | a byte `Field_LeaderFrame` counts down to 0 |
| `0x905DA0` | the object `MapView_CheckHeightScale` singles out |

## 3. The functions

Sizes are the true extent (last reachable instruction, jump table included),
measured by linear disassembly of each; `analysis/attract_catalog.md`'s figures
run on to the next 16-byte boundary. "Calls" is the attract cycle's count from
that catalogue.

| Address | Name | Bytes | Calls | What it does |
|---|---|--:|--:|---|
| `0x52D8F0` | `Field_LeaderFrame` | 0x2C | 12,177 | `Sprite_RestoreClut`; the byte `0x905B82` down to 0; `Field_CopyInput`; jump through `0x660918` by the leader's state |
| `0x52D920` | `Field_LeaderStart` | 0x124 | 12 | state 0: `+0xC`..`+0x20` and `+6`..`+0xB` cleared, `+0x3E` grounded, `Field_State +0x128` = 3 and `+0x136`..`+0x138` = 0, `+0x4B` = 0xFF; then state 0xC (input flag 0x20), 0xD (area 0xBD) or state + 1 with the standing animation |
| `0x52DA50` | `Field_LeaderControl` | 0x17 | 12,165 | state 1: the sub-state through `0x660954`, then `Sprite_ScriptTick` |
| `0x52DA70` | `Field_LeaderStand` | 0x118 | 12,165 | sub-state 0: eleven tests in order, then - only while a direction is held - `0x530480`, `+7` = 0, `+0xA` = 2, `+0xB` = 0, the animation, sub-state 1 |
| `0x52FEB0` | `Field_ZoneCounterRoll` | 0x120 | 12 | the word `ObjTrio +0x134` from the zone the leader stands in (section 4) |
| `0x52FFD0` | `Area_ZoneAt` | 0x52 | 12 | the first 8-byte record of the area's list whose box holds (x, z) |
| `0x5305B0` | `Field_LeaderAnimation` | 0x43 | 12 | `0x589330(animation)`, or with input flag 0 the pair at `0x6608D8` |
| `0x531BB0` | `Party_Count` | 0x20 | 12 | entries before the first `0xFF` of a 3-byte party list, at most 3 |
| `0x5322B0` | `ScriptContext_Reset` | 0x20 | 12 | a script context to position 0, speed index 3 |
| `0x533110` | `Field_PartySetUp` | 0x388 | 12 | the area set-up's party placement (section 4) |
| `0x5334A0` | `Field_PartyFirstFrame` | 0xD3 | 12 | one frame of each member not masked by `Field_ScriptFlags` bit i |
| `0x533580` | `Field_PartyPosition` | 0x102 | 12 | each member at (x, z) or its formation offset, facing `direction`, grounded |
| `0x533760` | `Field_PendingJump` | 0x18 | 12,165 | while `0x904EF0`, jump through `0x660B60` |
| `0x533BA0` | `Field_MemberSprite` | 0x134 | 18 | `Sprite_Current` set up as member `member` in slot `slot` |
| `0x533CE0` | `Field_PartyLoad` | 0x120 | 15 | the party list's members into the three objects |
| `0x533EF0` | `Party_Join` | 0x11A | 3 | a member joins, or `Field_Request` = 6 when there are three |
| `0x534010` | `Party_JoinReset` | 0x12 | 3 | the nine bytes at `0x9045FC` |
| `0x534EC0` | `Field_MemberTimers` | 0x4B | 15 | `Field_State +0x124` = 10 and `+0x125` = 0x28 from the actor's bits 0x80 / 0x20 |
| `0x536650` | `Sprite_ClearSteps` | 0x1E | 12 | `Sprite_Current +0xC`, `+0x10`, `+0x14` = 0 |
| `0x5366A0` | `Sprite_LoadPalette` | 0x55 | 18 | 32 words of a palette to a slot and to its twin in `Gfx_ClutStrip` |
| `0x536730` | `Member_ClearState` | 0x30 | 78 | a member's `+1`..`+4` |
| `0x536760` | `Party_ClearAll` | 0x33 | 15 | all three off and cleared |
| `0x5367A0` | `Party_ClearActive` | 0x35 | 12 | the ones that are on, cleared |
| `0x5367E0` | `PartySet_Load` | 0x6C | 3 | the set's files loaded and waited for |
| `0x536850` | `PartySet_LoadFirst` | 0x38 | 3 | mode 3's file unless it is already current |
| `0x536890` | `PartySet_LoadSecond` | 0x54 | 3 | mode `mode`'s file unless `0x90412C` already says so |
| `0x5368F0` | `PartySet_Find` | 0x16B | 6 | the party set for three member ids (section 4) |
| `0x536AC0` | `PartySet_Select` | 0x94 | 2 | `0x90412C` and one of four `LoadDatFile` indices; includes its own 4-entry jump table at `0x536B44` |
| `0x572570` | `MapView_GroundAt` | 0x1F | 24 | `MapView_CheckHeightScale`, `AreaMap_Elevation`, the scale cleared |
| `0x572590` | `MapView_CheckHeightScale` | 0x2E | 24 | the scale set for the object at `0x905DA0` (its `+0xB` bit 0) or for `Field_State +0x138` bit 0 |
| `0x454DC0` | `Sprite_ReleaseTint` | 0x2B | 18 | `Tint_Release` for each of the 32 tint records whose sprite is this one |

### `Field_ZoneCounterRoll` in full

```
if (Field_ScriptFlags & 0x20)            { counter = 0; return; }
zone   = Area_ZoneAt(leader x, leader z)[4]
base   = 0x660A24[zone * 2]
if (base == 0)                           { counter = 0; return; }
if (keep && counter != 0) { if (counter < base) return; counter = base; return; }
value  = base + min(Rand() & 0x1F, 0x660A25[zone * 2])     // 8-bit
if (any member has accessory 0x15) value <<= 1             // 8-bit
if (any member has accessory 0x14) value >>= 1
counter = value
```

What the counter counts is **not established** - a step count to the next
random encounter is the obvious reading (its callers are the field's step
code, and the two accessories double and halve it), but nothing in game was
checked, so it is a guess and the name says only what the code does.

### `Field_PartySetUp` in full

```
Field_ScriptFlags &= ~0x1000
whole = Field_ScriptFlags2 & 0x8000
if (!whole && !(input & 9) && area != 0xBD && area != 0xBB)
    whole = !(input & 0x40) || Flags_Test(0x904030, 0x77)
if (!whole) {                                  // the leader alone
    n = Party_Count(0)
    for each of the n members: its actor record's word +0x10 &= ~0x20
    if any of them has bit 0x80: ObjTrio +0x124 = 10
    ObjTrio +5 = 0; Sprite_Current = ObjTrio; actor record 0 -> ObjTrio +0x80
    Field_MemberCount = 1; ObjTrio +0x148 = 0; +0x138 &= ~1; +0x48 = 0
    input bit 0: bank 0x52 / 0xCB / 0x27A by actor 0's +9 (0, 9, 7; nothing else),
                 +0x24 = 0, Field_ScriptFlags |= 0x1000
    else bit 3:  bank 0x47 or 0x1FD, +0x2A = 0, +0x24 = 0
    else bit 6:  Field_MemberSprite(actor 0's +9, 0)
    else area 0xBD: ObjTrio +0x2B = 0, +0x70 = 0
} else {
    Field_MemberCount = Party_Count(0)
    if (!(Field_ScriptFlags2 & 0x8000)) Field_PartyLoad(0)      // read again
    else for each member: on, +0x34/+0x38/+0x3C = 0, its actor record copied,
         Field_MemberSprite - and with +0x24 bit 0 also Sprite_ReleaseTint,
         +0x27 = +5 + 0x78 and its palette
}
if (input & 8)      { ObjTrio +0x34 = 0xF0000, +0x38 = 0x410000; Kind2X = 0xF0000; Kind2Z = 0x3C0000 }
else if (flags & 0x80) { Kind2X = x; Kind2Z = z }
else { (Field_ScriptFlags & 0x800 ? 0x533690 : Field_PartyPosition)(x, z, flags);
       Kind2X = ObjTrio +0x34; Kind2Z = ObjTrio +0x38 }
```

### `PartySet_Find` in full

The current set wins if it is not `0xFF` and holds every id that is not
`0xFF`. Otherwise the three ids are sorted in place at `0x903850` and matched
against the table `0x669750` column by column: for column 0, 1, 2 the rows
`[start, limit)` are walked; a cell equal to the id being matched narrows the
range to the run of equal cells and moves on to the next id, and the row whose
match is the **last** id (`matched == count - 1`) is the answer. Not found
after three columns calls `0x536A60`, which never returns.

Consequences of that shape, all kept: with `count` 0 (all three `0xFF`) the
test `matched == count - 1` is `matched == 0xFFFFFFFF` and can never hold, so
three `0xFF`s always end at `0x536A60`; `matched` indexes `0x903850`
unchecked, so a table with many equal cells can read past the three ids into
the swap byte; and the sort's swap byte at `0x903854` is written only when a
swap happens, which the fuzz compares.

## 4. Quirks kept, and candidate defects

Everything here is the original's behaviour, reproduced. None of it is a
divergence; the ones marked **candidate** look like defects and are written
down, not fixed (`CLAUDE.md` rule 2 and 6).

1. **Every dispatch index is a whole byte and unchecked** (section 1). The
   exe only ever stores small values there, but a corrupted `Sprite_Current +1`
   of 15 or more jumps through the data after `0x660918`. **candidate.**
2. **`Area_ZoneAt` has no end test.** An area list with no record that covers
   (x, z) walks off the end of the data for ever. The lists shipped end with a
   catch-all; a bad one hangs the game. **candidate.**
3. **The zone counter's arithmetic is 8-bit.** Zone 1's base is 120 and a roll
   adds up to 31: with the doubling accessory, `151 * 2` keeps 46, not 302.
   **candidate** - it is exactly the case a player with that accessory in a
   high-rate zone meets.
4. **`Field_MemberSprite`'s column is 3 when the member is not in the set
   row**, and `+0x4C` is then the bank table's fourth dword. That is how the
   game draws a member who is not in the loaded set at all.
5. **`Field_MemberCount` bounds the member loops and is read again at every
   turn**, so a stand-in (in game: a callee) that raises it above 3 walks past
   `ObjTrio`. **candidate.**
6. `Field_LeaderAnimation`'s second path returns `Sprite_Current` in eax, not
   the animation result - the pointer is loaded after the call for the
   `+0x2A` store. No caller checked reads it; ours returns its low byte.
7. `Sprite_LoadPalette`'s twin address is `0x80F580 + ((dst - 0x80B580) >> 1)
   * 2`: an **odd** destination loses its low bit, so the copy in
   `Gfx_ClutStrip` is one byte off from the copy in the source strip. All
   shipped call sites pass `0x80D380 + slot * 64`, which is even.
8. `Party_Join` marks the actor record's `+0xB` bit 0 **before** testing
   whether the party is full, so a refused join still marks it. **candidate.**
9. `Party_Count` stops at three but the lists are three bytes: a list with no
   `0xFF` in it returns 3 without reading the next list.
10. `Field_PartySetUp` reads `Field_ScriptFlags2` bit 0x8000 twice, with
    `Party_Count` in between; a change under it takes the other branch.
11. `Field_PartyFirstFrame`'s member mask is `1 << i` with x86's shift (the
    count mod 32). With at most three members it can never differ, so the fuzz
    cannot tell the two apart - said here rather than claimed as checked.
12. The original writes into its own argument slots (`Field_ZoneCounterRoll`'s
    `keep`, `PartySet_Find`'s three) and reads them back. Every caller drops
    the arguments immediately afterwards, so it is invisible; ours uses
    locals.

## 5. The fuzz and its controls

`BOF3X_SHADOW=field_event` (`src/game/field_event_fuzz.cpp`), at start-up,
before the game's C runtime - so `Rand` gets a stand-in like every other
callee. Each of the 31 originals is byte-copied with **every** call re-aimed
at a recording stand-in (the calls between the 31 included, so each function
is tested alone), the three dispatch tables are swapped for tables of numbered
stand-ins in the copy's operand and in `field_event::g` alike, and
`PartySet_Select`'s inline jump table is relocated into its copy. A round is
one function: random state with each branch's boundaries seeded and byte
arguments given stale upper bytes; theirs, the same state again, ours; then
every byte of the state, the stand-ins' log (a count, a hash of every entry,
the first 48 kept) and the result compared.

The state compared is `ObjTrio` (996 bytes), eight actor records, the two
party lists, the current set, `PartySet_Find`'s ids, the join state, the
pending jump, `Game_AreaNumber`, the leader's timer, the input and script
flags, `Field_State` and the object at `0x905DA0`, `Field_Kind2X` / `Z`,
`Field_Request`, `MoveScript_EffectState`, `Field_MemberCount`,
`MapView_HeightScale`, `Draw_OtSlot`, `Sprite_Current`, the clut strip dirty
byte, 256 bytes of palette and their twins, the 32 tint records, a scratch
object and the zone record the stand-in returns.

The stand-ins give back what the caller reads (a count, a zone record, the
ground, a party set, `File_LoadDone`) and on 18 of every 24 calls disturb something the
caller reads again after the call - `Sprite_Current`, `Field_State`, the
member count, either flags word, the input, a party list byte, the current
set, an actor record, the area, an object's byte, a tint record. That is what
makes the "read again" quirks above testable. One bound: the count the
party-count stand-in returns never exceeds the real ids at the front of the
list, since `Field_PartySetUp` indexes the 24-entry `MoveScript_EffectState`
by them and an `0xFF` would read past it (review, 2026-09-22).

**Not generated**, because the original would fault or write outside the
compared state: a `Sprite_Current` / `Field_State` outside the five objects, a
dispatch byte past its table, more than three members, a
`MoveScript_EffectState` entry above 7, a member id of 24 or more for
`Party_Join`, a member index above 2 for `Member_ClearState`, a zone list
with no matching record, and a palette destination outside the 256 bytes at
`0x80D380`.

186,000 rounds (6,000 each), 327,714 calls to the stand-ins, 0 mismatches.
Branch coverage on the original's run: the zone counter stopped by the flag
772, by a zone with no base 2,939, rolled 2,289; the set-up loaded the party
677, set a member's sprite 1,055, placed the party 2,398; the leader walked
240; the first frame ran a member's frame 3,408; the party set was not in the
table 2,376.

### The negative controls

Each is a single planted change, built and run alone
(`analysis`-free: the script lives in the agent's scratch, the list is here).
"Refused" means the fuzz counted mismatching rounds and called `Fatal`.

| Control | Function | Refused | Rounds of 6,000 |
|---|---|---|--:|
| the state read before `Field_CopyInput` | `Field_LeaderFrame` | yes | 191 |
| the timer counted down by 2 | `Field_LeaderFrame` | yes | 1,989 |
| area 0xBD tested before input bit 5 | `Field_LeaderStart` | yes | 898 |
| `Sprite_ScriptTick` before the sub-state | `Field_LeaderControl` | yes | 6,000 |
| the second "is a direction held" dropped | `Field_LeaderStand` | yes | 250 |
| the actor index taken through `MoveScript_EffectState` | `Field_LeaderStand` | yes | 394 |
| `counter < base` as `counter <= base` | `Field_ZoneCounterRoll` | **no** | 0 |
| `counter + 1 < base` | `Field_ZoneCounterRoll` | yes | 158 |
| the base + roll sum in 16 bits | `Field_ZoneCounterRoll` | yes | 4 |
| the doubling in 16 bits | `Field_ZoneCounterRoll` | yes | 110 |
| the zone index cached across `Rand` | `Field_ZoneCounterRoll` | yes | 391 |
| `x > r2` as `x >= r2` | `Area_ZoneAt` | by a fault | - |
| `x` compared against `r3` | `Area_ZoneAt` | yes | 1,936 |
| input bit 1 instead of bit 0 | `Field_LeaderAnimation` | yes | 3,022 |
| the list walked to 4 | `Party_Count` | yes | 2,059 |
| speed index 2 instead of 3 | `ScriptContext_Reset` | yes | 6,000 |
| area 0xBB dropped from the test | `Field_PartySetUp` | yes | 270 |
| `Field_ScriptFlags2` cached across `Party_Count` | `Field_PartySetUp` | yes | 41 |
| bank 0x27B instead of 0x27A | `Field_PartySetUp` | yes | 602 |
| the leader's position read before the placement | `Field_PartySetUp` | yes | 750 |
| the flags masked to a byte for `0x533690` | `Field_PartySetUp` | yes | 652 |
| the member count cached across the frames | `Field_PartyFirstFrame` | yes | 104 |
| the formation index `d` instead of `d >> 1` | `Field_PartyPosition` | yes | 2,578 |
| bit 0x40 set on the leader too | `Field_PartyPosition` | yes | 4,510 |
| the pending flag ignored | `Field_PendingJump` | yes | 3,048 |
| the column search stopped at 2 | `Field_MemberSprite` | yes | 2,909 |
| `+0x70` cleared whole instead of its low byte | `Field_MemberSprite` | yes | 6,000 |
| `Party_ClearAll` called unconditionally | `Field_PartyLoad` | yes | 2,983 |
| the tint's first two bytes swapped | `Field_PartyLoad` | yes | 1,377 |
| the actor marked after the full test | `Party_Join` | yes | 1,471 |
| the member count cached across `Member_ClearState` | `Party_Join` | yes | 110 |
| the ninth byte left alone | `Party_JoinReset` | yes | 5,978 |
| the second timer 0x29 instead of 0x28 | `Field_MemberTimers` | yes | 129 |
| `+0x14` left alone | `Sprite_ClearSteps` | yes | 6,000 |
| the twin address without the lost low bit | `Sprite_LoadPalette` | yes | 2,944 |
| `+4` left alone | `Member_ClearState` | yes | 5,971 |
| the "off" byte left alone | `Party_ClearAll` | yes | 6,000 |
| cleared whether on or not | `Party_ClearActive` | yes | 62 |
| the second wait's first test dropped | `PartySet_Load` | yes | 6,000 |
| the current set compared whole | `PartySet_LoadFirst` | yes | 1,801 |
| mode 0 without bit 7 | `PartySet_LoadSecond` | yes | 1,181 |
| `matched == count` | `PartySet_Find` | yes | 886 |
| `0xFF` ids counted in the current-set test | `PartySet_Find` | yes | 1,290 |
| the sort swapping equal ids | `PartySet_Find` | yes | 237 |
| mode 3 recording the set | `PartySet_Select` | yes | 1,004 |
| the height scale left set | `MapView_GroundAt` | yes | 4,548 |
| the `0x905DA0` path falling through | `MapView_CheckHeightScale` | yes | 915 |
| 31 tint records instead of 32 | `Sprite_ReleaseTint` | yes | 1,645 |

Two results are information rather than a gap:

- **`counter < base` as `counter <= base` is not refused**, and cannot be: the
  two differ only when `counter == base`, and then both paths leave `base` in
  the counter. It is a change that changes nothing (`HANDOFF.md`, traps).
- **`x >= r2` in `Area_ZoneAt` is refused by a fault, not a count**: with the
  seeded coordinate 255 and a catch-all record ending at 255 the walk runs off
  the list, which is quirk 2 above showing itself. The comparison it was meant
  to test is covered by the `x` against `r3` control instead.

The rounds column counts the mismatching rounds of that function's 6,000 in
the run the control was planted in.

## 6. What the attract cycle does not reach

The attract sequence presses nothing, so within this file it never reaches:

- **The 13 other leader states** (`0x660918` entries 2..14) and the two other
  sub-states (`0x660954` 1 and 2): walking, the ladder, the boat and whatever
  else they are. Reaching them needs a direction held (sub-state 1) or a
  script that sets the state - the `field_view` recipe would enter the first
  of them.
- **The four pending jumps** (`0x660B60`): `Field_PendingJump` runs 12,165
  times in a cycle and returns at once every time, because `0x904EF0` is never
  set. A map transition sets it - an input recipe that walks through a door
  would.
- **`Party_Join` beyond its three calls** and the full-party path
  (`Field_Request` = 6): a fourth member joining. The script ops at
  `0x519890` are the callers.
- **`PartySet_Select`'s modes 1 and 2** (only mode 3 and mode 0 run in the
  attract: `0x536AC0` is called twice).
- **`Field_ZoneCounterRoll`'s roll** - the attract calls it 12 times, all in
  areas where the leader stands still; the doubling and halving accessories
  are a save-file matter.
- Areas other than the attract's: `Field_PartySetUp`'s `0xBB` and `0xBD`
  branches are reached (the attract visits `0xBD`), but the `input & 1` and
  `input & 8` set-ups are the naming and options screens the port dropped.

## 7. For the batch check

The 31 ranges, for `entries_logic.txt` (address, size in bytes):

```
0x52D8F0 0x2C    0x52D920 0x124   0x52DA50 0x17    0x52DA70 0x118
0x52FEB0 0x120   0x52FFD0 0x52    0x5305B0 0x43    0x531BB0 0x20
0x5322B0 0x20    0x533110 0x388   0x5334A0 0xD3    0x533580 0x102
0x533760 0x18    0x533BA0 0x134   0x533CE0 0x120   0x533EF0 0x11A
0x534010 0x12    0x534EC0 0x4B    0x536650 0x1E    0x5366A0 0x55
0x536730 0x30    0x536760 0x33    0x5367A0 0x35    0x5367E0 0x6C
0x536850 0x38    0x536890 0x54    0x5368F0 0x16B   0x536AC0 0x94
0x572570 0x1F    0x572590 0x2E    0x454DC0 0x2B
```

`0x536AC0`'s size includes its inline jump table (`0x536B44`, 16 bytes);
`0x52D8F0`, `0x52DA50` and `0x533760` end at a dispatch through a table that
is outside them.

What the live batch should look at, beyond the frame hash: the attract's
field scenes are entirely this file's leader frame (12,165 of them), so any
difference shows as the leader standing wrong or the camera misplaced; the
party set loader runs at the scene changes, so a wrong `PartySet_Find` shows
as the wrong sprites or the error loop `0x536A60` on screen.
