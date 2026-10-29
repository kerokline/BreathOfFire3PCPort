# The relocatable-image round: staging (IDEAS I44, stage 3)

**Status:** BUILT, stage D done 2026-10-29 (section 5's "as run"; the PR at the owner's word). Launched 2026-10-28 afternoon on `relocatable-image/round`
off `main` `15518394`, the PR #164 merge; stage A's three agents in worktrees
under `C:\Users\kerok\bof3x-runs\ri1027`, baseline build `wt_base`. The
owner's three calls are answered, section 7. Stage C merged and proved 2026-10-29
morning, section 4; stage D the same day, section 5.)

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
failed stage C's proof); and a word whose low 24 bits lie in a folded range
has those bits folded and its top byte kept (an ordering-table link, a
length byte over a 24-bit address: added in stage C1 after the first moved
proof run). Both sides of a comparison fold the same range (the
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

### Stage B wave 1 as run (2026-10-28 afternoon, RB0 and RB1..RB6)

**RB0** (`relocatable-image/rb0`, report `ri1027\rb0\REPORT.md`): `tools/gen_symbols.py`
emits every data accessor through `::bof3::Img(va)` and every callable
original through `::bof3::Code(va)`; `src/common/image_base.h` holds
`image::kBase` (one name: `kLinkedBase` was not added), `ImageBase()`,
`Img`, `Code` - **`consteval`, not `constexpr`**: `constexpr` helpers changed
18 objects' code (folding after inlining), `consteval` none. `image::Va`
prototyped on the message box (3 decisions per 195 uses).

**RB1..RB6** (six Opus agents, one family group each from RA1's split, worktrees
`ri1027\wt_rb<n>`, reports `ri1027\rb<n>\REPORT.md`; merged one at a time, gate after
each, round tip `f357193e` before this fold-in). Every data place in `src/game`,
`src/input` and `src/render` goes through `Img`: about 2,300 data constants
became `::bof3::image::Va` (battle 600, rest 478, effect/engine/platform 464,
area and ui 558, magic 173; field and scenario kept integer names and placed
each site), about 3,900 `<Name>_addr` places and about 1,000 identified raw
literals are wrapped, the 29 asm template addresses are `"i"(::bof3::Img(...))`
operands (`task_sched.cpp`, `field_e2.cpp`, `field_modes.cpp`; **stage C must
rewrite them again, an `"i"` operand wants a constant**). Census gates after
the wave: raw_literal 1,962 -> 727, asm_text 29 -> 0, range_cmp 157 -> 140
(table walks whose both ends are now placed), addr_eq 84 -> 86 and ptr_sub
67 -> 69 (RA2's statehash code), code gates unchanged (wave 2). `data_census`
27,435 -> 27,434 sites, `--strict` clean, `typed_left --check` current.

**What held, and what did not** (every group measured it):
- **RB0's accessor `Va` overloads do not hold the gate.** `Img(image::Va)` is
  `constexpr` and folds only after inlining: the code changed in five battle
  functions, nine rest functions, `_PartyForm_Swap`, `Identify_DrawEnemy`,
  `Party_SetUpMembers`, eleven effect objects (loop-base hoisting, strength
  reduction, scheduling, `.rdata` string order); a non-inline helper given a
  `Va` overload is emitted under the overload's mangled name. **The spelling
  that holds everywhere is the consteval `::bof3::Img(kX.v)`** (a computed
  address `Img(kX.v) + off`); no place overloads remain in the tree.
- `Img()` in a **fuzz** static region table reorders `.rdata`
  (`battle_odds_fuzz.cpp`); those tables name the VA as `kX.v`. Non-fuzz
  self-test tables with `Img()` literals compared identical and were kept
  (RB4 finding 2, RB5 point 3, RB6's title_states tables).
- `data_census.py` keys a site by its spelling: RB2's `see_through_base()`
  reads `::bof3::Img(X)` as `X` and `kX.v` as `kX` for `data_census` and
  `typed_left` only (one version merged; the other groups' copies reverted).
  Multi-declarator `constexpr Va kA{..}, kB{..};` lines are not read: split
  one per line.
- `reloc_census.py` missed eleven accessor sites the groups found by grep
  (RB1 five, RB6 six); a group working from census rows alone misses them.
- A census symbol plus a `call` context does not prove a place: `0x800000`
  (`Gfx_PacketPools+0x1e400`, `aligned_8000`) is a height (RB3 finding 5).
- Pointer values stored into game memory (frame-set tables, pool tables) are
  places (RB1, RB3); `rdata::Const` constants are our own copies, never
  placed; helpers that read a table and print it in a Fatal now receive the
  place, so in stage C those messages print the place (RB3, RB4).

**The coordinator's readings:** RB3's four forced comparisons (rest_2f
`TacticsMembers_Build`, rest_3g panels, rest_4a price tiers, rest_4d text
records) and RB6's table walks compare a placed cursor against a placed end:
correct, both move together. RB6's two mixed subtractions (`title_states.cpp`
`Byte()`, `window_task.cpp`'s Fatal index) now subtract the placed address
(`5b8b0da8`). RB2's `battle_e6.cpp:250` loop: both ends places, correct.

**Left for stage C** (each report's "sites left"): the range comparisons
whose loop starts at a `<Name>_addr` or whose constant is used only in the
comparison (`kPoolEnd`, `kClut`, `kChannelsEnd`, `kDataStart/End`, ...); the
membership tests (`InData`, `InPool`, `InImage`, `text_draw:693`); the clone
and patch read-backs; the slot-and-witness tables (`labels.cpp`,
`config_text`, `fishing_text`, `pause_text`); the eight handler-word-is-code
checks; `wide_staging`'s script VAs; `exe_icon.cpp:61`'s `kIconRects`; the
unidentified literals (50 battle, 2 field, menu_lists / menu_windows rows,
`0x663C8C`), each listed with file and line in its report.

### Stage B wave 2 as run (2026-10-28 evening, RC0 and RC1..RC4)

**RC0** (`relocatable-image/rc0`, report `ri1027\rc0\REPORT.md`): `::bof3::image::CodeVa`
beside `Va` (a separate type: a `Va` cannot go where a code VA is wanted);
`::bof3::Code(va)` stays the consteval place-maker and is now applied **once
inside** the four harness `*_AT` macros, so 787 call lines needed no edit and a
run-time value given to `*_AT` no longer compiles (the compiler sorts every
site); new `*_WORD` macros and `::bof3::CodePlace(word)` (`always_inline`
`constexpr`, the identity in stage B, code unchanged in every object) for the
words known only at run time - the `Phase` / `CallN` / `Alloc` / `EndStep`
helpers' bodies, 68 direct run-time lines, d3d_list's `H`; the 24 callee-table
casts through `Code()`; the `DirectSoundCreate` thunk a `CodeVa` with its stage
C binding written beside it. `tools/reloc_census.py` `BASE_HELPERS = {Img, Code,
CodePlace}`. **How stage C tells a VA from a word that is already a place:** by
value - a word in the linked code range `0x401000..0x5C4000` is a VA to bind,
anything else passes through - which holds only while no place of ours lies in
that range (the forced base must check it).

**RC1..RC4** (four Opus agents, reports `ri1027\rc<n>\REPORT.md`; merged one at a
time, gate after each): **860 code-address constants became `CodeVa`** (magic
45 + RC0's 5, battle and rest 292, field and scenario 232, effect / engine /
area / ui 291), each use decided by the compiler's diagnostic or by reading:
**a place** (a callee-table `Fn<>` / `Raw<>` cell, a task entry handed to
`Task_Create` / `Task_Restart`, a hook word stored where the game calls it -
`BattleHook_*`, the enemy record's `+0xF4`, 184 boss hook stores - the window
procedures, `Boot_Task`) is `::bof3::Code(kX.v)`; **a name** (a `*_AT`
argument, a `Phase` / `CallN` argument, a cell of our own table of VAs that
`Phase` places at run time, an `Inject` / `PatchBytes` / `RetargetCall` /
`OperandCall` argument, a clone row, a `Log` argument, every fuzz row and
`case` label) is `kX.v`; a word read from a table and called without a helper
goes through `CodePlace` (two effect step words, battle_fx_tasks' `H`, the
battle window and win-state dispatchers). `code_literal` fell 928 -> 923
(magic_s16's stack table by name); `abs_call` stays 1,340 by construction (the
census has no class for "placed at its helper"). The gate on the merged tree:
build green; `codegen_same.py` 647 objects, 1 differ (RA2's `statehash.cpp.obj`); `reloc_census --check`, `data_census --check` (27,434) and `--strict`, `typed_left --check` clean; the headless self-tests in both hosts exit 0 with `inject: 10087 ours`.

**What the groups learnt:** `Code(Code(x))` compiles and passes the gate, so a
`*_AT` or `RetargetCall` argument wrapped by mistake is caught only by reading
(RC4); a `CodeVa` through varargs compiles silently and only `-Wformat` on
`Log` catches it, a `Fatal` without the attribute would not (RC2); a two-name
`CodeVa a{..}, b{..};` line is not a census definition (RC3); `typed_left`
names a row by the first call after a `}` at column 0, so a `Code(` on the
first line after a namespace close renames a block (RC2); the effect_1b fuzz's
stringifying macro took `.v` inside.

**For stage C2's binder** (each report's last section): `Code()` runs at DLL
initialisation in every namespace-scope `kOriginals` table (RC0's 24 casts,
RC1's 7, RC2's 82, RC3's 59, RC4's about 85 cells), so the bind table is a
constant and **a miss in a table must return the trap page, not `Fatal`**
(cells with no bound function: the eight `Mp3_*`, d3d_list's 15 software
handlers, `kAreaExits` = `WorldMap_ExitRecords`, `BareRet`); the still-Capcom
callees (`Crt_GetPtd` hosted only; `Crt_findfirst` / `Crt_findnext` /
`Crt_sprintf` / `Crt_ftol` now `CodeVa`, `kFtol` called through `SH_AT`) want a
check of placed-or-named; the fuzz `Stub(F)` helpers turn a function pointer
back into a `StandIn` key and will miss once `Code()` is a lookup; code-byte
readers (`bof3::Operand` at magic_s02 / s20, `Widescreen_*`, `Imm16Bound`,
`CodeReads`, the title `StubFor` cases) read Capcom's `.text`, which is
`PAGE_NOACCESS` in mode B; the handler-word-is-code checks (eight, listed by
RA1) and the code-VA-against-word comparisons (`scena_sc6:2297` with its fuzz
write, `rest_2d:133/139`, `rest_2f:1477/1521`, `rest_3g:803-804`,
`menu_verbs:149`, `yes_no_layout:397`, `object_kinds:984-989`,
`mode_states:47/387` mid-function tails with nothing to bind) are stage C's
readings; about 330 clone call-table literals and the clone / relocation
arguments want `Code()` inside `CloneOriginal` / `Relocate` / `Patch32`, not
at the rows.

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

### Stage C as run (2026-10-28 evening to 2026-10-29 morning, one agent, the coordinator's proof)

**Branch `relocatable-image/c`**, one Opus agent for both steps, merged as
3aa7c7e3. C1 is `dfd29152` plus four fix-ups (`31dbe817`); C2 is
`e8929e66` plus four fix-ups (`44bea515`). Reports `ri1027\c1\REPORT.md`
(C1 and its fix-ups), `c1\PROOF.md`, `c2\REPORT.md`, `c2\PROOF.md`.

**C1, the move (mode S).** `ImageBase()` reads the main module's base from
the process environment block (`fs:0x30` + 8) in one non-volatile asm,
valid before `DllMain`, read once per function (`.text` +3.3 %; a lazily
set variable cost 21 %). `make_exe.py --image-base` writes a second
executable at another base (`build/bof3x-A00000.exe`, the `bof3x_exe_moved`
target, not shipped); the launcher's `BOF3X_IMAGEBASE` starts it. The legal
bases are `0x990000..0xA70000`: the image must lie wholly off the linked span
`0x400000..0x98F000` (so a word in the span is always a VA) and below 16 MB.
**The fence is the developer launcher's**: at `Engine_Main` only 64 KB of the
span is free (mapped files, heaps and stacks take the rest once the
executable is not there), so `bof3x-launcher-dev.exe` starts the game
suspended, reserves the whole span no-access, then resumes it; the engine's
own fence takes what is left. The rebase (`tools/gen_rebase.py` from
`recipes/exe-pointers.tsv`): 2,661 runs, data 9,936 words, bss 29, rdata 63,
text 8,784, each checked to lie in its class's linked range; **19 words the
recipe holds as pointers are left alone with a reason each** (`NOT_MOVED`):
the four bss numbers of design 3.4 and fifteen strings and numbers in
`.data` whose bytes read as addresses (`"%2d"` at `0x64D3EC` among them).
The 29 asm operands are displacements from `Bof3_ImageMove` in a register
the original leaves free. `orig::Name` is a constant object that places its
VA when converted or called. The crash reporter prints the VA beside a
faulting address in the image; `tools/proc_image.py` translates for the
tools. Design 13's measurements are in the C1 report section 4.

**C2, the binding (mode B).** `tools/gen_symbols.py --bind-out` writes the
bind table (10,088 rows, one per `impl`, sorted by VA; static data, no
initialiser). In `bof3x.exe` `Code(va)` is a lookup; a miss in a table
returns the trap and is logged (23 distinct VAs at start-up, the expected
set: d3d_list's 15 software-surface handlers and the 8 `Mp3_*`); a miss at
a call (`CodeToCall`, `CodePlaceToCall`, the harness `*_AT` / `*_WORD`
macros) is a `Fatal` naming the VA and its `[[func]]`. The rebase binds
8,673 code words to places and 111 to the trap at either base; `.stub` is
`PAGE_NOACCESS` after `CheckLayout`; `FillStub`, the `jmp` writing,
`WriteThunk`, `CheckStub` and `ProtectStub` are deleted; `Inject` writes
nothing in `bof3x.exe` and checks each (VA, ours) pair against the table.
**A place is `(top << 24) | VA`**, a `jmp ours` written at that address in a
reserved table (the top byte `0x7C` by measurement over 227 raw dumps, then
`0x70`, `0x7E`, `0x7B`, `0x7D`, then any top byte the process can reserve,
logged; a `Fatal` only when the address space is full). So a code word's
low three bytes are Capcom's wherever the game copies or reads it as data,
and the state hash maps a value back to its VA only inside that table. The
eight handler-word-is-code checks use `IsCodeWord` (hosted `.text`'s exact
extent; in `bof3x.exe` a bound function). The `DirectSoundCreate` thunk is
a thunk of ours calling through the slot as it reads at the call.

**The proof as run** (`c1\proof_c1.sh`, `c2\proof_c2.sh`; results per route
in the two PROOF files). Its shape, settled on cutsceneAndNue: **(1) ours at
`0x400000`, unfolded, against the `orig4` pair = the route's `b1027`
signature plus the three save-list pages** (the owner's saves changed since
`orig4`); **(2) ours moved against ours at `0x400000` with the same fold, by
`statehash.py diff` = the noise class only** (tick-3 upload frames, VRAM
shadow and sound windows, the D3D handle-cache page); Capcom's folded pair
is a cross-check, since a value fold cannot tell the music stream's heap
pointers from image pointers in `0xA00000..0xF3F000` of a `0x400000`
process. On the final tips all ten routes pass both at both bases; the
quicksave taken at one base and loaded at the other is refused as "another
span" in both directions, and one taken and loaded moved restores.
The attract sequence, six minutes at each base (`ri1027\smoke`): moved against the linked base with the same fold, identical on 9,793 ticks but the tick-3 upload frame; at the linked base against the 2026-10-04 references, the SunderLoop page, the window-record page and one six-tick VRAM and sound window. Its pace ranged 9,106 to 10,789 logic frames per 360 s across the four runs at both bases with the machine's load, no base effect visible; a clean measurement of the run-time base's cost is stage D's.

**What the proof found, a class each time** (the fence catching the rest):
C1 - a file-local identity accessor given a VA (`shell.cpp:237`, census
class `accessor`; sweep: 490 definitions, 4 sites); the `"%2d"` string
rebased as a pointer and the display blocks' 24-bit ordering-table links
unfolded (`Fold24`; a link folds on its low 24 bits); the first cell of a
two-cell `kOriginals` line bare (`field_event.cpp:43-47`; a per-cell sweep
of 8,679 casts and 358 task and hook arguments); the fuzz harnesses' shared
helpers (`magic_harness::PartyOf`, `EnemyOf`) building places from VAs for
live magic code (118 other harness hits fuzz-only). C2 - the hash's reverse
map by value renaming game data equal to an address of ours (a VRAM texel
`0x10770000` = `BattleAction_Kind3Wait`; six area tables holding the old
thunk's address): the place table; then `EffectKind0B_Rise` reading past
its 16-byte rise table into the low byte of `EffectKind0B_States[0]`, a
code word (Capcom's own over-read, its behaviour on both sides, no ledger
entry): the VA kept in the place's low three bytes. Proof-side: the runs
want `--host exe` and this branch's tools; a hosted Capcom reference crashed
in `BOF3.exe`'s own code on a `WM_LBUTTONDBLCLK` that reached the window
(a double-click at the machine; re-recorded).

**Left for stage D**: byte 3 / the high halfword of a code word read as
data (no reader known; 213 sub-word reads with unbounded indices listed by
`c2\subword.py`); `wide_rand.cpp:386-387`'s stores of two functions with no VA;
the docs that still describe the stub page (design 10, `state3-design.md`,
`state3-build.md`, `quicksave-census.md`'s `CheckStub` row).

## 5. Stage D: audit and docs

An independent read of stage C's diff and of every range comparison; I44's
outcome line, `state3-design.md`'s amendments, `STATUS.md`, `HANDOFF.md`;
`data_census.py`'s strict rule extended (or a second rule) so a new
absolute address in `src/game` is refused.

### Stage D as run (2026-10-29, one agent; report `ri1027\d\REPORT.md`)

**Branch `relocatable-image/d`** off the round tip `997b6d1d`. An independent
read of `git diff d1d4696b..997b6d1d` by six questions (one helper agent bounded
the sub-word reads), the docs, and a census rule. No game was run beyond the
headless self-tests.

**The audit's answers** (file and line in the report):
1. **Range comparisons** (`reloc_census.py --full`: 163 distinct lines in the
   game and platform scopes, plus the helpers): each tests a VA against VAs or
   a place against places. Most of the game's are world coordinates
   (`x == 0x4D8000`) or name compares (`rest_2d:133`, `menu_verbs:149`); the
   platform's place their bounds (`crash.cpp`, `snapshot.cpp`'s span,
   `input_script.cpp` `AsVa`, the fences' `0x400000..0x98F000`) or test VAs
   (`InCode`, `PatchBytes`' linked test, `exe_host.cpp`'s layout). The state
   hash's fold covers `0x53F000` bytes, the image to `.bss`'s end, not `.pool`
   (`0x93F000..0x987000`): a pointer into `.pool` is not folded at a moved base.
   That can only add a difference to a moved proof, never hide one; widening it
   changes every folded reference, so it is left for the coordinator.
2. **VAs and places outside the C sweeps' shapes**: no `Va` is built from a
   run-time word; every `Img()` of a variable reads a table of VAs of ours
   (`labels.cpp`, `wide_staging.cpp`, `name_tables.cpp`'s chunk tags); every
   `VaOf` is of a place, in a `Fatal`. **One place handed where a VA is meant:**
   `game_clock.cpp:95` gave `PatchBytes` `&Imp_GetTickCount`; it wrote the right
   bytes at a moved base only through `PatchBytes`' not-in-the-span branch and
   named the place as a VA - now `Imp_GetTickCount_addr` (`ffb98199`). Left, a
   diagnostic: `win_main.cpp`'s exit trace scans the stack for returns into
   `.stub`, where none lies in `bof3x.exe` since C2 (hosted unchanged).
3. **The sub-word reads**: all 388 rows of `c2\subword.py` (213 resolved, 175
   with a base the script could not follow), per row in `ri1027\d\q3\subword_bounds.tsv`:
   177 bounded by our code (masks, loop bounds, `Fatal` index checks,
   alignment: a 4-aligned base read at an offset 0 or 1 mod 4 never touches
   byte 3), 66 bounded only by the game's data, 36 further than `0x40` from
   a code word, 109 past the last code word of `.data`. **None shown to reach
   byte 3 or a high halfword.** Two not proven: `window_task.cpp`'s
   `kPlaceXY` (`0x66AE10`) u16 reads, reached by a placement of 12 or more -
   `Window_Kind0Open` strips the placement's two high bits before a kind-2 box
   can open, but a head that reaches a kind-2 box while window 0 is still
   allocated would keep them (no route found); `menu_draw_helpers.cpp:215`'s
   `ShopWin_CursorBoxWidths` u16 at index 3 would read the high halfword of
   `ShopWin_MemberStatsSteps[0]` (its `+0xA` writer for a kind-4 shop window not
   found). Both are Capcom's unchecked indices; reached, they would draw at a
   different wrong place than the original. The script listed only tables
   within `0x40` of a code word; the other 729 sub-word reads were not read.
4. **`wide_rand.cpp:386-387`** (and `draw_emit.cpp:225`, the same shape the C2
   sweep did not list): our functions' own addresses stored into
   `MapCell_Handlers` by the shadow self-tests, the table put back before any
   frame; hosted only (`exe_host.cpp` refuses `BOF3X_SHADOW`), where the hash
   renames nothing. No place needed: a documented exception, said at both
   sites (`ffb98199`).
5. **The place table's exposure**: byte 3 / the high halfword is the one
   difference (question 3). The fallback top byte changes nothing the game
   sees, only the hash's no-collision guarantee (logged). The ordering-table
   walkers (`d3d_list.cpp:207-212` `Gfx_DrawOTag`, `layering.cpp:203` and
   `:449`, `draw_order.cpp:244` and `:285`) read a link word, mask it to 24 bits
   and follow it: the links are built from packet and list-head addresses in
   `.bss` and `.pool`, never from a code word, and a link that reached one
   would land on `.stub`'s VA and fault, not read a top byte silently.
6. **The 111 trap words and the 23 start-up misses**: the 111 are the C
   runtime's tables (three at the head of `.data`, ten at `0x672930..0x672968`)
   and the blitters' jump table after `Gfx_PackRgb` (98 words,
   `0x6722BC..0x672450`), read by Capcom's code alone; nothing in `src/` names
   them. d3d_list's 15 software handlers are dispatched only when
   `Gfx_RenderFlags` bit 0 is set, which only Capcom's `Display_Setup` does
   (ours never, DIV-0031). The 8 `Mp3_*` are replaced by `mp3dec::Arm` before
   any music, refused under `BOF3X_MP3=original` - **and reachable under
   `BOF3X_MP3_COMPARE`**, whose comparison calls Capcom's decoder through
   `sound::kOriginals`: a fault on `.stub` in `bof3x.exe` (an `int3` before C2).
   Now refused in the exe host and taken to the hosted build by the launcher
   (`ffb98199`).

**The docs**: dated notes, the history kept, in `state3-design.md` (0, 1.1, 1.2,
2, 3.2, 6.2, 7, 11, 12), `state3-build.md` (2.2, 2.3, 4),
`exe-import-engine.md` (4.2, 5, 7.6), `exe-import.md` (6), `quicksave.md` (3),
`quicksave-census.md` (the `CheckStub()::mine` row), `mp3-decoder.md`;
`relocatable-image-design.md` BUILT with an "as built" paragraph under 0 and
notes at 1.4, 4.1, 4.4, 4.5, 6, 10; I44's status and outcome lines.

**The rule** (`tools/reloc_bare.py --check`, in CI beside `reloc_census.py
--check`): a hex literal in the linked span outside a `Va` / `CodeVa`
definition, a VA name in a memory accessor that does not place, a code VA
cast to a function pointer or handed to `Task_Create` without
`Img` / `Code` / `CodePlace`, in `src/game`, `src/input`, `src/render` (not the
fuzz): 2,889 sites listed today in `analysis/reloc_bare.tsv` (2,780 literals,
109 accessor sites in the harness helpers the fuzz alone reaches and
`rdata_consts.cpp`'s file read, no cast, no task argument), keyed by the
line's text; a new one is refused until it is placed or the list is
regenerated and committed. Its negative controls: stage C's four misses
(`shell.cpp:237`, `field_event.cpp:43`, `magic_harness.cpp:478`, a bare task
entry) and a planted literal, each refused. It cannot see a VA that reaches
a place at run time (a table of VAs of ours, a parameter, a word computed
from a code word), a literal under six hex digits, or an accessor it does not
recognise; the fence, the trap and `CodeToCall`'s `Fatal` are the run-time
answer to those.

**Left:** the fold's width (question 1); the two sub-word reads not proven
and the 729 not read (question 3); the exit trace's stack scan in `bof3x.exe`
(question 2); the run-time base's cost on the attract's pace, unmeasured
(needs a quiet game run at both bases, the coordinator's).

### Before the PR (2026-10-29, the coordinator)

**`main` merged twice** (PR #166, DIV-0185 in `Battle_SetHitPopup`; PR #167,
DIV-0186 in kind 0x6B's particles): one conflict each, both the same shape -
`main` changed a line the round had placed. Resolved keeping both: DIV-0185's
null-block test over the placed `At(Img(at::kHitClass.v))` read, and
DIV-0186's `Particles()` answering `Img(at::kPixels.v)` when the room is off
(a VA would have been a bare address at a moved base; `reloc_bare.py --check`
agrees). Nothing else of either PR touches the round's shapes.

**A regression the gate never saw.** Stage D's gate ran the self-test as
`BOF3X_SELFTEST_ONLY=1` without `BOF3X_SHADOW='*'`, so the shadow fuzz
suite had not run on the round since stage B. Hosted under `'*'` the build
faulted in `field_misc`'s fuzz, silently: the fault is under the loader lock,
where the crash reporter's thread cannot run (it waits its 20 s, then the
process dies with 0xC0000005 - `crash.cpp`'s design note). A temporary log in
the handler itself named it: `memcpy` from address 0, 0x2400 bytes -
`kDrawItems`, the fuzz's file-scope constant from `draw_pool::Items()`.
Bisected over the stage builds under `ri1027` (every `rb*` and `rc*` build
passes, stage C's fails): `draw_pool.cpp`'s `g_items = DrawItems` became a
dynamic initialiser when C1 made the macro `Img(...)` of a run-time base, and
the order between its unit and the fuzz's is unspecified - the fuzz read a
null pool. Fixed in `41372e3c`: the pool's two pointers start null, and
`Items()` / `Free()` answer the original's arrays at their place until
`DrawPool_Grow`'s switch; DIV-0062 is unchanged. A sweep of every file-scope
initialiser in `src` that names an `Img` macro, an `at::*At()` accessor or
the pool found no other that reads another unit's dynamically initialised
global (the rest read `Img()` itself, valid at any time). The full hosted
suite (`BOF3X_SHADOW='*'`), the exe-host and moved self-tests and the eight
check tools then pass on the merged branch; `main`'s own binaries pass the
suite too, so the fault was the round's alone. **The gate's lesson:** a
round's self-test line is `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, hosted,
beside the exe-host and moved runs - HANDOFF's "How to run things" says so.

**The PR's code review** (seven findings on PR #168, each read against the
code): five fixed in one commit, two left as the round's open items.
1. `BuildTable` checked one byte before writing a five-byte jump; a row
   within four bytes of the last would have landed in its rel32. All five
   bytes are checked now (the table's nearest rows are 11 bytes apart, the
   thunk's VA 0x3000 from any, so the stronger check passes today).
2. `Bind_Word` sent the DirectSoundCreate thunk's VA to the trap (`Find` has
   no row for it); it goes through `PlaceOf` now, as `Code` does.
3. The fold's width (`.pool` not folded at a moved base): the round's open
   item, left - it changes every folded reference.
4. `PatchBytes`' linked test relied on no module lying in the span, which the
   engine's `Fence` only checked for the code range; `Fence` is a `Fatal` on
   any module mapped in the span now.
5. The developer launcher fenced a hard-coded `0x400000..0x98F000`; it reads
   `SizeOfImage` from the child's own header now, as the engine's `LinkedEnd`
   does, so a section `make_exe.py` grows is fenced too. The state hash's
   `kExeHi` (Capcom's image, the fold) and `kOwnVa` (where our code starts)
   stay what they are: different things.
6. `BOF3X_IMAGEBASE=0x400000` with a hosted-only switch was refused by the
   launcher though `DetectHost` accepts it; only a base other than `0x400000`
   is refused hosted now.
7. `Code()` is a binary search per call where stage B resolved at compile
   time: the cost is the round's open measurement (the attract's pace at both
   bases); a bitmap over the code range would make it O(1) if it shows.

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
