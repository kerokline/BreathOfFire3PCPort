# Group BE6: the transformation, the gene cost, three effect tasks, the stat rebuild, BMAGIC's four cells

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md)
section 3), wave one, stage B. All 39 functions of the group are ours
(`src/game/battle_e6.cpp`, shadow name `battle_e6`), each read to its last
instruction with capstone and fuzzed through the boss harness's engine
frame ([`boss_harness.md`](boss_harness.md) section 10) without edits to it:
five `Run`s, 234,000 rounds, 0 mismatches. 66 controls planted: 65 refused by a count, one a near-equivalent recorded with its near variant refused (section 8). The owner's
`dragonTransform` recipe enters 15 of the 39 (section 9); the rest are fuzz
only.

What a gene or a form is in the game is not stated here: the names say what
the code does with bytes (the chosen genes at `0x904B84`, the form code
`0x904B89`), from the code, the callers' docs and the owner's recipe (the
Dragon command, a gene chosen, the transformation). Every name is a
hypothesis; the PSX twins `analysis/pairs_propagated.json` proposes are
cited in `symbols.toml`, none read.

| Unit (`Run`) | What | Functions | Extent |
|---|---|--:|---|
| `form` | the transformation and the genes | 14 | `0x451480..0x4525E7` |
| `stats` | the stat rebuild, the member roll and its two tests, the AP pop-up | 6 | `0x453300..0x453F95` |
| `tasks` | `BattleFx_Dispatch` slots 15, 16 and 18 and their states | 13 | `0x452680..0x452BE6` |
| `slots` | the `Field_Slots` release and start (round eleven's two debts) | 2 | `0x454A80..0x4552F5` |
| `cells` | BMAGIC's `MapCell_Handlers` 40..43 | 4 | `0x4CEB40..0x4CF4A3` |

## 1. The cut, read against the code

`analysis/round12_cut.tsv` lists 40 starts for BE6; `tools/band_rows.py
--group BE6` (group RT's tool) reads 10,130 bytes of them and flags none
"code no list has": **no function in the band that the cut left out.**

- **`0x452460` is not a function** (the cut's 265 bytes, hidden in
  `0x4523C0`): it is the case body for the byte 5 (index 4) of
  `DragonForm_PartyRecipe`'s eight-entry jump table at `0x452544`, reached
  only through that table (no address reference). It is taken with its host
  - no `[[func]]`, no inject line - and its 0x452460..0x45251D is inside
  ours of the host. The tool's "uncovered 0x45251E..0x452570" is the host's
  last case (`0x45251E`, the byte 6's), the table and padding.
- **`0x4523C0`'s extent is the tool's 0x1A4** (to the end of its table), not
  the cut's 160, which stopped at the case above.
- The other size differences are trailing padding (the cut's catalogue
  sizes run to the next 16-byte boundary); the extents in `symbols.toml` are
  the code's.
- Nineteen starts are hidden in a host's catalogue extent: `0x451480` in
  BE5's `0x450D60`; fourteen task states in `0x4525B0`'s (a 0x38-byte
  function whose catalogue extent ran on over `RestoreForm_Task` and the
  three tasks); BMAGIC's four in `FxSpiral_Draw`'s (ours in `magic_s23`, whose
  `symbols.toml` size 0x332 already stops before them, and whose source
  holds none of their code). Each is reached by address (a `.data` table
  cell, a stack-table immediate or `MapCell_Handlers`), so each is taken as
  its own function.

## 2. What each function does

### 2.1 The transformation (`form`)

`DragonForm_Transform` `0x4514A0` (called by `Accession_Start` `0x4EAEA0`)
changes the actor (`0x904B34`, a party member) into a form:

1. The record's buffs `+0x138..+0x13F` zeroed, `+0x130 &= ~0x18000`,
   `+0x134 &= 0xFFFB84FF`; `Battle_RecalcStats(actor)`; the work cells
   `0x675F56` (the form code) and `0x675F57` (its group code) zeroed;
   `+0x134 |= 2` (the actor read again).
2. **A recipe**: `DragonForm_FindRecipe` - the first of recipes 0..10 whose
   three genes (`0x64ED78`, 0xFF for any) are all among the chosen genes
   (`0x904B84`, count `0x904B87`), its group code from `0x64ED6C`; at recipe
   6, first `DragonForm_TryPartyRecipe`: with a gene 0x10 chosen and the
   party's size byte `0x904AB1` at 3, `DragonForm_PartyRecipe` pairs the
   other two members' `+0x89` bytes (skipping the actor and any member
   `Battle_ActorIsOut` rules out) in both orders against five pairings,
   answering forms 0xB..0x14 by `DragonGenes_Held` of a gene (7, 5, 0xD, 3:
   the group code 1 when held), 0 for one pairing, 0xFF for none - a non-zero
   answer is the recipe; else recipe 6 is skipped. A recipe goes to
   `DragonForm_ApplyRecipe` (the party records backed up to `0x939AE0`, five
   stats by the recipe's percents of `0x64EF00 + 14 r` through `0x446F50` /
   `0x446F20`, nine form bytes to `+0xCF..` / `+0xAF..` / `0x939EEA..`, up
   to eight abilities from `0x64F028 + 8 r`, `0xD9` and zeros), and the form
   code is the recipe + 4; codes 8 and 7 / 4 set `+0x134` bit 16 / 17.
3. **No recipe**: `DragonForm_Mix` - the party backed up; fourteen signed
   sums `0x675F48..0x675F55` from the chosen genes' rows (`0x64ED9C + 14 g`,
   genes 0xA / 0xB / 0xC adding none), each 0xB negating the first ten, each
   0xA doubling all fourteen, each 0xC moving every sum by one either way
   (`Rand`: a zero sum one time in ten, another half the time); the form
   code 3 / 2 / 1 / 0 by sums 13, 11 (at least 1) and 10 (at least 2); five
   stats through `DragonForm_StatShift` (the step of `0x64EEAC` by a sum + 2
   held to 0..4) and the code's column of `0x64EE98` (the first 1 below 1,
   the others 0), their changes kept at `0x939EE2..`; the group code the
   last of sums 0..4 above 0 (1..5), 6 for two or more;
   `DragonForm_SetMixBytes` (the nine form bytes from sums 0..4 through
   three five-byte tables, and 2s); `DragonForm_MixAbilities` (the code's
   three abilities of `0x64EEC4`, then `DragonForm_AddAbilityRow` by the
   sums above 0 - rows 0..9 and 11..14 of `0x64EED0`, a row's ability
   skipped when present, nine at most - then `0xD9` and zeros).
