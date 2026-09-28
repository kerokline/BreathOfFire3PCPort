# The boss harness: one fuzz for every boss group

**Status:** MEASURED (2026-09-28) - built by group BH of round eleven's wave
one, stage A, and proved on BH's own 26 functions: the 20 shared helpers
([`boss_h.md`](boss_h.md), three `Run`s, 0 mismatches) and the six spawn
helpers ([`boss_h.md`](boss_h.md) section 5, one `Run`, 0 mismatches), with
every shape but `kSetup` and `kTask` exercised by a clone and `Clone::via`
driven through six dispatchers of four other units. `kSetup` and `kTask` are
exercised first by stage B (BSA's set-ups, BSF's effect tasks): the first
group to use each is its first test, as the spell and area harnesses' first
users were.

`src/game/boss_harness.h` / `.cpp`: a copy of the spell round's harness
([`magic_harness.md`](magic_harness.md)) and the area round's
([`area_harness.md`](area_harness.md)), adapted to the boss band
`0x437A00..0x441000` of [`takeover-queue-bosses.md`](takeover-queue-bosses.md),
so that a boss group writes only its functions, a list and its seeds.
`magic_harness.*`, `scenario_harness.*` and `area_harness.*` are not edited:
this harness has its own recorder pool, its own `g_active`, its own standard
set, and all four run in one process (`BOF3X_SHADOW='*'`).

**Read this first if you are a stage-B group (BSA..BSE, then BSF..BSJ):**
section 3 is the recipe, section 8 a worked set-up and a worked kind, section
6 the traps already paid for.

## 1. What a boss function is, to the harness

Every function of the band hangs from one of three root sets
([`takeover-queue-bosses.md`](takeover-queue-bosses.md) §1.2) or from what
those store, and runs in one battle frame: `Sprite_Current` (an enemy's
object, or a task slot for an effect task), the current enemy `0x939AD8`,
the eight enemy objects (`0x93B960 + n * 0x128`), the party, the battle
bytes `0x904AA0..0x904BA0` with the fight byte `0x904AAA` and the three
hooks among them, the field's objects (the actors a set-up finds by tag), the
chapter bytes `0x8034E4` / `0x8034E5`, and the kind's own `.data` tables. So
one frame of that state is every input, and one recorder per callee is every
output. What the engine does around the call is the clone's **shape**
(`Clone::shape`, `boss_harness::Shape`) - the eight of the plan's section 2
and one more:

| `Shape` | Reached by | How the engine calls it | What the harness does |
|---|---|---|---|
| `kState` (default) | a kind's state table (`.data`), by `+1..+4` | the kind's dispatcher's `jmp`: `void (void)`, `Sprite_Current` and `0x939AD8` the enemy; `al` out where it tail-jumps to `Sprite_ScriptTick` | calls it with garbage words; `ret_mask 0xFF` if it answers |
| `kSetup` | `Boss_SetupTable[fight]` | `Battle_InitBossEncounter`'s `jmp`, after `0x494500`: `void (void)` | nothing more; the hooks it stores are logged (below) |
| `kEnd` | the hook `0x904B64` (`BattleHook_End`) | `BattleEnd_AwaitMemberTasks` `0x431464`, once, as the way out is picked: `void (void)` | nothing more |
| `kExit` | the hook `0x904B68` (`BattleHook_Exit`) | `BattleEnd_ExitHook` `0x4317B9` on the way out: `void (void)` | nothing more |
| `kEvent` | the hook `0x904B6C` (`BattleHook_Event`) | with one word, the phase code: 3 `Battle_PhaseDispatch`, 6 `Battle_Init`, 1 / 4 / 0 / 5 the action phases, 2 `BattleRoundEnd_NextRound` (whose `al` 0xFF holds the round) | the code drawn 0..6 each round; `al` compared (`ret_mask` 0 means 0xFF) |
| `kDispatch` | `BossKind_Table[kind]`, or a dispatcher a state table reaches | `mov al, [Sprite_Current + at]; jmp [eax * 4 + table]` | with `Clone::states` set, `Sprite_Current[state_at]` drawn below it each round (before the seed); the table is a `DataTable` |
| `kEnemyHook` | an enemy's `+0xF4` | with one word: 0 (`0x435BF5`, the action pick), 1 (`0x4367EE`, the hit), 2 (`BattleEnemy_RunAll`, when `+1` is set) | the word drawn 0..2 |
| `kTask` | the effect dispatchers' stack tables (`BattleBossFx_Dispatch` `0x4357D0` slots 2..7, `BattleFx_Dispatch` 8, `BattleMagicFx_Dispatch` 93) | `BattleTask_RunAll`: `void (void)`, `Sprite_Current` and `0x93B8C4` the slot | `Sprite_Current` a task slot (and `0x93B8C4` the same two times in three) |
| `kCallee` | none | called directly by the unit's own functions | arguments by `args`, answer by `ret_mask` |

