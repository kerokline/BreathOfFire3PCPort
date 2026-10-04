# Group E4B: effect kinds 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x9D, 0x9F, 0xA4 and kind 0x87's helpers

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave four, from the round branch's tip `89c25e1`. **66 functions ours**
(`src/game/effect_4b.cpp`, shadow name `effect_4b`): the cut table's 61 rows
for E4B (`analysis/round13_cut.tsv`, the band `0x489030..0x48B1F0`) and five
starts in the band no list of the cut has - kind 0x87's pane draw `0x489390`
and its midpoint helper `0x489630`, kind 0xA4's state 2 `0x48A550`, and the
two shared tails with their own `ret` kinds 0x9F and 0xA4 jump to, `0x48A480`
and `0x48A560` (section 5). Each read to its last instruction with capstone
and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
264,000 rounds, 0 mismatches; 104 of 105 controls refused by a count, the other an equivalent mutant whose near variant is refused (section 8). **Fuzz only**: no recorded
route enters any of the 66 (section 10). Every row of the cut is effect code,
the five `hypothesis` rows among them (the dispatchers of kinds 0x89, 0xA4,
0x8A and 0x8C and the tile `0x489CD0`): all taken. **No divergence of its
own, no ledger entry** (the tile's width is DIV-0041's since the round's
end, section 1.3); no unwritten memory reaches an output (section 7).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x87 (E4A's dispatcher `0x488F60` and states): six shaded panes of four `POLY_G3`s fanned over fourteen projected points of `0x654DA0`, stepped through four cases (a sound, a red / blue fade with the leader's animation, two green fades) | 5 | calls from E4A's `0x488F80`, `0x488FB0`, `0x488FE0` |
| 0x88: thirty-two particles (`0x2C` bytes at `EffectKind30_Shards`) emitted every eighth count from the record's point toward its `+0xC..`, each drawn as a ring of sixteen `POLY_G3`s turned to the screen angle of that line; the states move on when extra object 0's x is exactly `0x3C8000`, then `0x408000` | 11 | `Effect_KindHandlers[0x88]` (`0x655570`), `EffectKind88_States` `0x654EAC` (4) |
| 0x89: the party at pose 3, a full-screen tile at grey 0x80, faded out when something sets `+1` to 2 | 4 | `Effect_KindHandlers[0x89]` (`0x655574`), `EffectKind89_States` `0x654EBC` (4) |
| the full-screen tile (`TILE` 320 x 240 of the record's `+0x5D..+0x5F`) kinds 0x89, 0x9F, 0xA4 draw | 1 | calls and tail jumps |
| 0x9D: a textured quad of a field object's (`+0xB`) at its projected point, pulsing (`+6` 0) or growing to (0x70, 0x30) and shrinking; the object marked while it lives (areas 173 / 174, `area_w4c.cpp`'s `Effect9D`) | 8 | `Effect_KindHandlers[0x9D]` (`0x6555C4`), `EffectKind9D_States` `0x654ECC` (4) |
| 0x9F: the tile brightened to 0x7C, held, faded; the party tinted against it (chapter 15's `Scena15_Run4`) | 6 | `Effect_KindHandlers[0x9F]` (`0x6555CC`), `EffectKind9F_States` `0x654EDC` (4) |
| 0xA4: the tile brightened to white; the field objects whose record word is 0x261 (or 0x308 under flag 0x11) redrawn over it (chapter 15's `scena_sc15.cpp`) | 4 | `Effect_KindHandlers[0xA4]` (`0x6555E0`), `EffectKindA4_States` `0x654EEC` (3; entry 0 is kind 0x9F's start) |
| 0x8A: three lines at x 0x10 - kind 0x6F's segment and ribbon (E3C's), linked at `+0xC` - the six map cells under them blocked (byte 0x89) while it lives, until story flag 0x91 | 6 | `Effect_KindHandlers[0x8A]` (`0x655578`), `EffectKind8A_States` `0x654F04` (3) |
| 0x8B: the leader's poses 0x6F and 0x6E by the counter `0x903849` (chapter 11's, `scena_sc11.cpp`) | 5 | `Effect_KindHandlers[0x8B]` (`0x65557C`), `EffectKind8B_States` `0x654F38` (4) |
| 0x8C: presses of the pad's 0x40 counted in a timed window (two count-downs from `0x654F28` by `Rand`), a steered step of the leader by the d-pad words, presses of 0x20 counted against the kept count, passed within one (chapter 11's) | 16 | `Effect_KindHandlers[0x8C]` (`0x655580`), `EffectKind8C_States` `0x654F48` (23) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the eight dispatchers `evidence`). "Panes", "particles", "ring",
"tile", "quad", "lines", "presses" name the code's shapes and cells, not a
play-tested fact: where the game shows these kinds and what they look like was
not traced (section 10; the owner's word, not this doc's). **Spawners found**
(a grep of `src/game` for the kind stored into `+5`): kind 0x9D by
`area_w4c.cpp` (areas 173 / 174, `+0xB` `Field_ActiveMember`'s index, `+6` a
sub-kind); kinds 0x88 (`+6` 1, the point and its goal), 0x89 (`+0xB` a battle
index the kind never reads), 0x9F and 0xA4 by `scena_sc15.cpp`; 0x8B and 0x8C
together by `scena_sc11.cpp`. No spawner of kinds 0x87 or 0x8A was found in our
source.

## 1. What each function does

### 1.1 Kind 0x87's helpers (E4A's kind)

