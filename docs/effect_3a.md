# Group E3A: effect kinds 0x60, 0x61, 0x62, 0x64 and 0x68

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..11),
wave three, from the round branch's tip `0e0532c`. **48 functions ours**
(`src/game/effect_3a.cpp`, shadow name `effect_3a`): the cut table's 48 rows
for E3A (`analysis/round13_cut.tsv`, the band `0x4801F0..0x4823C2`), none
added, none dropped (section 9). Each read to its last instruction with
capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
192,000 rounds, 0 mismatches; 65 of 65 controls refused (section 5). One forced divergence,
ledgered: **DIV-0068** (section 2). **Fuzz only**: no recorded route enters any
of the 48 (section 10).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x60: a dispatcher, and the grey line its states 1 and 2 draw (its states are catalog part 6 rows) | 2 | `Effect_KindHandlers[0x60]` (`0x6554D0`), `EffectKind60_States` `0x65492C` (6) |
| 0x61: a party member drawn off screen, read back, scattered into flickering pixel particles that fly in or out | 6 | `Effect_KindHandlers[0x61]` (`0x6554D4`), `EffectKind61_States` `0x654958` (5) |
| 0x62: 24 orange rays shot from the point, then a widening orange ring | 9 | `Effect_KindHandlers[0x62]` (`0x6554D8`), `EffectKind62_States` `0x65496C` (3) |
| 0x64: a glow at the leader that grows, holds and rises; shards and sparks; a trail flown until the chapter's count; a fade | 17 | `Effect_KindHandlers[0x64]` (`0x6554E0`), `EffectKind64_States` `0x654978` (9) |
| 0x68: sixteen rising motes, then a wall of four quads linked into the map | 14 | `Effect_KindHandlers[0x68]` (`0x6554F0`), `EffectKind68_States` `0x65499C` (4), `EffectKind68_MoteStates` `0x6549AC` (3, called with the mote) |

The names are from what the code does: "line", "particles", "rays", "ring",
"glow", "trail", "motes", "wall" name the primitives each commits and the
cells it steps - not a play-tested fact. Where the game shows these kinds and
what they look like was not traced (section 10; the owner's word, not this
doc's). **The spawners in our source** (a grep of `src/game` for the kind
stored into `+5`): chapter 10's run 2 (`Scena10_Run2`, `scena_sc9b.cpp`,
[`scena_sc9b.md`](scena_sc9b.md)) spawns kind 0x68 at step 3 (`+1` = 0, the
point `(0x58000, 0x58000, 0x2000000)`), kind 0x61 four times through
`Spawn61(b6, b7)` (`+6` the direction, `+7` the party member: (0, 0), (0, 1),
(1, 0), (1, 1)), kind 0x62 at step 0xA (the point `(0x380000, 0x260000,
0x2800000)`), kind 0x64 at step 0x13 (`+0x34` / `+0x64` the leader's point,
the height the ground's + 0x80); chapter 8's run 11 (`Scena08_Scene11`,
`scena_sc7.cpp`) spawns kind 0x60 with `+1` = 0. The cut's `unit` column calls
the part-5 rows "Fn_480590 kind 0x61": they are kinds 0x62 and 0x68's helpers
(section 9).

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes) `Sprite_Current` and calls `Effect_KindHandlers[+5]`; each
dispatcher below is `mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx +
1]; jmp [eax * 4 + T]` with no compare. Every draw-mode packet is
`Gpu_GetTPage(0, abr, x, y)` then `Gpu_SetDrawMode(packet, 0, dtd, page, 0)`
(the fifth word the leftover of the five pushes) committed 0xC.

### 1.1 Kind 0x60 (`EffectKind60_Run` `0x4801F0`, hidden in `0x47FBE0`)

| Function | What |
|---|---|
| `EffectKind60_Run` `0x4801F0` | the dispatcher; the table's six are catalog part 6 rows `0x480210` (the point set, `+1` up), `0x480270`, `0x4802C0` (the point lowered, the line drawn, `+1` up at `0x98000`), then `0x492750` twice (`+1` up) and `Effect_StateRelease` |
| `EffectKind60_DrawLine(from, to)` `0x4804C0` | a draw mode (abr 1, `0x3C0, 0x100`, slot 1); a semi-transparent `LINE_G2` from `from` projected (grey 0x40) to `to` projected (black), committed 0x24 to slot 1 |

