# The region table: every run of the quicksave's span, with a class

**Status:** STABLE (2026-11-25; measured on the round base `f6d53766` with this
change, the Linux build). The state-by-name round's group A3
([`state-by-name-round.md`](state-by-name-round.md) 3.4, 4). No behaviour
changes: the table and its checks change nothing the game does and no state
hash, and have no ledger entry.

## 0. In one paragraph

A quicksave copies the exe host's span, `0x5DA000..0x987000`
(`exe_layout.h` `kDataVa..kOwnVa`: `.data`, `.bss`, the end of `.game`,
`.pool`; [`quicksave.md`](quicksave.md) 3). The region table splits that
span into **3,898 runs**, each with a name or an address, its length and one
of the five classes of 3.4: `state`, `process`, `derived`, `stack`,
`unknown`. `tools/region_table.py` makes it from four files of this
repository - `symbols.toml`, the new `tools/region_classes.txt`,
`tools/statehash_skip.txt` and `snapshot.cpp`'s `kKept` - and refuses a
table that disagrees with them. `tools/gen_symbols.py` writes it into the
build as `bof3/region_table.gen.h`; `src/hook/region_table.cpp` checks it at
every start. **1,630,961 bytes are `unknown` (42.3 % of the span)**; CI
fails when that number rises (`tools/region_unknown.txt`).

## 1. What the table is

| File | Role |
|---|---|
| `tools/region_table.py` | builds the table, refuses a disagreement, prints the counts (`--tsv`, `--largest N`, `--check`) |
| `tools/region_classes.txt` | the recorded classes: `ADDRESS LENGTH CLASS # why (source)`, the skip list's shape |
| `tools/region_unknown.txt` | the committed count of `unknown` bytes, which may only fall |
| `tools/gen_symbols.py` | writes `bof3/region_table.gen.h` beside `symbols.gen.h` on every run, so the build, CI's `symbols.toml generates` and `tools/linux_probe/linux_tree.py`'s compile all have it |
| `src/hook/region_table.{h,cpp}` | `RegionTable_Check`, called from `InjectAll` beside `Snapshot_Inject` |

A **run** is a piece of the span between two boundaries: the start or end of
a named member, of a row of `region_classes.txt`, or the place of a
`symbols.toml` name that has no length. Its **name** is the member that
covers it (`Name+0x..` when the run starts inside it); in a gap, the name
with no length that starts the gap (`~Name+0x..`: `~MessagePools`); else
none, and the address alone says where it is.

A **named member** is what has a length in `symbols.toml`: a GameData
member, as `gen_symbols.py` lays GameData out (every typed `[[data]]` that is
not a field of a block, every typed `[[block]]`), and an untyped `[[block]]`
that gives its records and overlaps no member (none does on this build). An
untyped block over typed members (`MsgBoxState`; `EnemyWorkingRecords`,
inside `Battle_Enemies`), a `[[data]]` with no `ctype` (84 of them:
`Task_Records`, `Text_Records`, the name tables) and a block with no records
(`MessagePools`, `PartyWorkingRecords`) have no length here: they name the
gap they start, or nothing when they lie inside a member. Of these 88, 87
lie in the span (`MsgBox_Step_CodeTable` is in `.text`) and 86 start a gap.

## 2. How a run gets a class

In this order:

1. **A row of `tools/region_classes.txt`** that covers it. Each row cites
   what the class rests on: a row of the census
   ([`quicksave-census.md`](quicksave-census.md) 4, 5 and 7, which already
   says for every skip-list row who owns it and what a restore does with
   it), the keep list, a ledger entry, a measurement.
2. **`state`**, when a named member covers it. The quicksave writes it, and
   a name says someone read it as the game's. A constant table in `.data`
   is `state` too: written, unchanged.
3. **`unknown`**, otherwise.

The rows are where judgement lives, and the generator holds them to the
lists beside them. It refuses (each refusal tried on a perturbed copy of the
files, 2026-11-25):

