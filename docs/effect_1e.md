# Group E1E: the leader's state 9 - `LeaderPanel_Stages`' steps

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9 and
10), wave one, on the round branch's tip `1bb41df`. **48 functions ours**
(`src/game/effect_1e.cpp`, shadow name `effect_1e`): the cut table's 48 rows
for E1E (`analysis/round13_cut.tsv`), none added, none dropped. Each read to
its last instruction with capstone and fuzzed through the scenario harness's
**field** mode ([`scenario_harness.md`](scenario_harness.md) sections 7 and
8): 144,000 rounds, **0 mismatches**; 58 of 58 controls refused. Ten `.data` tables
named. No route recorded so far enters any of the 48: fuzz only (section 9).

**The cut's label does not hold.** The 48 are not effect-kind states. Every
one is a step of the **leader's state 9**: `Field_LeaderStates[9]` (`0x528880`,
not in the cut) jumps through a twelve-entry table by `Sprite_Current +2`, and
each entry is a stage dispatcher that jumps through its own steps table by
`+3`. `Field_LeaderFrame` runs it for `ObjTrio`'s first member only
(`symbols.toml`), so `Sprite_Current` is the leader's `ObjTrio` record, not an
`Effect_Objects` record. What these steps drive is effect records 0..6 (their
state bytes and words), FE1's panel draws, the music and one `Sprite_Objects`
record - so the code sits beside the effect engine, but it is a field-action
state machine. The brief lists field-action states as round thirteen's
("what your functions are"), so the group took them; the coordinator may
prefer to count them with the field rounds (section 8 lists what else of the
machine is unowned). The cut's `unit` column for these rows (`T65FE40` ..)
names cells 0x400 below the ones that hold them (`0x660240` ..; section 5).

## 1. What each function does

