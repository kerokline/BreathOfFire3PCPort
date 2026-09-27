# The scenario banks, enumerated from their chapter tables

**Status:** MEASURED (2026-09-26) - a static walk, no takeover. The chapter
tables read off the exe by `tools/scenario_roots.py`, every root's call
closure decoded with capstone, the result held against the catalogue
([`remaining-catalog.md`](remaining-catalog.md)). Nothing here is fuzzed or
live-checked yet; the numbers say what a harness could reach, not what one
has.

## 1. The question

The catalogue ranked the scenario banks last, on the grounds that story code
is reached by playing the story, so the routes would have to be saves at
every chapter. This asks whether the banks can instead be *enumerated*
statically, the way the area handlers fall out of the area descriptor table
and the spell effects out of `Magic_Rows`, so that discovery is a script and
the saves are only the live check.

## 2. The tables

The scenario engine reaches a chapter's code through three tables of 20
pointers each, indexed by the chapter byte `Cond_ByteFA` (s8 `0x8034E0`, the
PSX's `0x8014686C`):

| PC table | Per chapter | Read by | PSX |
|---|---|---|---|
| `0x662C80` | a 5-slot vtable | `Field_ModeDispatch` `0x56D690` slot 0 every field frame; `0x56D6D0` slot 1 on an object trigger; `Scenario_StepHook` `0x56D700` slot 2; `Scenario_ArriveHook` `0x56D750` slot 3; `0x56D7A0` slot 4 | `0x801C944C` |
| `0x660B84` | call table A | `Scenario_CallA` `0x5341A0`, `jmp [[table] + n * 4]` | `0x801CDC4C` |
| `0x660BD4` | call table B | `0x5341C0`, the same shape | `0x801CDC9C` |

These are the only chapter-indexed tables in the exe: a byte scan of `.text`
for `movsx eax, byte [0x8034E0]` followed within 24 bytes by
`mov reg, [eax*4 + imm]` finds exactly these three immediates, at the seven
sites named. The sibling established the same three on the disc with 20/20
and 89/89 entries verified
([`loader_records/SCENARIO.md`](../../BreathOfFire3Recomp/docs/loader_records/SCENARIO.md)).

The three tables hold **176 distinct roots**. Chapters 17..19 have a vtable
of `Scenario_NoHook` stubs and one-entry call tables - SCENA17 is the staff
roll and 18, 19 hold nothing on the disc either.

## 3. The walk

From every root, a recursive-descent decode of the function to its extent
(the next recorded or hidden start), following `call` and `jmp` rel32 into
`.text` and every `[imm + reg*4]` pointer table. Three things the first
version got wrong, kept here because they generalise to any walk of this
exe:

- **A switch's jump table lives in `.text` right after its function**, and
  its cases may lie past a neighbouring start when the start lists split the
  function wrongly. A walk that only reads tables in `.data` misses every
  switch; a walk that stops at the extent misses the cases. Intra-procedural
  control flow is followed wherever it lands; only `call` targets, entries of
  a pointer table in `.data` / `.rdata`, and known starts count as functions.
- **A conditional jump into a neighbour's body is a shared tail**, not a new
  function. Treating it as one invented starts at odd addresses.
- **The engine's own functions dispatch into the banks.** `Scena01_Frame`
  and `Scena16_Frame` are ours; refusing to expand through ours lost every
  chapter whose frame is taken over. A function is expanded when its
  catalogue label is one the scenario family could own, or its name is
  `Scena*`, ours or not; anything else it calls is the frontier.

A call target no list records, or a pointer-table entry in `.data` that is
not a listed start, is added as a start and the walk re-run to a fixpoint.
One pass sufficed: **206 starts added**, all 16-byte aligned, all in
`0x530000..0x56FFFF`, which is where the catalogue's SCENA-shaped unlabelled
runs are.

## 4. What the tables reach

| | Functions | Bytes |
|---|--:|--:|
| Walked (the bank code) | 638 | 192,436 of `.text` covered by the decode; 210,160 by extent |
| of them, starts no list had | 205 | |
| of them, hidden (pointer-reached) starts | 347 | |
| of them, already ours | 17 | the chapter 16 demo and chapter 1's frame |
| Frontier (reached, not walked) | 232 | engine code: 98 `Boot: field, map and sprites`, 64 ours, 20 event script, 12 top-level modes, the rest single digits |

Per chapter, walked alone:

| Chapter | Roots (slots / A / B) | Functions | Bytes covered |
|--:|---|--:|--:|
| 0 | 4 / 1 / 0 | 27 | 6,747 |
| 1 | 5 / 3 / 1 | 52 | 15,418 |
| 2 | 4 / 1 / 2 | 78 | 17,760 |
| 3 | 5 / 2 / 1 | 39 | 9,330 |
| 4 | 5 / 2 / 1 | 21 | 4,682 |
| 5 | 5 / 11 / 5 | 57 | 15,860 |
| 6 | 5 / 10 / 8 | 129 | 34,644 |
| 7 | 5 / 2 / 2 | 70 | 18,144 |
| 8 | 4 / 7 / 1 | 36 | 10,187 |
| 9 | 5 / 14 / 10 | 110 | 30,507 |
| 10 | 4 / 6 / 4 | 42 | 14,360 |
| 11 | 5 / 9 / 5 | 38 | 8,276 |
| 12 | 5 / 10 / 5 | 91 | 36,874 |
| 13 | 4 / 8 / 1 | 67 | 23,590 |
| 14 | 4 / 13 / 1 | 41 | 13,028 |
| 15 | 4 / 4 / 1 | 26 | 12,429 |
| 16 | 4 / 1 / 1 | 15 | 3,410 |
| 17..19 | 4 / 1 / 1 | 1..2 | stubs |

Chapters share helpers, so the per-chapter counts sum past the union.

**Chapter 16 is the check.** Its closure is the 13 `Scena16_*` functions the
title-states round took over, plus slot 1 `0x56C080`, which
[`field-modes.md`](field-modes.md) records as the one that stays Capcom's,
plus the `0x43C9F0` stub in slots 2 and 3 - the same list, arrived at from
the tables alone.

## 5. Against the catalogue

| Catalogue label | Functions | Reached from the chapter tables |
|---|--:|--:|
| Scenario event banks (`SCENA`) | 309 | 184 |
| Unlabelled starts in the SCENA-shaped runs `0x53D3B0..0x56AD79` | 212 | 208 |
| `COMMU` | 205 | 1 |
| `SHISU` / `SISYOU` | 58 | 0 |
| `SCE1xEF` | 52 | 0 |
| `PLP` | 18 | 0 |

Two findings follow.

**The hinted runs are the banks.** 208 of the 212 unlabelled starts the
catalogue placed in the SCENA-shaped runs by their touches are in the
closure, and 22 more unlabelled functions outside those runs. The profile
hint the catalogue would not write into the label column is confirmed here
by structure.

**The SCENA label is wider than the chapter tables.** 125 of the 309
SCENA-labelled functions are not reached from them, and the tool says what
does reach each:

| What reaches them | Functions | Where |
|---|--:|---|
| Pointer tables past `EffectKind18_States`, `Effect_KindHandlers`, `EnemyOp_StepsF`; jump tables in BOSS and `Boot: battle` code; and the functions those call | 72 | `0x43C000..0x43F000`, `0x46F000`, `0x47F000..0x494000` - battle-side event scripts and effects |
| `WorldMap_Records` (31 directly), `WorldMap_FieldHooks`, unnamed pointer tables in the world-map data, one top-level mode, and the functions those call | 42 | `0x41F000`, `0x462000..0x465000`, `0x46E000` - the world map's per-chapter hooks |
| Called by area overlays (worlds 1, 3, 4) and by those | 9 | `0x479000..0x47B000`, `0x41F9B0`, `0x5658B0` - area handlers that call story code directly |
| In the bank region, referenced only from unnamed data tables | 2 | `0x55B8B0`, `0x55B960` - unresolved; the one place the walk may be short |

(By address region, from the JSON's `scena_missed`; the tool's own listing
is by reacher and finer.)

On the PlayStation a `SCENA` overlay held everything that chapter needed
that the resident code did not: its field scenes, its world-map hooks, its
event battles' scripts, its effects. The PC linked all of it into the exe,
and the world-map and battle parts hang off *those* subsystems' tables. So
"the scenario banks" as a takeover group are the 638 here; the other 125 are
world-map and battle work and belong with those rounds. The catalogue's row
should be read with that split.

## 6. What this changes

The plan that follows from this section is written out in
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) and listed as
[`IDEAS.md`](IDEAS.md) I23 (owner, 2026-09-26).

