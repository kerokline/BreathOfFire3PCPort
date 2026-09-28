# The boss round: the BOSS overlays enumerated from the engine's three root sets, and taken wave by wave

**Status:** PROPOSED (2026-09-28) - a plan and a cut, not a queue. Listed as
[`IDEAS.md`](IDEAS.md) I26; the method is the spell round's
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md),
[`magic_harness.md`](magic_harness.md)) and the area round's
([`takeover-queue-areas.md`](takeover-queue-areas.md)); section 1 is what
makes it apply to the bosses, section 6 what is different. **The tool
exists**: `tools/boss_rows.py` ([`boss-rows.md`](boss-rows.md)); every
number below is its output of 2026-09-28 at 4,554 ours, and the cut in
section 3 is `--groups`. To be scheduled as round eleven after the area
round (round ten's remaining waves) - the owner, 2026-09-28.

## 0. The question, and the answer in one paragraph

After the scenario and area rounds, the catalogue's largest labelled
remainder is the **boss battle scripts**: 372 functions by the label of
2026-09-25 ([`remaining-catalog.md`](remaining-catalog.md)), 525 by this
tool, in one band of `.text`. The owner asked (2026-09-28) whether they can
be staged as the areas were - enumerated from tables, cut into whole units,
fuzzed under a harness, checked live from a save. **They can, and the shape
is simpler than the areas'.** On the PlayStation each BOSS overlay was a
program of its own that the battle engine entered once through a 56-entry
table indexed by the boss id (the sibling's
`docs/loader_records/BOSS.md`, "PROVEN (static)"). The PC linked the 35
distinct images into one band and compiled each enemy's script once behind
a second table, so the engine reaches every boss function through **three
root sets, all readable off the exe**, the closure of which is the whole
band with nothing left over. The live check is one recipe save per fight,
which the owner records after the fuzz.

## 1. The measurement

`tools/boss_rows.py` over `analysis/pc_funcs.json` + `pc_hidden.json` and
`symbols.toml` at `4a15d61` (4,554 ours); the sibling's
`names/boss_records.toml` for the id-to-file map. A run takes about 40
seconds.

### 1.1 The band

**`0x437A00..0x441000`** (30 KiB): **541 functions** (538 listed, 5 starts
dropped as jump-table cases, 8 found by the descent), **16 ours** (the
Paralyzer and Head Cracker rows, `Magic_Rows` 123 and 128 - the spell round
read them as engine rows and they are: their code sits inside the boss band
because the PSX linked them into `BOSS` images), **525 to take, 33,300
bytes**. `Field_StartEventBattle` `0x4410B0` (ours, SE) follows the last
function, `0x440EF0`.

### 1.2 The roots

| Root set | Entries | Who calls through it | What an entry is |
|---|--:|---|---|
| `Boss_SetupTable` `0x656954`, 56 entries by the event-battle byte `0x904AAA` (**unnamed** in `symbols.toml`; the PSX's `Boss_EntryTable` `0x800B2048`, BOSS.md §3) | 55 (entry 0 is the bare `ret` `0x437CC0`) | `Battle_InitBossEncounter` `0x4942A0` (ours, BG): `call 0x494500`, then `jmp [eax*4 + 0x656954]` at `0x4942AC` (`pe_disasm`) | the fight's set-up: it stores the three hooks `0x904B64` / `0x904B68` / `0x904B6C` (`pe_xref`: 57 / 57 / 62 references, every store an immediate in the band) and returns to the dispatcher's caller |
| `BossKind_Table` `0x64B088`, 63 entries by an enemy's kind byte (**unnamed**; `EnemyRunAll`'s event-battle table, [`battle_flow.md`](battle_flow.md): "the same table one entry on; its state-0 entry is a null") | 61 in the band (kinds 19 and 20 are `Port_DroppedCall` `0x4DF820`) | `EnemyRunAll` (ours, round 7) while `0x904AAA` is set | the kind's per-frame dispatcher: `mov al, [sprite + 1]; jmp [eax*4 + T]` through the kind's own state table in `.data` |
| The effect dispatchers' stack tables: `BattleFx_Dispatch` `0x4352A0` slot 8, `BattleMagicFx_Dispatch` `0x435350` slot 93, and the eight-slot **kind-3 dispatcher `0x4357D0`** (unnamed; `BattleTask_RunAll`'s fourth kind, evidence in `symbols.toml` at `BattleTask_RunAll`) slots 2..7 | 8 | `BattleTask_RunAll` by a slot's kind byte `+6`, then the dispatcher by `+5` | a boss-specific effect task (slot 93 is `HeadCrackerRock_Task`, ours) |

