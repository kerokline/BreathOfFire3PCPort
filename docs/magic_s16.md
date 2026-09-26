# Four sparkle overlays: MAGIC071..074

**Status:** IN PROGRESS (2026-09-26) - sixty functions ours
(`src/game/magic_s16.cpp`, shadow name `magic_s16`), fuzzed headless
through the shared harness ([`magic_harness.md`](magic_harness.md), group
HX's consolidated one): 0 mismatches in 120,000 rounds, and 0 in 10,000
rounds of the answers check (section 4); 72 of 72 negative controls refused
by a count (exit 3). Fuzz only: no recorded route casts any of them
(section 8).

Group S16 of round nine's first spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4).

## 1. The four overlays, and what they are

| Overlay | `Magic_Rows` row | Loaded by ids (one id down: the name, a hypothesis) | Extent | Functions |
|---|--:|---|---|--:|
| MAGIC071 | 45 | `0x21` Healing Herb, `0x47` / `0xAF` Rejuvenate | `0x4B9930..0x4BA45E` | 12 |
| MAGIC072 | 113 | `0x48` / `0xB0` Restore | `0x4BA460..0x4BAF8E` | 12 |
| MAGIC073 | 47 | `0x49` / `0xB1` Vitalize | `0x4BAF90..0x4BBD57` | 18 |
| MAGIC074 | 114 | `0x4A` / `0xB2` Vigor | `0x4BBD60..0x4BCBEE` | 18 |

Extents from `tools/magic_rows.py --unit MAGIC07N` (capstone recursive
descent; every jump internal, no jump table, nothing `REFUSED`). None was
ours; none was found inside or missing from the extents; 60 functions,
12,517 bytes, as the queue counted. The names are the sibling's labels read
one id down ([`cut-content.md`](cut-content.md) §2), which breaks at least
once in the table: nothing here confirms them, and what these spells are in
play is the owner's to say.

**What they are, by reading.** Every one of the four holds a copy of the
**Healing Herb's sparkle code** - MAGIC070's (`Magic_Rows` row 44), taken by
round seven (`battle_items.cpp`) and round eight (`magic_fx_reached.cpp`).
A capstone compare, function by function against MAGIC070's (2026-09-25,
the scratch `x86dis.py`), finds the copies **instruction for instruction the
same**, relative branches normalised, but for the absolute addresses of each
overlay's own pool, current-sparkle cell and `.data` tables, and:

- MAGIC071's and 072's task and spawn are MAGIC070's too, with one byte
  changed: the spawn stores the effect's **kind** `+4` as 1 (071) and 2
  (072) where MAGIC070's stores 0 - and the kind picks the sparkle count,
  the launch delay, the rise length and the colour rows;
- MAGIC073's and 074's pools hold 160 records (`cmp al, 0xA0` in their
  allocs) where the others hold 128;
- MAGIC073 and 074 replace the task and spawn with a parent and a **child
  task per actor** (section 3).

So by reading, the four are the Healing Herb's glow over the caster (071,
072), or over each living actor of one side (073, 074), in other colours and
counts. That the pairs are single- and all-target versions of one heal is a
guess from their shape, not a measurement.

## 2. The sparkle code, forty functions

Written once in `magic_s16.cpp` over a table of the four overlays
(`kPools`: pool, count, current cell, the offset / colour / life / count /
delay / type tables, the update's three phase addresses and our draw, alloc
and free); each overlay's ten are named wrappers. MAGIC070's names are the
model:

| Copy of (MAGIC070) | 071 | 072 | 073 | 074 | Bytes | Does |
|---|---|---|---|---|--:|---|
| `Sparkle_Dispatch` | `0x4B9B10` | `0x4BA640` | `0x4BB390` | `0x4BC2A0` | 0x12 | `jmp [types + 4 * +1]` of the current sparkle, in place, unchecked |
| `Sparkle_Update` | `0x4B9B30` | `0x4BA660` | `0x4BB3B0` | `0x4BC2C0` | 0x8F | phase `+2` through a three-entry stack table; the disc; the rays |
| `Sparkle_Launch` | `0x4B9BC0` | `0x4BA6F0` | `0x4BB440` | `0x4BC350` | 0x161 | the delay; the owner's position; thrown by its offset row; its sway and rise |
| `Sparkle_Rise` | `0x4B9D30` | `0x4BA860` | `0x4BB5B0` | `0x4BC4C0` | 0x6C | the sway; the count up to the rise length |
| `Sparkle_Fade` | `0x4B9DA0` | `0x4BA8D0` | `0x4BB620` | `0x4BC530` | 0x85 | the sway; dimmer; at 0 the owner's count down and the free (tail jmp) |
| `Sparkle_DrawRaysG2` | `0x4B9E30` | `0x4BA960` | `0x4BB6B0` | `0x4BC5C0` | 0x185 | four gouraud lines |
| `Sparkle_DrawRaysG3` | `0x4B9FC0` | `0x4BAAF0` | `0x4BB840` | `0x4BC750` | 0x1ED | four three-point lines |
| `Sparkle_DrawDisc` | `0x4BA1B0` | `0x4BACE0` | `0x4BBA30` | `0x4BC940` | 0x22F | eight gouraud triangles |
| `Sparkle_Alloc` | `0x4BA3E0` | `0x4BAF10` | `0x4BBC60` | `0x4BCB70` | 0x4A | the first free record, its index in al; 0xFF when full |
| `Sparkle_Free` | `0x4BA430` | `0x4BAF60` | `0x4BBCB0` | `0x4BCBC0` | 0x2F | `+0..+4` of the current sparkle cleared |

Named `Magic07N_Sparkle*` (`symbols.toml`, each entry citing the compare
and its own addresses). The four pools (`0x2C`-byte records) and cells:

| | Pool | Records | Current cell | Offsets | Colours | Life | Counts | Delays | Types |
|---|---|--:|---|---|---|---|---|---|---|
| 071 | `0x685D98` | 128 | `0x687398` | `0x65AE2C` | `0x65AECC` | `0x65AF18` | `0x65AF08` | `0x65AF10` | `0x65AF20` |
| 072 | `0x6873A0` | 128 | `0x6889A0` | `0x65AF24` | `0x65AFC4` | `0x65B010` | `0x65B000` | `0x65B008` | `0x65B018` |
| 073 | `0x6889A8` | 160 | `0x68A528` | `0x65B01C` | `0x65B0BC` | `0x65B0DC` | `0x65B0D4` | `0x65B0D8` | `0x65B0E4` |
| 074 | `0x68A530` | 160 | `0x68C0B0` | `0x65B0E8` | `0x65B188` | `0x65B1A8` | `0x65B1A0` | `0x65B1A4` | `0x65B1B0` |

The pools and cells are back to back in `.bss`, each cell right after its
pool. Every reading of round seven's (`battle_items.md` §1) and eight's
(`magic_fx_reached.md` §4) holds for each copy: the sparkle re-read after
every call, the rays' arguments pushed with stale upper halves (the draws
read the low words), the unchecked phase and type.

## 3. The tasks

**MAGIC071 / 072** (rows 45 / 113). `Magic07N_Task` (`0x4B9930` / `0x4BA460`,
0x81): the phase `+1` through a six-entry stack table, then the pool walk
(every record with bit 0 becomes the current sparkle, its `+0x28` the owner
`0x93B940` for the dispatch, the owner put back after each). The table:
its own spawn, then five bodies shared with MAGIC070 - `BattleFx_TintActor`,
`BattleFx_Brighten`, `BattleFx_WaitStep4` (round eight's, `battle_odds.cpp`),
MAGIC070's `Sparkle_End`, `BattleFx_Finish`. `Magic07N_Spawn` (`0x4B99C0` /
`0x4BA4F0`, 0x14E): the pool cleared, the kind stored, the source sprite's
position, `Sparkle_CountByKind`'s copy of sparkles made, sound
`Sound_PlayEffect(0x100)`.

**MAGIC073 / 074** (rows 47 / 114): a parent and children.

