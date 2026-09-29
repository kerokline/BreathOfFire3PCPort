# Group BE5: the enemy AI's helpers, three effect slots, the transformation's stats and the Dragon command's run

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3), wave one, stage B, on the widened boss harness
([`boss_harness.md`](boss_harness.md) section 10) without edits to it.
**52 functions ours** (`src/game/battle_e5.cpp`, shadow name `battle_e5`),
each read to its last instruction with capstone: the cut's 47 less its start
`0x44B8D0` (a case, not a function) and the six the cut did not list. One
`Run`, 312,000 rounds (6,000 a function), 0 mismatches; 32 controls planted, all refused by a count (section 6).
The live route `dragonTransform.txt` enters four of them (section 10); the
rest are fuzz-only.

Names are this group's, from what the code does and who calls it: none of
the 52 has a name in the sibling (`pairs_propagated.json` pairs 44 of them
with PSX addresses the sibling's `names/*.toml` and `symbols.toml` leave
unnamed; each evidence string cites its twin). Nothing here states what a
command or gene does in the game: "the Dragon command" is the command
`BattleMenu_ConfirmDispatch` reaches through `0x44FF00`
([`battle_phases.md`](battle_phases.md)), "a gene" a bit of `0x904650` and a
byte of the slot lists, as the code handles them.

## 0. What the cut listed, and what the code has

