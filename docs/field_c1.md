# Group FC1: the Config row label and eleven effect kinds' states

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3, [`takeover-queue-round12.md`](takeover-queue-round12.md)), wave two, on the
round branch's tip `61be26e`. **41 functions ours** (`src/game/field_c1.cpp`,
shadow name `field_c1`): the cut table's 41 rows for FC1
(`analysis/round12_cut.tsv`), none added, none dropped - `tools/band_rows.py`
found no code in the band that no list has, and every start is a function
(section 5). Each read to its last instruction with capstone and fuzzed
through the scenario harness's field mode
([`scenario_harness.md`](scenario_harness.md) section 7) without edits to
it: 246,000 rounds, **0 mismatches**. **237 controls planted one at a time: 235 refused by a count, two equivalent (no input can tell them apart), each with a near variant refused** (section 6). Fuzz-only, except
what the whelp route very likely entered (section 9).

The band is labelled "field core" in the cut; read, it is almost all **effect
records**: the 20 `Effect_Objects` (`0x7E11E0`, `0x80` bytes) that
`Effect_RunObjects` runs every field frame through `Effect_KindHandlers`
(`0x655350`) by the kind byte `+5`, each kind's handler dispatching on its
state `+1` ([`frame-callees.md`](frame-callees.md) section 2). The one
exception is the Config screen's row draw `0x461800`.