**After every call, whatever the shape, the three hooks `0x904B64..6C` are
read back and logged** (with `Sprite_Current`), and they are in the compared
state as well: a set-up stores them and a hook re-points itself, so a function
that installs a different hook is refused on the spot. An enemy's `+0xF4` /
`+0xF8` / `+0xFC` are in the compared state too (the enemy objects are a
region).

**A reading the plan had to correct.** The plan called `0x904B64` "the
per-frame script, called from `0x431464`". `0x431464` is inside
`BattleEnd_AwaitMemberTasks` (`0x431320`, [`battle_turn_steps.md`](battle_turn_steps.md)):
it calls the hook once, after the members' end tasks, as the battle's way out
is picked - and in an event battle it is the hook that picks it (outside one
the step itself sets `0x904AA1` from `0x904AE8`). `BossHook_EndPickWay` is
the model: `0x904AE8` bit 1 (the win) sets the chapter's step and jumps to
`0x446DE0` (phase 5, step 1), else `0x446E00` (step 2). So `symbols.toml`
names the hooks `BattleHook_End` / `BattleHook_Exit` / `BattleHook_Event`,
and the shapes `kEnd` / `kExit` / `kEvent`.

## 2. How ours calls out

As in the other harnesses. Ours never calls a callee by its bare name:

