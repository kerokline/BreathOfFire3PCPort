# Group E1D: effect kinds 0x21..0x27

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..11),
wave one, on the round branch's tip `1bb41df`. **30 functions ours**
(`src/game/effect_1d.cpp`, shadow name `effect_1d`): the cut table's 30 rows
for E1D (`analysis/round13_cut.tsv`, the band `0x46F2B0..0x4702F6`), no start
dropped and none added. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
180,000 rounds, 0 mismatches. 72 controls planted one at a time, 71 refused by
a count, one equivalent with its near variant refused. **Fuzz only**: no
recorded route enters any of the 30 (section 9).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x21: an arm of four points round a centre, built and drawn by two callees nobody owns; grows, lifts, holds, swirls, folds | 7 | `Effect_KindHandlers[0x21]` (`0x6553D4`), `EffectKind21_States` `0x654284` (6) |
| 0x22: five kind-0x21 arms spawned at its point and waited on | 4 | `Effect_KindHandlers[0x22]` (`0x6553D8`), `EffectKind22_States` `0x65429C` (3) |
| 0x23: kind-0x24 rays spawned for 0x60 frames | 3 | `Effect_KindHandlers[0x23]` (`0x6553DC`), `EffectKind23_States` `0x6542A8` (3) |
| 0x24: a ray of random direction and colour from the leader, shrinking | 3 | `Effect_KindHandlers[0x24]` (`0x6553E0`), `EffectKind24_States` `0x6542B4` (3) |
| 0x25: a disc at the leader's screen point - grows, glows, shrinks - and its draw | 6 | `Effect_KindHandlers[0x25]` (`0x6553E4`), `EffectKind25_States` `0x6542C0` (5) |
| 0x26: a band of a dome widening at a point, and its draw | 4 | `Effect_KindHandlers[0x26]` (`0x6553E8`), `EffectKind26_States` `0x6542D4` (3) |
| 0x27: a kind-0x26 band every 16 frames for 0x1000 frames | 3 | `Effect_KindHandlers[0x27]` (`0x6553EC`), `EffectKind27_States` `0x6542E0` (3) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`). "Arm", "ray", "disc", "dome band" name the code's shape - the
points it builds, the primitives it commits - not a play-tested fact: where
the game shows these kinds, and what they look like on screen, was not traced
(section 9; the owner's word, not this doc's). The only spawner found in the
image is area 81's handler 0 (`Area81_SpawnEffect27AtObject` `0x40F5B0`,
`area_w2a.cpp`), which puts a kind-0x27 record at the running object; the
kinds 0x21, 0x24 and 0x26 are spawned by this group's own states; no
immediate `0x22`, `0x23` or `0x25` stored into a record's `+5` or pushed
before a call to `Effect_Spawn` / `Effect_SpawnAt` / `Effect_FindFree` was
found by a byte scan of `.text` (scratch `scan.py`) - a kind stored from
script data is not seen by that scan.

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes, `0x7E11E0`) `Sprite_Current` and calls `Effect_KindHandlers[+5]`.
Each kind here is a four-instruction dispatcher - `mov ecx, [Sprite_Current];
xor eax, eax; mov al, [ecx + 1]; jmp [eax * 4 + T]`, no compare - and the
states its table names. The byte `+1` is the state, `+9` a frame count, `+6`
kind 0x21's "done" byte; the rest per kind below. Every function's comment in
`effect_1d.cpp` is the full read; each `symbols.toml` evidence string the
summary. "Sprite_Current read afresh" means the original reloads the pointer
before an access (after a call, the harness's disturbance may have moved it);
ours reads it where the original does.

### 1.1 Kind 0x21 (`EffectKind21_Run` `0x46F2B0`)

The arm is the record from `+0xC`: `+0xC..+0x14` its centre (x, z, height,
written by the spawner), `+0x18..+0x57` the four points of 0x10 the callee
`0x46F570` writes each frame, the words `+0x58` (an angle the callee takes the
cosine and sine of), `+0x5A`, `+0x5C` (two turns) and `+0x5E` (a length).
States 1..5 each call `0x46F570(record + 0xC)` then `0x46F690(the same)` (two
Gouraud triangles of the four points) with the pointer taken on entry, then
step their words through that pointer:

| State | Function | What |
|---|---|---|
| 0 | `EffectKind21_Start` `0x46F2D0` | `+9` = 0xF, `+1` up |
| 1 | `EffectKind21_Grow` `0x46F2F0` | length `+0x5E` + 1; `+9` down, at 0 `+9` = 8, `+1` up |
| 2 | `EffectKind21_Lift` `0x46F340` | angle `+0x58` + 4; `+9` down, at 0 `+9` = 8, `+1` up |
| 3 | `EffectKind21_Hold` `0x46F390` | `+9` down, at 0 `+9` = 0x80, `+1` up |
| 4 | `EffectKind21_Swirl` `0x46F3D0` | turns `+0x5A` - 8, `+0x5C` - 0x40; `+9` down, at 0 `+1` up |
| 5 | `EffectKind21_Fold` `0x46F410` | angle `+0x58` - 8; below 0 (s16): `+6` = 1 and a tail `jmp` to `Effect_Release` |

### 1.2 Kind 0x22 (`EffectKind22_Run` `0x46F450`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind22_Sound` `0x46F470` | `Sound_PlayEffect(0x209)`; `+1` up (read after the call) |
| 1 | `EffectKind22_SpawnArms` `0x46F490` | five times `Effect_FindFree`; each record found: its address into the dword `+0xC + 4 i` of the record current on entry (**not written on `0xFF`**), `+0` 1, `+6` 0, `+5` 0x21, its centre from `Sprite_Current`'s `+0x34` / `+0x38` / `+0x3C` (each read afresh), `+0x58` 6, `+0x5A` 0, `+0x5E` 0, `+0x5C` 0x333 i (a fifth of 0x1000 each); `+1` up |
| 2 | `EffectKind22_WaitArms` `0x46F530` | returns while the byte `+6` of any of the five records `+0xC..+0x1C` point at is 0 (each pointer dereferenced as it stands); all five done: the counter byte `0x903848` up and `Effect_Release` |

