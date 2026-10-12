# The disc-only round: code review before the PR

**Status:** OPEN (2026-10-12) - a review of `platform/disc-only-round` at
`944f170d` against `main` `39bd3740`, for the session finishing the round to
act on before the PR. Strike an item here when it is fixed (with the commit),
or move it to [`owner-review.md`](owner-review.md) if it is the owner's call.

**What was measured:** read only, by four reviewers in parallel (hook /
launcher / input; the operand rewrite in `src/game`; `importer.py` /
`loc_build.py`; `exe_tables.py` / `exe_own.py`, rule 1 and the ledger), with
the findings below re-checked against the code. The five no-game-data checks
pass at the tip (`type1.py selftest`, `loc_build.py selftest`, `importer.py
check`, `exe_own.py check`, `exe_tables.py check`). **Not run:** no mingw
cross-compiler and no game data in the reviewing container, so no build, no
self-test under the exe host, no route. Every non-merge commit is signed off.

## 1. Gating

1. **A cooked `.iso` now makes `build` fail.** `tools/importer.py` `fmv_build`
   (line 1880) calls `xa.sectors(psx_disc.Disc(disc.path), FMV_STR)` for
   every PSX disc source; `xa.sectors` raises `SystemExit("XA needs a raw
   (2,352-byte sector) image")` on a cooked image (`tools/xa.py` line 65).
   `psx_disc` accepts cooked ISOs and nothing else in the importer needs a
   raw one, so a build that finished before now exits non-zero after the
   manifest is written. Want: a `skipped` `[fmv]` row, as for no ffmpeg. The
   same for `disc.files[FMV_STR]` raising `KeyError`, and for
   `subprocess.run` raising `OSError` (an ffmpeg that cannot execute), which
   also leaves `CAPCOM.AVI.str.tmp` behind.

2. **DIV-0100 says more than the code does.** The entry says the probe's
   bypass applies with a disc-built image mapped and no `BOF3.exe`;
   `src/game/win_main.cpp` line 514 tests `ExeImage_DiscMapped()` alone, and
   `g_disc_mapped` is set whether or not the file is present
   (`src/hook/exe_image.cpp` line 259). Its "Reversible?" ("put a `BOF3.exe`
   ... in the game directory and the original probe decides") is therefore
   false: a PC install pointed at a disc-built cache, with no `CAPCOM.AVI`,
   starts where the original shows the insert-the-disc box. Either add
   `&& !ExeFile_Present()` to the condition or correct the entry.

3. **The gaps mapped as zero by default, with no ledger entry.** A
   disc-built image runs with the US image's 3,358 PC bytes of `gap` ranges
   as zero (`Magic_Rows`' effect file numbers - the wrong effect file - the
   dragon-form sets, the ending roll's steps); only
   `BOF3X_EXEIMAGE_STRICT=1` refuses. DIV-0100 covers the probe only, and the
   owner's call ([`owner-review.md`](owner-review.md) "Decisions", the first
   item) is open. Rule 2: a provisional entry naming what the gaps break, or
   strict as the default until the call is made.

4. **`DIV-TBD-IN-1` never renumbered.** `src/game/win_main.cpp` lines 509 (the
   comment) and 519 (the log line) carry the placeholder; the ledger entry is
   DIV-0100, which nothing in `src/` cites. The only `DIV-TBD` left in
   `src/`, `tools/`, `docs/` and `recipes/`.

5. **DIV-0090's amendment sits in DIV-0091.** `docs/DIVERGENCE.md` about line
   5348: the "Amended 2026-10-12" paragraph follows the heading `### The MP3
   decoder is minimp3, not the library Capcom linked` and precedes `- **ID:**
   DIV-0091`, so it reads as DIV-0091's; DIV-0090's own text still says what
   DIV-0097 / DIV-0098 made false. Move it up into DIV-0090.

6. **A partial movie is installed, then sticks.** `fmv_build` has ffmpeg
   write straight to `base/fmv/CAPCOM.AVI`; `fmv_install` copies the file
   because it exists, never reading the manifest's `[fmv]` row or its
   sha256. A conversion cut short (Ctrl-C in the two minutes, ffmpeg killed)
   leaves a truncated file and no `[fmv]` row; `install` copies it, and every
   later `install` keeps the game directory's copy, so a correct rebuild is
   never installed. Want: ffmpeg to a temporary name renamed on success, and
   `fmv_install` requiring a row that is not `skipped` whose hash matches.

## 2. Not gating, worth a look

- **`bss_numbers`' "two small halfwords" tests one halfword**
  (`tools/exe_tables.py` line 223). Every `.bss` address is
  `0x676000..0x93D6EC`, so `v >> 16` is always below `0x100` and the rule is
  `(v & 0xFFFF) < 0x100 or >= 0xFF00`: a real pointer to a variable within
  `0x100` bytes of a 64 KiB boundary (`0x6AFF10`, say) is classed a number,
  and "a column of numbers" can spread it. Whether any of today's hits is
  such a word was not checked (no data); IN's "every class that must equal
  equal" on the real install suggests not. Test the high halfword too, or say
  in the docstring what the rule tests.
- **`fmv_install` on a PC install:** it runs for every install, so a PC
  install whose `CAPCOM.AVI` stayed on the CD gets the disc's converted movie
  in its game directory when the cache had a PSX source - `Disc_Probe`'s
  local test then answers "installed". A behaviour change with no ledger
  mention.
- **`check_relaid` on the PC path:** now runs on every PC-install `all` build
  and stops it if the donor differs in the rule's region. The output is
  unchanged when it passes; a build that used to succeed can now stop, which
  rests on the five-disc measurement.
- **The hosted checks, a little weaker:**
  - The Config screen's check compares the parallel arrays (`g_label_w_now`,
    `g_anchor_now`, `g_box_now`), not the `g_ops` fields the readers use
    (`src/game/config_text.cpp` 304-321, 362-368, 436-443). They agree only
    because each block's patches share one `BOF3X_ORIGINAL` name.
    `yes_no_layout` has the same shape (`g_stops` checked, `g_stops_moved`
    read).
  - The dropped `CheckSlideBound` checks also compared the opcode bytes
    before each immediate; the new entries compare the immediate only.
  - An entry registered late that falls under a jmp is marked `under_jmp`
    and never compared, with no log line (`src/hook/code_bytes.cpp` line
    153), where [`operand-store.md`](operand-store.md) section 1 says such
    entries are named in the log. None exist today.
