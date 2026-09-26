# The spell harness: one fuzz for every spell group

**Status:** IN PROGRESS (2026-09-26) - built and proved on one overlay
(Steal's, [`magic_steal.md`](magic_steal.md): 3 functions, 28 of 28
controls refused). The first spell wave's eleven groups each extended it on
their own branch; group HX folded those extensions into one harness
(section 7: what each group's calls become). Steal's counts are unchanged
through the fold.

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
   What else a group may say, when its functions need it:

   | Need | Say | Where |
   |---|---|---|
   | A function that answers (an index in al) | its `ret_mask` (`0xFF`): eax masked, logged after each pass, compared | `Clone` |
   | A function that must run undisturbed (it indexes its own stack by a cell it reads again) | `calm = true` | `Clone` |
   | Functions that take arguments | `args(k, a)`: the ten words function k is called with, after the seed | `Group` |
   | A cell to put back after every disturbance | `settle()` | `Group` |
   | Dispatchers that read the phase after a call through a table of n entries | `phase_span = n`: the disturbance keeps `+1` / `+2` below n (default 0: any byte) | `Group` |
   | A callee of up to ten arguments | `nargs`, `masks[10]`: arguments past the fourth logged in a second entry | `Callee` |
   | A pointer argument into the caller's frame | `deref[i] = n`: argument i logged as a hash of the n bytes at it, not the pointer | `Callee` |
   | A callee that writes through its pointers, moves a cell, or must answer from the state | `effect(args, answer)`: after the log and the disturbance; returns the answer. Logs with `Note` / `NoteBytes`, writes with `FillBytes`, values from `Noise` | `Callee` |
   | A callee the recorders cannot stand in for at all | `custom`: a stand-in of the callee's exact type, logging with `Record(address, ...)`, disturbing with `Stir()`, answering from `Noise()` | `Callee` |
   | A callee answering a C bool its callers test whole | `Answer::kBool` (0 a third of the time, else 1) | `Callee` |
   | A callee of the group's own called directly, not through a table | `Answer::kPhase`: logs the task it ran for, and the dword at `masks[0]` if not 0 | `Callee` |
   | A deterministic callee of compared state (GTE, GPU setters, `Math_Sin`) | `Answer::kThrough`: both sides call it for real, nothing logged | `Callee` |

   A group's callee list is registered **before** the standard set, so a
   group may list a standard callee to record it differently (a `deref`,
   an `effect`); the first listing of an address stands. Every new field
   has a default, so a group that needs none of them writes what Steal's
   file writes.
4. **One call**, `magic_harness::Run(group)`, from the group's
   `_Inject()` under its `BOF3X_SHADOW` name, before the `BOF3_INJECT`
   lines.

Then as always: `symbols.toml` entries with evidence and `impl`, the
module in `CMakeLists.txt` and at the end of `inject_all.cpp`, a line per
function in `analysis/calltrace/entries_logic.txt`, controls planted one at
a time. [`magic_steal.md`](magic_steal.md) and `magic_steal_fuzz.cpp` are
the worked example: 110 lines of group file for three functions, most of it the roll's seed.

## 4. What the harness does

- **The recorders.** One pool of 256 stand-ins (`Stub<I>`, ten argument
  slots each), assigned at start-up: first the group's callees, then the
  standard ones (below), then one per handler address its clones'
  immediates and its `.data` tables name. A callee's recorder logs its
  arguments masked to what the callee reads (or hashed through `deref`),
  four to a log entry, then disturbs, then answers by its kind: garbage; a
  byte in `lo..hi` with garbage above (`BattleTask_Create`: a slot 0..47;
  `lo` above `hi` wraps through 0xFF); a flag (0 in al a third of the time);
  a bool; `Rand`'s answer - negative values the CRT never gives, a third of
  the time near `SetRandHint`, the round's first exactly `SetRandFirst`;
  then the callee's `effect`, if any, has the last word. A handler's
  recorder logs the slot and its phase bytes. `StandIn` finds a recorder
  by the pointer ours passes, or by the original's address (a callee that
  became ours, called by `MH_AT`), or through the standard set's name for
  a callee a group listed by address.
- **The disturbance.** Two calls in three move something a caller may read
  again: `Sprite_Current`, the current slot, the owner, the source sprite,
  the target and actor bytes, the message-window byte, the effect flags,
  `Frame_Counter`, a field of the task or the owner, any byte of the target
  enemy's record, or (the group's `disturb`) a cell of the group's; then
  the group's `settle`. The target enemy's record is written only for a
  target of 3..10: a party target's "enemy" lies over the current-slot
  and owner cells (a torn owner pointer, written through by the next
  disturbance), and a side bit (`0x40`, `0x80`) points past the image -
  five groups of the first wave met it, S22's fuzz as a crash.
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
  routed; every region and the log compared (the log holds 32,768 entries a
  round; only the prefix a pass wrote is copied and compared). 2,000
  rounds per function by default.
