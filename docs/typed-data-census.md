# The typed-data census

**Status:** IN PROGRESS (measured 2026-10-19 at `origin/main` `19180e63`;
section 5's verification fixes applied the same day).
Group CS of the typed-data round ([`typed-data-round.md`](typed-data-round.md)
section 2): the tool, its table and the CI ratchet are in; stages B and C
consume it. No `src/` change, no behaviour change, no ledger entry.

`tools/data_census.py` reads `src/` and `symbols.toml` only and writes
`analysis/data_census.tsv` - addresses, names and our own source text, no
game data (CLAUDE.md rule 1; the one file of `analysis/` that `.gitignore`
lets through). Re-run it after any change to `src/` that touches a data
access:

    python tools/data_census.py            # writes the table, prints the counts
    python tools/data_census.py --counts   # the counts only
    python tools/data_census.py --check    # CI: fails when the site count rose

## 1. The counts, re-measured 2026-10-19

These replace section 0 of the round doc, whose numbers were `grep`
estimates. Where they differ, the reason is in the last column. The first
version of the census (`ab9fbbbe`) counted 39,503 sites; an independent
verification found whole classes it dropped (section 5), and the numbers here
are after those fixes.

| Shape | Round doc (2026-10-17) | Census (2026-10-19) | Why it differs |
|---|---:|---:|---|
| `at` constants defined in `*_callees.h` | 6,916 | **7,453** (7,659 in all files) | the census counts every constant in a `namespace at`, including each of several per declaration (`kA = .., kB = ..`) and brace-initialised ones (`kX{0x...}`) |
| distinct (name, value) pairs | - | 4,858 | the same name with the same value in several modules counts once |
| ... of them `.data` / `.rdata` addresses | - | 3,738 | |
| ... code addresses (in `.text`) | - | 388 (331 distinct) | callee and handler addresses, kept in the map, not counted as data sites |
| ... plain values (flags, counts, strides) | - | 732 | |
| distinct data addresses behind the `at` constants | 2,981 (all addresses) | **2,547** data + 331 code | the round doc's figure mixed code and data addresses |
| module constants outside any `namespace at` whose value is a data address | (in the 6,916's rule, not counted) | **2,019** definitions in 223 files; 1,333 (name, value) pairs over 1,060 distinct addresses | file-scope, module-namespace and function-local `constexpr kX = 0x...` (`msgbox_callees.h`, `scena_sc15.cpp`, `rest_4e.cpp`, ...); the round's "one name per address" rule covers them too |
| ... addresses no `at` constant names | - | **675** (332 with no `[[data]]` entry, 331 with no `symbols.toml` name) | |
| all distinct data addresses behind a constant | - | 3,222 (1,939 with no `symbols.toml` name) | the two maps together |
| most names for one address | "up to a dozen" | 25 (`0x903850`, the `SharedScratch` block); 530 addresses carry two or more | `at` constants only |
| global sites: an `at::` reference or a module-constant reference to a data address | about 900 | **29,048** | 23,314 `at::` references (every reference, whatever wraps it: `B(` 7,796, `At(` 4,939, `Mem(` 2,614, a bare reference 2,913, ...) and 5,734 module-constant references in an accessor's address argument (`W(kScriptPool + 2 * index)`, `B(kShakeShift)`; `scena_sc15` 500, `msgbox` 356, `scena_sc11` 321, `rest_2b` 310, `rest_2e` 295) |
| record sites: accessor on a pointer plus an offset | about 6,700 in 275 files | **20,067** in 484 files | the accessor vocabulary below, bare or namespace-qualified (`SetWord` 3,458, `SetLong` 3,155, `Long` 3,070, `Word` 2,934, `SetUL` / `UL` 2,822, ...); 14,109 on a local, 3,481 on a helper's result (`S() + 0x3C`), 1,546 an offset into a fixed record (`SB(4)` reading `kS + 4`), 931 on a `symbols.toml` pointer (`Sprite_Current + 0x38`) |
| raw-address sites: accessor on a literal | - | **1,828** | `B(0x8031D8)`, `PutByte(0x803430, 0)`; `rest_2e` 530, `field_s` 328, `rest_2d` 140, `shop_states2` 133 |
| **sites in all** (the CI number) | - | **50,943** in 528 files | 11,011 of them in fuzz harnesses; 50,863 in `src/game`, 34 `src/render`, 30 `src/input`, 16 `src/hook` |
| `k...Stride` definitions | 12 names for 8 strides | 441 definitions, 92 names, 34 values | every module's own names counted; `0xA4` alone is `kObjectStride`, `kSpriteStride`, `kCharStride`, `kRecordStride`, `kActorStride`, `kExtraStride`, `kStride`, `kMemberStride` |
| `static_assert(offsetof / sizeof ...)` | 65 | 70 | |
| `[[data]]` in `symbols.toml` | 2,696 (2,610 `ctype`, 2,407 `count`) | 2,696 (2,610, 2,407) | unchanged |

What the census adds that the round doc could not:

| Coverage | Count |
|---|---:|
| global sites whose address is a `[[data]]` entry's `pc` (`exact`) | 6,831 |
| global sites inside an entry's extent, `sizeof(ctype) * count` (`inside`) | 3,871 |
| global sites no entry covers (`none`) | 18,338 |
| global sites whose constant is ambiguous between modules | 8 |
| references to struct / array constants (`kWm87.box_states`, `kBannerStates[0]`) - not sites | 264 |
| distinct `at` data addresses with a `[[data]]` name | 933 (36 of them without a `ctype`) |
| ... with a `[[block]]` name | 6 |
| **... with no `symbols.toml` name (names missing)** | **1,608** (302 of them inside an entry's extent); 1,939 with the module constants' 331 |

Record sites by what the census could learn of the pointer, from its last
assignment in the same function (or, for a helper call or a fixed record, from
the argument itself):

| Family evidence | Sites |
|---|---:|
| made by a helper (`via=S` 2,572, `via=Sc` 2,203, `via=Cur` 1,126, `via=Owner` 399, `via=Enemy` 292, ...) | 10,708 (3,481 of them the call itself in the argument, `Long(Owner() + 0x38)`) |
| indexes an `at::` table (`table=0x...`), a `symbols.toml` name or a fixed record's constant (`base=...`; `Sprite_Current` 2,159, `kS` 1,546, `Gfx_PacketNext` 980, `Prim_VertexScratch` 139) | 6,382 |
| a stride seen (`* 0xA4`, `* kObjectStride`, `p += 0x84`) | 870 |
| a function parameter | 792 |
| nothing found | 1,903 |

The rows sum to 20,655, not 20,067: the stride row overlaps the helper row by
68 and the table / base row by 520 (588 sites carry two families). The helper
and table / base rows never overlap - a helper is recorded only when no
table or base was found. The table's `family` column holds every family
found, joined with `;` (`table=0x802000;stride=0xa4`).

The strides seen most: `0x28` 184, `0xA4` 146, `0x4` 83, `0x84` 48, `0x8`
43, `0x14C` 38. The helper names are the better key for stage B: `S` (2,572
sites), `Sc` (2,203) and most `Cur` (1,126) are one-line helpers returning
`Sprite_Current` (124 `Sc` / `S` definitions and 16 of the `Cur` ones read
so; a few `Cur` return another module pointer), so one typed helper per
module converts hundreds of sites. The 26 magic modules' `SB` / `SW` / `SD`
and their `Set` forms (`kS + k`, `kS = 0x903850`) are one more such helper.

## 2. The table

`analysis/data_census.tsv`, tab-separated, two tables under `#` header
lines, then `#count` lines (the counts above, which `--check` reads):

- `site` rows: `file`, `line`, `shape` (`global` / `record` / `raw`),
  `accessor` (the call wrapping it, `-` for a bare reference), `expr` (the
  constant's name - `at::kX`, `mh::at::kX` or a module constant's `kX` - or the
  argument, first 60 characters), `address` (`ambiguous` for the eight; for a
  record, the fixed record's address plus a literal offset, `0x903854` for
  `SB(4)`, else `-`), `family` (records: `via=`, `table=`, `base=`,
  `stride=`, `param`, `-`, joined with `;`), `cover` (`exact` / `inside` /
  `none`), `data_name` (the entry, `Name+0x..` when inside), `ctype`, `fuzz`
  (1 in a fuzz harness or harness file).
- `const` rows: `name`, `value`, `kind` (`data` / `code` / `value` /
  `unknown`), `n_modules`, `modules` (`file:line(namespace)` for every
  definition of that name with that value; `-` for file scope), `symbol` (the
  `symbols.toml` name at the value, `-` for none), `symbol_kind` (`data` /
  `block` / `func`), `cover`, `ctype`, `scope` (`at`: every constant of a
  `namespace at`; `module`: a constant outside one, listed only when its
  value is a data address).

SG reads the `const` rows with `kind = data` and `symbol = -` (1,608 of scope
`at`; with scope `module`, 331 more addresses); stage B reads the `site`
rows of its own files; the two together answer which module constants are
retirable (the address has a `[[data]]` entry with a `ctype`).

## 3. The CI rule

`.github/workflows/checks.yml`, job "Ledgers and symbols", step "The
typed-data census's raw site count may not rise": `python
tools/data_census.py --check` measures today's sites and fails when there are
more than the committed table's `#count sites` line (50,943). A change that
removes sites passes without touching the table; regenerate and commit the
table to lower the ceiling (stage B's groups do so with each merge). A change
that adds a raw access fails: use the `symbols.toml` name, or a typed
record, instead. The rule only counts; the table's line numbers go stale with
any edit and nothing compares them. The round doc's later rule (a raw
accessor in `src/game` fails outright, after stage B) is not built.

Checked 2026-10-19: one added `inline int CensusProbe() { return B(kMode) +
B(kStep); }` in `rest_4e.cpp` (file-scope `kMode = 0x939A3E`) fails with
50,945 against 50,943; one added `SB(4)` in `magic_s22.cpp` fails with 50,944.
Before section 5's fixes the first passed.

## 4. How a site is found, and what it misses

- **Global**:
  - every `at::kX` / `alias::at::kX` reference outside the `at` namespaces
    whose constant's value is in the image and outside `.text`
    (`meta.text_va`, `text_size`). The constant is resolved as the C++ would
    see it, in order: the explicit namespace or the file's `namespace at =
    ...` alias, the file itself, its fuzz twin's module (`x_fuzz.cpp` reads
    `x_callees.h`), its local includes, the enclosing namespace, then any
    unique definition. Values are evaluated from literals, other constants
    and `bof3::addr::` names (every `[[data]]`, `[[func]]` and `[[block]]`
    name).
  - every module constant (a `k` name not reached through `at::`) in an
    accessor's address argument whose value is a data address. The value is
    the file's own definition, else the first in its include closure, else
    the one value `src/` gives the name. A constant reached through a
    differently-named alias of an `at` namespace (`hat::kTarget` in
    `magic_s23.cpp`, 17 sites) is counted here.
  - One site per argument: a data constant inside a nested accessor
    (`Word(Mem(kS) + 4)`'s `Mem(kS)`) is that accessor's site, and the outer
    call is classified by what is left (here a record, `via=Mem`).
- **Record**: a call to one of the accessors below, bare or
  namespace-qualified (`move_script::SetLong(`, `sh::Mem(`), whose first
  argument is
  - a lower-case local, member or parameter `+` or `-` an expression,
    including one whose offset is an `at::` field constant
    (`Word(r + at::kStatus)`);
  - a helper's result `+` or `-` one (`S() + 0x3C`, `Owner() + 0x3C`;
    `family = via=<helper>`);
  - a `symbols.toml` data name, bare or as `bof3::addr::Name`, `+` or `-`
    one (`Sprite_Current + 0x38`: the pointer is typed, the field is not);
  - an offset into a fixed record, through a module's offset accessor:
    `SB(unsigned k) { return Mem(kS + k)[0]; }` makes `SB(4)` a record site
    with `family = base=kS` and address `kS + 4`. Any accessor defined on an
    integer parameter `k` with `(kX + k)` in its body, `kX` a data address,
    is read so: the 26 magic modules' `SB`, `SW`, `SD`, `SetSW`, `SetSD`.
  The vocabulary: `B W D L Byte Word Dword Long S8 S16 S32 U8 U16 U32 UL`,
  their `Set` forms, `Mem At Ptr PtrAt SetPtr Pointer SetPointer`,
  `PutByte PutWord PutLong GetByte GetWord GetLong`,
  `SB SW SD Short SetSB SetSW SetSD`. Per file, a name the
  file or a header it includes defines as a value cast (no memory access in
  the body, `Byte(v) { return static_cast<unsigned char>(v); }`,
  `field_misc.cpp`'s `Short(U v)`) is not an accessor there; `S8`..`U32` are
  casts unless a definition says otherwise.
- **Raw**: the same accessors with a first argument starting with a literal
  of six to eight hex digits inside the image and outside `.text`.

"Data address" is the image less `.text`, so the PE header's base counts: four
module constants (`kImageBase`, `kExeLo`, `kImageLo` = `0x400000`, and
`statehash.cpp`'s `kEnd`) are in the map, and one site (`exe_host.cpp`'s
`At(kImageBase)`) is a global.

Not counted, by design or as a known gap:

- `Word(p)` with no offset: in many files the same spelling is a value cast,
  so syntax cannot tell; the offset-0 field reads are under-counted.
- The generic helpers some modules use (`Put`, `Get`, `M`, `P`, `Entry`,
  `Text`, `Jump`, `Vertex`): their meaning differs by file. Their `at::`
  arguments are counted as global sites; their record and raw uses are not.
- A module-constant reference outside an accessor's address argument (passed
  to a function, stored in a table): unlike `at::` references, only the
  accessor-wrapped ones are sites.
- `B(bof3::addr::Name)` with no offset: about 114 by `grep` (an accessor
  wrapping `addr::Name` alone). The name is typed; the access is not.
- A `k` name with no constant value - a constant initialised at run time
  (`const U kHeader = at::AreaMap_HeaderAt();`) or a local pointer that
  happens to be called `k2` (`move_cmds.cpp`): 46 accessor calls start with
  one; those with an offset are counted as records (`family` from the name's
  assignment, mostly `-`), the 17 bare ones are not.
- Raw data-address literals outside an accessor call (a literal in an
  initialiser table or a fuzz harness's region list).
- An accessor's address argument built from a data constant with an
  index (`W(at::kAbilityRows + ability * 24)`, `W(kScriptPool + 2 * index)`)
  is one global site, not a record site; the `expr` column shows the constant
  only.
- Record sites in our own platform code (`sound.cpp`'s `WAVEFORMATEX`,
  `src/render`) are syntactically the same and are counted; their files say
  so. Fuzz-harness buffers (`g_cell + i`) likewise, with `fuzz = 1`.
- Inline-asm `"m"` operands (round doc section 7).

## 5. The verification and its fixes (2026-10-19)

An independent recount of `ab9fbbbe` (its own counter, comparison scripts
and patched copies of the tool, report outside the repo) agreed with the
census on what it counted (99.6% of accessor-wrapped `at::` sites row for
row, 40 / 40 site rows and 20 / 20 const rows correct) but found classes it
dropped and the ratchet could not see. Each was confirmed against `src/`
and fixed; the deltas are rows new in the regenerated table, matched against
the old one by (file, line, shape, accessor, expr) - none of the 39,503 old
rows was lost.

| Item | What was wrong | Fix | Sites |
|---|---|---|---:|
| D1 | accessor sites on a module constant (`msgbox.cpp` had 0 rows, `rest_4e.cpp` 1) were dropped; `B(kMode)` added to `rest_4e.cpp` passed `--check` | resolved through the file's include closure, counted as `global` | +5,751 (5,734 module, 17 through an `at` alias) |
| D2 | the constant map held `namespace at` only | data-valued module constants added with `scope = module` | 1,333 const rows; 675 addresses new to the map |
| D3 | a helper's result plus an offset (`S() + 0x3C`) was dropped | `record`, `family = via=<helper>` | +3,285 |
| D4 | `SB SW SD Short SetSB SetSW SetSD` were not accessors | added, with the per-file cast test; the offset form counted into the fixed record | +1,863 (1,546 offset form) |
| D5 | any `at::` in the argument dropped the site, a field offset too (`Word(r + at::kStatus)`) | only a data-valued `at::` outside a nested accessor defers to the globals loop | +416 records, +14 raw (`0x802000 + at::kSpriteStride * n`) |
| D6 | `bof3::addr::Name + off` did not match | `GLOBAL_OFF` takes `addr::` | +83 |
| D7 | named strides resolved by a src/-wide single value (`kObjectStride` is `0xA4` in 25 modules, `0x14C` in one) and suffixed literals (`12u`) read as names | the include-closure lookup; suffixes stripped | old rows with a stride 540 -> 824 (870 with the new rows) |
| D8 | the family table's overlap and the `;` join were not stated | stated above; the 326-site overlap of the first version was all stride rows, none between helper and table | - |
| - | a `k` name with no constant value leading the argument (`kHeader + i`, the local `k2 + 0x34`) was dropped as a constant | `record` | +28 |

Sites: 39,503 -> 50,943 (global 23,297 -> 29,048, record 14,392 -> 20,067,
raw 1,814 -> 1,828; fuzz 9,259 -> 11,011; files 517 -> 528). The verifier
estimated 49,000 to 50,500; the remainder is the nested-constant rule above
(a record outside a nested data constant is now counted, where the first
version dropped any argument holding an `at::` data constant).

Spot check of the new rows: 20 drawn at random (seed 20261019) from the 11,440
rows not in the old table - 9 module-constant globals, 9 helper records, 2
`SetSW` offset records - each read against its source line and its
constant's definition: 20 / 20 correct (file, line, shape, accessor,
constant, address).

## 6. After the stage-A merge (2026-10-19)

Section 1 measures `origin/main`'s `symbols.toml`. On `typed/integration`
(SG's corrected `[[data]]` entries, the second pairing pass, SD's
`records.h`, this census, merged in that order) the table was regenerated.
The site count is unchanged; only coverage moves:

| Count | Section 1 | After the merge |
|---|---:|---:|
| sites | 50,943 | 50,943 |
| global sites covered exactly by a `[[data]]` entry | 6,831 | 16,854 |
| global sites uncovered | 18,338 | 8,315 |
| distinct `at` data addresses with a `[[data]]` name | 933 | 1,384 |
| distinct `at` data addresses with no name | 1,608 | 1,157 |
| module-only addresses with no `[[data]]` entry | 332 | 295 |
| all distinct data addresses with no name | 1,939 | 1,451 |
| `static_assert` on `offsetof` / `sizeof` | 70 | 342 |
| `symbols.toml` `[[data]]` entries / with a `ctype` | 2,696 / 2,610 | 3,184 / 3,099 |

The 295 module-only addresses with no entry were outside SG's scan (it read
the `at` constants only); they are a follow-up for the next globals pass.
