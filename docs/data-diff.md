# The static data diff: a disc-built image's tables against the PC's

**Status:** STABLE (verified 2026-10-25 on the owner's machine: `BOF3.exe` and
the seven held discs, `tools/data_diff.py run`; branch `tools/data-diff-gate`
off `main` `48511747`)

## 1. Why

A disc-built install's `base/exe/` image (`exe_tables.py` `disc_built`,
[`exe-import-engine.md`](exe-import-engine.md)) leaves zero whatever no rule
places, and `recipes/exe-gaps.tsv` classes those runs. Its `unread` class only
says that no watched route read the bytes. Issues 130 and 134 (areas 62, 97,
162, 163: a descriptor's link count zero, 7.8) and 139 (area 98's first exit
alternative zero, 7.9) were such runs; each was found by a player. This check
finds that class of hole before a player does: it decodes the tables the game
reads as tables, from the PC's `.data` and from each disc's image, and compares
them value by value.

## 2. What it covers

`tools/data_diff.py run --exe BOF3.exe --disc DISC ...` builds each disc's image
in memory (the image the importer writes; its sha256 checked against
`recipes/exe.toml`), decodes these families from it and from the PC's, each
image through its **own** pointers:

| Family | What | Reader the layout is taken from |
|---|---|---|
| `area_descriptor` | the 200 descriptors' words `+0x00..+0x40`, `+0x30` as its four bytes (link index, `+0x32` the input-flags byte), and the `Area_Descriptors` pointer itself | `Area_LinkAt`, `Party_DropIn` (`src/game/area_entry.cpp`), area-rows.md, exe-import-engine.md 7.8 |
| `area_link` | each area's `+0x30 + 1` links (cell, run, axis, destination area / x / z, flags) and, for an indirect link, its alternative list pointer, count and `count + 1` alternatives | `Area_LinkAt` ([`area-entry.md`](area-entry.md)) |
| `area_zone` | each area's zone records through `Area_StateRecords` (box, zone id, track or music set and its flag), a list running to the next list (the reader has no end test) | `Area_ZoneAtIn`, `Area_ZoneIdAt`, `Area_ZoneTrack` (`src/game/window_task.cpp`) |
| `music_set` | the twelve music sets at `0x669A48`: list, count, each `(track, flag row, flag bit)` and the fallback track | `Area_ZoneTrack`; `window_task_fuzz.cpp` `kSets` |
| one per `tables.toml` table in `.data` | every named field of every record through `tables.py`'s `records` (bit fields and repeats as catalogued), the name field skipped: the five item tables, `abilities`, `shop_records`, `exp_table`, `event_battles`, the encounter and battle-rate tables, the field tables, `fishing_rule_*`, `key_defaults`, `sin_table` | `tables.toml`'s cites |

The area families have no record type in `src/game/records.h` (their readers
take raw offsets); `data_diff.py` holds their layouts in one place, each line
citing its reader, and they are part of the views hash below. The enemy stat
records (`EnemyData`, `0x8C55C8`) are not in `.data`: they load from each
area's chunk `0xC2000` of a DAT file and are compared, at the disc's stride, by
`cache_inventory.py dat` (`format-widen`); the exe-resident battle tables are
here (`event_battles`, `encounter_*`, the rate tables).

**Verdicts.** A value the PC has that the image's decode cannot reach (a link
past a zeroed count), or a PC non-zero that is zero in the image where **no
disc placed the bytes**, is a `hole`. Any other difference - the disc's own
placed value, zero or not - is a `differ`. Fields whose bytes the engine
fills at run time (`own`, `keys`: `src/hook/exe_own.cpp`, `Key_TableDefault`)
are counted as `runtime` and not compared. `language` is deliberately **not**
skipped: its class comes partly from a symbol-name rule (`exe_own.py`
`NAME_RULES`), and on both PSP builds that rule reached from `Area05_MessagesB`
into area 6's descriptor (section 3), so a `language` byte inside a decoded
table is a hole like any other.

**The report.** `recipes/data-diff.tsv`: per build, the image sha256, a hash
of the views (this tool's layouts and every compared table's shape in
`tables.toml`), per family the counts compared / runtime / hole / differ, and
one row per differing value: build, family, key, verdict, and where the
image's bytes came from (`map`, `place`, `table`, `widen`, `rebuilt`,
`layout`, or for an unfilled byte its class: `gap`, `unread`, `language`,
`pointer`, `none`). Keys only - no value (CLAUDE.md rule 1); `--values FILE`
writes the values for reading by hand, refused inside the repository.

**Known list.** `recipes/data-known.toml`, in the style of
`recipes/abc-known.toml`: each entry a `kind` (`difference` matches `differ`
rows, `hole` matches `hole` rows and needs `tracked`), `builds`, fnmatch
`keys`, `reason`, `cite`. A row no entry matches fails; an entry no row
matches fails as stale.

## 3. What it found

Measured 2026-10-25, the seven held discs (Japan, USA, Europe, France,
Germany, PSP Japan, PSP Europe) against `BOF3.exe`.

