# The image-layer round: the exe's tables as one file per table

**Status:** STAGED 2026-11-11 (IDEAS I58, its second step; nothing of the round
built). Section 1's numbers were measured on `main` `7a495bd6`; everything
else is a plan, and section 2 lists what it does not yet know.

The owner (2026-11-06, I58): move the tables the PC port holds in `BOF3.exe`
back into data files, so mods that edit them can be converted and the source
data looks closer to the PlayStation's. The owner (2026-11-11): the files need
not be EMI-shaped; the point is that a table is **a readable file outside the
exe, in the image layer**, and **one file per table** "makes the most sense,
and would be most human readable".

No table value appears in this note (rule 1). The table files this round makes
are built on the player's machine from the player's own copies and live in the
cache, never in the repository.

## 0. The shape in one paragraph

Every table [`tables.toml`](../tables.toml) names becomes a file of its own in
the cache: `base\exe\tables\<key>.bin`, the table's records in the PC's stride.
The image `base\exe\data.bin` stays the address space (pointers, the engine's
own bytes, everything not yet named) and no longer holds those tables' bytes;
the exe host lays each table file at its table's address at map time. An
optional layer, and so a mod, carries the same thing: `opt\<layer>\exe\` with
one file per table it changes and a list of the cells that are its own. One new
mod kind, `table-edit`, carries cell edits as text (table, record, field, new
value) and builds such a layer. The one image layer that exists today,
`psp-tables` (DIV-0111), moves to this form. Whatever `.data` holds that is not
yet a named table keeps travelling as byte ranges, as today.

## 1. What exists, measured 2026-11-11

- **The image.** The exe host maps the cache's `base\exe\data.bin` as `.data`
  where there is no `BOF3.exe` (`BOF3X_EXEIMAGE`, [`exe-import.md`](exe-import.md),
  [`disc-only-install.md`](disc-only-install.md)). `data.toml` classes every
  range by `how`; `src/hook/exe_image.cpp` `kClasses` says which classes must
  equal the PC's (`exe`, `rebuilt`, `layout`, `own`, `keys`, `none`, `pointer`)
  and which may differ (`map`, `place`, `table`, `widen`, `language`, `unread`,
  `gap`).
- **One image layer, of one shape.** `ExeImage_LayOptTables` lays
  `opt\<layer>\exe\place.bin` over `.data` at the ranges of `place.toml`, for
  each layer `BOF3X_OPT` names, in that order, after the image is filled and
  before it is compared with the file. It refuses a wrong sha256, a range
  outside `.data`, ranges out of order, a byte count that differs, and a range
  on a must-equal class (its self-test has those five controls). The only
  writer is `tools/psp_layers.py` `build_exe`, for `psp-tables`: the records of
  `exp_table` and `consumables`, a record's name field left out
  (`exe_ranges`). The loader takes any layer name already; what is fixed is
  the file's shape, one blob and a range list, and that nothing but
  `psp_layers.py` writes one.
- **The hosted build refuses an `exe` layer** (`dat_load.cpp` `ReadOptLayers`):
  its `.data` is Capcom's, the reference. That stays.
- **The catalogue.** `tables.toml` has 31 tables, 30 with a PC symbol:
  **23,722 bytes** of the 638,976 bytes of initialised `.data`
  (`sum(count * stride)` over the tables with a `symbol`; a ten-line
  `tomllib` read, 2026-11-11). No field is typed as a pointer; the types in
  use are `u8 s8 u16 s16 u32 s32`. `tools/tables.py dump` reads any of them
  from the exe or a disc, to the terminal or CSV.
- **The names beyond the catalogue.** `symbols.toml` has 3,250 `[[data]]`
  entries with `ctype` and `count`, and none with a size. How much of `.data`
  they cover is not measured (section 2, M2).
- **Where the rest came from** (I58's table, [`IDEAS.md`](IDEAS.md)): 493,349
  bytes from overlay sections the port dropped, 13,056 from EMIs with no PC
  container, 12,968 from the boot EXE, about 119,600 the port's own.
  `exe_maps/<build>.tsv` places 81 % of `.data` on each disc
  ([`exe-tables-by-build.md`](exe-tables-by-build.md)).
- **Names are not the tables'.** The image's name fields are blank (`widen`);
  a language layer's handlers write them (DIV-0008). A table file carries no
  name.

## 2. What is not measured, and first in the round (group M, read-only)

1. **Each catalogued table's class in every build's image.** For the 30 tables
   and the seven disc builds plus the PC-built image: the `how` of every byte
   of the table in `data.toml`. Expected `table` or `widen` throughout. A table
   on a must-equal class cannot be a file a layer changes; say which and why.
   Three to look at first: `area_descriptors` (200 x 4; I58's entrance task
   says it "is all pointer words and is filled from no disc"), `songs`
   (165 x 4), `key_defaults` (the `keys` class is `Shell_MapDefaultKeys`'s).
2. **What `symbols.toml` already names.** Bytes of `.data` under a `[[data]]`
   entry whose `ctype` and `count` give a size, by class, and how many of those
   bytes lie in `map` or `place` ranges (disc content) and not in a catalogued
   table. That number is how far "one file per table" can go without new
   reading, and it ranks what to catalogue next.
3. **Tables no file can hold as they are.** A record with a pointer word in it
   (found by `recipes/exe-pointers.tsv` against each table's range, since no
   field is typed as one), a table whose length another table's address
   depends on (the five item tables chain in `.data`, `tables.toml` says), a
   table a taken-over function indexes past its count.
4. **The cost at start.** Thirty small reads and hashes against one. Measure
   the exe host's time to the first frame before and after; the layer check
   today hashes one file.
5. **A PC-built cache.** `base\exe\` written from `BOF3.exe`: the same table
   files come out, byte for byte, as from the JP disc, apart from the cells
   [`exe-import.md`](exe-import.md) already lists as differing by build.
   Confirm, or list.
6. **`.rdata`.** The exe host fills `.rdata` from the engine
   (`rdata::Map`, the disc-only round). Is any game table there? Expected none.

M decides the list of tables that become files in this round. A table that
fails 1 or 3 stays in the image with its reason written in `tables.toml`.

### M1 to M3 as run (2026-11-11; M4, M5 and M6 not run)

Read from the seven disc-built images of the exe-gap round's end batch
(`C:\Users\kerok\bof3x-runs\eg1031\end\c_<build>\base\exe\data.toml`, generated
2026-10-31, one per disc build) against this branch's `tables.toml` and
`symbols.toml`; the two scripts are `bof3x-runs\il1111\m.py` and `m2.py`. The
images are eleven days old; group B runs M1 again on fresh ones.

**M1. 27 of the 30 tables are `table` or `widen` in all seven images, whole.**
The six item and ability tables are `widen` (13,156 bytes), 21 are `table`.
Three are not file tables:

| Table | Bytes | Class | Why it stays in the image |
|---|---|---|---|
| `area_descriptors` | 800 | `layout` in five builds; `layout` + `rebuilt` in two | all pointer words, the PC's layout; must equal |
| `key_defaults` | 128 | `keys` in all seven | `Shell_MapDefaultKeys` writes it; must equal |
| `sin_table` | 2,050 | `table` in five builds, `own` in two | a library table the engine fills where no disc places it; not game content |

So the round's file tables are **27 tables, 20,744 bytes** (23,722 less the
three). Which two builds differ for `area_descriptors` and for `sin_table` is
in the script's output and was not written down.

**M2. `symbols.toml` names 55,200 bytes of `.data` outside the catalogue.** Of
its 3,250 `[[data]]` entries, 2,457 are in initialised `.data`; 36 are
pointer-typed and 72 have a `ctype` with no simple size (records, or none),
and were left out. The rest cover 58,012 bytes, 55,200 of them outside every
catalogued table, in 2,102 arrays. By the US image's classes:

| Class | Bytes named outside the catalogue | What it means for a file |
|---|---|---|
| `map` + `place` | 20,770 | disc content with a name and a length: can become a file table once `tables.toml` has its stride and count |
| `layout` + `rebuilt` | 33,851 | pointer words (handler tables, lists of addresses): the engine's, never a file |
| `none`, `language`, `own`, `unread`, `gap` | 579 | not disc content |

The whole US image by class, for scale: `map` 468,248, `place` 9,539,
`widen` 13,156, `table` 9,638, `layout` 46,652, `rebuilt` 28,672, `unread`
37,270, `own` 18,918, `none` 3,321, `language` 2,817, `gap` 617, `keys` 128.
**About 457,000 bytes of disc content (`map` + `place` less the 20,770) have
no name yet.** That is section 8's cataloguing work, and it is most of the
image; this round makes the form, not the catalogue.

**M3, in part.** M1 answers the pointer half: no file table has a byte on a
pointer class (`pointer`, `rebuilt`, `layout`). The item tables' chain and
readers that index past a count are not checked.

## 3. The files

### 3.1 In the base

```
cache\base\exe\data.bin            the image, as today, zero where a table file is laid
cache\base\exe\data.toml           as today; the table ranges get the class `file`
cache\base\exe\tables\<key>.bin    one per tables.toml key: count * stride bytes, the PC's layout
cache\base\exe\tables\tables.toml  per file: key, address, count, stride, sha256, the build it came from
```

- A table file is the table's records in **the PC's stride**, so a field's
  `at` in `tables.toml` is its offset in the file's record, and `tables.py`
  reads the file with the reader it has. The 16-byte name field is in the
  record and zero; the loader does not lay it (the rule `exe_ranges` has).
- `file` is a new class in `kClasses`, not must-equal: zero in `data.bin`,
  filled from the table file. A table file that is missing, short, or whose
  sha256 is not `tables.toml`'s is a Fatal that names the file.
- The address is not in the file. It comes from `symbols.toml` through the
  generated binding, as every other address the engine uses; the copy in the
  cache's `tables.toml` is for the check that both agree.

### 3.2 In a layer

```
cache\opt\<layer>\exe\tables.toml  per table: key, file, sha256, and `cells` - the ranges that are the layer's
cache\opt\<layer>\exe\<key>.bin    the whole table, the layer's values in its cells
```

- A layer's file is a whole table so that it reads the same way the base's
  does. Only the bytes in `cells` are laid; the rest of the file is the base
  it was built over and is not trusted to be current.
- `cells` is `[record, at, length]` rows, not addresses, so a layer survives a
  table moving.
- Bytes outside every catalogued table stay in today's form (`place.toml` +
  `place.bin`, address ranges). A layer may have both parts.
- `psp-tables` is rebuilt in this form by `psp_layers.py`. A cache holding the
  old form is refused with one sentence that says to run the build again; no
  second reader is kept.

### 3.3 The order and the conflict rule

At map time: the image, the engine's fills, the base's table files, then each
layer in `BOF3X_OPT`'s order (its tables, then its raw ranges), then the
compare with the file where there is one. When two layers write one byte with
different values, the later layer wins and the exe host logs one line per
table with both layers' names and the cell count (the owner, section 7 call 3). A cell on a
must-equal class is refused, as now.

## 4. The mod kind

```toml
[mod]
id = "example"
name = "..."
version = "1.0"
author = "..."
kind = "table-edit"

