# Group E1C: effect kinds 0x36, 0x3C, 0x70 and 0x1C..0x20

**Status:** MEASURED (2026-09-29) - round thirteen, wave one
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 10), on
the round branch's `1bb41df`. **53 functions ours** (`src/game/effect_1c.cpp`,
declarations generated from `symbols.toml`, the module header
`src/game/effect_1c.h`, shadow name `effect_1c`): the cut's 52 rows for E1C
(`analysis/round13_cut.tsv`) and the one start its span holds that no list
had (`0x46F230`), each read to its last instruction with capstone and fuzzed
through the scenario harness's effect mode
([`scenario_harness.md`](scenario_harness.md) section 8), the harness
unchanged: 212,000 rounds, 0 mismatches. 73 controls planted one at a time: 71 refused, 2 equivalent mutants each with a refused near variant (section 8). Kind 0x1C is entered by the
recorded whelp route (section 9); the rest are fuzz only.

Eight kinds of the `Effect_Objects` pool (20 records of 0x80 at `0x7E11E0`;
`Effect_RunObjects` calls `Effect_KindHandlers[+5]` with `Sprite_Current` the
record). The spawners found in our area code (`grep` for `[5] = 0x..`,
2026-09-29): kind 0x1C `Area10_SpawnEffect1C`, kind 0x36
`Area39_SpawnEffect36`, kind 0x1F `Area175_SpawnEffect1F`; kinds 0x1D, 0x1E,
0x20, 0x3C and 0x70 are spawned by nothing in the area band (script data or
code outside it). What each effect looks like on screen is not stated here:
the descriptions are of what the code draws.

