# Group S33: Accession and Mighty Chop (MAGIC151, 154)

**Status:** IN PROGRESS (2026-09-27). All 57 functions are ours
(`src/game/magic_s33.cpp`, shadow name `magic_s33`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 114,000 rounds. @@CONTROLS_SUMMARY@@ Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, fifth spell wave, group S33
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 86 | 0x29E | MAGIC151 | 0x97 | Accession | `0x4EAE70..0x4ED0B1` | 44 |
| 35 | 0x29F | MAGIC154 | 0x9A | Mighty Chop | `0x4ED0C0..0x4ED66E` | 13 (+ `BattleFx_SetSize`, ours before) |

The extents are `tools/magic_rows.py --unit MAGIC151 / MAGIC154 --clones`
(capstone recursive descent; no jump table, nothing `REFUSED`). All 57
functions lie in the units' extents; none was found inside or missing from
them. MAGIC154 also holds `BattleFx_SetSize` (`0x4ED5C0`, round eight's CK),
which its copy's step table calls. 9,790 bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses. As in S31, the queue
lists the rows (35, 86) against the overlays in file order, but `Magic_Rows`
pairs them the other way round: MAGIC151 is row 86, MAGIC154 row 35
(`analysis/magic_rows.tsv`). What each spell looks like in play has not been
measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Accession (MAGIC151): the task.**
  - `Accession_Task`, the kind-2 task: `_Start`, `_Run`, `_End` by +1.
  - `Accession_Start`: the engine's `0x4514A0` (unnamed: resets the acting
    member's battle state; not read to its end), a banner (`BattleBanner_Add`
    kind 1, 0x3C frames) of the first eight characters of the system message
    the byte table `0x64ECB0` gives for the battle byte `0x904B89` (magic_lib's
    `kFormation`); +2 up when that byte is 3, 0xB or 0xD (+2 picks the step
    table below); +9 3 or 0x1E by the target's party record +0x89.
  - `Accession_Run` picks `Accession_StepsA` or `_StepsB` by +2; each is a
    five-step table by +3 sharing `_ActorPose` (animation +8 + 0x3C ensured on
    the acting member's record, Sprite_Current swapped to it), `_ActorScript`
    (the member's script ticked once a frame; at its end the controller child
    is made and +3 moves on) and `_Finish` (the member's +0 bit 0x40 cleared,
    +1 on).
  - Path A (`_LoadFormA`, `_ApplyA`): once the controller signals (+0xB 1), the
    DAT file `u16 [ptr][party set]` is loaded, `ptr` from `0x64E9BC + 8 x
    0x904B89` (the second pointer when the owner faces neither 0 nor 1); once
    loaded, the acting member's tint, palette, status tint, CLUT STP bits and
    animation are rebuilt.
  - Path B (`_LoadFormB`, `_ApplyB`): every party record +0 bit 0x40 and the
    table's first file; once loaded, each present member's tint, queued item
    and turn are undone, the **acting member's record is copied over party
    record 0** and placed at the fight's centre, members 1 and 2 are cleared
    (+0 0), the actor byte becomes 0, the battle windows 0xD.. are allocated
    again, and the new member 0 is rebuilt through `Field_MemberSprite` for
    the party set's first member. What this is in play is not measured.
  - `Accession_End`: once the controller has ended (+0xB 2), the done flag,
    the acting member's +0x134 bit 2 cleared and bit 0x20 set, freed.
- **Accession: the children** (kind 1, parameter 0x44, `AccessionChild_Task`
  by +1 through `AccessionChild_Kinds`):
  - the controller (`AccessionCtl_*`, +1 4): a bolt at once (sound 0x102), a
    ring four frames later, an orb four after that (sound 0x103); 0x14 frames
    on it signals the task (+0xB 1), waits for the task's +1 to reach 2, and
    when its three children have ended signals +0xB 2 and ends;
  - the orb (`AccessionOrb_*`, +1 0): grows its radius +9 to 0x20, emits a
    spark every fourth frame until the owner reaches step 5, waits for the
    sparks, dims and fades; under the actor matrix it draws a **shell** (a dome
    of 8 x 16 flat semi-transparent quads, tpage 0x55) and a **glow** (a band
    of 16 gouraud quads);
  - the sparks (`AccessionSpark_*`, +1 1): `_Draw(a1, a2)` draws +0xA lines
    wandering over a sphere of radius 0x200 from two angle bytes of
    `AccessionSpark_Angles` (`0x65BEA0`, 16 pairs by +4);
  - the bolt (`AccessionBolt_*`, +1 2): S22's shape - `_Start`, `_Rise`, then
    S22's `MyollnirBolt_Hold` and `JoltBolt_End`; under S25's
    `SpellSleep_PushTurnMatrix`, three rows of `_DrawBand` (17 steps of four
    gouraud quads);
  - the rings (`AccessionRing_*`, +1 3): a radius +0x14 from 0x100 widening to
    0x400, then S29's `DivineBeam_Widen`; two flat rings of 64 gouraud quads.
- **Mighty Chop (MAGIC154).**
  - `MightyChop_Task`: `_Start`, `_Throw`, `_End`, `BattleFx_Finish` by +1.
  - `MightyChop_Start`: the actor's animation 0xC; a **copy** of the acting
    actor's record (its first 0x80 bytes: a party record below 3, else the
    enemy's) as a child (kind 1, 0x52, +1 1); CLUT row 26's first 16 words
    back from their source; the owner's +0 bit 0x40.
  - The copy (`MightyChopCopy_*`, through `MightyChopChild_Kinds`):
    `BattleFx_SetSize`, `_Play` (its script until its end, the actor's sound
    (2, 4)), `_Wait` (its script to the end again, then the parent's +9 down),
    `BattleFx_FreeTask`.
  - `MightyChop_Throw`: once the copy has played (+0xB 0), six **blades**
    (the same kind, +1 0) at the source sprite plus the offset (0, -0x18000)
    turned by its direction, each started 3 i + 1 frames later.
  - A blade (`MightyChopBlade_*`): a draw-mode packet on layer 3, the
    frame-offset table `0x9039D8` swapped to the effects' `0x8E3580` round its
    step and screen update; `_Start` places it +0xB x 0x8000 along the owner's
    direction with sprite fields and animation 0; `_Grow` / `_Shrink` scale
    +0x40 / +0x44 up and down over four frames each, then free it. Entry 3,
    `_Sink`, is reached by no step.
  - `MightyChop_End`: once the copy's second script has ended (+9 0), the
    actor's animation 4, the owner's bit cleared, target flags 0x10.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with one exception
that follows the project's precedent: a phase past any of the fifteen
dispatch tables (eight stack tables, seven `.data` tables) aborts
([`magic_fx_reached.md`](magic_fx_reached.md) §3).

`AccessionSpark_Draw` keeps the original's use of its first argument's stack
slot as the running longitude (ours keeps it in a local byte: the same value
at every use).

## 3. Calls to other units

By raw address (never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x446770` | engine, unnamed | the dx / dz turn by direction (`MightyChop_Throw`, `MightyChopBlade_Start`; as S22, S23, S31 call it) |
| `0x4514A0` | engine, unnamed | `Accession_Start`'s first call: resets the acting member's battle state (no arguments) |

By name, already ours: `MyollnirBolt_Hold` and `JoltBolt_End` (S22),
`SpellSleep_PushTurnMatrix` (S25), `DivineBeam_Widen` (S29),
`BattleFx_SetSize`, `BattleFx_Finish`, `BattleFx_FreeTask` (CK),
`MagicFx_PushActorMatrix`, and the GTE / GPU / sprite / sound / battle
library (`Field_MemberSprite`, `Window_Alloc`, `Battle_ReturnQueuedItem`,
`LoadDatFile`, `BattleBanner_Add`, ...). No other wave-five group's address is
called.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `AccessionChild_Kinds` | `0x65BE74` | 5 |
| `AccessionOrb_Steps` | `0x65BE88` | 6 |
| `AccessionSpark_Angles` | `0x65BEA0` | 32 bytes |
| `AccessionSpark_Steps` | `0x65BEC0` | 3 |
| `AccessionBolt_Steps` | `0x65BECC` | 4 |
| `AccessionRing_Steps` | `0x65BEDC` | 3 |
| `MightyChopChild_Kinds` | `0x65BEE8` | 2 |
| `MightyChopBlade_Steps` | `0x65BEF0` | 4 |

Each count is where the next table starts (the dump of `0x65BE60..0x65BF0C`
read 2026-09-27); the next overlay's data starts at `0x65BF00`. The tables
MAGIC151 reads by `0x904B89` and the party set (`0x64ECB0`, `0x64E9BC`,
`0x669750`) and the window offsets (`0x64DF70`, `0x64E2BC`) are the engine's
and are read in place, not named here.

## 5. The fuzz

`BOF3X_SHADOW=magic_s33` runs `magic_harness::Run` over the 57 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s33_fuzz.cpp`:

- **Callees** (43 listed; the standard set supplies the rest):
  - the draws: `Gfx_CommitPrim` and `MapView_LinkPrimAt` have an `effect`
    that logs the primitive's bytes and moves `Gfx_PacketNext` on through a
    0x2000-byte buffer of the fuzz's own (S20, S24);
  - the projections log their SVECTORs through `deref` (6 bytes each);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - every call that acts on `Sprite_Current` (`Sprite_ScriptTickOnce`,
    `_ScriptTick`, `_SetAnimation`, `_EnsureAnimation`, `_LoadPalette`,
    `_SetClutStp`, `Battle_StatusTint`, `Field_MemberSprite`,
    `Sprite_UpdateScreen`) logs which sprite - MAGIC151 swaps it to a party
    record round them - and the screen update also logs `0x9039D8`;
  - `File_LoadDone` answers `kFlag` (its callers test all 32 bits);
  - this group's own draws, called directly, by address.
- **Tables:** the seven `.data` handler tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `0x9039D8`; the first 16
  words of CLUT row 26 and their source; the fight's centre `0x903780`;
  `0x904B50..0x904B9F` (`0x904B79`, `0x904B89`, `0x904B8A`, `0x904B8F`); the
  party set `0x90412C`; `0x939B00..0x939EFF` (the per-member cells
  `0x939B07` / `0x939C05` / `0x939C10`). 20,788 bytes of state in 19 regions.
- **Seed:** `0x904B89` always one of the 23 kinds whose `0x64E9BC` pointers
  are not null (entries 10, 19 and 20 are: a null there is a fault on both
  sides); each dispatcher inside its table; each count-down one step before
  and at its threshold; `Accession_Start` 3 / 0xB / 0xD and a party target
  with +0x89 0; the load gates +0xB 1 and the owner's facing 0..3;
  `Accession_ApplyA`'s bit 14 and `0x904B8A` equal to the actor; the orb's
  frame counter and the owner at step 5; the spark's +4 inside its 16 pairs
  and +0xA small; `AccessionBolt_DrawBand`'s three words its caller's rows two
  times in three (`Group::args`).
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex or scratch word,
  the fight's centre, `0x9039D8`, a task word (+0xC, +0x10, +0x14, +0x3E,
  +0x40, +0x44, +0x5D), a byte of party records 0..2 the rebuild reads (+0,
  +5, +8, +0x27, +0x2E, +0x30, +0x89, +0x90), and `0x904B79`, `0x904B89`,
  `0x90412C`, `0x904AB0`, `0x904AAC`, `0x904B8A` (`0x904B89` is read through a
  pointer only before any call).
- **Settle:** while `MightyChopBlade_Run` is fuzzed, +2 is kept inside its
  four-entry table after every disturbance: the original reads it after two
  calls, and past the table it calls through whatever follows (ours aborts).
  Found by the first run (`FATAL: MightyChopBlade_Run: phase 252`).

Result in this worktree (2026-09-27):

    shadow      magic_s33 self-test: 114000 rounds over 57 functions (2000 each), 7001837 calls to the stand-ins,
                0 MISMATCHES; 20788 bytes of state (19 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals (coverage
line in `build/bof3x.log`). @@STAR_RESULT@@

## 6. Controls

@@CONTROLS@@

## 7. What nothing reached

No recorded route casts either spell (queue §5); the live check is the owner
casting them, with a save that has them or DIV-0045's cheat. Things to look
for, by reading:

- Accession: a banner with a name; the caster's script; a bolt, rings, an orb
  with sparks; a file load and the caster rebuilt - by path B, the caster
  becoming party member 0 with the other two cleared and the windows rebuilt.
  Which kinds of `0x904B89` take path B (3, 0xB, 0xD) and what the loaded
  files hold was not read.
- Mighty Chop: the caster's animation 0xC, a copy of it playing its script,
  six blades that grow and shrink, then animation 4 and target flags 0x10.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the eight stack tables and the
  seven `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** everywhere this
  group creates a child (`Accession_ActorScript`, the controller's three,
  `AccessionOrb_Emit`, `MightyChop_Start`, `MightyChop_Throw`): slot 255 is
  past the image's end, an access violation in ours as in the original.
- **`0x904B89` indexes three tables unchecked**: `0x64ECB0` (bytes),
  `0x64E9BC` (26 pointer pairs, three of them null, dereferenced at once) -
  a kind of 10, 19 or 20, or past 25, reaching `Accession_LoadFormA` / `_B`
  faults. Whether a caster can have such a kind is not measured.
- **The acting member is indexed as a party record unchecked** (0..2 by
  design): an enemy actor would make `Accession_*` read and write past party
  record 4, and `Accession_ApplyB` would copy that over party record 0.
- **`AccessionSpark_Run` indexes `AccessionSpark_Angles` by +4 unchecked**
  (16 pairs; `AccessionOrb_Emit` gives +4 & 0xF).
- **`MightyChop_Start` indexes the enemy records by the actor byte - 3**,
  unchecked, and its 0x80-byte copy may overlap the new slot (ours copies
  dword by dword, forward, as `rep movsd` does).
- **`MightyChopBlade_Run` reads its phase after two calls** (a draw-mode
  commit): harmless in the game, where nothing those calls do moves +2.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy gets 55 lines under a `group S33` comment: 51 new,
plus four host extents re-listed smaller (`004EBDB0 337`, `004EC240 2B6`,
`004EC640 531`, `004ECE80 232`, each the function's own size). Two were listed
right already (`004EBAA0`, `004ECC50`).