### 1.2 Kind 0x61 (`EffectKind61_Run` `0x480590`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind61_Capture` `0x4805B0` | `Effect_FindFree` (none: `+1` up and nothing more); the frame byte `0x676266` = ObjTrio record `+7`'s `+0x148` not 0; `Gfx_ClearRect(0x340, 0x100, 0x80, 0x100)`; the member's first 0x80 bytes into the free record - in use, state 2, kind 0x3A, its point the member's, its screen position `+0x2E` / `+0x30` the frame's x / y (`EffectKind61_Frames`), `+0x25` `Gpu_GetTPage(0, 0, 0x340, 0x100)`, `+0x24` \| 0x88, `+0x26` 0x80; with `Sprite_Current` that record `Sprite_SetAnimation(+8)`, `Sprite_UpdateScreen`, `Effect_Release` (the borrowed record freed at once); `Sprite_Current` back, `+9` = 2, `+1` up |
| 1 | `EffectKind61_Store` `0x480730` | `+9` down; at 0 the rectangle `(0x340, 0x100, w, h)` of the frame read back to `0x92BF80` (`0x59E930`), `+1` up |
| 2 | `EffectKind61_Scatter` `0x4807A0` | the member's point projected (o); the count `0x676264` = 0; each read-back pixel of the frame's h rows and w columns (the row and column bytes) that is not 0 a particle of 0x14 at `0x92DF80 + 0x14 n`: its colour `+2`, its screen point `(o.x - x + column, o.y - y + row)` floats at `+4` / `+8`; the count n; `+1` up |
| 3 | `EffectKind61_Arm` `0x480910` | a 16-byte pattern (i & 1); each particle: every sixteenth the pattern shuffled by 16 swaps of two `Rand & 0xF` places; its flag `+0` the pattern's next byte, its speed `+0x10` / `+0x11` (`Rand & 0x1F`) - 0x10; with `+6` = 0 its count `+1` = (`Rand & 0x1E`) + 0xA and its point moved back by speed * count (in), else its count 0x28 - (`Rand & 0x1E`) (out); `+9` = 0x50, `+1` up, `Sound_PlayEffect(0x202)` |
| 4 | `EffectKind61_Twinkle` `0x480A50` | each flagged particle: `+6` = 0 - moved by its speed while its count lasts; `+6` set - its count run out first, then moved each frame; drawn as a `TILE_1` (its `+4..+0xF` the point, the colour the 15-bit pixel's channels `<< 3`, `>> 2`, `>> 7`, each `& 0xF8`), committed 0x14 to slot 2; every particle's flag toggled; `+9` down, at 0 `Effect_Release` |

### 1.3 Kind 0x62 (`EffectKind62_Run` `0x480B70`)

| State / helper | Function | What |
|---|---|---|
| 0 | `EffectKind62_Start` `0x480B90` | the rays cleared; `+9` = 0xFF, the wait `+0xA` = 0, the gap `+6` = 0x10; `+1` up; `Sound_PlayEffect(0x204)` |
| 1 | `EffectKind62_Rays` `0x480BD0` | the wait at the gap: a ray, the gap down 2 unless 0, the wait 0; else the wait up; the rays stepped; with `Field_Request` not 2, the message word `0x7DEE48` at 0x12 and the chapter's step `0x8034E5` at 0xC: the radii `+0xC` = 0xA000 and `+0x10` = 0xB000, their speeds 0, `+9` = 0, `+1` up |
| 2 | `EffectKind62_Blast` `0x480C70` | `Sound_PlayEffect(0x205)` at `+9` = 0x14, `+9` up to 0x15; the ring drawn; the speeds `+0x18` up 0x80, `+0x1C` up 0x8C, the radii down by them, held at 0; the rays stepped - none left and both radii 0: `Effect_Release` (a tail `jmp`) |
| | `EffectKind62_ClearRays` `0x4812B0` | the 24 rays of 0x38 at `0x92BF80` out of use |
| | `EffectKind62_SpawnRay` `0x4812D0` | the first free ray: in use, phase 0, count 8; an angle `Rand & 0xFFF`, a length ((`Rand & 0xFFF`) + 0x800) << 5; both shades the length / 0x300 (the compiler's signed magic divide of length << 8); both ends the record's point + (cos, sin) * length sar 12; its speed -(cos, sin) << 1 |
| | `EffectKind62_StepRays` `0x4813B0` | `EffectGte_LoadMapCamera`; a draw mode (abr 1, `0x2C0, 0x100`, slot 2); each ray drawn, then phase 0: its first end moved by the speed, shade `+3` down 10; phase 1: its second end, shade `+4`; the count down (phase 0 goes to 1, phase 1 ends the ray). Answers 1 in al when a ray was in use |
| | `EffectKind62_DrawRay(ray)` `0x4814B0` | a semi-transparent `LINE_G2` between the two ends projected, shaded (`+3`, `+3 >> 1`, 0) and (`+4`, `+4 >> 1`, 0), committed 0x24 to slot 2 |
| | `EffectKind62_DrawRing(point, inner, outer)` `0x481550` | a draw mode (abr 1, slot 2); 32 semi-transparent `POLY_G4` between the radii inner << 4 (orange) and outer << 4 (black) round point, (cos, sin) * r sar 8 at 0x80 steps; each committed 0x44; answers 0 in al |

### 1.4 Kind 0x64 (`EffectKind64_Run` `0x480D40`)

| State / helper | Function | What |
|---|---|---|
| 0 | `EffectKind64_Start` `0x480D60` | the glow's point `+0x64..+0x6F` the leader's (`ObjTrio +0x34`), 0x80 higher; the size `+0x2E` = 0; `+9` = 8; `+1` up |
| 1 | `EffectKind64_Grow` `0x480DB0` | the size up 0x20; the glow drawn (colour 6); `+9` down, at 0 `+9` = 0x80, `+1` up, `Sound_PlayEffect(0x209)` |
| 2 | `EffectKind64_Hold` `0x480E10` | the glow drawn; `+9` down, at 0 `+9` = 0x20, `+1` up |
| 3 | `EffectKind64_Rise` `0x480E50` | the glow up 0xC and its size down 6, drawn; at the count's end `+9` = 0x20, `+6` = 0, `+1` up, the sparks cleared, the record's point the glow's, the 16 shards started, `Sound_PlayEffect(0x207)` |
| 4 | `EffectKind64_Burst` `0x480F10` | the frame `+6` up, a spark (E4F's `0x493B50`) every fourth; the shards and the sparks drawn; at the count's end the trail's head `+0x18..` the glow's point, its speeds `+0xC` = -0x1000, `+0x10` = -0x400, `+1` up |
| 5 | `EffectKind64_Launch` `0x480F90` | the same spark and draws; `+9` = 0x20, `+1` up |
| 6 | `EffectKind64_Fly` `0x480FD0` | the spark; sparks, shards; the trail; the speeds down 0x800 / 0x200, the head moved by them, the shade `+3` down 4; at the count's end `+9` = 0x80, `+1` up |
| 7 | `EffectKind64_FlyWait` `0x481090` | the spark, sparks, shards, the trail; when the chapter's count `0x903848` is 0x18: the shade 0x80, `+9` = 8, `+1` up, `Sound_PlayEffect(0x208)` |
| 8 | `EffectKind64_Fade` `0x481100` | the size down 0x10, the shade down 8; the sparks; the trail in the shade; at the count's end `Effect_Release` (a tail `jmp`) |
| | `EffectKind64_DrawGlow(point, size, colour)` `0x481740` | a draw mode (abr 1, `0x2C0, 0x100`, dtd 1, slot 2); `EffectGte_LoadMapCamera`; point projected, `{size, size}` projected (`EffectGte_ProjectSize`), r its first word + (`Frame_Counter & 1`); a fan of 32 semi-transparent `POLY_G3`, the centre in ((c & 0xFC) << 5, (c & 0xFE) << 6, c << 7) as bytes, the rim black at (cos, sin) * r sar 12; each committed 0x34. The rim's depth: section 2 |
| | `EffectKind64_DrawTrail(from, to, shade)` `0x481910` | two semi-transparent `POLY_G4` along from - to projected, offset by the float at `0x5C41B8` (one side black, one (shade, shade, 0)), committed 0x44; up to eight `TILE_1` dots back from `to` less (`Frame_Counter & 0xF`) << 12 / << 10, stepping 0x10000 / 0x4000, while x is not below from's |
| | `EffectKind64_ClearSparks` `0x482050` | the eight sparks of 0x18 at `0x92BF80` out of use |
| | `EffectKind64_StepSparks` `0x482070` | `EffectGte_LoadMapCamera`; each spark in use: size `+0x14` down 0x10, life `+2` down (0: out of use), drawn. Answers 1 in al when one was in use |
| | `EffectKind64_DrawSpark(spark)` `0x4820C0` | a draw mode (abr 1, `0x3C0, 0`, dtd 1, slot 2); a disc of 32 `POLY_G3` round `+4` projected, r = `{+0x14, ?}` projected + (`Frame_Counter & 1`), centre (`+0x16`, `+0x16`, 0), rim (`+0x17`, `+0x17`, 0); committed 0x34 |
| | `EffectKind64_InitShard(shard)` `0x482240` | the shard's point the record's; three `Rand` angles; its two edge vectors (cos, sin of +-0x10) as words turned by `EffectGte_SetDiagonalOne`, `Gte_RotMatrixX` / `Y` (negated) / `Z`, `0x5A7C70` in place; shade `+0x2A` = 0x20, speed `+0x28` = (`Rand & 2`) + 3, angles `+0x20..+0x25` = 0 |
| | `EffectKind64_DrawShards` `0x482360` | a draw mode (abr 1, `0x380, 0x100`, dtd 1, slot 2); `EffectGte_LoadMapCamera`; the 16 shards of 0x2C at `0x92C040` each drawn (E4F's `0x493C60`) and its angle `+0x24` up 0x10 - no in-use test |

### 1.5 Kind 0x68 (`EffectKind68_Run` `0x481150`)

| State / helper | Function | What |
|---|---|---|
| 0 | `EffectKind68_Start` `0x481170` | the motes cleared; the count `+0x2E` = 0x40, the frame `+0x30` = 0; `+1` up; `Sound_PlayEffect(0x200)` |
| 1 | `EffectKind68_Rise` `0x4811A0` | the frame up; on every fourth count a free mote started; the motes stepped; the count down, at 0 `+9` = 0, `+1` up |
| 2 | `EffectKind68_Wall` `0x4811F0` | `Sound_PlayEffect(0x201)` at the frame 0x5A; the frame up; past 0x5A (a signed word) the wall drawn, shade frame * 4 - 0x168 (0xFF from 0x100, a word compare); no mote left: the count 0x5A, `+1` up |
| 3 | `EffectKind68_End` `0x481260` | the frame up, the wall past 0x5A; the count down, at 0 `Effect_Release` (a tail `jmp`) |
| | `EffectKind68_ClearMotes` `0x481B80` | the 16 motes of 0x1C at `0x92BF80` out of use; the flags `0x676261`, `0x676260` cleared |
| | `EffectKind68_FindMote` `0x481BB0` | the first free mote, or null |
| | `EffectKind68_InitMote(mote)` `0x481BD0` | in use, state 0, count 8, shade 0, speed 0, reload 0x20; the record's point, x moved ((`Rand & 0xFF`) - 0x80) << 10, the height up (`Rand % 0x300`) << 16 (a signed `idiv`) |
| | `EffectKind68_StepMotes` `0x481C40` | a draw mode (abr 1, `0x3C0, 0`, dtd 1, slot 2); `EffectGte_LoadMapCamera`; each mote in use called through `EffectKind68_MoteStates` by its `+1` (handed the mote, unbounded) and drawn. Answers 1 in al when one was in use |
| | `EffectKind68_DrawMote(mote)` `0x481CC0` | a disc of 16 `POLY_G3` round `+0xC` projected, r = `{0x18, ?}` projected + (`Frame_Counter & 1`), the centre the shade `+3` in all three channels, the rim black; committed 0x34 |
| mote 0 | `EffectKind68_MoteGlow(mote)` `0x481E00` | the shade up 0x10; the count down, at 0 the count its reload `+4`, `+1` up, `0x676261` set |
| mote 1 | `EffectKind68_MoteHold(mote)` `0x481E40` | the count down, at 0 0x10 and `+1` up |
| mote 2 | `EffectKind68_MoteRise(mote)` `0x481E60` | `0x676260` set; the speed `+8` up 0x400, the height `+0x10` up by it; the shade down 8; the count down, at 0 out of use |
| | `EffectKind68_DrawWall(shade)` `0x481EA0` | a draw mode (dtd 1, slot 2); `EffectGte_LoadMapCamera`; the record's x - 0x20000, z at heights 0x2000000 and 0x5000000 projected; four times x up 0x10000, a draw mode (dtd 0) and a semi-transparent `POLY_F4` (shade x3) from the last pair to the new, each linked into the map at (x, z) (`MapView_LinkPrimAt`, 0xC and 0x38) |

## 2. Divergence

**DIV-0068 (Forced): kind 0x64's glow, the rim vertices' depth.**
`EffectKind64_DrawGlow` computes each rim vertex's x and y into stack locals
`esp + 0x20` / `+ 0x24` of its frame and reads the depth from `esp + 0x28`
(`0x481836`, `0x48189D`), which it never writes; the renderer reads every
vertex's depth (`0x5A0E80` divides by it). A stack word never written cannot
be reproduced; ours writes the centre's depth there, as the two sibling discs
(`EffectKind64_DrawSpark`, `EffectKind68_DrawMote`) write it to all three
vertices. Every other byte is the original's. `BOF3X_ORIGINAL=
EffectKind64_DrawGlow` restores Capcom's. Everything else in the group is a
faithful replacement; the aborts are in section 6.

## 3. Tables and cells named

The dispatchers do not bound their index; each count is the table's own
length to the next table a dispatcher names, checked by a raw scan of the
image for every cell address (scratch `cellscan.py`). `band_rows.py` read one
run of 36 code pointers from `0x654958`; the dispatchers split it.

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind60_States` `0x65492C` | 6 | a zero dword at `0x654944` (it begins at cell 10 of E2G's `EffectKind5F_States` run) |
| `EffectKind61_States` `0x654958` | 5 | `0x65496C`, kind 0x62's |
| `EffectKind62_States` `0x65496C` | 3 | `0x654978`, kind 0x64's |
| `EffectKind64_States` `0x654978` | 9 | `0x65499C`, kind 0x68's |
| `EffectKind68_States` `0x65499C` | 4 | `0x6549AC`, the motes' |
| `EffectKind68_MoteStates` `0x6549AC` | 3 | `0x6549B8`, the table E3B's dispatcher `0x4823D0` indexes |
| `EffectKind61_Frames` `0x654948` | 16 bytes | two records (s16 x, y, w, h) by the frame byte |

Each state advances `+1` only to its table's next entry (or releases), so no
state reaches past its table in play. **Cells** (`effect_3a_callees.h`):
`0x676260` / `0x676261` kind 0x68's flags, `0x676264` kind 0x61's particle
count (u16), `0x676266` its frame byte; nothing else in the image reads them.
**The shared buffer at `0x92BF80`** (`EffectKind30_Shards` and on): kind
0x62's 24 rays of 0x38 (to `0x92C4C0`); kind 0x64's 8 sparks of 0x18 (to
`0x92C040`) and 16 shards of 0x2C at `0x92C040` (to `0x92C300`); kind 0x68's
16 motes of 0x1C (to `0x92C140`); kind 0x61's read-back pixels (w x h words
from `0x92BF80`) and its particles of 0x14 from `0x92DF80`. The pools overlap
between kinds (section 7).

**Arguments with leftovers**: the glow's size is pushed `mov cx, [eax +
0x2E]; push ecx` over the dispatcher's `ecx` (`Sprite_Current`'s upper half),
the callee reads the word (`mov ax, [esp + 0x64]`); the trail's shade `mov
cl, [eax + 3]` over a leftover `ecx`, read `mov bl, [esp + 0x54]`; the wall's
shade the whole `lea eax, [eax * 4 - 0x168]` of a register whose upper half is
`Sprite_Current`'s, read `mov bl, [esp + 0x64]`. The fuzz masks each so; ours
passes the value the callee reads.

## 4. The fuzz (`effect_3a_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_3a`, effect mode (`g.effect`; kinds
0x60, 0x61, 0x62, 0x64, 0x68, each clone its own), 4,000 rounds a function
(`BOF3X_E3A_ONLY=<name>` runs the clones whose name holds it). Shapes: 34
`kEffect` (the five dispatchers' `state_span` their table's length, section
3), 14 `kCall` (the helpers with arguments, handed the record's own points as
the states pass them, or a pool record). The five state tables are
`DataTable`s, swapped for recorders on both sides; `EffectKind68_MoteStates`
is left in place (its handlers take the mote as an argument, which a handler
recorder does not log), so both sides run Capcom's three there, which write
only the mote and the two flags (compared). **Regions** beyond effect mode's
standard ones: the cells `0x676260..0x676267` (8), kind 0x61's frames in
`.data` (16, seeded small: its second frame makes up to 4,096 particles, past
what the harness holds) and its first 0x100 particles from `0x92DF80`
(0x1400). Everything else the band touches is standard: the records,
`Sprite_Current`, `Frame_Counter`, `ObjTrio`, `Field_Request`, the counters
`0x903840..`, the chapter's step, the message word, the packet buffer, the
shards region.

**Callees**: the effect-standard rows for `Effect_FindFree` (a free record or
none), `Effect_Release` (clears `+0..+4`), `Sprite_UpdateScreen` (logs the
record), `Gfx_ClearRect`, `Sound_PlayEffect`, `Rand`, `Math_Sin` / `Cos`,
`EffectGte_LoadMapCamera`, `EffectGte_ProjectPoint` (the point hashed, out
fractional floats), `EffectGte_ProjectSize` (the point and the size's first
word hashed), `EffectGte_SetDiagonalOne`, `Gte_RotMatrixX` / `Y` / `Z`,
`0x5A7C70`, `MapView_LinkPrimAt` (moves the cursor two times in three), the
`Gpu_*`. **Re-listed in the group**: its own eighteen called by name (`kPhase`
for the five void ones, `kFlag` for the three answering in al,
`EffectKind68_FindMote` answering null a quarter of the time else a mote, the
nine with arguments masked as section 3 and their records hashed);
E4F's two by address (`0x493B50` no argument, `0x493C60` its shard hashed to
0x2C); `Sprite_SetAnimation` logging `Sprite_Current` (kind 0x61 points it at
the borrowed record); `0x59E930` filling the w x h pixels it reads back (E2C's
form); `Gfx_CommitPrim` moving the cursor as the standard row and, while
`EffectKind64_DrawGlow` runs, copying the last `POLY_G3`'s `+0x10` over its
`+0x20` and `+0x30` on both sides (the depth DIV-0068 replaces), with
`Gpu_SetPolyG3` re-listed to note which triangle that is (the harness's
disturbance moves the cursor between the triangle and its commit). The
standard rows the wave-two fold made louder (section 8.5 of the harness doc)
serve every caller here as they are: no caller here writes a size's second
word, takes a square root or reads a projected point's NaN.

**Seeds** (per function, after the harness's per-round fill): every record's
`+7` and every ObjTrio record's byte 7 below 3 (the disturbance may move
`Sprite_Current` onto the record kind 0x61 fills from ObjTrio); the frame
byte 0 or 1; the particle count at 0, 1, 2, 0x10, 0x11, 0x100 or below it; both
frames with x / y small and w x h at most 0x100 (0, negatives, single rows up
to 0x40), h below 0x100 (a first seed let w = 1 draw h = 0x100: the original's
row byte never reaches it, so its copy ran on writing particles until it
faulted - an access violation the crash reporter did not log - under `'*'`'s
stream and in 40,000 rounds alone, never in 4,000 alone); `+9` at 0, 1, 2, 0x13..0x15, 0xFF; `+6` at 0..4, 0x10. By function:
the members' `+0x148` 0 or 1 (capture); particles with flags 0 / 1 / other,
counts 0, 1, 2, 0x28, sane fractional floats (arm, twinkle); the wait at the
gap half the time, `Field_Request` / the message word / the step at and round
2, 0x12, 0xC, rays half in use with phases 0, 1, 2 (rays); the radii and
speeds round 0 and their steps, with negatives (blast); the size at 0, 6,
0x10, 0x20, 0xFFFF (glow states); the count at 0x18 and round it (fly-wait);
the trail's ends 0..9 dot steps apart half the time; sparks half in use, lives
at 1; the count `+0x2E` at 0, 1, 4, 5, 0x40, 0x41 and the frame `+0x30` at
0x59..0x5B, 0x99, 0x9A, 0x7FFF, 0xFFFF; motes half in use, states below 3,
counts at 1, both flags 0 / 1. **Arguments**: the glow's size word over
random upper bytes at 0, 0x20, 0x100, 0x7FFF, 0x8000, 0xFFFA, its colour 6 and
others; the shades over random upper bytes at 0, 0x40, 0x80, 0xFF; the ring's
radii at 0, 1, 0xA000 / 0xB000 and random. **Disturbance** (the group's, from
the hash only): `+9`, `+6`, `+0xA`, `+3`, the words `+0x2E` and `+0x30`, the
particle count (inside 0x100), the chapter's count (0x18 half the time).

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_3a`,
exit 0): **192,000 rounds over 48 functions, 5,252,926 calls to the
stand-ins, 0 mismatches**; 29,900 bytes of state in 48 regions. Every entry of
the five tables reached (each handler recorder 411..1,344 calls); the group's
own callees all reached (`EffectKind68_InitMote` 1,385 the fewest),
`0x59E930` 904, `Effect_Release` 5,914, `Rand` 967,488.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
696 self-test lines, no mismatch anywhere, `inject: 7618 ours, 0 left
original`; `effect_3a` there 192,000 rounds, 5,251,650 calls, 0 mismatches.
Before the frame-row fix above, `'*'` died twice the same way (exit
0xC0000005, no `FATAL`, no crash line) inside `effect_3a`: reproducible, not
the silent flake HANDOFF records; bisected by `BOF3X_E3A_ONLY` with
`BOF3X_E3A_ROUNDS=40000` to `EffectKind61_Scatter`. After it, 1,920,000 rounds
alone (40,000 a function), 0 mismatches. WIDE_LINE_PLACEHOLDER

## 5. Controls

Planted behind `BOF3X_E3A_CTL=<n>` in a scratch copy of `effect_3a.cpp`
(`build/e3a_plant.py`, `build/e3a_ctl.sh` in this worktree, not committed:
every plant behind the switch, one on at a time; rebuilt once, each run under
`BOF3X_E3A_ONLY=<filter>`, then the file restored and rebuilt; the committed
file has no switch). **65 of 65 refused by a count**, every refused run exit
3; counts are rounds refused of 4,000 a function, in this worktree. Control 65
plants the stale record read before `Gfx_ClearRect` (refused only in the rounds
where the disturbance moved `Sprite_Current` across the call). The rim depths
DIV-0068 replaces are levelled on both sides, so a plant there cannot be
refused by design; control 39 plants the centre's depth instead.

| # | Run (`_ONLY`) | Plant | Refused (of 4,000 a function) |
|--:|---|---|---|
| 1 | `_Run` | every dispatcher to the next entry of its table | EffectKind60_Run 2637; EffectKind61_Run 3218; EffectKind62_Run 2634; EffectKind64_Run 3545; EffectKind68_Run 3017 |
| 2 | `EffectKind60_DrawLine` | the line's grey 0x41 | EffectKind60_DrawLine 4000 |
| 3 | `EffectKind61_Capture` | the borrowed record's kind 0x3B | EffectKind61_Capture 2964 |
| 4 | `EffectKind61_Capture` | the frame byte inverted | EffectKind61_Capture 2964 |
| 5 | `EffectKind61_Capture` | Sprite_Current left on the borrowed record | EffectKind61_Capture 2918 |
| 6 | `EffectKind61_Store` | the rectangle at x 0x341 | EffectKind61_Store 863 |
| 7 | `EffectKind61_Scatter` | x one column on | EffectKind61_Scatter 1248 |
| 8 | `EffectKind61_Scatter` | the count stored one short | EffectKind61_Scatter 1248 |
| 9 | `EffectKind61_Arm` | the pattern (i + 1) & 1 | EffectKind61_Arm 3452 |
| 10 | `EffectKind61_Arm` | the in-count + 0xB | EffectKind61_Arm 1394 |
| 11 | `EffectKind61_Arm` | the out-count 0x29 - | EffectKind61_Arm 3342 |
| 12 | `EffectKind61_Twinkle` | green >> 3 | EffectKind61_Twinkle 3300 |
| 13 | `EffectKind61_Twinkle` | the flags of the unflagged not toggled | EffectKind61_Twinkle 2578 |
| 14 | `EffectKind61_Twinkle` | with +6 set the point moved while the count lasts | EffectKind61_Twinkle 2824 |
| 15 | `EffectKind62_Start` | the gap 0x11 | EffectKind62_Start 3978 |
| 16 | `EffectKind62_Rays` | the gap down 1 | EffectKind62_Rays 1579 |
| 17 | `EffectKind62_Rays` | message 0x13 | EffectKind62_Rays 357 |
| 18 | `EffectKind62_Rays` | the wait not cleared after a ray | EffectKind62_Rays 1588 |
| 19 | `EffectKind62_Blast` | the outer speed up 0x8D | EffectKind62_Blast 4000 |
| 20 | `EffectKind62_Blast` | released with the rays still in use | EffectKind62_Blast 874 |
| 21 | `EffectKind62_ClearRays` | only 23 rays cleared | EffectKind62_ClearRays 1996 |
| 22 | `EffectKind62_SpawnRay` | the shades one up | EffectKind62_SpawnRay 4000 |
| 23 | `EffectKind62_SpawnRay` | the speed's z not negated | EffectKind62_SpawnRay 4000 |
| 24 | `EffectKind62_StepRays` | phase 1's shade down 9 | EffectKind62_StepRays 3944 |
| 25 | `EffectKind62_StepRays` | phase 0's count reload 7 | EffectKind62_StepRays 3019 |
| 26 | `EffectKind62_DrawRay` | green >> 2 | EffectKind62_DrawRay 3960 |
| 27 | `EffectKind62_DrawRing` | the inner colour 0x7E | EffectKind62_DrawRing 4000 |
| 28 | `EffectKind62_DrawRing` | the outer radius << 5 at angle 0 | EffectKind62_DrawRing 3338 |
| 29 | `EffectKind64_Start` | the height 0x800001 up | EffectKind64_Start 4000 |
| 30 | `EffectKind64_Grow` | +9 = 0x81 | EffectKind64_Grow 853 |
| 31 | `EffectKind64_Hold` | the glow's colour 7 | EffectKind64_Hold 4000 |
| 32 | `EffectKind64_Rise` | fifteen shards | EffectKind64_Rise 857 |
| 33 | `EffectKind64_B` | a spark every eighth frame | EffectKind64_Burst 582 |
| 34 | `EffectKind64_Burst` | the trail's x speed -0x1001 | EffectKind64_Burst 793 |
| 35 | `EffectKind64_Launch` | +9 = 0x21 | EffectKind64_Launch 4000 |
| 36 | `EffectKind64_Fly` | the z speed down 0x1FF | EffectKind64_Fly 4000 |
| 37 | `EffectKind64_FlyWait` | the count 0x17 | EffectKind64_FlyWait 2033 |
| 38 | `EffectKind64_Fade` | the shade down 7 | EffectKind64_Fade 3981 |
| 39 | `EffectKind64_DrawGlow` | the centre's depth the rim x | EffectKind64_DrawGlow 4000 |
| 40 | `EffectKind64_DrawGlow` | green (c & 0xFC) << 6 | EffectKind64_DrawGlow 2586 |
| 41 | `EffectKind64_DrawGlow` | the radius without the frame bit | EffectKind64_DrawGlow 2465 |
| 42 | `EffectKind64_DrawTrail` | the dots step 0x8000 | EffectKind64_DrawTrail 2826 |
| 43 | `EffectKind64_DrawTrail` | the first quad's y + half | EffectKind64_DrawTrail 3954 |
| 44 | `EffectKind64_ClearSparks` | seven sparks | EffectKind64_ClearSparks 1941 |
| 45 | `EffectKind64_StepSparks` | the size down 0x11 | EffectKind64_StepSparks 3984 |
| 46 | `EffectKind64_DrawSpark` | the rim (+0x16) on the second vertex | EffectKind64_DrawSpark 3984 |
| 47 | `EffectKind64_InitShard` | the speed (Rand & 2) + 4 | EffectKind64_InitShard 4000 |
| 48 | `EffectKind64_InitShard` | Y turned by +y | EffectKind64_InitShard 3997 |
| 49 | `EffectKind64_DrawShards` | the angle up 0x11 | EffectKind64_DrawShards 4000 |
| 50 | `EffectKind68_Start` | the count 0x41 | EffectKind68_Start 3977 |
| 51 | `EffectKind68_Rise` | a mote every eighth | EffectKind68_Rise 614 |
| 52 | `EffectKind68_Wall` | the count 0x5B after the motes | EffectKind68_Wall 1359 |
| 53 | `EffectKind68_` | the wall's shade - 0x167 | EffectKind68_Wall 1393; EffectKind68_End 1488 |
| 54 | `EffectKind68_End` | drawn from 0x5A | EffectKind68_End 419 |
| 55 | `EffectKind68_ClearMotes` | flag A kept | EffectKind68_ClearMotes 1949 |
| 56 | `EffectKind68_FindMote` | the second free | EffectKind68_FindMote 2032 |
| 57 | `EffectKind68_InitMote` | the rise % 0x301 | EffectKind68_InitMote 3929 |
| 58 | `EffectKind68_StepMotes` | the motes drawn before their state | EffectKind68_StepMotes 4000 |
| 59 | `EffectKind68_DrawMote` | the step 0x101 | EffectKind68_DrawMote 4000 |
| 60 | `EffectKind68_MoteGlow` | the shade up 0x11 | EffectKind68_MoteGlow 4000 |
| 61 | `EffectKind68_MoteHold` | the count 0x11 | EffectKind68_MoteHold 1289 |
| 62 | `EffectKind68_MoteRise` | the speed up 0x401 | EffectKind68_MoteRise 4000 |
| 63 | `EffectKind68_DrawWall` | the wall's blue + 1 | EffectKind68_DrawWall 4000 |
| 64 | `EffectKind68_DrawWall` | the wall linked at z + 1 | EffectKind68_DrawWall 4000 |
| 65 | `EffectKind61_Capture` | the copy from the record read before the clear | EffectKind61_Capture 85 |

## 6. Aborts

Where the original jumps or indexes past what it owns, ours aborts with a
message (the round-nine rule, no ledger entry); none is reached by the states'
own steps:

- a dispatcher's `+1` past its table (the next kind's table is jumped
  through); a mote's `+1` past `EffectKind68_MoteStates`' three (E3B's table
  is called through);
- kind 0x61's `+7` past ObjTrio's three records (the field objects after them
  are read and copied); the frame byte past the two frames (kind 0x61's state
  table is read as a frame); `Effect_FindFree`'s answer past the 20 records;
- a frame of w or h above 0xFF in `EffectKind61_Scatter` (the row and column
  are bytes: the original never ends). The two frames are 32 x 48 and 64 x
  64; neither reaches it.

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x61's particles have no bound.** Each opaque pixel of the frame
  becomes a particle of 0x14 at `0x92DF80 + 0x14 n`, n up to w x h: the
  second frame (the member's `+0x148` not 0) is 64 x 64, up to 4,096
  particles (0x14000 bytes, to `0x941F80`). The 2,049th lands on
  `0x937F80`: its `+4` float on `Gfx_CurrentEnv` `0x937F84` and its `+8` float
  on `Sprite_Current` `0x937F88`, then `Frame_Counter`; the next state's
  `Sprite_Current` access goes astray. The first frame (32 x 48, 1,536) cannot
  reach it. Whether a member's captured sprite has more than 2,048 opaque
  pixels in its 64 x 64 is not measured. E2C's kind 0x6B has the same shape
  ([`effect_2c.md`](effect_2c.md) section 7). Ours writes as the original.
- **The particles' `+0xC` is never written.** `EffectKind61_Scatter` writes
  `+2`, `+4`, `+8`; `EffectKind61_Twinkle` copies `+4..+0xF` into the
  `TILE_1`, so the tile's depth is whatever the buffer held there (an earlier
  effect's bytes). Ours copies the same.
- **The glow's rim depth** is an unwritten stack word: DIV-0068 (section 2).
- **A capture with no free record** steps on anyway: `EffectKind61_Capture`
  with `Effect_FindFree` answering none still advances, so the store reads
  back whatever VRAM `(0x340, 0x100)` held and the scatter makes particles of
  it.
- **The pools overlap between kinds.** Kinds 0x62 (rays), 0x64 (sparks and
  shards), 0x68 (motes) and 0x61 (pixels) all start at `0x92BF80`, as do
  E2A..E2F's kinds there; kind 0x64's sparks end where its shards begin, so it
  does not overlap itself. Two of these alive at once would step each other's
  records. Chapter 10's run 2 spawns 0x68, 0x61, 0x62, 0x69, 0x64 on its count
  and timers; whether two are alive at once was not measured.
- **`EffectKind64_DrawShards` draws all 16 shards with no in-use test**, from
  the first frame of state 4 whether or not state 3 started them in this
  kind's life (it always has, by the table's order).

## 8. Calls across groups

- **Out of the group, to groups of this round** (`band_rows.py --edges`): E4F
  (wave four) `0x493B50` from the four spark ticks (`0x480F10`, `0x480F90`,
  `0x480FD0`, `0x481090`) and `0x493C60` from `EffectKind64_DrawShards`: raw
  (`effect_3a_callees.h` `kSparkSpawn`, `kShardDraw`), re-aimed at recorders in
  the fuzz. EGT's four (`EffectGte_*`, merged) by name, 34 sites.
- **Into the group from outside** (for the rebinding pass): E4D's `0x48D100`
  calls `EffectKind64_ClearSparks` `0x482050`; E4E's `0x4906A0` calls
  `EffectKind64_DrawSpark` `0x4820C0`; E4F's `0x493610` calls
  `EffectKind64_ClearSparks` and `EffectKind64_InitShard` `0x482240`, its
  `0x493BA0` calls `EffectKind64_DrawSpark`; the table E4F's dispatcher
  `0x493550` indexes (`0x655320`) holds `EffectKind64_Start` `0x480D60` as its
  state 0. Kind 0x60's catalog part 6 states `0x480270` / `0x4802C0` call
  `EffectKind60_DrawLine` `0x4804C0`. Being a wave before E4D..E4F, these are
  ours by name when those groups write.

## 9. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 48 to the byte (7,604 bytes against the
  cut's 7,825: 28 differ by padding only, none by code); every extent checked
  against the disassembly, none changed.
- **Hidden starts**: 29, each an entry by address - a cell of
  `Effect_KindHandlers`, of a kind's table or of `EffectKind68_MoteStates`. Their
  hosts: `0x47FBE0` (catalog part 6) for `0x4801F0`; `0x4804C0` (ours) for 25,
  whose recorded extent 0xDF0 spans to `0x4812B0` - ours ends at `0x480583`,
  so none of the 25 is a fall-through of it; `0x481CC0` (ours) for the three
  mote states, after its `ret` at `0x481DFD`. No start is a case or a shared
  tail; none added, none dropped.
- **The `hypothesis` rows** (`0x4801F0`, `0x4804C0`, `0x481150`): effect code
  (kind 0x60's dispatcher, its line, kind 0x68's dispatcher), taken.
- **The cut's `unit` / `label` columns**: the part-5 rows "Area overlays, world
  2" with the unit "Fn_480590 kind 0x61" (`0x4811F0`, `0x481260`,
  `0x481C40`..`0x481EA0`) are kind 0x68's; "X:Capcom's raw" rows are kinds
  0x62, 0x64 and 0x68's helpers. The PSX twins (`0x801F3F50`, `0x801F4E7C`,
  `0x801F4BC0`, `0x801F5050`) are AREA overlay copies with no name in the
  sibling.