`tools/band_rows.py --group BE5` (group RT's tool, 2026-09-29) against the
cut `analysis/round12_cut.tsv`:

- **Six starts the cut does not list**, resident code between two of its
  starts, each called or table-reached: `0x44B5E0` and `0x44B870` (called by
  `0x44B3A0`), `0x44B920` (called by `EnemyAI_TurnCheck` and
  `EnemyAI_ChooseActions`), `0x44C5C0` (`Effect_Handlers` slot 23, and
  `0x44EB50`'s tail `jmp`), `0x44FF60` (`0x64ECE8[1]`), `0x450250`
  (`0x64ECF0[4]`). Taken as the rest.
- **`0x44B8D0` is not a function**: it is case 5 of `0x44B870`'s eight-case
  switch (the jump table `0x44B900`, which `0x45A6AF` also reads - a
  second reader of the table, not a caller), 15 bytes that store the
  value's byte at `+0xE4`. The cut's 277 bytes run over cases 6 and 7, the
  table and into `0x44B920`. No `[[func]]`, no inject line; the host
  `EnemyAI_SetAttrByte` has it.
- **The cut's sizes are the catalogue's**: 38 of the 47 differ from the code
  (33 by trailing padding; `0x44B3A0` 1328 for 0x240, `0x44C3D0` 1008 for
  0x1EE, `0x44FF30` 112 for 0x23, `0x450200` 128 for 0x46 run over the
  unlisted starts after them). The extents in `symbols.toml` and section 9
  are the code's (every one ends at its last `ret` or tail `jmp`, the rest
  padding or a jump table).
- **Hidden starts in hosts already ours**: `0x44C3D0` and `0x44CCA0` lie in
  `Effect_ApplyResult`'s catalogue extent (`0x44B9F0`, 0x750). Ours of
  `Effect_ApplyResult` (`battle_damage.cpp`) does not contain them - it
  calls every slot through `Effect_Handlers` - so they are functions of
  their own, reached by their table cells. The other 38 hidden starts lie in
  hosts of this group (`0x44B3A0`, `0x44FDE0`, `0x450510`, `0x450D60`), each
  reached by a `.data` cell.

## 1. What each function does

### 1.1 The enemy AI's row helpers (`0x44B240..0x44B9E4`)

The callers are ours: `EnemyAI_TurnCheck` and `EnemyAI_ChooseActions`
([`battle_misc.md`](battle_misc.md), [`battle_damage.md`](battle_damage.md)),
and Capcom's `0x44AC80` / `0x44AB80`. A script is four rows of 16 bytes at
`0x8C5600 + 0x8C * object +0xF0`; byte 0 the condition, +1 the action's
kind, +2..+4 its operands, +6 a message word, +8..+15 eight bytes copied.

| Address | Name | What (by the code) |
|---|---|---|
| `0x44B240` | `EnemyAI_OtherRowsDone(row, enemy)` | al 1 when every row 0..3 but `row` of enemy `enemy`'s script is done: a row whose byte 0 is 0x63 answers 0, and so does one `EnemyAI_RowDone` calls not done. The script byte `+0xF0` is re-read per row. Condition 0x26 of `EnemyAI_ChooseActions` |
| `0x44B2E0` | `EnemyAI_SetRowDone(enemy, row, on)` | `+0xF1` bit `row & 31` set (on's byte not 0) or cleared; `shl al, cl` on a byte, so rows 8..31 name no bit |
| `0x44B320` | `EnemyAI_CondElement(mask)` | al: 0 unless the current enemy's word `+0x108` is not 0; acting kind `0x904B35` 4: the ability's element word (24-byte rows at `0x65C4DC` by the word `0x904B80`) & mask; kind 1 by a party member 0..2 with mask bit 8 clear: the member's weapon `+0x92`, its element byte `0x657463 + 28 * weapon` (the byte `Battle_CalcDamage` reads) & the mask's byte; else 0. `EnemyAI_TurnCheck`'s conditions 0..8 and 0x21..0x23 (named `EnemyAI_CondPartyFlag` there until now) |
| `0x44B3A0` | `EnemyAI_ApplyAction(enemy, row)` | the row's kind through the jump table `0x44B5C4`: 1 word `+0x90` = +2; 2 `Battle_ClearStatus(+5, 0xFFFF)` with `Sprite_Current` the enemy, then `0x44F1D0(+5, +0x92 \| row +2)`, `Sprite_Current` put back; 3 / 4 each bit of +2 from the top through `EnemyAI_ScaleStat` / `EnemyAI_SetAttrByte` with +3; 5 `+0xAA` / `+0xAE` = +3 by bits 1 / 0; 6 the words `+0x96` / `+0x94` scaled by +3 / 10, capped at 0xFFFF; 7 `0x904B97` = +2. Then always `+0x8E` = +4, +8..+15 to `+0x9C..+0xA3`, `0x453300(+5)`, and - HP and the row's word +6 both not 0, fewer than 8 - an enemy message: `0x939FC0 + 4n` = (+5 - 3, the word), `0x93C2A2` + 1 |
| `0x44B5E0` | `EnemyAI_ScaleStat(enemy, which, factor)` | not in the cut. Seven cases (`0x44B854`): HP `+0xA4` capped at `+0xD0`, AP `+0xA6` at `+0xD2`, `+0xD4` / `+0xD6` / `+0xD8` / `+0xDA` at 999, `+0x98` by the factor squared at 999; each `(x * f * 1000) / 10000` in 32 signed bits, the cap an unsigned compare |
| `0x44B870` | `EnemyAI_SetAttrByte(enemy, which, value)` | not in the cut. Eight cases (`0x44B900`): the byte at `+0xDF + which` for 0..6, `+0xE7` for 7 (`+0xE6` skipped). The cut's `0x44B8D0` is case 5 |
| `0x44B920` | `EnemyAI_DedupMessages()` | not in the cut. The enemy messages `0x939FC0` (count `0x93C2A2`; read by `BattleAction_EnemyMessages` / `BattleCommit_QueueMessages`) with each one dropped whose enemy's `+0x8C` byte and message word match one kept before it; the kept ones written back and counted. Its eax is not read (section 7) |

### 1.2 Three `Effect_Handlers` slots

Called by `Effect_ApplyResult` through `Effect_Handlers` (`0x64E73C`) with
no argument; the result record `*0x904B60` (+4 the HP delta, +6 the AP
delta, +8 flags) is what they fill.

| Address | Name | What |
|---|---|---|
| `0x44C3D0` | `Effect_DrainHp` (slot 22) | `0x44F6A0(actor, target)` answering not 0: result +8 = 1, nothing more. Else the target's HP / 5 as the HP delta (0 for a target with flag 0x10000), the acting sprite `*0x904B40` +8 = 1 (\| 4 when the actor's HP plus the delta passes its maximum), `0x590E80(&the actor's HP, its maximum, the delta)`, `Battle_SetDamagePopup(-change, actor)`, +8 = 0 |
| `0x44C5C0` | `Effect_DrainAp` (slot 23; `0x44EB50`, slot 124, jumps to it after acting kind 4 and ability 0x4E) | not in the cut. The same on AP (`+6`; party `+0x9A` / `+0xA2`, enemy `+0xA6` / `+0xB2`; flag 8), with `+8` = 2 on the acting sprite and the result, the pop-up `0x453EB0` |
| `0x44CCA0` | `Effect_QuarterAttack` (slot 38) | `(Rand & 0x7F) < 0x20`: the round flags `0x904AA8` \|= 0x80, the HP delta `Battle_CalcDamage(actor, target, 0xFFFF)`. Else result +8 = 1, `Battle_SetDamagePopup(0, target)`, tail `jmp 0x44FB30` |

### 1.3 `BattleForm_ApplyStats` (`0x44FDE0`)

Called by BE3's `0x442310`, BE4's `0x449A00` / `0x449C70` and Capcom's
`0x44D450`, `0x44DFD0`, `0x44E720` (the live route: from `0x442310`, frame
2,731). For each party member whose `+0x134` has bit 1: with `c` the
member's `+0x9E`, `c +0x22` and `c +2` = `w - (c[0] * w + 5) / 10` (w the
signed word `0x939EE0`, the product and division signed 32-bit); the words
`c +0x26 .. +0x2C` raised by `0x939EE2 .. 0x939EE8` and copied to `c +6 ..
+0xC`; the nine bytes `0x939EEA..0x939EF2` into `c +0x31..+0x39` and `c
+0x11..+0x19`. The block `0x939EE0..0x939EF2` is the harness's "the
transformation's cells"; what fills it (BE6's `0x4514A0..` by the plan) was
not read here.

