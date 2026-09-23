# The kind-2 object's script runner

**Status:** IN PROGRESS (2026-09-22) - the runner, its dispatcher and its
five states are ours (`src/game/kind2_object.cpp`), and so is
`AreaMap_Slope` `0x5722D0` (`src/game/area_slope.cpp`, section 7); each
fuzzed against a clone of Capcom's with negative controls at start-up
(`BOF3X_SHADOW=kind2_object`, `=area_slope`). **Not yet through the batch
check in game** - that runs centrally after the merge.

Group B of the parallel takeover of the field's frame loop
([`HANDOFF.md`](HANDOFF.md) "Pick up here", target 1). The script it runs is
the movement script of [`movement-script.md`](movement-script.md); this is
its second caller after the field objects' update.

## 1. What the kind-2 object is

`Sprite_Kind2` `0x7E0940`, one `0xA4`-byte object - the one
`MoveScript_ObjectKind` calls kind 2. What the evidence says it is, in
order of strength:

- It carries a movement-script context at `+0x80` (flags `+0x80`, wait
  `+0x81`, script number `+0x83`, speed `+0x84`, timed-move steps `+0x87`),
  run by `Kind2_Script` with the area descriptor's `+0x1C` script table -
  the field objects use `+0x10`.
- Its x and z are mirrored into `Field_Kind2X` / `Field_Kind2Z`
  (`0x905E64` / `0x905E60`), and those are what `Field_ViewReset` `0x56F670`
  centres the view on: `MapView_Origin` from their high words (`- 0x18`,
  `+ 3`), `0x929F14` / `0x929F18` from them (disasm to `0x56F805`).
  `AreaMap_Frame` `0x56E6C0` reads them every frame.
- Its elevation (the 16.16 long `+0x3C`) goes to `MapView_SetElevation`
  every frame it moves.