4. The history (`0x904608`, six records of four bytes, newest first) moves
   down one; record 0 takes the chosen genes (`0x904B87` of them, the rest
   of three 0xFF) and, in its byte 3, `(group << 5) + (code & 0x1F)`;
   `0x904B79` the group, `0x904B89` the code; `Battle_RecalcStats(actor)`.

`DragonHistory_Leave` `0x451480`: entry 6 of the history's step table
`0x64ED50` (BE5's `0x450F30` jumps through it by `0x904AA4`; its step 5
`0x451290` reads the history's form bytes) - window records 18 and 19's
`+3` = 4 and 2, `0x904AA3` = 3, `0x904AA4` = 2.

`DragonGenes_SumCost` `0x4525B0` (BE5's Dragon run steps `0x44FFA0`,
`0x450610`, `0x450700` twice, `0x450E70`; entered live at the gene screen):
`0x904B78` = the chosen genes' costs (`0x64EC9C`) summed as a byte, stored
after each - the cost [`battle_actions.md`](battle_actions.md) says ability
0x97 is charged. `DragonGenes_Held` `0x452570`: whether a gene is chosen.

### 2.2 The stat rebuild, the roll, the AP pop-up (`stats`)

- `Battle_RecalcStats` `0x453300` (26 call sites: BE2, BE3, BE4, BE5,
  `magic_s12`'s `0x4B2060`, Capcom's): a member (0..2) through
  `Battle_RecalcMemberStats(record + 0x80, actor)`; an enemy (actor - 3):
  four words `+0xB4..` = `0x446F20(+0xD4.., buff * 2 + 100)` by the s8
  buffs `+0x118..`, four bytes `+0xC9..` through `0x446F80` by
  `+0x11C..`, eight bytes copied from `+0xDF..` (not `+0xC6`), `+0x114` bit
  14 triples `+0xB4`, bit 12 moves `+0xB6` into it, both through
  `0x446F20(v, 100)`. Then, for either path, **enemy** number (actor - 3,
  or the member's own number) `+0x114` bits 11 / 13 zero its `+0xC4` /
  `+0xBF` (section 7, L2).
- `Battle_RecalcMemberStats` `0x453560`: the same on the member's view
  (`+0x80`) by its buffs `+0x138..`; on the record `+0x134` bits 0 and 14
  triple `+0xA4`, bit 12 moves `+0xA6` into it, bit 1 with the form byte
  `0x904B89` at 0x16 raises `+0xBB` / `+0xBA` by 0x1E through
  `0x446F80(v, 100)`, bits 11 / 13 zero `+0xB4` / `+0xAF`; `+0xA4` / `+0xA6`
  through `0x446F20(v, 100)`.
- `Battle_MemberRollByAction` `0x453910`: `Battle_PickFlag8Member`'s test
  when the action record's `+4` has bit 9 - 0 when the member is out, lacks
  `+0x130` bit 0, its `+0x124` is not the actor, `Battle_ActionBitSet` of the
  action record's (`0x904B40`) byte `+2`, or `Battle_MemberListFull`; else 1
  when `Rand` leaves no remainder by the odds of the action row's high nibble
  (`0x65C4D9 + 0x18 id`; `0x64F104` when the member's effect state
  `0x66972C + its +0x89` is 6, else `0x64F0FC`).
- `Battle_ActionBitSet` `0x453A90`: bit `action & 31` of the dword `0x904088
  + 4 (action >> 5)` - the set `Battle_SettleFlag8` adds to.
  `Battle_MemberListFull` `0x453AC0`: all ten bytes `+0xFE..` not 0.
- `Battle_SetApPopup` `0x453EB0` (`EnemyOp_ReceiveAction` passes the result's
  AP delta; BE3's `0x441A90`, `0x442890`, BE5's `0x44C5C0`):
  `Battle_SetDamagePopup`'s twin - a pop-up task `BattleTask_Create(0, 1)`,
  `+0xB` = 1, `+0x80` the actor's record, `+0x60` the s16 amount's
  magnitude, `+0x27` 1 below 0 else 2, `+7` 4 / 2 (and `+0x27` 1) / 0 by the
  actor's `+0x12C` / `+0x10C` bits 1, 3, 5.

### 2.3 Three effect tasks (`tasks`; `Sprite_Current` the task slot)

`BattleFx_Dispatch`'s stack table holds the three dispatchers (slots 15,
16, 18): each `call`s its table's entry by `+1` (ours aborts past it), then
tail-jumps to `Sprite_UpdateScreen` while `+0` is not 0.

- **Slot 15** (`BattleFxDash_Dispatch`, `BattleFxDash_Steps` `0x64F0D0`, 7;
  the task `0x442E60` creates with a copy of the acting member): `_Start`
  (`+0x20` = `+0x3C` + 0x4000000, pose `+8` + 0x38, `+0xB` = 0x10), `_WaitPose`
  (on `BattleObj_ScriptTickOnce`, pose + 0x3C), `_Rise` (`+0x3C` up
  0x1000000 a frame to `+0x20`; then placed at the target's side -
  `Field_Kind2X` / `_Kind2Z` plus the s8 pair of `0x64E4F4` by `0x904AAC`,
  the other pair and `+8 ^= 2` for a member target - pose + 0x40),
  `_Advance` (a trail task each frame, `BattleTask_Create(0, 0x12)` - slot
  18 - copied from the slot with `+0xB` passed down; a step toward the
  target, `MagicFx_StepToward(.., 0x50)`; near it,
  `MagicFx_NearSprite3D(.., 0x10000)` tested as a whole `eax`, pose + 0x44
  and `Battle_SetTargetFlag40`), `_Return` (velocities toward the actor, a
  sixteenth of each difference, rounded toward zero; `+0x14` = 0x40, `+0x20`
  = -8), `_Arc` (the velocities added, the word `+0x3E` by `+0x14` before
  `+0x3C` by `+0x18`, `+0x14` by `+0x20` - on at -0x40), `_Land` (the actor's
  `+0` bit 6 cleared, the owner given pose `+8` + 4 as `Sprite_Current`,
  which is put back; the slot freed).
- **Slot 16** (`BattleFxPose_Dispatch`, `BattleFxPose_Steps` `0x64F0EC`, 2):
  `_Start` (pose + 0x50, a tick, `+0x29` = 3, `+9` = 8), `_WaitOwner` (a
  tick; freed once the owner's `+1` is 5).
- **Slot 18** (`BattleFxTrail_Dispatch`, `BattleFxTrail_Steps` `0x64F0F4`,
  2: `_Start`, then round eleven's `BossWeretigrFx_TrailFade`):
  `BattleFxTrail_Start` - `+0` bit 5, `+0x5C` = 3, the shade bytes
  `+0x5D` / `+0x5F` / `+0x5E` = `+0xB` * 0xF0, `+9` = 8, a tail jump to
  `Sprite_ScriptTickOnce`.

### 2.4 The field slots (`slots`)

`Field_SlotsReleaseOwner` `0x454A80`: `Field_SlotRelease(i)` for each of the
eight `Field_Slots` records whose `+0xC` is the object. `Field_SlotStart`
`0x455290`: the first record without `+0` bit 0 gets `+0` = 1, `+0xC` the
object, `+4` the script, `+2` = 0xFF, `+3` the object's `+0x27`; its index,
0xFF when none is free. Both were read by AR2B ([`area_w2b.md`](area_w2b.md))
and left as round eleven's debts; nothing here changes their reading.

### 2.5 BMAGIC's four map cells (`cells`)

`MapCell_Handlers` 40..43, called by `DrawLayer_Open` with (the record,
byte 1, byte 0 of the run's head); each draws only when
`Area_TestCondition(the record's word)` answers.

- **40, `MapCell_DrawTexQuads`**: the cell at `x0 = (b1 - 0x80) << 7`, `z0 =
  (b0 - 0x80) << 7` (`0x4CF4B0(x0 - 0x80, z0)` first, its answer unread);
  entries of five dwords after the head: a vertex's x and z from its bytes 3
  and 2 (doubled), its height `0x4CF4B0(x, z)` plus its low word; the
  first projected (`Gte_LoadVertex`, `Gte_Rtps`, `Gte_StoreScreenXY` into the
  next packet `+8`), and on screen (the bounds at `0x5C41FC..0x5C4208`,
  exclusive) an FT4 of the other three (`Gte_Rtpt`), the fifth dword its
  texture, committed 0x48 bytes to 7 / 4 / 6 / `Draw_OtSlot` by texture
  bits 14 and 30, then the screen points and depths stored into it.
- **41, `MapCell_DrawShadedQuads`**: entries of eight dwords: a draw mode
  `(dword 5 >> 24 & 3) << 5 | 0x95` committed, a G4 of four vertices
  (`Gte_RotTransPers4` in the order 1, 2, 3, 0, the depths), semi-transparent
  by dword 5's bit 31, four colours from dwords 5..8, committed 0x44; after
  the entries a draw mode 0x95 committed.
- **42, `MapCell_DrawSpinQuads`**: a centre (dword 1) and three spin rates
  (dword 2, 10 bits each, times `Frame_Counter & 0xFFF`, times 4); entries of
  six dwords, each under a pushed matrix: the centre by `Gte_RotTrans` into
  the matrix's translation, its angles (the spin plus four times its own
  three 10-bit fields, & 0xFFF) by `Gte_RotMatrix` times `Camera_Matrix`,
  set; an FT4 of four sign-extended 10-bit vertices, dword 6 its texture,
  committed 0x48; popped.
- **43, `MapCell_DrawGroundSprite`**: a point on the ground
  (`Prim_VertexScratch`: x and z as 42's centre, y minus half of
  `AreaMap_Elevation` there) projected by `Gte_RotTransPers` into
  `MapView_ScreenXY`, z its answer; on screen (`0x5C4200..0x5C4210`,
  inclusive) and z not 0: the frame by `Frame_Counter` modulo byte 8 against
  the thresholds from byte 0xA; its corners four s8 * 1125 / z around the
  point (x87 arithmetic in double precision, stored as floats); an FT4 with
  the frame's texture, committed 0x48.

## 3. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (grep of `DIVERGENCE.md` and `cheats.cpp` for the 39 addresses: none
patched). Where the original indexes past a table, divides by a byte that
can be 0, reads stack bytes it never wrote or loops past its record, ours
aborts with a `Fatal` naming the function (the owner's rule, round9 doc
section 6; D106 for the unwritten stack bytes) - section 7.

## 4. Calls across groups

**Out of BE6**: to no function of another group (`band_rows.py --edges`: BE6
calls none of BE1..BE7). To code nobody owns, raw in `battle_e6_callees.h`
and in the harness's engine set: `0x446F20`, `0x446F50`, `0x446F80` (the
percent clamps, `kThrough`), `0x4CF4B0` (a BMAGIC vertex's height). Every
other callee is ours, by name.

**Into BE6 from outside the group** (for the rebinding pass):

| Callee | Callers |
|---|---|
| `DragonForm_Transform` `0x4514A0` | `Accession_Start` `0x4EAEA0` (ours, `magic_s33`) |
| `DragonHistory_Leave` `0x451480` | BE5's `0x450F30` through `0x64ED50[6]` (read in place) |
| `DragonGenes_SumCost` `0x4525B0` | BE5's `0x44FFA0`, `0x450610`, `0x450700` (twice), `0x450E70` |
| `BattleFxDash_Dispatch` / `BattleFxPose_Dispatch` / `BattleFxTrail_Dispatch` | `BattleFx_Dispatch`'s stack table (ours, `battle_fx_tasks`) |
| `Battle_RecalcStats` `0x453300` | BE2's `0x433DA0`, `0x434340`; BE3's `0x4420A0`, `0x442310`; BE4's `0x449A00`, `0x449C70`; BE5's `0x44B3A0`; `magic_s12`'s `0x4B2060` (ours); Capcom's `0x44D450` .. `0x44FB30` (16 sites nobody owns) |
| `Battle_MemberRollByAction` `0x453910` | `Battle_PickFlag8Member` (ours, `battle_sprites`, three sites) |
| `Battle_ActionBitSet` `0x453A90` | `Battle_SettleFlag8` (ours, `battle_sprites`); `boss_harness_eh`'s self-test copies it |
| `Battle_MemberListFull` `0x453AC0` | `Battle_MemberCoinFlip` (ours, `battle_sprites`) |
| `Battle_SetApPopup` `0x453EB0` | `EnemyOp_ReceiveAction` (ours, `enemy_ai_ops`); BE3's `0x441A90`, `0x442890`; BE5's `0x44C5C0` |
| `Field_SlotsReleaseOwner`, `Field_SlotStart` | areas 85, 174, 198 (`area_w2b`, `area_w4c`, `area_w4f`), `BossAmalgam_*` (`boss_sc`), `BossMyria_*` (`boss_sf`) |
| the four cells | `MapCell_Handlers[40..43]` (`DrawLayer_Open`, read in place) |

## 5. The rebinding

The round-ten form: the constant keeps its value (the fuzz files key on
it) and its initialiser names the function now that it is ours. The
build, `ledger_check.py` and `BOF3X_SHADOW='*'` after (section 6).

| File | Was | Now |
|---|---|---|
| `magic_s33.cpp` | `kResetActor = 0x4514A0` | `bof3::addr::DragonForm_Transform` |
| `magic_s12.cpp` | `kStatChanged = 0x453300` | `bof3::addr::Battle_RecalcStats` |
| `battle_fx_tasks.cpp` | `BattleFx_Dispatch`'s slots 15, 16, 18 `H(0x452680)`, `H(0x452AD0)`, `H(0x452B60)` | `H(bof3::addr::BattleFxDash_Dispatch)`, `_Pose_`, `_Trail_` |
| `battle_sprites.cpp` (and the comments of `battle_sprites_callees.h`) | `Raw<..>(0x453910)`, `(0x453A90)`, `(0x453AC0)` | `bof3::addr::Battle_MemberRollByAction`, `Battle_ActionBitSet`, `Battle_MemberListFull` |
| `enemy_ai_ops_callees.h` | `kApPopup = 0x453EB0` | `bof3::addr::Battle_SetApPopup` |
| `area_w2b_callees.h`, `area_w4c_callees.h`, `area_w4f_callees.h`, `boss_sc_callees.h`, `boss_sf_callees.h` | `kSlotsReleaseFor = 0x454A80`, `kSlotStart = 0x455290` | `bof3::addr::Field_SlotsReleaseOwner`, `Field_SlotStart` (four headers gained the `symbols.gen.h` include) |

**Left raw, on purpose**: the fuzz files' `CallSite` / `Imm` / `Callee`
rows naming these addresses (`magic_s33_fuzz`, `magic_s12_fuzz`,
`battle_fx_tasks_fuzz`, `battle_sprites_fuzz`, `enemy_ai_ops_fuzz`'s
`case kApPopup`, the area and boss fuzzes' `0x454A80` / `0x455290` rows -
they are the keys, the round-ten rule); `boss_harness_eh.cpp`'s copy of
`0x453A90` (the harness's own self-test of Capcom's bytes); comments that
cite an address; `boss_harness.h`'s band comment. **Not mine to edit**: no
file another group of this wave writes names a BE6 function (BE2..BE5 call
`0x453300`, `0x453EB0`, `0x4525B0` from code that is not ours yet - their
own groups call them raw, and the coordinator rebinds after both merge).

## 6. The fuzz

`BOF3X_SHADOW=battle_e6`, `src/game/battle_e6_fuzz.cpp`, `Group::engine`
set, five `Run`s (`BOF3X_BE6_RUN=form|stats|tasks|slots|cells` runs one),
6,000 rounds a function. Shapes: `kHelper` for the cdecl functions (their
words this file's `Args`, drawn in `Seed` into a global so that a word and
the memory it points at agree), `kStep` for `DragonHistory_Leave`, `kTask`
for the tasks (the dispatchers' `+1` drawn below 7, 2, 2; the three tables
`DataTable`s, their cells recorders). The group's own functions each other
calls are recorders (their own rows test them); `ret_mask` 0xFF on every
function answering in `al`.

| Run | Clones | Rounds | Calls | Result |
|---|--:|--:|--:|---|
| `form` | 14 | 84,000 | 168,016 | 0 mismatches |
| `stats` | 6 | 36,000 | 19,557 | 0 mismatches |
| `tasks` | 13 | 78,000 | 148,416 | 0 mismatches |
| `slots` | 2 | 12,000 | 23,969 | 0 mismatches |
| `cells` | 4 | 24,000 | 187,820 | 0 mismatches |

(This worktree's counts; they move with the build directory.)

`BOF3X_SHADOW='*'` (every group of every harness, this worktree, the build
with the rebinding of section 5): exit 0 on the first run, 958 lines of 0
mismatches and none other, BE6's five `Run`s among them (their call counts
within a few hundred of the table's: the build directory). `ledger_check.py`:
0 errors (6,277 `impl` lines, 6,277 detoured).

**Stand-ins beyond the harness's.** `FormEffect` on the recorders of
`DragonForm_FindRecipe`, `_ApplyRecipe` and `_Mix`: notes and moves the form
and group codes `0x675F56` / `0x675F57` (the transformation reads them again
after those calls); `DragonForm_FindRecipe`'s answers none half the time (so
the mix path is as common as the recipe path). `Battle_ActorIsOut` for
`DragonForm_PartyRecipe`: at most one member out a round (`OutEffect`), so
two others are always found - fewer is the original's unwritten-stack read,
which ours answers with 0 (section 7, L1; DIV-0063) and the fuzz leaves alone - and that clone runs `calm` (a
disturbance moving the actor between two of its calls would skip a second
member): its read again of the actor is therefore not probed. The roll's
three refusals (`Battle_ActorIsOut`, `_ActionBitSet`, `_MemberListFull`)
answer 0 two times in three (`MostlyZero`). The tasks' `BattleTask_Create`
answers 0xFF a third of the time (`CreateMayFail`: `_Advance` tests it; the
AP pop-up, which does not, runs in `stats` with the standard 0..47).
`Area_TestCondition` logs the word its callers load into `ax`; the GTE's
matrix family (`Gte_PushMatrix`, `_PopMatrix`, `_RotTrans`,
`_SetRotMatrix`, `_SetTransMatrix`) runs for real (`kThrough`), as the
engine set's other GTE calls do, so the push and pop keep the matrix each
pass starts from and the vectors built on the stack are compared through
what they project. The cells' heights (`0x4CF4B0`, `AreaMap_Elevation`,
whose `ax` alone is read) answer -256..256 (`SmallHeight`), so that a
seeded projection lands the vertices near the screen.

**Regions beyond the engine frame**: the work cells `0x675F48..0x675F57`,
the party backup's tail `0x939B20..0x939EC0`, the action bits `0x904088`
(0x20), `MoveScript_EffectState` (0x18), `Field_Slots` (0x80),
`Field_Kind2Z` / `_Kind2X`, `Prim_VertexScratch` and `MapView_ScreenXY` (8
each), `Draw_OtSlot`, and a 0x200-byte BMAGIC record of the fuzz's own; for
`cells` also the GTE's state `0x7DE428..0x7DE7A8` (`magic_s23_fuzz`'s
region) less `Gte_Vertices` `0x7DE468..0x7DE47F` (the loads copy each
vertex's fourth short, which the originals leave as stale stack bytes: L9)
and `Camera_Matrix` (0x20). Seeded two times in three for a projection: a
rotation near identity in `Gte_Matrix` and `Camera_Matrix`, a translation
1000..4000 deep, a distance 300..800, offsets near the screen's middle,
`Gte_MatrixDepth` 0..15.

**Seeds.** The chosen genes 0..3 of them mostly (0..7 else), from 0..0x10
mostly with one of 0xA / 0xB / 0xC half the time; the actor's character
index 0 (in the state) two times in three; the party count 0..3; a recipe's
gene planted for `_RecipeSlotHeld`; gene 0x10 and the party size 3 for
`_TryPartyRecipe`; for `_PartyRecipe` the actor a member or not and the
members' `+0x89` from 1..8; the sums at -2..2 and the form code 0..3 for
`_MixAbilities`; a row's ability planted in the list for `_AddAbilityRow`,
its count 0..9 mostly; recipes 0..20; signed bytes at -6..6 for
`_StatShift` / `_SetMixBytes`. The target 0..10 for the rebuild; the member
view at its own member's `+0x80` mostly; the form byte 0x16 half the time;
for the roll `+0x130` bit 0, `+0x124` the actor, `+0x89` inside the effect
states with a 6 planted half the time, and the action record's id drawn
until both odds tables are non-zero at its nibble (a zero is ours' abort,
not a count); the list full with one zero half the time; amounts -20..20
and garbage. The tasks: `+0x3C` below `+0x20` half the time, `0x904AAC`
0..7 mostly, `+0xB` 0..2 mostly, `+0x14` / `+0x20` one step from -0x40 half
the time, the owner's `+1` at 5 half the time. The slots: an object from
the party, the enemies or the harness's records planted in half the slots'
`+0xC`, every slot taken half the time. The cells: the length byte a whole
number of entries (1 + 5 n, 1 + 8 n, 3 + 6 n, n 0..4), slot 43's frame count
1..255, its head byte 0..39 and a threshold 0xFF at the fourth frame; the
cell bytes 0..255 mostly, garbage else. **Disturbance** (the group's): the
form and group codes, the gene count (0..5), a gene, the party size byte,
the form byte (0x16 half the time).

## 7. Latent defects (Capcom's, described, not fixed)

- **L1 - `DragonForm_PartyRecipe` reads two stack bytes it never wrote**
  when fewer than two of the other members are in (a member out by
  `Battle_ActorIsOut`, or the actor is not a member): its pair list is an
  uninitialised local, and every path then dispatches on or compares a
  garbage byte. Reached from the transformation with a gene 0x10 chosen and
  the party's size byte at 3, which an ordinary battle can be (a party of
  three with one member down); what the original then does depends on the
  stack its callers left. **Ours answers 0** - no party form, recipe 6
  skipped, the answer the two failing pairings give - by the owner's
  account of the game, 2026-09-29 (DIV-0063). As first taken it aborted
  (D106's rule), and the group's suggestion was 0xFF; 0 is the path the
  game's own failures take. The self-test's seven rows for it run ours
  alone; the owner's check in game is owed.
- **L2 - `Battle_RecalcStats` clears enemy bytes for a member**: after a
  member's rebuild its tail indexes the **enemy** objects by the member's
  own number, so enemy 0..2's `+0xC4` / `+0xBF` are zeroed when that
  enemy's `+0x114` has bit 11 / 13. Kept.
- **L3 - `Battle_MemberRollByAction` divides by the odds unchecked** (a zero
  in `0x64F0FC` / `0x64F104` at the action row's nibble): ours aborts. Which
  nibbles the action rows use was not measured.
- **L4 - `MapCell_DrawGroundSprite` divides by the record's byte 8
  unchecked** (ours aborts on 0), and its frame scan runs until a
  threshold byte above the tick, unbounded (kept: a read).
- **L5 - the three BMAGIC entry loops end only when their counter meets the
  length byte** (1 + 5 n, 1 + 8 n, 3 + 6 n); another length runs on past
  the record until something faults. Ours aborts first.
- **L6 - the three task dispatchers index their tables by `+1` unchecked**:
  ours aborts past 7 / 2 / 2.
- **L7 - `Battle_SetApPopup` does not check `BattleTask_Create`'s 0xFF**
  (D54's twin for the damage pop-up): kept, ours writes where the original
  would.
- **L8 - `DragonForm_Transform` copies `0x904B87` genes into the history**
  unbounded (more than three run into the older records, the form byte
  written after); kept.
- **L9 - the BMAGIC cells hand the GTE vertices whose fourth short is stale
  stack** (their SVECTORs' pad; `Gte_LoadVertex` / `_LoadVertices3` copy it
  into `Gte_Vertices`, where nothing reads it): harmless, D101's kind. Ours
  passes 0; the fuzz leaves those six dwords out of the compared state.

## 8. Controls

`controls.py` (scratch; the round's form): each plant one change in
`battle_e6.cpp`, anchored on a unique string, rebuilt, its unit run alone
(`BOF3X_BE6_RUN`), restored and rebuilt. **66 planted, 65 refused by a
count, one near-equivalent recorded and replaced** (in this worktree; the
numbers are rounds of 6,000 that mismatched).

| n | Run | Planted in ours | Refused (rounds) |
|--:|---|---|---|
| 1 | `form` | DragonHistory_Leave: 0x904AA4 = 3 | `DragonHistory_Leave` 6,000 |
| 2 | `form` | Transform: code 8 sets bit 15, not 16 | `DragonForm_Transform` 98 |
| 3 | `form` | Transform: the history moves four records | `DragonForm_Transform` 5,998 |
| 4 | `form` | Transform: group << 4 | `DragonForm_Transform` 4,265 |
| 5 | `form` | Transform: the mix code read before the mix | `DragonForm_Transform` 1,568 |
| 6 | `form` | FindRecipe: recipes 0..9 | `DragonForm_FindRecipe` 87 |
| 7 | `form` | FindRecipe: the party form answered as 6 | `DragonForm_FindRecipe` 480 |
| 8 | `form` | RecipeSlotHeld: 0xFF not "any" | `DragonForm_RecipeSlotHeld` 1,039 |
| 9 | `form` | TryPartyRecipe: party size 2 | `DragonForm_TryPartyRecipe` 1,298 |
| 10 | `form` | Mix: gene 0xB negates nine | `DragonForm_Mix` 341 |
| 11 | `form` | Mix: a zero sum moves one time in nine | `DragonForm_Mix` 537 |
| 12 | `form` | Mix: the first stat 0 below 1 | `DragonForm_Mix` 2,619 |
| 13 | `form` | Mix: group 6 for three positive | `DragonForm_Mix` 735 |
| 14 | `form` | Mix: code 1 by sum 10 at least 1 | `DragonForm_Mix` 561 |
| 15 | `form` | StatShift: held to 0..3 | `DragonForm_StatShift` 2,433 |
| 16 | `form` | SetMixBytes: the fourth byte 3 | `DragonForm_SetMixBytes` 6,000 |
| 17 | `form` | MixAbilities: row 13 for 14 | `DragonForm_MixAbilities` 1,108 |
| 18 | `form` | AddAbilityRow: full at 8 | `DragonForm_AddAbilityRow` 772 |
| 19 | `form` | ApplyRecipe: seven abilities at most | `DragonForm_ApplyRecipe` 725 |
| 20 | `form` | ApplyRecipe: the fourth stat by the third percent | `DragonForm_ApplyRecipe` 1,036 |
| 21 | `form` | PartyRecipe: 4 with 1 dropped | `DragonForm_PartyRecipe` 91 |
| 22 | `form` | PartyRecipe: 6 unheld answers 0xE | `DragonForm_PartyRecipe` 64 |
| 23 | `form` | GenesHeld: the last gene not searched | `DragonGenes_Held` 623 |
| 24 | `form` | SumCost: the next gene's cost | `DragonGenes_SumCost` 3,946 |
| 25 | `stats` | RecalcStats: the member view 4 bytes on | `Battle_RecalcStats` 1,634 |
| 26 | `stats` | RecalcStats: an enemy buff times 3 | `Battle_RecalcStats` 958 |
| 27 | `stats` | RecalcStats: bit 11 zeroes +0xC5 | `Battle_RecalcStats` 2,961 |
| 28 | `stats` | RecalcMemberStats: form 0x17 | `Battle_RecalcMemberStats` 1,255 |
| 29 | `stats` | RecalcMemberStats: +0x57 into +0x36 | `Battle_RecalcMemberStats` 6,000 |
| 30 | `stats` | MemberRoll: the odds tables swapped | `Battle_MemberRollByAction` 106 |
| 31 | `stats` | MemberRoll: +0x125 for the actor | `Battle_MemberRollByAction` 2,573 |
| 32 | `stats` | ActionBitSet: bit & 15 | `Battle_ActionBitSet` 1,515 |
| 33 | `stats` | ListFull: nine bytes | `Battle_MemberListFull` 199 |
| 34 | `stats` | ApPopup: negative +0x27 = 2 | `Battle_SetApPopup` 2,617 |
| 35 | `stats` | ApPopup: bit 2 for bit 3 | `Battle_SetApPopup` 769 |
| 36 | `tasks` | TaskDispatch: +1 tested for the update | `BattleFxDash_Dispatch` 894; `BattleFxPose_Dispatch` 2,897; `BattleFxTrail_Dispatch` 2,916 |
| 37 | `tasks` | DashStart: + 0x2000000 | `BattleFxDash_Start` 6,000 |
| 38 | `tasks` | DashWaitPose: pose + 0x3D | `BattleFxDash_WaitPose` 3,984 |
| 39 | `tasks` | DashRise: the enemy side read unflipped for a member | `BattleFxDash_Rise` 218 |
| 40 | `tasks` | DashRise: an unsigned height compare | `BattleFxDash_Rise` 1,529 |
| 41 | `tasks` | DashAdvance: the trail keeps bit 6, loses bit 7 | `BattleFxDash_Advance` 2,981 |
| 42 | `tasks` | DashAdvance: near tested by al | `BattleFxDash_Advance` 1,020 |
| 43 | `tasks` | DashReturn: sixteenths toward minus infinity | `BattleFxDash_Return` 3,384 |
| 44 | `tasks` | DashArc: on at -0x38 | `BattleFxDash_Arc` 3,023 |
| 45 | `tasks` | DashLand: owner pose + 5 | `BattleFxDash_Land` 6,000 |
| 46 | `tasks` | PoseStart: +0x29 = 4 | `BattleFxPose_Start` 6,000 |
| 47 | `tasks` | PoseWaitOwner: owner state 6 | `BattleFxPose_WaitOwner` 2,911 |
| 48 | `tasks` | TrailStart: +0x5F by 0xE0 | `BattleFxTrail_Start` 5,626 |
| 49 | `tasks` | PoseDispatch: its table one entry on (entry 1 for 0) | `BattleFxPose_Dispatch` 6,000 |
| 50 | `slots` | SlotsReleaseOwner: seven slots | `Field_SlotsReleaseOwner` 3,023 |
| 51 | `slots` | SlotStart: +2 = 0xFE | `Field_SlotStart` 2,972 |
| 52 | `slots` | SlotStart: the object's +0x26 | `Field_SlotStart` 2,786 |
| 53 | `cells` | TexQuads: bit 14 without 30 to slot 5 | `MapCell_DrawTexQuads` 688 |
| 54 | `cells` | TexQuads: no left bound | `MapCell_DrawTexQuads` 183 |
| 55 | `cells` | CellVertex: z by 3 times its byte | `MapCell_DrawTexQuads` 3,210; `MapCell_DrawShadedQuads` 3,290 |
| 56 | `cells` | ShadedQuads: mode \| 0x94 | `MapCell_DrawShadedQuads` 3,290 |
| 57 | `cells` | ShadedQuads: the green byte >> 9 | `MapCell_DrawShadedQuads` 3,290 |
| 58 | `cells` | SpinQuads: the second angle times 2 | `MapCell_DrawSpinQuads` 3,141 |
| 59 | `cells` | SpinQuads: the sign bit 8 | `MapCell_DrawSpinQuads` 3,105 |
| 60 | `cells` | GroundSprite: 1124 | `MapCell_DrawGroundSprite` 160 |
| 61 | `cells` | GroundSprite: the head + 4 | `MapCell_DrawGroundSprite` 310 |
| 62 | `cells` | GroundSprite: the height + 1 | `MapCell_DrawGroundSprite` 3,991 |
| 63 | `cells` | GroundSprite: the right bound exclusive | **not refused** (0 mismatches) |
| 64 | `cells` | GroundSprite: the right bound 0x5C4204 (slot 40s) | `MapCell_DrawGroundSprite` 4 |
| 65 | `form` | PartyRecipe: the pair tried one way only | `DragonForm_PartyRecipe` 608 |
| 66 | `form` | Transform: the last rebuild for the first actor read | `DragonForm_Transform` 5,831 |

What the first pass taught (each fixed in the fuzz, not in ours, then the
control run again):

- **25** first planted `actor <= 1` for the member test: member 2 then took
  the enemy path with index 255 and ours faulted at `Enemy(255)` (exit
  0xC0000005, not a count). Replaced by a plant inside the member path.
- **49** first planted a one-entry table: refused by ours' own `Fatal` (a
  state past it), not a count. Replaced by the table moved one entry on,
  still two long.
- **53, 54, 58..61, 63** were not refused at first: the GTE's state at
  start-up projects every vertex to one degenerate point, so slot 40's
  screen test never passed and slot 42's matrices changed nothing visible.
  The fuzz now holds the GTE's state (`0x7DE428..0x7DE7A8`, less
  `Gte_Vertices`) and `Camera_Matrix` as regions, seeds a sane projection
  two times in three and small heights from `0x4CF4B0` /
  `AreaMap_Elevation`, and fills the record with small vertex bytes; all
  were refused on the re-run but 63.
- **63** (slot 43's right bound `>` made `>= bound + 1`) is a near-
  equivalent: only a projected x in (380, 381) tells them apart, and no
  round landed there. Recorded as such; **64**, the bound read from slot
  40's `0x5C4204` instead, is refused (in 4 rounds: few projections land
  between the two bounds).

## 9. The live route

The owner's `tools/recipes/dragonTransform.txt` (2026-09-28) enters 15 of
the 39 (`analysis/calltrace/reach_dragon/reach_dragon_new.txt`):
`DragonGenes_SumCost` (frame 976, the gene screen, from BE5's `0x44FDE0`),
`DragonForm_Transform`, `_FindRecipe`, `_RecipeSlotHeld`,
`_TryPartyRecipe`, `_Mix`, `_StatShift`, `_SetMixBytes`, `_MixAbilities`,
`_AddAbilityRow`, `Battle_RecalcStats`, `Battle_RecalcMemberStats` (frame
1798), `Battle_MemberRollByAction` (2366), `Battle_SetApPopup` (2526),
`DragonForm_ApplyRecipe` (3163). The coordinator's live check after the
wave plays them; the other 24 (`DragonHistory_Leave`, `_PartyRecipe`,
`DragonGenes_Held`, the tasks, `Battle_ActionBitSet`,
`Battle_MemberListFull`, the slots, the cells) are fuzz only.

## 10. What nothing reached

- `DragonForm_PartyRecipe` with a member out and the actor a member, and
  its read again of the actor (the clone runs calm; L1).
- The divisions by zero and the loops past a record (L3..L5): ours' aborts
  are never taken by the seeds, by design.
- Slot 43's projected x in the one-unit window above its right bound
  (control 63); few rounds land between slot 43's and slot 40's bounds
  (control 64 refused in 4).
- The cells with a real area's records and the game's camera: the fuzz's
  records are its own (whole entries of small vertex bytes) and its
  projection is seeded; no recorded route draws a BMAGIC cell.

## 11. For `analysis/calltrace/entries_logic.txt`

Eighteen lines appended to the main checkout's file (none was there): the
hidden starts `0x451480` (0x1B, inside `0x450D60`'s 0x73B), the fourteen
task states `0x452680..0x452B90` (after `0x4525B0`'s 0x40) and BMAGIC's four
`0x4CEB40` (0x21A), `0x4CED60` (0x251), `0x4CEFC0` (0x2A9), `0x4CF270`
(0x234). The other twenty-one were listed already with the catalogue's
extents (`0x4523C0` as 0x1A9, its table and a few bytes of padding: left).
