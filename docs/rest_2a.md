# Group R2A: the band between the field core and chapter 0

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, on the
round branch's tip `fa583bc`. **22 functions ours** (`src/game/rest_2a.cpp`,
declarations in `src/game/rest_2a.h`, the cells it names by address in
`src/game/rest_2a_callees.h`, shadow name `rest_2a`): the cut's 19 rows for
R2A (`analysis/round14_cut.tsv`) and **three starts no list had**
(`0x537760`, `0x537B10`, `0x537CE0`), each read to its last instruction with
capstone and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
132,000 rounds, 0 mismatches. 71 controls planted one at a time: 70
refused, 1 an equivalent mutant with its near variant refused (section 6).
No recorded route enters any of the 22 (section 9).

**The band is not scenario code**, whatever the cut's classes said (18 of
its 19 rows `hypothesis`, 11 "Boot: Scena15" hidden in
`Scena15_RecordWord`'s catalog extent). It is four field-engine helpers and
one small subsystem:

- `Mode11_ListedSpriteScreens` (`0x5372E0`), a pass of game mode 11's field
  frame over the sprites;
- `Char_GainHp` / `Char_LoseAp` (`0x5373F0`, `0x537500`), two
  character-record helpers beside SX's `Char_LoseHp` (`0x537480`);
- `Area_ObjectHandler` (`0x537540`, named 2026-09-22, ours now) and the
  **four object handlers its fallback table names** -
  `Area_ObjectFallbacks` (`0x660CA0`, 11 entries) - with their states and
  the two effect spawns they share: 18 functions from `0x5375A0` to
  `0x537F1B`.