**The three hooks.** `0x904B64` is called with nothing pushed from
`0x431464` (the per-frame script), `0x904B68` from `0x4317B9` (the
transition), `0x904B6C` from `Battle_PhaseDispatch` and the action phases
with 0..6 ([`battle_flow.md`](battle_flow.md), [`battle_actions.md`](battle_actions.md),
[`battle_phases.md`](battle_phases.md) - the callers are all ours and their
fuzzes already swap the hooks for recorders). A hook re-points itself as
the fight progresses (`mov [0x904B64], imm` inside the hooks), which is why
a set-up's closure is four functions and not one.

**The kinds' tables.** Each kind's code stores the enemy's `+0xF4` hook and
`+0xF8` / `+0xFC` table pointers (`0x437F00`: `mov [ecx+0xF4], 0x437FF0;
mov [edx+0xF8], 0x64C880; mov [eax+0xFC], 0x64C870`) and jumps through
tables of its own by the sprite's state bytes. The tables lie in `.data`
`0x64C7B0..0x64DDEC` in kind order, each a flag header (bytes, then `FF`
padding, then pointers - the sibling's `Boss021_HandlerTable` shape,
`names/data.toml`) or a bare pointer run; the tool reads a table from the
address the code names to the next address any code or `symbols.toml`
names. Round seven named the first of them `EnemyOp_StepsB..F` /
`EnemyOp_ActSubsB..E` (`0x64C7B0..0x64C928`) as if they were the generic
enemy state's; they are kinds 1 and 2's, and the round renames them.
`EventBattle_Records` `0x64DDEC` (56 records of 4, read by
`Field_StartEventBattle` and `0x494500`) is the PSX's
`Boss_EncounterTable` (flags, arena, formation, file row - BOSS.md §2.3).

### 1.3 What the roots reach