- **Small, hook side:**
  - The launcher sets `BOF3X_EXEIMAGE` with the wide API; `exe_image.cpp`
    reads it with `GetEnvironmentVariableA` and `fopen`, so a cache path
    outside the ANSI code page fails (loudly).
  - A `BOF3X_EXEIMAGE_STRICT` value of 8 or more characters reads as strict
    (`exe_image.cpp` line 394; the safe direction).
  - `exe_image.cpp`'s comment lists `exe` among the classes that may differ;
    the code requires it equal (only a `pc-zh` image has it).
- **`exe_own.py check`:** its regex skips a row not written as `{0x.., 0x..,
  "..."` without a word, nothing compares the count with `kRows`, and
  `own_bytes()` turns an `ImportError` into an empty set, which would let
  `check_gaps`' overlap test pass vacuously.

## 3. Checked and found right

- **The operand rewrite:** each of the ~43 sites has the same address, width,
  sign extension and meaning as the old `CodeBytes` read, narrow and wide,
  Latin or not, `BOF3X_ORIGINAL` on or off. The switch tables cover every
  index the old tables did, with the same defaults; their bytes are built by
  looping the reader's own switch and checked hosted. Registration order is
  kept where one divergence caches another's target.
- **`exe_host.cpp` after the RD / OP conflict:** no leftovers; `OpenExe` ->
  `CopyData` -> `rdata::Map` -> `ExeImage_Source` -> `Shell_MapDefaultKeys`
  -> `ExeImage_CompareWithFile`, the key table written before the comparison
  that needs it equal.
- **The key table and the keyboard format:** `input::DefaultKeys` matches
  [`controls.md`](controls.md) section 1 and the 32-entry layout at
  `0x66C648`; `DIDATAFORMAT` / `DIOBJECTDATAFORMAT` are 24 / 16 bytes on
  i686.
- **The engine's own regions:** every one of the 17 lands on its declared
  end, none overlaps the key table, the sine rule is the one documented, and
  every byte the fill writes is compared with the PC's when the file is
  there.
- **The layout:** applied after the rebuild in `disc_built`, `recipe` and
  `measure`; a rebuilt word that disagrees with the table stops the build.
- **Rule 1:** no game data in the diff. `recipes/exe-layout.tsv`'s `target`
  column is the PC's pointer value for each of 18,840 words (code targets
  mostly as `[[func]]` names): the owner's "held as layout" of 2026-10-11,
  noted here, not raised.