- **Not taken, in the band**: `0x480210`, `0x480270`, `0x4802C0` (kind 0x60's
  states 0..2, catalog part 6 rows hidden in `0x47FBE0`) and `0x480300` (a
  part 6 row, PSX twin `0x801F9F8C`, called by kind 0x5F's states `0x47FE72`
  .. `0x4801D2`): in no group of this round, the coordinator's to place.

## 10. The live route

The catalog's reach columns (attract, shop, worldmap, combat) are empty for
all 48 rows, and no first-call trace under `analysis/calltrace` holds any of
them (only the entry lists name them). **Fuzz only**: kinds 0x61..0x68 are
chapter 10's run 2 and kind 0x60 chapter 8's run 11; a recorded route through
either chapter's scene would let the coordinator's frame-hash A/B cover them.

## 11. The rebinding

`band_rows.py --refs`: **no raw reference** to any of the 48 in `src/game` -
nothing to rebind. The five state tables and the motes' table are named as
`[[data]]` (section 3), with `EffectKind61_Frames`. The E4D / E4E / E4F
callers above are in groups of a later wave: they call these by name.

## 12. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03), one line per function with
the extent read (section 9); the host lines `004804C0 DF0`, `00481CC0 1DF` and
`00482360 344` stand beside the smaller extents added for `0x4804C0`
(0xC3), `0x481CC0` (0x13E) and `0x482360` (0x63).
