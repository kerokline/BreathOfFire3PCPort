# Group E3D: effect kinds 0x76..0x7D, 0x7F..0x82 and area 170's dial map

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..14),
wave three, from the round branch's tip `0e0532c`. **68 functions ours**
(`src/game/effect_3d.cpp`, shadow name `effect_3d`): the cut table's 63 rows
for E3D (`analysis/round13_cut.tsv`, the band `0x485CB0..0x487FE0`) and five
starts no list of the cut has - kind 0x82's state 2 `0x487DE0`, kind 0x77's
sub-state dispatcher `0x486280` and its three sub-states `0x4861C0`,
`0x4861E0`, `0x4862A0` (section 5). Each read to its last instruction with
capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
272,000 rounds, 0 mismatches; CONTROLS_SUMMARY. **Fuzz only**: no recorded
route enters any of the 68 (section 9).

Every row is effect code but one: **`0x486D60` (`EffectKind7D_SetMap`, a
`part5` row) is a map set-up**, not a state - it writes area 170's map cells
from the three dials and is called by `Area170_Init` as well as by kind 0x7D's
state 2. It is taken (a catalogued row, the addendum's rule), fuzzed in effect
mode like the rest (it reads no record), and named for kind 0x7D, whose dials
it applies.

| Kind | Functions | Reached through |
|---|--:|---|
| 0x76, 0x79, 0x7A: a full-screen quad of colour 0, semi-transparent (abr 2, 2, 0); 0x79 ends when the counter `0x903848` is 1 | 3 | `Effect_KindHandlers[0x76]` / `[0x79]` / `[0x7A]` (`0x655528`, `0x655534`, `0x655538`): a draw each, no state table |
| 0x7B, 0x7C: the same quad with a red that alternates between two shades every 17 frames; 0x7C ends at counter 4 | 6 | `[0x7B]` / `[0x7C]`, `EffectKind7B_States` `0x654AFC` (2), `EffectKind7C_States` `0x654B04` (2) |
| 0x77: a count in a menu box, to 200, one every 30 or 40 frames; at 200 the chapter's run 5 step 0x19 | 8 | `[0x77]`, `EffectKind77_States` `0x654B0C` (3); states 0 and 1 by `+2` through `EffectKind77_CountSteps` `0x654B18` (3) and `EffectKind77_ResumeSteps` `0x654B24` (2) |
| 0x78: a spinning ring of 32 red-and-white triangles about `Sprite_Objects` record 2, which it turns (area 130's `Area130_Effect78AtObject` spawns it) | 6 | `[0x78]`, `EffectKind78_States` `0x654B2C` (4) |
| 0x7D: area 170's three dials - a panel of three 7 x 9 grids the pad turns; confirm applies them to the map (`Area170_Tail37`'s case 40 spawns it) | 12 | `[0x7D]`, `EffectKind7D_States` `0x654B3C` (4) |
| 0x7F: a widening cone of 32 shaded quads about the record's point | 4 | `[0x7F]`, `EffectKind7F_States` `0x654C0C` (3) |
| 0x80: a trail of 32 points that moves along x, holds and narrows | 7 | `[0x80]`, `EffectKind80_States` `0x654C18` (9) |
| 0x81: drops from sixteen sources about the leader, poured until the counter is 12, then drained | 10 | `[0x81]`, `EffectKind81_States` `0x654C5C` (3) |
| 0x82: `Sprite_Objects` record 0 pushed a cell at a time (3, 4 or 5 pushes by `0x903849`) while it is short of the leader | 12 | `[0x82]`, `EffectKind82_States` `0x654C88` (24; states 11..23 E4A's) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the thirteen dispatchers `evidence`). "Ring", "cone", "trail",
"drops", "dials", "pushed" name the code's shape - the primitives it commits
and the cells it steps - not a play-tested fact: what the game shows with
these kinds and where was not traced (section 9; the owner's word, as the
brief says). The spawners named are the ones `src/game` shows; kinds 0x76 and
0x77 are also spawned by scenario code (SC13 and SC11's docs name them).

## 1. What each function does

Ours is `src/game/effect_3d.cpp`; every function carries its original's
address and a one-paragraph reading. In outline:

### 1.1 The full-screen quad (kinds 0x76, 0x79, 0x7A, 0x7B, 0x7C)

A draw mode (`Gpu_GetTPage(2, abr, 0x140, 0x140)`, dtd 1) committed (6, 0xC),
then a `POLY_G4` over (0, 0)..(320.0, 320.0) - floats written as bit patterns,
the depth words left as the packet had them - semi-transparent, committed (2,
0x44). Kinds 0x76 and 0x79 use abr 2 and colour 0 at every vertex; 0x7A abr 0.
Kinds 0x7B / 0x7C have a start (`+9` 0, `+1` 1, the shade `0x676278` 0xF, the
index `0x676279` 0) and a pulse: `+9` counts frames; when it was 0x10 or more
the shade is the next of the two bytes at `0x654AF8` (the index alternates 0, 1)
and `+9` 0; the quad's four vertices are that red, green and blue 0, abr 2.
`0x485EE0` and `0x486040` are byte for byte the same; `0x486070` is `0x485F10`
with a tail `jmp` to `Effect_Release` when the counter is 4, as `0x485D60` is
`0x485CB0` with one at 1.

### 1.2 Kind 0x77 (`EffectKind77_Run` `0x486180`)

State 0 (`_Count`, by `+2`): `_Begin` sets the tally word `0x939A00` to
`Rand() << 15` (count 0, frames 0, the pace bit Rand's bit 0), `+2` up;
`_Tick` releases the record when the counter is 1, else at a count of 200 or
more `+2` up, else the frames (the high byte's bits 0..6) up one, and at 30
(pace 0) or 40 (pace 1) they are 0 and the count up, `Sound_PlayEffect(0x20A)`
when the chapter row's flag 0x14 is set; `_End` waits while a message is open
(`Field_Request` 2), the game mode is 4 or the row's flag 0x2E is set, then
sets `Field_ScriptFlags` bit 8 (`0x9039A3` bit 0), the chapter's step 0x19 and
run 5 (`MoveScript_Var7`) and releases. State 1 (`_Resume`) is `_Tick` and
`_End` without the reset. State 2 (`_Show`) draws the box (`0x586160(0x8A,
0x1E, 0x2D, 0x14, 1)`) and the count (`Crt_sprintf` by the format `0x653074`,
`Text_DrawFont12` at (0x8F, 0x21)) and releases unless a message is open or
the mode is 4.

### 1.3 Kind 0x78 (`EffectKind78_Run` `0x486360`)

`_Start` keeps record 2's turn word (`Sprite_Objects` + 2 x 0xA4 + 0x32) in
`+0x32`, copies its screen words `+0x2E` / `+0x30`, `+9` 0x14, the radii `+0xC`
/ `+0x10` 0. `_Grow` turns record 2 by 0xF00, widens the radii by 0xF and 0xA
and draws the ring for 20 frames, then `_Spin` (60 frames) turns and draws,
and `_End` puts the turn back and releases. `EffectKind78_DrawRing(cx, cy,
rx, ry)` (each read as its low word) commits 32 flat triangles (`0x5A7570`):
the centre, the previous rim point and the next at angles 0x80 k, each rim
term `cos * r >> 12` sign-extended from its low word; red or white by
`Frame_Counter` bit 2 against k's parity; each linked at record 2's x, z
(`MapView_LinkPrimAt`, dy 0xC, 0x2C).

### 1.4 Kind 0x7D (`EffectKind7D_Run` `0x486640`) and the dial map

`_Start`: `+1` up, `+0x2E` 5, `+0x4C` the address of dial `+6` (`0x675DC8` +
`+6`). `_Input` turns the dial `+0x4C` points at by `Input_Pressed` bit 15
(up, 9 wraps to 0) and bit 13 (down, below 0 wraps to 8) with sound 0x204 when
it moved; a confirm button (`Field_ConfirmButtons`) plays 0x205, writes the
answer word `0x9039F6` 1 (area 170's tail timer, which its case 41 reads) and
dial `+6` the turn, `+1` up one; a cancel button plays 0x206, answer 0xFF,
`+1` up two; then the panel at (`+0x2E`, `+0x30`). `_Apply` (state 2) calls
`EffectKind7D_SetMap` and `+1` up; state 3 is `Effect_StateRelease`.

The panel (`_DrawPanel(x, y)`): three green bars, the three dials
(`_DrawDial` at x + 9, x + 0x6D, x + 0xD1, y + 10 with the grids `0x63CA48`,
`0x63CA87`, `0x63CAC6` and the turns `0x675DC8..CA`) and, while
`Frame_Counter` has bit 4, two white arrows and a white frame about dial `+6`.
A dial: a grey quad 0x5A by 0x46 (abr 2), then (abr 1) ten vertical and eight
horizontal grey lines and the 9 x 7 marks - column r shows the grid's column
(turn + r) mod 9 - each `_DrawMark`: 1 a red cross, 0xFF a blue bar. The
primitives are `_FillF4` (twelve words), `_FillF3` (ten), `_Line` (eight),
each coordinate the low word of its argument (`movsx`, `fild`), and
`_DrawMode(abr)`.

`EffectKind7D_SetMap` (`0x486D60`): for the 7 x 9 cells from column 0x16, row
0x8E, a cell is on when story flag 0x7E is clear and any grid, turned by its
dial, marks it. Each cell gets `AreaMap_SetByte(column, row, height or 0)` -
the heights at `0x654BCC` - and the low byte of the area block's dword the
cell's word names (`AreaMap_Header`: width w, depth d, base; the word
`0x8CB5AC[w row + column + 2 base]`, the dword `(d w + 1) / 2 + base + word`)
from `0x654B4C` (off or on); and the draw item `MapView_ItemAt` gives there,
if any, is retextured from that dword (`Prim_SetTexture(.., 2)`). The draw
items are DIV-0062's pool: Capcom's code names the array by the immediate at
`0x486EBD`, which `DrawPool_Grow` re-aims; ours reads `draw_pool::Items()`, and
`EffectKind7D_SetMap` is in `draw_pool.cpp`'s list of owned users (section 10).

### 1.5 Kind 0x7F (`EffectKind7F_Run` `0x486F10`)

`_Start`: `+0x2E` 0, `+0x30` 0x100, `+9` 0x28, `+1` up, sound 0x206. `_Grow`
draws the cone at the record's point between the radii `+0x2E` and `+0x30`,
widens both by 0x40 and after 40 frames goes to `Effect_StateRelease`. The cone
(`_DrawCone(point, r1, r2)`): a draw mode (`Gpu_GetTPage(0, 1, 0x380, 0x100)`,
dtd 1) and the map camera; 32 Gouraud quads between the rims (x, z + (cos a r
>> 4), height + (sin a r << 4)) at angles 0x80 k, projected
(`EffectGte_ProjectPoint`), grey 0x80 on the inner rim and black on the outer,
each after its own draw mode, both linked at (x, the previous outer point's
z).

### 1.6 Kind 0x80 (`EffectKind80_Run` `0x487270`)

The trail lives at `0x92BF80` (`EffectKind30_Shards`): 32 points of 0x20 (`+0`
x, z, height; `+0x10` the screen x, y, depth; `+0x1C` the angle and `+0x1E` the
half width, words), 31 angle words at `+0x400`, the size word `+0x43E`.
`_Start` places the record at (1, 0x25, 0x38 cells), the size 0x60, steps the
trail 32 times; `_Rise` plays 0x200 on its first frame, steps, draws and moves
x a cell a frame to 0x17; `_Hold` (32 frames), `_Fade` (16, the size 6
narrower a frame), then 0x492750 (states 4..7, a catalog row no group holds)
and `Effect_StateRelease`. `_TrailStep(pool)` moves the points down one, puts
the record's point first, projects each (`EffectGte_ProjectPoint`) and takes
its half width from `EffectGte_ProjectSize(point, (size, size))`; each pair's
screen direction is `Math_Ratan2` of the `_ftol`'d differences (0x1000 when
both are 0), a 0x1000 filled from the next real one ahead, else behind, else
0; each point's angle the mean of its neighbours' (`EffectAngle_Mean`, E2E's).
`_DrawTrail(pool)` caps point 0 (`EffectTrail_DrawCap`, E2F's), then for each
pair whose screen x or y differ (`fcomp`: a NaN counts as equal) two Gouraud
quads from the points to their rims at the angle + 0x400 and - 0x400, the
second a copy of the first (`rep movsd` from the cursor less 0x44) with its
rims moved, the shade 0x80 at the start falling 4 a pair; then caps point 31.

### 1.7 Kind 0x81 (`EffectKind81_Run` `0x487890`)

`_Start` puts the record at the leader's point, clears the drops
(`_ClearDrops`: bytes 0..2 of the 256 records of 0x18 at `0x92BF80`) and places
the sources (`_PlaceSources`: 16 records of 0x14 at `0x92D780`, each at the s8
cell pair `0x654C3C[i]`, on the ground's height, a first wait `Rand & 0x1F`).
`_Pour` steps to `_Drain` when the counter is 12, emits and moves; `_Drain`
moves and releases when no drop was in use. `_Emit`: each source on counts its
wait down; when it was 0, two drops from free records (`_FindFreeDrop`) at the
source plus `((Rand & 0xFF) - 0x100) << 8` in x and z, life 0x20 +
`Rand & 0x1F`, and a new wait `Rand & 7`; al 1 when any source was on.
`_MoveDrops`: the map camera; each drop in use counted (`0x67627C`), its speed
0x20000 more and its height moved by it, its blink bit flipped, drawn
(`_DrawDrop`: on the blink, a black one-pixel tile at the projection and a 3 x
3 tile of 8, semi-transparent, one up and left of it), its life down - at 0
freed; al 1 when any was in use.

### 1.8 Kind 0x82 (`EffectKind82_Run` `0x487C10`)

`_Start`: `+9` 0, `+1` 1, `+2` 0 (also entry 0 of the next table `0x654CE8`,
E4A's dispatcher `0x488220`'s). `_Wait` sets `Field_State` to the leader
(`ObjTrio`), `+1` 0x16 when record 0's x is the leader's or past it (signed),
0x17 when `0x90384A` is 0x80, then switches on `0x903849` through MSVC's two
tables inside the function (bytes at `0x487CD4`, jumps at `0x487CC0`): 3 sends
`+1` to 6, 4 to 4, 5 to 2, **0xFF** releases the record, anything else (6
among them) nothing. States 2, 4, 6, 8, 10 (`_Push2`..`_Push10`) point
`Sprite_Current` at record 0 for four adds of 0x4000 to its x (one cell) and
put it back, `+1` the next; states 3, 5, 7, 9 (`_Check3`..`_Check9`) set
`Field_State` to the leader and send `+1` to 0xD (`+9` 0) when record 0 has
reached the leader, else on. So 0x903849 = 5 pushes five times, 4 four, 3
three, each push checked; state 11 on is E4A's.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed. `cheats.cpp` and `widescreen.cpp` name no address of the band nor its
tables; `DIVERGENCE.md` names one - DIV-0062's site `0x486EBD`, the draw-item
array's immediate inside `EffectKind7D_SetMap`, which `DrawPool_Grow` re-aims
after every self-test. Ours reads the pool through `draw_pool::Items()`
(DIV-0062's rule for ours) and `EffectKind7D_SetMap` joins `kOwnedUsers` in
`draw_pool.cpp`, so `BOF3X_ORIGINAL=EffectKind7D_SetMap` keeps the original's
1,024 items as for the other users; the fuzz compares the original's array
(the switch runs after it). Checked 2026-10-03.

Where the original indexes past a table ours aborts with a `Fatal` naming the
function (the round-nine rule; nothing in the fuzz reaches it): the thirteen
dispatchers past their tables; the pulse's index `0x676279` past the two
shades (only these four functions write it, 0 or 1); `_Input`'s `+6` past the
three dials; a dial's turn past the grid's nine columns in `_DrawDial` and
`_SetMap` (only `_Input` writes the dials, 0..8).

## 3. The tables and the arguments pushed with leftovers

**The tables** (`symbols.toml` `[[data]]`): each the table's own length to the
next table a dispatcher indexes, or to the first dword that is not code -
checked by a raw scan of `.text` for every dispatcher's `jmp [eax*4 + T]` and
of the image for every cell address (scratch `e3dscan.py`), and against what
the states store into `+1` / `+2`:

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind7B_States` `0x654AFC` | 2 | `0x654B04`, kind 0x7C's |
| `EffectKind7C_States` `0x654B04` | 2 | `0x654B0C`, kind 0x77's |
| `EffectKind77_States` `0x654B0C` | 3 | `0x654B18`, `_Count`'s |
| `EffectKind77_CountSteps` `0x654B18` | 3 | `0x654B24`, `_Resume`'s |
| `EffectKind77_ResumeSteps` `0x654B24` | 2 | `0x654B2C`, kind 0x78's |
| `EffectKind78_States` `0x654B2C` | 4 | `0x654B3C`, kind 0x7D's |
| `EffectKind7D_States` `0x654B3C` | 4 | `0x654B4C`, `_SetMap`'s bytes (entry 3 `Effect_StateRelease`) |
| `EffectKind7F_States` `0x654C0C` | 3 | `0x654C18`, kind 0x80's (entry 2 `Effect_StateRelease`) |
| `EffectKind80_States` `0x654C18` | 9 | `0x654C3C`, the sources' cells (4..7 `0x492750`, 8 `Effect_StateRelease`) |
| `EffectKind81_States` `0x654C5C` | 3 | `0x654C68`, bytes a part-6 row reads |
| `EffectKind82_States` `0x654C88` | 24 | `0x654CE8`, E4A's `0x488220` indexes it (the tool's run says 53) |

**Arguments with leftovers** (the fuzz lists each callee with the mask of what
it reads; ours passes the values computed whole):

| Callee | Pushed | Read |
|---|---|---|
| `EffectKind78_DrawRing` | the centre words over the radii dwords' upper halves (`_Grow`) or the dispatcher's `ecx` (`_Spin`) | `movsx` of each low word |
| `EffectKind7D_DrawPanel` | `+0x2E` over `Sprite_Current`'s upper half, `+0x30` over a leftover | passed on in `lea`s, every callee reads low words |
| `EffectKind7D_DrawDial`, `_DrawMark`, `_Line`, `_FillF3` | coordinates `x + 10 b` from `movzx ax` over leftovers, `S[6] * 100` over `Sprite_Current`'s upper half | low words; colours, abe, turn, mark as bytes |
| `EffectKind7F_DrawCone` | `+0x2E` / `+0x30` over leftovers | low words |
| `EffectTrail_DrawCap` | angle and size `mov cx` / `mov dx`, the last shade a stack dword whose upper bytes were never written | words, the shade a byte (E2F's masks) |
| `EffectAngle_Mean` | `mov ax` / `mov dx` over leftovers | 12 bits each (E2E's) |

## 4. The fuzz (`effect_3d_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_3d`, effect mode (`g.effect`; kinds
0x76..0x7D, 0x7F..0x82, each clone its own), 4,000 rounds a function
(`BOF3X_E3D_ONLY=<name>` runs the clones whose name holds it,
`BOF3X_E3D_ROUNDS` sets the rounds). Shapes: 56 `kEffect` (the thirteen
dispatchers' `state_span` - or `sub_span` for kind 0x77's two by `+2` - their
table's length, section 3; the no-argument helpers too, which read no
argument), twelve `kCall` (the draws with arguments and the two trail
helpers). The eleven tables are `DataTable`s, swapped for recorders on both
sides (E4A's states, `0x492750`, `BareRet` and `Effect_StateRelease` among
the entries). **Regions** beyond effect mode's standard ones: `0x676278` (8:
the pulse's shade and index, the drops' moving count `0x67627C`), the dials
`0x675DC8` (4), the answer word `0x9039F4` (4, EKH's "a group lists it"), and
`0x92C5C4..0x92D8C0` (the drops past `EffectKind30_Shards`' standard 0x644 and
the sixteen sources after them). Everything else is standard: the records,
`Sprite_Current`, `Frame_Counter`, `Sprite_Objects` (records 0 and 2),
`ObjTrio`, `Field_State`, the counters `0x903848..0x90384A`, the chapter
bytes, the flag rows and `0x929ED0`, `Field_ScriptFlags`, `Field_Request`,
`Game_Mode`, the tally `0x939A00` (the panel cells), the input and button
words, the area block, the text scratch, the packet buffer, the trail inside
`EffectKind30_Shards`.

**Callees**: the effect-standard rows for `Effect_Release` (clears
`+0..+4`), `Flags_Test` (0 or 1), `Sound_PlayEffect`, `Rand`,
`AreaMap_Elevation`, `AreaMap_SetByte`, `MapView_ItemAt` (0 or a small item),
`Prim_SetTexture`, `Math_Sin` / `Cos` (never 0 or -1), `Math_Ratan2`,
`MapView_LinkPrimAt` (the cursor moved two times in three),
`EffectGte_LoadMapCamera`, `Crt_sprintf`, `Text_DrawFont12`, the `Gpu_*`,
`Gfx_CommitPrim` (moves the cursor), `0x586160`, `0x5A7570` (0x2C bytes of the
primitive filled), `_ftol` called for real. **Listed in the group**: its own
callees by name - the four no-argument ones (`_DrawCount`, `_SetMap`,
`_ClearDrops`, `_PlaceSources`) as `kPhase`, `_MoveDrops` and `_Emit` as
`kFlag` (al), the draws with section 3's masks (`_DrawCone` hashing the 12
bytes of its point, `_DrawDrop` the drop's 0x18), and `_FindFreeDrop` answering
as the real one (the first free record or null, null also a quarter of the
time); `EffectAngle_Mean` and `EffectTrail_DrawCap` (E2E's and E2F's, not in
the standard set) with their groups' masks. **Re-listed**:
`EffectGte_ProjectSize` hashing **both** words of the size (the trail writes
both; the standard row hashes the first only - control 93 is refused by it)
and filling out as the standard row does; `EffectGte_ProjectPoint` filling
three fractional floats as the standard row does, but, where out is a trail
point's projection, a third of the time the point before's exactly, so the
trail's "no direction" (0x1000) and its fill run (the standard fill almost
never repeats a value).

**Seeds** (per function, after the harness's per-round fill): every record's
`+6` below 3 and `+0x4C` one of the dials' addresses (kind 0x7D's `_Input`
reads through it and writes dial `+6`); the dials 0..8; the pulse's index 0
or 1; the counter at 1, 4, 0xC, 0, 2 or random; `+9` at 0, 1, 2, 0xF..0x11,
0xFF; the tally with the count at 0xC6..0xC8, 0, 0xFF and the frames at
0x1C..0x1E, 0x26..0x28, 0x7F, the pace bit half the time; `Field_Request` at
2, 0, 1, 5 and `Game_Mode` at 4, 2, 0x104 for `_End` / `_Show`; the pressed
word at bits 15, 13, both, none and the buttons at a few masks for `_Input`;
the area header for `_SetMap` (width below 5, depth below 9, base below 0x40,
and the 63 cell words it reads below 0x300, so its dword stays inside the 8 KiB
compared); `+0x34` about the rise's end; a third of the trail's pairs on one
screen point (one or both axes) for `_DrawTrail`; the drops' in-use, life and
blink bytes (all free a quarter of the time for `_MoveDrops`); the sources'
on and wait bytes; record 0's x at, one past, one short of, a cell either side
of the leader's, `0x903849` at 3, 4, 5, 0xFF, 6, 2, 7, 0xFE and `0x90384A` at
0x80 for kind 0x82. **Arguments**: a dial's grid one of the three and its turn
below 9 over random upper bytes; the mark byte at 1, 0xFF, 0, 2, 0xFE; the
cone's point a record's `+0x34`; the trail's pool `0x92BF80`; a drop record of
the 256. **Disturbance** (the group's, from the hash only): `+9`, `+0x2E` /
`+0x30`, the counter, the tally, the pressed word, a trail point's angle,
a drop's life, record 0's x, the record's point.

RESULT_PARAGRAPH

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 63 to the byte (8,156 bytes against
  the cut's 8,660: 40 differ by padding only), with one difference by code:
  `0x487C50`'s cut size 464 runs over `0x487DE0`, which is a state of its own
  (`EffectKind82_States[2]`, "code no list has"). `0x487C50`'s 0x181 bytes
  hold MSVC's switch tables (`0x487CC0`, five jumps; `0x487CD4`, 0xFD index
  bytes) after its last `ret`; the clone moves the jump table
  (`JumpTable{0x58, 0x70, 5}`) and reads the byte table in place.
- **Hidden starts**: 44 of the cut's and the five added, each an entry by
  address - a cell of `Effect_KindHandlers` or of a kind's table - none a case
  or a shared tail. Their recorded hosts: E3C's `0x485960` (its catalog extent
  `9AE` spans `0x485960..0x48630E`: thirteen of ours), and ours `0x486310`,
  `0x4864C0`, `0x486D60`, `0x486FC0`, `0x4875C0`, `0x487BF0`, whose
  `entries_logic.txt` lines ran past their `ret` over them (section 11). No
  host contains our code as a fall-through.
- **Tail jumps**: `_Pour` ends in `jmp EffectKind81_MoveDrops` (a function
  with its own frame and `ret`, called by `_Drain` too): taken as a function,
  the tail a call in ours. The `jmp Effect_Release` endings are ordinary tail
  calls.
- **The `hypothesis` rows** (`0x4861A0`, `0x486640`, `0x486CE0`, `0x486F10`,
  `0x487270`, `0x487C10`): five dispatchers (`EffectKind77_States[0]`'s by
  `+2`, `Effect_KindHandlers[0x7D]`, `[0x7F]`, `[0x80]`, `[0x82]`) and kind
  0x7D's mark draw - effect code, taken.
- **Added: kind 0x77's `0x486280`, `0x4861C0`, `0x4861E0`, `0x4862A0`** -
  catalog rows of parts 6 and 7 in no group of the round ("Unlabelled",
  "Scenario event banks"), inside the band and reached only through kind
  0x77's tables: `0x486280` is `EffectKind77_States[1]` (a dispatcher by `+2`,
  the addendum's rule), the three its and `_Count`'s sub-states. The kind is
  taken whole.
- **Added: `0x487DE0`** (above).
- **The cut's `unit` / `label` columns**: "Area overlays, world 2 / 4" and the
  units "Fn_486F10 kind 0x7F" for kind 0x80's rows, "Fn_487890 kind 0x81" for
  `0x487BF0`, are the catalog's guesses; the code says the kinds above.
  `0x486D60`'s "X:Capcom's raw, ours Area170_Init" is its caller.
- **Not taken, in no group**: `0x492750` (`EffectKind80_States[4..7]`, catalog
  part 6), `0x492530` and `0x492CF0` (they call `_ClearDrops` and
  `_FindFreeDrop`; catalog part 6 / 7) - outside the band, the coordinator's to
  place.

## 6. Controls

CONTROLS_SECTION

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x82's switch: 6 does nothing, 0xFF ends it.** `_Wait` maps
  `0x903849` 3, 4, 5 to three, four, five pushes and only 0xFF to the release;
  the byte table holds one more index (3) at 0xFF, none at 6. A script that
  writes 6 to end the push leaves the record waiting. Whether 6 is ever written
  was not traced (the writers of `0x903849` are the event scripts').
- **Kind 0x82's `_Wait` sets `+1` twice**: when record 0 has reached the
  leader it writes 0x16 (0x17 when `0x90384A` is 0x80), then the switch
  overwrites `+1` for 3, 4, 5 and releases for 0xFF - the "reached" exit is
  lost whenever a push count is pending. `Field_State` is left on the leader by
  `_Wait` and every check.
- **Kind 0x7D's `+6` is unchecked.** Area 170's case 40 stores the message
  word's low byte less 0xB; `_Input`'s confirm writes the turn to `0x675DC8 +
  +6`, so a message byte below 0xB or above 0xD writes past the three dials.
  `_Start` takes the address `0x675DC8 + +6` without a check either. Ours
  aborts on the write.
- **The pools overlap.** Kind 0x80's trail (0x440 bytes) and kind 0x81's 256
  drops (0x1800 bytes from `0x92BF80`, past `EffectKind30_Shards`' 0x540 into
  the single cells round thirteen's other groups name at `0x92C208..`, E2E's
  among them) and its sources at `0x92D780` share `EffectKind30_Shards` with
  kind 0x30's shards and the sparks E2A, E2B, E2E, E2F describe. Two of these
  kinds live at once would trample each other; nothing in the band checks.
- **`EffectKind7D_SetMap` indexes the area block by map data**: the dword it
  rewrites is `AreaMap_Header`'s arithmetic over a cell word the map file
  supplies, unchecked - right for area 170's map, anywhere for another.
- **Kind 0x76 draws nothing visible on the PlayStation's rule**: colour 0 under
  abr 2 (subtract) leaves the frame as it was. What the port's renderer makes of
  it was not looked at.
- **Kind 0x78 restores record 2's turn only in `_End`**: a record released
  early (by `Effect_Release` from outside) leaves record 2 turned.
- **The thirteen dispatchers do not bound their state bytes.** Every writer of
  `+1` / `+2` in the band steps inside its table (kind 0x82's 0x16 / 0x17 are
  inside its 24); ours aborts past any of them.

## 8. Calls across groups

**Outbound, raw**: none to a group of this round (`band_rows.py --edges`: the
fourteen edges are to EGT, E2E and E2F, merged, called by name). By address,
Capcom's and in no group: `0x5A7570` (the flat triangle's set-up, 2 sites) and
`0x586160` (the menu box, 1) - effect-standard rows of the harness. By name,
already ours: `EffectGte_*` (EGT), `EffectAngle_Mean` (E2E),
`EffectTrail_DrawCap` (E2F), `Effect_Release`, `Flags_Test`,
`Sound_PlayEffect`, `AreaMap_*`, `MapView_*`, `Prim_SetTexture`,
`Text_DrawFont12`, the `Gpu_*` / `Math_*` primitives, and through the tables
`Effect_StateRelease` (FC1) and `BareRet`.

**Inbound from outside the group** (for the rebinding pass):
- `Area170_Init` `0x426C30` (ours, `area_w4b`) calls `EffectKind7D_SetMap`
  through `area_w4b_callees.h`'s `kArea170MapSetUp` - rebound (section 10).
- `0x492530` calls `EffectKind81_ClearDrops` and `0x492CF0` calls
  `EffectKind81_FindFreeDrop` (both catalog rows of no group; a later taker
  calls them by name).
- `Effect_RunObjects` reaches the twelve kinds through `Effect_KindHandlers`
  (read in place). E4A's dispatcher `0x488220` jumps through `0x654CE8`, whose
  entry 0 is `EffectKind82_Start`.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for every row of the band, and no first-call trace names
any of the 68 (`analysis/calltrace/reach_dragon`, `reach_whelp` and today's
`reach_balioAndSunder_1_1003`, `_2_1003`, `reach_bossAndFlash_1003`,
`reach_dragonGene_1003`: every file scanned for the 68 addresses; the hidden
starts were not armed in those runs, the non-hidden ones were). **Fuzz
only.** No live run was made (the brief). A recorded walk through area 170's
dial puzzle (kind 0x7D, its map) or area 130 (kind 0x78) would let the
coordinator's frame-hash A/B cover these kinds; which scenes show the others is
the owner's to say.

## 10. The rebinding

`grep -rn -i` of the 68 addresses and the eleven tables' in `src/game`
(`band_rows.py --refs` found seven references to three functions):

- **Rebound**: `area_w4b_callees.h`'s `kArea170MapSetUp = 0x486D60` now reads
  `bof3::addr::EffectKind7D_SetMap` (the value unchanged, so `area_w4b`'s fuzz
  keys stand); its comment names the function. `area_w4b.cpp`'s comment on
  `Area170_Init` names it. `effect_2f.h`'s comment on `EffectTrail_DrawCap`
  names `EffectKind80_DrawTrail` beside E3D's `0x4875C0`.
- **Left raw**: `area_w4b_fuzz.cpp`'s `kCalls426C30` call site and its
  `"MapSetUp_486D60"` stand-in row (fuzz keys, the round-ten form); the
  harness's `scenario_harness.h` comment on `kArgs` ("E3D's quad helper
  `0x486AB0` reads twelve") - the harness is not ours to edit; it is
  `EffectKind7D_FillF4`, for the coordinator's fold.
- **Draw pool**: `draw_pool.cpp`'s `kOwnedUsers` gains
  `bof3::addr::EffectKind7D_SetMap` (section 2).

No raw reference to an E3D address sits in a file another group of this round
is writing.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03, a commented block): 55
lines - the read extents of the 68 less the 13 already listed right, six of
them correcting host lines left in place that ran past their `ret` over the
next starts: `00486310 1A5` (the code is 0x43), `004864C0 2DE` (0x173),
`00486D60 25F` (0x1A6), `00486FC0 41C` (0x2A1), `004875C0 35F` (0x2D0),
`00487BF0 143F` (0x1A). E3C's `00485960 9AE` spans thirteen of ours too; that
line is E3C's to fix.
