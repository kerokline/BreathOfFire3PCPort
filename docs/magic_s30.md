# Group S30: Venom and KaiserBreath (MAGIC130, MAGIC131)

**Status:** IN PROGRESS (2026-09-26). All 60 functions are ours
(`src/game/magic_s30.cpp`, shadow name `magic_s30`), fuzzed headless through
the shared harness: 0 mismatches over 120,000 rounds. There are 169 negative controls, and every one is refused (exit 3): 167 by a
count in the functions the plant touches, two by ours' own abort past a table
(each with a variant refused by a count).
Nothing recorded casts these spells, so this is fuzz only until the owner
sees them cast.

Round nine, second wave, group S30
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|---|---|---|---|---|---|--:|
| 141 | 0x290 | MAGIC130 | 0x82 | Venom | `0x4E4420..0x4E4C45` | 13 (2,007 bytes) |
| 144 | 0x291 | MAGIC131 | 0x83 | KaiserBreath | `0x4E4C50..0x4E6944` | 47 (7,092 bytes) |

Extents and starts are `tools/magic_rows.py --unit MAGIC130 / MAGIC131
--clones` (capstone recursive descent; every jump internal, no jump table,
nothing `REFUSED`). None was ours; none was found inside or missing from the
extents. The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) and are hypotheses; the function
names describe what the code does. What either spell is in play is not
measured and not stated here.

## 1. What each function does

`symbols.toml` has each function to the instruction. In outline:

**MAGIC130 (Venom).**

- `Venom_Task` (kind 2): a stack table by `+1` - `Venom_Start`, MAGIC226/227's
  `0x4F9F70`, `BattleFx_Finish`.
- `Venom_Start`: the task at the side's centre (`MagicFx_CenterOnSide`);
  one kind-1 child `0x66` of kind 0 (a sprite) and six of kind 1 (rings,
  `+0xB` their index); CLUT row 26 from its source with the stp bit, then its
  first entry again without; sounds `0x100`, `0x101`.
