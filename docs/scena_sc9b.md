# Chapter 9's tail and chapter 10: 0x557170..0x55C040

**Status:** IN PROGRESS (2026-09-28) - 63 functions ours
(`src/game/scena_sc9b.cpp`, shadow name `scena_sc9b`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md)):
0 mismatches in 378,000 (6,000 a function) rounds; 165 of 165 negative controls refused, at least one per function. Fuzz only: no recorded route
reaches chapter 9 or 10 (section 8).

Group SC9b of round ten's third wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §9,
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3), one stage:
read, written, fuzzed and controlled against the harness as merged at
`e4882fb`. No harness was edited.

## 1. The block, the two chapters' roots, and the starts

The band holds the tail of chapter 9 (`0x557170..0x558140`, 24 functions)
and all of chapter 10 (`0x558140..0x55C040`, 39). Group SC9a's
[`scena_sc9a.md`](scena_sc9a.md) has chapter 9's first block; its tables
reach into this band, and chapter 10's vtable and tables are its own.

| Table | Entry | Target | Here |
|---|---|---|---|
| chapter 9's vtable `0x6613E8` | slot 2, the step hook | `0x557270` Scena09_StepHook | yes |
| | slot 4, the cell hook | `0x557A20` Scena09_CellHook | yes |
| `Scena09_Objects` `0x661450` (SC9a's, read in place) | 5, 6 | `0x55BAC0`, `0x55BAE0` (chapter 10's objects 11, 12) | yes |
| | 7..11, 13..15 | Scena09_Object07..11, 13..15 | yes |
| `Scena09_CellHooks` `0x6614D8` (new) | 0..13 | Scena09_Cell00..Cell13 | yes |
| chapter 10's vtable `0x661510` (0x662C80 entry 10) | slot 0 | `0x558140` Scena10_Frame | yes |
| | slot 1 | `0x55B6D0` Scena10_ObjectTrigger | yes |
| | slot 2, 3 | `0x55BB00` Scena10_StepHook, `0x55BE70` Scena10_ArriveHook | yes |
| | slot 4 | 0 (no cell hook) | - |
| `Scena10_States` `0x661588` | 0 | `0x5646B0` (group SC13's block; chapters 9 and 12's state 0 too) | no |
| | 1, 2 | Scena10_EnterArea, Scena10_Run | yes |
| `Scena10_Runs` `0x661594` | 0 | `0x437CC0`, a bare ret | - |
| | 1..7, 10..13 | the runs | yes |
| | 8, 9 | **0** (a jump to address 0) | - |
| `Scena10_Objects` `0x661614` | 0..9, 11, 12 | the object handlers | yes |
| | 10 | **0** | - |
| call table A `0x65F7B4`, B `0x65F7CC` | 6 + 4 entries | `0x51A370`.. (group CALLS's, ours) | no |
| area 75's descriptor `0x60AA98` +0x34 [10, 11] / +0x3C [8, 9]; area 86's `0x611C38` +0x3C [0, 1] | | `0x55B8B0`, `0x55B960` | yes |

**Neither chapter reads `Cond_ByteFA`** (a scan of the band's memory
operands finds no `0x8034E0`); both read the flag row through `0x929ED0`,
which `Scenario_Start` points at `Cond_Flags + 8 * chapter`. So the fuzz
runs **two `Run` calls under the one shadow name** - chapter 9's tail with
the chapter byte 9, chapter 10 with 10 - so that each block's row pointer is
its chapter's, as SC3 did for chapters 3 and 4. The two shared object
handlers and the two area handlers go with chapter 10 (they lie in its
block).

**The tables' bounds, not the walk's closure.** `scenario_roots.py
--chapter 9` lists every chapter-10 function in chapter 9's closure: the
walk reads `Scena09_Objects` entries 5 and 6 (`0x55BAC0`, `0x55BAE0`, which
are chapter 10's too) and follows the immediates. The code of chapter 9's
tail reaches nothing in chapter 10 except those two; the chapters share only
them and state 0. The walk's "immediate `0x520000` pushed at `0x55BE2B`"
(SC9a's §1) is Scena10_StepHook's `cmp ebp, 0x520000`, an x compare, not a
code pointer.

**The starts.** `tools/scenario_rows.py --unit SC9b` lists 63, 0x4CC6 bytes,
0 ours, 6 not walked; this reading agrees function for function, extent for
extent (every gap between one function's end and the next start is `0x90`
padding; the last ends at `0x55C040`, chapter 11's frame). Against the three
start lists (`pc_funcs` 6 and `pc_hidden` 49 in the band):

- **3 dropped**, `pc_hidden` starts that are jump-table cases:
  `0x5589A0` and `0x558CC0` (Scena10_Run2's steps 1 and 14) and
  `0x559AD0` (Scena10_Run5's step 0xA). `pairs_propagated.json` pairs
  `0x559AD0` with PSX `0x801FA0BC` ("callers") - **wrong**: it is a case,
  not a function (the fourth such pair after `0x5455A0`, `0x54AAD0`,
  `0x551E40`, `0x553070`).
- **11 added** that no list had, all real functions: seven runs and
  helpers the tool adds from the tables and direct calls (`0x558960`,
  `0x558FC0`, `0x5594D0`, `0x559500`, `0x559B20`, `0x55AAD0`, `0x55AB00`),
  and four more of the six below.
- **The six the walk did not reach** (the tool's "not walked"):
  - `0x55AF20`, `0x55AF60`, `0x55AF90`, `0x55B190` - Scena10_Runs entries
    10..13. The walk stopped at the two 0 entries 8 and 9; the dispatcher's
    `jmp [0x661594 + run * 4]` has no bound, and runs 10..13 are started by
    the band itself (Scena10_Object11 run 0xA, Scena10_ArriveHook 0xC,
    Scena10_StepHook / Object12 0xB and 0xD). Real chapter code.
  - `0x55B8B0`, `0x55B960` - named by no chapter table at all: area 75's
    and area 86's descriptor tables (above). They search chapter 10's
    pickup records (`Scena10_Pickups`), so they are chapter 10's code that
    two areas' handler tables call; the scenario tools read chapter tables
    only. Taken here as `kState` handlers (void, no arguments).

`pairs_propagated.json`'s other three pairs in the band are consistent with
this reading and cited as hypotheses in `symbols.toml`: `0x558150`
(EnterArea, PSX `0x801F74A0`), `0x558550` (`0x801F7A50`), `0x55B790`
(`0x801FB5C0`). The sibling's `scenario_records.toml` gives SCENA09
`vtable[2]` `0x801FC91C`, `vtable[4]` `0x801FD280` and SCENA10 `vtable[0..3]`
`0x801F7F50`, `0x801FD464`, `0x801FDB14`, `0x801FE00C` by table position
(not read).

## 2. The tables

Named as `[[data]]` in `symbols.toml`, every one read in place:

| Address | Name | Size | Read by | Holds |
|---|---|--:|---|---|
| `0x661490` | `Scena09_Cells` | 14 x 5 bytes | Scena09_CellHook, handed to `0x56D800` | the cell records |
| `0x6614D8` | `Scena09_CellHooks` | 14 | Scena09_CellHook's tail jump | Scena09_Cell00..13, in order |
| `0x661528` | `Scena10_Pickups` | 16 x 6 bytes | Scena10_Object05, Scena10_PickupPose | a flag index (less 0x20), an item (less 0x4E), the x and z words |
| `0x661588` | `Scena10_States` | 3 | Scena10_Frame | `0x5646B0`, EnterArea, Run |
| `0x661594` | `Scena10_Runs` | 14 | Scena10_Run | ret, Run1..Run7, 0, 0, Run10..Run13 |
| `0x6615D0` | `Scena10_SpriteScript` | 20 bytes | Scena10_SpriteOp -> `EventOp_0x` | event-op bytes |
| `0x6615E4` | `Scena10_ShakeSteps` | 4 signed bytes | Scena10_Shake | by `Frame_Counter & 3` |
| `0x6615E8` / `0x6615F4` / `0x661608` | `Scena10_MsgKindsA` / `B` / `C` | 4 / 5 / 3 member bytes, then as many message words | Scena10_MsgByMemberA / B / C | |
| `0x661614` | `Scena10_Objects` | 13 | Scena10_ObjectTrigger | Object00..09, 0, Object11, Object12 |
| `0x6BC734` | `Scena10_Shaking` | byte | Scena10_Shake and the runs | the camera shake's switch |
| `0x6BC735` | `Scena10_Slot` | byte | the area entry, runs 1, 3, 4, 6, 7 | chapter 10's own effect slot |

Chapter 9's tables end at `Scena10_Hooks`: `Scena09_Objects` (16),
`Scena09_Cells`, a word of 0 padding, `Scena09_CellHooks` (14), then the
vtable. Chapter 10's vtable is 4 slots and 8 bytes of 0; `Scena10_Pickups`
follows, then the state and run tables; a dword of 0 after `Scena10_Runs`
(entry 14 or padding) then the script bytes; `Scena10_Objects` ends at
`Scena11_EffectRolls` `0x661648`.

**Every dispatcher is unchecked** (a signed byte for the state and the run,
an unsigned one for the object and the cell): ours aborts with a message for
an index outside the table's own entries and for a 0 entry, the project's
rule (round nine §6, the owner: no DIVERGENCE entry). Runs 8 and 9 and object
10 are 0 in the original - a jump or call to address 0. Nothing in the band
writes run 8 or 9 (runs 1..4, 6, 0xA..0xD are started here; 5 and 7 by
nothing in the band - the movement script's op `F6` can store any run), nor
an object byte of 10.

## 3. Chapter 9's tail

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x557170` | Scena09_Object07 | 0x11 | object entry 7 | flag 0xE |
| `0x557190` | Scena09_Object08 | 0x14 | 8 | run 6 at step 0x64 |
| `0x5571B0` | Scena09_Object09 | 0x11 | 9 | flag 0x10 |
| `0x5571D0` | Scena09_Object10 | 0x11 | 10 | flag 0x14 |
| `0x5571F0` | Scena09_Object11 | 0x14 | 11 | run 8 at 0xF |
| `0x557210` | Scena09_Object13 | 0x14 | 13 | run 0xF at 0 |
| `0x557230` | Scena09_Object14 | 0x14 | 14 | run 0xF at 0x14 |
| `0x557250` | Scena09_Object15 | 0x14 | 15 | run 0xF at 0x28 |
| `0x557270` | Scena09_StepHook | 0x7A6 | slot 2, (x, z), al | by the area: rectangles of the position start runs 0xF, 0x10, 2, 0xE, 8, 9, 0xB (below) |
| `0x557A20` | Scena09_CellHook | 0x2A | slot 4, (x, z), al | `0x56D800(Scena09_Cells, 14, x, z)`, then the cell handler with (x, z) in place |
| `0x557A50` | Scena09_Cell00 | 0x5F | cell 0, al | the leader's byte (+0x89) not 5: run 6 at 0; else at 0x14 or 5 by flag 0xC |
| `0x557AB0`, `0x557B70`, `0x557C30` | Scena09_Cell01..03 | 0xB9, 0xB9, 0xEA | cells 1..3 | the same, the first clear of three or four flags picking the step |
| `0x557D20`, `0x557DD0` | Scena09_Cell04, 05 | 0xAB | cells 4, 5 | the same; all flags set: `Cond_ByteFE` 2 / 3 alone |
| `0x557E80`..`0x558000` | Scena09_Cell06..10 | 0x51 | cells 6..10 | flag 0x3D clear: run 8 at 5; set: run 8 at 0..4 and flag 0x18..0x1C |
| `0x558060` | Scena09_Cell11 | 0x48 | cell 11 | flag 0x3D clear: set it, sound 0x203; else run 8 at 6 |
| `0x5580B0` | Scena09_Cell12 | 0x2D | cell 12 | flag 0x2C clear: run 9 at 0x14, 1; set: 0xFF |
| `0x5580E0` | Scena09_Cell13 | 0x53 | cell 13 | flag 0x33 set: 0xFF; else run 0xF at 0x32 (leader byte 4) or 0x34 |

The object entries are called with (object, bits) and read neither. The
cell handlers are reached by a tail jump with (x, z) on the stack and read
neither; each answers in al (1, or 0xFF where it declines).

**Scena09_StepHook** reads `Game_AreaNumber` again after each block's calls
and tests, per area, two flags and a rectangle of the signed 16.16 position:
0x25 (run 0xF step 0xF), 0x27 (x exactly 0x310000 or 0x318000: run 0xF step
0x1E; else al 0 at once), 0x29 (three rectangles: run 0xF step 0x64), 0x2E
(run 0x10, counter 0 = 1), 0x31 (flag 0x25 and run 2; run 2 step 0xA; the
script flags' bits 1..3 and run 0xE), 0x3B (run 0xF steps 0x82 / 0x8C), 0x77
(flag 0x15 and run 8 step 0x14; run 8 step 0x19; run 9 steps 0 / 0x1E), and
0x52 with `Cond_ByteFD` 0: the cell words (the arguments' high halves,
reloaded from the stack mid-function and compared as 16-bit unsigned),
flags 0x20 / 0x21 (flag 0x21 and run 0xB step 0x1E), then ObjTrio +0x8
(stored to `0x903850`; 4..6 decline) and the leader's +0x89 (2: run 0xB step
0; else counter 0 = 0x10 unless flag 0x20 set and 0x21 clear). As the
original has it: area 0x29's first rectangle tests x "equal to 0x400000, or
not above 0x408000" - any x up to 0x408000 (section 8).

## 4. Chapter 10

| Address | Name | Bytes | Shape | Does |
|---|---|--:|---|---|
| `0x558140` | Scena10_Frame | 0xE | slot 0 | tail jump through `Scena10_States` |
| `0x558150` | Scena10_EnterArea | 0x3FF | state 1 | by the area (below); every exit stores state 2 |
| `0x558550` | Scena10_StartRun1 | 0x97 | called by EnterArea | a drop-in, the kind-2 sprite, the view, a camera effect, music file 0x7A, run 1 |
| `0x5585F0` | Scena10_Run | 0xE | state 2 | tail jump through `Scena10_Runs` |
| `0x558600` | Scena10_Run1 | 0x354 | run 1 | steps 0..0xC: counts, a load, an area change into 0x42, the palette greyed, one into 0x52, a ground effect, flag 6 |
| `0x558960` | Scena10_Run2 | 0x654 | run 2 | steps 0..0x19: a message by the party, effects of kinds 0x68 / 0x61 / 0x62 / 0x69 / 0x64 on timers, two area changes into 0x62, music 0x7B, message 0x13, flag 8 |
| `0x558FC0` | Scena10_Run3 | 0x50C | run 3 | steps 0..5, 0xA..0x15: camera effects on the chapter's slot, a message by the party or a wait, an area change into 0x80, a key item and a stream, flags 0xA / 0xB |
| `0x5594D0` | Scena10_SpriteOp | 0x21 | called by run 3 | a free sprite index to the word 0x903850; event op 0x on `Scena10_SpriteScript` |
| `0x559500` | Scena10_Run4 | 0x3F4 | run 4 | steps 0..4, 6, 8..0xA, 0x14..0x18, 0x1E, 0x1F (a two-level switch): messages by flag 0xF and the items, the tally test, drop-ins, area changes into 0x58 / 0x4B |
| `0x559900` | Scena10_HasItem4Eto55 | 0x21 | called by run 4, al | `Inventory_Count(0, 0x4E..0x55, 0)` until one is not 0 |
| `0x559930` | Scena10_TallyMet | 0x34 | called by run 4, al | the bytes `0x903A10 / 12 / 13 / 15 / 17` at least 2 / 3 / 2 / 1 / 2 |
| `0x559970` | Scena10_Run5 | 0x1AC | run 5 | steps 0..6, 8..0xA: the camera pulled in and out, a party placement, a message by the party, event battle 0x24, an area change into 0x4B |
| `0x559B20` | Scena10_Run6 | 0xFB0 | run 6 | steps 0..0x32 (below); every exit calls Scena10_Shake |
| `0x55AAD0` | Scena10_Shake | 0x2B | called by runs 6, 7, 12 | with `Scena10_Shaking` set: `MapView_Redraw` 2, `Camera_ShiftY` += 4 x a step by the frame |
| `0x55AB00` | Scena10_Run7 | 0x414 | run 7 | steps 0..7, 0xA..0xF, 0x14 (below) |
| `0x55AF20` | Scena10_Run10 | 0x37 | run 10 | step 1: a drop-in, flag 0x34 |
| `0x55AF60` | Scena10_Run11 | 0x2C | run 11 | step 0x23: a drop-in, counter 0 0x20 |
| `0x55AF90` | Scena10_Run12 | 0x200 | run 12 | steps 0..5: message 2, a button, the shake, the kind-2 sprite's z moved by 7 and back with the F3 divisor from `Field_MoveSpeeds` +3, flag 0x35 |
| `0x55B190` | Scena10_Run13 | 0x3BE | run 13 | steps 0, 1, 5, 8, 0xA, 0xB, 0xF..0x13, 0x15..0x19 (a two-level switch): flags 0x38..0x3D, area changes into 0x78, event battle 0x23 at the leader |
| `0x55B550`, `0x55B5D0`, `0x55B650` | Scena10_MsgByMemberA / B / C | 0x7E, 0x7E, 0x7F | called by runs 5, 2, 3, ax | the message word of the first listed member byte a member holds; none: 0, 0, 0xFFFF |
| `0x55B6D0` | Scena10_ObjectTrigger | 0x1F | slot 1, the object | `Scena10_Objects[object +0x86]` (object, bits) |
| `0x55B6F0`..`0x55B730` | Scena10_Object00..03 | 0xE..0x19 | objects 0..3 | drop-ins 1 / 4; runs 2, 3 |
| `0x55B750`, `0x55B9E0`, `0x55BA20` | Scena10_Object04, 06, 07 | 0x37 | objects 4, 6, 7 | a drop-in, the object posed (+1, +0x83, +0x84, +0x8A), a flag |
| `0x55B790` | Scena10_Object05 | 0x113 | object 5 | the pickup at the sprite's (x, z): item into the inventory, its name record to `Text_Records`, system message 2 or 3 |
| `0x55B8B0` | Scena10_PickupPose | 0xA6 | areas 75 / 86 | the pickup at the sprite's (x, z): taken - the sprite hidden; else its pose |
| `0x55B960` | Scena10_PickupEffect | 0x71 | areas 75 / 86 | an effect of kind 0x5D for the sprite, or the mover's word +0xA less 2 |
| `0x55BA60` | Scena10_Object08 | 0x36 | object 8 | a drop-in, flag 0x15, the step + 1, the object's +0x8A + 1 |
| `0x55BAA0` | Scena10_Object09 | 0x11 | object 9 | flag 0x19 |
| `0x55BAC0`, `0x55BAE0` | Scena10_Object11, 12 | 0x14, 0x1C | objects 11, 12 (and chapter 9's 5, 6) | run 0xA at 1; the sprite cleared and run 0xD at 5 |
| `0x55BB00` | Scena10_StepHook | 0x36C | slot 2, (x, z), al | areas 0x4B, 0x52, 0x78, 0x80 (below) |
| `0x55BE70` | Scena10_ArriveHook | 0x1D0 | slot 3, (x, z), al | areas 0x3A, 0x69, 0x78, 0x80 (below) |

**Scena10_EnterArea** tests the area afresh before each block: 0x52
(`Cond_ByteFD` 0 and flag 4 clear: flag 0x33 cleared; 1 with flag 0x3D and
not 4: Scena10_StartRun1), 0x5E (flag 0x17 after 0x16 with the pass flags
0; two map bytes cleared), 0x62 (leaves at once unless `Cond_ByteFD` 0;
`Scenario_CallA(0)` and a drop-in), 0x69 (story flag 0x4F through the
literal `0x904030`, flag 0x36), 0x78 (drop-ins by flags 0x39..0x3C,
`Field_ScriptFlags` bit 4 set or cleared, `Scenario_CallA(5)`), 0x79 (the
camera words and `Kind2_Place(0)`; ObjTrio +0xB always cleared), 0x80
(leaves unless `Cond_ByteFD` 3; flag 0xC), 0x83 (a sound and the shake
flag), 0x84 (a sound, the shake, a camera effect on the chapter's slot).

**The state machine** is chapter 9's and 12's: each run a switch on the step
byte `0x8034E5`, each case waiting on counter 0 (`0x903848`), the request
byte, the wait word, a word timer (`0x8034E6`, decremented, 0 wraps), the
stream, `File_LoadDone` or the in-use bit of an effect record, then doing
one thing and setting the next step; a run ends with `ScriptFlags_Clear40`,
a flag and the run and step 0.

**Recurring shapes, written once:** the camera effect (kind 0x13 at a
camera-relative point: +0x64 / +0x68 / +0x6C, +9 its life; `SpawnCamera`),
here stored to the chapter's own slot `Scena10_Slot` where chapter 9 uses
`0x903850`; the kind-2 sprite moved (`Field_Kind2X / Z` with
`Sprite_Kind2` +0x34 / +0x38, `Field_ViewReset`, then +0x3E from
`AreaMap_Elevation` at the position read back, or `MapView_SetElevation`);
the camera shake (`Scena10_Shake`, switched by `Scena10_Shaking`).

**Run 6** (51 steps): transitions and area changes between 0x80, 0x79,
0x84 and 0x5E; the kind-2 sprite moved about (0x1F, 0x36), (0x51, 8),
(0x58, 0x39), (0x24, 0x2E), (5, 0x46), (0x1A, 0x11), (0x24, 0x2F) with the
shake switched on and off; camera effects of kinds 0x13 / 0x18 / 0x31;
flags 0x14, 0x16, 0x17. Steps 0x24 and 0x26 fall into 0x25 and 0x27 once
their effect is done (the jump table lists both).

**Run 7**: step 0 waits for no member (of `Field_MemberCount`) with +0x137
= 3; step 1 copies the members' +0x89 bytes to `0x903A10..` (the count read
again after `Transition_Start`); then `Scenario_CallB(2)`, flag 0x12 and an
area change into 0x79, the camera, a kind 0x31 effect and an area change
into 0x83, flag 0x18, a load, `Scenario_CallA(4)`, and step 0xF's flag 9 of
the literal row `0x904657` by flag 0x1A and `0x56D6F0`. Steps 6..0xE call
Scena10_Shake; the shared exit calls it when the step, read there, is above
6 (so a step of 0..6 that did not advance does not shake).

**Scena10_StepHook**: 0x4B (`Cond_ByteFD` 1, flag 0x10 clear, x at most
0x40000, the z cell 0x6B..0x6E: run 4 step 0x1E), 0x52 (`Cond_ByteFD` 0:
the cells, ObjTrio +0x8 as chapter 9's; group SC7's
`Scena07_PartyHas89State2` at `0x55BBB1` - 0: counter 0 0x10; else flag 0x33
and run 0xB step 0x23; then flag 7 at x 0x458000), 0x78 (`Cond_ByteFD` 0,
x 0x58000: a member with +0x89 = 2 - flag 0x37, an area change into 0x78,
run 0xD; `Cond_ByteFD` 5, x 0x2B8000: run 0xD step 0xA), 0x80 (ObjTrio +0x8
0..2 at z 0x118000: run 4 step 0x14; `Cond_ByteFD` 3 at x 0x520000: run 4
step 0 and flag 0xC cleared). **Scena10_ArriveHook**: 0x3A (flag 9, story
flag 0x43, counter 0 0xA), 0x69 (the z cell signed above 0x1C: run 0xC),
0x78 (x 0x250000, z 0x748000: run 0xD step 0xF, then on to the 0x80 test -
al 0; section 8), 0x80 (runs 3 and 4).

**As the originals have them** (each kept, and said at its site): the
chapter slot's record is read without testing the slot for 0xFF (runs 1, 3,
4, 6); run 2's step 0xA stores the timer and step before `Music_FadeOut`;
run 3's `Scena10_MsgByMemberC` of 0xFFFF waits instead of speaking; the
message helpers' answers are pushed whole (eax) to `Msg_OpenScript`;
Scena10_Object05 reads `Sprite_Current` again before each call's argument;
Scena10_PickupEffect reads the slot back from the sprite for every store.

## 5. The fuzz

`scena_sc9b_fuzz.cpp`, through `scenario_harness`, two groups under the
shadow name `scena_sc9b`:

- **The clones**: `tools/scenario_rows.py --unit SC9b --clones` at
  `e4882fb`, identical to this reading extent for extent and call for call.
  Shapes: slots 0 `kSlot`; slot 1 and the chapter-10 object entries
  `kObject` (the object a sprite record); the hooks `kHook` (al compared);
  the states, runs, helpers, cell handlers and chapter 9's object entries
  `kState`; `ret_mask` 0xFF on the cell handlers, `HasItem4Eto55` and
  `TallyMet`, 0xFFFF on the three message helpers.
- **One copy the group makes itself.** Scena10_Run6 has 120 call sites; the
  harness re-aims at most 64. The fuzz file copies it with
  `bof3::CloneOriginal`, every site re-aimed at a trampoline calling the
  harness's recorder for that callee (`StandIn`), the jump table relocated
  into the copy; the harness is handed a six-byte `jmp [copy]` of the fuzz's
  own, as SC12's Run4 / Run8 and SC9a's Run15. No harness edit.
- **Callees** beyond the harness's 70: `Flags_Test` as a `kFlag` (al
  alone); `0x56D800` a byte `0xFF..0x0D`; `Field_StartEventBattle`,
  `EventOp_0x` (group SE's, by name); `Scena07_PartyHas89State2` (SC7's, a
  `kFlag`); `Inventory_Count` / `Inventory_Add` / `Item_NamePtr` masked to
  the bytes they read (the originals push the item with garbage above al),
  `Item_NamePtr` answering a 16-byte record of the fuzz's own filled from
  the recorders' stream; `0x591900`, `0x587B80`, `0x57CD90` (a byte
  `0xFF..0x1D`); this group's own helpers called directly - StartRun1,
  SpriteOp, Shake, MsgByMemberA / B as `kPhase` recorders, and
  HasItem4Eto55, TallyMet (al 0 a third of the time) and MsgByMemberC (0xFFFF
  a third of the time) as stand-ins of their own type; the log slots of the
  typed table stand-ins.
- **Tables**: `Scena10_States` and `Scena10_Runs` swapped for recorders
  (their 0 entries too); `Scena09_CellHooks` and `Scena10_Objects` get a
  typed stand-in per entry (a wrong index is a different log) and are
  regions.
- **Regions** beyond the harness's 22: chapter 9 `Cond_ByteFE` and the
  cell-hook table (24 regions, 10,053 bytes); chapter 10 the shake flag and
  slot, the music byte, `Cond_ByteFE`, `0x904EE0`, `Camera_ShiftY`,
  `Draw_OtSlot`, `Draw_SortOnX`, `Gfx_ClutStrip` (0x4000), `0x903A14`,
  `Text_Records`, `MoveScript_Object`, `Field_Kind2Hold`,
  `Field_MoveSpeeds` +3, the object table, `Scena10_Pickups` (random, so a record word of 0x8000 or above occurs; control B5d) (37 regions, 26,563 bytes).
- **Seeds**: for each hook, a rectangle of its (x, z) picked with its area
  and `Cond_ByteFD`, and the arguments drawn at its edges, one past, a cell
  past or inside (rectangle-paired hook arguments); the leader's +0x8 0..7
  and +0x89 (2, 4, 5 and others); for chapter 10 the member count 0..4, the
  members' +0x89 from the three message lists (read in place) and +0x137,
  `MoveScript_Object` at a sprite record, the request byte 0 / 2 / 6, the
  wait word 0, the timer 0 / 1 / 2, the hold, the input, the shake flag, the
  slot and its record's in-use byte, the tally bytes at and one below each
  bound, `Camera_Distance` at the run-5 limits, `Sprite_Current` on a
  pickup record (the z word's sign flipped half the time); each run's case
  steps and one past, each with counter 0 at the value it waits on (or one
  off) two times in three; the state 0..2, the run 0..13, the object byte
  0..12.
- **Disturbance** beyond the harness's (drawn from its hash alone): the
  area, `Cond_ByteFD`, the leader's bytes (chapter 9); counter 0, the area,
  `Cond_ByteFD`, the shake flag and slot, an effect's in-use byte, the member
  count and bytes, the kind-2 position, the sprite's +0xB, the request byte,
  the hold (chapter 10). A `settle` moves, half the time after each
  disturbance, the cells a function reads again after a call: the area and
  `Cond_ByteFD` (the hooks, the area entry), the kind-2 position and counter
  0 (run 6), the member count and step (run 7), the sprite's +0xB (object 5,
  the pickup effect), the step (object 8), counter 0 (the other runs).

**Result, in this worktree** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc9b`,
exit 0): two groups under the one name. Chapter 9's tail: 144,000 rounds over
24 functions (6,000 each), 226,011 calls to the stand-ins, **0 mismatches**;
coverage `Flags_Test` 68,417, `ScriptFlags_Set40` 103,689, `Flags_Set`
40,061, `0x56D800` 6,000, the cell stand-ins 5,582, `Sound_PlayEffect` 1,985.
Chapter 10: 234,000 rounds over 39 functions (6,000 each), 252,745 calls,
**0 mismatches**, 26,563 bytes of state (37 regions); every callee the block
calls is reached - among the rarest `Scena10_StartRun1` 9, `Scena07_PartyHas89State2`
53, `Scena10_MsgByMemberB` 49, `Scena10_HasItem4Eto55` 85, `Music_Play` 104,
`Sprite_SetAnimation` 158, the most `Party_DropIn` 50,318 and
`ScriptFlags_Set40` 49,020 - and every state and run table entry
(`0x5646B0`, EnterArea, Run, the bare ret, the eleven runs and the 0
entries) about 400..2,000 each; the object stand-ins 6,000.
`BOF3X_SHADOW='*'`: exit 0 (every earlier group of the three harnesses, then
these two: 0 mismatches).

## 6. Controls

`scratchpad/sc9b/controls.py` (not committed; SC12's pattern): each mutant
planted in `scena_sc9b.cpp` on a unique anchor, rebuilt, the self-test run,
restored, and rebuilt at the end. Refused = exit 3 on a mismatch.

| Id | Mutant | Result |
|---|---|---|
| O7 | object 7: flag 0xF | refused (exit 3) |
| O8 | object 8: step 0x65 | refused (exit 3) |
| O9 | object 9: flag 0x11 | refused (exit 3) |
| O10 | object 10: flag 0x15 | refused (exit 3) |
| O11 | object 11: step 0xE | refused (exit 3) |
| O13 | object 13: run 0xE | refused (exit 3) |
| O14 | object 14: step 0x15 | refused (exit 3) |
| O15 | object 15: step 0x29 | refused (exit 3) |
| S1 | step hook 9: area 0x25 x from 0x200001 | refused (exit 3) |
| S2 | step hook 9: area 0x29 x < 0x408000 | refused (exit 3) |
| S3 | step hook 9: area 0x29 x 0x38000 dropped | refused (exit 3) |
| S4 | step hook 9: area 0x2E counter 0 = 2 | refused (exit 3) |
| S5 | step hook 9: area 0x31 flag 0x26 | refused (exit 3) |
| S6 | step hook 9: script flags | 0xC | refused (exit 3) |
| S7 | step hook 9: area 0x3B second hit counter 0 only | refused (exit 3) |
| S8 | step hook 9: area 0x77 step 0x1A | refused (exit 3) |
| S9 | step hook 9: area 0x52 z cell span 4 | refused (exit 3) |
| S10 | step hook 9: leader byte 4 passes | refused (exit 3) |
| S11 | step hook 9: 0x10 path needs both flags | refused (exit 3) |
| C1 | cell hook 9: count 0xD | refused (exit 3) |
| C2 | cell hook 9: none answers 0xFE | refused (exit 3) |
| K0 | cell 0: step 0x15 | refused (exit 3) |
| K1 | cell 1: step 0x1F | refused (exit 3) |
| K2 | cell 2: step 0x29 | refused (exit 3) |
| K3 | cell 3: step 0x33 | refused (exit 3) |
| K4 | cell 4: Cond_ByteFE 4 | refused (exit 3) |
| K5 | cell 5: Cond_ByteFE 4 | refused (exit 3) |
| K6 | cell 6: as cell 7 | refused (exit 3) |
| K7 | cell 7: as cell 8 | refused (exit 3) |
| K8 | cell 8: as cell 9 | refused (exit 3) |
| K9 | cell 9: as cell 10 | refused (exit 3) |
| K10 | cell 10: as cell 9 | refused (exit 3) |
| K6b | cells 6..10: flag 0x19 + step | refused (exit 3) |
| K11 | cell 11: sound 0x204 | refused (exit 3) |
| K12 | cell 12: 0xFE | refused (exit 3) |
| K13 | cell 13: leader byte 5 | refused (exit 3) |
| F1 | frame 10: state 1 read as 2 | refused (exit 3) |
| R1 | run 10: run 4 read as 5 | refused (exit 3) |
| T1 | object trigger 10: bits + 1 | refused (exit 3) |
| T2 | object trigger 10: index + 1 below 12 | refused (exit 3) |
| E1 | enter 10: flag 0x34 cleared | refused (exit 3) |
| E2 | enter 10: map byte (0x32, 0x1B) | refused (exit 3) |
| E3 | enter 10: area 0x62 leaves unless Cond_ByteFD 1 | refused (exit 3) |
| E4 | enter 10: story flag 0x4E | refused (exit 3) |
| E5 | enter 10: script flags | 0x20 | refused (exit 3) |
| E6 | enter 10: angle 0x361 | refused (exit 3) |
| E7 | enter 10: ObjTrio +0xB 1 | refused (exit 3) |
| E8 | enter 10: area 0x80 flag 0xD | refused (exit 3) |
| E9 | enter 10: area 0x83 shake 0 | refused (exit 3) |
| E10 | enter 10: angle 0x303 | refused (exit 3) |
| SR1 | start run 1: music 0x7B | refused (exit 3) |
| SR2 | start run 1: effect z 0x31F | refused (exit 3) |
| R1a | run 1: grey keeps bit 14 | refused (exit 3) |
| R1b | run 1: area flags 0x81 | refused (exit 3) |
| R1c | run 1: F3 divisor 0x21 | refused (exit 3) |
| R1d | run 1: ground + 0x81 | refused (exit 3) |
| R1e | run 1: step 0 counter 2 | refused (exit 3) |
| R2a | run 2: file 0x302 | refused (exit 3) |
| R2b | run 2: step 8 effect +7 0 | refused (exit 3) |
| R2c | run 2: step 0xA stores after the fade | refused (exit 3) |
| R2d | run 2: step 0x13 +0x6C + 1 | refused (exit 3) |
| R2e | run 2: area flags 0x83 | refused (exit 3) |
| R2f | run 2: step 0xB count before the stop | refused (exit 3) |
| R3a | run 3: none as 0xFFFE | refused (exit 3) |
| R3b | run 3: step 0xF life 0x19 | refused (exit 3) |
| R3c | run 3: key item 8 | refused (exit 3) |
| R3d | run 3: 0x903A14 kept | refused (exit 3) |
| R3e | run 3: step 5 flag 0xB | refused (exit 3) |
| R3f | run 3: music byte 0x3E | refused (exit 3) |
| SO1 | sprite op: a byte, not a word | refused (exit 3) |
| SO2 | sprite op: 0xFE as none | refused (exit 3) |
| R4a | run 4: message 0x26 for 0x27 | refused (exit 3) |
| R4b | run 4: step 3 stored after the message | refused (exit 3) |
| R4c | run 4: effect kind 0x5F | refused (exit 3) |
| R4d | run 4: counter 0 0x15 | refused (exit 3) |
| R4e | run 4: sleep 2 | refused (exit 3) |
| R4f | run 4: area flags 6 | refused (exit 3) |
| R4g | run 4: drop-in 9 | refused (exit 3) |
| HI1 | has item: from 0x4F | refused (exit 3) |
| HI2 | has item: seven | refused (exit 3) |
| TM1 | tally: +5 at least 2 | refused (exit 3) |
| R5a | run 5: camera limit 0xFD81 | refused (exit 3) |
| R5b | run 5: placement kind 0x25 | refused (exit 3) |
| R5c | run 5: battle 0x25 | refused (exit 3) |
| R5d | run 5: camera + 0xB | refused (exit 3) |
| R5e | run 5: counter 0 0x29 | refused (exit 3) |
| R6a | run 6: step 0 transition 1 | refused (exit 3) |
| R6b | run 6: music byte 0x89 | refused (exit 3) |
| R6c | run 6: step 9 z 0x29B | refused (exit 3) |
| R6d | run 6: elevation -0x55F | refused (exit 3) |
| R6e | run 6: script flags | 0x40 | refused (exit 3) |
| R6f | run 6: flag 0x15 | refused (exit 3) |
| R6g | run 6: shake by Cond_ByteFD above 1 | refused (exit 3) |
| R6h | run 6: kind-2 z 0x2E0001 | refused (exit 3) |
| R6i | run 6: step read again after the calls | refused (exit 3) |
| R6j | run 6: angle 0xFCD3 | refused (exit 3) |
| R6k | run 6: Cond_ByteFE 0x24 | refused (exit 3) |
| R6l | run 6: +0xC 0x101 | refused (exit 3) |
| R6m | run 6: timer before the effect test | refused (exit 3) |
| R6n | run 6: step 0x2A without a slot | refused (exit 3) |
| R6o | run 6: Cond_ByteFE only at 0x13 | refused (exit 3) |
| R6p | run 6: script mask 0xFF7E | refused (exit 3) |
| R6q | run 6: flag 0x18 | refused (exit 3) |
| R6r | run 6: no shake | refused (exit 3) |
| R6s | run 6: step 0x24 does not fall through | refused (exit 3) |
| SH1 | shake: twice the step | refused (exit 3) |
| SH2 | shake: frame & 1 | refused (exit 3) |
| R7a | run 7: member state 2 | refused (exit 3) |
| R7b | run 7: copies +0x8A | refused (exit 3) |
| R7c | run 7: count read before the transition | refused (exit 3) |
| R7d | run 7: distance 0x55D | refused (exit 3) |
| R7e | run 7: +0xC -0x2A5 | refused (exit 3) |
| R7f | run 7: exit shake above 7 | refused (exit 3) |
| R7g | run 7: literal-row flag 8 | refused (exit 3) |
| R7h | run 7: step 0xB miss skips the exit | refused (exit 3) |
| R10 | run 10: flag 0x35 | refused (exit 3) |
| R11 | run 11: counter 0 0x21 | refused (exit 3) |
| R12a | run 12: kind-2 z + 8 | refused (exit 3) |
| R12b | run 12: x -0x3B5 | refused (exit 3) |
| R12c | run 12: timer before the shake | refused (exit 3) |
| R12d | run 12: input byte only | refused (exit 3) |
| R12e | run 12: flag 0x36 | refused (exit 3) |
| R13a | run 13: area flags 0x80 | refused (exit 3) |
| R13b | run 13: effect 0x44 | refused (exit 3) |
| R13c | run 13: timer 2 | refused (exit 3) |
| R13d | run 13: placement (z, x) | refused (exit 3) |
| R13e | run 13: flag 0x3E | refused (exit 3) |
| R13f | run 13: call B 2 | refused (exit 3) |
| MA | message A: three kinds | refused (exit 3) |
| MB | message B: ids at +6 | refused (exit 3) |
| MC | message C: none 0xFFFE | refused (exit 3) |
| B0 | object 0: drop-in 2 | refused (exit 3) |
| B1 | object 1: drop-in 5 | refused (exit 3) |
| B2 | object 2: run 3 | refused (exit 3) |
| B3 | object 3: counter 0 0xB | refused (exit 3) |
| B4 | object 4: pose 6 | refused (exit 3) |
| B5a | object 5: item + 0x4F | refused (exit 3) |
| B5b | object 5: flag + 0x21 | refused (exit 3) |
| B5c | object 5: the sprite not read again | refused (exit 3) |
| B5d | object 5 / pickup: z zero-extended | refused (exit 3) on the second pass; stood on the first (below) |
| B6 | object 6: pose 0x1D | refused (exit 3) |
| B7 | object 7: pose 0x20 | refused (exit 3) |
| B8 | object 8: step + 2 | refused (exit 3) |
| B9 | object 9: flag 0x1A | refused (exit 3) |
| B11 | object 11: step 2 | refused (exit 3) |
| B12 | object 12: sprite +0 1 | refused (exit 3) |
| P1 | pickup pose: animation from +0 | refused (exit 3) |
| P2 | pickup pose: +0x2B | refused (exit 3) |
| PE1 | pickup effect: less 3 | refused (exit 3) |
| PE2 | pickup effect: +6 0 | refused (exit 3) |
| H1 | step hook 10: x < 0x40000 | refused (exit 3) |
| H2 | step hook 10: counter 0 0x11 | refused (exit 3) |
| H3 | step hook 10: step 0x24 | refused (exit 3) |
| H4 | step hook 10: flag 8 | refused (exit 3) |
| H5 | step hook 10: member byte 3 | refused (exit 3) |
| H6 | step hook 10: z cell span 3 | refused (exit 3) |
| H7 | step hook 10: x cell span 4 | refused (exit 3) |
| H8 | step hook 10: leader byte 2 fails | refused (exit 3) |
| H9 | step hook 10: z cell span 2 | refused (exit 3) |
| H10 | step hook 10: flag 0xD cleared | refused (exit 3) |
| A1 | arrive hook 10: story flag 0x44 | refused (exit 3) |
| A2 | arrive hook 10: z cell >= 0x1C | refused (exit 3) |
| A3 | arrive hook 10: area 0x78 answers 1 | refused (exit 3) |
| A4 | arrive hook 10: z above 0x2A0000 | refused (exit 3) |
| A5 | arrive hook 10: x cell span 1 | refused (exit 3) |

165 of 165 refused, every one by a count (exit 3 on a mismatch), at least one
per function (every one of the 63 has one or more). One
stood on the first pass and was fixed in the fuzz, as earlier groups found:
**B5d**, Scena10_Object05 / Scena10_PickupPose comparing the record's z word
zero-extended instead of the sprite's sign-extended - every record word in
the image is below 0x8000, so no input could tell them apart while the table
was the image's. `Scena10_Pickups` is now a region (random each round, put
back); B5d is refused, and B4, B5a..c, B8, P1, P2, PE1, PE2 were re-run on the
stronger fuzz and all refused. One more was found by the fuzz itself before
any control: ours read the step byte again after run 6's kind-2 cases' two
calls (14 mismatches in the first run); fixed, and planted back as control
R6i, refused.

## 7. Cross-group calls

| Callee | What | Owner | How |
|---|---|---|---|
| `0x5341C0` `Scenario_CallB` | call table B's thunk | named by SCH, not taken | by name |
| `0x4410B0` `Field_StartEventBattle` | an event battle's set-up by id | group SE, ours | by name |
| `0x57A010` `EventOp_0x` | the event op on `Scena10_SpriteScript` | group SE, ours | by name |
| `0x550F80` `Scena07_PartyHas89State2` | called at `0x55BBB1` | group SC7, ours | by name |
| `0x56D800` | the cell-record search | nobody (group SX this wave) | raw |
| `0x532ED0` | the party placed before an event battle | nobody (SX) | raw |
| `0x56D6F0` | `Field_StatusBits` bit 7 | nobody (SX) | raw |
| `0x591900` | a key-item byte into the list at `0x904554` | nobody (SX) | raw |
| `0x587B80` | a jmp to `0x5A6FF0`, the music buffer stopped | nobody (SX) | raw |
| `0x57CD90` | the first free `Sprite_Objects` index | nobody (SX) | raw |
| `0x5646B0` | state 0 of chapters 9, 10, 12 | group SC13, this wave | through `Scena10_States`, read in place |
| `Scena09_Objects` 5..15 | the chapter-9 object table | group SC9a, ours | inbound: read in place there |
| areas 75 and 86's descriptor tables | Scena10_PickupPose / PickupEffect | later area groups | inbound: read in place |
| `Flags_*`, `ScriptFlags_*`, `Msg_*`, `Field_ChangeArea`, `Scenario_CallA`, `Party_DropIn`, `Kind2_Place`, `Transition_Start`, `Effect_FindFree`, `Sound_*`, `Music_*`, `LoadDatFile`, `File_LoadDone`, `Task_Sleep`, `AreaMap_*`, `MapView_SetElevation`, `Field_ViewReset`, `Inventory_*`, `Item_NamePtr`, `Sprite_SetAnimation` | | ours (earlier rounds); `Sound_ResumeAll` Capcom's, named | by name |

## 8. What nothing reached, and latent defects

No route reaches chapter 9 or 10 (fuzz only, as every scenario group so
far). Every table entry and every callee the block
calls is reached by the fuzz (section 5), and the 165 controls, one or more
per function, are all refused by a count. What the fuzz cannot see: the
meaning of a scene, and the areas' side of the two pickup handlers (areas
75 and 86 are later area groups').

Latent, described and kept (never fixed; ours aborts only where the original
would jump through data or to address 0):

- **The unchecked tables** (section 2): a chapter-10 state of 3 or more runs
  `Scena10_Runs`' entries; runs 8 and 9 jump to address 0, a run of 14 reads
  the dword 0 after the table, beyond it the script bytes; an object byte of
  10 calls address 0, 13 and up read `Scena11_EffectRolls`; a
  `0x56D800` answer of 14..0x7F would jump through `Scena10_Hooks` (index 14
  is `Scena10_Frame`), though `0x56D800` answers the index of the record
  that matched (its loop runs over the records it is handed, so below the
  count) or 0xFF.
- **The chapter slot read unchecked**: `Scena10_Slot` is the index of the
  record runs 1, 3, 4 and 6 wait on, never tested for 0xFF; a failed
  `Effect_FindFree` leaves 0xFF and the wait reads record 0xFF, 0x7F80 bytes
  past `Effect_Objects` (a read only; both sides the same). Most steps that
  store the slot do not advance on 0xFF, but four waits follow a store that
  does: run 1 step 0 (after Scena10_StartRun1 or the area-0x84 entry), run
  3 step 0x13 (after step 0x12), run 4 step 9 (after step 8), run 6 step
  0xA (after step 9).
- **Run 7's copy is unbounded**: step 1 copies `Field_MemberCount` bytes
  from ObjTrio +0x89 to `0x903A10..`; a count above 8 would write past the
  eight bytes `Scena10_TallyMet` reads (the fuzz seeds 0..4).
- **Area 0x29's rectangle** in Scena09_StepHook is open on the left: its x
  test reads "0x400000, or not above 0x408000" (`je` then `jg`) where every
  other rectangle tests a lower bound (`jl`); any x up to 0x408000 with z in
  0x270000..0x288000 starts run 0xF at step 0x64.
- **Scena10_ArriveHook's area-0x78 hit answers 0**: it starts run 0xD at
  step 0xF and then falls to the area-0x80 test instead of returning 1, so
  the arrive hook's caller sees no hook while the run is started.
- **The pickup search's sign**: `Scena10_Pickups`' x / z words are
  zero-extended and the sprite's sign-extended, so a record word of 0x8000
  or above never matches (none of the sixteen records has one, measured
  2026-09-28; a behaviour, not a fault).

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D134 (tables back to back), D135
(`Effect_FindFree`'s none as a slot), D136 (reads and writes by an unchecked
byte or count), D153 (hooks without the script flag) in
[`known-defects.md`](known-defects.md).