- **The report.** One line of totals (the calls counted are calls and
  phases, not the extra entries - arguments past the fourth, notes,
  answers), coverage on as many lines as it takes (every recorder the
  originals called, and how often), and on a difference the first twelve
  rounds with the function, the log lengths, the first differing log entry
  and the first differing byte as region + offset, then a `Fatal` (exit 3).

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
(queue group L, [`magic_lib.md`](magic_lib.md) on its branch) is not here
yet: L's branch adds the seventeen functions an overlay calls, by name, and
they join this list when L merges (its symbols come with it). A group
written while they were Capcom's, listing one by address, keeps working:
its listing stands, and ours calling it by name finds it through the
standard set.

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

Steal through the folded harness (2026-09-26, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=magic_steal`, exit 0): 6000 rounds, **9278** calls, 0
mismatches, 11368 bytes (9 regions); coverage `Inventory_Add 763,
Msg_SystemPtr 2000, BattleQueue_Push 2000, Rand 2000, 0x4B58F0 515, phase
0x4F50E0 645, phase 0x4F5140 720, phase 0x4F52D0 635`. Every count matches
[`magic_steal.md`](magic_steal.md) section 3. Five of its controls were
planted again, and each was refused in the same number of rounds as
before: R1 7, R6 43, R15 94, S3 2,000, R11 763. `BOF3X_SHADOW='*'`: exit 0.

None of Steal's counts moved because nothing it reads changed. Its roll
seeds a party target a third of the time, and the disturbance no longer
writes that "enemy" record (it lay over the task slots), but no Steal
function reads those bytes after a call. The rounds' random draws are the
same (a group without `args` is still called with three words), and the
new entry kinds never occur in Steal's log.

## 7. The first wave's harness edits, folded (group HX)

Eleven groups of the first wave (L, S16..S25; base `9ad12f1`) each patched
this harness on their own branch. The fold keeps one name per concept,
every limit at the largest any group needed, and each group's option as a
per-group setting. When a group merges, it takes this harness and drops
its own edits of `magic_harness.h` / `.cpp` (and of
`magic_steal_fuzz.cpp`); its fuzz file changes as below. A group's
documented counts may move: the entry tags, the salted `Noise`, the second
log entry for arguments past the fourth, and the disturbance fix all
change what a round sees.

**What became of each group's addition:**

| Group addition | Became |
|---|---|
| L `Run(g, Extras)`: `Extras::args(k, a)` | `Group::args(k, a)`, ten words |
| L `Extras::returns[k]`; S17 `Clone::result_mask`; S18 `Clone::ret_mask`; S20 `LogReturn(base, mask)`; S21 `Clone::answer_bytes` | `Clone::ret_mask` (`answer_bytes` 1 is `0xFF`) |
| L `Act {address, fn(args)}`; S21 `Callee::effect(args, answer)`; S25 `SetCallHook(hook)` | `Callee::effect(args, answer)`, called after the log and the disturbance; it returns the answer |
| S17 `Callee::custom`; S19 `Custom {name, address, key, stand_in}` with `Run(g, customs, n)` | `Callee::custom` (the group's list is registered first, so it can replace a standard callee's recorder too) |
| S17 `Record(address, ...)`; S19 `Note(address, ...)` | `Record(address, a = 0, b = 0, c = 0, d = 0)` |
| L `Note(a, b, c, d)`; S21 / S25 `LogValue(v)` | `Note(a, b = 0, c = 0, d = 0)` |
| S21 / S25 `LogBytes(p, n)` | `NoteBytes(p, n)` (up to 16 bytes as they are, more as a hash) |
| S17 `Noise()`; S19 `Salt()`; S21 `Salted()` | `Noise()`, salted: a new value at each call, not one per log entry |
| S17 / S19 `Stir()`; S17 `HashBytes`; S21 `FillBytes` | the same |
| S18 `Callee::deref16` (a bit mask, 16 bytes); S24 `Callee::deref[4]` (byte counts); S20 `LogPointee(address, arg, bytes)` | `Callee::deref[10]`, byte counts: the argument is logged as a hash of its bytes |
| L / S18 `masks[8]` and eight-argument stubs; S21 / S25 ten-argument stubs | `masks[10]`, ten-argument stubs; arguments 4..9 go in a second log entry |
| S17 `Clone::invoke(fn)` | `Group::args(k, a)`: the arguments are computed after the seed, and the harness makes the call |
| S17 `Group::settle`; S18 `Clone::calm` | the same |
| S21 `Group::phase_span`; S24's phase bytes kept at 0 / 1 in Disturb 9 / 10 | `Group::phase_span` (S24: 2); the default 0 keeps the old any-byte |
| S23 / S25 `Answer::kBool` | S23's: exactly 0 (a third of the time) or 1. S25's third case (garbage with al 0) is dropped, because a C bool never answers it |
| S23 `Answer::kThrough`; S24 `Answer::kPhase`; S24's `kByte` wrapping | the same |
| Log of 2,048 (S18) / 4,096 (S17, S24) / 8,192 (S20) / 32,768 (S25) entries, compared by prefix | 32,768, compared by prefix |
| Calls per clone 32 -> 64 (S17) | 64; stand-ins 160 -> 256 |
| Coverage over several lines (S17, S19, S20, S21; S18 one 2,048-byte line) | lines of at most 900 characters, marked `, continued` |
| L's `StandIn` fallback to the original's address | the same, plus the standard set's name for a callee a group listed by address |
| L's effect library in `kStandard` | not folded: it needs L's symbols, so it comes with L's merge (L's own hunk of `kStandard`) |
| Disturb 12 / 13: L, S19 `< 11`; S16 `< 11` and not the two cells; S17 `>= 3`; S18 not `0x93B8C0..0x93B960`; S20, S21, S23, S25 `3..10`; S24 `<= 10` and not the cells | `3..10` |
| S24's `S24DBG-TEMP` lines and the byte-order mark it added to `magic_harness.cpp` | not carried |

**Each group's port:**

| Group | What its fuzz file changes |
|---|---|
| L | `mh::Run(group, extras)` becomes `Run(group)`, with `.args = &Args` on the `Group` (`Args` fills eight of the ten words as before). `kReturns[k]` becomes each clone's `ret_mask`. Each `Act {address, fn}` becomes an `effect` on that callee's listing: `fn(a)` becomes `fn(a, answer)`, ignoring `answer` (it already ran after the disturbance). `Note` is unchanged. Its seventeen library callees go into `kStandard` in its merge. Its call count drops by the return and argument entries it used to count. |
| S16 | Nothing in its fuzz file: its only edit was the Disturb fix, now `3..10`. |
| S17 | Each `Callee`'s trailing `nullptr` goes (a custom one becomes `..., lo, hi, {}, nullptr, &RecX}`). Each `Clone`'s trailing `0, nullptr` becomes `0` (`0xFF` for `Revive_IsStrong` and `ReviveMote_Alloc`). `InvokeEdge` becomes a `Group::args` that fills `Key(Gfx_PacketNext)`, `Key(&g_own.edge_out)` and `edge_v[0..7]` for `LeechShell_Edge`; every clone is then called with ten words. `Record`, `Stir`, `Noise`, `HashBytes` and `settle` are unchanged. |
| S18 | `deref16 = 1u << 2` becomes `deref {0, 0, 16}` (the positional field after `hi`). A clone's `..., ours, false, 0xFF}` becomes `..., ours, 0xFF}`, and `..., ours, true, 0}` becomes `..., ours, 0, true}` (the fields are `ret_mask`, then `calm`). |
| S19 | `kCustoms` entries move into `kCallees` as `{name, address, key, 0, {}, mh::Answer::kGarbage, 0, 0, {}, nullptr, stand_in}`. `mh::Run(group, kCustoms, n)` becomes `Run(group)`, `Note(address, ...)` becomes `Record(address, ...)`, and `Salt()` becomes `Noise()`. |
| S20 | Each `LogPointee(address, arg, n)` becomes `deref[arg] = n` on that callee's listing (Gte_RotTrans, Gte_RotMatrix and Gte_RotTransPers3 / 4 are all in its list). Each `LogReturn(base, 0xFF)` becomes `ret_mask 0xFF` on that clone. |
| S21 | `..., lo, hi, &Effect}` becomes `..., lo, hi, {}, &Effect}`. `LogValue` becomes `Note`, `LogBytes` becomes `NoteBytes`, and `Salted` becomes `Noise`. `answer_bytes 1` becomes `ret_mask 0xFF`; `phase_span` is unchanged. Its effects now run after the disturbance, not before: the harness has one order, L's. They read the caller's stack and S21's own cells, which the disturbance leaves alone, but its counts may move. |
| S22 | Nothing is required: its fuzz file made no harness edits, and its crash (the seed's `0x40` side bit, then Disturb 12 / 13 writing past the image) is the bug the fold fixes. Its own fuzz of three functions (`SelfTestOwn`: vectors by pointer, and a `Battle_ActorIsOut` that must leave one actor in) can move into the shared one, with `deref` on the GTE callees and an `effect` on `Battle_ActorIsOut`. Its `S22_FROM` / `S22_TO` lines are marked `// DEBUG`. |
| S23 | Nothing: `kBool` and `kThrough` keep its meaning. |
| S24 | `deref` and `kPhase` are unchanged. The phase bytes' 0 / 1 becomes `phase_span = 2` on its `Group` (`..., 2000, nullptr, 2}`). Its `S24DBG-TEMP` lines and the byte-order mark go. |
| S25 | `SetCallHook(&Hook)` becomes an `effect` on each callee it names (its list is registered first, so a standard callee can be listed to get one). The argument logging becomes `NoteBytes` / `Note`, or just `nargs` up to 10 with masks. `Advance` goes on `Gfx_CommitPrim` and `MapView_LinkPrimAt`, and `Sprite_Current[8] &= 3` on `Gte_PushMatrix` (after the disturbance, where its `after` half ran). `kBool` loses the al-0 garbage case. |

Steal needs none of the following, so none of its controls exercises them:
`args`, `ret_mask`, `calm`, `settle`, `phase_span`, `deref`, `effect`,
`custom`, `kBool`, `kPhase`, `kThrough`, and arguments past the fourth.
Each is its group's original code rewritten into one shape, and the first
group to port each one is its first test.