| Part | Functions | Reached through |
|---|--:|---|
| The Config screen's row label | 1 | a call from the panel draw `0x461710` (Capcom's, catalog part 7) |
| Kind 6's ticks 3 and 5; kind 0x19 (kind 6 with a drift) | 4 | `EffectKind06_Ticks` (ours reads it: `EffectKind06_Tick`), `Effect_KindHandlers[0x19]`, `EffectKind19_States` / `_Ticks` |
| Kind 4 (`Effect_HoldFlag1C`'s countdown) | 1 | `Effect_KindHandlers[4]` |
| Kind 0x31: the camera turn with a zoom | 4 | `Effect_KindHandlers[0x31]`, `CameraZoom_States` |
| Kind 0x32: a thing thrown from a sprite, bouncing; the shared release | 5 | `Effect_KindHandlers[0x32]`, `EffectKind32_States`; `Effect_StateRelease` in 49 table cells |
| Kinds 0x37, 0x14, 0x3C: an animation played through; two tinted sprites | 6 | their state tables, dispatched by catalog part 2's `0x46A320`, `0x46A5E0`, `0x46A930` |
| Kind 0x17: a block pushed along the map, with seven helpers | 13 | `EffectKind17_States` (dispatcher `0x46ABB0`, catalog part 2); the helpers by `E8` |
| Kind 0x1B: a shot along the leader's facing, with a trail | 5 | `EffectKind1B_States` (dispatcher `0x46B7A0`, catalog part 2); its hit test by `E8` |
| Kind 0x30's run and start | 2 | `Effect_KindHandlers[0x30]`, `EffectKind30_States` |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`). "Thrown", "pushed block", "shot", "trail" name the code's
shape - the motion, the map bytes and the sprites it touches - not a
play-tested fact: which scene spawns which kind was not traced, and no
Breath of Fire III gameplay is asserted here. No PSX twin has a name in the
sibling (`names/*.toml`, `symbols.toml`); the twins
`analysis/pairs_propagated.json` gives are cited in each evidence string as
hypotheses, not read.

## 1. What each function does

Every function's comment in `field_c1.cpp` is the full read and each
`symbols.toml` `evidence` string cites its instructions; this is the map. SC
is `Sprite_Current` (an effect record here); `+n` its bytes.

### 1.1 The Config screen

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `Config_DrawRowLabel` | `0x461800` | 0x16A | `(x, y, row, s)`: the row's box `Menu_DrawBox(x, y - s, 0xF9, 2s + 0xB, 0, style 0x903A5A)`; label `row` of six immediates on its stack; `s == 3` (the row under the cursor): the label large with `Text_DrawAt` right-aligned at `x + 0x3A - 6 len`, `y - 1`, and the hand at `x + 4` unless the menu step `0x929F02` is 1; else small with `Text_DrawSmall` at `x + 0x3A - 4 len`, `y + 1`; then a grey line primitive at `x16 + 0x74` from `y16 - s + 1` to `y16 + s + 9` |

### 1.2 Kinds 6 and 0x19, kind 4, kind 0x31

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `EffectKind06_PlayOnce` | `0x469D10` | 0x26 | `Sprite_ScriptTickOnce`; `+1` up when it ended; tail `Sprite_QueueOverlay` for kind 6, else `Sprite_UpdateScreen` |
| `EffectKind06_Fade` | `0x469D40` | 0x6C | `+9` down, at 0 `+1` up; else the tint bytes `+0x5D..+0x5F` six down, the tick, and on odd frames the word `+0x30` up (kind 6, overlay) or the dword `+0x10` down (else, the plain update) |
| `EffectKind19_Run` | `0x469DE0` | 0x12 | `jmp [EffectKind19_States + 4 * +1]`: `EffectKind06_Start`, `EffectKind19_Tick`, `EffectKind06_End` |
| `EffectKind19_Tick` | `0x469E00` | 0x2D | `call [EffectKind19_Ticks + 4 * +6]` (kind 6's six ticks again), then the words `+0x2E += +0xC`, `+0x30 -= +0x10` |
| `EffectKind04_HoldTick` | `0x469FB0` | 0x27 | `+9` down; at 0 `Flags_Clear(0x904030, 0x1C)` and `Effect_Release` - the countdown `Effect_HoldFlag1C` (SX2's) spawns |
| `EffectKind31_Run` | `0x46A020` | 0x1A | `call [CameraZoom_States + 4 * +1]`, then `MapView_Redraw = 2` |
| `CameraZoom_Start` | `0x46A040` | 0x28 | `CameraTurn_Start`, then the distance step `+0x10 = ((+0xC - Camera_Distance) << 16) / +9` |
| `CameraZoom_Step` | `0x46A070` | 0x4E | `CameraTurn_Step`, then `Camera_Distance` plus the step (16.16), held at the target `+0xC` once past it |
| `CameraZoom_End` | `0x46A0C0` | 0x15 | `Camera_Distance = +0xC`, tail `CameraTurn_End` |

`EffectKind19_States`' first and last entries are kind 6's start and end
(`worldmap_area.cpp`), and `EffectKind19_Ticks` holds the same six handlers
as `EffectKind06_Ticks`: kind 0x19 is kind 6 with a drift of the words
`+0x2E` / `+0x30` by `+0xC` / `+0x10` each frame.

### 1.3 Kind 0x32, the shared release, kind 0x37

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `EffectKind32_Run` | `0x46A0E0` | 0x12 | `jmp [EffectKind32_States + 4 * +1]`: `_Throw`, `_Arc`, `_Settle`, `Effect_StateRelease` |
| `EffectKind32_Throw` | `0x46A100` | 0xDB | facing the leader's (`ObjTrio +8 & 7`); bank 0x18, animation 8; the point of `Sprite_Objects[+6]`; `MoveCmd_Move` of a stack object (`+0` 0, `+4` 3: speed 3) in that direction - which sets the effect's speeds; then `_Arc` in the same frame |
| `EffectKind32_Arc` | `0x46A1E0` | 0xA8 | at or under the ground (`AreaMap_Elevation` against the height word `+0x3E`), or `+0` bit 7, or state 0: a hop - rise `+0x14` and fall `+0x20` from `EffectKind32_Hops[+2]`, state up; then the point moves by the speeds and the height by the rise, the rise less the fall |
| `EffectKind32_Settle` | `0x46A290` | 0x7E | outside `ground <= height < ground + 0x80` (or with bit 7): state up and `Sound_PlayEffect(0x106)`; the height moves, no step across |
| `Effect_StateRelease` | `0x46A310` | 5 | `jmp Effect_Release` - 49 cells of the kinds' state tables hold it |
| `EffectKind37_Start` | `0x46A340` | 0x4F | bank the word `+0x2C`, animation `+6 & 0x7F` (bit 7 into `+0x2A`), state 1 |
| `EffectKind37_Play` | `0x46A390` | 0x45 | each ended pass `+9` down, at 0 state 2; also state 2 at the frame `+7` (word `+0x58`); the update |

### 1.4 Kinds 0x14 and 0x3C: two tinted sprites

`EffectKind14_Start` `0x46A600` (0x1E9) and `EffectKind3C_Start` `0x46A950`
(0x1D7) take two free sprites (`Sprite_FindFree` into `+3`, `+4`; the first
given back when the second fails), run an event op for each with
`DamageScratch`'s word naming it (`EventOp_6x(EffectKind14_Op)` and
`Sprite_SetAnimationAt(0x61, 0xC)`, or `EventOp_0x(EffectKind3C_Op)`), put
`Sprite_Current` back, move the two `0x1000` apart diagonally (the two kinds
mirror each other), set their `+0` bit 5, clear `+0x5C` and tint them
`(0xF, 0, 0)` and `(0, 0, 0xF)`; `+9` 4 frames, state 1.
`EffectKind14_Hold` `0x46A7F0` (0x5E) and `EffectKind3C_Hold` `0x46AB30`
(0x7A) set bit 6 of the leader's `+0` (kind 0x14) or of
`Sprite_Objects[+0xB]` (kind 0x3C) for those frames, then clear it, free
both sprites and step to state 2 (`0x478160`, catalog part 7).

### 1.5 Kind 0x17: the pushed block

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `EffectKind17_Start` | `0x46ABD0` | 0xB1 | bank `+0xB`; `_TakeCell`; scale mode 1; speeds, brakes, push 0 |
| `EffectKind17_Push` | `0x46AC90` | 0x19C | nothing with `Game_Mode` 1; no push (`+0xA` 0): the saved map byte put back when `Field_Request` is 5; pushed: `_BlockedAhead` (blocked: state 4), else the push halved, the speeds from the direction's unit `0x6696DC[+8] << 2` plus `0xC00 * (push - 1)`, the brakes `0x400` against them, animation 1, state 2 |
| `EffectKind17_Slide` | `0x46AE30` | 0x2CF | the speed braked (restored under `0x1000`), the point moved; the fall; with the view's kind-2 point on the leader and the block more than 4 cells off, the view point nudged 5 along the push and `MoveScript_FAWord` from the ground's height against `MapView_Elevation` (/ 20); ground; `_Bump` stops it; `_BlockedAt` / `_BlockedAhead` mark it stopped; stopped on a whole cell: state 5, else `_AlignTarget` |
| `EffectKind17_Settle` | `0x46B100` | 0x1B1 | to the target cell edge `+0x18`; ground and fall; `_Bump`; sunk: back to the start's height; landed within `0x4000` and `_CellBlocked` clear: `_TakeCell`, state 5; else state 4 |
| `EffectKind17_Grow` | `0x46B2C0` | 0x73 | scale mode 2, the scales `+0x40` / `+0x44` up `0x4000` a frame to 2.0; then `_CameraBack` and the release |
| `EffectKind17_Rest` | `0x46B340` | 0x3B | `+9` frames, then state 1 and `_CameraBack` |
| `EffectKind17_TakeCell` | `0x46B380` | 0x37 | `+0xB` = the map byte of its cell, which becomes 0x10 |
| `EffectKind17_CameraBack` | `0x46B3C0` | 0x38 | the view's kind-2 point back on the leader, `MoveScript_FAWord` negated, `MoveScript_F3Divisor` 0x40, `Field_Kind2Hold` 1 |
| `EffectKind17_Bump` | `0x46B400` | 0xA6 | an object at the point (`Sprite_ObjectAt`: its `+0x80` bit 0) or a member other than the leader (`Party_MemberAt`: its `+2` = 1): state 4, the tick, al 1; else al 0 |
| `EffectKind17_AlignTarget` | `0x46B4B0` | 0xC5 | `(flag)`: off a whole cell, the target `+0x18` = the next cell edge in the speed's direction (or this cell's, for a flag); state 3 |
| `EffectKind17_BlockedAhead` | `0x46B580` | 0x33 | `_BlockedAt` two direction steps ahead (`Field_DirectionSteps[+8]`) |
| `EffectKind17_BlockedAt` | `0x46B5C0` | 0x106 | `(x, z)`: the cell blocked, or on the axis of motion the slope at the half cell (`AreaMap_Slope`, `DamageScratch`) higher than the block, or the next cell blocked |
| `EffectKind17_CellBlocked` | `0x46B6D0` | 0xCA | `(cell x, cell z)`: no floor word; or by the map byte against the height - `0xFx` within 0x100, `0x10` / `0x2x` / `0xAx` level, anything above |

### 1.6 Kind 0x1B, kind 0x30

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `EffectKind1B_Start` | `0x46B7C0` | 0xD1 | the leader's facing; `0x46D0E0` (FC2's); animation `0x4A + facing / 2`; palette `0x80D440`; at the leader's point 0xC0 up; speeds the direction's unit `<< 4`; range 0xA (the leader's member id `+0x89` 1) or 0x12 |
| `EffectKind1B_Fly` | `0x46B8A0` | 0xDF | moves; range down; out of range or floor: animation 0x4E, state up; `_Hit`: animation 0x4F, state up; else every other frame a trail (a free effect record of kind 0x1B in state 3) |
| `EffectKind1B_End` | `0x46B980` | 0x13 | the animation once, then the release |
| `EffectKind1B_Trail` | `0x46B9A0` | 0xE8 | a copy of the look and point of `Effect_Objects[the leader's +0xB]`; animation 0x4E; state 2 (`_End`) |
| `EffectKind1B_Hit` | `0x46BA90` | 0x9F | an object at the point (marked), ground more than 0x40 above the leader's height, or the map byte 0x11 |
| `EffectKind30_Run` | `0x46BB30` | 0x12 | `jmp [EffectKind30_States + 4 * +1]` |
| `EffectKind30_Start` | `0x46BB50` | 0x9F | `Sprite_InitFromEntry` of entry `+0xB` of the area's list (`Area_Descriptors[Game_AreaNumber] +8`); tint 0x80; `0x46BF80` (FC2's); state 1; `Sprite_UpdateScreenA` |

