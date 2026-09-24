# The battle task and the turn flow

**Status:** IN PROGRESS (2026-09-24). Twenty-one functions are ours
(`src/game/battle_flow.cpp`). Each is fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 21,000 rounds, 0 mismatches.
92 negative controls were planted: 91 were refused by a count, and one was a
change that changes nothing. **Not yet through a live check.** That check is
the coordinator's combat A/B after the merge.

This is group BB of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)). Every function is a
*faithful* replacement, so no `DIVERGENCE.md` entry is owed. One Capcom
defect was found (section 5). Only the PC port has it, and it is kept.

The battle engine is the PSX's `BATTLE.EMI` overlay compiled into the exe.
The RAM layout was read against the sibling's `docs/BATTLE_RAM.md` and
[`kinship-probe-battle-engine.md`](kinship-probe-battle-engine.md):

- **Enemy objects:** PC `0x93B960` + `0x128` n, PSX `0x801EB5A0` + `0x118` n.
  The working record sits at `+0x80` of each object.
- **Current enemy:** PC `0x939AD8`, PSX `0x801EB458`.
- **Sprite_Current:** PC `0x937F88`, PSX scratchpad `0x1F800044`.
- **Battle totals:** PC `0x904AA0..`, PSX `0x801462E0..`.

## 1. What is ours

Each extent was read to its last instruction by recursive descent (capstone,
2026-09-23). The calls count is from the combat route
(`analysis/calltrace/recipe_combat`). PSX twins come from
`analysis/pairs_propagated.json` unless the table says "read". A twin marked
"read" was also disassembled from the sibling's overlay capture and compared.

| Function | Entry | Bytes | PSX | Combat calls |
|---|---|--:|---|--:|
| `Battle_PhaseDispatch` | `0x42E400` | 0x62 | none paired | 2,091 |
| `BattleTask_RunAll` | `0x435110` | 0x61 | none paired | 2,091 |
| `BattleTask_Create` | `0x435180` | 0x6A | `0x801E584C` (read, the same) | 52 |
| `BattleTask_FreeCurrent` | `0x4351F0` | 0x65 | `0x801E58C8` (read) | 41 |
| `BattleTask_ClearAll` | `0x435260` | 0x35 | `0x801E5978` (read, the same) | 1 |
| `BattleEnemy_RunAll` | `0x435830` | 0x6A | none paired | 2,091 |
| `BattleEnemy_UpdateScreenAll` | `0x4358A0` | 0x2E | none paired | 2,091 |
| `BattleEnemy_SetAnimation` | `0x4358D0` | 0x134 + a 4-entry table | `0x801E2298` | 82 |
| `BattleEnemy_ScriptTick` | `0x436090` | 0x29 | `0x801E3070` (read, the same) | 2,042 |
| `BattleEnemy_ScriptTickOnce` | `0x4360C0` | 0x29 | `0x801E30D8` (read, the same) | 16 |
| `BattleEnemy_Chance70` | `0x436B50` | 0x6F | `0x801E42C0` | 4 |
| `Battle_EnemyDefeated` | `0x437470` | 0x105 | `0x801E542C` (read: same terms, same order) | 1 |
| `Battle_RollDrops` | `0x437580` | 0x117 | `0x801E525C` (read; **differs**, section 5) | 1 |
| `Battle_StartItemMagic` | `0x437780` | 0x46 | `0x800AB188` | 1 |
| `Magic_LoadForItem` | `0x4377D0` | 0x43 | `0x800AB1FC` | 1 |
| `Battle_StartAbilityMagic` | `0x437930` | 0x9B | `0x800AB038` | 3 |
| `Magic_LoadForAbility` | `0x4379D0` | 0x35 | `0x800AB120` | 3 |
| `BattleFx_RollingDigits` | `0x432F10` | 0x7A | `0x801E61FC` | 98 |
| `Battle_DrawNumber` | `0x444480` | 0x119 | `0x801D9654` (gap tier) | 343 |
| `Battle_DrawLabel` | `0x4445A0` | 0xB2 | `0x801D97D4` (gap tier) | 37 |
| `Battle_ActorIsOut` | `0x4456C0` | 0x66 | `0x801DB4EC` (read, the same) | 10,594 |

