# Group EGT: the effect engine's four GTE helpers

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 10), the
stage-A group beside EKH, on the round branch's tip `d19d803`. **4 functions
ours** (`src/game/effect_gte.cpp`, declarations in `src/game/effect_gte.h`,
shadow name `effect_gte`): the cut's four rows for EGT
(`analysis/round13_cut.tsv`), each read to its last instruction with capstone
and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
80,000 rounds, 0 mismatches. 41 controls planted one at a time: 38 refused,
3 equivalent mutants not refused, each with a near variant refused (section
6). Two of the four are entered by the whelpBoss route (section 9).

| Function | Entry | Bytes | PSX twin | Call sites (`E8`) |
|---|---|--:|---|--:|
| `EffectGte_LoadMapCamera` | `0x494060` | 0xAB | `0x801B0E10` | 113 |
| `EffectGte_ProjectPoint` | `0x494110` | 0x65 | `0x801B0EFC` | 266 |
| `EffectGte_SetDiagonalOne` | `0x494180` | 0x2F | `0x801B0F5C` | 7 |
| `EffectGte_ProjectSize` | `0x4941E0` | 0x91 | `0x801B0FD0` | 42 |

The twins are the cut's (call-anchored); the sibling names none of them
(`names/*.toml`, `symbols.toml`, its Ghidra export `GAME_EMI0_80196800`: all
`FUN_`), so every name here is from what the PC code does. The labelling
tool's "effect kind 186" for all four was a `hypothesis` by address: none is
a kind's state or handler - each is a helper called directly by the kinds
(and by areas 143, 146, 148, spells C2 and S32, and FC2's kind 0x30). All
four are taken.

## 1. What each function does

The world point `point` that three of them take is three dwords of the
engine's world space in the order **x, z, height**, as the callers' records
hold them (magic C2's streaks, S32's spiral, FC2's sparks); the height's
integer part is its high word.

- **`EffectGte_LoadMapCamera` `0x494060`** - the map camera into the GTE.
  `Gte_RotMatrix(Camera_Angles, M)` into a stack MATRIX; then (read after
  that call) the s16 vector `(MapView_FocusX >> 1, MapView_FocusZ >> 1,
  MapView_Elevation >> 1)` through `Gte_ApplyMatrix(M, it, V)`; then (read
  after that call) `M`'s translation = `(Camera_ShiftX + V.x, Camera_ShiftY +
  V.y, Camera_Distance + V.z + 0x1194)`, the three camera words s16
  (`movsx`); then `Gte_RotMatrix(Camera_Angles, M)` again (the angles
  re-read; `M`'s rotation is recomputed, its translation kept),
  `Gte_SetRotMatrix(M)`, `Gte_SetTransMatrix(M)`.
- **`EffectGte_ProjectPoint` `0x494110`** - a world point projected. The
  camera vector `((x >> 9) - 0x4000, (z >> 9) - 0x4000, -((height >> 16) /
  2))`, each to an s16 (arithmetic shifts; the halving toward zero,
  `cdq; sub eax, edx; sar eax, 1`); `Gte_RotTransPers(vector, out, ...)`
  writes the screen x and y as two floats to `out[0..1]`;
  `Gte_StoreDepthF(out + 2)` the depth / 16384 as a float. `point` is read
  whole before anything is written, so `out` may overlap it.
- **`EffectGte_SetDiagonalOne` `0x494180`** - nine word stores to a MATRIX's
  rotation: 1, 0, 0, 0, 1, 0, 0, 0, 1. The padding and translation are not
  touched. Its seven callers then premultiply it by `Gte_RotMatrixX`, `Y`
  and `Z`. **Not the GTE's identity** (0x1000): section 5.
- **`EffectGte_ProjectSize` `0x4941E0`** - a world-space size at a point's
  depth. The camera vector as `EffectGte_ProjectPoint` builds it,
  `Gte_RotTrans(vector, V, ...)`; with `depth = V.z`: `out[0] = size[0] *
  1000 / depth` (s16 `size`, `idiv`: toward zero, the quotient's low word
  stored), then `out[1]` from `size[1]` the same way - `size[1]` read after
  `out[0]` is written, so `out` may be `size` (FC2's `EffectKind30_SparkQuad`
  passes it in place).

## 2. Calling convention, arguments, answers

All four are `cdecl` and read only their stack arguments (`[esp + 4]`,
`+8`, `+0xC` at entry); no register argument. Every call out is relative and
leaves the function (`band_rows.py --clones`: 5, 2, 0, 1 sites), every jump
stays inside.

| Function | Arguments read | What eax holds at the `ret` |
|---|---|---|
| `EffectGte_LoadMapCamera` | none | the translation's z (`Gte_SetTransMatrix`, Capcom's `0x5A8E00`, ends `mov eax, [eax + 8]` on it) |
| `EffectGte_ProjectPoint` | `point` (3 dwords), `out` (12 bytes written) | `out + 2` (`Gte_StoreDepthF`, `0x5A9110`, leaves its argument in eax) |
| `EffectGte_SetDiagonalOne` | `matrix` (18 bytes written) | `matrix` (`mov eax, [esp + 4]` first) |
| `EffectGte_ProjectSize` | `point`, `size` (2 s16), `out` (2 s16 written) | the second quotient, all 32 bits |

Ours returns each of those values (the prototypes say `long`, `float *`,
`short *`, `long`), and the fuzz compares all 32 bits (`ret_mask`
`0xFFFFFFFF`). **No caller was found reading them**: a capstone scan of the
straight-line code after every `E8` site (scratch `egt/eaxuse2.py`) finds
eax written before it is read at all 7 sites of `0x494180` and all 42 of
`0x4941E0`; at 108 of 113 sites of `0x494060` and 250 of 266 of `0x494110`,
the rest reaching a branch or a `ret` first (not followed). So a caller may
declare them `void`; the values are kept because the original leaves them.

**The original's own argument slots.** `0x494110` hands
`Gte_RotTransPers` the addresses of its two argument slots as `p` and
`flag`: the depth-cue value lands in the slot that held `out` (the original
has `out` in `esi` by then); `flag` is never touched by the callee (its
`symbols.toml` entry). `0x4941E0` hands `Gte_RotTrans` its first slot as
`flag` (not touched either). The slots are the callee's under cdecl; ours
passes the address of its own `out` parameter as `p` (clang keeps it in the
incoming slot: `objdump`, `leal 0x18(%esp)`), which a caller could only see
by reading its outgoing arguments after the call. Control C38 below.

**Uninitialised stack the callees never read.** The fourth word of each
stack SVECTOR, and the MATRIX translation before `0x494060` writes it, are
never written; `Gte_ApplyMatrix`, `Gte_RotTransPers` and `Gte_RotTrans` read
three words, `Gte_RotMatrix` writes the rotation and padding (20 bytes, from
`Gte_IdentityRotation`) before `Gte_ApplyMatrix` reads them.

## 3. The fuzz (`effect_gte_fuzz.cpp`)

`scenario_harness::Run` with the four as `Shape::kCall` clones (field mode),
`Clone::pointers` making each pointer argument a scratch buffer
(`Scratch(i)`), `ret_mask` `0xFFFFFFFF` each, **20,000 rounds per function**.
`BOF3X_EGT_ONLY=<name>` runs one (the controls).

**The callees.** Each of the seven GTE callees is a recorder of the group's
own that logs what it is handed and then, in its `effect`, **calls the real
function** (ours, proven against Capcom's by the `psx_gte*` fuzzes) on those
arguments and answers what it answers - or, for `Gte_SetTransMatrix` and
`Gte_StoreDepthF`, what Capcom's leaves in eax (section 2). So a wrong
vector shows in the log, what it leads to lands in the GTE registers and the
outs (both compared), and the harness's disturbance, which runs in the
recorder before the effect, tests when each cell is read. Masks and derefs:

| Callee | Logged |
|---|---|
| `Gte_RotMatrix` (angles, matrix) | the angles' address and 6 bytes; the matrix not (a stack address, not yet written) |
| `Gte_ApplyMatrix` (matrix, vector, out) | 20 bytes of the rotation and padding, 6 of the vector |
| `Gte_SetRotMatrix`, `Gte_SetTransMatrix` (matrix) | 20 and 32 bytes |
| `Gte_RotTransPers` (vector, sxy, p, flag) | 6 bytes of the vector, `sxy` by value (a scratch address, the same on both passes) |
| `Gte_StoreDepthF` (out) | `out` by value |
| `Gte_RotTrans` (vector, out, flag) | 6 bytes of the vector |

A stack address itself is never logged (it differs between the clone's
frame and ours). The harness's standard rows for the same callees
(`kField`) are not used: the group's listing is registered first and stands.

**The state.** Field mode's standard regions (which hold `Camera_Angles`,
`MapView_FocusX` / `Z` / `MapView_Elevation` and `Camera_Distance`) and the
group's three: the GTE registers `0x7DE420..0x7DE500` and
`0x7DE780..0x7DE7A8` (the matrices, the vertices, the view-space vertex, the
near plane, the screen and depth FIFOs, the projection distance, the
offsets, the ramps), and `Camera_ShiftX` / `Y` `0x903800`. 22,832 bytes, 39
regions, compared with the log after both passes.

**The seeds** (after the harness's random fill; `Seed(k)`):

- GTE: half the time a rotation of moderate entries (0x1000, 0, -0x1000,
  +-0xB50, +-0x1000 at random), a translation from 0 / +-0x1000 / the dword
  limits / random, plausible projection registers.
- `LoadMapCamera`: each angle 0, 0x400, 0x800, 0xC00, 0xFFF, 0x1000, 0xF800
  or random; the focus dwords 0, 1, 2, 0xFFFF, 0x10000, 0x1FFFE, 0x1FFFF,
  the dword limits, -1 or random (bit 16, which the `>> 1` moves into the
  s16's sign); `Camera_ShiftX` / `Y` 0, 1, -1, 0x7FFF, -0x8000, 0x40,
  -0x40, 0x1000, small or random; `Camera_Distance` 0, 0x7FFF, -0x8000,
  -0x1194 and -0x1195 (the constant's sum at 0 and -1), -1, 0x200, random.
- The point (`ProjectPoint`, `ProjectSize`): x and z where `(v >> 9) -
  0x4000` is 0, -1, 0x4000, 0x7FFF, -0x8000, the dword limits, 0 / -1, a
  window of +-0x10000 around 0x800000, random; the height with its integer
  part odd and negative (-1, -3: where toward-zero halving and a shift
  differ), even, 0, 0x7FFF, -0x8000 (`0x80000000`), small +- or random.
- `ProjectSize`: the size words 0, 1, -1, 0x7FFF, -0x8000, 0x40, -0x40,
  0x1000, small, random; **the depth is never 0** (the original's fault,
  ours' abort): the seed computes it with its own copy of the vector
  (`CameraVectorOf`, so a plant in ours cannot steer it) and `Gte_ApplyMatrix`,
  and moves the translation's z off 0; half the time onto 1, -1, 2, -2, 7,
  1000, -1000, 64000, 0x7FFF, 0x10000, the dword limits or small - where the
  quotient overflows 16 bits and where it rounds.
- The arguments (`Args`): `ProjectPoint`'s out apart, over the point, or a
  dword into it; `SetDiagonalOne`'s matrix at any word of the buffer;
  `ProjectSize`'s out apart, **in place over the size** (twice as often),
  a word either side of it, or over the point.

**The disturbance** (the group's case of the harness's sixteen): an angle,
`Camera_ShiftX`, `Camera_ShiftY`, `Camera_Distance`, one focus dword, a
dword of the point or a word of the size - the cells the four read before
or after a call.

**Result** (2026-09-29, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=effect_gte`, exit 0): 80,000 rounds over 4 functions, 160,000
calls to the stand-ins, **0 mismatches**; coverage `Gte_RotMatrix` 40,000,
the other six 20,000 each. `BOF3X_SHADOW='*'`: exit 0, `inject: 6895 ours,
0 left original`, every self-test 0 mismatches (among them `magic_c2`,
`magic_s32`, `area_w3f`, `area_w3g`, `field_c2`, whose raw keys stand for
these four: section 8).