`Scena15_RecordWord` (`0x537580`, 0x14 bytes, SC15's) sits in the middle and
is untouched. The catalog's extent for it ran on to `0x537DDF`, which is why
eleven of this group's starts were "hidden" in it; its `entries_logic.txt`
line was already the right `14`. What the objects are in play is not read
here; the names say what the code does ("linked object": what
`Field_ObjectLinked` runs instead of an object's kind).

## 1. What each function does

"Sprite_Current" is the object whose handler runs; `+4` is its state byte
(the four handlers dispatch on it), `+7` bit 3 "do not turn", `+8` its
direction, `+0xB` the effect record it watches, `+0x34..+0x3C` its
position, `+0x5D..+0x5F` three colour bytes. "The member" is
`Field_ActiveMember` (a `Sprite_Objects` record): `+0x80` bit 0 its context
bit, `+0xA0` its area-handler index (low 7 bits; 0x7F none) and, bit 7,
the once-only mark the rolls set.

| Address | Name | What it does |
|---|---|---|
| `0x5372E0` | `Mode11_ListedSpriteScreens` | For each of the 30 `Sprite_Objects` records: `Sprite_Current` = it; live (`+0` bit 0): type `+6` not 9 - draw slot `+0x29` = 4 with `+0x24` bit 4, else `Draw_OtSlot`; type 9 with `Draw_OtSlot` 4 - nothing more. Then type 0xA: `Sprite_UpdateScreenA`. Else the bank list's record whose `+7` equals the sprite's u16 `+0x2C` (the list `Sprite_SetAnimationBank` searches: count byte `0x8C3580`, 8-byte records at `[0x7E0880]`); its bank word in `Mode11_ScreenBanks`: `Sprite_UpdateScreen`. `Mode11_FieldFrame`'s sixth call. |
| `0x5373F0` | `Char_GainHp` | The member byte's record (through `MoveScript_EffectState`): HP below max - HP + amount as a u16, held at max; then the actor `Field_State +0x148` names, HP above a quarter: its `+0x10` and `Field_State +0x90` lose 0x2000. HP at max: nothing at all. |
| `0x537500` | `Char_LoseAp` | AP `+0x1A` minus the amount while more, else 1; answers what it took. `Char_LoseHp` without the actor test. |
| `0x537540` | `Area_ObjectHandler` | Tail jmp: member `+0xA0 & 0x7F` not 0x7F - `Area_Descriptors[Game_AreaNumber] +0x3C` at that index; else `Area_ObjectFallbacks[(short)n]`. |
| `0x5375A0` | `LinkedObjectA_Run` | Fallbacks 0, 7, 9: `call [esp + 4 * +4]` over a five-entry stack table: Roll, Show2, Show5, EndAfterEffect, EndAfterMessage. |
| `0x5375E0` | `LinkedObjectA_Roll` | Rand & 0xF; turned away from the leader (`(0x802D48 ^ 0xFC) & 7`); `Field_ScriptFlags` \|= 0x100. Marked already: SpawnEffect19(1) for 8..15, (3) else, state 3. Else marked and by the roll: 0..3 SpawnEffect19(1), state 3; 4..6 SpawnEffect19(1), `Zenny_Add(2, 0)`, SpawnEffect32(the member's sprite index), state 1; 7 the same with 5, state 2; 8..15 SpawnEffect19(3), state 3. |
| `0x537760` | `LinkedObject_Show2` | The watched effect record free: `Crt_sprintf(Text_Records, Area08_MessageFormat, 2)`, `Msg_OpenSystem(5)`, `Field_Request` 2, state 4. |
| `0x5377B0` | `LinkedObject_Show5` | The same with 5. |
| `0x537800` | `LinkedObject_EndAfterEffect` | The watched record free: the event's end - turned to `+8` (unless `+7` bit 3), member `+0x80` bit 0 cleared, `Field_ScriptFlags` &= 0xFEFF, state 0. |
| `0x537850` | `LinkedObject_EndAfterMessage` | `Field_Request` not 2: the event's end. |
| `0x5378A0` | `LinkedObjectB_Run` | Fallbacks 1, 8: two states, Roll and EndAfterEffect. |
| `0x5378D0` | `LinkedObjectB_Roll` | Rand & 0xF, turned away; SpawnEffect19(2) above 6, else (3); `Field_ScriptFlags` \|= 0x100; state 1. No mark, no zenny. |
| `0x537950` | `LinkedObjectC_Run` | Fallbacks 3, 6: A's table with C's roll. |
| `0x537990` | `LinkedObjectC_Roll` | A's roll with the second sub-kind 5 where A has 3. |
| `0x537B10` | `LinkedObjectD_Run` | Fallback 5: six states - Roll, ShowAmount three times, WaitMessage, ColourStep. |
| `0x537B50` | `LinkedObjectD_Roll` | A's roll body with `Zenny_Add(1)` to state 2 and `(10)` to state 3, every other path state 1; then `+0x5E`, `+0x5F`, `+0x5D` = 0xB0. |
| `0x537CE0` | `LinkedObjectD_ShowAmount` | The watched record free: `LinkedObjectD_Amounts[+4]` not 0 - the message with it, state 4; 0 - state 5. |
| `0x537D50` | `LinkedObjectD_WaitMessage` | `Field_Request` not 2: state 5. |
| `0x537D70` | `LinkedObjectD_ColourStep` | `+0x5D` not 0: `+0x5D`, `+0x5F`, `+0x5E` each += 0x10 (0xB0 reaches 0 in five frames); 0: the event's end. |
| `0x537DE0` | `LinkedObject_SpawnEffect19` | `Effect_FindFree` into `+0xB`; not 0xFF: the record `+0` 1, `+5` 0x19, `+6` the argument's byte, `+0xC` 0, `+0x10` = (s8) `LinkedObject_EffectRise` + 0x28, `+0x34..+0x3C` the object's position (read after that call). |
| `0x537EA0` | `LinkedObject_EffectRise` | (s8)(`+0x30` - the leader's `+0x30`) / 28, four times that when negative; al. |
| `0x537ED0` | `LinkedObject_SpawnEffect32` | `Effect_FindFree` into `+0xB`; not 0xFF: `+0` 1, `+5` 0x32, `+6` the argument's byte. |

**Re-reads kept** (each a control in section 6): the rolls read the member
pointer after the turn's call and again after `Zenny_Add`; the ends read the
member after the turn and `Sprite_Current` after that; the show states read
`Sprite_Current` after `Msg_OpenSystem`; `SpawnEffect19` re-reads the effect
index through the *old* sprite's `+0xB` after `LinkedObject_EffectRise` and
the position from the *new* `Sprite_Current`.

**Dead test, as the original has it:** before each roll's turn the code
compares the object's `+8` with the leader's and jumps on `setne; xor 4`,
which is never 0 - the turn is unconditional (unless `+7` bit 3).

## 2. Starts, extents, the cut

- **Three starts no list had** (the band tool's "code no list has"), each
  its own function with its own `ret`, reached by address: `0x537760`
  (A's and C's stack immediates, state 1), `0x537B10` (`Area_ObjectFallbacks[5]`),
  `0x537CE0` (D's stack immediate, states 1..3). The cut's sizes for
  `0x5375E0`, `0x537990` and `0x537B50` (464, 448, 512) ran on over them.
- **Jump tables inside code**: each roll ends with an 8-entry switch table
  (`0x537738`, `0x537AE8`, `0x537CC0`), part of the function's extent (the
  clone relocates it). No start is a case, a shared tail or data; none
  dropped.
- **Extents**: the band tool's (`--byte-tables`) are right for all 22; the
  cut's are padding past them except the three above.
- **No code in the band left untaken** besides `Scena15_RecordWord`
  (`0x537580`, ours since round ten). `0x537F20` (`Scena00_Frame`) starts the
  chapter bank after it.

## 3. The tables

| Table | Name | Count | Reader |
|---|---|--:|---|
| `0x660CA0` | `Area_ObjectFallbacks` (`[[data]]`) | 11 | `Area_ObjectHandler` by `(short)n`, unchecked; eleven code pointers up to `0x660CCC` (ours aborts outside 0..10) |
| `0x660CCC` | `LinkedObjectD_Amounts` (`[[data]]`) | 4 | `LinkedObjectD_ShowAmount` by `+4`, unchecked; only 1..3 reach it (ours aborts past 3) |
| `0x660C3C` | `Mode11_ScreenBanks` (`[[data]]`) | 31 | `Mode11_ListedSpriteScreens` to its 0xFFFF (30 words and the terminator, counted by hand) |
| stack | the four Runs' tables | 5, 2, 5, 6 | `call [esp + 4 * +4]`, unchecked (ours aborts past the count) |

`Area_ObjectFallbacks` holds `LinkedObjectA_Run` at 0, 7 and 9,
`LinkedObjectB_Run` at 1 and 8, `LinkedObjectC_Run` at 3 and 6,
`LinkedObjectD_Run` at 5, and AR1F's `Area74_ClearMemberBit0` at 2, 4 and 10.
The index is the caller's argument: `Field_ObjectLinked` passes the byte
`0x802DC9`.

## 4. The fuzz (`rest_2a_fuzz.cpp`)

Field mode, 6,000 rounds a function (132,000), 0 mismatches in this
worktree. Shapes: kSprite for the handlers, states, dispatchers and
`Mode11_ListedSpriteScreens`; kSprite with `ret_mask` 0xFF for
`LinkedObject_EffectRise`; kCall for `Char_GainHp`, `Char_LoseAp` (`ret_mask`
the whole eax), `Area_ObjectHandler` and the two spawns.

- **Tables**: the dispatchers' stack tables are the clones' immediates
  (handler recorders); `Area_ObjectFallbacks` and `Area74_Handlers` are
  DataTables. `Area_ObjectHandler` runs with `Game_AreaNumber` 74 always (the
  self-test checks that area 74's descriptor `+0x3C` is `Area74_Handlers`),
  so its area path jumps only through recorders; the member's `+0xA0` is
  0x7F or 0..6, either with bit 7.
- **Regions** beyond field mode's: `Field_ActiveMember`'s cell, the bank
  list (count, 17 records, the pointer), `Draw_OtSlot`'s dword,
  `Text_Records`' first 16 bytes, `CharacterRecords` from `0x903A94` (the
  style cells hold the first 0x24 bytes) to `Cond_Flags`.
- **Seeds**: the bank list (count 0..16, records' bank words mostly from
  `Mode11_ScreenBanks`, the sprites' `+0x2C` mostly a record's `+7`); the
  first four sprites and a quarter of the rest (live bit, types 9 / 0xA,
  `+7` bit 3, `+0xB` 0..19 or 0xFF a sixth of the time, `+0x24` bit 4,
  `+0x5D` at 0 / 0x10 / 0xB0 / 0xF0, `+0xA0`); the eight character records'
  HP / max / AP at their edges; `Field_State +0x148` below 8; the member a
  sprite record (`Sprite_Current` a third of the time); `Field_Request` 2
  half the time; `Draw_OtSlot` 4 half the time; each watched effect record's
  `+0` in use or not; a dispatcher's `+4` below its table, `ShowAmount`'s
  0..3; Rand's hint at the rolls' switch edges.
- **Arguments**: the record helpers' member byte below
  `MoveScript_EffectState`'s 24, the amount at 0, 1, 2, 0xFFFF, 0x10000, ...;
  `Area_ObjectHandler`'s n 0..10 under random upper bytes.
- **Callees re-listed**: `Effect_FindFree` (0xFF a quarter, else 0..19),
  `Sprite_UpdateScreen` / `_A` and `Sprite_FaceDirection` (each logs
  `Sprite_Current`; the turn's argument masked to its byte - the originals
  push whole registers), the group's own spawns (the argument's byte) and
  `LinkedObject_EffectRise`, `Zenny_Add`, `Rand`.
- **Disturbance** (the group's case, from its hash): `Sprite_Current`'s `+7`,
  `+8`, `+0xB` (0..19 only: the spawn writes the record it names after the
  call), position, colour bytes; the member pointer and its mark; an effect
  record's `+0`; `Field_Request`; `Draw_OtSlot`; a bank record's `+7`.

**What nothing reached**: nothing of the 22 is unreached by the fuzz
(coverage line: every handler and callee called). The harness names each
clone "outside the field runs" in the log (the band lies between them): a
note, not a refusal.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **L1. Effect record 255.** The spawns store `Effect_FindFree`'s 0xFF in
  `+0xB` unchecked, and `LinkedObject_Show2`, `_Show5`, `_EndAfterEffect`
  and `LinkedObjectD_ShowAmount` then test `Effect_Objects + 0xFF * 0x80`
  (`0x7E9160`, inside `Gfx_PacketPools`) for "in use". Play reaches it when
  all 20 effect records are taken at the roll (the zenny paths spawn twice:
  the second spawn's 0xFF overwrites the first's index). If that byte's bit 0
  is set the object waits in its state, `Field_ScriptFlags` 0x100 still set,
  until the draw pool's byte changes. Ours reads the byte in place, as R1D's
  `PartyAction_WaitEffectDone` does (`docs/rest_1d.md` L1); any other index
  past 19 (no code of this band stores one) aborts.
- **L2. One record past the bank list.** `Mode11_ListedSpriteScreens`, when
  no record's `+7` matches the sprite's `+0x2C`, reads the bank word of the
  record at the count (one past the list) and tests it against
  `Mode11_ScreenBanks`. Ours reads it in place.
- **L3. Unbounded indexes** (ours aborts; no state or caller writes such a
  value): each Run's state byte past its stack table (the original calls
  through its own saved registers and return address); `Area_ObjectHandler`'s
  `(short)n` outside 0..10 (`Field_ObjectLinked` passes a byte: 11..255
  would jump through `LinkedObjectD_Amounts` and the tables after it);
  `Game_AreaNumber` past `Area_Descriptors`' 200; `LinkedObjectD_Amounts` by a
  state past 3. The area path's own index (`+0xA0 & 0x7F`) has no count the
  code knows: read in place by both.
- **L4. Ranges.** `Char_GainHp`'s sum wraps at 0x10000 before the clamp
  (`Field_EquipTick` adds 1: unreachable below a maximum of 0xFFFF);
  `Char_LoseAp` at AP 0 takes 0xFFFFFFFF and leaves AP 1, as `Char_LoseHp`.
  The `MoveScript_EffectState` index is the member byte unchecked past 24,
  as `Char_LoseHp`. `Area_ObjectHandler` hands the handler the caller's
  dword where ours passes the u16 zero-extended (every handler read here is
  void and reads no argument).

**Needs a ledger entry:** none. No read of memory the original never wrote
reaches a draw or a decision beyond L1, which is the same read R1D's L1
reports and play reaches; the owner's word on it is the round's debt 3.

## 6. Controls

71 planted by `scratchpad/r2a/controls.py` (plant on a unique anchor,
rebuild, run `BOF3X_R2A_ONLY=<clone>`, restore, rebuild), all in
`rest_2a.cpp`:

| Id | Function | Plant | Result |
|---|---|---|---|
| C01..C08 | `Mode11_ListedSpriteScreens` | type 9 test as 8; `+0x24` bit 4 as 5; `Draw_OtSlot` 4 as 5; type 0xA as 0xB; one record less searched; bank word from `+2`; the list's first word skipped; live bit 0 as 1 | refused (4157, 2858, 1560, 4163, 2278, 4746, 254, 6000) |
| C67 | | the bank byte word `+0x2C` read as a byte | refused (967) |
| C09..C12 | `Char_GainHp` | `>=` max as `>`; the sum unwrapped; quarter as half; `Field_State +0x90` not cleared | refused (641, 1012, 270, 1051) |
| C13, C14, C66 | `Char_LoseAp` | `<=` as `<`; the answer masked to 16 bits; member byte ^ 1 | refused (408, 2104, 3076) |
| C15 | `Area_ObjectHandler` | the 0x7F test on the low 6 bits | **not refused: equivalent** - it differs only for `+0xA0 & 0x7F` of 0x3F, which in the area path indexes area 74's table past its 7 entries (the original jumps through non-table memory); no input both sides survive tells it apart |
| C70 | | near variant: index 6 sent to the fallback path | refused (455) |
| C16, C17 | | the area index halved; the fallback index halved | refused (2498, 2243) |
| C18, C19, C21 | A / B / D Run | two states of the stack table swapped | refused (2335, 6000, 1962) |
| C20 | C Run | state 0 as A's roll | stopped by the harness's Fatal (ours called a handler no C clone registers) |
| C71 | | re-plant: state 0 as `LinkedObject_Show2` | refused (1222) |
| C65 | the Runs | state n dispatched as n - 1 | refused (15608) |
| C22..C36 | the rolls | the turn guard bit; the turn's xor; the script flag bit; the mark bit; the marked path's edge; the mark written; each switch edge; the zenny amount's edge; the member read before `Zenny_Add`; the index divisor; D's colour byte; A's, C's, D's parameters | refused (2990, 3030, 4506, 2944, 278, 1522, 211, 259, 424, **2**, 666, 6000, 5104, 2971, 259) |
| C48, C49, C68 | `LinkedObjectB_Roll` | the edge 6 as 5; state 2; `Sprite_Current` read before the spawn | refused (317, 6000, 183) |
| C37..C42 | the show states | amount 3; in-use bit 1; `Field_Request` 3; `Msg_OpenSystem(6)`; `Sprite_Current` read before the calls; record 255 read as record 0 | refused (3566, 2455, 7084, 7084, 423, 499) |
| C43..C47 | the ends | the turn by `+9`; member bit 1 cleared too; the script flag kept; the member read before the turn; `Field_Request` 3 | refused (1772, 1821, 588, **2**, 3569) |
| C50..C54 | D's states | amount 0 to state 4; the amount by state ^ 1; wait to state 4; `+0x5F` by 0x20; the end at 0x10 | refused (1804, 1734, 3623, 4794, 2374) |
| C55..C60 | `LinkedObject_SpawnEffect19` | kind 0x18; + 0x27; the rise zero-extended; the position read before the call; the index re-read from the new sprite; `+0x8` for `+0xC` | refused (4543, 4543, 2311, 148, 143, 4543) |
| C61, C62 | `LinkedObject_EffectRise` | / 27; << 1 | refused (482, 2399) |
| C63, C64, C69 | `LinkedObject_SpawnEffect32` | kind 0x31; the sub-kind >> 1; `+0` 3 | refused (4543, 4524, 4543) |

C31 and C46 (a pointer read moved before a call) are refused by 2 rounds
each: the disturbance moves the member pointer in one case of eleven after a
third of the calls. Refused; a heavier weighting would make them louder.

**Under the repaired disturbance** (2026-10-05, round fourteen's review
item 1: the group's case drawn by `sh::DisturbCase(h, 11)`, so the cases 0,
3, 6 and 9 - `+7`, the position, the member's mark, `Draw_OtSlot` - run for
the first time). The 71 controls re-run on `451edeb`, same script: 69
refused, C15 not refused (the equivalent mutant, as before), C20 stopped by
the harness's Fatal (as before). Counts moved by at most 39 rounds; C46 went
2 -> 4, C31 stayed at 2 (the member pointer's case is now one of eleven,
where before it was one of the eight values `h % 12` could take there). Eight new controls, one or more on each cell
a formerly dead case moves, each a re-read after a call replaced by the
value read before it *only while the pointer is unchanged* (so the
harness's own move of `Sprite_Current` or the member cannot refuse it: only
the group's case can). All eight refused (rounds of 6,000; "Roll" runs the
four rolls, 24,000):

| Id | Function | Plant | Result |
|---|---|---|---|
| C72 | the rolls (`RollBody`) | case 0: the turn guard's `+7` read before `Rand` | refused (31 of 24,000) |
| C73 | `LinkedObjectB_Roll` | case 0: the turn guard's `+7` read before `Rand` | refused (6) |
| C74..C76 | `LinkedObject_SpawnEffect19` | case 3: the position's `+0x34`, `+0x38`, `+0x3C` read before `LinkedObject_EffectRise` | refused (9, 6, 2) |
| C77 | the rolls (`RollBody`) | case 6: the member's mark `+0xA0` read before `Rand` and the turn | refused (93 of 24,000) |
| C78, C79 | `Mode11_ListedSpriteScreens` | case 9: `Draw_OtSlot` read once before the loop, for the draw slot; for type 9's test at 4 | refused (140, 31) |

The fuzz is unchanged. C76 (2 rounds) is the thinnest: case 3 picks one of
the three dwords, and the spawn's position copy is the only re-read of it.

## 7. Calls across groups

- **Out**: none to another group of this round. Every callee is ours or
  Capcom's CRT, called by name: `Rand`, `Crt_sprintf` (Capcom's),
  `Sprite_FaceDirection`, `Sprite_UpdateScreen`, `Sprite_UpdateScreenA`,
  `Effect_FindFree`, `Msg_OpenSystem`, `Zenny_Add`.
- **In** (from outside the group, all ours): `Mode11_FieldFrame` (FC3) calls
  `Mode11_ListedSpriteScreens`; `Field_EquipTick` (FE2's `event_objs`) calls
  `Char_GainHp` three times; `Field_FloorHurt` (FE2) calls `Char_LoseAp`;
  `Field_ObjectLinked` (`object_kinds.cpp`) calls `Area_ObjectHandler` by
  name. The four Runs are reached only through `Area_ObjectFallbacks`; the
  states only through the Runs' stack immediates.
- **Harness rows** (`scenario_harness.cpp`, not edited): `{"0x5372E0", ...}`
  (FC3's raw row: void, no arguments - matches) and `{"0x537500", ..., 2,
  {kU16, kU8}, kGarbage}` (FE2's: the amount's word and the member byte,
  the answer unread by FE2 - matches what the function reads). Both still
  work by their raw keys; the fold can move them to `FIELD_OURS`.

## 8. The rebinding

Rebound (the value unchanged, so the fuzz keys stand):
`field_c3_callees.h` `kPartyScreens` = `bof3::addr::Mode11_ListedSpriteScreens`;
`event_objs_callees.h` `kHpGain` = `bof3::addr::Char_GainHp`;
`field_e2_callees.h` `kHpLoss2` = `bof3::addr::Char_LoseAp`.
Left raw on purpose: the fuzz files' keys (`field_c3_fuzz.cpp`'s and
`field_e2_fuzz.cpp`'s call-site tables, `FE2_AT(537500)`), `object_kinds.cpp`'s
stand-in switch `case 0x537540` and its clone row (call-site targets), the
two harness rows above, and the comment in `event_objs_callees.h`'s stub
struct.

## 9. The live route

The cut's reach column is empty for all 19 rows, and no trace under
`analysis/calltrace` (`*/bof3x.callcounts.tsv`) has an entry for any of the
22. The owner's recipes named for wave two (`campingFishing.txt`,
`menu_screens.txt`, `shop.txt`) enter other groups' bands. **Fuzz only.**
`Mode11_ListedSpriteScreens` runs every frame of game mode 11, and the
object handlers whenever a linked object without an area handler is
touched; a route through either would be the live check.

## 10. Self-tests and the entry list

- `BOF3X_SHADOW=rest_2a`: exit 0, 132,000 rounds, 0 mismatches (this
  worktree).
- `BOF3X_SHADOW='*'` in this worktree: exit 0 (1,114 s), and with
  `BOF3X_WIDE=1` exit 0 (1,196 s); no mismatch in either log, `rest_2a`'s
  line the same as alone. Neither died silently.
- `tools/ledger_check.py`: 0 errors.
- `analysis/calltrace/entries_logic.txt` (main checkout): the 15 starts that
  had no line appended with the extents read. `0x5372E0`, `0x5373F0`,
  `0x537500`, `0x537540`, `0x537DE0`, `0x537EA0` already had theirs;
  `00537ED0 50` is padding past the 0x4C read, left.
- Nothing in `DIVERGENCE.md`, `cheats.cpp`, `widescreen.cpp` or `labels.cpp`
  names an address of the band, and no full-frame fill is drawn in it.