- **`EffectKind87_Setup`** `0x489030`: the six panes (`0x1C` bytes from
  `0x92BF80`): live (`+0x18` 1), case (`+0x17`) 0, timer `+0x19` = 15 * i,
  grey 0xF0 (`+0x14..+0x16`), packet slot `+0x1B` 3 for even i and 1 for odd;
  each pane's corner pointers `+0..+0xC` and centre `+0x10` into the fourteen
  projected points at `0x92C028` (`0xC` each), animations `+0x1A` 1, 1, 7, 5,
  5, 3; and rows 8..13 of `0x654DA0` made the midpoints of two of rows 0..7
  (six calls of `EffectKind87_Midpoint`).
- **`EffectKind87_FadePanes`** `0x4891F0`: all six live at case 3, timer 0x1E,
  colour (0, 0xF0, 0).
- **`EffectKind87_StepPanes`** `0x489220` (answers `al`): the camera loaded;
  the fourteen rows of `0x654DA0` projected into `0x92C028..`; the camera
  again; each live pane by its case (the switch at `0x489380`, four entries; a
  case past 3 does nothing): 0 - the timer down, at 0 a sound of `0x654E80`'s
  three words by the byte `0x676280` (which steps 0, 1, 2, 0), the next case,
  timer 0xF; 1 - red and blue down 0x10, at timer 0xA the leader given the
  pane's animation (`Sprite_Current` the leader for the call), the timer down,
  at 0 the next case; 2 / 3 - green down 0x10 / 8, the timer down, at 0 the
  pane dead. Cases 1..3 draw the pane. `al` 1 when any pane was live.
- **`EffectKind87_DrawPane`** `0x489390` (a pane): a draw mode (page `(0x380,
  0x100)`, dtd 1) committed in the pane's slot; four `POLY_G3`s,
  semi-transparent, from the centre over corners (+0, +4), (+4, +0xC), (+0xC,
  +8), (+8, +0), each point's three dwords copied, the centre in the pane's
  colour and the corners in half of it, each committed (`0x34`).
- **`EffectKind87_Midpoint`** `0x489630` (a, b, c bytes): row c of `0x654DA0`
  = (row a + row b) `sar` 1, three dwords.

### 1.2 Kind 0x88

`EffectKind88_Run` `0x4896A0` jumps through `EffectKind88_States` by `+1`.
`_Start` (state 0) clears the particles when `+6` is set, `+9` 0, `+1` up.
`_Emit` / `_EmitOn` (states 1, 2): every eighth count (`+9 & 7` 0) a free
particle (`_FindParticle`: the first of 32 whose `+3` is 0) is set up
(`_InitParticle`: live, at the record's point `+0x34..`, velocity a 32nd of
the way to `+0xC..`, size (0x40, 0x20), life 0x20, its angle); `+9` up; with
`+6` set the particles are moved (`_MoveParticles`); when
`Sprite_ObjectsExtra[0] +0x34` is exactly `0x3C8000` (state 1) or `0x408000`
(state 2), `+1` up. `_Fade` (state 3) moves them and releases the record when
none is live. `_MoveParticles` (answers `al`): the camera loaded; each live
particle moved, its angle the screen angle of the record's point to its goal
(`_ScreenAngle`: both projected, `Math_Ratan2(dy, dx)` of the float
differences), its life down - at 0, or with its x at or past extra object 0's
x less 0x10000, dead - and drawn (`_DrawBurst(point, +8, +0xA, +6, 0x80,
0)`), the dying one too. `_DrawBurst`: a draw mode linked at the point; the
point projected and `EffectGte_ProjectSize(point, {w, h})`, each half widened
by `Frame_Counter & 1`; sixteen `POLY_G3`s from the projected centre to two
points 0x100 apart on the ellipse of that size, turned by `turn`, the centre
grey `centre` and the rim `rim`, each linked (`0x34`).

### 1.3 Kind 0x89 and the tile

`EffectKind89_Run` `0x489BE0`. `_Start` (state 0): each party member below
`Field_MemberCount` (read again after each) made `Sprite_Current` at pose
`+0x29` 3 and redrawn; `Sprite_Current` put back; the colour `+0x5D..+0x5F`
0x80; `+1` = 1. `_Hold` (state 1): the tile. `_Fade` (state 2): the colour
down 2 while `+0x5D` is not 0, else the chapter's step byte `0x8034E5` up and
`+1` = 3 (entry 3 is `BareRet`); then the tile. **`EffectKind89_DrawTint`**
`0x489CD0`: a draw mode (page `(0x3C0, 0)`, abr 2, dtd 1) committed in slot
3; a `TILE` at the cursor `(0, 0)` 320 x 240 in the record's colour,
semi-transparent, committed (3, `0x1C`). The 320 at `0x489D47` is one of
the fills [`DIVERGENCE.md`](DIVERGENCE.md) DIV-0041 lists
([`widescreen.md`](widescreen.md) section 5 names it): **widened at round
thirteen's end** (2026-10-03), the tile at `(Widescreen_FillX(), 0)`
`Widescreen_FillWidth()` x 240 as `Effect_DrawScreenTint` (E4D) draws -
Capcom's `(0, 0)` 320 x 240 until `Widescreen_ArmFills` has run (so the fuzz
still compares the original's floats) and whenever the picture is narrow.

### 1.4 Kind 0x9D