| Refused | Its message, as tried |
|---|---|
| a `process` row that is not a `kKept` range, or a `kKept` range without its row | `kKept ranges with no process row: 0x7CC350 4 (D3d_Device)` (a row dropped); `... 0x66C858 4 (Task_StackTop)` (a row added to `kKept`) |
| a `kKept` row it cannot read as `{VA, bytes, "Name"}` | `kKept has 18 rows, 17 of them read as {VA, bytes, "Name"}` |
| a skip-list byte that no row covers | `tools/statehash_skip.txt:214: 0x5DA000 (in 0x5DA000 4) has no row in tools/region_classes.txt: read it and give it a class` |
| two rows over one byte | `0x7CC380 lies inside the row of line 54 (0x7CC378 + 0x12000)` |
| a class that is not one of the five, a row outside the span | `class 'stat' is not one of state, process, derived, stack, unknown` |

So **`process` is the keep list, exactly**: 17 rows, 7,569 bytes. And
**every range the state hash cannot see has been read**: a new skip-list
row fails the build until someone writes its class.

To class a run: add its row, with the source, and lower
`tools/region_unknown.txt` by the bytes it takes out of `unknown`
(`python3 tools/region_table.py --check` says by how much). Naming it in
`symbols.toml` does the same for `state`.

### The rows as written

| Class | Rows | Bytes | What |
|---|--:|--:|---|
| `process` | 17 | 7,569 | `kKept`, row for row |
| `stack` | 5 | 20 | the four task records' saved `esp` (`Task_Records+4` of each) and `Task_StackTop`: 2.2's "five words written fresh by the restore" |
| `derived` | 14 | 246,736 | SP-A's sound and music (`Sound_Channels`, `Sound_Banks`, the stream and file pointers, `Music_Events..Music_Data`, `Music_Finished`, `Music_Staging`); SP-G's caches (`D3d_BackMaterialHandle..` 1,280, `Gfx_TexCache`, `Font_TexCache`, `D3d_CellTexCache`); the frame's own (`D3d_Vertices`, `Gfx_PacketPools`, `DrawLayers` and `Gfx_FrameNodes`) |
| `state` | 16 | 444,468 | the skip-list rows that are the game's (DIV-0028's fade words, DIV-0023's vertex halves, `Music_Volume`, the draw pool's words and Capcom's unused pool arrays, DIV-0038's two pointer slots), and `.pool` (294,912) |
| `unknown` | 15 | 37,908 | read, and no class fits or nobody has decided (section 5) |

`Task_SchedulerEsp` is in the keep list, so it is `process`, not `stack`:
the table follows `kKept`. `Task_StackTop` is `stack` today; the round
doc's 3.3 moves it into the keep list, and the check makes that move two
lines at once (section 6).

## 3. Measured on this build

`python3 tools/region_table.py` at the round base `f6d53766` with this
change (`symbols.toml` unchanged by it):

| Class | Bytes | % of the span | Runs |
|---|--:|--:|--:|
| `state` | 1,969,050 | 51.1 | 2,895 |
| `process` | 7,569 | 0.2 | 25 |
| `derived` | 246,736 | 6.4 | 29 |
| `stack` | 20 | 0.0 | 5 |
| `unknown` | 1,630,961 | 42.3 | 944 |
| **the span** | **3,854,336** | | **3,898** |

By source: 1,524,582 bytes are `state` by a name (2,879 runs); 736,701
bytes take their class from a row (96 runs); 1,593,053 bytes are `unknown`
because nothing names them (923 runs), and 37,908 because a row says so (21
runs).

**Against the round doc's 2.3** (2,971 GameData members, 2,940 of them in
`.data`/`.bss`, 1,924,181 of 3,555,328 bytes, 54.1 %, 855 unnamed runs of
1,630,375 bytes). In 2.3's frame, `0x5DA000..0x93E000`, with GameData's
members alone, this build gives **2,971 members, 2,940 in the frame,
1,924,181 bytes, 54.1 %** - the same - and **856 unnamed runs of 1,631,147
bytes**: by size 1,348,009 bytes in runs over 4 KB, 258,471 in runs of 257
bytes to 4 KB, 24,667 in shorter ones. The last two buckets are 2.3's
exactly; the first is 772 bytes and one run more. 2.3's own figures do not
add up to its frame (1,924,181 + 1,630,375 = 3,554,556, 772 short of
3,555,328), so the difference is in how 2.3's short script closed the
frame, not in the names. The round base merged `main`, which renamed three
members since 2.3's commit `65b7810f` (`Area111_Grid` and
`Area111_GridStart` to `Area111_SlidingPlatforms` and `..Start`,
`AreaW1f_OtherCounter` to `AreaW1f_PartnerCounter`, commit `43c7a629`) and
moved no extent: the same count measured on `65b7810f`'s `symbols.toml`
gives the same members and bytes.

