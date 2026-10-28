# The relocatable-image round: staging (IDEAS I44, stage 3)

**Status:** IN PROGRESS (launched 2026-10-28 afternoon on `relocatable-image/round`
off `main` `15518394`, the PR #164 merge; stage A's three agents in worktrees
under `C:\Users\kerok\bof3x-runs\ri1027`, baseline build `wt_base`. The
owner's three calls are answered, section 7.)

The owner's word, 2026-10-27: stage "the data image as typed source" work as
a round. Stages 1 and 2 of [`IDEAS.md`](IDEAS.md) I44 are done (the
typed-data round, PRs #94 and #100: every data site named or listed with a
reason, the generated `GameData` struct). What is left is **stage 3, the
relocatable image**: the player's `.data` and `.rdata` placed at any address,
data-to-data pointers rebased at load, code pointers bound straight to our
functions - and with that `/FIXED`, the base at `0x400000` and the
entry-stub page retired. After it, the only pin to x86-32 is pointer width
inside the records (I44 "What follows"; not this round).

## 0. What pins the image today, measured 2026-10-27

On `main` `e84aaf6d`:

| What | Count | Source |
|---|---|---|
| `GameData` | `0x5C4000`, `0x37B000` bytes, 2,965 members | `build/gen/bof3/symbols.gen.h` (`kGameDataVa`, `kGameDataSize`) |
| How a typed global is reached | `#define Party_Zenny (*reinterpret_cast<unsigned long *>(0x904058))` - an absolute address in every generated accessor | same file, `tools/gen_symbols.py` |
| Raw data sites | 27,435: 17,259 by a constant address (`global`: 7,505 `at::` + 9,754 module constants), 9,047 through a pointer plus offset (`record`), 1,129 `raw`; 8,917 of the 27,435 in fuzz files | `tools/data_census.py --counts` |
| Pointer words in `.data` | 2,687 runs: 9,951 words into `.data`, 8,784 into `.text`, 63 into `.rdata`, 33 into `.bss` | `recipes/exe-pointers.tsv` |
| Code pointers bound by the entry-stub page | 9,142 words, 6,346 targets, 6,211 of them function starts | [`exe-import.md`](exe-import.md) 5, [`state3-design.md`](state3-design.md) 2 |
| `at::` constants that are code addresses | 388 (331 distinct) | `data_census.py --counts` (`at_constants_code`, `distinct_code_addresses`) |
| Inline-asm memory operands (`"m"`) in `src/game` | 171, in a set of 126 files that hold asm | `grep` |
| Run-time raw calls through an address | 61 as I44 cites [`round-13-cleanup.md`](round-13-cleanup.md) 1.3; **1,340 by RA1's census** (`abs_call`, game and platform: 1,284 through the call-through helpers, 28 function-pointer casts, 18 `orig::F()`, 10 to functions still Capcom's) | RA1 as run, below |
| The executable | `bof3x.exe` based at `0x400000`, spanning `0x400000..0x93F000`, its own sections holding Capcom's range | [`state3-design.md`](state3-design.md) 1.1 |

Two consequences of that table:

- **Record sites do not care where the image is.** A pointer plus an offset
  follows the pointer. The 17,259 global sites, the generated accessors, the
  code-address constants and the asm operands are what name the base.
- **The 8,917 fuzz sites stay where they are.** The shadow fuzz clones
  Capcom's code and runs in the hosted build only, where `BOF3.exe` is at
  its fixed base by construction. `BOF3.exe` stays at `0x400000` for ever;
  only the player's executable moves. **Amended by RA1 and RA3 (2026-10-28):**
  the fuzz is compiled into the same `bof3x.dll` the exe host loads, so there
  is no hosted-only compile in which the base could stay `constexpr`; the base
  is one run-time value, `ImageBase()`, that reads `0x400000` hosted
  ([`relocatable-image-design.md`](relocatable-image-design.md) 1).

## 1. The method: change the spelling first, the address second

The typed-data round's gate was `tools/codegen_same.py`: the machine code of
every object identical to the baseline's, so "no behaviour changed" was a
proof, not a test. This round keeps that gate for as long as it can:

1. **Stage B rewrites every base-naming site as `base + offset`, with the
   base a `constexpr` `0x400000`.** The compiler folds it; the machine code
   is byte-identical; `codegen_same.py` says `0 differ`. Every one of the
   17,259 sites can be converted this way, in parallel by file group,
   exactly as stage B of the typed-data round was.
