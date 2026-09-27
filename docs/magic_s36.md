# Group S36: Magic Ball, Intimidate and Aura Breath (MAGIC172, 173, 218)

**Status:** IN PROGRESS (2026-09-27). All 45 functions are ours
(`src/game/magic_s36.cpp`, shadow name `magic_s36`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 90,000 rounds. CONTROLS_SUMMARY Nothing recorded casts
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

- **Callees** (34 listed; the standard set supplies the rest):
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
  (`Group::args`), `AuraBreath_InReach` an enemy record one step either side
  of the reach.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  word, `0x9039D8`, a trail word, a struck byte, the task's +0x2E / +0x30 /
  +0x10 / +0x14 / +0x5D.

Result in this worktree (2026-09-27):

    shadow      magic_s36 self-test: 90000 rounds over 45 functions (2000 each), 9956048 calls to the stand-ins,
                0 MISMATCHES; 27684 bytes of state (17 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). STAR_RESULT

## 6. Controls

CONTROLS_TABLE

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
