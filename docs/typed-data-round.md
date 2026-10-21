# The typed-data round: staging (IDEAS I44, stages 1 and 2)

**Status:** IN PROGRESS (staged 2026-10-17; stages A and B run 2026-10-19 on
`typed/b1`, 50,943 raw data sites down to 27,394 with the machine code
identical to `origin/main`, merged as PR #94; stage C, staged in
[`typed-data-stage-c.md`](typed-data-stage-c.md), built 2026-10-19..20 on
`typed/c` (C0..C4, section 3), machine code identical to `main` (56d81b42, then
61023304 merged in), the smoke run passed 2026-10-20, its PR next. The idea is
[`IDEAS.md`](IDEAS.md) I44; this round is its first two stages, which change
no behaviour. Stage 3, the relocatable image, is not staged here.)

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
stage B, a raw accessor in `src/game` fails the check (built in stage C4
as `data_census.py --strict`: unless `analysis/typed_left.tsv` lists the
site with a reason).

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

SG's widths are measured by `tools/data_widths.py`. It skips the 7
`[[block]]` base addresses but not scalars inside a block's records: 73 SG
entries sat inside MsgBoxState, WindowRecords, EnemyWorkingRecords and
CharacterRecords at the first SG commit (counted by the SG verification,
2026-10-19; 74 after its fixes, over the same address ranges). They become
fields of SD's record types in stage C, not before.

### Stage A as run (2026-10-19)

