# The spell harness: one fuzz for every spell group

**Status:** IN PROGRESS (2026-09-25) - built and proved on one overlay
(Steal's, [`magic_steal.md`](magic_steal.md): 3 functions, 28 of 28
controls refused); no wave group has used it yet.

Group SH of round nine ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md)).
`src/game/magic_harness.h` / `.cpp`: what every spell group needs to take
one overlay of `Magic_Rows`, so that a group writes only its functions, a
list and its seeds. It generalises the two start-up fuzzes that took spell
code before - `battle_fx_tasks_fuzz.cpp` (round eight, CE) and
`magic_fx_reached_fuzz.cpp` (CJ) - whose section 9 said what a class
harness must hold ([`magic_fx_reached.md`](magic_fx_reached.md)).

## 1. What a spell function is, to the harness

Every function of a BMAGIC overlay is a battle task's step: `void (void)`,
`__cdecl`, run by `BattleTask_RunAll` with `Sprite_Current` the slot (and
`0x93B8C4` the same slot), `0x93B940` the slot's owner, reading the battle
bytes `0x904AA0..0x904B50`, the message-window byte `0x939F60`, the party
and enemy records and its own overlay's `.data`. So one frame of that
state is every input, and one recorder per callee is every output.

## 2. How ours calls out

Ours never calls a callee by its bare name. It calls through the harness,
which in the game is the callee itself and during a group's fuzz is the
recorder standing in for it:

| Call | Write | In the game |
|---|---|---|
| a named callee, Capcom's or ours | `MH_CALL(Rand)()`, `MH_CALL(Inventory_Add)(c, i, 1)` | the name |
| an unnamed one | `MH_AT(void (__cdecl*)(unsigned, unsigned), 0x4B58F0)(item, category)` | that address |
| a phase an effect dispatches to (a stack table's immediate, a `.data` entry) | `magic_harness::Phase(bof3::addr::SkillSteal_Start)()` | the address: Capcom's code, or the `jmp` Inject put there to ours |

A phase is always called by its **address**, even when it is ours: the
recorder for a handler is keyed on the address the table holds. The check
is `if (!g_active)` - one byte, false outside the fuzz.

## 3. What a group writes

1. **Ours**, one `.cpp` (`src/game/magic_<name>.cpp`), calling out as
   above, each stack-table dispatcher aborting past its table (the
   project's precedent, [`magic_fx_reached.md`](magic_fx_reached.md) §3).
2. **The clone table**: `python tools/magic_rows.py --unit MAGIC0NN
   --clones` prints it for the unit's functions not yet ours - each
   function's extent (its recursive descent, jump tables included), every
   `E8`/`E9` that leaves it, every stack-table immediate, every jump table,
   and a note for each `call`/`jmp` through `.data` (list that table as a
   `DataTable`). Rename the `Fn_` placeholders; check the `REFUSED` lines
   (a conditional jump out of the extent cannot be copied).
3. **A `magic_harness::Group`** (`src/game/magic_<name>_fuzz.cpp`): the
   clones; any callee the standard set lacks (`Callee`: name, original
   address, the pointer ours passes, the arguments and what of each the
   callee reads, how it answers); the `.data` handler tables; any region
   beyond the standard ones (the overlay's own `.data`, a pool); a
   `Seed(k)`; optionally a `disturb(h)` for cells of the group's own.
4. **One call**, `magic_harness::Run(group)`, from the group's
   `_Inject()` under its `BOF3X_SHADOW` name, before the `BOF3_INJECT`
   lines.

Then as always: `symbols.toml` entries with evidence and `impl`, the
module in `CMakeLists.txt` and at the end of `inject_all.cpp`, a line per
function in `analysis/calltrace/entries_logic.txt`, controls planted one at
a time. [`magic_steal.md`](magic_steal.md) and `magic_steal_fuzz.cpp` are
the worked example: 110 lines of group file for three functions, most of it the roll's seed.

## 4. What the harness does

- **The recorders.** One pool of 160 stand-ins (`Stub<I>`), assigned at
  start-up: first the standard callees (below), then the group's, then one
  per handler address its clones' immediates and its `.data` tables name.
  A callee's recorder logs its arguments masked to what the callee reads,
  then disturbs, then answers by its kind: garbage; a byte in `lo..hi`
  with garbage above (`BattleTask_Create`: a slot 0..47); a flag (0 in al
  a third of the time); or `Rand`'s answer - negative values the CRT never
  gives, a third of the time near `SetRandHint`, the round's first exactly
  `SetRandFirst`. A handler's recorder logs the slot and its phase bytes.
- **The disturbance.** Two calls in three move something a caller may read
  again: `Sprite_Current`, the current slot, the owner, the source sprite,
  the target and actor bytes, the message-window byte, the effect flags,
  `Frame_Counter`, a field of the task or the owner, any byte of the target
  enemy's record, or (the group's `disturb`) a cell of the group's. The
  target's record is not written for a target of 11 or more (none: a side
  bit such as `0x40` would point it past the image), nor where a target of
  0..2 lands it on the current-slot or owner cell (group S16,
  [`magic_s16.md`](magic_s16.md) section 4).
- **The copies.** `bof3::CloneOriginal` with every call re-aimed at its
  recorder (`expected` checked: a site already re-aimed is refused), the
  stack-table immediates checked and re-aimed, jump tables moved into the
  copy; `.data` tables swapped for their recorders while the fuzz runs.
- **The state.** Standard regions: the 48 task slots, `0x93B8C0..0x93B960`,
  `Sprite_Current` / `Gfx_ClutStripDirty` / `Frame_Counter`, the battle
  bytes, the message-window byte, party records 0..4, enemy records 0..7,
  two sprite records of the harness's own; then the group's. Each round:
  random bytes, the pointers put back inside (`Sprite_Current` and the
  current slot at slots 0..3, the owner at a slot or a record, the source
  at a record, the target 0..10, the actor 0..4, the phases 0..2, the
  slots' owners), the group's seed; theirs, then ours with the recorders
  routed; every region and the log compared. 2,000 rounds per function by
  default.
- **The report.** One line of totals, one of coverage (every recorder the
  originals called, and how often), and on a difference the first twelve
  rounds with the function, the log lengths and the first differing byte as
  region + offset, then a `Fatal` (exit 3).

**The standard callees** (`kStandard`): `BattleTask_Create`,
`BattleTask_FreeCurrent`, `Battle_SetTargetFlags`, `Battle_SetTargetFlag40`,
`Sound_PlayById`, `Sound_PlayEffect`, `BattleActor_SetAnimation`,
`BattleActor_UpdateScreenXY`, `BattleActor_PlaySound`, `BattleActor_Flash`,
`BattleActor_FxSize`, `Sprite_ScriptTickOnce`, `Sprite_UpdateScreen`,
`Sprite_ReleaseTint`, `MagicFx_PushActorMatrix`, `MagicFx_DrawDisc`,
`MagicFx_DrawFan`, `MagicFx_DrawRing`, `Gte_PopMatrix`, `AreaMap_TintClut`,
`Inventory_Add`, `Msg_SystemPtr`, `BattleQueue_Push`, `Rand` and
`0x4B58F0`. Each is written `MH_OURS(name)` or `MH_THEIRS(name)`; a callee
that changes hands (a later round takes `Rand`) fails at start-up with a
line saying so, and moves column. The effect library at `0x4FAF90..0x4FC32F`
(queue group L) is mostly not here yet: when L is taken, its functions join
the standard set, and until then a group lists what it calls of it.

## 5. What it cannot do

- **Tell a function's meaning.** The fuzz proves ours equals Capcom's on
  the state it builds; what the spell looks like is the owner's eye
  (DIV-0045's cheats put a skill in a list).
- **Reach what the seed does not.** A threshold moved by one shows only
  when a round lands on it: seed each compare's two sides (the proof's
  thinnest control, a tier threshold, was refused in 7 rounds of 2,000).
- **Copy a function whose conditional jump leaves its extent**, or one
  tail-jumping through `.text` (`REFUSED` in the clone table). None so far.
- **Know a handler's arity.** A phase is `void (void)`; a callee's
  arguments are what the group says. A wrong mask hides a difference in
  the bits masked off; a wrong count logs garbage the same on both sides.

## 6. Shadow name and self-test

The harness has no shadow name of its own: each group's is its module's
(`magic_steal` for the proof). `BOF3X_SHADOW='*'` runs every group.