| | Functions |
|---|--:|
| In the band | 541 |
| Reached by exactly one unit | 496 |
| Reached by exactly two units (a shared body: `0x438030` by kinds 1 and 39, seven of kind 43's by kind 50, ...) | 15 |
| Shared helpers, reached by three or more units | 20 |
| Reached by nothing | 10 - **all ours** (the Head Cracker rock's states, reached through the engine rows) |

**So the closure is the band**: nothing in it is reached by nothing, and
nothing outside it is reached that is not engine code. The 20 shared
helpers (1,598 bytes; `0x4394A0` at 527 bytes, `0x43B180` at 254, `0x439030`
at 174 and `0x438F40` at 134 the large ones; `0x437CC0` the bare `ret` that
fills every empty hook slot; `0x43EB60`, `0x43C9F0`, `0x440820` the small
common hooks) are the analogue of the spell round's group L and the
scenario round's SE: one group, first.

**Units** (123): 55 set-ups (1..4 functions each: the entry and its hooks),
61 kinds (3..28 functions: the dispatcher, its state handlers, the tasks it
spawns), 7 effect tasks not yet ours (3..24 functions). Kind 62 (28
functions, `0x43C9A0..0x4406D7`) and the kind-3 dispatcher's slot 5
(`0x440830..0x44103A`, 24 functions) are the largest; the median unit is 3.

**The block rule holds with the set-ups and the kinds interleaved in
address order**: the PC linked the images in the disc's file order, a
fight's set-up beside the kinds it introduces, and a kind that several
fights use (the same enemy in two fights - ids 14 and 46 share
`BOSS014.EMI` = `BOSS046.EMI` on the disc, BOSS.md §5.3) compiled once. The
ids' entries in address order are

```
1 2 3 39 4 5 6 7 13 8 9 10 11 12 16 14 46 15 17 18 19 20 21 22 23 30 24 48
25 26 27 28 29 31 32 33 34 41 35 47 36 43 37 38 44 40 53 42 45 49 50 51 52 54 55
```

which is the sibling's file order exactly where ids share a file (2, 3
and 39 in `BOSS002`; 4, 5, 6 in `BOSS004`; 13 and 16 in `BOSS013`; 34 and
41 in `BOSS034`; 35 and 47; 36 and 43; 38 and 44; 40 and 53; 45 and 49). 17
consecutive unit spans overlap, every one because a shared body or a
kind's table lies inside a neighbour's span (kind 39's `0x438030` inside
kind 1's; kind 62's `0x43C9A0` inside kind 33's); no unit's *exclusive*
functions interleave with another's.

**The frontier** - engine code the band calls, not walked - is 117
functions, **67 ours** (`EnemyOp_*`, `Sprite_*`, `Effect_*`, `Battle_*`,
`Sound_PlayById`, `Music_*`, `Rand`, `Port_DroppedCall`), 3 named not ours
(`MoveCmd_OpE9`, `Crt_sprintf`, `Rand`'s CRT neighbour) and 47 unnamed: the
battle engine's `0x4365D0`, `0x436620`, `0x436BC0`..`0x4373C0` (enemy-state
table entries, PSX `0x801E38F8`..), the encounter set-up's `0x4948E0`,
`0x494920`, `0x494980`, `0x4949D0`, `0x4949F0`, `0x494A60` (`Top-level
modes` by the catalogue: the enemy spawn helpers every set-up calls with a
slot number), `0x446700`, `0x446DE0`..`0x446E20`, the item menu's
`0x44A010`..`0x44A4F0`, `0x454A80`, `0x455290`. A far better starting
position than the scenario round's 172 unnamed; the spawn helpers are a
small engine group of their own (section 7).

### 1.4 What the label overstated, and what it missed

The catalogue's "Boss battle scripts" label (427 on 2026-09-25, 372 today)
was the neighbour fill's; the tool finds 525 to take because 153 of the
band's functions carried other labels or none (`Unlabelled`, the effect
tables). Nothing labelled BOSS lies outside the band.

## 2. What a boss function is, to a harness

One frame of battle state is every input, as it is for a spell. The shapes
the roots give:

| Shape | Called by | Arguments | Answers |
|---|---|---|---|
| A set-up entry | `Battle_InitBossEncounter` through `Boss_SetupTable`, after `0x494500` | none | the three hooks stored |
| The per-frame script hook `[0x904B64]` | `0x431464`, every battle frame | none | may re-point itself |
| The transition hook `[0x904B68]` | `0x4317B9` | none | - |
| The event hook `[0x904B6C]` | `Battle_PhaseDispatch` (3, phase not 0), `Battle_Init` (6), the action phases (1, 4, 0, 5) | one word, the phase code | may re-point itself |
| A kind's dispatcher | `EnemyRunAll` through `BossKind_Table`, `Sprite_Current` the enemy | none | jumps by the sprite's state byte |
| A kind's state handler | the dispatcher through the kind's table | none | sets the next state; `Sprite_ScriptTick` tails |
| An enemy hook `+0xF4` | `EnemyRunAll` with 2 (when `+1` is set) | one word | - |
| An effect task | `BattleTask_RunAll` through the dispatchers' stack tables, `Sprite_Current` the slot | none | the slot's state |

What they read, by the frontier and the functions read so far
([`battle_sprites.md`](battle_sprites.md), [`battle_fx_tasks.md`](battle_fx_tasks.md),
the spell round's engine rows): `0x904AAA` (compared with 0x10, 0x19, 0x1A,
0x25 - fights that share code branch on the id), the battle flags
`0x904AE5` / `0x904AE8`, `Sprite_Current` and `0x937F88` / `0x939AD8` (the
current sprite and enemy), the enemy records `0x93B9E0 + n * 0x128` (their
HP at `+0xA4`, the state bytes `+1`, `+2`, `+4`, the hook `+0xF4`, the
tables `+0xF8` / `+0xFC`), the party records, the chapter bytes
`0x8034E4` / `0x8034E5` (`0x437DE0` sets the scenario's next state on a
flag - a boss hook drives the chapter), `0x904B35`, the round flags
`0x904AA8`, `Rand`.

The harness sets `0x904AAA` to the fight's id and **leaves the real tables
in place** (the kinds' tables are read-only dispatch: swapping their
entries for recorders is what the spell harness's `DataTable` does),
randomises the battle frame as `magic_harness` does, and calls each root
and each hook and state handler under Capcom's and ours, comparing the
state after and the recorders' tapes. Controls as every round's: a mutant
per function the fuzz must refuse. The enemy spawn helpers
(`0x4949D0(slot)`, `0x4949F0(slot, record, 1)`, `0x494920(slot)`) want
typed stand-ins that build a record, as `Battle_SetupEnemy`'s fuzz did.

## 3. The groups

Whole units in address order, the shared helpers first, about 50
functions not yet ours a group (`tools/boss_rows.py --groups`,
2026-09-28). A function two units share goes with its first group; a unit
is never split. **Eleven groups, 525 functions.**

| Group | Units | Band | Fns | Ours | To take | Bytes to take |
|---|---|---|--:|--:|--:|--:|
| BH | H: the 20 helpers three or more units share | `0x437CA0..0x440829` | 20 | 0 | 20 | 1,598 |
| BSA | K06, K07, B01, K01, K39, K02, K46, B02, B03, B39 | `0x437A10..0x43D662` | 49 | 0 | 49 | 2,441 |
| BSB | K03, B04, B05, B06, K04, K05, B07, B13, K08..K11, B08, B09, B10 | `0x438290..0x43A022` | 52 | 0 | 52 | 3,630 |
| BSC | B11, K12, B12, K13, K14, K17, B16, K15, K53, B14, B46, K16, B15 | `0x439410..0x43A589` | 53 | 0 | 53 | 3,448 |
| BSD | K18, B17, K21, K22, K23, K26, K24, B18, B19, B20, K25, B21, K27 | `0x43A590..0x43B74A` | 52 | 0 | 52 | 3,236 |
| BSE | B22, K28, B23, B30, K29, K55, B24, B48, K30, K31, K32, B25, B26 | `0x43B5B0..0x43E7A0` | 52 | 0 | 52 | 3,538 |
| BSF | B27, FB8, K33, K62, B28, F2 | `0x43C480..0x4406D7` | 64 | 10 | 54 | 3,569 |
| BSG | K34, B29, K35, K36, K37, B31, K38, B32, B33, F6, K40 | `0x43CDE0..0x43E535` | 53 | 0 | 53 | 3,931 |
| BSH | K48, B34, B41, K41, K42, K54, B35, B47, K43, K50, B36, B43, F3, K44 | `0x43DEF0..0x43ECC0` | 46 | 0 | 46 | 2,417 |
| BSI | B37, K45, K51, B38, B44, K47, K60, B40, B53, K49, B42, K52, K56, B45, B49, K57, B50, K58, B51, F7 | `0x43ECC0..0x43FE8C` | 51 | 1 | 50 | 2,189 |
| BSJ | K59, B52, F4, K61, B54, FB93, B55, F5 | `0x43F7A0..0x44103A` | 49 | 5 | 44 | 3,303 |

`B<id>` is `Boss_SetupTable[id]`, `K<kind>` is `BossKind_Table[kind]`, `F<slot>`
the kind-3 dispatcher's slot, `FB8` / `FB93` the two other dispatchers'.
The spans overlap because a shared body lies in a neighbour's span (BSA
reaches to `0x43D662` through kind 39's `0x438030`; BSF through kind 62);
**a group owns the functions the tool lists for it**
(`analysis/boss_funcs.tsv`, a function a line with its group), not a band.

## 4. The waves

Two waves. **Wave one is BH and the first five groups** (BSA..BSE, 278
functions): the helpers, then the fights of chapters 0..7 in the order the
game meets them, which is also address order - so the first recipe saves
the owner records are the earliest fights. **Wave two is BSF..BSJ** (247
functions), the later fights and the effect tasks.

Each wave: agents in worktrees, one group each, headless self-tests, merged
one at a time, a round doc per group on `magic_s16.md`'s shape, merges
verified in the detached worktree (round10 doc section 10). Rate-limit cuts
resume. The harness (section 7) goes in wave one as the scenario and area
harnesses did in round ten's wave one, proved on BH and BSA, with the
other four groups staged behind it.

## 5. The live check

Fuzz-only until a route exists, as every spell, scenario and area group
is. The live check per fight is **a recipe save before the fight** played
through it under original and ours with the frame hash compared
([`input-script.md`](input-script.md), `tools/recipe_saves.py`), which the
owner records after the fuzz. The owner's estimate (2026-09-28): one fight
is within reach of a save now; the whole sweep is not, and the round does
not wait for it.

**Which fight is which.** The 53 `push id; call Field_StartEventBattle`
sites in the chapter banks (`tools/boss_rows.py`, the report's last line;
the sibling's BOSS.md §2.1 has the same map for the PSX's 49) give the
chapter of every id: 1 and 4..7 in chapter 0, 3 in chapter 1 (and one
register-passed id), 8..14 in chapter 2, 15 and 16 in chapters 2 and 3,
17..22 in chapter 5, 23..28 in chapter 6, 29..34 in chapter 7 (and 8), 53
and 54 in chapter 9, 35 and 36 in chapters 9 and 10, 37 and 38 in chapters
12 and 13, 49..52 in chapters 13 and 14, 55 in chapter 15; ids 39..48 come
from one register-passed call in chapter 15 (the sibling: the same dynamic
call at `0x801FAEFC`). So the first five groups are chapters 0..7's fights,
and `tools/recipe_saves/adult_ryu` / `combat.txt` (an ordinary encounter)
is the model for the saves; the combat route entered one function of the
band (`0x4AEE90`'s tail, round 8) and no fight.

**Ground truth on the sibling.** Its `docs/loader_records/BOSS.md` proves
the same chain statically (the id byte `0x801462E6`, `Boss_EncounterTable`,
the 56-entry `Boss_EntryTable` entered by one `jalr` at `0x800A8B40`, 55 of
55 entry pcs inside their own file's section) and its residency timeline
records twelve BOSS images loaded across its play sessions (BOSS001, 002,
004, 007, 008, 012, 013, 014, 015, 017, 051, 052 - "resident" means the
band's header id was read, not that the fight was played; 051 and 052 on
early dates are suspect for that reason). It names **no** function inside
a BOSS overlay (`names/functions.toml` has none; `Boss021_HandlerTable` in
`names/data.toml` is the one name), so the round's names come from what the
functions do, and the sibling validates the *shape and the entry points*,
not the bodies. Its per-image function-start counts
(`analysis/overlay_captures_all.json`: BOSS001 23 roots, BOSS018 37, BOSS055
55, BOSS015 6) are the check on ours per file: a PC unit set for one file
should have about that many functions.

What the fuzz cannot see and the fight would: a pointer stored into an
enemy record at run time and called later by engine code the harness does
not run (`+0xF4` with 2 from `EnemyRunAll` is covered; anything the
transition hook arms is not), and the chapter-state writes
(`0x8034E4` / `0x8034E5`) that hand the field back to the scenario.

## 6. What is different from the spell, scenario and area rounds

- **Three root sets, none a table of the unit's own.** A spell's row named
  its overlay; an area's descriptor named its tables; a boss's set-up
  names its hooks *by storing them*, and a kind's dispatcher names its
  tables the same way. The tool follows stored immediates, not fields.
- **The unit is the closure, not the file.** On the disc the unit was the
  BOSS image; on the PC a kind used by two fights is one body of code, so
  the round's units are set-ups, kinds and effect tasks, and the sibling's
  file map is the check on their order, not the cut.
- **Tiny units, as the areas' were**: a median of three functions; the
  group is the unit of work, the unit the unit of evidence (its root, its
  PSX file, its tables).
- **The hooks re-point themselves.** A function's closure includes the
  functions it installs as the next hook; a harness that calls a hook must
  read `0x904B64..6C` after and compare.
- **The engine side is ours already.** Every caller of the three root sets
  and the three hooks is ours (BG, round 7's phases, `EnemyRunAll`,
  `BattleTask_RunAll`), and their fuzzes swapped the hooks for recorders;
  the boss round's fuzz is the other half of the same seam.
- **The band is also the spell round's engine rows' home**: 16 functions
  in it are ours (Paralyzer, Head Cracker) and the tool keeps them in their
  groups' counts as "ours".

## 7. Before the first cut

1. **The tool** - done: `tools/boss_rows.py` ([`boss-rows.md`](boss-rows.md)).
2. **Name the tables** in `symbols.toml`: `Boss_SetupTable` `0x656954` (56),
   `BossKind_Table` `0x64B088` (63), the kind-3 dispatcher `0x4357D0`, and
   the hooks `0x904B64` / `0x904B68` / `0x904B6C` as `[[data]]`; rename
   `EnemyOp_StepsB..F` / `EnemyOp_ActSubsB..E` to the kinds they belong to
   (kind 1: `0x64C890`, `0x64C8C0`, `0x64C8C8`; kind 2: `0x64C8D4`..).
   Half an hour; group BH's, with the helpers.
3. **The harness**, `src/game/boss_harness.*`, from `magic_harness` with
   the eight shapes of section 2 and the battle frame's state; the enemy
   spawn helpers (`0x4948E0`..`0x494A60`, six functions, `Top-level modes`
   by the catalogue) read and taken as a small engine group beside BH, or
   stood in with typed recorders - the reader decides. Proved on BH and
   BSA.
4. **Recipe saves**: the owner's, after the fuzz; the first fight (id 1,
   chapter 0) needs a new game and nothing else.
5. Then wave one.