CS, SD and SG ran in parallel worktrees from `origin/main` `19180e63`, with
the pairing pass of [`install-gap-pairs.md`](install-gap-pairs.md) 8 beside
them. Each was then **re-derived independently** by an agent that did not see
the branch first (reports in `C:\Users\kerok\v_typed\`). All four agreed on
the core and each had missed a class of cases:

- **SG** (`[[data]]` entries): 31 of 485 entries wrong. Its scanner did not
  count `lea` or `add reg, addr` as indexing, read dword loads masked to 16 bits
  as dwords, and missed wider stores from a neighbour. The fix moved them to
  `analysis/typed_globals_unsettled.tsv` and committed the corrected scanner as
  `tools/data_widths.py`; 488 entries stand.
- **SD** (`records.h`): no offset or width wrong; four fields of the wrong
  signedness, and about 79 bytes that readers use left as "unread" padding.
  The fix named them: 263 fields (from 206), five of the eight open questions
  settled ([`typed-records.md`](typed-records.md) 3).
- **CS** (the census): it missed whole classes of sites (module constants
  outside `namespace at`, helper-based records, seven accessor names), so its
  baseline rose from 39,503 to 50,943 and the ratchet now sees them.
- **The pairs:** all 385 pairs reproduced; five errors in the doc, corrected.

The merged tree did not build: SG had typed `Party_Zenny`, whose macro then
rewrote every `bof3::addr::Party_Zenny`. Fixed on `typed/integration`
(`29e8ea61`) with `<Name>_addr` emitted beside each typed entry, and
`tools/codegen_same.py` added, which became the round's proof (section 3).

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

### B0 as run (2026-10-19)

Stage B ran in two parts: B0, every global site in the tree by one tool, then
B1, the record sites by file group. B0 is on `typed/b0-globals` from
`typed/integration` `29e8ea61`. No behaviour change, no ledger entry.

**The tool.** `tools/typed_convert.py` finds every reference to a data-address
constant, as the census does: an `at::` constant, or a module constant that the
file or its includes define (never a same-named constant elsewhere in `src/`,
and never `S::kChoice`, an enumerator, or `at::kTable[0]`). For each reference
it reads the accessor around it from that accessor's own definition. The
evaluator walks the body: `return At(a)[0]`, `memcpy(&v, At(a), sizeof v)`,
`static_cast<short>(Word(Mem(a)))`, a setter's `memcpy` or delegation, `using`
declarations and namespace aliases. That gives the access's width,
signedness, result type, and whether it reaches memory through an
out-of-line function. A site becomes typed only when:

1. the address is exactly a `[[data]]` entry with a `ctype`, or a constant
   offset onto an element of a typed array (`Name[i]`). A dynamic index
   (`B(at::kT + i)`, `W(at::kT + 2 * i)`) is accepted only on an array with a
   `count` whose element size is the stride. The covering entry is the one
   that starts last, because entries nest (`Party_Zenny` inside `Cond_Flags`).
2. the access width is the `ctype`'s. If the signedness differs, the read
   becomes `static_cast<T>(Name)` with T of the access's signedness. An
   lvalue use (`=`, `|=`, `+=`, `++` and the other bit-preserving operators)
   writes `Name` directly. `/=`, `>>=` and `%=` are left raw.
3. the expression keeps its type. If the accessor returns a type that promotes
   differently from the `ctype` (a `U` from a u16 load), the result is
   `static_cast<R>(Name)`, unless the value is used directly: assigned,
   initialised (not `auto`), returned, compared for equality, tested, or used
   as an index. A setter `SetW(K, v);` becomes `Name = v;` only as a whole
   statement.
4. the outermost form that types is used. The tool climbs from the
   constant's accessor through a pointer reader (`Word(At(K))`), a subscript
   or dereference (`At(K)[i]`, `*At(K)`), and a cast or cast function
   (`static_cast<signed char>(B(K))`, `S8(B(K))`). Only one-argument calls,
   and two-argument stores, are climbed.

The other address forms keep their accessor and rename only the constant:

- a pointer form (`At(K)`, `Mem(K)` with nothing loading through it) becomes
  `At(Name_addr)`;
- an address handed to a helper the tool does not read as an accessor becomes
  `Entry(Name_addr, i)`;
- an address used bare becomes `Name_addr`, or `Name_addr + 0x8` for an
  element. A module constant used bare must be its entry's own address.

Two cases keep their accessor too:

- **Harness sites.** In every `*_fuzz.cpp` / `*_harness.*` file, a site keeps
  its accessor and only the constant changes (`B(Name_addr)`,
  `Mem(Name_addr)`).
- **Seam sites.** In other files, an accessor that reaches memory through the
  harnesses' out-of-line `Mem` (`area_harness::Mem`, `sh::Mem`, `ah::Mem`,
  `bh::Mem`) also keeps its accessor.

A constant named as a bound or a size (`kCellsEnd`, `kBurstSize`) whose value
is a different entry's address is never renamed in an address form, because
`Area16_PlaceMessages_addr` would read as that table and not as the end of the
one before. A load through such a constant still converts. Anything else is
left raw with a reason.

`--gate BASE` rebuilds each file's objects with ninja and compares them,
function by function, against the baseline build (`codegen_same.py`'s
disassembly). If an object differs, bisection over the file's sites finds the
sites that cause it. Each such site steps down a tier (typed, then typed with
the full cast, then the address form where rule 4 allows it) or is left raw
with the reason `gate`. A compile error counts as a difference.

`--retire --since typed/integration` replaces each module constant that this
stage left unused with the note `// kX: now Name (symbols.toml).`, keeping the
comment that followed the constant. These are constants in a `*_callees.h` or
a `.cpp`, that something used at `since`, that nothing that can see them
spells now (code or comment, through `at::`, not as another scope's
enumerator), and whose value is a typed entry or element. A statement
declaring several constants keeps the rest. A constant that a comment still
cites stays, even when that leaves it unused: 10 such constants now warn
`-Wunused-const-variable`. They include `object_kinds.cpp`'s four, cited by a
region table's comments, `kFrameSet` (twice), and the fuzz files' `kState` /
`kSlot`.

**The run.** There were 14 batches of 30 to 41 files (`git log
typed/integration..typed/b0-globals`). The baseline was the `origin/main` build
at `.claude/worktrees/typed-baseline/build`. After every batch the full build
and `codegen_same.py` printed `637 objects compared, 0 differ`, and they did
again after each of the follow-up commits:

- the 81 bound-named address forms put back;
- 20 retired constants a comment still cited restored;
- 11 accessor helpers that B0 left unused removed (an unused helper is not
  emitted);
- the redundant parentheses of the early batches' `Mem((X_addr + 0x70))`
  removed.

| Count | Before | After |
|---|---:|---:|
| census sites (the CI number) | 50,943 | **32,265** |
| global sites | 29,048 | **10,358** |
| record sites | 20,067 | 20,079 |
| `at` constant definitions | 7,659 | 4,311 |
| data-valued module constant definitions | 2,019 | 937 |

Record sites rose by 12 because a renamed inner constant uncovers its outer
call. Under the census's rule D5, `Word(Ptr(at::kCell) + 2)` used to be the
constant's site and is now a record site.

Converted: 19,910 references.

- **7,908 typed:**
  - 6,878 typed loads and lvalues;
  - 175 with the keep-the-promotion cast;
  - 855 stores.
- **12,002 address forms:**
  - 4,444 seam, in the area, magic, boss and scenario modules;
  - 2,782 harness;
  - 2,526 bare;
  - 1,350 pointer;
  - 712 handed to a helper;
  - 269 in an argument expression;
  - less the 81 put back, spread over these kinds.

Left raw: 11,521 references, every one in `analysis/typed_b0_left.tsv`
(regenerated from the final tree with `--dry-run --remaining`; since C0,
folded into `analysis/typed_left.tsv`):

| Reason | References |
|---|---:|
| `no-entry`: no `[[data]]` entry covers the address | 9,135 |
| `inside-entry`: inside an entry but not on an element, or an entry without a `count` | 685 |
| `width`: the access width is not the `ctype`'s (mostly record fields of `Effect_Objects` / `ObjTrio` by fixed offset) | 576 |
| `untyped-entry`: the entry has no `ctype` | 351 |
| `name`: a bound or size name on another entry's address | 219 |
| `gate`: typed, but the code changed (dynamic indexes, a signed entry stored and re-read) | 191 |
| `pointer-in-integer`: a pointer read or stored through an integer entry | 151 |
| `type`: a pointer or float entry read as an integer | 96 |
| `index`: a dynamic index on an entry without a `count`, or with another stride | 60 |
| `accessor`: an accessor whose body the tool does not read | 24 |
| `no-symbols-header`: the file does not include `symbols.gen.h` | 17 |
| `macro`: inside a `#define` | 15 |
| `no-object`: a header's own site (the gate builds objects) | 1 |

The TSV also lists bare module-constant references, which the census does
not count.

Retired: 4,455 constant definitions became notes. The CI ratchet is now
32,265 (`data_census.py --check` passes). `ledger_check.py` reports 0 errors.

**For B1.** The conventions B0 settled:

- **Spellings.**
  - A scalar is `Name`, an array element `Name[i]` (decimal below 10, else
    hex).
  - An address is `Name_addr`, and `Name_addr + 0x8` for an element.
  - `bof3::addr::Name` cannot be written for a typed entry, because the macro
    rewrites it.
  - A changed signedness is `static_cast<std::int16_t>(Name)` and the like
    (`CANON` in the tool).
  - A kept promotion is `static_cast<std::uint32_t>(Name)`.
- **The harness rule** covers more than the fuzz files. Every module whose
  accessors go through an out-of-line harness `Mem` (the `area_w*`, `magic_*`,
  `boss_*` and `scena_*` modules) keeps the seam. A typed field there must
  still go through `Mem()`, or the object changes and the fuzz loses its
  recorder.
- **Inbounds pointer arithmetic is not free.** Many dynamic indexes
  (`B(at::kT + i)` as `T[i]`) and some stores to a signed entry compiled
  differently and were left raw. Expect the same of record fields reached by
  a computed index, and keep the gate per object.
- **Readers' widths disagree with the entries.** 576 sites read a typed entry
  at another width: words and dwords inside `Effect_Objects`, `ObjTrio` and
  `Sprite_Objects` by fixed offset. These are B1's or stage C's fields, not
  B0's.
- **151 pointer-holding cells are typed `unsigned long`.** Examples are
  `BattleE4_MenuActor` and `BattleFlow_TaskCurrent`. Retyping them as
  `unsigned char *` is an SG follow-up that would let them convert.

### B1 as run (2026-10-19)

The record sites, by seven file groups (magic, effect, rest, area, battle,
field, and scenario with the menus and platform), each in its own worktree
from `typed/b0-globals`, each owning a disjoint list of files. The brief and
the lists are outside the tree (`C:\Users\kerok\b1\`); stage C reuses them.

- **The proof:** `tools/codegen_same.py` against the `origin/main` build after
  every batch, `637 objects compared, 0 differ`, and again after each of the
  seven merges and on a clean build of the result. The state-hash runs this
  section first planned were not needed: identical instructions cannot
  behave differently. No game was run for this round.
- **Converted:** 4,884 census sites (magic 1,347, battle 876, effect 641,
  field 604, area 535, scenario/UI 505, rest 376), on all nine structs.
  Record sites 20,079 -> 15,219; all raw sites 32,265 -> 27,394.
- **Left raw:** every site with a reason, in
  `analysis/typed_b1_<group>_left.tsv` (15,228 rows; the totals by reason
  are in [`typed-data-stage-c.md`](typed-data-stage-c.md) 0; since C0,
  folded into `analysis/typed_left.tsv`).
- **The family audit.** The gate cannot catch a pointer typed as the wrong
  struct when the offsets agree. Three read-only audits checked about 1,500
  typed pointers and found 13 wrong (one in magic, nine in battle's fuzz
  harnesses where a party index ran past the three records, one each in
  rest, area and field). All 13 were put back to raw (`b78726f8`), each listed
  with reason `audit-family`. The audits also confirmed magic's central call
  (`Sprite_Current` in BMAGIC code is a 0x84 task slot, from
  `BattleTask_RunAll` 0x435110) and area's handler typing.
- **What the groups had in common** (stage C's input): each wrote its own view
  helpers and its own answer to the assignment-order trap (C++17 evaluates
  `p->f = expr`'s right side first; the setter read the pointer first); the
  sprite head's bytes are padding in four structs; several struct field names
  mislead (`WindowRecord::centre_x` is the top-left corner; `BattleTask::y` is
  world z).

### C0 as run (2026-10-19)

The consolidation of [`typed-data-stage-c.md`](typed-data-stage-c.md)
section 2, one agent alone on the tree, on `typed/c0` from `main` 56d81b42.
No site converted and no behaviour change.

- **The shared views.** `src/game/record_views.h`, namespace `rv`, always
  spelled qualified: about thirty files keep a local function-pointer
  `As<T>(f)` and nine a local `Current()`, which would hide an unqualified
  shared name. Five templates, each a cast and nothing else:
  - `rv::As<T>(p)`: the record of family `T` at `p`, const when `p` is (the
    rest group's const / non-const helper pairs);
  - `rv::AsConst<T>(p)`: a read-only view of a writable pointer (the old
    `reinterpret_cast<const records::T*>`);
  - `rv::Current<T>()`: `Sprite_Current` as `T`, read at every call;
  - `rv::Bytes(r)`: a typed record's own bytes, for an offset the struct
    pads or reads at another width;
  - `rv::Store(field, v)` / `rv::Store<P>(field, v)`: a setter's store
    through a typed field, a call like the setter, so the field's pointer is
    evaluated in the setter's order and not after the value as `p->f = v`
    would be; `P` is the setter's parameter type. The header documents the
    order.
- **The survey.** 488 per-file helper definitions on 56d81b42, in 217 files:
  401 typed views (`ScTask()` 49 files, `EnemyRec` 24, `CurFx` 24, `AsFx` 24,
  `Spr` 22, `CurSprite` 17, `Fx` 14, `CurEffect` 12, `ScEnemy` 12, `Cur` 12,
  `Win` 12, and 41 more names), 29 `Bytes(records::T*)` reverse views, 58
  `Store` (15 effect files' `template <P, T> void Store(T&, P)`, 43 magic
  files' `void Store(std::int32_t&, std::int32_t)`). Every one was a pure
  cast of an expression (`Sprite_Current`, `p`, `Party(i)`, `Party(a) -
  0x10`, `Enemy(a) + at::kEObject`, `Member(slot) - 0x80`, ...), a call of
  another view (`Obj(n)` = `Spr(Object(n))`), or the store. All 488 went;
  none needed to stay. The list with each replacement is in the round's
  scratch (`C:\Users\kerok\bof3x-runs\c_typed\c0\survey.tsv`).
- **The move.** A script replaced each call with its body under the shared
  name, the arguments put in place of the parameters (`PartyRec(a)` became
  `rv::As<records::PartyMember>(Party(a) - 0x10)`), and the 1,137 inline
  `reinterpret_cast<records::T*>` locals with `rv::As` / `rv::AsConst`, in
  seven batches by group (256 files). The tree now spells 3,059 `rv::As`, 244
  `rv::AsConst`, 1,786 `rv::Current`, 431 `rv::Bytes` and 361 `rv::Store`.
  The family notes beside the helpers stay where they were, reworded for the
  shared names. After every batch the full build and `codegen_same.py`
  against a `main` 56d81b42 build printed `640 objects compared, 0 differ`.
- **`tools/typed_convert.py`.** Two zero-width insertions at one offset each
  lay within the other, so `render` dropped both (B1's effect group: 34
  false gate failures); they are merged in site order. An insertion at the
  edge of a replaced span was dropped the same way and now renders beside
  it. `--selftest` checks the renderer on ten cases (six failed before).
- **One left-raw list.** `tools/typed_left.py` writes
  `analysis/typed_left.tsv` from the tree: one row per census site (27,408),
  keyed by file, function, shape, accessor, address expression and an
  ordinal, with the reason, evidence and source carried by key, so a list
  no longer goes stale with an edit; `--check` says whether the committed
  file is current. It replaces `analysis/typed_b0_left.tsv` and the seven
  `analysis/typed_b1_<group>_left.tsv` (B0 and B1 above cite them; they are
  in git history at `56d81b42`). `typed_left.py import` built it once,
  carrying each old row's line from the tree its list was written against
  by a line diff and matching it to the census site there by offset or
  constant: 25,501 of the 26,745 rows landed. The 1,244 that did not are
  1,234 of B0's bare module-constant references, which the census does not
  count; 9 B1 `audit-family` rows on subscript writes (`p[k] = v`), which
  the census does not count either; and one B1 `gate` row in
  `save_menu.cpp` whose site the quicksave round has since typed. 1,914
  sites carry no reason yet (`-`): 1,827 raw-literal sites no list covered,
  56 global, 31 record.
- **The census.** 27,407 -> 27,408. Not a new access: deleting `Dir()` in
  `save_menu.cpp` uncovered one record site it had hidden from the census's
  accessor scan, `At(dir + n * at::kDirectoryBytes)` (now under
  `rv::As<records::SaveDirEntry>`). `data_census.py --check` fails by that
  one until the census is regenerated.
- **For C1 and C2.** The event, scenario and field dialects viewed any
  sprite-family pointer as `SpriteObject` (`Spr(p)`, the field's typed
  `Sprite_Current`) and used it below +0x80 only; those are now
  `rv::As<records::SpriteObject>` / `rv::Current<records::SpriteObject>()`
  on party members, kind-2 objects and enemies too, sound only for the head:
  they want C1's `SpriteHead`. The 431 `rv::Bytes` sites are offsets the
  structs pad or read at another width: mostly the 16.16 halves +0x36 /
  +0x3A / +0x3E (86), +0x14 and +0x20 (46), +0x34 / +0x38 read as words
  (30).

### C1 as run (2026-10-19)

The types of [`typed-data-stage-c.md`](typed-data-stage-c.md) section 3,
one agent on `typed/c1` from `typed/c` 0e929409 (C0 merged), beside C3's
entry work. Header and doc work, and the renames of existing typed uses;
no raw site converted. The record is
[`typed-records.md`](typed-records.md) sections 6 to 8.

- **`SpriteHead`.** One struct for `+0x00..+0x7F`, and the five families
  derive from it (`SpriteObject`, `PartyMember`, `EffectObject`,
  `BattleEnemy`, `BattleTask`), so `p->field` is spelled as before. Read
  byte by byte against every family's readers: the layout is one wherever
  the sprite engine reads it; the families disagree at the handler bytes
  `+5..+0xB`, the handler dwords `+0x18..+0x23`, and the popup task's
  `+0x32` and `+0x60`. Those are anonymous unions of each family's own name
  (`actor` / `kind` at `+5`, `type` / `param` / `runner` at `+6`, and so
  on), so each family keeps its field; the dwords got neutral names
  (`work_18`, `work_1c`, `work_20`). The 16.16 positions are unions with
  their `u16` halves (`x_whole`, `z_whole`, `height_whole`, and `_frac`); a
  throwaway conversion of 21 sites in `field_c1.cpp` / `field_c2.cpp` gated
  `0 differ` and was reverted.
- **The renames** (1,937 sites in 124 files): `BattleTask::y` -> `z`,
  `vel_x` / `vel_y` -> `step_x` / `step_z`, `start_x` / `start_y` and
  `SpriteObject::accel_x` / `accel_z` -> `work_18` / `work_1c`,
  `BattleTask::param` -> `kind` and `kind` -> `runner`,
  `WindowRecord::centre_x` / `centre_y` -> `x` / `y`,
  `PartyMember::member` -> `actor`, `raised` -> `reach`,
  `SpriteObject::pose` -> `flip`, `EffectObject::in_use` -> `flags`, and
  the party's and enemy's `state[0..3]` -> `state` / `step` / `state3` /
  `state4`; `WindowRecord::kind`'s comment from its writers. Each use was
  found by compiling with the old name hidden and edited at the reported
  column, so no homonym on another struct was touched.
- **The gate caught one thing.** After the renames three objects differed:
  where two branches store to different state bytes, the compiler merged
  the stores through the array index, and could not through two named
  fields. Seven stores in `EnemyOp_CastStart`, `BattleObj_CastStart` and
  `Field_LeaderStand` are spelled `states[k]` (the head keeps the four
  bytes as an array too); then `0 differ`.
- **Views** (cast targets, `rv::As<T>`): `ScriptContext` (a field object's
  `+0x80`, a member's `+0x124`), `MemberFieldTail` and `ActionRecord` (the
  party tail in the field and in battle; the latter also the enemy's
  `+0x104`), `BattleWindow` and `ItemListWindow`, `EffectWork` (the kinds'
  own words at `+0xC`, `+0x10`, `+0x2E`, `+0x30`).
- **New families:** `CharRecord` with `CharStatBlock`, `EncounterSlot`,
  `CommunityRecord`, `CommunityBuilding`, `SharedScratch` (three views by
  offset over the relocated PSX scratchpad: its readers give no offset one
  meaning), base stat names in `PartyMember` and `BattleEnemy`; in two new
  headers, the GPU packets and `SVector` (`records_gpu.h`) and eight effect
  sub-pools (`records_effect.h`). Each named only as far as its readers
  agree.
- **What it unlocks:** about 6,200 of the 7,982 left-list rows with C1's
  reasons (`typed-records.md` 8 has the table by type, the traps and the
  `symbols.toml` entries SG could type with these structs).
- **Checks:** the three headers compile standalone with `-std=c++20 -Wall
  -Wextra` and under `#pragma pack(1)`, 656 offsets and sizes asserted; the
  tree builds; `codegen_same.py` against the `main` 56d81b42 build: 640
  objects compared, 0 differ; `data_census.py --check` 27,408, unchanged;
  `typed_left.py --check` current after regenerating six rows whose keys
  named `->member` (their reasons carried).

### C2 as run (2026-10-19)

Seven agents, one per B1 file group, from `typed/c` `1b1701a1` (C0, C1 and
C3's entries merged, the census and the left-list regenerated), each on its own
branch `typed/c2-<group>` with its B1 file list and the brief
`C:\Users\kerok\bof3x-runs\c_typed\BRIEF_C2.md`. Each converted the rows of
`analysis/typed_left.tsv` in its files whose reason C1's types or C3's entries
had removed, with the gate after every batch; merged one at a time in the
order they handed back (battle, area, magic, rest, effect, field, scen_ui),
the build and the gate after each merge, `0 differ` every time.

| Group | Files | Sites before | After | Converted |
|---|---:|---:|---:|---:|
| battle | 161 | 3,422 | 2,989 | 433 |
| area | 131 | 2,411 | 1,337 | 1,074 |
| magic | 142 | 5,471 | 3,559 | 1,912 |
| rest | 111 | 4,240 | 3,276 | 964 |
| effect | 143 | 5,714 | 3,862 | 1,852 |
| field | 128 | 2,472 | 1,725 | 747 |
| scen_ui | 399 | 3,668 | 2,947 | 721 |
| the tree | | 27,408 | 19,705 | 7,703 |

What converted, by what unlocked it: the shared head's names and the 16.16
halves on every family (the largest share in every group); `EffectWork` for
the kinds' own words; `BattleTask` head fields on the task records (magic,
about 1,050); `ActionRecord`, `BattleWindow`, `ItemListWindow`, `CharRecord`,
`SVector`, the effect sub-pools, the GPU packets typed by the setter each
function calls; C3's 72 entries and the retyped pointer cells (`rest` took
324 global references by the converter, `scen_ui` about 70); raw literals on
typed entries. What stayed, by the groups' own lists: the 1,546
`DamageScratch` rows (the magic harness's out-of-line `Mem()` is the seam:
`rv::As<DamageScratch>(Mem(kS))->d_08` passes a different address to `Mem`,
measured as `1 differ`), the pools and packet kinds with no struct (G3, G4,
F4, LineG2, Tile1; kind 0x48's sparks, 0x50's specks and the rest the effect
hand-back lists), the window records' per-handler bytes past +7, the 17
pointer entries still typed as integers, and the gate's own refusals (a u16
read of a dword field that becomes a dword load, adjacent float stores the
compiler merges, a loop induction variable that changes when its base is
typed).

