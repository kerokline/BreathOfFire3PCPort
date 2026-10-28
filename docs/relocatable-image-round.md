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
| Run-time raw calls through an address | 61 | [`round-13-cleanup.md`](round-13-cleanup.md) 1.3, as I44 cites it (not re-measured) |
| The executable | `bof3x.exe` based at `0x400000`, spanning `0x400000..0x93F000`, its own sections holding Capcom's range | [`state3-design.md`](state3-design.md) 1.1 |

Two consequences of that table:

- **Record sites do not care where the image is.** A pointer plus an offset
  follows the pointer. The 17,259 global sites, the generated accessors, the
  code-address constants and the asm operands are what name the base.
- **The 8,917 fuzz sites stay where they are.** The shadow fuzz clones
  Capcom's code and runs in the hosted build only, where `BOF3.exe` is at
  its fixed base by construction. The hosted build keeps the base `0x400000`
  for ever; only the player's executable moves.

## 1. The method: change the spelling first, the address second

The typed-data round's gate was `tools/codegen_same.py`: the machine code of
every object identical to the baseline's, so "no behaviour changed" was a
proof, not a test. This round keeps that gate for as long as it can:

1. **Stage B rewrites every base-naming site as `base + offset`, with the
   base a `constexpr` `0x400000`.** The compiler folds it; the machine code
   is byte-identical; `codegen_same.py` says `0 differ`. Every one of the
   17,259 sites can be converted this way, in parallel by file group,
   exactly as stage B of the typed-data round was.
2. **Stage C makes the base a run-time value** in the exe host (a constant
   in the hosted build, so the hosted build's code may still fold it), and
   moves the image. Here the machine code changes everywhere at once, by one
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

### RA3: the loader's design
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

- The base a run-time value in the exe host, set by the loader of RA3's
  design; the hosted build keeps it `constexpr`.
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
