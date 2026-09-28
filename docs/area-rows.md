# The area tool: every area's code, found from its tables and cut into blocks

**Status:** MEASURED (2026-09-27, round ten, group ART) - `tools/area_rows.py`
exists and its report replaces the scratch walk the area plan was written
from ([`takeover-queue-areas.md`](takeover-queue-areas.md) section 1a has
the numbers side by side). No function taken; four `[[data]]` entries named
(section 5). The cut into groups (section 4) is a proposal for the
coordinator; the area harness (group ARH) consumes `--clones`.

## 1. What it reads, and how to run it

```
python tools/area_rows.py --exe .../bof3/BOF3.exe --analysis .../analysis \
    --sibling .../BreathOfFire3Recomp                -> the report
    ... --unit AREA033                               -> one block, function by function
    ... --unit AREA033 --clones                      -> area_harness clone tables (C++)
    ... --groups                                     -> the group table (markdown)
```

The flags are `magic_rows.py`'s (`--exe`, `--analysis`, `--sibling`,
`--symbols`, `--unit`, `--clones`, `--quiet`) plus `--groups` and
`--group-size` (default 50), and `--no-write` (2026-09-28: a read-only run -
the TSVs are written from the running checkout's `symbols.toml` into the
`--analysis` directory the tool reads, so a worktree's run overwrote the main
checkout's; [`round-10-cleanup.md`](round-10-cleanup.md) item 4). It writes `analysis/area_rows.tsv` (an area a
line: its descriptor, root counts, closure, exclusive functions, unit, group,
the routes that reach it, every start of its block) and
`analysis/area_funcs.tsv` (a function a line: start, size, unit, `exclusive` /
`shared` / `gap`, group, source, ours, the areas that reach it, the roots it
is, the routes that reach it, the catalogue's label). Both are gitignored
(CLAUDE.md rule 1). A run takes about 70 seconds: the discovery re-descends
the band to a fixpoint (3 rounds). `--unit` takes `AREA033` or `33`.

Inputs: the exe; `analysis/pc_funcs.json` and `pc_hidden.json` (the starts),
`remaining_catalog.tsv` (labels, for the report only), the route reaches
`hidden_reached_*.json`, `pc_hidden_reached.json` and `calltrace/*`
(the "live" column), `area_pairs*.json` (PSX twins of the roots,
[`attract-remaining.md`](attract-remaining.md) section 5);
`symbols.toml`; the sibling's `analysis/file_ids.json` (the world of an area
is the `BIN/WORLDnn` directory of its `AREAnnn.EMI`: areas 0..37, 38..75,
76..113, 114..151, 152..199). The clone sites are
`magic_rows.clone_sites`, imported, so a clone table means exactly what a
spell group's did; each clone's comment line names the root tables it came
from with the call shape each gives (a descriptor handler or choice or the
init `void(void)`; a step, arrive or cell hook `(x, z)` answering in `al`; a
tail kind a field-frame phase; an object trigger `(object, flags)`).

## 2. The roots

| Table | What the tool reads | Measured |
|---|---|---|
| `Area_Descriptors` `0x667590` -> 200 descriptors `0x5DB318..0x64AD68` | `+0x34` choice handlers, `+0x3C` handler array, `+0x40` init | choice 927 entries over 106 areas, handlers 678 over 119, inits 67; 808 distinct functions |
| `Area_StepHook` `0x56E050` / `Area_ArriveHook` `0x56E4E0` | the bias, bound, byte index table and jump table read off each switch; each case's call | 38 areas -> 28 handlers (one, `0xAE`'s `Scenario_NoHook`, outside the band); 8 areas -> 8 |
| `Field_ModeTailKinds` `0x662CE8` | 64 slots | 63 set, 47 in the band; attributed by the area code that stores the kind (`mov byte [0x9039F3], n`): 50 kinds armed so; slots 1, 8, 9 by nothing in the band |
| `Area_CellHooks` `0x662F28` | 28 `(area, fn)` records, the reader's bound (`0x56E670`, ARH's `Area_CellHook`); each a hook `(x, z)` answering in `al` | 28 areas, 18 functions, 17 in the band (`0x4FEEB0`, area `0x6D`, outside) |
| `WorldMap_Records` / `WorldMap_FieldHooks` | eleven records' five code fields and area byte; twelve hooks | 64 functions, all in the band |
| `Field_ObjectTriggers` `0x662E20` (**not in the plan**) | ids 1..65 by `object[+0x86]` | 64 functions, 51 in the band; area-less |
| the areas' data blocks | every dword of `.data` from area 0's first table to area 199's descriptor end naming a band start, beyond the descriptor tables | 1,586 pointers, 1,228 in a `+0x34` / `+0x3C` table, **358 beyond** (270 functions) |
| outside the seven (scan 7) | rel32 calls into the band from outside it, not the hook switches; `.data` elsewhere naming a band start | 22 functions called from engine code, 21 named by unnamed tables at `0x6541xx`, `0x6555xx`, `0x660CA8`, `0x66094C`; area-less |

