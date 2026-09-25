# The action phases

**Status:** IN PROGRESS (2026-09-25). Twenty-three functions are ours
(`src/game/battle_actions.cpp`): the battle's phase 3 and every function its
step tables reach that the round listed. Each is fuzzed headless against a
copy of Capcom's with every call re-aimed at a recorder: 23,000 rounds, 0
mismatches. 118 negative controls were planted: 117 were refused by a count,
and one (M2) is a change that changes nothing.

This is group CB of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Every function is a
*faithful* replacement, so no `DIVERGENCE.md` entry is owed. Two latent
defects of the original were found (section 3); both are kept. The model is
[`battle_flow.md`](battle_flow.md), whose `Battle_PhaseDispatch` calls
`0x42F220` as its phase 3.

## 1. The shape: one phase, seven step tables

`0x42F220` is not jumped to: `Battle_PhaseDispatch` calls it. It **calls**
through the first table; every other table is reached by a 14-byte stub
(`xor eax, eax; mov al, [step byte]; jmp [eax*4 + table]`), so the stubs'
targets run in `0x42F220`'s call, and all 23 are `void (void)` (cdecl, no
argument read; the prototype is `0x42F220`'s call site). The seven tables are
named in `symbols.toml` (`[[data]]`, `unsigned long`, the counts below).
Ours reads each table in `.data` afresh at each dispatch, as the stub does,
so an index past the count goes where the original's goes.

| Table | At | Count | By | Entries |
|---|---|--:|---|---|
| `BattleAction_Steps` | `0x64AE80` | 5 | `0x904AA1` (called) | `0x42F250` `0x42F500` `0x42FC50` `0x42FDD0` `0x430010` |
| `BattleAction_BeginSteps` | `0x64AE94` | 2 | `0x904AA2` | `0x42F260` `0x42F4C0` |
| `BattleAction_KindSteps` | `0x64AE9C` | 6 | `0x904AA2` = the command kind | `0x42F510` `0x42F5B0` `0x42F510` `0x42F5E0` `0x42F670` `0x42FAF0` |
| `BattleAction_AbilitySteps` | `0x64AEBC` | 3 | `0x904AA3` | `0x42F680` `0x42F880` `0x42FAB0` |
| `BattleAction_ItemSteps` | `0x64AF08` | 3 | `0x904AA3` | `0x42FB00` `0x42FBD0` `0x42FC10` |
| `BattleAction_EffectSteps` | `0x64AF14` | 3 | `0x904AA2` | `0x42FC60` `0x42FD20` `0x42FD90` |
| `BattleAction_AfterSteps` | `0x64AF20` | 3 | `0x904AA2` | `0x42FDE0` `0x42FE20` `0x42FF70` |