### 1.7 Tables named (`[[data]]`)

`EffectKind19_States` `0x653EF4` (3), `EffectKind19_Ticks` `0x653F00` (6),
`CameraZoom_States` `0x653F24` (3), `EffectKind32_States` `0x653F30` (4),
`EffectKind32_Hops` `0x653F40` (three pairs), `EffectKind37_States`
`0x653F58` (3), `EffectKind14_States` `0x653F68` (3), `EffectKind14_Op`
`0x653F78` (32 bytes), `EffectKind3C_States` `0x653F98` (3), `EffectKind3C_Op`
`0x653FA8` (20 bytes), `EffectKind17_States` `0x653FBC` (6),
`EffectKind1B_States` `0x653FD4` (4), `EffectKind30_States` `0x653FE4`. The
counts are the states' own `+1` writes; each table runs into the next.
`EffectKind30_States`' end is not established (FC2's states follow; the
count 4 is what the fuzz exercises). `CameraTurn_States`' evidence ("seven
more .text addresses follow") is these: `CameraZoom_States` and
`EffectKind32_States`. `band_rows.py` reads `0x46BB50` as `0x653FBC[10]`;
the code reads it as `EffectKind30_States[0]` (`0x46BB30`'s table is
`0x653FE4`).

## 2. Divergence: none, and the Config screen's patches

Each function is a faithful replacement; no `DIVERGENCE.md` entry is new.
**`0x461800` carries three divergences' byte patches** (`config_text.cpp`):
DIV-0015 re-points its six label operands (`0x461832` .. `0x46185A`), the
layout moves its two `0x3A` anchors (`0x46189D`, `0x4618ED`), and DIV-0017
rewrites its large branch's `len * 6` (`0x461894`) and re-aims its
`Text_DrawAt` call (`0x46189F`). A replacement that took these as constants
would have undone all three. **Ours reads each of them from the original's
code at every call** - the six operands, the two anchor bytes, the width
code (either form; anything else a Fatal), the call's rel32 - so the patches
hold for ours as they did for Capcom's, whatever the order of
`ConfigText_Inject`, `ConfigText_Apply` (at `FIRST.DAT`'s load) and
`FieldC1_Inject`, and `BOF3X_ORIGINAL=ConfigText` still reverts them. Both
ledger entries say so (2026-09-29 notes). **Not seen in game through ours**:
the Config screen under an overlay is the owner's check for the coordinator's
live batch.

Where the original would jump through a state table to what is not code,
write through an index past `Sprite_Objects` (30), `Sprite_ObjectsExtra`
(4), `Effect_Objects` (20) or `ObjTrio` (3), divide by `+9` = 0
(`CameraZoom_Start`) or read its own stack frame past the six labels
(`Config_DrawRowLabel`, a row above 5), ours aborts with a message (round9
doc section 6; no ledger entry). Reads of the image's tables past their ends
(`0x6696DC[+8]`, `Field_DirectionSteps[+8]`, `EffectKind32_Hops[+2]`,
`Sprite_Objects[+6]`, `Effect_Objects[ObjTrio +0xB]`,
`Area_Descriptors[Game_AreaNumber]`) are kept: ours reads what the original
reads.

## 3. The arguments pushed with leftovers

- `Config_DrawRowLabel`'s box takes eax's low word with the fourth argument's
  byte and the **caller's** upper half (its only caller, `0x461710`, has
  loaded the x argument there); ours passes the byte alone.
  `Menu_DrawBox` reads the low words (`menu_windows.cpp`), so the fuzz
  compares its `y` and `h` as words.
- `EffectKind17_BlockedAt` reads its arguments' high words **through its own
  frame at `+2`**: the cells it hands `_CellBlocked` carry the next stack
  word in their upper halves; `_CellBlocked`, `AreaMap_ByteAt` and
  `AreaMap_Elevation`'s construction read the low words only.
- `AreaMap_SetByte`, `AreaMap_ByteAt`, `Sprite_SetAnimationBank` are pushed
  whole registers holding a word or a byte (`mov cx, [eax + 0x3A]; push ecx`);
  `AreaMap_Slope`'s direction is eax with the effect's `+8` in al.

## 4. The fuzz (`field_c1_fuzz.cpp`)

One `scenario_harness::Group`, `field` on, 6,000 rounds a function: the clone
table of `band_rows.py --clones` (no jump table inside a function, nothing
refused); every effect state and dispatcher a `kSprite`, the Config row draw
and the `E8` helpers a `kCall` (the four answering in al with `ret_mask`
0xFF); `sprite_span` 20, so every index byte a disturbance writes stays
inside the records. The five tables our dispatchers read are swapped for
recorders (`EffectKind19_States` 3, `_Ticks` 6, `CameraZoom_States` 3,
`EffectKind32_States` 4, `EffectKind30_States` 4); the catalog's
dispatchers' tables are not read by any function here.

**In this worktree** (`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=field_c1`, exit 0):
246,000 rounds over 41 functions, 465,961 calls to the stand-ins, **0
mismatches**; 24,616 bytes of state in 38 regions; 271 stand-ins (174 of the
field-standard set). Every callee and every table entry the five
dispatchers reach was called (coverage line in the log: the fewest
`Effect_FindFree` 523, `Menu_DrawHand` 891, `AreaMap_Slope` 1,303).

**Seeds** (every round, then per function): `Sprite_Current` an effect
record half the time; the index bytes `+3`, `+4`, `+0xB` below 20, `+6`
below 30, `+8` a direction, `+2` a hop - in that record and in the first
four sprite records (a disturbance or `EventOp_*`'s stand-in can move
`Sprite_Current` there mid-function); the position on the area block's 32 x
32 cells, its fraction 0 a third of the time; the leader's `+0xB` below 20;
`Field_Kind2Hold` and `DamageScratch` 0 half the time. Per function: each
dispatcher's byte inside its table; `+9` at 0, 1, 2 for the countdowns (not
0 for `CameraZoom_Start`, which divides by it); the zoom's target near
`Camera_Distance` plus the step, the step's sign both ways; `+7` against
the word `+0x58`; `Game_Mode` 1 and `Field_Request` 5 for `_Push`; for
`_Slide` the brakes that zero a speed, the view on the leader with the block
4 cells off and at the boundary `0x40000`, the fractions 0; for `_Settle`
the target at and around the point and speeds across `0x4000`; the scale at
and around `0x20000`; the cell words `_CellBlocked` and `_Fly` read planted
0 a third of the time (in `Seed`: an `args` hook's writes are lost); area 0..3
for `_Start`, whose `Area_Descriptors[0..3]` point at the fuzz's own
descriptors while it runs (put back after), each `+8` at an entry list that
is a region of its own.

### 4.1 The stand-ins this group lists

Registered before the standard sets, so they stand:

| Callee | Listed as | Why (read) |
|---|---|---|
| this group's `E8` callees | `kPhase` for the three void ones (`_Arc`, `_TakeCell`, `_CameraBack`), `kFlag` with their masks for the four answering in al, `_AlignTarget` byte-masked | their state or arguments logged; a stir after each |
| `0x46D0E0`, `0x46BF80` | `kPhase`, by address | FC2's, void, no arguments (read: no `[esp + 4]`) |
| `Menu_DrawBox` | masks 0xFFFF x 4, 0xFF, 0xFF | it reads `U16` x y w h and the flags' and colour's bytes |
| `AreaMap_SetByte` | 0xFFFF, 0xFFFF, 0xFF | `S16` x z and the value's byte |
| `AreaMap_ByteAt` | 0xFFFF x 2, answers 0x10, 0x11, 0x2x, 0xAx, 0xFx or any | the bytes its callers test |
| `AreaMap_Elevation` | answers at, a step off, 0x80 / 0x100 off the height word, the leader's height + 0x40 or `MapView_Elevation` | the heights its callers compare it with (a random word falls in `_Settle`'s 0x80 window 1 time in 512) |
| `AreaMap_Slope` | direction masked 0xFF; sets `DamageScratch` 0 or 1 | its callers test that byte straight after (`area_slope.cpp`) |
| `MoveCmd_Move` | object masked 0, its bytes `+0` and `+4` noted | the only two it reads (`0x578C10` read to its `ret` at `0x578D04`) |
| `Sprite_FindFree`, `Sprite_ObjectAt`, `Party_MemberAt` | answers 0..29, 0..0x21, 0..2, or 0xFF a third of the time | the standard `kFlag` answers any byte, which the callers use as an index |
| `EventOp_6x`, `EventOp_0x` | move `Sprite_Current` half the time | they place a sprite and leave `Sprite_Current` on it; the starts put it back |