[[set]]
table = "weapons"        # a tables.toml key
record = 12
field = "attack"         # a tables.toml field name, or `at = 0x11, type = "u8"` for an unnamed byte
value = 40
```

- The zip holds only the modder's own numbers. It holds no table and no
  record of Capcom's, so it can be shared (rule 1, [`LICENSING.md`](LICENSING.md) 3).
- `mods.py` refuses: a table that is not a file table (section 2), a record
  past `count`, a field `tables.toml` does not have, a value the type does not
  hold, a name field (names are a language's; a text mod goes the `loc` way),
  two rows for one cell.
- The build step reads the install's `base\exe\tables\<key>.bin`, writes the
  values, and emits `opt\mod-<id>\exe\` with `cells` listing exactly the rows'
  bytes. The manifest lists the files as the mod's.
- `tools/tables.py export --cache <cache> --csv <dir>` writes one CSV per
  table for reading and editing; `tools/make_mod.py table-edit --from <dir>
  --to <dir>` compares two such trees and writes the `[[set]]` rows. The CSVs
  are game data and stay on the player's machine.

This is the authoring route. The conversion route, a disc romhack's edits to a
dropped section sorted through `exe_maps` onto table files and raw ranges
(I58's design step 3, second bullet), reuses these files and is group D,
after [`disc-patch-round.md`](disc-patch-round.md)'s group B.

## 5. Groups

| Group | What | Files it owns | After |
|---|---|---|---|
| **M** | section 2's six measurements, written into this note | none in `src/`; `docs/image-layer-round.md` | - |
| **A** | the loader: `tables.toml` + per-table files in a layer, `cells`, the conflict log, the `file` class, the self-test with synthetic tables (no game data) | `src/hook/exe_image.cpp`, `.h` | M |
| **B** | the base: `exe_tables.py` writes `base\exe\tables\`, `data.bin` zero there; `psp_layers.py` writes `psp-tables` in the new form; `importer.py verify` knows both | `tools/exe_tables.py`, `tools/psp_layers.py`, `tools/importer.py`, `recipes/psp-layers.toml` | M; merges with A |
| **C** | the mod kind: `mods.py` kind `table-edit` and its builder, `tables.py export`, `make_mod.py table-edit`, `mods.py selftest` with a synthetic table | `tools/mods.py`, `tools/table_edit_build.py` (new), `tools/tables.py`, `tools/make_mod.py`, `docs/mods.md` | A and B |
| **D** | conversion: a changed dropped section placed through `exe_maps` onto table cells and raw ranges, with a refusal that names file, section, range and reason for what does not place | `tools/disc_patch_build.py` (the disc-patch round's) | the disc-patch round's group B; may be a later round |

A and B change what the exe host reads at start, so they merge together and
are gated together (section 6). C touches no engine file.

## 6. Acceptance

1. **The image is the same image.** With no layer, the mapped `.data` after
   the table files are laid is byte for byte what it was before this round, on
   every install kind the harness holds (PC, PC + disc, each disc-only build,
   PSP-only): the exe host logs the sha256 of `.data` after map on both sides.
   Where `BOF3.exe` is present, `ExeImage_CompareWithFile`'s counts do not
   change.
2. **The state-hash routes: 0 differ** against the round's baseline
   ([`state-hash.md`](state-hash.md) A/B, the routes the last round used), with
   no layer and with `psp-tables`.
3. `psp-tables` in the new form changes exactly the 250 bytes DIV-0111
   records, over the same three images.
4. The self-tests: the loader's (a synthetic table laid, two layers on one
   cell with the later winning and the line logged, and the refusals: wrong
   hash, short file, a cell past the table, a cell on a must-equal class, the
   old `place.toml`-only form of a table layer) and `mods.py selftest`'s
   `table-edit` cases. No game data in either.
5. **One edit, end to end, on the owner's machine:** a `table-edit` mod that
   changes one number the owner can read on a menu, added on the Mods tab,
   built, seen in the game; removed, and the number is the original again.
   `tables.py dump` on the running install's files agrees both times.
6. The start-up cost from M4, measured again, and stated.

## 7. The owner's calls (answered 2026-11-11)

1. **What a table file holds: binary records in the PC's stride**, as section
   3 has it, with `tables.py export` giving CSV and the mod format being text.
   The engine lays bytes without parsing, and a table with unread fields
   (`tables.toml` has several `unread_*`) loses nothing.
2. **The base too**, as written, not layers alone. The cost is acceptance 1 on
   every install kind.
3. **Two mods on one cell: the later mod in the Mods tab's order wins**; the
   launcher says so beside both mods and the log has the line. (I58 as written
   refused instead unless the order says which wins; the order always says.)
4. **Names: for reading, never for the game.** A table file has no name and
   the game reads none from one. The owner: pull the names from their own map
   "as a title / natural language read, but not as a name the game reads". So:
   - `tables.py export` adds a `name` column to each CSV of a table that has a
     name field, filled from the install's language (the text the engine would
     write into that record's name field for the language given, English
     where the install has it). The column is a title for the row. `make_mod.py
     table-edit` ignores it, and a change to it is reported as ignored with
     the sentence that a rename is a text mod.
   - The Mods tab, and any refusal that names a record, shows the record's
     name beside its number, looked up at display time from the player's own
     install.
   - A `[[set]]` row still names a record by number. The name is not in the
     mod zip (it is the game's text, rule 1) and not a key (it changes with
     the language).
5. **When: before the disc-patch round.** Groups M, A, B, C touch none of its
   files. Group D needs that round's builder.

## 8. Not in this round

- **Cataloguing the dropped overlay sections.** Most of `.data` is not a named
  table today. M2 ranks what to name next; naming it is reading work for later
  rounds, a table at a time, and each new `tables.toml` entry becomes a file
  with no loader change. The area link lists (I58's entrance task) are the
  first candidate and fix that task's second part as a side effect: an install
  with no `BOF3.exe` gets an `area_links` file the save editor can read.
- **A table that grows, moves or changes stride.** The files have the PC's
  count and stride. A mod that adds a record is refused with that reason.
- **Code.** The PC runs no MIPS; a romhack's code change is ours to
  reimplement under a DIV, as I58 says.
- **`BOF3.exe` itself.** Nothing writes the player's exe. The hosted build
  keeps its own `.data` and refuses `exe` layers.
- **I55's `decompile` / `compile`.** The table files are one of the things
  that tree would show; nothing here waits for it.

## 9. Ledger

- The loader reading table files and any layer's tables is a change to what
  the engine reads at start: one new DIV entry at merge (number taken then),
  Tier Extension, with DIV-0111 amended to say its layer moved to the new
  form. With no layer the bytes are the same, which acceptance 1 proves.
- A mod's values are the modder's and are not DIV entries; the manifest rows
  record them (I58).

## 10. Where things are

- Branch `docs/image-layer-round` (this note alone), from `main` `7a495bd6`.
- The design this follows: [`IDEAS.md`](IDEAS.md) I58. The round before it in
  I58's order: [`disc-patch-round.md`](disc-patch-round.md), staged, its four
  owner calls open.
- The loader: `src/hook/exe_image.cpp` (`ExeImage_LayOptTables`,
  `ExeImage_OptSelfTest`, `kClasses`). The one writer today:
  `tools/psp_layers.py` (`exe_ranges`, `build_exe`), ruled by
  `recipes/psp-layers.toml`. The catalogue: `tables.toml`, `tools/tables.py`.
  The image's writer: `tools/exe_tables.py`. Mods: `tools/mods.py` (`KINDS`),
  [`mods.md`](mods.md).
