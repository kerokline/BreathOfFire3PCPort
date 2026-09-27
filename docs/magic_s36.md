# Group S36: Magic Ball, Intimidate and Aura Breath (MAGIC172, 173, 218)

**Status:** IN PROGRESS (2026-09-27). All 45 functions are ours
(`src/game/magic_s36.cpp`, shadow name `magic_s36`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 90,000 rounds. 254 of 255 negative controls refused, every one by a count (exit 3); the one standing is an equivalent mutant with its near variant refused. Nothing recorded casts
these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S36
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 23 | 0x2AA | MAGIC172 | 0xAC | Magic Ball | `0x4F1E40..0x4F3535` | 18 |
| 34 | 0x2AB | MAGIC173 | 0xAD | Intimidate | `0x4F3540..0x4F4A53` | 18 |
| 142 | 0x2AE | MAGIC218 | 0xDA | Aura Breath | `0x4F52F0..0x4F59CF` | 9 |

The extents are `tools/magic_rows.py --unit MAGIC1NN / MAGIC218 --clones`
(capstone recursive descent; no jump table, nothing `REFUSED`). All 45
functions lie in the units' extents; none was found inside or missing from
them, and none was ours before. 12,729 bytes, as the queue counted. The rows
pair with the overlays in the queue's order here (`analysis/magic_rows.tsv`).
MAGIC218's extent ends at `0x4F59CF`; `0x4F59D0` is MAGIC219's (group S37).

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. What each spell looks
like in play has not been measured; the descriptions below are the code's.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Magic Ball (MAGIC172).**
  - `MagicBall_Task`, the kind-2 task: `MagicBall_Start`, then
    `BattleFx_Finish` (the done flag once every child has ended).
  - `MagicBall_Start` makes nine children of kind 1, parameter 0x51: the
    **core** (+1 0) and eight **orbs** (+1 1, +4 the core's slot, +9 i + 1,
    +0xB i); sound 0x100.
  - The core (`MagicBallCore_*`, five steps through `MagicBallCore_Steps`):
    `_Start` puts it 0x800000 above the caster heading for the source sprite
    (`0x904B4C`); `_Fly` steps it (`MagicFx_StepTowardPoint`, 0x40) toward the
    source's point 0x1000000 up and re-aims it each frame; at the source
    (`MagicFx_NearSprite`, 0x8000), or once its heading has swung between 0x600
    and 0xA00 in one frame (it passed the source), the target is flagged 0x10,
    sound 0x101, and `_Swell` (+0xA and +0xB up for 16 frames), `_Shrink`
    (+0xA down, sound 0x102) and MAGIC060's `MagicFx_CountDown2Release`
    follow. `MagicBallCore_Run` draws by step: five spark lines, the disc and
    the ring in flight; eight sparks, disc, ring and both swell rings at the
    hit; the two swell rings while swelling; eight sparks while shrinking.
  - An orb (`MagicBallOrb_*`) waits +9 frames, starts above the caster, flies
    as the core does (without the hit) and, once the core's task is at step 2,
    fades (MAGIC168's `ShadowSeeker_Fade`); each frame a disc shaded from
    `MagicBallOrb_Shades` by its +0xB.
  - The draws are screen-space primitives with float vertices (the PC port's
    layout) round the task's screen point +0x2E / +0x30, all on layer 3 after a
    draw-mode packet (tpage 0x35): `_DrawDisc` (16 gouraud triangles, radius
    0x10), `_DrawRing` (16 gouraud quads, 0x10..0x18), `_DrawRingOut` /
    `_DrawRingIn` (+0xB..2 +0xB and 2 +0xB..4 +0xB, shaded from +0xA), and the
    sparks `_DrawSpark` / `_DrawSparkShort`: gouraud lines out from radius
    0x10, each far end turned by `Rand & 0x7F` (0x3F) and the radius walked
    by `sin((i & 0x1F) << 7) x (Rand & mask) >> 12`, until the radius falls
    below 0x10 (0xC). The loop counter lives in the angle argument's own
    stack slot.
- **Intimidate (MAGIC173).**
  - `Intimidate_Task`: `Intimidate_Start`, `BattleFx_Finish`.
  - `Intimidate_Start` makes two **trails** (kind 1, parameter 0x18, +1 0,
    +4 0 and 1), restores CLUT row 26 plain (no STP bits), sound 0x100.
  - A trail (`IntimidateTrail_*`, four steps): `_Start` puts it beside the
    caster - an offset chosen by the acting actor (a party member
    (0x4000, 0, 0xC00000); an enemy whose record +0x8C is 0x61
    (0x38000, 0, 0), 0x6C (0x20000 or 0x30000 by facing bit 1, 0, 0x800000),
    any other (0x10000, 0, 0x800000)) turned by the direction (the engine's
    `0x446770`) - and seeds its screen trail; `_Fly` steps it (0xC0) at the
    source as the core does, pushing its screen point each frame; trail 0
    flags the target 0x10 every frame after the first step, and on arrival
    (step 2) a **burst** is made (the same kind, +1 1, the owner's child, at
    this point); `_Grow` lengthens the trail to 8 points; `_Fade` pushes gaps
    until +9 runs out.
  - `IntimidateTrail_Points` (`0x6B2858`) holds 32 (x, y) screen words a
    trail; `_Push` / `_PushGap` shift a trail's points +0xB..+0xA - 1 one on
    and put the screen point (or (-1, -1)) at +0xB.
  - `_DrawThin` (trail 1) and `_DrawWide` (trail 0) draw a ribbon of gouraud
    quads through the points from +0xB (past any gap) to +0xA: the thin one 2
    either side of the line (shades 0x6D and 0x61 less 12 per point, blue 1);
    the wide one in two passes, 0xC to each side, the line's own edge shaded
    (0x6D / 0x61 less 12 k, then 0x91 / 0x81 less 16 k) and the offset edge 1;
    a closing draw-mode packet 0x15.
  - The burst (`IntimidateBurst_*`, with the effects' frame-offset table
    `0x8E3580` in `0x9039D8` round its steps): a sprite set up with animation
    0, its script ticked while +9 rises, then twice a frame until it ends,
    then +9 falls; each frame a flat disc of 8 triangles, radius
    `(Rand & 3) + 0x1C`, and before step 3 `Sprite_UpdateScreen`.
- **Aura Breath (MAGIC218).**
  - `AuraBreath_Task`: `AuraBreath_Start`, `BattleFx_Finish`.
  - `AuraBreath_Start` clears `AuraBreath_Struck` (`0x6B4A58`, a byte an
    enemy), makes the **dome** (kind 1, 0x67, +9 1), restores CLUT row 26 with
    its STP bits, sound 0x100.
  - The dome (`AuraBreathDome_*`, three steps): `_Wait` +9 frames then moves
    to the caster; `_Grow` widens it (+9 up by 2, the reach word 0x903850
    +9 x 16) and flags 0x10 **once** every enemy (3..10) not out, within reach
    (`AuraBreath_InReach`) and not the acting actor; `_Fade` counts +0x5D down
    and ends. `_Draw`, under the actor matrix: 8 bands x 32 semi-transparent
    textured quads (tpage 0x340 / 0x100, CLUT row 0x1FA) on
    `Prim_VertexScratch`, each after its own draw-mode packet (0xB5) and both
    linked at the quad's corner map point (`MapView_LinkPrimAt`), v and height
    from `AuraBreathDome_V` / `_Height` by band, shaded by +0x5D (a signed
    byte) x 8.
  - `AuraBreath_InReach(record)`: 1 in all of eax when the squared (x sar 9,
    z sar 9) distance from Sprite_Current to the record is at most the reach
    word squared, else 0. **Also called by MAGIC058** (group S11's Sanctuary),
    which calls it by address; taking it changes nothing for that caller.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, except that a phase
past any of the eleven dispatch tables (three stack tables, eight `.data`
tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3, the
precedent). No other function aborts: every unchecked index here reads or
writes inside the image, so ours does the same as the original (section 8).

Calls that push an argument the callee does not read push it in ours too:
`MagicFx_StepTowardPoint`'s fourth word (the original's own uninitialised
stack; ours passes 0, the recorder masks it), `Gte_RotTransPers4`'s flag.

## 3. Calls to other units

By name, already ours: `BattleFx_Finish` (group L's list), `MagicFx_CountDown2Release`
(S12, MAGIC060), `ShadowSeeker_Fade` (S29, MAGIC168), `MagicFx_StepTowardPoint`,
`MagicFx_NearSprite`, `MagicFx_PushActorMatrix` (L), `Battle_ActorIsOut`, and the
GTE / GPU / sprite / sound library.

By raw address: only the engine's unnamed `0x446770` (the dx / dz turn by
direction, as S22, S23 and S31 call it). No call into a unit not yet ours.

Shared bodies: `AuraBreath_InReach` (`0x4F5970`) is reached from MAGIC058 too
(`analysis/magic_funcs.tsv`); S11 calls it by raw address (`kNearSprite` in
`magic_s11.cpp`), which can now be rebound to the name.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `MagicBallChild_Kinds` | `0x65C180` | 2 |
| `MagicBallCore_Steps` | `0x65C188` | 5 |
| `MagicBallOrb_Steps` | `0x65C19C` | 3 |
| `MagicBallOrb_Shades` | `0x65C1A8` | 20 bytes (10 pairs) |
| `IntimidateChild_Kinds` | `0x65C1BC` | 2 |
| `IntimidateTrail_Steps` | `0x65C1C4` | 4 |
| `IntimidateBurst_Steps` | `0x65C1D4` | 4 |
| `AuraBreathDome_TaskTable` | `0x65C20C` | 1 |
| `AuraBreathDome_Steps` | `0x65C210` | 3 |
| `AuraBreathDome_V` | `0x65C21C` | 8 bytes |
| `AuraBreathDome_Height` | `0x65C224` | 8 bytes |
| `IntimidateTrail_Points` | `0x6B2858` | 2 x 32 word pairs |
| `AuraBreath_Struck` | `0x6B4A58` | 8 bytes |

Each `.data` count is where the next table starts (the words at
`0x65C180..0x65C22C` read 2026-09-27; `0x65C1E4..` is MAGIC213's, `0x65C22C..`
MAGIC219's). The two `.bss` tables are sized by their writers: two trails of
32 points end where `Magic213Mote_Pool` (`0x6B2958`) begins. None of their
contents are copied here; ours reads them in place.

## 5. The fuzz

`BOF3X_SHADOW=magic_s36` runs `magic_harness::Run` over the 45 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s36_fuzz.cpp`:

- **Callees** (35 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes (`NoteBytes`, the size the call names)
    and moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's
    own; `Gte_RotTransPers4` logs its four SVECTORs through `deref`;
  - `Math_Ratan2` answers, for the three flight steps, half the time a
    heading one step either side of the turn test's bounds (0x600, 0xA00,
    either sign) from the old one - a garbage heading almost never lands
    there;
  - `Math_Sin` answers 0 fifteen times in sixteen in half of a spark's
    rounds, so the spark's radius walk runs on past its 32-step wave (with a
    garbage sine it ends in a step or two);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - `Battle_SetTargetFlags`, while `AuraBreathDome_Grow` is fuzzed, writes a
    new byte half the time into the struck entry of the enemy it flags (the
    dome stores 0xFF there before the call, so what the entry holds after is
    the call's);
  - `AuraBreath_InReach` (called directly by the dome) answers a whole eax of
    0 or 1 (`kBool`) and logs the reach word and Sprite_Current's point;
  - the sprite calls log which sprite; `Sprite_UpdateScreen` also logs
    `0x9039D8` (the frame-offset table the burst swaps round its steps);
  - this group's own draws and helpers by address (their callers call them
    directly), the sparks with their two words.
- **Tables:** the eight `.data` handler tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `0x9039D8`; CLUT row 26
  and its source; `IntimidateTrail_Points` as far as a trail +4 below 48 and a
  point index to 256 reach (`0x1B84` bytes); `AuraBreath_Struck`. 27,684 bytes
  of state in 17 regions.
- **Settle:** after every disturbance the first four task slots' +4 stays
  below 48: `MagicBallOrb_Fly` reads the task slot +4 names (past 47 that is
  past the image: a fault on both sides), and the trails index their points
  by it.
- **Seed:** each dispatcher inside its table; each count one step before and
  at its threshold (+9 at 0x10 / 1 / 0x60, +0xA at 8 / 0x10 / 0x20, +0x5D at
  1); the core's task at step 1 or 2 for `MagicBallOrb_Fly`; the acting
  enemy's +0x8C 0x61, 0x6C or other for `IntimidateTrail_Start`; trail 0 at
  step 1 for `_Fly`; short trails with one or two gaps at the head for the
  ribbons; the struck bytes mostly clear and the actor an enemy for
  `AuraBreathDome_Grow`; the orb's +0xB inside its shade table; step 2 or 3
  for the sparks' shade branch; the sparks' masks their callers' 3, 7 or 0xF
  (`Group::args`); `AuraBreath_InReach` an enemy record the seed puts one
  step either side of the reach (in the seed: `Group::args` runs after the
  round's input is captured, so memory written there is lost).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, `0x9039D8`, a trail word, a struck byte, the task's +0x2E / +0x30 /
  +0x10 / +0x14 / +0x5D.

Result in this worktree (2026-09-27):

    shadow      magic_s36 self-test: 90000 rounds over 45 functions (2000 each), 10698750 calls to the stand-ins,
                0 MISMATCHES; 27684 bytes of state (17 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). `BOF3X_SHADOW='*'`: exit 0 (10,739,648 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches); group S11, which calls `AuraBreath_InReach` by address, passes with it.

## 6. Controls

255 plants, each put in `magic_s36.cpp` one at a time by a script (not
committed) that planted, rebuilt, checked the build had recompiled the file,
ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s36`, restored; after the last it
restored, rebuilt and ran the clean self-test (0 mismatches, exit 0). The M-,
I- and A- controls are Magic Ball, Intimidate and Aura Breath.

**254 of 255 refused**, every one by exit 3 with a count only in the
functions the plant touches (a plant in a shared helper - `AngleTo`,
`StepTowardSource`, `TurnedPast`, the disc, ring, ribbon and trail helpers -
in each function that uses it).

- **M77, equivalent**: the spark's turned angle adds `Rand & jitter` to the
  dword at `0x903854` and keeps 12 bits; adding the word instead cannot
  change those bits (a carry only moves upward). Its near variant M77b
  (`& 0x1FFF`) was refused in 1,207 / 1,253 rounds.

The first run (the same 255) left four standing and refused one thinly, and
the fuzz was strengthened before this whole set was run again: M82 (the short
spark's floor) wanted small sine steps (`Math_Sin` 0 or +-0x400 in half of a
spark's rounds); A60 (`InReach`'s bound) was seeded in `Group::args`, which
runs after the round's input is captured, so the placement was lost - it
moved into the seed; A23 (the struck byte stored before the flags call) wanted
`Battle_SetTargetFlags` to move that byte; M24 (+9's 0x10 bound in flight)
was seeded at 0xB / 0xC, not 0xF / 0x10 (refused in 1 round, now 115). M9's
plant counts each orb twice (it was meant as a reorder).

The thinnest (fewer than 60 rounds): M60 16, M34 19, M47 29, M33 32, I18 32,
I14 55, I87 56 - each a read or store that only a disturbance at one call
tells apart.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| M1 | MagicBall_Task: entries swapped | MagicBall_Task 2000 |
| M2 | Start: core parameter 0x52 | MagicBall_Start 2000 |
| M3 | Start: core +1 2 | MagicBall_Start 1695 |
| M4 | Start: seven orbs | MagicBall_Start 2000 |
| M5 | Start: orb +9 i + 2 | MagicBall_Start 2000 |
| M6 | Start: orb +0xB i + 1 | MagicBall_Start 2000 |
| M7 | Start: orb +4 core + 1 | MagicBall_Start 2000 |
| M8 | Start: sound 0x101 | MagicBall_Start 2000 |
| M9 | Start: each orb counted twice | MagicBall_Start 1962 |
| M10 | MagicBallChild_Task: kinds swapped | MagicBallChild_Task 2000 |
| M11 | Core_Run: Swell / Shrink swapped | MagicBallCore_Run 837 |
| M12 | Core_Run: step 2 sparks mask 3 | MagicBallCore_Run 212 |
| M13 | Core_Run: step 4 three sparks mask 0xF | MagicBallCore_Run 177 |
| M14 | Core_Run: step 3 rings swapped | MagicBallCore_Run 188 |
| M15 | Core_Run: default without the ring | MagicBallCore_Run 437 |
| M16 | Core_Run: +1 tested for +0 | MagicBallCore_Run 996 |
| M17 | sparks: 0x99A | MagicBallCore_Run 826 |
| M18 | sparks: short ones at 2, 3 | MagicBallCore_Run 826 |
| M19 | sparks: Rand & 0xFF | MagicBallCore_Run 807 |
| M20 | sparks: 0xC00 | MagicBallCore_Run 389 |
| M21 | Core_Start: y + 0x800001 | MagicBallCore_Start 1984 |
| M22 | Core_Start: +0xA kept | MagicBallCore_Start 1994 |
| M23 | AngleTo: (dz, dx) | MagicBallCore_Start 1511, MagicBallCore_Fly 2000, MagicBallOrb_Start 384, MagicBallOrb_Fly 2000, IntimidateTrail_Start 2000, IntimidateTrail_Fly 2000 |
| M24 | Core_Fly: +9 below 0x11 | MagicBallCore_Fly 115 |
| M25 | Core_Fly: speed 0x41 | MagicBallCore_Fly 2000 |
| M26 | Core_Fly: lift 0x800000 | MagicBallCore_Fly 2000 |
| M27 | StepTowardSource: y sar 0x10 | MagicBallCore_Fly 2000, MagicBallOrb_Fly 2000, IntimidateTrail_Fly 2000 |
| M28 | StepTowardSource: x - 0x3FFF | MagicBallCore_Fly 2000, MagicBallOrb_Fly 2000, IntimidateTrail_Fly 2000 |
| M29 | Core_Fly: near 0x7FFF | MagicBallCore_Fly 2000 |
| M30 | CoreHit: flags 0x11 | MagicBallCore_Fly 1756 |
| M31 | CoreHit: sound 0x102 | MagicBallCore_Fly 1756 |
| M32 | CoreHit: +9 0xF | MagicBallCore_Fly 1756 |
| M33 | TurnedPast: >= 0x600 | MagicBallCore_Fly 32, IntimidateTrail_Fly 38 |
| M34 | TurnedPast: <= 0xA00 | MagicBallCore_Fly 19, IntimidateTrail_Fly 23 |
| M35 | TurnedPast: heading & 0x7FF | MagicBallCore_Fly 181, IntimidateTrail_Fly 172 |
| M36 | TurnedPast: no absolute value | MagicBallCore_Fly 173, IntimidateTrail_Fly 165 |
| M37 | Core_Fly: old heading from +0x14 | MagicBallCore_Fly 2000 |
| M38 | Core_Swell: +0xB up by 3 | MagicBallCore_Swell 2000 |
| M39 | Core_Swell: below 0x11 | MagicBallCore_Swell 513 |
| M40 | Core_Swell: +0xA up by 1 | MagicBallCore_Swell 517 |
| M41 | Core_Shrink: down by 3 | MagicBallCore_Shrink 2000 |
| M42 | Core_Shrink: sound 0x103 | MagicBallCore_Shrink 475 |
| M43 | Core_Shrink: +0xB up by 2 | MagicBallCore_Shrink 1995 |
| M44 | Orb_Run: +0 and +2 tested apart | MagicBallOrb_Run 332 |
| M45 | Orb_Run: Start / Fly swapped | MagicBallOrb_Run 1334 |
| M46 | Orb_Start: y + 0x800001 | MagicBallOrb_Start 506 |
| M47 | Orb_Start: +9 kept | MagicBallOrb_Start 29 |
| M48 | Orb_Fly: core at step 1 | MagicBallOrb_Fly 1283 |
| M49 | Orb_Fly: the core +1 | MagicBallOrb_Fly 624 |
| M50 | Orb_Fly: speed 0x41 | MagicBallOrb_Fly 2000 |
| M51 | DrawDisc: radius 0x11 | MagicBall_DrawDisc 1993 |
| M52 | DrawDisc: centre x 11 | MagicBall_DrawDisc 1979 |
| M53 | DrawDisc: rim blue x 13 | MagicBall_DrawDisc 1979 |
| M54 | DrawDisc: rim green x 3 | MagicBall_DrawDisc 1981 |
| M55 | DiscG3: commit 0x30 | MagicBall_DrawDisc 2000, MagicBallOrb_DrawDisc 2000 |
| M56 | DiscG3: steps of 0x80 | MagicBall_DrawDisc 2000, MagicBallOrb_DrawDisc 2000 |
| M57 | DiscG3: fifteen | MagicBall_DrawDisc 2000, MagicBallOrb_DrawDisc 2000 |
| M58 | RimPair: first y by the sine | MagicBall_DrawDisc 2000, MagicBallOrb_DrawDisc 2000 |
| M59 | RimPair: second at word + 2 | MagicBall_DrawDisc 2000, MagicBallOrb_DrawDisc 2000 |
| M60 | RimPair: angle stored after the sine | MagicBall_DrawDisc 23, MagicBallOrb_DrawDisc 16 |
| M61 | RingG4: commit 0x40 | MagicBall_DrawRing 2000, MagicBall_DrawRingOut 2000, MagicBall_DrawRingIn 2000 |
| M62 | RingG4: +0x28 at the inner radius | MagicBall_DrawRing 2000, MagicBall_DrawRingOut 1996, MagicBall_DrawRingIn 1992 |
| M63 | RingG4: lit at 0x14 | MagicBall_DrawRing 2000, MagicBall_DrawRingIn 2000 |
| M64 | RingG4: blue from 0x90385A | MagicBall_DrawRing 1997, MagicBall_DrawRingOut 1992, MagicBall_DrawRingIn 1996 |
| M65 | DrawRing: outer 0x17 | MagicBall_DrawRing 1995 |
| M66 | DrawRing: blue x 15 | MagicBall_DrawRing 1977 |
| M67 | DrawRingOut: green x 7 | MagicBall_DrawRingOut 1978 |
| M68 | DrawRingOut: inner lit | MagicBall_DrawRingOut 2000 |
| M69 | DrawRingIn: outer x 3 | MagicBall_DrawRingIn 1977 |
| M70 | DrawRingIn: green x 4 | MagicBall_DrawRingIn 1976 |
| M71 | DrawRingOut: red 2 | MagicBall_DrawRingOut 1986 |
| M72 | Spark: radius 0x11 | MagicBall_DrawSpark 1995, MagicBall_DrawSparkShort 1996 |
| M73 | Spark: wave (i & 0x3F) | MagicBall_DrawSpark 278, MagicBall_DrawSparkShort 591 |
| M74 | Spark: counter from 0 | MagicBall_DrawSpark 2000, MagicBall_DrawSparkShort 2000 |
| M75 | Spark: mask not sign-extended | MagicBall_DrawSpark 122, MagicBall_DrawSparkShort 114 |
| M76 | Spark: radius stepped down | MagicBall_DrawSpark 1959, MagicBall_DrawSparkShort 1950 |
| M77 | Spark: the angle word, not the dword (EQUIVALENT: & 0xFFF) | **not refused**: NOT REFUSED (exit 0) |
| M77b | Spark: the angle & 0x1FFF (near variant) | MagicBall_DrawSpark 1207, MagicBall_DrawSparkShort 1253 |
| M78 | Spark: near shade - 0x23 | MagicBall_DrawSpark 1800 |
| M79 | Spark: far shade - 0x26 | MagicBall_DrawSpark 1800 |
| M80 | Spark: turn & 0xFF | MagicBall_DrawSpark 1585 |
| M81 | Spark: limit 0x11 | MagicBall_DrawSpark 1019 |
| M82 | SparkShort: limit 0xD | MagicBall_DrawSparkShort 283 |
| M83 | SparkShort: turn & 0x7F | MagicBall_DrawSparkShort 1662 |
| M84 | SparkShort: near - 0x17 | MagicBall_DrawSparkShort 1829 |
| M85 | Spark: near shade below step 4 | MagicBall_DrawSpark 405, MagicBall_DrawSparkShort 412 |
| M86 | Spark: near blue a quarter | MagicBall_DrawSpark 1996, MagicBall_DrawSparkShort 1998 |
| M87 | Spark: far blue unsigned | MagicBall_DrawSpark 849, MagicBall_DrawSparkShort 1020 |
| M88 | Spark: commit 0x20 | MagicBall_DrawSpark 2000, MagicBall_DrawSparkShort 2000 |
| M89 | Spark: first shade x 5 | MagicBall_DrawSpark 446, MagicBall_DrawSparkShort 408 |
| M90 | Spark: loop while above | MagicBall_DrawSpark 1019, MagicBall_DrawSparkShort 283 |
| M91 | OrbDisc: radius 0x19 | MagicBallOrb_DrawDisc 1997 |
| M92 | OrbDisc: shades by +0xB | MagicBallOrb_DrawDisc 1835 |
| M93 | OrbDisc: green +0xB | MagicBallOrb_DrawDisc 1980 |
| M94 | OrbDisc: rim blue the shade | MagicBallOrb_DrawDisc 1998 |
| M95 | OrbDisc: +0xB + +9 | MagicBallOrb_DrawDisc 1854 |
| I1 | Intimidate_Task: entries swapped | Intimidate_Task 2000 |
| I2 | Start: parameter 0x19 | Intimidate_Start 2000 |
| I3 | Start: three trails | Intimidate_Start 2000 |
| I4 | Start: +4 i ^ 1 | Intimidate_Start 2000 |
| I5 | Start: the row with STP | Intimidate_Start 2000 |
| I6 | RestoreRow26: one word short | Intimidate_Start 2000 |
| I7 | IntimidateChild_Task: kinds swapped | IntimidateChild_Task 2000 |
| I8 | Trail_Run: Grow / Fade swapped | IntimidateTrail_Run 1000 |
| I9 | Trail_Run: draws swapped | IntimidateTrail_Run 784 |
| I10 | Trail_Run: +2 not tested | IntimidateTrail_Run 271 |
| I11 | Trail_Start: party 0x4001 | IntimidateTrail_Start 1222 |
| I12 | Trail_Start: party height 0x800000 | IntimidateTrail_Start 1225 |
| I13 | Trail_Start: kind 0x62 | IntimidateTrail_Start 276 |
| I14 | Trail_Start: facing bit 0 | IntimidateTrail_Start 55 |
| I15 | Trail_Start: 0x30000 for 0x61 | IntimidateTrail_Start 273 |
| I16 | Trail_Start: others 0x18000 | IntimidateTrail_Start 384 |
| I17 | Trail_Start: party below 2 | IntimidateTrail_Start 392 |
| I18 | Trail_Start: source read after the turn | IntimidateTrail_Start 32 |
| I19 | Trail_Start: every axis by +0xC | IntimidateTrail_Start 2000 |
| I20 | Trail_Start: second point at 2 | IntimidateTrail_Start 2000 |
| I21 | Trail_Start: +0xA 2 | IntimidateTrail_Start 2000 |
| I22 | Trail_Start: +9 0x11 | IntimidateTrail_Start 2000 |
| I23 | Trail_Start: +0x10 kept | IntimidateTrail_Start 2000 |
| I24 | Trail_Fly: +0xA below 9 | IntimidateTrail_Fly 485 |
| I25 | Trail_Fly: speed 0xC1 | IntimidateTrail_Fly 2000 |
| I26 | Trail_Fly: lift 0x1000000 | IntimidateTrail_Fly 2000 |
| I27 | Trail_Fly: z from the source x | IntimidateTrail_Fly 1765 |
| I28 | Trail_Fly: flags off step 2 | IntimidateTrail_Fly 725 |
| I29 | Trail_Fly: trail 1 flags | IntimidateTrail_Fly 1591 |
| I30 | Trail_Fly: burst at step 3 | IntimidateTrail_Fly 1459 |
| I31 | Trail_Fly: burst parameter 0x19 | IntimidateTrail_Fly 1245 |
| I32 | Trail_Fly: burst +1 0 | IntimidateTrail_Fly 1245 |
| I33 | Trail_Fly: burst owned by the trail | IntimidateTrail_Fly 1096 |
| I34 | Trail_Fly: the trail counted | IntimidateTrail_Fly 1096 |
| I35 | Trail_Fly: burst y from z | IntimidateTrail_Fly 1245 |
| I36 | Trail_Fly: flags 0x20 | IntimidateTrail_Fly 681 |
| I37 | Trail_Grow: at 9 | IntimidateTrail_Grow 463 |
| I38 | Trail_Grow: early +9 7 | IntimidateTrail_Grow 463 |
| I39 | Trail_Grow: after the push at 7 | IntimidateTrail_Grow 991 |
| I40 | Trail_Grow: +0xB kept | IntimidateTrail_Grow 1063 |
| I41 | Trail_Fade: owner counted up | IntimidateTrail_Fade 476 |
| I42 | Trail_Fade: a point, not a gap | IntimidateTrail_Fade 2000 |
| I43 | TrailShift: stops above +0xB | IntimidateTrail_Push 958, IntimidateTrail_PushGap 943 |
| I44 | TrailShift: from +0xA - 2 | IntimidateTrail_Push 958, IntimidateTrail_PushGap 943 |
| I45 | TrailShift: y moved from x | IntimidateTrail_Push 958, IntimidateTrail_PushGap 943 |
| I46 | TrailShift: head at +0xB + 1 | IntimidateTrail_Push 2000, IntimidateTrail_PushGap 2000 |
| I47 | TrailPoint: 16 a trail | IntimidateTrail_Start 1953, IntimidateTrail_DrawThin 1968, IntimidateTrail_DrawWide 1991, IntimidateTrail_Push 1944, IntimidateTrail_PushGap 1939 |
| I48 | Push: y from +0x32 | IntimidateTrail_Push 2000 |
| I49 | PushGap: y 0xFFFE | IntimidateTrail_PushGap 2000 |
| I50 | RibbonHead: no gap scan | IntimidateTrail_DrawThin 912, IntimidateTrail_DrawWide 954 |
| I51 | RibbonHead: Ratan2 (dy, dx) | IntimidateTrail_DrawThin 2000, IntimidateTrail_DrawWide 2000 |
| I52 | RibbonHead: Sprite_Current not read again | IntimidateTrail_DrawThin 62, IntimidateTrail_DrawWide 127 |
| I53 | RibbonHead: answers k | IntimidateTrail_DrawThin 1499, IntimidateTrail_DrawWide 1539 |
| I54 | Offset: angle & 0x1FFF | IntimidateTrail_DrawThin 1420, IntimidateTrail_DrawWide 1454 |
| I55 | Offset: y at the x word | IntimidateTrail_DrawThin 1429, IntimidateTrail_DrawWide 1474 |
| I56 | RibbonStep: last y from x | IntimidateTrail_DrawThin 1429, IntimidateTrail_DrawWide 1474 |
| I57 | RibbonTurn: (dy, dx) | IntimidateTrail_DrawThin 1429, IntimidateTrail_DrawWide 1474 |
| I58 | DrawThin: width 3 | IntimidateTrail_DrawThin 1995 |
| I59 | DrawThin: shades swapped | IntimidateTrail_DrawThin 1429 |
| I60 | DrawThin: 11 k | IntimidateTrail_DrawThin 1429 |
| I61 | DrawThin: +0x28 on the + side | IntimidateTrail_DrawThin 1429 |
| I62 | DrawThin: one quad more | IntimidateTrail_DrawThin 331 |
| I63 | DrawThin: closing 0x35 | IntimidateTrail_DrawThin 2000 |
| I64 | ThinShade: near blue the shade | IntimidateTrail_DrawThin 1420 |
| I65 | ThinShade: corner 3 red the first | IntimidateTrail_DrawThin 1429 |
| I66 | WideShade: corner 2 blue 1 | IntimidateTrail_DrawWide 1474 |
| I67 | DrawWide: width 0xB | IntimidateTrail_DrawWide 1993 |
| I68 | DrawWide: second pass 0x90 | IntimidateTrail_DrawWide 768 |
| I69 | DrawWide: second pass 12 k | IntimidateTrail_DrawWide 768 |
| I70 | DrawWide: first pass on the - side | IntimidateTrail_DrawWide 1456 |
| I71 | WidePass: corner 2 at the new point | IntimidateTrail_DrawWide 1474 |
| I72 | WidePass: corner 3 y the last | IntimidateTrail_DrawWide 1474 |
| I73 | WidePass: commit 0x40 | IntimidateTrail_DrawWide 1474 |
| I74 | Burst_Run: the battle table during | IntimidateBurst_Run 768 |
| I75 | Burst_Run: the effect table after | IntimidateBurst_Run 2000 |
| I76 | Burst_Run: update below step 2 | IntimidateBurst_Run 266 |
| I77 | Burst_Run: Rise / Play swapped | IntimidateBurst_Run 998 |
| I78 | Burst_Run: disc before the screen point | IntimidateBurst_Run 1020 |
| I79 | Burst_Start: +0x25 0x1E | IntimidateBurst_Start 2000 |
| I80 | Burst_Start: +0x27 0xA1 | IntimidateBurst_Start 2000 |
| I81 | Burst_Start: +0x24 5 | IntimidateBurst_Start 2000 |
| I82 | Burst_Start: +0x5F kept | IntimidateBurst_Start 1988 |
| I83 | Burst_Start: +0x29 2 | IntimidateBurst_Start 2000 |
| I84 | Burst_Start: +0x2C a byte | IntimidateBurst_Start 1985 |
| I85 | Burst_Start: animation 1 | IntimidateBurst_Start 2000 |
| I86 | Burst_Start: +0x2B 0 | IntimidateBurst_Start 2000 |
| I87 | Burst_Start: +9 on the old task | IntimidateBurst_Start 56 |
| I88 | Burst_Rise: by 2 | IntimidateBurst_Rise 2000 |
| I89 | Burst_Rise: at 0x14 | IntimidateBurst_Rise 509 |
| I90 | Burst_Play: one tick | IntimidateBurst_Play 2000 |
| I91 | Burst_Play: the first tick tested | IntimidateBurst_Play 900 |
| I92 | Burst_Fade: by 2 | IntimidateBurst_Fade 2000 |
| I93 | Burst_Fade: owner counted up | IntimidateBurst_Fade 496 |
| I94 | BurstDisc: radius + 0x1D | IntimidateBurst_DrawDisc 1994 |
| I95 | BurstDisc: Rand & 7 | IntimidateBurst_DrawDisc 989 |
| I96 | BurstDisc: centre x 5 | IntimidateBurst_DrawDisc 1980 |
| I97 | BurstDisc: steps of 0x100 | IntimidateBurst_DrawDisc 2000 |
| I98 | BurstDisc: +0x1C from x | IntimidateBurst_DrawDisc 2000 |
| I99 | BurstDisc: centre blue 1 | IntimidateBurst_DrawDisc 2000 |
| A1 | AuraBreath_Task: entries swapped | AuraBreath_Task 2000 |
| A2 | Start: struck 6, 7 kept | AuraBreath_Start 2000 |
| A3 | Start: parameter 0x68 | AuraBreath_Start 2000 |
| A4 | Start: dome +9 2 | AuraBreath_Start 2000 |
| A5 | Start: the row without STP | AuraBreath_Start 2000 |
| A6 | RestoreRow26Stp: bit 0x4000 | AuraBreath_Start 2000 |
| A7 | Start: owner +9 for the direction | AuraBreath_Start 1956 |
| A8 | AuraBreathDome_Task: the wait step | AuraBreathDome_Task 2000 |
| A9 | Dome_Run: Grow / Fade swapped | AuraBreathDome_Run 1335 |
| A10 | Dome_Run: matrix after the draw | AuraBreathDome_Run 681 |
| A11 | Dome_Run: +2 not tested | AuraBreathDome_Run 366 |
| A12 | Dome_Wait: +0x5D 0x11 | AuraBreathDome_Wait 498 |
| A13 | Dome_Wait: +0xA 5 | AuraBreathDome_Wait 498 |
| A14 | Dome_Wait: height from z | AuraBreathDome_Wait 498 |
| A15 | Dome_Grow: by 3 | AuraBreathDome_Grow 2000 |
| A16 | Dome_Grow: reach sl 3 | AuraBreathDome_Grow 1993 |
| A17 | Dome_Grow: struck 1 only | AuraBreathDome_Grow 1928 |
| A18 | Dome_Grow: out test one down | AuraBreathDome_Grow 1999 |
| A19 | Dome_Grow: the next record | AuraBreathDome_Grow 1726 |
| A20 | Dome_Grow: actor one up | AuraBreathDome_Grow 290 |
| A21 | Dome_Grow: struck 1 | AuraBreathDome_Grow 881 |
| A22 | Dome_Grow: flags 0x40 | AuraBreathDome_Grow 1394 |
| A23 | Dome_Grow: struck after the flags | AuraBreathDome_Grow 882 |
| A24 | Dome_Grow: below 0x21 | AuraBreathDome_Grow 141 |
| A25 | Dome_Grow: at 0x5F | AuraBreathDome_Grow 748 |
| A26 | Dome_Grow: frame bit 1 | AuraBreathDome_Grow 319 |
| A27 | Dome_Grow: seven enemies | AuraBreathDome_Grow 1324 |
| A28 | Dome_Fade: even frames | AuraBreathDome_Fade 663 |
| A29 | Dome_Fade: +9 by 2 | AuraBreathDome_Fade 2000 |
| A30 | Dome_Fade: below 0x1F | AuraBreathDome_Fade 278 |
| A31 | Dome_Fade: owner counted up | AuraBreathDome_Fade 559 |
| A32 | Draw: shade sl 2 | AuraBreathDome_Draw 1982 |
| A33 | Draw: shade unsigned | AuraBreathDome_Draw 210 |
| A34 | Draw: height reach sl 3 | AuraBreathDome_Draw 1987 |
| A35 | Draw: radius sl 5 | AuraBreathDome_Draw 1987 |
| A36 | Draw: radius steps 0x20 | AuraBreathDome_Draw 2000 |
| A37 | Draw: height angle from 0x80 | AuraBreathDome_Draw 2000 |
| A38 | Draw: seven bands | AuraBreathDome_Draw 2000 |
| A39 | Draw: height not negated | AuraBreathDome_Draw 1999 |
| A40 | Draw: the outer corners from A4 | AuraBreathDome_Draw 2000 |
| A41 | Draw: 31 quads | AuraBreathDome_Draw 2000 |
| A42 | Draw: angle (j & 0x3F) | AuraBreathDome_Draw 2000 |
| A45 | Draw: link x from BA | AuraBreathDome_Draw 2000 |
| A46 | Draw: link x sl 8 | AuraBreathDome_Draw 2000 |
| A47 | Draw: mode 0x95 | AuraBreathDome_Draw 2000 |
| A48 | Draw: mode linked at 2 | AuraBreathDome_Draw 2000 |
| A49 | Draw: link 0x44 | AuraBreathDome_Draw 2000 |
| A50 | Draw: tpage x 0x300 | AuraBreathDome_Draw 2000 |
| A51 | Draw: CLUT 0x1FB | AuraBreathDome_Draw 2000 |
| A52 | Draw: half at band / 2 | AuraBreathDome_Draw 2000 |
| A53 | Draw: parity bit 1 | AuraBreathDome_Draw 2000 |
| A54 | Draw: bottom - 2 | AuraBreathDome_Draw 2000 |
| A55 | Draw: u3 + 0x40 | AuraBreathDome_Draw 2000 |
| A56 | Draw: v of the next band | AuraBreathDome_Draw 2000 |
| A57 | Draw: corners 1 / 2 swapped | AuraBreathDome_Draw 2000 |
| A58 | Draw: blue from 0x90385A | AuraBreathDome_Draw 1987 |
| A60 | InReach: strictly within | AuraBreath_InReach 460 |
| A61 | InReach: record x sar 8 | AuraBreath_InReach 982 |
| A62 | InReach: reach unsigned | AuraBreath_InReach 86 |
| A63 | InReach: dz against x | AuraBreath_InReach 1001 |


## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat. Things
to look for, by reading only:

- Magic Ball: a ball from above the caster to the target with sparks, a disc
  and a ring; eight smaller orbs following; two expanding rings at the hit.
- Intimidate: two ribbons curving from beside the caster to the target, a
  flat flash and a sprite animation where they land.
- Aura Breath: a dome of textured bands growing round the caster, each enemy
  it reaches flagged once.

`IntimidateTrail_Start`'s enemy offsets (kinds 0x61 and 0x6C at record
+0x8C) say that two enemy kinds cast Intimidate from a different place; which
enemies those are was not read.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the three stack tables and the
  eight `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in all four
  creators (`MagicBall_Start` nine times, `Intimidate_Start` twice,
  `IntimidateTrail_Fly`, `AuraBreath_Start`): slot 255 lies past the image's
  end - an access violation, in ours as in the original (the same address is
  written).
- **`MagicBallOrb_Fly` reads the task slot its +4 names, unchecked**: +4 is
  the core's slot as `MagicBall_Start` got it, so a "none free" there (above)
  would make every orb read past the image too.
- **`MagicBall_DrawSpark` / `_DrawSparkShort` loop until a random walk falls
  below its floor**: the radius steps by `sin(wave) x (Rand & mask) >> 12`
  over a 32-step wave whose sum is about 0, so the loop's length is unbounded
  in principle (every step draws one more line). Faithful in ours.
- **The ribbons' gap scan is unbounded**: from point +0xB while the x word is
  -1 (0xFFFF), with no limit; once a trail's points are all gaps it runs on
  into `Magic213Mote_Pool` and beyond until some word is not 0xFFFF. It reads
  only. Whether `_Fade` can make a trail all gaps before it is freed: it
  pushes one gap a frame for +9 = 8 frames over +0xA - +0xB points, so by
  reading the last frames draw from gaps at the head - not measured.
- **`MagicBallOrb_DrawDisc` indexes `MagicBallOrb_Shades` by +0xB,
  unchecked**: the orbs' +0xB is 0..7 from `MagicBall_Start`; past 9 it reads
  the next table's pointer bytes as shades.
- **`IntimidateTrail_*` index their points by +4 and +0xA / +0xB,
  unchecked**: +4 is 0 or 1 from `Intimidate_Start`; a larger one writes into
  `Magic213Mote_Pool` and on.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy got 40 lines under a `group S36` comment: 35 new,
plus five host extents re-listed smaller (`004F2AA0 23D`, `004F3140 21D`,
`004F46D0 8B`, `004F48D0 184`, `004F5970 60`). Five were listed right already
(`004F2680`, `004F2860`, `004F2F20`, `004F4640`, `004F55E0`). MAGIC169's host
line `004F1DE0 89B` covers MAGIC172's first eleven functions; their own lines
are the smaller extents.