**For the fold** (`scenario_harness.cpp`, the coordinator's): the masks of
`Menu_DrawBox`, `AreaMap_SetByte`, `AreaMap_Slope`'s direction and
`MoveCmd_Move`'s object (the standard rows compare leftover upper bytes);
`AreaMap_Slope`'s `DamageScratch` effect; answers in range for
`Sprite_FindFree` / `Sprite_ObjectAt` / `Party_MemberAt`. FH's field-standard
stand-ins this group ran for the first time and found right as they stand:
`Sprite_ScriptTick`, `_ScriptTickOnce`, `_UpdateScreen`, `_UpdateScreenSlot`,
`_UpdateScreenA`, `_QueueOverlay`, `_SetTint`, `_LoadPalette`,
`_InitFromEntry`, `Effect_Release`, `Gfx_CommitPrim`, `Gpu_SetLineF2`,
`Text_DrawSmall`, `Menu_DrawHand`, `CameraTurn_Start` / `_Step` / `_End`.

### 4.2 Two fuzz faults found on the way (the fuzz's, not ours)

- The first run mismatched in three functions with the logs equal: the
  group's disturbance and three effects chose values with `Pick` - the
  harness's `Next()` - so the two passes drew differently. They draw from
  the hash they are given now (`PickBy`).
- The second crashed at start-up: the disturbance wrote 0 into `+9` under
  `CameraZoom_Start`, which divides by it after `CameraTurn_Start`'s stand-in
  (the game cannot: `CameraTurn_Start` divides by it first). It keeps `+9`
  non-zero for that function.

## 5. What the cut and the tool said, settled

- **41 of 41 are functions** and the band has nothing else: `band_rows.py`
  "0 not listed"; the gaps `0x46A3E0..0x46A5FF` and `0x46A850..0x46A94F` are
  catalog part 2's rows (`0x46A3E0`, `0x46A450`, `0x46A500`, `0x46A5E0`,
  `0x46A850`, `0x46A930`: kind handlers of `Effect_KindHandlers`), not the
  cut's.
- **The sizes**: the tool's extents, read from the code, are right; 30 of
  the cut's sizes are padding past them, none differs in code.
