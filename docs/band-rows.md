# The band tool: a group of round twelve's cut, with its clones, reach, raw references and cross-group edges

**Status:** MEASURED (2026-09-28, round twelve stage A, group RT, at the
round's base `430f34b`) - `tools/band_rows.py` exists and has been run over
all fourteen groups of the cut (section 3). No function taken, nothing
named, no C++ changed. It is the first command each group of
[`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) runs;
section 4 is what the groups and the coordinator should know before
stage B.

## 1. What it reads, and how to run it

```
python tools/band_rows.py --exe .../bof3/BOF3.exe --analysis .../analysis \
    [--symbols symbols.toml] [--sibling .../BreathOfFire3Recomp] [--cut <tsv>]
    ... --groups                       -> the cut's groups with counts, and the tool's reading of each
    ... --group BE4                    -> a function a line: extent, hidden / host, twin, name, reach, flags
    ... --group BE4 --clones           -> the clone tables (C++), BH_ for BE*, SH_ for the field groups
    ... --group FO --clones --harness boss      (override the namespace)
    ... --function 0x446DE0            -> one row and its clone (a comma-separated list works;
                                          an address outside the cut wants --harness)
    ... --group BE4 --refs             -> every raw 0x... of the group's functions in src/game
    ... --edges [--group BE4]          -> the calls between groups of the cut (markdown)
    ... --tsv <path>                   -> also write the rows printed (all of them with --groups)
    ... --harness area                 -> area_harness (AH_) clone tables (round thirteen, section 6)
    ... --byte-tables                  -> bound a two-level switch by its byte table (section 6; off by default)
```

`--cut` defaults to `<analysis>/round12_cut.tsv` (the plan's section 8
makes it). **The tool writes nothing unless `--tsv` names a path, and
refuses a `--tsv` that is the cut table** - round eleven's boss tool
overwrote its own cut once ([`takeover-queue-round11.md`](takeover-queue-round11.md)
section 4). `--with-ours` keeps functions already ours in `--clones` (a
taken group re-checking its table). From a worktree give `--exe` and
`--analysis` the main checkout's paths. A `--groups` run takes about five
seconds; a `--group` run about as long.

Inputs: the exe; the cut table; `analysis/pc_funcs.json`,
`pc_hidden.json` and `symbols.toml` (every start any list knows - the span
bounds); `analysis/pairs_propagated.json` (the PSX twin, a hypothesis as
everywhere else) and the sibling's `symbols.toml` for the twin's name when
it has one; `analysis/pc_xref.json` (merged into the reach, section 2).
The descent helpers and the clone sites are `magic_rows.py`'s
(`decode`, `_jump_cap`, `_byte_table`, `_cmp_bound`, `clone_sites`),
imported, so a clone table means what a spell, area or boss group's did.

The output lists addresses, sizes, counts and our own source lines, but it
is derived from copyrighted game code: keep it under `analysis/` or a
scratch directory, never in a commit (CLAUDE.md rule 1).

## 2. The method

Round twelve has no enumerating table: a group owns the functions the cut
table lists for it (the plan's section 3). So the tool takes each row and
reads the exe for it.

1. **The extent, read from the code.** A recursive descent of the start
   (capstone) inside its **span**: from the start to the next start that
   `pc_funcs.json`, `pc_hidden.json`, `symbols.toml` or the cut knows. It
   follows fall-through and every branch whose target is in the span, and
   jump tables (`jmp [r*4 + T]` with `T` in `.text`, bounded at the `cmp` /
   `ja` before it, the byte table of MSVC's two-level switch to its bound).
   Padding (`nop`, `int3`) ends a path. The extent is one past the last
   byte reached, tables included. The cut's size is the catalogue's - to
   the next start, padding and all - and is printed beside it where they
   differ, with `padding` when the difference is only padding.
2. **Starts that are not functions.** A start whose first instruction its
   host's code reaches - by fall-through, an in-function branch, or as a
   case of the host's jump table - and that nothing else names (no call,
   jump or immediate from other code, no `.data` cell, no `.text` cell
   outside the host's own tables) is flagged `inside host, no address
   reference` (round eleven's `0x44103A` shape). The host is the cut's
   `host` column (its descent over its whole recorded body, to the next
   `pc_funcs` start) and the start's predecessor (the previous start, or
   the last piece of unlisted code before it, step 3). **Such a start is
   then absorbed**: the function before it reads on over it, to a
   fixpoint, so its host's clone carries its cases.
3. **Code no list has.** After a function's extent and its padding, code
   on the next 16-byte boundary before the next start (MSVC aligns
   functions to 16; `magic_rows.py`'s rule) is descended as a function of
   its own and printed as an extra row of the group, flagged `not in the
   cut`. Bytes after that which are neither padding nor such code are
   flagged `uncovered` (a table the descent did not bound, or code only a
   case reaches). A start that does not decode, or lies in a table another
   descent read, is flagged `data` and gets no clone.
4. **The reach.** A linear sweep of `.text` (capstone's lite mode,
   restarted at every known start and one byte past anything that does not
   decode) for every `call` / `jmp` / `jcc` to the start and every
   immediate or displacement naming it (`stack` - a stack table's
   `mov [esp + k], imm32` -, `push`, `store`, `reg`, `mem`);
   `pc_xref.json`'s entries are merged in by site (it indexes no rel32
   transfer - HANDOFF's trap - and at `430f34b` added no immediate the
   sweep had not seen). Then every aligned dword of every section holding
   the address: a `.data` cell is named by the `symbols.toml` table that
   covers it (by its `count`), else by the last named table inside the run
   of code pointers holding it, else as `run <start>[index]`. A caller is
   named as ours (its `symbols.toml` name), this round's (its group) or
   Capcom's.
5. **The clone** is `magic_rows.clone_sites` over `[start, extent)`: every
   E8 / E9 rel32 leaving it, every stack-table immediate, every jump table,
   and a `REFUSED` line for what a byte copy cannot carry. The comment
   block above each names what reaches it, each callee as ours / this
   round's / Capcom's, the `.data` addresses its code names (with their
   `symbols.toml` names), and the flags. The `Clone` row is
   `boss_rows.py --clones`'s form with `BH_N` / `SH_N`; `ret_mask`, the
   shape and the state fields are left to the group (`// ret_mask: yours`).
6. **Edges**: every call, jump or code immediate in a cut function's
   extent whose target is a function of another group.

## 3. The proof, at `430f34b`

### 3a. `--groups` reproduces the plan

The cut's counts per group - functions, hidden, bytes (the cut's sizes),
already ours by an `impl` - are the plan's section 3 table exactly, group
by group: **631 functions, 418 hidden, 123,089 bytes, 0 ours**
(BE1 39 / 28 / 7,063 through FS 52 / 37 / 14,676). No figure differs.

### 3b. The fourteen groups read

| Group | Cut + not listed | Bytes read (cut's) | Extents differ: code / padding only | Not functions (absorbed) | Code no list has | Clones | Edges out / in |
|---|--:|--:|--:|--:|--:|--:|---|
| BE1 | 39 + 2 | 6,778 | 1 / 30 | 0 | `0x432430`, `0x432440` | 41 | 4 / 12 |
| BE2 | 48 + 0 | 7,811 | 0 / 33 | 0 | - | 48 | 10 / 0 |
| BE3 | 47 + 1 | 5,416 | 1 / 39 | 0 | `0x437230` | 48 | 11 / 5 |
| BE4 | 56 + 0 | 8,829 | 0 / 23 | 0 | - | 56 | 4 / 17 |
| BE5 | 47 + 6 | 6,767 | 5 / 33 | 1: `0x44B8D0` | `0x44B5E0`, `0x44B870`, `0x44B920`, `0x44C5C0`, `0x44FF60`, `0x450250` | 53 | 11 / 3 |
| BE6 | 40 + 0 | 10,130 | 2 / 14 | 1: `0x452460` | - | 40 | 0 / 15 |
| BE7 | 31 + 0 | 7,133 | 0 / 20 | 0 | - | 31 | 12 / 0 |
| FC1 | 41 + 0 | 6,558 | 0 / 30 | 0 | - | 41 | 4 / 0 |
| FC2 | 40 + 0 | 5,917 | 0 / 26 | 0 | - | 40 | 5 / 2 |
| FC3 | 57 + 0 | 7,156 | 0 / 45 | 0 | - | 57 | 12 / 0 |
| FE1 | 45 + 0 | 7,027 | 0 / 22 | 0 | - | 45 | 14 / 1 |
| FE2 | 44 + 8 | 10,194 | 4 / 13 | 1: `0x56D240` | `0x536BF0`, `0x536EC0`, `0x56DA10`, `0x56DB80`, `0x56DDD0`, `0x56DE30`, `0x56DE50`, `0x56DF10` | 52 | 0 / 30 |
| FO | 44 + 1 | 11,289 | 6 / 18 | 5: `0x577600`, `0x5776A0`, `0x577B50`, `0x578550`, `0x578790` | `0x576960` | 45 | 0 / 5 |
| FS | 52 + 1 | 13,801 | 1 / 35 | 0 | `0x58CAE0` | 53 | 3 / 0 |
| **all** | **631 + 19** | | **20 / 381** | **8** | **19** | **650** | **90** |

- **Extents**: 381 of the 631 differ from the cut only by the padding
  after them; 230 agree. The 20 that differ in code: eleven functions
  followed by code no list has (the catalogue counted that code into the
  function before it - `0x44B3A0`'s 1,328 bytes are 576 of its own, then
  all of the unlisted `0x44B5E0` and the head of `0x44B870`), seven case
  starts cut out of their host, the host that absorbs one of them
  (`0x4523C0`'s 160 become 420 with its case `0x452460` and its table),
  and `0x578A40` (below).
- **Not functions**: eight starts of the cut are cases of their host's
  jump table and nothing else names them. Five hosts are ours already:
  `Scena17_DrawLine` (`0x56D240`, case 3 of its switch - the same finding
  as [`scena_sc15.md`](scena_sc15.md), made independently),
  `MoveScript_GroupE` (`0x577600`, `0x5776A0`), `MoveScript_GroupF`
  (`0x577B50`), `MoveScript_GroupC` (`0x578550`), `MoveScript_Group8`
  (`0x578790`); two are cut rows (`0x452460` of `0x4523C0`, BE6) or code no
  list has (`0x44B8D0`, a case of `0x44B870`, BE5). One start outside the
  cut is absorbed the same way: `0x56DB00` (`pc_hidden`), case 4 of the
  unlisted `0x56DA10`'s switch (read by hand, `cmp eax, 4` / `ja` / `jmp
  [eax*4 + T]`, entry 4).
- **Code no list has**: 19 functions in the groups' spans that neither
  `pc_funcs.json`, `pc_hidden.json` nor `symbols.toml` lists, each reached
  by something - a `.data` cell (`0x56DA10`, `0x56DB80`, `0x56DDD0`,
  `0x56DE30`, `0x56DE50`, `0x56DF10` are `Field_ModeTailKinds` slots 5, 10,
  6, 27, 44, 55; `0x44C5C0` is `Effect_Handlers[23]`), a call (`0x44B5E0`,
  `0x44B870` from `0x44B3A0`; `0x576960` from ours `SaveMenu_DrawSlots`) or
  a jump. The cut does not list them; the tool prints them with the group
  whose span holds them.
- **Flagged `uncovered`** after the flags above are only the absorbed
  cases' rows (their host's other code) and `0x578A40` (FO, hidden in ours
  `MoveScript_Group8`, reached only by a `.text` cell the descents did not
  place in a table; 0x87 bytes after it unreached). No start `falls into`
  the next; no start is `data`; every start is reached by something.
- **Clones printed**: 650 (every row; none is ours, none is `data`).
  `REFUSED` lines: three, all in absorbed case rows jumping back into their
  host (`0x452460`, `0x56D240`, `0x577B50`) - their host's clone carries
  them.
- **Raw references in `src/game`** (`--refs`): 1,180 lines naming 126 of
  the 650 functions - BE4 502 (22 functions), FE2 184 (14), BE3 138 (12),
  BE6 107 (11), the rest under 60 each. Comments count; the group reads
  each.

### 3c. Three clones checked by hand

Each against a capstone listing of its range (scratch only):

- **A cdecl helper**, `0x446110` (BE4): `Battle_CalcDamage` (ours) pushes
  three words, calls it and adds 12 to `esp`; it reads `[esp + 8]` and
  answers in `eax`. The listing's last instruction is the `ret` at
  `+0x9D`, reached by the `jne` / `jl` to `+0x98`: the extent `0x9E`
  agrees. Its three calls to `Rand` `0x5B93D2` sit at `+0x2D`, `+0x50`,
  `+0x59`: the tool's `CallSite`s. The cut says 158 too.
- **A hidden state handler**, `0x448BA0` (BE4, hidden in `0x447F40`):
  the `.data` cell `0x64E4A0` holds it (`pc_hidden.json` names the same
  cell), the first of a run after `BattleItemCmd_TargetSteps`; one call at
  `+0x21` to `0x591810`, `ret` at `+0x50`, then 15 bytes of `nop` to the
  next start. Extent `0x51` against the cut's 96: padding only, as the
  tool says.
- **A function with a jump table**, `0x4523C0` (BE6): `cmp ecx, 7` / `ja` /
  `jmp [ecx*4 + 0x452544]` at `+0x73`, so eight entries, the table at
  `+0x184` and its displacement at `+0x76` - the tool's `JumpTable {0x76,
  0x184, 8}`; the code's last `ret` at `+0x182`, the table to `+0x1A4`: the
  extent `0x1A4`. Entry 4 is `0x452460`, the cut's hidden row whose only
  reference (`pc_hidden.json`'s `.text` cell `0x452554`) is that entry -
  the absorbed case of section 3b. Calls: `Battle_ActorIsOut` at `+0x22`,
  `0x452570` at `+0xE9`, `+0x116`, `+0x13B`, `+0x160`, all agreed.

### 3d. The regression against `boss_rows.py`

BSC's unit `K12` (Amalgam, 11 functions, `0x4396B0..0x439B06`):
`boss_rows.py --unit K12 --clones --no-write` with the `symbols.toml` of
`4a15d61` (before round eleven took them; at `430f34b` they are ours and
it prints nothing), against `band_rows.py --function <the 11> --harness
boss` with the same `symbols.toml`. **The 18 C++ lines (7 `CallSite`
arrays, 11 `Clone` rows) are identical**, sizes and call sites alike, once
this tool's `// ret_mask: yours` comment is stripped.

`python tools/ledger_check.py`: 62 ledger entries, 0 errors.

## 4. What the groups and the coordinator should know

- **Run it first**: `--group <G>` for the rows and flags, `--refs` for the
  rebinding list (the plan's section 6: the callers are ours, so rebinding
  is each group's first step), `--clones` for the fuzz file.
- **Eight starts are not functions** (section 3b). Five sit in functions
  already ours: the group takes nothing there but should check that ours
  handles the case (it does, if ours was read to its last instruction) and
  say so in its doc. `0x452460` goes with `0x4523C0`'s clone (BE6);
  `0x44B8D0` with `0x44B870`'s (BE5).
- **Nineteen functions are in no list**; the coordinator decides whether
  the group whose span holds them takes them (round ten's rule, "a group
  owns the functions whose address lies in its band", would say yes). FE2
  would grow by 8, BE5 by 6.
- **Cross-group edges** (`--edges`, 90): BE1 -> BE4 4, BE2 -> BE3 5,
  BE2 -> BE4 3, BE2 -> BE6 2, BE3 -> BE4 6, BE3 -> BE5 1, BE3 -> BE6 4,
  BE4 -> BE5 2, BE4 -> BE6 2, BE5 -> BE4 4, BE5 -> BE6 7, BE7 -> BE1 12;
  FC1 -> FC2 2, FC1 -> FO 2, FC2 -> FE1 1, FC2 -> FE2 4, FC3 -> FE2 12,
  FE1 -> FE2 14, FS -> FO 3. All calls or jumps, no code immediate; BE4
  and BE5 call each other. A clone re-aims every call at a recorder, so
  no edge orders the merges; they say whose `symbols.toml` names another
  group's rebinding will want.
- **Limits**: the reach sweep is linear, so a jump table in `.text` read as
  code could in principle yield a false reference (none seen); a
  reference through a computed address (`add eax, imm`) is invisible, as it
  is to `pe_xref.py`. `.data` cells are counted on 4-byte alignment. The
  `uncovered` flag says where a descent stopped short, not why.

## 5. What it does not do

- It changes nothing: no function taken, no name, no C++; no `symbols.toml`
  entry for the nineteen unlisted functions (a group's, if it takes them).
- It reads no route: the live reach is the tracer's
  (`BOF3X_CALLTRACE_REACH`, the plan's section 7).
- It does not recut: the cut table is the authority and is only read.

## 6. Round thirteen's use: any cut table, `--harness area`, `--byte-tables` (2026-09-29)

Added on `phase-3/round13-prep` from `61be26e` for the round-thirteen draft
([`takeover-queue-round13.md`](takeover-queue-round13.md)). Nothing else in
the tool changed.

- **Any cut table.** `--cut` already took any TSV with the seven columns;
  round thirteen's draft (`round13_cut_draft.tsv`, the plan's section 3)
  adds columns of its own after them, which the tool ignores. `tools/area_rows.py`
  cannot serve that cut: its band is `0x401000..0x430000` and its roots the
  area descriptors, and 627 of the draft's 629 rows lie outside the band
  (the plan's section 4). This tool reads any row wherever it lies.
- **`--harness area`** prints the clone tables in `area_harness`'s form
  (`area_harness::CallSite` / `Imm` / `JumpTable` / `Clone`, `AH_N`), the
  same field order as the other two (checked against
  `src/game/area_harness.h`'s `Clone` and `area_w4f_fuzz.cpp`'s rows). The
  default is unchanged: boss for `BE*`, scenario for the rest.
- **`--byte-tables`.** Behind MSVC's two-level switch (`cmp r, n; ja;
  mov cl, [r + T2]; jmp [ecx*4 + T]`) `magic_rows._jump_cap` has no cap,
  so the table read stopped at the first case past the span, and a case
  lying past the next start was not seen as one. With the flag the dword
  table's length is the largest byte of `T2[0..n]` plus one, so every case
  is read and a start that is only a case is flagged `inside host` and
  absorbed. **Off by default.**

**The regression** (scratch `regress.py`, 2026-09-29, `symbols.toml` of
`61be26e`, the main checkout's `analysis/round12_cut.tsv`): `--groups
--tsv` (all 641 rows), `--edges`, `--group BE5 --clones`, `--group FE2
--clones`, `--group FO`, `--function 0x446DE0,0x452460 --harness boss`, and
the round-thirteen draft's `--groups --tsv`, run before the change and
after it. **Without `--byte-tables` every output is identical, line for
line.** With it, round twelve's cut changes in two rows and one verdict:

| Row | Without | With |
|---|---|---|
| `0x56D240` (FE2) | inside host (reached by host `0x56D1A0`, a case in its host's table) | the same, the reason adding "a case of `0x56D1A0`" |
| `0x578A40` (FO) | `uncovered`, reached only by the `.text` cell `0x578AD8` (section 3b: "a `.text` cell the descents did not place in a table") | **inside host: a case of `0x578A00`**, `MoveScript_Group9` (ours), entry 0 of its four-entry table at `0x578AD8` behind the index table `0x578AE8` (symbols.toml's own evidence for `MoveScript_Group9` names both) |

and the round-thirteen draft gains its two non-functions, `0x4201F0` (case
9 of ours `Area141_Tail52` `0x420060`: `cmp eax, 0x33; ja; mov cl, [eax +
0x420328]; jmp [ecx*4 + 0x4202EC]`, the cell `0x420310`) and `0x422530`
(case 0 of ours `Area148_Tail31` `0x422510`: `cmp eax, 0x15; ja; mov cl,
[eax + 0x422778]; jmp [ecx*4 + 0x422750]`), both read by hand with capstone.
`tools/area_rows.py`'s own walk had already dropped both as "inside
another" (its run at `61be26e`, below). So `0x578A40` is a ninth
round-twelve cut start that is not a function; FO owns it by the cut.

The area tool at `61be26e`, for the record (`python tools/area_rows.py
--exe ... --analysis <a scratch copy of the inputs, with the catalog
regenerated at 61be26e> --quiet`; it writes its two TSVs into `--analysis`,
so it was pointed at a copy): band `0x401000..0x430000`, 1,457 starts, 1,414
ours; 1,566 functions after discovery, 1,554 ours; worlds 0..4 "to take" 3,
0, 0, 0, 0; **catalogue "Area overlays" outside the band: 627; reached by an
area: 2**.

## 7. `--pointer-scan`: the starts only a pointer names, and the cases a table read short (2026-10-01)

Round twelve's debt 5 ([`takeover-queue-round12.md`](takeover-queue-round12.md)
section 7): wave two found two shapes the tool could not see, and the fold
is this flag. **Written in a cloud session without the exe; compiled and
linted, not yet run.** The first run over the fourteen bands is the owner's,
with the regression below; until then this section describes the code, not
a measurement. **Off by default**, and with it off nothing in the output
changes (the row gains an internal field the TSV does not write).

- **Starts only a `.data` pointer names.** FC2's `0x46C730`, `0x46CEF0`,
  `0x46D400` (`Effect_KindHandlers` `0x34`, `0x3A`, `0x41`) and `0x46C820`
  (`EffectKind34_V0States[2]`), FC3's `FieldCore_State2Steps` entries 4..8:
  each a dispatcher of 0x12..0x23 bytes in the padding after a function,
  reached by a table's cell and by nothing in `.text`, so no descent reached
  it and the 16-byte rule (section 2, "code no list has") did not either -
  `--group FC2` printed "0 not listed" ([`field_c2.md`](field_c2.md)
  section 5, [`field_c3.md`](field_c3.md) section 5). `Band.pointer_scan`
  reads every 4-aligned dword of every section but `.text`; a value inside a
  group's band (its first cut entry to the span end of its last) that no
  list knows - `pc_funcs`, `pc_hidden`, `symbols.toml`, the cut, the
  absorbed starts, the code the 16-byte rule found - is a hit. A hit that
  decodes and lies in no member's descent or table **becomes a member**
  (`Band.extra_of`, so it gets a row, a clone, the flags and the edges),
  flagged `not in the cut: code no list has, found by the pointer scan
  (<the cells, with their table's name>)`. A hit inside a member's code is
  printed as an entry into it, not a function; one that does not decode as
  data. The report at the end of `--groups` or `--group` lists them all.
- **A cut start that is a case of a table read short.** `0x578A40` (FO) was
  reached only by the `.text` cell `0x578AD8`, which no reader had placed in
  a table: `MoveScript_Group9`'s two-level switch had no cap without
  `--byte-tables` (section 6). `--byte-tables` settled that one; the general
  shape is any jump table whose read stopped before the cell - a `cmp` bound
  smaller than the table, a switch the descent did not reach. `Band.table_owner`
  takes every table any member's or bound's descent read and follows its run
  of `.text` pointers past the read's end (up to `TABLE_RUN_MAX` cells, 4-aligned);
  a cut start whose only references are `.text` cells inside such a run is
  flagged `inside host, no address reference (a case of <owner>'s table <base>
  (cell <c>, past the read; <who>))` and absorbed by `settle` like the other
  cases. The report counts them.

```
python tools/band_rows.py --exe .../BOF3.exe --analysis .../analysis --groups --pointer-scan
python tools/band_rows.py ... --group FC2 --pointer-scan        (expect 0x46C730, 0x46C820, 0x46CEF0, 0x46D400 as rows)
python tools/band_rows.py ... --group FC3 --pointer-scan        (expect 0x525CA0, 0x5261E0, 0x526490, 0x526A90, 0x526B80)
python tools/band_rows.py ... --group FO --pointer-scan         (expect 0x578A40 a case of 0x578A00's table 0x578AD8, without --byte-tables)
```

**The regression to run first**, section 6's: `--groups --tsv`, `--edges`,
`--group BE5 --clones`, `--group FE2 --clones`, `--group FO`, `--function
0x446DE0,0x452460 --harness boss`, and round thirteen's cut's `--groups
--tsv`, at this commit and the one before it, **without the flag: every
output must be identical, line for line.** Then the three expectations
above with it. What the scan prints beyond them - any other start in the
fourteen bands that only a pointer names - is the debt's answer and goes
here, with the group whose band holds it; the cut is not rewritten (section
5), the group that owns the band decides whether to take it.

What it does not do: a pointer computed at run time (`add eax, imm`, a
table base in a register) is invisible, as in section 4; a cell in `.text`
that is not in any table's run (a stack table's immediate is an
instruction operand, filtered in `reach`) names nothing here; a hit before
a band's first cut row or between two bands is not reported.
