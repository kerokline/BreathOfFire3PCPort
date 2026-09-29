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
it: 246,000 rounds, **0 mismatches**. CONTROLS_SUMMARY. Fuzz-only, except
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
246,000 rounds over 41 functions, 466,228 calls to the stand-ins, **0
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

CONTROLS_SECTION

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
