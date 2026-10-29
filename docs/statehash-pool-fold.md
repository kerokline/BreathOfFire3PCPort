# The state hash's fold and `.pool`: staging the fix (PR #168 review, left open)

**Status:** STAGED (2026-10-29; not launched. Found in PR #168's review and
left open as part of that PR; one agent, no owner's call needed beyond
section 5's one question of shape.)

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
