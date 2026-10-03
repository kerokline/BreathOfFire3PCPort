# Group E3B: effect kinds 0x63, 0x65, 0x67, 0x69 and 0x6C

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9, 10 and
14), wave three, from the round branch's tip `0e0532c`. **49 functions ours**
(`src/game/effect_3b.cpp`, shadow name `effect_3b`): the cut table's 49 rows for
E3B (`analysis/round13_cut.tsv`, the band `0x4823D0..0x484000`), none dropped,
none added (section 5). Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
196,000 rounds, 0 mismatches; @CONTROLS@. **Fuzz only**: no recorded route
enters any of the 49 (section 9).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x63: a disc of 32 shaded triangles on the ground at a fixed point that grows, holds and shrinks under a full-screen tint while every live sprite and member is refreshed, then a wait on the counter `0x903848` and the tint's fade | 10 | `Effect_KindHandlers[0x63]` (`0x6554DC`), `EffectKind63_States` `0x6549B8` (7) |
| 0x65: a field wait - on `Field_Request` 3 and the save block's dword `0x904134` a multiple of five, a held button toggles story flag 0x4F, shakes the camera 60 frames and opens script message 1 | 6 | `Effect_KindHandlers[0x65]` (`0x6554E4`), `EffectKind65_States` `0x6549D4` (5) |
| 0x67: a kind-0x13 record spawned every frame | 2 | `Effect_KindHandlers[0x67]` (`0x6554EC`), `EffectKind67_States` `0x6549EC` (3: `EffectKind54_Start`, its spawner, `BareRet`) |
| 0x69: nine parts of one effect - a column at the spawner's point and two rings of four orbiting it - each a flickering column of quads or lines and a glow; the spawner waits for all nine | 23 | `Effect_KindHandlers[0x69]` (`0x6554F4`), a stack table of three by `+1`; `EffectKind69_Parts` `0x6549F8` (3) by `+2`; `EffectKind69_Part0Steps` `0x654A04`, `_Part1Steps` `0x654A14`, `_Part2Steps` `0x654A24` (4 each) by `+3` |
| 0x6C: sixteen sparks scattered from a fixed point over `EffectKind30_Shards`, flying and fading as textured quads | 8 | `Effect_KindHandlers[0x6C]` (`0x655500`), `EffectKind6C_States` `0x654A34` (2); the sparks' `EffectKind6C_SparkStates` `0x654A3C` (2) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the five kinds' dispatchers and kind 0x69's four sub-dispatchers
`evidence`). "Disc", "tint", "shake", "column", "glow", "ring", "spark" name the
code's shape - the primitives it commits and the cells it steps - not a
play-tested fact: where the game shows these kinds and what they look like was
not traced (section 9; the owner's word, not this doc's). **No PSX twin**:
`analysis/pairs_propagated.json` pairs none of the 49 (the only pair in the
band is `0x4837B0`'s, not ours). The spawners found in our source: chapter 8's
third scene (`Scena08_Scene3`, `scena_sc7.cpp`: "an effect of kind 0x63" after
event battle 0x20), chapter 10's second run (`Scena10_Run2`, `scena_sc9b.cpp`:
kind 0x69 among 0x61..0x69), area 49 (`Area49_SpawnEffect6C`, `area_w1c.cpp`:
kind 0x6C with the leader's `+0x89` at 5). No spawner of kind 0x65 or 0x67 was
found by a grep of `src/game`.

## 1. What each function does

Every state runs with `Sprite_Current` an `Effect_Objects` record (20 of 0x80;
`Effect_RunObjects`), reads it again after every call, and is reached through
a `.data` cell (a kind's table) unless said otherwise.

### 1.1 Kind 0x63 (`EffectKind63_Run` `0x4823D0`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind63_Start` `0x4823F0` | the point (`0x240000`, `0x418000`), its height `AreaMap_Elevation(x, z)` to the word `+0x3E`; the radius `+0x18` 0, the rim shade `+0x5C` 0x3D, the tint `+0x5D..+0x5F` 0 (the original clears `eax` after the call and stores `al`), `+1` = 1 |
| 1 | `EffectKind63_Grow` `0x482470` | the point projected into `+0x74..` (`EffectGte_ProjectPoint`), the disc (radius, 0xFF, rim); radius up 5, rim up 2; past 0x190 (signed) `+9` = 0xF, `+1` up |
| 2 | `EffectKind63_Hold` `0x4824E0` | the disc white to the rim; `+9` down, at 0 the tint 0x60, `+1` up |
| 3 | `EffectKind63_Shrink` `0x482550` | the disc, E4D's tint `0x48CA90`, the sprites refreshed; radius down 10, rim down 2; at 0 or below the counter `0x903848` up, `+1` up |
| 4 | `EffectKind63_WaitCue` `0x4825D0` | the counter at 0x2B: `+9` = 0x10, `+1` = 5; the tint, the sprites (a tail `jmp`) |
| 5 | `EffectKind63_FadeOut` `0x482600` | `+9` down; not 0: the tint's three bytes down 6; 0: `+1` = 6; the tint, the sprites |
| 6 | `EffectKind63_End` `0x482650` | each member below `Field_MemberCount`: `ObjTrio` record `+0x29` = 6; `Effect_Release` |

`EffectKind63_RefreshSprites` `0x4826B0` (called by state 3, tail-jumped to by
4 and 5): with `Sprite_Current` each of the thirty `Sprite_Objects` records
(written for every record), a live one (`+0` bit 0) gets `+0x29` = 5 and
`Sprite_UpdateScreen`; then each member below `Field_MemberCount` (read again
after each call) the same; `Sprite_Current` put back.
`EffectKind63_DrawDisc` `0x482740` (cdecl `radius, centre, rim`): a draw mode
(`Gpu_GetTPage(0, 1, 0x3C0, 0)`, dtd 1) in slot 1, then 32 semi-transparent
`POLY_G3`s in slot 1: the centre the record's floats `+0x74` / `+0x78` (copied
as dwords) and `+0x7C` (through `fld` / `fst`), shaded the low byte of `centre`;
the rim points at `a` and `a + 0x80` (`a` = 0..0xF80; `Math_Cos` / `Math_Sin`
times the s16 radius `sar 12`, `fild`-added to the centre), shaded the low byte
of `rim`. The original keeps `a` in its first argument's slot and uses its third
argument's slot as a temporary.

### 1.2 Kind 0x65 (`EffectKind65_Run` `0x4828B0`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind65_WaitRequest` `0x4828D0` | `Field_Request` 3: `+1` = 1 |
| 1 | `EffectKind65_Check` `0x4828F0` | nothing while the request is 3; else the dword `0x904134` (in the save block) not a multiple of 5 (`div`, unsigned): `+1` = 0; a multiple: `ScriptFlags_Set40`, `+1` = 2 |
| 2 | `EffectKind65_WaitInput` `0x482930` | `Input_Held` not 0: `Sound_PlayEffect(0x202)`, `Flags_Toggle(0x904030, 0x4F)` (story flag 0x4F), `+9` = 60, `+1` = 3 |
| 3 | `EffectKind65_Shake` `0x482970` | `+9` down; 0: `Msg_OpenScript(1)`, `Field_Request` = 2, `+1` = 4; else `MapView_Redraw` = 2 and `Camera_ShiftY` += 4 times the signed byte `EffectKind65_ShakeSteps[+9 & 3]` |
| 4 | `EffectKind65_Close` `0x4829D0` | the request no longer 2: `ScriptFlags_Clear40`, `Sound_PlayEffect(0x206)`, `+1` = 0 |

### 1.3 Kind 0x67 (`EffectKind67_Run` `0x482A00`)

Its table's state 0 is E2G's `EffectKind54_Start` (`+9` = 0, `+1` = 1) and its
state 2 `BareRet`. State 1, `EffectKind67_SpawnKind13` `0x482A20`:
`Effect_FindFree`'s byte to `0x903850`; not 0xFF: that record `+0` = 1, kind
`+5` = 0x13, `+0x64` = -0x386, `+0x68` the s16 `Camera_Angles + 2`, `+0x6C` =
0xE2, `+9` = 0x1E. It never moves its own `+1`: one spawn a frame for as long
as the record lives (section 7).

### 1.4 Kind 0x69 (`EffectKind69_Run` `0x482A80`)

The dispatcher builds a stack table of three (`mov [esp + k], imm32`) and
`call`s `[esp + +1 * 4]`:

| `+1` | Function | What |
|---|---|---|
| 0 | `EffectKind69_Spawn` `0x482AB0` | `Sprite_Current` to `EffectKind69_Parent` `0x676268`; nine records from `Effect_FindFree` (its 0xFF not tested), each `+0` = 1, kind 0x69, `+1` = 2, `+3` = 0: one with `+2` = 0, four with `+2` = 1 and `+4` = 0..3, four with `+2` = 2 and `+4` = 0..3; its own `+0xB` = 0, `+1` up, `Sound_PlayEffect(0x203)` |
| 1 | `EffectKind69_WaitParts` `0x482BB0` | `+0xB` at 9 (every part counted in): a tail `jmp` to `Effect_Release` |
| 2 | `EffectKind69_PartRun` `0x482BD0` | `jmp [EffectKind69_Parts + +2 * 4]` |

A part (`EffectKind69_Part0` `0x482BF0`, `_Part1` `0x482DF0`, `_Part2`
`0x483540`): `call [its steps + +3 * 4]`, `EffectKind69_UpdateScreenXY`; with
`+3` past 0 (read again) `EffectKind69_PushPointMatrix`, its draw, a tail `jmp`
to `Gte_PopMatrix`. The draws: part 0 the column (0x60, 0x20, 7); part 1 the
column (0x20, 0x50, 0x1F) and the glow; part 2 `0x4837B0` (lines; catalog
part 6, no group) and the glow.

| `+3` | Part 0 | Part 1 | Part 2 |
|---|---|---|---|
| 0 | `_Part0Place` `0x482CE0`: `+0xB` = `Rand() & 0xF`, `+9` = `+0xA` = 0, the parent's x and z, the height `AreaMap_Elevation`; `+3` up | `_Part1Place` `0x482E40`: the same counts, `+0xC` = `+4 << 5`, the orbit of radius 0x20 round the parent, the parent's height; `+3` up | `_Part2Place` `0x483580`: `+0xC` = `(+4 << 5) + 0x10`, radius 0x18 |
| 1 | `_Part0Grow` `0x482D50`: `+0xB` up; `+0xA` up 2 below 0x12, at it `+9` = 60, `+3` up | `_Part1Grow` `0x482F00`: the same, top 0x10 | `_Part2Grow` `0x483640`: top 0x10, no `+0xB` step |
| 2 | `_Part0Hold` `0x482D80`: `+0xB` up, `+9` down, at 0 `+3` up | `_Part1Orbit` `0x482F30`: `+0xB` up, `+0xC` up, the orbit at 0x20; `+9` down, at 0 `+3` up | `_Part2Orbit` `0x483660`: the orbit at 0x10 |
| 3 | `_Part0Fade` `0x482DB0`: `+0xB` up, `+0xA` down, at 0 the parent's `+0xB` up and `Effect_Release` | `_Part1Fade` `0x482FD0`: the orbit at 0x20, `+0xA` down, at 0 the same end | `_Part2Fade` `0x483700`: the orbit at 0x18, `+0xA` down 2 |

The orbit: `DamageScratch` `0x903850` = the radius, `0x903854` = `(+0xC & 0x7F)
<< 5`; x = `Math_Cos` times `DamageScratch` (read again after the call) plus
the parent's x, z = `Math_Sin(0x903854)` (read again) times it plus the
parent's z - no shift (a radius of 0x20 is two map cells in 16.16).

`EffectKind69_PushPointMatrix` `0x482C30`: `Gte_PushMatrix`; a MATRIX on the
stack: its translation `Gte_RotTrans` of the point's SVECTOR (`x sar 9 -
0x4000`, `z sar 9 - 0x4000`, `-(s16 +0x3E / 2)`), its rotation `Gte_RotMatrix`
of (0, 0, 0x400 - 0 when `+8` bit 0 is set), times `Camera_Matrix`
(`Gte_MulMatrix0(camera, m, m)`), set as the rotation and the translation.
`EffectKind69_UpdateScreenXY` `0x483B00`: `BattleActor_UpdateScreenXY`'s
shape, the same 0x95 bytes for the part's record - `Gpu_SetTile1` at the
cursor (never committed), `Gte_RotTransPers` into it, `Gte_StoreDepthF`, the
float x / y through `_ftol` to the words `+0x2E` / `+0x30`.
`EffectKind69_DrawColumn` `0x483080` (cdecl `width, base, spread`): a draw mode
(0x35, dtd 1) in slot 2; seventeen rings `i` = 1..0x11 in the matrix's frame,
built in `Prim_VertexScratch` (x always 0): a ring's y =
`Math_Sin(((+0xB + i) & 0xF) << 8) * R sar 12` with `R` = `base` plus or minus
`Rand() & spread` (s16 each; `Rand`'s bit 0 picks the sign), its z = `-64 i`,
the previous ring's y and z kept as the other two corners. Four semi-transparent
`POLY_G4`s a ring (`Gte_RotTransPers4`, `Gte_PrimDepths4_10B`, slot 2, 0x44),
the band moved by the low word of `width` down, twice up, once up and back,
shaded from `7 * +0xA` (in `0x903858`, its low byte read again for every
shade) and the low byte of `13 * +0xA`, all 1 on the first ring. A ring's quads
are committed only while the float at `0x5C41DC` is below the first quad's
third screen y (or unordered: `fcomp`'s C0). `EffectKind69_DrawGlow`
`0x483970`: `DamageScratch` = `(Rand() & 7) + 3 * +0xA`; eight
semi-transparent `POLY_G3`s in slot 2 round the screen point (`+0x2E`,
`+0x30`): the centre shaded (`+0xA << 3`, `+0xA << 3`, `6 * +0xA`), the rim
points at `a` and `a + 0x200` (`Math_Sin` for x, `Math_Cos` for y, times
`DamageScratch` read again after each call) shaded 1.

### 1.5 Kind 0x6C (`EffectKind6C_Run` `0x483BA0`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind6C_Start` `0x483BC0` | the point (`0xB8000`, `0x548000`, `0x1000000`), the sparks scattered, `+1` up |
| 1 | `EffectKind6C_Live` `0x483C00` | the sparks drawn; none live (`al` 0): a tail `jmp` to `Effect_Release` |

The sparks are sixteen records of 0x28 at `EffectKind30_Shards` `0x92BF80`,
reached through the cursor `0x67626C` (a pointer cell read again for every
access; E3C's kind at `0x484050..` uses it too). `EffectKind6C_ScatterSparks`
`0x483C10` (also called by E3C's `0x484070`): for each record `+4` / `+8` /
`+0xC` the record's point, a distance `(Rand() & 0x7F) << 2` and an angle
`(Rand() & 0x3FF) + 0x200`, the step `+0x14` / `+0x18` = `Math_Cos` / `Math_Sin`
times the distance `sar 7`, `+0` = 1, `+1` = 0, the size `+0x24` 0, `+3` =
0x40, `+0x26` = 7, `+2` = 4. `EffectKind6C_DrawSparks` `0x483D10` (answers
`al`): `EffectGte_LoadMapCamera`, a draw mode (page `Gpu_GetTPage(0, 1, 0x3C0,
0)`, dtd 0) in slot 2; each live record: its state through
`EffectKind6C_SparkStates` by its `+1` (a `call`), its quad, the cursor read
back; `al` 1 when any was live. The states: `EffectKind6C_SparkFly`
`0x483FA0` (the point moves by twice the step, the size grows 0x80; `+2` down,
at 0 `+2` = 0x40 and `+1` up) and `EffectKind6C_SparkFade` `0x484000` (the
point moves by the step; `+3` - the shade - and `+2` down; at 0 the spark
freed). `EffectKind6C_DrawSpark` `0x483DA0` (cdecl, the record; also called by
E3C's `0x4841E0`): a semi-transparent `POLY_FT4`, the point `+4` projected
(`EffectGte_ProjectPoint`) and the size word `+0x24` handed as `(w, h)` and
scaled at that depth into the same two words (`EffectGte_ProjectSize`); the
corners `x - (w sar 1)` and that plus `w`, `y - (h sar 1)` and that plus `h`
(`fild`, `fsubr`, `fiadd`), the depth copied, u 0xE0 / 0xFF, v 0x30 / 0x4F,
`Gpu_GetClut(0x50, 0x1E3)`, `Gpu_GetTPage(0, 1, 0x2C0, 0x100)`, red / green /
blue the record's `+3` where `+0x26` has bit 2 / 1 / 0 (7: grey), slot 2.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-10-03; E4D's `0x48CA90`, called raw, has
a 320-wide fill `widescreen.cpp` does not patch - DIV-0041 lists `0x48CB07`
among the sites not yet moved). Where the original indexes past a table or a
pool ours aborts with a `Fatal` naming the function (the round-nine rule;
nothing in the fuzz reaches it): the dispatchers past their tables (kind 0x69's
past its stack table of three), the parts past their four steps, a spark past
its two states, a member index past `ObjTrio`'s three records, and a spawned
record past the twenty (section 7).

## 3. The tables and the arguments pushed with leftovers

**The tables** (`symbols.toml` `[[data]]`): each the table's own length to the
next table a dispatcher indexes, checked by a raw scan of the image for every
cell address (scratch `refs.py`: each of the ten tables and the byte table is
named by exactly one instruction, its reader's) and against what the states store into the
index byte:

| Table | Count | Indexed by | Ends at |
|---|--:|---|---|
| `EffectKind63_States` `0x6549B8` | 7 | `+1` (`jmp`) | `0x6549D4`, kind 0x65's |
| `EffectKind65_States` `0x6549D4` | 5 | `+1` | `0x6549E8`, four bytes |
| `EffectKind65_ShakeSteps` `0x6549E8` | 4 bytes | `+9 & 3` (`movsx`) | `0x6549EC` |
| `EffectKind67_States` `0x6549EC` | 3 | `+1` | `0x6549F8`, kind 0x69's parts |
| `EffectKind69_Parts` `0x6549F8` | 3 | `+2` (`jmp`) | `0x654A04` |
| `EffectKind69_Part0Steps` `0x654A04` | 4 | `+3` (`call`) | `0x654A14` |
| `EffectKind69_Part1Steps` `0x654A14` | 4 | `+3` | `0x654A24` |
| `EffectKind69_Part2Steps` `0x654A24` | 4 | `+3` | `0x654A34` |
| `EffectKind6C_States` `0x654A34` | 2 | `+1` | `0x654A3C` |
| `EffectKind6C_SparkStates` `0x654A3C` | 2 | a spark's `+1` (`call`) | `0x654A44`, which E3C's `0x484050` indexes |

`band_rows.py` counts runs of code pointers (12 from `0x6549B8`, 5 from
`0x6549D4`, 28 from `0x6549EC`, 25 from `0x6549F8`, 22 / 18 / 14 from the three
step tables, 10 from `0x654A34`, 8 from `0x654A3C`): the tables lie back to
back, so each run goes on into the next. Kind 0x69's state table is not in
`.data`: three immediates on the stack, `kImms482A80`. `EffectKind69_Parent`
`0x676268` is named as data (a pointer); the spark cursor `0x67626C` is left
unnamed for E3C, whose kind uses it too (section 8).

**Arguments with leftovers**:

| Callee | Pushed | Read by the callee |
|---|---|---|
| `EffectKind63_DrawDisc` (states 1..3) | radius `mov ax, [+0x18]` over `Sprite_Current`'s upper half; centre the immediate 0xFF; rim `mov dl, [+0x5C]` over a leftover (or 0xFF) | `movsx edi, word`; `mov al, byte`; `mov bl, byte` |
| `EffectKind69_DrawColumn` (parts 0, 1) | three immediates | width's low word (`sub word, di`; `lea ebp, [edi + edi]` then `bp`), base and spread `movsx word` |

The fuzz lists each with those masks; ours passes the values the callee reads.

## 4. The fuzz (`effect_3b_fuzz.cpp`)

@FUZZ@

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 49 (6,409 bytes against the cut's
  6,764: 38 differ by padding only, none by code); each was read again to its
  last instruction and agrees.
- **Hidden starts**: 40, each an entry by address - a cell of
  `Effect_KindHandlers` (five), of a kind's table (32), or kind 0x69's stack
  immediates (three) - not a case or a shared tail. Their recorded hosts: E3A's
  `0x482360` (eight: kind 0x63's dispatcher and states 0..6), and ours now:
  `0x482740` (13), `0x482C30` (9), `0x483080` (5), `0x483B00` (3), `0x483DA0`
  (2). None of the hosts contains our code as a fall-through: each host ends
  at its own `ret` before the next start (the catalog's extents ran on through
  the padding; `entries_logic.txt` gets the smaller extents, section 11).
- **Tail jumps**: `EffectKind63_WaitCue` and `_FadeOut` end in a `jmp` to
  `EffectKind63_RefreshSprites`, which has its own frame and `ret` and is
  called by `_Shrink`: a function, as the cut has it.
- **The `hypothesis` row** `0x482A00`: kind 0x67's four-instruction dispatcher
  (`Effect_KindHandlers[0x67]`): effect code, taken.
- **Not taken, in the band**: `0x4837B0` (0x1C0 bytes, between
  `EffectKind69_Part2Fade` and `EffectKind69_DrawGlow`) - a catalog part 6 row
  ("Scenario effects (SCE1xEF overlays)", PSX `0x801D218C`, a disputed call
  pair) in no group of this round, called only by `EffectKind69_Part2`: it
  draws `+0xA - 1` semi-transparent `LINE_G2`s in slot 2 from the matrix's
  origin with `Rand`, writing `0x903850..0x90385F` and `Prim_VertexScratch`'s
  first vertex. Called raw; the coordinator's to place.
- **The cut's `unit` / `label` columns**: the units are right for this band
  (each kind's dispatcher and its states); the labels are "Unlabelled" or the
  table's.

## 6. Controls

@CONTROLS_SECTION@

## 7. Latent defects (Capcom's, described, not fixed)

1. **`EffectKind69_Spawn` never tests `Effect_FindFree` for none.** With fewer
   than nine free records the original takes 0xFF as a record and writes kind
   0x69's bytes at `Effect_Objects + 0xFF * 0x80` = `0x7E9160`, outside the
   pool (whatever lies there). Ours aborts with a message instead. Whether
   ordinary play reaches it depends on how many effects are live when chapter
   10's scene spawns the kind (`Scena10_Run2` spawns several kinds around it);
   not traced.
2. **`EffectKind69_Parent` is one cell.** Every part reads its parent through
   `0x676268`; a second kind-0x69 spawner while the first's parts live
   retargets all of them, so the first never counts to nine (it waits forever)
   and the second's `+0xB` is raised by both sets - a test for exactly 9 that
   counts can step past (four parts end in the same frame). Ours keeps the one
   cell as the original does.
3. **`EffectKind67_SpawnKind13` never leaves its state.** It spawns one
   kind-0x13 record a frame for as long as the kind-0x67 record lives (until
   the pool is full, then one whenever a record frees). Faithful; whether that
   is meant is the owner's question, not a fault ours fixes.
4. **The member loops are unchecked.** `EffectKind63_End` and
   `_RefreshSprites` walk `ObjTrio` by `Field_MemberCount` (a byte): a count
   past 3 writes and updates past the three records. Ours aborts past three;
   the count is at most three in play (the party's field objects).
5. **The dispatchers are unbounded** (every kind here, kind 0x69's three
   sub-dispatchers by `+2` / `+3`, the sparks by `+1`); kind 0x69's past its
   stack table of three calls its own saved registers and return address. Ours
   aborts.
6. **The sparks share `EffectKind30_Shards`.** Kind 0x6C writes sixteen
   records of 0x28 from `0x92BF80`; kind 0x30's shards (24 of 0x38) and the
   sparks after them (wave two's E2A, E2B, E2E, E2F) and E3C's kind use the
   same bytes. Two such kinds live at once overwrite each other. Described,
   numbered by none.

## 8. Calls across groups

| Call | From | To | How |
|---|---|---|---|
| `0x48CA90` (E4D, wave four) | `EffectKind63_Shrink`, `_WaitCue`, `_FadeOut` | the full-screen tint in `+0x5D..+0x5F` | raw (`effect_3b_callees.h` `kScreenTint`), a stand-in in the fuzz that logs the record and the three bytes and moves the cursor 0x28 |
| `0x4837B0` (no group: catalog part 6) | `EffectKind69_Part2` | the lines | raw (`kKind69Lines`), a stand-in logging the record, `+0xA`, `+0xB` and filling the cells it writes |
| `EffectKind54_Start` (E2G, merged) | `EffectKind67_States[0]` | | the table's cell, read in place |
| `EGT`'s `EffectGte_ProjectPoint`, `_ProjectSize`, `_LoadMapCamera` | kinds 0x63, 0x6C | | by name |

**Inbound** (for the rebinding pass): E3C's `0x484070` calls
`EffectKind6C_ScatterSparks` `0x483C10` and E3C's `0x4841E0` calls
`EffectKind6C_DrawSpark` `0x483DA0` (E3C merges after this group and calls
them raw until then; `band_rows.py --edges`). The engine reaches the five
kinds only through `Effect_KindHandlers` (`0x6554DC`, `0x6554E4`, `0x6554EC`,
`0x6554F4`, `0x655500`), no raw reference in our source.

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract, shop,
worldmap, combat) are empty for all 49 rows, and no first-call trace under
`analysis/calltrace` has any of them (a grep of the directory finds them only
in the entry lists). The spawners are chapter 8's and chapter 10's scenes and
area 49's handler (above), none on a recorded route. **Fuzz only**; the
coordinator's frame-hash A/B covers the kinds if the owner records a route
through those scenes.

## 10. The rebinding

`band_rows.py --refs`: no raw reference to any of the 49 in `src/game` (and a
grep of `src` for each address finds only `effect_3b*`), so nothing to rebind.
Left raw in our files: `0x48CA90` (E4D's, a later wave) and `0x4837B0` (no
group's), both in `effect_3b_callees.h`, and the spark cursor `0x67626C`
(shared with E3C, section 3). For the coordinator: E3C's raw calls to
`0x483C10` and `0x483DA0` become `EffectKind6C_ScatterSparks` and
`EffectKind6C_DrawSpark` once both have merged.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file, one line per function with the extent
read (none was there with that extent; the hosts' longer lines - `00482740
4ED`, `00482C30 447`, `00483080 728`, `00483B00 10F`, `00483DA0 3C8` - stay, the
smaller extents now beside them): @ENTRIES@.