## 4. Divergence

None. The four are faithful; `DIVERGENCE.md` and `cheats.cpp` name no byte
in `0x494060..0x494270` (grep, 2026-09-29). The one behaviour ours does not
reproduce is the divide fault of section 5, where ours aborts with a message
(the project's rule for a fault, not a choice of behaviour).

## 5. Latent defects (Capcom's, described, not fixed)

- **`EffectGte_SetDiagonalOne` writes 1, not 0x1000.** (D215) The PSX twin
  `0x801B0F5C` begins `addiu v0, zero, 0x1000` and stores it on the
  diagonal (read in the sibling's generated overlay code,
  `overlays_static_0170.c`); the PC port stores 1. In the GTE's 4.12 fixed
  point that is 1/4096 of the identity, and its seven callers (E1C's
  `0x46EA80`, Capcom's `0x46F570`, E2E's `0x47B070`, E3A's `0x482240`, E3C's
  `0x4851E0`, E4D's `0x48D860`, E4E's `0x4906F0`) premultiply it by
  `Gte_RotMatrixX` / `Y` / `Z` (`MulMatrix0`: each element `>> 12`), so the
  matrix they build collapses to elements of 0, 1 and -1 - whatever those
  effects draw with it would be squashed onto their translation. Ours
  reproduces the 1s (control C22 refuses 0x1000). No recorded route reaches
  any of the seven (section 9); what the effect looks like on the PC is for
  the owner's eye, and a fix would be a divergence.
- **`EffectGte_ProjectSize` divides by the depth unchecked** (D216): a point at
  camera depth exactly 0 faults (`idiv`; no handler, the process ends). Ours
  aborts with a `Fatal` naming the point. No recorded route enters
  `0x4941E0`; whether play ever puts a point on the eye's plane is not
  established.
- **`EffectGte_ProjectSize` keeps the quotient's low 16 bits** (D216): near the
  eye, `size * 1000 / depth` passes 0x7FFF and wraps (size 0x40 at depth 1:
  64,000, stored as -1,536). Reproduced (control C33 refuses a saturating
  version).
- **Ranges, not defects**: the focus words wrap into an s16 after `>> 1`
  (focus dwords past 0xFFFF), the point's x and z after `>> 9` (past
  0x1000000), the height's integer part is halved and its fraction dropped.
  Reproduced; the seeds cross each wrap.

