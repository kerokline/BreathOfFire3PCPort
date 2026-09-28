# World 4, areas 175..187: the band `0x4292C0..0x42A320`

**Status:** IN PROGRESS (2026-09-28) - 49 functions ours
(`src/game/area_w4d.cpp`, shadow name `area_w4d`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area
(thirteen): 0 mismatches in 294,000 rounds (in this worktree); 286 controls planted, 279 refused by a count, 6 equivalent (each with a refused variant), 1 a no-op plant replaced
(section 4). Fuzz only: no recorded route reaches the band (section 8). No
divergence; area 175's two-state glide dispatcher aborts past its table, and
its script message aborts on an area number past `Area_Descriptors`
(section 6).

Group AR4D of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
49 starts, none ours before, **49 taken**; no start dropped, none added
(section 7).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`area_funcs.tsv`) and agrees with
the reading; the clone tables are `area_rows.py --clones`'s rows, each read
against the disassembly. What an area *is* in the story is not read here.
The PSX twins are the sibling's `names/area_records.toml` (descriptor
handlers and inits only; choices, hooks, tails, triggers and state tables
have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase, `kHook` a step or cell hook `(x, z)` answering
in `al`, `kState` an entry of a `.data` state table (the area's own, or
`EffectKind18_States`), `kCallee` a function called directly (by the
group's own code, or as an object trigger `(object, 0x904030)` answering in
`al`). A function that is both a choice and a handler is fuzzed as a handler
when it does not write the message word.

### 1.1 Areas 175..185: one body eleven times over

