# Group E2G: effect kinds 0x15, 0x54, 0x55, 0x57, 0x5A, 0x5B, 0x66 and kind 0x18's sub-kind 0x20

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..11),
wave two, from the round branch's tip `dcef661`. **58 functions ours**
(`src/game/effect_2g.cpp`, shadow name `effect_2g`): the cut table's 56 rows
for E2G (`analysis/round13_cut.tsv`, the band `0x47DBE0..0x47FD80`) and two
starts no list of the cut has - the sub-kind's shared draw `0x47E120` and kind
0x5E's dispatcher `0x47F5D0` (section 5). Each read to its last instruction
with capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
@@FUZZ@@. **Fuzz only**: no recorded route enters any of the 58 (section 9).

| Kind | Functions | Reached through |
|---|--:|---|
| 0x66: a window (area 135's choice spawns it, `+6` the answer) - grows over eight frames, stands with the message and two or three textured pieces, shrinks away | 9 | `Effect_KindHandlers[0x66]` (`0x6554E8`), `EffectKind66_States` `0x654758` (6) |
| 0x18, sub-kind 0x20: two textured strips lowered and raised with story flag 0x2A (`Cond_ByteFE` the step) | 7 | `EffectKind18_States[0x20]` (`0x6540EC`), `EffectKind18Sub20_States` `0x654798` (5) by `+2` |
| 0x15: the second party member's side of a timed exchange on the counters `0x903849..0x90384B` | 10 | `Effect_KindHandlers[0x15]` (`0x6553A4`), `EffectKind15_States` `0x6547C0` (10) |
| 0x54: a field object's side of the same exchange | 11 | `Effect_KindHandlers[0x54]` (`0x6554A0`), `EffectKind54_States` `0x6547E8` (15) |
| 0x55: a thirty-second clock and the count, then the end | 6 | `Effect_KindHandlers[0x55]` (`0x6554A4`), `EffectKind55_States` `0x654824` (3) |
| 0x57: a ring of sixteen shaded quads round the screen's centre | 4 | `Effect_KindHandlers[0x57]` (`0x6554AC`), `EffectKind57_States` `0x65483C` (2) |
| 0x5A: a glowing cylinder on the ground at (10, 0x63) | 3 | `Effect_KindHandlers[0x5A]` (`0x6554B8`), `EffectKind5A_States` `0x654844` (2) |
| 0x5B: area 75's two beams - a textured quad from a point down to the ground beside it | 5 | `Effect_KindHandlers[0x5B]` (`0x6554BC`), `EffectKind5B_States` `0x65484C` (3) |
| 0x5D, 0x5E, 0x5F: dispatchers only (their states are catalog part 6 rows, no group's this round) | 3 | `Effect_KindHandlers[0x5D..0x5F]` (`0x6554C4..0x6554CC`), `EffectKind5D_States` `0x654898` (5), `EffectKind5E_States` `0x6548AC` (4), `EffectKind5F_States` `0x654904` (10) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the eleven dispatchers `evidence`). "Window", "strips", "clock",
"ring", "cylinder", "beam" name the code's shape - the primitives it commits
and the cells it steps - not a play-tested fact: where the game shows these
kinds and what they look like was not traced (section 9; the owner's word,
not this doc's). The spawners found in our source: area 135's choice
(`Area135_ChoiceStartTail14`, `area_w3d.cpp`: kind 0x66, `+6` the answer),
area 8 (`Area08_SpawnEffect54`, `area_w0a.cpp`: kind 0x54, `+0xB` the active
member's object) and area 135's set-up (kind 0x54, `docs/area_w3d.md`), area
145's step hook and chapter 3's first scene (kind 0x15: `docs/area_w3f.md`,
`docs/scena_sc3.md`), chapter 7 (kinds 0x57, "in area 2", and 0x5F:
`scena_sc7.cpp`), areas 104 / 121 and chapter 9b (kind 0x5D). Kind 0x55's
spawner (area 8's, `docs/area_w0a.md` C49) and kind 0x5B's (area 75: its
effect slots `0x93C34E` / `0x93C34F`, `area_w1f_callees.h`) are named in those
groups' docs; no spawner of kind 0x5A or of sub-kind 0x20 was found by a grep
of our source for the kind stored into `+5`. The PSX twins
`analysis/pairs_propagated.json` gives - `0x801F2DD8` for `0x47EFF0` and
`0x801F2E64` for `0x47F040`, both in AREA075's overlay - agree with kind 0x5B
being area 75's; the sibling names neither.

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes) `Sprite_Current` and calls `Effect_KindHandlers[+5]`; each
dispatcher below is `mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx +
1]; jmp [eax * 4 + T]` with no compare (sub-kind 0x20's by `+2`).

### 1.1 Kind 0x66 (`EffectKind66_Run` `0x47DBE0`)

| State | Function | What |
|---|---|---|
| 0 | `EffectKind66_Start` `0x47DC00` | no message (`0x7DEE48` = 0xFFFF), `+9` = 0, `+1` = 1 |
| 1 | `EffectKind66_Open` `0x47DC20` | `+9` up; with `+6` set `Window_DrawOutline` centred on the window's rectangle (`0x654750`: x, y words, w 0xC8, h 0x30) at w = 25 `+9`, h = 6 `+9`; at 8, `+1` = 2 |
| 2 | `EffectKind66_Show2` `0x47DCA0` | with `+6`: `Window_DrawFrame` at the rectangle and `EffectKind66_Message`; pieces (0 at place 0), (2 at place 1) |
| 3 | `EffectKind66_Show3` `0x47DCF0` | the same, pieces (1, 0), (2, 2) |
| 4 | `EffectKind66_Show4` `0x47DD40` | the same, pieces (0, 0), (2, 1), (3, 1) |
| 5 | `EffectKind66_Close` `0x47DDA0` | with `+6` the outline at `+9`; `+9` at 0 a tail jump to `Effect_Release`, else `+9` down |
| - | `EffectKind66_Message` `0x47DE10` | with a message set: `MsgBox_FrameTask` (its `al` unread); `MsgBoxState`'s first byte 2 -> 7 and its `+6` = 0xA; 7 -> `+6` = 0xA |
| - | `EffectKind66_DrawPiece(piece, place)` `0x47DE50` | a POLY_FT4: the corners of place record `0x654770 + 8 place` (three of x, y, w, h s16) as floats, the texture corners of piece record `0x654788 + 4 piece` (four of u, v, w, h), shade 0x80, CLUT `(0, 0x1E8)`, page `(1, 0, 0x240, 0x100)`, committed (1, 0x48) |

Only `_Open` steps `+1` (to 2); states 3..5 are set from outside - area 135's
code, not read here.

### 1.2 Kind 0x18's sub-kind 0x20 (`EffectKind18Sub20_Run` `0x47DFD0`)

`EffectKind18_States[0x20]`: the record's `+5` is 0x18 and `+1` 0x20; this
dispatcher jumps by `+2`.

| Sub-state | Function | What |
|---|---|---|
| 0 | `EffectKind18Sub20_Start` `0x47DFF0` | story flag 0x2A (`Flags_Test(0x904030, 0x2A)`) set: height `+0x3C` = -0x180, `+2` = 3; clear: -0x90, `+2` = 1; the draw |
| 1 | `_WaitSet` `0x47E040` | `Cond_ByteFE` = 1; once the flag is set, sound 0x202 and `+2` up. Nothing drawn |
| 2 | `_Lower` `0x47E070` | `Cond_ByteFE` = 0; `+0x3C` down 8; at -0x180 or below `MoveCmd_TestFB(0x27, 0x2F)` (answer unread) and `+2` up; the draw |
| 3 | `_WaitClear` `0x47E0B0` | `Cond_ByteFE` = 2; once the flag is clear, `MoveCmd_TestFB(0x27, 0x2F)`, sound 0x202, `+2` up. Nothing drawn |
| 4 | `_Raise` `0x47E0F0` | `Cond_ByteFE` = 0; `+0x3C` up 8; at -0x90 or above `+2` = 1; the draw |
| - | `_Draw` `0x47E120` | four rows (z 0x300000 + 0x20000 i) of two shaded POLY_FT4s; vertices built in `Prim_VertexScratch` (x -0x2940, the second's far pair -0x2A40; y 0x100 i - 0x2740 / - 0x2840; z the low word of `+0x3C` and 0x100 above), `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, textures 0x23500125 / 0x23800126, `MapView_LinkPrimAt(0x2C0000, z, 0, 0x48)` |

### 1.3 Kind 0x15 (`EffectKind15_Run` `0x47E2D0`) and kind 0x54 (`EffectKind54_Run` `0x47E680`)

The two kinds step one exchange on three bytes of the chapters' counters:
**the cue** `0x90384A` (1 begin, 2, 3, 5; bit 7 at the end), **the hit**
`0x903849` (1 a hit to take, 2 taken, 0 cleared) and **the count** `0x90384B`
(kind 0x54 raises it; bit 7 - kind 0x55 sets it - ends both). Kind 0x15 works
on `ObjTrio` record 1 (`0x802E8C`, the second member): it points `Field_State`
and `Sprite_Current` at it for `Sprite_SetAnimation` and reads its `+0x58`
(the animation word) and `+0x4A` (1 once the animation has run). Kind 0x54
works on `Sprite_Objects[+0xB]` the same way; its spawner in area 8 stores the
active member's object index there. Every state of both first sends the
record to its end state (9, 0xE) when the count's bit 7 is set.

| Kind 0x15 state | Function | What |
|---|---|---|
| 0 | `_Start` `0x47E2F0` | `+9` = 0, the wait byte `0x67625C` = 0, `+1` = 1 |
| 1 | `_Begin` `0x47E310` | the member's `+0x124` bit 6 set, animation 0x10, the cue 1, `+1` = 2 |
| 2 | `_WaitPose4` `0x47E370` | a hit: cue 5, hit 2, `+1` = 6. Else the member's animation 4 run through: the wait from `0x6547AC[Rand() & 7]`, `+1` = 3 |
| 3 | `_Countdown` `0x47E410` | a hit as state 2; else the wait down, at 1 or 0: cue 2, animation 1, the wait from `0x6547B8[Rand() & 3]`, `+1` = 4 |
| 4, 8 | `_WaitCue3` `0x47E4B0` | the cue at 3: `+1` = 5 |
| 5 | `_Cooldown` `0x47E4E0` | the cue 0; the timer word `0x8034E6` at 0xFF: cleared and the wait taken as 2; the wait down, at 1 or 0 the hit cleared and `+1` = 1 |
| 6 | `_Hit` `0x47E540` | `+9` = 0; the member's bit 6, sound 0x20E, animation 0x57, `+1` = 7 |
| 7 | `_WaitPose8` `0x47E5B0` | the member's animation 8 run through: animation 1, the wait from `0x6547B8`, `+1` = 8 |
| 9 | `_End` `0x47E630` | animation 1; bit 6 of `+0x124` cleared on the record `Field_State` names after the call; `Effect_Release` |

| Kind 0x54 state | Function | What |
|---|---|---|
| 0 | `_Start` `0x47E6A0` | `+9` = 0, `+1` = 1 (also kind 0x67's state 0) |
| 1 | `_Arm` `0x47E6C0` | the cue at 1: the object's `+0` bit 6 cleared, `+1` = 2 |
| 2 | `_Cue` `0x47E710` | cue 2: animation 3, `+1` = 3; cue 5: animation 3, `+1` = 0xA |
| 3 | `_Strike` `0x47E790` | a hit to take: the count up, hit 2, sound 0x20D, the record `Sprite_Current` names after it at animation 4 or below sets the timer word `0x8034E6` = 0xFF; animation 2, `+1` = 7. Else animation 8 run through: animation 4, `+1` = 4 |
| 4, 0xB | `_WaitPose2` `0x47E850`, `_WaitPose2B` `0x47E9E0` | animation 2 run through: `+1` = 5 / 0xC |
| 7 | `_WaitPose8` `0x47E8A0` | animation 8 run through: `+1` = 8 |
| 5, 8, 0xC | `_Rearm` `0x47E900` | the object's bit 6 set, the cue 3, animation 1, `+1` = 1 |
| 6, 9, 0xD | `BareRet` | nothing |
| 0xA | `_Recoil` `0x47E980` | animation 8 run through: animation 4, `+1` = 0xB |
| 0xE | `_End` `0x47EA30` | the object's bit 6 set, animation 1, `Effect_Release` |

### 1.4 Kind 0x55 (`EffectKind55_Run` `0x47EA90`)

| State | Function | What |
|---|---|---|
| 0 | `_Start` `0x47EAB0` | the frames `+0x5D` = 0, the seconds `+0x5E` = 30, `+1` up |
| 1 | `_Tick` `0x47EAD0` | the clock and the count drawn; the frames down, below 0 (s8) back to 0x1D and the seconds down; the seconds below 0: both 0, `+1` up |
| 2 | `_Finish` `0x47EB30` | the count at 16 or more: bit 7 on the count and the cue; else on the count; `Music_FadeOutStop(0xA)`, `Effect_Release` |
| - | `_DrawTime` `0x47EB90` | a box `0x586160(0x78, 0x38, 0x46, 0x14, 1)`; `Crt_sprintf` of the seconds and the hundredths (frames * 100 / 30, toward zero) by the format `0x654830`; `Text_DrawFont12` at (0x7D, 0x3B); then the text made "." and drawn at (0x95, 0x39) and (0x95, 0x3D) |
| - | `_DrawCount` `0x47EC30` | a box `0x586160(0xC8, 0xA0, 0x2E, 0x14, 1)`; the count by `Boss26Fx_CountFormat` at (0xCD, 0xA3); the text `0x66A094` by `Text_DrawAt` at (0xE5, 0xA4) |

### 1.5 Kinds 0x57, 0x5A, 0x5B

| Kind, state | Function | What |
|---|---|---|
| 0x57, 0 | `EffectKind57_Show` `0x47ECC0` | the ring drawn; outside area 2 (`Game_AreaNumber`), `+1` up |
| 0x57, 1 | `EffectKind57_Release` `0x47ECE0` | `jmp Effect_Release` (`Effect_StateRelease`'s five bytes again) |
| 0x57 | `EffectKind57_DrawRing` `0x47ECF0` | a draw mode (page `(0, 2, 0x3C0, 0x100)`, dtd 1); sixteen semi-transparent POLY_G4s round (0xA0, 0x78) at angles 0x100 k: inner (80 cos, 60 sin) >> 12, outer (240 cos, 180 sin) >> 12, each an s16; black inside, white outside |
| 0x5A, 0 | `EffectKind5A_Place` `0x47EEC0` | the point (0xA0000, 0x630000), its height the ground's (`AreaMap_Elevation`'s word << 16), `+1` up. Also kind 0x42's state 0 |
| 0x5A, 1 | `EffectKind5A_Draw` `0x47EF10` | `Area146_DrawGlowCylinder` at the record's point |
| 0x5B, 0 | `EffectKind5B_Start` `0x47EF60` | area 75's slot `0x93C34E + +0xB` = the record's index; `+0xA` = 0x80, `+0xC` = 0, `+6` = 0x1E, `+0x5D` = 0x80, `+1` up |
| 0x5B, 1 | `EffectKind5B_Glow` `0x47EFB0` | `+0xA` plus the byte `+0xC`, the dword `+0xC` = 0 (area 75's `Area75_DrainEffects` steps it); the beam (`+6`, blend 0) |
| 0x5B, 2 | `EffectKind5B_Fade` `0x47EFF0` | `+0x5D` down 2, at 0 `Effect_Release` and on; every 32 steps `+6` up; the beam (`+6`, blend 1) |
| 0x5B | `EffectKind5B_DrawBeam(length, abr)` `0x47F040` | a texture window (0xD0, 0x40, 0x10, 0x10) linked at the record's point; a POLY_FT4 from the point (x, z -/+ 0x8000, `+0x3C` + 0xE00000) to the ground at x - (length << 15), each corner by `EffectGte_ProjectPoint`; CLUT (0xD0, 0x1E3), page (0, 1, 0x2C0, 0x100), u from `+0xA & 7`, shade `+0x5D`, blend `abr`; linked (0x48); the window put back (0, 0, 0x100, 0x100) |

### 1.6 Kinds 0x5D, 0x5E, 0x5F

Dispatchers only: `EffectKind5D_Run` `0x47F2B0`, `EffectKind5E_Run`
`0x47F5D0`, `EffectKind5F_Run` `0x47FD80`. Their states (`0x47F2D0..`,
`0x47F5F0..`, `0x47FDC0..`) are catalog part 6 rows ("Scenario event banks"),
in no group of this round; `EffectKind5F_States[0]` is `Task_StartHold60`
(ours, `magic_engine.cpp`) and its entries 7..9 E3A's.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-09-29). Where the original indexes past a
table ours aborts with a `Fatal` naming the function (the round-nine rule;
nothing in the fuzz reaches it): the eleven dispatchers past their tables,
`EffectKind66_DrawPiece` past its three places or four pieces, kinds 0x54's
states when `+0xB` names no object of the thirty (only where the object is
read, written or animated - where the original only stores its address in
`Sprite_Current` and puts it back, ours does the same), and
`EffectKind5B_Start` past area 75's two slots.

## 3. The tables and the arguments pushed with leftovers

**The tables** (`symbols.toml` `[[data]]`): each the table's own length to the
next table a dispatcher indexes, or to the first dword that is not code -
checked by a raw scan of the image for every cell address (scratch
`cellscan.py`) and against what the states store into `+1`:

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind66_States` `0x654758` | 6 | `0x654770`, the place records |
| `EffectKind18Sub20_States` `0x654798` | 5 | `0x6547AC`, kind 0x15's waits |
| `EffectKind15_States` `0x6547C0` | 10 | `0x6547E8`, kind 0x54's (the run of code pointers goes on to 28) |
| `EffectKind54_States` `0x6547E8` | 15 | `0x654824`, kind 0x55's; `+1` reaches 0xE |
| `EffectKind55_States` `0x654824` | 3 | `0x654830`, the clock's format |
| `EffectKind57_States` `0x65483C` | 2 | `0x654844`, kind 0x5A's |
| `EffectKind5A_States` `0x654844` | 2 | `0x65484C`, kind 0x5B's |
| `EffectKind5B_States` `0x65484C` | 3 | `0x654858`, data a part-6 row reads |
| `EffectKind5D_States` `0x654898` | 5 | `0x6548AC`, kind 0x5E's (the tool's run says 9) |
| `EffectKind5E_States` `0x6548AC` | 4 | `0x6548BC`, bytes |
| `EffectKind5F_States` `0x654904` | 10 | `0x65492C`, where E3A's dispatcher `0x4801F0` indexes (the run says 16) |

**Arguments with leftovers**:

| Callee | Pushed | Read by the callee |
|---|---|---|
| `Window_DrawFrame` (from states 2..4) | x `cx`, y `ax`, w `dl`, h `cl` over the caller's upper bytes | x, y s16, w, h bytes (`symbols.toml`) |
| `Window_DrawOutline` (states 1, 5) | x, y whole (computed), w = 25 and h = 6 times a register whose upper half is `Sprite_Current`'s (state 1) or the dispatcher's `eax` (state 5) | w, h `and 0xFF` / `mov cl, bl`; x and y passed on whole |
| `EffectKind5B_DrawBeam` | length `movzx cx` / `movzx ax` of `+6` over a leftover upper half | `and edi, 0xFFFF`; abr `and ecx, 0xFF` |
| `EffectKind66_DrawPiece` | immediates | `and eax, 0xFF` each |

The fuzz lists each with those masks; ours passes the values the callee reads
(and computes the outline's x and y whole, as the original).

## 4. The fuzz (`effect_2g_fuzz.cpp`)

@@FUZZSECTION@@

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 56 to the byte (5,389 bytes against the
  cut's 5,836: 48 differ by padding only, none by code), with one exception
  settled by hand: `0x47E0F0`'s extent 0x1D2 runs on through its `jmp
  0x47E120` into the code after it, which sub-states 0 and 2 also tail-jump to
  (`band_rows.py`'s clone table lists `0x47E120` as "Capcom's raw" for them).
  **`0x47E120` is taken as a function of its own** (`EffectKind18Sub20_Draw`,
  0x1A2 bytes; [`scenario_harness.md`](scenario_harness.md) 8.5 names it
  among the shared tails): three states reach it by address, so it is an entry,
  not a tail of one; `_Raise`'s clone ends at its `jmp` (0x2C).
- **Hidden starts**: 50, each an entry by address - a cell of
  `Effect_KindHandlers`, of `EffectKind18_States` or of a kind's table - not a
  case or a shared tail. Their recorded hosts: E2F's `0x47DAC0` (kind 0x66's
  seven), `0x47DE50` (its catalog extent 0xD34 spans `0x47DE50..0x47EB84`: 31
  hidden starts of ours), `0x47EC30`, `0x47ECF0`, `0x47F040` (ours now) and `0x47FBE0`
  (catalog part 6). None of the hosts contains our code as a fall-through.
- **The `hypothesis` rows** (`0x47E680`, `0x47EF40`, `0x47F2B0`, `0x47FD80`):
  each is the four-instruction dispatcher of its kind
  (`Effect_KindHandlers[0x54]`, `[0x5B]`, `[0x5D]`, `[0x5F]`): effect code,
  taken.
- **Added: `0x47F5D0`**, kind 0x5E's dispatcher (`Effect_KindHandlers[0x5E]`
  `0x6554C8`), in the band but in no list of the cut (the catalog files it as a
  part 6 row, "Scenario event banks", PSX twin `0x801F74F8` by a gap); the
  addendum's rule for a dispatcher in the band that no list holds. Its table
  starts inside kind 0x5D's run of code pointers, which is how kind 0x5D's
  table has five entries, not the tool's nine.
- **Not taken, in the band**: `0x47F2D0..0x47FD7F` less `0x47F5D0` - the states
  of kinds 0x5D and 0x5E and their callees (`0x47F2D0`, `0x47F340`,
  `0x47F3E0`, `0x47F550`, `0x47F5F0`, `0x47F610`, `0x47F720`, `0x47F7A0`,
  `0x47F910`, `0x47F9E0`, `0x47FAF0`, `0x47FBE0`), catalog part 6 rows in no
  group of this round: the coordinator's to place.
- **The cut's `unit` / `label` columns**: right for this band. `0x47EFF0` and
  `0x47F040` carry "Area overlays, world 1" and the unit "Fn_47ECA0 kind 0x57"
  (their twins are in AREA075); they are kind 0x5B's.

## 6. Controls

@@CONTROLS@@

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x15 leaves `Sprite_Current` on the member.** `_WaitPose4` and
  `_WaitPose8` point `Field_State` and `Sprite_Current` at `ObjTrio` record 1
  before testing its animation, and return without putting `Sprite_Current`
  back when the animation has not run through (every frame until it has). The
  frame's next record is unaffected - `Effect_RunObjects` sets
  `Sprite_Current` for each - but when the kind-0x15 record is the last live
  one, whatever reads `Sprite_Current` after the effect pass sees the member,
  not an effect record. `Field_State` is left on the member on every path of
  kind 0x15 (the other states do not put it back either). Ours does the same.
- **Kind 0x5B draws a released record.** `_Fade` calls `Effect_Release` when
  `+0x5D` reaches 0 and goes on: it steps `+6` and draws the beam once more from
  the freed record (whose `+0..+4` are 0 and the rest as it was).
- **Unchecked indexes**: kind 0x54's `+0xB` into the thirty `Sprite_Objects`
  (area 8 stores the active member's index, which is only in range while that
  member is a field object of the pool), kind 0x5B's `+0xB` into area 75's two
  slots, the eleven dispatchers' state bytes. Every writer of `+1` / `+2` in the
  band steps it inside its table; ours aborts past any of them.
- **Sub-kind 0x20 draws nothing while it waits**: `_WaitSet` and `_WaitClear`
  (sub-states 1 and 3) do not call the draw, so the strips are drawn only while
  they move (sub-states 0, 2, 4). Whether something else draws them at rest is
  not read here; it may be the intent.
- **Kind 0x57 never ends in area 2**: `_Show` steps to its release only
  outside area 2, so there the ring is drawn until something else frees the
  record.
- **Kind 0x66's states 3..5 are set from outside** and its dispatcher does
  not bound them; a writer past 5 would jump into the place records.

## 8. Calls across groups

**Outbound, raw**: none to a group of this round (`band_rows.py --edges`: the
four E2G -> EGT edges are `EffectGte_ProjectPoint`, ours and called by name).
By address, Capcom's and in no group: `0x586160` (the menu box kind 0x55
draws, 2 sites) and `0x5A7840` (the texture-window primitive, 2 sites) - both
effect-standard rows of the harness. By name, already ours: `Window_*`,
`MsgBox_FrameTask`, `Flags_Test`, `MoveCmd_TestFB`, `Sprite_SetAnimation`,
`Effect_Release`, `Music_FadeOutStop`, `Text_*`, `AreaMap_Elevation`,
`Area146_DrawGlowCylinder` (AR3F's), the `Gpu_*` / `Gte_*` / `Math_*`
primitives, and FC1's `Effect_StateRelease` and magic's `Task_StartHold60`
through the tables.

**Inbound from outside the group** (for the rebinding pass): none by call.
`Effect_RunObjects` reaches the ten kind dispatchers through
`Effect_KindHandlers` and kind 0x18's `EffectKind18_Run` the sub-kind's through
`EffectKind18_States[0x20]` (read in place). Two tables of other groups hold
our states: **kind 0x42's `0x65449C[0]`** (E2C's dispatcher `0x475CA0`) is
`EffectKind5A_Place`, and **kind 0x67's `0x6549EC[0]`** (E3B's `0x482A00`) is
`EffectKind54_Start`. The spawners that store our kinds are listed at the top.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 57 rows of the band it lists, and neither first-call
trace (`analysis/calltrace/reach_dragon`, `reach_whelp`: a grep of every
file for the 58 addresses) names any. **Fuzz only.** No live run was made (the
brief). Which scene shows which kind is the owner's to say; a recorded walk
through area 8, 75 or 135 (or chapter 3's first scene, area 145) would let the
coordinator's frame-hash A/B cover these kinds.

## 10. The rebinding

`grep -rn -i` of the 58 addresses and the eleven tables' in `src/game`
(`band_rows.py --refs` found one): **rebound**, `magic_engine.cpp`'s comment
on `Task_StartHold60` - "the .data table 0x654904 another task (0x47FD80, no
group's) jumps through" now names `EffectKind5F_States` and
`EffectKind5F_Run` (the one line, addresses kept beside the names). **Left
raw**: `area_w0c_callees.h` and `area_w3f.cpp` cite `0x47EF32` - a call site
inside `EffectKind5A_Draw`, not a function's address, still right as a site;
and `symbols.toml`'s `Task_StartHold60` evidence says "0x47FD80 (no group's)" -
another entry's evidence, left for the coordinator. No constant, call site or
fuzz key of another file names an E2G address; `field_o_callees.h`'s
`kFmtTime = 0x654830` is the clock's format (data, shared with the save
screen's play time), not ours to name.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): 56 lines, the read extents
of the 58 less two already listed right (`0047DE10 34`, `0047EB90 97`). They
correct four host lines left in place: `0047DE50 D34` (the code is 0x17B),
`0047EC30 B5` (0x61), `0047ECF0 348` (0x1A8) and `0047F040 282` (0x26D) - each
old line spans the hidden starts after its host's `ret`.
