# Group E4E: effect kinds 0x9B, 0x9C, 0x9E, 0xA0 and six dispatchers

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..14),
wave four, from the round branch's tip `89c25e1`. **49 functions ours**
(`src/game/effect_4e.cpp`, shadow name `effect_4e`): the cut table's 48 rows
for E4E (`analysis/round13_cut.tsv`, the band `0x48DF90..0x491C96`) and kind
0xA3's dispatcher `0x4912F0`, which no list held (section 9). Each read to its
last instruction with capstone and fuzzed through the scenario harness in
effect mode ([`scenario_harness.md`](scenario_harness.md) section 8) without
edits to it: 196,000 rounds, 0 mismatches; controls in section 5. **No
divergence**; every row is effect code, the nine `hypothesis` rows among them
(section 9). **Fuzz only**: no recorded route enters any of the 49
(section 10).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x9B: a ring of sixteen quads round the point that rises and stays | 5 | `Effect_KindHandlers[0x9B]` (`0x6555BC`), `EffectKind9B_States` `0x655190` (3) |
| 0x9C: a glow and a trail at a fixed place, eight flickering panels, five beams, then the glow shrinking and the beams fading | 14 | `Effect_KindHandlers[0x9C]` (`0x6555C0`), `EffectKind9C_States` `0x65519C` (8) |
| 0x9E: a dispatcher (its states are catalog part 6 rows) and four quads those states draw | 5 | `Effect_KindHandlers[0x9E]` (`0x6555C8`), `EffectKind9E_States` `0x6551BC` (7) |
| 0xA0: a glow at the leader that grows, holds and rises; 32 shards and 16 sparks of its own pools; a trail flown to a fixed point until the chapter's count; a fade | 19 | `Effect_KindHandlers[0xA0]` (`0x6555D0`), `EffectKindA0_States` `0x6551E4` (9) |
| 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9: dispatchers only (their states are catalog part 6 rows); 0xA7's and 0xA9's call through their tables and then draw a glow / a disc | 6 | `Effect_KindHandlers[0xA1..0xA3, 0xA7..0xA9]`, six tables (section 3) |

The names are from what the code does: "ring", "glow", "trail", "panels",
"beams", "plate", "wall", "shards", "sparks" name the primitives each commits
and the cells it steps - not a play-tested fact. Where the game shows these
kinds and what they look like was not traced (section 10; the owner's word,
not this doc's). **The spawners in our source** (a grep of `src/game` for the
kind stored into `+5`): area 100's tail `Area100_TailEffect9B` and
`Area100_SpawnEffect9B` (`area_w2d.cpp`) spawn kind 0x9B at (0x460000,
0x340000); chapter 15 (`scena_sc15.cpp`) spawns kind 0x9C in run 3 (step 2,
once counter 0 is 0xC), kind 0xA0 in run 4 (once counter 0 is 0xF -
`EffectKindA0_Sparkle` waits for 0x14 and `_FlyWait` for 0x17 on the same
counter `0x903848`), kinds 0xA8 and 0xA9 in run 6, and kind 0x9E as a marker
(`Scena15_SpawnMarker` `0x56ADF0`: `+0x34` / `+0x38` the place, `+0x3C` the
ground + 0x200, `+6`, `+7`, `+0xB` its bytes). Kinds 0xA1..0xA3 and 0xA7 have
no spawner in our source. The cut's `label` / `unit` columns say "Area
overlays, world 4" and "Fn_48DDE0 kind 0x9A" for kind 0x9B's three states
(they are 0x9B's, hidden in E4D's `0x48DC40`; section 9).

## 1. What each function does