| Kind | Dispatcher (`Effect_KindHandlers` cell) | Table (named here) | States |
|---|---|---|---|
| 0x36 | `EffectKind36_Run` `0x46A850` (`0x655428`) | `EffectKind36_Frames` `0x653F88` (4 s16 pairs, data) | one handler |
| 0x3C | `EffectKind3C_Run` `0x46A930` (`0x655440`) | `EffectKind3C_States` `0x653F98` (3, FC1's) | FC1's `_Start`, `_Hold`, E2D's `0x478160` |
| 0x70 | `EffectKind70_Run` `0x46D780` (`0x655510`) | none | one handler |
| 0x1C | `EffectKind1C_Run` `0x46D890` (`0x6553C0`) | `EffectKind1C_States` `0x654210` (5) | `_Start`, `_Rise`, `_Hold`, `_Fade`, `_End` |
| 0x1D | `EffectKind1D_Run` `0x46DD50` (`0x6553C4`) | `EffectKind1D_States` `0x654224` (5) | the same five |
| 0x1E | `EffectKind1E_Run` `0x46E200` (`0x6553C8`) | `EffectKind1E_States` `0x654238` (5) | `_Start`, `_Glow`, `_Burst`, `_FreeModel`, FC1's `Effect_StateRelease` |
| | | `EffectKind1E_ShardStates` `0x65424C` (4, by a shard's +1) | `_ShardFly`, `_ShardFade`, `_ShardNext2`, `_ShardNext3` |
| 0x1F | `EffectKind1F_Run` `0x46EE90` (`0x6553CC`) | `EffectKind1F_States` `0x65425C` (3) | `_Start`, `_Debris`, `Effect_StateRelease` |
| 0x20 | `EffectKind20_Run` `0x46EF70` (`0x6553D0`) | `EffectKind20_States` `0x654268` (7) | `_Start`, `_Grow`, `_Wait`, `_Rise`, `_Shrink`, `_Aim`, `_Fall` |

No dispatcher bounds its index. Each table's length is the run of code
pointers to the next table a dispatcher names, read by hand
(`e1c/e1cdis.py tbl`, scratch): `0x654210` five then `0x654224` (kind 0x1D's)
five then `0x654238` five then `0x65424C` four then `0x65425C` three then
`0x654268` seven then `0x654284`, E1D's kind 0x21 table
([`scenario_harness.md`](scenario_harness.md) 8.8). `band_rows.py` names these
cells `EffectKind18_States[105..133]` by that table's count of 160, an upper
bound (8.3); they are not kind 0x18's.

## 1. What each function does

Offsets are of the effect record (`Sprite_Current`) unless a record is named.
`+1` is the kind's state, `+9` a frame count, the point `+0x34` / `+0x38` /
`+0x3C` (x, z, height; the height's integer part in its high word).

### 1.1 Kind 0x36, kind 0x3C's dispatcher, kind 0x70

- **`EffectKind36_Run` `0x46A850`** (0xDF; hidden in ours
  `EffectKind32_Arc`'s recorded extent, which is 0xA8 bytes and does not
  reach it: an entry of its own, by its `Effect_KindHandlers` cell). While
  `+9` is not 0 it counts down. At 0: a VRAM move (`Gpu_SetDrawMove` at
  `Gfx_PacketNext`) of the 16 x 40 rectangle whose origin is
  `EffectKind36_Frames[+1]` (x / 4 toward zero + 0x2C0, y + 0x100) to frame
  0's origin (the same sums, pushed as dwords); `+1` up, `+0x29` 0, `+9` the
  new `+1` (a longer wait each frame), `Gfx_CommitPrim(+0x29, 0x18)`; at `+1`
  4, `Effect_Release`. Four frames, `+1` 0..3.
- **`EffectKind3C_Run` `0x46A930`** (0x12; hidden in the same extent): `jmp
  [EffectKind3C_States + 4 * +1]`. FC1 named the table and took its first two
  states in round twelve; the dispatcher was catalog part 2.
- **`EffectKind70_Run` `0x46D780`** (0xA5; a `hypothesis` row, read and
  found to be effect code: `Effect_KindHandlers[0x70]`). Nothing while bit 10
  of `Field_ScriptFlags2` is set; else the area's per-frame hook by
  `Game_AreaNumber` - 49 `Area49_EffectFrame`, 117 / 118 / 169 / 171
  `Area117_MembersFrame` .. `Area171_MembersFrame` (all ours, round ten) -
  then, `Field_MemberCount` and `Sprite_Current` read after the hook, for
  each member `i` whose bit `1 << i` (its low byte) is set in `+0xB`: the
  ObjTrio record's `+1` = 1, `+2` = 2 for the leader and 1 for the others,
  `+3` = `+4` = 0.

### 1.2 Kinds 0x1C and 0x1D: a ring and its specks

The two kinds share one shape. A ring of semi-transparent `POLY_G4` quads
round the point at radius `+0x2E`, from the ground `+0x3C` to `+0x3C +
(+0x30 << 16)`, and specks - 0x80 records of 0x14 at `EffectKind30_Shards`
`0x92BF80` (the memory kind 0x30's shards use) - spawned by E2A's `0x471D10`
inside the ring, risen and drawn as `TILE_1`s.

- **`EffectKind1C_Start` `0x46D8B0`**: the point set to (0x600000,
  0x110000) - the spawner's x and z overwritten - and `+0x3C` the ground there
  (`AreaMap_Elevation`, its s16 `<< 16`); radius 0x180, height 0x500; `+9` 0;
  the specks cleared; `+1` up; `Sound_PlayEffect(0x202)`.
- **`EffectKind1D_Start` `0x46DD70`**: radius 0x280, height 0x600 at the
  spawner's point; `+9` 0; the specks cleared; `+1` up. No sound.
- **`_Rise`** (`0x46D930`, `0x46DDA0`): the ring at brightness `+9`, a speck
  spawned every other frame (kind 0x1C on even frames, kind 0x1D on odd), the
  specks moved; `+9` up 4 - at its wrap to 0, `+9` 0x20 and `+1` up (64
  frames).
- **`_Hold`** (`0x46D980`, `0x46DDF0`): the ring at 0xFF, a speck every other
  frame, the specks moved. Kind 0x1C: `+9` down 1, at 0 `+9` 0xFC and `+1` up
  (32 frames). Kind 0x1D: held until the counter byte `0x903848` is 0x1F (an
  event script's count), then `+9` 0xFC and `+1` up.
- **`_Fade`** (`0x46D9D0`, `0x46DE30`): the ring at `+9`, the specks moved,
  none spawned; `+9` down 4, at 0 `+1` up (63 frames).
- **`_End`** (`0x46DA10`, `0x46DE70`): the specks moved; none left,
  `Effect_Release` (a tail jump).
- **`EffectKind1C_DrawRing` `0x46DA20`** (cdecl, the brightness byte):
  `EffectGte_LoadMapCamera`; the point at angle 0 projected at both heights;
  then 16 steps of angle 0x100 (the angle kept in the original's argument
  slot): both points at the new angle projected (`Math_Cos` / `Math_Sin`
  times the radius `>> 4`, `EffectGte_ProjectPoint` twice); a draw mode
  (`Gpu_GetTPage(0, 1, 0x3C0, 0)`, dtd 1 - the fifth pushed zero, left on the
  stack by `add esp, 0x10`, is the mode's texture window) linked at the point
  (`MapView_LinkPrimAt(x, z, 0, 0xC)`); a `POLY_G4` at `Gfx_PacketNext` (read
  again) whose vertices are the last and the new projections (three dwords
  each: x, y, depth floats); the shade 0x40 (0x44 on odd frames) times the
  brightness `>> 8` on the first two vertices, the shade then down 8 for
  steps 4..11 and up 8 for the others, the new shade on the last two; the
  quad linked (0x44) and a second draw mode (dtd 0) linked.
- **`EffectKind1D_DrawRing` `0x46DE80`**: 32 steps of 0x80; the draw mode and
  the quad **committed** at slot 1 (`Gfx_CommitPrim` 0xC and 0x44) only for
  steps below 0xB and from 0x1C - steps 0xB..0x1B are built at the same
  `Gfx_PacketNext` and never committed, a gap of 17 of the 32; the shade down
  8 for steps 8..0x17; no second draw mode.
- **`EffectKind1C_MoveShards` `0x46DD00`**, **`EffectKind1D_MoveShards`
  `0x46E140`**: each speck in use (`+0`) risen by its speed (`+0xC` less the s16
  `+2 << 8`), drawn (E2A's `0x471E20`, linked at the speck's point; or
  `0x46E190`, committed at slot 1), freed when `+0xC` is below the record's
  `+0x3C` (signed; `Sprite_Current` read after the draw); `al` 1 when any was
  in use.
- **`EffectShards_Clear` `0x46E120`**: `+0` of all 0x80 specks 0. Also called
  by E2A's `0x471990`.

### 1.3 Kind 0x1E: the extra sprite's model breaking apart

The model is `Sprite_ObjectsExtra[0]`'s: its faces (0x28 bytes each, four
vertices of three s16 at `+2`) at the pointer `+0x50`, their count a signed
byte at the pointer `+0x54`.

- **`EffectKind1E_Start` `0x46E220`**: the sixteen debris, the eight shards,
  the model split; `+9` 0, `+1` up.
- **`EffectKind1E_Glow` `0x46E250`**: the model's lines at shade `+9`; `+9`
  up 2; from 0x80, `Sound_PlayEffect` 0x206, 0x20E, 0x203, `+9` 0, `+1` up.
- **`EffectKind1E_Burst` `0x46E2B0`**: the debris drawn, the faces stepped,
  the shards drawn; no shard left, `+1` up.
- **`EffectKind1E_FreeModel` `0x46E2D0`**: `Sprite_ObjectsExtra[0]`'s `+0` 0
  (the extra sprite freed); `+1` up. State 4 is `Effect_StateRelease`.
- **`EffectKind1E_DrawModel` `0x46EC20`** (cdecl, the shade byte):
  `Sprite_Current` kept and set to `Sprite_ObjectsExtra`; `Gte_PushMatrix`;
  the count byte read; the extra sprite's matrix (`Sprite_ObjectMatrix`)
  loaded (`Gte_SetRotMatrix`, `Gte_SetTransMatrix`) and a copy handed to
  `Camera_LoadMatrix`; a draw mode (`Gpu_GetTPage(0, 1, 0x380, 0x100)`, dtd 0)
  committed; the faces pointer read; per face a semi-transparent `LINE_F4`
  of the shade, its four vertices projected (`Gte_RotTransPers4` to `+8`,
  `+0x14`, `+0x2C`, `+0x20` - the last two swapped: a closed outline),
  committed with its depths (`Gte_StoreDepthF4`, `Gfx_CommitPrim(1, 0x38)`)
  when `EffectKind1E_Winding` of `(+8, +0x14, +0x20)` or of `(+0x20, +0x2C,
  +8)` is above 0; `Gte_PopMatrix`; `Sprite_Current` put back.
- **`EffectKind1E_Winding` `0x46EE20`** (cdecl): `(b.x - a.x)(c.y - b.y) -
  (c.x - b.x)(b.y - a.y)` of three pairs of s16, in `ax`. See section 7.
- **`EffectKind1E_SplitModel` `0x46E6E0`**: 27 faces, each copied (dword by
  dword) to `0x92C348 + 0x28 i`; its centre (the four vertices' sums `>> 2`)
  to the piece record `0x92C0C0 + 0x18 i`, the centre normalised
  (`Gte_VectorNormalS`) into `+8` and each s16 there `>> 8` (a direction),
  `+0x10..+0x15` 0; the copy's vertices made relative to the centre.
- **`EffectKind1E_StepPieces` `0x46E830`**: each piece's centre x, y moved by
  its velocity `+8` / `+0xA`, its z velocity `+0xC` up by bit 0 of
  `Frame_Counter` and z `+4` by it; `EffectKind1E_TurnPiece(the copy, the
  model's face, the piece)`.
- **`EffectKind1E_TurnPiece` `0x46E890`** (cdecl): the piece's angles
  `+0x10..+0x14` each `Rand & 0xFC0`; a rotation of them (`Gte_RotMatrix`), no
  translation, loaded; each relative vertex turned (`Gte_RotTrans`) and
  written to the model's face plus the centre (16 bits). The model itself is
  rewritten in place each frame.
- **`EffectKind1E_ShardsInit` `0x46E2E0`**, **`EffectKind1E_ShardInit`
  `0x46E320`** (cdecl; also E2B's `0x473810`'s): the cursor
  `EffectKind1E_ShardCursor` `0x675FDC` walked over eight shards of 0x28 at
  `EffectKind30_Shards`; each: its point the record's, a direction
  `(Rand & 0xFF) - 0x80`, `(Rand & 0xFF) - 0x80`, `Rand & 0x7F` normalised
  (`Gte_VectorNormal`) and its z `<< 8`, in use, state 0, size 0, shade 0x40,
  count 4.
- **`EffectKind1E_ShardsDraw` `0x46E3B0`**: `EffectGte_LoadMapCamera`; the
  cursor walked over the eight: each in use stepped (a call through
  `EffectKind1E_ShardStates[+1]`, the handler reading the cursor) and drawn
  (`EffectKind1E_ShardQuad` of the cursor, read after the call); `al` 1 when
  any was in use.
- **`EffectKind1E_ShardQuad` `0x46E400`** (cdecl): FC2's
  `EffectKind30_SparkQuad` with the shard's point `+4`, size `+0x24` and
  shade `+3` - a semi-transparent `POLY_FT4`, the square scaled at its depth
  (`EffectGte_ProjectSize` in place), corners at the x87's 53 bits rounded
  once to floats, `Gpu_GetClut(0xA0, 0x1E3)`, `Gpu_GetTPage(0, 1, 0x2C0,
  0x100)`, `MapView_LinkPrimAt(x, z, 2, 0x48)`.
- **`EffectKind1E_ShardFly` `0x46E5F0`**: moved by twice its direction, size
  up 0x80, count down; at 0 the count 0x40 and state 1. **`_ShardFade`
  `0x46E660`**: moved by its direction, shade and count down 1; at 0 the
  shard freed. **`_ShardNext2` `0x46E6C0`**, **`_ShardNext3` `0x46E6D0`**:
  one body twice - the state up 1. No shard reaches them: state 1 frees
  without stepping.
- **`EffectKind1E_DebrisInit` `0x46EA60`**, **`_DebrisInitOne` `0x46EA80`**
  (cdecl; PSX twin `0x801F3BE4` by callers, unnamed in the sibling): sixteen
  debris records of 0x2C at `0x92C780`: the point the record's; two edges
  (`Math_Cos` / `Math_Sin` of 0x10 and of -0x10) turned in place by a matrix
  of three random angles (`EffectGte_SetDiagonalOne`, `Gte_RotMatrixX`, `_Y`
  of the second negated, `_Z`; `0x5A7C70` between `Gte_PushMatrix` and
  `Gte_PopMatrix`); a scale 10 + `Rand % 4`; angle and shade 0. E3C's
  `0x4851E0` is the same with 0x20 and 8 + `Rand % 8`.
- **`EffectKind1E_DebrisDraw` `0x46EBA0`**: a draw mode committed,
  `EffectGte_LoadMapCamera`, each debris drawn (E3C's `0x485030`, a G3), its
  angle up 0x10, its shade up 0x20 while the record's `+9` is below 4, else
  down 2.

### 1.4 Kind 0x1F: debris

- **`EffectKind1F_Start` `0x46EEB0`**: 32 debris records of 0x2C at
  `EffectKind30_Shards` (E3C's `0x4851E0`); `+9` 0, `+1` up.
- **`EffectKind1F_Debris` `0x46EEE0`**: kind 0x1E's debris loop over the 32;
  the last `Sprite_Current` read's `+9` up 1; above 0x44, `+1` up
  (`Effect_StateRelease`).

### 1.5 Kind 0x20: a fan

- **`EffectKind20_Run` `0x46EF70`**: a **call** through `EffectKind20_States`
  by `+1`, then - `+1` read again not 0 - a tail jump to `EffectKind20_Draw`.
- **`_Start` `0x46EFA0`**: the timer word `+0x2C` 0x20, `+0x64`, `+0x68`,
  `+0x6C` 0. **`_Grow` `0x46EFE0`**: `+0x64` and `+0x68` up 4, 32 frames.
  **`_Wait` `0x46F030`**: 16. **`_Rise` `0x46F060`**: the height up 0x40
  units a frame, `+0x64` down 6, `+0x68` up 6, 12 frames. **`_Shrink`
  `0x46F0C0`**: `+0x64` up 3, `+0x68` down 3, 24 frames. Each: the timer down,
  at 0 the next state's length and `+1` up.
- **`_Aim` `0x46F100`**: after 16 frames, the point and the point one unit
  further in z projected; `+0x6C` = `Math_Ratan2(dy, dx)` of their screen
  difference + 0x400; the timer 0x1E, the fall speed `+0x10` 0, `+1` up.
- **`_Fall` `0x46F1C0`**: `+0x10` up 0x2000 and added to z `+0x38`; `+0x64`
  down 6, not below 8; `+0x68` up 6; after 30 frames `Effect_Release`.
- **`EffectKind20_Draw` `0x46F230`**: a draw mode (dtd 1) committed; E4F's
  `0x493090(a copy of the point, +0x64, +0x68, +0x6C, 0xC0, 0)` - G3
  triangles round the point's screen position, sized and turned by the three
  words. The words are pushed as the registers that held them: `+0x64` with
  the high half of `+0x38`, `+0x68` with `+0x3C`'s, `+0x6C` with `+0x38`'s.
  Ours pushes the same (section 4).

## 2. What the cut and the tool said, settled

- **53, not 52.** `0x46F230` (0x77 bytes) is code no list had
  (`band_rows.py`: "code no list has, in the span of 0x46f1c0"), reached only
  by `EffectKind20_Run`'s tail `jmp`. It is not a shared tail: it has its own
  frame (`sub esp, 0x10`) and `ret`, and the jump is `EffectKind20_Run`'s
  last instruction - a tail call. Taken as `EffectKind20_Draw`, a clone of
  its own; the dispatcher calls it by name.
- **Extents.** The tool's read extents are right for all 53; the cut's sizes
  are the catalog's, padding included (32 of the 52 differ by padding only).
  `0x46F1C0`: the cut's 240 bytes run over `0x46F230`; it is 0x6B.
- **Hidden starts.** 35 rows were hidden in a recorded host: `0x46A850` /
  `0x46A930` in ours `EffectKind32_Arc` (its recorded extent `0046A1E0 A8`
  ends at `0x46A288`; ours `field_c1.cpp` holds none of this code), and the
  rest in Capcom's `0x46D770` (a 16-byte helper, see below), `0x46DD00`,
  `0x46E190`, `0x46E400`, `0x46EE20` - each an entry by its own
  `.data` cell, none a fall-through.
- **The `hypothesis` rows** (`0x46A930`, `0x46D780`, `0x46E200`, `0x46EE90`)
  are all effect code: kind dispatchers, and kind 0x70's handler. All taken.
- **Not taken, in the span, not in the cut**: `0x46D770` (catalog part 2, 16
  bytes: `EffectKind30_ShardsStep` then a tail jump to
  `EffectKind30_SparksDraw`; called by ours `Area135_SpawnCountdown`, AR3D's
  `kEngine46D770`) and `0x46E190` (catalog part 6, PSX twin `0x801F752C`:
  the specks' `TILE_1` committed at slot 1; called only by
  `EffectKind1D_MoveShards`). Neither is in any round-thirteen group; both
  are effect code. For the coordinator. `0x46E190` is `kEffectStd`'s
  `FX_RAW(0x46E190)` stand-in.
- **No case, no shared tail, no second entry** among the 53.

## 3. Calling convention, arguments, answers

All cdecl. The state handlers and dispatchers take nothing. The seven with
arguments: `EffectKind1C_DrawRing` / `1D_DrawRing (unsigned brightness)` and
`EffectKind1E_DrawModel (unsigned shade)` read the byte only (their callers
`push ecx` with the byte in `cl`); `EffectKind1E_ShardInit`, `_ShardQuad`,
`_DebrisInitOne` a record; `_TurnPiece (copy, face, piece)`; `_Winding (a, b,
c)`. Answers read by a caller: `EffectKind1C_MoveShards`,
`EffectKind1D_MoveShards`, `EffectKind1E_ShardsDraw` in `al`;
`EffectKind1E_Winding` in `ax` (its upper half is the original's stack
leftovers: not reproducible, not read).

## 4. The fuzz (`effect_1c_fuzz.cpp`)

`scenario_harness::Run` in effect mode (`g.effect`, kinds 0x36, 0x3C, 0x70,
0x1C..0x20), 4,000 rounds a function. The 46 without arguments are `kEffect`
clones, each with its kind (`+5`) and its table's length as `state_span`
(the dispatchers' exactly: 4, 3, 5, 5, 5, 3, 7; the states their kind's); the
seven with arguments `kCall`, the records pointed into by `Args`
(`EffectKind1E_ShardInit` / `_ShardQuad` a shard of the eight,
`_TurnPiece` a copy, the model's face and its piece, `_DebrisInitOne` a
debris record, `_Winding` three scratch buffers). `ret_mask` `0xFF` on the
three answering in `al`, `0xFFFF` on `_Winding`. `BOF3X_E1C_ONLY=<name>`
runs the clones whose name contains it.

**Tables swapped** (`DataTable`, both sides): `0x653F98` (3), `0x654210` (5),
`0x654224` (5), `0x654238` (5), `0x65424C` (4), `0x65425C` (3), `0x654268`
(7).

**Callees.** The group's own called directly by name (`kPhase` for the
void ones, `kFlag` for the three answering `al`, the three byte-argument
draws with a mask of 0xFF: their callers push a whole `ecx`;
`EffectKind1E_Winding` answering `ax` 0, 1 or -1 three times in four, the
compare's boundary); the other
groups' by address (section 6) - `0x471D10` louder (it fills the first free
speck and answers its index), `0x471E20` hashing the speck's 16 bytes,
`0x485030` the debris record's 0x2C, `0x4851E0` filling the record,
`0x493090` hashing the point and masking the three words to 16 bits and the
two bytes to 8. **Re-listed louder** than the standard rows: 
`EffectGte_ProjectPoint` (the point hashed, the three floats of `out`
filled - the standard row logs both stack pointers and fills nothing),
`EffectGte_ProjectSize` (point and size hashed, `out` filled),
`Gte_RotTrans` and `Gte_RotTransPers4` (the SVECTORs hashed to 6 bytes: the
callers leave the padding word as stack garbage; the four screen points
filled as floats), `Gte_SetTransMatrix` (the rotation hashed and the
translation noted, not the padding), `Gte_SetRotMatrix` (18 bytes).

**Regions** beyond effect mode's: `0x92C5C4..0x92CA40` (the rest of the
0x80 specks past the standard 0x644, kind 0x1E's pieces `0x92C0C0..`, copies
`0x92C348..` and debris `0x92C780..0x92CA40`), the shard cursor `0x675FDC`,
the fuzz's own 0x440-byte model and its four count bytes.

**Seeds**: `Sprite_ObjectsExtra[0]`'s `+0x50` / `+0x54` at the fuzz's model
and count (0, 1, 2, 27 or below 28 - never negative: section 7); the shard
cursor at one of the eight; `0x903848` 0x1F, one either side or random; `+9`
at 0, 1, 2, 4, 0xFC, 0xFF, 0x20, 0x7E, 0x80, 0x44, 0x45, 3 or random - each
compare's boundary. Per function: kind 0x36 `+9` 0 two times in three; kind
0x70 bit 10 of `Field_ScriptFlags2` half the time, the five areas or any,
`Field_MemberCount` 0..8 with every record's `+0xB` kept below bit 3 when
past three (the hooks' recorders may move `Sprite_Current` among the records;
section 7); the specks a third in use, their height after the rise at, one
either side of or anywhere about `+0x3C`; the eight shards half in use, their
state below 4, their count 1, 2, 0 or any; kind 0x20's timer 1, 2, 0 or any
and `+0x64` 13, 14, 15, 8, 0 or any (the floor at 8).

**Disturbance** (the group's case, from its hash only): `+9`, the timer
`+0x2C`, a shard's in-use byte, a shard's state and count, a speck's in-use
byte, `0x903848`, `+0x3C`. Not the shard cursor: a state handler that moved
it would send `EffectKind1E_ShardsDraw`'s walk past the eight shards into
bytes whose `+1` indexes past `EffectKind1E_ShardStates` (the first run did,
and Capcom's copy jumped through a random word) - the original's handlers
never move it.

**Result** (2026-09-29, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=effect_1c`, exit 0): **212,000 rounds over 53 functions, 3,624,591
calls to the stand-ins, 0 mismatches**; 27,000 bytes in 49 regions and the
log compared. Every table entry reached (the 36 `phase` lines of the
coverage: 543..4,044 each), every callee reached (the rarest: the five area
hooks 319..352, `Math_Ratan2` 1,576, `Gpu_SetDrawMove` 2,013). 

Under `BOF3X_SHADOW='*'` (the final build, this worktree): exit 0,
`inject: 6948 ours, 0 left original`, 681 self-test lines, 0 mismatches,
`effect_1c` 0 mismatches there too. With `BOF3X_WIDE=1` (DIV-0041 on):
`effect_1c` alone exit 0, 0 mismatches; `'*'` stops at `battle_e7`
(`GeneWin_ListSlideOut`, `List2`, `List3` - the failure the round's
section 11 hands to its own session, here under the wide bound) before
reaching `effect_1c`; `'*,-battle_e7'` wide: exit 0, 680 self-test lines, 0 mismatches, `effect_1c`
0 mismatches.

## 5. Divergence

None. The 53 are faithful; `DIVERGENCE.md`, `cheats.cpp` and
`widescreen.cpp` name no byte in `0x46A850..0x46A942` or
`0x46D780..0x46F2A6` (grep, 2026-09-29). Where the original would jump
through a table past its code, index `EffectKind36_Frames` past its four,
write past ObjTrio's three records, or walk ~65,000 faces past the model,
ours aborts with a message (section 7) - the project's rule for a fault.

## 6. Calls across groups

**Out of E1C, raw until their owners merge** (`effect_1c_callees.h`, called
`SH_AT`): E2A's `0x471D10` (a speck spawned; from `_Rise` / `_Hold` of both
kinds) and `0x471E20` (a speck's tile; `EffectKind1C_MoveShards`); E3C's
`0x485030` (a debris G3; `_DebrisDraw`, `EffectKind1F_Debris`) and `0x4851E0`
(debris set-up; `EffectKind1F_Start`); E4F's `0x493090`
(`EffectKind20_Draw`); nobody's `0x46E190` (section 2) and the library
layer's `0x5A7C70`. EGT's four are called by name.

**Into E1C from outside** (for the rebinding pass): E2A's `0x471990` calls
`EffectShards_Clear` `0x46E120`; E2B's `0x473810` calls
`EffectKind1E_ShardInit` `0x46E320`. And the engine: `Effect_RunObjects`
through `Effect_KindHandlers[0x1C..0x20, 0x36, 0x3C, 0x70]`, and every state
through its table's cell (in place; `Inject`'s detour at each entry serves
them).

## 7. Latent defects (Capcom's, described, not fixed)

- **`EffectKind1E_DrawModel`'s back-face test reads the halves of a float.**
  `EffectKind1E_Winding` is the PlayStation's screen-space winding test on
  s16 `(x, y)` pairs, as the PSX GTE wrote them. On the PC,
  `Gte_RotTransPers4` writes each screen point as two **floats** (x at `+8`,
  y at `+0xC`), so the pairs it is handed are the low and high 16 bits of
  each vertex's x float: its sign is noise in the mantissa, and which of the
  model's outlines are drawn does not follow their facing. The PC has the
  float version beside it - `0x4941B0` (EGT's doc section 7: the same
  cross product on float vertices, in x87, three other callers) - which this
  caller does not use. What the model looks like on the PC is for the
  owner's eye; a fix would be a divergence.
- **A negative face count** (`Sprite_ObjectsExtra[0]`'s `+0x54` byte 0x80 and
  above) is `movsx`'d and the loop runs `0xFF80..0xFFFF` times, reading far
  past the model. Ours aborts. No reach is established.
- **`EffectKind70_Run` with more than three members**: the bit `1 << i` is
  taken with the shift count masked to five bits and its low byte, so only
  members below 8 (and 32..39) can be written; a set bit for member 3..7
  writes past ObjTrio's three records. Only when `Field_MemberCount` is above
  three and the spawner set those bits; ours aborts.
- **The unbounded dispatchers** (seven: the table at the top) and
  `EffectKind36_Run`'s frame index: a state byte past its table jumps through
  the next table's cells or data. Ours aborts.
- **`EffectKind1C_Start` overwrites its spawner's placement**:
  `Area10_SpawnEffect1C` sets `+0x34` / `+0x38` from its own object, and the
  first state replaces both with (0x600000, 0x110000). Observed, not
  necessarily a defect (the ring may belong at a fixed place in its area).
- **One pool, several strides**: `EffectKind30_Shards` `0x92BF80` holds kind
  0x30's shards (FC2), kinds 0x1C / 0x1D's specks (0x14 apart), kind 0x1E's
  eight shards (0x28), kind 0x1F's debris (0x2C) and, from `0x92C0C0`, kind
  0x1E's pieces. Two of these kinds live at once would write over each other;
  nothing in the code prevents it.
- **Dead states**: `EffectKind1E_ShardNext2` / `_ShardNext3` are unreachable
  (state 1 frees its shard and never steps).
- **Ranges**: kind 0x20's `+0x64` floors at 8 only in `_Fall`; `_Rise`
  takes it down 72 from 128 and `_Shrink` up 72 (net 0). Reproduced.

## 8. Controls

`e1c/controls.py` (scratch): each plant replaces a string inside one function of
`effect_1c.cpp` (found after that function's own first line), rebuilds, runs
the self-test on the clone it touches (`BOF3X_E1C_ONLY`), restores the file and
rebuilds. **73 planted: 71 refused, 2 not refused - each an equivalent mutant
with its near variant refused.** The count is the rounds that mismatched of
4,000. C54 was not refused on its first run: the `EffectKind1E_Winding`
recorder answered a random `ax`, which is 0 once in 65,536 - the fuzz's
fault; its stand-in now answers 0, 1 or -1 three times in four (`FxWinding`)
and C54 is refused (the count shown), with C72 its twin on the first test.
The thinnest: C29 (5 rounds, `+9` at 0x7F of the Glow compare) and C58 (14,
the Debris compare at 0x44): the seeds put `+9` at 0x7E / 0x80 and 0x44 /
0x45, but both functions step `+9` (up 2, up 1) before comparing, so only
the random draws land on the planted boundary. A seed of 0x7D / 0x43 would
thicken them; not added.

| # | Function (EffectKind...) | Plant | Refused |
|---|---|---|--:|
| C00 | 36_Run | `rect[2] = 0x10; -> rect[2] = 0x11;` | 2028 |
| C01 | 36_Run | `if (Cur()[1] == 4) -> if (Cur()[1] == 3)` | 1018 |
| C02 | 3C_Run | `Cur()[1], 3, -> (Cur()[1] + 1) % 3, 3,` | 4000 |
| C03 | 70_Run | `i == 0 ? 2 : 1 -> i == 0 ? 2 : 2` | 895 |
| C04 | 70_Run | `case 0x76: -> case 0x77:` | 329 |
| C05 | 1C_Run | `Cur()[1], 5, -> (Cur()[1] + 1) % 5, 5,` | 4000 |
| C06 | 1C_Start | `SetWord(Cur() + 0x2E, 0x180); -> SetWord(Cur() + 0x2E, 0x181);` | 4000 |
| C07 | 1C_Start | `(static_cast<short>(ground))) << 16 -> (static_cast<short>(ground))) << 15` | 3942 |
| C08 | 1C_Rise | `s[9] = 0x20; -> s[9] = 0x21;` | 269 |
| C09 | 1C_Hold | `s[9] = 0xFC; -> s[9] = 0xFD;` | 266 |
| C10 | 1C_Fade | `s[9] + 0xFC -> s[9] + 0xFD` | 4000 |
| C11 | 1C_End | `if (!SH_CALL -> if (SH_CALL` | 4000 |
| C12 | 1C_DrawRing | `k >= 4 && k < 0xC -> k >= 4 && k < 0xB` | 3940 |
| C13 | 1C_DrawRing | `))) << 16) + UL(s + 0x3C) -> ))) << 15) + UL(s + 0x3C)` | 4000 |
| C14 | 1C_DrawRing | `RingMode(0); -> RingMode(1);` | 4000 |
| C15 | 1C_MoveShards | `MoveShards(at::kShardTile) -> MoveShards(at::kShardTile2)` | 4000 |
| C16 | 1C_MoveShards | `< Long(Cur() + 0x3C) -> <= Long(Cur() + 0x3C)` | 3393 |
| C17 | 1D_Run | `Cur()[1], 5, -> (Cur()[1] + 1) % 5, 5,` | 4000 |
| C18 | 1D_Start | `0x280); -> 0x281);` | 4000 |
| C19 | 1D_Rise | `if (Frame_Counter & 1) -> if (!(Frame_Counter & 1))` | 4000 |
| C20 | 1D_Hold | `== 0x1F) -> == 0x1E)` | 2169 |
| C21 | 1D_Fade | `s[9] + 0xFC -> s[9] + 0xFB` | 4000 |
| C22 | 1D_End | `if (!SH_CALL -> if (SH_CALL` | 4000 |
| C23 | 1D_DrawRing | `k < 0xB || k >= 0x1C -> k < 0xB || k >= 0x1D` | 4000 |
| C24 | 1D_DrawRing | `k >= 8 && k < 0x18 -> k >= 8 && k < 0x17` | 3940 |
| C25 | EffectShards_Clear | `at::kShardStride)[0] = 0; -> at::kShardStride)[0] = 1;` | 4000 |
| C26 | 1D_MoveShards | `any = 1; -> any = 2;` | 4000 |
| C27 | 1E_Run | `Cur()[1], 5, -> (Cur()[1] + 1) % 5, 5,` | 4000 |
| C28 | 1E_Start | `Cur()[9] = 0; -> Cur()[9] = 1;` | 4000 |
| C29 | 1E_Glow | `< 0x80) return; -> < 0x7F) return;` | 5 |
| C30 | 1E_Burst | `if (!SH_CALL -> if (SH_CALL` | 4000 |
| C31 | 1E_FreeModel | `Sprite_ObjectsExtra[0] = 0; -> Sprite_ObjectsExtra[0] = 1;` | 4000 |
| C32 | 1E_ShardsInit | `+ 0x28); -> + 0x2C);` | 4000 |
| C33 | 1E_ShardInit | `shard[3] = 0x40; -> shard[3] = 0x41;` | 4000 |
| C34 | 1E_ShardInit | `<< 8); -> << 9);` | 4000 |
| C35 | 1E_ShardsDraw | `any = 1; -> any = 3;` | 3986 |
| C36 | 1E_ShardsDraw | `SH_CALL(EffectKind1E_ShardQuad)(Cursor()); -> SH_CALL(EffectKind1E_ShardQuad)(At(c));` | **not refused**: equivalent - the state handlers (the real ones and the stand-ins) never move the cursor, so it equals the walk's own pointer after the call. Near variant C35 refused |
| C37 | 1E_ShardQuad | `prim[0x35] = 0x4F; -> prim[0x35] = 0x4E;` | 4000 |
| C38 | 1E_ShardQuad | `StoreFloat(prim + 0x18, left + static_cast<double>(w)); -> StoreFloat(prim + 0x18, left + static_cast<double>(h));` | 4000 |
| C39 | 1E_ShardFly | `+ 0x80u); -> + 0x81u);` | 4000 |
| C40 | 1E_ShardFade | `r[3] - 1 -> r[3] - 2` | 4000 |
| C41 | 1E_ShardNext2 | `r[1] + 1 -> r[1] + 2` | 4000 |
| C42 | 1E_ShardNext3 | `r[1] + 1 -> r[1] + 2` | 4000 |
| C43 | 1E_SplitModel | `SW(r + k) >> 8 -> SW(r + k) >> 7` | 4000 |
| C44 | 1E_SplitModel | `SW(d + 0x16)) >> 2 -> SW(d + 0x16)) >> 1` | 4000 |
| C45 | 1E_StepPieces | `Word(r + 0xC) + odd) -> Word(r + 0xC) + (odd ^ 1))` | 4000 |
| C46 | 1E_TurnPiece | `& 0xFC0u); -> & 0xFE0u);` | 1971 |
| C47 | 1E_TurnPiece | `Word(piece + 2) + static_cast<U>(out[1]) -> Word(piece + 2) + static_cast<U>(out[2])` | 4000 |
| C48 | 1E_DebrisInit | `i < 16; -> i < 15;` | 4000 |
| C49 | 1E_DebrisInitOne | `& 3u) + 10u -> & 3u) + 11u` | 4000 |
| C50 | 1E_DebrisInitOne | `-static_cast<int>(static_cast<short>(ay)) -> static_cast<int>(static_cast<short>(ay))` | 3996 |
| C51 | 1E_DebrisDraw | `DebrisLoop(at::kDebris, 16); -> DebrisLoop(at::kDebris, 15);` | 4000 |
| C52 | 1E_DebrisDraw | `0x20u : 0xFFFEu -> 0x20u : 0xFFFDu` | 3422 |
| C53 | 1E_DrawModel | `SH_CALL(Gfx_CommitPrim)(1, 0x38); -> SH_CALL(Gfx_CommitPrim)(1, 0x39);` | 2905 |
| C54 | 1E_DrawModel | `(prim + 0x20, prim + 0x2C, prim + 8) > 0 -> (prim + 0x20, prim + 0x2C, prim + 8) >= 0` | 1774 |
| C55 | 1E_Winding | `w4 * w1 - w3 * w2 -> w4 * w1 + w3 * w2` | 3999 |
| C56 | 1F_Run | `Cur()[1], 3, -> (Cur()[1] + 1) % 3, 3,` | 4000 |
| C57 | 1F_Start | `i < 32; -> i < 31;` | 4000 |
| C58 | 1F_Debris | `> 0x44) -> > 0x43)` | 14 |
| C59 | 20_Run | `if (Cur()[1] != 0) -> if (Cur()[1] > 1)` | 558 |
| C60 | 20_Start | `SetWord(Cur() + 0x2C, 0x20); -> SetWord(Cur() + 0x2C, 0x21);` | 4000 |
| C61 | 20_Grow | `Tick(0x10); -> Tick(0x11);` | 1601 |
| C62 | 20_Wait | `Tick(0xC); -> Tick(0xD);` | 1601 |
| C63 | 20_Rise | `+ 0x400000); -> + 0x410000);` | 4000 |
| C64 | 20_Shrink | `+ 3); -> + 4);` | 4000 |
| C65 | 20_Shrink | `if (Word(s + 0x2C) != 0) return; -> if (Word(s + 0x2C) > 1) return;` | 834 |
| C66 | 20_Aim | `+ 0x400u); -> + 0x401u);` | 1601 |
| C67 | 20_Aim | `+ 0x10000u); -> + 0x8000u);` | 1601 |
| C68 | 20_Fall | `SetUL(s + 0x64, 8); -> SetUL(s + 0x64, 9);` | 2345 |
| C69 | 20_Draw | `0xC0, 0); -> 0xC1, 0);` | 4000 |
| C70 | 20_Draw | `(UL(s + 0x3C) & 0xFFFF0000u) \| Word(s + 0x68) -> (UL(s + 0x38) & 0xFFFF0000u) \| Word(s + 0x68)` | **not refused**: equivalent to the callee - `0x493090` reads 16 bits of the word (the stand-in's mask), so the high half pushed is never read. Near variant C71 refused |
| C71 | 20_Draw | `(UL(s + 0x3C) & 0xFFFF0000u) \| Word(s + 0x68) -> (UL(s + 0x3C) & 0xFFFF0000u) \| Word(s + 0x6C)` | 4000 |
| C72 | 1E_DrawModel | `(prim + 8, prim + 0x14, prim + 0x20) > 0 -> (prim + 8, prim + 0x14, prim + 0x20) >= 0` | 2044 |

## 9. The rebinding

- **In ours**: the seven state tables and the frames table by name
  (`AddressOf(EffectKind1C_States)` ..., `EffectKind36_Frames`,
  `EffectKind1E_ShardCursor`, `EffectKind30_Shards`); `effect_1c_callees.h`
  keeps the raw callees of other groups and the unnamed pieces, debris and
  model cells.
- **Comments in merged files** (the round-ten form, the value kept): seven
  mentions of `0x46D780` in `area_w1c.cpp`, `area_w3a.cpp`, `area_w4b.cpp`
  now name `EffectKind70_Run`.
- **Left raw, not this group's to edit**: none found. `band_rows.py --refs`
  printed only those seven comments; no `_callees.h` of another group keys on
  an E1C address. When E2A and E2B merge, their calls into `0x46E120` and
  `0x46E320` become `EffectShards_Clear` and `EffectKind1E_ShardInit`.

## 10. The live route

The catalog's reach columns (attract, shop, worldmap, combat) are empty for
all 53. The first-call traces: **`reach_whelp` enters kind 0x1C** - its
call counts (`analysis/calltrace/hash_whelp_orig`, `hash_w2_whelp_orig`)
show `0x46DA20` 159 calls, `0x46DD00` 160, `0x46E120` 1: one whole cycle
(`_Start` once, `_Rise` 64 frames, `_Hold` 32, `_Fade` 63, `_End` once and
released). The state handlers are reached through their table, which the
trace does not count. So kind 0x1C is covered by the coordinator's whelp
frame-hash A/B; the other seven kinds are fuzz only until a recorded route
shows them.

## 11. For `analysis/calltrace/entries_logic.txt`

39 lines appended to the main checkout's file (the other 14 of the 53 were
there with the same extent), under a comment: the hosts `0046D770 C0`,
`0046DD00 17F`, `0046E190 150`, `0046E400 2D9`, `0046EE20 74B` are cut by
these; `0046F230 77` is new.