`EffectKind9D_Run` `0x489D70`. `_Start` (state 0): nothing while
`Field_Request` is 3 or 1; else the field object `+0xB` (of 30) marked
(`+0x24` bit 3), given the record's texture words `+0x2E` / `+0x30`, its point
copied to the record's; the colour 0; `+6` 0 - the pulse at once and `+1` =
1, else `+1` = 2. `_Pulse` (state 1): `Field_Request` 3 or 1 - the object let
go (bit 3 cleared) and `+1` = 0; else projected, `+9` up 8, `+0x5D` /
`+0x5E` `Math_Sin` / `Math_Cos(+9 << 4) sar 7`, the pulse quad drawn.
`_Grow` (state 2): projected; `+0x5D` up 2 below 0x70 and `+0x5E` up 1 below
0x30 (signed compares); the glow drawn; let go on a request; at exactly
(0x70, 0x30) `+6` 0 and `+1` = 1. `_Shrink` (state 3): down 2 and 1 while
not 0, the glow drawn, released at (0, 0). `_DrawPulse` / `_DrawGlow` (x, y -
a projected point's two floats): a `POLY_GT4` at the cursor,
`Gte_PrimDepthFlat4_14`, corners (x, y) .. (x + `[0x5C41F0]`, y +
`[0x5C41EC]`) summed in the FPU, the texture the record's u `+0x2E` and v
`+0x30` -/+ 0xC and -0x25 / +0xB, page (2, abr 1, `(0x340, 0x100)`),
semi-transparent, linked at the record's point (`0x54`); the pulse's upper
corners 0x90 grey and lower corners green `+0x5D + 0x20` / `+0x5E + 0x20`,
the glow's upper `+0x5D` grey and lower `+0x5E` grey. `_Project` (out): the
object of `+0xB` given the texture words again; the vector `((+0x34 sar 9) -
0x4000, (+0x38 sar 9) - 0x4000, -(+0x3E / 2))` through `Gte_RotTransPers`,
the depth to `+0x60`, out less `[0x5C41F8]` / `[0x5C41F4]`.

### 1.5 Kinds 0x9F and 0xA4

`EffectKind9F_Run` `0x48A350`, `EffectKindA4_Run` `0x48A4C0`. **State 0 of
both is `EffectKind9F_Start`** `0x48A370`: the party at pose 3, the colour 0,
`+1` = 1. Kind 0x9F: `_Brighten` up 4 below 0x7C (signed), else the counter
`0x903848` up and `+1` = 2; `_Hold`; `_Fade` down 2 while `+0x5D` is not 0,
else the party's pose given `Draw_OtSlot` and the record released. Each but
the release draws the tile and tail-jumps to **`EffectKind9F_TintParty`**
`0x48A480`: `-(s8 +0x5D / 2)` to each member's colour `+0x5D..+0x5F` unless
its `+0x89` is 7. Kind 0xA4: `_Brighten` up 8 below 0xF8 (unsigned), else all
0xFF, the step byte up and `+1` = 2; `_Hold` (state 2, `0x48A550`); both draw
the tile and tail-jump to **`EffectKindA4_MarkSprites`** `0x48A560`: each of
the thirty field objects whose `Scena15_RecordWord(+0x2C)` is 0x261, and each
whose word (asked again) is 0x308 with flag 0x11 of the chapter's row set, at
pose 3 and redrawn as `Sprite_Current`; `Sprite_Current` put back.

### 1.6 Kind 0x8A

`EffectKind8A_Run` `0x48A5F0`. `_Block` (state 0): map byte 0x89 at x 0x10
on the six z cells of `0x654EFC` (`AreaMap_SetByte`); `+1` = 1. `_Lines`
(state 1): `+6` 0, both ends' x 0x100000, the foot `+0x3C` 0 (so the top
`+0x20` / `+0x14` is 0x800000); three lines, each with `0x654F10`'s z pair at
`+0x10` / `+0x1C`, drawn by `_DrawSegment(+0xC, +0x18)`; story flag 0x91 set:
`+1` = 2. `_Unblock` (state 2): the six cells back to 0, the record released.
**`_DrawSegment` `0x48A730` and `_DrawRibbon` `0x48A8E0` are E3C's
`EffectKind6F_DrawSegment` / `_DrawRibbon` (`0x484B10` / `0x484D00`) byte for
byte** but for the link point (`+0xC` / `+0x10`, not `+0x34` / `+0x38`), the
colour row (`0x654EF8`, not `0x654A9C`) and the segment's closing draw mode,
which this copy lacks (a capstone diff of the two, every other instruction
equal). Ours is E3C's code with those three differences.

### 1.7 Kind 0x8B

`EffectKind8B_Run` `0x48AB30`. `_Start` `+9` 0xB4, `+1` 1. `_Wait`: once the
counter `0x903849` is 1 or more, the leader (`ObjTrio`, made `Field_State`
and `Sprite_Current`) gets `+0x124` bit 6 and animation 0x6F, its `+7` kept
to bits 0x11; `Sprite_Current` put back; `+1` 2. `_Pose`: the leader made
`Field_State` and `Sprite_Current`; at its word `+0x58` 0xC and byte `+0x4A`
1, animation 0x6E, `+7` kept to 0x11, `Sprite_Current` put back, `+1` 3 -
**otherwise `Sprite_Current` is left on the leader** (section 7). `_Again`:
the counter 0, `+1` 1.

### 1.8 Kind 0x8C