The table's own numbers differ from 2.3's for three reasons, each by
construction:

- **The span is the quicksave's**, `0x5DA000..0x987000`, 3,854,336 bytes:
  2.3's frame plus the page `0x93E000..0x93F000` (the end of `.game`) and
  `.pool` (`0x48000`).
- **Rows cut runs.** A class row inside a gap or a member makes runs of its
  own (`Font_TexCache`'s, the keep rows inside `0x7CAE38 5440`), and a name
  without a length starts a run in its gap: 3,898 runs, not 2.3's "about
  3,800".
- **`unknown` is not "unnamed".** The span holds 1,930,155 unnamed bytes
  (2.3's 1,631,147, the page `0x93E000..0x93F000`'s 4,096, `.pool`'s
  294,912). Rows give 299,242 of them another class - `.pool` 294,912
  `state`, 3,004 inside keep rows `process`, 1,310 `derived`, 16 `stack` -
  and make 48 named bytes `unknown` (the display objects and the desktop
  words of section 5): 1,930,155 - 299,242 + 48 = 1,630,961.

## 4. The largest `unknown` runs

Adjacent `unknown` runs taken together (`--largest 12`):

| Run | Bytes | Between | What is known |
|---|--:|---|---|
| `0x813580` | 729,088 | `Gfx_ClutStrip` .. `Encounter_Rows` | the arena past the message pools, where `LoadDatFile` puts an area and the party's sprite banks ([`arena-edits.md`](arena-edits.md); 2.3) |
| `0x8CB5B4` | 102,348 | `AreaMap_Corners` .. `Gfx_UnpackScratch` | |
| `0x66C85C` | 37,284 | `Task_StackTop` .. `Area42_Rank` | "unread data" (skip row `0x66C854 37292`; census 7): an explicit row |
| `0x803580` | 32,768 | `~MessagePools` | the message pools (the block has no length) |
| `0x93116C` | 28,180 | `Effect2e_SphereLight` .. `Rest4b_BusyB` | |
| `0x68E1B8` | 26,400 | `~RottenBreath_Motes` .. `FlamePool` | an effect's motes (the `[[data]]` has no `ctype`) |
| `0x685D94` | 25,380 | `BattleItems_SparkleCurrent` .. `ReviveMote_Pool` | |
| `0x8C55C8` | 24,504 | `Encounter_Rows` .. `AreaMap_Header` | |
| `0x6BEA20` | 16,416 | `SpriteCell_Table` .. `Gfx_ClutRows` | |
| `0x92D9DE` | 14,218 | `Effect2e_SphereVerts` .. `Effect2e_SphereLight` | |
| `0x6424C4` | 13,720 | | |
| `0x93C354` | 11,436 | to `.pool` | the C runtime's data at `.data`'s end, explicit rows among them, then the 6,420 bytes from `kBssEnd` to `.pool`: zero at load, nothing of Capcom's |

The first is 44.7 % of all `unknown` bytes. It is the game's state by every
account (an area's data and the sprite banks live there), and the cheapest
fall of the number: a `[[block]]` with its length, or a `state` row citing
[`arena-edits.md`](arena-edits.md), takes 729,088 bytes out at once. That
is a reading for whoever lowers the count, not done here.

## 5. The `unknown` rows, and what is reported

Fifteen rows say `unknown` on purpose. Two kinds:

- **The process's by their reading, but not in `kKept`.** Inside the skip
  row `0x7CAE38 5440` the census says "keep the objects", and `kKept` keeps
  four (`D3d_DeviceDesc`, `Dd_DirectDraw`, `Dd_StageSurface`, `D3d_Device`);
  seven more objects lie between them - `DDraw_Primary`,
  `DDraw_BackBuffer`, `PsxRest_ZBuffer`, `PsxRest_Clipper`,
  `PsxRest_Direct3D`, `D3d_Viewport`, `D3d_BackMaterial` - with the row's
  last `0x18` bytes (`SpriteCell_Count` among them). And the three skip rows
  of 2026-11-24 are the machine's screen: `Display_Setup`'s copy of the
  desktop's height (`0x6C9F40`), `Display_DesktopWidth` (`0x7C9F44`),
  `Desktop_Height`/`Desktop_Width` (`0x939A30`). A restore writes all of
  these from the file. In one process that is a no-op; **across machines a
  portable load would write another machine's desktop size, and across
  builds another process's object pointers.** Whether each belongs in the
  keep list is the format's call (A1); until then they are `unknown`, since
  `process` means `kKept`.
- **The C runtime's words**, dead in the exe host (census 5, 7;
  [`state3-build.md`](state3-build.md) 3.1): written only by Capcom's
  start-up, which the exe host does not run, and read by nobody. No class of
  3.4 fits a word nobody reads; writing them is harmless.

And `0x66C85C`, 37,284 bytes "unread" by the skip list's comment, is
`unknown` for the same reason as the CRT's.

**A note on 2.3's "the largest named members are derived or the
display's".** By the census, `Gfx_VramShadow` (1 MB) is **state**: it is the
source SP-G reconverts its caches from (census 4, the CLUT rows and the
surfaces), and the state hash sees it as the game's logic data
([`state-hash.md`](state-hash.md) 3). `Gfx_UnpackScratch` (`0x1F020`),
`Gfx_ClutStrip` and `Gfx_ClutStripSource` (`0x4000` each) are `state` here by
their names: no reading says they are rebuilt. They are I45's first
candidates to read (section 8).

## 6. The checks

**At generation** (the build, CI): section 2's refusals. `kKept` is in an
unnamed namespace of `snapshot.cpp` and `snapshot.h` exports nothing of it,
so the comparison with it is made here, on the source: the generator
parses `constexpr Kept kKept[] = { ... };` and fails the build on a
difference. The build lists `snapshot.cpp` as an input of the generation,
so an edit of `kKept` regenerates and rechecks the table.

**At start-up** (`RegionTable_Check`, every start, both hosts): the span is
`exe_layout.h`'s `kDataVa..kOwnVa`; the runs lie end to end over it; every
run that starts a `symbols.toml` name starts at that name's VA in this
build's `symbols.gen.h` - for a GameData member `<Name>_addr`, the
compiler's `offsetof`, so the table is checked against the layout as
`Snapshot_Inject` checks `kKept`'s literals against the names; the
`process` runs are the keep rows the table was made with, byte for byte;
every skip-row byte lies in a run a row classes; the counts by class are
the generator's. Any difference is a `Fatal`. Measured: see the self-test's
log line in section 9.

**In CI** (`.github/workflows/checks.yml`, "The region table against the
keep list and the skip list; its unknown bytes may not rise"):
`python tools/region_table.py --check`, from the repository's files alone.
It fails when the `unknown` bytes rise above `tools/region_unknown.txt`
(tried: a committed 1,630,960 fails with the message; 1,630,962 passes and
says to lower the number). A fall passes and asks for the number to be
lowered in the same change, so the gain is kept.

**What would make the `kKept` comparison a run-time one.** One declaration
exported by `snapshot.h` and one definition in `snapshot.cpp`, outside its
unnamed namespace:

```cpp
// snapshot.h
struct SnapshotKept { std::uint32_t at, bytes; const char* name; };
const SnapshotKept* Snapshot_Kept(std::size_t& count);
// snapshot.cpp (Kept would become SnapshotKept)
const SnapshotKept* Snapshot_Kept(std::size_t& count) { count = kKeptCount; return kKept; }
```

Then `RegionTable_Check` compares its process runs with the live list as it
now compares them with the generated copy. Not done here: `snapshot.{h,cpp}`
are A1's in this round.

### The table as the one source of `kKept` (after A1 merges)

3.4 has the table replace the hand-kept lists. For `kKept` that change is:

1. `region_classes.txt`'s `process` rows become the keep list's source; each
   row's comment already carries its reason, and a row gains its name (the
   generator has it: the member at the row's start).
2. The generator emits `kKept` itself into `region_table.gen.h`, as
   `{VA, bytes, name}` rows in the rows' order, with a `static_assert` per
   row that the VA is the name's `<Name>_addr` (in place of
   `Snapshot_Inject`'s run-time loop over `names[]`).
3. `snapshot.cpp` drops its literal table and its name check and includes
   the generated one (`using region_gen::kKept`); the comparison of
   section 6 retires, since there is one list.
4. When 3.3 moves `Task_StackTop` into the keep list, its row changes class,
   `stack` to `process`, in the same line. Until step 3, adding it to
   `kKept` alone fails the build with `kKept ranges with no process row:
   0x66C858 4 (Task_StackTop)` - measured, section 2 - which is the point.

The skip list's rows stay the state hash's: the hash is a tool with its own
reasons per row (noise, ours, a ledger number), and its references are
recorded under its exact list. What the table replaces there is the
skip list's implicit "these are the process's": the table says which.

## 7. What the classes mean in a portable load

As 3.4 has it: `state` written, `process` kept, `derived` as the participant
says, `stack` written fresh, `unknown` written. The table itself changes no
load: A1's format reads it, or not, at its merge.

## 8. What I53 and I45 start from

- **I53 (the wide image)** needs every byte of the span named before the
  layout may change. It starts from the `unknown` runs (944, 1,630,961
  bytes), largest first (section 4), and from the 86 `~Name` labels: names
  in `symbols.toml` that carry no length here (section 1), each a `ctype`
  and `count`, or a block's records, from becoming a member. The `state` bytes that
  are `state` only by a name (1,524,582) are the field-by-field writer's
  list; `region_classes.txt`'s rows are the exceptions it must honour.
- **I45 (a smaller file)** starts from `derived`: 246,736 bytes that a
  participant or the next frames rebuild (`Music_Staging` 73,728,
  `Gfx_PacketPools` 131,072 among them) could leave the file, each with its
  participant's consent. Then the `state` arrays unused under DIV-0062
  (`DrawItems` 147,456, `DrawItemPool_Free` 2,048: Capcom's pool, which ours
  does not use while the grow is on), and the named members no reading has
  classed yet (`Gfx_UnpackScratch`, the CLUT strips: section 5).

## 9. Measured, and not checked

- The table and its counts: `python3 tools/region_table.py` on the round
  base `f6d53766` with this change (sections 3, 4); the refusals and the
  ratchet tried on perturbed copies of the files (sections 2, 6).
- The self-test (the Linux ELF, `BOF3X_LAUNCHER=1 BOF3X_SELFTEST_ONLY=1`,
  offscreen, the laptop's cache): exit 0, 10,402 log lines (10,401 at
  `65b7810f`, and this one), line 10,356: `region_table 3898 runs over
  0x005DA000..0x00987000: state 1969050, process 7569, derived 246736,
  stack 20, unknown 1630961 bytes; 3023 names at their places, 17 keep rows
  process, 176 skip rows classed`.
- The state hash unmoved: `bridgeWalk`, headless, `--direct`, `en-US`,
  `--only-slot0`, `BOF3X_SCALE=2`, the committed skip list (176 ranges,
  hash `929F2913`), once on the round base's ELF and once on this change's:
  `tools/statehash.py diff` says **identical on all 2074 compared ticks**
  (files in `~/bof3x-runs/sbn1125/A3/root/bw_base`, `bw_a3` on the laptop;
  both runs `end: done`).
- The no-data checks this touches, locally: `ledger_check.py --verbose`,
  `ledger_switches.py --check`, `ledger_registry.py --check`,
  `gen_symbols.py`, `region_table.py --check`: all pass.
- **Not checked:** the classes of the rows rest on the census's reading,
  not on a load across builds (that is the proof's, B3). The 32-bit
  Windows build compiles `region_table.cpp` in CI, not here (no Windows
  toolchain on the laptop). `RegionTable_Check`'s `Fatal` paths were not
  driven by a doctored header (each would need a rebuild); the refusals
  before them, in the generator, were. Whether the 84 length-less `[[data]]` entries'
  gaps are theirs to the next member, as the `~Name` labels read, is not
  claimed: a label marks a start, not a length.