### 1.4 The Dragon command's run (`0x44FF10..0x451471`)

`BattleMenu_ConfirmDispatch` (by the command `0x904AA2`) reaches `0x44FF00`
(**not ours, in no group**: 16 bytes, `jmp [0x64ECCC + 4 * byte 0x904AA3]`),
whose table `DragonCmd_Parts` holds seven parts; five of them dispatch again
by `0x904AA4`:

| `0x904AA3` | Part | Its table (by `0x904AA4`) and steps |
|--:|---|---|
| 0 | `DragonCmd_LoadDispatch` `0x44FF10` | `DragonCmd_LoadSteps` `0x64ECE8` (2): `_LoadStart` (window 16's +3 zero: `LoadDatFile(0xCC)`), `_LoadWait` (`File_LoadDone`: CLUT rows 0x1A, 0x1B, `Gfx_ClutStripDirty` + 1, part + 1) |
| 1 | `DragonCmd_Open` `0x44FFA0` | a step itself: windows 21 (the menu) and 17 claimed, sound 0x102, the 18 bits of `0x904650` into the byte flags `0x939A60`, the Dragon cells zeroed (`0x7E01B8`, `0x904AA6`, `AA7`, `0x904B72`, `B74`, `B87`, `B78`), line 1 (`0x904AA5`), `0x4525B0` (BE6), part + 1 |
| 2 | `DragonCmd_MenuDispatch` `0x450070` | `DragonCmd_MenuSteps` `0x64ECF0` (5): `_MenuInput` (the line `0x42 + 0x904AA5` shown; cancel two steps on, confirm one; 0x2000 / 0x8000 move the line, 0x4000 picks line 1; wrapped 0..2), `_MenuPick` (part 3 + the line), `_MenuClose`, `_MenuCancel` (window 21 closed: `0x447F40` (BE4), the actor's name, `LoadDatFile(0xCF)`), `_MenuCancelLoad` (`File_LoadDone`: back to the command step, `0x904AA2 = 0x904AA3 = 1`) |
| 3 | `DragonCmd_SlotsDispatch` `0x450280` | `DragonCmd_SlotsSteps` `0x64ED04` (7), the first slot list (`0x904608`, six slots; window 18, its cursor +0x12 / +0x10): `_SlotsCloseMenu`, `_SlotsOpen` (line 0x59), `_SlotsCursor` (cancel, confirm through `DragonCmd_SlotAffordable`, bit 0x10 to part 6 on a slot not empty, the cursor), `_SlotsCancel`, `_SlotsBack` (to part 2), `_SlotsConfirm` (the slot's genes into `0x904B84`, `0x4525B0`, the member's `+0x134` \|= 4), `_SlotsClose` (then part 4's step 3) |
| 4 | `DragonCmd_GenesDispatch` `0x4506C0` | `DragonCmd_GenesSteps` `0x64ED20` (5): `_GenesOpen`, `_GenesPick` (the grid of three rows of six over `0x939A60`, up to three genes into `0x904B84`, the banner and message lines), `_GenesConfirm` (the member's `+0x134` \|= 4), `_GenesClose` (`0x447F40`, `LoadDatFile(0xCF)`), `DragonCmd_Commit` (`File_LoadDone`: the command record's +1 = 4, the member's +1 = 2, `0x904AA1 = 1`, `0x904AA2..AA4 = 0`) |
| 5 | `DragonCmd_Slots2Dispatch` `0x450B20` | `DragonCmd_Slots2Steps` `0x64ED34` (7), the second slot list (`0x904620`, twelve slots; window 19): the same seven steps with line 0x63, the pricer `DragonCmd_Slot2Affordable`, no bit-0x10 branch |
| 6 | `DragonCmd_StoreDispatch` `0x450F30` | `DragonCmd_StoreSteps` `0x64ED50` (7): `_StoreOpen` (window 19), `_StoreWait` (its x 0xA3), `_StoreCursor` (line 0x61: the destination in the second list), `_StoreAsk` (line 0x62: a yes / no hand when the slot is not empty), `_StoreCopy` (the four bytes), `_StoreSource` (line 0x60: the source in the first list), and BE6's `0x451480` |

`DragonCmd_SlotAffordable` `0x450510` and `DragonCmd_Slot2Affordable`
`0x450D60` (called by the two cursors): 0 for an empty slot (+3 0xFF), else
the byte sum of `0x64EC9C[gene]` over its gene bytes not 0xFF against the
member's AP (`0x802DDA` by the menu actor's +5): al 1 when it is at most
that. The four cursor moves (`MoveCursor` in ours) are one body with two
bounds: `a + b` below 5 / 0xB and `a` at most 3 / 9.

Each dispatcher's table length is counted from the code: the dispatchers
have no compare, every step moves `0x904AA4` inside its own table (or moves
the part on and zeroes it), and each table ends where the next begins; the
dword after `DragonCmd_StoreSteps` is not code. The parts table's seven
likewise (`DragonCmd_LoadSteps` follows it).

## 2. Divergence

None: every function is a faithful replacement, and no `DIVERGENCE.md`
entry or `cheats.cpp` patch names any of the 52 addresses (grepped). Ours
aborts where the original would jump or write past a table (section 7);
those are the owner's rule for an unchecked index, not divergences.

## 3. Calls across groups (for the rebinding pass)

Out of this group, raw in `battle_e5_callees.h` until the owner merges:

| Callee | Owner | Called by |
|---|---|---|
| `0x447F40` | BE4 | `DragonCmd_MenuCancel`, `_SlotsClose`, `_GenesClose`, `_Slots2Close` |
| `0x4525B0` | BE6 | `DragonCmd_Open`, `_SlotsConfirm`, `_GenesPick` (twice), `_Slots2Confirm` |
| `0x453300` | BE6 | `EnemyAI_ApplyAction` |
| `0x453EB0` | BE6 | `Effect_DrainAp` |
| `0x451480` | BE6 | an entry of `DragonCmd_StoreSteps` (read in place; the table's recorder in the fuzz) |
| `0x44F1D0`, `0x44F6A0`, `0x44FB30`, `0x590E80` | nobody (part 7) | `EnemyAI_ApplyAction`; the drains; `Effect_QuarterAttack`; the drains |

Into this group from outside it (inbound):

| Caller | Owner | Callee |
|---|---|---|
| `0x442310` | BE3 | `BattleForm_ApplyStats` (call at `0x4423E9`) |
| `0x449A00`, `0x449C70` | BE4 | `BattleForm_ApplyStats` (`0x449B6B`, `0x449DBC`) |
| `0x44D450`, `0x44DFD0`, `0x44E720` | nobody (Capcom's) | `BattleForm_ApplyStats` |
| `0x44AC80`, `0x44AB80` | nobody (Capcom's) | `EnemyAI_SetRowDone`, `EnemyAI_CondElement`, `EnemyAI_ApplyAction`, `EnemyAI_DedupMessages` |
| `0x44EB50` | nobody (`Effect_Handlers` slot 124) | tail `jmp` to `Effect_DrainAp` |
| `0x44FF00` | nobody | `DragonCmd_Parts`' seven by `0x904AA3` |
| `EnemyAI_TurnCheck`, `EnemyAI_ChooseActions` | ours (battle_misc, battle_damage) | rebound (section 8) |

## 4. Named data (`symbols.toml` `[[data]]`)

`DragonCmd_Parts` `0x64ECCC` (7), `DragonCmd_LoadSteps` `0x64ECE8` (2),
`DragonCmd_MenuSteps` `0x64ECF0` (5), `DragonCmd_SlotsSteps` `0x64ED04` (7),
`DragonCmd_GenesSteps` `0x64ED20` (5), `DragonCmd_Slots2Steps` `0x64ED34`
(7), `DragonCmd_StoreSteps` `0x64ED50` (7). The cost bytes `0x64EC9C`, the
slot lists `0x904608` / `0x904620` and the flags `0x939A60` are described in
`battle_e5_callees.h`, not named in `symbols.toml`.

## 5. The fuzz

`src/game/battle_e5_fuzz.cpp`: one `boss_harness::Run` with `Group::engine`,
52 clones in their shapes (`kHelper` for the AI helpers,
`BattleForm_ApplyStats` and the two pricers, `al` compared where they
answer - `ret_mask 0xFF` on `EnemyAI_OtherRowsDone`, `EnemyAI_CondElement`
and both pricers; `kStep` for the three effect slots and the 33 steps;
`kDispatch` with `state_cell 0x904AA4` and `states` the table's length for
the six dispatchers), the six step tables as `DataTable`s, 6,000 rounds a
function. `BOF3X_BE5_ONLY=<substring>` runs the clones whose name holds it.

**The listing** (beyond the harness's engine set): the group's own four
called directly (`EnemyAI_ScaleStat`, `EnemyAI_SetAttrByte`, the two
pricers, `kFlag` for the pricers); this wave's `0x447F40`, `0x4525B0`,
`0x453300`, `0x453EB0`; and four masks narrowed where the caller pushes a
register whose upper bytes are its own garbage (different in the copy and
in ours) and the callee reads only the byte (each read checked with
capstone): `Battle_ClearStatus` (actor `cmp dl, 2` / `and eax, 0xFF`),
`Battle_SetDamagePopup` and `0x453EB0` (actor `cmp al, 2` / `and eax,
0xFF`), `Battle_CalcDamage` (ours: both actors `& 0xFF`), `0x44F6A0` (its
second word handed to `0x44F770`, which reads the byte). Where the upper
bytes are the original's own computation, ours reproduces them instead:
`0x453300`'s word carries the row pointer + 16's upper bytes,
`0x44F1D0`'s two words `Battle_ClearStatus`'s eax's upper half, `0x590E80`'s
cap the result pointer's upper half - the standard whole-word masks stand
and compare them.

**Louder stand-ins**: `0x4525B0`'s moves the gene count `0x904B87` half the
time (noting the old value first): `DragonCmd_GenesPick` reads it again
after the call. The pop-ups' (`Battle_SetDamagePopup`, `0x453EB0`) note the acting
sprite's +8 as they are called (the drains mark it and clear it after); the
standard `Sound_PlayEffect`'s moves `Input_Pressed` half the time (noted
first): the cursors read it again after a refusal's sound.

**Regions** beyond the engine frame: the gene flags `0x939A60` (0x14), the
repeat latch `0x7E01B8` (4), and `0x93C320..0x93C340` (the message list's
lines 12..15, past the enemies' tail).

**Seeds** (per function, after the harness's fill): the AI script rows
(0x63 a quarter of the time), the enemy 0..7 (past the eighth the objects
run off the image's end `0x93F000` - a fault on both sides, not a count);
the acting kind 4 / 1 / others, the actor 0..3, the ability and weapons,
`+0x108` 0; a row of kind 0..8 in the area block, HP 0, the message count
at 0, 1, 6, 7, 8; the stats at and around their caps; up to eight messages
with repeated enemies' groups and words; the drains' target and actor
0..10, flag 0x10000 on half, each stat at its maximum minus 0..3 half the
time; the members' `+0x134` bit 1 and `0x939EE0` at its signed ends; for
every Dragon step the keys (the confirm and cancel masks single bits, the
pressed word one of them, 0x10, both, none or anything), the menu line
0..3 and with bit 7, the grid row 0..2 / 0xFF and column 0..6 (a gene's flag
is written through them: never past the grid), the gene count 0..4, the
picked genes 0..19 (written through), the flags, the eighteen slots (genes
0..17 or 0xFF, the fourth byte 0xFF a third of the time), both cursors with
their ends and -1, windows 4, 16..19, 21's in-use bit and +3, windows 18 /
19's x at 0x5B / 0xA3 and not, the menu actor's +5 0..2 and AP.

**Disturbance** (the group's case, from the hash only): the pressed word,
the confirm mask, the menu line, the grid's row and column, the gene count,
the message count, the prompt's answer, a cursor word, the drains' target
and actor, and `EnemyAI_ApplyAction`'s enemy's `+0x92`, `+5` and HP (read
again after `Battle_ClearStatus` and `0x453300`). The engine frame moves
the step bytes, `Sprite_Current`, the menu actor and the result record.

Results (this worktree, 2026-09-29; counts depend on the build directory):

    shadow      battle_e5 self-test: 312000 rounds over 52 functions (6000 each), 344778 calls to the stand-ins, 0 MISMATCHES; 36008 bytes of state (30 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'` in this worktree: exit 0 (headless, 2026-09-29, the tip of section 5), 950 totals lines, every one 0 mismatches, no Fatal; battle_e5's line in it: 345,316 calls (the count moves with the other groups' DLL state, as ever).

## 6. Controls

`BOF3X_BE5_ONLY=<clone>` with one change planted in ours at a time
(scratch `controls.py`: plant on a unique anchor, rebuild, run, restore; one
rebuild at the end), this worktree, 2026-09-29, the tip of section 5.
**32 planted, 32 refused by a count.** A first pass (before the louder pop-up
and sound stand-ins) left control 12 unrefused - the drains OR a flag into the
acting sprite's +8 and clear the byte after the pop-up, so the mark was wiped
before the compare; the pop-ups' stand-ins now note it - and refused 25 in
one round only (38 now).

| n | Clone | Planted | Refused in |
|--:|---|---|--:|
| 1 | `EnemyAI_OtherRowsDone` | OtherRowsDone: a row of 0x64, not 0x63, ends the test | 2,775 of 6,000 |
| 2 | `EnemyAI_SetRowDone` | SetRowDone: row & 7, not & 31 (rows 8..15 name a bit) | 1,188 of 6,000 |
| 3 | `EnemyAI_CondElement` | CondElement: mask bit 9, not 8, stops an attack | 10 of 6,000 |
| 4 | `EnemyAI_ApplyAction` | ApplyAction: the queue full at 7 | 426 of 6,000 |
| 5 | `EnemyAI_ApplyAction` | ApplyAction kind 2: +0x92 dropped from the status word | 371 of 6,000 |
| 6 | `EnemyAI_ApplyAction` | ApplyAction: 0x453300's word without the row pointer's upper bytes | 6,000 of 6,000 |
| 7 | `EnemyAI_ApplyAction` | ApplyAction kind 2: Sprite_Current not read again after Battle_ClearStatus | 15 of 6,000 |
| 8 | `EnemyAI_ScaleStat` | ScaleStat: AP capped at HP's maximum +0xD0 | 250 of 6,000 |
| 9 | `EnemyAI_SetAttrByte` | SetAttrByte: case 7 writes +0xE6 | 404 of 6,000 |
| 10 | `EnemyAI_DedupMessages` | DedupMessages: the action word not compared | 3,498 of 6,000 |
| 11 | `Effect_DrainHp` | Drain: the party target's stat / 4 | 262 of 6,000 |
| 12 | `Effect_DrainAp` | Drain: the flag when the sum reaches the maximum (>=) | 147 of 6,000 |
| 13 | `Effect_DrainHp` | Drain: the pop-up's actor not read again after 0x590E80 | 79 of 6,000 |
| 14 | `Effect_QuarterAttack` | QuarterAttack: below 0x21 | 65 of 6,000 |
| 15 | `BattleForm_ApplyStats` | ApplyStats: + 4, not + 5, before the division | 733 of 6,000 |
| 16 | `DragonCmd_MenuInput` | MenuInput: the line wraps above 3 | 76 of 6,000 |
| 17 | `DragonCmd_SlotsCursor` | MoveCursor: a + b up to sum_max (<=) | 7 of 6,000 |
| 18 | `DragonCmd_SlotAffordable` | Affordable: the cost below the AP (<) | 68 of 6,000 |
| 19 | `DragonCmd_GenesPick` | GenesPick: the done line needs AP above the cost (>) | 21 of 6,000 |
| 20 | `DragonCmd_GenesPick` | GenesPick: the count not read again after 0x4525B0 | 42 of 6,000 |
| 21 | `DragonCmd_GenesDispatch` | GenesDispatch: through the slots' table | 6,000 of 6,000 |
| 22 | `DragonCmd_Commit` | Commit: the command record's +1 = 5 | 5,029 of 6,000 |
| 23 | `DragonCmd_StoreAsk` | StoreAsk: the hand at 0xD6 | 2,504 of 6,000 |
| 24 | `DragonCmd_Open` | Open: 17 gene bits, not 18 | 4,023 of 6,000 |
| 25 | `DragonCmd_SlotsCursor` | SlotsCursor: Input_Pressed not read again after the refusal sound | 38 of 6,000 |
| 26 | `DragonCmd_Slots2Close` | Slots2Close: window 18's +0 cleared too (as SlotsClose) | 2,983 of 6,000 |
| 27 | `DragonCmd_StoreSource` | StoreSource: bit 0x10 not counted with confirm | 155 of 6,000 |
| 28 | `DragonCmd_StoreCopy` | StoreCopy: three bytes, not four | 4,999 of 6,000 |
| 29 | `Effect_DrainHp` | Drain (HP): the flag when the sum reaches the maximum (>=) | 137 of 6,000 |
| 30 | `DragonCmd_StoreSource` | StoreSource: Input_Pressed not read again after the refusal sound | 148 of 6,000 |
| 31 | `EnemyAI_ApplyAction` | ApplyAction kind 6: capped at 0xFFFE | 58 of 6,000 |
| 32 | `DragonCmd_MenuInput` | MenuInput: key 0x4000 picks line 2 | 1,245 of 6,000 |

## 7. Latent defects (Capcom's, kept)

- **`EnemyAI_DedupMessages` keeps its messages in an eight-dword stack
  buffer** with no bound: a ninth distinct message would be written onto
  its return address. `EnemyAI_ApplyAction`, the list's one writer here,
  stops at eight, so the list never holds nine; ours aborts at a ninth
  instead (never reached by the fuzz, which keeps the count at most 8).
- **The six Dragon dispatchers and `0x44FF00` index their tables with no
  bound.** Every step keeps `0x904AA4` inside its table; a byte past one
  would run the next table's steps (or, past `DragonCmd_StoreSteps`, jump
  to the data dword `0x00070700`). Ours aborts past each table.
- **`DragonCmd_Slots2Close` waits on window 18's in-use bit and clears
  windows 17, 16 and 19** - the second list is window 19; its twin
  `DragonCmd_SlotsClose` waits on 18 and clears 17, 18, 16, 19. Kept as
  read; whether window 18 can still be up when part 5 closes was not
  measured.
- **`EnemyAI_CondElement` indexes the ability rows by the whole word
  `0x904B80`** and the weapon table by the member's `+0x92`, unchecked;
  `Effect_Drain*` index the enemies by `target - 3` / `actor - 3` unchecked
  (past 10 the objects run off the image, a fault).
- **The slot pricers sum the costs in a byte**; the three largest of the 18
  cost bytes sum well below 256 (measured off the exe), so the sum cannot
  wrap with the table as shipped.
- **`EnemyAI_DedupMessages`' eax** is whatever its loop left (the caller's
  eax's upper bytes when the list is empty); `EnemyAI_TurnCheck` hands it
  on and its one caller (`0x436908`) loads eax again at once, so ours is
  `void`.

## 8. The rebinding

Every raw reference to a BE5 function in our files, rebound in the
round-ten form (the value unchanged, so the fuzz keys stand):
`battle_damage.cpp`'s `kOriginals` (`0x44B3A0`, `0x44B2E0`, `0x44B240`,
`0x44B920` -> `bof3::addr::EnemyAI_ApplyAction`, `_SetRowDone`,
`_OtherRowsDone`, `_DedupMessages`) and `battle_misc.cpp`'s (`0x44B320`,
`0x44B3A0`, `0x44B2E0`, `0x44B920`), with the two `_callees.h` comments.
**Left raw on purpose**: the fuzz files' keys (`battle_damage_fuzz.cpp`'s
and `battle_misc_fuzz.cpp`'s `case 0x44B...:` stand-ins and `CallSite`
rows) and comments that describe the original's calls by address. No raw
reference to a BE5 function lies in a file another group of this wave
writes.

## 9. For `analysis/calltrace/entries_logic.txt`

The 52 extents of section 1 (the code's, from the entry to the last `ret`
or tail `jmp`), appended to the main checkout's file on 2026-09-29 (49
lines; three were there already: `0x44B240 7E`, `0x44B2E0 32`,
`0x44B320 7E`). The hosts `0x44B3A0 645`, `0x44FDE0 726`, `0x450510 848`
and `0x450D60 73B` are cut by the smaller extents added for them
(`240`, `112`, `82`, `82`); `Effect_ApplyResult`'s `0x44B9F0 750` by the
three effect slots' lines.

## 10. The live route

`tools/recipes/dragonTransform.txt` (the owner's, 2026-09-28) under the
reach switch (`analysis/calltrace/reach_dragon/`): **`BattleForm_ApplyStats`
entered at frame 2,731** (from BE3's `0x442310`, return `0x4423EE`), and
two of the Dragon run's steps shown by their callees' return addresses,
since their starts were not listed then: **`DragonCmd_Open`** (it called
`0x4525B0` at frame 976, return `0x450054`) and **`DragonCmd_GenesClose`**
(it called `0x447F40` at frame 1,678, return `0x450A7E`) - so the route
passed through parts 1 and 4, and with them `DragonCmd_GenesDispatch` and
`DragonCmd_Commit` at least (the only way to part 4's close and out). With
this group's lines in `entries_logic.txt` the coordinator's live check will
say which of the 52 it enters; the AI helpers and the effect slots depend
on the encounter's enemies and are not claimed.

## 11. What nothing reached

- **Ours' three aborts**: `EnemyAI_DedupMessages`' ninth distinct message
  (the seed keeps the list at 8 at most, as its writer does) and the
  dispatchers' step byte past a table (the harness draws it below each
  table's length). Both would fault or run foreign code on Capcom's side.
- **Enemy indexes past 10** (`EnemyAI_OtherRowsDone`'s enemy, the drains'
  target and actor): the objects at `0x93B960 + 0x128 n` run off the image
  (`0x93F000`) at n = 11; the seeds keep inside, so the fuzz never compares
  a fault.
- **The cost sum's byte wrap** in the pricers (section 7: the table as
  shipped cannot reach it; random bytes stand for the table's cells only
  through the gene bytes 0..17 and 0xFF).
- Every one of the 52 was called 6,000 times and every recorder of the
  listing was reached (section 5's coverage line); the rarest branches the
  controls measure are `EnemyAI_CondElement`'s attack path with mask bit 8
  (control 3) and `MoveCursor`'s `a + b` at its bound (control 17).