The catalogue's reason for ranking the banks last was that discovery needed
saves. It does not: the 638 functions and their roots are a script's output,
reproducible from the exe. The saves remain the *live check* - one recorded
route per chapter - but a fuzz-first wave on the same pattern as the area
handlers and the spell effects is open:

- a harness that sets `Cond_ByteFA` to a chapter and calls its five vtable
  slots and its call-table entries under randomised `Cond_Flags`, the story
  flags at `0x904030`, the scenario bytes `0x8034E0..E9`, `Field_State` and
  the field words the frontier shows the banks touching;
- `Scenario_Start(n)` for any chapter, the way the demo already calls it
  with 16, for the load path.

The order the per-chapter table suggests: 16 is done; 17..19 are stubs;
chapters 0, 4, 8, 11 and 15 are under 40 functions each and share the same
state-machine shape (a frame function switching on `0x8034E5`, a table of
tiny state handlers); 6, 9 and 12 are the large ones.

What the walk cannot see: a function pointer stored into a variable at run
time and called later (a task entry passed through `Task_Create` is followed
when the address is an immediate - 4 of the 638 arrived that way - but a
pointer computed or copied from a structure is not), and any bank code the
world map or a battle calls that is not also reached from the chapter
tables. The 125 above are the measured size of that second gap for the
SCENA label; the first is unmeasured and is what the live check per chapter
would close.

## 7. Reproducing

```
python tools/scenario_roots.py                      # the report above
python tools/scenario_roots.py --chapter 6          # one chapter's roots and closure
python tools/scenario_roots.py --json analysis/scenario_roots.json
```

The JSON holds the roots per chapter, the 206 added starts, the walked set
with and without pushed pointers, each function's first reacher, and the
reacher of every missed SCENA-labelled function. Inputs: `bof3/BOF3.exe`,
`analysis/pc_funcs.json`, `analysis/pc_hidden.json`,
`analysis/remaining_catalog.tsv`, `symbols.toml`.
