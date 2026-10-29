# The state hash's fold and `.pool`: staging the fix (PR #168 review, left open)

**Status:** DONE (merged as PR #170, 2026-10-29 evening, `main` `db017c74`; the
lifted-mask pair and the ten routes measured the same evening, section 7
"The coordinator's runs". Built on branch `statehash/pool-fold` off `main`
`e839eaf7`: the fold to `0x587000`, header `BOF3SH5` with the extent - the
owner's answer (a) to section 5 - both self-tests extended and passing in
both hosts; section 7, "as run". Owed, the coordinator's: section 2.2's pair
on `cutsceneAndNue` with rows 175-183 lifted, and the ten routes of
section 2.3. Found in PR #168's review and left open as part of that PR.)

## 0. The finding, measured on `relocatable-image/round` `e04d4a59`

`BOF3SH4`'s fold ([`state-hash.md`](state-hash.md) 8) reads a word whose
value lies in a *folded range* as the value it would hold with the image at
`0x400000`. The range is `[F, F + kImageSize)`:

| Constant | Value | Where |
|---|---|---|
| `kImageSize` | `kExeHi - kExeLo` = `0x93F000 - 0x400000` = **`0x53F000`**, BOF3.exe's `SizeOfImage` | `src/hook/statehash.cpp:140-142`; `Fold` at `:206-213`, `Fold24` at `:219-224`, `InImage` at `:232`, the bounds at `:395-400`, the self-test's draws at `:428-477` |
| `IMAGE_SIZE` | `0x53F000` | `tools/statehash.py:54`; `fold_dump` at `:418-424`, the bounds at `:434`, the text at `:310` and `:447`, the self-test at `:497-530` |
| `.pool` | `kPoolVa` `0x93F000`, `kPoolSize` `0x48000`, to `kOwnVa` `0x987000` | `src/hook/exe_layout.h:45-48`, with the `static_assert`s at `:63-64` |
| The extent the hosts already use for "in the image" | `kPoolVa + kPoolSize - kImageBase` = **`0x587000`** | `src/hook/host.cpp:56` (the loader's fence), `src/hook/input_script.cpp:296` (`AsVa`) |
| The pages the hash covers | `kBase` `0x5DA000` to `kEnd` `0x93E000` (`statehash.cpp:116-117`): `.pool`'s own pages are **not hashed** | the constants |

So a moved `bof3x.exe` (base `0xA00000` in the proof) carries `.pool` at
`0xF3F000..0xF87000`, and a word that names a draw item - a pointer, or an
ordering-table link with its length byte on top - reads `0xF3Fxxx` there and
`0x93Fxxx` at `0x400000`. Neither value lies in `[F, F + 0x53F000)`, so the
fold leaves both raw and they differ.

**Why the both-bases proof passes today.** Every word known to name a draw
item is masked by the skip list's `ours` rows
(`tools/statehash_skip.txt:174-182`: `DrawItemPool_Free`,
`Gfx_PacketPools` 128 KiB, `DrawLayers` and `Gfx_FrameNodes`,
`Gfx_OtHeads+0x10` / `+0x30`, `DrawItemPool_Top`, `DrawItems` 144 KiB,
`Gfx_OtPointers+0x10`), rows written for DIV-0062 because ours draws from a
pool Capcom's build does not have. The fold's correctness thus rests on the
skip list's completeness: a new word holding a `.pool` address outside those
rows - a record field, an arena word, a list the game builds - would show as
a false difference between the bases, and would be read as a defect of the
move. The finding is the dependency, not a present failure: on the final
tips all ten routes and the attract sequence passed at both bases
([`relocatable-image-round.md`](relocatable-image-round.md) 4, "the proof as
run").

Two more consequences of the narrow extent, for the agent to keep in view:

- `InImage` (`:232`) decides which words are Capcom's slots at the census
  by the same `kExeHi`; a slot holding a `.pool` address at start-up is not
  listed on either side. Consistent today, since both sides use the same
  rule, and unchanged by the fix unless the agent finds a load-time word
  that names `.pool` (none expected: Capcom's image has no `.pool`).
- `Fold24` still applies at the moved base: `0xF87000` is below 16 MB, as
  [`relocatable-image-design.md`](relocatable-image-design.md) 1.2 requires
  of any base (`<= 0xA70000`), so a 24-bit link into a moved `.pool` has a
  24-bit value and the rule reaches it once the range is widened.

## 1. The fix

**Fold to `.pool`'s end.** The folded range becomes `[F, F + kImageExtent)`
with `kImageExtent = kPoolVa + kPoolSize - kImageBase = 0x587000`, the
extent `host.cpp` and `input_script.cpp` already use, taken from
`exe_layout.h` rather than written again. In `statehash.cpp` that is
`kImageSize`'s definition and its every use (`Fold`, the bounds at `:395-400`
and `:641-668`, the self-test's draws); `InImage`'s `kExeHi` test is left as
it is (section 0, the slots) unless the agent's reading says otherwise, and
the reason is written at the line. In `statehash.py`, `IMAGE_SIZE` and its
uses (`fold_dump`, the bounds, the two texts, the self-test). The comments
at `statehash.cpp:81` and the file's header, and `state-hash.md` 8 point 1,
say `0x587000` and why.

**What does not change.** With no folded range the hash stays `BOF3SH3`'s
bit for bit: every reference recorded so far (`cn_<route>_orig4r`, the
attract pairs) was hashed with the image at `0x400000` and nothing folded,
so none is re-recorded. The skip list's `ours` rows stay: they mask what
differs between Capcom's build and ours by construction, which is a
different reason from the one this fix removes.

**The header (section 5's question).** `BOF3SH4` records the image's base
and the fold address, not the extent. A folded `BOF3SH4` file and a folded
file after the fix are hashed over different ranges, and `statehash.py`
cannot tell them apart by their headers. Two shapes:

- **(a) `BOF3SH5`, the extent in the header** (recommended): one `u32` after
  the fold address; `statehash.py` refuses a comparison across different
  extents as it refuses different folds (`:261-264`), and reads a `BOF3SH4`
  file as extent `0x53F000`. The unfolded hash is still `BOF3SH3`'s bit for
  bit, so the version bump costs nothing recorded.
- **(b) keep `BOF3SH4`** and note in `state-hash.md` that the only folded
  files are the proof's own, under `ri1027\c1`, `c2` and `smoke`, which the
  fix re-records.

## 2. The proof

1. **The two self-tests, extended.** `statehash.cpp`'s `SelfTest` and
   `statehash.py selftest` draw their synthetic pointers from
   `[0x400000, 0x400000 + kImageSize)`; each gains pointers and 24-bit links
   into `[kPoolVa, kOwnVa)` on both sides. Before the fix the moved page
   differs; after it the pages hash alike, and a changed data byte is still
   seen. Headless, no game.
2. **One real pair with the mask lifted.** `cutsceneAndNue` (the route the
   proof settled its shape on; it reaches battle, where `Gfx_OtHeads+0x10`
   and `Gfx_OtPointers+0x10` hold `.pool` addresses, the skip list's own
   evidence of 2026-10-05) run moved and at `0x400000` with the same fold,
   under a `--skip-list` file that drops rows 174-182. With the old extent
   the pair differs on the pages those rows cover; with the fix it differs
   on the noise class alone (the tick-3 upload frame, the VRAM and sound
   windows, the D3D handle-cache page - the proof's known set). This is
   the measurement the finding asks for: the fold correct without the mask.
   Two game runs, the coordinator's, in the round's harness root.
3. **The proof's pairs as recorded** (`c1`, `c2`, `smoke`) compare as before
   under the full skip list, once re-recorded under the new header if (a).

## 3. What else names the extent

`0x93F000` as "the image's end" appears in `src/game` at 10 places
(`band_census.cpp:42`, `scenario_harness.cpp:151`, five fuzz files' comments
and guards, `magic_engine.cpp:305`'s comment) and in `tools/make_exe.py:14`,
`tools/reloc_census.py:21`. Each of those means Capcom's image or the fuzz
harness's view of it, where `.pool` is rightly outside; none folds or
compares across bases. The agent lists them in the report with a word each
and changes none without a reason written at the line.

## 4. Ownership, numbering

One agent, one worktree off `relocatable-image/round`'s tip, files
`src/hook/statehash.cpp`, `tools/statehash.py`, `docs/state-hash.md` (8 and
the header table), this doc's "as run" section; the coordinator runs the
pair of section 2.2. No ledger entry: the hash is tooling, not behaviour.
No defect number: the proof did not fail. `git commit -s`.

## 5. For the owner

One question: the header, (a) `BOF3SH5` with the extent recorded, or (b)
`BOF3SH4` kept with the note. The proposal is (a).

## 6. Not verified

- That no load-time word of Capcom's image holds a `.pool` address (section
  0's `InImage` note): the agent greps `recipes/exe-pointers.tsv` for targets
  at or above `0x93F000` and reports the count (expected 0).
- That rows 174-182 are the only skip rows masking `.pool` addresses: the
  agent reads the list once with `.pool` in mind.
- Whether the run-time words outside the slots (895 to 3,144 per dump,
  `state-hash.md` 8 measured 2) include `.pool` addresses at either base:
  the pair of section 2.2 answers it on one route; the other nine are a
  re-run of the proof scripts, if the owner wants the full set.

## 7. As run (2026-10-29, one agent, worktree `C:\Users\kerok\bof3x-runs\pf1029\wt`)

**What changed.** `src/hook/statehash.cpp`: `kImageSize` (`0x53F000`) is
now `kImageExtent = exe_layout::kPoolVa + kPoolSize - kImageBase`
(`0x587000`), used by `Fold`, `Fold24`, `CheckImageBase` (whose overlap test
now runs to `0x987000`), the start-up fences and log lines, and the
self-test's draws; the header is `BOF3SH5`, the extent written after the
fold address. `InImage`'s `kExeHi` test stays, with the reason at the line:
no load-time pointer word of `BOF3.exe` names `0x93F000..0x987000` (below),
and `bof3x.exe`'s `.pool` is zero at the census. `tools/statehash.py`: the
extent read from `exe_layout.h` through `make_exe.read_layout` (asserted
`0x587000`), `BOF3SH5` read, a `BOF3SH4` file read as extent `0x53F000`,
runs folding the same bases over different extents refused, `fold_dump`,
`check_fold` and the texts on the extent. `tools/quicksave_check.py`'s
`HashRun` read its records from a table of versions 1-3 and raised
`KeyError` on any `BOF3SH4` file; it now takes `Run.header`.
`state-hash.md` 8: point 1, point 7, the header paragraph with a table of
the versions, the variable rows, the tests. No ledger entry, no defect
number (section 4).

**Measured.**

- **The self-tests, section 2.1.** Both extended as section 2.1 asks (the
  numbers in `state-hash.md` 8, "The tests"). Before the fix - the extent
  put back to `0x53F000` in a scratch build, not committed - `bof3x.exe`'s
  self-test exits 3: "the page with the image at 0x70000000 hashes
  0x2340E720, at 0x400000 0x17D605B3, both folding 0x70000000"; with that
  check made a log line, the `.pool` page's own check exits 3: "a page of
  pointers and links into .pool 0x0093F000..0x00987000 moved to 0x70000000
  and 0x00A00000 hashes 0xD97F997D, at 0x400000 0x37438283, folding both".
  The tool's self-test with `IMAGE_EXTENT` set to `0x53F000` fails the two
  folded-equal checks (`.data` and links). After: exit 0 in all three
  starts (hosted with `BOF3X_SHADOW='*'`; `--host exe`; `--host exe` with
  `BOF3X_IMAGEBASE=0xA00000` - the exe host refuses `BOF3X_SHADOW` by
  design, so those two run without it), and `statehash.py selftest` passes.
- **The unfolded hash unchanged.** `main`'s `statehash.py` and this one on
  the same inputs, 33 commands: `check` of every route's `orig4` pair against
  its own `orig4b`, of the `orig4` pairs against the `BOF3SH4` re-hashes
  `ri1027\ra2\sh\cn_<route>_orig4r.sh`, `diff` of the `BOF3SH4` folded pairs
  `ri1027\c1\sh\cn_<route>_orig5f` / `_c1m` and of `ri1027\smoke`'s
  `attract_lo` / `attract_m`, `diff` of the combat pair, `info` of a
  `BOF3SH3` file: identical output and exit code, all 33. Nothing under
  `analysis/statehash` is re-recorded. (The `BOF3SH4` folded files compare
  among themselves as before; against a `BOF3SH5` run folding the same base
  they are refused, by design.)
- **Section 6, item 1.** Every word of `recipes/exe-pointers.tsv` read from
  `BOF3.exe`: 18,831 pointer words, **0** with a target at or above
  `0x93F000`; the highest is `0x93C6C0` (`.bss`). Capcom's sections end with
  `.rsrc` at `0x93E000 + 0xCA8`.
- **Section 6, item 2.** Read with `.pool` in mind, the rows whose stated
  reason is a pointer into the draw pool are the **nine ranges on lines
  175-183** of `tools/statehash_skip.txt` (line 174 is their comment; the
  plan's "174-182" is one line short and leaves out `Gfx_OtPointers+0x18`,
  line 183; `pf1029\statehash_skip_nopool.txt` drops all nine). No other
  row's reason names the pool. A value scan of the 219 raw dumps of
  `analysis/statehash` for words in `0x93F000..0x987000` (whole, or as a
  24-bit link under a top byte) finds such values inside other rows too -
  `Music_Staging` (line 30), the task scheduler's range (20), `Sound_Banks`
  (26), the D3D caches (167, 168) - and outside every row, but in Capcom's
  dumps as in ours (the VRAM shadow's texels at `0x74C000..0x78Fxxx` lead),
  so a value in the range is no evidence of a pointer and the dumps cannot
  settle it offline; section 2.2's pair is the measurement.
- **Section 3, what else names `0x93F000`** (none changed): in `src/game`,
  `band_census.cpp:42` (follows the area scripts' and sprites' pointers,
  Capcom's data, never `.pool`); `scenario_harness.cpp:151` and
  `field_e1_fuzz.cpp:195` (the fuzz harness's readable image and a text
  pointer's guard, hosted only, Capcom's image); the comments at
  `battle_e5_fuzz.cpp:328`, `battle_turn_steps_fuzz.cpp:61`,
  `rest_3b_fuzz.cpp:351`, `rest_3d_fuzz.cpp:267` and `magic_engine.cpp:305`
  (where Capcom's objects run off his image's end - true of `BOF3.exe`);
  in `tools`, `make_exe.py:14` (`.pool`'s own VA in the layout listing) and
  `reloc_census.py:21` (`BOF3.exe`'s `SizeOfImage`, `.pool` counted beside
  it). Eight places in `src/game` and two in `tools`, not the plan's ten in
  `src/game`; in `src/hook`, `exe_layout.h` (the layout itself) and
  `statehash.cpp`'s `kExeHi` (kept, above).
- **The gates.** `cmake --preset i686 && cmake --build build`: 0.
  `reloc_bare --check`, `reloc_census --check`, `data_census --check`,
  `typed_left --check`, `ledger_check`: all pass. (A first version used two
  `static_assert`s and an `kPoolVa - kExeLo`, which `reloc_census` counts as
  new `addr_eq` / `ptr_sub` sites; they were rewritten rather than the
  census regenerated.)

**The coordinator's runs** (2026-10-29 18:12..19:50, `pf1029\proof_pf2.sh`,
the branch's build in `pf1029\Lp` - the code PR #170 merged, `main` differing
by docs alone - hashes, checks and diffs in `pf1029\sh`; the machine idle,
nobody at it).

- **Section 2.2, the fold without the mask.** `cutsceneAndNue` moved
  (`BOF3X_IMAGEBASE=0xA00000`) against ours at `0x400000` folding
  `0xA00000`, under `statehash_skip_nopool.txt` (the skip list without lines
  175-183): **identical on all 7,425 ticks** (`diff_cutsceneAndNue_lo_m_np.txt`).
  The fold is correct where the mask used to hide it, and section 6's item 3
  is answered on that route: the run-time `.pool` words fold alike at both
  bases.
- **Section 2.3, the ten routes under `BOF3SH5`.** Per route three runs:
  ours at `0x400000` unfolded, `check`ed against the `orig4` pair
  (`check_<route>_a2.txt`; the script's own `check` step pointed at the
  worktree, which holds no `analysis/statehash`, and was re-run from the
  main checkout's tools and references); ours at `0x400000` folding
  `0xA00000`; ours moved; `diff` of the last two under the full skip list.
  **Unfolded against the references:** every route's page table is the
  stage C2 proof's (`ri1027\c2\sh\check_<route>_c2a.txt`) line for line -
  the same pages, first and last ticks and tick counts, on nine routes;
  whelpBoss lacks the one three-tick sound span (`0x7DE000`, ticks
  2497..2563) the C2 run had. So the `BOF3SH5` hash where nothing is folded
  is `BOF3SH4`'s in play as in the self-test, and the references stand.
  **Moved against linked, same fold:** identical on combat (2,561 ticks),
  cutsceneAndNue (7,425), field_menu (1,409), menu_screens (1,729) and shop
  (3,137); the other five differ on the proof's noise class alone -
  `Gfx_VramShadow` windows, the `Snd_BufferDesc` page, `Rest4b_BusyB`'s
  page and the `Display_DesktopWidth` page from tick 3 or a stream start
  (caughFish 9 pages, dragonTransform 28, whelpBoss 7), the D3D
  handle-cache page `0x6C3000` alone (masterAndManillo, the world map).
  No page outside that class on any route at either base.

Nothing is owed. The two sub-word reads and the 729 not read (the round's
question 3) and the exit trace's stack scan (question 2) stay with
[`relocatable-image-round.md`](relocatable-image-round.md) section 5.