All are cdecl; the states and dispatchers are `void (void)` on
`Sprite_Current` (an `Effect_Objects` record). "+n" is a byte of the record
unless a word or dword is said. Every draw opens with
`Gpu_GetTPage(0, abr, x, y)` / `Gpu_SetDrawMode(cursor, 0, dtd, page, 0)` (the
fifth word the leftover of GetTPage's five pushes) and commits or links the
primitives it fills; the projected points come from EGT's
`EffectGte_ProjectPoint` / `_ProjectSize` after `EffectGte_LoadMapCamera`.

### 1.1 Kind 0x9B (`EffectKind9B_Run` `0x48DF90`, hidden in E4D's `0x48DC40`)

- `EffectKind9B_Start` `0x48DFB0` (state 0): `+0x3C` = the ground under
  `(+0x34, +0x38)` (`AreaMap_Elevation`, an s16 << 16); the height `+0x14` and
  its speed `+0x20` = 0.
- `EffectKind9B_Rise` `0x48E000` (1): speed up 0x100000, height up by it; the
  ring drawn at a stack copy of the point; at a height of 0x8000000 or more
  (signed), state 2.
- `EffectKind9B_Hold` `0x48E070` (2): the ring at 0x8000000. Never leaves
  (no release; section 7).
- `EffectKind9B_DrawRing` `0x48E0A0` (`point`, `height`): sixteen
  semi-transparent `POLY_G4`, the corners at `point + ((cos << 9) sar 4, (sin
  << 9) sar 4)` for angles 0x100 apart, each quad's bottom at the point's
  height and its top `height` higher, projected for the previous angle and
  this one. Per quad a draw mode (dtd 1) linked 0xC at the corner
  (`MapView_LinkPrimAt`), the quad linked 0x44, a draw mode (dtd 0) linked
  0xC. The shade starts at `((Frame_Counter & 1) + 8) << 2` and is carried on:
  +4 at quads 0..3 and 12..15, -4 at 4..11.

### 1.2 Kind 0x9C (`EffectKind9C_Run` `0x48E320`, hidden in `0x48E0A0`)

A fixed place: the point `(0x308000, 0x740000, ground)` and the trail's other
end `(0x308000, 0x718000, 0xFEC00000)`. Every state but 0 and 7 draws the
trail (catalog part 6's `0x48ED80`: `+0x34`, `+0xC`, the byte `+0x5D`) and
the glow (`EffectKindA0_DrawGlow(+0x34, the word +0x2E, 2)`).

| State | Function | What |
|--:|---|---|
| 0 | `EffectKind9C_Start` `0x48E340` | the points; `+0x2E`, `+0x5D` = 0; `+9` = 8 |
| 1 | `EffectKind9C_Grow` `0x48E3D0` | size `+0x2E` up 0x10, shade `+0x5D` up 8; trail, glow; on `+9` to 0: `+9` = 0x3C, `Sound_PlayEffect(0x20D)` |
| 2 | `EffectKind9C_Hold` `0x48E450` | trail, glow; on `+9` to 0: panels' shade `+0x5E` = 0, `+9` = 4 |
| 3 | `EffectKind9C_PanelsIn` `0x48E4B0` | `+0x5E` up 0x40, the panels in it (0xFF when it wrapped to 0); trail, glow; on 0: `+9` = 4 |
| 4 | `EffectKind9C_PanelsOut` `0x48E530` | `+0x5E` up 0xC0, the panels; trail, glow, the beams (0x40); on 0: `+9` = 0x3C |
| 5 | `EffectKind9C_Beams` `0x48E5B0` | trail, glow, beams; on 0: `+9` = 0x3C, `Sound_PlayEffect(0x20E)` |
| 6 | `EffectKind9C_Shrink` `0x48E620` | `+0x2E` down 0x10 while above 0 (s16), `+0x5D` down 8 while above 0 (s8); trail, glow, beams; on 0: `+9` = 0x10, `+0x2E` = 0x40 |
| 7 | `EffectKind9C_End` `0x48E6B0` | `+0x2E` down 4, the beams that size; on 0 `Effect_Release` (a tail `jmp`) |

- `EffectKind9C_DrawPanels` `0x48E6F0` (`shade`, the byte read): eight
  semi-transparent `POLY_F4` side by side at z 0x718000, x from 0x2C8000 by
  0x10000, from height 0 to 0xFD000000; each a draw mode linked 0xC at its
  left edge, the quad committed 0x38 to slot 1 in `(shade, shade, shade)`.
- `EffectKind9C_DrawBeams` `0x48E800` (`size`): five beams down a zigzag of
  six points at z 0x718000 (x 0x308000 / 0x300000, heights 0xFD000000,
  0xFE000000, 0xFE800000, 0xFF000000, 0xFF800000, 0).
- `EffectKind9C_DrawBeam` `0x48E8E0` (`from`, `to`, `size`): a draw mode
  committed to slot 3; an opaque `LINE_F2` (0x40, 0, 0) between the points
  projected (floats, 0x20 bytes) committed to slot 3; its angle
  `Math_Ratan2` of the `_ftol` screen differences as floats; at each end a
  fan of radius `EffectGte_ProjectSize(end, {size, size})` + (`Frame_Counter`
  & 1) at the angle + 0x400 / + 0xC00, then the two side quads. The ends'
  screen points are `_ftol` of the line's packet floats, read again before
  each use as the original does.
- `EffectKind9C_DrawBeamEnd` `0x48EA80` (`x`, `y`, `r`, `a`, each read as a
  word): eight semi-transparent `POLY_G3` fanned round the screen point,
  centre (0x40, 0, 0), rim black at `(cos, sin) * r sar 12`, 0x100 apart.
- `EffectKind9C_DrawBeamSides` `0x48EBB0` (eight words): a `POLY_G4` with the
  beam's two ends (0x40, 0, 0) and black outer corners at a1 and a2 + 0x800;
  then `rep movsd` copies it 0x44 bytes on - where the first's commit moved
  the cursor - and sets the outer corners at a1 + 0x800 and a2; both
  committed 0x44 to slot 3.

### 1.3 Kind 0x9E (`EffectKind9E_Run` `0x48F070`, hidden in `0x48ED80`)

The dispatcher only; the seven states are catalog part 6 rows `0x48F090`,
`0x48F170`, `0x48F1E0`, `0x48F240`, `0x48F300`, `0x48F360` and
`Effect_StateRelease`. Those states call four draws (the `hypothesis` rows),
each `(a, b)` - `a` the record's point `+0x34`, `b` a half-extent (`+0xC`,
or the cells `0x6762A0` the part-6 states keep) - whose corners are `a + b`,
`a - b` and the two crossed corners, `+6` bit 0 choosing which of x / z of the
crossed corners takes the sum (`Sprite_Current` read afresh for each):

- `EffectKind9E_DrawPlate` `0x48F5D0`: an opaque white `POLY_F4`, linked 0x38
  at `(a.x, a.z)`.
- `EffectKind9E_DrawTexPlate` `0x48F720`: a semi-transparent `POLY_FT4`
  (0x20, 0x20, 0x20), its texture from `EffectKind9E_Pages` record 0 or 1 by
  `+6` bit 2 (page `Gpu_GetTPage(1, 1, x, y)`, CLUT `Gpu_GetClut(0xA0, 0x1F0)`,
  u / v and +0x18 / +0x10 at the corners), linked 0x48.
- `EffectKind9E_DrawWall` `0x48F8F0`: an opaque `POLY_F4` standing on `a + b`
  and one crossed corner, 0x100000 high, shaded `((+6 & 0xFE) << 6, (~+6 &
  0xFE) << 6, 0)`, linked 0x38.
- `EffectKind9E_DrawShadePlate` `0x48FA80`: a draw mode (abr 0) linked 0xC,
  then the plate's corners semi-transparent in the wall's shade, linked 0x38.

### 1.4 Kind 0xA0 (`EffectKindA0_Run` `0x48FC20`, hidden in `0x48FA80`)

| State | Function | What |
|--:|---|---|
| 0 | `EffectKindA0_Start` `0x48FC40` | the glow's point `+0x64..+0x6C` = the leader's (`ObjTrio +0x34..`), 0x800000 higher; size `+0x2E` = 0; `+9` = 8; `Sound_PlayEffect(0x20B)` |
| 1 | `EffectKindA0_Grow` `0x48FCA0` | size up 0x20, `EffectKindA0_DrawGlow(+0x64, +0x2E, 6)`; on `+9` to 0: `+9` = 0x40 |
| 2 | `EffectKindA0_Hold` `0x48FCF0` | the glow; on 0: `+9` = 0x20 |
| 3 | `EffectKindA0_Rise` `0x48FD30` | height up 0xC0000, size down 6, the glow; on 0: `+9` = 0x20, `+6` = 0, the sparks cleared, `+0x34..` = `+0x64..`, shards 0..15 set up there, shards 16..31's speed and shade words 0 |
| 4 | `EffectKindA0_Sparkle` `0x48FDF0` | the `+6` tick (a spark at `+0x34..` every fourth), the shards, the sparks; at the count `0x903848` = 0x14: the head `+0x18..` = `+0x64..`, its speed `+0xC..` = 0, the pull `+0x64..` = `((0x2F8000, 0x278000, 0xF000000) - point) sar 7` |
| 5 | `EffectKindA0_Launch` `0x48FEB0` | tick, shards, sparks; `+9` = 0x20, `Sound_PlayEffect(0x209)` |
| 6 | `EffectKindA0_Fly` `0x48FF00` | tick, sparks, shards, the trail `(+0x18, +0x34, 0x40)`; speed += pull, head += speed, held at the target once its x is below 0x2F8000; `+3` down 4; on 0: shards 16..31 set up at the head (the points swapped round them), `+9` = 0x80 |
| 7 | `EffectKindA0_FlyWait` `0x490090` | the tick spawns a spark at the point and one at the head; sparks, shards, trail; at the count 0x17: `+3` = 0x80, `+9` = 8, `Sound_PlayEffect(0x20A)` |
| 8 | `EffectKindA0_Fade` `0x490180` | size down 0x10, `+3` down 8; sparks; the trail in the shade `+3`; on 0 `Effect_Release` (a tail `jmp`) |

The helpers, most of them kind 0x64's (E3A) forms over this kind's own pools
`EffectKindA0_Sparks` `0x6762B0` (16 of 0x18) and `EffectKindA0_Shards`
`0x676430` (32 of 0x2C):

- `EffectKindA0_DrawGlow` `0x4901D0` (`point`, `size` a word, `colour` a
  byte): `EffectKind64_DrawGlow`'s fan of 32 `POLY_G3` (draw mode 0x2C0 /
  0x100, slot 2), centre `((c & 0xFC) << 5, (c & 0xFE) << 6, c << 7)`, rim
  black - but every vertex at the centre's depth: the stack word E3A's
  version never writes (DIV-0068) is written here. Also called by kind 0x9C
  and by E4F's `0x493570`, `0x4935D0`, `0x493610`.
