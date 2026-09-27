# Group S15: Chill, Foretell and Influence (MAGIC067, 068, 069)

**Status:** IN PROGRESS (2026-09-26). All 52 functions are ours
(`src/game/magic_s15.cpp`, shadow name `magic_s15`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 104,000 rounds. CONTROLS_SUMMARY Nothing recorded
casts these spells, so this is fuzz only until the owner sees them cast.

Round nine, fourth spell wave, group S15
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4, §6b).

| Row | File | Overlay | Ability id | Read one id down | Extent | Functions |
|--:|---|---|---|---|---|--:|
| 18 | 0x258 | MAGIC067 | 0x43 | Chill | `0x4B66D0..0x4B73F6` | 16 |
| 40 | 0x259 | MAGIC068 | 0x44 | Foretell | `0x4B7400..0x4B7D39` | 14 (+1 ours before) |
| 75 | 0x25A | MAGIC069 | 0x45 | Influence | `0x4B7DE0..0x4B8D66` | 22 |

The extents are `tools/magic_rows.py --unit MAGIC06N --clones` (capstone
recursive descent; one jump table, in `ChillRay_PushMatrix`, bounded by its
`cmp 3` and moved into the copy by the harness; nothing `REFUSED`). All 52
lie in the units' extents; none was found inside or missing from them.
MAGIC068's extent also holds `MagicFx_PushActorMatrix` (`0x4B7D40`, ours
since round seven, reached from 39 files); it is called by name here. 9,299
bytes, as the queue counted.

The names are the sibling's labels read one id down
([`cut-content.md`](cut-content.md) §2) - hypotheses, and the function names
below say what the code does, not what the spell is. What each spell looks
like in play has not been measured.

## 1. What each function does

`symbols.toml` gives each function to the instruction. In outline:

- **Chill (MAGIC067)** - Corona's code (group S31's MAGIC137) with other
  constants, and a marker step:
  - `Chill_Task`, the kind-2 task: a five-entry stack table by +1 -
    `Chill_Start`, MAGIC137's `Corona_Wait` (target flags 0x20 after 24
    frames), `Chill_WaitChildren`, `Chill_SpawnMarks`, MAGIC131's
    `MagicFx_EndWhenChildrenDone`.
  - `Chill_Start` makes a ray (kind 1, 0x13, +1 0, the owner's direction)
    and a flash (+1 1), restores CLUT row 2 with the STP bit and the first 32
    words of row 26 without it; in an event battle whose acting enemy's
    +0x100 is 0x29, a copy of that enemy's record (+1 3) that plays
    animation 3 and runs the engine's `0x43EC10`.
  - `Chill_WaitChildren`: once both have ended, the target flagged 0x40.
  - `Chill_SpawnMarks`: 30 frames on, one marker child (+1 2) at each actor
    of the target's side that is not out; they run MAGIC038's
    `WarShoutBuff_Start` then MAGIC105's `MagicFx_EndWithChildren`
    (`ChillMark_Run`).
  - The ray (`ChillRay_Run`, four steps by +2): `_Start` centres on the side
    and starts at an offset turned by the direction; `_Grow` and `_Shrink`
    raise and lower it (also Corona's steps 1 and 2); MAGIC137's
    `CoronaRay_Advance` moves it on and ends it. Each frame the ray's matrix
    (`_PushMatrix`, turned about z) and a fan of gouraud-textured quads
    (`_Draw`: radius 0x200 up, CLUT row 0x1E2, a scrolling u).
  - The flash (`ChillFlash_Run`): Corona's flash steps (MAGIC137, 086, 078,
    060) and `ChillFlash_Draw`, a full-screen gouraud quad bright on the
    owner's side.
- **Foretell (MAGIC068)**:
  - `Foretell_Task`: `_Start`, `_Read`, `_Pause`, `_Show`, `_End`.
  - `Foretell_Start` takes the owner's place and makes one child (kind 1,
    0x1C): `ForetellOrb_*` shows a 12-wide texture cell above the caster that
    counts three steps of 15 frames with sound 0x100 each, then sound 0x101
    and the cell at u 0xC for 15 frames, and ends.
  - `Foretell_Read`, once the child has ended: a score from both sides -
    each present member (+0 set; one with +0x91 bit 0x40 takes 0x18 off and
    is not counted) and each enemy not out adds a step by the ratio word
    +0x98 / +0xA0 (members) or +0xA4 / +0xB0 (enemies) in percent; then the
    battle byte `0x904AB2` less the enemies counted, times 8; a band of the
    difference between the members' mean byte +0x8A and the enemies' mean
    word +0x98; and `Foretell_Bias` by the count difference. The score picks
    one of the system messages 0x3C..0x40, queued with
    `BattleQueue_Push(1, 0x3C, text)`. Then +0xB gets one bit for each of six
    record bytes (+0xBF, +0xC0, +0xC1, +0xC3, +0xC2 at most 1; +0xC4 at most
    3) of any enemy not out. What the words and bytes mean (HP, levels,
    elements?) is not measured.
  - `_Pause` waits 4 frames; `_Show` draws the six bits as coloured 8 x 12
    tiles (`Foretell_DrawTiles`, colours from `Foretell_TileColours`; a grey
    tile when none is set) until a button is pressed (`Input_Pressed`);
    `_End` waits for the message window to close (`0x939F60`), sliding the
    tiles up by 8 a frame, then ends the effect.
