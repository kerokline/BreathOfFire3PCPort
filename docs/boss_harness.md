# The boss harness: one fuzz for every boss group

**Status:** MEASURED (2026-09-28, widened the same night) - **round twelve's
battle groups (BE1..BE7): read section 10 first.** Group EH widened the
harness to the battle engine's address runs at the round's base `430f34b`
(four engine shapes, `Clone::state_cell` / `Via::state_cell`,
`Group::engine` with its regions, pointers, disturbance and 120 standard
callees, 115 of them new: with the boss set they cover all 160 functions the
308 call outside themselves), proved
by a self-test of its own with Capcom's code on both sides
(`boss_harness_eh`: 11 functions, 44,000 rounds, 0 mismatches, nine controls
all refused by a count) and by every boss group's counts unchanged under
`BOF3X_SHADOW='*'` (section 10.8). What follows up to section 9 is round
eleven's harness, unchanged for a boss group.

Round eleven's status: MEASURED (2026-09-28) - built by group BH of round eleven's wave
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

**Since round eleven's cleanup (2026-09-28,
[`round-11-cleanup.md`](round-11-cleanup.md) section 1)** the harness carries
what stage B worked around: `OtherStates` (two overloads) for the seeds, the
louder standard stand-ins (`Battle_RemoveFromTurnOrder`, `0x446DE0`,
`Msg_OpenScript`, `Scenario_CallA`, `Battle_OpenMsgWindow`) and the opt-in
`CreateMayFail`, `Clone::states` honoured for a `kTask` (drawn after the
seed), the disturbance's case 11 below `phase_span`, and a log line for a
handler listed in tables with different `nargs`. The `'*'` run at that tip is
owed; the cleanup doc says which groups' counts it may move.

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
   ones your tables hold (`BossOp_ScriptTick`, `BossOp_Death`, `BareRet`,
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
  `kDispatch`'s state byte below `Clone::states`; the seed; a `kTask`'s
  state byte below `Clone::states` (after the seed, so it wins over an
  owner that is the slot itself); a `via`'s state byte. Then the arguments (the shape's, then the group's `args`), theirs,
  ours, compare.
- **The disturbance** (two calls in three, after the recorder logs):
  `Sprite_Current` (an enemy, or a slot for `kTask`), `0x939AD8`, the owner,
  the current slot, the target, the actor, the message byte, the round
  flags' low byte, `Frame_Counter`, a field of `Sprite_Current` (`+0..+5`,
  `+8..+0xB`, `+0x2E`, `+0x30`, `+0x48`, `+0x4A`, `+0x4B`, `+0x92`; the state
  bytes kept below `phase_span` when the group sets it), a byte of the
  current enemy's record (**never** `+0xF4..+0x100`: its hook, its tables
  and its kind are pointers and an index the originals follow; its state
  bytes `+1..+4` below `phase_span` too, since the cleanup), the
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
| `BattleTask_FreeCurrent`, `Battle_EnemyDefeated`, `BattleFx_FreeTask` | - | garbage |
| `Battle_OpenMsgWindow` | - | garbage; **its effect moves the banner's character `0x66972D`** when that cell is one of the group's regions (`BannerCharEffect`, BSC's) |
| `BattleEnemy_SetAnimation`, `Battle_ClearActorBit`, `Battle_SetTargetFlag40`, `Transition_Start`, `Port_DroppedCall` | a byte | garbage |
| `Battle_RemoveFromTurnOrder` | a byte | garbage; **its effect moves a party record's `+0x91` bit 0x40, `+8` or `+0`** - the end hooks read them after (`TurnOrderEffect`, the five groups' copy) |
| `BattleTask_Create` with `CreateMayFail` listed by the group | | a slot, or **0xFF a third of the time** (opt-in: seven originals index by the answer untested, D163) |
| `BattleEnemy_ScriptTick`, `BattleEnemy_ScriptTickOnce`, `File_LoadDone` | - | flag |
| `Battle_ActorIsOut` | a byte | flag |
| `BattleWin_DrawMediumBox` (2), `BattleWin_DrawTileRgb` (5), `BattleBanner_Add` (5), `Battle_CopyEnemyData` (2), `Battle_LoadSoundByKey` (2, flag), `LoadDatFile` (1) | whole words | garbage |
| `0x446DE0`, `0x446E00`, `0x446E20` (unnamed, nobody's: `0x904AA0 = 5`, `0x904AA2 = 0`, `0x904AA1` = 1 / 2 / 3 - the end phase's steps) | - | garbage; **`0x446DE0`'s effect Notes the chapter step `0x8034E5`, then moves it** (`EndWinEffect`, BSH's form: the store before the call is compared, not wiped) |
| `EnemyData_FindByTag`, `BossActor_Index` (BH's) | the tag (a byte) | a byte 0xFF..7 / 0xFF..0x1D |
| `BossActor_Find` (BH's) | the tag | **a pointer to one of field objects 0..3 (in the compared state), never null** - its callers write through it without a test |
| `BossActor_ClearBit40` (BH's) | the tag | garbage; its effect flips bit 0x40 of one of field objects 0..3 (louder than the real one) |
| `BossActor_CopyFrom` (3: tag, from, what), `BossActor_Clear` (1) (BH's) | | garbage |
| `Msg_OpenScript` | a short | garbage; **its effect moves the script bits `0x904AAD` half the time** (`ScriptBitsEffect`, BSC's) |
| `Scenario_CallA` | a word | garbage; **its effect moves the move counter `0x903848` half the time** when that cell is one of the group's regions (`MoveCounterEffect`, BSC's) |
| `Msg_SystemPtr` (a short; answers a pointer), `Text_DrawAt` (5), `Text_DrawFont12` (4), `Str_CopyN` (3), `Field_MemberSprite` (2), `AreaMap_Elevation` (2), `AbilityList_Add` (4, flag) | | garbage unless said |
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
- **An effect that overwrites a cell the caller may have stored before the
  call wipes the store** instead of comparing it: `Note()` the old value
  first (`EndWinEffect`; BSH's `0x446DE0` left two controls unrefused until
  it did, round 11 doc section 5.3).
- **A handler address in two `DataTable`s with different `nargs`** takes
  the first-listed table's; a `Callee`'s `nargs` wins over a table's. `Run`
  logs each such conflict once; BSA's k39, BSF's K33 and BSI's k58 rely on
  the order (`BareRet` in a hook table and a state table), so list the
  table whose `nargs` the run wants first.
- **A `kDispatch`'s ours forwards the caller's word and answers the entry's
  eax** (the original's `jmp` leaves both in place); the standard
  `Port_DroppedCall` at one argument refuses a dropped word. A group whose
  ours does not forward overrides the listing with 0 arguments (BSA, BSE).
- **The other state bytes are the seed's**: `OtherStates(drawn, below)` /
  `OtherStates(at, n1, n2, n3)` draw them inside their tables, or a hook
  plant runs past its table and refuses only by a Fatal (BSA's 13, BSI's
  first 18).

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
| `0x4390E0` | the `+0xF4` hook: `jmp [0x64CB04 + 4 * (word & 0xFF)]`, 3 entries (all `BareRet`) | `kEnemyHook` |

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

## 10. Round twelve: the battle engine's groups

Group EH of round twelve's wave one, stage A (2026-09-28 night, the round's
base `430f34b`, tree-identical to `7e382c3`), for the seven battle groups
BE1..BE7 ([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md)
section 3; the 308 rows `BE*` of `analysis/round12_cut.tsv` are the
authority). EH took no function. **A boss group sees none of this**: every
addition is a field with a default that reproduces round eleven's harness,
and every new draw, region and stand-in is behind `Group::engine` or a new
shape - section 10.8 shows every boss group's counts unchanged.

### 10.1 What changed, in one table

| What | Where | A boss group |
|---|---|---|
| Four shapes: `kStep`, `kWindow`, `kMember`, `kHelper` (10.3) | `Shape`, appended after `kCallee` | never uses them (a Fatal without `engine`) |
| `Clone::state_cell`: the dispatched byte at an absolute address (a step table's `0x904AA2`, BATE's `0x929F01`) for `kDispatch`, `kStep` (before the seed) and `kTask` (after it) | appended after `Clone::via`, default 0 | 0: `Sprite_Current[state_at]` as before |
| `Via::state_cell`: plant through a dispatcher that indexes by an absolute byte | appended after `Via::state`, default 0 | 0: as before |
| `Group::engine`: the engine frame (regions 10.4, pointers, the disturbance's case 12, `kEngineStandard` 10.5) | appended after `Group::kind`, default false | false: the boss frame exactly |
| The bands: `at::kBossBand`, `at::kEngineBands`, `InEngineBands` (10.2) | `boss_harness.h` | not consulted |
| Capacity: 512 stand-ins (256), 64 regions (40), 128 KiB of state (64) | `boss_harness.cpp` | capacity only: no draw moves |
| The MISMATCH report names the first differing log entry's recorder and its words on each side (the first three times) | `Run` | a log line more on a difference, nothing on a pass |
| Engine helpers for seeds: `WindowAt`, `CurrentWindow`, `TextBuffer`; engine effects `WindowAllocEffect`, `WindowFreeEffect`, `TextPtrEffect`, `SprintfEffect`, `TextArg0/3/4Effect`, `BannerTextEffect` | `boss_harness.h` | unused |

### 10.2 The band, and every place it was a test

The plan said the band constant `0x437A00..0x441000` becomes the union of
the battle runs. The harness never tested it: the constant lived in a
comment of `boss_harness.h` and in `tools/boss_rows.py`. Every place an
address range decides something, and what each accepts now:

| Place | Test | Accepts |
|---|---|---|
| `Run`, per clone (new) | an engine group's clone must lie in `at::kEngineBands` - `0x42D7A0..0x4552F6` (BATE's windows to the last debt row `0x455290`'s 0x66 bytes), `0x4CEB40..0x4CF4A4` (BMAGIC's four), `0x597FC0..0x59DB61` (the battle windows) - else a Fatal naming it (a cut mistake). A clone outside the image (a group's six-byte `jmp [copy]` wrapper, section 6) is not checked | every `BE*` row of the cut; the boss band lies inside the first run |
| `Register` | a callee listed by address must lie in `.text` `0x401000..0x5C3000`; one listed by name must not (it is ours) | all three runs (`0x59DB61 < 0x5C3000`) |
| `Readable` (new, the text effects) | a string argument is followed only inside the compared state, the text buffer, the loaded image (`SizeOfImage` from its header) or this thread's stack | - |
| `Clone::via`, handlers, `CloneOriginal` | no range test | - |
| `tools/boss_rows.py` `BAND_LO` / `BAND_HI` | the boss round's enumeration from its three root sets | unchanged: it cuts boss units, not engine bands; round twelve's clone tables are `tools/band_rows.py` (group RT) |
| `docs/boss-rows.md`, `takeover-queue-bosses.md` | the boss band as a fact of round eleven | unchanged |

### 10.3 The call shapes of the 308

How each of the 308 is reached (capstone over each cut extent; every dword of
the image equal to the entry, classified; `pc_funcs.json`'s callers):

| Group | through a `.data` table | a stack table's immediate | called directly | not a function |
|---|--:|--:|--:|--:|
| BE1 | 28 (BATE by `0x929F00..02`; the phase steps by `0x904AA2..AA4`; `BattleResult_*Steps`) | - | 11 | - |
| BE2 | 15 (`EnemyOp_*`, by an enemy's `+1..+3`) | 21 (`BattleFx_Dispatch` slots and their own sub-tables by `+1`) | 12 | - |
| BE3 | 41 (`EnemyOp_*`; `BattleObj_*` by a member's `+1` / `+2`) | - | 6 | - |
| BE4 | 25 (`BattleItemCmd_*` by `0x904AA4`) | - | 31 | - |
| BE5 | 39 (the Dragon run's step tables at `0x64ECE8..` by `0x904AA4`; `Effect_Handlers` slots) | - | 7 | 1 (`0x44B8D0`) |
| BE6 | 15 (`0x64F0D0` by a task's `+1`; BMAGIC's `MapCell_Handlers` slots) | 3 | 21 | 1 (`0x452460`) |
| BE7 | 1 | 20 (window handlers by the record's `+2` / `+3`) | 10 | - |

Round eleven's nine shapes cover the enemy states (`kState` / `kDispatch`,
`Sprite_Current` an enemy) and the effect tasks (`kTask`, now with
`state_cell` too). What they did not cover, and the shape added for it:

| Shape | Reached by (in the 308) | How the engine calls it | What the harness does |
|---|---|---|---|
| `kStep` | a phase, menu or mode step table indexed by an absolute byte: `Battle_InputSteps` / `BattleAction_KindSteps` / `BattleItemCmd_*` by `0x904AA2` / `AA3` / `AA4`, BATE's by `0x929F00..02`, the Dragon run's by `0x904AA4`; `Effect_Handlers` slots | the dispatcher's `jmp [table + 4 * byte cell]`: `void (void)` | `Sprite_Current` a party member or an enemy (half and half); with `states` and `state_cell`, that byte drawn below `states` before the seed. Its dispatcher is a `kDispatch` with the same `state_cell` |
| `kWindow` | a window's state handler: `Window_Handler4Kinds`' stack table, a window dispatcher's own (`0x598890`, `0x597FA0`, `0x598DC0` ...) | `mov ecx, [0x905B84]; call [esp + 4 * byte [ecx + 2 or 3]]`: `void (void)` | `0x905B84` one of the 22 window records (every engine round); with `states`, the record's byte `state_at` drawn below it before the seed (the dispatcher's immediates are its `Imm`s) |
| `kMember` | a battle object's state: `BattleObj_StateTable` by `+1`, its sub-tables (`BattleObj_SwingSubs`, `_CastDoneSubs`, `_State12Subs`) by `+2` | `BattleObj_RunState`: `void (void)`, `Sprite_Current` a member's ObjTrio record | `Sprite_Current` a party member, `Field_State` the same member three times in four; with `states`, `Sprite_Current[state_at]` drawn below it before the seed |
| `kHelper` | a cdecl helper its callers call directly: the action's begin (`0x435AB0..`), damage (`0x446110`, `0x4461B0`), the target picker `0x447F40`, the transformation's helpers, buffs, BMAGIC's slots (three words: `DrawLayer_Open`'s record, byte 1, byte 0), the window draws (`0x5982D0`, `0x599780` ...) | 0..10 words, `eax` / `al` / `ax` out | the group's `args` (garbage otherwise), the answer by `ret_mask` (0xFF al, 0xFFFF ax, 0xFFFFFFFF eax); `Sprite_Current` a member or an enemy - `kCallee`'s contract in the engine frame |

Every engine round also points `0x905B84` at a window record, `Field_State`
at a member, the menu actor `0x939EC4` at a member and its command record
`0x939FA0` at that member's `+0x124`, the acting sprites `0x904B3C` /
`0x904B40` at the harness's two records, the result record `0x904B60` at a
member's or an enemy's `+0x104` (every pointer cell the 308 dereference that
the boss frame did not set: a `mov r32, [abs]` followed by a memory operand
on r32, scanned over the 308), and puts a NUL every 16th byte of the text
buffer. The engine disturbance (case 12, which is the chapter bytes' for a
boss group) repoints `0x905B84`, moves a byte of that record (`+2` / `+3`
below `phase_span` when set), repoints `Field_State`, the menu actor and its
record, moves a step byte `0x904AA1..AA4` (below `phase_span` when set), or
repoints the result record. A group's `disturb` is still case 14.

**Worked example 1: a cdecl helper**, `0x4457F0` (BE4, not hidden, 0xB1
bytes; its callers `Battle_SpawnActorCopies`, `ItemMenu_CanUseSelected` and
`0x447F40`): one word, of which it uses the low byte - an actor, searched
downward within its side (0..2, 3..10) with `Battle_ActorIsOut` at each, the
argument slot itself the counter and pushed whole; answers an actor in `al`,
0xFF for none. The name below is a placeholder; BE4 names it. The clone row, the args hook and the
group, as a BE4 agent would paste them:

```cpp
namespace bh = boss_harness;
using S = bh::Shape;
constexpr bh::CallSite kCalls4457F0[] = {{0x17, 0x4456C0}, {0x42, 0x4456C0}, {0x6A, 0x4456C0}, {0x93, 0x4456C0}};
const bh::Clone kClones[] = {
    {"Battle_FindTarget", 0x4457F0, 0xB1, kCalls4457F0, 4, nullptr, 0, nullptr, 0, BH_FN(Battle_FindTarget), 0xFF, false, S::kHelper},
};
// the actor byte: the party's 0..2, the enemies' 3..10 and beyond, garbage above it half the time
void Args(unsigned, std::uint32_t* a) {
    a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? bh::Next() % 13 : bh::Next() & 0xFF);
}
bh::Group g{"battle_e4", kClones, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, 6000};
g.args = &Args;
g.engine = true;
bh::Run(g);
```

`Battle_ActorIsOut` is a standard recorder (a byte, `kFlag`), so its answer
is 0 a third of the time and the loops' every exit is reached; ours calls it
`BH_CALL(Battle_ActorIsOut)(actor)` and answers the byte. The self-test runs
exactly this row (10.8): 4,000 rounds, 0 mismatches, control 8 (`cmp bl, 4`
for 3) refused in 139.

**Worked example 2: a hidden state handler and its dispatcher**, `0x441A30`
and `0x441A10` (BE3, both hidden in `BattleObj_PickPose` `0x4412B0`'s
catalogue extent). `0x441A10` is `jmp [0x64E07C + 4 * Sprite_Current[+2]]`
(`BattleObj_StateTable` entry 6 of `0x64DFE0` reaches it by `+1`); `0x441A30`
is entry 0 and 2 of `0x64E07C`: `Sprite_Current +0x4B` and `+0x58` into
`Field_State +0x12F` and `+0x140`, `+4 = 0`, `+2 = 1`. `BattleObj_StateTable`
has states 0..12 ([`battle_obj_states.md`](battle_obj_states.md)); seed a
member's other state bytes inside their tables (`OtherStates`).

```cpp
const bh::DataTable kTables[] = {{0x64E07C, 2}};   // count from the code, not the tool: the self-test draws the first two
const bh::Clone kClones[] = {
    {"BattleObj_Swing2Dispatch", 0x441A10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(BattleObj_Swing2Dispatch), 0, false, S::kMember, 2, 2},
    {"BattleObj_Swing2Enter", 0x441A30, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(BattleObj_Swing2Enter), 0, false, S::kMember},
};
bh::Group g{"battle_e3", kClones, 2, nullptr, 0, kTables, 1, nullptr, 0, &Seed, nullptr, 6000};
g.engine = true;
bh::Run(g);
```

Ours of the dispatcher reads the cell and calls it as read (section 2),
aborting past its table's length; ours of the handler writes through
`Sprite_Current` and `Field_State`, each re-read where the original re-reads
it. The names here are placeholders; BE3 names them. The
self-test runs both (4,000 rounds each, 0 mismatches; control 4, `+2 = 2`,
refused in all 4,000).

A step, for completeness: `0x42F5E0` is `jmp [0x64AEB4 + 4 * byte 0x904AA3]`
over five entries (`0x42F5F0 .. 0x42FAB0`; the sixth dword is `0x76767676`,
not code), so `{..., S::kDispatch, 1, 5, {}, 0x904AA3}` with
`DataTable {0x64AEB4, 5}`, and its entries are `kStep`s; driving `0x42F640`
through it is `Via {0x42F5E0, 0x64AEB8, 0, 1, 0x904AA3}`.

### 10.4 The regions and seeds

Standard for an engine group (three or more of BE1..BE7 touch each; the
touches are absolute operands and `base + disp` operands over each extent):

| Region | Size | What | Groups |
|---|--:|---|---|
| `0x803160` `WindowRecords` | 22 x 0x24 | the window records (`+0x93` window 4's `+3` is written by four groups) | BE1..BE6 |
| `0x905B84` | 4 | the record a window handler runs for (a pointer, set every round) | BE7 (every `kWindow`) |
| `0x905D98` `Field_State` | 4 | a member (a pointer, set every round) | BE2, BE3, BE4 |
| `0x903A50..0x903B24` | 0xD4 | `DrawItemPool_Top`'s tail `0x903A5A`, `CharacterRecords`' `0x903A70`, `Field_ActorStates[0]` | BE1..BE4, BE6, BE7 |
| `0x9045FC..0x904654` | 0x58 | the 18 dwords at `0x904608` and their neighbours | BE2, BE5, BE6, BE7 |
| `0x939EC0..0x939F60` | 0xA0 | the menu actor `0x939EC4` (a pointer, set), the transformation's cells `0x939EE0..0x939EF2` | BE3..BE7 |
| `0x939F64..0x93A000` | 0x9C | `0x939F86`, `0x939F9B`, the command record `0x939FA0` (a pointer, set), `0x939FC0..`, `0x939FFC` | BE3, BE4, BE5 |
| `0x929F00..0x929F20` | 0x20 | BATE's mode bytes, `0x929F04` / `06`, `Field_Kind2Hold`, `MapView_Elevation` | BE1, BE4, BE7 |
| `0x7E1BE8` | 8 | `Input_Held`, `Input_Pressed` | BE1, BE4, BE5 |
| `0x90358C` | 8 | `Field_ConfirmButtons`, `Field_CancelButtons` | BE1, BE4, BE5 |
| the text buffer | 0x200 | strings `TextPtrEffect` answers, NUL-ended every 16 bytes | the engine stand-ins |

The boss frame's regions stay under them (the party, the enemies, the task
slots, the battle bytes to `0x904BA0`, `Cond_Flags`, the enemy rows, the
packet pointer ...): the party (all seven groups), the battle bytes (all
seven), `Sprite_Current` (all seven), the enemies (five), the task slots and
the slot / owner cells (four each), `Gfx_PacketNext` (four) were already there.

**A group lists it** (fewer than three groups, or a buffer only one reads):
BATE's sprintf buffer `0x904BA0` and its `0x675E94..0x675EC0` cells (BE1),
`Text_Records` `0x904D00..0x904D0C` (BE1, the result screen), `0x802D24`
(BE1), `0x675ECC..0x675ED4` (BE2), `0x675F18..0x675F1D` and `0x939A60` (BE4 /
BE5), `0x675F48..0x675F57`, `Field_Slots` `0x9035C0`, `Prim_VertexScratch`,
`MapView_ScreenXY`, the matrices `0x5C41FC..0x5C4210` (BE6's BMAGIC),
`0x7E01B8` (`Input_AutoRepeat`'s latch, written by BE4 and BE5 directly),
the draw pool `DrawItemPool_Top` `0x9039D4` and `Draw_OtSlot` `0x92BF19`
(the window draws append through stand-ins; only BE2 writes `0x9039D8`, only
BE6 reads the slot), `0x939A04..0x939A0C` (BE1), `0x939C14` (BE2),
`0x903850..0x903852` and `MoveScript_FAWord` (BE4). Seeds that matter, from
the reads: the step bytes `0x904AA1..AA4` at their tables' lengths and one
past (a dispatcher by one is a `kDispatch` with `state_cell` - never let a
seed draw past a table); a window record's `+2` / `+3` inside the handler's
stack table (`kWindow` `states`); a member's `+1` / `+2` inside
`BattleObj_StateTable` and its sub-tables; the word `0x904B82`
at 0 (`0x42F640`); a `kHelper`'s words inside the tables they index
(`0x446D90` reads `0x656B14[word >> 8]`).

### 10.5 The standard callees of the battle runs

`kEngineStandard` in `boss_harness.cpp`, registered for an engine group only
and **before** `kStandard`, so its louder forms stand. The frontier: every
rel32 call or tail `jmp` from the 308 extents to a function outside them -
**160 functions**, all covered by `kEngineStandard` + `kStandard` (a scratch
check against the source, 0 missing). By family (masks are what the callee
reads: a byte or short where its first read of the word is one, capstone):

| Family | Callees | Answer / effect |
|---|---|---|
| text (the louder forms) | `Msg_SystemPtr` (short), `Item_NamePtr` (2 bytes) | **`TextPtrEffect`**: a NUL-ended string of the text buffer (the caller follows it) |
| | `Crt_sprintf` (dst **mask 0**, fmt, one value) | **`SprintfEffect`**: the format's first 15 bytes and a NUL into dst when dst is the compared state or the stack, noted; a static buffer outside the regions is left alone and the format noted |
| | `Text_DrawAt`, `Text_DrawSmall` (text **mask 0**) | **`TextArg4Effect`**: the string noted (hash, length; "unreadable" for a pointer outside `Readable`), the answer where it ends |
| | `Text_DrawFont8`, `Text_DrawFont12` (text mask 0) | `TextArg3Effect`: noted |
| | `Text_CharCount` (mask 0) | `kByte` 0..17 and the string noted |
| | `BattleBanner_Add` (text mask 0), `BattleBanner_Set`, `_ShowName`, `_ClearAll`, `Item_HelpMessage` | `BannerTextEffect` for `_Add`; garbage |
| windows | `Window_Alloc` (slot, kind byte) | **`WindowAllocEffect`**: the record claimed as the real one (byte 0 free: 1, `+1` kind, `+2` / `+3` 0, the slot; else 0xFF) |
| | `Window_FreeCurrent` | **`WindowFreeEffect`**: `+0`, `+2`, `+3` of `0x905B84`'s record zeroed |
| | `Window_ResetAll`, `ItemMenu_FreeWindows` | garbage |
| menu and window draws | `Menu_DrawPiece` (two shorts), `_DrawPieces`, `_DrawBox` (the flags and the colour bytes: callers load the colour into `al` only - the self-test's first finding), `_DrawBorder`, `_DrawBackdrop`, `_DrawHand` (the third word unused: mask 0), `_DrawIcon`, `_DrawIcon8`, `_DrawScrollBar`; `BattleWin_DrawCommandLabel`, `_DrawCommandCross`, `_DrawPartyStatus`, `_DrawQuadF4`, `_DrawLineAdd`, `_DrawLineHalf`; `Gpu_GetClut`, `Gpu_SetLineF2` / `F3` / `F4`, `Gpu_SetPolyG4`, `Gpu_SetTile`, `Gpu_SetShadeTex`, `Prim_SetTexture` | garbage |
| input | `Input_AutoRepeat` (a short) | garbage |
| the battle engine | `BattleObj_ScriptTick`, `_ScriptTickOnce` (`kFlag`); `BattleObj_PickPose`, `_EndAction`; `BattleQueue_Push`; `BattleTask_ClearAll`; `Battle_ApplyDamage`, `_CalcDamage`, `_BuildTurnOrder`, `_ClearActingFlags`, `_ClearStatus`, `_PlayActorCue`, `_PlayHitSound`, `_ReturnQueuedItem`, `_SetActorBit`, `_SetDamagePopup`, `_SetHitPopup`, `_StatusTint`; `Effect_ApplyResult`; `Formation_ApplyStatMods`; `Char_RecalcStats`; `Field_SlotRelease`; `Gfx_ClutStripCopyRow`; `MapView_SetElevation` | garbage |
| | `Battle_DefaultTarget` | `kByte` 0xFF..10 |
| | `Battle_ReturnTrue`, `Battle_RollPendingFlag`, `EnemyAI_RowDone`, `Area_TestCondition` | `kFlag` |
| | `Battle_WrapIndex` | **`kThrough`** (pure: both sides run it) |
| items, stats | `Inventory_Add`, `_Remove`, `Item_CanUse`, `Stat_AddCap999` (`kFlag`); `Item_EquipMask`, `Item_IconKind`, `Stat_AddClamped`, `PartySet_Select`, `Equip_PreviewSet` (its two out-pointers mask 0: **the group's** - the recorder does not fill them) | garbage |
| sprites, sound, tasks | `Sprite_ReleaseTint`, `_AnimFromSet`, `_LoadPalette`, `_SetClutStp`, `Sprite_SetTint` (`kFlag`), `Sound_StopChannels`, `Task_Restart` | garbage |
| the GTE (BE6's BMAGIC) | `Gte_LoadVertex`, `_LoadVertices3`, `_Rtps`, `_Rtpt`, `_StoreScreenXY`, `_StoreScreenXY3`, `_StoreDepthF4`, `_PrimDepths4_10`, `_PrimDepthFlat4_10`, `_RotMatrix`, `_MulMatrix0`, `_RotTransPers`, `_RotTransPers4` | **`kThrough`**: the callers read results back through pointers into their own frames. The five matrix calls of `kStandard` (`Gte_PushMatrix` ... `Gte_SetTransMatrix`) stay recorders, so the matrix the real ones use is whatever it was - the same on both passes; BE6 lists the family itself if it wants them all one way |
| Capcom's, by address | `0x446F20` / `0x446F50` / `0x446F80` ((a x b) / 100 clamped to 999 / 9999 / 100), `0x5B9450` (the CRT's `memcpy`), `0x494E70` (the eight enemies' `+0..+3` zeroed, compared) | **`kThrough`** |
| | `0x42E0E0`, `0x42E250`, `0x437230`, `0x441510`, `0x44FB30` (none); `0x42E2F0`, `0x452EB0`, `0x452F10` (`al`, `kFlag`); `0x452DD0` (1, `kFlag`); `0x44F1D0`, `0x4CF4B0` (2); `0x44F6A0` (2, the first unread, `kFlag`); `0x590E80` (3: a stat add with a cap); `0x591810` (two bytes, `kFlag`); `0x59DB70` (6) | garbage unless said |

Of `kStandard`, the engine groups reach `Sound_PlayEffect` (83 sites),
`Gfx_CommitPrim` (32), `Battle_ActorIsOut` (25), `Rand` (25),
`Sprite_SetAnimation` (17), `BattleTask_Create` (14), `Gpu_SetDrawMode`,
`Gpu_GetTPage`, `BattleEnemy_ScriptTick`, `LoadDatFile`, ... unchanged.
**Not standard, on purpose**: a function of the 308 itself (its owning group
takes it; the others call it raw until it merges - 10.6), and the entries of
a `.data` table a group dispatches through (list the table: `DataTable`).

### 10.6 The cross-group edges (for the merge order)

A rel32 call or tail `jmp` from one group's function to another's (capstone
over the extents). Until the callee's group merges, the caller calls it raw
(`BH_AT(type, address)`) and lists it in its own `callees`; after, by name.

| Caller group | Caller -> callee | Callee group |
|---|---|---|
| BE1 | `0x42EE00` -> `0x444660`; `0x42EF50` -> `0x446D90`; `0x42FE20`, `0x431C10` -> `0x44A910` | BE4 |
| BE2 | `0x433DA0`, `0x434340` -> `0x442310`; `0x436640` -> `0x437450`; `0x4366B0` -> `0x4376F0`, and tail `jmp` `0x4376A0` | BE3 |
| BE2 | `0x436290`, `0x436BE0` -> `0x446770` | BE4 |
| BE2 | `0x433DA0`, `0x434340` -> `0x453300` | BE6 |
| BE3 | `0x437260` -> `0x4467C0`; `0x441D80` -> `0x446810`; `0x441ED0` -> `0x446770`; `0x4424A0` -> `0x44A910`, `0x44AA90` | BE4 |
| BE3 | `0x442310` -> `0x44FDE0` | BE5 |
| BE3 | `0x441A90`, `0x442890` -> `0x453EB0`; `0x4420A0`, `0x442310` -> `0x453300` | BE6 |
| BE4 | `0x449A00`, `0x449C70` -> `0x44FDE0` | BE5 |
| BE4 | `0x449A00`, `0x449C70` -> `0x453300` | BE6 |
| BE5 | `0x450200`, `0x450680`, `0x450A70`, `0x450EF0` -> `0x447F40` | BE4 |
| BE5 | `0x44B3A0` -> `0x453300`; `0x44FFA0`, `0x450610`, `0x450700`, `0x450E70` -> `0x4525B0` (the Dragon command task) | BE6 |
| BE7 | `0x597FC0` -> `0x432170` | BE1 |

Through tables: the Dragon run's step tables at `0x64ECE8..` that BE5's
dispatchers (`0x44FF10`, `0x450070`, `0x450280`, `0x4506C0`, `0x450B20`,
`0x450F30`) index hold BE6's `0x451480` (the split point of the run at
`0x451480` puts the table's last reader and its entry in different groups).
BE2's `EnemyOp_*` dispatchers (`0x436270`, `0x436620`, `0x436BC0`,
`0x436F00`) read tables that run on into `EnemyOp_Act5Subs` (`0x64B250..`,
BE3's entries `0x437050 .. 0x4373C0`) only if their counts go that far: the
tool's lengths are too long (section 6), so count from the code.

**The order these edges allow**: BE6 calls no other group, and four call it
(BE2, BE3, BE4, BE5); BE4 is called by four too (BE1, BE2, BE3, BE5).
**BE4 and BE5 call each other** (`0x449A00` / `0x449C70` -> `0x44FDE0`;
`0x450200` ... -> `0x447F40`), the one cycle. Callees first:
**BE6, then BE4 and BE5 (either; the second rebinds the first's raw calls),
then BE3, BE2, BE1, BE7**. Any order works, since every group calls
another's raw until it merges; this one leaves the fewest raw calls to
rename after each merge.

### 10.7 What the pass found about the cut and the plan

- **Two starts are not functions.** `0x452460` (BE6, 265 bytes by the cut)
  is case 4 of `0x4523C0`'s switch (its table at `0x452544`); `0x44B8D0`
  (BE5, 277) is three three-instruction case bodies of `0x44B3A0`'s second
  switch (table `0x44B900`, also read at `0x45A6AF`), and its extent runs
  over that table and into the function at `0x44B920`. Each belongs to its
  host; the group takes the host.
- **Three called functions are in no start list**: `0x437230` (after
  `0x437200`'s padding; `0x437200`'s cut size 64 runs 16 bytes into it),
  `0x441510` (after `BattleObj_PickPose`'s switch table) and `0x591810` (after
  `0x5917D0`'s). They are standard recorders by address here; nobody owns
  them this round.
- **Nine of the plan's section-5 live rows are not in the cut**: `0x598810`
  (BE7's), `0x4468B0`, `0x446990`, `0x4469D0`, `0x446F20`, `0x446F50`,
  `0x446F80` (BE4's) and `0x44FB30`, `0x44FCE0` (BE5's) are part 2 or 7 rows
  of the catalogue, not part 4; the recipe enters them, but no group owns
  them. `0x446F20` / `50` / `80` are pure (kThrough here), `0x44FB30` a
  recorder.
- **BATE calls three unlabelled part-7 functions** (`0x42E0E0`, `0x42E250`,
  `0x42E2F0`), and BE5 `0x44F1D0`, `0x44F6A0` - outside every group.

### 10.8 The proof

**(a) Every boss shadow unchanged.** `BOF3X_SHADOW='*'` headless in this
worktree, the build at `430f34b` (before, 8.4 minutes, exit 0) and at EH's
harness commit `656c684` (after, 9.5 minutes, exit 0), every shadow's
totals and coverage lines compared in order (a scratch script over the two
`build/bof3x.log`s): **all 126 `boss_*` runs of the 12 boss shadows are
identical line for line** - rounds, calls to the stand-ins, bytes of state,
regions, 0 mismatches, and every coverage count:

| Shadow | Runs | Rounds | Calls | Before = after |
|---|--:|--:|--:|---|
| `boss_spawn` | 1 | 24,000 | 12,000 | yes |
| `boss_h` | 3 | 130,000 | 2,066,575 | yes |
| `boss_sa` | 10 | 294,000 | 358,401 | yes |
| `boss_sb` | 15 | 416,000 | 519,947 | yes |
| `boss_sc` | 13 | 318,000 | 440,557 | yes |
| `boss_sd` | 13 | 312,000 | 491,548 | yes |
| `boss_se` | 13 | 322,000 | 359,974 | yes |
| `boss_sf` | 6 | 324,000 | 461,616 | yes |
| `boss_sg` | 11 | 318,000 | 557,116 | yes |
| `boss_sh` | 14 | 298,000 | 332,384 | yes |
| `boss_si` | 20 | 330,000 | 302,885 | yes |
| `boss_sj` | 7 | 264,000 | 515,098 | yes |

Of the 213 shadows, 202 are identical; the 11 that moved are
`boss_harness_eh` (new), five area groups (`area_w0b`, `w1b`, `w1e`, `w2b`,
`w3a`) and five spell groups (`magic_fx_reached`, `magic_s16`, `s17`, `s34`,
`s35`) - call counts only, by 4 to 978 in hundreds of thousands, 0
mismatches before and after. Their harnesses and files are untouched; the DLL
they run in grew (the engine set, 512 stand-in templates, a new file), and
those fuzzes store pointers into our DLL in game memory, so their branches
move with the build (section 6, "counts depend on the build directory").
`boss_harness_eh`'s own call count moved the same way between two of EH's
builds (183,220, 183,046, 183,241 at the tip). **(b) The new
shapes run**: `BOF3X_SHADOW=boss_harness_eh` (`src/game/boss_harness_eh.cpp`)
drives eleven of Capcom's functions from the runs - `0x42F5F0`, `0x42F640`
(`kStep`), `0x42F5E0` (`kDispatch`, `state_cell 0x904AA3`), `0x42F640` again
through it (`Via::state_cell`), `0x598DC0` / `0x598DF0` (`kWindow`, the
dispatcher's three immediates), `0x441A10` / `0x441A30` (`kMember`),
`0x4457F0`, `0x453A90`, `0x42D8C0` (`kHelper`; the last one the text family:
six `Msg_SystemPtr` into `Text_DrawAt` / `Text_DrawSmall`, two `Crt_sprintf` +
`Text_DrawFont12`, `Menu_DrawBox` / `Border`, BE1's `0x42DB40` as a recorder of
the test's own). "Ours" is the file's own
copy of the same bytes with every call and immediate routed to
`boss_harness::StandIn`, so both passes are Capcom's code.

    shadow      boss_harness_eh self-test: 44000 rounds over 11 functions (4000 each), 183220 calls to the stand-ins, 0 MISMATCHES; 35952 bytes of state (27 regions) and the stand-ins' log compared

The first two runs were not clean, and each was the harness's, found before
any group depended on it: `Menu_DrawBox`'s colour word listed whole (callers
load the byte into `al`; the upper bytes are the caller's register, different
in the two copies - now a byte) and `Crt_sprintf` logged a fourth word its
callers never push (the caller's frame - now three words). The controls
(`BOF3X_EH_CONTROL=n`, one change in the file's copy):

| n | Planted | Refused |
|--:|---|---|
| 1 | `kWindow` `0x598DF0`: the record's `+4` = 0x5B | 4,000 of 4,000 |
| 2 | `kStep` `0x42F640`: `0x904AA1` = 4 (also the via run's copy) | 2,058 + 2,001 (the via) |
| 3 | `kHelper` `0x453A90`: `sete` for `setne` | 4,000 |
| 4 | `kMember` `0x441A30`: `+2` = 2 | 4,000 |
| 5 | `kWindow` dispatch `0x598DC0`: its stack table's entry 1 aimed at entry 2's handler | 1,362 (the rounds drawing state 1) |
| 6 | `kDispatch` `state_cell` `0x42F5E0`: the byte stored, not loaded (entry 0 always) | 3,177 (the rounds not drawing 0) |
| 7 | `0x42D8C0`: the other `sprintf` format (the engine `Crt_sprintf`'s note) | 4,000 |
| 8 | `kHelper` `0x4457F0`: `cmp bl, 4` for 3 (the side's bound) | 139 |
| 9 | `kStep` `0x42F5F0`: window 4's `+3` (`0x8031F3`) = 2 | 4,000 |

Two byte controls first tried for 5 and 6 (dispatch by the record's `+2`, by
`0x904AA2`) index a random byte past the table and fault instead of counting
(section 6's trap); they were replaced by the two above.

### 10.9 Limits

- **The shapes are a reading of the 308's starts, not of their bodies**: a
  group that finds a function reached two ways lists it once per way (two
  clone rows).
- **`kHelper`'s words are the group's**: the harness draws garbage; a word a
  helper indexes a table by must be seeded inside it (`args`).
- **String effects note at most 64 bytes to a NUL**; a text pointer a
  quieter stand-in answered is noted "unreadable" (0xFFFFFFFF), never
  followed. A caller's stack buffer that no stand-in fills is uninitialised
  and differs between the copy and ours: list the callee with an effect that
  fills it (`Equip_PreviewSet`'s out-pointers, a `sprintf` into a static
  buffer outside the regions such as BATE's `0x904BA0` - list the buffer as
  a region).
- **`kThrough` callees are not logged**: their arguments are compared only
  through what they compute.
- **The engine regions add 1,976 bytes and 11 regions** to the boss frame's
  33,976 bytes and 16 (35,952 and 27 in the self-test's line); an engine group's own regions go after them (64 regions,
  128 KiB in all).
- **Counts depend on the build directory**, as ever; the before / after
  comparison below is one worktree's two builds.
