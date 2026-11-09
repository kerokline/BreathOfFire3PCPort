# The source-layout round: staging (IDEAS I44's last step, I5 folded in)

**Status:** IN PROGRESS (stage A merged with PR #207 on 2026-11-07; **stage B0
run 2026-11-08** on `source-layout/stage-b` off `main` `67c3259b`, root
`C:\Users\kerok\bof3x-runs\slb1108`, section 4 "Stage B0 as run": the 468
helper clashes renamed, the mover reports none, 39 `low32-site` rows, group
s's decisions applied where the code stays identical and 62 blocked on
measured gate classes, the sheet in two tables and its PSX flags read against
the PlayStation side. Not merged to `main` and no PR while the beta is being
cut. The moves B1..B5 wait on the owner's read of the sheet's 212 rows and
the calls in 7b.)

The typed-data round named every data access, the relocatable-image round
removed every fixed address, and the 64-bit round builds the same source at
two widths. What is still not Capcom's shape is **where the code lives**: our
301 game files are the takeover's waves (`area_w1c.cpp`, `rest_3d.cpp`,
`effect_engine_b.cpp`), cut by address range and by the order the rounds
reached them, not by the source files Capcom compiled. The owner folded I5
into I44 on 2026-10-21 as "a source-layout pass after stage 3 - which of our
files should be split or joined to match", and ordered it after the 64-bit
round ([`x64-build-round.md`](x64-build-round.md) 9).

This round recovers Capcom's translation-unit boundaries from the binary and
moves our functions so that each of our files holds whole translation units.
**It changes no function's body.** The proof is that every function compiles
to the same machine code it did before, wherever it now lives, and the state
hash on the ten routes.

## 0. What the binary and our tree say today, measured 2026-11-07

Scripts in `C:\Users\kerok\bof3x-runs\sl1107\m` (scratch; `gaps.py`,
`psxruns.py`, `xref.py`, `order.py`, `cluster.py`), over `bof3/BOF3.exe` and
`symbols.toml` at `main` `39d88ef9`. Stage A's SL1 replaces them with a
committed tool; these numbers size the round and nothing more.

| Measurement | Result | What it means |
|---|---|---|
| Functions in `.text` (`symbols.toml` `[[func]]`) | 10,100; 10,088 with `impl` | the whole game, the runtime's called entries |
| Start alignment | 10,070 of 10,100 on 16 bytes | MSVC6's per-function alignment |
| Filler before a start | `0x90` before 9,467 of 10,099; code before 486; `0x00` 143 (the high byte of a `ret n`) ; `0xCC` 3 | **no linker seam byte**: an object's edge looks like a function's edge. The boundaries must come from order, not from padding |
| PC runs whose PSX twins are in the same order (same overlay, next PSX address within `0x2000`) | 1,956 runs over the 5,613 paired functions; 1,107 of two or more, 299 of five or more, the longest 34 | both compilers emit a file's functions in source order; a run is inside one file, and a break in PSX order where PC order continues is a boundary candidate |
| Of those runs (three or more), how many of **our** files each spans | one file 580, two 85, three 25, four to seven 11 | 121 runs of Capcom's order are cut across our files today |
| `.data` (initialised) statics read by functions at most 12 apart, in address order: is the next one's reader later in `.text`? | 2,690 of 2,950 neighbours yes; longest increasing run 2,427 of 2,951 | **the linker laid `.data` in the same object order as `.text`**: a static's place in `.data` says which object it came from |
| The same for `.rdata` | 218 of 239; LIS 175 of 240 | the same, fewer anchors |
| The same for `.bss` | 904 of 1,290; LIS 308 of 1,291 | weak: uninitialised globals are partly COMMON symbols, which the linker places by its own rule; `.bss` file statics still follow object order. SL1 separates the two |
| Our files that are one contiguous `.text` run | 116 of 301; 1,201 contiguous pieces in all | most of our files interleave with others in Capcom's order |
| Candidate units from PSX order and shared `.data`/`.rdata` statics alone (functions joined when they share a run or a local static) | 1,127 joined segments of two or more holding 5,243 functions; 4,857 functions with no join yet | the joins are the sure part; the rest is placing each boundary inside the gaps, which `.data` order (above) and the call graph (a `static` function is called only inside its file) decide |
| Rich header (`tools/pe_rich.py`) | 1,009 object contributions, 110 import thunks, 16 unmarked; the largest record 712 from one tool build | [`SHARED_SOURCE.md`](SHARED_SOURCE.md) 2's ~883 objects, the runtime's and the libraries' among them. Which record is the game's is **not** established |
| Our side of a move | 343 game `.cpp` (254 `*_fuzz.cpp`), 313 files with an `_Inject()`; 600 `src/game/` lines in `CMakeLists.txt`; 1,178 `src/game/...` citations in `docs/`, 85 with a line number; 424 `static` / `inline` definitions in `src/game` | what a function's move drags with it |

The game's own code is `0x401000..0x5AB7E0`; the MP3 decoder
(`0x5AB7E0..0x5B8DA0`) and the C runtime (`0x5B9380..0x5C3660`) are out of
scope: the first is replaced (DIV-0091), the second is Microsoft's and was
replaced at its boundary ([`platform-layers-plan.md`](platform-layers-plan.md)
2.3, 2.4).

## 1. What "matching Capcom's files" means here

A **unit** is one object file of the 2001 link, recovered as a contiguous
`.text` range plus the `.data`, `.rdata` and `.bss` ranges its statics came
from. The round's target, under the owner's call 1 (section 7):

- every function of ours lives in the file of its unit, and no unit is split
  across two of our files;
- a file holds one unit (or, if the owner chooses, adjacent units of one
  subsystem - never parts of two);
- functions inside a file are in Capcom's order, so a reader walking a file
  walks the binary;
- a file's statics are the unit's statics - the typed `GameData` member stays
  the place (the image is not moved; this round changes only which source
  file names it);
- the fuzz files, the `_callees.h` headers and the `_Inject()` registrations
  follow their functions; nothing is left in a file named for a wave.

What the round does **not** do: rename functions, change a body, change a
signature, touch the image's layout, change the hosted build's clone fuzz
beyond where its files live, widen anything (I53). The typed-data round's
comments are retired after the moves, as stage E (section 5a), never inside
a move.

Units that hold only the porting house's code (the Direct3D replay, the
DirectInput and DirectSound layer, `WinMain`) were replaced by ours long ago;
where our replacement already lives under `src/render`, `src/input` or
`src/hook`, it stays there. The round lays out `src/game`.

## 2. The method: a unit table, then pure moves, then the gate

1. **The unit table** (stage A): `analysis/tu_layout.tsv`, one row per
   function - `pc`, name, unit id, the unit's `.text` range, a confidence
   (`joined` by a PSX run or a shared static; `placed` by `.data` order or
   the call graph; `guessed` by neighbourhood alone), the evidence column, and
   the proposed file. Committed with its tool and a CI check that the table
   covers every `[[func]]` with an `impl`. It holds addresses and our names,
   like `analysis/data_census.tsv`; no byte of the game.
2. **The owner reads the proposed layout** (between stages A and B): the
   unit count, the file names, the directory shape, the `guessed` rows.
3. **The moves** (stage B): `tools/move_funcs.py` moves a function's
   definition, its file-statics and helpers, its `_Inject()` lines, its fuzz
   cases and its callees-header constants, and rewrites `symbols.toml`'s
   `impl` and the build's source list. No hand edits of bodies. Each group
   moves one `.text` range.
