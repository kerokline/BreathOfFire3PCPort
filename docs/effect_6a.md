# Group E6A: kind 0x18's sub-kinds 0x2D, 0x2E..0x32, 0x3E and 0x4A's state 1

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave six, from the round branch's tip `c4deb22`. **48 functions ours**
(`src/game/effect_6a.cpp`, shadow name `effect_6a`): the cut table's 48 rows
for E6A (`analysis/round13_cut.tsv`, the band `0x50C0D0..0x50E3FA`), none
added, none dropped. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
192,000 rounds, 0 mismatches; CONTROLS_SUMMARY. **Fuzz only**: no recorded
route enters any of the 48 (section 9). Every row is effect code (no
`hypothesis` row in the group; none left original). No full-frame fill in
the band (no DIV-0041 site); no divergence.

All are sub-kinds of effect kind 0x18: `EffectKind18_Run` (`0x46D830`, ours)
jumps through `EffectKind18_States` by `+1`, which `EffectKind18_Start` copies
from `+0xB`. Seven sub-kinds of the band dispatch again by `+2` through a
table of their own, five states each, the shape of E5G's sub-kind 0x2C
([`effect_5g.md`](effect_5g.md) section 1.3): place (0), wait for the leader at
the cell's front (1), slide open (2), wait for the leader two cells off (3),
slide shut (4, back to 1). The eighth row is sub-kind 0x4A's state 1, an entry
of E5G's table.

| Sub-kind | Functions | Reached through |
|---|--:|---|
| 0x2D: one textured quad at a variant's cell, lying across x or z (bit 0 of the spawn's z cell clear: across z), at the variant's height, flat; it slides 0x20 a frame to 0x100 (sound 0x200) and back (sound 0x201), the direction a sign that bit 1 of the spawn's z cell picks; linked into the map's depth order at the record (dy -2) | 7 | `EffectKind18_States[0x2D]` (`0x654120`), `EffectKind18Sub2D_States` `0x65EAB0` (5) |
| 0x3E: 0x2D's states with the front one cell further back (the centre at cell - 2, against cell + 1 everywhere else), the far test across the centre at 0x8000 (section 7), and **nothing drawn**: the slide and the sounds only | 6 | `EffectKind18_States[0x3E]` (`0x654164`), `EffectKind18Sub3E_States` `0x65EB08` (5) |
| 0x2E, 0x30, 0x31: one quad on the ground (four `AreaMap_Elevation` reads), across x or z (the spawn's z cell 0: across z), sliding 0x20 a frame to -0x100 and back; three draws that differ only in the texture word | 7, 7, 7 | `EffectKind18_States[0x2E]`, `[0x30]`, `[0x31]` (`0x654124`, `0x65412C`, `0x654130`); `EffectKind18Sub2E_States` `0x65EB30`, `_30_` `0x65EB6C`, `_31_` `0x65EB88` (5 each) |
| 0x2F, 0x32: the same sliding to +0x100, one draw between them (`EffectKind18Sub2F_Draw`, which E6B's sub-kinds call at 13 sites) | 6 + the draw, 6 | `EffectKind18_States[0x2F]`, `[0x32]` (`0x654128`, `0x654134`); `EffectKind18Sub2F_States` `0x65EB50`, `_32_` `0x65EBA4` (5 each) |
| 0x4A's state 1: once a bit of `+0xB` (the variant's byte E5G's place wrote) is set in `Cond_ByteFE`, and `Field_Request` is not 2, the map byte 0xA1 at the cell and the next along z (`AreaMap_SetByte`), sound 0x200 unless a message is up, `+2` up (to 0x2C's opening); E5G's `EffectKind18Sub2C_Draw` either way | 1 | `EffectKind18Sub4A_States[1]` (`0x65EA7C`, E5G's table) |