`EffectKind8C_Run` `0x48AC30` through 23 entries (2, 5..9, 14, 19
`BareRet`). The cells `0x676284..0x67628E` (section 3). `_Start` (0): both
count-downs a byte of `0x654F28` by `Rand & 0xF`, the counts 0. `_WaitA` (1)
/ `_WaitB` (4): the first count-down down - at 0 a sound (0x200 / 0x201), the
count kept as the target, `+1` 0xA; the pad's 0x40 pressed - counted, `+1` 3
/ 1; else 1 / 3. `_PressA` (3): 0x40 counted, the frame count up, 0x1C or
more `+1` 0x16, else 4. `_Check` (0xA): a target of 0 fails (0x16), else
0xB. `_Steer` (0xB): one of the eight d-pad words kept, sound 0x202, the
leader moved 0x800 along x or z by the word read again; `+1` 0xC, `+9` 0xC.
`_SteerWait` (0xC): `+9` down, from 0 to 0xD. `_SteerBack` (0xD): the step
count up - at 8 the leader's x and z to multiples of 0x1000, the count 0,
`+1` 0xF; else moved back by the kept word, `+1` 0xB. `_Ready` (0xF): 0x20
counted, `+1` 0x10. `_CountA` (0x10) / `_CountB` (0x12) / `_CountPress`
(0x11): the second count-down and 0x20's presses, `0x903849` up on a press in
0x10, 0x1C frames without one fails. `_Judge` (0x14): the count within one of
the target (signed) passes (0x15), else fails (0x16). `_Pass`: `0x903848` 0,
the chapter's step byte 0xF, released. `_Fail`: the step byte 8, released.

## 2. Divergence