Names by shape (`LeaderPanel_`, after FE1's `FieldPanel_*` draws they call;
`S<n>` the stage, `+2`'s value). What the machine is **in the game** is not
stated here: nothing in the code names it, and the owner has not said
(CLAUDE.md, game facts come from the owner). The cells it drives:

| Cell | What the code does with it |
|---|---|
| `Sprite_Current` `+2` / `+3` | the stage (`LeaderPanel_Stages`) / the step (each stage's table) |
| `+6`, `+7`, `+9`, `+0xB`, `+0xC` | a yes / no choice bit, a started flag, a frame / slide count, a result code (1 or 2), the held button bits |
| `0x7E11E1`.. (effect record 0) | `+1` its state; `+0xB` a hold; `+0xC` / `+0x10` / `+0x14` its step x / y / z; `+0x34` / `+0x38` its x / z; `+0x3E` its height |
| records 1..6 | `+1` state bytes the stages advance or wait on; record 4 `+6` a mode; record 5 `+0xA` a counter and `+0x10` a frame; record 6 `+6` a three-way choice, `+0xB` a hold |
| `0x939A1C` | a `Sprite_Objects` index (0xFF: none - stage 1's step 0, `0x5289C0`, writes it); that record's `+1`, `+6` (a kind), `+0x1C`, `+0x38`, `+0x9C` (a count) |
| `0x939A20` / `0x939A24` | pointers to a 10-byte and a 20-byte record in image tables (`0x66A4E8` / `0x66A528`, set by `0x52B250` from `0x904130` / `0x90412E`) |
| `0x939A28` | `FieldPanel_DrawBlink`'s switch: set when a count beats the best |
| `0x6BC71C` / `0x6BC71D` | the pose before / now (0, 1, 2); `0x52B1B0` sounds when they differ |
| `0x9040EC + kind` | the best count of each kind (a save-block byte) |
| `0x90412E` / `0x90412F` | an item byte `Inventory_Remove` is handed, cleared when it refuses |

### 1.1 The dispatchers and tables

| Table | Entries | Indexed by | Entries' owners |
|---|--:|---|---|
| `LeaderPanel_Stages` `0x6601F0` | 12 | `0x528880` by `+2` | `0x5288A0`, `0x5289A0` (stages 0 and 1, not in the cut), `LeaderPanel_S2` .. `LeaderPanel_S9`, `0x52AF60`, `0x52B0E0` (stages 10, 11, not in the cut) |
| `LeaderPanel_Stage1Steps` `0x66022C` | 11 | `0x5289A0` by `+3` | five of them ours (entries 5, 6, 8, 9, 10); 0..4 and 7 not in the cut |
| `LeaderPanel_Stage2Steps` `0x660258` | 4 | `LeaderPanel_S2` | all ours |
| `LeaderPanel_Stage3Steps` `0x660268` | 4 | `LeaderPanel_S3` | all ours |
| `LeaderPanel_Stage4Steps` `0x660278` | 15 | `LeaderPanel_S4` | all ours |
| `LeaderPanel_Stage5Steps` `0x6602B4` | 3 | `LeaderPanel_S5` | `LeaderPanel_UseItem`, `S5Wait`, `LeaderPanel_Leave` |
| `LeaderPanel_Stage6Steps` `0x6602C0` | 3 | `LeaderPanel_S6` | `UseItem`, `S6Wait`, `Leave` |
| `LeaderPanel_Stage7Steps` `0x6602CC` | 3 | `LeaderPanel_S7` | `0x52B200` (not in the cut), `S7Wait`, `Leave` |
| `LeaderPanel_Stage8Steps` `0x6602D8` | 2 | `LeaderPanel_S8` | `S8Leave`, `S8End` |
| `LeaderPanel_Stage9Steps` `0x6602E0` | 5 | `LeaderPanel_S9` | `S9Begin`, `S9Wait`, `S9Menu`, `0x52A6C0` (E1F), `0x52AF30` (not in the cut) |

Each count is the run of code pointers to the next table a dispatcher indexes
(`0x660220` is `0x5288A0`'s, `0x6602F4` `0x52A6C0`'s), checked by hand; none of
the dispatchers bounds its index (`mov ecx, [Sprite_Current]; xor eax, eax;
mov al, [ecx + 3]; jmp [eax * 4 + T]`).

### 1.2 The 48

| Function | Address | Size | Table entry | What |
|---|---|--:|---|---|
| `LeaderPanel_S1Choose` | `0x528CD0` | 0xC8 | Stage1Steps[5] | box 3 at (0x6C, 0x56); `Input_Pressed` 0x5000 toggles `+6` (sound 0x100), 0x60 takes it (0x40 sets `+6`, sound 0x106; else 0x103) and `+3` up; the hand by `+6`, the shade, the prompt box (`0x469750`) and its message (`MessagePools` + the word `0x803600`) |
| `LeaderPanel_S1Out` | `0x528DA0` | 0xCE | [6] | `+9` up, box 3 slid 0x50 a count; at 5: `+6` clear `Game_Step` up, else record 1 `+1` up and `+3` = 0; the prompt drawn 8 higher a count while `+9` |
| `LeaderPanel_S1Box2In` | `0x528E70` | 0x48 | [8] | `+9` down, box 2 slid in; at 0 `+6` = 1, `+3` up |
| `LeaderPanel_S1Box2Wait` | `0x528EC0` | 0x2F | [9] | box 2; confirm or cancel: `+3` up |
| `LeaderPanel_S1Box2Out` | `0x528EF0` | 0x60 | [10] | `+9` up, box 2 slid out; at 4 sound 0x102, `+6` = 1, `+3` = 4 |
| `LeaderPanel_S2` | `0x528F50` | 0x12 | Stages[2] | dispatcher |
| `LeaderPanel_S2Begin` | `0x528F70` | 0x22 | Stage2Steps[0] | record 2 `+1` up, animation 1, `+3` = 1 |
| `LeaderPanel_S2Wait` | `0x528FA0` | 0x8C | [1] | confirm: record 2's `+0x18` = 0, sound 0x200, both poses 0xFF, animation 2, `+0x38` = 0x410000, `+9` = 0, `+3` = 2; cancel with record 3 idle: back to stage 1 |
| `LeaderPanel_S2Anim` | `0x529030` | 0x37 | [2] | `Sprite_ScriptTick`; at frame word `+0x58` 6: record 0 `+1` up, `+0xB` = `+7` = 0, `+3` = 3 |
| `LeaderPanel_S2Steer` | `0x529070` | 0xD5 | [3] | record 0 at state 3: stage 3; else record 0's y step lowered on 0x20; the first 0xA000 held sets the x step (0x800 / -0x800), let go or out of x (0x60000, 0x150000) stops it |
| `LeaderPanel_S3` | `0x529150` | 0x12 | Stages[3] | dispatcher |
| `LeaderPanel_S3Begin` | `0x529170` | 0x58 | Stage3Steps[0] | records 1 `+2` / 4 `+1` up, record 4 mode 1, `+6` = 1, `0x6BC70C` = 0, `+7` = `+0xB` = 0, animation 3, `+3` up |
| `LeaderPanel_S3Run` | `0x5291D0` | 0x2F6 | [1] | record 0 moved by the pad: its z past 0x3E8000 ends the stage; on the ground a `Rand` chance against the 20-byte record's `+0x11` every 16th frame goes to stage 5; else the pose, record 0's steps and the animation by `Input_Held` and `0x903850`, the camera's z clamped to 0x150000..0x3C0000, the x step stopped at the edges |
| `LeaderPanel_S3Hold` | `0x5294D0` | 0x25 | [2] | `Field_Kind2Hold` clear: the divisor 0x40, the camera's z 0x3C0000, `+3` up |
| `LeaderPanel_S3End` | `0x529500` | 0x26 | [3] | `Field_Kind2Hold` and record 3 clear: back to stage 1 |
| `LeaderPanel_S4` | `0x529530` | 0x12 | Stages[4] | dispatcher |
| `LeaderPanel_S4Begin` | `0x529550` | 0x4F | Stage4Steps[0] | record 4 `+1` up, record 2 idle, record 4 mode 2, the camera's z the picked sprite record's, `Music_FadeOut(8)`, record 3 `+1` = 7 |
| `LeaderPanel_S4Music` | `0x5295A0` | 0x47 | [1] | record 4 at 3: track 0x29, record 3 mode 2, record 5 `+1` up, `+9` = 0 |
| `LeaderPanel_S4Run` | `0x5295F0` | 0x265 | [2] | record 0's z past 0x3E8000 lands it (records set, the picked record `+1` = 6, track 0x2A, animation 0xA, `+3` = 3); else by the pad 0xC000 / 0x3000 / 0x20 the pose, record 5's frame and counter, the picked record's `+0x1C` pulled or blinked; each way `0x52B1B0` |
| `LeaderPanel_S4Pose` | `0x529860` | 0x3B | (called) | pose 2 and the 10-byte record's `+7` while `+9`, else pose 1 and its `+4` |
| `LeaderPanel_S4Blink` | `0x5298A0` | 0x44 | (called) | the picked record's `+0x1C` 0 or 0xFF by `Frame_Counter` & (3 >> the record's `+3`) |
| `LeaderPanel_S4Best` | `0x5298F0` | 0x69 | [3] | record 4 at 4: the picked record's count over the kind's best replaces it (blink on); sound 0x102, `+9` = 8 |
| `LeaderPanel_S4HeaderIn` | `0x529960` | 0x8E | [4] | the header slid in; at 0 sound 0x20C or 0x20B by the kind's byte of `0x66A6AD` (36 a kind) |
| `LeaderPanel_S4IconIn` | `0x5299F0` | 0x6A | [5] | the kind's icon slid in |
| `LeaderPanel_S4RowIn` | `0x529A60` | 0x74 | [6] | the kind's row, count 0, rolled in |
| `LeaderPanel_S4Count` | `0x529AE0` | 0xE3 | [7] | the count up to the picked record's `+0x9C` (0x20 jumps to it); at it sound 0x20D (blink) and 0x102 |
| `LeaderPanel_S4TotalIn` | `0x529BD0` | 0x9B | [8] | the total slid in, the blink |
| `LeaderPanel_S4Take` | `0x529C70` | 0x105 | [9] | 0x60: kind 0x16 result 1; else `Inventory_Add(0, kind + 0x38, 1)` - refused result 2, taken `+3` = 0xD (then up to 0xE) |
| `LeaderPanel_S4Result` | `0x529D80` | 0xE7 | [10] | the result's message (0x43 / 0x42) slid in; result 1 goes to step 0xC with `+6` = 0, 2 to 0xB |
| `LeaderPanel_S4ResultWait` | `0x529E70` | 0x8C | [11] | message 0x42; 0x60: step 0xE |
| `LeaderPanel_S4AnyKey` | `0x529F00` | 0x95 | [12] | message 0x43; any button: `+6` = 0, `+3` up |
| `LeaderPanel_S4Again` | `0x529FA0` | 0xF5 | [13] | message 0x44 and a yes / no hand on `+6` |
| `LeaderPanel_S4Out` | `0x52A0A0` | 0x162 | [14] | the panel slid out; at 4 stage 0xB (result 1, `+6` clear) or stage 1 with record 1 `+1` up; track 0x28 |
| `LeaderPanel_S5` | `0x52A210` | 0x12 | Stages[5] | dispatcher |
| `LeaderPanel_UseItem` | `0x52A230` | 0x27 | Stage5Steps[0], Stage6Steps[0] | `Inventory_Remove(3, 0x90412E, 1)`; refused clears `0x90412E` / `F`; tail `jmp 0x52B200` |
| `LeaderPanel_S5Wait` | `0x52A260` | 0x20 | Stage5Steps[1] | record 4 at 3: record 3 mode 3, `+3` up |
| `LeaderPanel_Leave` | `0x52A280` | 0xF | Stage5/6/7Steps[2] | `0x52B330(0x60)`: a press of 0x60 goes to stage 8 |
| `LeaderPanel_S6` | `0x52A290` | 0x12 | Stages[6] | dispatcher |
| `LeaderPanel_S6Wait` | `0x52A2B0` | 0x3A | Stage6Steps[1] | record 4 at 3: track 0x28, sound 0x203, record 3 mode 4 |
| `LeaderPanel_S7` | `0x52A2F0` | 0x12 | Stages[7] | dispatcher |
| `LeaderPanel_S7Wait` | `0x52A310` | 0x3A | Stage7Steps[1] | the same, mode 5 |
| `LeaderPanel_S8` | `0x52A350` | 0x12 | Stages[8] | dispatcher |
| `LeaderPanel_S8Leave` | `0x52A370` | 0x74 | Stage8Steps[0] | wait word clear: `Task_Sleep(1)`, the camera to (0xF0000, 0x3C0000), `Field_ViewReset`, `0x52CE20` (the effect records cleared), `Transition_Start(3)`, the standing animation by `Field_State +0x89` |
| `LeaderPanel_S8End` | `0x52A3F0` | 0x28 | [1] | wait word clear: stage 1, `+3` = `+4` = 0 |
| `LeaderPanel_S9` | `0x52A420` | 0x17 | Stages[9] | `FieldPanel_DrawShade`, then the dispatch (`+3` read after the call) |
| `LeaderPanel_S9Begin` | `0x52A440` | 0x3D | Stage9Steps[0] | record 3 idle: `Music_FadeOut(0x10)`, sound 0x102, record 6 `+1` up, `+6` = 0 |
| `LeaderPanel_S9Wait` | `0x52A480` | 0x12 | [1] | record 6 at 3: `+3` up |
| `LeaderPanel_S9Menu` | `0x52A4A0` | 0x211 | [2] | record 6's three-way choice by the auto-repeated up / down; cancel and confirm move record 6 `+1` (3 / 4 -> 5, 8 -> 0xB, 0xE -> 0xF, by the choice for confirm); at step 2 the choice's message and the hand |

### 1.3 The PSX twins

`analysis/pairs_propagated.json` gives twins for 21 of the 48 (`symbols.toml`
`psx`, cited in each evidence string), `0x801DA068` .. `0x801DC9BC` - AREA
overlay copies, unnamed in the sibling (`names/*.toml`, `symbols.toml`
searched). None was read: they name nothing to transfer.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed. `DIVERGENCE.md`, `src/game/cheats.cpp` and `src/game/widescreen.cpp`
name no address in `0x528CD0..0x52A6B0` nor the tables `0x6601F0..0x6602F3`
(grepped 2026-09-29): no byte patch lies inside the 48.

## 3. The arguments, and what each callee reads

Where the original builds an argument from a register whose upper bytes are
not the value, ours hands on what the callee reads (each callee read with
capstone) or computes the same 32 bits:

- **Sprite_Current's upper half.** `movzx ax, byte [eax + 9]` with `eax` the
  `Sprite_Current` pointer leaves the pointer's upper 16 bits above the byte;
  `S1Out`, `S1Box2In`, `S1Box2Out`, `S4HeaderIn`, `S4Result` and `S4Out`
  multiply that whole value (box 3 at `0x6C - 0x50 * (hi | +9)`). Ours computes
  the same 32 bits (`PtrHi`). The callees read 16 bits of it: 
  `FieldPanel_DrawHeader` (`0x52D080`) and `FieldPanel_DrawMessage`
  (`0x52D560`) hand x and y to `0x52CFE0`'s `movsx word` and `Text_DrawAt`'s
  words.
- **A callee's leftovers.** `S1Out`'s prompt y and `0x469750`'s y (`movzx dx`
  / `xor cx, cx` over a register a call clobbered), `S4TotalIn`'s and
  `S4Out`'s total y and `S4Out`'s message y (`movzx ax` over the previous
  call's answer), and the kind, count and row bytes pushed as whole registers
  (`mov cl, [..]; push ecx`). The group's callee rows mask each to what the
  callee reads: `0x469750` x, y, w, h `& 0xFFFF` (both of `0x469790` and
  `0x469960` `and ..., 0xFFFF`) and the colour a byte; `Text_DrawAt` x, y
  words, the colour `& 0xFF` (`0x516B30`); `FieldPanel_DrawKindIcon` x, y
  words, the kind a byte; `FieldPanel_DrawKindRow` three bytes (its
  `0x52CE60` `and eax, 0xFF`); `FieldPanel_DrawTotal` and
  `FieldPanel_DrawMessage` words; `Inventory_Remove` three bytes
  (`0x591B60`: `mov bl, [esp + 0xC]`, `mov al, [esp + 0x14]`, `and eax,
  0xFF`); `0x52B330` a word (`and eax, edx; test ax, ax`).
- `Inventory_Add` (`kU8` in the standard set), `Menu_DrawHand` (`kU16`) and
  `Sprite_EnsureAnimation` (a byte) already mask as their readers need.

## 4. The fuzz (`effect_1e_fuzz.cpp`)

All 48 are `kSprite` clones in field mode, **not effect mode**: effect mode
puts `Sprite_Current` on an effect record every round and the disturbance
keeps it there, while these run on the leader's record. The seed puts
`Sprite_Current` on `ObjTrio`'s first record two times in three (else one of
the first four sprite records, where the field disturbance moves it), and
the group lists what effect mode would have given it: the regions
`0x939A00` + 0x30, `0x6BC700` + 0x20, `MessagePools` + 0xE8, `Game_Mode` /
`Game_Step`, `Field_MenuButton` + 0x10 (the confirm and cancel words), and
FE1's nine panel draws as callees (masks as section 3). 3,000 rounds a
function, `sprite_span` 5 (stage 9's table, the only one read after a call).

**The seed**, every round: the three `ObjTrio` and four sprite records'
`+2` random, `+3` below 5 (the dispatcher's own table length for the eight
dispatchers), `+6` / `+7` 0 or 1, `+9` small, `+0xB` 0..2, `+0xC` 0x8000 /
0x2000 / 0xA000, `+0x58` 6; the sprite index below 30, the picked record's
kind 0x16 or below 0x74 (so `0x9040EC + kind` stays in the save block's
region), its count `+0x9C` at `+9` give or take one; the kind's best at the
count give or take one; the record pointers `0x939A20` / `0x939A24` into the
harness's scratch buffers (random bytes, compared); effect records 0..6's
compared bytes at their boundaries (record 0's x at 0x60000 / 0x80000 /
0x120000 / 0x150000 give or take one, its z at 0x3E8000 / 0x150000 /
0x3C0000, its y step at 0x110000, the states 3, 4, 8, 0xE, the choice 0..3
and 0xFF); `Frame_Counter` with its low nibble clear half the time; the pad
and the confirm / cancel words on the bits the steps test; the wait word,
`Field_Kind2Hold`, `0x903850`, `Field_State +0x89` 0 often. **Louder
stand-ins**: `AreaMap_Elevation` answers at record 0's kept height give or
take one two times in three; `0x52B2E0` writes `0x903850` and `0x52B370`
record 0's `+0xB` and the pose (each read by `S3Run` after the call). **The
disturbance** (the group's, from the hash only) moves record 4's state,
`0x903850`, record 0's hold, the pose, `+9` / `+0xB` / `+6`, record 6's choice
and state, record 0's y step, record 3's state and the sprite index (below
30).

In this worktree: **144,000 rounds, 381,599 calls to the stand-ins, 0
mismatches**, 22,896 bytes of state in 41 regions, 297 stand-ins. Every entry
of the eight steps tables was reached (their handler recorders 186..2,967
calls each; `0x52A6C0` 569 and `0x52AF30` 601 through stage 9's), and every
callee listed.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py`'s read sizes are the code's; the cut's are the
  catalog's with the padding (44 rows), none differs by code.
- **Hosts**: the catalog's hosts `0x5287B0` and `0x5298A0` are recorded
  extents that swallowed their neighbours (`entries_logic.txt` had `005287B0
  10A5` and `005298A0 190A`); `0x5287B0` is `Field_CellKind`'s caller (a
  0xC3-byte function, not ours, not in the cut) and `0x5298A0` is
  `LeaderPanel_S4Blink` (0x44 bytes). Every hidden row is an entry by
  address (a table cell), none a fall-through.
- **Not a function / shared tails**: none. `0x529860` and `0x5298A0` are
  reached only by `E8` from `0x5295F0` - helpers, taken as functions of their
  own (called by name through the harness, `kPhase`).
- **The unit column**: the cut's `T65FDF8` .. `T65FEE8` for these rows are
  0x400 below the cells that hold them (`0x6601F8` .. `0x6602E8`); the
  "dispatchers" it names (`0x521A00`, `0x5220C0` = `Field_ActionBySet[11]`,
  ...) do not index these rows' tables. The real dispatcher chain is section
  1.1.

## 6. Controls

A script (session scratchpad `e1e/controls.py`) planted each change in `effect_1e.cpp`, rebuilt, ran `BOF3X_SHADOW=effect_1e` headless, restored and rebuilt: **58 of 58 refused by a count**, every one exit 3. Every function has at least one; the weakest, C58 (a confirm branch of `S9Menu` needing choice 2 with record 6 at 0xE), is refused in 3 rounds, C42 and C12 in 24 and 15.

| # | Function | Plant | Result |
|---|---|---|---|
| C1 | `LeaderPanel_S1Choose` | `Hand(0x70, 0x82u` -> `Hand(0x70, 0x83u` | refused, 3,000 of 3,000 rounds |
| C2 | `LeaderPanel_S1Out` | `if (s[9] == 5) {` -> `if (s[9] == 4) {` | refused, 635 of 3,000 rounds |
| C3 | `LeaderPanel_S1Out` | `BoxPrims(0x12u - (count << 3));` -> `BoxPrims(0x13u - (count << 3));` | refused, 2,673 of 3,000 rounds |
| C4 | `LeaderPanel_S1Box2In` | `(PtrHi(s) | s[9]) + 0x44u)` -> `(PtrHi(s) | s[9]) + 0x45u)` | refused, 3,000 of 3,000 rounds |
| C5 | `LeaderPanel_S1Box2Wait` | `if (Input_Pressed & (Field_CancelButtons | Field_ConfirmButtons)) {` -> `if (Input_Pressed & (Field_CancelButtons)) {` | refused, 556 of 3,000 rounds |
| C6 | `LeaderPanel_S1Box2Out` | `S()[3] = 4; } Shade();` -> `S()[3] = 5; } Shade();` | refused, 306 of 3,000 rounds |
| C7 | `LeaderPanel_S2` | `LeaderPanel_Stage2Steps, LeaderPanel_Stage2Steps_count);` -> `LeaderPanel_Stage2Steps + 1, LeaderPanel_Stage2Steps_count);` | refused, 3,000 of 3,000 rounds |
| C8 | `LeaderPanel_S2Begin` | `Animate(1);` -> `Animate(2);` | refused, 3,000 of 3,000 rounds |
| C9 | `LeaderPanel_S2Wait` | `SetLong(S() + 0x38, 0x410000);` -> `SetLong(S() + 0x38, 0x420000);` | refused, 948 of 3,000 rounds |
| C10 | `LeaderPanel_S2Anim` | `if (Word(s + 0x58) != 6) return;` -> `if (Word(s + 0x58) != 7) return;` | refused, 1,983 of 3,000 rounds |
| C11 | `LeaderPanel_S2Steer` | `!= 0x8000u ? 0x800u : 0xFFFFF800u` -> `!= 0x8000u ? 0x801u : 0xFFFFF800u` | refused, 55 of 3,000 rounds |
| C12 | `LeaderPanel_S2Steer` | `x <= 0x60000 || x >= 0x150000` -> `x < 0x60000 || x >= 0x150000` | refused, 15 of 3,000 rounds |
| C13 | `LeaderPanel_S3` | `LeaderPanel_Stage3Steps, LeaderPanel_Stage3Steps_count);` -> `LeaderPanel_Stage3Steps + 1, LeaderPanel_Stage3Steps_count);` | refused, 3,000 of 3,000 rounds |
| C14 | `LeaderPanel_S3Begin` | `B(at::kEff4Mode) = 1;` -> `B(at::kEff4Mode) = 3;` | refused, 3,000 of 3,000 rounds |
| C15 | `LeaderPanel_S3Run` | `(x >= 0x120000 && dx > 0)` -> `(x > 0x120000 && dx > 0)` | refused, 87 of 3,000 rounds |
| C16 | `LeaderPanel_S3Run` | `t[2] = 5;` -> `t[2] = 6;` | refused, 359 of 3,000 rounds |
| C17 | `LeaderPanel_S3Run` | `animation = 6;` -> `animation = 7;` | refused, 419 of 3,000 rounds |
| C18 | `LeaderPanel_S3Run` | `if (step > 0) Tick();` -> `if (step >= 0) Tick();` | refused, 255 of 3,000 rounds |
| C19 | `LeaderPanel_S3Run` | `SetL(at::kEff0StepZ, static_cast<U>(Long(r + 4)));` -> `SetL(at::kEff0StepZ, static_cast<U>(Long(r + 8)));` | refused, 70 of 3,000 rounds |
| C20 | `LeaderPanel_S3Hold` | `MoveScript_F3Divisor = 0x40;` -> `MoveScript_F3Divisor = 0x41;` | refused, 2,056 of 3,000 rounds |
| C21 | `LeaderPanel_S3End` | `if (Field_Kind2Hold != 0 || B(at::kEff3State) != 0) return;` -> `if (Field_Kind2Hold != 0) return;` | refused, 1,215 of 3,000 rounds |
| C22 | `LeaderPanel_S4` | `LeaderPanel_Stage4Steps, LeaderPanel_Stage4Steps_count);` -> `LeaderPanel_Stage4Steps + 1, LeaderPanel_Stage4Steps_count);` | refused, 3,000 of 3,000 rounds |
| C23 | `LeaderPanel_S4Begin` | `SH_CALL(Music_FadeOut)(8);` -> `SH_CALL(Music_FadeOut)(9);` | refused, 3,000 of 3,000 rounds |
| C24 | `LeaderPanel_S4Music` | `MusicTo(0x29);` -> `MusicTo(0x2B);` | refused, 1,214 of 3,000 rounds |
| C25 | `LeaderPanel_S4Run` | `p[1] = 6;` -> `p[1] = 7;` | refused, 575 of 3,000 rounds |
| C26 | `LeaderPanel_S4Run` | `Animate(8);` -> `Animate(9);` | refused, 470 of 3,000 rounds |
| C27 | `LeaderPanel_S4Run` | `B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) - 1);` -> `B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) + 1);` | refused, 813 of 3,000 rounds |
| C28 | `LeaderPanel_S4Run` | `static_cast<signed char>(RecordA()[5])` -> `static_cast<signed char>(RecordA()[6])` | refused, 468 of 3,000 rounds |
| C29 | `LeaderPanel_S4Pose` | `static_cast<signed char>(a[7])` -> `static_cast<signed char>(a[6])` | refused, 2,380 of 3,000 rounds |
| C30 | `LeaderPanel_S4Blink` | `!= 0 ? 0 : 0xFF);` -> `!= 0 ? 0 : 0xFE);` | refused, 2,611 of 3,000 rounds |
| C31 | `LeaderPanel_S4Best` | `if (Word(p + 0x9C) > best) {` -> `if (Word(p + 0x9C) >= best) {` | refused, 63 of 3,000 rounds |
| C32 | `LeaderPanel_S4HeaderIn` | `36u * kind) >= 2 ? 0x20C` -> `36u * kind) >= 3 ? 0x20C` | refused, 72 of 3,000 rounds |
| C33 | `LeaderPanel_S4IconIn` | `PickedKind("LeaderPanel_S4IconIn"); Icon(48u * S()[9] + 0x80u, kind);` -> `PickedKind("LeaderPanel_S4IconIn"); Icon(48u * S()[9] + 0x81u, kind);` | refused, 3,000 of 3,000 rounds |
| C34 | `LeaderPanel_S4RowIn` | `KindRow(PickedKind("LeaderPanel_S4RowIn"), 0, row);` -> `KindRow(PickedKind("LeaderPanel_S4RowIn"), 1, row);` | refused, 3,000 of 3,000 rounds |
| C35 | `LeaderPanel_S4Count` | `if (B(at::kBlink) != 0) Sound(0x20D);` -> `if (B(at::kBlink) == 0) Sound(0x20D);` | refused, 894 of 3,000 rounds |
| C36 | `LeaderPanel_S4TotalIn` | `Total(30u * S()[9] + 0x9Cu); SH_CALL(FieldPanel_DrawBlink)();` -> `Total(31u * S()[9] + 0x9Cu); SH_CALL(FieldPanel_DrawBlink)();` | refused, 2,413 of 3,000 rounds |
| C37 | `LeaderPanel_S4Take` | `static_cast<unsigned char>(kind + 0x38)` -> `static_cast<unsigned char>(kind + 0x39)` | refused, 696 of 3,000 rounds |
| C38 | `LeaderPanel_S4Result` | `S()[3] = 0xC;` -> `S()[3] = 0xD;` | refused, 153 of 3,000 rounds |
| C39 | `LeaderPanel_S4ResultWait` | `if (Input_Pressed & 0x60) S()[3] = 0xE;` -> `if (Input_Pressed & 0x60) S()[3] = 0xF;` | refused, 1,067 of 3,000 rounds |
| C40 | `LeaderPanel_S4AnyKey` | `Message(0x94, 0x43); if (Input_Pressed != 0) {` -> `Message(0x94, 0x45); if (Input_Pressed != 0) {` | refused, 3,000 of 3,000 rounds |
| C41 | `LeaderPanel_S4Again` | `Hand(0x20, 12u * S()[6] + 0xAAu);` -> `Hand(0x20, 12u * S()[6] + 0xABu);` | refused, 3,000 of 3,000 rounds |
| C42 | `LeaderPanel_S4Out` | `t[2] = 0xB;` -> `t[2] = 0xC;` | refused, 24 of 3,000 rounds |
| C43 | `LeaderPanel_S5` | `LeaderPanel_Stage5Steps, LeaderPanel_Stage5Steps_count);` -> `LeaderPanel_Stage5Steps + 1, LeaderPanel_Stage5Steps_count);` | refused, 3,000 of 3,000 rounds |
| C44 | `LeaderPanel_UseItem` | `SH_CALL(Inventory_Remove)(3, B(at::kItemA), 1)` -> `SH_CALL(Inventory_Remove)(3, B(at::kItemA), 2)` | refused, 3,000 of 3,000 rounds |
| C45 | `LeaderPanel_S5Wait` | `Effect3Mode(3);` -> `Effect3Mode(6);` | refused, 1,271 of 3,000 rounds |
| C46 | `LeaderPanel_Leave` | `at::kLeaveOnPress)(0x60);` -> `at::kLeaveOnPress)(0x61);` | refused, 3,000 of 3,000 rounds |
| C47 | `LeaderPanel_S6` | `LeaderPanel_Stage6Steps, LeaderPanel_Stage6Steps_count);` -> `LeaderPanel_Stage6Steps + 1, LeaderPanel_Stage6Steps_count);` | refused, 3,000 of 3,000 rounds |
| C48 | `LeaderPanel_S6Wait` | `LeaderPanel_S6Wait(void) { WaitThenMode(4); }` -> `LeaderPanel_S6Wait(void) { WaitThenMode(6); }` | refused, 1,182 of 3,000 rounds |
| C49 | `LeaderPanel_S7` | `LeaderPanel_Stage7Steps, LeaderPanel_Stage7Steps_count);` -> `LeaderPanel_Stage7Steps + 1, LeaderPanel_Stage7Steps_count);` | refused, 3,000 of 3,000 rounds |
| C50 | `LeaderPanel_S7Wait` | `LeaderPanel_S7Wait(void) { WaitThenMode(5); }` -> `LeaderPanel_S7Wait(void) { WaitThenMode(6); }` | refused, 1,189 of 3,000 rounds |
| C51 | `LeaderPanel_S8` | `LeaderPanel_Stage8Steps, LeaderPanel_Stage8Steps_count);` -> `LeaderPanel_Stage8Steps + 1, LeaderPanel_Stage8Steps_count);` | refused, 3,000 of 3,000 rounds |
| C52 | `LeaderPanel_S8Leave` | `SH_CALL(Sprite_SetAnimationAt)(0xA, 2);` -> `SH_CALL(Sprite_SetAnimationAt)(0xA, 3);` | refused, 957 of 3,000 rounds |
| C53 | `LeaderPanel_S8End` | `S()[4] = 0;` -> `S()[4] = 1;` | refused, 2,026 of 3,000 rounds |
| C54 | `LeaderPanel_S9` | `Shade(); Run("LeaderPanel_S9"` -> `Run("LeaderPanel_S9"` | refused, 3,000 of 3,000 rounds |
| C55 | `LeaderPanel_S9Begin` | `SH_CALL(Music_FadeOut)(0x10);` -> `SH_CALL(Music_FadeOut)(0x11);` | refused, 1,223 of 3,000 rounds |
| C56 | `LeaderPanel_S9Wait` | `if (B(at::kEff6State) != 3) return;` -> `if (B(at::kEff6State) != 4) return;` | refused, 1,015 of 3,000 rounds |
| C57 | `LeaderPanel_S9Menu` | `if (c > 2) B(at::kEff6Choice) = 0;` -> `if (c > 3) B(at::kEff6Choice) = 0;` | refused, 121 of 3,000 rounds |
| C58 | `LeaderPanel_S9Menu` | `else if (choice <= 1 && state == 0xE)` -> `else if (choice <= 2 && state == 0xE)` | refused, 3 of 3,000 rounds |

## 7. What nothing reached, and the limits

- The dispatchers' Fatal (a step past the table) and `Picked`'s (a sprite
  index of 30 or more) are never met in the fuzz by construction: the seed
  keeps both inside.
- The unowned callees (`0x52B1B0`, `0x52B200`, `0x52B2A0`, `0x52B2E0`,
  `0x52B330`, `0x52B370`) are recorders typed by reading; their bodies
  (effect records, sound) are not proved here.
- `Task_Sleep` is a recorder: the real one parks the task and resumes in
  ours' frame, which the fuzz does not run.

## 8. Calls across groups, rebinding, inbound

**Raw calls into this round's other groups** (`effect_1e_callees.h`):
`0x469750` (E1B, 2 sites: `S1Choose`, `S1Out`) and `0x52CE20` (E1F, 1:
`S8Leave`). **Unowned** (catalog part 7, not in the round's cut, called raw):
`0x52B1B0`, `0x52B200`, `0x52B2A0`, `0x52B2E0`, `0x52B330`, `0x52B370`. The
rest of the leader's state 9 is unowned too: `0x528880` (the state's
dispatcher, `Field_LeaderStates[9]`), stages 0, 1, 10, 11 (`0x5288A0`,
`0x5289A0`, `0x52AF60`, `0x52B0E0`) and their steps (`0x5288C0`, `0x528940`,
`0x528970`, `0x5289C0`, `0x528A70`, `0x528A90`, `0x528BE0`, `0x528C20`,
`0x52AF30`, `0x52AF80`, `0x52B0B0`, `0x52B100`, `0x52B120`, `0x52B160`,
`0x52B180`), `0x52B250`, `0x52B460` - catalog part 7 "Unlabelled" (and
`0x528880` part 2): **for the coordinator**, who may place them with this
group's names.

**Inbound**: none from outside the group but the tables - the only
references to the 48 are the cells of `LeaderPanel_Stages` and the steps
tables, and `0x5295F0`'s two `E8` to `0x529860` / `0x5298A0`.

**Rebinding**: `grep -rn -i` for each of the 48 addresses over `src/game`
(2026-09-29): three hits, none a constant to rebind - `scenario_harness.cpp` /
`.h` name `0x528CD0` as the start of an effect run's band (a range bound, not
a reference to the function; and not ours to edit), and `field_e1.cpp` line
127's comment names `0x528A90..0x52A420` as Capcom's callers of the panel
draws (a range; left). No raw reference to the tables. Nothing left raw.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, worldmap,
combat) are empty for all 48, and no first-call trace under
`analysis/calltrace` (`reach_dragon`, `reach_whelp`, the `recipe_*` runs)
enters `0x528CD0..0x52A6B0` (grepped 2026-09-29). **Fuzz only.** A route that
puts the leader in state 9 would cover it; which place does that is the
owner's to say.

## 10. Latent defects (Capcom's, described, not fixed)

- **L1. The sprite index `0xFF`.** Stage 1's step 0 (`0x5289C0`) sets
  `0x939A1C` to 0xFF ("none"); `0x52B6C0` later stores a real index. Stage 4's
  steps index `Sprite_Objects` by it unchecked (`0x7DEE80 + 0xA4 * n`): at
  0xFF they read and write `0x7E91DC..` past the 30 records. Ours aborts at 30
  or more. Whether ordinary play reaches stage 4 with 0xFF is not measured.
- **L2. A count of 256 or more.** `S4Count` compares the byte `+9` with the
  picked record's word `+0x9C`: at 0x100 or more the step never ends (`+9`
  wraps; 0x20 sets `+9` to the word's low byte, still unequal). `S4Best`
  compares the word with a byte best and stores its low byte.
- **L3. The best table by kind.** `0x9040EC + kind` is indexed by the sprite's
  `+6` unchecked (no table bound is known; ours indexes as the original).
- **L4. The dispatchers** do not bound `+3` (every stage) - ours aborts past
  each table.

## 11. For `analysis/calltrace/entries_logic.txt`

47 lines appended to the main checkout's file (2026-09-29), one per
function with its extent (`00529860 3B` was already there); `005287B0 10A5`
and `005298A0 190A` are cut by them (`005298A0 44` added, the smaller
extent).