| Function | Entry | Bytes | Reached as | Does |
|---|---|--:|---|---|
| `Magic073_Task` | `0x4BAF90` | 0x61 | row 47 | phase `+1`: spawn, wait; then the pool walk |
| `Magic073_Spawn` | `0x4BB000` | 0x162 | its entry 0 | pool cleared, kind 0; a child (kind 1, parameter `0x26`) per actor not out |
| `Magic073_Wait` | `0x4BB170` | 0x22 | entry 1 | children gone and nobody reacting: the done flag, free |
| `Magic073_ActorDispatch` | `0x4BB1A0` | 0x12 | kind 1, parameter `0x26` | `jmp [0x65B0E0 + 4 * +1]` |
| `Magic073_ActorTask` | `0x4BB1C0` | 0x46 | `0x65B0E0[0]` | the child's phase `+2` through six entries |
| `Magic073_ActorStart` | `0x4BB210` | 0x15F | its entry 0 | the delay; the actor's position; the sparkles |
| `ActorFx_WaitStep4` | `0x4BB370` | 0x18 | entry 3 (and MAGIC168's, 169's) | four or fewer sparkles left: `+9` 8, phase on |
| `Magic073_CountReacting` | `0x4BBCE0` | 0x78 | `Magic073_Wait`'s call | actors in state 6 and not out (section 6) |
| `Magic074_Task` | `0x4BBD60` | 0x61 | row 114 | phase `+1`: spawn, `0x4E5200`; then the pool walk |
| `Magic074_Spawn` | `0x4BBDD0` | 0x162 | its entry 0 | as 073's: kind 1, parameter `0x24` |
| `Magic074_ActorDispatch` | `0x4BBF40` | 0x12 | kind 1, parameter `0x24` | `jmp [0x65B1AC + 4 * +1]` |
| `Magic074_ActorTask` | `0x4BBF60` | 0x46 | `0x65B1AC[0]` | as 073's, its own start |
| `Magic074_ActorStart` | `0x4BBFB0` | 0x15F | its entry 0 | as 073's on 074's pool |
| `ActorFx_Tint` | `0x4BC110` | 0x82 | entry 1 of both child tables | the delay; the actor's tints released and a tint (0, 0, 0, 1) set, kept in `+0xA` |
| `ActorFx_Untint` | `0x4BC1A0` | 0xBA | entry 4 | the tint record `+0xA`'s colour down; at 0 released, the actor flashed |
| `ActorFx_End` | `0x4BC260` | 0x31 | entry 5 | sparkles gone: the parent's count down, the actor's flag `0x40`, free |