**Every one is reached by the combat route**, so the combat A/B covers them
all. The fuzz alone covers:

- the event-battle paths (`0x904AAA` non-zero);
- task-slot exhaustion (index `0xFF`);
- a drop that lands and merges;
- the second task of `Battle_StartAbilityMagic`;
- a negative digit phase.

What each function does is written above it in `battle_flow.cpp`. The short
version:

- **The battle frame.** `0x42E2F0` calls `PhaseDispatch`, `EnemyRunAll`,
  `TaskRunAll` and `UpdateScreenAll` once a frame.
  - `PhaseDispatch` calls entry `0x904AA0 & 0xFF` of six phase handlers
    that the original builds on its stack: `0x42E470`, `0x42E990`,
    `0x42F070`, `0x42F220`, `0x4302B0`, `0x4311E0`.
  - `TaskRunAll` runs each non-zero one of the 48 task slots (`0x93A000`,
    `0x84` bytes each) by its kind `+6`. The four kinds are on a stack table:
    `0x4352A0`, `0x435350`, `0x4378B0`, `0x4357D0`.
  - `EnemyRunAll` runs each active enemy's state `+0x100` through the
    `.data` table `0x64B084`.
- **Stack tables and bad indices.** The original checks neither table index.
  An index past the end would call through its own stack. Ours aborts
  (rule 4), as mode_flow's `Transition_Task` does.
- **The event battle.** When the byte `0x904AAA` is non-zero, two things
  change:
  - `EnemyRunAll` calls the enemy's `+0xF4` hook with 2 (when its `+1` is
    set). It then uses `0x64B088`, the same table one entry on; that table's
    state-0 entry is a null.
  - `PhaseDispatch` calls the hook stored at `0x904B6C` with 3, **but only
    when the phase is not 0** (section 2).

  The name "event battle" is a guess. `0x904AAA` is compared with 0x10,
  0x19, 0x1A and 0x25 elsewhere, and the hooks it enables are stored by
  `0x437CC0`.
- **`BattleEnemy_ScriptTick` / `ScriptTickOnce`** are the enemy state's
  animation tick. They answer 1 while the enemy is stunned or dead (`+0x92
  & 0x44`), or gated by `0x904B8E`. Otherwise they tail-jump to
  `Sprite_ScriptTick` or `Sprite_ScriptTickOnce`.
- **`BattleEnemy_SetAnimation`** takes the animation byte from the table at
  `+0xFC`, one on for modes 2 and 3. It passes that byte to
  `Sprite_EnsureAnimation` and sets the flip byte `+0x2A` from its bit 7.
- **`BattleEnemy_Chance70`** has five gates, then bit 15 of `+0x110`, then
  `Rand() % 100 < 70`. What its callers decide with it is unread; the name
  is a hypothesis.
- **The magic pair.** `Magic_LoadFor*` load the magic file of the item or
  ability's row and set `0x904AA9 |= 4`. `Battle_Start*Magic` start a kind-2
  task with that row, owned by the argument. When the ability record's
  `+0xD & 0x10` is set, a second task (kind 1, parameter 0x43) is started
  and pointed at the first.
- **`Battle_DrawNumber`** does `sprintf("%3d")` and then draws one 8 x 8
  SPRT per non-space character.
- **`Battle_DrawLabel`** draws one 24 x 8 cell.
- **`BattleFx_RollingDigits`** is a task's cycling digits: `+0x32` mod 10,
  with `+0xA` + 1 digits drawn through `Battle_DrawNumber`.
- **`Battle_ActorIsOut`** is the "absent or dead" test. The sibling named its
  PSX twin `Battle_ActorFrameUpdate` (hypothesis); the code is this test.

## 2. Found by the fuzz

The first run mismatched in 70 rounds, all in `Battle_PhaseDispatch`. The
original has `test al, al` twice. The second test comes after `mov eax,
[0x904AA0]` has replaced `al`, so it tests the phase's low byte. As a result,
phase 0 never runs the event hook. Ours has done the same since then, and
control P1 guards it.