- **Influence (MAGIC069)**:
  - `Influence_Task`: `Influence_Start`, then MAGIC222's `BattleFx_Finish`.
  - `Influence_Start` takes the source sprite's screen point, makes six
    children (kind 1, 0x39): three `InfluenceRight_*` (+1 0) and three
    `InfluenceLeft_*` (+1 1), numbered 0..2 with delays 1..3; restores 48
    words of CLUT row 26; sound 0x100.
  - Each child after its delay starts 0x37 right (left) of the owner's point
    and slides back to it by a decreasing step of 11..1, drawing a dashed
    ring (`Influence_DrawRing`), four fans of three triangles
    (`Influence_DrawTriangles`, angles from `Influence_TrianglePoints`) and,
    past step 2, a cross (`Influence_DrawCross`), all semi-transparent on
    layer 3. The first right-hand child alone goes on: sound 0x103, 16
    frames, then `Influence_SpawnMarks`; it blinks until it is the owner's
    last child, sound 0x101, and shrinks away. The others end at the point.
  - `Influence_SpawnMarks`: one marker (+1 2) for every enemy, then every
    member, not out, not the acting actor, not the target, whose record has
    a word at most 1 (+0xBA for an enemy, +0xAA for a member) or a flag bit
    (+0x92 / +0x90 bit 0x20, dword +0x114 / +0x134 bit 0x4000, a member's
    +0x134 bit 0 too).
  - `InfluenceMark_*`: the actor's sprite tinted black, a textured quad above
    it (lifted by its height byte: a member's pair at `0x64DFC8` by +0x89,
    an enemy type's byte at `0x8C564F`, S20's), the tint record brightened
    to 0x10 and back; at its end sound 0x102 (first marker only), the tint
    released, the actor flashed; the quad grows to 0x18 over 20 frames and
    MAGIC058's `0x4AF490` counts the owner down and frees it.

## 2. Divergence

No ledger entry. Each function is a faithful replacement, with three
exceptions that follow the project's precedents:

- a phase past any of the fifteen dispatch tables (five stack tables, ten
  `.data` tables) aborts ([`magic_fx_reached.md`](magic_fx_reached.md) §3);
- `ChillRay_PushMatrix` aborts on a direction byte past 3, where the
  original's four-entry jump table skips the store and turns the ray by an
  uninitialised stack word (S31's `CoronaRay_PushMatrix`, the same code);
- `Foretell_Read` aborts with a message on a zero divisor - no member
  counted, no enemy counted, or a counted record's divisor word 0 - where
  the original's `idiv` faults (the live-target divide, the owner's word of
  2026-09-26, [`takeover-queue-round9.md`](takeover-queue-round9.md) §6).

Calls that push one argument more than the callee takes, as the originals
do, push it in ours too: `Gte_RotTrans` gets a flag pointer, the projection
a depth and a flag pointer.

## 3. Calls to other units

By raw address (a phase in a table; never bound or renamed here):

