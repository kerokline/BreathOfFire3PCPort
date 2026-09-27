# Group S15: Chill, Foretell and Influence (MAGIC067, 068, 069)

**Status:** IN PROGRESS (2026-09-26). All 52 functions are ours
(`src/game/magic_s15.cpp`, shadow name `magic_s15`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)) without edits to
it: 0 mismatches over 104,000 rounds. 251 of 254 negative controls refused, 3 equivalent mutants recorded with a refused near variant each. Nothing recorded
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

- **Callees** (37 listed; the standard set supplies the rest):
  - `BattleTask_Create`, listed over the standard one: while
    `Chill_SpawnMarks` is fuzzed each create moves a byte of one actor's
    position (+0x34..+0x3F), so a marker that copied the position before its
    create rather than after shows (control C26; the harness's own
    disturbance moves only the target's record, too rarely);
  - `Gfx_CommitPrim` has an `effect` that logs the primitive's bytes and
    moves `Gfx_PacketNext` on through a 0x2000-byte buffer of the fuzz's own
    (every primitive of a draw is built at the same pointer);
  - the projection logs its SVECTORs through `deref`; the ray's matrix
    push's GTE callees log theirs and write a result where the real ones
    write (S22's effects);
  - `0x446770` logs the task's direction and dx / dz and writes a new pair;
  - `Sprite_SetAnimation` logs which sprite;
  - `Battle_ActorIsOut` (`kFlag`: every caller tests al); while
    `Foretell_Read` is fuzzed its enemies' answers follow a mask the seed
    chose, one enemy is always in, and an in-enemy's divisor word zeroed by a
    disturbance gets a 1 (the read follows the answer);
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
  divisor word 1..0x3FF with its ratio word 0 or below it (the 0..70 %
  bands), at least one member counted, the six flag bytes 0..4 half the
  time, the enemies' out-mask; then a quarter of the time the kept enemy's
  word +0x98 near 0x8000 (the 16-bit mean far below 0, the difference past
  16 bits), else two times in three the members' bytes and the enemies'
  words set so the means' difference is on a level-band edge; then, two
  times in three, the fuzz's replica of the score picks the bias byte that
  puts the score on one of the message bounds 0, 0x1E, 0x32, 0x46, 0x5A,
  100 or one either side (section 6); `Input_Pressed` 0 half the time;
  the message byte 0 half the time; the tint record at 0xF / 1 for
  Brighten / Fade; the Influence record words and bits around their tests;
  the ray's quad count mostly below 0x30 and step 3 half the time.
- **Disturb** (the group's case): `Gfx_PacketNext`, a vertex word, a scratch
  byte (the ray's loop bound, word 0x903850, kept below 0x30),
  `Field_Kind2*`, the task's +0xC / +0xD / +0x2E..+0x31 / +0x34 / +0x38, a
  tint byte, a byte of any actor's position (+0x34..+0x3F).

Result in this worktree (2026-09-26):

    shadow      magic_s15 self-test: 104000 rounds over 52 functions (2000 each), 1951585 calls to the stand-ins,
                0 MISMATCHES; 25906 bytes of state (22 regions) and the stand-ins' log compared

Every callee listed and every handler was called by the originals
(coverage line in `build/bof3x.log`; the thinnest, `Influence_DrawCross` by
address, 181 times). `BOF3X_SHADOW='*'`: exit 0 (1,977,741 stand-in calls for this group in that run: the harness's pointers into the DLL move a few branches, 0 mismatches).

## 6. Controls

254 plants, each put in `magic_s15.cpp` one at a time by a script (not committed; the round's scratchpad, `s15/controls.py` and `run_controls.py`) that planted, rebuilt, checked the build had recompiled the file, ran `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s15`, restored; after the last it restored, rebuilt and ran the clean self-test (0 mismatches, exit 0). The C-, F- and I- controls are Chill, Foretell and Influence.

**251 of 254 refused**, 2 of them by a Fatal rather than a count (C2, F43: C2 names a handler no stand-in covers, and its near variant C2b is refused by a count; F43 makes ours count no enemy and abort, where the original would fault). **3 equivalent** mutants (no input can tell them apart), each with a near variant that is refused: F19, F30, I78. None other stands.

Foretell_Read's score is seen only through which of five messages it picks, so a plant that moves it by one shows only on a message bound. The seed reckons the score before the bias byte from the seeded records (a replica in the fuzz, with `Battle_ActorIsOut`'s answers following a seeded mask) and sets the bias byte so the score lands on a bound or one either side; it also puts the means' difference on the level-band edges. With both, the level-band plants were refused in single digits: they need a round on a band edge and a message bound at once. Seeding changed after the first full run, and the harness's random stream runs on across a group's functions, so every control from F6 on (all Foretell_Read's and every later function's) was planted again on the final fuzz; the table gives that run. C1..C82 (C2b) and F1..F5 plant in functions fuzzed before Foretell_Read, whose inputs did not move.

The thinnest (fewer than 60 rounds): **F24** (Read: level bound -10 -> -9) 4; **F46** (Show: byte test) 4; **F23** (Read: level band 5 -> 0x59) 7; **F7** (Read: party band > 50 -7) 10; **F18** (Read: percent x 99) 11; **F8** (Read: enemy band > 50 6) 13; **I13** (Right_Run: Sc not re-read) 14; **I73** (Triangles: cos by the local) 15; **F33** (Read: bound 0x32 -> 0x33) 19; **F11** (Read: bounds >= ) 22; **F34** (Read: bound 0x1E -> 0x1F) 24; **F22** (Read: level 0x82 -> 0x81) 25; **F32** (Read: bound 0x46 -> 0x45) 25; **F35** (Read: negative 0x3F) 31; **F31** (Read: bound 0x5A -> 0x5B) 36; **F13** (Read: 0x40 takes 0x17) 39; **C26** (MarkAt: x read before the create) 40; **F25** (Read: diff as int) 40.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| C1 | Chill_Task: entries 2/3 swapped | Chill_Task 810 |
| C2 | Chill_Task: entry 1 Corona_End | a Fatal: magic_harness: ours calls 0x4E75F0, which no stand-in covers: list it in the group's callees |
| C2b | Chill_Task: entry 1 Chill_WaitChildren | Chill_Task 370 |
| C3 | Start: +9 0x19 | Chill_Start 1940 |
| C4 | Start: ray facing from owner +9 | Chill_Start 1948 |
| C5 | Start: flash +1 2 | Chill_Start 1966 |
| C6 | Start: parameter 0x14 (first) | Chill_Start 2000 |
| C7 | Start: row 2 bit 0x4000 | Chill_Start 2000 |
| C8 | Start: row 2 one word short | Chill_Start 2000 |
| C9 | Start: row 26 second half skipped | Chill_Start 2000 |
| C10 | Start: event test inverted | Chill_Start 1231 |
| C11 | Start: 0x2A | Chill_Start 1234 |
| C12 | Start: 0x7C bytes copied | Chill_Start 1230 |
| C13 | Start: copy +1 2 | Chill_Start 1230 |
| C14 | Start: copy +5 0x14 | Chill_Start 1230 |
| C15 | Start: copy +6 2 | Chill_Start 1230 |
| C16 | Start: copy +9 1 | Chill_Start 1230 |
| C17 | WaitChildren: at 1 | Chill_WaitChildren 1025 |
| C18 | WaitChildren: +9 0x1F | Chill_WaitChildren 498 |
| C19 | SpawnMarks: side bit 0x80 | Chill_SpawnMarks 245 |
| C20 | SpawnMarks: seven enemies | Chill_SpawnMarks 245 |
| C21 | SpawnMarks: members not tested | Chill_SpawnMarks 266 |
| C22 | MarkAt: +4 6 | Chill_SpawnMarks 429 |
| C23 | MarkAt: +1 3 | Chill_SpawnMarks 429 |
| C24 | MarkAt: +3 the battle index | Chill_SpawnMarks 235 |
| C25 | MarkAt: z from +0x3C | Chill_SpawnMarks 429 |
| C26 | MarkAt: x read before the create | Chill_SpawnMarks 40 |
| C27 | ChillChild_Task: flash / marker swapped | ChillChild_Task 992 |
| C28 | Ray_Run: Grow / Shrink swapped | ChillRay_Run 1010 |
| C29 | Ray_Run: no pop | ChillRay_Run 1032 |
| C30 | Ray_Run: draws with +0 clear | ChillRay_Run 981 |
| C31 | Ray_Start: no centring | ChillRay_Start 2000 |
| C32 | Ray_Start: offset 0xFFFC1000 | ChillRay_Start 1999 |
| C33 | Ray_Start: z from Kind2X | ChillRay_Start 2000 |
| C34 | Ray_Start: sound 0x101 | ChillRay_Start 2000 |
| C35 | Ray_Start: +0x14 0x2FFE | ChillRay_Start 2000 |
| C36 | Ray_Start: +0xB 0x35 | ChillRay_Start 2000 |
| C37 | Ray_Start: +0xA 0x11 | ChillRay_Start 2000 |
| C38 | Grow: >> 5 | ChillRay_Grow 1025 |
| C39 | Grow: at 0xC2 | ChillRay_Grow 500 |
| C40 | Grow: angle + 2 | ChillRay_Grow 2000 |
| C41 | Shrink: at 0x62 | ChillRay_Shrink 528 |
| C42 | Shrink: height up | ChillRay_Shrink 996 |
| C43 | Shrink: step 3 | ChillRay_Shrink 2000 |
| C44 | PushMatrix: facing sl 9 | ChillRay_PushMatrix 1462 |
| C45 | PushMatrix: height / 4 | ChillRay_PushMatrix 2000 |
| C46 | PushMatrix: x sar 8 | ChillRay_PushMatrix 2000 |
| C47 | PushMatrix: no MulMatrix | ChillRay_PushMatrix 2000 |
| C48 | Ray_Draw: radius 0x201 | ChillRay_Draw 2000 |
| C49 | Ray_Draw: step i + 0x11 | ChillRay_Draw 1955 |
| C50 | Ray_Draw: angle mask 0x1F | ChillRay_Draw 950 |
| C51 | Ray_Draw: first arc 0x581 | ChillRay_Draw 2000 |
| C52 | Ray_Draw: one quad fewer | ChillRay_Draw 1955 |
| C53 | Ray_Draw: step 3 test at 2 | ChillRay_Draw 1627 |
| C54 | Ray_Draw: step 3 / 4 | ChillRay_Draw 784 |
| C55 | Ray_Draw: ramp over 15 | ChillRay_Draw 1248 |
| C56 | Ray_Draw: ramp shade sar 3 | ChillRay_Draw 1745 |
| C57 | Ray_Draw: tail over 9 | ChillRay_Draw 1324 |
| C58 | Ray_Draw: far + 2 | ChillRay_Draw 1803 |
| C59 | Ray_Draw: CLUT 0x1E3 | ChillRay_Draw 1955 |
| C60 | Ray_Draw: tpage y 0x101 | ChillRay_Draw 1955 |
| C61 | Ray_Draw: u base 0x55 | ChillRay_Draw 1955 |
| C62 | Ray_Draw: u x 2 | ChillRay_Draw 1955 |
| C63 | Ray_Draw: frame x 1 | ChillRay_Draw 1955 |
| C64 | Ray_Draw: v + 4 | ChillRay_Draw 1955 |
| C65 | Ray_Draw: +0x50 0xFE | ChillRay_Draw 1955 |
| C66 | Ray_Draw: commit 0x50 | ChillRay_Draw 1955 |
| C67 | Ray_Draw: opening tpage 0xB4 | ChillRay_Draw 2000 |
| C68 | Ray_Draw: GT4 step 0x10 | ChillRay_Draw 1955 |
| C69 | Flash_Run: steps 1/2 swapped | ChillFlash_Run 967 |
| C70 | Flash_Run: draws with +0 clear | ChillFlash_Run 958 |
| C71 | Flash_Draw: facing bit 1 | ChillFlash_Draw 966 |
| C72 | Flash_Draw: bottom 238 | ChillFlash_Draw 2000 |
| C73 | Flash_Draw: word sl 2 | ChillFlash_Draw 1994 |
| C74 | Flash_Draw: +0x34 from SB(2) | ChillFlash_Draw 1994 |
| C75 | Flash_Draw: commit 0x40 | ChillFlash_Draw 2000 |
| C76 | Flash_Draw: opening tpage 0x15 | ChillFlash_Draw 2000 |
| C77 | Flash_Draw: not semi-transparent | ChillFlash_Draw 2000 |
| C78 | Mark_Run: entries swapped | ChillMark_Run 2000 |
| C79 | Enemy_Task: entries 0/1 swapped | ChillEnemy_Task 1333 |
| C80 | Enemy_Task: entry 2 BattleFx_Finish | ChillEnemy_Task 667 |
| C81 | Enemy_Start: animation 2 | ChillEnemy_Start 2000 |
| C82 | Enemy_Start: +9 1 | ChillEnemy_Start 1986 |
| F1 | Foretell_Task: Pause / Show swapped | Foretell_Task 789 |
| F2 | Start: owner's +9 | Foretell_Start 1980 |
| F3 | Start: parameter 0x1D | Foretell_Start 2000 |
| F4 | Start: +0xB 2 | Foretell_Start 2000 |
| F5 | Start: z not copied | Foretell_Start 1756 |
| F6 | Read: wait at 1 | Foretell_Read 1356 |
| F7 | Read: party band > 50 -7 | Foretell_Read 10 |
| F8 | Read: enemy band > 50 6 | Foretell_Read 13 |
| F9 | Read: party zero -0x17 | Foretell_Read 83 |
| F10 | Read: enemy zero 0x17 | Foretell_Read 80 |
| F11 | Read: bounds >=  | Foretell_Read 22 |
| F12 | Read: 0x40 member counted | Foretell_Read 144 |
| F13 | Read: 0x40 takes 0x17 | Foretell_Read 39 |
| F14 | Read: member byte +0x8B | Foretell_Read 233 |
| F15 | Read: member ratio +0x9A | Foretell_Read 275 |
| F16 | Read: enemy ratio divisor +0xAE | Foretell_Read 242 |
| F17 | Read: enemy sum +0x9A | Foretell_Read 331 |
| F18 | Read: percent x 99 | Foretell_Read 11 |
| F19 | Read: step carried reset | equivalent: the ratio is never negative (both words zero-extended), so the carried step is never used; near variant F11 refused |
| F20 | Read: battle byte x 4 | Foretell_Read 318 |
| F21 | Read: battle byte 0x904AB3 | Foretell_Read 432 |
| F22 | Read: level 0x82 -> 0x81 | Foretell_Read 25 |
| F23 | Read: level band 5 -> 0x59 | Foretell_Read 7 |
| F24 | Read: level bound -10 -> -9 | Foretell_Read 4 |
| F25 | Read: diff as int | Foretell_Read 40 |
| F26 | Read: mean unsigned | Foretell_Read 95 |
| F27 | Read: bias at +8 | Foretell_Read 406 |
| F28 | Read: bias unsigned | Foretell_Read 304 |
| F29 | Read: bias by enemies - members | Foretell_Read 340 |
| F30 | Read: clamp 99 | equivalent: the clamped score is only compared with 0x5A afterwards, 100 and 99 both pick 0x3C; near variant F30b refused |
| F30b | Read: clamp 0x5A (near variant of F30) | Foretell_Read 847 |
| F31 | Read: bound 0x5A -> 0x5B | Foretell_Read 36 |
| F32 | Read: bound 0x46 -> 0x45 | Foretell_Read 25 |
| F33 | Read: bound 0x32 -> 0x33 | Foretell_Read 19 |
| F34 | Read: bound 0x1E -> 0x1F | Foretell_Read 24 |
| F35 | Read: negative 0x3F | Foretell_Read 31 |
| F36 | Read: queue 0x3D | Foretell_Read 1353 |
| F37 | Read: flag byte +0xBF < 1 | Foretell_Read 214 |
| F38 | Read: flags 8 / 0x10 swapped | Foretell_Read 551 |
| F39 | Read: flag +0xC4 at most 2 | Foretell_Read 114 |
| F40 | Read: flags for out enemies too | Foretell_Read 994 |
| F41 | Read: +9 0x17 | Foretell_Read 1353 |
| F42 | Read: +0xA 5 | Foretell_Read 1353 |
| F43 | Read: enemies skip 'out' inverted | a Fatal: Foretell_Read: no enemy counted, the original's idiv faults |
| F44 | Pause: +0xA 0x3F | Foretell_Pause 494 |
| F45 | Show: +1 3 | Foretell_Show 1004 |
| F46 | Show: byte test | Foretell_Show 4 |
| F47 | End: done bit 8 | Foretell_End 715 |
| F48 | End: unsigned test | Foretell_End 236 |
| F49 | End: step 7 | Foretell_End 556 |
| F50 | Tiles: layer 2 | Foretell_DrawTiles 2000 |
| F51 | Tiles: x 0xE9 | Foretell_DrawTiles 1998 |
| F52 | Tiles: spacing 11 | Foretell_DrawTiles 1386 |
| F53 | Tiles: five bits | Foretell_DrawTiles 672 |
| F54 | Tiles: colour byte 1 | Foretell_DrawTiles 1385 |
| F55 | Tiles: stride 4 | Foretell_DrawTiles 1360 |
| F56 | Tiles: height 11 | Foretell_DrawTiles 1995 |
| F57 | Tiles: grey 0x61 | Foretell_DrawTiles 610 |
| F58 | Tiles: commit 0x18 | Foretell_DrawTiles 1386 |
| F59 | Tiles: Sc not re-read | Foretell_DrawTiles 187 |
| F60 | Orb_Run: Spin / Count swapped | ForetellOrb_Run 967 |
| F61 | Orb_Start: lift 0x1000000 | ForetellOrb_Start 2000 |
| F62 | Orb_Start: sound 0x101 | ForetellOrb_Start 2000 |
| F63 | Orb_Start: +0xA 4 | ForetellOrb_Start 2000 |
| F64 | Orb_Start: no screen point | ForetellOrb_Start 2000 |
| F65 | Spin: +9 0x10 | ForetellOrb_Spin 432 |
| F66 | Spin: no pop | ForetellOrb_Spin 2000 |
| F67 | Count: back to 0 | ForetellOrb_Count 275 |
| F68 | Count: cell 0xB | ForetellOrb_Count 119 |
| F69 | Count: last sound 0x100 | ForetellOrb_Count 119 |
| F70 | Orb_End: owner not counted down | ForetellOrb_End 444 |
| F71 | Orb_Draw: half 11 | ForetellOrb_Draw 2000 |
| F72 | Orb_Draw: tpage x 0x3C1 | ForetellOrb_Draw 2000 |
| F73 | Orb_Draw: CLUT 0x1E1 | ForetellOrb_Draw 2000 |
| F74 | Orb_Draw: v 0x25 | ForetellOrb_Draw 2000 |
| F75 | Orb_Draw: u width 0xC | ForetellOrb_Draw 2000 |
| F76 | Orb_Draw: commit 0x44 | ForetellOrb_Draw 2000 |
| I1 | Influence_Task: entries swapped | Influence_Task 2000 |
| I2 | Start: y from +0x2E | Influence_Start 2000 |
| I3 | Start: parameter 0x3A | Influence_Start 2000 |
| I4 | Start: +9 n | Influence_Start 2000 |
| I5 | Start: two per side | Influence_Start 2000 |
| I6 | Start: side +1 2 | Influence_Start 2000 |
| I7 | Start: third CLUT block from +0x1A10 | Influence_Start 2000 |
| I8 | Start: sound 0x101 | Influence_Start 2000 |
| I9 | Start: +0xB 1 | Influence_Start 1700 |
| I10 | Child_Task: right / left swapped | InfluenceChild_Task 1352 |
| I11 | Right_Run: Pick / Blink swapped | InfluenceRight_Run 798 |
| I12 | Right_Run: cross past 1 | InfluenceRight_Run 114 |
| I13 | Right_Run: Sc not re-read | InfluenceRight_Run 14 |
| I14 | Right_Run: layer 2 | InfluenceRight_Run 2000 |
| I15 | Right_Run: triangles before ring | InfluenceRight_Run 449 |
| I16 | Right_Launch: x + 0x38 | InfluenceRight_Launch 490 |
| I17 | Right_Launch: slide 0xC | InfluenceRight_Launch 490 |
| I18 | Right_Launch: y - 0x11 | InfluenceRight_Launch 490 |
| I19 | Right_Launch: ring 0x21 | InfluenceRight_Launch 490 |
| I20 | Right_Slide: x right | InfluenceRight_Slide 1516 |
| I21 | Right_Slide: sound 0x104 | InfluenceRight_Slide 237 |
| I22 | Right_Slide: first test at 1 | InfluenceRight_Slide 238 |
| I23 | Pick: at 0x11 | InfluenceRight_Pick 1002 |
| I24 | Pick: +0xB 1 | InfluenceRight_Pick 528 |
| I25 | Blink: up by 3 | InfluenceRight_Blink 1115 |
| I26 | Blink: down by 1 | InfluenceRight_Blink 403 |
| I27 | Blink: owner count 2 | InfluenceRight_Blink 970 |
| I28 | Blink: +9 0xF | InfluenceRight_Blink 965 |
| I29 | Shrink: at 6 | InfluenceRight_Shrink 530 |
| I30 | Left_Run: steps swapped | InfluenceLeft_Run 2000 |
| I31 | Left_Run: draws at step 0 | InfluenceLeft_Run 266 |
| I32 | Left_Launch: x - 0x36 | InfluenceLeft_Launch 484 |
| I33 | Left_Slide: x left | InfluenceLeft_Slide 1516 |
| I34 | Mark_Run: entry 4 BattleFx_FreeTask | InfluenceMark_Run 431 |
| I35 | Mark_Start: member below 2 | InfluenceMark_Start 170 |
| I36 | Mark_Start: tint 1, 0, 0 | InfluenceMark_Start 2000 |
| I37 | Mark_Start: x - 0x21 | InfluenceMark_Start 2000 |
| I38 | Mark_Start: y - 0xD | InfluenceMark_Start 2000 |
| I39 | Mark_Start: lift pair by +8 < 1 | InfluenceMark_Start 155 |
| I40 | Mark_Start: lift by +0x88 | InfluenceMark_Start 433 |
| I41 | Mark_Start: enemy lift stride 0x8B | InfluenceMark_Start 1290 |
| I42 | Mark_Start: lift added | InfluenceMark_Start 1855 |
| I43 | Mark_Start: +9 1 | InfluenceMark_Start 2000 |
| I44 | Mark_Start: tint to +9 | InfluenceMark_Start 1992 |
| I45 | Brighten: two channels | InfluenceMark_Brighten 2000 |
| I46 | Brighten: at 0x11 | InfluenceMark_Brighten 1040 |
| I47 | Fade: stride 11 | InfluenceMark_Fade 1991 |
| I48 | Fade: sound for all | InfluenceMark_Fade 538 |
| I49 | Fade: flash the target | InfluenceMark_Fade 1030 |
| I50 | Fade: +0xA 5 | InfluenceMark_Fade 1037 |
| I51 | Fade: release +0xB | InfluenceMark_Fade 1034 |
| I52 | Show: to 0x1C | InfluenceMark_Show 471 |
| I53 | Show: at 0x15 | InfluenceMark_Show 948 |
| I54 | Mark_Draw: width 0x31 | InfluenceMark_Draw 2000 |
| I55 | Mark_Draw: tpage y 0 | InfluenceMark_Draw 2000 |
| I56 | Mark_Draw: CLUT x 0x10 | InfluenceMark_Draw 2000 |
| I57 | Mark_Draw: u 0x3F | InfluenceMark_Draw 2000 |
| I58 | Mark_Draw: v + 0x1F | InfluenceMark_Draw 2000 |
| I59 | Mark_Draw: shade 0x7F | InfluenceMark_Draw 2000 |
| I60 | shade 0x2F x | Influence_DrawTriangles 1198 |
| I61 | Triangles: radius >> 3 | Influence_DrawTriangles 1968 |
| I62 | Triangles: step 2 test at 3 | Influence_DrawTriangles 562 |
| I63 | Triangles: x 5 | Influence_DrawTriangles 791 |
| I64 | Triangles: 0x81 - 6 x +9 | Influence_DrawTriangles 796 |
| I65 | Triangles: three fans | Influence_DrawTriangles 2000 |
| I66 | Triangles: turn sl 3 | Influence_DrawTriangles 2000 |
| I67 | Triangles: second point j | Influence_DrawTriangles 2000 |
| I68 | Triangles: mask 0x7F | Influence_DrawTriangles 1995 |
| I69 | Triangles: blue 2 | Influence_DrawTriangles 2000 |
| I70 | Triangles: commit 0x30 | Influence_DrawTriangles 2000 |
| I71 | Triangles: y from +0x2E | Influence_DrawTriangles 2000 |
| I72 | Triangles: points at +1 | Influence_DrawTriangles 2000 |
| I73 | Triangles: cos by the local | Influence_DrawTriangles 15 |
| I74 | Ring: segments bit 2 only | Influence_DrawRing 2000 |
| I75 | Ring: shade 0x7F | Influence_DrawRing 818 |
| I76 | Ring: end angle i + 2 | Influence_DrawRing 2000 |
| I77 | Ring: commit 0x1C | Influence_DrawRing 2000 |
| I78 | Ring: 63 segments | equivalent: index 63 has (i & 0xC) = 0xC and is never drawn; near variant I78b refused |
| I78b | Ring: 59 segments (near variant of I78) | Influence_DrawRing 2000 |
| I79 | Cross: arm 5 | Influence_DrawCross 2000 |
| I80 | Cross: vertical arm reversed | Influence_DrawCross 2000 |
| I81 | Cross: shade 0x7F | Influence_DrawCross 2000 |
| I82 | SpawnMarks: actor not excluded | Influence_SpawnMarks 167 |
| I83 | SpawnMarks: target check on members dropped | Influence_SpawnMarks 69 |
| I84 | SpawnMarks: enemy word at most 2 | Influence_SpawnMarks 396 |
| I85 | SpawnMarks: enemy bit 0x10 | Influence_SpawnMarks 903 |
| I86 | SpawnMarks: enemy bit 0x8000 | Influence_SpawnMarks 925 |
| I87 | SpawnMarks: member bit 0 dropped | Influence_SpawnMarks 91 |
| I88 | SpawnMarks: member word +0xAC | Influence_SpawnMarks 233 |
| I89 | SpawnMarks: +4 not counted | Influence_SpawnMarks 1259 |
| I90 | SpawnMarks: +0x80 Sprite_Current | Influence_SpawnMarks 1631 |
| I91 | SpawnMarks: +1 1 | Influence_SpawnMarks 1807 |
| I92 | SpawnMarks: seven enemies | Influence_SpawnMarks 2000 |
| I93 | SpawnMarks: +0xB the enemy number | Influence_SpawnMarks 965 |

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

Numbered D89, D90, D91, D96, D97 and D106 in [`known-defects.md`](known-defects.md).

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