Nothing else differed.

## 3. Found on the way

- **`0x4360C0` is 41 bytes.** The round list described it as "recursive,
  calls Battle_ApplyDamage and Effect_ApplyResult". That came from the
  extent `pe_funcs.py` gave it, which ran on into unlisted functions from
  `0x4360F0` on.
  - Those functions are the real callers. `0x4360F0` is the state-0 entry of
    `0x64B084`, and it tail-jumps through `0x64B1A0` by `+1`.
  - `entries_logic.txt` lists `004360C0 A8C`. It should be `29`, and the
    code after it needs entries of its own.
  - The same kind of overrun affects four more lines (section 7).
- **`0x42E2F0` is not in `symbols.toml`.** The round's task text called the
  battle frame "already ours", but no entry names it. It is in
  `entries_logic.txt` (`104`).
- **Other groups' functions**, reached through raw addresses in
  `battle_flow_callees.h` and not bound here:
  - `0x446650` (BE): `Battle_RemoveFromTurnOrder`. It is the PSX
    `0x801DD114` by the sibling's decomp of `Battle_EnemyDefeated`, and reads
    its actor as a byte.
  - `0x494ED0` (BG): ORs bit `n & 0x1F` into the dword at
    `0x904068 + (n >> 5) * 4`, reading `n` as a word. The enemy's flag
    `+0x8C` goes there on a kill; the PSX engine function is `0x800A9FA4`.
  - `0x446FD0` (BF): `word 0x904B82 &= ~(1 << n)`.
- **The item magic-row table** `0x64B274` holds four pointers:
  - The first three match the PSX sub-tables' spacing (100, 92 and 72
    bytes).
  - The fourth is `0x675ED8`, not `0x64B38C`. The port moved or grew
    category 3 somewhere else. Why was not looked into.
- **Latent, as the original has them:**
  - Neither magic starter tests `BattleTask_Create`'s `0xFF`. With all 48
    slots taken, the owner goes to `0x9423FC` (the 256th slot's `+0x80`),
    past the array. The PSX's `0x801E584C` also returns `0xFF` untested.
  - `Battle_ActorIsOut` does not check the enemy index.
  - `Battle_RollDrops` indexes its eight-byte chance table on the stack by
    an unchecked class. Ours aborts above 7.
  - `Battle_DrawNumber` draws the first character before its NUL test. The
    CRT's "%3d" is never empty, so this cannot show in game.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_flow`
(`src/game/battle_flow_fuzz.cpp`). It makes 21 byte-copies:

- Every call and tail jump out is re-aimed at a recorder, using `CloneCall`
  with `expected`.
- Both stack tables' immediates are checked and then re-aimed in the copies.
- `SetAnimation`'s jump table (`0x435A04`) is relocated into its copy.

It also swaps these for recorders, and puts every one back afterwards:

- the first nine entries of `0x64B084`;
- the event hook `0x904B6C`;
- every enemy's `+0xF4`.

Every enemy's `+0xFC` points into one table of its own, at a different
offset per enemy.

**Each round** starts from random bytes over the following regions (15 KB):

- the task slots and the enemy objects;
- the pointers `0x937F88` and `0x939AD8`;
- the battle globals `0x904AA0..0x904D00`;
- ObjTrio;
- the item rows, the ability rows, the magic files and the ability records.

The pointers and indices are then put back inside their arrays. Each branch's
boundaries are seeded:

- the phase;
- slot fill (none free, one free, the 47th);
- event on or off;
- modes 0..4 and animation `0xFE` / `0xFF`;
- every `Chance70` gate opened, then one shut again;
- `Rand` values:
  - at 69 / 70 / 71 / 99 / 100 and their negatives;
  - on a drop class's chance and one above it;
- drop counts 0, 14, 15 and 16, with list items equal to the drop;
- files `0xFFFF`;
- digit counts `0xFF` / 0 / `0x80` and phases −10..10;
- text:
  - "%3d" of the value;
  - short random strings, with spaces;
  - empty strings with bytes after the NUL.

**The recorders are not quiet.** Each one moves a byte that its caller reads
again after the call:

- the phase, after the event hook;
- `0x939AD8`, after an enemy hook and after `Rand`;
- the animation table's bit 7 and `Sprite_Current`, after
  `Sprite_EnsureAnimation`;
- `0x904B80`, after `BattleTask_Create`;
- the text, after `Gpu_GetClut`.

A general disturber also moves one of 20 watched bytes three calls in four.
In addition:

- The commit recorder moves the packet cursor.
- `Gpu_GetTPage`'s recorder returns a full dword.
- `Gpu_SetSprt`'s recorder hashes the primitive as the caller left it.

Result (2026-09-24), `BOF3X_SELFTEST_ONLY=1`:

    battle_flow self-test: 21000 rounds over 21 functions (1000 each), 68726 calls to the stand-ins, 0 MISMATCHES
    coverage: phases 6 of 6, task kinds 4 of 4, enemy state entries 9 of 9, event hook 421, enemy hook 1013,
      screen updates 4014, animations 646; ticks 708 / 725 (early 1: 292 / 275 rounds), chance 1 281 0 719
      (rand reached in 333); created none 332; kills: battle end 165, flag cleared 375; drops rolled 966,
      merged 93, appended 399; files 998, second tasks 517; digits drawn 1924; texts drawn 989 (with a space
      99), sprites 4527; actor out 371 in 1000

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 821 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`controls.py` in the session scratchpad)
planted each one alone: replace, build, self-test, revert. The table gives
the rounds that refused each one, of 1,000.