4. **The gate after every merge:** `codegen_same.py --by-function` (SL3) on
   the i686 build: every function's disassembly identical to `main`'s,
   matched by symbol, whatever object now holds it; the initialised data the
   same by symbol. Then the hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`
   suite and the exe host's self-tests. The x86_64 preset builds.
5. **The proof** (stage D): the ten routes on the exe host against the
   current references, the state hash identical but for the run noise its
   repeats show.

Why per function and not per object: today's `codegen_same.py` compares
object by object, and every move changes two objects. The one real risk of a
move is **inlining**: a `static` helper the compiler inlined into a caller in
one file is no longer visible to a caller moved to another, and that caller's
code changes. The move tool keeps a `static` helper with all its callers
(Capcom's own `static`s are inside one unit by definition, so a correct table
never separates them); where our wave files made a helper shared that Capcom's
units do not share, it goes to a header as `inline` and the gate decides. A
differing function is a stop, not a waiver: it is moved back or explained in
the round's exception list with the reason and a fuzz or route that runs it.

## 3. Stage A: the table and the tools (three groups, in parallel, no `src/game` change)

Each group an Opus agent in its own worktree under
`C:\Users\kerok\bof3x-runs\sl<mmdd>`, reset onto the round branch's tip.
Agents run no game.

### SL1: the unit table from the PC binary
`tools/tu_census.py` and `analysis/tu_layout.tsv`, from `bof3/BOF3.exe` and
`symbols.toml`:

- the joins of section 0 (PSX order runs; a `.data` / `.rdata` static read
  only inside a short `.text` window), with each join's evidence;
- `.data` and `.rdata` order as a monotone alignment against `.text` order:
  find where the alignment steps, and place the boundary between the two
  readers that step. Separate `.bss` file statics from COMMON symbols (a
  COMMON symbol is read from several units and sorted by the linker's rule -
  measure the rule before relying on it);
- the call graph: a function called only from inside one candidate unit is a
  `static` candidate there; a function called from many units is `extern`;
  string literals and float constants in `.rdata` read by one window;
- the switch tables in `.text` (`analysis/pc_jumptables.json`) as part of
  their function, so a table never reads as a boundary;
- the Rich count as a bound: report how many units the table finds in the
  game's range, and how that compares with the Rich records;
- every boundary that two signals disagree on, listed.

Output: the TSV, the counts in this doc's section 3 "as run", a sheet of
the 50 least certain boundaries for the coordinator's read, and **the owner's
layout sheet** (section 7, calls 1..3): one row per unit with its range,
function count, a one-line description of what it does, its PSX overlay,
the proposed subsystem folder and file name, and a proposal - stand alone, or
join the named neighbour units in one file - with the reason. The owner's
examples set the tone: enemy groups apart from area data, bosses and fonts
together. Written as `analysis/tu_layout_sheet.md` (addresses, our names and
counts only; no game bytes).
Also a `--check` mode for CI: the table covers every `impl` function, and
(after stage B) every function's `impl` is its row's file.

### SL1 as run (2026-11-07, branch `source-layout/sl1`, report `sl1107\reports\sl1.md`)

Built `tools/tu_census.py` (writes the table and the sheet; `--report`
prints every measurement below, `--uncertain N` the N least certain
boundaries, `--check` is the CI check and reads only `symbols.toml` and the
table), `analysis/tu_layout.tsv` (10,088 rows: the 10,069 `impl` functions
in `0x401000..0x5AB7E0` and the 19 runtime functions as `outside`; an empty
`psx_unit_agreement` column for SL2) and `analysis/tu_layout_sheet.md` (one
row per unit). The binary's facts are cached in `analysis/` (ignored).

- **The contiguity test passes, with one qualification.** PSX-adjacent
  twins of one image (5,222 pairs, step under `0x2000`): 4,891 are close
  forward on the PC (3,838 the very next function), 83 close backward
  (1.7%: 31 by one function, 34 by two to eight, 18 by nine to forty), 248
  far (other units, placed in another order by the other linker). Of the 83,
  57 fall inside one unit of the final table: **small reorderings inside a
  unit** (a draw callback before its task on one side and after it on the
  other, `Gpu_Set*` from the PSX library's one-function objects, copied
  helpers paired to a copy). None interleaves two units, so contiguity holds
  and the table rests on it; "functions in Capcom's order" can only mean the
  PC's order for those 57.
- **`.bss` and COMMON.** By named variable, first reader in `.text`: `.bss`
  below `0x6CA000` follows object order (378 variables, LIS 301; the last
  object-ordered static `0x6C9F50`, the next read address `0x6D0000`);
  from `0x6CA000` it does not (1,326 variables, LIS 128): the COMMON symbols
  and the port's large blocks. Their rule is **not** found: neighbours share
  subsystems (`D3d_*`, `Snd_*`/`Music_*`, `Gte_*` runs from `0x7C9F44`) but
  neither text order nor size orders them. Only the lower part is used.
  `.data`: 2,808 variables, LIS 2,273; `.rdata`: 70, LIS 47.
- **Switch tables**: 424 dispatches through a `.text` table, none outside
  its function's extent.
- **Joins**: 4,910 PSX order spans, 359 `.data` and 190 `.bss` local
  statics; 2,703 segments (588 of two or more holding 7,956 functions).
  559 hard cuts (PSX image changes and steps back). Statics against PSX on
  36 boundaries (17 kept joined, 19 cut).
- **Calibration** (fires inside a PSX-joined run / across a PSX image cut,
  397 and 318 samples): `.data` order 23 / 3, `.bss` order 4 / 0, local
  code-pointer table 53 / 4, only callers 69 / 2, any call 112 / 15, same
  name prefix 303 / 25; code-pointer cells in order 99 / 156, so unused.
- **Units: 676**, at least 560 (559 disjoint hard cuts). Through the passes:
  2,703 after the joins, 2,238 after placing, 841 after name prefixes and
  calls, 676 after one- and two-function stragglers. The Rich header's C
  records are 712 (build 8799), 117 (8797) and 1 (8168): the count sits
  under the 712, but it moves with the guessed passes, and which record is
  the game's is still not established. Sizes: 68 of one function, 128 of
  two to four, 249 of five to 14, 200 of 15 to 49, 31 of 50 or more.
- **Rows**: 7,202 `joined`, 62 `placed`, 2,805 `guessed` (a row is as sure
  as the weakest step that put it in its unit). Of the local statics, all
  190 `.bss` ones and 386 of 436 `.data` ones are read inside one unit.
- **Least certain boundaries** (`--uncertain`): 36 statics against PSX, 29
  placing signals refused by a PSX cut, 28 guesses refused, 202 PSX cuts
  whose place in a twinless run is a guess, 39 boundaries with no evidence
  either way, 1,952 guessed joins. The 50 first are in the report.
- **The sheet** proposes 556 files in 14 folders (`area/` 174 units,
  `magic/` 142, `effect/` 134, `boss/` 60, `scenario/` 51 and nine more):
  509 units stand alone; the bosses join in runs (the owner's example), and
  units that share a name stem or are one function beside a neighbour of
  the same folder are proposed together. Areas never join another area.
- **Seen, not followed up**: the 15 game functions whose start is not on 16
  bytes are all in `0x5A9907..0x5AA79C` (`Task_*`, `Tex_Convert*`,
  `Font_UnpackGlyph`, `Cell_Unpack*`, `Gfx_MoveCells`, `Gfx_PackRgb`), which
  fits hand-written assembly (the Rich header has 26 objects of record 14);
  the reading of the record numbers is not verified.
- **Not verified**: that the guessed boundaries are Capcom's (2,805 rows);
  the COMMON rule; which Rich record is the game's; that a PSX twin's overlay
  is right where a helper was copied into several overlays (36 twins left
  out; the disagreements list more).

### SL2: the PlayStation side
The PSX binary is the second observation of the same tree
([`SHARED_SOURCE.md`](SHARED_SOURCE.md) 1, 2). From the sibling
(`../BreathOfFire3Recomp`, read only): the main executable's and each
overlay's function order and their `.data` / `.sdata` / `.sbss` layout (the
gp-relative blocks are per file on Psy-Q), giving PSX units the same way SL1
gives PC ones. Then the agreement: each PC unit against the PSX unit of its
twins - same set of functions, a superset (the port added code), split or
joined. An overlay (`AREA030.EMI`, `BATTLE.EMI`) is a separately linked
image, so its boundaries are independent of the main executable's: list
which PC units are one overlay's code.

Output: `analysis/tu_psx.tsv` and an agreement column SL1's table reads
(merged by the coordinator after both). Where the PSX says a unit has a name
(an overlay's file name, a debug string naming a `.c` file in either binary -
search both for `.c` and `.C` strings), record it: those are the only real
file names this round can find.

### SL2 as run (2026-11-07, branch `source-layout/sl2`, report `sl1107\reports\sl2.md`)

`tools/tu_psx.py` (reads the sibling by `--sibling`, default
`../BreathOfFire3Recomp`) writes `analysis/tu_psx.tsv` and
`analysis/tu_psx_ranges.tsv`; `--units` writes every PSX unit, `--strings`
runs the file-name search. Measured on the round branch's `symbols.toml` and
the sibling's JP images (`SLPS_009.90`, `overlay_captures_all.json`,
`emi_sections.json` with the disc). No game run, no `src/` change.

**What the PSX images offer, measured.** The brief's per-file `.sdata` /
`.sbss` blocks do not exist here: the whole boot executable has one
gp-relative access (the `gp` set-up at `0x8014AA90`, `gp` = `0x8018BC2C`) and
the 406 overlay images one between them, so Capcom built with `-G0` and every
static is reached by `lui` + offset. `.text` gives no seam either: inside the
boot executable's code functions abut with no padding (956 of 1,006
neighbour gaps in the sibling's `functions.tsv` sizes are 0; 42 of the other
50 are in Sony's libraries at `0x80178000` and up). What is
left is the order of the absolute data the functions read, and on the
overlays even that is thin: **Capcom kept most overlay variables in the boot
image's data** (`0x80143000..0x8014A000`): `GAME.EMI#0`'s functions read 424
boot-data addresses 1,994 times and 229 of their own 263 times; over the 333
overlay images, 7,117 boot-data addresses against 4,645 of their own. So an
overlay's own data orders few of its functions.