The spawn goes over the enemies 3..10 when the target byte `0x904B44` has
bit `0x40`, else the party 0..2, and puts a child on each actor
`Battle_ActorIsOut` does not answer for: the child's owner is the parent,
its `+4` the actor, its `+9` a delay of 1 and then 28 more for each child
(so they start one after another). The child's table (by `+2`): its start
(the delay out, the actor's record `+8` and position taken, the actor's
screen point, the sparkles - their kind the **parent's** `+4`, read through
the owner cell), `ActorFx_Tint`, `0x4F4DA0` (MAGIC213's: the tint
brightened, group C1's), `ActorFx_WaitStep4`, `ActorFx_Untint`,
`ActorFx_End`. The sparkles' owner is the child, so their fade counts the
child's `+0xB` down; the child's end counts the parent's. MAGIC074's parent
ends through `0x4E5200` (MAGIC131's wait-and-end, group S30's: children
gone, the done flag, free); MAGIC073's through its own wait, which also
waits for `Magic073_CountReacting` to be 0.

`ActorFx_Tint` and `ActorFx_Untint` are `BattleFx_TintActor` and
`Sparkle_End` done on the child's actor record instead of the source
sprite. The `ActorFx_` names are for the shared bodies; the ones only one
overlay reaches carry its number.

The child dispatches read their `.data` tables in place, unchecked
(`Magic073_ActorPhases` `0x65B0E0` and `Magic074_ActorPhases` `0x65B1AC`,
named): `+1` is 0 as the spawn sets it and nothing steps it. Each table's
second entry is the sparkle-type table's first - `0x65B0E4` and `0x65B1B0`
begin one word in - and 074's runs on into MAGIC075's code pointers. So
`tools/magic_rows.py` counts "10 code entries" at `0x65B1AC`: two are this
table's.

## 4. The fuzz

`BOF3X_SHADOW=magic_s16`, `magic_s16_fuzz.cpp`, one `magic_harness::Run`:

- **the clones**: sixty, the clone tables as `magic_rows.py --clones`
  printed them (the task's and child's stack-table immediates re-aimed at
  handler recorders, every `E8` / `E9` at its callee's);
- **callees the standard set lacks**: the draws' `Gpu_SetDrawMode`,
  `MapView_LinkPrimAt`, `Gpu_SetLineG2`, `Gpu_SetLineG3`, `Gpu_SetPolyG3`,
  `Gpu_SetSemiTrans`, `Math_Sin`, `Math_Cos`; `Battle_ActorIsOut` (low byte,
  a flag), `Sprite_SetTint`; and ours called by the clones' `E8` / `E9` -
  each overlay's dispatch, disc, rays (their low words), alloc (a byte
  0..255: 0xFF a full pool, 128..254 past the pool), free, and
  `Magic073_CountReacting`. `Gpu_SetDrawMode`'s fifth argument and
  `Sprite_SetTint`'s (both constant) are past the recorder's four;
- **the `.data` tables**, swapped for recorders: `0x65AF20`, `0x65B018`
  (one entry each), `0x65B0E0`, `0x65B1AC` (two; the sparkle tables inside);
- **regions** (46,068 bytes with the standard ones): the four pools and
  cells and as far past 074's as an alloc's 254 reaches
  (`0x685D98..0x68D130`); the overlays' `.data` less the four table cells;
  `MoveScript_TintRecords` whole (an unbounded `+0xA` index); `Gfx_PacketNext`
  and a buffer of the fuzz's own it points into; the scratch words
  `0x903850..0x90385F`; the three records past the enemies
  (`0x93C2A0..0x93C617`) that `Magic073_CountReacting` reaches;
- **the seed** by role: every round the packet pointer into the buffer, every
  pool record's owner `+0x28` a real slot or record (the walk makes it the
  owner, which a recorder writes through), each current cell a record of its
  pool with type 0, the exe's `.data` two rounds in three (read at start-up,
  not written in the source); then the task phases 0..5 or 0..1, the child
  phase 0..5, the side bit `0x40` half the time, delays at 1, `+0xB` at 0 or
  around 4, the actors 0..10, state 6 on half the actors
  `Magic073_CountReacting` reads, the update's phase 0..2, a launch or fade
  at its last frame, a rise at its last count, a pool full or filled to a
  point for the allocs;
- **the disturbance** of the group's cells: a current cell moved to another
  record, the packet pointer moved, a scratch word, a byte of a current
  sparkle below its owner.

