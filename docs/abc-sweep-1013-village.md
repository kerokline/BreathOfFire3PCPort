# The faerie village on the disc-only installs: the stats, the village words, set 8

**Status:** MEASURED (2026-10-13 morning, the owner away; branch
`away/1013-village` off `d0d9162a`). D264 and D265 (with D247) fixed and
seen in the harness; `Menu_ButtonSets`' set 8 read, not filled (the owner's
call, section 4.1). Every claim cites the run, tool output or probe that
produced it; frames and caches stay under `C:\Users\kerok\ab` (game-derived,
rule 1). Not seen by the owner.

A new doc rather than a section of [`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md):
that doc's fixes were all `twin` rows; this one adds a rule shape and a
language-build rule, and the two read better apart.

## 1. What was open

From the D260..D263 fixes' run ([`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md)
3.5, run `abx\runs\fx_fairyVillage`): the faeries' stat bars black and the
food counter 34 (84 on the PC) on both disc-only installs - **D264**; PSP-EU's
board without its `Culture` label - **D265**; and, from 3.3, the PSX builds'
`Menu_ButtonSets+0x2A` row (`0x663866..0x66386C`) classed `unread` and not
looked at.

## 2. D264: the faeries' stats, a new rule shape (`faerie`)

**Where the PC holds them.** Sixty trait records at `0x653200`, 20 bytes each:
a name of 16, then four stat bytes at `+0x10` (`rest_4a_callees.h`
`kRecordTraits`, "+0x10..+0x13 read by the ticks"; `rest_4b_callees.h`
`kBars` `0x653210`). On every disc-built image the stats were `gap` /
`language-zero` rows (`recipes/exe-gaps.tsv` before: 59 rows a build of
`0x653224 + 20 r`, four bytes each, and `0x653200..0x653214` holding record 0's
name and stats), so zero.

