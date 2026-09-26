# The area round: the area overlays enumerated from their tables, and taken wave by wave

**Status:** PROPOSED (2026-09-26) - a plan, not a queue. Listed as
[`IDEAS.md`](IDEAS.md) I24; nothing here is scheduled or cut. The method is
the spell round's ([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md),
[`magic_harness.md`](magic_harness.md)) and the scenario plan's
([`takeover-queue-scenario.md`](takeover-queue-scenario.md)); section 1 is
what makes it apply to areas, and section 6 is what is different.

## 0. The question, and the answer in one paragraph

Until now an area's code was taken one function at a time as a route
happened to enter it (`Area29_PickFieldObject`, `Area33_ClearCellsA`, the
world-map hooks of round eight's group DA). The owner asked (2026-09-26)
whether the area overlays could instead be enumerated *programmatically*, as
`Magic_Rows` enumerates the spells and the chapter tables enumerate the
scenario banks. **They can.** The engine reaches an area's code through
seven tables and the area's own data block, all readable off the exe; the
closure of calls from those roots is the area code as the engine sees it;
and the linker kept the per-area files in area order, so each area's code is
one block of `.text` between its neighbours', the same fact the spell round
built its units on. Discovery is a script; the live check per world is a
recorded walk the owner makes later, after the fuzz, as the spell and
scenario rounds do.

## 1. The measurement

Made 2026-09-26 with a copy of `tools/scenario_roots.py` re-aimed at the
area tables (the session scratchpad, not the repo: the clean tool is the
round's first step, section 7). The numbers below are that walk's, over
`analysis/pc_funcs.json` + `pc_hidden.json` starts and the catalogue of
[`remaining-catalog.md`](remaining-catalog.md) as of 2026-09-25; every one
is to be re-derived by the tool before a group is cut.

### 1.1 The band

The area code is one band, **`0x401000..0x430000`** (192 KiB, the very
start of `.text`, before `BattleAction_End` `0x430010`): **1,457 starts, 66
of them ours** (DA's world-map areas, `Area29_*`, `Area33_*`). The catalogue
labels 1,073 of them "Area overlays" (world 0: 84, 1: 241, 2: 188, 3: 315,
4: 245), 205 are unlabelled, 47 are `WorldMap_Records`' (DA), and the rest
are a few dozen the neighbour fill mislabelled (BATE, BATTLE, effect kinds).

### 1.2 The roots

| Root table | Entries into the band | Who calls through it | Shape |
|---|--:|---|---|
| `Area_Descriptors[k] +0x3C`, the handler arrays (678 entries over 119 areas) | 678 | the movement-script ops `03 n` and `DE` (`MoveScript_GroupD`, ours; [`movement-script.md`](movement-script.md)) with `Sprite_Current` the object; `DE` returns `Sprite_Current[8]` | `void (void)` |
| `Area_Descriptors[k] +0x34`, the choice tables (934 entries) | 934 | `MsgBox_ChoiceCommit` / `MsgBox_MenuCommit` (ours), `[[desc + 0x34] + 4 * id]` for a choice id below 0x80 ([`item-use.md`](item-use.md) §5) | `void (void)` |
| `Area_Descriptors[k] +0x40`, the init (67 areas) | 67 | `Area_Enter` (ours), once per area entry | `void (void)` |
| `Area_StepHook` / `Area_ArriveHook`, the switches over the area number ([`event-ops.md`](event-ops.md) §6) | 46 | ours, `(x, z)` in, `al` out | `int (long, long)` |
| `0x662CE8`, 64 slots, **unnamed** | 47 | `Field_ModeTailRun` `0x56D920`, `jmp [eax*4 + 0x662CE8]` by a byte ([`field-modes.md`](field-modes.md)) | a phase |
| `0x662F28`, 100 `(area, fn)` pairs, **unnamed** | 31 | `0x56E670`, the chapter's cell hook's per-area table, **still Capcom's** ([`event-ops.md`](event-ops.md) §11) | a phase |
| `WorldMap_Records` / `WorldMap_FieldHooks` | 64 | the world map (round eight DA, [`worldmap_area.md`](worldmap_area.md)) | done |

`+0x38` of every descriptor is null in the image (the item-use fuzz's P61
control calls through it and faults; nothing in the exe does).

**The area's own data block.** The descriptors lie in `.data` in area
order, `0x5DB318` (area 0) to `0x64AD68`, and each area's data (its scripts,
placement tables, handler arrays, and the *state tables of its own tasks*)
lies around its descriptor. Scanning the block between neighbouring
descriptors for code pointers finds **1,141 pointers, 329 of them not in any
descriptor field**: the pointer tables an area's frame functions jump
through by a state byte, the same shape as a scenario chapter's state
handlers, and internal to the unit. (The split by descriptor is approximate:
area 33's tables sit after its descriptor; the tool should cut the data
blocks by the walk, not by the descriptors.)

814 distinct roots from the descriptors, 46 from the hooks, 64 from the
world map, 47 + 31 from the two engine tables, 329 from the data blocks; the
walk adds 30 starts no list had (2 passes to the fixpoint).

### 1.3 What the roots reach

| | Functions |
|---|--:|
| Starts in the band | 1,457 |
| Reached by the walk | 1,133 |
| Not reached | 324 |
| In no area's closure (ours, shared bodies, the gaps) | 401 |

Per area, taking only the functions reached by that area and no other:

| | |
|---|--:|
| Areas with band code | 161 |
| Blocks out of area order by lowest address | **1** |
| Blocks overlapping the previous | **3** |
| Exclusive functions per area: median / mean / max | 3 / 5.6 / 29 |
| Functions shared by two or more areas | 203 (126 by two; 26 by exactly eleven, the world-map copies) |

**So the block rule holds**: an area's unit is the block of `.text` from
its first exclusive function to the next area's, and the 324 unreached
starts fall into those blocks the way `magic_rows.py`'s unreached functions
fell into their overlays'. The largest areas are 122 (29 exclusive
functions), 105 (28), 135 (27), 174 (22), 77 (21), 116 (20); most are under
five.

**The frontier** - engine code the band calls, not walked - is 231
functions, **139 of them ours by name** (`AreaMap_*`, `Field_*`, `Flags_*`,
`Gte_*`, `Gpu_*`, `MapView_*`, `Msg_Open*`, `Sprite_*`, `Party_*`,
`Inventory_*`, `Sound_*`, `Music_*`, `Text_Draw*`, the world-map HUD) and 33
in `Boot: field, map and sprites`. That is a far better starting position
than the scenario round's, whose frontier was 232 with 172 unnamed.

### 1.4 What the label overstates

About 630 functions the catalogue labels "Area overlays" lie **outside the
band**, and the walk reaches almost none of them - on purpose:

| Region | Functions | Reached through | What they are |
|---|--:|---|---|
| `0x460000..0x48FFFF` | 419 | `EffectKind18_States` `0x65406C` and the unnamed pointer tables around it (`0x653B00..0x6545xx`) | effect-kind state handlers |
| `0x500000..0x52FFFF` | 194 | field-core state tables at `0x65E000..0x660100` (near `Cursor_Hidden`, `FieldCore_FadeSteps`), read by dispatchers around `0x512000` / `0x513000` | field-core state machines |
| `0x594000..` | 14 | `SpriteCell_Sizes`' neighbours | sprite draw |

On the PlayStation every AREA overlay statically linked the common field
code it used, so a PSX twin inside an area overlay does not make the PC
function an area's: the PC compiled it once. These are engine rounds of
their own, with the same walker aimed at their own tables (the owner,
2026-09-26: effect kinds and field core on their own round). This round is
the band.

## 2. What an area function is, to a harness

One frame of field state is every input. The shapes the roots give:

| Shape | Called by | Arguments | Answers |
|---|---|---|---|
| A handler `+0x3C[n]` | `MoveScript_GroupD` ops `03` / `DE`, `Sprite_Current` the running object | none | `DE`: `Sprite_Current[8]` read after |
| A choice handler `+0x34[id]` | `MsgBox_ChoiceCommit`, the message word `0x7DEE48` read after | none | may open another message |
| The init `+0x40` | `Area_Enter`, after the map and party are placed | none | - |
| A step or arrive hook | `Area_StepHook` / `Area_ArriveHook` | `(x, z)` | `al` |
| A mode-tail or cell-hook phase | `Field_ModeTailRun`, `0x56E670` | none | - |
| A state handler | the area's own frame function, through a table in the area's data | none | - |

What they read, by the frontier and the functions read so far: `Field_State`
and the leader's record, `Sprite_Current` and `Sprite_Objects`, the party
records and `Field_MemberCount`, `Cond_Flags` and the story flags
`0x904030`, `AreaMap_Header` / `AreaMap_Bytes` / the elevation,
`Game_AreaNumber`, `Field_Request`, `MoveScript_Var*`, `Frame_Counter`,
`Rand`, the descriptor itself and the area's tables.

The harness sets `Game_AreaNumber` to the group's area and **leaves the real
descriptor and the real tables in place** (the inverse of the item-use and
event-leader fuzzes, which pointed `Area_Descriptors[0..3]` at descriptors
of their own to test the engine side), randomises the field frame, and calls
each root and each state phase under Capcom's and ours, comparing the state
after and the recorders' tapes. Controls as every round's: a mutant per
function the fuzz must refuse.

## 3. The groups

Whole areas only, in address order, about 40..60 functions a group, so a
group is several small areas or one large one. With the block sizes of 1.3
that is **about 25 groups** over the ~1,390 functions not yet ours. The
exact cut is the tool's output (section 7), not this document's; the world
boundaries fall where the catalogue's world labels change, roughly world 0
at `0x401000..0x404000` (the attract and world-map areas), then worlds 1..4
in order.

Shared bodies (203) are keyed by address, as the spell round's 781 were:
taking one takes it for every area that reaches it.

## 4. The waves

Three or four waves of six to eight groups. The first wave is **world 0**:
its areas are the ones the attract cycle and the three recorded routes
enter (11 of its functions sit in hosts the attract run reached,
[`remaining-catalog.md`](remaining-catalog.md) §3 item 6), so the first
groups get a live check from the recordings that already exist. Then worlds
1..4 in order, which is also address order.

Each wave: agents in worktrees, one group each, headless self-tests, merged
one at a time, a round doc per group on `magic_s16.md`'s shape. Rate-limit
cuts resume.

## 5. The live check

Fuzz-only until a route exists, as every spell group is. The live check per
world is **a recorded walk through its areas** under original and ours with
the frame hash compared ([`input-script.md`](input-script.md)), which the
owner records after the fuzz, not before it. World 0 is covered by the
attract cycle and the three routes today.

What the fuzz cannot see and the walk would: a pointer stored into an
object at run time and called later (the walk follows immediates only), and
an area reached through code outside the seven tables. The cell hook's
reader `0x56E670` is Capcom's, so its per-area calls are not on any tape
until it is taken; it is small and belongs to the first wave.

## 6. What is different from the spell and scenario rounds

- **Seven root tables, not one.** The tool must read them all; a group's
  clone table lists which table each root came from, so the harness knows
  the shape.
- **Heavier sharing.** The eleven world-map areas are one file compiled
  eleven times on the disc and once here: 26 functions shared by exactly
  eleven areas. They are DA's and mostly ours already.
- **Tiny units.** A median of three exclusive functions per area means the
  group, not the area, is the unit of work; the area is the unit of
  *evidence* (its PSX twin, its descriptor, its walk).
- **Ground truth per root.** The sibling's `names/area_records.toml` pairs
  every descriptor root with its PSX entry (728 pairs, 198 of 200 areas
  agreeing on the descriptor's shape, [`attract-remaining.md`](attract-remaining.md) §5).
  Names come from what the function does, as DA's did (`Area33_ClearCellsA`).
- **The data blocks are part of the unit.** A spell's `.data` was tables
  and constants; an area's holds the state tables its own code dispatches
  through, so the tool must scan it for roots, and the harness must swap
  those tables for recorders while the fuzz runs (the spell harness's
  `DataTable`).

## 7. Before the first cut

1. **The tool, `tools/area_rows.py`** - clean, not the scratch copy: read
   the seven tables and the descriptors' data blocks, walk the closure
   (the three walker lessons of [`scenario-roots.md`](scenario-roots.md)
   §3 apply), cut the band into per-area blocks by exclusive functions,
   assign the gaps, list shared bodies, and print per area what
   `magic_rows.py --unit` prints per overlay, with `--clones`. Its report
   replaces section 1's numbers. Half a day.
2. **Name the tables**: `0x662CE8` (64) and `0x662F28` (100 pairs) in
   `symbols.toml`, and the cell hook `0x56E670`; the descriptor's fields
   as a `[[data]]` layout if the tool wants them by name.
3. **The harness**, `src/game/area_harness.*`, from `magic_harness` with
   the six shapes of section 2 and the field-frame state; proved on one
   small world 0 area the attract cycle enters.
4. Then the first wave.