The eleven descriptors (`0x642450`, `0x6427C0`, `0x642B18`, `0x642E70`,
`0x6431C8`, `0x643520`, `0x643878`, `0x643BD0`, `0x643F40`, `0x6442B0`,
`0x644628`) name **the same code**: the same init (`0x4298A0`), the same two
handlers (0 `0x429810`, 1 `0x429320`), and a choice table of the same
bodies but for **entry 27**, where each area names its own copy (`0x5C`
bytes each, byte for byte the same but for the address of the five-entry
jump table at `+0x48`, measured by a byte compare of the eleven). Area 175's
table has 37 entries (its six own handlers are choices 31..36); the other
ten have 31. As in the other worlds, the `+0x34` array runs into the `+0x3C`
array (choices 29 and 30 are handlers 0 and 1). `Area_StepHook` names one
step hook (`0x429DC0`) for all eleven, `Area_CellHooks` one cell hook
(`0x4298D0`) for areas `0xAF..0xB9`. The PSX files agree: areas 176..185
pair one init `0x801F32D8` and handlers `0x801F3328` / `0x801F3348`
(descriptors `0x801F376C`, `0x801F3784` for 177..182, `0x801F379C` for 183
and 184, `0x801F3A44`); area 175 has its own section (`0x801F531C`, init
`0x801F44D8`, handlers 0..7 `0x801F3FBC`..`0x801F446C`). Choices 4, 7, 8,
11, 12, 16, 17, 23 and 25 are `0x4291F0`..`0x429290` (the block below the
band, AR4C's), and 13 and 18 the shared `ret` `0x437CC0`; all read in place.

The shared bodies are named for the area whose block holds them (the tool's
unit), and fuzzed under that area's number.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x4292C0` .. `0x429D10` (eleven) | `Area175_ChoiceMessageByAnswer` .. `Area185_ChoiceMessageByAnswer` | `0x5C` each | each area's choice 27 | kChoice | the answer read signed: 0..3 message `0xFA`..`0xFD`, 4 message `0xF9`; anything else (a negative answer is above 4 unsigned) leaves the word; ours one body behind eleven names |
| `0x429910` | `Area179_ChoiceVar8` | `0x1C` | choice 0 | kChoice | message `0xFFFF`; the byte after `MoveScript_Var7` (`0x8034E5`) 3 for answer 0, else 2 |
| `0x429A10` | `Area181_ChoiceCounter0` | `0x30` | choice 1 | kChoice | message `0xFFFF`; answer 0: counter 0 `0x28` and `0x8034E5` `0x15`; else `0x32` and `0x19` |
| `0x429BA0` | `Area184_ChoiceMessage5A` | `0x29` | choice 2 | kChoice | answer 1: message `0x5A` and the focus object's trigger id `+0x86` `0xFF`; else `0xFFFF` |
| `0x429BD0` | `Area184_ChoiceTailState1` | `0x25` | choice 3 | kChoice | answer 1: message `0x5F`, tail state 1; else `0xFFFF` |
| `0x429700` | `Area176_ChoiceCount3E` | `0x30` | choice 5 | kChoice | answer 0: message `0xFFFF`, facility byte `+4` 0; else message `0x5F`, facility byte `+2` up one |
| `0x429C00` | `Area184_ChoiceMessage7A` | `0x29` | choice 6 | kChoice | answer 0: message `0x7A`; else `0x6E` and the focus trigger id cleared |
| `0x429C30` | `Area184_ChoiceMessage77` | `0x24` | choice 9 | kChoice | answer 0: `0xFFFF`; else message `0x77`, facility byte `+4` `0xA` |
| `0x429630` | `Area175_ChoiceAskZenny6F` | `0x4D` | choice 10 | kChoice | answer 0: message `0x6F`, tail sub-kind 0; `Party_Zenny` 500 or more (unsigned): `0x7D`, sub-kind 1; else `0x7C` and the focus trigger id cleared |
| `0x429C60` | `Area184_ChoiceAskZenny7F` | `0x48` | choice 14 | kChoice | answer not 0: message `0x7B`, facility byte `+5` 1; 500 or more: `0x7F`, 0; else `0x7C`, 1 |
| `0x429CB0` | `Area184_ChoiceMessage89` | `0x24` | choice 15 | kChoice | answer 0: message `0x89`; else `0x88`, facility byte `+2` 2 |
| `0x429680` | `Area175_ChoiceMessage9F` | `0x19` | choice 19 | kChoice | message `0x9F`, `0xA0` for an answer not 0 |
| `0x429B00` | `Area183_ChoiceMessage78` | `0x38` | choice 20 | kChoice | answer 0: message `0x78`, facility `+4` `0xB`; else `0x72`, `0xC`; facility `+3` 0 either way |
| `0x429CE0` | `Area184_ChoiceMessageF7` | `0x29` | choice 21 | kChoice | answer 0: `0xFFFF`; else `0xF7` and the focus trigger id cleared |
| `0x429820` | `Area178_ChoiceMessageEF` | `0x1A` | choice 22 | kChoice | message `0xEF`; facility `+2` 1, or 2 for an answer not 0 |
| `0x429990` | `Area180_ChoiceStoreAnswer` | `0x14` | choice 24 | kChoice | message `0xFFFF`; facility `+1` the answer |
| `0x429790` | `Area177_ChoiceMessageF9` | `0x15` | choice 26 | kChoice | message `0xF9`, `0xF8` for an answer not 0 (`neg; sbb; add`) |
| `0x429D70` | `Area185_ChoiceSetFacility0` | `0x41` | choice 28 | kChoice | answer 0 or 1: `0xFFFF` and facility `+0` the answer; else `0x2A1` and the focus trigger id cleared |
| `0x429810` | `Area178_PlaySound201` | `0xC` | choice 29 = handler 0 (PSX `0x801F3328`; area 175's `0x801F3FBC`) | kHandler | `Sound_PlayById(0x201)` |
| `0x429320` | `Area175_ClampXToKind2` | `0x16` | choice 30 = handler 1 (PSX `0x801F3348`; area 175's `0x801F3FDC`) | kHandler | `Field_Kind2X` at `0x2E0000` or less (signed): the running object's x is it |
| `0x4298A0` | `Area179_Init` | `0x2A` | the init (PSX `0x801F32D8`; area 175's `0x801F44D8`) | kInit | `Cond_ByteFA` above 7 (signed): the engine's field reset `0x455450` (nobody's; section 9); the step hook's arm byte `0x9046CF` 0; `Cond_ByteFD` not 0 (read after the reset): `Sound_PlayEffect(0x20C)` |
| `0x4298D0` | `Area179_CellHook` | `0x38` | `Area_CellHooks` for areas `0xAF..0xB9` | kHook | the cell's x word `0x3F`, z word `0xE` and the leader's pose 1: tail kind `0x1A` (`Cond_ByteFA` above 7) or 6, al 1; else al 0 |
| `0x429DC0` | `Area185_StepHook` | `0x60` | `Area_StepHook`'s cases for areas 175..185 | kHook | x's high word `0x41..0x44` and z's `0x14..0x17` (16-bit): if `0x9046CF` is set, `Sound_PlayEffect(0x20C)`, the byte cleared, `Field_ChangeArea` to the return point (`0x904150` word, `0x904148`, `0x90414C`) with flags 4; off the square the byte set; al 0 always |

The "facility" bytes are `0x939A3C..0x939A41`: engine state that the code at
`0x456AF0..0x460480` reads and writes (`pe_xref`, 2026-09-28; the name-entry
commit of [`save-interchange.md`](save-interchange.md) sets `+2` and `+4`).
What they mean is the engine's; not read this round.

### 1.2 Area 175's own (descriptor `0x642450`; PSX `0x801F531C`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x429340` | `Area175_SlideOrPlace` | `0x79` | handler 2 = choice 31 (PSX `0x801F400C`) | kHandler | `Field_Request` 2: the running object slid by `Area175_SlideSteps[Frame_Counter & 0xF]` (x plus, z minus, `<< 11`; object and counter read again for z), the active member's script back one op (word `+0x8A` less 2); else placed at (`0x2C0000`, `0x5C0000`), `+4` 0 |
| `0x4293C0` | `Area175_SpawnEffect1F` | `0x7F` | handler 3 = choice 32 (PSX `0x801F40AC`) | kHandler | `Effect_FindFree` to the object's `+0xB` (`Sprite_Current` read again after); none: the script back one op; else that effect record `+0` 1, kind `0x1F`, at (`0x2E0000`, `0x5C0000`), `+0x3C` `0x1A00000` |
| `0x429440` | `Area175_GlideRun` | `0x12` | handler 4 = choice 33 (PSX `0x801F4204`) | kHandler | `jmp [Sprite_Current[4] * 4 + Area175_GlideStates]` (unchecked; ours aborts past two) |
| `0x429460` | `Area175_GlideBegin20` | `0x14` | `Area175_GlideStates[0]` | kState | count `+0xA` `0x20`, state 1 |
| `0x429480` | `Area175_GlideStep` | `0x62` | `Area175_GlideStates[1]` | kState | count less 1; 0: state 0; else the slide by `Area175_GlideSteps[count & 0xF]`, the script back one op |
| `0x4294F0` | `Area175_SpawnEffect39` | `0x44` | handler 5 = choice 34 (PSX `0x801F431C`) | kHandler | `Effect_FindFree`; a slot: `+0` 1, kind `0x39`, words `+0x2E` / `+0x30` the running object's |
| `0x429540` | `Area175_OpenScriptMessage` | `0x67` | handler 6 = choice 35 (PSX `0x801F439C`) | kHandler | the byte two past the script object's position in `Area_Descriptors[Game_AreaNumber] +0x10` script `[object +3]` picks a word of `Area175_ScriptMessages`: `Msg_OpenScript`; `Field_Request` 2, the script object's `+0` bit `0x10` cleared and `0x20` set, its position on 1 (the object read again after the call) |
| `0x4295B0` | `Area175_PoseAnimA` | `0x2B` | handler 7 = choice 36 (PSX `0x801F446C`) | kHandler | the script object's `+0` bit `0x40`; the running object's `+7` bit 8 and `+0` bit `0x10`; `Sprite_SetAnimation(0xA)` |
| `0x4295E0` | `Area175_Trigger64` | `0x42` | object trigger 64 (a gap of the tool) | kCallee | the object `+1` 4, `+0x84` 2, `+0x83` `0xD`, word `+0x8A` 0; story flag `0x95`; `Party_DropIn(4)`; counter 0 cleared; al 0 |

### 1.3 Area 185's draw (a gap of the tool)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x429E20` | `Area185_DrawTileField` | `0x1DC` | `EffectKind18_States[103]` (`0x654208`) | kState | `Sprite_Current` an effect record: `MapView_ScreenXY` = (-12544, -14848); a draw mode (tpage `0xB5`, dtd 1) linked at the record (size `0xC`); 32 rows of 8 semi-transparent `TILE_1`s (256), then a draw mode (tpage `0xB5`) linked (size `0xC`) |

Each tile: the row counter r = 0, -8, .. -`0xF8` and the column u = 0,
`0x10`, .. `0x1F0` step together; the shade s = (r - (`Frame_Counter` & 7) +
`0x17F`) `>> 1` is the blue byte, s / 4 the red and green; for k = 0..7 the
angle a = k `* 0x200 + 0x100` and m = (`Frame_Counter` & `0xF`) + u, the
counter read once, after the first sine; the vertex in `Prim_VertexScratch`:
x = `((sin a * m * 5) >> 16)` (32-bit product, logical shift) plus the
screen x float through `fild` / `fadd` / `_ftol` (`0x5B9550`), y the same
with the cosine and the screen y, height `-0x340 - (sin(5m) >> 5)`;
`Gte_RotTransPers` into the tile at `+8`, `Gte_StoreDepthF` at `+0x10`, the
tile linked at the record (size `0x14`). The row counter's part in the shade
took a mismatch to see: at the shade the frame has three pushes outstanding,
so `[esp + 0x20]` is the row counter, not the column counter it is four
pushes later (section 3).

### 1.4 Area 186 (descriptor `0x645228`; PSX `0x801F3B34`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42A180` | `Area186_Init` | `0x34` | init (PSX `0x801F2F24`) | kInit | story flag `0x58`: the byte after `Field_Kind2X` (`0x905E68`) 1: `Area186_Start(0xA)`; 2: `Area186_Start(0)` |
| `0x42A1C0` | `Area186_Start` | `0x7B` | called by the init | kCallee | `Camera_Angles` (`0x100`, 0, 0), `Camera_Distance` `0x100`; `Sprite_ObjectsExtra` record 0 x `0x48000`; state 0: z `0xA0000`, `+0x83` 1, `Kind2_Place(0)`; else z `0x640000`, `+0x83` 2, `Kind2_Place(1)`; tail state the argument, timer `0x96`, kind 39 |
| `0x42A000` | `Area186_TailShift` | `0x17C` | tail kind 39 | kTail | the s8 state through a byte table (`0x42A170`, 12) and a jump table (`0x42A15C`, 5) in the body: 0 and `0xA`: with `MapView_FocusZ` `0x4400` / `0x6C00`, `Field_Kind2Z`, the extra record 0's z and `Sprite_Kind2`'s z less / plus `0x1E0000`, `MapView_Origin`'s second word less / plus `0x1E`, the focus `0x6200` / `0x4E00`, `MapView_FillCells`; then the timer down and at 0 counter 3 `0x1C`, the timer `0xF`, state 1 / `0xB`. 1 and `0xB`: the timer down and at 0 `Field_ChangeArea(0xAD, ...)` at (`0x120000`, `0x460000`) flags `0x81` / (`0x310000`, `0x600000`) flags `0x82`, kind and state 0. 2..9, negative, above `0xB`: nothing |

### 1.5 Area 187 (descriptor `0x645A70`; no PSX pairing)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42A240` | `Area187_ChoiceFocusPair` | `0x3C` | choice 0 | kChoice | the answer read signed picks a byte pair of `Area187_FocusPairs`: the focus object's dwords `+0x18` / `+0x1C` the two bytes (focus and answer read again for the second); message `0xFFFF` |
| `0x42A280` | `Area187_Trigger14` | `0xA` | object trigger 14 (a gap) | kCallee | tail kind `0x1D`; al 0 |
| `0x42A290` | `Area187_TailMessage2` | `0x84` | tail kind 29 (a gap) | kTail | s8 state 0: `ScriptFlags_Set40`, `Msg_OpenScript(2)`, `Field_Request` 2, the state (read again) on 1; 1: `Field_Request` not 2: `jmp Area131_DisarmTail`; 2: `Field_Request` not 2: `Area131_DisarmTail`, `0x904152` 0, story flag `0x77` cleared, `Field_ChangeArea` to the return point with flags 4 |

Choice 1 is **`Area130_ChoiceTailState2`** (`0x41D270`, AR3C's), read in
place: nothing of ours calls it.

## 2. Ours

`src/game/area_w4d.cpp`, calling out only through the harness (`AH_CALL`
for every named callee; `AH_AT` for the one raw address, `0x455450`). The
group's own callee (`Area186_Start`, from the init) is called the same way,
so the fuzz stands a recorder in for it. Shapes that repeat are one helper:
the eleven copies of choice 27 (`ChoiceMessageByAnswer`), the Zenny test
(`HasZenny500`), the focus object's trigger id (`DropFocusTrigger`: the focus
read before the message store, as the originals), the slide steps
(`SlideBy` / `SlideZBy`, each called with the object and index as the
original reads them), tail kind 39's two shifts and two leaves
(`Area186Shift`, `Area186Leave`), the return point (`ChangeToReturnPoint`:
area as a word, the flags 4; `Field_ChangeArea` reads the area as a word
and the flags as a byte, so the originals' `mov dx, word` with a stale upper
half is invisible). The draw's `_ftol` is `FtolLow`: the sum in double (the
x87 at 53 bits; every value it meets is small and exact), truncated toward
zero, the low word stored. Kept as the originals: every re-read after a call
(`Sprite_Current` after `Effect_FindFree`, `MoveScript_Object` after
`Msg_OpenScript`, the tail state after `Msg_OpenScript`, `Cond_ByteFD`
after the field reset, the return point after the sound and the disarm,
`Frame_Counter` after the draw's first sine and not after its cosine, the
screen floats at every `fadd`), the order of every call, the byte and word
widths of every store, the unchecked `.data` reads of section 6.

## 3. The fuzz

`BOF3X_SHADOW=area_w4d` (`src/game/area_w4d_fuzz.cpp`): thirteen `Run`s under
the one shadow name, one per area with its `Group::area` (175..187), 6,000
rounds per function, the real descriptors and tables in place,
`Area175_GlideStates` as a `DataTable` (its entries recorders while the fuzz
runs).

- **Callees the group lists:** `Effect_FindFree` (a slot of the group's
  four effect records or none), `Msg_OpenScript`, `Sprite_SetAnimation`,
  `Flags_Set` / `Clear` / `Test` (`kFlag`: the init tests `al` only),
  `Party_DropIn`, `Sound_PlayById`, `Sound_PlayEffect`, `Field_ChangeArea`
  (area masked to its word, flags to its byte), `MapView_FillCells`,
  `Kind2_Place`, `ScriptFlags_Set40`, `Area131_DisarmTail`, the raw
  `0x455450`, `Area186_Start` (its byte); the draw's `Gpu_SetDrawMode`,
  `MapView_LinkPrimAt`, `Gpu_SetTile1`, `Gpu_SetSemiTrans`, `Math_Sin`,
  `Math_Cos`, `Gte_RotTransPers` (the vertex logged by its six bytes, the
  tile by its address, the depth local not at all), `Gte_StoreDepthF`, and
  `_ftol` `0x5B9550` as `kThrough` (both copies keep Capcom's).
- **Louder stand-ins** (part of the time, from `Noise`): `Effect_FindFree`
  moves `Sprite_Current` and the active member; `Msg_OpenScript` the script
  object, the tail state and `Field_Request`; `ScriptFlags_Set40` the tail
  state; `Sound_PlayEffect`, `Flags_Clear` and `Area131_DisarmTail` a word of
  the return point; `0x455450` `Cond_ByteFD`; `MapView_FillCells` the tail
  timer; `Math_Sin` the frame counter and `Sprite_Current`; `Math_Cos` the
  frame counter and a screen float (a whole or half number); the link moves
  the packet pointer on by the size four calls in five (logging the
  primitive first) and now and then `Sprite_Current`; the setters and the
  GTE stand-ins write the primitive's bytes.
- **Regions beyond the field frame:** the twenty effect records, the active
  member, script object and focus pointers, the facility's six bytes, the
  arm byte, effect "record" `0xFF` (where a spawn that took the miss
  for a slot would write), the vertex scratch, the screen floats, `Gfx_PacketNext` and the
  fuzz's 4 KiB packet buffer, `MapView_FocusZ`, `MapView_Origin`,
  `Sprite_Kind2`'s z, the camera angles (35 regions, 23,047 bytes).
- **Every round:** the active member at a field object, a party record, one
  of the four party objects or the running object; the script object at a
  field object or a party record; the focus at a field object or a party
  object; the packet pointer into the buffer.
- **Seeds:** the choice answer at 0..5, the sign edge and above (`0xFF`,
  `0x80`, `0x81`, `0x7F`); `Party_Zenny` at 500, one either side, 0, all
  ones and with a high bit or byte; `Field_Kind2X` at `0x2E0000` and beside,
  negative, `0x80000000`; `Field_Request` 2 and beside; the glide state 0 or
  1 (2 or more aborts ours and would jump into data: never seeded) and its
  count at 0, 1, 2, the nibble edges; the script message's script index one
  of the fourteen (kept so after every move of the pointer or the byte, by
  `Settle`) and its position inside the script half the time, any word one
  time in sixteen; `Cond_ByteFA` at 7, 8, 6, 9, `0x7F`, `0x80`, `0xFF`;
  `Cond_ByteFD` 0 or not; the cell hook's words at `0x3F` / `0xE`, one off,
  with a high byte or half; the leader's pose 1 and beside; the step hook's
  high words on and beside `0x41..0x44` / `0x14..0x17` with a high byte
  (`Around4`); the arm byte 0, 1, 2, `0x80`; tail kind 39's state at each
  value its table reaches, the empty ones, above `0xB` and negative, the
  focus at `0x4400` / `0x6C00` and one off, the timer at 1, 0, 2, `0x101`,
  `0xFFFF`; the init's mode byte 1, 2, 0, 3, `0x81`, `0x82`; the start's
  argument 0, `0xA`, 1, `0xFF`, `0x100`, `0xA0A`, `0x80`; tail kind 29's
  state 0..3, `0xFF`, `0x80` with `Field_Request` 2 or not; the draw's
  `Sprite_Current` at an effect record.
- **The group's disturbance** (from the hash it is given): the tail state
  and timer, counter 0, the three object pointers, a facility byte, the arm
  byte, the map focus, `Field_Request`, the choice answer.

**Result (in this worktree):** 294,000 rounds over the 49 functions (6,000
each), 12,385,898 calls to the stand-ins, 0 mismatches, 23,047 bytes of
state (35 regions) and the log compared. Coverage: every callee each
function can reach was called and both glide states (2,945 / 3,055), e.g.
area 175's `Effect_FindFree` 12,000, `Msg_OpenScript` 6,000; area 179's
`Sound_PlayEffect` 3,845, the field reset 2,506; area 185's
`Field_ChangeArea` 543 (the step hook fired), the draw's links 1,548,000
and sines 3,072,000; area 186's `MapView_FillCells` 278, `Field_ChangeArea`
373, `Area186_Start` 1,346; area 187's `Msg_OpenScript` 929,
`Area131_DisarmTail` 1,388, `Field_ChangeArea` 718.

The first run had 6,000 mismatches, all in the draw: the shade was built
from the column counter (section 1.3); fixed in ours, 0 since.

`BOF3X_SHADOW='*'`: exit 0 on the first run, `inject: 5407 ours`, 499 self-test lines, no mismatch (in this worktree).

## 4. Controls

Planted one at a time in `area_w4d.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w4d.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w4d`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all thirteen runs). **286 planted, 279 refused by a count (exit 3), 6 equivalent** (each with a near variant planted and refused), 1 planted as a no-op by mistake (H28, replaced by H28b); no hang, no fault. Every one of the 49 functions has at least one control of its own (each of choice 27's eleven copies its own, C175..C185); a control in a shared helper lists the function it was refused in (a Fatal ends the self-test after the first area that differs, so a helper shared across areas lists the first area's).

A first pass (282 planted) left eight standing. One was the fuzz's fault: with `Effect_FindFree` answering none, A37's mutant writes effect "record" `0xFF`, outside every region - refused (1,184 rounds) once the fuzz compared that record (the region AR2A added for the same reason). The first pass's counts were measured before that region was added; the four planted after it (A37, H28b, A13b, A16b) after. H28 was not a mutant (the plant added 0). The other six are equivalent (below).

The thinnest: B22 39 (Area179_CellHook), G22 48 (Area187_TailMessage2), B19 64 (Area179_CellHook), H19 77 (Area186_TailShift), B18 78 (Area179_CellHook), E11 86 (Area185_StepHook).

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| H1 | `ChoiceMessageByAnswer (x11)` | answers above 3 leave the word | Area175_ChoiceMessageByAnswer 329 |
| H2 | `ChoiceMessageByAnswer (x11)` | answer 3's message 0xFE | Area175_ChoiceMessageByAnswer 366 |
| H3 | `ChoiceMessageByAnswer (x11)` | the bound compared signed | Area175_ChoiceMessageByAnswer 310 |
| H4 | `ChoiceMessageByAnswer (x11)` | the answer zero-extended | equivalent: every byte of `0x80` or more is above 4 whether read signed or not; variant H3 refused |
| C175 | `Area175_ChoiceMessageByAnswer` | answer 3 leaves the word | Area175_ChoiceMessageByAnswer 366 |
| C176 | `Area176_ChoiceMessageByAnswer` | answer 3 leaves the word | Area176_ChoiceMessageByAnswer 376 |
| C177 | `Area177_ChoiceMessageByAnswer` | answer 3 leaves the word | Area177_ChoiceMessageByAnswer 316 |
| C178 | `Area178_ChoiceMessageByAnswer` | answer 3 leaves the word | Area178_ChoiceMessageByAnswer 323 |
| C179 | `Area179_ChoiceMessageByAnswer` | answer 3 leaves the word | Area179_ChoiceMessageByAnswer 357 |
| C180 | `Area180_ChoiceMessageByAnswer` | answer 3 leaves the word | Area180_ChoiceMessageByAnswer 348 |
| C181 | `Area181_ChoiceMessageByAnswer` | answer 3 leaves the word | Area181_ChoiceMessageByAnswer 349 |
| C182 | `Area182_ChoiceMessageByAnswer` | answer 3 leaves the word | Area182_ChoiceMessageByAnswer 348 |
| C183 | `Area183_ChoiceMessageByAnswer` | answer 3 leaves the word | Area183_ChoiceMessageByAnswer 348 |
| C184 | `Area184_ChoiceMessageByAnswer` | answer 3 leaves the word | Area184_ChoiceMessageByAnswer 353 |
| C185 | `Area185_ChoiceMessageByAnswer` | answer 3 leaves the word | Area185_ChoiceMessageByAnswer 382 |
| H5 | `DropFocusTrigger (x6)` | trigger id 0xFE | Area175_ChoiceAskZenny6F 1039 |
| H6 | `DropFocusTrigger (x6)` | +0x87 cleared | Area175_ChoiceAskZenny6F 1039 |
| H7 | `MemberStep (x3)` | back 3 | Area175_SlideOrPlace 1380, Area175_SpawnEffect1F 1206, Area175_GlideStep 5134 |
| H8 | `FtolLow (draw)` | rounded, not truncated | Area185_DrawTileField 6000 |
| H9 | `FtolLow (draw)` | the float's fraction dropped | Area185_DrawTileField 6000 |
| H10 | `FtolLow (draw)` | the whole read signed | Area185_DrawTileField 6000 |
| H11 | `HasZenny500 (x2)` | above 500 | Area175_ChoiceAskZenny6F 500 |
| H12 | `HasZenny500 (x2)` | signed compare | Area175_ChoiceAskZenny6F 1909 |
| H13 | `HasZenny500 (x2)` | the word only | Area175_ChoiceAskZenny6F 1015 |
| H14 | `SlideBy (x2)` | index & 7 | equivalent: both step tables repeat every 4 bytes, so `& 7` reads what `& 0xF` reads; variant H14b refused |
| H14b | `SlideBy (x2)` | index & 0xE | Area175_SlideOrPlace 697, Area175_GlideStep 2778 |
| H15 | `SlideBy (x2)` | step << 12 | Area175_SlideOrPlace 683, Area175_GlideStep 2356 |
| H16 | `SlideZBy (x2)` | z not negated | Area175_SlideOrPlace 683, Area175_GlideStep 2356 |
| H17 | `SlideZBy (x2)` | step zero-extended | Area175_SlideOrPlace 338, Area175_GlideStep 1388 |
| H18 | `SlideBy (x2)` | step zero-extended | Area175_SlideOrPlace 338, Area175_GlideStep 1388 |
| H19 | `Area186Shift (tail 39)` | focus one above matches too | Area186_TailShift 77 |
| H20 | `Area186Shift (tail 39)` | origin by twice | Area186_TailShift 278 |
| H21 | `Area186Shift (tail 39)` | Field_Kind2Z not moved | Area186_TailShift 278 |
| H22 | `Area186Shift (tail 39)` | the extra object's x moved | Area186_TailShift 278 |
| H23 | `Area186Shift (tail 39)` | Sprite_Kind2 z by dz + 1 | Area186_TailShift 278 |
| H24 | `Area186Shift (tail 39)` | focus to + 1 | Area186_TailShift 275 |
| H25 | `Area186Shift (tail 39)` | no MapView_FillCells | Area186_TailShift 278 |
| H26 | `Area186Shift (tail 39)` | counter 3 0x1D | Area186_TailShift 395 |
| H27 | `Area186Shift (tail 39)` | timer reset 0x10 | Area186_TailShift 395 |
| H28 | `Area186Shift (tail 39)` | timer down before the shift | not a mutant: planted as a no-op by mistake (the timer plus 0); replaced by H28b, refused |
| H29 | `Area186Shift (tail 39)` | timer read before FillCells | Area186_TailShift 121 |
| H30 | `Area186Leave (tail 39)` | area 0xAC | Area186_TailShift 373 |
| H31 | `Area186Leave (tail 39)` | x and z swapped | Area186_TailShift 373 |
| H32 | `Area186Leave (tail 39)` | kind left 1 | Area186_TailShift 373 |
| H33 | `Area186Leave (tail 39)` | timer down by 2 | Area186_TailShift 1568 |
| H34 | `Area186Leave (tail 39)` | state left | Area186_TailShift 373 |
| H35 | `ChangeToReturnPoint (x2)` | flags 5 | Area185_StepHook 544 |
| H36 | `ChangeToReturnPoint (x2)` | x and z swapped | Area185_StepHook 544 |
| H37 | `ChangeToReturnPoint (x2)` | area the next word | Area185_StepHook 544 |
| A1 | `Area175_ClampXToKind2` | at or above | Area175_ClampXToKind2 568 |
| A2 | `Area175_ClampXToKind2` | unsigned compare | Area175_ClampXToKind2 2163 |
| A3 | `Area175_ClampXToKind2` | z set | Area175_ClampXToKind2 3861 |
| A4 | `Area175_ChoiceAskZenny6F` | message 0x70 | Area175_ChoiceAskZenny6F 685 |
| A5 | `Area175_ChoiceAskZenny6F` | sub-kind 2 at answer 0 | Area175_ChoiceAskZenny6F 685 |
| A6 | `Area175_ChoiceAskZenny6F` | message 0x7E | Area175_ChoiceAskZenny6F 4276 |
| A7 | `Area175_ChoiceAskZenny6F` | sub-kind 2 with the money | Area175_ChoiceAskZenny6F 4276 |
| A8 | `Area175_ChoiceAskZenny6F` | message 0x7D short | Area175_ChoiceAskZenny6F 1039 |
| A9 | `Area175_ChoiceAskZenny6F` | answer 1 as 0 | Area175_ChoiceAskZenny6F 651 |
| A10 | `Area175_ChoiceMessage9F` | answer above 1 | Area175_ChoiceMessage9F 638 |
| A11 | `Area175_ChoiceMessage9F` | base 0xA0 | Area175_ChoiceMessage9F 6000 |
| A12 | `Area175_SlideOrPlace` | request 2 or more | Area175_SlideOrPlace 3288 |
| A13 | `Area175_SlideOrPlace` | the object not read again for z | equivalent: no call lies between the x and the z step, so nothing can move `Sprite_Current` between the reads; variant A13b refused |
| A14 | `Area175_SlideOrPlace` | counter + 1 for x | Area175_SlideOrPlace 1380 |
| A15 | `Area175_SlideOrPlace` | x 0x2D0000 | Area175_SlideOrPlace 4620 |
| A16 | `Area175_SlideOrPlace` | z through the first read | equivalent: no call lies between the two stores (the same reason); variant A16b refused |
| A17 | `Area175_SlideOrPlace` | state 1 | Area175_SlideOrPlace 4620 |
| A18 | `Area175_SlideOrPlace` | no step back | Area175_SlideOrPlace 1380 |
| A19 | `Area175_SlideOrPlace` | the glide's steps | equivalent: `Area175_SlideSteps` and `Area175_GlideSteps` hold the same 16 bytes; variant A19b refused |
| A19b | `Area175_SlideOrPlace` | the steps + 1 | Area175_SlideOrPlace 1380 |
| A20 | `Area175_SpawnEffect1F` | the object read before the call | Area175_SpawnEffect1F 2762 |
| A21 | `Area175_SpawnEffect1F` | kind 0x20 | Area175_SpawnEffect1F 4794 |
| A22 | `Area175_SpawnEffect1F` | x 0x2F0000 | Area175_SpawnEffect1F 4794 |
| A23 | `Area175_SpawnEffect1F` | z 0x5D0000 | Area175_SpawnEffect1F 4794 |
| A24 | `Area175_SpawnEffect1F` | height 0x1A10000 | Area175_SpawnEffect1F 4794 |
| A25 | `Area175_SpawnEffect1F` | +0 2 | Area175_SpawnEffect1F 4794 |
| A26 | `Area175_SpawnEffect1F` | none 0xFE | Area175_SpawnEffect1F 1206 |
| A27 | `Area175_SpawnEffect1F` | slot not stored | Area175_SpawnEffect1F 4771 |
| A28 | `Area175_GlideRun` | the other state | Area175_GlideRun 6000 |
| A29 | `Area175_GlideBegin20` | count 0x21 | Area175_GlideBegin20 6000 |
| A30 | `Area175_GlideBegin20` | state 0 | Area175_GlideBegin20 6000 |
| A31 | `Area175_GlideStep` | stops at 1 | Area175_GlideStep 473 |
| A32 | `Area175_GlideStep` | state 1 at the end | Area175_GlideStep 866 |
| A33 | `Area175_GlideStep` | x by count + 1 | Area175_GlideStep 5134 |
| A34 | `Area175_GlideStep` | count less 2 | Area175_GlideStep 6000 |
| A35 | `Area175_GlideStep` | no step back | Area175_GlideStep 5134 |
| A36 | `Area175_GlideStep` | the slide's steps | Area175_GlideStep 5134 |
| A37 | `Area175_SpawnEffect39` | none 0xFE | Area175_SpawnEffect39 1184 |
| A38 | `Area175_SpawnEffect39` | kind 0x3A | Area175_SpawnEffect39 4759 |
| A39 | `Area175_SpawnEffect39` | +0 2 | Area175_SpawnEffect39 4759 |
| A40 | `Area175_SpawnEffect39` | +0x2E from +0x30 | Area175_SpawnEffect39 4759 |
| A41 | `Area175_SpawnEffect39` | +0x30 a byte | Area175_SpawnEffect39 4742 |
| A42 | `Area175_SpawnEffect39` | the object read before the call | Area175_SpawnEffect39 2104 |
| A43 | `Area175_OpenScriptMessage` | operand one byte early | Area175_OpenScriptMessage 5405 |
| A44 | `Area175_OpenScriptMessage` | the next message word | Area175_OpenScriptMessage 5675 |
| A45 | `Area175_OpenScriptMessage` | the neighbouring script | Area175_OpenScriptMessage 5392 |
| A46 | `Area175_OpenScriptMessage` | request 3 | Area175_OpenScriptMessage 6000 |
| A47 | `Area175_OpenScriptMessage` | bit 0x20 cleared | Area175_OpenScriptMessage 2972 |
| A48 | `Area175_OpenScriptMessage` | bit 0x40 set | Area175_OpenScriptMessage 4547 |
| A49 | `Area175_OpenScriptMessage` | on 2 | Area175_OpenScriptMessage 6000 |
| A50 | `Area175_OpenScriptMessage` | the object not read again | Area175_OpenScriptMessage 1975 |
| A51 | `Area175_OpenScriptMessage` | position a byte | Area175_OpenScriptMessage 338 |
| A52 | `Area175_PoseAnimA` | script bit 0x60 | Area175_PoseAnimA 2999 |
| A53 | `Area175_PoseAnimA` | +7 bit 0x18 | Area175_PoseAnimA 3021 |
| A54 | `Area175_PoseAnimA` | +0 bit 0x30 | Area175_PoseAnimA 3079 |
| A55 | `Area175_PoseAnimA` | animation 0xB | Area175_PoseAnimA 6000 |
| A56 | `Area175_Trigger64` | +1 5 | Area175_Trigger64 5996 |
| A57 | `Area175_Trigger64` | +0x84 3 | Area175_Trigger64 6000 |
| A58 | `Area175_Trigger64` | +0x83 0xC | Area175_Trigger64 6000 |
| A59 | `Area175_Trigger64` | +0x8A 1 | Area175_Trigger64 6000 |
| A60 | `Area175_Trigger64` | flag 0x96 | Area175_Trigger64 6000 |
| A61 | `Area175_Trigger64` | drop-in 5 | Area175_Trigger64 6000 |
| A62 | `Area175_Trigger64` | counter 0 1 | Area175_Trigger64 6000 |
| A63 | `Area175_Trigger64` | answers 1 | Area175_Trigger64 6000 |
| B1 | `Area176_ChoiceCount3E` | byte +3 at answer 0 | Area176_ChoiceCount3E 721 |
| B2 | `Area176_ChoiceCount3E` | up two | Area176_ChoiceCount3E 5279 |
| B3 | `Area176_ChoiceCount3E` | message 0x60 | Area176_ChoiceCount3E 5279 |
| B4 | `Area176_ChoiceCount3E` | answer 1 as 0 | Area176_ChoiceCount3E 699 |
| B5 | `Area177_ChoiceMessageF9` | answer 1 only | Area177_ChoiceMessageF9 4706 |
| B6 | `Area177_ChoiceMessageF9` | 0xF7 for not 0 | Area177_ChoiceMessageF9 5346 |
| B7 | `Area178_PlaySound201` | sound 0x202 | Area178_PlaySound201 6000 |
| B8 | `Area178_ChoiceMessageEF` | message 0xEE | Area178_ChoiceMessageEF 6000 |
| B9 | `Area178_ChoiceMessageEF` | 2 or 3 | Area178_ChoiceMessageEF 6000 |
| B10 | `Area178_ChoiceMessageEF` | answer above 1 | Area178_ChoiceMessageEF 701 |
| B11 | `Area179_Init` | at 7 or more | Area179_Init 861 |
| B12 | `Area179_Init` | unsigned chapter | Area179_Init 1825 |
| B13 | `Area179_Init` | arm byte 1 | Area179_Init 5983 |
| B14 | `Area179_Init` | sound 0x20D | Area179_Init 3845 |
| B15 | `Area179_Init` | Cond_ByteFD read before the reset | Area179_Init 597 |
| B16 | `Area179_Init` | sound on fd 0 | Area179_Init 6000 |
| B17 | `Area179_CellHook` | x 0x3E | Area179_CellHook 381 |
| B18 | `Area179_CellHook` | z a byte | Area179_CellHook 78 |
| B19 | `Area179_CellHook` | x a byte | Area179_CellHook 64 |
| B20 | `Area179_CellHook` | pose not tested | Area179_CellHook 1222 |
| B21 | `Area179_CellHook` | pose 1 or 2 | Area179_CellHook 179 |
| B22 | `Area179_CellHook` | at 7 or more | Area179_CellHook 39 |
| B23 | `Area179_CellHook` | kind 7 | Area179_CellHook 193 |
| B24 | `Area179_CellHook` | answers 2 | Area179_CellHook 321 |
| B25 | `Area179_CellHook` | z 0xF | Area179_CellHook 396 |
| B26 | `Area179_ChoiceVar8` | 3 or 4 | Area179_ChoiceVar8 6000 |
| B27 | `Area179_ChoiceVar8` | message 0xFFFE | Area179_ChoiceVar8 6000 |
| B28 | `Area179_ChoiceVar8` | answer 1 as 0 | Area179_ChoiceVar8 649 |
| B29 | `Area180_ChoiceStoreAnswer` | answer + 1 | Area180_ChoiceStoreAnswer 6000 |
| B30 | `Area180_ChoiceStoreAnswer` | byte +2 | Area180_ChoiceStoreAnswer 6000 |
| B31 | `Area180_ChoiceStoreAnswer` | message 0xFFFE | Area180_ChoiceStoreAnswer 6000 |
| B32 | `Area181_ChoiceCounter0` | counter 0x29 | Area181_ChoiceCounter0 664 |
| B33 | `Area181_ChoiceCounter0` | byte 0x16 | Area181_ChoiceCounter0 664 |
| B34 | `Area181_ChoiceCounter0` | counter 0x31 | Area181_ChoiceCounter0 5336 |
| B35 | `Area181_ChoiceCounter0` | byte 0x18 | Area181_ChoiceCounter0 5336 |
| B36 | `Area181_ChoiceCounter0` | answer 1 as 0 | Area181_ChoiceCounter0 647 |
| B37 | `Area181_ChoiceCounter0` | message 0xFFFE | Area181_ChoiceCounter0 6000 |
| B38 | `Area183_ChoiceMessage78` | message 0x79 | Area183_ChoiceMessage78 683 |
| B39 | `Area183_ChoiceMessage78` | +4 0xD at 0 | Area183_ChoiceMessage78 683 |
| B40 | `Area183_ChoiceMessage78` | +4 0xD | Area183_ChoiceMessage78 5317 |
| B41 | `Area183_ChoiceMessage78` | +3 1 | Area183_ChoiceMessage78 6000 |
| B42 | `Area183_ChoiceMessage78` | message 0x73 | Area183_ChoiceMessage78 5317 |
| B43 | `Area184_ChoiceMessage5A` | answer 2 | Area184_ChoiceMessage5A 979 |
| B44 | `Area184_ChoiceMessage5A` | message 0x5B | Area184_ChoiceMessage5A 655 |
| B45 | `Area184_ChoiceMessage5A` | 0xFFFE else | Area184_ChoiceMessage5A 5345 |
| B46 | `Area184_ChoiceTailState1` | state 2 | Area184_ChoiceTailState1 689 |
| B47 | `Area184_ChoiceTailState1` | any answer not 0 | Area184_ChoiceTailState1 4639 |
| B48 | `Area184_ChoiceTailState1` | message 0x60 | Area184_ChoiceTailState1 689 |
| B49 | `Area184_ChoiceMessage7A` | message 0x7B | Area184_ChoiceMessage7A 678 |
| B50 | `Area184_ChoiceMessage7A` | message 0x6F | Area184_ChoiceMessage7A 5322 |
| B51 | `Area184_ChoiceMessage7A` | answer 1 as 0 | Area184_ChoiceMessage7A 657 |
| B52 | `Area184_ChoiceMessage77` | message 0x76 | Area184_ChoiceMessage77 5343 |
| B53 | `Area184_ChoiceMessage77` | +4 0xB | Area184_ChoiceMessage77 5343 |
| B54 | `Area184_ChoiceMessage77` | answer 1 as 0 | Area184_ChoiceMessage77 729 |
| B55 | `Area184_ChoiceAskZenny7F` | message 0x7C for an answer | Area184_ChoiceAskZenny7F 5350 |
| B56 | `Area184_ChoiceAskZenny7F` | +5 0 for an answer | Area184_ChoiceAskZenny7F 5350 |
| B57 | `Area184_ChoiceAskZenny7F` | message 0x80 | Area184_ChoiceAskZenny7F 528 |
| B58 | `Area184_ChoiceAskZenny7F` | +5 2 with the money | Area184_ChoiceAskZenny7F 528 |
| B59 | `Area184_ChoiceAskZenny7F` | message 0x7D short | Area184_ChoiceAskZenny7F 122 |
| B60 | `Area184_ChoiceAskZenny7F` | +5 3 short | Area184_ChoiceAskZenny7F 122 |
| B61 | `Area184_ChoiceMessage89` | message 0x8A | Area184_ChoiceMessage89 684 |
| B62 | `Area184_ChoiceMessage89` | +2 3 | Area184_ChoiceMessage89 5316 |
| B63 | `Area184_ChoiceMessage89` | message 0x87 | Area184_ChoiceMessage89 5316 |
| B64 | `Area184_ChoiceMessageF7` | message 0xF6 | Area184_ChoiceMessageF7 5325 |
| B65 | `Area184_ChoiceMessageF7` | answer 1 as 0 | Area184_ChoiceMessageF7 684 |
| E1 | `Area185_ChoiceSetFacility0` | answer 2 stored | Area185_ChoiceSetFacility0 341 |
| E2 | `Area185_ChoiceSetFacility0` | the answer flipped | Area185_ChoiceSetFacility0 1321 |
| E3 | `Area185_ChoiceSetFacility0` | message 0x2A0 | Area185_ChoiceSetFacility0 4679 |
| E4 | `Area185_ChoiceSetFacility0` | message 0xFFFE | Area185_ChoiceSetFacility0 1321 |
| E5 | `Area185_StepHook` | x to 0x45 | Area185_StepHook 153 |
| E6 | `Area185_StepHook` | z to 0x16 | Area185_StepHook 157 |
| E7 | `Area185_StepHook` | x from 0x42 | Area185_StepHook 148 |
| E8 | `Area185_StepHook` | z's low word | Area185_StepHook 631 |
| E9 | `Area185_StepHook` | x a byte | Area185_StepHook 324 |
| E10 | `Area185_StepHook` | arm 2 | Area185_StepHook 5370 |
| E11 | `Area185_StepHook` | fires unarmed | Area185_StepHook 86 |
| E12 | `Area185_StepHook` | left armed | Area185_StepHook 537 |
| E13 | `Area185_StepHook` | sound 0x20D | Area185_StepHook 544 |
| E14 | `Area185_StepHook` | answers 1 on the square | Area185_StepHook 544 |
| E15 | `Area185_StepHook` | the return point read before the sound | Area185_StepHook 87 |
| E16 | `Area185_StepHook` | armed not tested whole | Area185_StepHook 251 |
| D1 | `Area185_DrawTileField` | screen x -12672 | Area185_DrawTileField 6000 |
| D2 | `Area185_DrawTileField` | screen y -14912 | Area185_DrawTileField 5388 |
| D3 | `Area185_DrawTileField` | first draw mode dtd 0 | Area185_DrawTileField 6000 |
| D4 | `Area185_DrawTileField` | first link size 0xD | Area185_DrawTileField 6000 |
| D5 | `Area185_DrawTileField` | seven tiles a row | Area185_DrawTileField 6000 |
| D6 | `Area185_DrawTileField` | shade by & 3 | Area185_DrawTileField 6000 |
| D7 | `Area185_DrawTileField` | shade + 0x180 | Area185_DrawTileField 6000 |
| D8 | `Area185_DrawTileField` | blue + 1 | Area185_DrawTileField 6000 |
| D9 | `Area185_DrawTileField` | a third | Area185_DrawTileField 6000 |
| D10 | `Area185_DrawTileField` | green the shade | Area185_DrawTileField 6000 |
| D11 | `Area185_DrawTileField` | angle k / 4 | Area185_DrawTileField 6000 |
| D12 | `Area185_DrawTileField` | angle + 0x80 | Area185_DrawTileField 6000 |
| D13 | `Area185_DrawTileField` | the counter read before the sine | Area185_DrawTileField 6000 |
| D14 | `Area185_DrawTileField` | m by & 7 | Area185_DrawTileField 6000 |
| D15 | `Area185_DrawTileField` | x by 4m | Area185_DrawTileField 6000 |
| D16 | `Area185_DrawTileField` | y >> 15 | Area185_DrawTileField 6000 |
| D17 | `Area185_DrawTileField` | y by the x float | Area185_DrawTileField 6000 |
| D18 | `Area185_DrawTileField` | the counter read again for y | Area185_DrawTileField 6000 |
| D19 | `Area185_DrawTileField` | lift by 3m | Area185_DrawTileField 6000 |
| D20 | `Area185_DrawTileField` | base -0x341 | Area185_DrawTileField 6000 |
| D21 | `Area185_DrawTileField` | lift >> 4 | Area185_DrawTileField 6000 |
| D22 | `Area185_DrawTileField` | lift shifted unsigned | equivalent: an arithmetic and a logical shift by 5 differ only in the top five bits, and only the low word is stored; variant D21 refused |
| D23 | `Area185_DrawTileField` | the point into +0xC | Area185_DrawTileField 6000 |
| D24 | `Area185_DrawTileField` | the depth at +0xC | Area185_DrawTileField 6000 |
| D25 | `Area185_DrawTileField` | tile link size 0x10 | Area185_DrawTileField 6000 |
| D26 | `Area185_DrawTileField` | tile link dy 2 | Area185_DrawTileField 6000 |
| D27 | `Area185_DrawTileField` | tile linked at (z, x) | Area185_DrawTileField 6000 |
| D28 | `Area185_DrawTileField` | Sprite_Current not read again for the links | Area185_DrawTileField 6000 |
| D29 | `Area185_DrawTileField` | 31 rows | Area185_DrawTileField 6000 |
| D30 | `Area185_DrawTileField` | columns by 8 | Area185_DrawTileField 6000 |
| D31 | `Area185_DrawTileField` | last draw mode dtd 1 | Area185_DrawTileField 6000 |
| D32 | `Area185_DrawTileField` | last link size 0x14 | Area185_DrawTileField 6000 |
| D33 | `Area185_DrawTileField` | opaque tiles | Area185_DrawTileField 6000 |
| D34 | `Area185_DrawTileField` | the tile pointer read once | Area185_DrawTileField 6000 |
| D35 | `Area185_DrawTileField` | first draw mode tpage 0xB4 | Area185_DrawTileField 6000 |
| D36 | `Area185_DrawTileField` | the row in the shade unsigned-halved differently | Area185_DrawTileField 6000 |
| F1 | `Area186_TailShift` | state 9 shifts up | Area186_TailShift 1198 |
| F2 | `Area186_TailShift` | leave flags 0x80 | Area186_TailShift 186 |
| F3 | `Area186_TailShift` | leave z 0x610000 | Area186_TailShift 187 |
| F4 | `Area186_TailShift` | focus to 0x6201 | Area186_TailShift 137 |
| F5 | `Area186_TailShift` | next state 0xC | Area186_TailShift 201 |
| F6 | `Area186_TailShift` | state 2 leaves | Area186_TailShift 1172 |
| F7 | `Area186_TailShift` | down by 0x1F0000 | Area186_TailShift 137 |
| F8 | `Area186_TailShift` | up focus 0x6C01 | Area186_TailShift 141 |
| F9 | `Area186_TailShift` | next state 2 | Area186_TailShift 194 |
| F10 | `Area186_TailShift` | leave x 0x130000 | Area186_TailShift 186 |
| F11 | `Area186_Init` | flag 0x59 | Area186_Init 6000 |
| F12 | `Area186_Init` | start 0xB | Area186_Init 699 |
| F13 | `Area186_Init` | start 1 | Area186_Init 647 |
| F14 | `Area186_Init` | mode 2 or more | Area186_Init 2258 |
| F15 | `Area186_Init` | flag tested whole | Area186_Init 340 |
| F16 | `Area186_Init` | mode 1 only | Area186_Init 647 |
| F17 | `Area186_Start` | angle 0x101 | Area186_Start 6000 |
| F18 | `Area186_Start` | third angle 1 | Area186_Start 6000 |
| F19 | `Area186_Start` | second angle 1 | Area186_Start 6000 |
| F20 | `Area186_Start` | distance 0x200 | Area186_Start 5996 |
| F21 | `Area186_Start` | x 0x49000 | Area186_Start 6000 |
| F22 | `Area186_Start` | state 1 as 0 | Area186_Start 434 |
| F23 | `Area186_Start` | z 0xA1000 | Area186_Start 1335 |
| F24 | `Area186_Start` | +0x83 3 at 0 | Area186_Start 1335 |
| F25 | `Area186_Start` | Kind2_Place(2) at 0 | Area186_Start 1335 |
| F26 | `Area186_Start` | z 0x650000 | Area186_Start 4665 |
| F27 | `Area186_Start` | +0x83 3 | Area186_Start 4665 |
| F28 | `Area186_Start` | Kind2_Place(2) | Area186_Start 4665 |
| F29 | `Area186_Start` | state + 1 | Area186_Start 6000 |
| F30 | `Area186_Start` | timer 0x97 | Area186_Start 6000 |
| F31 | `Area186_Start` | kind 0x28 | Area186_Start 6000 |
| G1 | `Area187_ChoiceFocusPair` | x the pair's second byte | Area187_ChoiceFocusPair 5620 |
| G2 | `Area187_ChoiceFocusPair` | y the next pair's first | Area187_ChoiceFocusPair 5338 |
| G3 | `Area187_ChoiceFocusPair` | x to +0x14 | Area187_ChoiceFocusPair 6000 |
| G4 | `Area187_ChoiceFocusPair` | the answer unsigned | Area187_ChoiceFocusPair 1928 |
| G5 | `Area187_ChoiceFocusPair` | message 0xFFFE | Area187_ChoiceFocusPair 6000 |
| G6 | `Area187_ChoiceFocusPair` | y to +0x20 | Area187_ChoiceFocusPair 6000 |
| G7 | `Area187_ChoiceFocusPair` | x a signed byte | Area187_ChoiceFocusPair 393 |
| G8 | `Area187_Trigger14` | kind 0x1E | Area187_Trigger14 6000 |
| G9 | `Area187_Trigger14` | answers 1 | Area187_Trigger14 6000 |
| G10 | `Area187_TailMessage2` | message 3 | Area187_TailMessage2 929 |
| G11 | `Area187_TailMessage2` | the state read before the calls | Area187_TailMessage2 606 |
| G12 | `Area187_TailMessage2` | request 1 | Area187_TailMessage2 929 |
| G13 | `Area187_TailMessage2` | state + 2 | Area187_TailMessage2 929 |
| G14 | `Area187_TailMessage2` | state 1 waits on 3 | Area187_TailMessage2 308 |
| G15 | `Area187_TailMessage2` | state 2 waits on 3 | Area187_TailMessage2 283 |
| G16 | `Area187_TailMessage2` | the byte 1 | Area187_TailMessage2 670 |
| G17 | `Area187_TailMessage2` | flag 0x78 | Area187_TailMessage2 718 |
| G18 | `Area187_TailMessage2` | no disarm in state 2 | Area187_TailMessage2 718 |
| G19 | `Area187_TailMessage2` | no ScriptFlags_Set40 | Area187_TailMessage2 929 |
| G20 | `Area187_TailMessage2` | state 3 as 2 | Area187_TailMessage2 336 |
| G21 | `Area187_TailMessage2` | state read unsigned (0x80 as 2) | Area187_TailMessage2 465 |
| G22 | `Area187_TailMessage2` | the byte cleared after the flag | Area187_TailMessage2 48 |
| H28b | `Area186Shift (tail 39)` | timer counted down before the shift and MapView_FillCells | Area186_TailShift 128 |
| A13b | `Area175_SlideOrPlace` | z through the first read, by the counter + 1 | Area175_SlideOrPlace 1314 |
| A16b | `Area175_SlideOrPlace` | z through the first read, 0x5D0000 | Area175_SlideOrPlace 4686 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (5): `Area175_SlideSteps`
`0x642494` (16 signed bytes), `Area175_GlideStates` `0x6424A4` (2),
`Area175_GlideSteps` `0x6424AC` (16 signed bytes), `Area175_ScriptMessages`
`0x6424BC` (4 words), `Area187_FocusPairs` `0x645A5C` (4 byte pairs). The
two step tables hold the same period-4 pattern (so `& 7` reads what `& 0xF`
does, and either table reads what the other does: section 4). Tail kind
39's two tables are in `.text`, inside its extent; each copy of choice 27
has its jump table inside its own extent.

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area175_GlideRun` jumps through a
  two-entry table by `Sprite_Current[4]`; an index of 2 or more reads
  `Area175_GlideSteps` as a pointer (not code). Ours aborts with a message
  there (the owner's rule: no DIVERGENCE entry). The glide's own states
  store only 0 and 1.
- **Area 175's script message** reads the operand byte by the script
  object's `+3` and `+0xA` unchecked (as the movement-script engine does)
  and indexes `Area175_ScriptMessages` (four words) by it unchecked: an
  operand of 4 or more opens the zero words after the table and then
  whatever follows in `.data`. Ours reads the same; an area number past
  `Area_Descriptors`' 200 aborts where the original reads past the table.
- **Choice 27 leaves the message word for an answer outside 0..4**, and
  area 187's choice reads byte pairs past its four for an answer of 4 or
  more (its choice table's pointer bytes) or before them for a negative one.
  Both stay in `.data`; faithful. What the choice box offers is not read.
- **Effect slots unchecked.** Area 175's two spawns write the record at
  `Effect_Objects + slot << 7` for any slot but `0xFF`; `Effect_FindFree`
  answers only 0..19 or `0xFF`.
- **Tail kind 29's state 2** (the leave to the return point) is set by
  nothing in this band: state 0 moves to 1, and state 1 disarms the tail
  once the message closes. What else writes the tail state (a message
  script's op?) is not read.
- **Tail kind 39's states 2..9** do nothing forever; `Area186_Start` arms
  only 0 and `0xA`.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading: 49 starts, extents, call sites,
  and the twelve in-function jump tables (eleven copies of choice 27's five
  entries at `+0x48`; tail kind 39's five at `+0x15C` through its byte table
  of twelve at `+0x170`). Areas 185..187's rows were printed from the main
  checkout (its `symbols.toml`, before this group's entries); 175..184 from
  this worktree before its entries.
- **Dropped:** none. **Added:** none; every gap between the band's
  functions is padding (`nop` runs) or a jump table.
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x4295E0` and `0x42A280` (object triggers 64 and 14),
  `0x42A290` (tail kind 29, armed by trigger 14's immediate `0x1D`),
  `0x429E20` (`EffectKind18_States[103]`). Each sits in the block of the
  area named.
- **`0x42A1C0`** has no root of its own: only area 186's init calls it; it
  arms tail kind 39 by an immediate.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 49). Every function is reached only in play: the choices
  when an area's message box asks, the handlers from its movement scripts,
  the init on entry, the step and cell hooks on a step in areas 175..185,
  the two tail kinds once armed, the triggers by an object's trigger id,
  the draw while an effect of kind `0x18` sits in state 103.

## 9. Calls across groups

- **Raw address nobody owns**, in `area_w4d_callees.h`: `0x455450` (engine;
  when `Game_AreaNumber` differs from the word `0x802290` it resets a block
  of field state through `0x45E6B0`, `0x4560D0`, `0x455F40`, `0x4561A0`),
  called by `Area179_Init` for `Cond_ByteFA` above 7.
- **By name, ours:** `Effect_FindFree`, `Msg_OpenScript`,
  `Sprite_SetAnimation`, `Flags_Set` / `Clear` / `Test`, `Party_DropIn`,
  `Sound_PlayById`, `Sound_PlayEffect`, `Field_ChangeArea`,
  `MapView_FillCells`, `Kind2_Place`, `ScriptFlags_Set40`,
  `Area131_DisarmTail` (AR3C's, tail-jumped to by tail kind 29),
  `Gpu_SetDrawMode`, `MapView_LinkPrimAt`, `Gpu_SetTile1`,
  `Gpu_SetSemiTrans`, `Math_Sin`, `Math_Cos`, `Gte_RotTransPers`,
  `Gte_StoreDepthF`. **Capcom's:** `_ftol` `0x5B9550` (the draw's copy keeps
  it; ours computes it). No harness edit; no `AH_THEIRS` moved.
- **Table entries of other groups', read in place:** area 187's choice 1
  `Area130_ChoiceTailState2` (AR3C); areas 175..185's choices 4, 7, 8, 11,
  12, 16, 17, 23, 25 (`0x4291F0`..`0x429290`, AR4C's band); the shared `ret`
  `0x437CC0`.
- **Inbound, for the rebinding pass:** `Area_StepHook`'s eleven cases for
  areas 175..185 (`0x56E202`, `0x56E212`, .. `0x56E2A2`, rel32 calls, and
  `event_ops.cpp`'s `kStepHandlers` by the address) into `Area185_StepHook`.
  The rest reach the band through tables read in place: the descriptors,
  `Area_CellHooks` (`0x662F6C`..`0x662FBC`, eleven pairs) into
  `Area179_CellHook`, `Field_ModeTailKinds` 29 / 39 (`0x662D5C` /
  `0x662D84`), `Field_ObjectTriggers` 14 / 64 (`0x662E54` / `0x662F1C`),
  `EffectKind18_States[103]` (`0x654208`).