Two things the reading settled that the plan had otherwise:

- **A descriptor's handler array and choice table overlap.** In most areas
  the `+0x34` table is the tail of the `+0x3C` array (area 3's is `+0x3C`
  plus 8). Bounding each run by the next table start cut 40 handlers; not
  letting the two stop each other gives 678, against the PSX's 671
  (`names/area_records.toml`), and the two areas that differ are the two
  `area_pairs.json` already lists as disagreeing (75, 86).
- **`0x662F28` holds 28 records, not 100.** Its reader `0x56E670` stops at
  `0x663008`; what follows is `MapCell_Handlers`.
- **`+0x38` is not null everywhere**: area 77's points at a colour matrix in
  `.data`. It is never code.

**The data blocks, corrected by the walk.** A dword belongs by default to the
area whose descriptor ends next above it (the PSX put the descriptor at the
end of an area's section). The correction: the dword's table - the nearest
start at or below it that a descriptor or any band code names, within 0x400
bytes - read by **one** area only (its descriptor fields or its exclusive
code) is that area's. 333 pointers moved, **every one to the area before**:
on the PC an area's data runs on past its descriptor. The eleven world maps
move 20 each (area 33's is the plan's example, and it is the rule for all
eleven), 121 and 104 move 29 and 22, 135 15, and the rest 2..8 each.
Without the correction the world-map areas' state handlers were counted as
the next area's, which is where the plan's one out-of-order block and three
overlaps came from.

## 3. The walk and the block rule, as measured

**Starts.** 1,457 recorded or hidden in `0x401000..0x430000`, 66 ours;
the descent dropped 32 (each inside another: a switch case, fallen into, or
before the function's own jump table - the first lesson of
[`scenario-roots.md`](scenario-roots.md) section 3) and found 141 (a direct
call, a tail `jmp`, a table entry, or code after a jump table no list
covered - most in `0x401040..0x402100` and `0x40F340..0x40F9D0`).
1,566 functions.

**The edge rules.** A `jcc` into a neighbour's start is an edge (a shared
tail), not a merge - the clone table then REFUSES it (`conditional jump out`),
so a group sees it. Fall-through into a start merges it. Code immediates that
are starts, `.data` tables the code indexes or takes the address of (runs
of code pointers bounded by the next named table), and the tail kinds a
function arms are edges. Ours is walked through. Everything outside the band
is the frontier.

**Where area code ends.** At `0x42D710`, not `0x430000`: past the last
function any area reaches are **76 starts** - `0x42D710` (called by engine
code at `0x517330`), the `BATE.EMI` code (16) and the `BATTLE.EMI` code (10)
the catalogue already labels, and 41 the descent found among them. **41 of
the band's 66 ours are there** (battle); the area band proper holds 25 ours:
`Area29_PickFieldObject` and area 33's 19 in world 0, the world-map hooks of
round eight elsewhere.

**Reach.** 1,490 functions before `0x42D710`: **1,380 reached by some
area**, the other 110 only by area-less roots: 79 are such roots themselves
(object triggers 51, calls from engine code 19, the unarmed tail kinds and
what they reach 8, the no-world-map hook 1), 21 are named by the unnamed
`.data` tables of scan 7, the rest reached from those. **None is reached by
nothing.**

**Per area.**

| | |
|---|--:|
| Areas with band code | 168 |
| With an exclusive function (a block of its own) | 163 |
| Exclusive functions per area: median / mean / max | 4 / 7.7 / 46 |
| Blocks out of area order | **0** |
| Blocks overlapping the next | **0** |
| Interleaved areas merged into one unit | none |
| Shared bodies | 122 (51 by two; 32 by exactly eleven - the eleven areas `0xAF..0xB9` (175..185), whose descriptors name one set of choice handlers, one init, one cell hook and one step hook; a few by up to 15) |
| Gaps (reached by no area; assigned to the block they lie in) | 110, 0 ours |