What each looks like in the game is not stated here (the owner's to say);
the descriptions are what the code draws and tests. As in E5G's doc, "panel"
is a word for a textured quad that slides; whether these are doors, gates or
something else is not established, and why 0x3E slides and sounds with
nothing drawn is not known (another sub-kind's record may draw it, or the
slide may only gate the sounds). **No spawner is found**: a grep of our
sources for these sub-kind numbers stored into `+0xB` finds none (kind 0x18
records come from the areas' effect lists, data not code); the spawn's `+0x36`
is the variant and its `+0x3A` the axis (0x2D: two bits of it).

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`, the seven dispatchers `evidence`); no PSX twin gives a name
(`analysis/pairs_propagated.json` has no pair in the band).

## 1. What each function does

**The front test** (every waiting and place state): the leader's point
(`ObjTrio +0x34` / `+0x38`) against the record's cell - with `+8` set, z first:
`|z - ((cell z + d) << 16 | 0x8000)|` within 0x8000, then `|x - (cell x << 16)|`
or `|x - (cell x << 16) - 0x10000|` within 0x10000; without, x first and z
second. `d` is +1 for every sub-kind but 0x3E's -2. **The far test** is the
same at 0x20000 both (0x3E: 0x8000 then 0x20000). The absolute value is
`cdq; xor; sub` (0x80000000 stays negative, so near). Every word the draws
compute is a 16-bit value (the originals store `cx` / `dx` / `di`).

### 1.1 Sub-kind 0x4A's state 1

| PC | Name | What |
|---|---|---|
| `0x50C0D0` | `EffectKind18Sub4A_WaitCond` | `+0xB & Cond_ByteFE` and `Field_Request != 2`: `AreaMap_SetByte(+0x36, +0x3A, 0xA1)`, `Sprite_Current` read again, `AreaMap_SetByte(+0x36, +0x3A + 1, 0xA1)`, `Field_Request` read again (0: sound 0x200), `Sprite_Current` read again, `+2` up; a tail jump to `EffectKind18Sub2C_Draw` (E5G's) either way (hidden in E5G's `0x50BDC0`) |

### 1.2 Sub-kind 0x2D

| PC | Name | What |
|---|---|---|
| `0x50C140` | `EffectKind18Sub2D_Run` | `jmp [EffectKind18Sub2D_States + +2 * 4]`, unbounded (hidden in E5G's `0x50BDC0`) |
| `0x50C160` | `_Place` (0) | from the spawn's z cell's low byte: `+0xA` = bit 1 (`sar dl, 1; and dl, 1`), `+8` = bit 0 clear; the variant `v` = s16 `+0x36`: `+0x36` / `+0x3A` the cell bytes of `0x65EAC4[v]`, `+0x3E` the s16 of `0x65EAE4[v]`, `+0x30` 0, `+2` up; the leader at the front: `+0x30` = 0x100, `+2` = 3. The draw |
| `0x50C2A0` | `_WaitNear` (1) | the front test: sound 0x200 unless `Field_Request`, `+2` up (`Sprite_Current` read after the sound). The draw |
| `0x50C380` | `_Open` (2) | `+0x30` up 0x20; at 0x100 (signed) `+2` up. A tail jump to the draw |
| `0x50C3A0` | `_WaitFar` (3) | the far test fails: `+2` up. The draw |
| `0x50C470` | `_Close` (4) | `+0x30` down 0x20; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1. A tail jump to the draw |
| `0x50C4B0` | `EffectKind18Sub2D_Draw` | a draw mode (page 0x95) linked by `MapView_LinkPrimAt(+0x34, +0x38, -2, 0xC)`; one POLY_FT4 in `Prim_VertexScratch`: with `+8` y `(+0x3A << 7) - 0x3FC0` at all four corners and x `(+0x36 << 7) - slide - 0x3F40` (corners 0, 2) / `- 0x4040` (1, 3); without, x `(+0x36 << 7) - 0x3FC0` and y `slide + (+0x3A << 7) - 0x3F40` / `- 0x4040`; `slide` = `+0x30` times the s8 `0x65EB04[+0xA]` (16-bit `imul`). Flat: corners 0, 1 at `+0x3E - 0x140`, 2, 3 at `+0x3E`. `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture(((+8 == 0) ^ +0xA) << 16 \| +8 << 21 \| 0x1500117, p, 1)`, linked likewise (0x48) |

### 1.3 Sub-kind 0x3E (nothing drawn)

| PC | Name | What |
|---|---|---|
| `0x50C6E0` | `EffectKind18Sub3E_Run` | `jmp [EffectKind18Sub3E_States + +2 * 4]`, unbounded (hidden in `0x50C4B0`) |
| `0x50C700` | `_Place` (0) | `+8` = whether the spawn's z cell (word `+0x3A`) is 0; the variant's cell (`0x65EB1C`) into `+0x36` / `+0x3A`, `+0x30` 0, `+2` up; the leader at the front (cell - 2): `+0x30` = 0x100, `+2` = 3 |
| `0x50C830` | `_WaitNear` (1) | the front test (cell - 2): sound 0x200 unless `Field_Request`, `+2` up |
| `0x50C910` | `_Open` (2) | `+0x30` up 0x20; at 0x100 `+2` up |
| `0x50C930` | `_WaitFar` (3) | the far test (0x8000 across the centre of cell - 2, 0x20000 along the edge) fails: `+2` up |
| `0x50CA00` | `_Close` (4) | `+0x30` down 0x20; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1 |

### 1.4 Sub-kinds 0x2E, 0x2F, 0x30, 0x31, 0x32

Five copies of one set of states, compared instruction by instruction
(`e6cmp.py` in the scratch directory): they differ only in the cell table,
the draw called and the slide's direction.

| Sub-kind | Run | Place, WaitNear, Open, WaitFar, Close | Cells (room) | Slide | Draw |
|---|---|---|---|---|---|
| 0x2E | `0x50CA40` | `0x50CA60`, `0x50CB80`, `0x50CC60`, `0x50CC80`, `0x50CD50` | `0x65EB44` (4) | to -0x100 | `EffectKind18Sub2E_Draw` `0x50CD90` |
| 0x2F | `0x50CFE0` | `0x50D000`, `0x50D120`, `0x50D200`, `0x50D220`, `0x50D2F0` | `0x65EB64` (2) | to +0x100 | `EffectKind18Sub2F_Draw` `0x50E1C0` |
| 0x30 | `0x50D330` | `0x50D350`, `0x50D470`, `0x50D550`, `0x50D570`, `0x50D640` | `0x65EB80` (2) | to -0x100 | `EffectKind18Sub30_Draw` `0x50D680` |
| 0x31 | `0x50D8D0` | `0x50D8F0`, `0x50DA10`, `0x50DAF0`, `0x50DB10`, `0x50DBE0` | `0x65EB9C` (2) | to -0x100 | `EffectKind18Sub31_Draw` `0x50DC20` |
| 0x32 | `0x50DE70` | `0x50DE90`, `0x50DFB0`, `0x50E090`, `0x50E0B0`, `0x50E180` | `0x65EBB8` (2) | to +0x100 | `EffectKind18Sub2F_Draw` `0x50E1C0` |

Each `_Run` is `jmp [EffectKind18SubNN_States + +2 * 4]`,
unbounded, hidden in the draw before it. `_Place` (0): `+8` = whether the
spawn's z cell is 0, the variant's cell into `+0x36` / `+0x3A`, `+0x30` 0, `+2`
up; the leader at the front: `+0x30` = +-0x100 (the slide's end), `+2` = 3; the
draw. `_WaitNear` (1): sound 0x200 unless `Field_Request`, `+2` up; the draw.
`_Open` (2): the slide 0x20 toward its end, there (signed, `>=` / `<=`) `+2`
up; a tail jump to the draw. `_WaitFar` (3): the far test fails, `+2` up; the
draw. `_Close` (4): the slide 0x20 back, at 0 or past it sound 0x201 unless
`Field_Request` and `+2` = 1; a tail jump to the draw. None of these places
writes `+0x3E` or `+0xA`: the draws do not read them.

**The ground draws** (`0x50CD90`, `0x50D680`, `0x50DC20`, `0x50E1C0`; the
first 137 instructions of each identical): the draw mode (page 0x95)
committed at slot 6 (0xC); one POLY_FT4: with `+8` y `(+0x3A << 7) - 0x3FC0`
at all four corners and x `(+0x36 << 7) - +0x30 - 0x3F40` (corners 0, 2) /
`- 0x4040` (1, 3); without, x `(+0x36 << 7) - 0x3FC0` and y `(+0x3A << 7) +
+0x30 - 0x3F40` / `- 0x4040`. The heights from the ground: each vertex's 16.16
point is `(s16 + 0x4000) << 9`; `AreaMap_Elevation` at vertex 0's point twice
(corner 0 = `0x40 - h / 2`, corner 2 = `0x180 - h / 2`, a signed divide), then
at vertex 1's point (read from the scratch after the second call) twice
(corners 1 and 3 likewise). `Gte_RotTransPers4`, `Gte_PrimDepths4_10`; the
texture word from `Sprite_Current` read again; `Prim_SetTexture(word, p, 1)`,
committed at slot 6 (0x48). The words:

| Draw | Texture word |
|---|---|
| `EffectKind18Sub2E_Draw` `0x50CD90` | `(+8 * 0x21) << 16 \| 0x150010F` (`shl 5; add; shl 16`: `+8` at bits 16 and 21) |
| `EffectKind18Sub30_Draw` `0x50D680` | `(+8 == 0) << 16 \| +8 << 21 \| 0x150010F` |
| `EffectKind18Sub31_Draw` `0x50DC20` | `((+8 == 0) < 0x10) \| +8 << 21 \| 0x150010F` - always `+8 << 21 \| 0x150010F` (section 7) |
| `EffectKind18Sub2F_Draw` `0x50E1C0` | `+8 << 21 \| 0x150010F` |

## 2. Divergence

None: each function is a faithful replacement. `widescreen.cpp`,
`cheats.cpp` and `DIVERGENCE.md` patch no byte inside the band (a grep of
every address); no full-frame fill is in it (no `320.0f` / `240.0f`
operand: every draw is a projected quad).

## 3. The tables

**The sub-state tables** (`symbols.toml` `[[data]]`): each its own length,
checked by hand against what the states store into `+2` (1..4, back to 1;
`_Place` sets 3) and against the dword after it (`band_rows.py` reads 5 code
entries for each; here they agree):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind18Sub2D_States` `0x65EAB0` | 5 | `0x65EAC4`, 0x2D's cells (data) |
| `EffectKind18Sub3E_States` `0x65EB08` | 5 | `0x65EB1C`, 0x3E's cells (data) |
| `EffectKind18Sub2E_States` `0x65EB30` | 5 | `0x65EB44`, 0x2E's cells (data) |
| `EffectKind18Sub2F_States` `0x65EB50` | 5 | `0x65EB64`, 0x2F's cells (data) |
| `EffectKind18Sub30_States` `0x65EB6C` | 5 | `0x65EB80`, 0x30's cells (data) |
| `EffectKind18Sub31_States` `0x65EB88` | 5 | `0x65EB9C`, 0x31's cells (data) |
| `EffectKind18Sub32_States` `0x65EBA4` | 5 | `0x65EBB8`, 0x32's cells (data) |

**The data read in place** (raw in `effect_6a_callees.h`, not named in
`symbols.toml`; their bytes are not copied here): 0x2D's cells `0x65EAC4`
(two bytes a variant) and heights `0x65EAE4` (s16), room 16 each (the last
entry of each zero); its signs `0x65EB04` (s8 by `+0xA`, room four before
`EffectKind18Sub3E_States`; the place writes 0 or 1); 0x3E's cells
`0x65EB1C` (room 10, to `EffectKind18Sub2E_States`); 0x2E's `0x65EB44`
(room 4), 0x2F's `0x65EB64`, 0x30's `0x65EB80`, 0x31's `0x65EB9C`, 0x32's
`0x65EBB8` (room 2 each). Each of the last five is followed by a two-byte
sign pair of the shape 0x2D's `0x65EB04` and E5G's `0x65EA5C` hold, which no
code in the band reads (the ground draws add or subtract the slide
directly). Ours aborts on an index past a table's room (section 6).

## 4. The fuzz (`effect_6a_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_6a`, effect mode (`g.effect`; kind 0x18
for every clone), 4,000 rounds a function (`BOF3X_E6A_ONLY=<name>` runs the
clones whose name holds it). Shape: all 48 `kEffect` (the seven
dispatchers' `sub_span` their table's length, 5); none takes an argument,
none answers (no `ret_mask`). The seven tables are `DataTable`s, swapped for
recorders on both sides. **Regions**: none beyond effect mode's standard
ones - the records, `Sprite_Current`, `ObjTrio` (the leader's `+0x34`,
`+0x38`), `Prim_VertexScratch`, `Field_Request`, `Cond_ByteFE` (in the
camera cells' region), the packet buffer. The cell, height and sign tables
are the image's, read-only, left in place.

**Callees**: the standard and effect-standard rows for `AreaMap_Elevation`,
`AreaMap_SetByte` (s16, s16, byte - `0x50C0D0` pushes `eax` / `edx` with
leftovers above `ax` / `dx`), `Sound_PlayEffect`, `MapView_LinkPrimAt` (its dy
a signed byte; the callers push the immediate -2), `Gte_RotTransPers4`,
`Gte_PrimDepths4_10`, `Prim_SetTexture`, `Gfx_CommitPrim`, the `Gpu_*`. None
re-listed: no caller here needs another reading. **The draws the states
call**, listed by name as `kPhase` (the recorder logs `Sprite_Current`):
`EffectKind18Sub2D_Draw`, `_2E_`, `_2F_`, `_30_`, `_31_`, and E5G's
`EffectKind18Sub2C_Draw` (0x4A's tail jump). Each draw is also fuzzed
itself as a `kEffect` clone.

**Seeds** (per function, after the harness's per-round fill): on all 20
records the slide `+0x30` one step from each end of the function's
direction, at it, one either side and past it (0, 0x1F..0x21, 0xDF..0xE1,
0x100, 0x120, 0xFFE0, any; or 0, 0xFFDF..0xFFE1, 0xFF1F..0xFF21, 0xFF00,
0xFEE0, 0x20, any), `+8` 0 half the time, `+0xA` inside its room (0..3);
the current record's cell words small (0..0x7F) two times in three; for a
place state the variant inside its room and the z cell 0 (or 0..3, for 0x2D's
two bits) half the time, the leader tested against the variant's cell (read
from the image at run time); the leader about the front seven times in
eight - a whole cell -3..+4 from `cell + d` or `cell` on either axis and a
fraction at the tests' boundaries (0, 1, 0x7FFF, 0x8000, 0x8001, 0xFFFF,
random); `Field_Request` 0, 2 or any; for `0x50C0D0` `+0xB` one bit half the
time and `Cond_ByteFE` 0, that bit, every other bit or any.
**Disturbance** (the group's, from the hash only): the slide at or about
0x100 / any, `+8`, `+0xA` inside its room, the leader's x or z, a cell word,
`Field_Request`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_6a`,
exit 0): 192,000 rounds over 48 functions, 386,211 calls to the stand-ins,
**0 mismatches**; 24,756 bytes of state in 45 regions; 397 stand-ins. Every
entry of the seven tables reached (each handler recorder 703..899 calls);
`AreaMap_Elevation` 64,000, `AreaMap_SetByte` 2,956, `Sound_PlayEffect`
7,255, `MapView_LinkPrimAt` 8,000, the draws 4,000..40,000 (the first run
found one mismatch class, section 5).

STAR_RESULTS

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 48 (8,672 bytes against the cut's
  8,983: 43 differ by padding only, none by code); each extent checked by
  hand to its `ret` or tail `jmp`. No shared tail, no case of a switch, no
  second entry among them; the tool lists no code of the band no list has.
  The band ends at `0x50E3FA` (the last draw's `ret`); `0x50E400` is
  `EffectKind18_States[0x33]`, E6B's.
- **Hidden starts**: 43, each an entry by address - a cell of
  `EffectKind18_States` or of a sub-kind's table - not a case. Their
  recorded hosts: E5G's `0x50BDC0` (the 0x4A state and 0x2D's six), and this
  band's draws `0x50C4B0`, `0x50CD90`, `0x50D680`, `0x50DC20`, whose catalog
  extents (`8DB`, `8EB`, `59B`, `59B`, and `0x50E1C0`'s `D0B`) run on over the
  states after their `ret`. None of the hosts contains another's code as a
  fall-through: `0x50C4B0` ends at `0x50C6D0`, `0x50CD90` at `0x50CFD2`,
  `0x50D680` at `0x50D8C9`, `0x50DC20` at `0x50DE6E`, `0x50E1C0` at
  `0x50E3FA`.
- **The sub-kind dispatchers**: every `EffectKind18_States` cell pointing
  into the band is one of this group's rows (`[0x2D]`..`[0x32]`, `[0x3E]`).
- **The draws as functions**: each has its own frame and `ret` and is reached
  by `call` and tail `jmp` from its states (`0x50E1C0` also from E6B's 13
  sites), so each is one function, fuzzed as a state (it takes no argument
  and reads only `Sprite_Current`).
- **The cut's columns**: the unit hints (the seven tables, `0x65EA78` for
  `0x50C0D0`) are right; `0x50E1C0`'s "table 0x65EBF4 read by 0x50EAA0" names
  E6B's table, one of whose states calls it. The "(7)", "(6)", "(1)" counts
  are rows per hint, not table lengths.
- **The first run**: `EffectKind18Sub3E_WaitFar` mismatched in 781 of 4,000
  rounds - ours had taken the far test's first bound as 0x20000 like every
  other sub-kind's; the original's is 0x8000 (`cmp eax, 0x8000` at
  `0x50C959` and `0x50C9B6`). Read again and fixed in ours; 0 since.
- **PSX twins**: none in the band.

## 6. Controls

CONTROLS

## 7. Latent defects (Capcom's, described, not fixed)

- **Unchecked indexes**: the seven dispatchers do not bound `+2` (every
  writer in the band keeps it inside its table: the states step 1..4 and
  back, the places set 3); the variant index is the spawn's x cell, s16 and
  unchecked, into tables with room for 16 (0x2D), 10 (0x3E), 4 (0x2E) and 2
  (0x2F..0x32) - a spawn whose x cell is past them reads the next table's
  bytes (for 0x2E..0x32 the unused sign pair, then a state table's code
  pointers) as a cell; 0x2D's draw indexes its four signs by `+0xA` (only its
  place writes it, 0 or 1). Ours aborts past any.
- **`EffectKind18Sub31_Draw`'s dead compare**: its texture word ORs in
  `(+8 == 0) < 0x10` (`sete cl; cmp ecx, 0x10; setl dl`), which is always 1,
  into a word whose bit 0 is already set - so the word is `+8 << 21 |
  0x150010F` whatever `+8` is. Its siblings put `(+8 == 0)` at bit 16
  (`0x50D680`) or `+8` at bits 16 and 21 (`0x50CD90`); the source perhaps
  meant `<< 0x10` (a shift typed as a compare). What the game draws is the
  same either way for `+8` set; for `+8` clear 0x30's draw sets bit 16 where
  0x31's does not. Kept as read; whether 0x31 was meant to look like 0x30 is
  the owner's question, not a defect ours can settle.
- **Sub-kind 0x3E's far test** crosses the centre at 0x8000 where every other
  sub-kind's far test has 0x20000: the leader need only step half a cell off
  the front's axis for the slide to close. As read; perhaps deliberate.
- **0x3E draws nothing**: its states slide and sound with no draw at all.
- **The INT_MIN distance**: as in E5G - a leader exactly 0x8000 cells away
  counts as near. Unreachable on a map; ours computes it the same way.
- **`+0x3A` reused**: 0x2D's place reads the spawn's z cell's low byte for two
  flags (`+8`, `+0xA`) and then overwrites `+0x3A` with the variant's cell;
  0x2E..0x32 and 0x3E test the whole word for 0. As read.

## 8. Calls across groups

**Outbound**: one, to a merged group, by name: `0x50C0D0`'s tail jump to E5G's
`EffectKind18Sub2C_Draw` (`0x50B8B0`). By name, already ours:
`AreaMap_Elevation`, `AreaMap_SetByte`, `Sound_PlayEffect`,
`MapView_LinkPrimAt`, `Gfx_CommitPrim`, the `Gpu_*`, `Gte_RotTransPers4`,
`Gte_PrimDepths4_10`, `Prim_SetTexture`. No raw-address call.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `0x50E420`, `0x50E540`, `0x50E620`, `0x50E640` (two), `0x50E710`, `0x50E770`, `0x50E890`, `0x50E970`, `0x50E990` (two), `0x50EA60`, `0x50EB40` | E6B (wave six) | `EffectKind18Sub2F_Draw` `0x50E1C0`: 13 sites (eight `call`, five tail `jmp`) - E6B's files, raw until this merges |
| `EffectKind18Sub4A_Run` `0x50BFD0` through `EffectKind18Sub4A_States[1]` | E5G (ours) | `EffectKind18Sub4A_WaitCond`, read in place |
| `EffectKind18_Run` `0x46D830` through `EffectKind18_States` `[0x2D]`..`[0x32]`, `[0x3E]` | ours (worldmap_area) | the seven dispatchers, read in place |

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 48 rows, and no first-call trace under
`analysis/calltrace` names any of the 48 (a grep of every file: the hits are
the extent lists `entries*.txt`, not traces). **Fuzz only.** No live run was
made (the brief). No spawner is known (the top); a recorded route through an
area whose effect list holds one of these sub-kinds would let the
coordinator's frame-hash A/B cover it.

## 10. The rebinding

`grep -rn -i` of the 48 addresses and the seven tables in `src/game`
(`band_rows.py --refs`): two references, both comments in E5G's
`effect_5g.cpp` (lines 261 and 612, "E6A's 0x50C0D0"), now
`EffectKind18Sub4A_WaitCond`, each on its own line; and the evidence string
of `symbols.toml`'s `EffectKind18Sub4A_States`, likewise. Nothing left raw in
our files. **For the coordinator**: E6B's files call `0x50E1C0` raw (13 sites,
above); once this merges they bind to `EffectKind18Sub2F_Draw`.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 43 lines, the read
extents of the hidden starts. The five draws' host lines (`0050C4B0 8DB`,
`0050CD90 8EB`, `0050D680 59B`, `0050DC20 59B`, `0050E1C0 D0B`) are left in
place (each spans the hidden starts after its `ret`; the code is 0x221,
0x243, 0x24A, 0x24F, 0x23B), as E5G left its hosts'.