- **Hosts**: no start here lies inside a host that is ours in a way that
  holds its code - `0x469FB0` follows `CameraTurn_End`'s `ret` (the
  catalog's host `0x469EF0` is `CameraTurn_Step`, which ends at `0x469F66`),
  the `0x46A020..0x46A1D9` run follows `Effect_HoldFlag1C` (SX2's, ends
  `0x46A01B`). `entries_logic.txt`'s catalog lines `0046A1E0 119B`, `0046B6D0
  3B8`, `0046BA90 279` swallowed their neighbours; the smaller extents are
  added (section 11).
- **`0x46BB50`** is `EffectKind30_States[0]`, not `0x653FBC[10]` (1.7).
- **No start is a jump-table case**; none of FH's thirteen self-test copies
  is FC1's.

## 6. Controls

A script (`controls.py` with `controls_list.py`, in the session
scratchpad's `fc1/`) planted each control alone in `field_c1.cpp` - anchored
on a string that must occur exactly once -, rebuilt, ran
`BOF3X_SHADOW=field_c1`, restored, and rebuilt at the end; a control counts
as refused when the harness names the planted function among those that
mismatched (exit 3). **The first pass (the fuzz of commit `58d2bb1`) refused
225 of 235.** The ten it did not were the fuzz's fault first, as rounds nine
to eleven taught: `+6`'s bits 6 and 7 never seeded for `EffectKind37_Start`
(J3, J4: `SeedRecord` kept it below 30 for `_Throw`'s index); `EventOp_6x`'s
stand-in did not note the `DamageScratch` word it reads (L3); a speed and
brake that land exactly on `0x1000` (Q1), the view point exactly `0x40000`
from the block after its move (Q3, Q15), the settle target exactly where the
step lands (S1), and position fractions whose low 12 bits are 0 (a9) never
seeded. With those seeds (commit `8c782e4`) **the whole list was run again:
235 of 237 refused by a count**, none by a Fatal alone. The two left are
equivalent:

- **Q16** (`view > ground` made `>=` in `_Slide`): at `view == ground` the
  original stores 0 into `MoveScript_FAWord` before the test and the mutant
  stores `0 / 20` - the same 0. No input tells them apart. Its near variant
  **Q16b** (`view > ground + 0x80`) is refused (6 rounds).
- **Y1** (`pos + 0x10000` made `pos + 0xFFFF` in `_AlignTarget`): the branch
  runs only when the position's low word is not 0, and then both round down
  to the same cell edge. Equivalent; its near variant **Y1b** (`pos + 0x8000`)
  is refused (320 rounds).

The weakest refusal is Q12 (`_Slide`'s rest state 5 made 4: 1 round of 6,000
reaches that path - a stopped block on a whole cell, which needs `_Bump`,
`_BlockedAt` and `_BlockedAhead`'s stand-ins to answer as they must); it is a
count, not a Fatal, and seeding that path harder is left to a later pass. Counts are in this worktree.

| | Function | Original | Planted | Refused in |
|---|---|---|---|--:|
| R1 | `Config_DrawRowLabel` | `s` | `s + 1` | 6,000 |
| R2 | `Config_DrawRowLabel` | `0xF9` | `0xF8` | 6,000 |
| R3 | `Config_DrawRowLabel` | `0xB` | `0xA` | 6,000 |
| R4 | `Config_DrawRowLabel` | `style` | `style ^ 1` | 6,000 |
| R5 | `Config_DrawRowLabel` | `r))` | `(r == 1 ? 2u : r)))` | 1,055 |
| R6 | `Config_DrawRowLabel` | `6` | `5` | 1,776 |
| R7 | `Config_DrawRowLabel` | `1` | `2` | 1,776 |
| R8 | `Config_DrawRowLabel` | `1` | `2` | 881 |
| R9 | `Config_DrawRowLabel` | `4` | `5` | 898 |
| R10 | `Config_DrawRowLabel` | `4` | `3` | 4,224 |
| R11 | `Config_DrawRowLabel` | `3` | `3 || s == 4` | 828 |
| R12 | `Config_DrawRowLabel` | `0x74` | `0x75` | 6,000 |
| R13 | `Config_DrawRowLabel` | `1` | `2` | 6,000 |
| R14 | `Config_DrawRowLabel` | `9` | `8` | 6,000 |
| R15 | `Config_DrawRowLabel` | `0x80` | `0x81` | 6,000 |
| R16 | `Config_DrawRowLabel` | `0x20` | `0x1C` | 6,000 |
| R17 | `Config_DrawRowLabel` | `kRowAnchorSmall)[0])` | `kRowAnchorBig)[0]) + 1` | 4,224 |
| A1 | `EffectKind06_PlayOnce` | `1` | `2` | 3,991 |
| A2 | `EffectKind06_PlayOnce` | `6` | `7` | 2,905 |
| B1 | `EffectKind06_Fade` | `0xFA` | `0xFB` | 4,778 |
| B2 | `EffectKind06_Fade` | `1` | `2` | 1,421 |
| B3 | `EffectKind06_Fade` | `1u` | `2u` | 1,532 |
| B4 | `EffectKind06_Fade` | `6` | `5` | 2,300 |
| B5 | `EffectKind06_Fade` | `o = Cur();` | `(nothing)` | 168 |
| B6 | `EffectKind06_Fade` | `1` | `2` | 5,987 |
| C1 | `EffectKind19_Run` | `Cur()[1]` | `(Cur()[1] + 1) % 3u` | 6,000 |
| C2 | `EffectKind19_Tick` | `Cur()[6]` | `(Cur()[6] + 1) % 6u` | 4,030 |
| C3 | `EffectKind19_Tick` | `0xC` | `0xE` | 6,000 |
| C4 | `EffectKind19_Tick` | `-` | `+` | 5,999 |
| D1 | `EffectKind04_HoldTick` | `0x1C` | `0x1D` | 2,028 |
| D2 | `EffectKind04_HoldTick` | `1` | `2` | 3,972 |
| D3 | `EffectKind04_HoldTick` | `== 0` | `<= 1` | 2,083 |
| E1 | `EffectKind31_Run` | `2` | `3` | 6,000 |
| E2 | `EffectKind31_Run` | `Cur()[1]` | `(Cur()[1] + 1) % 3u` | 6,000 |
| E3 | `CameraZoom_Start` | `16` | `15` | 5,194 |
| E4 | `CameraZoom_Start` | `frames` | `frames + 1` | 5,193 |
| E5 | `CameraZoom_Start` | `0xC` | `0x10` | 6,000 |
| E6 | `CameraZoom_Step` | `target` | `target - 1` | 308 |
| E7 | `CameraZoom_Step` | `target` | `target + 1` | 534 |
| E8 | `CameraZoom_Step` | `16` | `15` | 4,044 |
| E9 | `CameraZoom_Step` | `(nothing)` | `=` | 844 |
| E10 | `CameraZoom_End` | `0xC` | `0xE` | 5,991 |
| F1 | `EffectKind32_Run` | `Cur()[1]` | `(Cur()[1] + 1) % 4u` | 6,000 |
| F2 | `EffectKind32_Throw` | `7` | `3` | 3,026 |
| F3 | `EffectKind32_Throw` | `0x18` | `0x19` | 6,000 |
| F4 | `EffectKind32_Throw` | `8` | `9` | 6,000 |
| F5 | `EffectKind32_Throw` | `0x38` | `0x34` | 5,997 |
| F6 | `EffectKind32_Throw` | `3` | `4` | 6,000 |
| F7 | `EffectKind32_Throw` | `0` | `1` | 6,000 |
| F8 | `EffectKind32_Throw` | `(nothing)` | `^ 1` | 6,000 |
| F9 | `EffectKind32_Throw` | `SH_CALL(EffectKind32_Arc)()` | `Cur()[0x2A] = 0` | 6,000 |
| F10 | `EffectKind32_Throw` | `0` | `1` | 6,000 |
| G1 | `EffectKind32_Arc` | `=` | `(nothing)` | 138 |
| G2 | `EffectKind32_Arc` | `0x80` | `0x40` | 752 |
| G3 | `EffectKind32_Arc` | `0` | `1` | 1,319 |
| G4 | `EffectKind32_Arc` | `o[2]` | `(o[2] ^ 1u)` | 4,761 |
| G5 | `EffectKind32_Arc` | `4 + 8u` | `8u` | 4,758 |
| G6 | `EffectKind32_Arc` | `+` | `-` | 5,999 |
| G7 | `EffectKind32_Arc` | `1` | `2` | 4,666 |
| H1 | `EffectKind32_Settle` | `0x80` | `0x7F` | 72 |
| H2 | `EffectKind32_Settle` | `(nothing)` | `=` | 313 |
| H3 | `EffectKind32_Settle` | `0x106` | `0x107` | 5,292 |
| H4 | `EffectKind32_Settle` | `1` | `2` | 5,111 |
| H5 | `EffectKind32_Settle` | `0x80` | `0x40` | 416 |
| I1 | `Effect_StateRelease` | `Effect_Release` | `Sprite_UpdateScreen` | 6,000 |
| J1 | `EffectKind37_Start` | `0x2C` | `0x2E` | 6,000 |
| J2 | `EffectKind37_Start` | `1` | `2` | 6,000 |
| J3 | `EffectKind37_Start` | `7` | `6` | 4,375 |
| J4 | `EffectKind37_Start` | `0x7F` | `0x3F` | 2,976 |
| J5 | `EffectKind37_Start` | `1` | `2` | 6,000 |
| K1 | `EffectKind37_Play` | `2` | `3` | 949 |
| K2 | `EffectKind37_Play` | `o[7] != 0 && static_cast` | `static_cast` | 1,063 |
| K3 | `EffectKind37_Play` | `0x58` | `0x5A` | 1,106 |
| K4 | `EffectKind37_Play` | `ended` | `!ended` | 5,987 |
| K5 | `EffectKind37_Play` | `1` | `2` | 3,968 |
| L1 | `EffectKind14_Start` | `0` | `2` | 1,353 |
| L2 | `EffectKind14_Start` | `1` | `3` | 2,662 |
| L3 | `EffectKind14_Start` | `3` | `4` | 2,592 |
| L4 | `EffectKind14_Start` | `0xC` | `0xD` | 2,686 |
| L5 | `EffectKind14_Start` | `self` | `Cur()` | 1,700 |
| L6 | `EffectKind14_Start` | `0x1000u` | `0x800u` | 2,601 |
| L7 | `EffectKind3C_Start` | `0xFFFFF000u` | `0xFFFFF800u` | 2,565 |
| L8 | `EffectKind14_Start` | `0x20` | `0x10` | 2,637 |
| L9 | `EffectKind14_Start` | `0` | `1` | 2,601 |
| L10 | `EffectKind14_Start` | `0xF` | `0xE` | 2,686 |
| L11 | `EffectKind14_Start` | `0` | `1` | 2,686 |
| L12 | `EffectKind14_Start` | `Cur()` | `self` | 196 |
| L13 | `EffectKind14_Start` | `+` | `-` | 2,686 |
| L14 | `EffectKind3C_Start` | `op` | `op + 1` | 2,650 |
| L15 | `EffectKind14_Start` | `Cur()` | `Sprite_Objects` | 3,496 |
| M1 | `EffectKind14_Hold` | `0x40` | `0x20` | 2,996 |
| M2 | `EffectKind14_Hold` | `0xBF` | `0x3F` | 981 |
| M3 | `EffectKind14_Hold` | `0` | `1` | 1,994 |
| M4 | `EffectKind14_Hold` | `2` | `3` | 1,994 |
| M5 | `EffectKind14_Hold` | `1` | `2` | 4,006 |
| N1 | `EffectKind3C_Hold` | `0x40` | `0x41` | 1,990 |
| N2 | `EffectKind3C_Hold` | `0xBF` | `0xBE` | 912 |
| N3 | `EffectKind3C_Hold` | `0` | `1` | 1,898 |
| N4 | `EffectKind3C_Hold` | `2` | `1` | 2,007 |
| N5 | `EffectKind3C_Hold` | `0xB]` | `0xA] % 20` | 2,865 |
| O1 | `EffectKind17_Start` | `0xB` | `0xA` | 5,985 |
| O2 | `EffectKind17_Start` | `6` | `7` | 6,000 |
| O3 | `EffectKind17_Start` | `=` | `(nothing)` | 5,994 |
| O4 | `EffectKind17_Start` | `0` | `1` | 5,983 |
| O5 | `EffectKind17_Start` | `SH_CALL(EffectKind17_TakeCell)();` | `(nothing)` | 6,000 |
| O6 | `EffectKind17_Start` | `0` | `1` | 6,000 |
| P1 | `EffectKind17_Push` | `1` | `2` | 2,679 |
| P2 | `EffectKind17_Push` | `5` | `4` | 408 |
| P3 | `EffectKind17_Push` | `1` | `0` | 617 |
| P4 | `EffectKind17_Push` | `0xC00u` | `0xB00u` | 1,071 |
| P5 | `EffectKind17_Push` | `+` | `-` | 225 |
| P6 | `EffectKind17_Push` | `2` | `3` | 811 |
| P7 | `EffectKind17_Push` | `0xFFFFFC00u` | `0xFFFFFD00u` | 427 |
| P8 | `EffectKind17_Push` | `0x400u` | `0x500u` | 375 |
| P9 | `EffectKind17_Push` | `1` | `2` | 1,075 |
| P10 | `EffectKind17_Push` | `4` | `3` | 2,135 |
| P11 | `EffectKind17_Push` | `0xB` | `0xA` | 392 |
| P12 | `EffectKind17_Push` | `push` | `push + 1` | 445 |
| Q1 | `EffectKind17_Slide` | `(nothing)` | `=` | 243 |
| Q2 | `EffectKind17_Slide` | `+` | `-` | 2,977 |
| Q3 | `EffectKind17_Slide` | `(nothing)` | `=` | 326 |
| Q4 | `EffectKind17_Slide` | `5u` | `4u` | 139 |
| Q5 | `EffectKind17_Slide` | `20` | `16` | 223 |
| Q6 | `EffectKind17_Slide` | `0x14` | `0x13` | 425 |
| Q7 | `EffectKind17_Slide` | `0xFFFFFFF8u` | `0xFFFFFFF7u` | 2,939 |
| Q8 | `EffectKind17_Slide` | `SH_CALL` | `!SH_CALL` | 6,000 |
| Q9 | `EffectKind17_Slide` | `2` | `1` | 1,419 |
| Q10 | `EffectKind17_Slide` | `1` | `0` | 1,421 |
| Q11 | `EffectKind17_Slide` | `&&` | `||` | 24 |
| Q12 | `EffectKind17_Slide` | `5` | `4` | 1 |
| Q13 | `EffectKind17_Slide` | `== 0` | `!= 1` | 886 |
| Q14 | `EffectKind17_Slide` | `== 0` | `!= 1` | 486 |
| Q15 | `EffectKind17_Slide` | `0x38` | `0x34` | 424 |
| Q16 | `EffectKind17_Slide` | `(nothing)` | `=` | NOT-refused |
| Q17 | `EffectKind17_Slide` | `0x1C` | `0x18` | 732 |
| S1 | `EffectKind17_Settle` | `(nothing)` | `=` | 866 |
| S2 | `EffectKind17_Settle` | `<` | `>` | 440 |
| S3 | `EffectKind17_Settle` | `(nothing)` | `=` | 31 |
| S4 | `EffectKind17_Settle` | `start_x), S32(start_z` | `start_z), S32(start_x` | 1,114 |
| S5 | `EffectKind17_Settle` | `4` | `3` | 262 |
| S6 | `EffectKind17_Settle` | `||` | `&&` | 1,054 |
| S7 | `EffectKind17_Settle` | `(nothing)` | `=` | 106 |
| S8 | `EffectKind17_Settle` | `5` | `6` | 75 |
| S9 | `EffectKind17_Settle` | `0x36), Word(o + 0x3A` | `0x3A), Word(o + 0x36` | 183 |
| S10 | `EffectKind17_Settle` | `SetL(Cur() + 0x14, 0);` | `(nothing)` | 1,344 |
| S11 | `EffectKind17_Settle` | `SH_CALL(EffectKind17_TakeCell)();` | `(nothing)` | 78 |
| T1 | `EffectKind17_Grow` | `2` | `3` | 6,000 |
| T2 | `EffectKind17_Grow` | `0x4000u` | `0x3000u` | 5,991 |
| T3 | `EffectKind17_Grow` | `(nothing)` | `=` | 1,597 |
| T4 | `EffectKind17_Grow` | `Field_Kind2Hold != 0 || o` | `o` | 500 |
| T5 | `EffectKind17_Grow` | `0 && Field_Kind2Hold == 0` | `0` | 2,203 |
| T6 | `EffectKind17_Grow` | `0x4000u` | `0x4001u` | 6,000 |
| U1 | `EffectKind17_Rest` | `1` | `2` | 1,480 |
| U2 | `EffectKind17_Rest` | `== 0` | `<= 1` | 813 |
| U3 | `EffectKind17_Rest` | `SH_CALL(EffectKind17_CameraBack)();` | `(nothing)` | 1,522 |
| V1 | `EffectKind17_TakeCell` | `0xB` | `0xA` | 5,991 |
| V2 | `EffectKind17_TakeCell` | `0x10` | `0x11` | 6,000 |
| V3 | `EffectKind17_TakeCell` | `0x3A` | `0x38` | 5,945 |
| W1 | `EffectKind17_CameraBack` | `0u - fa` | `fa` | 6,000 |
| W2 | `EffectKind17_CameraBack` | `0x40` | `0x41` | 6,000 |
| W3 | `EffectKind17_CameraBack` | `1` | `2` | 6,000 |
| W4 | `EffectKind17_CameraBack` | `0x34` | `0x38` | 6,000 |
| X1 | `EffectKind17_Bump` | `1` | `2` | 2,984 |
| X2 | `EffectKind17_Bump` | `0) Member(` | `1) Member(static_cast<signed char>(member) == 1 ? 0 :` | 887 |
| X3 | `EffectKind17_Bump` | `2` | `3` | 870 |
| X4 | `EffectKind17_Bump` | `4` | `5` | 5,195 |
| X5 | `EffectKind17_Bump` | `0` | `2` | 695 |
| X6 | `EffectKind17_Bump` | `0x1E ? SpriteRec(i, who) : ExtraRec(i - 0x1E` | `0x1D ? SpriteRec(i, who) : ExtraRec((i - 0x1D) % 4` | 847 |
| Y1 | `EffectKind17_AlignTarget` | `0x10000u` | `0xFFFFu` | NOT-refused |
| Y2 | `EffectKind17_AlignTarget` | `0xFF` | `0xFE` | 296 |
| Y3 | `EffectKind17_AlignTarget` | `0xFFFF0000u` | `0xFFFF8000u` | 594 |
| Y4 | `EffectKind17_AlignTarget` | `3` | `2` | 836 |
| Y5 | `EffectKind17_AlignTarget` | `0x38` | `0x3A` | 2,722 |
| Y6 | `EffectKind17_AlignTarget` | `0` | `1` | 789 |
| Z1 | `EffectKind17_BlockedAhead` | `2u` | `3u` | 4,598 |
| Z2 | `EffectKind17_BlockedAhead` | `4 + 8u` | `8u` | 4,756 |
| Z3 | `EffectKind17_BlockedAhead` | `(nothing)` | `& 3` | 3,666 |
| a1 | `EffectKind17_BlockedAt` | `0` | `1` | 395 |
| a2 | `EffectKind17_BlockedAt` | `0x8000u` | `0x4000u` | 854 |
| a3 | `EffectKind17_BlockedAt` | `(nothing)` | `^ 1` | 854 |
| a4 | `EffectKind17_BlockedAt` | `(nothing)` | `=` | 32 |
| a5 | `EffectKind17_BlockedAt` | `1` | `2` | 301 |
| a6 | `EffectKind17_BlockedAt` | `cell_x` | `cell_x + 1` | 152 |
| a7 | `EffectKind17_BlockedAt` | `=` | `(nothing)` | 86 |
| a8 | `EffectKind17_BlockedAt` | `!` | `=` | 507 |
| a9 | `EffectKind17_BlockedAt` | `0xFFFF` | `0xFFF` | 263 |
| a10 | `EffectKind17_BlockedAt` | `1` | `2` | 3,966 |
| b1 | `EffectKind17_CellBlocked` | `1` | `0` | 1,976 |
| b2 | `EffectKind17_CellBlocked` | `=` | `(nothing)` | 23 |
| b3 | `EffectKind17_CellBlocked` | `0xF0` | `0xE0` | 187 |
| b4 | `EffectKind17_CellBlocked` | `0x10` | `0x11` | 66 |
| b5 | `EffectKind17_CellBlocked` | `0xA0` | `0xB0` | 56 |
| b6 | `EffectKind17_CellBlocked` | `(nothing)` | `- 1` | 54 |
| b7 | `EffectKind17_CellBlocked` | `16` | `16 | 0x8000` | 4,024 |
| b8 | `EffectKind17_CellBlocked` | `cx, cz` | `cz, cx` | 3,876 |
| b9 | `EffectKind17_CellBlocked` | `0x20` | `0x30` | 40 |
| c1 | `EffectKind1B_Start` | `8` | `9` | 5,964 |
| c2 | `EffectKind1B_Start` | `0x7B` | `0x7C` | 6,000 |
| c3 | `EffectKind1B_Start` | `0x4A` | `0x4B` | 6,000 |
| c4 | `EffectKind1B_Start` | `1` | `2` | 6,000 |
| c5 | `EffectKind1B_Start` | `0xC0u` | `0xBFu` | 6,000 |
| c6 | `EffectKind1B_Start` | `4` | `3` | 4,520 |
| c7 | `EffectKind1B_Start` | `1` | `2` | 3,021 |
| c8 | `EffectKind1B_Start` | `2` | `3` | 2,972 |
| d1 | `EffectKind1B_Fly` | `1` | `2` | 5,848 |
| d2 | `EffectKind1B_Fly` | `1` | `2` | 830 |
| d3 | `EffectKind1B_Fly` | `0x1B` | `0x1C` | 506 |
| d4 | `EffectKind1B_Fly` | `3` | `2` | 506 |
| d5 | `EffectKind1B_Fly` | `0x4F` | `0x50` | 1,988 |
| d6 | `EffectKind1B_Fly` | `0x4E` | `0x4D` | 2,985 |
| d7 | `EffectKind1B_Fly` | `!` | `=` | 4,524 |
| d8 | `EffectKind1B_Fly` | `0` | `1` | 1,982 |
| d9 | `EffectKind1B_Fly` | `0xC` | `0x10` | 4,441 |
| e1 | `EffectKind1B_End` | `SH_CALL` | `!SH_CALL` | 6,000 |
| f1 | `EffectKind1B_Trail` | `0xB]` | `0xA] % 20` | 5,674 |
| f2 | `EffectKind1B_Trail` | `0x1D` | `0x1E` | 6,000 |
| f3 | `EffectKind1B_Trail` | `0x4C` | `0x50` | 6,000 |
| f4 | `EffectKind1B_Trail` | `0x29` | `0x28` | 5,980 |
| f5 | `EffectKind1B_Trail` | `0x3E` | `0x3C` | 5,984 |
| f6 | `EffectKind1B_Trail` | `0x4E` | `0x4F` | 6,000 |
| f7 | `EffectKind1B_Trail` | `2` | `3` | 6,000 |
| f8 | `EffectKind1B_Trail` | `0x2C` | `0x2E` | 6,000 |
| f9 | `EffectKind1B_Trail` | `0` | `1` | 6,000 |
| g1 | `EffectKind1B_Hit` | `0x40` | `0x41` | 41 |
| g2 | `EffectKind1B_Hit` | `0x11` | `0x10` | 362 |
| g3 | `EffectKind1B_Hit` | `1` | `0` | 3,899 |
| h1 | `EffectKind30_Run` | `Cur()[1]` | `(Cur()[1] + 1) % 4u` | 6,000 |
| h2 | `EffectKind30_Start` | `1` | `2` | 5,957 |
| h3 | `EffectKind30_Start` | `8u` | `4u` | 5,707 |
| h4 | `EffectKind30_Start` | `1` | `2` | 6,000 |
| h5 | `EffectKind30_Start` | `0x80` | `0x81` | 6,000 |
| h6 | `EffectKind30_Start` | `0` | `1` | 5,993 |
| h7 | `EffectKind30_Start` | `0` | `1` | 5,985 |
| h8 | `EffectKind30_Start` | `SH_AT(void (__cdecl*)(), at::kFc2Place)()` | `Cur()[0x28] = 0` | 6,000 |
| h9 | `EffectKind30_Start` | `(nothing)` | `+ 8` | 6,000 |
| Q16b | `EffectKind17_Slide` | `ground` | `ground + 0x80` | 6 |
| Y1b | `EffectKind17_AlignTarget` | `0x10000u` | `0x8000u` | 320 |

## 7. Calls across groups

**Out of FC1, raw** (`field_c1_callees.h`, a `kPhase` recorder each in the
fuzz; the coordinator rebinds after FC2 merges - FC1 merges last):

| Callee | Owner | Called from |
|---|---|---|
| `0x46D0E0` | FC2 | `EffectKind1B_Start` |
| `0x46BF80` | FC2 | `EffectKind30_Start` |
| `0x57AD10` `EventOp_6x` | FO | `EffectKind14_Start` (twice) - by name, hand-agnostic: `SH_CALL(EventOp_6x)` and the fuzz's `KeyOf(EventOp_6x)` work whichever side holds it |

**Into FC1 from outside the group** - no raw `E8` call; everything arrives
through a table:

| Function | Reached from |
|---|---|
| `Config_DrawRowLabel` | Capcom's `0x461710` (catalog part 7, in no group) |
| `EffectKind04_HoldTick`, `EffectKind19_Run`, `EffectKind31_Run`, `EffectKind32_Run`, `EffectKind30_Run` | `Effect_KindHandlers` 4, 0x19, 0x31, 0x32, 0x30, read in place by ours (`Effect_RunObjects`, `frame_callees.cpp`) |
| `EffectKind06_Fade`, `EffectKind06_PlayOnce` | also `EffectKind06_Ticks` 3 and 5, read in place by ours (`EffectKind06_Tick`, `worldmap_area.cpp`) |
| `EffectKind37_*`, `EffectKind14_*`, `EffectKind3C_*`, `EffectKind17_Start` .. `_Rest`, `EffectKind1B_Start` .. `_Trail` | their state tables, read by catalog part 2's dispatchers `0x46A320`, `0x46A5E0`, `0x46A930`, `0x46ABB0`, `0x46B7A0` (in no group) |
| `EffectKind3C_Hold` | also `0x65461C[35]` (its dispatcher not traced) |
| `Effect_StateRelease` | 49 cells: `EffectKind18_States` (read by ours, `EffectKind18_Run`), `0x653A44`, `0x6542F8` and others |

## 8. The rebinding

`band_rows.py --refs` found 13 raw references to 4 of the 41 in `src/game`,
all in comments or harness data:

- **Named** (the line only): `worldmap_area.cpp` (`EffectKind06_Tick`'s
  comment: `0x469D40` / `0x469D10`), `scena_sx2.cpp` and
  `scena_sx2_callees.h` (`0x469FB0`), `config_text.cpp`'s comment on the
  label sites and its two anchor patches' trailing comments (`0x461800`).
- **Left raw, on purpose**: `config_text.cpp`'s patch addresses
  (`0x461832`.., `0x46189D`, `0x4618ED`, `0x461894`, `0x46189F`) - they are
  operands inside the function, not the function, and ours reads them there
  (section 2); its other comments on the screen's layout; and
  `scenario_harness.cpp`'s field-runs table (`{0x461800, 0x461980}`,
  `{0x469D10, 0x46D5F0}`: band limits, and the harness is not a group's to
  edit).
- No `_callees.h` constant of another group names an FC1 function.

## 9. The live route

Neither route's first-call trace (`analysis/calltrace/reach_whelp`,
`reach_dragon`, 2026-09-29 at `979a567`) lists any of the 41: the tracer
armed `entries_logic.txt`'s starts, and none of these had a line (section
11). One inference from what it did arm: **`whelpBoss.txt` enters
`EffectKind06_Start` `0x469BD0` at frame 284 from `Effect_RunObjects`
(`0x494053`) while kind 6's own run `EffectKind06_Run` is first entered at
frame 1,269** - and the only other way to `0x469BD0` is
`EffectKind19_States[0]` through `EffectKind19_Run`'s tail jump. So kind
0x19 ran from frame 284 (to `EffectKind06_End` at 305): **`EffectKind19_Run`
and `EffectKind19_Tick` very likely**, and through `EffectKind19_Ticks`
either `EffectKind06_Fade` or `EffectKind06_PlayOnce` (the other ticks,
`EffectKind06_Blink`, are first entered at frame 2,760). An inference, not a
trace; with this group's lines in `entries_logic.txt` the coordinator's A/B
can see them. The dragon route reaches kind 6 through its own run only.
Everything else here is fuzz-only.

## 10. Latent defects (Capcom's, described, not fixed)

- **Unchecked indexes**: the five dispatchers here and the five catalog ones
  (`jmp [table + 4 * +1]`); the two `Hold` states write
  `Sprite_Objects[+3]`, `[+4]` and (`EffectKind3C_Hold`)
  `Sprite_Objects[+0xB]` - the last from the spawner, never checked;
  `EffectKind17_Bump` and `EffectKind1B_Hit` index by `Sprite_ObjectAt`'s
  answer as a **signed** byte. Ours aborts on each write outside the records.
  In ordinary play the answers are in range.
- **`EffectKind1B_Trail` copies from `Effect_Objects[the leader's +0xB]`**,
  unchecked: the leader record's byte `+0xB` taken as an effect index. What
  writes it was not traced; if it is not an effect index when a shot is
  fired, the trail copies another record's look and point (a read, no
  crash while the byte is below 0xFF).
- **`Config_DrawRowLabel` with a row above 5** reads its own stack frame as
  a string pointer (the caller loops six rows; not reachable in play).
- **`CameraZoom_Start` divides by `+9`** (as `CameraTurn_Start` does just
  before it, [`frame-callees.md`](frame-callees.md) section 3): a zoom spawned
  with 0 frames faults in `CameraTurn_Start` first.
- **`EffectKind30_Start` indexes `Area_Descriptors` by `Game_AreaNumber`**
  and follows the descriptor's `+8` unchecked - as every area reader does.
- **`EffectKind32_Throw` hands `MoveCmd_Move` a stack object** with only
  `+0` and `+4` written; `MoveCmd_Move` reads exactly those two (read), so
  nothing shows.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): 34 lines - the 41 but the
seven already exact (`00461800 16A`, `0046B380 37`, `0046B3C0 38`, `0046B400
A6`, `0046B4B0 C5`, `0046B580 33`, `0046B5C0 106`); `0046A1E0 A8`, `0046B6D0
CA` and `0046BA90 9F` beside the catalog's larger extents (the file keeps the
smaller of a duplicate).