Three read-only family audits followed (slices battle+magic, effect+area,
field+rest+scen_ui; brief `BRIEF_AUDIT.md`), reading every typed pointer in
the diff, the groups' own thin-evidence lists first. The binary settled the
three flagged action-record addresses (0x939FA0, 0x904B40, 0x904B60: every
writer stores a member's +0x124 or an enemy's +0x104). **No group had typed a
pointer as the wrong family.** What the audits found was the class the gate
cannot see and C1's shared head made possible: a head field's name used for
a kind's own word, and a function typed as the field sprite object that other
families also run through. The fixes (`typed/c2-fix`, the paragraph "C2
audit as run" below) moved 63 effect uses of a kind's +0x32 from `draw_key`
to a new `EffectWork::work_32`, the movement-script interpreter, its op
handlers and the sprite projection (about 190 lines) from `SpriteObject` to
`SpriteHead`, two kinds' +0xC/+0x10 to `work_0c` / `work_10`, four party-member
union bytes back to raw, one pre-C2 `BattleTask` pointer in the Weretiger
sprite to `SpriteHead` (its callers make the acting member current, which
the B1 audit's swap scan had missed), and `MagicFx_NearSprite3D` /
`NearPoint3D` to the head. `0 differ` throughout; 19,707 sites after.

Two tool lessons: `typed_convert.py` mis-rendered CRLF files for the battle
group (comment text spliced into an array index, enumerators renamed to
`_addr` forms), so every group diffed the tool's edits before building and
most converted by hand; and `data_census.py` counts
`At(Name_addr + k * 0x24)` as a raw site but `At(Name_addr) + k * sizeof(T)`
not, which C4's struct-typed arrays make moot.

### C2 audit as run (2026-10-19)

Three read-only family audits read C2's diff (`typed/c` acd99ccc against
1b1701a1), one per slice: effect and area (ea), field, rest and scenario
UI (frs), battle and magic (bm). No slice found a struct on the wrong
record outright; they found names on the right record that the bytes do
not mean there, and sprite-head bytes reached through a family narrower
than the pointer. One agent on `typed/c2-fix` applied the findings the
coordinator accepted, a commit per letter or few, each gated `0 differ`:

- **A** (ea F1, doubt 1; frs F4): `EffectWork::work_32` (u16, `+0x32`)
  added; 56 lines (63 uses) in kinds 0x2B, 0x43, 0x45, 0x51, 0x78, 0x98,
  0xAE and kind 18's subs 0x0F, 0x17, 0x24, 0x25, 0x26, 0x39, 0x2B moved
  off `draw_key`, the 52 of ea F1 among them; kind 0x78's three reads of
  Sprite_Objects record 2's turn back to `W(at::kObject2Turn)`.
  `Sub44_Follow`'s store stays `draw_key` (a real key).
- **B** (ea F2): one site, the stars' level back to `Word(Vertices() + 8)`.
- **C** (ea doubt 4): two, `Field_State + 0x130` as
  `MemberFieldTail::script`.
- **D** (ea doubt 5): three, area 75's effect meter as `EffectWork::work_0c`.
- **E** (frs F1): four, a party member's `+7` / `+6` in `field_c3.cpp` as
  raw bytes.
- **F** (frs F2): `Sprite_ProjectA` and `Sprite_UpdateScreen` on
  `SpriteHead`, 21 lines; then, at the coordinator's word, the same in
  `Sprite_UpdateScreenA` and `Sprite_QueueOverlay` (2 more).
- **G** (frs F3, D3): the movement interpreter's named functions on
  `SpriteHead` for their `Sprite_Current` / `Sc()` roots, 109 lines in
  four files; then, at the coordinator's word, the rest of the interpreter
  the same way (every other function of `move_groups.cpp`,
  `move_script.cpp`, `move_cmds.cpp`, and `field_o.cpp` `MoveCmd_Op88*`,
  `Op87*`, `OpDB`: 75 lines more). `Field_ActiveMember` (`Fam()`, in
  `MoveCmd_OpF9`, `Op88*`, `Op87*`) keeps `SpriteObject`: a Sprite_Objects /
  Sprite_ObjectsExtra record (symbols.toml), and `speed`,
  `script_position`, `target_x` / `target_z` are that family's. No
  `Sprite_Current` / `Sc()` site in the interpreter reads past `+0x7F`;
  `Party_MoveMember` types no member root (its `object` is only arithmetic).
- **H** (frs F5): two, the world map plate's cell ahead as `work_0c` /
  `work_10`.
- **I** (bm F1): two, `Weretiger_DrawSprite`'s acting member as
  `SpriteHead`.
- **J** (bm doubt 5): two, `MagicFx_NearSprite3D` / `_NearPoint3D` on
  `SpriteHead`.

Left for the naming pass or C4: the misleading names on the right family
(ea doubt 8, bm names), frs D1's window-record bytes named as other
modules' globals, frs D2, D4, D5, D6, ea doubts 2, 3, 6, 7.
`codegen_same.py` against the `main` build: 640 objects compared, 0
differ; `data_census.py --check` 19,707 (19,705 committed; the sites put
back to raw).

### C3 naming as run (2026-10-19)

The last bullet of [`typed-data-stage-c.md`](typed-data-stage-c.md)
section 5: one agent on `typed/c3-naming` from `typed/c` eb9dac20 (C0..C2,
the audit fixes, the regenerated analysis), alone on the tree. Renames by
what the readers do with the bytes, each with its evidence in the entry's
`note` (symbols.toml) or the field's comment, and in the commit; seven
commits, each gated `0 differ`.

- **Enumerators** (B0's `*_addr` spellings): six clone and stub indexes in
  five fuzz files named after the clone or stub they index
  (`kTargetConfirm`, `kBossEncounter`, `kFormWait`, `kSimLevelLit` /
  `kSimLevelDark`, `kScreenRun`), clear of the `at::` constants that made
  `typed_convert.py` take them.
- **52 `[[data]]` entries.** Thirteen globals by their readers:
  `Config_WindowColour` / `Config_Background` (the Config screen's rows 1
  and 2), `Gene_Bits`, `Area189_StepsSinceEvent` / `_EventsInRow`,
  `Area_EnterTransition` (GameMode_Enter's transition), `Chapter_FlagRow`,
  `EventBattle_Id` / `EventBattle_Flags` (Field_StartEventBattle's id and
  `EventBattle_Records[id] +0`), `Battle_PartyStanding`,
  `Transition_Busy` (Transition_Start is a no-op while it is set),
  `Character_IndexById` (char_id to the CharacterRecords index),
  `Fish_Nibbling` (rest_1f's "pose" was one byte with one reader, the
  fish). Five battle-wide counts whose readers all agree kept their word
  and lost the module prefix (`Battle_EnemyCount`, `_EnemiesLeft`,
  `_DropCount`, `_EndFlags`, `_ExpTotal`). Every `field_of =
  "WindowRecords"` entry (34, the frs audit's D1) is now `Window<N>` (the
  record; its scalar the `in_use` byte, its `_addr` the base the `+4` /
  `+6` uses read from) or `Window<N>_<field>` by `WindowRecord`'s head
  name, by the readers' word past `+7` where they agree, `_ParamA` where
  they do not (`Window0_ParamA`, `Window1_ParamA`).
- **Entry metadata:** `AreaMap_EntryHandlers` count 128 -> 4 (the mask's
  reach was not the table): field_o's eight piece lists B0 had spelled
  as `AreaMap_EntryHandlers_addr + 0xbc..0x1f4` are their constants again
  (+9 census sites, reason `no-entry`), and the map_layers fuzz names its
  128 stand-ins `kMaskReach`. `Field_Members` noted, not renamed: it is
  `ObjTrio + 0x148`, record 0's `char_index` (C4's ObjTrio as
  `PartyMember[3]`). The brief's item 4 constants were already retired by
  C2 with notes that match their addresses (checked against the history).
- **Fields:** `BattleEnemy::result_head` / `command` and
  `PartyMember::queued_kind` are `ActionRecord`'s `command` /
  `command_kind` on all three; `BattleEnemy::kind` (`+0x8C`) is
  `enemy_kind` (compiled with the head's `kind` hidden for the pass, so no
  use fell through to `+5`); `BattleEnemy::item_a8` / `item_ac` named out
  of the pads (EnemyData `+0x30` / `+0x34`, Identify_DrawEnemy's item
  words). `EffectSpark`'s comment no longer claims E3B's sparks.
- **Head names at typed sites:** the world-map dial frame's y (`+0x2E`,
  19 lines in the eleven `FrameSlide*` copies and `WorldMap_FrameWait`)
  from `screen_x` to `EffectWork::work_2e`. Kind 0x13's `angle[0..2]` is
  right (CameraTurn_Start's three target angles); `area_w3f.cpp`'s
  `Kind13` comment and parameters were what said x / y. No new view was
  needed.
- **Docs:** `window-task.md` 3 and `window_task.cpp` say the record's
  top-left, not its centre; `typed-records.md` rows follow the fields; the
  renamed entries renamed in every doc but this one and the stage-C
  staging doc, which keep the names they were written with.

Left as they are (the hand-back lists each): the misleading non-union
names on the right family (ea doubt 8, bm's Leech `anim_data`), `reach`
on kind 9 (saved and restored only), `work_18` as a place id (neutral
already), `ItemListWindow::marked` for the sell list's row, frs D2, D4,
D5, D6, ea doubts 2, 3, 6, 7, the `Rest2b_Model*` / `Rest2c_Figure*`
family (a struct, C4), `ScriptContext +6 / +8` (one reader, a split of the
view), the `.rdata` float and `Imp_*` names, `Menu_PlayTimePieces`' count
(it reaches `0x6633E0`, the wheel's first list; unmeasured here), the
battle phase bytes whose module names disagree (`BattleE4_Step2..4`,
`BossSe_Script`, `BossSb_Countdown`, `BattleActorCopies_EntryCount`, ...).
`codegen_same.py` against the baseline: 640 objects compared, 0 differ;
`data_census.py --check` 19,716 (+9, the restored constants);
`typed_left.py --check` current.

### C4 as run (2026-10-19 .. 2026-10-20)

[`typed-data-stage-c.md`](typed-data-stage-c.md) section 6, in two parts
on `typed/c4` with an audit and a fix branch between; one agent at a time,
every commit gated `0 differ` (640 objects) with both `--check`s green.

**Part 1: the struct-typed tables** (from `typed/c` 14124a24; merged as
f56ed9a8).

- **Tools.** `tools/record_sizes.py` reads each `records::` struct's size
  and fields from the headers' `static_assert`s; `data_widths.py` and
  `data_census.py` size a `records::T` entry by its struct;
  `gen_symbols.py` checks that a `records::` ctype names a struct and lets
  a `[[block]]` carry a ctype equal to its stride (an array, or one
  record when the count is 1); `typed_convert.py` converts inside record
  arrays (`Name[i].field`, one union name per family, the per-handler view
  bytes left raw with reason `view`) and grew `--addr-names`,
  `--only-entries`, `--retire-fields`; `typed_left.py` keeps a reason
  across an edit that changes only `expr`.
- **Entries.** `records::` arrays, each count checked against its readers'
  loop: Sprite_Objects `SpriteObject` x30, Sprite_ObjectsExtra x4,
  Sprite_Kind2 one object, ObjTrio `PartyMember` x3 (Field_Members retired
  into `ObjTrio[0].char_index`), Effect_Objects `EffectObject` x20,
  Prim_VertexScratch `SVector` x4, EffectKindA0_Sparks / _Shards,
  Save_Directory `SaveDirEntry` x16, and five new ones: Encounter_Slots
  0x939F00 x8, Community_Records 0x9046D0 x60, Community_Buildings 0x9048B0
  x8, BattleTask_Slots 0x93A000 x48, Battle_Enemies 0x93B960 x8. Typed
  blocks: WindowRecords `WindowRecord` x22, CharacterRecords `CharRecord`
  x8, the damage scratch block one record of 0x1C with Scratch_Swap its
  +0xC field (185 `bof3::addr::<block>` spellings became `<block>_addr`).
- **Sites.** Per table `typed_convert --gate --only-entries T
  --addr-names T`; the field entries' uses as their records' fields
  (`Window16_State` -> `WindowRecords[16].state`, 217 uses); ten field
  entries retired; BattleE3_CurrentEnemy and BattleE4_MenuActor retyped as
  `records::BattleEnemy *` / `records::PartyMember *` with their pointer
  cells' sites; BattleFlow_TaskCurrent through magic's `Pointer()` left
  (an out-of-line seam: the gate refused it). Census 19,716 -> 16,458.
- **The owner's call on the scratch block** (2026-10-20). The pass had
  replaced per-function module names (kPendingX, kPickSlot, kEffectSlot,
  kRadius, ...) with the struct's neutral fields. Kept the typing; renamed
  the block `DamageScratch` -> `SharedScratch` (the PSX scratchpad it
  relocates has no one meaning; the old name is in its note) and gave the
  names back as typed aliases in their own scope, `inline auto& kPendingX
  = SharedScratch.d_10;` (or `= SharedScratch;` for a base read at several
  fields, `at::kTint.b_01`): 70 aliases (68 scratch constants and rest_1g's
  kEff6Page / kEff6Shown), each folding to its absolute address. Census
  16,443.

**The audit** (three read-only slices, `audit/c4/{A,B,C}`): no wrong
address and no wrong size; wrong names. A: object 2's turn word in
effect_3d spelled as `Sprite_Objects[2].draw_key` against a recorded C2
decision; `.digits` (+0xA, the battle tasks' damage digits) on effect
objects and party members; Sprite_Kind2 typed past the 0xA0 bytes its
readers reach. B: Community_Buildings addresses spelled off the end of
Community_Records. C: the scratch block's address uses respelled from the
module names to `SharedScratch_addr + 0xN` (874 sites, 167 constants), the
names lost where nothing was typed; 15 raw sites the census did not see.

**The fix branch** (`typed/c4-fix`, merged as 6f863cc0): the scratch
block's address uses spelled through the module names again (872 sites; 82
retired base constants back as `kX = SharedScratch_addr;`, 40 aliases with
`kX_addr` companions for their address uses); the three wrong-meaning
field names backed off, with `typed_convert` KEEP_RAW for per-site
exclusions (effect_3d's kObject2Turn, the Community_Buildings sites);
SpriteHead +0xA a union with `digits` for BattleTask only, EffectWork
`work_3c` / `work_3e`; `records::SpriteObjectBody` (0xA0) as Sprite_Kind2's
ctype, SpriteObject deriving from it; 416 stale retire notes regenerated as
`Name[i].field` / `Name_addr + 0xN`; the census counting `AddrOf(&kX)` and
`reinterpret_cast<T*>(&kX)[i]`, the alias rule by tier, the `rv::As`
split. Census 16,443 -> 18,630 (helper-base sites such as `SW(k)` on kS
count again).

**Part 2: the census sees every raw spelling, GameData, the strict rule**
(from `typed/c` 6f863cc0).

- **What counts** (the owner's decision for the strict rule, 2026-10-20):
  `B(Name_addr + k)`, `At(Name_addr)` and every other accessor on a typed
  entry's `<Name>_addr` are raw sites, and so is a subscript of
  `rv::Bytes(...)`. `data_census.py` now counts them
  ([`typed-data-census.md`](typed-data-census.md) 4). Census 18,630 ->
  27,390: +8,459 `<Name>_addr` sites (4,975 in fuzz and harness files,
  3,484 in the game's), +301 `rv::Bytes` subscripts, none lost.
- **Every site a reason.** `analysis/typed_left.tsv` had 10,094 rows at
  `-`: 1,134 took the C2 groups' own reasons (their `left.tsv` lists, the
  import the round owed), 8,459 typed_convert's verdict on the site (a
  gated run over every `<Name>_addr` spelling for the refusals, a dry run
  for the rest: seam, harness, pointer form, helper, ...), 501 a rule by
  shape and family (`view` for the `rv::Bytes` subscripts). The gated run
  typed only two sites and wrote two, and re-spelled module constants as
  `<Entry>_addr + k` in the same pass (what the fix branch had undone), so
  its source edits were dropped and only its verdicts kept.
- **GameData** (`tools/gen_symbols.py`). `bof3::GameData`, 0x37B000 bytes
  from `bof3::kGameDataVa` 0x5C4000 (the first page past `.text`) to the
  image's end 0x93F000, 2,965 members in address order (every typed
  `[[data]]` that is not a block's field, every typed `[[block]]`;
  `pad_<address>` between; no member needed packing), its size
  `static_assert`ed; `BOF3_GAME` the one pointer to it. Each member's
  `<Name>_addr` is computed from the struct (`kGameDataVa + offsetof`) and
  `static_assert`ed equal to `symbols.toml`'s pc with the member's size,
  and the member's macro casts that `<Name>_addr`, so every spelling
  compiles and keeps its address. Macros spelled `BOF3_GAME->Name` were
  tried first: with every global one object the optimiser merged and split
  code differently (rest_4c.cpp's CommuHiLo_Close), and the gate refused
  them. Outside GameData with their literals: 4 entries inside `.text`
  and 135 that start inside an earlier member (two views of the same
  bytes, the areas' Choices / Handlers pairs).
- **The strict rule** (`data_census.py --strict`, a step in `checks.yml`
  beside the ratchet; [`typed-data-census.md`](typed-data-census.md) 3):
  a raw site in `src/game` with no reasoned row in
  `analysis/typed_left.tsv` fails, and so does a listed `src/game` key
  that no longer measures. 27,327 exceptions, the whole committed list's
  `src/game` part.

`codegen_same.py` against the baseline: 640 objects compared, 0 differ;
`data_census.py --check` 27,390, `--strict` ok; `typed_left.py --check`
current.

### The smoke run as run (2026-10-20)

Stage C's call 2, run unattended at the owner's word from `typed/c` `9eaf3e1d` (`main` `61023304` merged in): a
plain copy of the branch's `build/` (the launcher, its dev twin, `bof3x.dll`, `bof3x.exe`) in an ini-less folder,
root `C:\Users\kerok\bof3x-runs\typedc-smoke`, against the main checkout's `bof3\`. Defender stopped nothing.

- **The hosted self-test**, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' BOF3X_LANG=original`: exit 0, `inject: 10087
  ours`, `self-test only: done`, 1,086 fuzz lines every one `0 MISMATCHES` (the install-gap round's run had 1,085), no
  `FATAL`. **The exe host's**, `BOF3X_SELFTEST_ONLY=1`: exit 0, no `FATAL`.
- **The combat route with the state hash** (`BOF3X_OPT=none BOF3X_LAYERING=0`, `tools/statehash_skip.txt`, the exe
  host, the conditions of the morning's `cn_combat_a1020`): `recipe done at recipe frame 2620`, 2,561 ticks, played
  twice - the two runs identical on every tick. Against the morning's references (`cn_combat_orig3` / `orig3b`, and
  main's own `a1020`) seven pages differ, at tick 3 only: five of `Gfx_VramShadow`, the page of `Snd_BufferDesc` /
  `Music_Events`, the page of `Gfx_CurrentEnv` / `Sprite_Current`; every later tick is the same. **The control
  settles it**: `main` `b3498050`'s own build (the gate baseline, `typed-baseline-c2`) on the same route is identical
  to `typed/c`'s on all 2,561 ticks and shows the same seven tick-3 pages against `a1020`. So they are not stage C's;
  they lie between the morning's runs (main at `84e43c32`) and now - PR #98's changes or the machine's state - and are
  the tick-3 class HANDOFF's state-hash item already noted as one run in five that morning, here steady.

## 4. Stage C

Staged in [`typed-data-stage-c.md`](typed-data-stage-c.md): C0 the
consolidation, C1 the types (the shared sprite head, the renames, the views,
the new structs), C2 the sites they unlock with a family audit after, C3 the
globals' follow-ups and a naming pass, and C4, this section's old content,
built 2026-10-20 ("C4 as run" above): the typed tables as struct arrays, the
data image as one `GameData` struct generated by `tools/gen_symbols.py`
(its members' addresses computed from the struct and checked against
`symbols.toml`; the macros cast those addresses, so the machine code is
unchanged), and the census's strict rule (`data_census.py --strict` in
`checks.yml`: a raw site in `src/game` fails unless
`analysis/typed_left.tsv` lists it with a reason).

## 5. Ownership, merge order, numbering

| File or area | Owner |
|---|---|
| `tools/data_census.py`, `analysis/data_census.tsv`, the CI rule | CS |
| `src/game/records.h` | SD |
| `src/game/record_views.h` (the shared typed views, `rv::`) | C0 |
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
