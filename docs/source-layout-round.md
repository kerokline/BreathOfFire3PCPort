# The source-layout round: staging (IDEAS I44's last step, I5 folded in)

**Status:** IN PROGRESS (stage A launched 2026-11-07 on `source-layout/round`
off `main` `39d88ef9`, worktrees under `C:\Users\kerok\bof3x-runs\sl1107`; the
owner's calls answered the same day, section 7. Staged 2026-11-07 on
`docs/source-layout-staging`, the owner's pick of the next large round. Stage
A needs no `src/game` change and runs beside the 64-bit round's stage D;
stage B waits for stage D's merge and the owner's reading of SL1's sheet.)

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

**What stage B needs first** (the owner's, section 7a): the renames, the
64-bit gate's standing (SL3's dry run showed inlining and `Low32`'s file and
line differing at x86-64 for a pure move; i686 identical), and the owner's
read of the layout sheet.

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
`39d88ef9` the next free are DIV-0205 and D406, which stage D of the 64-bit
round may take first).

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