**Where the discs hold them.** A scratch probe over `exe_twins.sources`
(`psp_exe.sources_of`) searched every source for sixty records of stride 9
whose bytes +5..+9 are the PC's sixty stat words in order: on all seven discs
**one** table in `ETC/COMMU00.EMI#0` (and the same in section 14 of each
village area overlay `WORLD04/AREA175..185.EMI`, the shared community
module; and on the PSP discs a third copy in `BOOT.BIN`), at `0x801F26F0`
(PSX-JP, PSP-JP, PSP-EU) or `0x801F2700` (US, Europe, France, Germany), all
sixty records matching. The table ends where the area-limit run D261 takes
begins (`0x801F2700 + 59 * 9 + 5 = 0x801F2918` on PSX-EU, the twin's address):
the disc's record is a 5-byte name and the same four stats, the shape
DIV-0064 16 found the names by.

**The rule** (`tools/psp_exe.py`, `faerie`; the constants `FA_*`): the range
`0x653200..0x6536AC` of every disc build, its mask the four stat bytes of
records 0..58, copied from the disc's record r at `+5`. The place is found at
recipe time by all sixty stat words (`records_place`, which now takes the
disc's own stride and field beside the PC's). It stops at record 59's stats
(`0x6536AC`): those sit in the area-limit run, which the maps place on five
builds and D261's twin on PSX-JP and PSX-EU, so the rows do not overlap. The
names are left to the language layer (label group 16). `check` holds a row to
the rule's range and file and its filled count to four bytes a record.

**Measured** (`regen.sh` in scratch: `exe_tables recipe`, `psp_exe recipe`,
`exe_tables recipe`, `exe_own measure`, `exe_tables gaps`, the seven discs):

| | |
|---|---|
| `psp_exe recipe` | `faerie 0x653200..0x6536AC from ETC/COMMU00.EMI#0 ...: 236 bytes, 0 not BOF3.exe's` on all seven builds |
| `recipes/exe-gaps.tsv` | each build loses 59 `language-zero` rows; `0x653200..0x653214` becomes `0x653200..0x653210` (record 0's name field - section 5) |
| the rebuilt caches (`ab\c_psxeu`, `ab\c_pspeu`) | the 60 stat words of `base/exe/data.bin` equal `BOF3.exe`'s and non-zero on both |
| the exe host's log (`selftest_*.log`) | PSX-EU `gap 224 ranges 3065 bytes` (282 / 3,301 before, `abc-sweep-1012-fixes.md` 3.6); PSP-EU `gap 193 ranges 1931 bytes` |

`exe-pointers.tsv`, `exe-places.tsv`, `exe-layout.tsv`, `exe-rebuild.tsv`
unchanged; `exe.toml`'s image hashes and `exe-psp.tsv`'s hash move.

## 3. D265 (and D247): label group 14 from a PSP disc

D265's blank `Culture` is D247 met in play: `convert_labels_psp` never built
label group 14 ("the stake title is not after its money title in
BOOT.BIN", [`loc-build-psp.md`](loc-build-psp.md) 3.2), so the board draws
`0x669E10`, zero on a disc image (`rest_4b.cpp` 1262: no group, the shipped
slot with a count of 2). PSX-EU's layer has the group from `COMMU*.EMI`.

**Where the five words are in PSP-EU's `BOOT.BIN`** (scratch probes
`probe14*.py`): list 13's run at `0x2EF15A` carries `Culture`; the pairs
`faeries` / `faery` in two copies of 7-byte slots end at the kind table
(`0x2EF870`); `Cash` and the zenny code follow the rectangles (`0x2EF554`);
and `Pot` is at `0x2EF71C`, after the name entry's words (`OK Del Next Back
End L R >`) and their eight pointers, then NUL and the zenny code - the PSX
stake slot's shape (`Pot\0\x60`). What follows `Pot` is CommuDraw_ShowSteps'
category table, the PC's 8 x 9 bytes at `0x652DEC` (`symbols.toml`, R4D:
"byte data follows (0x652DEC)"), found once in the PC's `.data` and once in
each PSP `BOOT.BIN` by its first 32 bytes.

**The rule** (`tools/loc_build.py` `convert_labels_psp`): the stake title is
the first word after the pointer table that ends before that category table
(a new anchor `village draws`, `0x652DEC`, 32 bytes, held as its SHA-256 like
the others; `check_anchors` over the PC install passes); the culture word
`lists[16]`, the pair the first two of four slots of 5..8 bytes before the kind
table whose halves match, the money title the 8 bytes after the rectangles.
Every word must be text and under 8 bytes, or the group is reported missing.

**Measured:** over the PC install and from the disc-only cache alike, PSP-EU
builds `village words 5`; the group's payload in `ab\c_pspeu\loc\en-150`'s
`FIRST.DAT` is byte for byte the PSX-EU layer's and the PC-path layer's
(`05 Culture 00 faeries 00 faery 00 Cash 89 C3 00 Pot 00`). PSP-JP's Japanese
layer is not changed (called directly, the converter stops at group 7's
codes before group 14, as it did). The PSX
converter is untouched; `loc_build.py selftest` ok.

## 4. Runs (harness root `C:\Users\kerok\ab`, the eu triple)

`abc_run.py prepare --set eu --root ab --build <worktree>/build` rebuilt the
three caches with this branch's tools (1,446 s; the machine shared with other
agents). Then `fairyVillage` from a scratch copy with `# save fairyVillage`
added as a comment (no frame moves; the trap of [`abc-harness.md`](abc-harness.md)).

**A harness trap met, and closed.** The first run (`runs\vil_fairyVillage`)
ended the PC and PSX-EU sides "FATAL" in 3 s and 2 s with no frames: their
`bof3x.log` is the 00:58 PSP-EU crash's (pid 20400, frame 1704, `esi
0x803ACA` - D260's), which `L\bof3x.log` still held, so `input_run` read its
`FATAL` before the new game rewrote the log - the `abc-sweep-1012-fixes.md`
3.5 trap with a `FATAL` instead of a `done`. **`tools/input_run.py` now
removes the launcher directory's `bof3x.log` before launching.** The re-run
(`runs\vil2_fairyVillage`) is the one below.

| `fairyVillage` | `fx` (before, `abx`) | `vil2` (this branch) |
|---|---|---|
| all three sides | `done` at 4719 | `done` at 4719, 157 timed frames each |
| frames differing A-B / A-C / B-C (of 157) | 89 / 95 / 92 | **24 / 30 / 22** |
| village frames (area 177, 134 of them) differing A-B | the board counter, food, bars, pop-ups throughout | **5**: `t000810..t000930`, the ranked list's page counter `236,44-264,52` only |
| A-C there | the same plus `Culture` | the same 5, and `t004230..t004320` (the shop's `R` / `R1` corner button: PSP art, B-C the same) |

At `t001350` and `t001710` the three installs draw the same board: food `84`,
`Culture 0`, `Hunt` / `Clear` / `Build`, the faerie `Pan`'s pop-up with its
four bars (`analysis/shots/village_1013_sheet.png`; the frames 630..1800 and
4230..4320 per install in `analysis/shots/village_1013_<install>/`). The
other spans of the report are the ones `fx` had (the title, the PSP's area
art on the world map, the PSP title framing D256).

**Not measured:** the stats in memory during play - the bars, the food
counter and the pop-ups are the reads of them, and every one equal is the
picture evidence; no `mem_dump` was taken. The faeries' simulation over time
beyond the route's 4,719 frames.

### 4.1 `Menu_ButtonSets`' set 8 on the Western PSX builds (read; not filled)

`Menu_ButtonSets` `0x66383C` is nine 5-byte records (a count, up to four verb
indices; `symbols.toml` gives `count = 5`, one record, which is why
`exe_own` classes the bytes past it `unread`). `0x663866` is set 8's second
verb. A scratch probe (`probe_sets.py`) found sets 0..7 verbatim on every
disc; set 8 is `02 14 0B` on the PC, PSX-JP and the PSP discs' JP-shaped
EMIs, and **`02 14 16` on US, Europe, France and Germany** (and PSP-EU's
`BOOT.BIN`). The US verb table (`probe_verbs.py`) has 23 entries, the 23rd
`End`; JP and the PC have 22, verb `0x0B` `Quit` (US) / 终了. So the Western
releases gave set 8 (`Knd`, then `End`) a verb of its own where the port
reuses `Quit`. The disc's byte cannot be placed (verb 22 would index the PC's
pointer table past its end, into `Menu_ButtonSets` itself), the map rightly
refused it, and the image holds 0: a set-8 row would draw `Knd` / `Use`. No
route reaches set 8 (unseen since 2026-09-21; who opens it is unread).

Not filled, because no disc of those four builds carries the PC's byte and
the choice is a design one - **for the owner**:
1. the port's `Quit`: a one-byte fill of `0x0B` for those builds (a rule that
   maps the disc's 23rd verb to the PC's, written down as such);
2. the Western releases' `End`: a 23rd verb slot in the engine
   (`menu_verbs.cpp`, DIV-0018's table; a `src/` change, and the PC path would
   draw `End` too under a Latin layer);
3. leave it (`Use`), as now.

## 5. Left

- **Record 0's name field** `0x653200..0x653210` stays a `language-zero` gap:
  label group 16 writes faerie r's name into record r + 1's field
  ([`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4's defect, unchanged
  here - a `src/` and PC-path change, the coordinator's and the owner's). With
  the stats filled the names are the one thing of the table still off.
- The ranked lists' page counter `"%d/%d"` (`0x6531F4`, engine-own) - the five
  village frames that still differ.
- The verb row of section 4.1.
- PSP-EU's Config text (D246) is still not built.

## 6. Checks

| Check | Result |
|---|---|
| `psp_exe.py check`, `exe_tables.py check`, `importer.py check` | OK / 0 errors (18,833 pointer words; the layout 19,217 words, 384 numbers) |
| `importer.py verify` `ab\c_pspeu`, `ab\c_psxeu` | 0 problems each; `base/exe/` the image `exe.toml` records (PSX-EU's `opt/area4-walls` "slot 6 composed with the layer" line is HANDOFF item 7's, unrelated) |
| `loc_build.py selftest`; `check_anchors` over the PC install | ok; ok |
| `ledger_check.py` | 114 entries, 0 errors, 2 notes |
| exe host self-test (`BOF3X_SELFTEST_ONLY=1`, scratch `village\L`, no ini, `BOF3X_EXEIMAGE` = the cache, `en-150`) on `ab\g_pspeu`, `ab\g_psxeu` | exit 0 both; `inject: 10088 ours`; `self-test only: done` |
| `BOF3X_SHADOW='*'` self-test | not run: no `src/` change |

## 7. For the other files (done on this branch)

- `docs/DIVERGENCE.md`: DIV-0101 amended (the `faerie` rule, a
  `language-zero` class closed); DIV-0090's PSP amendment amended (group 14).
- `docs/known-defects.md`: D264 fixed; D265 and D247 fixed (one cause).
- `docs/abc-harness.md`: the stale-log trap closed (section 4).
