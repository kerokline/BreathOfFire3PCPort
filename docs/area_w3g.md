# World 3, areas 148..151: a light switch and a searchlight, a dust shake and a view shift, a top-up choice, the world map's tenth copy

**Status:** IN PROGRESS (2026-09-28) - 56 functions ours
(`src/game/area_w3g.cpp`, shadow name `area_w3g`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), five `Run` calls:
0 mismatches in 288,000 rounds; 296 controls planted, 295 refused by a count, 1 equivalent with its variant refused (section 9). Fuzz only: no
recorded route reaches any of the 56 (section 8). No divergence.

Group AR3G of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18): the
band `0x4223A0..0x4249D0`, whole areas as `tools/area_rows.py --groups` cut
them - the last of world 3. What each area *is* in the story is not read
here: the names come from what the code does.

## 1. The band, the areas, the function count

`analysis/area_funcs.tsv` lists 53 starts for areas 148..151 (the tool's
`AR3` row); the tool's `--unit AREA148..151 --clones` rows list 56: area
148's object trigger `0x4227C0` and effect-kind handler `0x422840` are in
the rows and not the tsv, and area 149's `0x423380` is a gap of the tsv that
the rows name "called from `0x56ABC4`" (chapter 15's `Scena15_Runs`, group
SC15's; its `scena_sc15_callees.h` calls it by raw address as `kViewShift`).
**All 56 are functions and all are taken: no start dropped, none added.**
Every one was read to its last instruction with capstone (the scratch
`wdis.py`); every gap between them is `nop` padding. The tool's clone rows
match the reading call site for call site; three functions have a jump
table (the tails, below), relocated by the harness's `JumpTable`.

| Area | Descriptor | Roots | Block | Fns |
|--:|---|---|---|--:|
| 147 | `0x6342E8` (no choice, handler or init table) | none | none (`--unit AREA147`: "no unit") | 0 |
| 148 | `0x635388` | choices `0x635364` (8; the array starts two dwords before the handler array `0x63536C`, so choices 2..7 are handlers 0..5), handlers 4 and 5, tail kind 31, arrive hook, cell hook, object trigger 17, effect kind `0x7E`; init the bare `ret` `0x437CC0` | `0x4223A0..0x42315B` | 18 |
| 149 | `0x6361B8` | handler 1 (handler 0 is `Area146_ToExtraObject0`), init, tail kind 33, the chapter's call | `0x423160..0x4236C3` | 9 |
| 150 | `0x636A18` (a choice table only) | choices `0x636A08` (3), tail kind 58, step hook | `0x4236D0..0x423942` | 5 |
| 151 | `0x637078` (only a choice table, `+0x34` -> `0x637070`, whose entry is `Area130_ChoiceTailState2`, and no init) | `WorldMap_Records` record 9, `WorldMap_FieldHooks` entry 9, six state tables in its data block | `0x423950..0x4249C1` | 24 |

Area 148's other choice / handler entries are other bands' (`0x42C8A0`,
`0x425DB0`: world 4's; `Area146_ClearFlag46`, `Area146_ToExtraObject0`:
AR3F's). **Area 148's init is the bare `ret` at `0x437CC0`** (the one
`Area145_Init` is, round10 doc section 16) where the sibling's
`names/area_records.toml` gives the PSX descriptor an init at `0x801F4274` -
for the owner and the divergence map, as area 145's.

**PSX twins** (`names/area_records.toml`, hypotheses read against the PC
code; the shapes agree): area 148 handlers 4 / 5 `0x801F3A20` /
`0x801F3AE4`; area 149 handler 1 `0x801F2EFC`, init `0x801F36CC`; area 151's
hooks 0..4 `0x801F2E5C`, `0x801F4538`, `0x801F40E4`, `0x801F338C`,
`0x801F46D4`.

## 2. Area 148

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x4223A0` | `Area148_MemberMessageA` | `0x8B` | handler 4 / choice 6 | the first of four member ids (`0x6353D8`: 5, 2, 8, 4 - the outer loop) that party record 0 .. `Field_MemberCount` - 1 has at `+0x89` (the inner loop; the count read once, unsigned, unchecked against the three records): `Msg_OpenScript(0x6353DC[i])` (messages `0xB..0xE`), `Field_Request` 2; none: nothing |
| `0x422430` | `Area148_MemberMessageB` | `0x8B` | handler 5 / choice 7 | the same over ids `0x6353E4` (4, 8, 2, 5) and messages `0x6353E8` (`0xF..0x12`) |
| `0x4224C0` | `Area148_ChoiceMessage` | `0x16` | choice 0 | message 6 when the cursor byte is not 0, else 4 |
| `0x4224E0` | `Area148_ChoiceSetFlag59` | `0x2C` | choice 1 | cursor 0: message 5 (stored before the call), `Flags_Set(0x904030, 0x59)`; else no message |
| `0x422510` | `Area148_Tail31` | `0x27E` | tail kind 31 | by the s8 `0x9039F4` through the byte table `0x422778` (22) and the case table `0x422750` (10, both inside the function): **0** `Party_DropIn(0)`, state 1; **1** the script byte `0x90384B` at `0x18`: disarmed, flag `0x46`, `Field_ChangeArea(0x95, 0x1C0000, 0xC0000, 0x80)`; **0xA** an `Effect_FindFree` slot kept in `0x9039F5` (none: again next frame) becomes an effect of kind `0x13` (`+0x64` = -`0x39A`, `+0x68` the camera's angle 1, `+0x6C` = `0x190`, life `0x30`: the shape area 145's tail spawns), state `0xB`; **0xB** once that record's bit 0 clears, `Kind2_Place(2)`, state `0xC`; **0xC** the script byte at `0x20`: flag `0x5A`, the u16 timer `0x9039F6` = `0x20`, state `0xD`; **0xD** at the timer's end a second kind-`0x13` effect (-`0x2AA`, `0x200`, life `0x20`), state `0xE`; **0xE** the script byte 0: `ScriptFlags_Clear40`, disarmed; **0x14** flag `0x6A` toggled, `Sound_PlayEffect(0x204)`, and by the flag now set `Gfx_ClutAdjust(0, 0x380, -6, -4, -4)` or `(0, 0x180, 0, 0, 0)`; `Draw_PassFlags` 0, the timer `0xF`, state `0x15`; **0x15** at the timer's end `Draw_PassFlags` `0x1F`, `ScriptFlags_Clear40`, disarmed. Any other state: nothing |
| `0x422790` | `Area148_ArriveHook` | `0x2E` | `Area_ArriveHook` (`0x56E551`) | z's high word `0x1C`, x's `0x12..0x14` (16-bit): `ScriptFlags_Set40`, tail kind 31 at state 0, al 1; else al 0 |
| `0x4227C0` | `Area148_Trigger17` | `0x16` | `Field_ObjectTriggers` id 17 (`0x662E60`) | `ScriptFlags_Set40`, tail kind 31 at state `0xA`; al 0 |
| `0x4227E0` | `Area148_SwitchHook` | `0x5C` | `Area_CellHooks` pair 24 (area `0x94`) | the one record `0x6353F0` (x `0x4A`, z `0x37`, facing 1 in the low nibble, state `0x14`) whose x / z are the argument bytes and whose nibble is the leader's whole facing byte: `ScriptFlags_Set40`, tail kind 31 at the record's state, al 1; else al 0 |

So a floor switch facing one way flips story flag `0x6A`, darkens or restores
the CLUT with it and blanks the draw passes for fifteen frames; the flag
gates the searchlight below. The trigger's branch plays two flashes about a
kind-2 placement; the arrive hook's drops the party in, then changes to area
`0x95` when the script byte says.

**Effect kind `0x7E`: a searchlight** (`Effect_KindHandlers[0x7E]`,
`0x655548`; `Sprite_Current` the effect record):

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x422840` | `Area148_BeamRun` | `0x12` | `Effect_KindHandlers[0x7E]` | `Area148_BeamStates` `0x6353F4` by `+1` (3; ours aborts past) |
| `0x422860` | `Area148_BeamStart` | `0x4F` | state 0 | `+8` = `+0xB`; the word `+0x3E` + `0xC0`; `Area148_BeamAhead`; `+8` = (`+8` + 3) & 7; `Area148_BeamVelocity`; `+6` 2, `+9` `0x50`, `+1` 1 |
| `0x4228B0` | `Area148_BeamSweep` | `0xBC` | state 1 | with `Field_Request` set only the draw; else the point `+0x18` / `+0x1C` moves by `+0xC` / `+0x10`, `+9` - 1, the draw, the member turn; at `+9` 0 the facing + 2 (& 7), the velocity again, `+9` `0x50`, `+0xA` = `Rand()` % 45 (`idiv`): not 0, `+9` - 1 and state 2 (a pause of that many frames) |
| `0x422970` | `Area148_BeamPause` | `0x46` | state 2 | the draw, the member turn; `+0xA` - 1, at 0 `+9` + 1 and state 1 |
| `0x4229C0` | `Area148_BeamAhead` | `0x5D` | called | (`+0x18`, `+0x1C`) = (`+0x34`, `+0x38`) + 5 `Field_DirectionSteps[+8]`; `+0x20` = `AreaMap_Elevation` there (low word, signed) << 16 |
| `0x422A20` | `Area148_BeamVelocity` | `0x2D` | called | `+0xC` / `+0x10` = the walk deltas `0x6696DC[+8]` << 1 |
| `0x422A50` | `Area148_BeamTurnMembers` | `0xF9` | called | with `+0x3C` set to `+0x20` for the two asks (then put back through `Sprite_Current` read again): two cells about the effect from `0x635414` by (s8 `0x635400[(+9 >> 3) * 2 + k]` + (`+8` & 6) * 4) * 2, `Party_MemberAt(x, z, 0)` there; a member found, `Field_ScriptFlags2` bits 10 and 12 clear and `Field_Request` 0: its facing & 7 made odd, stored only when its `+1` is 1, then `Member_SetState2_8(member, 2)` |
| `0x422B50` | `Area148_DrawBeam` | `0x232` | called (`from`, `ahead`) | nothing unless `Draw_PassFlags` and flag `0x6A`. A draw-mode packet (dtd 1) linked at (`+0x18`, `+0x1C`); the map camera `0x494060`; a `LINE_F2` (opaque) between the two points projected by `0x494110`, coloured from `0x6353CC` by `+6`, linked (`0x20`); the line's slope `Math_Ratan2(dy, dx)` of the projected vertices' float differences through the CRT's `_ftol`; the screen size of (`0x40`, 0) at each end (`0x4941E0`) + (`Frame_Counter` & 1): r1 at `from`, r2 at `ahead`; `Area148_DrawBeamFan(ahead's vertex, r2, angle + 0xC00)`; `Area148_DrawBeamBand(from's vertex, r1, angle + 0x400, ahead's vertex, r2, angle + 0xC00)`; a draw-mode packet (dtd 0) |
| `0x422D90` | `Area148_DrawBeamBand` | `0x252` | called | two semi-transparent `POLY_G4`s, the second built at the first's pointer + `0x44` as a copy of it (after its link); both have the two ends in the beam's colour, their black vertices at r (cos, sin) >> 12 about each end - the first at a1 and a2 + `0x800`, the second at a1 + `0x800` and a2 (each angle masked to 16 bits before the `0x800`); each linked (`0x44`) |
| `0x422FF0` | `Area148_DrawBeamFan` | `0x16C` | called | eight semi-transparent `POLY_G3`s at `Gfx_PacketNext` (read again), the centre in the beam's colour, two black rim vertices r (cos, sin) >> 12 about it at a + k `0x100` and a + (k + 1) `0x100` (half a turn); each linked (`0x34`) |

So the light sweeps five steps ahead of itself along its facing, turning two
steps (a quarter turn) every eighty frames and pausing 0..44 frames at
random; any party member it meets in two cells about its end is turned to
the odd facing next to its own and put in state 2 / 8; it draws a line to
the lit point, a half-disc glow there and a band either side of the line.
What the members' state 2 / 8 means is the engine's (`Member_SetState2_8`,
[`scena_sx2.md`](scena_sx2.md)).

**The x87 in `Area148_DrawBeam`.** The two float differences go through
`fld dword` / `fsub dword` / `_ftol` (`0x5B9550`: the control word's rounding
set to truncate for one `fistp` to 64 bits, the low dword returned), rounded
at whatever precision the control word holds, and the vertices handed to the
band and fan through `fld dword` / `_ftol`. Ours does the same instructions in
inline asm (`FtolDiff`, `Ftol`, `move_cmds.cpp`'s `Math_Ratan2` idiom), so the
result is the original's for every float, NaN and huge values included
(`_ftol`'s invalid answer is `0x80000000:00000000`, low dword 0). The
integer-to-float stores (`fild` / `fstp dword`) are C++ conversions, rounded
to nearest as `fstp` does.

**The band's stack.** `Area148_DrawBeam` pushes the band's eight arguments
above the fan's four argument slots, which the fan has rewritten (its
arguments sign-extended, its loop counter left at 0, its angle `+ 0x800`):
the band reads only its own eight argument slots (read to the end: its
deepest read is the eighth, `[esp + 0x20]` at its entry), so the stale slots
below are not an input. The band also rewrites its own slots as scratch
(the fan does too); nothing reads them after either call.

**Tables** (`[[data]]`): `Area148_BeamColours` `0x6353CC` (four `(r, g, b)`),
`Area148_MemberIdsA` / `_MessagesA` `0x6353D8` / `0x6353DC`, `_MemberIdsB` /
`_MessagesB` `0x6353E4` / `0x6353E8`, `Area148_Switch` `0x6353F0`,
`Area148_BeamStates` `0x6353F4` (3), `Area148_TurnSteps` `0x635400` (10
pairs), `Area148_TurnCells` `0x635414`.

## 3. Area 149

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x423160` | `Area149_DustRun` | `0x12` | handler 1 | `Area149_DustStates` `0x6361FC` by `Sprite_Current[4]` (4; ours aborts past) |
| `0x423180` | `Area149_DustNear880` | `0x41` | dust state 0 | `Camera_Distance` (s16) below `0x880`: extra record 0's `+0x84` = 1, counter 0 (`0x903848`) = `0xA`, `Sound_PlayEffect(1)`, `+4` + 1. Then `Area149_SpawnDust(-4)` and `MoveScript_Object`'s word `+0xA` - 2 (the op runs again) |
| `0x4231D0` | `Area149_DustNear680` | `0x55` | dust state 1 | below `0x680`: counter 0 = `0xF`, `Sound_PlayEffect(0)`, `+4` + 1. Then extra record 0's x (s32) below `Field_Kind2X` + `0x20000` (signed): its `+0x84` = 3; the dust at -3, the op again |
| `0x423230` | `Area149_DustAtZero` | `0x54` | dust state 2 | `Camera_Distance` 0: counter 0 = `0x14`, `Sound_PlayEffect(1)`, `+4` + 1; the same mark; the dust at -2, the op again |
| `0x423290` | `Area149_DustHold` | `0x16` | dust state 3 | the dust at -1, the op again |
| `0x4232B0` | `Area149_SpawnDust` | `0xCC` | called (s8 dx) | nothing on frames with `Frame_Counter` & 3; else twice (z one cell on, then one back) an `Effect_FindFree` slot (none: the next) of kind `0x37` at (`Field_Kind2X` + dx << 16, `Field_Kind2Z` +- `0x10000`), the word `+0x2C` `0x2C0` (stored twice), `+0x3E` the elevation there, `+9` 1, `+6` / `+7` 0, `+0x29` 6 |
| `0x423380` | `Area149_ViewShiftBack` | `0xCA` | called (`0x56ABC4`, `Scena15_Runs` step `0x33`) | with `MapView_FocusX` at `0x63FF`: the map moved 0x14 cells back in x - `Sprite_Kind2`'s x and `Field_Kind2X` - `0x140000`, `MapView_Origin`'s word - `0x14`, the focus `0x77FF`, the kind-2 sprite's `+0x3E` = `AreaMap_Elevation` at the new x, `MapView_SetElevation(it)`, the view shift `0x56FCA0`; every live (bit 0) kind-`0x37` record of the twenty moved and its height again; extra record 0 moved and its height again |
| `0x423450` | `Area149_Tail33` | `0x1CD` | tail kind 33 | by the s8 `0x9039F4` through `0x423610` (13) and the case table `0x4235FC` (5): **0** with the focus at `0x77FF` the map moved `0x14` cells on (focus `0x63FF`; no dust records), the timer - 1, at 0 state 2; **2** the script byte `0x90384B` = `0x20`, disarmed, `Field_ChangeArea(0xA7, 0x280000, 0x630000, 0x81)`; **0xA** with the focus at `0x63FF` the map moved back (focus `0x77FF`), the timer, at 0 state `0xC`; **0xC** as 2 to area `0x94` (`0xA0000`, `0x190000`, `0x81`) |
| `0x423620` | `Area149_Init` | `0xA4` | init | came from area `0x94` (the word `0x802290`): extra record 0's x high word + `0x16`, `+0x83` 1, the word `+0x8A` 0, its `+0x3E` the elevation, `Cond_ByteFE` 1; with flag `0x11` of the bank `0x904000` tail kind 33 at state 0 and the timer `0x5A`. Came from `0xA7`: `+0x8A` 0, `+0x83` and `Cond_ByteFE` 2, tail kind 33 at state `0xA`, the timer `0x5A` |

So area 149 is entered from area `0x94` (148) or `0xA7` with the map shifted
`0x14` cells in x (the focus words `0x63FF` / `0x77FF` say which way it
stands); its tail shifts it for 90 frames and changes area again. Handler 1
raises dust about the kind-2 sprite at four camera distances.
Tables: `Area149_DustStates` `0x6361FC` (4).

## 4. Area 150

| PC | Name | Size | Root | What |
|---|---|--:|---|---|
| `0x4236D0` | `Area150_ChoiceFill5B` | `0x49` | choice 0 | the message from `0x636A5C` by the s8 cursor (unchecked); cursor 0: item `0x5B` topped up to 16 - `Inventory_Add(0, 0x5B, 16 - Inventory_Count(0, 0x5B, 0)` as a byte`)` (a fourth word, 0, is pushed and not read) - and `Sound_PlayEffect(0x106)` when it answers (al) not 0 |
| `0x423720` | `Area150_ChoiceMessage` | `0x17` | choice 1 | the message from `0x636A60` by the s8 cursor (unchecked) |
| `0x423740` | `Area150_ChoiceTailState` | `0x1C` | choice 2 | no message; the tail's state `0xE` when the cursor is not 0, else `0xC` |
| `0x423760` | `Area150_Tail58` | `0x1A9` | tail kind 58 | by the s8 `0x9039F4` - `0xA` through `0x4238FC` (13) and the case table `0x4238E0` (7): **0xA** `Msg_OpenScript(0x2D)`, `Field_Request` 2, state `0xB` (the choice sets `0xC` or `0xE`); **0xC** once `Field_Request` is not 2: `ScriptFlags_Clear40`, `Field_ChangeArea(0xBD, 0x17800000, 0xF000000, 0xA)`, `Field_ScriptFlags2` bit 6, `0x904153` = `0xA`, the word `0x90405C` = 0, `0x90405F` = `0xF0`, `0x90405E`, `0x929EC1`, `0x9036D0` = 0, `Field_StatusBits` bit 0, disarmed; **0xE** once not 2: `ScriptFlags_Clear40`, disarmed; **0x14** with `Field_Request` 0: `Msg_OpenScript(0x25` with flag 3 of the bank `0x904000`, else `0x20)`, `Field_Request` 2, state `0x15`; **0x15** once not 2: `Party_HealJoined`, `Sound_LoadStream(0)`, state `0x16`; **0x16** `Sound_StreamDone` (all of eax) not 0: counter 0 = 1, `Music_Play(0x95, 0x10)`, `Transition_Start(1)`, `Draw_PassFlags` `0x1F`, flag 3 of `0x904000` and flag `0x8A` of `0x904030` cleared, disarmed |
| `0x423910` | `Area150_StepHook` | `0x33` | `Area_StepHook` (`0x56E1B2`) | x's high word `0x17..0x1C` and z's `0x33..0x38` (16-bit): `ScriptFlags_Set40`, tail kind 58 at state `0xA`, al 1; else al 0 |

So stepping into a 6 x 6 box asks a question (message `0x2D`); one answer
changes to area `0xBD` with nine cells reset, the other closes. States
`0x14..0x16` (armed by no code in this band: a script's) heal the party
behind a message and a streamed sound, then restart the music.
Tables: `Area150_MessagesA` / `_MessagesB` `0x636A5C` / `0x636A60`.

## 5. Area 151: the world map's tenth copy, and what differs

Area 151 is `WorldMap_Records` record 9 and `WorldMap_FieldHooks` entry 9.
A capstone compare of its 24 functions against area 87's
([`area_w2b.md`](area_w2b.md) section 4; the scratch `cmp.py`: instruction by
instruction, addresses inside a function made relative) finds **23 the same**
but for jump targets, calls to its own copies and table operands, and one
that differs; one of area 87's 25 has no copy in the band:

| Function | Area 87 | Area 151 |
|---|---|---|
| field hook (`PlaceMessage`) | `0x174` bytes: place rows on a `0xA1` cell, else the leader's cell record (searched with no bound), its name set (3 sets of (id, 4 items), searched) into four `Text_Records` rows, `Msg_OpenScript(set + 0x16)` | **`0x12B` bytes**: the `0xA1` branch is area 87's (7 place rows); the other has **no cell search and no name-set search** - the one name set at `0x63719C` (id `0x10`, five items), its items into **five** rows, `Msg_OpenScript(0x16)` |
| `PlateStart` (plate state 0) | `0x40FEC0`, bank `0x156` | **none in the band**: area 151's plate table names `0x424BA0` (a plate start with bank `0x1D1`), which area 152's plate table (`0x6373A4`) names too - the linker's fold, in group AR4A's band this wave; not called by ours (a table entry) |
| `DrawHud` | label cell `0x803580` | `0x803580`, the same |
| `DrawDrift` | `0x462` bytes | area 87's (not area 121's two-square body), over `Area151_DriftUV` |

So the "name-set layout" that moved area 88's call sites is gone here too, as
in area 121, but differently: area 151 keeps the items-to-rows loop over one
fixed set. Ours is area 87's body as AR3A copied it (`area_w3a.cpp`) over a
`WorldMapTables` for area 151 (`area_w3g_callees.h`) with area 151's own
place hook. The body is now copied five times in ours (AR2B, AR2E, AR3A,
AR3B and this group); one shared body is the rebinding pass's.

| Function | Area 87 | Area 151 | Size |
|---|---|---|--:|
| `PlaceMessage` (field hook) | `0x40FC60` | `0x423950` | `0x174` / `0x12B` |
| `PlateRun` (record `+0`) | `0x40FDE0` | `0x423A80` | `0xD6` |
| `PlateShow` / `Grow` / `Hold` / `Shrink` (plate states 1..4) | `0x40FF20..` | `0x423B60`, `0x423CB0`, `0x423D00`, `0x423D60` | `0x142`, `0x41`, `0x58`, `0x50` |
| `HudRun` (record `+0xC`) / `HudFrame` | `0x410170` / `0x410190` | `0x423DB0` / `0x423DD0` | `0x12`, `0xA` |
| `FrameStep` / `SlideIn` / `Hold` / `SlideOut` | `0x4101A0..` | `0x423DE0`, `0x423E00`, `0x423E30`, `0x423E60` | `0x12`, `0x21`, `0x28`, `0x41` |
| `BoxStep` / `SlideIn` / `Hold` / `SlideOut` | `0x410270..` | `0x423EB0`, `0x423ED0`, `0x423F40`, `0x423FB0` | `0x12`, `0x62`, `0x6E`, `0x57` |
| `DrawFrame` / `DrawSprite` / `DrawHud` | `0x4103D0` / `0x4105A0` / `0x410660` | `0x424010` / `0x4241E0` / `0x4242A0` | `0x1C5`, `0xBC`, `0x58` |
| `Record8Run` (record `+8`) / `Record8Place` | `0x4106C0` / `0x4106E0` | `0x424300` / `0x424320` | `0x12`, `0x154` |
| `Record4Run` (record `+4`) / `Record4MarkCell` | `0x410840` / `0x410860` | `0x424480` / `0x4244A0` | `0x12`, `0xB5` |
| `DrawDrift` (record `+0x10`) | `0x410920` | `0x424560` | `0x462` |

The state tables' shared entries are other groups': `0x424BA0` (AR4A's band),
`WorldMapHud_Start` `0x419110`, `WorldMap_FrameWait` `0x411310`,
`WorldMapHud_BoxWait` `0x414BB0`, `0x4253C0` (AR4A's band) and
`Area65_Record8Move` `0x40C490`, `Area45_Record4Tick` `0x408990` - none called
by ours.

**The tables** (`[[data]]`, in area 151's data block before and after its
descriptor):

| Table | At | What |
|---|---|---|
| `Area151_PlateAnims` | `0x636A68` | (u16 place, u8 animation, u8) x 7, searched with no bound |
| `Area151_Cells` | `0x636A84` | (x, z, -, id `0x10`) x 1, then the descriptor's `+0x20` data: `Record4MarkCell` by `+0xB`, unchecked |
| `Area151_PlaceMessages` | `0x6370BC` | 7 rows of `0x20` |
| `Area151_NameSet` | `0x63719C` | id `0x10`, five items (the hook reads the items only) |
| `Area151_PlateStates` / `HudStates` / `FrameStates` / `BoxStates` | `0x6371A4` / `0x6371B8` / `0x6371C0` / `0x6371D0` | 5 / 2 / 4 / 4 |
| `Area151_Sprites` / `Buttons` | `0x6371E0` / `0x637238` | 22 x 4 / 6 (read to 8) |
| `Area151_Record8States` / `Directions` / `Record8Anims` / `Record4States` | `0x637250` / `0x63725C` / `0x63726C` / `0x637274` | 3 / 4 x 4 / 4 x 2 / 2 |
| `Area151_DriftUV` | `0x63727C` | u, v, size x 4 |

## 6. Latent defects (described, not fixed)

The owner's rule (round9 doc section 6): ours aborts where the original
would fault; a silent read or write past a table into mapped memory is
reproduced as the original makes it and described here.

1. **`Area148_MemberMessageA` / `B` walk party records to
   `Field_MemberCount` unchecked** against the three `ObjTrio` records (a
   count above 3 reads `+0x89` of the bytes after them). Reproduced.
2. **`Area148_Tail31` state `0xB` reads `Effect_Objects` by the byte
   `0x9039F5`** unchecked (the slot its state `0xA` kept; any other writer of
   that byte - every tail kind shares it - makes it read another record, up to
   "record 255"). Reproduced (a read).
3. **`Area148_BeamStart` hands `+0xB` to `Area148_BeamAhead` as the facing
   unmasked**: the first `Field_DirectionSteps` read is by whatever `+0xB` the
   spawner left (later ones are `& 7`). `Area148_BeamTurnMembers` indexes its
   two tables by `+9 >> 3` and the facing unchecked. Reproduced (reads in
   `.data`).
4. **`Area148_DrawBeamBand` builds its second quad at the first's pointer +
   `0x44`**, not at `Gfx_PacketNext`: it assumes `MapView_LinkPrimAt` moved
   the pointer by exactly the size. Reproduced (ours writes where the
   original writes).
5. **`Area150_ChoiceFill5B` tops up by `16 - count` as a byte**: holding more
   than 16 of item `0x5B` makes the count wrap to 240..255 and hands that to
   `Inventory_Add`. Its two message tables are read by the signed cursor
   unchecked (a negative cursor reads before them). Reproduced.
6. **Area 151's**: the unbounded plate search (`Area151_PlateAnims` has
   seven entries), `Area151_Cells` by `+0xB` unchecked (one record, then the
   descriptor's data), the unchecked `.data` dispatches (ours aborts past each
   of the six tables) - area 45's set, [`area_w1b.md`](area_w1b.md).
7. **Area 148's init is a bare `ret`** where the PSX has one (section 1): for
   the owner's divergence map, not a defect of the PC code as it stands.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D133 (the dispatchers), D135 (`Effect_FindFree`'s none
as a slot), D136 (reads and writes by an unchecked byte or count), D138 (the
bare-`ret` inits), D143 (the world-map copies' searches), D157 (the top-up
wrap), D158 (nibble against facing), D160 (draw slips) in
[`known-defects.md`](known-defects.md).

## 7. The fuzz

`BOF3X_SHADOW=area_w3g` (`src/game/area_w3g_fuzz.cpp`): five `Run` calls
under the one shadow name, `Group::area` each area's number (148 twice), the
real descriptors and tables in place. `BOF3X_AR3G_GROUP=n` runs one alone
(1480 area 148's hooks and tails, 1481 its searchlight, 149, 150, 151; the
controls script's shortcut).

- **Area 148, hooks and tails (1480; 6,000 rounds a function)**: the member
  ids of the three party records one of the tables' four or others,
  `Field_MemberCount` 0..4; the cursor 0, 1, 2, 3 and the sign's edges; the
  tail's state each case and the gaps between (2, 9, `0xF`, `0x13`, `0x16`,
  `0x80`, `0xFF`), the script byte at `0x18` / `0x20` / 0 and one off, the
  timer at 1, 2, 0, `0x100`, `0x101`, the kept slot one of the twenty (its bit
  0 either way) or any byte; the arrive hook's words at and about the box with
  bits above a byte; the switch's cell bytes with bits above, one off, the
  leader's facing 1, `0x11`, `0x81` (the nibble compare). Regions:
  `Effect_Objects`' twenty records, `Draw_PassFlags`, the camera's angle 1.
  `Effect_FindFree` answers `0xFF` or 0..19.
- **Area 148's searchlight (1481; 6,000)**: every state reached through the
  swapped `Area148_BeamStates`; its own callees stood in (the ahead, velocity
  and turn as phases; the draw, fan and band logging their arguments, the
  fan's and band's as low words); `Party_MemberAt` answering a member or none
  and logging `Sprite_Current +0x3C` at each ask; `Rand`'s first answer a
  multiple of 45, one off, or -1; `0x494110` writing either random bits (NaN
  and huge floats) or small screen floats into the vertex; `0x4941E0` writing
  a small or any size; `_ftol` run for real on the clone's side (`kThrough`).
  A packet buffer of the fuzz's with room for the band's second quad; the link
  stand-in logs each primitive and writes its tag (so a copy made before the
  link shows).
- **Area 149 (6,000)**: `Camera_Distance` at `0x87F..0x881`, `0x67F..0x681`,
  0, 1 and the sign's edges; extra record 0's x at the kind-2 x + 2 cells and a
  step either side; `MoveScript_Object` one of the field objects (a region,
  kept there by the settle); `Frame_Counter` & 3 zero half the time; the focus
  at `0x63FF` / `0x77FF` and one off, the dust records live or not, of kind
  `0x37` or next to it; the came-from word `0x94`, `0xA7` and neighbours.
- **Area 150 (6,000)**: the cursor as area 148's; the tail's states and the
  gaps; `Field_Request` 0, 2 and others; `Sound_StreamDone` answering 0, al 0
  with bits above, or not 0; `Inventory_Count` answering 0..`0x12` in al with
  bits above.
- **Area 151 (4,000)**: area 115's group (`area_w3a_fuzz.cpp`, itself area
  88's) over area 151's tables - the six state tables as `DataTable`s, the
  plate animations, cell record, place rows, name set, direction and drift
  tables as regions from the fuzz's own literal addresses (put back to the
  exe's bytes two rounds in three); the place in one of the seven rows or in
  none, items held or not, the set ended early by `0xFF`; `settle` keeps `+1`
  inside the plate table and the place in the last animation entry (the search
  has no bound).

**Result (in this worktree):** 0 mismatches in every run; rounds / calls to
the stand-ins in the `'*'` run: 1480 48,000 / 14,405; 1481 60,000 /
532,022; 149 54,000 / 69,491; 150 30,000 / 7,682; 151 96,000 / 509,821 (the
runs share the harness's random stream, so a change to one moves the next
ones' counts). Every state-table entry reached (the beam's states about 2,000
each, the dust states about 1,500, the world map's plate states about 800, the
frame and box states about 1,000, record-8 about 1,330, record-4 about 2,000);
the band and fan 3,463 calls each, `Math_Ratan2` 3,463, `Member_SetState2_8`
1,047, `Field_ChangeArea` 29 / 617 / 387 in 1480 / 149 / 150.
`BOF3X_SHADOW='*'`: exit 0, 491 self-test lines, no mismatch or Fatal,
`inject: 5414 ours` (all 56 injected; 5,358 before).

## 8. What reaches it, calls across groups

- **Reach - no recorded route reaches any of the 56.** A scan of every
  `analysis/calltrace/*/*.tsv` (and `hidden_reached*.json`) for addresses in
  `0x4223A0..0x4249D0` finds none (311 files). Fuzz only.
- **Cross-group raw-address calls** (`area_w3g_callees.h`): `0x494060`,
  `0x494110`, `0x4941E0` (the map camera, a point projection, a screen size at
  a point - engine code nobody owns; `0x4941E0` read for this group) and
  `0x56FCA0` (the view shift after a focus change, engine, nobody's). No call
  into another group's band this wave. Named and ours: `Msg_OpenScript`, the
  `Flags_*` set, `Party_DropIn`, `Party_MemberAt`, `Party_HealJoined`,
  `Field_ChangeArea`, `Effect_FindFree`, `Kind2_Place`, `ScriptFlags_*`,
  `Sound_*`, `Music_Play`, `Transition_Start`, `Gfx_ClutAdjust`,
  `AreaMap_Elevation`, `MapView_SetElevation`, `MapView_LinkPrimAt`, the
  `Gpu_*` set, `Math_Sin` / `Cos` / `Ratan2`, `Member_SetState2_8`,
  `Inventory_Count` / `Add`, `Rand`, and the world map's (`WorldMap_*`,
  `Item_NamePtr`, `Gte_*`, `Prim_SetTexture`, `Text_DrawAt`, ...).
- **Inbound from outside the band** (for the rebinding pass): engine
  `0x56ABC4` (`Scena15_Runs`, and ours `scena_sc15_callees.h` `kViewShift`)
  into `Area149_ViewShiftBack`; `Area_StepHook` `0x56E1B2` (ours
  `event_ops.cpp` `kStepHandlers`) into `Area150_StepHook`;
  `Area_ArriveHook` `0x56E551` (`kArriveHandlers`) into `Area148_ArriveHook`.
  Named by tables: `Field_ModeTailKinds` 31 / 33 / 58, `Area_CellHooks` pair
  24, `Field_ObjectTriggers` id 17, `Effect_KindHandlers[0x7E]`,
  `WorldMap_FieldHooks[9]`, `WorldMap_Records` record 9, the descriptors (read
  in place, no rebinding).
- **Outbound folds**: area 151's plate state 0 `0x424BA0` and record-8 state 0
  `0x4253C0` are AR4A's band (table entries, not calls).
- `analysis/calltrace/entries_logic.txt`: 48 lines appended; eight extents
  were there already (`0x4229C0 5D`, `0x422A20 2D`, `0x422A50 F9`, `0x422B50
  232`, `0x422D90 252`, `0x4232B0 CC`, `0x424010 1C5`, `0x4241E0 BC`). Host
  lines with larger extents at the same starts (`00422FF0 2B6`, `00423380
  58D`, `00423910 4CA`, `00423DE0 227`, `004242A0 BDA`) cover the functions
  after them: the smaller extents are appended beside them.

## 9. Controls

Planted one at a time in `area_w3g.cpp` (and, for the table constants,
`area_w3g_callees.h`) by a script (the scratch `controls.py` / `plants.py`,
not committed): each anchored on a string the file holds once; plant,
rebuild (checking `area_w3g.cpp` recompiled), run the control's group alone
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w3g BOF3X_AR3G_GROUP=n`), restore;
after the last, a rebuild and a clean full run (exit 0, 0 mismatches in all
five runs). The world-map controls are AR2B's / AR3B's set (their shapes
kept, renumbered `G..`) plus area 151's own (its place hook, its table
constants). **296 planted, 295 refused by a count, 1 equivalent** with its
near variant refused by a count.

On the first run 293 of 294 that built were refused by a count on first
planting; one plant (A50) did not compile as first written and was replanted.
**B24 is equivalent**: `Area148_BeamAhead` takes the elevation's low word
signed and shifts it left 16; taking it unsigned gives the same dword for
every input (the extension bits are all shifted out). Its variant B24b (the
low *byte*, signed) is refused. The table is that run (in this worktree;
rounds of a function's 6,000, 4,000 in the world map).

| # | group | planted | refused in rounds |
|---|---|---|---|
| # | group | planted | refused in rounds |
|---|---|---|---|
| A1 | 148 | MessageA: B ids | Area148_MemberMessageA 2246 |
| A2 | 148 | MessageB: A messages | Area148_MemberMessageB 2462 |
| A3 | 148 | MessageBy: three ids | Area148_MemberMessageA 537; Area148_MemberMessageB 515 |
| A4 | 148 | MessageBy: one record fewer | Area148_MemberMessageA 1024; Area148_MemberMessageB 1042 |
| A5 | 148 | MessageBy: request 3 | Area148_MemberMessageA 2387; Area148_MemberMessageB 2462 |
| A6 | 148 | MessageBy: id from +0x88 | Area148_MemberMessageA 2454; Area148_MemberMessageB 2521 |
| A7 | 148 | Choice0: 5 | Area148_ChoiceMessage 785 |
| A8 | 148 | Choice0: cursor 1 only | Area148_ChoiceMessage 4426 |
| A9 | 148 | Choice1: flag 0x58 | Area148_ChoiceSetFlag59 778 |
| A10 | 148 | Choice1: message 6 | Area148_ChoiceSetFlag59 743 |
| A11 | 148 | Choice1: the message kept | Area148_ChoiceSetFlag59 1735 |
| A12 | 148 | Tail31: state 2 after the drop | Area148_Tail31 211 |
| A13 | 148 | Tail31: DropIn(1) | Area148_Tail31 211 |
| A14 | 148 | Tail31: waits on 0x19 | Area148_Tail31 61 |
| A15 | 148 | Tail31: flag 0x47 | Area148_Tail31 29 |
| A16 | 148 | Tail31: z + 1 | Area148_Tail31 29 |
| A17 | 148 | Tail31: not disarmed | Area148_Tail31 29 |
| A18 | 148 | Tail31: slot not kept | Area148_Tail31 209 |
| A19 | 148 | Tail31: x + 1 | Area148_Tail31 201 |
| A20 | 148 | Tail31: life 0x31 | Area148_Tail31 201 |
| A21 | 148 | Kind13: kind 0x14 | Area148_Tail31 275 |
| A22 | 148 | Kind13: angle unsigned | Area148_Tail31 127 |
| A23 | 148 | Tail31: bit 1 | Area148_Tail31 72 |
| A24 | 148 | Tail31: Kind2_Place(3) | Area148_Tail31 147 |
| A25 | 148 | Tail31: waits on 0x21 | Area148_Tail31 45 |
| A26 | 148 | Tail31: flag 0x5B | Area148_Tail31 20 |
| A27 | 148 | Tail31: timer 0x21 | Area148_Tail31 20 |
| A28 | 148 | Tail31: y 0x201 | Area148_Tail31 74 |
| A29 | 148 | Tail31: state 0xF | Area148_Tail31 74 |
| A30 | 148 | Tail31: waits on 1 | Area148_Tail31 38 |
| A31 | 148 | Tail31: toggles 0x6B | Area148_Tail31 213 |
| A32 | 148 | Tail31: sound 0x205 | Area148_Tail31 213 |
| A33 | 148 | Tail31: red -5 | Area148_Tail31 140 |
| A34 | 148 | Tail31: rows 0x181 | Area148_Tail31 73 |
| A35 | 148 | Tail31: pass flags 1 | Area148_Tail31 213 |
| A36 | 148 | Tail31: timer 0x10 | Area148_Tail31 213 |
| A37 | 148 | Tail31: pass flags 0x1E | Area148_Tail31 70 |
| A38 | 148 | Timer: a byte | Area148_Tail31 474 |
| A39 | 148 | Arrive: z 0x1D | Area148_ArriveHook 759 |
| A40 | 148 | Arrive: four columns | Area148_ArriveHook 149 |
| A41 | 148 | Arrive: al 2 | Area148_ArriveHook 511 |
| A42 | 148 | Arrive: x a byte | Area148_ArriveHook 223 |
| A43 | 148 | Trigger: state 0xB | Area148_Trigger17 6000 |
| A44 | 148 | Trigger: kind 0x20 | Area148_Trigger17 6000 |
| A45 | 148 | Trigger: al 1 | Area148_Trigger17 6000 |
| A46 | 148 | Switch: facing a nibble | Area148_SwitchHook 373 |
| A47 | 148 | Switch: z against x | Area148_SwitchHook 482 |
| A48 | 148 | Switch: the state byte 2 | Area148_SwitchHook 482 |
| A49 | 148 | Switch: none al 1 | Area148_SwitchHook 5518 |
| A50 | 148 | Switch: x a word | Area148_SwitchHook 233 |
| B24b | 148 beam | Ahead: height a signed byte | Area148_BeamAhead 5978 |
| A51 | 148 | Switch: no Set40 | Area148_SwitchHook 482 |
| B1 | 148 beam | BeamRun: the next state | Area148_BeamRun 6000 |
| B2 | 148 beam | Start: +8 from +0xA | Area148_BeamStart 5174 |
| B3 | 148 beam | Start: height + 0xC1 | Area148_BeamStart 6000 |
| B4 | 148 beam | Start: facing + 4 | Area148_BeamStart 5922 |
| B5 | 148 beam | Start: colour 3 | Area148_BeamStart 6000 |
| B6 | 148 beam | Start: +9 0x51 | Area148_BeamStart 6000 |
| B7 | 148 beam | Start: no velocity | Area148_BeamStart 6000 |
| B8 | 148 beam | Sweep: request 1 moves | Area148_BeamSweep 9 |
| B9 | 148 beam | Sweep: x by +0x10 | Area148_BeamSweep 4009 |
| B10 | 148 beam | Sweep: +9 - 2 | Area148_BeamSweep 3915 |
| B11 | 148 beam | Sweep: facing + 3 | Area148_BeamSweep 761 |
| B12 | 148 beam | Sweep: Rand % 44 | Area148_BeamSweep 773 |
| B13 | 148 beam | Sweep: +9 0x4F | Area148_BeamSweep 767 |
| B14 | 148 beam | Sweep: no pause state | Area148_BeamSweep 743 |
| B15 | 148 beam | Sweep: always a pause | Area148_BeamSweep 38 |
| B16 | 148 beam | Sweep: Rand unsigned | Area148_BeamSweep 38 |
| B17 | 148 beam | Pause: - 2 | Area148_BeamPause 6000 |
| B18 | 148 beam | Pause: +9 + 2 | Area148_BeamPause 1473 |
| B19 | 148 beam | Pause: stays | Area148_BeamPause 1473 |
| B20 | 148 beam | Pause: no member turn | Area148_BeamPause 6000 |
| B21 | 148 beam | Ahead: four steps | Area148_BeamAhead 4835 |
| B22 | 148 beam | Ahead: z from +0x3C | Area148_BeamAhead 6000 |
| B23 | 148 beam | Ahead: << 15 | Area148_BeamAhead 6000 |
| B24 | 148 beam | Ahead: height unsigned | equivalent: 0 mismatches in 6,000 rounds (the sign bits are shifted out); variant B24b refused |
| B25 | 148 beam | Velocity: << 2 | Area148_BeamVelocity 4486 |
| B26 | 148 beam | Velocity: z from the next | Area148_BeamVelocity 5819 |
| B27 | 148 beam | Turn: facing & 7 | Area148_BeamTurnMembers 2967 |
| B28 | 148 beam | Turn: +9 >> 2 | Area148_BeamTurnMembers 5474 |
| B29 | 148 beam | Turn: +0x3C not set | Area148_BeamTurnMembers 6000 |
| B30 | 148 beam | Turn: +0x3C not put back | Area148_BeamTurnMembers 6000 |
| B31 | 148 beam | Turn: flag bit 11 | Area148_BeamTurnMembers 501 |
| B32 | 148 beam | Turn: request ignored | Area148_BeamTurnMembers 1050 |
| B33 | 148 beam | Turn: even - 1 | Area148_BeamTurnMembers 373 |
| B34 | 148 beam | Turn: +1 2 | Area148_BeamTurnMembers 810 |
| B35 | 148 beam | Turn: state 3 | Area148_BeamTurnMembers 723 |
| B36 | 148 beam | Turn: dz from dx | Area148_BeamTurnMembers 4763 |
| B37 | 148 beam | Turn: stored before the test | Area148_BeamTurnMembers 738 |
| B38 | 148 beam | DrawBeam: pass flags ignored | Area148_DrawBeam 809 |
| B39 | 148 beam | DrawBeam: flag 0x6B | Area148_DrawBeam 5191 |
| B40 | 148 beam | DrawBeam: first dtd 0 | Area148_DrawBeam 3515 |
| B41 | 148 beam | DrawBeam: line semi | Area148_DrawBeam 3515 |
| B42 | 148 beam | DrawBeam: link 0x24 | Area148_DrawBeam 3515 |
| B43 | 148 beam | DrawBeam: dx reversed | Area148_DrawBeam 2398 |
| B44 | 148 beam | DrawBeam: dy less x0 | Area148_DrawBeam 2467 |
| B45 | 148 beam | DrawBeam: Ratan2 (dx, dy) | Area148_DrawBeam 2936 |
| B46 | 148 beam | DrawBeam: offset 0x41 | Area148_DrawBeam 3515 |
| B47 | 148 beam | DrawBeam: r1 without the frame bit | Area148_DrawBeam 2561 |
| B48 | 148 beam | DrawBeam: r2 by bit 1 | Area148_DrawBeam 3054 |
| B49 | 148 beam | DrawBeam: fan angle + 0xC01 | Area148_DrawBeam 3515 |
| B50 | 148 beam | DrawBeam: band angle + 0x401 | Area148_DrawBeam 3515 |
| B51 | 148 beam | DrawBeam: band x0 from the depth | Area148_DrawBeam 2214 |
| B52 | 148 beam | DrawBeam: last dtd 1 | Area148_DrawBeam 3515 |
| B53 | 148 beam | Colour: the next byte | Area148_DrawBeam 3476; Area148_DrawBeamBand 5930; Area148_DrawBeamFan 5985 |
| B54 | 148 beam | FtolDiff: rounded, not truncated | Area148_DrawBeam 1283 |
| B55 | 148 beam | Ftol: rounded down | Area148_DrawBeam 2503 |
| B56 | 148 beam | Band: second at + 0x40 | Area148_DrawBeamBand 6000 |
| B57 | 148 beam | Band: copied before the link | Area148_DrawBeamBand 5814 |
| B58 | 148 beam | Band: a2 + 0x800 masked | Area148_DrawBeamBand 832 |
| B59 | 148 beam | Band: r2 for r1 | Area148_DrawBeamBand 5968 |
| B60 | 148 beam | Band: v1 red 0 | Area148_DrawBeamBand 4071 |
| B61 | 148 beam | Band: second y from x0 | Area148_DrawBeamBand 5998 |
| B62 | 148 beam | Scaled: >> 11 | Area148_DrawBeamBand 6000; Area148_DrawBeamFan 5972 |
| B63 | 148 beam | Band: x1 whole | Area148_DrawBeamBand 2033 |
| B64 | 148 beam | Fan: seven | Area148_DrawBeamFan 6000 |
| B65 | 148 beam | Fan: step 0x80 | Area148_DrawBeamFan 6000 |
| B66 | 148 beam | Fan: rim not black | Area148_DrawBeamFan 6000 |
| B67 | 148 beam | Fan: link 0x30 | Area148_DrawBeamFan 6000 |
| B68 | 148 beam | Fan: opaque | Area148_DrawBeamFan 6000 |
| B69 | 148 beam | Fan: r unsigned | Area148_DrawBeamFan 1005 |
| B70 | 148 beam | Turn: +0x3C from +0x1C | Area148_BeamTurnMembers 6000 |
| C1 | 149 | DustRun: the next state | Area149_DustRun 6000 |
| C2 | 149 | Near880: at 0x880 too | Area149_DustNear880 319 |
| C3 | 149 | Near880: +0x84 2 | Area149_DustNear880 4113 |
| C4 | 149 | Near880: counter 0xB | Area149_DustNear880 4109 |
| C5 | 149 | Near880: sound 2 | Area149_DustNear880 4113 |
| C6 | 149 | Near880: dust -5 | Area149_DustNear880 6000 |
| C7 | 149 | Near680: 0x681 | Area149_DustNear680 343 |
| C8 | 149 | Near680: counter 0x10 | Area149_DustNear680 3014 |
| C9 | 149 | Near680: sound 1 | Area149_DustNear680 3017 |
| C10 | 149 | Near680: +4 + 2 | Area149_DustNear680 3003 |
| C11 | 149 | AtZero: <= 0 | Area149_DustAtZero 1669 |
| C12 | 149 | AtZero: counter 0x15 | Area149_DustAtZero 668 |
| C13 | 149 | AtZero: dust -1 | Area149_DustAtZero 6000 |
| C14 | 149 | Hold: dust -2 | Area149_DustHold 6000 |
| C15 | 149 | Mark: one cell | Area149_DustNear680 1182; Area149_DustAtZero 1174 |
| C16 | 149 | Mark: 4 | Area149_DustNear680 2154; Area149_DustAtZero 2153 |
| C17 | 149 | Mark: unsigned | Area149_DustNear680 956; Area149_DustAtZero 936 |
| C18 | 149 | Again: - 1 | Area149_DustNear880 6000; Area149_DustNear680 6000; Area149_DustAtZero 6000; Area149_DustHold 6000 |
| C19 | 149 | Again: +0xC | Area149_DustNear880 6000; Area149_DustNear680 6000; Area149_DustAtZero 6000; Area149_DustHold 6000 |
| C20 | 149 | Spawn: & 7 | Area149_SpawnDust 2205 |
| C21 | 149 | Spawn: once | Area149_SpawnDust 4536 |
| C22 | 149 | Spawn: kind 0x38 | Area149_SpawnDust 4534 |
| C23 | 149 | Spawn: dx << 15 | Area149_SpawnDust 4126 |
| C24 | 149 | Spawn: dx unsigned | Area149_SpawnDust 2598 |
| C25 | 149 | Spawn: behind first | Area149_SpawnDust 4534 |
| C26 | 149 | Spawn: +0x2C 0x2C1 | Area149_SpawnDust 4534 |
| C27 | 149 | Spawn: +0x29 7 | Area149_SpawnDust 4534 |
| C28 | 149 | Spawn: height a byte | Area149_SpawnDust 4532 |
| C29 | 149 | Spawn: +6 1 | Area149_SpawnDust 4534 |
| C30 | 149 | ShiftBack: focus 0x63FE | Area149_ViewShiftBack 1714 |
| C31 | 149 | ShiftBack: no dust | Area149_ViewShiftBack 1142 |
| C32 | 149 | Shift: origin 0x15 | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C33 | 149 | Shift: 0x13 cells | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C34 | 149 | Shift: focus not written | Area149_ViewShiftBack 1070; Area149_Tail33 233 |
| C35 | 149 | Shift: height at the old x | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C36 | 149 | Shift: elevation a word | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C37 | 149 | Shift: no view shift | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C38 | 149 | Shift: kind 0x36 | Area149_ViewShiftBack 1144 |
| C39 | 149 | Shift: nineteen records | Area149_ViewShiftBack 265 |
| C40 | 149 | Shift: bit 1 | Area149_ViewShiftBack 1130 |
| C41 | 149 | Shift: extra z from +0x3C | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C42 | 149 | Shift: extra not moved | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C43 | 149 | Shift: sprite not moved | Area149_ViewShiftBack 1144; Area149_Tail33 240 |
| C44 | 149 | Tail33: focus 0x77FE | Area149_Tail33 136 |
| C45 | 149 | Tail33: state 3 | Area149_Tail33 172 |
| C46 | 149 | Tail33: area 0xA8 | Area149_Tail33 310 |
| C47 | 149 | Tail33: script byte 0x21 | Area149_Tail33 310 |
| C48 | 149 | Tail33: 0xA shifts on | Area149_Tail33 104 |
| C49 | 149 | Tail33: state 0xD | Area149_Tail33 152 |
| C50 | 149 | Tail33: x 0xB0000 | Area149_Tail33 313 |
| C51 | 149 | Tail33: not disarmed | Area149_Tail33 313 |
| C52 | 149 | Init: from 0x95 | Area149_Init 1543 |
| C53 | 149 | Init: + 0x17 | Area149_Init 1022 |
| C54 | 149 | Init: +0x83 2 | Area149_Init 1019 |
| C55 | 149 | Init: FE 3 | Area149_Init 1019 |
| C56 | 149 | Init: flag 0x12 | Area149_Init 1022 |
| C57 | 149 | Init: the 0x904030 bank | Area149_Init 1022 |
| C58 | 149 | Init: timer 0x5B | Area149_Init 692 |
| C59 | 149 | Init: from 0xA6 | Area149_Init 1481 |
| C60 | 149 | Init: state 0xB | Area149_Init 969 |
| C61 | 149 | Init: x read before the add | Area149_Init 1022 |
| C62 | 149 | Init: +0x8A 1 | Area149_Init 969 |
| D1 | 150 | Fill: messages B | Area150_ChoiceFill5B 5601 |
| D2 | 150 | Fill: cursor unsigned | Area150_ChoiceFill5B 2189 |
| D3 | 150 | Fill: counts 0x5C | Area150_ChoiceFill5B 800 |
| D4 | 150 | Fill: to 15 | Area150_ChoiceFill5B 800 |
| D5 | 150 | Fill: sound 0x107 | Area150_ChoiceFill5B 551 |
| D6 | 150 | Fill: the sound always | Area150_ChoiceFill5B 249 |
| D7 | 150 | Fill: the count a word | Area150_ChoiceFill5B 797 |
| D8 | 150 | Message: table A | Area150_ChoiceMessage 5716 |
| D9 | 150 | TailState: 0xD | Area150_ChoiceTailState 796 |
| D10 | 150 | TailState: the message kept | Area150_ChoiceTailState 1966 |
| D11 | 150 | Tail58: message 0x2E | Area150_Tail58 263 |
| D12 | 150 | Tail58: 0xA to 0xC | Area150_Tail58 263 |
| D13 | 150 | Tail58: 0xC waits on 3 | Area150_Tail58 161 |
| D14 | 150 | Tail58: area 0xBE | Area150_Tail58 399 |
| D15 | 150 | Tail58: flag2 bit 7 | Area150_Tail58 296 |
| D16 | 150 | Tail58: 0x904153 0xB | Area150_Tail58 399 |
| D17 | 150 | Tail58: 0x90405C 1 | Area150_Tail58 399 |
| D18 | 150 | Tail58: 0x90405F 0xF1 | Area150_Tail58 399 |
| D19 | 150 | Tail58: 0x90405E 1 | Area150_Tail58 399 |
| D20 | 150 | Tail58: 0x929EC1 1 | Area150_Tail58 399 |
| D21 | 150 | Tail58: 0x9036D0 1 | Area150_Tail58 399 |
| D22 | 150 | Tail58: status bit 1 | Area150_Tail58 304 |
| D23 | 150 | Tail58: 0xE no Clear40 | Area150_Tail58 196 |
| D24 | 150 | Tail58: 0x14 with request 1 | Area150_Tail58 28 |
| D25 | 150 | Tail58: flag 4 | Area150_Tail58 58 |
| D26 | 150 | Tail58: messages swapped | Area150_Tail58 58 |
| D27 | 150 | Tail58: 0x14 to 0x16 | Area150_Tail58 58 |
| D28 | 150 | Tail58: no heal | Area150_Tail58 215 |
| D29 | 150 | Tail58: stream 1 | Area150_Tail58 215 |
| D30 | 150 | Tail58: done by al | Area150_Tail58 90 |
| D31 | 150 | Tail58: counter 2 | Area150_Tail58 430 |
| D32 | 150 | Tail58: music 0x96 | Area150_Tail58 432 |
| D33 | 150 | Tail58: transition 2 | Area150_Tail58 432 |
| D34 | 150 | Tail58: pass flags 0x1E | Area150_Tail58 432 |
| D35 | 150 | Tail58: flag 0x8B | Area150_Tail58 432 |
| D36 | 150 | Tail58: flag 3 of 0x904030 | Area150_Tail58 432 |
| D37 | 150 | Step: x from 0x18 | Area150_StepHook 482 |
| D38 | 150 | Step: seven rows | Area150_StepHook 256 |
| D39 | 150 | Step: kind 0x3B | Area150_StepHook 1475 |
| D40 | 150 | Step: state 0xB | Area150_StepHook 1475 |
| D41 | 150 | Step: al 2 | Area150_StepHook 1475 |
| D42 | 150 | Step: z a byte | Area150_StepHook 508 |
| G1 | 151 map | PlaceMessage: message 0x17 | Area151_PlaceMessage 814 |
| G2 | 151 map | kWm151: four text rows | Area151_PlaceMessage 617 |
| G3 | 151 map | kWm151: items from the id byte | Area151_PlaceMessage 805 |
| G4 | 151 map | PlaceMessage: 0xFE ends | Area151_PlaceMessage 200 |
| G5 | 151 map | PlaceMessage: unseen name | Area151_PlaceMessage 635 |
| G6 | 151 map | PlaceMessage: key item 0x17 | Area151_PlaceMessage 313 |
| G7 | 151 map | PlaceMessage: name + 0x37 | Area151_PlaceMessage 722 |
| G8 | 151 map | PlaceMessage: state 2 as 1 | Area151_PlaceMessage 251 |
| G9 | 151 map | PlaceMessage: waits on 3 | Area151_PlaceMessage 244 |
| G10 | 151 map | PlaceMessage: request 3 | Area151_PlaceMessage 1005 |
| G11 | 151 map | PlaceMessage: Cond_ByteFA unsigned | Area151_PlaceMessage 61 |
| G12 | 151 map | kWm151: six place rows | Area151_PlaceMessage 63 |
| G13 | 151 map | kWm151: plate animations from the second | Area151_PlateShow 133 |
| G14 | 151 map | kWm151: cells from the second | Area151_Record4MarkCell 2719 |
| G15 | 151 map | kWm151: sprites from the second | Area151_DrawSprite 3894 |
| G16 | 151 map | kWm151: buttons from the second | Area151_DrawFrame 648 |
| G17 | 151 map | kWm151: directions from the second | Area151_Record8Place 3874 |
| G18 | 151 map | kWm151: record-8 animations from the second | Area151_Record8Place 3714 |
| G19 | 151 map | kWm151: drift size from the next | Area151_DrawDrift 813 |
| G20 | 151 map | kWm151: label 0x803584 | Area151_DrawHud 2863 |
| G21 | 151 map | HudFrame: the box first | Area151_HudFrame 4000 |
| G22 | 151 map | PlateRun: 0xA0 kind 3 | Area151_PlateRun 320 |
| G23 | 151 map | PlateRun: bit 11 | Area151_PlateRun 1263 |
| G24 | 151 map | PlateRun: the next state | Area151_PlateRun 4000 |
| G25 | 151 map | PlateShow: kind 2 animation 2 | Area151_PlateShow 346 |
| G26 | 151 map | PlateShow: +9 7 | Area151_PlateShow 1995 |
| G27 | 151 map | PlateShow: +0x44 | Area151_PlateShow 2005 |
| G28 | 151 map | PlateGrow: step 0x1000 | Area151_PlateGrow 4000 |
| G29 | 151 map | PlateGrow: no pin | Area151_PlateGrow 4000 |
| G30 | 151 map | PlateHold: request 4 | Area151_PlateHold 342 |
| G31 | 151 map | PlateHold: Game_Mode 2 holds | Area151_PlateHold 797 |
| G32 | 151 map | PlateShrink: released on 4 | Area151_PlateShrink 271 |
| G33 | 151 map | PlateShrink: back to 2 | Area151_PlateShrink 590 |
| G34 | 151 map | HudRun: the other entry | Area151_HudRun 4000 |
| G35 | 151 map | FrameStep: the next entry | Area151_FrameStep 4000 |
| G36 | 151 map | FrameSlideIn: above 0x10 | Area151_FrameSlideIn 379 |
| G37 | 151 map | FrameHold: mode 3 | Area151_FrameHold 652 |
| G38 | 151 map | FrameSlideOut: below -0x30 | Area151_FrameSlideOut 61 |
| G39 | 151 map | BoxStep: the next entry | Area151_BoxStep 4000 |
| G40 | 151 map | BoxSlideIn: below 0xC8 | Area151_BoxSlideIn 112 |
| G41 | 151 map | BoxHold: 0x59 frames | Area151_BoxHold 248 |
| G42 | 151 map | BoxSlideOut: above 0xF0 | Area151_BoxSlideOut 377 |
| G43 | 151 map | BoxLeaves: bit 9 | Area151_BoxSlideIn 924; Area151_BoxHold 834 |
| G44 | 151 map | DrawFrame: second key over seven | Area151_DrawFrame 1433 |
| G45 | 151 map | DrawFrame: legend 3 y + 0x17 | Area151_DrawFrame 2547 |
| G46 | 151 map | DrawFrame: party set & 0xFF | Area151_DrawFrame 77 |
| G47 | 151 map | DrawSprite: CLUT 0x7B81 | Area151_DrawSprite 4000 |
| G48 | 151 map | DrawSprite: semi by & 0x7F | Area151_DrawSprite 4 |
| G49 | 151 map | DrawHud: cap at x + 0x7F | Area151_DrawHud 2863 |
| G50 | 151 map | DrawHud: label & 0xFFF | Area151_DrawHud 2693 |
| G51 | 151 map | Record8Place: bank 0x47 | Area151_Record8Place 4000 |
| G52 | 151 map | Record8Place: first nudge >> 12 | Area151_Record8Place 1844 |
| G53 | 151 map | Record8Place: up for +6 2 | Area151_Record8Place 1554 |
| G54 | 151 map | Record4MarkCell: cell 0xA1 | Area151_Record4MarkCell 2781 |
| G55 | 151 map | Record4MarkCell: released on 8 | Area151_Record4MarkCell 1085 |
| G56 | 151 map | Record4MarkCell: bank 0x204 | Area151_Record4MarkCell 2781 |
| G57 | 151 map | DrawDrift: wrap to -7 | Area151_DrawDrift 1594 |
| G58 | 151 map | DrawDrift: x within 24 | Area151_DrawDrift 531 |
| G59 | 151 map | DrawDrift: CLUT 0x78CC | Area151_DrawDrift 918 |
| G60 | 151 map | DrawDrift: commit 0x44 | Area151_DrawDrift 1755 |
| G61 | 151 map | DrawDrift: size from the u table | Area151_DrawDrift 918 |
| G62 | 151 map | DrawDrift: +0x38 by b << 9 | Area151_DrawDrift 3066 |
| G63 | 151 map | Record8Run: the next entry | Area151_Record8Run 4000 |
| G64 | 151 map | Record4Run: the other entry | Area151_Record4Run 4000 |
| G65 | 151 map | PlateShrink: no overlay | Area151_PlateShrink 3139 |
| G66 | 151 map | PlateShow: +7 from +0xA | Area151_PlateShow 3990 |
| G67 | 151 map | DrawFrame: needle y + 0x17 | Area151_DrawFrame 2914 |
| G68 | 151 map | FrameSlideIn: tail to the step | Area151_FrameSlideIn 4000 |
| G69 | 151 map | FrameHold: x 0x11 | Area151_FrameHold 4000 |
| G70 | 151 map | BoxSlideIn: - 9 | Area151_BoxSlideIn 4000 |