**Joins** (the tool's doc has the rules): `window` (an item of the image's own
data read only within 12 functions), `inversion` (two such items in the
opposite order to their readers - two units never invert; a drop of more than
12 functions is read as a section restart), `shared` (overlays: a boot-data
item read by at most 6 neighbouring functions), and `call` (a helper called
only by near neighbours) as a **hint column only**: on the boot image's Sony
libraries, whose duplicated statics (`memclr`, `memclr_2`, `memclr_3`;
`csqrt`; the `InitCARD`/`StartCARD`/`StopCARD` pairs; 11 groups by the
sibling's names) each mark two files, call joins put two copies in one unit 5
times in 11. The unit level (`psx_unit`: window + inversion + shared) and the
data level (`psx_unit_data`: window + inversion) put two copies in one unit 0
times in 11.

| Measurement | Result |
|---|---|
| PC functions with a PSX twin (`psx`) | 5,613: image from `psx_overlay` 4,985, from it through a capture with the same bytes 20 (`MAGIC018.EMI#3` is captured as `MAGIC008.EMI#0`, `BOSS034.EMI#15` as `BATTLE.EMI#15`, and three more), boot code 221, the one overlay whose known starts hold it 337, the overlay of its PC neighbours' twins 48; **2 unplaced**: `0x4621C0` `Title_Task` (`0x801D0C04`, in no captured or surveyed code section) and `0x510630` `AreaMap_FrameAreaBD` (`0x801F2C04` starts 41 AREA images) |
| PSX images with twins, and their functions | 334 (boot + 333 overlay images); 11,684 functions (known starts, `jal` targets and the twins) |
| Data items read, ordering domains | 5,184 items of the images' own data in 526 domains; joins found: window 775, inversion 1,138, shared 1,398, call 1,278 (hint) |
| Units (`psx_unit`) | 8,176; 755 of two or more functions holding 4,263; largest 30. Weakest link: shared 513, window 152, inversion 90; 7,421 of one function |
| Data units (`psx_unit_data`) | 9,795; 308 of two or more holding 2,197; largest 30 |
| PC rows in a unit of two or more | 2,104 of 5,613 (data level 996); 1,651 rows carry the `call_joined` hint |
| Agreement, units of two or more with twins | 551: `same` 163, `superset` 8, `subset` 169, `subset+superset` 160 - **500 (91%) contiguous on the PC** - and `interleaved` 51 |
| PC order as a check of the joins | twins of PSX neighbours are PC neighbours across 94.0% of window joins (418 pairs), 94.9% inversion (157), 92.6% shared (623), 86.0% call (506) - and **91.0% of open boundaries** (2,183). The PC keeps the PSX order across file edges as much as inside files, so PC adjacency cannot confirm a join; it only shows `call` is the weakest |
| Source-file names (deliverable 2) | see below: three, all Sony's; **no Capcom file name in either binary** |

**A unit is a lower bound.** An open boundary is one no join crosses; a real
file may hold several neighbouring units, and 7,421 functions have no join at
all (most overlay functions read no data of their own overlay). SL1 should
read `psx_unit` as "these functions are one file", never as "this is where a
file ends". The `interleaved` units (51) are where the PC puts a function
twinned into another unit between a unit's twins: either a wrong join or the
port's reorder; not separated here.

**File names in the binaries.** Searched: `BOF3.exe`, the 407 PSX images, and
all 886 files on the JP disc, for `name.c` / `.h` / `.cpp` / `.obj` / `.lib` /
`.asm` shapes, RCS `$Id:` lines, and `assert` / `__FILE__` / `.pdb` / `RSDS` /
`NB10` text. The PC exe has none: no hit, no debug directory, no PDB path.
The PSX has exactly three real names, each an RCS line of **Sony's**
libraries, in `SLPS_009.90` (and again in `LOGO/LOGO.EXE`): `intr.c` at
`0x8014A0EC`, `bios.c` at `0x8014A39C`, `sys.c` at `0x8014A6F4` (the boot
image's `.rdata`, below `.text` at `0x8014AA04`). The string is a file's first
`.rdata` item, so the first item read after it names its unit: `intr.c` is
`main:u402` (`0x8017525C..0x80175940`, `ResetCallback`..`restartIntr`, libetc),
`bios.c` is `main:u432` (`0x801765D8..0x80177CC0`, `getintr`, the `CD_*` layer,
libcd), `sys.c` holds `ResetGraph` `0x8017B884` (libgpu; its unit is one
function at the data level because the 12-function inversion cap cuts it).
None has a PC twin, so `file_name` is empty on every row. The other 66
name-shaped hits are chance byte runs in data files (`AREA*.EMI` data
sections, one `.STR`), none in a code image.

**PC address ranges that are one PSX image's code** (deliverable 4):
`analysis/tu_psx_ranges.tsv` has all 574 runs over 336 images (a run: PC
functions in `pc` order whose twins are all in one image; PC functions with no
twin between two twins stay inside). 208 images are one PC run. The images
with 30 or more twins, their runs of three or more twins (`pc` hex, twins):

| PSX image | Twins | PC ranges |
|---|---:|---|
| `GAME.EMI#0` | 439 | `454AD0..455450` (12), `462AE0..462BA0` (6), `469BB0..46A450` (17), `46B7A0..46D850` (10), `495800..496A00` (18), `517200..519890` (49), `51AC50..526DD0` (45), `52D8F0..5366A0` (149), `537540..537D50` (3), `56D5E0..56E6C0` (20), `573080..573560` (9), `576B50..57B780` (54), `57CF60..57DF00` (11), `589810..589970` (5), `591F30..592F00` (20), `594E00..5954B0` (8) + 3 runs of 1-2 |
| `BATTLE.EMI#15` | 264 | `437780..437A10` (4), `447140..44A5C0` (74), `44AAD0..454590` (157), `494280..494E70` (9), `4FAFF0..4FC260` (20) |
| `BATTLE.EMI#3` | 235 | `42E4A0..42E930` (4), `42E9A0..4376A0` (106), `441180..447110` (110), `44A5C0..44AAD0` (14) + 1 run of 1-2 |
| `main` | 221 | `4549B0..454AB0` (5), `4976D0..498D20` (30), `516B30..5171E0` (5), `5366A0..537540` (18), `56E6C0..573080` (45), `57B780..57CEF0` (36), `588F00..589660` (8), `5903F0..591F30` (36), `595C50..596330` (7), `59E230..5B9450` (25) + 4 runs of 1-2 |
| `AREA030.EMI#4` | 182 | `464660..469BB0` (93), `528880..52D8F0` (89) |
| `SHOP.EMI#0` | 82 | `573560..5763F0` (21), `57F5D0..585940` (46), `59B470..59C2C0` (14) + 1 run of 1-2 |
| `START.EMI#8` | 77 | `496CC0..4976D0` (34), `588800..588C00` (3), `589970..58FD60` (33), `59A3A0..59A9C0` (7) |
| `SCENA08.EMI#0` | 69 | `4790F0..47A950` (6), `47BC10..47D220` (20), `47FD80..480590` (13), `519EA0..519FD0` (7), `551020..553B30` (23) |
| `SCENA03.EMI#0` | 65 | `46A5E0..46ABB0` (7), `46F2B0..46FE60` (23), `519A00..519A60` (3), `5428C0..545010` (31) + 1 run of 1-2 |
| `SCENA09.EMI#0` | 62 | `519FD0..51A370` (20), `553B30..558140` (40) + 1 run of 1-2 |
| `COMMU02.EMI#8` | 58 | `45C8E0..45E6A0` (45), `45E6D0..45F1A0` (12) + 1 run of 1-2 |
| `AREA134.EMI#13` | 55 | `41D790..41DAD0` (14), `487C10..4889E0` (41) |
| `AREA135.EMI#14` | 54 | `41DBE0..41EFE0` (31), `47D910..47E2D0` (22) + 1 run of 1-2 |
| `SCENA15.EMI#0` | 52 | `46DD50..46E120` (6), `489D70..48A5F0` (9), `48F070..48FC20` (12), `51AB50..51AC20` (3), `567DC0..56B2A0` (22) |
| `AREA077.EMI#14` | 51 | `40EC90..40F140` (19), `4771B0..477B20` (9), `47AA50..47B2B0` (6), `4FD7E0..4FDF20` (17) |
| `SCENA10.EMI#0` | 51 | `47F2B0..47FD80` (13), `51A370..51A420` (3), `51A430..51A510` (3), `558140..55BAC0` (30) + 1 run of 1-2 |
| `SCENA17.EMI#0` | 50 | `56C130..56C3B0` (9), `56C3E0..56D510` (40) + 1 run of 1-2 |
| `AREA098.EMI#13` | 46 | `413A00..413C80` (7), `480590..4823D0` (36) + 2 runs of 1-2 |
| `SCENA13.EMI#0` | 46 | `485BA0..485CB0` (4), `4889E0..488F60` (12), `51A5A0..51A780` (8), `561DB0..5646A0` (21) + 1 run of 1-2 |
| `AREA172.EMI#13` | 44 | `427BB0..428450` (19), `48CB30..48CDD0` (6), `48D070..48DDE0` (19) |
| `SCENA00.EMI#0` | 43 | `462BF0..464660` (25), `537F20..539AD0` (17) + 1 run of 1-2 |
| `AREA082.EMI#13` | 39 | `40F610..40F6E0` (4), `46EF70..46F2B0` (8), `471930..471D10` (7), `506BD0..507D90` (19) + 1 run of 1-2 |
| `AREA174.EMI#13` | 37 | `428870..4291F0` (23), `4896A0..489BE0` (8), `514A70..515000` (6) |
| `SCENA01.EMI#0` | 37 | `46D890..46DD00` (6), `5198F0..5199B0` (4), `539AD0..53DDA0` (23) + 3 runs of 1-2 |
| `AREA173.EMI#14` | 35 | `428450..428870` (9), `513860..514880` (26) |
| `MAGIC008.EMI#0` | 35 | `4A1980..4A1EC0` (8), `4A1F00..4A29C0` (19), `4A2AF0..4A3020` (8) |
| `AREA067.EMI#14` | 34 | `40CA30..40CEF0` (21), `472420..4731A0` (13) |
| `SCENA11.EMI#0` | 34 | `493430..493550` (3), `55C040..55E4E0` (28) + 2 runs of 1-2 |
| `AREA008.EMI#13` | 33 | `4014E0..401670` (10), `47E2D0..47ECA0` (23) |
| `AREA048.EMI#13` | 33 | `408FF0..4093F0` (10), `4710A0..471930` (8), `4FEF50..4FF970` (15) |
| `SCENA06.EMI#0` | 33 | `519BF0..519CB0` (4), `519CE0..519DE0` (8), `54A910..54EFC0` (21) |
| `AREA068.EMI#13` | 32 | `40CEF0..40D360` (12), `48AB30..48B200` (20) |
| `AREA103.EMI#13` | 32 | `4144E0..4146C0` (8), `4741E0..474940` (6), `503660..504900` (18) |
| `AREA108.EMI#14` | 32 | `4168E0..416D60` (7), `416DE0..417580` (10), `4737A0..473DD0` (6), `4823D0..4828B0` (8) + 1 run of 1-2 |
| `AREA119.EMI#13` | 32 | `41A730..41A9D0` (10), `48DDE0..48DF90` (9), `501BA0..502810` (12) + 1 run of 1-2 |
| `SCENA14.EMI#0` | 32 | `51A780..51AB50` (13), `5646A0..567DC0` (19) |
| `AREA188.EMI#13` | 31 | `42A320..42A4B0` (5), `42A4C0..42A7D0` (8), `485CB0..486180` (9), `493550..493B50` (9) |
| `SCENA05.EMI#0` | 31 | `46FEE0..470300` (4), `519AE0..519BF0` (10), `546390..54A910` (14) + 2 runs of 1-2 |
| `SCE15EF1.EMI#0` | 31 | `48FC20..490180` (9), `490390..491AA0` (22) |
| `AREA104.EMI#14` | 30 | `4146C0..4160E0` (27), `41C0A0..41C790` (3) |
| `MAGIC015.EMI#0` | 30 | `49FA70..4A11E0` (28) + 2 runs of 1-2 |

Every AREA and SCENA image lands in two to five PC runs: the port linked an
overlay's files apart from each other, so **no PC range is a whole overlay**;
the run, not the overlay, is the PC-side unit candidate. `main`'s last run
(`0x59E230..0x5B9450`, 25 twins) holds four `Window_*` functions, then the
port's twins of Sony library code - libgpu's primitive set-up (`Gpu_SetPoly*`,
`Gpu_SetSprt*`...), `Math_Ratan2` (`ratan2`) - and the C runtime's `rand`
(`Rand`, the BIOS thunk's twin), so it spans the MP3 decoder's range, which
has no twin.

**Not verified.** That `-G0` holds for every overlay (two gp-relative
accesses in all images says it does, but an overlay could reach the boot
image's small data some other way); that the boot image's data range
`0x80141000..0x80196800` is all its own (the zero runs below `.text` are read
as data by its code; part could be overlay buffers); that an `inversion` is
never a section restart under 12 functions (small images, where a whole image
is a few files, are where it could join two); that `shared` items are
statics rather than externs of the boot image; that the sibling's overlay
starts are complete (a missed start merges two functions, which can only add
a join's span, never cross-check one); and the `same bytes` aliases, which
take the image name of the first capture with identical bytes.

### The coordinator's join of SL1 and SL2 (2026-11-07, `source-layout/round`)

`tu_census.py` now reads `analysis/tu_psx.tsv` (`--psx`, default on; `''`
skips it): the table's `psx_unit_agreement` column holds SL2's
`<image>:<unit>:<agreement>` for 5,612 rows (every row with a placed twin),
and the sheet gained a **PSX check** column. Every other column of the table
is byte for byte what SL1 wrote.

The cross-check, measured on the joined table: of the 488 PSX units with two
or more PC twins, **59 are put in more than one SL1 unit** (57 across two,
2 across three; 268 twins), so 92 of the 676 units carry a note on the
sheet. Of the 59, SL2 itself marks 42 `interleaved` (the twins are not
contiguous on the PC: code the port placed twice or apart, such as
`AREA033.EMI#13`'s scenes that sit in both `tu_401b80` (Area16) and
`tu_403ce0` (WorldMap33)), and 14 `single` (a PSX data-only unit). So the
two sides disagree in a handful of places, not as a pattern; the notes are
the first rows to read before the moves. Not done: SL2's `psx_unit_data`
column is not joined (it is stricter and smaller than `psx_unit`).

### SL3: the move tooling and the per-function gate
- `codegen_same.py --by-function`: disassemble every object of both builds,
  key each function by its symbol, compare across objects; the same for
  initialised data by symbol. A symbol present on one side only is a
  difference. Kept beside the object mode, which the other rounds use.
- `tools/move_funcs.py`: given `tu_layout.tsv`'s file column, move each
  function's definition (with its leading comment block), its file-statics
  and `static` helpers, its `_Inject()` registration lines, its fuzz cases,
  and the callees-header constants it uses; write the new files' includes;
  rewrite `symbols.toml` `impl`; regenerate the build's `src/game` source
  list. Decide how the source list is kept so that two groups' moves merge
  **without a hand-resolved conflict** (a generated `cmake/game_sources.cmake`
  from the table is one way). Idempotent: run twice, no change.
- `analysis/tu_moves.tsv` and `tools/cite_lookup.py` (the owner's call 4:
  the docs keep their old paths, and the map traces them): the map has one row
  per moved function - old path, old first and last line at `main` before the
  moves, new path, function name, `pc`, unit id. `cite_lookup.py
  src/game/old.cpp:123` answers with the function that held that line and
  where it lives now; `--scan docs/` lists every citation it cannot resolve
  (a line outside any function: an include, a file comment). The map is also
  the bridge to other decompilations and wikis, so it keys on `pc` as well as
  on our names. SL3 writes the tool and the map's format; stage B fills it.
- A dry run on one small range (two wave files that a unit crosses) in the
  agent's worktree, the gate passing, then reverted: the tools' proof. The
  dry run's diff stays in the report, not on the branch.

### SL3 as run (2026-11-07, branch `source-layout/sl3`, report `sl1107\reports\sl3.md`)

**Built** (tools only; no `src/` file, no `CMakeLists.txt` line changed on the branch):

- `tools/codegen_same.py --by-function`: every object of both builds read as
  COFF (i386, AMD64, bigobj), each symbol disassembled with capstone and
  normalised - a relocation names its target (a file-local one also by the
  hash of its own body, a section-relative one by the bytes it points at),
  a branch resolved inside one object names the symbol it lands in, trailing
  padding dropped - then compared by symbol across objects; initialised
  data by symbol, `.bss` by size; a name in several objects (COMDATs, the
  wave files' own `B`, `U`, `S`) as the set of its bodies. `--inject-ok`
  lists a changed `*_Inject()` / `bof3::InjectAll` as `INJECT` and does not
  count it; `--anonymous-locals` knows a file-local by its body alone (for
  a commit that renames helpers, below); `--show NAME`. The object mode is
  unchanged. Whole build: about 40 s a pair on this machine; a self-compare
  reads 24,331 function definitions at i686 and 16,477 at x86_64, 0 differ.
- `tools/cpp_items.py`: a file cut into top-level items (functions,
  declarations, namespace and `extern "C"` blocks, directives) with leading
  comments, context (namespaces, `#pragma clang attribute` stack, `#if`
  stack), the names each defines and uses. Finds 10,036 of the 10,088
  `impl` functions; the 52 others are generated by macros (`magic_s16.cpp`
  40, `task_sched.cpp` 6, `sound.cpp` 5, `sound_rest.cpp` 1), and the mover
  refuses them by name.
- `tools/move_funcs.py`: the table's columns are `name`, `file` (or
  `proposed_file`, or `--file-column`), optional `pc` and `unit`; other
  columns ignored, so `tu_layout.tsv` feeds it as written. It moves the
  definition with its leading comment in `pc` order under its pragma, the
  file-locals it reaches (transitively, each in its namespaces), the
  `BOF3_INJECT` lines into a new `<Stem>_Inject()` called from
  `inject_all.cpp` right after the last source's call (after each source's
  self-test), and `impl` in `symbols.toml`. Its decisions, against the
  brief where they differ:
  - a file-local reached from two files is **copied** (default), not put in
    a header as `inline`: `inline` adds clang's inline hint and can change
    a caller; the header mode (`--shared header`, the item verbatim in
    `<S>_local.h`) exists, but the dry run's two sources each have their own `U`,
    `Long`, `S`, `UL`, so two headers met in one target at once. Shared state (a non-const
    variable, a function with a `static` local) is refused;
  - the callees constants are **not** moved: each target gets its own
    `namespace at`, and where its functions come from two groups,
    `<stem>_callees.h` makes `<stem>::at` a union (`using namespace` of
    each group's), so `at::kX` in a moved body is untouched; the old
    constants are stage E's remnants;
  - the fuzz rows are **not** moved: a row is an entry of its group's own
    table type, counted by `static_assert`s and run by that group's harness;
    the rows keep working (symbols.gen.h declares every function) and the
    tool lists their lines for stage C (section 5);
  - refused before anything is written, all reasons listed: an existing
    target file (a unit's functions move in one run), a source with
    uncommitted changes (the map's lines must be at a commit), two different
    items of one name meeting in a target, an undeclared global.
  Idempotent: a second run says "nothing to move".
- `analysis/tu_moves.tsv` (format) and `tools/cite_lookup.py`: columns
  `pc name kind unit old_path old_first old_def old_last new_path base`;
  `kind` is `func`, `local`, `copied` (one row per copy) or `shared`.
  **`move_funcs.py --map-only`** writes every row of a table without moving,
  so the coordinator writes the map once on `main` before stage B and the
  groups' moves find their rows already there - no group edits a map line.
  The coordinator adds `!/analysis/tu_moves.tsv` to `.gitignore` with that
  commit (left out here to keep this branch clear of SL1's lines there).
  `cite_lookup.py path:line` answers from the map, else from the file at
  `base` (`git show`) cut into items, and names the item and its place now;
  `--scan` lists what it cannot resolve. Before any move: 1,138 citations in
  `docs/`, `CLAUDE.md`, `CONTRIBUTING.md`, 9 unresolved, all paths that no
  longer exist (`area4_walls.cpp` 3, `field_fc3.cpp`, `field_fc3_fuzz.cpp`,
  `ruby_probe.cpp`, `gfx_sprite_uv.cpp`, `title_tasks.cpp`, and this doc's
  `old.cpp` example).
- **The source list:** no list of game files is committed. Stage B's first
  step is `python tools/game_sources.py adopt`: it cuts `_dll_sources`' run
  from its first `src/game/` entry to its last (611 entries with the 13
  `src/input`, `src/audio`, `src/render` ones inside it) into
  `cmake/game_sources_order.txt`, and `CMakeLists.txt` includes
  `cmake/game_sources.cmake`, which globs `src/game/**/*.cpp`
  (CONFIGURE_DEPENDS) and orders it: the adopted order, a new file right
  after the file its lowest-`pc` function came from (the map's `old_path`),
  `x_fuzz.cpp` after `x.cpp`. A file's place depends only on the tree, so
  any merge order gives the same list, and the link order - which orders the
  static initialisers (the `draw_pool` trap, HANDOFF) - keeps every old
  file's place. Not done on this branch, because the 64-bit round's stage D
  still edits that list. Adopted in the dry run: `codegen_same.py` object
  mode `652 objects compared, 0 differ`, and every loaded section of
  `bof3x.dll` byte-identical (only `.buildid` differs).

**The dry run** (moved, built, gated, reverted): kind 0xA7's six functions,
a PSX-ordered run cut across two wave files today - `EffectKindA7_Run`
(0x491AA0, `effect_4e.cpp`), `_Start`, `_Grow`, `_Hold`, `_Shrink`
(0x491AE0..0x491B70) and `_DrawGlow` (0x491E30, all `rest_3f.cpp`) - into a
new `src/game/effect/effect_kind_a7.cpp`. Diff: `effect_4e.cpp` -12,
`rest_3f.cpp` -100, `inject_all.cpp` +2, `symbols.toml` 6 `impl` lines, new
`effect_kind_a7.cpp` 226 lines, `.h` 5, `_callees.h` 12 (the `at` union of
`effect_4e::at` and `rest_3f::at`); 20 file-locals copied (24 reached, 4 the
same in both sources), 8 fuzz lines listed. Both presets built before and
after.

- i686, `--by-function --inject-ok`: **0 differ** (22,803 code, 11,778
  data, 3,565 bss symbols); 4 `INJECT` (the two sources' `_Inject()`, the
  new `EffectKindA7_Inject`, `InjectAll`); 8 names with more copies of the
  same body (`rest_3f::at`'s three dynamically initialised constants and
  their guards, `__cxx_global_var_init*`, one `.refptr`). The object mode on
  the same pair: 4 objects differ and one is new. A planted change
  (`0x10` -> `0x11` in `_Start`) read as `DIFFER _EffectKindA7_Start`.
- x86_64: **3 differ**, each explained. `AddressOf` (a copied helper) and
  so `EffectKindA7_Run`, its caller: the 64-bit `Low32` reports
  `__FILE__` and `__LINE__` when it fails, so the copy carries the new
  file's name and line. `EffectKindA7_DrawGlow`: the helper `SetMode`, called
  from three places in `rest_3f.cpp`, has one caller in the new file and
  clang inlined it there (frame 0xA8 -> 0xB8) - the inlining risk of section 2,
  measured at the first try, at the width section 2.4 does not gate.

**The file-local measurement** (the brief's "424 `static`/`inline`"): with
anonymous namespaces counted, `src/game`'s non-fuzz files hold 9,269
file-local code and data definitions (7,267 functions, 1,315 constants, 559
variables, 128 functions with a `static` local; 85 of them spelled `static`)
and the headers 173 functions. By which `impl` functions reach them: none
2,295 (the `_Inject()`s, self-tests, unused); one 1,595; two or more in one
proxy unit 497; two or more across proxy units 4,882 - of these 27 variables
and 7 functions with a `static` local, which a move cannot copy. The proxy
unit is a PSX-ordered run (singletons alone; 6,431 units): an upper bound,
since SL1's units are coarser. `move_funcs.py --check` over that proxy
layout (the 161 runs that span two or more of our files, 930 functions):
79 move as they are; 82 are refused - 126 name clashes (one helper name, two
codes: `At` 22, `UL` 14, `L` 11, `Word` 6, `Long` 6, ...), 22 shared state,
1 undeclared global (`g_trade_confirm_layout`).

**For stage B, from this:** (1) the name clashes are the main cost: a unit
whose sources spell `At` or `UL` differently needs a rename commit before
its move - which edits callers' bodies, so it needs the owner's word under
section 1 - gated by `--by-function --anonymous-locals`; (2) shared state
needs a hand decision per case (27 variables, 7 functions); (3) the x86_64
gate will show inlining and `Low32` line differences that i686 does not:
section 2.4 gates i686 and section 6 both widths, so the exception list
needs a class for them, or the 64-bit gate is advisory.

**Not verified:** the hosted `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`
suite and the exe host's self-tests on the dry run (no game run, by the
brief), so the moved `_Inject()` order is untested in a process; the
`--shared header` mode beyond planning; adding to an existing target file
(refused); `--map-only` on a whole table (run on the dry run's six rows,
twice, the same 30 rows); the `adopt` step against the list as stage D of
the 64-bit round leaves it.

### Stage A's end: the mover against SL1's whole table (the coordinator, 2026-11-07)

Two fixes to `move_funcs.py` on the round branch: it reads SL1's header, which
`tu_census.py` writes as a comment line (`#pc<TAB>name...`), and it keeps
**overloads** (`L(uint32_t)` beside `L(const unsigned char*)` in one source)
together instead of refusing them as a clash - 135 of the first run's 607
clash refusals were overloads.

`python tools/move_funcs.py analysis/tu_layout.tsv --check` then refuses the
whole layout, nothing written, with 648 problems touching **164 of the 556
proposed files** (the rest would move as they are):

| Refusal | Count | What it needs |
|---|---:|---|
| a file-local helper name from two sources with different bodies (`At`, `L`, `Long`, `UL`, `Message`, ...) | 468 | a rename before the move (SL3's point: a body edit, so the owner's word; machine code identical, gated by `--anonymous-locals`) |
| file-static state or a function with a `static` local reached from more than one proposed file | 107 | a hand decision each: the unit table is wrong there, or the state becomes one shared definition |
| a function `cpp_items.py` cannot cut (made by a macro) | 52 | expand the macro in place first, or move the macro with its users |
| a global named from a file that would not hold it | 18 | the global moves with its unit, or an `extern` declaration |
| other (a file-local function in the table, no `BOF3_INJECT` line for a function the host binds elsewhere) | 3 | table rows to correct |

The full list: `C:\Users\kerok\bof3x-runs\sl1107\m\check_full2.txt`.

**What stage B needs first:** the renames and the 64-bit gate are answered
(section 7a: renames allowed with each reason recorded, as step B0; the
x86-64 gate takes a named exception class); still owed, the owner's read of
the layout sheet and stage D of the 64-bit round merged.

## 4. Stage B: the moves (after stage D of the 64-bit round merges, and section 7)

Groups by `.text` range so that no two touch one unit; the boundaries are
SL1's, rounded to unit edges. A first cut, to be redrawn from the table:

| Group | Range (cut to SL1's nearest unit edge) | What lives there |
|---|---|---|
| B1 | `0x401000..0x42E400` | read from the table |
| B2 | `0x42E400..0x4551A0` | the battle engine ([`takeover-queue-round7.md`](takeover-queue-round7.md)) |
| B3 | `0x4551A0..0x4A0000` | read from the table |
| B4 | `0x4A0000..0x540000` | read from the table |
| B5 | `0x540000..0x5AB7E0` | read from the table |

Only B2's range is a documented subsystem; the other cuts are by size
(about 0x2D000..0x6B000 bytes each) and are redrawn when the table exists.

Moves are mechanical, so the groups may run in parallel; they merge one at a
time with the gate after each (section 2.4), because a merge is where a
shared helper shows. If SL3's source list cannot merge cleanly, run them one
after another instead.

Each group's report: the files made, emptied and deleted; the functions whose
`static` helper became a header `inline`, and the gate's verdict on each; the
`guessed` rows it placed and how.

### Stage B0 as run (2026-11-08, branch `source-layout/stage-b` off `main` `67c3259b`)

Run while the owner was away, by their word for "the next staged round"
with the beta cut (PR #211, `game\`) left alone: nothing of this merges to
`main` or opens a PR until the beta is wrapped, and the moves (B1..B5) wait
on the owner's read of the sheet, as call 1 says. What ran is the part of
stage B that the sheet does not decide. Root `C:\Users\kerok\bof3x-runs\slb1108`:
`base` is `main` `67c3259b` built at both widths (the gate's baseline), `wt`
the round branch, one worktree per group, briefs in `briefs\`, reports in
`reports\<group>.md`.

**The coordinator's steps first:** the two tables of 7a with their headers,
whitelisted in `.gitignore` (`74fca916`); `game_sources.py adopt` (`229b2838`;
the object-mode gate at i686 `661 objects compared, 0 differ`).

**The helper renames (groups r1..r4, then a cascade pass).** The 468 clash
rows of stage A's end, split by *source* file into four groups of 117 so no
two agents touched one file; every new name is the old name plus the
source's stem in CamelCase (`At` in `rest_2h.cpp` -> `AtRest2h`) - each group
chose that over a descriptive name on its own, because two groups naming
the same job separately could clash again in the target, and the stem
cannot. One commit per source file, the rows in `analysis/tu_renames.tsv`
in the same commit (r1's rows landed in its last commit instead). Two forms
the rows did not foresee: a `using move_script::At;` line cannot be renamed,
so the uses are written in full (`move_script::At(...)`) and the `using`
line kept as a comment (r2, r3, r4) or replaced by a same-line
`constexpr auto& AtRest2h = move_script::At;` (r1) - no line added or
removed anywhere, because the 64-bit `Low32()` message carries `__LINE__`.
**Cascades:** a helper whose text matched the other source's exactly stops
matching once a helper it calls is renamed, so new clash lines appear after
a wave (r1 found 7 inside its own files, r2 3, r4 reported 6 in others');
after the four merges the check listed 6, and a cascade pass in one worktree
cleared them in two layers (9 renames: `sprite_find`'s `kObjects, kExtra`
pair, `rest_1d`, `field_misc` twice). Merged one branch at a time
(r2 `3149e5d9`, r4, r1, r3 `c960e11b`, the pass `f95ab5e8`), the i686 gate
after each merge the same line: `codegen_same --by-function --anonymous-locals:
22977 code, 11341 data, 821 bss symbols compared (24391 / 24391 function
definitions read), 0 differ`. `move_funcs.py --check` on the tip: **0
"differs from the one from" lines** (467 on `main`: one of the 468 rows was
a `kObjects, kExtra` pair on one line, and one, `fmv_play.cpp`'s `Str`, had
nothing to rename); 448 rename rows in the table (438 helpers, 8 constants,
2 variables).

**The shared state and undeclared globals (group s, decisions only).** Its
files overlap the rename groups', so it edited no `src/` file: 135 rows of
kind `state` (115) and `global` (20) with the decision each, and a report
with the order to apply them (`reports\s.md` 4). **No case is (a)**: every
refused item is the port's own state (DIV switch bytes, `BOF3X_SHADOW`
clones, self-test stand-ins, callees seams, log switches, the data cache), so
shared state gives no evidence that the table cut a unit and no sheet row
needs to join. 102 promotions (83 variables to `extern` in the file's
header, 11 `Callees g` seams through the `_callees.h` pattern, 8 functions),
3 `static const` tables simply copied, the 16 undeclared globals already
external and only wanting an `extern` line, the 3 others named. Found on the
way, for the mover: `cpp_items.is_const` reads `const void* g_datlog_caller`
as a constant (it would copy and split state unrefused); the mover copies
each source's `using namespace <stem>;` into the target, and 54 proposed
targets then hold two `g` seams ("reference to 'g' is ambiguous"); a
promotion reads as DIFFER in every function that names it until the gate
maps the reference too. PR #209 had added four state items after the stage
A count (111 lines, not 107).

**The 64-bit gate across worktrees** read thousands of differences from
`__FILE__`'s absolute path in the `Low32()` message (measured on a tree with
no source change: 8870 differ); `codegen_same.py` now replaces each build's
source root (its `CMakeCache.txt`) by a placeholder (`7a66069f`, merged
`6fc7da2d`), after which `main` against the adopted tree reads 0 differ at
x86_64. On the renamed tree it reads **51 differ**, all one cause: the
64-bit `Low32()` takes `__builtin_FUNCTION()`, so its message names the
caller, and a renamed helper that calls it (`Addr` -> `AddrSaveMenu`,
`AddressOf` -> `AddressOfRest3e`, `PtrValue` -> `PtrValueMagicS16`, ...)
changes the string and every function that names the helper. Call 8's
`low32-site` class is this mechanism (the message's file, line or function),
so these rows go in `analysis/tu_gate_exceptions.tsv`.

**Group t** (the tooling, merged `47d3d81c`; `reports\t.md`): `codegen_same.py
--exceptions` (a listed code symbol of a known class prints `EXCEPT`, an unknown
class exits 2, an unused row is reported); the source-root placeholder above;
`PROMOTED` (a file-local that gained external linkage with its body unchanged,
its references mapped too - the `g_effect_key_off` scratch case reads 0
differ); `tools/tu_renames.py --check` / `--fill` and the CI step; a data
symbol's body ends at its next anchor; `cpp_items.py` knows a function a
macro invocation defines, and the mover moves a macro family only whole
(`task_sched`'s 6 move and gate 0 differ; `MS16_SPARKLES`' 40 are split across
4 files by the table and, sent to one file, 4 `SparkleDispatch` functions
differ - a tail `jmp` became a `call`, cause not found; `sound.cpp`'s 5 want two
globals declared; `sound_rest.cpp`'s 1 would be ambiguous beside `sound.cpp`'s
helpers); group s's mover findings as refusals (a pointer-to-const is state,
`static const` / `constexpr` locals are constants, `extern "C"` in an anonymous
namespace is not local, a bind in another file stays; a name two sources'
`using namespace` lines would both bring into a target is refused: 293 lines
in 50 targets, which want a qualification pass - a body edit, so the owner's
word). On the merged tree `tu_renames.py --check`: 583 rows, 0 problems;
`--fill` wrote 482 commits (the 101 unfilled are group s's unapplied
decisions). The 39 `low32-site` rows are in (`175028c8`); the x86_64 gate
with them: `15409 code, 850 data, 424 bss symbols compared, 0 differ; 39
excepted`.

**The self-tests on the renamed tree** (`175028c8`'s build, a clean launcher
copy `slb1108\L32`, the harness game copy `x641107\game`): the hosted suite
under `BOF3X_SHADOW='*'` exit 0, 32,265 log lines, "self-test only: done";
the exe host (`--host exe`, no `BOF3X_SHADOW` - it refuses the shadow fuzz with
a Fatal, which is its rule, not a defect) exit 0, "self-test only: done".

**The sheet in two tables** (`47f0eab2`, the owner's ask of 2026-11-08 to
review the contentions remotely): `tu_census.py` writes the 232 rows that
want a call first (167 joins - 2 files of bosses, 17 "one area", 15 "the
same names", 13 "a one-function unit, nothing separates it from its
neighbour" - and 92 rows with a PSX check, 27 both), then the 444 stand-alone
rows no neighbour or PlayStation unit contests. The table regenerates
identical. The 92 PSX checks form 41 clusters; about a third pair an area
unit with an effect unit through a recurring `u000`, which reads as an
overlay's common helper rather than a cut (a guess, being checked against the
PlayStation decompilation's duplicate-function list).

**Group a** (group s's decisions applied, merged after `1365570a`; `reports\a.md`):
what keeps every function's code identical went in - the 16 `extern`
declarations plus 7 undeclared globals the check found after group t
(`psx_gte`, `rest_2h` x2, `sound` x3, `world_map`), the `gfx_frame` inject
split, 39 of the 83 variable promotions, 1 of the 11 `Callees g` seams
(`frame_callees`, its struct in a new `frame_callees_callees.h`), and group
t's three late items (`g_icons`, `g_ja_icons`, `rest_3e::dock::g_on`). Gates at
its tip: i686 `0 differ; 43 promoted to external linkage, the same body`;
x86_64 with `--exceptions` `0 differ; 39 excepted; 43 promoted`;
`tu_renames --check: 593 rows, 0 problems`; both self-test suites pass on its
build (hosted 32,265 lines, exe host 10,356, no FATAL, every MISMATCHES 0).
**62 rows are blocked** - the source left alone, each row's reason starting
`BLOCKED (group a, B0, ...)` with the first difference - in three measured
classes, each a real code change a promotion makes:
1. i686: the optimiser folds or shrinks an internal global and cannot once it
   is external (first case `win_main`'s `g_background`: `_Game_WinMain` stores
   `1` -> `0`, `WinMain_InputAllowed` `movzx` -> `mov al, 1`; also `msg_pool`'s
   `g_pool`, `map_layers`' `g_side_dup`);
2. the section's data layout changes, so neighbouring file-locals change
   extent (first case `msgbox`'s `ruby::g_hidden`; `dat_load`'s 11 and
   `sound`'s 4 are here);
3. x86_64 only, i686 identical: an internal function pointer folded to a
   direct call goes through the pointer once external (first case
   `Text_DrawImmediate`: `call Text_DrawAt` -> `call qword ptr [rip] g_draw_at`).
44 variables fall in these; 10 seams (5 class 3; `title_states`, whose
pooled `_GLOBAL__sub_I_*` initialiser an exception row cannot name for one
file; 4 whose structs use the `.cpp`'s own aliases and do not compile as
written); and all 8 function promotions (i686 gets an out-of-line copy and
the callers change - `Sprite_Draw` no longer inlines `Draw` - and the i686
gate has no `inline` class by call 8). Ten `=` rows that relied on those
functions keep their refusal. **The same differences come back in B1..B5
the moment a reader moves out of the file**, whatever B0 does: so the owner's
call is whether classes 2 and 3 become x86_64 gate classes, and whether class
1 and the function promotions are acceptable on i686 (the gate would then
need a class there too, against call 8's "i686 stays exact"). The apply
scripts (`a_scratch\promote.py`, `promote_fn.py`, `seam.py`, `movetype.py`)
re-apply any blocked row once its class is accepted. Tool fixes on the way:
`codegen_same`'s promotion pairing picks the file-local of the same object
(two one-byte `g_faerie_name_end`s pooled under one body); `tu_renames
--check` reads `<stem>_callees.h` too; `move_funcs` follows nested project
includes for a declaration. `move_funcs.py --check` on the tip: file-static
state 107 -> 64, static local 5 -> 5, undeclared global 26 -> 0, the sparkle
family 1, the ambiguous `using` names 289 lines in 50 targets (untouched).

**The PlayStation side of the sheet's flags** (the owner's ask of 2026-11-08;
the read is private scratch, `reports\frost_psx.md`, the method ours:
`tu_psx.py`'s loader and `mask_words` over all 407 JP images): the US
decompilation's dedupe is a hashing tool plus a shared-header convention that
saves no list and sees 24 images, so the measurement was redone here. Of the
92 flags then: 45 rows are common code duplicated across overlays (not a
cut; what recurs is the overlay entry dispatcher, `u000` being merely each
image's first unit), 4 common by near-copies only, **37 one overlay's own
code** (the real cut questions: window and inversion joins in `GAME.EMI`,
`main` and `BATTLE.EMI`, and area overlays that link their effect-kind code
beside the area script, which the PC moved into `effect/` units), 4 two PC
functions twinned to one PSX function, 2 a bug of ours (the check joined
`Title_Task` and `AreaMap_FrameAreaBD` on an empty unit key; fixed
`f47eacfc`, with the unit size counted in PlayStation functions, not PC
copies). **28 twin claims re-pointed** (`1365570a`, `symbols.toml`): 18 twins
that pointed at another overlay's copy while their own overlay holds one
(Area16's and Area151's frame and box helpers, `Area75_DrawWindow`,
`Area121_RingRise`, `Magic117_Brighten`, `Area188_ShiftCameraDown2`) plus 10
companions at the same constant shift, each confirmed by masked words (5
raw-exact, the rest 1..5 words apart); areas 75, 121, 188 and Magic117 each
became one unit again, 11 flagged rows cleared, no proposed file changed,
669 units. `Area130_ChoiceTailState2` stays on `AREA033`: `AREA130` holds no
copy, the body is the world-map areas' shared choice helper, so row 29 keeps
its flag until the census treats such shared code as common. On the 47
joins the decompilation's unit table agrees with five and objects to one:
row 585, `tu_52d8f0` in `field_4`, whose boundary it puts inside ours at
`Field_ActionState` (a one-function unit there) - the owner's. The sheet now
reads **212 rows that want a call, 457 with one recommended layout**.

**Left in B0, for the owner** (section 7b collects the questions): the 62
blocked promotion rows (the gate classes above); the qualification pass for
the 50 ambiguous targets (a body edit); row 585's extent; the sheet's 212
rows; the `SparkleDispatch` difference and `Gfx_DrawOTag` (differs on i686
when moved to `platform/d3d.cpp`: a copied helper inlined, so it cannot leave
`d3d_list.cpp` as the table has it - a sheet row).

## 5. Stage C: what follows the moves (one agent, after B)

- `tu_moves.tsv` complete; `cite_lookup.py --scan` over `docs/`,
  `CLAUDE.md`, `CONTRIBUTING.md` and the `recipes/` comments, with the
  unresolved list read by hand. The docs' paths are **not** rewritten (call
  4); [`docs/README.md`](README.md) says once that a `src/game/` path in a
  doc written before this round resolves through the map.
- The fuzz files: one per new file, named after it; the hosted build's fuzz
  table regenerated.
- `tu_census.py --check` in `checks.yml`: a function's `impl` is its unit's
  file. A function of ours that is not Capcom's (a helper we added) is listed
  in the table as `ours` and may live where its author puts it.
- `docs/source-layout.md`: the table's reader - units by subsystem, each with
  its range, its file, its PSX twin unit and its confidence. The group docs
  of the takeover rounds keep their history; a line at the top of each says
  where its functions now live.
- [`SCAFFOLDING.md`](SCAFFOLDING.md) and [`docs/README.md`](README.md): the
  new tree's shape, where a new function of ours goes.

## 5a. Stage E: the comment clean-up (I48), at the end (the owner's call 5)

After stage D's proof, on the new layout, so it runs once: the typed-data
round's retire notes (`// kX: now Name[i].field (symbols.toml).`), comments
that name a wave file or a group that no longer exists, and stale
`_callees.h` remnants. Its own commits, gated by `codegen_same.py` (comments
only, so every object identical), never mixed into a move commit. The brief
is [`IDEAS.md`](IDEAS.md) I48.

## 6. Stage D: the proof and docs (the coordinator)

The gate on the tip at both widths; the hosted self-tests with
`BOF3X_SHADOW='*'` (the relocatable-image round's lesson: a move changes
static-initialisation order across files, which only the shadow suite saw
last time); the ten routes on the exe host at the image's default base and
at `BOF3X_IMAGEBASE=0xA00000`, the state hash against the current references;
the 64-bit build's routes beside them. STATUS, HANDOFF, IDEAS I44's outcome,
this doc's "as run". **The ledger:** nothing a player sees changes, so no DIV
is expected; the doc says so.

## 7. For the owner

**Answered by the owner on 2026-11-07**, when stage A was launched:

1. **Granularity: decided file by file.** Some units are better apart (the
   owner's example: enemy groups apart from area data), others better
   together (bosses, fonts). So SL1 proposes, unit by unit, whether it stands
   alone or joins its neighbours in one file, with a reason, and **the owner
   decides each row** on that sheet before stage B. A unit is never split
   across files, whatever the call.
2. **Subsystem folders**, `src/game/<subsystem>/`, not one large folder. SL1
   proposes the subsystem set with the file names.
3. **File names as proposed:** SL1 names each file from its contents and its
   PSX overlay; the owner reads the list with call 1's sheet.
4. **The docs keep their old paths, with a map** (`analysis/tu_moves.tsv`,
   `tools/cite_lookup.py`), so a citation still traces to other
   decompilations and wikis by the names and addresses it was written with.
   No doc is rewritten for a move.
5. **The comment clean-up (I48) at the end of this round**, as stage E
   (section 5a), after the proof, so it runs once on the final layout.
6. **Stage A launched 2026-11-07** beside the 64-bit round's stage D (it
   changes `tools/` and `analysis/` only). Stage B after stage D's merge and
   the owner's reading of the sheet; on 2026-11-07 no open PR and no unmerged
   local branch touched `src/game`.

### 7a. Answered by the owner after stage A (2026-11-07)

7. **Renames of file-local helpers: allowed, each with its reason
   written down.** The owner: as long as the reasons are documented, so that a
   rename is easy to revisit if it turns out wrong. So stage B opens with a
   rename step (B0) before any move:
   - one commit per source file (or per clash family), message naming each
     rename `old -> new` and why (the clash it removes: the two sources, the
     target file they meet in);
   - every rename also a row in `analysis/tu_renames.tsv`: source file, old
     name, new name, kind (helper, constant, variable), reason, the target file,
     the commit - kept beside `tu_moves.tsv` and never pruned, the record to
     revisit from;
   - the gate is `codegen_same.py --by-function --anonymous-locals` on i686
     (the code identical, helpers matched by body), and the 64-bit gate under
     call 8;
   - a rename only removes a clash `move_funcs.py --check` reports; no
     rename for taste. The 107 shared-state cases and the 52 macro-made
     functions are not renames and are decided one by one, each with its
     reason in the same table (`kind` = `state` / `macro`).
8. **The 64-bit gate: an exception class, not advisory.** The i686 gate stays
   exact. On x86-64 a function may differ only in a named class, listed in
   `analysis/tu_gate_exceptions.tsv` (symbol, class, reason, the commit that
   moved it), and anything outside a class is a stop as on i686. The classes
   SL3's dry run showed:
   - `inline`: the compiler inlined (or stopped inlining) a moved or copied
     helper because its callers in the file changed;
   - `low32-site`: `Low32()`'s failure message names the new file and line.
   A new class needs the coordinator's entry in this doc with its first case.
   `codegen_same.py` gains `--exceptions FILE` to apply the list (stage B's
   first tooling change, with B0).

### 7b. Open after stage B0 (2026-11-08), the owner's

9. **The sheet's 212 rows that want a call** (`analysis/tu_layout_sheet.md`,
   first table; the second table's 457 rows have one recommended layout).
   The join classes as first written: 2 boss files (one file per boss
   instead?), 17 "one area", 15 "the same names", 13 "a one-function unit,
   nothing separates it from its neighbour". The PSX flags: 37 rows are one overlay's own code and
   worth the look; the common-code ones are not a cut. Row 585 (`tu_52d8f0`,
   `field_4`): the PlayStation decompilation's unit boundary falls inside
   ours at `Field_ActionState`. A per-class answer is enough ("joins stand as
   proposed", "bosses one file each") and is recorded here.
   **Answered in part on 2026-11-08:** the "a one-function unit" joins stand
   as proposed (28 rows); the "one area" joins stand as proposed (32 rows:
   one area's field scripts in one file); **the boss units stand alone, one
   file per unit** (59 units, `boss/boss_nue.cpp`, `boss/boss_myria.cpp`,
   ...). The boss join had been rule-only: `tu_census.py` joined any two
   neighbouring `boss/` units on the owner's remark and measured no likeness.
   The evidence ran the other way - the units are named one or two bosses
   each, and where a unit has PlayStation twins they sit in that boss's own
   overlay (`BOSS012`, `BOSS018`, `BOSS029`, `BOSS034`, `BOSS035`), no unit
   in two overlays and no two neighbours sharing one - so the rule was
   dropped and the sheet regenerated: 613 proposed files, 153 rows that want
   a call, 516 settled. **The 516 settled rows are confirmed** (the owner,
   later the same evening): every stand-alone unit keeps its own file.
   **The 36 "the same names" rows were audited and decided the same evening**
   (`slb1108\same_names_audit.txt`; the measure per seam: calls across,
   unit-local statics read on both sides, PSX images, the hard cut between
   them). Every one of the 14 groups was split by a PlayStation cut, of two
   kinds. *Class A, 10 groups, 28 rows:* the twin of a one- or two-function
   unit inside the run points at another overlay. A masked scan of the host
   overlay (group p's method) found a copy for seven of them -
   `EffectKind81_FindFreeDrop`, `EffectKindB9_SpawnSpark`, `Leech_WaitOrbs`,
   `EffectKind18Sub54_Pulse`, `Scena10_Object11`, `Scena10_Object12`,
   `Scena18_EnterArea` (this one at the very same address) - and those twins
   were re-pointed in `symbols.toml`, which merged their units (661 units
   now, the whole Leech spell one unit of 23). The other four
   (`BattleIntro_Finish`, `Magic018Row53_Wait`, `Scena16_EnterArea`,
   `Scena17_Intro04`) are a function that on PlayStation lived in one overlay
   and was reached from others, which the PC build holds one copy of among
   the host unit's functions: the join is right, and those 16 rows stand as
   proposed. *Class B, 3 groups, 8 rows:* one image, but the PlayStation link
   order runs backwards at the seam (`battle_4`, `field_4` with its four
   units, `gpu`): two objects there, so they **stand alone**; `tu_census.py`
   now refuses a same-names join across a `psx back` cut, areas excepted
   (the "one area" call). With every join class decided, the sheet's first
   table holds the PSX checks only: 71 rows want a call, 590 are settled,
   617 proposed files (71 flagged rows then; 67 after the Celerity re-point
   below). The PSX flags and row 585 are read next.
   **The PSX flags, read again after the merge of `main` (2026-11-08 night;
   `slb1108\reports\psx_flags_units_1108.txt`, `psx_flags_rows_1108.txt`, the
   classifier of the PlayStation read run over the regenerated tables).** 67
   rows carry a flag. *32 are common code* (28 masked-identical copies in
   other overlays, 4 near-copies): a shared helper whose twin was paired to
   one overlay's copy; no cut. *2 are a double claim* (AREA091: two PC copies,
   `Area91_EffectRings` and `EffectKind18Sub21_DrawSpiral`, both twinned to
   one 1,028-byte function; no cut). *33 rows, 16 units, are one overlay's
   own code*, in four classes. (a) An area overlay that compiled its effect
   kind beside its area script (AREA068, 082, 085, 091, 098, 168; 12 rows):
   the PC links every effect kind in the effect range, far from the area
   code, so on the PC they are separate objects and the proposal (apart)
   stands. (b) Cleared by an approved join (SCENA16, SCENA17; 4 rows): the
   flag is per unit, the file is one. (c) A helper far from the unit that
   claims it (MAGIC004's `Magic008_DrawFlash`, MAGIC065's
   `MagicFx_EndWhenIdle`, SCENA13's `ClutStrip_Restore`, AREA049's
   `Area34_SpawnEffect51`, BATTLE u371's `Battle_Start*Magic` against
   `BossActor_Clear`, main u005's `Sprite_UpdateScreen*` against
   `Field_DrawFrame`; 13 rows): not neighbours on the PC; apart. (d)
   **Neighbouring PC units that the PlayStation holds in one unit** (6
   rows): `magic/bone_dance` | `battle/bone_dance_follow` (rows 392-393; the
   census's cut there is a back-order read against a twin of unknown image,
   `BattleFx_FreeTask` at `0x800C2120`, so it is weak); `field/field_6` |
   `field/field_7` (574-575) and `field/area_map` | `field/kind_2` (604-605),
   both a back-order within one image against a strong PSX window join -
   the two readings contradict. **The owner joined all three** (later that
   night): recorded in `analysis/tu_joins.tsv`, which `tu_census.py` now
   reads (`--joins`) and applies over its own rules, so the sheet shows the
   join with the owner's reason; 612 proposed files, no unit changed. `BattleFx_TintActor` was re-pointed to MAGIC062's exact copy and
   the Celerity spell became one unit of 18, as PSX MAGIC062 u001 has it.
   **Row 572** (`tu_52d8f0`, `field/field_4`, 70 functions, the old row 585):
   the US decompilation's unit boundary falls inside it at
   `Field_ActionState`; a unit is never split by the sheet's rule, so this
   stays as one file unless the owner says otherwise.
10. **The gate for shared state** (group a's three classes, section 4): a
   promotion that changes the object is refused today. Do classes 2 (data
   layout) and 3 (an x86_64 indirect call through a now-external pointer)
   become x86_64 exception classes, and are class 1 (i686 folding of an
   internal global) and the 8 function promotions (an out-of-line copy on
   i686) acceptable - which means a named class on i686 too, against call
   8's "i686 stays exact"? The same differences return in B1..B5 as soon as
   a reader of the state moves out of its file.
   **Answered 2026-11-08 night:** classes 2 and 3 become named gate classes
   (`data-layout`, `x64-indirect`; group g builds them and re-applies the
   rows, worktree `slb1108\g`, brief `briefs\g.md`); class 1 and the 8
   function promotions stay refused, their readers move with them.
11. **The qualification pass:** 289 names in 50 proposed targets that two
   sources' `using namespace <stem>;` lines would make ambiguous (`g` seams
   mostly) want writing as `<stem>::g` in the moved bodies - a body edit by
   section 1's rule, so the owner's word.
   **Answered 2026-11-08 night: run it.** Group q qualifies the names in two
   passes (100 files first, `briefs\q.md`; the 13 files group g also edits
   after g merges), each name a `qualify` row in `tu_renames.tsv`.
12. **The commit trailer:** the groups' commits carry `Co-Authored-By: Claude
   Opus 5.5` (the model that did the work), the coordinator's `Claude Fable
   5.1`; both are signed off. Left as they are unless the owner wants one.

## 8. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/tu_census.py`, `analysis/tu_layout.tsv` | SL1 |
| `analysis/tu_psx.tsv`, the agreement column's input | SL2 |
| `tools/codegen_same.py` (the new mode), `tools/move_funcs.py`, `tools/cite_lookup.py`, the `tu_moves.tsv` format, the build's source-list shape | SL3 |
| each `.text` range's files | its stage B group |
| `docs/`, the fuzz table, `checks.yml` | stage C |
| `STATUS.md`, `HANDOFF.md`, the proof | the coordinator |

Merge order: SL1, SL2, SL3 (any order, no shared files), the coordinator's
join of SL2's column into the table, the owner's read (section 7), B1..B5 one
at a time with the gate after each, C, D, E. Every commit `git commit -s`. The
round branch `source-layout/round` off `main` at launch; one harness root
for the round under `C:\Users\kerok\bof3x-runs`. **Numbering:** no DIV or D
number is expected; if one is needed, check `main` at the time (on
`39d88ef9` the ledger ended at DIV-0204 and the defects at D405; stage D of
the 64-bit round took D406 and D407 on 2026-11-07).

## 9. Not in this round

Renaming functions or globals; changing any body; the image layout; the wide
image (I53); `src/render`, `src/input`, `src/hook`, `src/launcher`
beyond what a moved `_Inject()` line needs; the runtime's and the decoder's
ranges.

## 10. Not verified

- That MSVC6 emitted a unit's functions in source order with nothing between
  them (the usual behaviour without `/Gy`; with `/Gy` and `/OPT:REF` the
  linker may drop or reorder COMDATs). SL1 checks it against the PSX runs:
  if PC order breaks inside a PSX run in a way that is not a boundary, the
  assumption fails and the table's `placed` rows lose their basis.
- The COMMON-symbol rule for `.bss`.
- Which Rich record is the game's, and so the expected unit count.
- That moves leave every function's code the same: the 424 `static` /
  `inline` definitions are where it can fail, and SL3's dry run is the first
  measurement.