`0x903848` is the chapters' counter byte (`scenario_harness` `at::kCounter`) -
the event scripts wait on it, so a kind-0x22 effect tells its script it is
over. Which script waits was not traced.

### 1.3 Kinds 0x23 and 0x24 (`0x46F790`, `0x46F820`)

| State | Function | What |
|---|---|---|
| 0x23 / 0 | `EffectKind23_Start` `0x46F7B0` | `+9` = 0x60, `+1` up, `Sound_PlayEffect(0x204)` |
| 0x23 / 1 | `EffectKind23_SpawnRays` `0x46F7D0` | with `+9` bit 2, `Effect_FindFree`; a record found gets `+0` 1, `+5` 0x24 (nothing else); `+9` down, at 0 `+1` up |
| 0x23 / 2 | FC1's `Effect_StateRelease` `0x46A310` | |
| 0x24 / 0 | `EffectKind24_Start` `0x46F840` | through `record + 0xC` taken on entry: length `+0xC` 0x600, scales `+0x12` = `+0x14` = 0x100, angles `+0xE` = `Rand & 0x700`, `+0x10` = `Rand & 0xF00`, colour `+0x18`, `+0x17`, `+0x16` = 0xFF (that order), then `+0x16` = 0 on an odd `Rand`, `+0x18` = 0 on the next odd one; `+1` up |
| 0x24 / 1 | `EffectKind24_Shrink` `0x46F8B0` | `0x46FAE0(record + 0xC)` draws four Gouraud lines from **the leader's** point (`ObjTrio` `+0x34..+0x3C`), not the ray record's; `+0x12` - 0x10, held at 0 (s16); while it is below 0x80, `+0x14` - 0x10; once `+0x14` is below 0 (s16), `+0x12` = 0 (sic: section 7) and `+1` up |
| 0x24 / 2 | `Effect_StateRelease` | |