| | Planted | Refused in |
|---|---|---|
| P1 | PhaseDispatch: the hook in any event battle, phase 0 too | 73 |
| P2 | PhaseDispatch: the phase not re-read after the hook | 176 |
| P3 | PhaseDispatch: the hook called with 2 | 421 |
| T1 | TaskRunAll: a slot run only with bit 0 | 1000 |
| T2 | TaskRunAll: 0x93B940 not stored | 1000 |
| T3 | TaskRunAll: Sprite_Current not set | 1000 |
| C1 | TaskCreate: a slot free only when its byte is 0 | 665 |
| C2 | TaskCreate: kind and parameter swapped | 666 |
| C3 | TaskCreate: 47 slots | 160 |
| F1 | FreeCurrent: +0x48 kept | 997 |
| F2 | FreeCurrent: +7 zeroed too | 997 |
| A1 | ClearAll: +0x5F kept | 1000 |
| A2 | ClearAll: 47 slots | 1000 |
| E1 | EnemyRunAll: the event path through 0x64B084, not 0x64B088 | 542 |
| E2 | EnemyRunAll: the event state from the loop object, 0x939AD8 not re-read | 294 |
| E3 | EnemyRunAll: the hook whatever byte +1 | 461 |
| E4 | EnemyRunAll: seven enemies | 482 |
| E5 | EnemyRunAll: 0x904AAA read once, before the loop | 76 |
| U1 | UpdateScreenAll: 0x939AD8 not set | 993 |
| U2 | UpdateScreenAll: an enemy active by bit 0 | 894 |
| S1 | SetAnimation: mode 3 not one on | 168 |
| S2 | SetAnimation: mode 2 flips inverted | 158 |
| S3 | SetAnimation: bit 7 read before the call | 318 |
| S4 | SetAnimation: the byte passed with bit 7 | 320 |
| S5 | SetAnimation: the flip to the Sprite_Current of before the call | 279 |
| S6 | SetAnimation: modes above 3 as mode 3 | 354 |
| K1 | ScriptTick: status 0x40 only | 75 |
| K2 | ScriptTick: the +0x114 test inverted | 184 |
| K3 | ScriptTickOnce: Sprite_ScriptTick instead | 725 |
| K4 | ScriptTickOnce: the status byte +0x93 | 500 |
| H1 | Chance70: status mask 0x4060 | 88 |
| H2 | Chance70: the turn gate at 4 | 77 |
| H3 | Chance70: magic id 0xA0 | 114 |
| H4 | Chance70: bit 14 of +0x110 | 193 |
| H5 | Chance70: below 71 | 27 |
| H6 | Chance70: Rand taken unsigned | 47 |
| H7 | Chance70: +0x90 bit 0 | 225 |
| H8 | Chance70: the animation gate left out | 47 |
| D1 | EnemyDefeated: the zenny yield not zeroed | 1000 |
| D2 | EnemyDefeated: the dead bit on +0x92 | 709 |
| D3 | EnemyDefeated: the battle ends at one left | 316 |
| D4 | EnemyDefeated: the round flag cleared whatever the actor | 385 |
| D5 | EnemyDefeated: +0x110 and 0xFFFFFF80 | 912 |
| D6 | EnemyDefeated: state 3 | 1000 |
| D7 | EnemyDefeated: drops rolled before the turn order | 1000 |
| D8 | EnemyDefeated: the flag number from Sprite_Current | 846 |
| D9 | EnemyDefeated: the EXP from +0x94 | 1000 |
| R1 | RollDrops: chance class 3 is 4 | 6 |
| R2 | RollDrops: a drop only below the chance | 112 |
| R3 | RollDrops: the search stops at the first match | 29 |
| R4 | RollDrops: the PSX's, with no append after a match | 63 |
| R5 | RollDrops: the list grows to 16 | 63 |
| R6 | RollDrops: 0x939AD8 not re-read after Rand | 300 |
| R7 | RollDrops: the item word not zeroed after the append | 331 |
| I1 | StartItemMagic: a task of kind 1 | 1000 |
| I2 | StartItemMagic: the id not stored | 479 |
| I3 | ItemRow: the next byte | StartItemMagic 995, LoadForItem 997 |
| I4 | StartItemMagic: the owner at +0x7C | 1000 |
| L1 | LoadMagicRow: flag 8 | LoadForItem 356, LoadForAbility 363 |
| L2 | LoadMagicRow: 0xFFFF loaded too | LoadForItem 511, LoadForAbility 491 |
| L3 | LoadMagicRow: rows 4 bytes apart | LoadForItem 996, LoadForAbility 995 |
| B1 | StartAbilityMagic: flag 0x20 | 486 |
| B2 | StartAbilityMagic: the record by the argument, 0x904B80 not re-read | 267 |
| B3 | StartAbilityMagic: the second task's owner the caller's | 517 |
| B4 | StartAbilityMagic: the second task parameter 0x44 | 517 |
| B5 | StartAbilityMagic: +0xB the first index | 516 |
| B6 | StartAbilityMagic: records 16 bytes apart | 493 |
| Q1 | LoadForAbility: the next row byte | 992 |
| G1 | RollingDigits: the phase mod 9 | 562 |
| G2 | RollingDigits: x - 8n - 11 | 608 |
| G3 | RollingDigits: the digit from the phase as u16 | 267 |
| G4 | RollingDigits: digit 0 not drawn | 608 |
| G5 | RollingDigits: the slot not re-read for each digit | 45 |
| N1 | DrawNumber: a while loop (NUL tested first) | 119 |
| N2 | DrawNumber: the pen kept on a space | 114 |
| N3 | DrawNumber: the digit not stored back | 989 |
| N4 | DrawNumber: u taken before Gpu_GetClut | 848 |
| N5 | DrawNumber: v 0xD8 | 989 |
| N6 | DrawNumber: the value not masked | 1000 |
| N7 | DrawNumber: the tpage not masked | 1000 |
| N8 | DrawNumber: y as u16 | 506 |
| W1 | DrawLabel: u from 0x60 | 1000 |
| W2 | DrawLabel: 16 wide | 1000 |
| W3 | DrawLabel: the CLUT row from the cell | 995 |
| W4 | DrawLabel: x and y swapped | 1000 |
| O1 | ActorIsOut: member records 0x140 apart | 93 |
| O2 | ActorIsOut: a member's status byte +0x93 | 118 |
| O3 | ActorIsOut: enemy index actor - 2 | 427 |
| O4 | ActorIsOut: actor 3 a member | 62 |
| G6 | RollingDigits: the slot pointer not re-read between the step and the mod | **not refused; no input could tell** |