| Call | Write | In the game |
|---|---|---|
| a named callee, Capcom's or ours | `BH_CALL(Sprite_ScriptTick)()`, `BH_CALL(BossActor_Find)(6)` | the name |
| an unnamed one | `BH_AT(void (__cdecl*)(), 0x446E20)()` | that address |
| a function of the group's own called directly, or a stack table's immediate | `boss_harness::Phase(0x43B0D0)()` - or `BH_CALL(Name)` once it is named | the address: Capcom's code, or the `jmp` Inject put there to ours |
| an entry of a `.data` table (a kind's state table, a hook table) | **as read**: `reinterpret_cast<Handler>(Long(At(table + 4 * state)))()` | the entry itself; the fuzz swaps the table's cells for recorders (`DataTable`), so ours reaches them by reading the cell |
| a hook in a cell (`0x904B64..6C`, an enemy's `+0xF4`) | as read, through the cell | the harness keeps its own recorders in those cells at each round's start |

A hook or a table pointer a function **stores** (`mov [0x904B64], imm`,
`mov [ecx + 0xF4], imm`) is a literal in ours too: store the same address
(`SetLong(At(at::kHookEnd), 0x437DE0)`). The clone keeps the immediate
(`tools/boss_rows.py --clones` says "stored or pushed, not re-aimed"), the
compared state holds both sides' stores, and they must be equal.

## 3. What a group writes

1. **Read** every function of your units to its last instruction
   (`tools/pe_disasm.py`, capstone). `tools/boss_rows.py ... --unit <UNIT>
   --clones` prints each unit's clone rows (`boss_harness::` types, this
   API one for one); `analysis/boss_funcs.tsv`'s group column is ownership.
   Read each row against the code (section 6: the tool reads a table to the
   next named address, and says "72 code entries" of a table of 12).
2. **Ours**, `src/game/<module>.cpp`, calling out as in section 2; a
   dispatcher through a `.data` table aborts past its table with a
   `bof3::Fatal` (the original jumps through whatever follows; round9 doc
   section 6), as `boss_h.cpp`'s `Dispatch` does. Callees nobody owns yet
   (another stage-B group's this wave) by raw address in
   `<module>_callees.h`. BH's 26 are ours once BH merges: call them by name
   (`BH_CALL(BossActor_Clear)(0)`, `BH_CALL(BossMap_SetCorners)(1, 0)`); the
   ones your tables hold (`BossOp_ScriptTick`, `BossOp_Death`, `Boss_Nop`,
   ...) are table entries like any other ([`boss_h.md`](boss_h.md) section 1
   lists them).
3. **The fuzz**, `src/game/<module>_fuzz.cpp`: per fight id or kind, a
   `boss_harness::Group` -

   | Field | What to give |
   |---|---|
   | `shadow`, `clones`, `n_clones` | your module's name; the clone rows, each with its `shape` (and `ret_mask 0xFF` for a function answering in `al` - every event hook, every state that tail-jumps to `Sprite_ScriptTick`) |
   | `callees` | any callee the standard set (section 5) lacks, or one it records too coarsely; your own functions called directly (`{"Name", 0x43B0D0, KeyOf(&::Name), nargs, {masks}, Answer::kGarbage, 0, 0}`) |
   | `data_tables` | each `.data` table your code jumps through, from its first pointer: the kind's `+1` table, its `+2` / `+3` sub-tables, its `+0xF4` hook table (`nargs 1`). Not the byte tables (`+0xF8`, `+0xFC`) |
   | `regions` | any state beyond section 4's (a table your code writes, a cell of the area block, the packet buffer is already there) |
   | `seed(k)` | function k's boundaries (section 7) |
   | `disturb(h)`, `settle()` | what your functions read again after a call that the standard disturbance does not move; `settle` to put back a cell your function cannot survive garbage in |
   | `rounds` | 6,000..8,000 (the round's rule: fewer and controls go unrefused) |
   | `args(k, a)` | the words of a `kCallee` (and of a hook, to override the shape's draw) |
   | `fight` | the set-up's id: written into `0x904AAA` every round. For a kind, one of the fights that spawn it. `-1` draws 1..55 each round |
   | `kind` | for a kind's functions: written into the current enemy's `+0x100` every round (the byte `BossKind_Table` is indexed by, and some kinds read it). `-1` leaves it to the fill |

   One `boss_harness::Run(group)` per fight id or kind, from your
   `SelfTest()`, under your module's `BOF3X_SHADOW` name.
4. **Injects**: `<Module>_Inject()` runs the self-test when the shadow name
   is listed, then one plain `BOF3_INJECT(Name);` line per function (no
   token-pasting macro: `tools/ledger_check.py` greps them).
5. Then as always: `symbols.toml` `[[func]]` with evidence and `impl`,
   `[[data]]` for each table you name; the module in `CMakeLists.txt` and
   in `src/hook/inject_all.cpp` (at the end, before `DrawPool_Grow`); a line
   per function in the main checkout's `analysis/calltrace/entries_logic.txt`;
   `docs/<module>.md` and its row in `docs/README.md`; controls planted one
   at a time.

## 4. What the harness does

- **The recorders.** One pool of 256 stand-ins, assigned at start-up: the
  group's callees, then the standard set, then **four hook recorders** of
  the harness's own (keys that no code holds: they are reached only through
  the cells), then one handler recorder per stack-table immediate and per
  `DataTable` entry. A callee's recorder logs its arguments masked (or
  hashed, `deref`), disturbs, answers by its kind, then runs its `effect`.
  A handler's recorder logs `Sprite_Current`, `0x939AD8`, the state bytes
  `+1..+4` and, for a hook or a table with `nargs`, its first word. Answer
  kinds, `deref`, `effect`, `custom`, `kPhase`, `kThrough`: as in
  [`magic_harness.md`](magic_harness.md) section 3.
- **The state** (the regions, compared byte for byte after both passes):
  the 48 task slots; `0x93B8C0..0x93B960` (the current slot, the owner);
  the eight enemy objects and 0x80 bytes past them (the last working
  record's tail); `0x937F80..0x937F98` (`Sprite_Current`,
  `Gfx_ClutStripDirty`, `Frame_Counter`); `0x939AD0..0x939B20` (the current
  enemy, `0x939B1C`); the battle bytes `0x904AA0..0x904BA0` (the three hooks,
  `0x904B8E`, `0x904B90` among them); the message byte `0x939F60`; the three
  party records; `0x8034E0..0x8034F0` (the chapter bytes); the 30 field
  objects; `0x929ED0` (the chapter's flag bits pointer) and `Cond_Flags`;
  the area's enemy rows and data records `0x8C5580..0x8C5A28`;
  `Gfx_PacketNext`; two sprite records of the harness's own and a 16 KB
  packet buffer. About 34 KB; the group's regions after (64 KB in all).
- **Each round**: random bytes; then `Fix`: `Sprite_Current` an enemy (a
  task slot for `kTask`), `0x939AD8` the same two times in three, the
  current slot and owner, the source, a target of 0..10 and an actor of
  0..2, the four slots' phase bytes and owners, **every enemy's `+0xF4` the
  enemy-hook recorder and `+0xF8` / `+0xFC` the harness's two records**,
  **the three hook cells their recorders**, `0x929ED0` a row of
  `Cond_Flags`, `Gfx_PacketNext` the packet buffer; `0x904AAA` =
  `Group::fight`; the current enemy's `+0x100` = `Group::kind`; a
  `kDispatch`'s state byte below `Clone::states`; the seed; a `via`'s state
  byte. Then the arguments (the shape's, then the group's `args`), theirs,
  ours, compare.
- **The disturbance** (two calls in three, after the recorder logs):
  `Sprite_Current` (an enemy, or a slot for `kTask`), `0x939AD8`, the owner,
  the current slot, the target, the actor, the message byte, the round
  flags' low byte, `Frame_Counter`, a field of `Sprite_Current` (`+0..+5`,
  `+8..+0xB`, `+0x2E`, `+0x30`, `+0x48`, `+0x4A`, `+0x4B`, `+0x92`; the state
  bytes kept below `phase_span` when the group sets it), a byte of the
  current enemy's record (**never** `+0xF4..+0x100`: its hook, its tables
  and its kind are pointers and an index the originals follow), the
  chapter's run or step, the battle-end byte `0x904AE8`, the fight byte
  (to `Group::fight` or one of the values boss code compares it with: 0x10,
  0x19, 0x1A, 0x25), or the group's `disturb`; then the group's `settle`.
- **`Clone::via`: driving a function the way its unit does.** For a
  function reached only through another unit's table (BH's helpers are
  nearly all like that), `via = {dispatcher, cell, state_at, state}`: the
  harness sets `Sprite_Current[state_at] = state` after the seed (or, with
  `state_at` 0, the call's first word - a hook table dispatched by its
  argument), plants the pass's function (the copy, then ours) in `cell`,
  calls `dispatcher` - Capcom's code at that address, or whoever owns it by
  then, the same on both passes - and puts the cell back. `StandIn` answers
  the planted function itself, so a dispatcher that is ours by then (it
  reads the cell and calls through the harness) still reaches it. BH drives
  `BossTorast_ActDispatch` through each of kinds 8..11's dispatchers this
  way ([`boss_h.md`](boss_h.md) section 3).