No code bounds any of them. The counts are where the next reader's table
starts: `0x64AEB4` (two entries, `0x42F5F0` `0x42F640`) is `0x42F5E0`'s,
`0x64AEC8..0x64AF07` are `0x42F9D0`'s two 32-byte id tables, `0x64AF2C` is
group CC's. `KindSteps`' six follow from its reader: the kind is 0..5 (kind 4
is the ability by `BattleAction_EffectPreload`'s test, 5 the item), and
nothing else reads `0x64AEAC` / `0x64AEB0`.

The flow of one action, by the step bytes (`0x904AA0` phase, `0x904AA1`
step, `0x904AA2` / `0x904AA3` sub-steps):

- **Step 0** - `Battle_BeginAction` takes the next actor out of the turn
  order and fills the acting block; `BattleAction_EnterKind` moves to step 1
  with `0x904AA2` = the kind. None left: phase 4.
- **Step 1** - by kind: 0 and 2 `BattleAction_KindPlain`, 1
  `BattleAction_KindOne`, 3 `0x42F5E0` (no group's), 4 the ability's three
  steps, 5 the item's three. Kinds 0 and 2 end the action at once (back to
  step 0); kind 1 and the ability and the item go to step 2.
- **Step 2** - the effects: `EffectWait` until nothing is pending,
  `EffectPreload`, `EffectLoaded`; then step 3.
- **Step 3** - `AfterSettle`, `0x42FE20` (no group's), `EnemyMessages`; then
  step 4.
- **Step 4** - `BattleAction_End`: back to step 0, or phase 4 when the battle
  has ended, or (round flags bit 6) the actor and target swapped for one
  more action of kind 1.

What the kinds are in game terms was not read and is not guessed here.

## 2. What is ours

Each extent was read to its last instruction (capstone, linear, every jump
checked internal, 2026-09-25). PSX twins are the catalogue's pairs
(`analysis/pairs_propagated.json` via `remaining_catalog.tsv`), not read.

| Function | Entry | Bytes | Calls out | PSX (pair) |
|---|---|--:|---|---|
| `Battle_ActionPhase` | `0x42F220` | 0x2C | table | `0x801D2AA0` |
| `BattleAction_BeginStep` | `0x42F250` | 0xE | table | `0x801D2B14` |
| `Battle_BeginAction` | `0x42F260` | 0x257 | `0x453FA0`, `0x435AB0` | the sibling's `Battle_BeginAction` |
| `BattleAction_EnterKind` | `0x42F4C0` | 0x3A | hook | `0x801D2FB8` |
| `BattleAction_KindStep` | `0x42F500` | 0xE | table | `0x801D300C` |
| `BattleAction_KindPlain` | `0x42F510` | 0x96 | 4 | `0x801D3048` |
| `BattleAction_KindOne` | `0x42F5B0` | 0x2E | 1 | `0x801D3160` |
| `BattleAction_AbilityStep` | `0x42F670` | 0xE | table | `0x801D32C0` |
| `BattleAction_AbilityCheck` | `0x42F680` | 0x1F6 | 13 | `0x801D32FC` |
| `BattleAction_AbilityCommit` | `0x42F880` | 0x149 | 3 | `0x801D360C` |
| `BattleAction_AbilityStart` | `0x42FAB0` | 0x3C | 2 | `0x801D399C` |
| `BattleAction_ItemStep` | `0x42FAF0` | 0xE | table | `0x801D3A08` |
| `BattleAction_ItemCheck` | `0x42FB00` | 0xC1 | 6 | `0x801D3A44` |
| `BattleAction_ItemCommit` | `0x42FBD0` | 0x35 | 2 | `0x801D3B94` |
| `BattleAction_ItemStart` | `0x42FC10` | 0x3D | 2 | `0x801D3BFC` |
| `BattleAction_EffectStep` | `0x42FC50` | 0xE | table | `0x801D3C68` |
| `BattleAction_EffectWait` | `0x42FC60` | 0xB5 | 3 | `0x801D3CA4` |
| `BattleAction_EffectPreload` | `0x42FD20` | 0x6F | 1 | `0x801D3E60` |
| `BattleAction_EffectLoaded` | `0x42FD90` | 0x3D | 2, hook | `0x801D3F4C` |
| `BattleAction_AfterStep` | `0x42FDD0` | 0xE | table | `0x801D3FCC` |
| `BattleAction_AfterSettle` | `0x42FDE0` | 0x38 | 1 | `0x801D4008` |
| `BattleAction_EnemyMessages` | `0x42FF70` | 0x9A | 2 | `0x801D4284` |
| `BattleAction_End` | `0x430010` | 0x195 | 2, hook x3 | `0x801D43BC` |

The globals, as the code uses them (the names are ours, from the reads):

- **The two blocks.** `0x904B34` and `0x904B44`, 16 bytes each, the same
  layout: +0 the actor (0..2 a member of ObjTrio `0x802D40` stride 0x14C,
  3..10 an enemy at `0x93B960` + 0x128 (a - 3); the target byte may be a
  side, 0x40 / 0x80 / 0xC0), +1 the command kind (the acting block's), +2 /
  +4 / +6 screen words, +8 the object, +0xC its action record (a member's
  +0x124, an enemy's +0x104; the u16 at +2 the action id). The screen words
  are the object's +0x2E / +0x30 plus an s8 pair - a member's from
  `0x64DF70` at 2 * ((+8) + (+0x89) * 4), an enemy's own +0xF2 / +0xF3 - and
  +0x32.
- **The stat copy.** 32 bytes of a member's +0xA0 / an enemy's +0xB0 go to
  `0x939FE0` at each action's start (and at the swap). The sibling's
  `BATTLE_RAM.md` names the PSX's the effective stat block that
  `Battle_BeginAction` copies to `0x801EC278`.
- `0x904B80` the action id in hand, `0x904B82` a bit per actor with an effect
  pending (`Battle_SetActorBit`), `0x904AA8` the round flags (bits 2, 4, 5, 6,
  10, 11, 12, 13 are touched here), `0x904AAA` the "event battle" byte whose
  hook `0x904B6C` is called with 1, 4, 0 and 5 at the points below
  (battle_flow.md names it; the name is a guess there too).

What each does (the comment above each function in `battle_actions.cpp` has
the order of reads and stores in full):

- **`Battle_ActionPhase`** calls the step; then, when a button of
  `Field_CancelButtons` is held (`Input_Held`), clears round-flag bit 4.
- **`Battle_BeginAction`** searches the turn order `0x904ACC` from
  `0x904AE2` below `0x904AE3` for a slot that is not 0xFF. The actor picks
  its action (`Battle_MemberAutoTarget(a)`, or `0x435AB0(a - 3)`), is stored,
  fills the acting block and the stat copy; a target of 0..10 fills the
  target block (a side or anything above 10 does not). `0x904AA2` and
  `0x904AE2` count one.
- **`BattleAction_EnterKind`**: the hook with 1; step 1 with `0x904AA2` =
  the kind (read after the hook).
- **`BattleAction_KindPlain`** (kinds 0, 2): `Battle_ClearActingFlags`; for
  kind 2 by an enemy, its name record into `Text_Records[0]` and a kind-2
  banner of system message 0x7F; then the next actor.
- **`BattleAction_KindOne`**: the pending bit, the object's state +1 = 4,
  step 2.
- **The ability** (`0x904AA3` 0..2):
  - `AbilityCheck`: the id to `0x904B80`. `Battle_ActionSuitsTarget`
    non-zero turns the action away (out of the turn order, the object's
    +1 = 2, +2 = 0, the next actor). Else the name banner (unless the id is
    0x97), then two refusals as a system banner: message 0x37 when the actor's
    status has bit 4 and the record's +0x15 bit 2; message 0x36 when the cost
    (`Skill_ApCost(object +5, id, 1)`, or `0x904B78` for 0x97; kept in
    `0x904B88`) is above the actor's AP (a member's +0x9A, an enemy's +0xA6).
    Else `0x904AA5` = 0, round-flag bit 5 when `Battle_PickFlag8Member`
    answers 0, and on.
  - `AbilityCommit` waits for round-flag bit 5 (`AbilityCheck` sets it when
    `Battle_PickFlag8Member` answered 0; otherwise other code must - not
    looked for), clears it, the object's +1 = 7, +2 = 0, the
    pending bit, `0x904B8D` = the id; for the ids 0x24, 0x25 and 0x8C a new
    target from `0x42F9D0` (which also picks the id again) fills the target
    block; the magic file (`Magic_LoadForAbility`).
  - `AbilityStart` waits for `File_LoadDone`, then
    `Battle_StartAbilityMagic(the record's +2 byte, the object)` and step 2.
- **The item** (`0x904AA3` 0..2): `ItemCheck` as the ability's first half -
  turned away by `Battle_ItemSuitsTarget` (the queued item goes back first,
  `Battle_ReturnQueuedItem`), else the item's name (`Item_NamePtr(+3, +2)`)
  as a kind-1 banner; `ItemCommit` the object's +1 = 0xC, the pending bit,
  `Magic_LoadForItem`; `ItemStart` waits for the file, then
  `Battle_StartItemMagic` and step 2.
- **`EffectWait`**: each actor, members then enemies, with flags bit 6 (a
  member's +0x130, an enemy's +0x110) and state +1 not 6 gets +1 = 6, +2..+4
  = 0 and its pending bit. Then on when `Battle_AnyFlagF0` answers 0, nothing
  is pending and round-flag bit 2 is set.
- **`EffectPreload`**: for kind 4 or 5 whose magic row has a file,
  `LoadDatFile(0xD0)`; on. The ability row is indexed by the whole u16
  (`Magic_LoadForAbility` uses the low byte).
- **`EffectLoaded`** waits for the file: the hook with 4, round flags lose
  0x400, step 3, `Gfx_ClutStripCopyRow(0x1A)`.
- **`AfterSettle`**: with bit 2 and nothing pending, `Battle_SettleFlag8`
  non-zero goes on to `0x42FE20`; zero skips it (`0x904AA2` = 2).
- **`EnemyMessages`**: unless `0x939F60` is set, one message a call from the
  4-byte list `0x939FBC` (1-based by the count `0x93C2A2`: +0 an enemy index
  from 0, +2 a system message): the enemy's name into `Text_Records[0]` and
  `BattleQueue_Push(1, 0, Msg_SystemPtr(id))`; none left, step 4.
- **`End`**, with bit 2 and nothing pending: round flags lose 0x2804, step 0,
  the hook with 0; battle over (`0x904AE8`): phase 4 at step 2. Else
  `Battle_ClearActingFlags`, bit 12 cleared, and with bit 6 the two blocks
  swap, the new actor's kind is 1, its stat copy, a kind-1 banner of the text
  pointer `0x669E0C` (its flags' bit 15) or `0x669DF8`, and step 0 at
  `0x904AA2` = 1 - `EnterKind` next, so the new actor acts once more. Last,
  the hook with 5.

**Argument bits the originals leave as they fall.** Several calls push a
register whose low byte is the argument and whose upper bits are stale:
`Battle_MemberAutoTarget`'s is the `push ecx` slot of `0x42F260` (the
caller's `ecx`), `Battle_SetActorBit`'s, `Battle_RemoveFromTurnOrder`'s,
`Skill_ApCost`'s, `Item_NamePtr`'s, `Magic_LoadForAbility`'s and
`Magic_LoadForItem`'s carry whatever the register held, and
`Msg_SystemPtr`'s in `EnemyMessages` the upper half of the enemy record's
second dword. Ours passes the clean byte or word. Every one of those callees
reads only that byte or word (our sources for the ones that are ours;
`0x453FA0` and the five callees it hands the value on to by their
disassembly, 2026-09-25: each `and eax, 0xFF` or a byte compare). The fuzz
records the same masked values, so it cannot see these bits either way.

## 3. Found on the way

**Defect, latent: `BattleAction_AbilityCommit`'s target block has no upper
bound.** For the ids 0x24, 0x25 and 0x8C the new target is `0x42F9D0`'s
answer, and `0x42F880` fills the target block with `cmp al, 2; ja` to the
enemy path and no further test (`Battle_BeginAction` stops at 10:
`cmp al, 0xA; ja`). `0x42F9D0` answers a side - 0x40, 0x80 or 0xC0 - when
the picked id's record byte +0x10 (`0x65C4D8` + 24 id) has bit 4: read
2026-09-25, of the ids in its two tables (`0x64AEC8`, `0x64AEE8`) the ones
with that byte `0xF2` (`0x5C`, `0x5D`, `0x60`, `0x61`, `0x62`, `0x65`,
`0x66`) do, and for a member actor the answer is 0x40. The enemy path then
reads the s8 at `0x93BA52` + 0x128 * (0x40 - 3) = `0x9400DA`, past the end of
`.data` (`0x93D6EC`) and of the image (`0x93F000`), and stores pointers to
`0x93FFE8` / `0x9400EC` in `0x904B4C` / `0x904B50`. Whether that faults
depends on what is mapped there at run time, which was not looked at, and on
whether those three ids are ever used in play (not known here; ask the
owner). The PSX's twin was not read. **Kept, as the original has it** -
ours computes the same addresses (control M5 plants the bound and is refused).
For the coordinator's numbering; no D-number assigned.

**Latent: the "turned away" store writes a member's object by the formula
for any actor.** `AbilityCheck` and `ItemCheck` store +1 = 2 and +2 = 0 at
ObjTrio + 0x14C * actor without testing for a member, so an enemy actor
would write inside whatever follows ObjTrio (`0x803124`..). Whether an enemy
reaches kinds 4 and 5 was not settled: `0x435AB0` stores a two-bit kind
first (`and al, 3` into `0x904B35`), the rest of it was not read. Kept.

**Other groups' and no group's addresses** (called through raw addresses in
`battle_actions_callees.h`, never bound here):

- `0x42F5E0` (kind 3's stub, `jmp [0x64AEB4 + 0x904AA3 * 4]`) and its two
  targets `0x42F5F0` (`Battle_ClearActingFlags`, a banner of `0x669E08`,
  `Battle_SetActorBit`, state 9) and `0x42F640` (once `0x904B82` is clear:
  round-flag bit 2, step 3, both sub-steps 0), and `0x42FE20` (`AfterSteps` entry 1; calls
  `0x44A910` and `BattleQueue_Push`) are in **no group**: the combat route
  did not reach them (none is in the round's table). They are reached from
  this group's tables and are the natural next takeover beside it, with
  `0x64AEB4` named then.
- `0x42F9D0` (`AbilityCommit`'s target pick) is in no group either; its
  `entries_logic.txt` line `0042F9D0 7D5` runs over all of this group from
  `0x42FAB0` (it is 0xD3 bytes, to `0x42FAA2`). The catalogue's "folded
  into `0x42F9D0`" for eleven of this group's entries is that overrun.
  `dialogue-localisation.md` lists `0x42E400` and `0x42F9D0` among the
  ability table's readers (with `Str_CopyN`) by those overlong extents: the
  readers are `BattleAction_AbilityCheck` `0x42F680` (the name copy) and
  `0x42FE20` (it copies a record's first 16 bytes to `Text_Records[1]`).
- `0x435AB0`, the enemy's action pick (it takes the enemy index, calls
  `Rand` and stores a kind in `0x904B35`), is in no group and unnamed.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_actions`
(`src/game/battle_actions_fuzz.cpp`). It makes 23 byte-copies with every
relative call re-aimed at a recorder (`CloneCall` with `expected`; four
functions begin with their own call). The seven tables' 25 entries in `.data`
and the event hook `0x904B6C` are swapped for recorders, which the copies and
ours both read through memory; each stub's `disp32` is checked against its
table first. All is put back afterwards.

**Each round** starts from random bytes over (about 38 KB):

- ObjTrio and on to `0x803B90` (the member formula of section 3 for actors
  to 10, and `0x8031F3`);
- the battle globals `0x904AA0..0x904C00`, `Text_Records[0..1]`, the name
  buffer `0x904EA0`;
- `0x939F60..0x93C2B0`: the message list, the stat copy, the enemy objects,
  the message count;
- the two input words;
- constant data, put back after: the stand pairs `0x64DF70`, the action
  records `0x65C4C8` (1,024 of them), the ability rows and magic files, the
  item rows, the two banner text pointers.

Then the pointers and indices are put back inside what they index: the
actor and target bytes 0..10, both blocks' object and record pointers at real
objects, every record's action id below 0x400 with a category 0..3, the
members' stand indices, the turn order (0xFF or an actor), the step bytes
inside their tables, the message list. Each branch's boundaries are seeded:
the step byte of each dispatch; the search's ends and a turn order mostly
spent; targets 0..3, 10, 11, 0x40, 0x80, 0xC0, 0xFF; kind 2 by an enemy;
status bit 4 with the record bit, the id 0x97, AP near the cost; the ids
0x24 / 0x25 / 0x8C; state 6 and flag bit 6 across all eleven actors; a row
with no file; the round flags' bits 2, 5, 6 and 12; the message count at 0
and 1; the event byte on and off.

**The recorders are not quiet.** Each one moves what its caller reads again
after the call: the kind, the battle end and the event byte after the hook;
the target byte and `0x904AE2` after the pick; `0x904AA2` and the actor after
`Battle_ClearActingFlags`; the actor, both pointers and the id after
`Battle_SetActorBit`; the actor after the removal and the return; the id
after `0x42F9D0`; the pending word after `Battle_AnyFlagF0`; `0x904AA2` after
`Battle_SettleFlag8`; the count after the push. `Str_CopyN`'s writes bytes
into the name buffer and the banner's hashes the buffer it is given; the AP
cost comes at the AP's edges. A general disturber moves one of 22 watched
bytes three calls in four.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    battle_actions self-test: 23000 rounds over 23 functions (1000 each), 32675 calls to the stand-ins, 0 MISMATCHES
    coverage: step entries 25 of 25, hook 1304; begin: none left 420, found 579 (target block 311); checks:
      turned away 626, name banners 1182, status gate 112, AP short 110, AP enough 468; commits 664; preloads
      402; messages 592; end: swaps 231, battle end 356

Both runs exited 0 (`BOF3X_SHADOW='battle_actions'` and `BOF3X_SHADOW='*'`),
with `inject: 1047 ours` and no mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`controls.py` in the session scratchpad)
planted each one alone in `battle_actions.cpp`: replace, build, self-test,
revert. Every refused one exited 3 (the Fatal). The table gives the rounds
that refused each one, of 1,000 (a control in a shared helper names each
function that refused it).

| | Planted | Refused in (of 1,000) |
|---|---|---|
| A1 | ActionPhase: the cancel test before the step | 43 |
| A2 | ActionPhase: bit 5 cleared, not 4 | 306 |
| A3 | ActionPhase: either word, not both | 281 |
| S1 | BeginStep: by 0x904AA3 | 684 |
| S2 | KindStep: by 0x904AA3 | 813 |
| S3 | AbilityStep: the item table | 1000 |
| S4 | ItemStep: the ability table | 1000 |
| S5 | EffectStep: by 0x904AA3 | 666 |
| S6 | AfterStep: the effect table | 1000 |
| S7 | ActionPhase: by 0x904AA2 | 823 |
| B1 | BeginAction: 0x904AE2 stored before the step on | 456 |
| B2 | BeginAction: the actor stored before its pick | 554 |
| B3 | BeginAction: the enemy pick given the actor | 424 |
| B4 | BeginAction: a target of 11 filled | 55 |
| B5 | BeginAction: the target read before the pick | 217 |
| B6 | BeginAction: 0x904AE2 not counted at the end | 579 |
| B7 | BeginAction: 0x904AA2 not counted | 579 |
| B8 | member block: the stand index by char id * 2 | BeginAction 228, AbilityCommit 55 |
| B9 | enemy block: x from +0xF3 | BeginAction 461, AbilityCommit 194 |
| B10 | member block: the offset unsigned | BeginAction 137, AbilityCommit 32 |
| B11 | stat copy: an enemy from +0xA0 | BeginAction 424, End 133 |
| B12 | stat copy: 28 bytes | BeginAction 579, End 174 |
| B13 | BeginAction: none left sets step 1 | 421 |
| B14 | member block: z from +0x30 | BeginAction 262, AbilityCommit 59 |
| E1 | EnterKind: the kind read before the hook | 264 |
| E2 | EnterKind: the hook with 0 | 514 |
| K1 | KindPlain: the bytes read before ClearActingFlags | 234 |
| K2 | KindPlain: the name for kind 0 | 390 |
| K3 | KindPlain: an enemy from 4 | 39 |
| K4 | name record: 8 bytes | KindPlain 326, EnemyMessages 592 |
| K5 | KindPlain: message 0x7E | 326 |
| K6 | name record: not zeroed first | KindPlain 326, EnemyMessages 592 |
| K7 | system banner: timer 0x2E | KindPlain 326, AbilityCheck 222 |
| K8 | system banner: the window after the message | KindPlain 326, AbilityCheck 222 |
| O1 | KindOne: the object read before the call | 315 |
| O2 | KindOne: state 5 | 1000 |
| O3 | KindOne: step 3 | 1000 |
| C1 | AbilityCheck: the actor not re-read after the removal | 143 |
| C2 | AbilityCheck: turned away with +1 = 3 | 310 |
| C3 | AbilityCheck: the name shown for 0x97 too | 192 |
| C4 | AbilityCheck: the name 15 bytes | 498 |
| C5 | AbilityCheck: banner timer 0x3D | 498 |
| C6 | AbilityCheck: status bit 5 | 177 |
| C7 | AbilityCheck: the record byte +0x14 | 113 |
| C8 | AbilityCheck: the AP re-read after the cost | 80 |
| C9 | AbilityCheck: a cost equal to the AP refused | 37 |
| C10 | AbilityCheck: the cost of a member by the formula whatever the actor | 282 |
| C11 | AbilityCheck: the cost outside battle | 407 |
| C12 | AbilityCheck: 0x20 when the pick answers non-zero | 257 |
| C13 | AbilityCheck: 0x904AA5 kept | 452 |
| C14 | AbilityCheck: the status gate by the id of before the banner | 54 |
| C15 | AbilityCheck: the cost byte 0x904B79 for 0x97 | 171 |
| C16 | AbilityCheck: the actor for the stats read before the banner | 154 |
| C17 | AbilityCheck: an enemy AP +0xA4 | 77 |
| C18 | AbilityCheck: 0x904AA3 kept | 468 |
| M1 | AbilityCommit: runs without bit 5 | 484 |
| M2 | AbilityCommit: the object not re-read for +2 | **not refused; no input could tell** |
| M3 | AbilityCommit: 0x904B8D read before the bit | 235 |
| M4 | AbilityCommit: 0x26 for 0x25 | 93 |
| M5 | AbilityCommit: the target block bounded at 10 (as Battle_BeginAction) | 73 |
| M6 | AbilityCommit: the file by the id of before the pick | 127 |
| M7 | AbilityCommit: state 8 | 660 |
| M8 | AbilityCommit: 0x904AA8 bit 5 kept | 664 |
| T1 | AbilityStart: 0x904AA3 kept | 326 |
| T2 | AbilityStart: the owner the action record | 503 |
| T3 | AbilityStart: the id byte +3 | 489 |
| T4 | AbilityStart: without waiting for the file | 497 |
| I1 | ItemCheck: the removal by the actor of before the return | 144 |
| I2 | ItemCheck: the record not re-read after the check | 307 |
| I3 | ItemCheck: category and item swapped | 672 |
| I4 | ItemCheck: banner timer 0x1F | 684 |
| I5 | ItemCheck: 0x904AA3 read before the banner | 28 |
| I6 | ItemCheck: the removal skipped | 316 |
| I7 | ItemCheck: turned away with +2 kept | 315 |
| N1 | ItemCommit: the record read before the bit | 315 |
| N2 | ItemCommit: state 0xD | 990 |
| R1 | ItemStart: the id a byte | 251 |
| R2 | ItemStart: step 1 | 494 |
| W1 | EffectWait: a member's flags +0x131 | 604 |
| W2 | EffectWait: enemies 3..9 | 121 |
| W3 | EffectWait: +4 kept | 325 |
| W4 | EffectWait: a state-6 actor done again | 686 |
| W5 | EffectWait: the pending word not tested | 262 |
| W6 | EffectWait: 0x904AA8 bit 3 | 168 |
| W7 | EffectWait: the pending word read before the flag test | 105 |
| P1 | EffectPreload: the ability row by the low byte | 93 |
| P2 | EffectPreload: file 0xCF | 402 |
| P3 | EffectPreload: kind 5 not handled | 215 |
| P4 | EffectPreload: 0x904AA2 counted only with a file | 598 |
| L1 | EffectLoaded: the event byte read before File_LoadDone | 129 |
| L2 | EffectLoaded: the hook with 5 | 262 |
| L3 | EffectLoaded: 0x904AA8 loses 0x800 | 395 |
| L4 | EffectLoaded: row 0x1B | 519 |
| L5 | EffectLoaded: the stores after the row copy | 505 |
| F1 | AfterSettle: zero gives 3 | 351 |
| F2 | AfterSettle: 0x904AA2 read before the call | 180 |
| F3 | AfterSettle: the pending word not tested | 140 |
| G1 | EnemyMessages: the enemy index one on | 592 |
| G2 | EnemyMessages: the count not re-read after the push | 274 |
| G3 | EnemyMessages: the message id a byte | 592 |
| G4 | EnemyMessages: the busy byte ignored | 150 |
| G5 | EnemyMessages: entry n - 1 | 524 |
| G6 | EnemyMessages: the push (1, 1, ..) | 592 |
| G7 | EnemyMessages: none left, step 3 | 258 |
| X1 | End: the last hook by the event byte read first | 38 |
| X2 | End: 0x904AA8 keeps bit 2 | 704 |
| X3 | End: battle end at step 3 | 356 |
| X4 | End: a member's flags +0x134 | 19 |
| X5 | End: the kind not set to 1 | 118 |
| X6 | End: 12 bytes swapped | 155 |
| X7 | End: the texts inverted | 174 |
| X8 | End: 0x904AA2 0 after the swap | 172 |
| X9 | End: banner timer 0x10 | 174 |
| X10 | End: bit 6 tested before Battle_ClearActingFlags | 185 |
| X11 | End: bit 12 kept | 265 |
| X12 | End: the battle-end test before the hook | 75 |
| X13 | End: the stat block of the old actor | 166 |
| X14 | End: no pending test | 134 |

**M2** changes nothing: `0x42F880` re-reads `0x904B3C` between its two
stores with no call between (only the store to `0x904AA8`, which cannot
alias the pointer), so both reads see the same object. It is not counted.

**Thirteen controls were thin or blind in the first run** and are refused
now, each once the recorder it depended on moved what its caller reads back,
and once the stand-ins' pseudo-random answers stopped being one value per
call (a recorder that moved a byte half the time and answered 0 half the time
did both on the same bit, so its caller never saw the move): C8 (0, then 80),
X10 (1, then 185), L1 (4, then 129), F2 (6, then 180), W7 (8, then 105), C14
(9, then 54), A1 (14, then 43), L5 (18, then 505), B2 (18, then 554), I2 (21,
then 307), C16 (33, then 154), M5 (19, then 73). S3 (0x904AA2 for
`AbilityStep`'s index) faulted in the first run - the index ran past the
three entries into `0x42F9D0`'s id bytes - and was replaced by the item
table, refused in 1,000.

**The thinnest now:** X4 (19), I5 (28), C9 (37), X1 (38), K3 (39), A1 (43).

**What the fuzz cannot see:**

- anything the callees really do, or what the tables' other readers do;
- the order of stores with no call between them, and a store moved across a
  call whose recorder neither reads nor writes it (`0x8031F3` around
  `Str_CopyN`);
- `0x904AE3` read once or each pass (no call inside the search);
- the argument bits of section 2;
- a target side in `AbilityCommit` (section 3): both sides would read past
  the image, so the recorder answers 0..10 only.

## 5. For the batch check

The combat A/B `analysis/validate_combat.sh` reaches all 23 (every one is a
combat-route entry of the round's catalogue). `entries_logic.txt` gets one
line per function with the extents of section 2. What is fuzz only: the
battle-end path of `End` unless the route's last action ends the battle
there, the swap unless the route has a bit-6 round, kind 2 by an enemy, the
AP and status refusals, and the event hooks.