## 6. Controls

`egt/controls.py` (scratch): each plant replaces a string that occurs
exactly once in `effect_gte.cpp`, rebuilds, runs the self-test on the one
clone it touches, restores the file and rebuilds. **41 planted: 38
refused, 3 not refused - each an equivalent mutant with its near variant
refused.** The count is the rounds that mismatched of 20,000.

| # | Function | Plant | Refused |
|---|---|---|--:|
| C01 | LoadMapCamera | 0x1194 -> 0x1195 | 20,000 |
| C02 | LoadMapCamera | `Camera_ShiftX` zero-extended | 7,116 |
| C03 | LoadMapCamera | the y translation from the turned x | 17,580 |
| C04 | LoadMapCamera | `Camera_Distance` zero-extended | 11,274 |
| C05 | LoadMapCamera | `MapView_FocusX >> 2` | 11,748 |
| C06 | LoadMapCamera | the focus shifted arithmetically (`sar` for `shr`) | **not refused**: equivalent - both shifts give the same bits 0..30, and only the low 16 are stored. Near variant C41 refused |
| C07 | LoadMapCamera | the focus's y from `MapView_Elevation` | 16,139 |
| C08 | LoadMapCamera | the focus read before the first `Gte_RotMatrix` | 109 (by the disturbance) |
| C09 | LoadMapCamera | `Camera_ShiftX` read before `Gte_ApplyMatrix` | 98 (by the disturbance) |
| C10 | LoadMapCamera | the second `Gte_RotMatrix` left out | 20,000 |
| C11 | LoadMapCamera | `SetTransMatrix` before `SetRotMatrix` | 20,000 |
| C12 | LoadMapCamera | answers the translation's x | 19,857 |
| C13 | ProjectPoint | the height halved by a shift (floor), not toward zero | 7,648 |
| C14 | ProjectPoint | x less 0x3FFF | 20,000 |
| C15 | ProjectPoint | z `>> 8` | 16,064 |
| C16 | ProjectPoint | the height not negated | 14,477 |
| C17 | ProjectPoint | x from z | 17,884 |
| C18 | ProjectPoint | x shifted logically | **not refused**: equivalent - the shifts differ only in bits 23..31 of the result, and the low 16 are stored. Near variant C39 refused |
| C19 | ProjectPoint | the depth stored at `out + 1` | 20,000 |
| C20 | ProjectPoint | answers `out + 1` | 20,000 |
| C21 | ProjectPoint | the height `>> 15` | 16,672 |
| C22 | SetDiagonalOne | the middle one 0x1000 (the GTE identity) | 20,000 |
| C23 | SetDiagonalOne | the padding word written too | 20,000 |
| C24 | SetDiagonalOne | answers `matrix + 1` | 20,000 |
| C25 | SetDiagonalOne | one off-diagonal word not written | 20,000 |
| C26 | ProjectSize | scaled by 1024 | 8,225 |
| C27 | ProjectSize | the depth the turned y | 14,867 |
| C28 | ProjectSize | `out[1]` from `size[0]` | 11,163 |
| C29 | ProjectSize | `size[0]` zero-extended | 3,320 |
| C30 | ProjectSize | answers the first quotient | 10,767 |
| C31 | ProjectSize | `size[1]` read before `out[0]` is written | 1,920 (the in-place rounds) |
| C32 | ProjectSize | divided by `depth \| 1` | 2,857 |
| C33 | ProjectSize | the quotient saturated to an s16 | 2,228 |
| C34 | ProjectSize | `size[0]` read before `Gte_RotTrans` | 35 (by the disturbance) |
| C35 | ProjectSize | the vector's x and z swapped | mismatched from its first rounds (logged), then ours' depth-0 abort ended the run (the seed steers the clone's depth, not the mutant's) |
| C36 | LoadMapCamera | `Camera_Angles` copied once before the first call | 310 (by the disturbance) |
| C37 | LoadMapCamera | `Camera_Distance` read before `Gte_ApplyMatrix` | 122 (by the disturbance) |
| C38 | ProjectPoint | the depth cue into a local, not `out`'s argument slot | **not refused**: equivalent to the fuzz and to every caller that does not read its outgoing arguments (section 2); nothing the harness compares holds the slot. Near variant C40 refused |
| C39 | ProjectPoint | x `>> 10` (C18's near variant) | 13,984 |
| C40 | ProjectPoint | the depth cue into `out[3]` (C38's near variant) | 20,000 |
| C41 | LoadMapCamera | the focus through an s16 before the shift (C06's near variant) | 5,058 |

The thinnest are the read-order controls (C08, C09, C34, C36, C37), refused
only by the disturbance moving a cell between the calls: 35..310 rounds of
20,000.

## 7. Calls across groups

Every caller in the cut calls these by name from now on. `band_rows.py
--byte-tables --group EGT` lists who reaches them: this round's groups (E1C,
E2A, E2B, E2E, E3A, E3C, E4D, E4E and more), Capcom's raw callers, and ours
(`Area143_DrawGlowCylinder`, `Area146_DrawGlowCylinder`, `Area148_DrawBeam`,
`EffectKind30_SparksDraw`, `EffectKind30_SparkQuad`, magic C2's and S32's
draws). The `E8` counts are the table at the top. For the 35 groups:

- **Include `game/effect_gte.h`** and call `SH_CALL(EffectGte_...)`; the
  answers may be ignored (section 2).
- **A stand-in** for them in a group's fuzz may call the real function as
  this group's effects do (they are deterministic in the GTE registers and
  the camera cells), or log and fill: `EffectGte_ProjectPoint` writes 12
  bytes at `out` (two floats, a float depth), `EffectGte_ProjectSize` 4 at
  `out`, `EffectGte_SetDiagonalOne` 18 at `matrix`; `LoadMapCamera` writes
  `Gte_Matrix` (0x7DE4A0, 32 bytes).
- **`EffectGte_ProjectSize` aborts at depth 0**: a group whose fuzz calls
  the real function must keep its point off the eye plane (this file's
  `SteerDepth`), or stand in for it.
- **Not taken, not in the cut: `0x4941B0`** (43 bytes, catalog part 2,
  between `0x494180` and `0x4941E0`): three pointers to projected vertices
  (floats) a, b, c: `(c.y - b.y)(b.x - a.x) - (b.y - a.y)(c.x - b.x)`, the
  cross product's z of the edges `a->b`, `b->c`, in x87, tail-jumping `_ftol` `0x5B9550`; its three callers
  (`0x476C67`, S32's `0x4EA85A` as `kFacing`, `0x57F21F`) test `ax` for a
  back face. A fifth helper of the same run, for the coordinator.

## 8. The rebinding

`band_rows.py --refs --group EGT` and `grep -rn -i -E
"0x(494060|494110|494180|4941E0)" src/game`. Rebound to the names, the
value unchanged (`bof3::addr::EffectGte_*`), one line each:

| File | Constants |
|---|---|
| `area_w3f_callees.h` | `kSetMapCamera`, `kProjectPoint` (and `#include "bof3/symbols.gen.h"`, which it lacked) |
| `area_w3g_callees.h` | `kSetMapCamera`, `kProjectPoint`, `kScreenSize` |
| `field_c2_callees.h` | `kCameraMatrices`, `kProjectPoint`, `kProjectSize` (and the include) |
| `magic_c2.cpp` | `kSetMapCamera`, `kProjectPoint` |
| `magic_s32.cpp` | `kSetMapCamera`, `kProjectPoint` |

Ours in those files still calls through `SH_AT` / `AH_AT` / `MH_AT` with the
constant, so the fuzz keys stand. **Left raw, on purpose**: the fuzz files'
`CallSite` tables (the disassembly's targets) and the key rows that list
them by address - `magic_c2_fuzz.cpp` `C2_RAW(0x494060)`, `C2_RAW(0x494110)`;
`magic_s32_fuzz.cpp` `S32_RAW(0x494060)`, `S32_RAW(0x494110)`; the
`area_w3f_fuzz.cpp`, `area_w3g_fuzz.cpp` and `field_c2_fuzz.cpp` rows keyed on
the rebound constants. A row whose key equals its address registers for
Capcom's or ours alike, so **no row broke** (`'*'`: section 3); and the
comments that call the four "nobody's". **For the coordinator**:
`scenario_harness.cpp` (EKH's file) has three raw rows in `kField`, `"0x494060"`,
`"0x494110"`, `"0x4941E0"` (FC2's, lines 629..631 at `d19d803`) - still valid
as keys, stale as "nobody's". The `symbols.toml` evidence strings of other
entries that cite the addresses are left as they are.

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract,
shop, worldmap, combat) are empty for all four. The first-call traces:
`analysis/calltrace/reach_whelp` (all original) enters **`0x494110` at frame
2,235** (from `0x464175`) and **`0x494060` at frame 11,782** (from
`0x46DA2C`, in E1C's `0x46DA20`); `reach_dragon` enters none. The counted
traces `hash_whelp_*` / `hash_w2_whelp_*` agree: `0x494060` 159 calls, all
from `0x46DA2C`; `0x494110` 10,207, from `0x464175` (2,176), `0x46DA84`,
`0x46DAA8` (159 each), `0x46DB5E`, `0x46DB82` (2,544 each) and `0x471E38`
(2,625). No trace enters `0x494180` or `0x4941E0`: those two are fuzz-only.
The coordinator's frame hash on the whelpBoss route covers the other two
after the merge.

## 10. For `analysis/calltrace/entries_logic.txt`

The main checkout's file already holds the four with the extents read here
(`00494060 AB`, `00494110 65`, `00494180 2F`, `004941E0 91`); nothing
appended.