**The regression check.** Run against the recipe and tools of `main` before
PR #136 (`b0fef7a2`, extracted with `git archive` under
`C:\Users\kerok\bof3x-runs\datadiff\pre136` and imported with `--tree`), it
reports on every PlayStation build `area_descriptor/a062`, `a097`, `a162` and
`a163` `link_last` as holes (`unread` for 62 and 97, `gap` for 162 and 163)
with 16 to 32 unreachable `area_link` values each - issues 130 and 134 - and
`area_link/a098/l00/alt0/*` as holes (`gap`, on all seven builds) - issue 139,
whose fix came after that commit too. 1,174 rows there, 546 of them holes no
entry lists (the run fails); on current `main` none of those.

**Holes on current `main`**, every one classed:

| Builds | Key | Bytes from | What it is | Done |
|---|---|---|---|---|
| `psp-jp`, `psp-eu` | `area_descriptor/a006/link_last` and 28 `area_link/a006/l01/*` values | `language` (`Area05_MessagesB+0x648`) | area 6's descriptor `+0x2C..+0x3C` (`0x5DFD34..0x5DFD3D`) unplaced; the link index read 0, so the area's second link - an indirect one, three alternatives - was unreachable. The PSP's `BOOT.BIN` carries the nine bytes at its left neighbour's displacement, but they hold one non-zero byte, under the 7.9 rule's two | **fixed** in the importer: section 4 |
| the five PSX builds | `area_descriptor/a089/input_flags`, `a129/input_flags` | `gap`, `unread` | the fishing spots' `+0x32` byte (exe-import-engine.md 7.8 *Left*): no disc holds it at a neighbour's displacement | **fixed** in the importer (2026-10-27): the descriptor-table rule, exe-import-engine.md 7.11; the entry `fishing-spot-input-flags` dropped |

**Release differences**, every one listed with its ledger entry:

| Entry | Builds | Keys | Cite |
|---|---|---|---|
| `psp-exp-table` | `psp-jp`, `psp-eu` | 287 `exp_table` values, rows 629..692 (roster index 6) | DIV-0104 |
| `psp-consumable-87` | `psp-jp`, `psp-eu` | `consumables/0087/field_use` | DIV-0104 |
| `western-rule-rows` | US, EU, FR, DE, `psp-eu` | 8 `fishing_rule_lines` rows (9 on DE) | DIV-0101 amended (D303, D286) |
| `jp-event-op-15` | `psx-jp` | `event_op_lengths/0015/length` | `tables.toml` (entry 15, never used) |

Everything else - every item, ability, shop, event-battle, encounter and
rate-table field, every zone and music set, every other descriptor and link -
equals the PC's on all seven builds.

## 4. The importer fix (section 7.10 of exe-import-engine.md)

The 7.9 neighbour rule places a stretch that agrees byte for byte at a placed
neighbour's displacement when it holds four bytes with two non-zero. Area 6's
nine bytes on the PSP (`0x5DFD34..0x5DFD3D`: the zero `+0x2C` word, the link
index 1, three zeros) have one. `find_places` now also takes a stretch with
one non-zero byte when it **closes the run**: every byte of the unplaced run
outside it is a pointer word (here `+0x28`, a layout word). A placed stretch
is by construction the PC's bytes; recipes regenerated over the seven discs
(`exe_tables.py recipe`, `psp_exe.py recipe`, `exe_tables.py recipe`,
`exe_own.py measure`, `exe_tables.py gaps`): one place row added on each PSP build (`psp-jp` `BOOT.BIN` `0x003008D0`, `psp-eu` `0x00300A90`, nine bytes, 465 to 466 and 455 to 456 places), the two `language` gap rows gone, nothing else in any recipe file moved, `placed_differ` unchanged on every build. `importer.py
check`, `exe_own.py check`, `exe_tables.py check`, `psp_exe.py check`,
`tables.py check`: 0 errors. **A PSP-built install needs the rebuild**
(`importer.py stale` names `exe`). Area 6's second link is indirect; what a
PSP-only install did at that cell before the fix is not seen in play.

## 5. The gate, and why this one

`tools/data_diff.py check`, in CI (`.github/workflows/checks.yml`), with no
game data: the committed report's every row listed in `data-known.toml` and
no entry stale; the report made from the images `recipes/exe.toml` now names
(each build's sha256) and from today's views (their hash); every disc build
compared; the decoders' self-test on synthetic images (issue 130's shape, a
zeroed link count, and 139's, a zeroed alternative, each reported as a hole;
a placed zero as a difference; a runtime byte skipped; a hole refused by a
`difference` entry).

Chosen over a pre-release step because the image is a pure function of the
committed recipes: any change to the importer's output changes
`recipes/exe.toml`'s hashes, and CI then fails until someone with the discs
re-runs `data_diff.py run` and the new report's rows are each listed. The diff
itself needs the discs and the PC's exe, which CI does not have; what CI
holds is that it was run on exactly the images the recipe describes, and that
nothing it found is unexplained.

**When it fails:** re-run

    python tools/data_diff.py run --exe <game>/BOF3.exe --disc <each of the seven discs>

then fix the importer for a new hole, or add a `data-known.toml` entry for a
new release difference with its ledger cite. A changed `tables.toml` shape
(a field added) changes the views hash and asks for the same re-run.

## 6. Not covered

- Tables read through code-relative offsets with no catalogue entry (the
  per-area handler tables, the effect tables): a byte there is compared only by
  `exe-gaps.tsv`'s classes. Adding a `tables.toml` entry adds it here.
- The descriptor's other pointer targets (entry lists, scripts, the model) are
  compared as pointers, not followed.
- The DAT-side tables (enemy stats, area chunks): `cache_inventory.py dat`.