- `EffectKindA0_DrawTrail` `0x490390` (`from`, `to`, `shade`):
  `EffectKind64_DrawTrail`'s two quads; then `TILE_1` dots from `to +
  (Frame_Counter & 0xF) * pull`, stepping `pull << 4` (`Sprite_Current +0x64..`
  read afresh each step), **while x is above from's - with no count**
  (E3A's has eight; section 7).
- `EffectKindA0_ClearSparks` `0x490630`, `EffectKindA0_SpawnSpark` `0x490650`
  (the first free spark at `+0x34..`: life 0x10, size 0x100, shades 0x60 /
  0), `EffectKindA0_StepSparks` `0x4906A0` (`EffectKind64_StepSparks` over
  sixteen, drawing each with E3A's `EffectKind64_DrawSpark`; al 1 when one was
  in use, no caller reads it).
- `EffectKindA0_InitShard` `0x4906F0`: `EffectKind64_InitShard` with the
  shade 0x40 (that one's 0x20).
- `EffectKindA0_DrawShards` `0x490810`: a draw mode (0x380 / 0x100, slot 2),
  each of the 32 shards drawn and its angle `+0x24` up 0x10.
- `EffectKindA0_DrawShard` `0x490880`: a `POLY_G3` - the shard's point and the
  point plus each edge turned by `+0x24` (`(cos e0 - sin e1) sar 8`, `(sin e0 +
  cos e1) sar 8`, `e2 << 12`, each times the speed `+0x28`), the first vertex
  `(k, k, 0)` with `k` the shade `+0x2A` held to 0..0xFF.
- `EffectKindA0_SwapLong` `0x490A30`: two dwords swapped by three xors.

### 1.5 The dispatchers of kinds 0xA1..0xA3 and 0xA7..0xA9

`jmp [table + +1 * 4]`, unbounded: `EffectKindA1_Run` `0x490A60`,
`EffectKindA2_Run` `0x4910D0`, `EffectKindA3_Run` `0x4912F0`,
`EffectKindA8_Run` `0x491BA0`. `EffectKindA7_Run` `0x491AA0` and
`EffectKindA9_Run` `0x491C60` **call** through theirs and then, with
`Sprite_Current` read again and `+1` not 0, draw: kind 0xA7 the glow
`0x491E30(+0x34, the word +0xC, 7)` (catalog part 6, `EffectKindA0_DrawGlow`'s
form on slot 7), kind 0xA9 the disc `0x492260(the word +0x2E, the word +0x30,
0x80)` (catalog part 6).

## 2. Divergence

None. Every function is a faithful replacement; the aborts are in section 6.
`EffectKindA0_DrawGlow` is E3A's glow without its defect - the original
writes every depth here - so DIV-0068 does not reach it.

## 3. Tables and cells named

The dispatchers do not bound their index; each count is the table's own
length to the next table a dispatcher names, checked by a raw scan of the
image for every cell address in `0x655170..0x6552B0` (each table's start is
named by exactly one dispatcher; `0x655208` / `0x65520C` are data catalog part
6 code reads). `band_rows.py`'s counts ran on into the next tables (18 code entries
for kind 0x9B's, 15 for 0x9C's, 35 for 0xA1's, 29 for 0xA2's).

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind9B_States` `0x655190` | 3 | `0x65519C`, kind 0x9C's (it begins at entry 8 of E4D's kind 0x9A run at `0x655170`) |
| `EffectKind9C_States` `0x65519C` | 8 | `0x6551BC`, kind 0x9E's |
| `EffectKind9E_States` `0x6551BC` | 7 | `0x6551D8`, data (`EffectKind9E_Pages`) |
| `EffectKind9E_Pages` `0x6551D8` | 12 bytes | two records (s16 x, s16 y, u8 u, u8 v), read by `EffectKind9E_DrawTexPlate` only |
| `EffectKindA0_States` `0x6551E4` | 9 | `0x655208`, data |
| `EffectKindA1_States` `0x655220` | 6 | `0x655238`, kind 0xA2's |
| `EffectKindA2_States` `0x655238` | 4 | `0x655248`, kind 0xA3's |
| `EffectKindA3_States` `0x655248` | 11 | `0x655274`, kind 0xA7's (its first five are kind 0xA1's states) |
| `EffectKindA7_States` `0x655274` | 4 | `0x655284`, kind 0xA8's |
| `EffectKindA8_States` `0x655284` | 3 | `0x655290`, kind 0xA9's |
| `EffectKindA9_States` `0x655290` | 4 | `0x6552A0`, kind 0xAA's (E4F's `0x491D70`), inside a run of seven code pointers |
| `EffectKindA0_Sparks` `0x6762B0` | 0x180 bytes | 16 sparks of 0x18 |
| `EffectKindA0_Shards` `0x676430` | 0x580 bytes | 32 shards of 0x2C (two runs of 16, the second at `0x6766F0`); `SuperComboHit_Pool` follows at `0x6769C0` |

