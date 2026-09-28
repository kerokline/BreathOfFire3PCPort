# World 3, areas 148..151: a light switch and a searchlight, a dust shake and a view shift, a top-up choice, the world map's tenth copy

**Status:** IN PROGRESS (2026-09-28) - 56 functions ours
(`src/game/area_w3g.cpp`, shadow name `area_w3g`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), five `Run` calls:
0 mismatches in 288,000 rounds; CONTROLS_SUMMARY (section 9). Fuzz only: no
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

CONTROLS_SECTION
