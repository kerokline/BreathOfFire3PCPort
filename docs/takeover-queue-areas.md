# The area round: the area overlays enumerated from their tables, and taken wave by wave

**Status:** PROPOSED (2026-09-26) - a plan, not a queue. Listed as
[`IDEAS.md`](IDEAS.md) I25; the method is the spell round's
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md),
[`magic_harness.md`](magic_harness.md)) and the scenario plan's
([`takeover-queue-scenario.md`](takeover-queue-scenario.md)); section 1 is
what makes it apply to areas, and section 6 is what is different.
**2026-09-27: the tool exists** - `tools/area_rows.py`
([`area-rows.md`](area-rows.md), round ten's group ART). Its numbers are
section 1a and replace section 1's where they differ; its group cut is in
section 3. The two engine tables and the descriptor layout are named in
`symbols.toml` (section 7 step 2, but for the cell hook `0x56E670`, group
ARH's).

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

## 1a. The tool's numbers (2026-09-27)

`tools/area_rows.py` over the same inputs ([`area-rows.md`](area-rows.md)
has the method). Section 1 is kept as the scratch walk measured it; where
the two differ, this is the one to use.

| | Section 1 (scratch, 2026-09-26) | The tool (2026-09-27) | Why |
|---|--:|--:|---|
| Starts in the band / ours | 1,457 / 66 | 1,457 / 66 listed; 1,566 after 32 dropped and 141 found | the descent keeps code after jump tables and code only a call or table names |
| Where area code ends | `0x430000` | **`0x42D710`** | past it are 76 starts no area reaches: `BATE.EMI` 16, `BATTLE.EMI` 10 and 50 more; 41 of the band's 66 ours are there |
| Descriptor choice / handler / init entries | 934 / 678 / 67 | 927 / 678 / 67 (808 distinct) | runs bounded by the next named table, but a descriptor's `+0x34` and `+0x3C` overlap and do not bound each other (678 against the PSX's 671: areas 75, 86) |
| Hook-switch roots | 46 | 38 + 8 areas -> 28 + 8 handlers | read per case off the switches |
| `0x662CE8` | 47 | 47 in the band, 50 kinds attributed to the area that arms them | slots 1, 8, 9 armed by no immediate |
| `0x662F28` | "100 pairs", 31 | **28 records**, 17 in the band | the reader stops at `0x663008` (`MapCell_Handlers` follows) |
| An eighth table | - | **`Field_ObjectTriggers` `0x662E20`**: 65 ids, 51 in the band | found by the tool's scan of `.data` |
| Engine calls into the band, other `.data` naming it | - | 22 and 21 functions | area-less roots |
| Data-block pointers / beyond the descriptors | 1,141 / 329 | 1,586 / 358 | scanned whole, `+0x34` / `+0x3C` runs excluded |
| Data pointers the walk re-assigned | "area 33's tables" | **333, all to the area before** | every world map's tables (20 each) and many others lie after the area's own descriptor |
| Reached by some area | 1,133 | 1,380 of the 1,490 before `0x42D710` | the other 110 by area-less roots only; none by nothing |
| Areas with band code / with a block | 161 | 168 / 163 | |
| Exclusive per area: median / mean / max | 3 / 5.6 / 29 | 4 / 7.7 / 46 | the world maps' after-descriptor tables no longer counted as the next area's |
| Largest | 122, 105, 135, 174, 77, 116 | 135 (46), 121 (39), 104 (34), 75 (29), 33 (26), 45 / 65 / 88 (25) | the same cause: 122's, 105's and 116's were 121's, 104's and 115's |
| Blocks out of order / overlapping | 1 / 3 | **0 / 0** | the same cause |
| Shared bodies | 203 (26 by exactly eleven) | 122 (32 by exactly eleven: areas 175..185, not the world maps, which have a copy each) | |
| Frontier / ours by name | 231 / 139 | 177 / 122 | the scratch walk expanded unlabelled engine code outside the band |
| Groups | ~25 | 28 | section 3 |

The block rule holds with no exception: every area's code is one block in
area order. `+0x38` is null in 199 descriptors, not 200 (area 77 has a
colour matrix).

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

Shared bodies (122 by the tool) are keyed by address, as the spell round's
781 were: taking one takes it for every area that reaches it.

**The cut** (`tools/area_rows.py --groups`, 2026-09-27; [`area-rows.md`](area-rows.md)
section 4): whole areas in address order, one world at a time (the world is
the area file's `BIN/WORLDnn`: areas 0..37, 38..75, 76..113, 114..151,
152..199), about 50 functions not yet ours a group. 28 groups, 1,465
functions to take.

| Group | World | Areas | Band | Fns | Ours | To take | Bytes to take |
|---|--:|---|---|--:|--:|--:|--:|
| AR0A | 0 | 0..5, 7..8, 10..13, 15 | `0x401000..0x401B80` | 51 | 0 | 51 | 2,522 |
| AR0B | 0 | 16, 18..26 | `0x401B80..0x403400` | 61 | 0 | 61 | 5,710 |
| AR0C | 0 | 27..29, 32..37 | `0x403400..0x4053B0` | 75 | 20 | 55 | 3,973 |
| AR1A | 1 | 38..41 | `0x4053B0..0x406650` | 48 | 0 | 48 | 4,350 |
| AR1B | 1 | 42..47 | `0x406650..0x408FF0` | 56 | 1 | 55 | 9,732 |
| AR1C | 1 | 48..52 | `0x408FF0..0x40AB00` | 57 | 0 | 57 | 6,480 |
| AR1D | 1 | 53, 55..57, 59..64 | `0x40AB00..0x40B8C0` | 47 | 0 | 47 | 3,180 |
| AR1E | 1 | 65, 67 | `0x40B8C0..0x40CEF0` | 49 | 0 | 49 | 5,233 |
| AR1F | 1 | 68..69, 71..75 | `0x40CEF0..0x40EB90` | 59 | 0 | 59 | 6,798 |
| AR2A | 2 | 76..82, 84 | `0x40EB90..0x40F720` | 50 | 0 | 50 | 2,564 |
| AR2B | 2 | 85..88 | `0x40F720..0x411F10` | 68 | 2 | 66 | 9,454 |
| AR2C | 2 | 90..92, 94 | `0x411F10..0x4135B0` | 45 | 0 | 45 | 5,406 |
| AR2D | 2 | 95..100, 103 | `0x4135B0..0x4146C0` | 53 | 0 | 53 | 3,974 |
| AR2E | 2 | 104..106 | `0x4146C0..0x4168E0` | 53 | 1 | 52 | 8,224 |
| AR2F | 2 | 108, 110..113 | `0x4168E0..0x418BE0` | 53 | 0 | 53 | 8,546 |
| AR3A | 3 | 115..119 | `0x418BE0..0x41A9D0` | 57 | 1 | 56 | 7,083 |
| AR3B | 3 | 120..121 | `0x41A9D0..0x41C890` | 54 | 0 | 54 | 7,377 |
| AR3C | 3 | 124..125, 127..128, 130..134 | `0x41C890..0x41DAD0` | 56 | 0 | 56 | 4,215 |
| AR3D | 3 | 135 | `0x41DAD0..0x41EFE0` | 46 | 0 | 46 | 4,972 |
| AR3E | 3 | 136, 139..142 | `0x41EFE0..0x420800` | 52 | 0 | 52 | 5,715 |
| AR3F | 3 | 143..146 | `0x420800..0x4223A0` | 53 | 0 | 53 | 6,696 |
| AR3G | 3 | 148..151 | `0x4223A0..0x4249D0` | 56 | 0 | 56 | 9,282 |
| AR4A | 4 | 152..155, 166..167 | `0x4249D0..0x426560` | 48 | 0 | 48 | 6,618 |
| AR4B | 4 | 168..172 | `0x426560..0x428450` | 56 | 0 | 56 | 7,486 |
| AR4C | 4 | 173..174 | `0x428450..0x4292C0` | 39 | 0 | 39 | 3,401 |
| AR4D | 4 | 175..187 | `0x4292C0..0x42A320` | 49 | 0 | 49 | 3,855 |
| AR4E | 4 | 188..191 | `0x42A320..0x42BD60` | 51 | 0 | 51 | 6,367 |
| AR4F | 4 | 192..193, 196..199 | `0x42BD60..0x42D710` | 48 | 0 | 48 | 6,219 |

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
