# The typed-data round: staging (IDEAS I44, stages 1 and 2)

**Status:** DRAFT (staged 2026-10-17 against `origin/main`; nothing built. The
idea is [`IDEAS.md`](IDEAS.md) I44; this stages its first two stages, which
change no behaviour. Stage 3, the relocatable image, is not staged here.)

The game's code is ours; what our code still reads through numbers is
Capcom's data image. This round makes every such read a named, typed access
whose layout is checked at compile time against the original addresses,
with the image still mapped where it is, so the state hash proves nothing
moved. Mechanical, parallel by file group, the shape of a takeover round.

## 0. What the sources do today, measured 2026-10-17

Counts from `grep` over `src/game` (the scripts of this doc's first step
replace them with a committed census):

| Shape | Count | What it is |
|---|---:|---|
| `constexpr ... kName = 0x...` in `*_callees.h` | 6,916 constants, 2,981 distinct addresses | a global named per module: the same address carries up to a dozen names across modules |
| `B(at::kX)`, `W(at::kX)`, `Address(at::kX)` | about 900 sites | a global read through its module's constant |
| `Word(p + 0x..)`, `Byte(s + n)`, `Dword(r + n)` on a local pointer | about 6,700 sites in 275 files | a **record field by offset** on a pointer into a table |
| `kXStride = 0x..` in headers | 12 names for 8 strides | the record families, named differently by each module: `0xA4` is `kObjectStride`, `kCharStride`, `kSpriteStride` and `kRecordStride`; `0x14C` is `kPartyStride`, `kObjStride`, `kMemberStride` |
| `static_assert(offsetof ...)` / `sizeof` | 65 | the few records already typed |
| `[[data]]` entries in `symbols.toml` | 2,696 (2,610 with a `ctype`, 2,407 with a `count`) | the typed globals `gen_symbols.py` already emits as lvalues or element-0 pointers |

So the remainder is two different jobs, and the round splits on them:
**globals** (one `[[data]]` entry per distinct address, the module constants
retired) and **records** (one struct per family, the field offsets named
once, the 6,700 sites converted to member access).

## 1. The rules

- **No behaviour change.** The image stays at its base; a typed access
  compiles to the same load or store. Proof: the state hash on the eleven
  routes and the attract identical before and after each merge, the
  headless self-tests, and the hosted build's clone fuzz where a module has
  one.
- **Evidence per entry.** A `[[data]]` entry's `ctype` and `count` cite the
  instruction width and the stride the reader used (the existing entries'
  style); a struct's field cites the module doc that read it. A field no
  reader names stays padding, by size, never a guess.
- **One name per address.** Where modules named one address differently,
  the census picks the `symbols.toml` name (or adds one with the evidence)
  and the module constants go; the old names may stay as comments at the
  sites for one round, never as a second definition.
- **Rules 1, 4 and 7** as everywhere: counts and addresses only in docs; a
  converted site is converted whole or left, never half; `git commit -s`.

## 2. Stage A (three groups, in parallel; SD merges first)

### CS: the census (tools; no `src/` change)

`tools/data_census.py`: every raw accessor site in `src/` with its file,
line, shape (global by constant, record by pointer and offset, raw
address), the address or the stride family it belongs to, and whether a
`[[data]]` entry already covers it and with what `ctype`. Also the
constant map: each `at::k` constant, its address, every module that defines
it, the `symbols.toml` name at that address or none. Output:
`analysis/data_census.tsv` (committed: addresses and names, our work
product) and the counts above re-measured in `docs/typed-data-census.md`.
The CI rule (`checks.yml`): the census's site count may not rise; after
stage B, a raw accessor in `src/game` fails the check.

### SD: the record types (`src/game/records.h`; reads, then one header)

One struct per record family, from the module docs and the fuzz harnesses
that already know the layouts:

| Family | Stride | Where the layout is read |
|---|---:|---|
| the sprite / object record | `0xA4` | [`sprite-draw-order.md`](sprite-draw-order.md), `sprite_screen.cpp`, the four stride names above |
| the party member | `0x14C` | the battle docs, `battle_e4.cpp`'s readers |
| the effect record | `0x80` | the round-thirteen effect docs |
| the enemy | `0x128`, its data `0x8C` | the battle docs |
| the task record | `0x84` | [`SCAFFOLDING.md`](SCAFFOLDING.md) 3, `task_stacks.py` |
| the window record | `0x24` | [`menu-screens.md`](menu-screens.md), `FieldMenu_Open`'s evidence |
| the save summary, the directory entry | `0x1C`, `0x18` | [`save-files.md`](save-files.md) |