| Address | Owner | Reached as |
|---|---|---|
| `0x43EC10` | engine, unnamed | entry 1 of `ChillEnemy_Task`: the script until the done flag (as S31 calls it) |
| `0x446770` | engine, unnamed | the dx / dz turn by direction (`ChillRay_Start`; as S22, S31 call it) |
| `0x4B1740` | MAGIC060 (S12) | entry 3 of `ChillFlash_Steps` |
| `0x4AF490` | MAGIC058 (S11) | entry 4 of `InfluenceMark_Steps`: the owner's +0xB down, the task freed |

By name, already ours: `Corona_Wait`, `CoronaRay_Advance`,
`CoronaFlash_Start` (S31), `MagicFx_EndWhenChildrenDone` (S30),
`BarrierRing_Grow` (S19), `MagicFx_WaitA` (S17), `WarShoutBuff_Start`
(S07), `MagicFx_EndWithChildren` (S24), `BattleFx_Finish` /
`BattleFx_FreeTask`, `MagicFx_CenterOnSide` and `MagicFx_PushActorMatrix`
(L, round seven), and the GTE / GPU / sprite / sound / message library.

Called by others: S31's `CoronaRay_Steps` holds `0x4B6B20` / `0x4B6B70`
(its `kRayPhase1` / `kRayPhase2`), now `ChillRay_Grow` / `ChillRay_Shrink`;
they can be rebound to the names.

## 4. Named data (`symbols.toml` `[[data]]`)

| Table | Address | Entries |
|---|---|--:|
| `ChillChild_Kinds` | `0x65AC78` | 4 |
| `ChillRay_Steps` | `0x65AC88` | 4 |
| `ChillFlash_Steps` | `0x65AC98` | 4 |
| `ChillMark_Steps` | `0x65ACA8` | 2 |
| `Foretell_Bias` | `0x65ACB0` | 12 bytes (s8, read at +7 + index) |
| `Foretell_TileColours` | `0x65ACBC` | 6 x 6 bytes |
| `ForetellChild_Kinds` | `0x65ACE0` | 1 |
| `ForetellOrb_Steps` | `0x65ACE4` | 4 |
| `InfluenceChild_Kinds` | `0x65ACF4` | 3 |
| `InfluenceRight_Steps` | `0x65AD00` | 5 |
| `InfluenceLeft_Steps` | `0x65AD14` | 2 |
| `InfluenceMark_Steps` | `0x65AD1C` | 5 |
| `Influence_TrianglePoints` | `0x65AD30` | 4 bytes |

Each handler table's count is `magic_rows.py`'s, checked against the next
table's start in a dump of `0x65AC70..0x65AD4C` (2026-09-26); the two
byte tables are read in place (the doc copies none of their values).

## 5. The fuzz

`BOF3X_SHADOW=magic_s15` runs `magic_harness::Run` over the 52 clones, 2,000
rounds each, with no harness edits; what the harness lacks is built in
`magic_s15_fuzz.cpp`:

- **Callees** (36 listed; the standard set supplies the rest):
  - `Gfx_CommitPrim` has an `effect` that logs the primitive's bytes and
    moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's own
    (every primitive of a draw is built at the same pointer);
  - the projection logs its SVECTORs through `deref`; the ray's matrix
    push's GTE callees log theirs and write a result where the real ones
    write (S22's effects);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - `Sprite_SetAnimation` logs which sprite;
  - `Battle_ActorIsOut` (`kFlag`: every caller tests al) keeps one enemy in
    while `Foretell_Read` is fuzzed and puts a 1 in an in-enemy's divisor
    word if a disturbance zeroed it (the read follows the answer);
  - `Gte_PushMatrix` keeps the facing inside 0..3 while
    `ChillRay_PushMatrix` is fuzzed;
  - this group's own draws and pushes and `Influence_SpawnMarks`, by address
    (their callers call them directly).
- **`settle`:** while `InfluenceMark_Start` is fuzzed, the task's +0xB (the
  actor index it reads again after two calls) stays 0..10: past the enemy
  records it reads past the image on both sides (the game writes only
  0..10 there).
- **Tables:** the ten `.data` handler tables of section 4.
- **Regions** beyond the standard ones: `Gfx_PacketNext` and the packet
  buffer; `Prim_VertexScratch`; `0x903850..0x90385F`; `Field_Kind2Z` /
  `Field_Kind2X`; `MoveScript_TintRecords`; `Input_Pressed`; CLUT rows 2 and
  26 and their sources; `Foretell_Bias` + `Foretell_TileColours`;
  `Influence_TrianglePoints`; the lift bytes of enemy types 0..7. 25,906
  bytes of state in 22 regions.