**G6** changes nothing. No call separates the two reads, so both see the
same slot. It is not counted.

**Five controls were blind in the first run** and are refused now: P2 (19
rounds, then 176), E2 (18, then 294), S3 (7, then 318), B2 (20, then 267)
and R6 (20, then 300). Each became visible once the recorder it depended on
was given the side effect its caller reads back. N4 went from 17 to 848 the
same way, and R1 went from 2 to 6 once rolls were seeded on the chances.

**The thinnest now:** R1 (6), H5 (27), R3 (29), G5 (45), H6 (47) and H8 (47).

**What the fuzz cannot see:**

- anything the callees really do;
- what the six phases, the four task kinds and the enemy states do;
- the order of stores with no call between them (`FreeCurrent`'s eleven
  bytes, `TaskRunAll`'s three stores);
- the argument slots the original overwrites (`Battle_StartAbilityMagic`
  stores the indices into its argument 1, `Battle_DrawNumber` uses
  argument 4 as a temporary; callers never read them);
- the upper 16 bits `BattleFx_RollingDigits` leaves on the x and y it pushes.
  `Battle_DrawNumber` reads only the words, and the recorder records only
  the words.

## 5. Defect: a merged drop also appends an item 0

This is for [`known-defects.md`](known-defects.md); the coordinator assigns
the number.

**What the PSX does.** `Battle_RollDrops` (`0x801E525C`, read 2026-09-23)
searches the result list for the dropped item. On a match it adds one to that
entry's count, zeroes the enemy's item word, and jumps past the append
(`j 0x801E53F0`).

**What the PC port does.** `0x437580` has no such exit.

1. A match counts one and zeroes the item word, as on the PSX.
2. The search then goes on, comparing the remaining entries with the zeroed
   word, which is 0. Any item-0 entries after the match are counted too.
3. After the loop, the zeroed word is appended as a new entry with count 1.

**The effect.** Two drops of the same item in one battle leave an extra
(item 0, count 1) entry in the list (`0x904AF4` / `0x904B14`). Each such
entry also uses up one of the list's 15 entries.

**Latent.** What the results screen does with item 0 was not read. No drop
landed on the combat route, and a merge needs two identical drops in one
battle.

**Kept, as the original has it.** Control R4 plants the PSX's behaviour and
is refused in 63 rounds, so the difference is observable.

## 6. For the batch check

The combat A/B `analysis/validate_combat.sh` reaches all 21 functions. The
kill frame is the only one that reaches `Battle_EnemyDefeated` and
`Battle_RollDrops`. No drop lands on that route (the sibling's Gunhead has 4
in 256 and 1 in 256). The event-battle paths, a full task table and a merged
drop remain fuzz only.

`BOF3X_ORIGINAL` with all 21 names adds about 500 characters to the list.

## 7. Corrections for `analysis/calltrace/entries_logic.txt`

The merger should apply these:

| Line now | Should be |
|---|---|
| `0042E400 15C9` | `0042E400 62` |
| `00432F10 E47` | `00432F10 7A` |
| `00435260 5C6` | `00435260 35` |
| `004358D0 145` | `004358D0 144` (the body 0x134 and its table 0x10) |
| `004360C0 A8C` | `004360C0 29` |
| `00436B50 8F8` | `00436B50 6F` |
| `004379D0 2F0` | `004379D0 35` |

Every other entry of the group is already listed with its right size.
Cutting the seven over-long lines leaves the code after them unlisted. That
includes the enemy state handlers from `0x4360F0` on and the phases from
`0x42E470` on; the merger should judge what to add.

## 8. Open

- What the six battle phases, the four task kinds, the enemy states and the
  event hooks (stored by `0x437CC0`) do.
- What `0x904AAA`'s values mean.
- What `BattleEnemy_Chance70` decides.
- What the result screen does with an item-0 entry (section 5).