- On the PSX the pair is `0x80149304` / `0x80149308`, which the sibling's
  notes call the camera in the battle escape code and measure sound
  distances against (`SE_PlayNearSelf`, the sibling's `SOUND_CUES.md`).

So, as a **hypothesis**: the kind-2 object is the field's camera focus, an
invisible object the area scripts move and glide so that the view follows.
Nothing here was checked in game. (The PC keeps x above z - `0x905E64` is
x - where the PSX keeps x first; a porting-house layout change, harmless.)

## 2. The runner

| | PC | PSX (`GAME.EMI` §0) | size | hidden_b calls | |
|---|---|---|---|---|---|
| `Kind2_Run` | `0x573080` | `FUN_801C67EC` | `0xE` | 12,165 (9,168 from `0x517205`, 2,997 from `0x51724F`) | **ours** |
| `Kind2_Dispatch` | `0x573090` | `0x801C6830` | `0xE` | 8,201 | **ours** |
| state 0 `Kind2_Script` | `0x5730A0` | `FUN_801C686C` | `0x207` | not traced | **ours** |
| state 1 `Kind2_Move` | `0x5732B0` | `0x801C6BB0` | `0x6C` | 6,269 | **ours** |
| state 2 `Kind2_WaitRequest` | `0x573320` | `0x801C6C74` | `0x1D` | not traced | **ours** |
| state 3 `Kind2_Travel` | `0x573340` | `0x801C6CAC` | `0x6F` | not traced | **ours** |
| state 4 `Kind2_Lift` | `0x5733B0` | `FUN_801C6D60` | `0x46` | 195 | **ours** |

Call counts from `analysis/calltrace/hidden_b/bof3x.callcounts.tsv`; three
states are not in the tracer's entry list (they are reached only through the
table or a tail jump), so their counts are unmeasured - 8,201 dispatches less
6,269 and 195 leaves 1,737 for states 0, 2 and 3 together. The PSX twins:
`pairs_propagated.json` pairs `0x573090` with `FUN_801C686C` by callers; read
side by side, `FUN_801C686C` is state 0, and the PSX's own table (below) gives
the rest. Each was compared instruction by instruction against the PSX MIPS
(`GAME_EMI0_80196800` from the sibling's Ghidra folder, capstone); the only
difference is that the PC's state 1 tail-jumps into state 0 where the PSX
calls it.

`Kind2_Run` is `jmp [0x6632AC + k[1] * 4]`, `Kind2_Dispatch` is
`jmp [0x6632B4 + k[2] * 4]` ("k +n" is byte n of `Sprite_Kind2`).

## 3. The tables, and what the index bytes can hold

The two tables are one run of `.data` (dumped from `BOF3.exe`, 2026-09-22):

| address | entry | |
|---|---|---|
| `0x66329C`..`0x6632A8` | `0x571B40` `0x571BE0` `0x571D30` `0x571E20` | `AreaMap_HeaderPass`'s handlers (group D) |
| `0x6632AC` | `0x437CC0` | `Kind2_RunTable` [0] - a bare `ret` |
| `0x6632B0` | `0x573090` | [1] - `Kind2_Dispatch` |
| `0x6632B4`..`0x6632C4` | `0x5730A0` .. `0x5733B0` | `Kind2_StateTable` [0..4] |
| `0x6632C8` | `0` | |
| `0x6632CC`.. | `0x00030202` ... | bytes, not addresses |

So a run byte of 2..6 would run a state directly, skipping the dispatch; 7
jumps to address 0; 8 and up into data. A state of 5 jumps to 0. **The PSX has
exactly the same layout** at `0x801CDCF4`: `jr ra`, the dispatcher, the five
states, 0 - the compiler laid out two adjacent switch tables, and the port
copied the arrangement.

What the bytes hold: a capstone pass over every function of `pc_funcs.json`
for direct stores (2026-09-22) finds `k +1` written only with 0
(`0x495881`, `0x56AC7C`, `Kind2_Script`'s end) and 1 (`0x573544`, in
`0x5734F0`, which starts the object's script); `k +2` only with 0..4. Two
indirect writes are possible and not bounded by that pass: the movement op
`E5` copies `Sprite_Current[3]` into `Sprite_Current[1]`, and
`Kind2_Script` points `Sprite_Current` at the object before stepping - so an
`E5` in a kind-2 script copies `k +3` into the run byte; `k +3` is cleared by
both the start and the end, so it would copy a 0.

Ours indexes **the original's own tables in `.data`** by the whole byte, so
every out-of-range value goes exactly where it went in 2001, and every
in-range entry reaches ours through the detour at the original address.

## 4. The states

- **0, `Kind2_Script`.** A wait at `+0x81` is counted down first. Otherwise
  `Sprite_Current` is set to the object (and left so) and the script steps.
  Context bit 8 ends the script: run byte, state and `+3` to 0, speed 3.
  A result of `0xFF` with context bit 4 is a **glide** toward
  `Field_Kind2X` / `Z`: distance the larger of `|dx|`, `|dz|`; at 0 the bit
  is cleared; else `MoveScript_F3Divisor` = speed x 8, the ground's
  elevation there from `AreaMap_Elevation`, and the slope
  `+0x14 = (elevation long - ground << 16) / ((0x80 / F3Divisor) x
  frames)`, frames the high word of twice the distance; `MoveScript_FAWord`
  0, state 3, `Field_Kind2X` / `Z` set to the object's position, `+0x3E`
  to `MapView_Elevation`'s low word. A result of `0xFF` without bit 4: the
  view is reset on the object (`Field_ViewReset`), direction and state 0.
  Else context bit `0x20` gives state 2, bit `0x80` state 4, a timed move
  (`+0x87`) state 1 with its first frame of elevation, nothing state 0.
- **1, `Kind2_Move`.** While `Field_Kind2Hold`, the elevation long grows by
  the slope and goes to `MapView_SetElevation`. When the hold clears: unless
  context bit `0x40`, `MapView_SetElevation(AreaMap_Elevation(Field_Kind2X,
  Field_Kind2Z))`; steps and bit `0x40` cleared, state 0, and state 0 runs
  at once (tail jump).
- **2, `Kind2_WaitRequest`.** While `Field_Request` is 2, nothing; then state
  0 and context bit `0x20` cleared.
- **3, `Kind2_Travel`.** The glide: while the hold, a non-zero slope is
  added and set; when it clears, grounded as state 1 does (with no `0x40`
  test), slope 0, bit 4 cleared, direction 0, state 0.
- **4, `Kind2_Lift`.** For `+9` frames the elevation word `+0x3E` grows by
  the slope's low word; then state 0 and context bit `0x80` cleared.

Movement op `85` (in `MoveScript_Group8`, already ours) moves the object
toward ObjTrio 0 while `Field_Kind2Hold`; op `D0` places it at
`Field_Kind2X` / `Z`. Who sets and clears the hold was not read.

## 5. Quirks kept, and defects of the 2001 code

Kept, each commented in the source and each with a control (section 6):

- the dispatchers' unchecked whole-byte indexes (section 3);
- `Sprite_Current` left on the object;
- `|INT_MIN|` is `INT_MIN` in the distance (`cdq` / `xor` / `sub`);
- frames are the **signed** high word of twice the distance;
- `MoveScript_F3Divisor`, the elevation long and `MapView_Elevation` read
  back after `AreaMap_Elevation`, not kept from before it; the context flags
  read after the calls in states 1 and 3, and state 4's count read again
  after `MapView_SetElevation`;
- state 4's rise is 16-bit (word plus the slope's low word), where states 1
  and 3 add longs - the PSX's `FUN_801C6D60` is 16-bit too;
- `MapView_SetElevation` is given whatever dword the register held; it uses
  the low 16 bits only, which is all the recorder compares.

**Defects**, written down and not fixed (a fix is the owner's call and a
[`DIVERGENCE.md`](DIVERGENCE.md) entry):

- **K1. The glide divides by zero when the object is nearly there.** A
  distance of 1..`0x7FFF` - under half a unit on the larger axis - makes
  frames 0 and the second `idiv` faults. So does a speed index whose speed
  is 0 (index 0; also 6 and 7, past `Field_MoveSpeeds`' six bytes) - the
  first `idiv` - and a speed above 16 (index 8 reads `0x40`), whose quotient
  is 0. The PSX has the same arithmetic with compiled-in `break` traps
  (`trap(0x1c00)` in the decompilation), so it is the source's, not the
  port's. Latent: whether a shipped kind-2 script ever glides from within
  half a unit, or sets such a speed, was not measured. Ours faults at the
  same instruction of the same computation (`Idiv`, inline `idiv`). D12 in
  [`known-defects.md`](known-defects.md).
- **K2. A glide of 0x4000 units or more** (distance `>= 0x40000000`) counts
  negative frames and the slope's sign flips. Unreachable on any map this
  size; noted for completeness.
- **K3. Unchecked dispatch** (section 3): a run byte of 7 or a state of 5
  jumps to address 0. No store in the exe produces either.

## 6. The fuzz, its reach, and the controls

`BOF3X_SHADOW=kind2_object`: seven byte-copies; the five states' calls
re-aimed at recorders (state 1's tail jump too, so each state is tested
alone), the two dispatchers' `jmp [eax*4 + table]` disp32 re-aimed at a table
of seven recorders laid out as the original's pair (state table two entries
into the run table); ours given the same through its `Callees`. The
recorders log their arguments (`MapView_SetElevation`'s low 16 bits) and
change what the caller reads after them: the step the context flags, speed
(kept 1..5), steps, slope, elevation, and the position (object and
`Field_Kind2X` / `Z` shifted together, so that the distance stays clear of
K1); the ground the elevation long, `MapView_Elevation`, the divisor (kept
1..`0x80`), the flags. Seeded: every run byte 0..6 and state 0..4, wait 0,
the hold, bit `0x40`, zero slope, zero count, `Field_Request` 2, and the
glide's distance - 0, or `0x8000`..`0x7FFF7FFF` on the larger axis with
edges `0x8000`, `0xFFFF`, `0x10000`, `0x3FFFFFFF`, `0x40000000`,
`0x7FFF7FFF`, the smaller axis 0, equal or below. Never generated: the
faulting inputs of K1, frames of -1, both axes `INT_MIN`.

The start-up line, 2026-09-22:

```
shadow      kind2_object self-test: 40000 rounds (16000 of Kind2_Script, 4000 of each other), 38586 calls to the stand-ins, Kind2_Script's branches wait 2707 / end 5046 / glide 1047 (negative frames 297) / there 429 / view reset 1798 / to state 2 2357, 4 1139, 1 687, 0 493; 0 MISMATCHES; Sprite_Kind2, Field_Kind2X / Z, the hold, the divisor, FAWord, MapView_Elevation, Field_Request, Sprite_Current and the stand-ins' log compared
```

**Negative controls** (each planted in ours, built, self-tested, reverted;
`exit 3` is the launcher's Fatal): 39 run, 36 refused by comparison and one
by a fault; the other three change nothing.

| control | result |
|---|---|
| run byte 2+ ignored / state masked `& 3` | refused (2,831 / 787 mismatches) |
| wait not counted down; `Sprite_Current` not set; end keeps `+3`; end speed 2; flags read before the step | refused (2,707; 6,556; 5,028; 5,046; 5,580) |
| frames unsigned (the signed high word) | refused (297) |
| frames odd (`| 1`) | refused (524) |
| frames from the distance, not twice it | refused **by a fault**, not by comparison (exit `0xC0000005`; this control makes frames 0 for distances under `0x10000`, where the division must fault) - the "frames odd" control above is its comparison-refused replacement |
| divisor kept from before the ground call | refused (222) |
| `MapView_Elevation` read before the call and used | refused (321) |
| elevation long read before the call | refused (427) |
| `FAWord` not cleared; "there" keeps bit 4; glide x not re-read; reset clears `+8` before the call; reset without the x store | refused (1,344; 429; 1,203; 883; 1,187) |
| bit `0x80` tested before `0x20`; no steps leaves the state; the timed move's first frame 16-bit | refused (1,180; 394; 486) |
| state 1: bit `0x40` ignored; flags read before the calls; no tail step; steps not cleared; hold adds 16-bit | refused (1,000; 317; 2,022; 1,129; 1,317) |
| state 2: `& 0xCF` for `& 0xDF`; waits on request 1 | refused (997; 1,852) |
| state 3: zero slope still set; slope not cleared; flags read before the calls | refused (612; 1,346; 635) |
| state 4: long add; count read before the call; end keeps bit 7 | refused (1,326; 508; 1,017) |
| the distance's tie picks `dz` (`>` for `>=`) | **not refused - changes nothing** (equal values) |
| the ground's word shifted as unsigned (`movzx` for `movsx`) | **not refused - changes nothing** (the sign bits are shifted out by `<< 16`) |
| `MapView_Elevation` read early but unused | **not refused - changes nothing** (a faulty control; the "used" one above replaces it) |

What the fuzz does not reach: the out-of-range table indexes (7+ would jump
to 0 - not generated), the faulting inputs of K1 (not generated; ours faults
by construction, `Idiv`), and the real callees - `MoveScript_Step`,
`AreaMap_Elevation`, `MapView_SetElevation` have their own fuzzes,
`Field_ViewReset` is Capcom's. The batch check in game is the test of the
whole.

## 7. `AreaMap_Slope` `0x5722D0`

`AreaMap_Elevation`'s entry said it was the same function with a store to
`0x903850`. Read to its end (2026-09-22, `0x2A0` bytes, no calls, every jump
internal) it is not: it shares the grid and cell lookup and the corner sums,
but returns **how steep the ground is**, and stores in scratch byte 0
(`DamageScratch`, the PSX scratchpad's first byte) 1 for sloped and 0 for
flat or outside the grid. The three callers read (`0x46B611`, `0x518784`,
`MoveCmd_Move` `0x578CC3`) test that byte straight after the call.

- whole cell: flat, the elevation formula;
- one half: flat when `b == e` and `a == d` (then `(a + b) << 4`), else
  `|(a + b) - (d + e)| << 4`;
- both halves: four corner sums `p0..p3`, flat when equal (`p0 << 5`), else
  by the third argument's low byte `n`, `i = n >> 1`:
  `|p[i] - p[(i - 2) & 3]| << 5`, the larger of `p[i]` and `p[(i + 1) & 3]`
  for odd `n`.

Quirks kept: it **uses its caller's argument slots as scratch** (x and y
masked to `0xFFFF8000`, then y's bytes hold the edge bytes or `p0..p3`), and
**`p[i]` is not masked** - for `n >= 8` it reads the direction dword's own
bytes, for `n >= 16` the caller's stack. So ours reads and writes the
arguments where the caller pushed them, through `__builtin_frame_address(0)`
(the prologue checked in the built DLL: `push ebp; mov ebp, esp`, arguments
from `8(%ebp)`). Callers: `0x518760` passes `Sprite_Current[8] & 7`;
`MoveCmd_Move` its direction argument; `0x46B611` / `0x46B66B` the `+8` byte
of the object in `eax`, unmasked - a sprite's direction byte there may carry
bit 3 (as `Field_ObjectUpdate` sets it), so `n` of 8..15 is plausible in game,
reading the dword's own bytes; `n >= 16` was not seen. Whether that is a
defect depends on what the callers mean by it; recorded, not judged.

PSX twin `0x80155578` by `psx_pair` (call-anchored); its body is not in the
sibling's boot decompilation and was not read. Call trace hidden_b: 4,612
calls (2,481 from `0x518784`, 2,131 from `0x578CC3`) - **reached by the
attract cycle**, contrary to the handoff's note.

`BOF3X_SHADOW=area_slope`: 131,072 rounds, both called through `CallFramed`,
a naked trampoline that lays a 160-byte block out as the stack at the call -
the three arguments, then 148 bytes of "caller's frame" - and copies it back,
so the scribbled slots and the bytes read past them are inputs and compared
outputs. Areas re-drawn every 256 rounds: random; one byte everywhere (all
flat) with a few disturbed; or corner dwords of one four-byte pattern with
two pairs equal and scale 0, where one half's edges are level but not with
each other - without that kind, the control that crosses the flat test's
pairing was not refused.

```
shadow      area_slope self-test: 131072 rounds (76637 outside the grid; whole cells 13651; one half 13256 flat / 13976 sloped; both halves 4296 flat / 9256 sloped, 5199 of them with a corner index past the four), 0 MISMATCHES; eax, scratch bytes and the 160 bytes of argument slots and caller's frame compared
```

Controls: 19 run, 18 refused - outside returns 0 (76,220) / keeps the flag
(76,326); x slot not written (54,434); y slot not masked (40,789); edge bytes
not written (27,199); corner index masked `& 3` (4,247); odd case unsigned
max (2,009) / ignored (1,926); whole cell height once (8,058); the flat
test's pairing crossed (4,612); halves swapped (18,619); four-corner flat
test short one (19); opposite corner `i - 1` (6,774); four flat `<< 4`
(3,733); direction byte 7 bits (479); edge flag left 0 (13,919); next height
from `h[0]` (4,033); height product untruncated (7,819). Not refused:
`(i + 2) & 3` for `(i - 2) & 3` - the same value.

## 8. For the batch check

New functions for `--original`: `Kind2_Run,Kind2_Dispatch,Kind2_Script,
Kind2_Move,Kind2_WaitRequest,Kind2_Travel,Kind2_Lift,AreaMap_Slope`. All are
reached by the attract cycle as far as the tracer says (`Kind2_Run`,
`Kind2_Dispatch`, `Kind2_Move`, `Kind2_Lift`, `AreaMap_Slope` counted;
`Kind2_Script` necessarily, as the only way into state 1; states 2 and 3
unmeasured - the new-game capture may be what reaches them).