- **Seed:** each dispatcher inside its table; each count-down one step
  before and at its threshold; the enemies' types 0..7; the target's side
  bit half the time; for `Chill_Start` the event-battle byte and the acting
  "enemy"'s +0x100 0x29, each two times in three; for `Foretell_Read` every
  divisor word 1..0x3FF with its ratio word mostly below it (the 0..70 %
  bands), the means close (the level bands), the six flag bytes 0..4 half
  the time, at least one member counted; `Input_Pressed` 0 half the time;
  the message byte 0 half the time; the tint record at 0xF / 1 for
  Brighten / Fade; the Influence record words and bits around their tests;
  the ray's quad count mostly below 0x30 and step 3 half the time.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte (the ray's loop bound, word 0x903850, kept below 0x30),
  `Field_Kind2*`, the task's +0xC / +0xD / +0x2E..+0x31 / +0x34 / +0x38, a
  tint byte.

Result in this worktree (2026-09-26):

    shadow      magic_s15 self-test: 104000 rounds over 52 functions (2000 each), 1915826 calls to the stand-ins,
                0 MISMATCHES; 25906 bytes of state (22 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`; the thinnest, `Influence_DrawCross` by
address, 181 times). ALL_RESULT

## 6. Controls

CONTROLS_TEXT

## 7. What nothing reached

No recorded route casts any of these spells (queue §5); the live check is
the owner casting them, with a save that has them or DIV-0045's cheat.
Things to look for:

- Chill: a ray crossing the field, a screen flash from the caster's side,
  then a marker at each actor of the target's side; in an event battle a
  copy of the caster.
- Foretell: a counting cell above the caster, a battle message chosen by
  the score, and up to six coloured tiles until a button is pressed.
- Influence: six rings sliding in to the caster from both sides, then a
  tinted quad over each actor whose record passes `Influence_SpawnMarks`'
  test.

`Chill_Start`'s third child is reached only in an event battle whose acting
enemy's +0x100 is 0x29 (the same test as Corona's); which boss that is was
not read. Which message each score picks (the text of system messages
0x3C..0x40) was not read either.

## 8. Latent defects (Capcom's, kept)

Described here, not numbered:

- **Every dispatcher's index is unchecked**: the five stack tables and the
  ten `.data` tables. Ours aborts.
- **`BattleTask_Create`'s "none free" (0xFF) is unchecked** in every
  caller: `Chill_Start` (three calls), `Chill_SpawnMarks`, `Foretell_Start`,
  `Influence_Start` (six), `Influence_SpawnMarks` - slot 255 is
  `0x9423FC`, past the image's end (`0x93F000`): an access violation, in
  ours as in the original.
- **`Foretell_Read` divides by counts and record words that can be 0**: no
  present member without +0x91 bit 0x40, no enemy in, or a counted member's
  word +0xA0 / enemy's word +0xB0 of 0 - an integer-divide fault in the
  original, an abort in ours (section 2). Whether a battle can reach one
  (every member flagged 0x40 when the spell resolves) is not measured.
- **`ChillRay_PushMatrix` turns by an uninitialised word** for a direction
  past 3 (ours aborts). The ray's +8 comes from the owner's +8 in
  `Chill_Start`.
- **`Chill_Start` indexes the enemy records by the actor byte - 3**,
  unchecked: with a party actor it reads a "record" among the task slots.
  Reached only in an event battle.
- **`InfluenceMark_Start` indexes the records by its +0xB**, unchecked; only
  `Influence_SpawnMarks` writes it, with 0..10.

## 9. For `analysis/calltrace/entries_logic.txt`

The main checkout's copy got 47 lines: 44 new, and three host extents
re-listed at the function's own size (`004B6BC0 DC` under `D3D`, `004B7900
10A` under `2C6`, `004B8BD0 197` under `1A0`; the consolidation keeps the
smaller). Five were listed right already (`004B7BD0`, `004B8530`,
`004B8690`, `004B8910`, `004B8A90`). The host line `004B6620 593`
(MAGIC066's, group S14) runs over `Chill_Task` onwards; the consolidation
cuts it at `004B66D0`.