Largest: 135 (46 exclusive), 121 (39), 104 (34), 75 (29), 33 (26), 45, 65,
88 (25). Areas with no block: those with no band code (the descriptor's code
fields empty and no hook) and five whose every function is shared.

**So the block rule holds exactly**: each area's code is one block, in area
order, from its lowest exclusive function to the next area's, and the
shared bodies and gaps sit inside those blocks. A shared body is keyed by
address, as the spell round's were: taking it takes it for every area.

**The frontier**: 177 functions outside the band, 122 of them ours by name;
the rest unlabelled engine helpers (17), boot code (6 + 3), top-level modes,
field objects, three SCENA-labelled, three CRT. Of the 627 functions the
catalogue labels "Area overlays" outside the band, the areas reach 2 - the
plan's section 1.4 stands.

## 4. The groups

Whole units in address order, one world at a time, cut into about 50
functions not yet ours each (`--groups`; balanced by the running total, a
cut at the unit boundary nearest each multiple). **28 groups**, 1,465
functions to take.

| Group | World | Areas | Band | Fns | Ours | To take | Bytes to take | Live |
|---|--:|---|---|--:|--:|--:|--:|---|
| AR0A | 0 | 0..5, 7..8, 10..13, 15 | `0x401000..0x401B80` | 51 | 0 | 51 | 2,522 | - |
| AR0B | 0 | 16, 18..26 | `0x401B80..0x403400` | 61 | 0 | 61 | 5,710 | - |
| AR0C | 0 | 27..29, 32..37 | `0x403400..0x4053B0` | 75 | 20 | 55 | 3,973 | combat, worldmap |
| AR1A | 1 | 38..41 | `0x4053B0..0x406650` | 48 | 0 | 48 | 4,350 | - |
| AR1B | 1 | 42..47 | `0x406650..0x408FF0` | 56 | 1 | 55 | 9,732 | worldmap |
| AR1C | 1 | 48..52 | `0x408FF0..0x40AB00` | 57 | 0 | 57 | 6,480 | - |
| AR1D | 1 | 53, 55..57, 59..64 | `0x40AB00..0x40B8C0` | 47 | 0 | 47 | 3,180 | - |
| AR1E | 1 | 65, 67 | `0x40B8C0..0x40CEF0` | 49 | 0 | 49 | 5,233 | - |
| AR1F | 1 | 68..69, 71..75 | `0x40CEF0..0x40EB90` | 59 | 0 | 59 | 6,798 | - |
| AR2A | 2 | 76..82, 84 | `0x40EB90..0x40F720` | 50 | 0 | 50 | 2,564 | - |
| AR2B | 2 | 85..88 | `0x40F720..0x411F10` | 68 | 2 | 66 | 9,454 | worldmap |
| AR2C | 2 | 90..92, 94 | `0x411F10..0x4135B0` | 45 | 0 | 45 | 5,406 | - |
| AR2D | 2 | 95..100, 103 | `0x4135B0..0x4146C0` | 53 | 0 | 53 | 3,974 | - |
| AR2E | 2 | 104..106 | `0x4146C0..0x4168E0` | 53 | 1 | 52 | 8,224 | worldmap |
| AR2F | 2 | 108, 110..113 | `0x4168E0..0x418BE0` | 53 | 0 | 53 | 8,546 | - |
| AR3A | 3 | 115..119 | `0x418BE0..0x41A9D0` | 57 | 1 | 56 | 7,083 | worldmap |
| AR3B | 3 | 120..121 | `0x41A9D0..0x41C890` | 54 | 0 | 54 | 7,377 | - |
| AR3C | 3 | 124..125, 127..128, 130..134 | `0x41C890..0x41DAD0` | 56 | 0 | 56 | 4,215 | - |
| AR3D | 3 | 135 | `0x41DAD0..0x41EFE0` | 46 | 0 | 46 | 4,972 | - |
| AR3E | 3 | 136, 139..142 | `0x41EFE0..0x420800` | 52 | 0 | 52 | 5,715 | - |
| AR3F | 3 | 143..146 | `0x420800..0x4223A0` | 53 | 0 | 53 | 6,696 | - |
| AR3G | 3 | 148..151 | `0x4223A0..0x4249D0` | 56 | 0 | 56 | 9,282 | - |
| AR4A | 4 | 152..155, 166..167 | `0x4249D0..0x426560` | 48 | 0 | 48 | 6,618 | - |
| AR4B | 4 | 168..172 | `0x426560..0x428450` | 56 | 0 | 56 | 7,486 | - |
| AR4C | 4 | 173..174 | `0x428450..0x4292C0` | 39 | 0 | 39 | 3,401 | - |
| AR4D | 4 | 175..187 | `0x4292C0..0x42A320` | 49 | 0 | 49 | 3,855 | - |
| AR4E | 4 | 188..191 | `0x42A320..0x42BD60` | 51 | 0 | 51 | 6,367 | - |
| AR4F | 4 | 192..193, 196..199 | `0x42BD60..0x42D710` | 48 | 0 | 48 | 6,219 | - |

