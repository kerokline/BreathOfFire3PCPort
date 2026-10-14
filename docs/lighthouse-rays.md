# The lighthouse's light rays (D243): a matrix of ones, not the identity

**Status:** MEASURED (2026-10-14) - group LR of the ultrawide round, branch
`wide/lr` off `a80f30a5`. The cause is found, the fix is built behind a switch
(DIV-LR-A below, for the coordinator to number), self-tested and recaptured.
**Not seen by the owner.**

The owner's report (known-defects D243, confirmed 2026-10-13): on
`tools/recipes/eldritchGene.txt` (save `eldritchGene`, `--lang en-US`), once
the lighthouse switch is thrown, the PlayStation draws long translucent rays,
white to yellow and cyan over the sky, fanning out of the globe on the
pedestal over the whole scene. The PC draws only the globe's glow, then the
white-out. (The "faint diagonal beam" at the old capture's `f01380` is another
effect: kind 0x4A's trail after the flying faerie, section 2.)

## 1. The answer in one paragraph

The rays are **effect kind 0x4B** (`EffectKind4B_*`, group E2E's): 32
semi-transparent `POLY_G3` wedges from one point at map cell (0x73.8, 0x0B.8)
- the globe - each turned 0x10 a frame, shade (v, v, v / 2) at the centre and
black at the two far corners, drawn additively (draw mode page (0x380, 0x100),
rate 1) for 0xB9 frames. Each wedge's two edge vectors are
`(Math_Cos(+-0x20), Math_Sin(+-0x20), 0)`, about 4,096 long, turned by a
random rotation built on `EffectGte_SetDiagonalOne`'s matrix. **Capcom's PC
`EffectGte_SetDiagonalOne` writes 1 on the diagonal where the PlayStation twin
writes 0x1000** (known-defects D215, read in round thirteen and never seen in
play until now): the GTE's `>> 12` products then collapse the rotation to
0 / 1 / -1, every edge becomes 0 or -1, and all 32 wedges are drawn as points
on the globe. Everything else on the path - the kind's code, the debris draw,
the draw mode, the semi-transparency, the blend, the ordering slot - is the
same on both machines. **The port's defect, a restoration**: one constant.

## 2. Finding the effect

- **The area.** `Game_AreaNumber` (`0x904EFC`) reads 77 on the route from
  `Frame_Counter` 318 (the live watch below) - "the lighthouse, inside"
  (`../BreathOfFire3Recomp/names/places.toml`, id 77); the round-ten band doc
  is [`area_w2a.md`](area_w2a.md).
- **The live effect pool.** A read-only watcher of `Effect_Objects`
  (`0x7E11E0`, 20 records of 0x80; scratch `uw_lr/effwatch.py`, which plays
  the recipe through `tools/input_run.py` and reads only the PID the
  launcher reports) logged every change of the in-use records with
  `Frame_Counter`:

  | Frame_Counter | What is alive |
  |---|---|
  | 319.. | six kind 0x18 records: five sub-kind 5 (the CLUT pulses, E5A - the globe's glow) and one sub-kind 0x31 (a sliding floor quad, E6A) |
  | **872..1057** | **kind 0x4B** - the switch to the flash |
  | 1193.., 1459.. | kind 6 records, kind 0x13 (the camera turn) |
  | 1334..1514 | kind 0x4A (the faerie's trail: the "faint beam") |
  | 1745..2045 | kind 0x4C (sparks) |

  Kind 0x4B's frames are the rays' window: the recipe's frames run about
  equal to `Frame_Counter` here (both 318 at the area's entry), and the
  white-out follows at recipe frame ~1020.
- **The PlayStation's kind 0x4B.** `Effect_KindHandlers`' twin
  `0x801C8C68` (JP `GAME.EMI`) points entries 0x4A..0x4C into the area
  overlay - `BIN/WORLD02/AREA077.EMI`, code section at `0x801F2C00`:
  kind 0x4B at `0x801F3E04`, its states `0x801F3E48` (start) and
  `0x801F3F08` (scatter), the per-wedge init `0x801F4A68` and draw
  `0x801F47C8`. Read with capstone (`uw_lr/psxdis.py`). The PC's kind 0x4B
  is one of the porting house's merges of the area overlays into the exe.

## 3. What it draws, call for call, both machines

| | PlayStation (`AREA077.EMI`) | PC (`BOF3.exe`, ours identical) |
|---|---|---|
| start | `+0x34` 0x738000, `+0x38` 0xB8000, `+0x3C` (ground + 0x80) << 16; 32 inits (`0x801F4A68`) inside a push / pop | `EffectKind4B_Start` `0x47AA70`: the same |
| init | point from `+0x34..`; angles Rand & 0xFFF, & 0x3FF, & 0xFFF; edges `rcos/rsin(0x20)`, `(-0x20)`, z 0; **matrix `0x801B0F5C` (0x1000 diagonal)**, RotMatrixX / Y(-) / Z, ApplyMatrixSV twice; scale (Rand & 0xF) + 1; angle, shade 0 | `EffectKind4B_DebrisInit` `0x47B070`: the same, **matrix `0x494180` (1 diagonal)** |
| scatter | `GetTPage(0, 1, 0x380, 0x100)`, SetDrawMode, commit (1, 0xC); map camera; 32 draws; angle += 0x10; shade += 0x20 while `+9` < 4 else -= 2; 0xB9 frames | `EffectKind4B_Scatter` `0x47AAF0`: the same, commit (1, 0xC) |
| draw | `SetPolyG3`, `SetSemiTrans(1)`; centre and two edges turned by the angle (`cos * ex - sin * ey >> 8`, times the scale; height `<< 12` times the scale) projected; colour (v, v, v >> 1), black, black; commit (1, 0x1C) | `EffectDebris_Draw` `0x485030` (E3C's): the same, commit (1, 0x34) - the PC packet's size |

The PC's colour path for code 0x32 under that mode (`D3d_PrimColor`,
`D3d_SetBlend`, [`d3d-draw.md`](d3d-draw.md)): alpha 0xFF, SRCBLEND ONE,
DESTBLEND ONE - additive, the PlayStation's rate 1. Nothing on the draw side
loses the rays.

The twins of the matrix helper, read with capstone:

```
PSX 0x801B0F5C (JP GAME.EMI):  addiu v0, zero, 0x1000 ; sh v0, (a0) ... sh v0, 8(a0) ... sh v0, 0x10(a0)
PC  0x494180   (BOF3.exe):     mov edx, 1 ; mov word [eax], dx ... [eax + 8], dx ... [eax + 0x10], dx
```

## 4. Measured live

Every run: `python tools/input_run.py <copy> --out <dir> --lang en-US
--no-front --launcher <uw_lr/L>/bof3x-launcher.exe --game <main>/bof3`,
`BOF3X_OPT=none`, the copies made by `tools/recipe_shots.py` (every 6
frames over 840..1080 or 840..1500; every 60 over 840..1500).

1. **Ours before (`a80f30a5`).** No rays in any capture from 870 to 1014
   (`analysis/shots/lighthouse_rays_1014/before_sheet.png`).
   `BOF3X_DRAWORDER=884-884:-1e5,-1e5,1e5,1e5` (with `POLY_G3` added to the
   diagnostic's boxes, section 6) logs the frame's 408 primitives: the 32
   rays are there, slot 1, code 0x32, centre shade `FF FF 7F`, every box
   (47.2, 95.9)-(47.3, 95.9) - a tenth of a pixel. The 32 records at
   `0x92CC84` (read once kind 0x4B is alive, `Frame_Counter` 872): the edge
   words `+0x10..+0x1D` are all 0 or -1.
2. **Capcom's effect** (`--original "EffectGte_SetDiagonalOne,
   EffectKind4B_Run,EffectKind4B_Start,EffectKind4B_Scatter,
   EffectKind4B_DebrisInit,EffectDebris_Draw"`, the log's `inject: 10082
   ours, 6 left original`): no rays (`capcom_sheet.png`), and the 32
   records at frame 872 are byte-identical to ours before. **Capcom's PC never
   drew them.** (An all-original run, `--original '*'`, is useless here: it
   cannot write its own frames and makes no logic frames with `--no-front` -
   HANDOFF's traps; its captures were another window.)
3. **Ours with DIV-LR-A.** All 32 edges at frame 872 are 4,000+ long; the
   captures 882..1014 show the fan of rays from the globe across the floor,
   the walls and the sky, white to yellow, cyan over the sky, the party under
   them (`fix_sheet.png`, `fix/f00942.png`), as the owner's PlayStation
   picture; the white-out follows at 1026 as before.

Not measured: the frames after the flash against the PlayStation (the
owner's picture may be later than kind 0x4B's window - it shows the party
placed as our 870..1014 captures do, so it is read as this window, a guess),
and widths `BOF3X_WIDE=1` / `2` live (the self-tests run at both; the rays
are drawn from the GTE's projection like every other effect).

## 5. The fix: DIV-LR-A

`EffectGte_SetDiagonalOne` (ours, `src/game/effect_gte.cpp`) writes 0x1000
on the diagonal once a switch is armed; the switch is a four-byte word armed
through `bof3::PatchBytes("DiagonalIdentity", ...)` in `EffectGte_Inject`
**after** the `effect_gte` fuzz, so the fuzz keeps comparing ours against
Capcom's 1s (DIV-0121's pattern). `BOF3X_ORIGINAL=DiagonalIdentity` (or `*`)
gives the 1s back with ours running; `BOF3X_ORIGINAL=EffectGte_SetDiagonalOne`
gives Capcom's function.

**It reaches all seven callers**, each a "debris"-like effect whose
PlayStation twin builds the same matrix (D215's list): `EffectKind1E_DebrisInitOne`
`0x46EA80` (kind 0x1E), `EffectKind21_ArmPoints` `0x46F570` (kind 0x21),
`EffectKind4B_DebrisInit` `0x47B070` (the rays), `EffectKind64_InitShard`
`0x482240`, `EffectDebris_InitOne` `0x4851E0` (kinds 0x1F, 0x72),
`EffectKind97_DebrisInit` `0x48D860`, `EffectKindA0_InitShard` `0x4906F0`.
Each of them has drawn its shards, arms or debris collapsed onto one point on
the PC since 2001; with the fix each turns its edges by a true rotation, as
the PlayStation. Only kind 0x4B's twin was read call for call here (it calls
`0x801B0F5C` at `0x801F4AF8`); the other six twins' calls of `0x801B0F5C` are
D215's reading, not re-read. **None of the other six was seen in play** - no
recorded route is known to reach them (D215) - so their look is the owner's
to judge when a route reaches one.

**Checks** (all from an ini-less launcher copy in scratch):

- `BOF3X_SHADOW=effect_gte`: 80,000 rounds, 0 mismatches (Capcom's rule,
  before arming); then `IdentityCheck` (`src/game/effect_gte_fuzz.cpp`),
  armed: 20,000 random matrices at the fuzz's offsets - the twin's nine
  words, padding and translation untouched, the matrix answered - and kind
  0x4B's edge turned by a random rotation built on ours keeps its length
  (4,096 within 1%). Log: `effect_gte: DIV-LR-A identity check passed`.
- **Controls** (`uw_lr/controls.py`: one plant at a time, rebuild, the
  `effect_gte` self-test, restore): K1 diagonal 0x0FFF and K2 the last word
  left 1 - refused by the identity check (`round 0 word 4 ... 0x0FFF, want
  0x1000`; `word 12 ... 0x0001`); K3 an off-diagonal word 0x1000, K4 the
  switch armed before the fuzz and K5 a wrong answer - refused by the fuzz
  (`MISMATCH: round 2, EffectGte_SetDiagonalOne`). 5 of 5 refused; the
  restored build passes.
- `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'` at
  `BOF3X_WIDE` unset, 1 and 2: section 7.

## 6. Tooling changed

`src/hook/draw_order.cpp` (`BOF3X_DRAWORDER`, diagnostic only, no ledger
entry): `POLY_G3` (codes 0x30..0x33) now has a box, and each logged G3's 0x34
bytes are printed, as the tall quads' are. That is how the rays were found
drawn and degenerate.

## 7. Self-tests

`BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'
<copy>/bof3x-launcher.exe --game <main>/bof3 --no-config` from the ini-less
scratch copy `uw_lr/T` (the build the controls restored; the tip differs
from it only by a comment in `effect_gte.cpp`), one run at a time:

| `BOF3X_WIDE` | Exit | `inject:` | `DIV-0041` | DIV-LR-A |
|---|--:|---|---|---|
| unset | 0 | `10087 ours, 0 left original by BOF3X_ORIGINAL` | the width rules line and the faster slide's check (no widescreen line: narrow) | `identity check passed (20000 rounds ...)`, the switch on |
| 1 | 0 | the same | `widescreen: 53 columns a side (426 x 240)`, `full-frame fills at (-53, 0) 426 x 240`, the faster slide on at 53 | the same |
| 2 | 0 | the same | `widescreen: 120 columns a side (560 x 240)`, `full-frame fills at (-120, 0) 560 x 240`, the faster slide on at 120 | the same |

`python tools/ledger_check.py`: 121 entries, 0 errors, 2 notes (DIV-LR-A is
not in the ledger yet; the coordinator's insert names it).

**The round's rule** (narrow and 16:9 bit for bit as at the round's base):
DIV-LR-A does change narrow and 16:9 - on purpose, in the seven effects
section 5 lists, nothing else. It is not a width change; it is a restoration
the coordinator asked for on this branch. `BOF3X_ORIGINAL=DiagonalIdentity`
gives the round's base behaviour back exactly (ours then writes Capcom's 1s,
the fuzz's rule).

## 8. For the other files

**A. DIVERGENCE.md - a restoration (the coordinator numbers it):**

### The debris matrix is the GTE's identity: the lighthouse's rays come back

- **ID:** DIV-LR-A
- **Date:** 2026-10-14
- **Subsystem:** the effect engine's GTE helper `EffectGte_SetDiagonalOne`
  `0x494180` (ours, `src/game/effect_gte.cpp`)
- **Tier:** Restoration - the PlayStation's behaviour, lost in the port.
- **Original behaviour:** Capcom's PC writes 1 on the matrix's diagonal
  (`mov edx, 1`). Its seven callers premultiply it by `Gte_RotMatrixX / Y /
  Z` (each product `>> 12`), so the rotation they build collapses to 0, 1 and
  -1 and every edge it turns becomes 0 or -1 (known-defects D215). Seen in
  play as D243: kind 0x4B's 32 additive `POLY_G3` rays in the lighthouse
  (area 77, `eldritchGene.txt`, `Frame_Counter` 872..1057) are drawn as
  points on the globe; `BOF3X_ORIGINAL` of the six functions involved draws
  none, its records byte-identical to ours before the change.
- **New behaviour:** the diagonal is 0x1000, the identity in 4.12; the rays
  fan out of the globe over the scene, and the six other callers' shards,
  arms and debris turn by a true rotation.
- **Rationale:** the PlayStation twin `0x801B0F5C` (JP `GAME.EMI`) stores
  0x1000; kind 0x4B's twin (`AREA077.EMI` `0x801F3E04..0x801F4B7C`) is the PC's
  call for call otherwise. The owner's PlayStation picture shows the rays.
- **Also in the PSX version?** No - the PSX is what this restores.
- **Verification:** live (section 4 of `docs/lighthouse-rays.md`): before,
  the 32 rays' boxes a tenth of a pixel (`BOF3X_DRAWORDER`) and their edges 0
  or -1; after, every edge 4,000+ long and the rays in every capture from
  recipe frame 882 to 1014. `effect_gte` fuzz 80,000 rounds on Capcom's rule,
  the armed identity check 20,000 rounds, 5 of 5 controls refused. **The
  other six callers not seen in play; not seen by the owner.**
- **Reversible?** `BOF3X_ORIGINAL=DiagonalIdentity`.

**B. known-defects.md:**

- D243: "Fixed by DIV-LR-A. The cause is D215: kind 0x4B's rays are turned by
  `EffectGte_SetDiagonalOne`'s matrix of ones and drawn as points; Capcom's PC
  never drew them (`docs/lighthouse-rays.md`)." The heading's "before the big
  screen flash ... not looked at" becomes "fixed".
- D215: "Seen in play 2026-10-14 (D243, the lighthouse's rays); fixed by
  DIV-LR-A. The status line: a port defect, restored."

**C. owner-review.md:** play the lighthouse switch on `eldritchGene.txt`
(or look at `analysis/shots/lighthouse_rays_1014/fix_sheet.png` against your
PlayStation picture): the rays' brightness, length and how long they last;
and, whenever a route reaches one, the six other effects DIV-LR-A changes
(kinds 0x1E, 0x1F, 0x21, 0x64, 0x72, 0x97, 0xA0 - section 5).

**D. README.md row:** `lighthouse-rays.md` - D243, the lighthouse's light
rays: effect kind 0x4B's 32 additive wedges, drawn as points on the PC
because `EffectGte_SetDiagonalOne` writes 1 where the PlayStation writes
0x1000 (D215); fixed by DIV-LR-A, live-captured. MEASURED.

**E. effect_gte.md section 5:** D215's bullet gains "Seen in play as D243 and
restored by DIV-LR-A ([`lighthouse-rays.md`](lighthouse-rays.md)); ours
writes 1 only under `BOF3X_ORIGINAL=DiagonalIdentity`."