**The answers.** The harness compares memory and the recorders' log, not
eax, and five of the sixty answer in al: the four allocs and
`Magic073_CountReacting`. After the run, each is cloned plainly (the
count's two calls kept on `Battle_ActorIsOut`, ours, which only reads) and
run against ours for 2,000 rounds from the same random pool, party and
enemy bytes: al and the pool compared.

**Result** (2026-09-26, after the merge; this worktree):

    shadow      magic_s16 self-test: 120000 rounds over 60 functions (2000 each), 2016703 calls to the stand-ins,
                0 MISMATCHES; 46068 bytes of state (18 regions) and the stand-ins' log compared
    shadow      magic_s16 answers: 10000 rounds over 5 functions (the allocs' al and pool, the count's al),
                1977 full pools, 1365 counts not 0, 0 MISMATCHES

Coverage (the originals' calls): every callee and every handler the clones
name is reached - e.g. `Battle_ActorIsOut` 44,175, `BattleTask_Create`
7,518, `Sprite_SetTint` 992, each overlay's alloc 31,715 .. 74,300 and free
about 950. The counts vary a little run to run: pointer values of the DLL's
own (the harness's records, the buffer) land in bytes a later round reads.
`BOF3X_SHADOW='*'`: exit 0.

**The harness, after the merge.** This group's seed (the side bit `0x40` on the target byte) exposed two crashes in the target-enemy disturbance (cases 12 and 13): a target of 11 or more pointed `EnemyOf` past the image, and a target of 0..2 reached the current-slot and owner cells below the enemy records, which a later disturbance wrote through. Group HX's consolidated harness (`ea27991`, [`magic_harness.md`](magic_harness.md) §7) now disturbs the target record only for targets 3..10; this group's fuzz file needed no change. The run above is on that harness, in this worktree (2026-09-26); `BOF3X_SHADOW='*'` exit 0. The answers check (the five al answers) is the one feature built in the group's own fuzz file, as the harness compares memory and the log only.

## 5. The controls

Seventy-two, planted one at a time by a script (the scratch `controls.py`, not committed: replace, build, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_s16`, restore), re-run on the ported fuzz after the merge of the consolidated harness (`ea27991`), 2026-09-26. **72 of 72 refused** by a count (exit 3), each in the function or functions its plant touches; counts are this worktree's.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| T1 | Magic071_Task: table entries 0 and 1 swapped | Magic071_Task 636 |
| T2 | Walk: the owner not put back | Magic071_Task 1,603, Magic072_Task 1,597, Magic073_Task 1,665, Magic074_Task 1,603 |
| T3 | Walk: records with bit 1, not bit 0 | Magic071_Task 1,999, Magic072_Task 1,995, Magic073_Task 1,999, Magic074_Task 1,999 |
| T4 | Walk: 074's pool walked to 0x9F | Magic074_Task 1,006 |
| T5 | Magic073_Task: its two entries swapped | Magic073_Task 2,000 |
| T6 | Walk: the owner saved before the phase (072) | Magic072_Task 76 |
| S1 | Magic071_Spawn: kind 0 (MAGIC070's) | Magic071_Spawn 1,956 |
| S2 | Spawn: +8 from the source's +9 | Magic071_Spawn 1,565, Magic072_Spawn 1,565 |
| S3 | Spawn: +9 = 7 | Magic071_Spawn 1,589, Magic072_Spawn 1,612 |
| S4 | MakeSparkles: Sprite_Current read before the alloc | Magic071_Spawn 1,206, Magic072_Spawn 1,199, Magic073_ActorStart 447, Magic074_ActorStart 396 |
| S5 | MakeSparkles: delay by n >> 1 | Magic071_Spawn 1,871, Magic072_Spawn 1,853, Magic073_ActorStart 845, Magic074_ActorStart 788 |
| S6 | MakeSparkles: shade Rand & 7 | Magic071_Spawn 1,939, Magic072_Spawn 1,929, Magic073_ActorStart 880, Magic074_ActorStart 838 |
| S7 | MakeSparkles: the bound read once | Magic071_Spawn 1,880, Magic072_Spawn 1,856, Magic073_ActorStart 702, Magic074_ActorStart 678 |
| S8 | Spawn: sound 0x101 | Magic071_Spawn 2,000, Magic072_Spawn 2,000 |
| S9 | ClearPool: +2 kept | Magic071_Spawn 2,000, Magic072_Spawn 2,000, Magic073_Spawn 2,000, Magic074_Spawn 2,000 |
| S10 | MakeSparkles: a full pool not skipped (0xFF taken as a record) | Magic071_Spawn 435, Magic072_Spawn 437, Magic073_ActorStart 196, Magic074_ActorStart 135 |
| A1 | SpawnOnActors: delay step 0x1D | Magic073_Spawn 1,116, Magic074_Spawn 1,057 |
| A2 | SpawnOnActors: enemies 3..9 | Magic073_Spawn 1,037, Magic074_Spawn 998 |
| A3 | Magic073_Spawn: parameter 0x27 | Magic073_Spawn 1,705 |
| A4 | SpawnOnActors: the child's +1 = 1 | Magic073_Spawn 1,705, Magic074_Spawn 1,695 |
| A5 | SpawnOnActors: the child not counted | Magic073_Spawn 1,679, Magic074_Spawn 1,673 |
| A6 | SpawnOnActors: the side bit 0x80 | Magic073_Spawn 1,037, Magic074_Spawn 998 |
| A7 | SpawnOnActors: Sprite_Current read before the create | Magic073_Spawn 112, Magic074_Spawn 105 |
| A8 | Magic074_Spawn: kind 0 | Magic074_Spawn 1,856 |
| W1 | Magic073_Wait: flag 2 | Magic073_Wait 247 |
| W2 | Magic073_Wait: the count not asked | Magic073_Wait 1,013 |
| C1 | CountReacting: the members' state 5 | answers check: Magic073_CountReacting (20 of 10,000) |
| C2 | CountReacting: the enemies' own state bytes (0x93B961) | answers check: Magic073_CountReacting (1076 of 10,000) |
| C3 | CountReacting: members 0..1 | Magic073_CountReacting 2,000 |
| D1 | Magic073_ActorDispatch: index +1 ^ 1 | Magic073_ActorDispatch 2,000 |
| D2 | Magic074_ActorTask: by +1, not +2 | Magic074_ActorTask 1,685 |
| D3 | Magic074_ActorTask: entry 0 073's start | Magic074_ActorTask 308 |
| AS1 | ActorStart: the kind the child's own +4 | Magic073_ActorStart 931, Magic074_ActorStart 943 |
| AS2 | ActorRecord: a member below 4 | Magic073_ActorStart 61, Magic074_ActorStart 77, ActorFx_Tint 91, ActorFx_Untint 97 |
| AS3 | ActorStart: +0xA = 1 | Magic073_ActorStart 834, Magic074_ActorStart 878 |
| AS4 | MakeSparkles: the kind not read again after Rand | Magic071_Spawn 1,931, Magic072_Spawn 1,911, Magic073_ActorStart 752, Magic074_ActorStart 699 |
| WS1 | ActorFx_WaitStep4: above 5 | ActorFx_WaitStep4 326 |
| TI1 | ActorFx_Tint: red 1 | ActorFx_Tint 992 |
| TI2 | ActorFx_Tint: +0xA not kept | ActorFx_Tint 987 |
| TI3 | ActorFx_Tint: the release skipped | ActorFx_Tint 992 |
| UT1 | ActorFx_Untint: tint bytes +1..+3 | ActorFx_Untint 2,000 |
| UT2 | ActorFx_Untint: the next actor flashed | ActorFx_Untint 1,015 |
| UT3 | ActorFx_Untint: Sprite_Current not read again for +2 | ActorFx_Untint 50 |
| E1 | ActorFx_End: the parent's count kept | ActorFx_End 998 |
| E2 | ActorFx_End: flag on +5 | ActorFx_End 1,007 |
| SD1 | Dispatch: 071 through 072's table | Magic071_SparkleDispatch 2,000 |
| UP1 | Update: launch and rise swapped | Magic071_SparkleUpdate 1,345, Magic072_SparkleUpdate 1,370, Magic073_SparkleUpdate 1,329, Magic074_SparkleUpdate 1,322 |
| UP2 | Update: the disc without the launched test | Magic071_SparkleUpdate 336, Magic072_SparkleUpdate 326, Magic073_SparkleUpdate 316, Magic074_SparkleUpdate 326 |
| UP3 | Update: G3 radius + 1 | Magic071_SparkleUpdate 495, Magic072_SparkleUpdate 489, Magic073_SparkleUpdate 511, Magic074_SparkleUpdate 522 |
| UP4 | Update: G2 at Frame_Counter, not / 2 | Magic071_SparkleUpdate 495, Magic072_SparkleUpdate 489, Magic073_SparkleUpdate 511, Magic074_SparkleUpdate 522 |
| UP5 | Update: the sparkle not read again before G3 | Magic071_SparkleUpdate 2, Magic072_SparkleUpdate 1, Magic073_SparkleUpdate 1 |
| L1 | Launch: offset row & 0xF | Magic071_SparkleLaunch 252, Magic072_SparkleLaunch 247, Magic073_SparkleLaunch 251, Magic074_SparkleLaunch 243 |
| L2 | Launch: y thrown down | Magic071_SparkleLaunch 1,017, Magic072_SparkleLaunch 1,011, Magic073_SparkleLaunch 1,020, Magic074_SparkleLaunch 966 |
| L3 | Launch: life by shade | Magic071_SparkleLaunch 914, Magic072_SparkleLaunch 876, Magic073_SparkleLaunch 917, Magic074_SparkleLaunch 769 |
| L4 | Launch: brightness 0x11 | Magic071_SparkleLaunch 1,017, Magic072_SparkleLaunch 1,011, Magic073_SparkleLaunch 1,020, Magic074_SparkleLaunch 966 |
| L5 | Launch: the sparkle not read again after the x Rand | Magic072_SparkleLaunch 1 |
| R1 | Rise: at count + 1 | Magic071_SparkleRise 1,030, Magic072_SparkleRise 997, Magic073_SparkleRise 1,026, Magic074_SparkleRise 1,046 |
| R2 | Sway: 25 wide | Magic071_SparkleRise 2,000, Magic071_SparkleFade 2,000, Magic072_SparkleRise 2,000, Magic072_SparkleFade 2,000, Magic073_SparkleRise 2,000, Magic073_SparkleFade 2,000, Magic074_SparkleRise 2,000, Magic074_SparkleFade 2,000 |
| F1 | Fade: Frame_Counter & 7 | Magic071_SparkleFade 221, Magic072_SparkleFade 215, Magic073_SparkleFade 246, Magic074_SparkleFade 243 |
| F2 | Fade: the owner's count kept | Magic071_SparkleFade 936, Magic072_SparkleFade 978, Magic073_SparkleFade 982, Magic074_SparkleFade 962 |
| G1 | RaysG2: shade * 5 | Magic071_SparkleRaysG2 1,937, Magic071_SparkleRaysG3 1,925, Magic072_SparkleRaysG2 1,945, Magic072_SparkleRaysG3 1,932, Magic073_SparkleRaysG2 1,944, Magic073_SparkleRaysG3 1,934, Magic074_SparkleRaysG2 1,942, Magic074_SparkleRaysG3 1,926 |
| G2 | RaysG2: eight rays | Magic071_SparkleRaysG2 2,000, Magic072_SparkleRaysG2 2,000, Magic073_SparkleRaysG2 2,000, Magic074_SparkleRaysG2 2,000 |
| G3 | RaysG3: half a quarter | Magic071_SparkleRaysG3 2,000, Magic072_SparkleRaysG3 2,000, Magic073_SparkleRaysG3 2,000, Magic074_SparkleRaysG3 2,000 |
| G4 | RaysG3: the middle point's colour 2 | Magic071_SparkleRaysG3 2,000, Magic072_SparkleRaysG3 2,000, Magic073_SparkleRaysG3 2,000, Magic074_SparkleRaysG3 2,000 |
| G5 | RaysG2: the scratch angle not read again after Cos | Magic071_SparkleRaysG2 11, Magic072_SparkleRaysG2 11, Magic073_SparkleRaysG2 7, Magic074_SparkleRaysG2 15 |
| G6 | RaysG2: the packet pointer read once | Magic071_SparkleRaysG2 156, Magic072_SparkleRaysG2 165, Magic073_SparkleRaysG2 149, Magic074_SparkleRaysG2 150 |
| DI1 | Disc: radius Rand & 7 | Magic071_SparkleDisc 961, Magic072_SparkleDisc 950, Magic073_SparkleDisc 952, Magic074_SparkleDisc 917 |
| DI2 | Disc: colour row + 1 | Magic071_SparkleDisc 1,947, Magic072_SparkleDisc 1,949, Magic073_SparkleDisc 1,964, Magic074_SparkleDisc 1,964 |
| DI3 | Disc: sixteen triangles | Magic071_SparkleDisc 2,000, Magic072_SparkleDisc 2,000, Magic073_SparkleDisc 2,000, Magic074_SparkleDisc 2,000 |
| AL1 | Alloc: bit 1 marked | Magic071_SparkleAlloc 733, Magic072_SparkleAlloc 756, Magic073_SparkleAlloc 759, Magic074_SparkleAlloc 732 |
| AL2 | Alloc: one record short | Magic071_SparkleAlloc 4, Magic072_SparkleAlloc 2, Magic073_SparkleAlloc 7, Magic074_SparkleAlloc 3 |
| FR1 | Free: +4 kept | Magic071_SparkleFree 1,991, Magic072_SparkleFree 1,990, Magic073_SparkleFree 1,991, Magic074_SparkleFree 1,994 |

The thinnest are the re-reads: UP5 (the sparkle read again before the G3 rays; 1..2 rounds per overlay, none in 074's run), L5 (after the x `Rand`; 1 round, in 072) and AL2 (the alloc's last record; 2..7). Each shows only when the group's disturbance moves the current cell across exactly that call, or the pool fills to its last record. C1 and C2 are refused only by the answers check (the count answers in al, which the harness does not compare): 20 and 1,076 of its 10,000 rounds.

## 6. Defects (Capcom's, latent, kept)

For the coordinator to number; nothing is fixed here.

- **`Magic073_CountReacting` reads the wrong enemies' state.** It asks
  `Battle_ActorIsOut(i + 3)` for i 0..7 and then reads the state byte at
  `0x93BCD9 + 0x128 i` - `0x93B961` (enemy 0's state byte `+1`, as the queue's §3 reads Head
  Cracker's wait) plus three records. So enemy i's liveness gates enemy i + 3's reaction
  state; for i 5..7 the byte is past the eight enemy records, in
  `0x93C2B1..` (the message queue's neighbourhood, `0x93C2A0`). By reading:
  MAGIC073's end waits on the wrong enemies' reactions, and with six or
  more enemies on bytes that are not an enemy's; whether that ever holds
  the effect up in play is not measured (a watch on `0x904AA8` bit 2 with
  a live cast would say). The party half is right. Control C2 (the fix)
  is refused by the fuzz, so the fuzz sees the index; ours keeps the
  original's. The PSX's was not compared.
- **The stack tables are unbounded** (the task's `+1` - 6 or 2 entries -,
  the child's `+2`, the update's `+2`): as every copy of this code (known
  defects D59's kind). Ours aborts; nothing steps them past.
- **The `.data` dispatches are unbounded**, and overlap: a child `+1` of 1
  would run the sparkle update on the child, 2 on (074) MAGIC075's code.
  Nothing sets it.
- **The tint index `+0xA` is unbounded** (`ActorFx_Untint`): up to 255
  records of 12 bytes past `MoveScript_TintRecords`' 32 - as MAGIC070's
  `Sparkle_End`.
- **An alloc answering 128..254 cannot happen** (the allocs answer below the
  pool or 0xFF), but the spawns would write that far (into the next pool
  and the current cell): no defect, a reason the fuzz's region runs on.

## 7. For `analysis/calltrace/entries_logic.txt`

39 lines appended to the main checkout's copy under a `group S16` comment:
the 12 draws were already listed with their right extents; 9 starts are
listed as hosts whose extents run on over their neighbours (`0x4B9B10`,
`0x4BA640`, `0x4BB390`, `0x4BC2A0` at 315 where each is 0x12;
`0x4BA3E0`, `0x4BAF10`, `0x4BBC60`, `0x4BCB70` - the last at 709, into
MAGIC075 - where each is 0x4A; `0x4BBCE0` at 5B1 where it is 0x78). The
second comment line says so; the host lines were not edited.

## 8. What reaches it

Nothing recorded. The combat route's traces enter no function of these
four overlays (the queue's §5). The live check is the owner casting
Healing Herb / Rejuvenate, Restore, Vitalize and Vigor (by their labels),
with a save that has them or DIV-0045's cheat; the thing to see is the
sparkles rising over the caster (071, 072) or over each living party
member in turn (073, 074), coloured and counted as before, and for 073 the
battle going on after the glow. MAGIC070's copy of the same code is what
the combat route does reach (`Sparkle_Dispatch`, 2,554 calls in its trace,
`symbols.toml`).

No divergence and no ledger entry: every function is a faithful
replacement, except that a phase past a stack table aborts, as every stack
dispatcher the project has taken does.