- **The copies.** `bof3::CloneOriginal` with every call re-aimed at its
  recorder (`expected` checked), the stack-table immediates re-aimed, jump
  tables moved into the copy (`JumpTable`); the `DataTable` cells swapped
  for their recorders while the `Run` lasts.
- **The report.** A totals line per `Run`, the coverage (every recorder the
  originals called and how often: the hook recorders as `hook 0x904B64`
  and so on, handlers as `phase 0x...`), and on a difference the first
  twelve rounds (function, log lengths, first differing entry, first
  differing byte as region + offset), per function the rounds it
  mismatched in, then a `Fatal` (exit 3).

## 5. The standard callees

`kStandard` in `boss_harness.cpp`: the band's frontier
([`takeover-queue-bosses.md`](takeover-queue-bosses.md) §1.3,
`tools/boss_rows.py`, 2026-09-28) wherever `symbols.toml` has a signature,
plus what BH's helpers reach, plus three unnamed engine setters, plus BH's
spawn helpers. `BH_OURS` for ours, `BH_THEIRS` for Capcom's; one that
changes hands fails at start-up with a line saying so and moves column.

| Callee | Args (masks) | Answer |
|---|---|---|
| `BattleTask_Create` | kind, parameter (bytes) | a slot 0..47 |
| `BattleTask_FreeCurrent`, `Battle_EnemyDefeated`, `Battle_OpenMsgWindow`, `BattleFx_FreeTask` | - | garbage |
| `BattleEnemy_SetAnimation`, `Battle_RemoveFromTurnOrder`, `Battle_ClearActorBit`, `Battle_SetTargetFlag40`, `Transition_Start`, `Port_DroppedCall` | a byte | garbage |
| `BattleEnemy_ScriptTick`, `BattleEnemy_ScriptTickOnce`, `File_LoadDone` | - | flag |
| `Battle_ActorIsOut` | a byte | flag |
| `BattleWin_DrawMediumBox` (2), `BattleWin_DrawTileRgb` (5), `BattleBanner_Add` (5), `Battle_CopyEnemyData` (2), `Battle_LoadSoundByKey` (2, flag), `LoadDatFile` (1) | whole words | garbage |
| `0x446DE0`, `0x446E00`, `0x446E20` (unnamed, nobody's: `0x904AA0 = 5`, `0x904AA2 = 0`, `0x904AA1` = 1 / 2 / 3 - the end phase's steps) | - | garbage |
| `EnemyData_FindByTag`, `BossActor_Index` (BH's) | the tag (a byte) | a byte 0xFF..7 / 0xFF..0x1D |
| `BossActor_Find` (BH's) | the tag | **a pointer to one of field objects 0..3 (in the compared state), never null** - its callers write through it without a test |
| `BossActor_ClearBit40` (BH's) | the tag | garbage; its effect flips bit 0x40 of one of field objects 0..3 (louder than the real one) |
| `BossActor_CopyFrom` (3: tag, from, what), `BossActor_Clear` (1) (BH's) | | garbage |
| `Msg_OpenScript`, `Msg_SystemPtr` (a short; answers a pointer), `Text_DrawAt` (5), `Text_DrawFont12` (4), `Str_CopyN` (3), `Field_MemberSprite` (2), `Scenario_CallA` (1), `AreaMap_Elevation` (2), `AbilityList_Add` (4, flag) | | garbage unless said |
| `MoveCmd_TestFB` (two shorts), `Flags_Set` / `Flags_Clear` / `Flags_Test` (bits, a byte; `Flags_Test` a bool), `MoveCmd_OpE9` (theirs, 7), `Crt_sprintf` (theirs, 4) | | |
| `Sound_PlayEffect`, `Sound_PlayById` | a short | garbage |
| `Sprite_UpdateScreen`, `Sprite_QueueOverlay`, `Effect_Release` | - | garbage |
| `Sprite_PoseFromSet` (3), `Sprite_SetAnimation` (a byte), `Sprite_SetAnimationAt` (byte, short) | | garbage |
| `Sprite_EnsureAnimation` (a byte), `Sprite_ScriptTick`, `Sprite_ScriptTickOnce`, `Sprite_SetAnimationBank` (a short) | | flag |
| `MagicFx_StepToward` (2), `MagicFx_NearSprite3D` (2, flag), `MagicFx_CenterOnSide` | | |
| `Gfx_CommitPrim` | slot, size (bytes) | garbage; **its effect advances `Gfx_PacketNext` by the size** (inside the harness's buffer), as the real one does - a draw reads the pointer again for its next primitive |
| `Gfx_ClearRect` (4), `Gpu_SetPolyFT4` / `Gpu_SetPolyG3` / `Gpu_SetSprt` (1), `Gpu_SetSemiTrans` (prim, byte), `Gpu_SetDrawMode` (5), `Gpu_GetTPage` (4) | | garbage |
| `Gte_PushMatrix`, `Gte_PopMatrix`, `Gte_RotMatrixYXZ` (2), `Gte_TransMatrix` (2), `Gte_RotTrans` (2), `Gte_SetRotMatrix` (1), `Gte_SetTransMatrix` (1), `Math_Sin`, `Math_Cos` (1) | whole words | garbage (list them `kThrough`, or with `deref`, when the vectors matter) |
| `Rand` (theirs) | - | the harness's Rand (`SetRandHint`, `SetRandFirst`) |

Not in the set, on purpose: the kinds' table entries (`EnemyOp_*`,
`0x4365D0`, `0x436620`, `0x436BC0`, `0x436F00`, `0x437030`, `0x437180`,
`0x437240`, `Port_DroppedCall` in its slots) - list the table
(`DataTable`) and each entry gets a handler recorder; the frontier's other
unnamed engine functions (`0x4373C0`, `0x437450`, `0x4376A0`, `0x4376F0`,
`0x44103A`, `0x441090`, `0x446700`, the item menu's `0x44A010..0x44A4F0`,
`0x454A80`, `0x455290`): the group that calls one reads it and lists it
with its arity (a missing callee is a `Fatal` naming its address).

## 6. What it cannot do, and traps

- **Tell a function's meaning**, or reach what the seed does not: seed each
  compare's two sides (section 7).
- **A table index past its table crashes both sides** instead of counting:
  a dispatcher's state byte past its table jumps through whatever follows
  (often the next table's code, which then runs on both sides). Set
  `Clone::states` for a `kDispatch`; for anything else seed the byte, and
  plant the next entry rather than draw past it.
- **`Clone::via` plants a pointer that differs between the passes.** A
  function that reads the `.data` around its own cell sees the copy's
  address on one pass and ours on the other. Met once:
  `BossTorast_DeathFxFlash` reads a colour at `0x64CA6C + 3 * kind`, and kinds
  46 and 47 land on its own cell (8 mismatched rounds of 6,000 before BH's
  seed kept those kinds out, [`boss_h.md`](boss_h.md) section 3). A
  mismatch whose first differing log entry holds bytes of a code address is
  this.
- **`BossActor_Find`'s stand-in never answers null**, so the three spawn
  helpers' null write (a fault in Capcom's, a `Fatal` in ours) is never
  reached; a caller that tests the answer (`0x494570`) wants its own
  listing with an effect that answers 0 sometimes.
- **A clone with more than 64 call sites** is copied by the fuzz file itself
  (`bof3::CloneOriginal`, every site re-aimed at a trampoline into
  `StandIn`) and handed to the harness as a six-byte `jmp [copy]`
  ([`magic_harness.md`](magic_harness.md) section 5).
- **`CloneOriginal` after Inject is a Fatal**: a group's `Run` must precede
  its own `BOF3_INJECT` lines (the recipe's order). A clone whose call
  reaches a function an earlier module injected is fine (the call is
  re-aimed at the recorder anyway).
- **An entry that opens with a `jmp`** (`BossOp_ScriptTick` is one, five
  bytes) clones only when its call row names offset 0 (the tool's rows do).
- **The tool's table extents are too long**: it reads a table to the next
  address any code or `symbols.toml` names, so a kind's `+1` table followed
  by its sub-tables reads as one (kind 8's "72 code entries" is 12, then
  its `+2` / `+3` / `+4` tables, then its hook table). Count from the code:
  the dispatcher's state values, the next table's address.
- **The flag header before a kind's table is its `+0xFC` and `+0xF8` byte
  tables**: kind 8's state 0 (`0x438E70`) stores `+0xFC = 0x64CA90` and
  `+0xF8 = 0x64CA9C`, the bytes before its pointer table `0x64CAA4`. They are
  read in place by `BattleEnemy_SetAnimation` and the kind's code; not a
  `DataTable`.
- **Counts depend on the build directory**, as in every harness: a
  worktree's and the main checkout's coverage counts differ; judge by 0
  mismatches and controls refused.
- **Effects and every group callback draw from `Noise()`**, never `Next()`
  or `BH_PICK` (the passes would diverge); an `args` hook that writes memory
  is lost (the input state is captured before it: plant in `Seed`, keep a
  global for `args`, as `boss_spawn_fuzz.cpp`'s `g_tag`).

## 7. Seeds that matter for boss code

- The **fight byte** `0x904AAA`: `Group::fight` sets it; seed the other
  values a shared function compares it with (0x10, 0x19, 0x1A, 0x25) in
  `Seed` when yours does.
- The **state bytes** `+1..+4` of `Sprite_Current`: `Clone::states` for a
  dispatcher, a pick per table entry otherwise (a state per entry, not
  random bytes: the round's lesson).
- **HP `+0xA4`** against each threshold the code compares, and 0xFFFF
  (`cmp word [eax+0xA4], 0xFFFF` is the dead test).
- The **counters** a state counts down (`+9`, `+0xA`, `+0x40`, `+0x44`):
  their ends and the halving or stepping sequence, 0, 1, and the signed
  boundary for a `jle`.
- The **kind** `+0x100` when the code indexes by it (BH's colours): the
  kinds of the table and some others.
- The **battle-end byte** `0x904AE8` bits 0 and 1, the **chapter bytes**
  `0x8034E4` / `0x8034E5` when a hook reads them before it writes.
- The **field actors**: `+6 = 7` and the tag at `+0x9E` in some of the 30
  objects, decoys with the tag and another type (`boss_spawn_fuzz.cpp`).

## 8. A worked set-up and a worked kind

### 8.1 Set-up 1 (`B01`, `BOSS001`, area 24 row 4, kinds 6 and 7 - the tool's names Gary, Mogu)

`tools/boss_rows.py --unit B01 --clones` gives four functions (read them;
this is the shape, not a reading of their bodies):

| Address | What (by the code) | Shape |
|---|---|---|
| `0x437CD0` | stores `0x904B64 = 0x437DE0`, `0x904B68 = 0x437E10`, `0x904B6C = 0x437CF0`; `ret` | `kSetup` |
| `0x437CF0` | the event hook: a seven-entry jump table by the phase code (`JumpTable {0x15, 0xCC, 7}`), `al` 0 on every path | `kEvent`, `ret_mask 0xFF` |
| `0x437DE0` | the end hook: by `0x904AE8` bit 0 the chapter bytes, then `jmp 0x446E20` | `kEnd` |
| `0x437E10` | the exit hook: `BossActor_ClearBit40(6)`, `BossActor_CopyFrom(6, enemy 0, 1)`, `BossActor_Find(6)` into `Sprite_Current`, `Sprite_SetAnimationBank(0x5F)`, ...; the same for tag 7 and enemy 1 | `kExit` |

Ours stores the hooks as literals:

```cpp
extern "C" void __cdecl Boss01_Setup(void) {
    SetLong(At(0x904B64), 0x437DE0);   // BattleHook_End (name them once taken)
    SetLong(At(0x904B68), 0x437E10);
    SetLong(At(0x904B6C), 0x437CF0);
}
```

and its fuzz is one `Run` with `fight = 1`:

```cpp
namespace bh = boss_harness;
using S = bh::Shape;
constexpr bh::JumpTable kTables437CF0[] = {{0x15, 0xCC, 7}};
constexpr bh::CallSite kCalls437DE0[] = {{0x17, 0x446E20}, {0x22, 0x446E20}};
constexpr bh::CallSite kCalls437E10[] = {{0x3, 0x4949D0}, {0x11, 0x4949F0}, {0x18, 0x494920}, /* ... the tool's rows */};
const bh::Clone kClones01[] = {
    {"Boss01_Setup", 0x437CD0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(Boss01_Setup), 0, false, S::kSetup},
    {"Boss01_Event", 0x437CF0, 0xE8, nullptr, 0, nullptr, 0, kTables437CF0, 1, BH_FN(Boss01_Event), 0xFF, false, S::kEvent},
    {"Boss01_End", 0x437DE0, 0x27, kCalls437DE0, 2, nullptr, 0, nullptr, 0, BH_FN(Boss01_End), 0, false, S::kEnd},
    {"Boss01_Exit", 0x437E10, 0xC9, kCalls437E10, 10, nullptr, 0, nullptr, 0, BH_FN(Boss01_Exit), 0, false, S::kExit},
};
void Seed01(unsigned k) {
    // the event hook's reads: the actor 0x904B34, 0x904B35, the pointer 0x904B40 (seed it into a region),
    // 0x904AA8 bit 6, the target, 0x904AAD bit 0; the end hook's 0x904AE8 bit 0 ...
}
bh::Group g{"boss_bsa", kClones01, 4, nullptr, 0, nullptr, 0, nullptr, 0, &Seed01, nullptr, 6000};
g.fight = 1;
bh::Run(g);
```

What the harness does for it: `Boss01_Setup`'s three stores land in the
compared battle bytes and in the logged hooks; the event hook is called with
a phase code 0..6 and its `al` compared; the exit hook's `BossActor_Find(6)`
answers one of field objects 0..3, which `Sprite_Current` then points at, so
the writes through it are compared; `0x446E20` is a standard recorder.

### 8.2 Kind 8 (`K08`, Torast - the tool's name; area 27)

| Address | What (by the code) | Shape |
|---|---|---|
| `0x438E50` | `jmp [0x64CAA4 + 4 * Sprite_Current +1]` - the kind's `+1` table, 12 entries (state 0 `0x438E70`, then `EnemyOp_*` and BH's `BossTorast_ActDispatch` at 6) | `kDispatch`, `state_at 1`, `states 12` |
| `0x438E70` | state 0: `0x939AD8 +0xFC = 0x64CA90`, `+0xF4 = 0x4390E0`, `+0xF8 = 0x64CA9C`, `Sprite_Current +1 = 2`, `jmp Sprite_ScriptTick` | `kState`, `ret_mask 0xFF`, calls `{{0x38, 0x5893A0}}` |
| `0x4390E0` | the `+0xF4` hook: `jmp [0x64CB04 + 4 * (word & 0xFF)]`, 3 entries (all `Boss_Nop`) | `kEnemyHook` |

```cpp
const bh::DataTable kTables08[] = {{0x64CAA4, 12}, {0x64CB04, 3, 4, 1}};   // the +1 table; the hook table, one word
const bh::Clone kClones08[] = {
    {"BossTorast_Dispatch", 0x438E50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(BossTorast_Dispatch), 0, false, S::kDispatch, 1, 12},
    {"BossTorast_Enter", 0x438E70, 0x3D, kCalls438E70, 1, nullptr, 0, nullptr, 0, BH_FN(BossTorast_Enter), 0xFF, false, S::kState},
    {"BossTorast_Hook", 0x4390E0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(BossTorast_Hook), 0, false, S::kEnemyHook},
};
bh::Group g{"boss_bsb", kClones08, 3, nullptr, 0, kTables08, 2, nullptr, 0, &Seed08, nullptr, 6000};
g.fight = 8;
g.kind = 8;
bh::Run(g);
```

Ours of the dispatcher reads the table cell and calls it as read (section
2), aborting past 12; ours of state 0 stores the three literals into
`0x939AD8`'s object (re-read before each store if the original re-reads) and
returns `BH_CALL(Sprite_ScriptTick)()`; ours of the hook aborts past its 3.
The hook's word is drawn 0..2, so every entry of `0x64CB04` is reached; the
dispatcher's state byte is drawn below 12, so every `+1` entry is.

## 9. Shadow names and self-test

The harness has no shadow name of its own: each group's is its module's
(`boss_h`, `boss_spawn` for BH). `BOF3X_SHADOW='*'` runs every group of
every harness. BH's figures (this worktree, 2026-09-28): section 3 of
[`boss_h.md`](boss_h.md).