Each struct: named fields where a reader names them, `unsigned char
pad_<offset>[n]` elsewhere, `static_assert(sizeof == stride)` and one
`static_assert(offsetof == ...)` per named field. The tables that hold them
become typed arrays through `symbols.toml` (`ctype` the struct, `count`).
SD changes no site; it ships the header and its asserts, which is why it
merges first and fast.

### SG: the globals (`symbols.toml`, `tools/gen_symbols.py`)

For each of the 2,981 distinct addresses without a `[[data]]` entry (or
with one lacking a `ctype`): the entry, with the width the readers use
(from CS's table and the module docs), `count` where a stride is known.
`gen_symbols.py` already emits the macros; SG adds the one thing missing,
an **address-to-name check**: a module constant whose address has a
`[[data]]` entry is flagged by CS as retirable. SG does not touch sites.

## 3. Stage B (after SD and SG merge; file groups in parallel)

The conversion, by file group as the takeover rounds split the sources
(the effect, battle, field, menu, area, scenario, rest and platform
groups), each owning its files whole:

- a global site `B(at::kX)` becomes the `symbols.toml` name's lvalue;
  the module's constant is removed once its last site is gone;
- a record site `Word(s + 0x1C)` becomes `s->field` on a typed pointer,
  the cast placed once where the pointer is made (the table index or the
  argument);
- a site the group cannot type (a width the docs do not settle, an offset
  no reader names) is left raw and listed in the hand-back, never guessed.

Each group's check: the build, `BOF3X_SHADOW='*'` headless, its module's
fuzz where there is one, and the census's count for its files at zero.
Merge one group at a time, the state hash on one route after each; the
full eleven and the attract at the end of the stage.

## 4. Stage C: the layout as one struct (one group, after stage B)

`tools/gen_symbols.py` grows a second output: `GameData`, one struct
spanning `.rdata` and `.data` from the typed entries in address order, with
named padding between them, `static_assert`ed on every member's offset
(`pc` less the image base) and on the total size. The engine takes a
`GameData*` at the image base; the macros of today become members, by a
mechanical rewrite the census drives. The padding members are the census
of what in `.data` is still unknown, by address and size, in
`docs/typed-data-census.md`'s last section. Still no behaviour change;
the same proofs.

## 5. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/data_census.py`, `analysis/data_census.tsv`, the CI rule | CS |
| `src/game/records.h` | SD |
| `symbols.toml` `[[data]]` entries | SG in stage A; in stage B each file group adds only entries its own files need, merged by entry keyed on `pc`, then a `tomllib` check |
| `tools/gen_symbols.py` | SG in stage A, stage C's group after |
| each source file | its file group in stage B, one owner |
| `docs/README.md` rows | each group its own; keep both sides |

Merge order: SD, SG, CS in that order (CS's rule reads SD's and SG's
output); then the file groups one at a time; then stage C.

No ledger entry: nothing the player sees or presses changes. If a
conversion finds a width the original read differently from what a module
assumed (a 16-bit read of a 32-bit field, say), that is a finding for the
defect list, not a fix inside this round. Every commit `git commit -s`.

## 6. The brief, per group

Each brief names: the goal; the files it owns; the docs to read first
([`IDEAS.md`](IDEAS.md) I44, this doc's section 1, `tools/gen_symbols.py`'s
docstring, [`SCAFFOLDING.md`](SCAFFOLDING.md) on the generated header, the
module docs for its files); the checks that mean done; the hand-back
report's shape (sites converted, sites left raw and why, entries added).
Rules 1, 4 and 7 restated. A usage cut resumes with a message naming the
next step; only the hand-back report means merge.

## 7. Not in this round

- Stage 3 of I44 (the relocatable image, the entry-stub page retired, the
  61 run-time raw calls): after this round, its own staging.
- The inline-asm sites that name a global by `"m"` (the x87 clones): they
  keep working because nothing moves; they convert with stage 3.
- `src/hook`'s own reads of the image (the host, the exe image, the state
  hash): they address the image by design and stay.
