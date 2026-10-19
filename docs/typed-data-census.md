# The typed-data census

**Status:** IN PROGRESS (verified 2026-10-19 at `origin/main` `19180e63`).
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
estimates. Where they differ, the reason is in the last column.

| Shape | Round doc (2026-10-17) | Census (2026-10-19) | Why it differs |
|---|---:|---:|---|
| `at` constants defined in `*_callees.h` | 6,916 | **7,453** (7,659 in all files) | the census counts every constant in a `namespace at`, including each of several per declaration (`kA = .., kB = ..`) and brace-initialised ones (`kX{0x...}`) |
| distinct (name, value) pairs | - | 4,858 | the same name with the same value in several modules counts once |
| ... of them `.data` / `.rdata` addresses | - | 3,738 | |
| ... code addresses (in `.text`) | - | 388 (331 distinct) | callee and handler addresses, kept in the map, not counted as data sites |
| ... plain values (flags, counts, strides) | - | 732 | |
| distinct data addresses behind the constants | 2,981 (all addresses) | **2,547** data + 331 code | the round doc's figure mixed code and data addresses |
| most names for one address | "up to a dozen" | 25 (`0x903850`, the `DamageScratch` block); 530 addresses carry two or more | |
| global sites: a reference to an `at::k` data constant | about 900 | **23,297** | the estimate counted `B(`, `W(`, `Address(` only; the census counts every reference, whatever wraps it (`B(` 7,796, `At(` 4,939, `Mem(` 2,614, a bare reference 2,913, `Byte(` 701, `W(` 404, ...) |
| record sites: accessor on a pointer plus an offset | about 6,700 in 275 files | **14,392** in 473 files | the accessor vocabulary below, bare or namespace-qualified (`SetWord` 2,783, `Long` 2,481, `Word` 2,317, `SetLong` 2,247, `SetUL` / `UL` 2,299, ...); 13,545 on a local, 847 on a `symbols.toml` pointer (`Sprite_Current + 0x38`) |
| raw-address sites: accessor on a literal | - | **1,814** | `B(0x8031D8)`, `PutByte(0x803430, 0)`; `rest_2e` 530, `field_s` 328, `rest_2d` 140, `shop_states2` 131 |
| **sites in all** (the CI number) | - | **39,503** in 517 files | 9,259 of them in fuzz harnesses; 39,434 in `src/game`, 32 `src/render`, 30 `src/input`, 7 `src/hook` |
| `k...Stride` definitions | 12 names for 8 strides | 441 definitions, 92 names, 34 values | every module's own names counted; `0xA4` alone is `kObjectStride`, `kSpriteStride`, `kCharStride`, `kRecordStride`, `kActorStride`, `kExtraStride`, `kStride`, `kMemberStride` |
| `static_assert(offsetof / sizeof ...)` | 65 | 70 | |
| `[[data]]` in `symbols.toml` | 2,696 (2,610 `ctype`, 2,407 `count`) | 2,696 (2,610, 2,407) | unchanged |

What the census adds that the round doc could not:

| Coverage | Count |
|---|---:|
| global sites whose address is a `[[data]]` entry's `pc` (`exact`) | 5,044 |
| global sites inside an entry's extent, `sizeof(ctype) * count` (`inside`) | 3,118 |
| global sites no entry covers (`none`) | 15,127 |
| global sites whose constant is ambiguous between modules | 8 |
| references to struct / array constants (`kWm87.box_states`, `kBannerStates[0]`) - not sites | 264 |
| distinct data addresses with a `[[data]]` name | 933 (36 of them without a `ctype`) |
| ... with a `[[block]]` name | 6 |
| **... with no `symbols.toml` name (names missing)** | **1,608** (302 of them inside an entry's extent) |

Record sites by what the census could learn of the pointer, from its last
assignment in the same function:

| Family evidence | Sites |
|---|---:|
| made by a helper (`via=Sc`, `via=Cur`, `via=Party`, ...) | 6,925 |
| indexes an `at::` table (`table=0x...`) or a `symbols.toml` name (`base=...`; `Sprite_Current` 2,159, `Gfx_PacketNext` 976, `Prim_VertexScratch` 139) | 4,676 |
| a stride seen (`* 0xA4`, `* kObjectStride`, `p += 0x84`; overlaps the rows above) | 540 |
| a function parameter | 745 |
| nothing found | 1,832 |

The strides seen most: `0x28` 100, `0xA4` 89, `0x4` 56, `0x84` 43, `0x14C`
35, `0x1C` 31. The helper names are the better key for stage B: `S` (1,734
sites), `Sc` (1,238) and most `Cur` (676) are one-line helpers returning
`Sprite_Current` (124 `Sc` / `S` definitions and 16 of the `Cur` ones read
so; a few `Cur` return another module pointer), so one typed helper per
module converts hundreds of sites.

## 2. The table

`analysis/data_census.tsv`, tab-separated, two tables under `#` header
lines, then `#count` lines (the counts above, which `--check` reads):

- `site` rows: `file`, `line`, `shape` (`global` / `record` / `raw`),
  `accessor` (the call wrapping it, `-` for a bare reference), `expr` (the
  constant's name, or the argument, first 60 characters), `address`
  (`ambiguous` for the eight), `family` (records: `via=`, `table=`, `base=`,
  `stride=`, `param`, `-`), `cover` (`exact` / `inside` / `none`),
  `data_name` (the entry, `Name+0x..` when inside), `ctype`, `fuzz` (1 in a
  fuzz harness or harness file).
- `const` rows: `name`, `value`, `kind` (`data` / `code` / `value` /
  `unknown`), `n_modules`, `modules` (`file:line(namespace)` for every
  definition of that name with that value), `symbol` (the `symbols.toml`
  name at the value, `-` for none), `symbol_kind` (`data` / `block` /
  `func`), `cover`, `ctype`.

SG reads the `const` rows with `kind = data` and `symbol = -` (the 1,608);
stage B reads the `site` rows of its own files; the two together answer
which module constants are retirable (the address has a `[[data]]` entry
with a `ctype`).

## 3. The CI rule

`.github/workflows/checks.yml`, job "Ledgers and symbols", step "The
typed-data census's raw site count may not rise": `python
tools/data_census.py --check` measures today's sites and fails when there are
more than the committed table's `#count sites` line (39,503). A change that
removes sites passes without touching the table; regenerate and commit the
table to lower the ceiling (stage B's groups do so with each merge). A change
that adds a raw access fails: use the `symbols.toml` name, or a typed
record, instead. The rule only counts; the table's line numbers go stale with
any edit and nothing compares them. The round doc's later rule (a raw
accessor in `src/game` fails outright, after stage B) is not built.

## 4. How a site is found, and what it misses

- **Global**: every `at::kX` / `alias::at::kX` reference outside the `at`
  namespaces whose constant's value is in the image and outside `.text`
  (`meta.text_va`, `text_size`). The constant is resolved as the C++ would
  see it, in order: the explicit namespace or the file's `namespace at =
  ...` alias, the file itself, its fuzz twin's module (`x_fuzz.cpp` reads
  `x_callees.h`), its local includes, the enclosing namespace, then any
  unique definition. Values are evaluated from literals, other constants and
  `bof3::addr::` names (every `[[data]]`, `[[func]]` and `[[block]]` name).
- **Record**: a call to one of the accessors below, bare or
  namespace-qualified (`move_script::SetLong(`, `sh::Mem(`), whose first
  argument is a lower-case local, member or parameter `+` or `-` an
  expression, or a `symbols.toml` data name `+` or `-` one
  (`Sprite_Current + 0x38`: the pointer is typed, the field is not).
  The vocabulary: `B W D L Byte Word Dword Long S8 S16 S32 U8 U16 U32 UL`,
  their `Set` forms, `Mem At Ptr PtrAt SetPtr Pointer SetPointer`,
  `PutByte PutWord PutLong GetByte GetWord GetLong`. Per file, a name the
  file or a header it includes defines as a value cast (no memory access in
  the body, `Byte(v) { return static_cast<unsigned char>(v); }`) is not an
  accessor there; `S8`..`U32` are casts unless a definition says otherwise.
- **Raw**: the same accessors with a first argument starting with a literal
  of six to eight hex digits inside the image and outside `.text`.

Not counted, by design or as a known gap:

- `Word(p)` with no offset: in many files the same spelling is a value cast,
  so syntax cannot tell; the offset-0 field reads are under-counted.
- The generic helpers some modules use (`Put`, `Get`, `M`, `P`, `Entry`,
  `Text`, `Jump`, `Vertex`): their meaning differs by file. Their `at::`
  arguments are counted as global sites; their record and raw uses are not.
- Raw data-address literals outside an accessor call (a literal in an
  initialiser table or a fuzz harness's region list).
- An accessor's address argument built from an `at::` constant with an
  index (`W(at::kAbilityRows + ability * 24)`) is one global site, not a
  record site; the `expr` column shows the constant only.
- Record sites in our own platform code (`sound.cpp`'s `WAVEFORMATEX`,
  `src/render`) are syntactically the same and are counted; their files say
  so. Fuzz-harness buffers (`g_cell + i`) likewise, with `fuzz = 1`.
- Inline-asm `"m"` operands (round doc section 7).