2. **Stage C makes the base a run-time value** in both hosts (one DLL serves
   both, RA3 section 1; it reads `0x400000` hosted), and moves the image. Here the machine code changes everywhere at once, by one
   decision in one header, and the proof becomes a run: the state hash of
   the image at `0x400000` against the image at another base, equal on every
   route once pointer words are compared as offsets.

The risk is concentrated in stage C, and stage C is small in lines.

## 2. Stage A: census, hash, design (three groups, in parallel, no `src/game` change)

### RA1: the census of what names the base
`tools/reloc_census.py`, written to run in CI: every site in `src/` that
depends on the image's address, by class - generated accessor, `at::`
global, module constant, code-address constant, asm memory operand,
absolute call, **a comparison against an address range** (our code, or
Capcom's logic reproduced in ours, testing "is this pointer inside `.bss`"
or "below `0x900000`" - such a test silently changes meaning when the image
moves, so each one is listed and read), and pointer arithmetic that
subtracts an image address. Fuzz and hosted-only files counted apart, with
their reason. Output a TSV under `analysis/`, its counts in this doc. It
sizes stage B's groups.

### RA2: the state hash made address-independent
I44 and [`state3-design.md`](state3-design.md) 6.2 name it as the
precondition. The hash already normalises our pointers in Capcom's slots
([`state-hash.md`](state-hash.md) 7, `BOF3SH3`). Extend it: a word in a
pointer slot (`recipes/exe-pointers.tsv`'s rows, and the run-time slots the
`BOF3SH3` census records) that lies in the image is hashed as its offset
from the image's base. Proof without moving anything: today's references
(`cn_<route>_orig3` and `_orig3b`) re-hashed under the new normalisation
still compare identical, pair for pair, and a synthetic page with every
slot moved by a constant hashes the same. One game run per reference, by
the coordinator.

**RA2 as run (2026-10-28, `relocatable-image/ra2` `174c1f97`; [`state-hash.md`](state-hash.md) 8).**
The rule is wider than the paragraph above: **every aligned word** of a page
whose value lies in a folded range `[F, F+0x53F000)` is hashed as
`0x400000 + (value - F)`, not only the pointer slots, because 895..3,144
run-time pointers sit outside the start-up slots (a slot-only rule would have
failed stage C's proof). Both sides of a comparison fold the same range (the
image's own base when moved, and `BOF3X_STATEHASH_FOLD=<addr>` for the side
at `0x400000`), because no 64 KiB-aligned window of `0x53F000` bytes below
`0x80000000` is free of game data in 219 dumps; the tool refuses pairs whose
folds differ. With nothing folded the hash is `BOF3SH3`'s bit for bit, so the
`cn_<route>_orig4` / `_orig4b` references (not `orig3`, stale above) stand
unchanged; the header is `BOF3SH4`; `StateHash_SetImageBase()` waits for
stage C's loader. The proof is one Capcom-side run per reference with the
new build checked against its pair (`ri1027\ra2\rehash.sh`), run by the
coordinator in a quiet window. **For stage C2:** a code word bound to our
function hashes as the bare tag where Capcom's references hold his address
(123 pages), so the binder must give the hash the our-function to Capcom-
address map. Report: `C:\Users\kerok\bof3x-runs\ri1027\ra2\REPORT.md`.