Each state of 0x9B, 0x9C and 0xA0 advances `+1` only to its table's next entry
(or releases, or stays), so no state of these reaches past its table in play.
Kind 0xA0's pools are its own: nothing else in the image reads `0x6762B0..
0x6769AF` (unlike kind 0x64's, which share `EffectKind30_Shards`).
**Cells**: the chapter's count `0x903848` (0x14, 0x17), the leader's point
`0x802D74`, the trail's half-width float `0x5C41B8` (`effect_4e_callees.h`).

**Arguments with leftovers** (the fuzz masks each to what the callee reads;
ours passes the value it reads): the glow's size `mov cx, [eax + 0x2E]; push
ecx` (read `mov ax, [esp + 0x60]`); the glow's / trail's shade `mov cl`
(`mov bl` / `mov cl` byte reads); the panels' shade `push eax` with `al` the
byte and the upper bytes `Sprite_Current`'s; the beams' size in state 7 `mov
cx; push ecx`, passed on whole to `EffectKind9C_DrawBeam`, which reads the
word; the beam ends' and sides' arguments the `_ftol` dwords and the
projected radius dword, read as words (`movsx`); kind 0xA7's `mov dx, [eax +
0xC]; push edx`.

## 4. The fuzz (`effect_4e_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_4e`, effect mode (`g.effect`; kinds 0x9B,
0x9C, 0x9E, 0xA0, 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9, each clone its own),
4,000 rounds a function (`BOF3X_E4E_ONLY=<name>` runs the clones whose name
holds it, `BOF3X_E4E_ROUNDS` the rounds). Shapes: 34 `kEffect` (the
dispatchers with `state_span` their table's count, the states, and the
no-argument helpers), 15 `kCall` (the draws and helpers with arguments,
`Args` handing them the record's own points as the states do: `+0x34` /
`+0xC` / `+0x64` / `+0x18`, a shard of the pool, sizes and shades over random
upper bytes, the swap's two dwords - the same dword one time in eight, where
the xors make it 0). The ten state tables are swapped for recorders on both
sides (`DataTable`), so every entry of every table is reached (coverage line:
each 340..1,600 times).

**Seeds** (`Seed`): `+9` at 1 (the countdowns' edge), `+6` at multiples of 4
and not, the size word `+0x2E` at 0, 0x10, 0x11, the sign edges, the shades
`+0x5D` (s8 edges) and `+0x5E` (0, 0x40, 0xC0), the chapter's count at 0x14,
0x17 and around; kind 0x9B's height so that this step lands at, below and
above 0x8000000; kind 0xA0's head so that this step's x lands at, below and
above 0x2F8000; the sparks half in use with lives at 1; the shards' shade
word at its clamps (-1, 0, 0xFF, 0x100) and speeds. **The trail's dot loop**
(`EffectKindA0_DrawTrail`, section 7) ends only when the pull's x is negative:
for that clone every record's pull x is -0x100..-0x4000 and its point within
0x10000 behind and 0x80000 ahead of its head, so the loop ends in a few hundred
dots on both sides whichever record the harness's disturbance makes current.

**Group disturbance** (`Disturb`, from the hash only): `+9`, `+6`, the size
word, the two shades, `+3`, the chapter's count. Not the pull (the loop's end
depends on it).

**Callees listed** (beyond the harness's standard and effect-standard rows):
the group's own called directly by name - the three void helpers `kPhase`,
`EffectKindA0_StepSparks` `kFlag`, the rest typed by what they read (a stack
point hashed and not logged, a record or pool point logged and hashed, bytes
and words masked); `EffectKindA0_SwapLong`'s stand-in swaps (the states read
the points again after it); E3A's `EffectKind64_DrawSpark` by name (0x18
hashed); `0x491E30` re-listed with the size a word (the standard row compares
the whole register, whose upper half is `edx`'s leftover). **Re-listed louder
or safer than the standard rows**: `Gfx_CommitPrim` and `MapView_LinkPrimAt`
stop the cursor 0x90 short of the packet buffer's end, not 0x40 (the beam
sides' copy and the textured plate write up to 0x88 / 0x48 bytes past the
cursor before it moves); `EffectGte_ProjectSize` hashes both words of the
size (both callers write both; the fold's row hashes the first). **Not
re-listed**: `EffectGte_ProjectPoint` (the fold's fractions serve the
`_ftol` reads), `Math_Cos` / `Math_Sin`, `Math_Ratan2` (its float arguments
compared whole), the square root (not called).

**Regions**: kind 0xA0's pools `0x6762B0..0x6769AF` (0x700) beyond the effect
standard ones.

**The result** (in this worktree): `effect_4e` alone exit 0, **196,000 rounds
over 49 functions, 2,941,298 calls to the stand-ins, 0 mismatches**; 26,548
bytes of state in 46 regions compared. `'*'` and `'*'` with `BOF3X_WIDE=1`:
section 8.

## 5. Controls

CONTROLS_TABLE

## 6. Aborts

Where the original jumps past what it owns or loops without end, ours aborts
with a message (the round-nine rule, no ledger entry):

- a dispatcher's `+1` past its table (the original jumps through the dword
  after: the next kind's table, or data - `EffectKind9E_States`' eighth
  "entry" is `EffectKind9E_Pages`' first dword, 0x340);
- `EffectKindA0_DrawTrail` past 0x400 dots (section 7: the original draws on
  until the coordinate wraps). Ordinary play draws about nine.

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0xA0's trail can draw without end.** `EffectKindA0_DrawTrail`'s dots
  start at `to + (Frame_Counter & 0xF) * pull` and step `pull << 4` while x is
  above `from`'s; nothing counts them (kind 0x64's same trail stops at
  eight). State 4 sets the pull to `(0x2F8000 - x) sar 7` from the glow's
  point, the leader's. When the leader stood **west of x 0x2F8000** (the pull
  positive), the first frame of state 6 draws with `from` still the glow's
  point and `to` the same point: with `Frame_Counter & 0xF` not 0 the first dot
  is east of `from` and every step moves it further east, so the loop runs
  until x passes 0x7FFFFFFF (about 2^31 / (16 * pull) dots of 0x14 bytes of
  packet each - far past `Gfx_CommitPrim`'s pool, which stops advancing 0x54
  short of its end, so the dots overwrite the last packet until the loop ends).
  From the second frame the head is held at 0x2F8000 and the loop ends at
  once. With the leader east of 0x2F8000 (the pull negative, the direction the
  hold at 0x2F8000 assumes) the dots run back towards the head and stop; a
  pull of 0 (the leader within 0x7F of it) draws none. Where chapter 15's run
  4 has the leader when it spawns kind 0xA0 is not measured. Ours aborts past
  0x400 dots (section 6); the nearest sensible answer would be E3A's count of
  eight. The fix and its ledger entry are the owner's word.
- **`EffectKind9C_DrawBeamSides` copies its first quad to where it expects the
  cursor**: `rep movsd` to the first quad's address + 0x44 regardless of
  whether `Gfx_CommitPrim` moved the cursor; when the packet pool is full
  (the commit stops 0x54 short of the end) the copy writes up to 0x34 bytes
  past the pool. Ours copies the same.
- **Shards 16..31 are drawn from state 4 on**, before state 6's end sets them
  up: state 3 zeroes only their speed and shade words, so each is drawn as a
  black triangle collapsed onto a stale point (whatever an earlier life of
  the kind left); not visible if the renderer drops degenerate triangles.
- **Kind 0x9B never leaves state 2** (no release, no count): the ring stays
  until something else frees the record (the area's tail, an area change).
  Not a fault on its own; noted for whoever traces area 100.
- **`EffectKindA0_StepSparks`' answer is read by no caller**, and kind 0xA0's
  state 8 lowers the glow's size word that no state draws any more.
- `EffectKindA0_DrawGlow` calls `EffectGte_ProjectSize`, which divides by the
  projected depth unchecked (EGT's defect, [`effect_gte.md`](effect_gte.md)).

## 8. Self-tests

STAR_RESULTS

`tools/ledger_check.py`: LEDGER_RESULT.

## 9. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 48 to the byte (8,656 bytes against the
  cut's 8,931: 29 differ by padding only, none by code); every extent checked
  against the disassembly, none changed. `0x4912F0` (0x12 bytes) is added.
- **Added: `0x4912F0`**, `Effect_KindHandlers[0xA3]`, a catalog part 2 row
  ("Table Effect_KindHandlers", hidden in `0x490A30`) in the band that no list
  held and the tool did not flag (its reach is a `.data` cell only): a kind's
  dispatcher, taken whole with its table (the brief's addendum). Its states
  are catalog part 6 rows.
- **Hidden starts**: 29 of the cut's, each an entry by address - a cell of
  `Effect_KindHandlers` or of a kind's table. Hosts: E4D's `0x48DC40` for
  kind 0x9B's four (E4D's recorded extent 0x460 spans them; E4D takes
  `0x48DC40` itself this wave, wave four); `0x48E0A0` (ours) for kind 0x9C's
  nine - its code ends with `ret` at `0x48E310`, so none is a fall-through;
  `0x48ED80` (catalog part 6) for `0x48F070`; `0x48FA80` (ours) for kind
  0xA0's ten, after its `ret` at `0x48FC1C`; `0x490A30` (ours) for `0x490A60`,
  `0x4910D0` (and `0x4912F0`), after its `ret` at `0x490A52`; `0x4918B0`
  (catalog part 6) for `0x491AA0`, `0x491BA0`, `0x491C60`. No start is a case
  or a shared tail; none dropped.
- **The nine `hypothesis` rows** (`0x48E0A0`, `0x48EA80`, `0x48EBB0`,
  `0x48F070`, `0x48F5D0`, `0x48F720`, `0x48F8F0`, `0x48FA80`, `0x490A60`):
  all effect code (kind 0x9B's ring, kind 0x9C's beam fans and sides, kind
  0x9E's dispatcher and draws, kind 0xA1's dispatcher), taken.
- **The cut's `label` / `unit` columns**: "Area overlays, world 4" / "Fn_48DDE0
  kind 0x9A" for `0x48DFB0`, `0x48E000`, `0x48E070` - they are kind 0x9B's
  states 0..2 (`EffectKind9B_States`, which begins inside the run the unit
  scan read from E4D's kind 0x9A table `0x655170`). The PSX twins
  (`analysis/pairs_propagated.json`): none for the 49 (the band's three -
  `0x48ED80`, `0x48F3D0`, `0x490A80` - are catalog part 6 rows).
- **Not taken, in the band** (catalog part 6, "Scenario effects", in no group
  of this round; the coordinator's to place): `0x48ED80` (kind 0x9C's trail,
  PSX twin `0x801D1C1C`) and the six states of kind 0x9E in `0x48ED80`'s
  extent (`0x48F090`, `0x48F170`, `0x48F1E0`, `0x48F240`, `0x48F300`,
  `0x48F360`), `0x48F3D0` (PSX twin `0x801F9020`, called by kind 0x9E's states
  1 and 2); kinds 0xA1..0xA3's states `0x490A80`, `0x490BB0`, `0x490C50`,
  `0x490E20`, `0x490FA0` (PSX twin `0x801D21B4` for the first), `0x4910F0`,
  `0x491140`, `0x4911C0`, `0x491270`, `0x491310`, `0x491410`, `0x4914B0`,
  `0x491680`, `0x4917C0`; `0x4918B0`; kinds 0xA7..0xA9's states `0x491AE0`,
  `0x491B00`, `0x491B40`, `0x491B70`, `0x491BC0`, `0x491BF0`, `0x491C40`,
  `0x491CA0`, `0x491CD0`, `0x491D10`, `0x491D30`; and the two draws the
  dispatchers call, `0x491E30` and `0x492260` (E4F's band). Each is reached
  only through the tables named here or by the rows listed.

## 10. The live route

The catalog's reach columns (attract, shop, worldmap, combat) are empty for
all 49 rows, and no file under `analysis/calltrace` but the entry lists names
any of them (a scan of every trace for the 49 addresses, 2026-10-03). **Fuzz
only**: kind 0x9B is area 100's (its tail, `area_w2d.cpp`), kinds 0x9C, 0x9E,
0xA0, 0xA8, 0xA9 chapter 15's runs 3..6; a recorded route through either
would let the coordinator's frame-hash A/B cover them.

## 11. Calls across groups and the rebinding

- **Out of the group**: EGT's four (`EffectGte_*`, merged) by name; E3A's
  `EffectKind64_DrawSpark` `0x4820C0` by name from `EffectKindA0_StepSparks`
  (the brief's inbound note: E4E's `0x4906A0`). No call to a group of this
  wave. Raw, to rows of no group (`effect_4e_callees.h`): `0x48ED80` (kind
  0x9C's states 1..6), `0x491E30` (kind 0xA7's dispatcher), `0x492260` (kind
  0xA9's), and Capcom's library `0x5A7C70`.
- **Into the group from outside** (for the rebinding pass): E4F's `0x493570`,
  `0x4935D0` and `0x493610` call `EffectKindA0_DrawGlow` `0x4901D0` (raw in
  E4F's files until both merge); catalog part 6's `0x48F1E0` / `0x48F300` call
  `EffectKind9E_DrawPlate`, `0x48F240` calls `_DrawTexPlate`, `_DrawWall` and
  `_DrawShadePlate`. Engine: `Effect_RunObjects` through
  `Effect_KindHandlers[0x9B..0xA9]`.
- **Rebinding**: `band_rows.py --refs`: **no raw reference** to any of the 49
  in `src/game` (and none to `0x4912F0` by grep) - nothing to rebind. The
  eleven state tables and the two pools are named as `[[data]]` (section 3).

## 12. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03), one line per function with
the extent read (section 9), 33 lines: the other 16 were there already with
the same extent. The host lines `0048E0A0 64C`, `0048FA80 750` and `00490A30
E80` stand beside the smaller extents added for those three (0x271, 0x19D,
0x23); E4D's `0048DC40 460` beside kind 0x9B's four hidden starts.