### 1.4 Kind 0x25 (`EffectKind25_Run` `0x46F900`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind25_Start` `0x46F920` | `Prim_VertexScratch[0]` = (leader x `0x802D74` sar 9) - 0x4000, `[2]` = -(leader height word `0x802D7E` / 2, toward zero), `[1]` = (leader z sar 9) - 0x4000; `Gte_RotTransPers(Prim_VertexScratch, MapView_ScreenXY, p, flag)` - its answer to the dword `+0x60`, the two floats through the CRT's `_ftol` to the words `+0x2E` / `+0x30`; `+9` 0, `+1` up |
| 1 | `EffectKind25_Grow` `0x46F9D0` | `EffectKind25_DrawDisc(+0x2E, +0x30, +9, 0x80, 0)`; `+9` + 0xF; above 0x3C (unsigned) `+9` = 0xFF, `+1` up |
| 2 | `EffectKind25_Glow` `0x46FA20` | the disc at radius 0x3C + (`+9` & 1); at `+9` = 0xD7 `Sound_PlayEffect(0x208)`; `+9` down, at 0 `+9` = 0x3C, `+1` up |
| 3 | `EffectKind25_Shrink` `0x46FA90` | the disc at radius `+9`; `+9` down; below 0 (s8) `+1` up |
| 4 | `Effect_StateRelease` | |

`EffectKind25_DrawDisc` `0x46FCF0` (x, y, radius, centre, rim): a draw mode
(`Gpu_GetTPage(0, 1, 0x3C0, 0)`, `Gpu_SetDrawMode(packet, 0, 1, it, 0)`,
committed 0xC), then 32 semi-transparent Gouraud triangles (`Gpu_SetPolyG3`,
`Gpu_SetSemiTrans(1)`, committed 0x34): the centre (x, y) as floats and two rim
points `(x + cos(u) * r sar 12, y + sin(u) * r sar 12)` for u = a and a + 0x80,
a = 0..0xF80; the centre shaded `centre`, the rim `rim`. The original keeps the
angle and the next one in its own argument slots (it overwrites them): ours
keeps them in locals, same values.

### 1.5 Kinds 0x26 and 0x27 (`0x46FE60`, `0x46FEE0`)

| State | Function | What |
|---|---|---|
| 0x26 / 0 | `EffectKind26_Start` `0x46FE80` | the dword `+0xC` = 0, `+1` up |
| 0x26 / 1 | `EffectKind26_Widen` `0x46FEA0` | `EffectKind26_DrawBand(+0x34, +0x38, +0x3C, word +0xC)`; `+0xC` + 0x10; above 0x500 (signed) `+1` up |
| 0x26 / 2 | `Effect_StateRelease` | |
| 0x27 / 0 | `EffectKind27_Start` `0x46FF00` | the dword `+0xC` = 0x1000, `+1` up |
| 0x27 / 1 | `EffectKind27_Emit` `0x46FF20` | when `+0xC`'s low four bits are 0, `Effect_FindFree`; a record found: `+0` 1, `+5` 0x26, its `+0x34` / `+0x38` `Sprite_Current`'s, its `+0x3C` = `AreaMap_Elevation(x, z)`'s word sign-extended `<< 16` (on the ground); `+0xC` down, at 0 `+1` up |
| 0x27 / 2 | `Effect_StateRelease` | |