None of this group's own: every function is a faithful replacement but the
tile's width, DIV-0041's (below). Every call goes through
`SH_CALL` (the fuzz stands recorders in); `Sprite_Current` is read again
wherever the original reads `[0x937F88]` again; the FPU work (`fild` /
`fadd` / `fsub` / `fstp`, `_ftol`'s truncation) is done in the FPU as
Capcom's is. Where the original indexes past a table or a pool, ours aborts
(section 6). `widescreen.cpp`, `cheats.cpp` and `DIVERGENCE.md` patch no byte
inside the 66 (a grep for every address). DIV-0041 names `0x489D47`, and
since round thirteen's end `EffectKind89_DrawTint` draws it widened under the
wide picture (section 1.3; DIV-0041's round's-end amendment).

## 3. The tables

| Table | Named | Entries | Why that many |
|---|---|--:|---|
| `0x654EAC` | `EffectKind88_States` | 4 | the states set at most 3; `0x654EBC` is kind 0x89's |
| `0x654EBC` | `EffectKind89_States` | 4 | `_Fade` sets 3 (`BareRet`); `0x654ECC` is kind 0x9D's |
| `0x654ECC` | `EffectKind9D_States` | 4 | to `0x654EDC`, kind 0x9F's (state 3 is set by no code of the band) |
| `0x654EDC` | `EffectKind9F_States` | 4 | to `0x654EEC`, kind 0xA4's (state 3 likewise) |
| `0x654EEC` | `EffectKindA4_States` | 3 | `0x654EF8` is data (the colour row) |
| `0x654F04` | `EffectKind8A_States` | 3 | `0x654F10` is data (the line pairs) |
| `0x654F38` | `EffectKind8B_States` | 4 | the states set at most 3; `0x654F48` is kind 0x8C's |
| `0x654F48` | `EffectKind8C_States` | 23 | the states set at most 0x16; `0x654FA4` is the next kind's (`0x48B220..`, another band) |

None is bounded by a compare (`band_rows.py` counted the runs of code
pointers - 19, 15, 11, 7, 3, 3, 34, 30 - on into the next tables, as the
addenda warn). Data read in place (no bytes copied here): `0x654DA0`, kind
0x87's fourteen point rows of `0x10` (rows 8..13 rewritten by
`EffectKind87_Midpoint` - Capcom's code writes its own `.data`), `0x654E80`
three sound words, `0x654EF8` kind 0x8A's one colour row (the next row would
read `0x654EFC`'s map cells), `0x654EFC` six cells, `0x654F10` three z pairs,
`0x654F28` sixteen count-down bytes, the switch table `0x489380` inside
`EffectKind87_StepPanes`, and the `.rdata` floats `0x5C41EC..0x5C41F8`.
**The cells** of the band (`effect_4b_callees.h`): `0x676280` kind 0x87's
sound index; `0x676284` the frame count, `0x676286` the steered word,
`0x676288` / `0x67628E` the two count-downs, `0x676289` the step count,
`0x67628A` the target, `0x67628C` the count - kind 0x8C's; `0x903848` /
`0x903849` the chapters' counters; `0x8034E5` the chapter's step byte;
`0x802034` extra object 0's x.

## 4. The fuzz (`effect_4b_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_4b`, effect mode (`g.effect`; kinds
0x87..0x8C, 0x9D, 0x9F, 0xA4, each clone its own but the shared tile and
start), 4,000 rounds a function (`BOF3X_E4B_ONLY=<name>` runs the clones
whose name holds it). Shapes: 56 `kEffect` (the eight dispatchers'
`state_span` their table's length; the states, the void draws and helpers,
the particle finder), ten `kCall` (the helpers with arguments). `ret_mask` 0xFF on
`EffectKind87_StepPanes` and `EffectKind88_MoveParticles` (their callers test
`al`), 0xFFFFFFFF on `EffectKind88_FindParticle` (a pointer) and
`EffectKind88_ScreenAngle` (the callers keep `ax`; ours returns
`Math_Ratan2`'s whole answer as the original does). The eight tables are
`DataTable`s, swapped for recorders on both sides. **Regions** beyond effect
mode's standard ones: the cells `0x676280` (`0x10`), kind 0x87's point rows
`0x654DA0` (`0xE0`, in `.data`: restored after the run with every region),
`Draw_OtSlot` `0x92BF19`. Everything else is standard: the records,
`Sprite_Current`, `EffectKind30_Shards` (the panes, the fourteen points, the
particles), `ObjTrio`, `Sprite_Objects`, `Sprite_ObjectsExtra`,
`Field_MemberCount`, the flag row pointer, `Field_Request`, `Field_State`,
`Input_Pressed`, the chapter bytes, the counters, the packet buffer.

**Callees**: the effect-standard rows (`Effect_Release`, `Flags_Test`,
`Sound_PlayEffect`, `Rand`, `Sprite_SetAnimation`, `Sprite_UpdateScreen`,
`AreaMap_SetByte`, `BareRet`, `Math_Sin` / `Cos` / `Ratan2`,
`EffectGte_LoadMapCamera` / `_ProjectPoint`, `MapView_LinkPrimAt`,
`Gfx_CommitPrim`, the `Gpu_*`, `Gte_PrimDepthFlat4_14`); `_ftol` for real on
both sides. **Listed here**: `Scena15_RecordWord` (SC15's, in no standard
set: its word index masked to 16 bits, answering 0x261 or 0x308 a third of
the time each). **Re-listed**: `Gte_RotTransPers` with its vertex hashed at
six bytes - `EffectKind9D_Project`'s `SVECTOR` has a fourth word Capcom never
wrote (stack), which the effect row hashes (eight bytes) and which our
`Gte_RotTransPers` never reads - its screen point filled with fractional
floats; `EffectGte_ProjectSize` with the size hashed at both words, which
every caller here writes (`{w, h}` of a particle, `{0x40, 0}`). **The group's
own** called directly, by name: the pane draw (the pane hashed, `0x1C`), the
midpoint at three bytes, the finder answering the first free particle or null
(null a quarter of the time), the particle set-up, the burst (the point
hashed, the words at 16 bits, the greys at 8), the screen angle (both points
hashed), the projection filling its out with fractional floats (the out is
the caller's local: not logged), the two quads at their two float words, the
segment (both points hashed) and the ribbon (eight words at 16 bits); the
mover as `kFlag`; the clears, the pulse, the tile, the tint and the mark as
`kPhase`.

**Seeds** (per function, after the harness's per-round fill): on all 20
records `+0xB` below 30, `+6` 0 for kind 0x8A's draws (its one colour row)
and half the time otherwise, `+9`, `+0x5D`, `+0x5E` at every compare's
boundaries (0..3, 0x6E..0x71, 0x7B / 0x7C, 0x7F / 0x80, 0xF7 / 0xF8, 0xFF;
0x2F..0x31); `Field_MemberCount` 0..3; members' `+0x89` 7 half the time;
`Field_Request` 1, 3, 0, 2 a third of the time; kind 0x87's panes - pointers
on the fourteen points, case 0..4, live or not, the timer at 0, 1, 2, 0xA,
0xB, 0xF, 0x1E - and the sound index 0..2; kind 0x88's particles live or
not, life 0..2, 0x20, 0xFF, x at and round extra object 0's x - 0x10000, the
goal x exactly `0x3C8000` / `0x408000` half the time and `+9`'s low bits 0
half the time; kind 0x9D's growth at its stop; the objects' record words for
the mark; `0x903849` round 1; the leader's `+0x58` / `+0x4A` at the pose's
test; kind 0x8C's cells at their compares (count-downs 0..2, the frame count
0x1A..0x1C, the step count 6..8, the target and the count within and past
one, the kept word and the pad word among every value tested).
**Arguments**: the pane draw a pane; the midpoint three rows below fourteen
over random upper bytes; the burst a particle's point; the angle the record's
`+0x34` and `+0xC`, the segment its `+0xC` and `+0x18` (as their callers);
the quads' floats with fractions and one time in eight a NaN (quiet or
signalling) or an infinity; the projection a scratch buffer. **Disturbance**
(the group's, from the hash only): `+9`, the colour bytes, `+0xB`,
`Field_MemberCount` (re-read by kind 0x89's start after each redraw),
`0x903849`, kind 0x8C's count-downs, the pad's word among the tested ones
(re-read by `_Steer` after its sound).

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_4b`,
exit 0): 264,000 rounds over 66 functions, 2,075,152 calls to the stand-ins,
**0 mismatches**; 24,997 bytes of state in 48 regions; 411 stand-ins. Every
entry of the eight tables reached (each handler recorder 147..1,400 calls,
`BareRet` 2,382); `Effect_Release` 13,756, `Scena15_RecordWord` 240,000,
`Sprite_SetAnimation` 5,373, `EffectKind88_ScreenAngle` 89,392.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
701 self-test lines, no `MISMATCH` line, `inject: 7853 ours, 0 left
original`; `effect_4b` there 264,000 rounds, 2,074,730 calls, 0 mismatches.
**With `BOF3X_WIDE=1`**: `'*'` exit 0, 701 self-test lines, no `MISMATCH`
line, the same `effect_4b` counts. Neither died silently (a first attempt
exited 127 before starting the game: the script's own quoting of
`BOF3X_SHADOW`, fixed and re-run). `tools/ledger_check.py`: 70 entries, 0
errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the cut's 61 to the byte (the cut's sizes
  ran on into padding for 42 of them). One differs by code: the cut's
  `0x489220` (1,029 bytes) is 0x170 - its switch table ends at `0x489390`,
  which is a function of its own (its frame, its `ret`, called from
  `0x489362`): **`EffectKind87_DrawPane`**, added. The tool also printed
  `0x48A420` at 0xA0, running over the shared tail; it ends at its `jmp
  Effect_Release` (0x5D).
- **Starts no list of the cut has, in the band, taken**: `0x489630` (catalog
  part 7, unlabelled; the cut names it only as the host of `0x4896A0..`; the
  harness's effect-standard set has a row for it by address, EKH's), kind
  0x87's midpoint, called six times by `0x489030`; `0x48A550` (catalog part 6,
  paired with SCENA15 by callers; [`scena_sc15.md`](scena_sc15.md) names it
  and SC15 did not take it), kind 0xA4's state 2, `EffectKindA4_States[2]`;
  `0x48A480` and `0x48A560`, which [`scenario_harness.md`](scenario_harness.md)
  section 8.5 lists as "second entries or shared tails" of E4B: each is
  reached only by tail `jmp`s (from three states and two states), and each
  ends in its own `ret` (`0x48A560` with its own frame), so each is taken
  whole as a function (E3C's `0x485C60`, E2G's `0x47E120` the models) and
  the states call it.
- **Not functions, merged**: none. `0x489C80` is a single `jmp 0x489CD0` (a
  table entry: kept as the state it is); `0x489C90` falls into
  `0x489CD0` by a `jmp` of 0 bytes - ours calls the tile.
- **The `hypothesis` rows**: `0x489BE0`, `0x489CD0`, `0x48A4C0`, `0x48A5F0`,
  `0x48AC30` - three dispatchers in `Effect_KindHandlers`, a dispatcher and a
  draw: all effect code, taken.
- **The cut's hints**: the `unit_desc` "Fn_48AB30 kind 0x8B (19)" holds kind
  0x8C's states too (`0x654F38`'s run continues into `0x654F48`); "kind 136"
  holds `0x489B40` (kind 0x88's particle set-up) and "kind 137" holds kind
  0x9D's dispatcher host - the code decides (section 3).
- **Code in the band no group takes**: none - `0x489030..0x48B1FC` is all
  ours now.

## 6. Where ours aborts

Each where Capcom's code reads or writes past what it owns; in the band's own
code every one is kept in range, so ordinary play does not reach them:

- a dispatcher's `+1` past its table (the next table or data);
- `EffectKind87_StepPanes`' sound index `0x676280` past three (the code wraps
  it at 3);
- `EffectKind87_Midpoint`'s rows past fourteen (`.data` after the table; the
  only caller passes 0..13);
- `+0xB` past `Sprite_Objects`' thirty (kind 0x9D's; the spawner stores
  `Field_ActiveMember`'s index);
- `Field_MemberCount` past three, or a member index past `ObjTrio`'s three
  (kinds 0x89, 0x9F, 0xA4);
- kind 0x8A's colour row by `+6` past one (state 1 sets `+6` 0).

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x8B's `_Pose` leaves `Sprite_Current` on the leader** on every
  frame its test fails: it makes the leader `Sprite_Current` before the test
  and puts the record back only when the test passes. `Effect_RunObjects`
  sets `Sprite_Current` afresh for the next record, so the next effect is not
  affected; whatever runs after the last record of the frame sees the leader.
  Ours does the same (control 77 is refused by it).
- **Kind 0x88 draws a particle on the frame it dies** (the draw follows the
  `+3` clear), and states 1 and 2 wait for extra object 0's x to equal
  `0x3C8000` / `0x408000` exactly - an object that steps past it never moves
  the kind on.
- **Kind 0x89 and kind 0x9D / 0x9F never leave their last states by
  themselves**: kind 0x89's `_Fade` sets `+1` 3, a `BareRet`, and the record
  stays live; kinds 0x9D and 0x9F have a state 3 no code of the band sets
  (their spawners or scripts must). Kind 0x89's state 2 is likewise set from
  outside.
- **`EffectKind9D_Project` hands `Gte_RotTransPers` an `SVECTOR` whose fourth
  word it never writes** and a fourth argument - its own argument's stack slot
  as the PSX flag pointer - that the port's `Gte_RotTransPers` does not take.
  Neither reaches an output: ours reads three words and no flag
  ([`psx_gte_transform.cpp`](../src/game/psx_gte_transform.cpp)); the fuzz
  hashes six bytes (section 4).
- **Kind 0x87 rewrites the image's `.data`**: `EffectKind87_Setup` recomputes
  rows 8..13 of `0x654DA0` every time - idempotent while rows 0..7 are
  constant.
- **Unchecked indexes** (section 6): all kept in range by the band's own code.

## 8. Controls

Each a plant in `effect_4b.cpp` (anchored on a unique string), rebuilt, run with `BOF3X_E4B_ONLY` on the clones it touches, restored and rebuilt (the scratchpad's `controls.py` / `control_list.py`). **104 of 105 refused**, each by a mismatch (exit 3 after a `MISMATCH` line: a differing log entry or state byte); the one not refused is an equivalent mutant, its near variant refused.

| # | Function | Mutant | Result |
|--:|---|---|---|
| 1 | `EffectKind87_Setup` | pane slot parity swapped | refused |
| 2 | `EffectKind87_Setup` | pane 2's animation 6 | refused |
| 3 | `EffectKind87_Setup` | a midpoint into the wrong row | refused |
| 4 | `EffectKind87_FadePanes` | fade timer 0x1F | refused |
| 5 | `EffectKind87_StepPanes` | the sound index wraps at 4 | refused |
| 6 | `EffectKind87_StepPanes` | the animation at timer 0xB | refused |
| 7 | `EffectKind87_StepPanes` | case 3 fades green by 0x10 | refused |
| 8 | `EffectKind87_StepPanes` | a case past 3 drawn | refused |
| 9 | `EffectKind87_DrawPane` | the last triangle's corner | refused |
| 10 | `EffectKind87_DrawPane` | the corners' blue a quarter | refused |
| 11 | `EffectKind87_Midpoint` | a logical halving | refused |
| 12 | `EffectKind88_Run` | kind 0x89's table | refused |
| 13 | `EffectKind88_Start` | +9 1 | refused |
| 14 | `EffectKind88_Emit` | every fourth count | refused |
| 15 | `EffectKind88_Emit` | goal at or past | refused |
| 16 | `EffectKind88_EmitOn` | goal 0x408001 | refused |
| 17 | `EffectKind88_Fade` | released while live | refused |
| 18 | `EffectKind88_ClearParticles` | +4 cleared | refused |
| 19 | `EffectKind88_FindParticle` | free by +2 | refused |
| 20 | `EffectKind88_MoveParticles` | life ends at 1 | refused |
| 21 | `EffectKind88_MoveParticles` | past, not at | refused |
| 22 | `EffectKind88_MoveParticles` | centre grey 0x81 | refused |
| 23 | `EffectKind88_DrawBurst` | the ring's step 0x101 | refused |
| 24 | `EffectKind88_DrawBurst` | the rotation's sign | refused |
| 25 | `EffectKind88_DrawBurst` | the third corner centre grey | refused |
| 26 | `EffectKind88_DrawBurst` | no odd-frame widening | refused |
| 27 | `EffectKind88_ScreenAngle` | dy reversed | refused |
| 28 | `EffectKind88_InitParticle` | velocity y a sixteenth | refused |
| 29 | `EffectKind88_InitParticle` | life 0x21 | refused |
| 30 | `EffectKind89_Start` | one member short | refused |
| 31 | `EffectKind89_Start` | blue 0x7F | refused |
| 32 | `EffectKind89_Hold` | the wrong tail | refused |
| 33 | `EffectKind89_Fade` | stays at state 2 | refused |
| 34 | `EffectKind89_DrawTint` | the tile 322 wide | refused |
| 35 | `EffectKind89_DrawTint` | shade-texture on | refused |
| 36 | `EffectKind9D_Start` | Field_Request 1 not waited | refused |
| 37 | `EffectKind9D_Start` | bit 2 marked | refused |
| 38 | `EffectKind9D_Start` | +1 = 3 | refused |
| 39 | `EffectKind9D_Pulse` | bit 2 let go | refused |
| 40 | `EffectKind9D_Pulse` | +9 up 7 | refused |
| 41 | `EffectKind9D_Pulse` | sar 6 | refused |
| 42 | `EffectKind9D_Grow` | an unsigned compare | refused |
| 43 | `EffectKind9D_Grow` | either at its top | refused |
| 44 | `EffectKind9D_Shrink` | either at 0 | refused |
| 45 | `EffectKind9D_DrawGlow` | height from the width constant | refused |
| 46 | `EffectKind9D_DrawPulse` | v + 0xC | refused |
| 47 | `EffectKind9D_DrawGlow` | page y 0 | refused |
| 48 | `EffectKind9D_DrawPulse` | green + 0x21 | refused |
| 49 | `EffectKind9D_DrawGlow` | the greys swapped | refused |
| 50 | `EffectKind9D_Project` | a flooring halving | refused |
| 51 | `EffectKind9D_Project` | y less the x centre | refused |
| 52 | `EffectKind9D_Project` | the depth to +0x64 | refused |
| 53 | `EffectKind9F_Start` | pose 2 | refused |
| 54 | `EffectKind9F_Brighten` | up to 0x7D | refused |
| 55 | `EffectKind9F_Brighten` | counter up 2 | refused |
| 56 | `EffectKind9F_Hold` | tint before the tile | refused |
| 57 | `EffectKind9F_Fade` | pose 3, not Draw_OtSlot | refused |
| 58 | `EffectKind9F_TintParty` | skips +0x89 6 | refused |
| 59 | `EffectKind9F_TintParty` | a flooring halving | refused |
| 60 | `EffectKindA4_Brighten` | up to 0xF8 | refused |
| 61 | `EffectKindA4_Brighten` | +1 = 3 | refused |
| 62 | `EffectKindA4_Hold` | no tile | refused |
| 63 | `EffectKindA4_MarkSprites` | word 0x262 | refused |
| 64 | `EffectKindA4_MarkSprites` | flag 0x12 | refused |
| 65 | `EffectKind8A_Block` | map byte 0x88 | refused |
| 66 | `EffectKind8A_Lines` | top + 0x81 | refused |
| 67 | `EffectKind8A_Lines` | the flag's sense | refused |
| 68 | `EffectKind8A_Unblock` | map byte 1 | refused |
| 69 | `EffectKind8A_DrawSegment` | line linked at 0x21 | refused |
| 70 | `EffectKind8A_DrawSegment` | no odd-frame widening | refused |
| 71 | `EffectKind8A_DrawRibbon` | a corner grey 1 | refused |
| 72 | `EffectKind8A_DrawRibbon` | second quad turned 0x801 | refused |
| 73 | `EffectKind8B_Start` | +9 0xB5 | refused |
| 74 | `EffectKind8B_Wait` | waits for 2 | refused |
| 75 | `EffectKind8B_Wait` | +7 kept to 0x13 | refused |
| 76 | `EffectKind8B_Pose` | +0x4A not tested | refused |
| 77 | `EffectKind8B_Pose` | Sprite_Current put back (the quirk fixed) | refused |
| 78 | `EffectKind8B_Again` | counter 1 | refused |
| 79 | `EffectKind8C_Start` | Rand & 7 | refused |
| 80 | `EffectKind8C_Start` | second count-down + 1 | refused |
| 81 | `EffectKind8C_WaitA` | target + 1 | refused |
| 82 | `EffectKind8C_WaitA` | idle to 4 | refused |
| 83 | `EffectKind8C_PressA` | past, not at, 0x1C | refused |
| 84 | `EffectKind8C_WaitB` | idle to 1 | refused |
| 85 | `EffectKind8C_Check` | +1 = 0xC | refused |
| 86 | `EffectKind8C_Steer` | 0xC000 not taken | refused |
| 87 | `EffectKind8C_Steer` | x up 0x400 | refused |
| 88 | `EffectKind8C_Steer` | the pad word not read again | refused |
| 89 | `EffectKind8C_SteerWait` | at 1 | refused |
| 90 | `EffectKind8C_SteerBack` | seven steps | refused |
| 91 | `EffectKind8C_SteerBack` | x to 0x2000 | refused |
| 92 | `EffectKind8C_SteerBack` | 0x4000 moves x back | refused |
| 93 | `EffectKind8C_Ready` | the 0x40 button | refused |
| 94 | `EffectKind8C_CountA` | counter not up | refused |
| 95 | `EffectKind8C_CountA` | idle to 0x11 | refused |
| 96 | `EffectKind8C_CountPress` | the 0x40 button | refused |
| 97 | `EffectKind8C_CountB` | timer left 1 | refused |
| 98 | `EffectKind8C_Judge` | one over not passed | refused |
| 99 | `EffectKind8C_Pass` | step 0xE | refused |
| 100 | `EffectKind8C_Fail` | step 9 | refused |
| 101 | `the eight dispatchers` | every dispatcher's last state unreachable | refused |
| 102 | `EffectKind87_StepPanes` | case 0's next timer 0xE | refused |
| 103 | `EffectKind88_Emit` | Sprite_Current not read again after the move | refused |
| 104 | `EffectKind9D_Start` | Sprite_Current not read again (no call between: an equivalent mutant) | not refused: equivalent - no call lies between the two reads of `Sprite_Current`, so nothing can move it; the near variant 103 (the read after a call dropped) is refused |
| 105 | `EffectKindA4_MarkSprites` | Sprite_Current not put back | refused |

## 9. Calls across groups

**Outbound**: none to another group of this round (`band_rows.py --edges`:
only EGT's helpers, ours). By name, already ours: EGT's
`EffectGte_LoadMapCamera`, `_ProjectPoint`, `_ProjectSize`; SC15's
`Scena15_RecordWord`; `Effect_Release`, `Flags_Test`, `Sound_PlayEffect`,
`Rand`, `Sprite_SetAnimation`, `Sprite_UpdateScreen`, `AreaMap_SetByte`,
`MapView_LinkPrimAt`, `Gfx_CommitPrim`, `Gte_RotTransPers`,
`Gte_PrimDepthFlat4_14`, the `Gpu_*` and `Math_*` primitives; `BareRet` and
`Effect_StateRelease` through the tables. No callee is called raw.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `0x488F80` (`0x488F95`), kind 0x87's state 0 | E4A (wave four) | `EffectKind87_Setup` `0x489030` |
| `0x488FB0` (`0x488FB0`, `0x488FB9`) | E4A | `EffectKind87_StepPanes` `0x489220`, `EffectKind87_FadePanes` `0x4891F0` |
| `0x488FE0` (`0x488FE0`) | E4A | `EffectKind87_StepPanes` `0x489220` |
| `scenario_harness.cpp` `kEffectStd` | EKH's row | `FX_RAW(0x489630)`, three words: a harness file, left raw for the coordinator (ours calls `EffectKind87_Midpoint` by name and lists it itself; the row could become `FX_OURS(EffectKind87_Midpoint)` at the fold) |

`Effect_RunObjects` reaches the eight dispatchers through
`Effect_KindHandlers` (read in place); the eight state tables are read by
their dispatchers only.

## 10. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for every row of the band, and no first-call trace under
`analysis/calltrace` names any of the 66 addresses (a grep of every file but
the entries lists). **Fuzz only.** No live run was made (the brief). Which
scene shows which kind is the owner's to say; recorded walks through areas
173 / 174 (kind 0x9D), chapter 11's press count (0x8B, 0x8C) or chapter 15's
scenes (0x88, 0x89, 0x9F, 0xA4) would let the coordinator's frame-hash A/B
cover them.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 56 lines and a comment, the
read extents of the 66 less ten already listed right. They re-list seven host
lines smaller (each old line spans the hidden starts after its host's `ret`):
`00489220 405` (0x170), `00489630 16F` (0x6D), `00489B40 190` (0x96),
`00489CD0 17F` (0x9D), `00489E50 1B2` (0xA7), `0048A2A0 489` (0xAB),
`0048A8E0 BBB` (0x24E).

## 12. The rebinding

`grep -rn -i` of the 66 addresses and the eight tables in `src` and `docs`
(`band_rows.py --refs`: 0 references). **Rebound**: none needed in our
source. **Left raw**: `scenario_harness.cpp`'s `FX_RAW(0x489630)` (a harness
file, section 9); `scena_sc15.cpp`'s comment citing the call sites `0x48A57A`
/ `0x48A59D` (addresses inside `EffectKindA4_MarkSprites`, still right). E4A's
three calls into kind 0x87's helpers are E4A's raw addresses until both
merge (its `_callees.h`, in its branch).