**The proof as run (2026-10-28 afternoon, `ri1027\ra2\sh`, checks `check_<route>_orig4r*.txt`).**
Every route shows pages `0x929000` / `0x92A000` / `0x905000` (`Save_Staging` and
its neighbours) from the first save-list read: the owner's save slot 6 was
written at 00:11 that night, after the `orig4` pairs of 2026-10-23, and the
morning's quiet Capcom-side runs of the ledger stage D proof (`cn_<route>_dOrig`,
a build without RA2) show exactly those three pages against the same pairs on
every route - so they are the saves, not the hash. The afternoon's runs were
first lost to the owner at the machine (the game pauses on focus loss: two
replays exited mid-recipe, a desktop-width page changed at tick 250), then run
beside six building agents, which adds transient VRAM-shadow, unpack-scratch
and sound pages and, on `cutsceneAndNue`, two field pages from a tick that
differs between the two sides (pace noise). Nothing systematic beyond the
saves appeared. **The authoritative proof is `ri1027\ra2\proof_unattended.sh`
re-run with no agent building and nobody at the machine**; its three controls
(caughFish on `main`'s build and on 2026-10-27's, cutsceneAndNue on
2026-10-27's) separate the saves from the build. **Run 16:47..16:55:** caughFish
on `main`'s build and on 2026-10-27's (before ledger stage D) both show the
three save-list pages from the same ticks (190 / 243) as RA2's build does, so
the pages are the owner's saves alone - not RA2's hash and not stage D's
launcher; everything else in those controls is render and sound noise under
the compile load. cutsceneAndNue on 2026-10-27's build under the same load
shows the same two field pages (`0x903000` from tick 874, `0x92B000` from
2523) RA2's runs showed, so those are the load's too, not RA2's. The chain
ended 16:57; the quiet re-run remains the proof. The `orig4` pairs want
re-recording with the current saves before stage C's `orig5f` pairs, or the
save-list pages a skip entry (the coordinator's call).

### RA3: the loader's design
**As run (2026-10-28, `relocatable-image/ra3` `23d2baac`):**
[`relocatable-image-design.md`](relocatable-image-design.md), DRAFT, every
bullet below answered or its missing measurement named (its section 13).
The decisions: the image stays in `bof3x.exe`'s own sections at the PE
header's `ImageBase`, read at run time in both hosts; the image below 16 MB;
`at::` constants stay addresses behind `Img(va)` / `Code(va)` (RB0 follows);
the 18,831 pointer words rebased after the data copy; 8,673 code words bound
by name, 111 unbound into the trap, the stub page `PAGE_NOACCESS`; 14 import
slots; quicksaves across bases refused; the proof base `0xA00000`. Report:
`C:\Users\kerok\bof3x-runs\ri1027\ra3\REPORT.md`.

A design doc in the shape of `state3-design.md`, answering with measurements:
- where the image goes (a `VirtualAlloc` block, or a section of ours without
  a fixed base), and what `GetModuleHandle(nullptr)` still has to be;
- the rebase: every `.data` pointer word of `exe-pointers.tsv` by class,
  what a disc-built image's unfilled words mean ([`exe-import-engine.md`](exe-import-engine.md)),
  and the run-time pointers the game stores into its own records (heap,
  arena) - those already hold real addresses and need nothing, but list
  any that are built from a constant;
- the code words: bind each of the 8,784 `.text` words to our function by
  name (`symbols.toml`), the mid-function ones that are numbers
  ([`exe-code-pointers-midfunc.md`](exe-code-pointers-midfunc.md)) left as
  numbers, and what replaces the stub page's `int3` wall as the loud trap
  for a word nobody bound;
- the 13 import slots, `.rdata`'s 63 data-pointer words and its 127 code
  words that no code calls;
- whether the entry-stub page survives as a fallback switch for one release
  or goes;
- what changes for the quicksave (DIV-0145..0147 snapshot native memory -
  I46: a snapshot taken at one base and loaded at another must rebase or
  refuse), the crash reporter's addresses, and `tools/` that name Capcom's
  addresses in the exe host.

### RA1 as run (2026-10-28)

`tools/reloc_census.py` on the round tip `15518394`, its output
`analysis/reloc_census.tsv` (the method, every class and the scope rules are
in its docstring; it reuses `data_census.py`'s reader). A site is one token
of our C++ naming an address in `0x400000..0x93F000` (or our draw pool); 40
seconds, deterministic (two runs byte-identical).

| Class | game | platform | hosted | fuzz | launcher | all |
|---|--:|--:|--:|--:|--:|--:|
| `accessor` (a generated typed name) | 22,356 | 169 | 0 | 3,425 | 2 | 25,952 |
| `accessor_addr` (`Name_addr`, `bof3::addr::Name`) | 5,461 | 16 | 0 | 7,281 | 0 | 12,758 |
| `inject` (`BOF3_INJECT`, `Inject(...)`) | 10,071 | 0 | 0 | 18 | 0 | 10,089 |
| `image_meta` (`bof3::image::kBase`, `kTextVa`) | 1 | 6 | 0 | 0 | 0 | 7 |
| `at_global` | 5,182 | 1 | 0 | 1,614 | 0 | 6,797 |
| `module_const` | 2,159 | 53 | 0 | 1,103 | 0 | 3,315 |
| `code_const` (a code address by name) | 2,386 | 19 | 82 | 5,395 | 0 | 7,882 |
| `raw_literal` | 1,837 | 125 | 0 | 2,143 | 0 | 4,105 |
| `code_literal` | 862 | 66 | 10 | 44,183 | 0 | 45,121 |
| `asm_operand` | 14 | 0 | 0 | 0 | 0 | 14 |
| `asm_text` (a literal in a naked function's template) | 29 | 0 | 0 | 1 | 0 | 30 |
| `abs_call` (a code address called) | 1,330 | 10 | 26 | 14 | 0 | 1,380 |
| `range_cmp` | 138 | 19 | 0 | 114 | 0 | 271 |
| `addr_eq` (`==`, `!=`, `case`) | 67 | 17 | 172 | 1,865 | 0 | 2,121 |
| `ptr_sub` | 47 | 20 | 0 | 74 | 0 | 141 |
| `const_def` (3,281 spell a literal, 1,018 derived) | 3,934 | 23 | 0 | 342 | 0 | 4,299 |
| **all** | 55,874 | 544 | 290 | 67,572 | 2 | 124,282 |

**Scope rules.** `fuzz`: `data_census.py`'s file rule (`_fuzz.`,
`_harness`, `fuzz_`). `hosted`: a site in another file that only the hosted
build executes - inside `CloneOriginal(...)`, inside a `StubFor` / `StubAt`
body (a clone's callee table), or a code address inside `PatchBytes(...)` /
`RetargetCall(...)`. `launcher`: another process, reads `BOF3.exe` as a file.
`platform`: `src/hook`, `render`, `audio`, `input`, `common`.

**What a stage B group edits** is every game and platform site but the
generated names (`accessor`, `accessor_addr`, `inject`, `image_meta`: RB0's,
through `gen_symbols.py`) and the readings (`range_cmp`, `addr_eq`: the
coordinator's): **18,097 sites in 511 files**. Of the literal sites, 2,992
are constant definitions that spell an image literal - with the
`base + offset` spelling these are the edit points, and the 7,341 game
references to `at::` and module constants follow them without an edit while
the base is `constexpr`.

**Proposed stage B groups** (`--groups 6`, families whole; the file list per
group is written by `--groups 6 --groups-tsv FILE`):

| Group | Families | Files | Sites |
|---|---|--:|--:|
| 1 | magic | 50 | 3,205 |
| 2 | battle | 78 | 3,005 |
| 3 | rest | 51 | 2,769 |
| 4 | effect, engine, platform | 131 | 2,748 |
| 5 | field, scenario | 98 | 3,181 |
| 6 | area, ui | 103 | 3,189 |

**The readings** (`--ranges`): 157 range comparisons and 84 equality tests
in the game and platform scopes, plus the calls of the range helpers
the census finds (seven in the game, of `InData`, `InImage` and `InPool`;
eight in the fuzz). Read by kind in the
RA1 report: table-walk loop bounds (both ends image constants - they move
together), membership tests on a run-time pointer, eight "is this handler
word inside Capcom's `.text`" checks that a code word bound to our function
will fail, world coordinates that only look like addresses (never to be
converted), and the platform's own layout checks.

**`--check`** fails when a gated count rises in the game or platform scope
(the literal, call and reading classes: `asm_operand` 14, `asm_text` 29,
`abs_call` 1,340, `range_cmp` 157, `addr_eq` 84, `ptr_sub` 67, literal
`const_def` 2,992, `raw_literal` 1,962, `code_literal` 928). The counts are
stable run to run, so the check can join `checks.yml` beside
`data_census.py --check`; stage B lowers every gate, and each group commits
the regenerated file.

## 3. Stage B: the spelling (file groups, in parallel, after RA1 and RA3 merge)

First **RB0** (one agent, alone): `tools/gen_symbols.py` emits every
accessor and `GameData`'s address through one base, `bof3::kImage` (or the
name RA3 chose), a `constexpr` `0x400000` in this stage; `at::` constants
become offsets or stay addresses with one helper that adds the base -
whichever RA3 chose. `codegen_same.py`: `0 differ`.

Then the file groups convert the global sites RB0 does not reach - module
constants, hand-written `at::` uses, asm operands (an `"m"` operand on a
global becomes a register operand loaded from `base + offset`; with a
`constexpr` base the assembler may still emit the absolute form, so check
`codegen_same.py` per file), code-address constants (to the symbol's name
where ours is meant, or a comparison against the bound value). The groups
follow the typed-data round's file families, sized from RA1's census (about
17,000 sites; the typed-data round's stages A and B together took about
23,500).
Every group's gate: build, `codegen_same.py` `0 differ`, the headless
self-tests. A range comparison RA1 listed is not converted by a group: it
goes to the coordinator with its reading.

## 4. Stage C: the move (one agent, then the coordinator's runs)

- The base a run-time value in both hosts, `ImageBase()`, set by the loader
  of RA3's design (`0x400000` hosted); the image below 16 MB because the
  ordering-table links are 24-bit (RA3 section 1, `draw_pool.cpp:250`); stage
  C in two steps, C1 the move with the stub page intact, C2 the code words
  bound and the page a no-access trap (the owner's call 2).
- `bof3x.exe` linked without `/FIXED` at `0x400000`; the image placed by
  the loader; pointer words rebased; code words bound; the stub page as
  RA3 decided.
- **The proof**, by the coordinator, game runs serialised: the ten routes
  and the attract with the image at `0x400000` (the loader's default path,
  against the current references, unchanged) and at a forced other base
  (`BOF3X_IMAGEBASE=<addr>`, a developer switch); the address-independent
  hash equal between the two on every route; the self-tests in both; a
  quicksave taken at one base and loaded at the other doing what RA3
  decided. No ledger entry if the hashes agree: nothing a player sees
  changes.

## 5. Stage D: audit and docs

An independent read of stage C's diff and of every range comparison; I44's
outcome line, `state3-design.md`'s amendments, `STATUS.md`, `HANDOFF.md`;
`data_census.py`'s strict rule extended (or a second rule) so a new
absolute address in `src/game` is refused.

## 6. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/reloc_census.py`, `analysis/reloc_census.tsv` | RA1 |
| `tools/statehash.py`, `src/hook/statehash.*` | RA2 |
| `docs/relocatable-image-design.md` | RA3 |
| `tools/gen_symbols.py`, generated headers' shape | RB0 |
| each source file | its stage B group, one owner |
| `src/hook/exe_host.cpp`, `exe_image.*`, `detour.cpp`, `CMakeLists.txt`'s `bof3x.exe` link | stage C |
| `STATUS.md`, `HANDOFF.md`, reference runs | the coordinator |

Merge order: RA1, RA2, RA3 (any order, no shared files); RB0; the file
groups one at a time, `codegen_same.py` after each; stage C; stage D.
Every commit `git commit -s`. Agents run no game: the coordinator runs the
references (RA2) and the proof (stage C), one at a time, in this round's
own harness root under `C:\Users\kerok\bof3x-runs\ri1027` (Defender-excluded).

## 7. For the owner

1. **The route.** This stage is I44's route: the image movable, the
   source free of Capcom's addresses - the "reconstructed source" goal.
   There is a cheaper road to a 64-bit build that does not need it: keep
   the image at `0x400000` in every build (a 64-bit process can usually
   reserve that address), keep its pointers 32-bit, bind code words
   through a table, and give the game's records a low-memory heap. That
   road leaves every fixed address in the source and depends on each
   system letting a program map low memory (not verified on any system; on
   macOS the default zero page covers the low 4 GB). The proposal is I44's
   route; stage A is useful to both, so it can start before this is
   settled. **Answered 2026-10-28 (the owner): I44's route.** Stages B, C
   and D run as staged.
2. **The stub page**: kept for one release behind a switch, or removed in
   stage C. RA3 will measure; the call is the owner's. **Answered 2026-10-28 (the owner): removed at the end of stage C**
   (RA3's option R, [`relocatable-image-design.md`](relocatable-image-design.md)):
   stage C runs as C1 (the image moved with the stub page intact, proving
   the move alone) then C2 (the code words bound by name, the page a
   no-access trap, proving the binding alone); no `BOF3X_STUBPAGE` switch,
   no mode in the quicksave's build id.
3. **Stage B's size**: one wave (about 17,000 sites, near the typed-data
   round's stages A and B together), or two. **Answered 2026-10-28 (the owner): two waves, split by kind**
   (RA3's reading): wave 1 the data places (RB0, then the file groups,
   `codegen_same.py` the gate); wave 2 the code places (the helpers, the
   code-address definitions, the thunk), each a place-or-name judgment the
   gate cannot see because `Code()` folds to the identity in stage B, proved
   by C2's run. RB0 prototypes the typed `Va` on one family before wave 1 is
   sized from RA1's census.

## 8. The brief, per group

`C:\Users\kerok\bof3x-runs\ri1027\` holds `brief_common.md` and the stage A
briefs; stage B's are written after RA1's census sizes the groups. Each
names: the goal; the worktree's reset target (the round branch's tip, not
`main`); the files it owns; what to read first (this doc, I44,
`state3-design.md`, `typed-data-round.md` sections 1 and 5, the module docs
for its files); the checks that mean done; the report's shape. A usage cut
resumes with a message naming the next step; only the report means merge.

## 9. Not in this round

- Widening the records' pointers: the 64-bit build (PLAN phase 4, I33's
  platform library).
- I5's translation-unit layout (folded into I44 after stage 3 by the owner,
  2026-10-21).
- The hosted build's base: it stays `0x400000`, by construction.