`EffectKind26_DrawBand` `0x46FFB0` (x, z, height, t): a draw mode (as the
disc's) and `EffectGte_LoadMapCamera`; of t's low word, the outer polar angle
`ro` = t, at most 0x400, shaded 0x80 - past 0x400, `ro` = 0x400 and the shade
(0x500 - t) * 0x80 / 0x100 (a signed divide by `cdq; and edx, 0xFF; add; sar
8`, its low byte); the inner `ri` = t - 0x100, shaded 0 - below 0 as s16, `ri`
= 0 and the shade (0x100 - t) * 0x80 / 0x100. Then sixteen semi-transparent
Gouraud quads (`Gpu_SetPolyG4`, `Gpu_SetSemiTrans(1)`, committed 0x44) at a =
0..0xF00 by 0x100, b = a + 0x100: each vertex `EffectGte_ProjectPoint` of the
world point (x + (3 cos(u) sin(p) sar 8, low four bits cleared), z + (3 sin(u)
sin(p) sar 8, cleared), height + 3 cos(p) `<< 12`) for (u, p) = (a, ro), (a,
ri), (b, ro), (b, ri), shaded outer, inner, outer, inner. So a band of a dome of
radius three cells round the point, from the pole outward as t grows; the
caller's t runs 0, 0x10, .. 0x500 (81 frames), the band fading as it passes
0x400. The 20 `Math_Sin` / `Math_Cos` calls a quad makes are in the
original's order (it recomputes `sin(ro)` twice per vertex), because each is a
recorder in the fuzz.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-09-29). Where the original indexes past a
table ours aborts with a `Fatal` naming the function (the owner's rule, round9
doc section 6; nothing reaches it): the seven dispatchers past their tables
(the original jumps through the next kind's entry - they lie back to back - or,
past `EffectKind27_States`, through the two dwords at `0x6542EC` that are not
code), and `Effect_FindFree`'s answer past the twenty records (it answers
0..19 or `0xFF`). `EffectKind22_WaitArms` dereferences its five cells as the
original does, unchecked (section 7).