- `VenomChild_Task` dispatches by `+1` through `Venom_ChildKinds`.
- The sprite (`VenomSprite_Run`, shared with MAGIC126 and 129): the sprite
  bank `0x9039D8` at `0x8E3580` around the phase, then `0x8B3580`; phases
  `_Start` (screen `(0xDC, 0x6E)`, the control fields, shade `0x80`),
  `_ShadeUp` (+8 to `0`), `MagicFx_CountDown9`, `_ShadeDown` (-8 to `0x80`,
  then the owner's count down and free).
- The ring (`VenomRing_Run`, shared with MAGIC126 and 129): phases `_Start`
  (screen x `26 i + 0x98`, y `VenomRing_ScreenY[i] + 0x50`, radius `0x20`),
  `_Grow`, `MagicFx_CountDown9`, `BlizzardShard_End` (S22's); a draw mode, then
  `_DrawFan` (eight gouraud triangles round the point) and `_DrawBand` (eight
  gouraud quads between the radius and twice it plus `Rand & 7`).
- `MagicFx_CountDown9` (`0x4E47F0`): `+9` down, `+2` on at 0.

**MAGIC131 (KaiserBreath).**

- `Kaiser_Task` (kind 2): an eight-step stack table by `+1`, then a draw mode,
  every live mote of its pool run, a draw mode:
  1. `Kaiser_Start`: every mote's `+0..+2` cleared; the side's centre; a
     child of kind 1 (the flash); CLUT row 26 with the stp bit; sound `0x102`.
  2. `Kaiser_LoadFormFile`: after `+9`, `LoadDatFile(0x228 / 0x227 / 0x229)`
     by `0x904B89` 7 / 8 / other.
  3. `Kaiser_HideParty`: once `File_LoadDone`, the three members' `+0`
     cleared, a child of kind 0 (the sprite), CLUT row 1 from its source.
  4. `Kaiser_WaitChildren`: when the children are gone, another flash.
  5. `Kaiser_ReloadParty`: after `+9`, if a member has `+0x89` 4 and `+0x134`
     bit 0, a file by the party set `0x90412C` (7, 0xD, 0xE, 0xF) and
     `0x904AAC` bit 1; else `PartySet_Select(set & 0x7F, 2 or 1)`.
  6. `Kaiser_LoadSetFile`: once loaded, `LoadDatFile` of the word the table
     `0x64E9BC` gives for `0x904B89`, `0x904AAC` bit 1 and the party set.
  7. `Kaiser_ShowParty`: once loaded, each member's sprite on, its animations
     set (standing, the `+0x90` / `+0x91` states, out).
  8. `MagicFx_EndWhenChildrenDone`.
- `KaiserChild_Task` dispatches by `+1` through `Kaiser_ChildKinds`:
  - **the sprite** (`KaiserSprite_Run`: bank `0x813580`; every fourth frame
    nine CLUT entries of row 1 cycle through the source): `_Start` (a
    sixteen-step glide run backwards from a start point by the facing),
    `_Glide`, `_Brake`, `_Tick`, `_Breathe` (sound, target flag `0x10`, the
    pillar and ring children, 8 + 28 motes), `_WaitScript`, `_Leave` (flag
    `0x40`), `_Exit`;
  - **the flash** (`KaiserFlash_Run`): `MagicFx_ClearCount9`,
    `MagicFx_CountUp9By2`, `_WaitLoad` (the owner's `+9` 0 and the file
    loaded), MAGIC078's `MagicFx_WaitA`, MAGIC060's `0x4B1740`; `_Draw`, a
    semi-transparent flat quad over the screen, shade `+9 x 15`;
  - **the pillar** (`KaiserPillar_Run`, `_Start`, `_Grow`, `_Fade`, `_Draw`:
    eight rings of sixteen textured quads, fourteen drawn);
  - **the ring** (`KaiserRing_Run`, `_Start`, `_Grow`, `_Fade`, `_Draw`: sixteen
    textured quads, ten drawn, blend mode `+0xB`).
- **The motes**: a pool of 48 records of `0x2C` at `0x6A4E38`
  (`KaiserMote_Pool`), the one being run at `0x6A5678` (`KaiserMote_Current`).
  `KaiserMote_Task` dispatches by the mote's `+1`: kind 0 (`KaiserMoteA_*`,
  eight, angles by `<< 7`) and kind 1 (`KaiserMoteB_*`, 28, angles `<< 6`, a
  lift and a delay `+0x26`); each starts at radius 64 round
  `(Field_Kind2X, Field_Kind2Z)`, spreads to `0x200` and fades. Each draws its
  screen point (`KaiserMote_UpdateScreenXY`, BattleActor_UpdateScreenXY's
  shape on the mote's fields) and a fan of eight gouraud triangles
  (`KaiserMote_Draw`). `KaiserMote_Alloc` / `_Free`.
- The shared bodies: `MagicFx_EndWhenChildrenDone` (`0x4E5200`),
  `MagicFx_ClearCount9` (`0x4E5930`), `MagicFx_CountUp9By2` (`0x4E5950`).

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with the project's
precedent exceptions ([`magic_fx_reached.md`](magic_fx_reached.md) §3; round
nine §7):

- a phase past any of the thirteen stack or `.data` dispatch tables aborts;
- `Kaiser_LoadSetFile` aborts where the original reads a file index through a
  null entry of `0x64E9BC` (section 8), where the original faults.

Two calls push one argument more than the callee's prototype takes, as the
originals do: `Gte_RotTransPers4` gets its flag pointer, `Gte_RotTransPers`
its flag pointer.

## 3. The shared bodies other groups call by address

These are called by raw address from first-wave groups (in stack tables or
`.data` tables); after the merge they can be bound to these names:

| Address | Name | Prototype | Called raw by |
|---|---|---|---|
| `0x4E47F0` | `MagicFx_CountDown9` | `extern "C" void __cdecl MagicFx_CountDown9(void)` | S20 (`Magic088_WaveRun`'s table), S22 (`BlizzardShard_Run`), S25 (`SpellRagnarok_SpritePhases`) |
| `0x4E5200` | `MagicFx_EndWhenChildrenDone` | `extern "C" void __cdecl MagicFx_EndWhenChildrenDone(void)` | S16 (`Magic074_Task`), S18 (`Buff_Task`), S19 (`Shield_Task`), S20 (MAGIC088 / 092 tasks), S22 (`Jolt_Task`, `Lightning_Task`) |
| `0x4E5930` | `MagicFx_ClearCount9` | `extern "C" void __cdecl MagicFx_ClearCount9(void)` | MAGIC060 and 064's tables (groups S12, S14: not yet taken) |
| `0x4E5950` | `MagicFx_CountUp9By2` | `extern "C" void __cdecl MagicFx_CountUp9By2(void)` | sixteen files' tables (`analysis/magic_funcs.tsv`) |

All four read and write only `Sprite_Current` (`+2`, `+9`, `+0xB`), the
effect-done bit `0x904AA8 | 4` and `BattleTask_FreeCurrent` (the last two
`0x4E5200` only).
Also shared, not called by name by anyone yet: `VenomSprite_Run`,
`VenomSprite_Start` .. `_ShadeDown`, `VenomRing_Run` .. `_DrawBand` are reached
from MAGIC126 and 129 (groups S29 and C2).

## 4. Calls into units not ours (raw addresses)

| Address | Owner | Reached as |
|---|---|---|
| `0x4F9F70` | MAGIC226/227 (S38) | `Venom_Task`'s stack table, entry 1 |
| `0x4B1740` | MAGIC060 (S12) | `KaiserFlash_Phases` entry 4 (a `.data` cell) |

`MagicFx_WaitA` (S17) and `BlizzardShard_End` (S22) are ours and sit in
`.data` tables; the engine and library callees are called by name. The
CRT's `_ftol` (`0x5B9550`) is inlined as `battle_items.cpp`'s `Ftol16`.

## 5. The fuzz

`BOF3X_SHADOW=magic_s30`, `magic_s30_fuzz.cpp`, one `magic_harness::Run`
(the consolidated harness, no edits):

- **callees** beyond the standard set: the GPU / GTE layer of the draws,
  `Math_Sin` / `Math_Cos`, `Sprite_SetAnimation`, `Sprite_ScriptTick`,
  `LoadDatFile`, `File_LoadDone` (`kBool`: its callers test all of eax),
  `PartySet_Select`, `Battle_ActorIsOut`; the group's own called directly
  (`kPhase`, the mote functions logging `KaiserMote_Current`);
  `KaiserMote_Alloc`'s recorder answers `0xFF` or `0..0x2F`; `_ftol` is
  `kThrough` (the copy calls it, ours computes);
- **effects**: `Gfx_CommitPrim` logs the packet buffer at every commit;
  `Gte_RotTransPers4` / `Gte_RotTransPers` write screen floats (small,
  fractional, huge, NaN) where the real ones write, and `Gte_StoreDepthF` a
  depth, so the mote's truncation to its screen point is compared;
  `Sprite_UpdateScreen` logs the sprite bank `0x9039D8` (without it controls
  V9 and K32 were not refused: the bank is put back before the phase ends);
- **`deref`**: the projections' vertices (6 bytes each);
- **`ret_mask 0xFF`** on `KaiserMote_Alloc`;
- **`.data` tables**: the eleven dispatch tables of section 7;
- **regions**: the scratch `0x903850` and `Prim_VertexScratch`,
  `Gfx_PacketNext` and a 512-byte buffer of the fuzz's own, `0x9039D8`, CLUT
  rows 1 and 26 and their sources (`0x80B980` 0x220 bytes: the sprite's
  cycle reads up to 0x107 words in), the mote pool and its current cell,
  `Field_Kind2Z` / `X`, `Field_MemberCount`, `0x904B89`, `0x90412C`;
- **the seed**: every round `Gfx_PacketNext` into the buffer, every mote's
  owner `+0x28` a real slot or record (`Kaiser_Task` makes it the owner, which
  a recorder writes through), the current mote a record, `0x904B89` from the
  entries of `0x64E9BC` that are not null, a party set among 7 / 0xD / 0xE /
  0xF (and others, bit 7 a quarter of the time), 0..4 members, the facing
  0..3; then per function the dispatch indices inside their tables, every
  count-down one or two from its end, the shade / radius / count boundaries,
  the members' `+0x89` 4 and `+0x134` bit 0, `+0x90` / `+0x91` bits, the frame
  counter's low bits, a full or part-full pool for the alloc;
- **the disturbance** of the group's cells: `Gfx_PacketNext`, a scratch or
  vertex word, the current mote moved or one of its fields (never its
  owner), `Field_Kind2*`, the facing, the party set, the member count (0..4),
  `0x904B89` (valid entries only), a sprite field the harness leaves alone.

Result in this worktree (2026-09-26):

    shadow      magic_s30 self-test: 120000 rounds over 60 functions (2000 each), 3703482 calls to the stand-ins,
                0 MISMATCHES; 16144 bytes of state (22 regions) and the stand-ins' log compared

Every callee and handler the clones name is reached (the coverage line), e.g.
`KaiserMote_Task` 47,918 calls from the walk, `PartySet_Select` 394,
`Gte_RotTransPers` 2,000. `BOF3X_SHADOW='*'`: exit 0.

One lesson for the harness: a group's `disturb` must draw only from the hash
it is given (or `Noise`), never `mh::Next` - the first run drew
`0x904B89` / `0x90412C` values with `Next` inside `disturb` and mismatched in
6,517 rounds, the two passes seeing different values.

## 6. Controls

169 plants, each put in `magic_s30.cpp` one at a time by a script not
committed (the scratch `controls.py`: replace a unique anchor, build, run
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s30`, restore; rebuilt after the
last). All 169 were re-run after the `Sprite_UpdateScreen` effect was added,
and **all are refused** (exit 3). The figures are mismatched rounds out of
2,000, in this worktree.

- V8 and M1 (a dispatch by `+2` for `+1`) are refused only by ours' own
  abort past the two-entry table; their variants V8b and M1b (`+1 ^ 1`) are
  refused by a count.
- V9 and K32 (the sprite bank's value) were **not refused** before the
  `Sprite_UpdateScreen` effect (section 5): the bank is put back before the
  phase returns, and nothing else sees it.
- The thinnest are the re-reads across a call, seen only when the
  disturbance moves that cell in that call: M4 (the cosine's angle re-read
  from the scratch, 1..2 rounds per mote function), K93 (the ring's new x
  re-read after the cosine, 2), M27 (the mote re-read after the second
  `Rand`, 7), K15 (a child count of exactly 1, 6), K34 (`+4` exactly 0x7A, 27),
  M29 (the pool's last record, 20).

Re-run 2026-09-26 on the kFlag-fixed harness
([`magic_harness.md`](magic_harness.md) §8): 21 controls in the affected
functions, 21 refused. The section-8 functions are `Kaiser_HideParty`,
`Kaiser_LoadSetFile`, `Kaiser_ShowParty`, `KaiserSprite_Tick`,
`KaiserSprite_Breathe`, `KaiserSprite_WaitScript` and `KaiserFlash_WaitLoad`;
their controls K11..K13, K22..K28, K46..K54, K63 and K64 were re-planted from
the table (a scratch script: plant, rebuild, check the file recompiled,
self-test, restore) and each refused by a count in its own function only.
The other 148 plant in functions that reach no `kFlag` / `kBool` recorder
and stand. The thinnest: K53 (the owner read before the child) 52, as
before; K27 212 (was 274), K25 424, K28 295 (was 265), K22 325. No fuzz
change. Clean self-test after the last: 0 mismatches, exit 0.

| | Planted | Refused in |
|---|---|---|
| V1 | Venom_Task: entries 0 and 2 swapped | Venom_Task 1,369 |
| V2 | Venom_Start: +9 0x1F | Venom_Start 1,915 |
| V3 | Venom_Start: the sprite child +1 1 | Venom_Start 1,785 |
| V4 | Venom_Start: ring +0xB i + 1 | Venom_Start 2,000 |
| V5 | Venom_Start: CLUT row 26 first entry keeps the stp bit | Venom_Start 1,036 |
| V6 | Venom_Start: second sound 0x102 | Venom_Start 2,000 |
| V7 | Venom_Start: the count up before the child fields (a child in the task's own slot) | Venom_Start 227 |
| V8 | VenomChild_Task: by +2 | ours' own abort: VenomChild_Task: phase 2, past the 2-entry table |
| V9 | VenomSprite_Run: bank 0x8E3581 | VenomSprite_Run 542 |
| V10 | VenomSprite_Run: +0 whole, not bit 0 | VenomSprite_Run 577 |
| V11 | VenomSprite_Start: +0xA 0xFE | VenomSprite_Start 2,000 |
| V12 | VenomSprite_Start: x 0xDD | VenomSprite_Start 1,999 |
| V13 | VenomSprite_Start: +0x5C 0 | VenomSprite_Start 2,000 |
| V14 | VenomSprite_Start: +0x29 4 | VenomSprite_Start 2,000 |
| V15 | ShadeUp: +0x5F by 7 | VenomSprite_ShadeUp 2,000 |
| V16 | ShadeUp: on at 8 | VenomSprite_ShadeUp 453 |
| V17 | ShadeDown: at 0x88 | VenomSprite_ShadeDown 890 |
| V18 | ShadeDown: +0x5E by -7 | VenomSprite_ShadeDown 2,000 |
| V19 | VenomRing_Run: band before fan | VenomRing_Run 1,159 |
| V20 | VenomRing_Run: tpage 0x75 | VenomRing_Run 2,000 |
| V21 | VenomRing_Start: x step 27 | VenomRing_Start 1,769 |
| V22 | VenomRing_Start: y + 0x51 | VenomRing_Start 2,000 |
| V23 | VenomRing_Start: radius 0x21 | VenomRing_Start 2,000 |
| V24 | VenomRing_Grow: at 0x11 | VenomRing_Grow 873 |
| V25 | MagicFx_CountDown9: at 1 | MagicFx_CountDown9 1,344 |
| V26 | DrawFan: rim green x 8 | VenomRing_DrawFan 1,991 |
| V27 | DrawFan: second point x from the y | VenomRing_DrawFan 2,000 |
| V28 | DrawFan: seven triangles | VenomRing_DrawFan 2,000 |
| V29 | DrawFan: third vertex blue from +8 | VenomRing_DrawFan 1,970 |
| V30 | DrawFan: rim y about +0x2E | VenomRing_DrawFan 2,000 |
| V31 | DrawFan: radius +0x14 re-read in place of the scratch word | VenomRing_DrawFan 1,445 |
| V32 | DrawBand: Rand & 3 | VenomRing_DrawBand 997 |
| V33 | DrawBand: fourth vertex blue from +8 | VenomRing_DrawBand 1,991 |
| V34 | DrawBand: fourth y from the x | VenomRing_DrawBand 2,000 |
| V35 | DrawBand: commit 0x40 | VenomRing_DrawBand 2,000 |
| V36 | DrawBand: second vertex green 2 | VenomRing_DrawBand 2,000 |
| V37 | Venom_Start: a ring owned by the owner | Venom_Start 1,817 |
| K1 | Kaiser_Task: entries 3 / 4 swapped | Kaiser_Task 487 |
| K2 | Kaiser_Task: tpage 0x54 | Kaiser_Task 2,000 |
| K3 | Kaiser_Task: live by bit 1 | Kaiser_Task 2,000 |
| K4 | Kaiser_Task: the owner not put back | Kaiser_Task 1,501 |
| K5 | Kaiser_Task: the current mote not set | Kaiser_Task 2,000 |
| K6 | Kaiser_Start: +2 not cleared | Kaiser_Start 2,000 |
| K7 | Kaiser_Start: +9 0xB | Kaiser_Start 1,973 |
| K8 | Kaiser_Start: direction after the centre | Kaiser_Start 162 |
| K9 | Kaiser_Start: CLUT bit 0x4000 | Kaiser_Start 2,000 |
| K10 | LoadFormFile: 0x227 at 9 | Kaiser_LoadFormFile 253 |
| K11 | HideParty: member 2 left on | Kaiser_HideParty 1,348 |
| K12 | HideParty: +9 0x19 | Kaiser_HideParty 1,354 |
| K13 | HideParty: CLUT one entry on | Kaiser_HideParty 1,354 |
| K14 | WaitChildren: +9 0xB | Kaiser_WaitChildren 1,023 |
| K15 | WaitChildren: at 1 child | Kaiser_WaitChildren 6 |
| K16 | ReloadParty: character 5 | Kaiser_ReloadParty 280 |
| K17 | ReloadParty: +0x134 bit 1 | Kaiser_ReloadParty 160 |
| K18 | ReloadParty: set 0xD files swapped | Kaiser_ReloadParty 32 |
| K19 | ReloadParty: mode 0 for 1 | Kaiser_ReloadParty 204 |
| K20 | ReloadParty: set not masked | Kaiser_ReloadParty 95 |
| K21 | ReloadParty: the last member skipped | Kaiser_ReloadParty 95 |
| K22 | LoadSetFile: halves swapped | Kaiser_LoadSetFile 313 |
| K23 | LoadSetFile: next word | Kaiser_LoadSetFile 773 |
| K24 | ShowParty: animation + 5 | Kaiser_ShowParty 1,084 |
| K25 | ShowParty: mask 0x2000 | Kaiser_ShowParty 423 |
| K26 | ShowParty: +0x91 bit 2 | Kaiser_ShowParty 820 |
| K27 | ShowParty: out animation from the member, not Sprite_Current re-read | Kaiser_ShowParty 274 |
| K28 | ShowParty: at most three | Kaiser_ShowParty 265 |
| K30 | EndWhenChildrenDone: bit 3 | MagicFx_EndWhenChildrenDone 731 |
| K31 | KaiserChild_Task: by +2 | KaiserChild_Task 1,477 |
| K32 | KaiserSprite_Run: bank 0x813581 | KaiserSprite_Run 1,277 |
| K33 | KaiserSprite_Run: every other frame | KaiserSprite_Run 237 |
| K34 | KaiserSprite_Run: wrap at 0x7A | KaiserSprite_Run 27 |
| K35 | KaiserSprite_Run: source one on | KaiserSprite_Run 739 |
| K36 | KaiserSprite_Run: +4 by 8 | KaiserSprite_Run 257 |
| K37 | Sprite_Start: y acceleration -2 | KaiserSprite_Start 441 |
| K38 | Sprite_Start: move before accelerate | KaiserSprite_Start 635 |
| K39 | Sprite_Start: y velocity not negated | KaiserSprite_Start 635 |
| K40 | Sprite_Start: +0x2A 2 | KaiserSprite_Start 194 |
| K41 | Sprite_Start: +0x27 3 | KaiserSprite_Start 635 |
| K42 | Glide: second velocity ay 0 | KaiserSprite_Glide 214 |
| K43 | Glide: +9 7 | KaiserSprite_Glide 673 |
| K44 | Glide: accelerate before move | KaiserSprite_Glide 2,000 |
| K45 | Brake: +9 0x25 | KaiserSprite_Brake 668 |
| K46 | Tick: animation 2 | KaiserSprite_Tick 644 |
| K47 | Tick: +9 0x28 | KaiserSprite_Tick 644 |
| K48 | Breathe: flags 0x20 | KaiserSprite_Breathe 642 |
| K49 | Breathe: children kinds 1, 2 | KaiserSprite_Breathe 642 |
| K50 | Breathe: late delays & 7 | KaiserSprite_Breathe 642 |
| K51 | Breathe: kind-0 index k + 1 | KaiserSprite_Breathe 638 |
| K52 | Breathe: kind-1 phase 1 | KaiserSprite_Breathe 642 |
| K53 | Breathe: the owner read before the child is made | KaiserSprite_Breathe 52 |
| K54 | WaitScript: +9 0xE | KaiserSprite_WaitScript 1,359 |
| K55 | Leave: ay 1 | KaiserSprite_Leave 217 |
| K56 | Leave: +0xA 0x1F | KaiserSprite_Leave 657 |
| K57 | the facing test: 2 for 3 | KaiserSprite_Start 218, KaiserSprite_Glide 223, KaiserSprite_Leave 229 |
| K58 | Exit: below 9 | KaiserSprite_Exit 205 |
| K59 | Exit: the x velocity accelerated | KaiserSprite_Exit 482 |
| K60 | KaiserFlash_Run: drawn at +2 not 1 | KaiserFlash_Run 643 |
| K61 | ClearCount9: +9 1 | MagicFx_ClearCount9 2,000 |
| K62 | CountUp9By2: at or above 0x10 | MagicFx_CountUp9By2 1,074 |
| K63 | WaitLoad: the owner's +0xA | KaiserFlash_WaitLoad 1,007 |
| K64 | WaitLoad: +0xA 9 | KaiserFlash_WaitLoad 680 |
| K65 | Flash_Draw: shade x 16 | KaiserFlash_Draw 1,990 |
| K66 | Flash_Draw: third corner y 319 | KaiserFlash_Draw 2,000 |
| K67 | Flash_Draw: commit 0x3C | KaiserFlash_Draw 2,000 |
| K68 | Pillar_Run: drawn before the push | KaiserPillar_Run 1,021 |
| K69 | Pillar_Start: x and z swapped | KaiserPillar_Start 2,000 |
| K70 | Pillar_Start: shade 0x11 | KaiserPillar_Start 2,000 |
| K71 | Pillar_Grow: by 5 | KaiserPillar_Grow 2,000 |
| K72 | Pillar_Fade: shade by -3 | KaiserPillar_Fade 2,000 |
| K73 | Pillar_Draw: shade zero-extended | KaiserPillar_Draw 882 |
| K74 | Pillar_Draw: rings 0x41 apart | KaiserPillar_Draw 2,000 |
| K75 | Pillar_Draw: quad 11 drawn | KaiserPillar_Draw 2,000 |
| K76 | Pillar_Draw: u << 5 | KaiserPillar_Draw 2,000 |
| K77 | Pillar_Draw: far v one more | KaiserPillar_Draw 2,000 |
| K78 | Pillar_Draw: old height + 1 | KaiserPillar_Draw 2,000 |
| K79 | Pillar_Draw: tpage y 0x101 | KaiserPillar_Draw 2,000 |
| K80 | Pillar_Draw: the angle kept, not re-read after the cosine | KaiserPillar_Draw 243 |
| K81 | Rtp4: vertices 2 and 3 swapped | KaiserPillar_Draw 2,000, KaiserRing_Draw 2,000 |
| K82 | Pillar_Draw: band by j / 2 | KaiserPillar_Draw 2,000 |
| K83 | Ring_Start: radius 0x41 | KaiserRing_Start 2,000 |
| K84 | Ring_Start: +0xA 2 | KaiserRing_Start 2,000 |
| K85 | Ring_Grow: by 0x21 | KaiserRing_Grow 2,000 |
| K86 | Ring_Grow: +0xB 2 | KaiserRing_Grow 425 |
| K87 | Ring_Fade: by 0x11 | KaiserRing_Fade 2,000 |
| K88 | Ring_Draw: blend & 7 | KaiserRing_Draw 1,020 |
| K89 | Ring_Draw: bottom -0x3FF | KaiserRing_Draw 2,000 |
| K90 | Ring_Draw: angle 0x600 skipped | KaiserRing_Draw 2,000 |
| K91 | Ring_Draw: semi-transparency 1 | KaiserRing_Draw 1,999 |
| K92 | Ring_Draw: v 0xBE | KaiserRing_Draw 2,000 |
| K93 | Ring_Draw: the new x kept, not re-read after the cosine | KaiserRing_Draw 2 |
| K94 | Ring_Draw: u parity from 0 | KaiserRing_Draw 2,000 |
| M1 | KaiserMote_Task: by +2 | ours' own abort: KaiserMote_Task: phase 164, past the 2-entry table |
| M2 | MoteA_Run: drawn by +1 | KaiserMoteA_Run 512 |
| M3 | the start radius >> 4 | KaiserMoteA_Start 2,000, KaiserMoteB_Start 2,000 |
| M4 | MotePlace: the cosine of the kept angle | KaiserMoteA_Spread 1, KaiserMoteA_Fade 2, KaiserMoteB_Start 1, KaiserMoteB_Spread 1, KaiserMoteB_Fade 1 |
| M5 | MotePlace: x by +0xE | KaiserMoteA_Spread 2,000, KaiserMoteA_Fade 2,000, KaiserMoteB_Spread 2,000, KaiserMoteB_Fade 2,000 |
| M6 | AngleA << 6 | KaiserMoteA_Start 1,691, KaiserMoteA_Spread 1,638, KaiserMoteA_Fade 1,625 |
| M7 | AngleB << 7 | KaiserMoteB_Start 1,830, KaiserMoteB_Spread 1,802, KaiserMoteB_Fade 1,553 |
| M8 | MoteA_Start: +6 0x15 | KaiserMoteA_Start 2,000 |
| M9 | MoteA_Start: the owner's +0x38 | KaiserMoteA_Start 2,000 |
| M10 | MoteA_Spread: +6 by 3 | KaiserMoteA_Spread 1,993 |
| M11 | MoteA_Spread: at 0x1E0 | KaiserMoteA_Spread 906 |
| M12 | MoteA_Fade: radius by 0x11 | KaiserMoteA_Fade 2,000 |
| M13 | MoteA_Fade: not freed | KaiserMoteA_Fade 431 |
| M14 | MoteB_Run: drawn in its delay too | KaiserMoteB_Run 638 |
| M15 | MoteB_Run: the last delay step skipped | KaiserMoteB_Run 644 |
| M16 | MoteB_Start: lift << 5 | KaiserMoteB_Start 1,805 |
| M17 | MoteB_Start: +6 9 | KaiserMoteB_Start 2,000 |
| M18 | MoteB_Spread: kind A's angles | KaiserMoteB_Spread 1,806 |
| M19 | MoteB_Fade: by -3 | KaiserMoteB_Fade 2,000 |
| M20 | Mote_Draw: Rand & 7 | KaiserMote_Draw 1,019 |
| M21 | Mote_Draw: kind 0 x 15 | KaiserMote_Draw 992 |
| M22 | Mote_Draw: kind 1 + 5 | KaiserMote_Draw 1,004 |
| M23 | Mote_Draw: last angle not wrapped | KaiserMote_Draw 2,000 |
| M24 | Mote_Draw: last y about x | KaiserMote_Draw 2,000 |
| M25 | Mote_Draw: rim blue 2 | KaiserMote_Draw 2,000 |
| M26 | Mote_Draw: centre y from +0x20 | KaiserMote_Draw 2,000 |
| M27 | Mote_Draw: the mote not re-read after the second Rand | KaiserMote_Draw 7 |
| M28 | Alloc: sets bit 1 too | KaiserMote_Alloc 481 |
| M29 | Alloc: 47 records | KaiserMote_Alloc 20 |
| M30 | Alloc: none is 0xFE | KaiserMote_Alloc 997 |
| M31 | Free: +4 kept | KaiserMote_Free 1,992 |
| M32 | UpdateScreenXY: z >> 8 | KaiserMote_UpdateScreenXY 2,000 |
| M33 | UpdateScreenXY: height >> 1 not / 2 | KaiserMote_UpdateScreenXY 525 |
| M34 | Ftol16: NaN 1 | KaiserMote_UpdateScreenXY 1,226 |
| M35 | Ftol16: range from -1000 | KaiserMote_UpdateScreenXY 229 |
| M36 | UpdateScreenXY: y from x | KaiserMote_UpdateScreenXY 1,735 |
| M37 | UpdateScreenXY: x - 0x3FFF | KaiserMote_UpdateScreenXY 2,000 |
| V8b | VenomChild_Task: by +1 ^ 1 | VenomChild_Task 2,000 |
| M1b | KaiserMote_Task: by +1 ^ 1 | KaiserMote_Task 2,000 |

## 7. Named data (`symbols.toml` `[[data]]`)

| Kind | Items |
|---|---|
| Dispatch tables | `Venom_ChildKinds` `0x65BC54` (2), `VenomSprite_Phases` `0x65BC5C` (4), `VenomRing_Phases` `0x65BC6C` (4), `Kaiser_ChildKinds` `0x65BC88` (4), `KaiserSprite_Phases` `0x65BC98` (8), `KaiserFlash_Phases` `0x65BCB8` (5), `KaiserPillar_Phases` `0x65BCCC` (3), `KaiserRing_Phases` `0x65BCE8` (3), `KaiserMote_Kinds` `0x65BCF4` (2), `KaiserMoteA_Phases` `0x65BD0C` (3), `KaiserMoteB_Phases` `0x65BD58` (3) |
| Value tables | `VenomRing_ScreenY` `0x65BC7C` (6 words), `KaiserPillar_V` `0x65BCD8` / `_H` `0x65BCE0` (8 bytes each), `KaiserMoteA_Angles` `0x65BCFC` (8), `KaiserMoteB_Angles` `0x65BD18` / `_Lift` `0x65BD38` (28 each) |
| Pool | `KaiserMote_Pool` `0x6A4E38` (48 x `0x2C`), `KaiserMote_Current` `0x6A5678` |

Addresses and sizes only; no values are recorded here. `magic_rows.py`
counts "10 / 20 / 46 code entries" at some tables because the next tables'
code pointers follow them; the lengths above are the dispatchers' own (the
number of phases their children step through).

## 8. Latent defects (Capcom's, kept)

Numbered D89, D97 and D107 in [`known-defects.md`](known-defects.md).

Described, not numbered, not fixed:

- **`Kaiser_LoadSetFile` reads through `0x64E9BC` unchecked.** The table is
  indexed by `0x904B89` (8-byte entries) and entries 10, 19 and 20 hold null
  pointers (read 2026-09-26), so with `0x904B89` one of those the original
  reads a word at address `2 x party set`: an access violation. Ours aborts
  with a message there. Whether `0x904B89` can hold those values while this
  spell runs is not measured. The party-set index (`0x90412C & 0xFF`, bit 7
  kept) is unchecked too: with bit 7 set it reads 256 bytes further on, a
  wrong file index rather than a fault.
- **Every dispatcher's index is unchecked** (two stack tables, eleven `.data`
  tables); ours aborts past each.
- **The value tables are indexed unchecked**: `VenomRing_ScreenY` by `+0xB`,
  the mote tables by `+7` - reads within `.data`, kept.
- **`Kaiser_ReloadParty` / `Kaiser_ShowParty` walk the members by
  `Field_MemberCount`**, unbounded by the three party records; the fuzz keeps
  it at 0..4.

## 9. What nothing reached

No recorded route casts either spell (queue §5). The live check is the owner
casting them, with a save that has them or DIV-0045's cheat. By reading, for
Venom: a sprite that brightens, holds and darkens at a fixed screen point,
with six rings of fans; for KaiserBreath: the party's sprites hidden while a
file loads, a sprite gliding in with a cycling palette, a textured pillar,
a ring and 36 motes round the targets' point, flashes, and the party
reloaded and shown again. That reading is a hypothesis for the owner's eye.

## 10. For `analysis/calltrace/entries_logic.txt`

59 lines appended to the main checkout's copy under a `group S30` comment:
`004E5FB0 250` was listed right already; `004E4810`, `004E5B40`, `004E6200`,
`004E6830` and `004E68B0` were host extents (`1326`, `466`, `626`, `7F`,
`536`), re-listed at each function's own size.