"Areas" lists the areas with a block; an area with none (no band code, or
all of it shared) has its shared code in a neighbour's block. "Bytes to
take" are the functions' own bytes (descent to the last instruction, jump
tables included). "Live" is a route that reached a function of the group
(`hidden_reached_*.json`, the call traces): the **combat** route enters area
29 (its init `0x4037B0`, `Area29_PickFieldObject`, ours), the **worldmap**
route area 33's world-map code and the world-map copies of areas 45, 88,
104 and 115. **The attract cycle reaches no band function** (no band entry
in `pc_hidden_reached.json` or the `all_*` / `hidden_b` traces).

**For group ARH's proof area**: no world-0 area of 3..8 exclusive functions
is on any recording. The one small, not-yet-ours set in world 0 on a recording is
**area 33's seven** (`0x404180`, `0x4041B0`, `0x4041E0`, `0x404680`,
`0x4046A0`, `0x404800`, `0x404820`; the world-map HUD's state handlers and
the two record fields `+0x4` / `+0x8`, which jump through area 33's state
tables `0x5EF688` / `0x5EF6AC`), whose 19 neighbours are ours and whose
area the worldmap route enters (of the seven, `0x404180` is in its
hidden-reach list; the others are not hidden starts or were not entered). `--unit AREA033 --clones` prints them.

## 5. The names this group gave

`symbols.toml`, each with its evidence: **`Area0_Descriptor`** `0x5DB318`
(0x44 bytes: the descriptor layout every area shares, field by field, with
the reader of each), **`Field_ModeTailKinds`** `0x662CE8` (64),
**`Field_ObjectTriggers`** `0x662E20` (65), **`Area_CellHooks`** `0x662F28`
(28 records). The cell hook `0x56E670` itself is group ARH's to name and take.

## 6. How sure, and what it cannot see

- **Checked against group ARH's hand reading of area 11** ([`area_011.md`](area_011.md)
  section 5, on ARH's branch): descriptor `0x5E2738`, two handlers and the
  init, no hook, tail kind, cell hook or data-block root, block
  `0x401750..0x401840` of three exclusive functions, and the clone rows
  (`0x1C` no calls; `0x46` `{0x30, 0x57CE10}`; `0x79` `{0x8, 0x57C140},
  {0x1B, 0x57C140}`) - the tool prints exactly that. No difference.
- The roots are read off the exe with the reader of each table checked
  (the switch bounds, the cell-hook loop bound, the trigger index). The
  handler counts agree with the PSX in every area but 75 and 86.
- The data-block pointers are "a dword naming a start, or a 16-aligned
  address after padding or a `ret`": a script byte run that happens to form
  one would add a false root. None was seen (every one lands on a start).
- A function pointer built at run time, or stored by engine code, is not
  followed; the 110 area-less functions are placed by address alone, and
  their harness shape is what their root says (a trigger takes
  `(object, flags)`, a tail kind nothing, a hook `(x, z)`).
- The unattributed tail kinds 1, 8, 9 (`0x403570`, `0x4075D0`,
  `0x407940`) are stored by code that computes the kind; they sit in area 27's
  and area 44's blocks by address.
- The 21 functions named by the unnamed `.data` tables at `0x6541xx` /
  `0x6555xx` (most 0x12 bytes, one per area) are a per-area table of some
  engine subsystem no one has read; they are placed by address.