Every function is `void`. The two draw helpers read their arguments as the
callee's code does (section 3); ours declares them as whole words and narrows
inside, so a Capcom caller pushing a register with leftovers above the byte or
word (under `BOF3X_ORIGINAL`) still gets the original's reading. The CRT's
`_ftol` is reproduced (truncation through a 64-bit `fistp`, 0 in the low word
for NaN and out of range, as `battle_items.cpp`'s `Ftol16`).

## 3. The arguments pushed with leftovers

| Callee | Pushed | Read by the callee |
|---|---|---|
| `EffectKind25_DrawDisc` | x `ax`, y `dx`, radius `cx` (`movzx cx, byte +9`, or `0x3C + (+9 & 1)` in `edx` over `xor dx, dx`), 0x80, 0 - upper halves `Sprite_Current`'s or the caller's | x, y, radius `movsx` 16 bits; centre `mov al, [arg3]`, rim `mov bl, [arg4]` |
| `EffectKind26_DrawBand` | x, z, height dwords, t `cx` (upper half `Sprite_Current`'s) | x, z, height whole; t: `cmp cx`, `lea ebp, [ecx - 0x100]` then `test bp` / `and 0xFFFF` - 16 bits |

The fuzz lists both with those masks (`k16` / `k8`), and draws the helpers'
own arguments with leftovers above them.

## 4. The fuzz (`effect_1d_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_1d`, effect mode (`g.effect`, kinds
0x21..0x27), 6,000 rounds a function (`BOF3X_E1D_ONLY=<name>` runs the clones
whose name holds it). Shapes: 28 `kEffect` (each clone's own kind; the seven
dispatchers' `state_span` their table's length - 6, 3, 3, 3, 5, 3, 3), two
`kCall` (the draw helpers). The seven tables are `DataTable`s, swapped for
recorders on both sides. No region beyond effect mode's standard ones: the
records, `Sprite_Current`, `ObjTrio` (the leader), `Prim_VertexScratch`,
`MapView_ScreenXY` and the counter `0x903848` (in `0x903840..`), the packet
buffer.

**Callees**: the effect-standard rows for `Effect_FindFree` (a free record or
none), `Effect_Release` (clears `+0..+4`), `0x46F570` (the arm hashed to
`+0x54`), `0x46FAE0` (the ray hashed), `Sound_PlayEffect`, `Rand`,
`AreaMap_Elevation`, `Math_Sin` / `Cos`, the `Gpu_*`, `Gfx_CommitPrim` (moves
the cursor), `EffectGte_LoadMapCamera`; the CRT's `_ftol` `kThrough`. Re-listed
in the group: the two helpers by name (masks of section 3); `0x46F690`
(standard: the pointer logged) with the arm hashed to `+0x54`, so the words the
states step are compared at the draw too; `EffectGte_ProjectPoint` - the
standard row logs the point's pointer, a stack local whose address differs
between the copy and ours, so the point is hashed (12 bytes) instead and out
filled with floats; `Gte_RotTransPers` - the effect-mode row fills the screen
point with whole numbers, which cannot tell `_ftol`'s truncation from rounding
(control 41 passed against it), so ours fills fractions and, one time in
eight, NaN or a value past 2^63.

**Seeds** (per function, after the harness's per-round fill): `+9` at every
compare the states make (0, 1, 2, 4, 5, 8, 0x2C..0x2E, 0x3C, 0x3D, 0x7F..0x81,
0xD7, 0xD8, 0xFF or random); kind 0x21's angle `+0x58` at the fold's 0 and its
sign (0, 7..9, 0x8007..0x8009, 0xFFFF); `EffectKind22_WaitArms`: every record's
five cells at records (the original dereferences them) and each record's `+6`
0 one time in eight (all five done about half the time); the ray's scales
`+0x12` / `+0x14` at 0x10, 0x90 and their neighbours and signs;
`EffectKind26_Widen`'s `+0xC` at 0x4EF..0x4F1, 0x7FFFFFEF..F0, `0xFFFFFFF0`;
`EffectKind27_Emit`'s `+0xC` with its low nibble 0 or not and at 1;
`EffectKind26_DrawBand`'s t at 0, 0x80, 0xFF, 0x100, 0x101, 0x3FF..0x401,
0x4FF..0x501, 0x7FFF, 0x8000, 0x80FF, 0x8100, 0xFFFF, with leftovers above;
`EffectKind25_DrawDisc`'s radius in the callers' 0..0x4B half the time.
**Disturbance** (the group's, from the hash only): `+9`, the arm's `+0x58` and
`+0x5E`, the ray's `+0x12` / `+0x14`, the dword `+0xC`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_1d`,
exit 0): 180,000 rounds over 30 functions, 4,197,701 calls to the stand-ins,
**0 mismatches**; 24,756 bytes of state in 45 regions. Every entry of the
seven tables reached (each state's handler recorder 946..2,032 calls; the
five `Effect_StateRelease` cells 9,206 together), `Effect_FindFree` 36,277,
`Effect_Release` 7,028, `AreaMap_Elevation` 2,066, the glow's sound among
`Sound_PlayEffect`'s 12,330.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read all 30 to the byte (2,829 bytes against the
  cut's 3,075: 26 differ by padding only, none by code). The two not hidden,
  `0x46FCF0` and `0x46FFB0`, have `entries_logic.txt` lines longer than their
  code (0x2B7 spans kind 0x26's and 0x27's starts; 0x434 runs past the `ret` at
  `0x4702F5`): section 11.
- **Hidden starts**: 28, each an entry by address - a cell of
  `Effect_KindHandlers` or of its kind's state table - not a case or a shared
  tail. Their recorded hosts: `0x46EE20` (E1C's, a kind-0x1E state; its extent
  line covers `0x46F2B0..0x46F530`), `0x46F6F0` (catalog part 6: the arm's
  triangle draw `0x46F690` calls) and `0x46FCF0` (ours now). None of the hosts
  contains our code as a fall-through: each start is reached only by its cell.
- **The `hypothesis` rows** (the five dispatchers `0x46F790`, `0x46F820`,
  `0x46F900`, `0x46FE60`, `0x46FEE0`): each is the four-instruction dispatcher
  of its kind, `Effect_KindHandlers[0x23..0x27]`: effect code, taken.
- **No start dropped, none added**: the tool printed 0 "code no list has" in
  the band. The code between (`0x46F570..0x46F78A`, `0x46FAE0..0x46FCE3`) is
  four functions of catalog part 6 (below), not in the cut.

## 6. Controls

Planted one at a time behind `BOF3X_E1D_CTL=<n>` in a scratch copy of
`effect_1d.cpp` (scratch `plant.py`, `ctl.sh`: plant, rebuild, run each under
`BOF3X_E1D_ONLY=<its function>`, restore, rebuild; the committed file has no
switch). Counts are rounds refused of 6,000, in this worktree.

| # | Function | Plant | Refused |
|--:|---|---|--:|
| 1..7 | the seven dispatchers | jump to the next entry (mod the table) | 6,000 each |
| 8 | `EffectKind21_Start` | `+9` = 0xE | 6,000 |
| 9 | `EffectKind21_Grow` | length + 2 | 6,000 |
| 10 | `EffectKind21_Grow` | reload 9 | 293 |
| 11 | `EffectKind21_Lift` | angle + 3 | 6,000 |
| 12 | `EffectKind21_Hold` | reload 0x7F | 293 |
| 13 | `EffectKind21_Swirl` | `+0x5A` - 7 | 6,000 |
| 14 | `EffectKind21_Swirl` | `+0x5C` - 0x3F | 6,000 |
| 15 | `EffectKind21_Swirl` | no `+1` at 0 | 293 |
| 16 | `EffectKind21_Fold` | folds at `<= 0` | 718 |
| 17 | `EffectKind21_Fold` | done `+6` = 2 | 3,560 |
| 18 | `EffectKind21_Hold` | the draw skipped | 6,000 |
| 19 | `EffectKind21_Grow` | a draw before the build | 6,000 |
| 20 | `EffectKind22_Sound` | sound 0x20A | 6,000 |
| 21 | `EffectKind22_Sound` | `+1` on the record read before the call | 217 |
| 22 | `EffectKind22_SpawnArms` | arm `+6` = 1 | 5,993 |
| 23 | `EffectKind22_SpawnArms` | `+0x5C` = 0x334 i | 5,953 |
| 24 | `EffectKind22_SpawnArms` | the cell zeroed on `0xFF` | 4,786 |
| 25 | `EffectKind22_SpawnArms` | height from `+0x40` | 5,993 |
| 26 | `EffectKind22_SpawnArms` | `+0x58` = 7 | 5,993 |
| 69 | `EffectKind22_SpawnArms` | x from the entry record, not `Sprite_Current` afresh | 993 |
| 27 | `EffectKind22_WaitArms` | four arms | 382 |
| 28 | `EffectKind22_WaitArms` | counter + 2 | 3,190 |
| 29 | `EffectKind22_WaitArms` | reads `+7` | 2,700 |
| 30 | `EffectKind23_Start` | `+9` = 0x5F | 6,000 |
| 31 | `EffectKind23_Start` | sound 0x205 | 6,000 |
| 32 | `EffectKind23_SpawnRays` | bit 3 | 1,778 |
| 33 | `EffectKind23_SpawnRays` | kind 0x25 | 2,624 |
| 34 | `EffectKind24_Start` | angle `& 0x7FF` | 5,819 |
| 35 | `EffectKind24_Start` | the colour tests swapped | 2,962 |
| 36 | `EffectKind24_Start` | scale 0x101 | 6,000 |
| 37 | `EffectKind24_Shrink` | below 0x81 | 551 |
| 38 | `EffectKind24_Shrink` | `+0x14` zeroed (the store "fixed") | 3,078 |
| 39 | `EffectKind25_Start` | height `>> 1` (floor) | 1,503 |
| 40 | `EffectKind25_Start` | x - 0x3FFF | 6,000 |
| 41 | `EffectKind25_Start` | `_ftol` rounding to nearest | 1,573 |
| 71 | `EffectKind25_Start` | `_ftol` out of range answers 0xFFFF | 1,178 |
| 42 | `EffectKind25_Start` | `+0x60` not stored | 6,000 |
| 43 | `EffectKind25_Grow` | `+9` + 0xE | 3,252 |
| 44 | `EffectKind25_Grow` | `< 0x3C` | 348 |
| 45 | `EffectKind25_Glow` | sound at 0xD6 | 373 |
| 46 | `EffectKind25_Glow` | radius 0x3D - bit | 6,000 |
| 47 | `EffectKind25_Glow` | reload 0x3B | 316 |
| 48 | `EffectKind25_Shrink` | end at `+9` = 0xFF only | 1,597 |
| 49 | `EffectKind25_DrawDisc` | centre and rim swapped | 5,979 |
| 50 | `EffectKind25_DrawDisc` | 31 triangles | 6,000 |
| 51 | `EffectKind25_DrawDisc` | `sar 11` | 5,958 |
| 52 | `EffectKind25_DrawDisc` | b = a | 6,000 |
| 53 | `EffectKind25_DrawDisc` | radius unsigned | 1,434 |
| 54 | `EffectKind26_Start` | `+0xC` = 1 | 6,000 |
| 55 | `EffectKind26_Widen` | + 0x11 | 6,000 |
| 56 | `EffectKind26_Widen` | unsigned compare | 1,207 |
| 57 | `EffectKind26_DrawBand` | outer cap 0x3FF | 3,539 |
| 58 | `EffectKind26_DrawBand` | outer shade by a logical shift | 1,599 |
| 59 | `EffectKind26_DrawBand` | inner never below 0 | 2,005 |
| 60 | `EffectKind26_DrawBand` | mask `~7` | 6,000 |
| 61 | `EffectKind26_DrawBand` | height `<< 11` | 6,000 |
| 62 | `EffectKind26_DrawBand` | vertex 1 shaded outer | 4,165 |
| 63 | `EffectKind26_DrawBand` | vertex 1: cos before sin | 6,000 |
| 64 | `EffectKind27_Start` | 0xFFF | 6,000 |
| 65 | `EffectKind27_Emit` | nibble `& 7` | 46 |
| 66 | `EffectKind27_Emit` | ground zero-extended from its word | **0: equivalent** |
| 72 | `EffectKind27_Emit` | ground sign-extended from its byte (66's near variant) | 2,013 |
| 67 | `EffectKind27_Emit` | kind 0x25 | 2,020 |
| 68 | `EffectKind27_Emit` | down 2 | 6,000 |
| 70 | `EffectKind27_Emit` | elevation (z, x) | 2,020 |

**71 of 72 refused by a count.** Control 66 is an equivalent mutant: `movsx
ecx, ax; shl ecx, 16` keeps only the word's 16 bits in the top half, so
sign- and zero-extending give the same dword - no input tells them apart; its
near variant 72 is refused. Control 41 passed on the first run, against the
effect-mode row's whole-number screen floats: the fuzz's fault (section 4),
refused once the group re-listed `Gte_RotTransPers`; every other control was
refused on the first run.

## 7. Latent defects (Capcom's, described, not fixed)

- **`EffectKind22_WaitArms` reads a cell `EffectKind22_SpawnArms` did not
  write.** When `Effect_FindFree` answers `0xFF` for an arm (the pool of 20 is
  full), the dword `+0xC + 4 i` keeps whatever the record held from its last
  use - another kind's data, 0, or a record address from an earlier kind-0x22
  life. The wait then reads byte `+6` through it: a small value faults, a
  stale record address answers for a record that may be anything now. So a
  full pool can hang the effect (the byte 0, forever), end it early, or crash.
  Ours dereferences the cell as the original does.
- **The arms' "done" byte outlives them.** `Effect_Release` clears `+0..+4`
  only, so a folded arm's `+6` = 1 stays until its record is reused. If another
  spawner takes the record first and writes `+6` (kind 0x22 itself writes 0),
  the waiting parent sees "not done" and waits on a record that is no longer
  its arm.
- **Spawned records rely on `+1` being 0**: kinds 0x21 (arms), 0x24 (rays) and
  0x26 (bands) get `+0` and `+5` but not `+1`, so they start at state 0 only
  because `Effect_Release` cleared `+1` (as FC2 found for kind 0x34).
- **`EffectKind24_Shrink` zeroes the wrong scale**: when `+0x14` goes below 0
  it stores 0 to `+0x12` (already at 0 by then) and leaves `+0x14` negative. No
  effect: the next state is `Effect_StateRelease`, so the ray is not drawn
  again. Reproduced (control 38 shows it is observable in the record).
- **Rays are drawn from the leader**, not from the kind-0x23 record that
  spawned them: `0x46FAE0` reads `ObjTrio` `+0x34..+0x3C`, and neither the
  spawner nor `EffectKind24_Start` gives the ray a position. Whether that is
  the intent (a burst round the party) is the owner's to say.
- **`EffectKind26_DrawBand`'s outer shade wraps past t = 0x500** ((0x500 - t)
  * 0x80 / 0x100 is negative, its low byte bright). Unreached: the only caller
  draws with t up to 0x500 and stops.
- **The seven dispatchers index unchecked.** Every writer of `+1` in the band
  steps it inside its table; ours aborts past it.

## 8. Calls across groups

**Outbound, raw** (`effect_1d_callees.h`): `0x46F570` (5 sites, the arm's
points), `0x46F690` (5 sites, the arm's draw) and `0x46FAE0` (1 site, the ray's
lines) - catalog part 6 ("Scenario event banks", by their PSX twins' section:
`0x46F570` is paired with `0x801F7FFC`, `0x46FAE0` disputed), **in no group of
this round**: the coordinator's to place. Each read to its last instruction for
what it reads and writes (the comments in `effect_1d_callees.h`); `0x46F690`
calls `0x46F6F0` twice (the triangle, also part 6). **By name, already ours**:
EGT's `EffectGte_LoadMapCamera` (1 site) and `EffectGte_ProjectPoint` (4) -
`band_rows.py --edges`' five E1D -> E4F edges -, FC1's `Effect_StateRelease`
(five table cells), and the engine's.

**Inbound from outside the group** (for the rebinding pass): none by call.
`Effect_RunObjects` (ours) reaches the seven dispatchers through
`Effect_KindHandlers` `0x6553D4..0x6553EC` (read in place: no rebinding);
`Area81_SpawnEffect27AtObject` `0x40F5B0` (ours, `area_w2a.cpp`) stores the
kind 0x27.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 30 rows and for the four part-6 functions beside
them. `analysis/calltrace/reach_dragon` and `reach_whelp`
(`bof3x.calltrace.tsv`): neither enters `0x46FCF0`, `0x46FFB0` (listed in
`entries_logic.txt` at the time, so armed), nor the armed part-6 callees
`0x46F570`, `0x46F690`, `0x46FAE0` - so no arm, ray, disc or band was drawn on
either route; the hidden starts were not in the list then and are not
evidence either way. `Effect_RunObjects` runs on both (19,816 and 19,756
calls). **Fuzz only**: the coordinator's frame-hash A/B covers none of the 30
until the owner records a route through a place that shows these kinds (area
81 spawns kind 0x27).

## 10. The rebinding

`grep -rn -i` of the 30 addresses and the seven tables' in `src/game` (and
`band_rows.py --refs`): the only raw references are in
`scenario_harness_ekh.cpp` - EKH's self-test copies `0x46F2B0` and `0x46F7D0`
from the image (clone lines, comments, `CloneOriginal` and its control's byte
check) and lists `{0x654284, 6}` as a `DataTable`. **Left raw, for the
coordinator**: that file is a harness, which the brief forbids a group to
edit; the addresses there name Capcom's originals being copied, and the values
stay valid (the fold would spell them `bof3::addr::EffectKind21_Run`,
`EffectKind23_SpawnRays` and `EffectKind21_States`). No constant, call site or
fuzz key of another file names an E1D address. Our own fuzz file lists the
seven tables by address beside their names in `symbols.toml`.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block): the 28
hidden starts, and the smaller extents of `0x46FCF0` (0x16E; its line 0x2B7
spans kind 0x26's and 0x27's starts) and `0x46FFB0` (0x346; its line 0x434
runs past the `ret`) - their old lines left for the merger to drop.
