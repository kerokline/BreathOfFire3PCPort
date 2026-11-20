# `BOF3.exe` out of the player's run: staging

**Status:** BUILT (2026-11-19, the local branch `exe-retire/round`, not merged
to `main`, not pushed): groups X1 to X6 ran the same day, section 9. **Not run
yet** (section 10): the ten state-hash routes, X7 (the proof that nothing
opens the file), the Sources page's window, the save editor's window. Not
played. Asked by the owner on 2026-11-19: "remove the original exe from any
post-install player usage"; the five calls of section 7 answered the same day,
all as proposed. Staged on the Linux round's wave A merge (`linux/lcA`
`33de9f1b`); sections 1 to 8 are the staging, with section 8's items answered
where they were. IDEAS I70; DIV-0241, DIV-0242, DIV-0212 amended.

## 1. What is asked, and what is not

**Asked:** after an install is made, nothing a player runs opens `BOF3.exe`:
not the launcher, not the engine, not the save editor. The file becomes what
a disc image is: a source the importer reads when it builds an install.

**Not asked, and kept:**

- **The importer reads it.** A player who owns the 2001 PC release gives the
  folder as a source, and the importer takes `.data` and the Chinese text
  from it, once.
- **The hosted build runs inside it** (`--host bof3`: `bof3x.dll` injected
  into `BOF3.exe`). It is the developer's oracle: the shadow suite, the
  `BOF3X_ORIGINAL` A/B switch, the call trace, the reference pairs. A release
  ships only the player's launcher, which holds no injection code
  ([`launcher-settings.md`](launcher-settings.md) section 2). Rule 6 of
  `CLAUDE.md` depends on it: it is how "what it was" stays answerable.
- **The engine keeps its reader** (`src/hook/exe_file.cpp`, portable since
  wave A's C2) behind the developer's switch `BOF3X_EXEFILE=1`, for the
  harness's runs on `<main checkout>\bof3` and for the checks below.

## 2. What reads it today, after install

A disc-built install already plays with no `BOF3.exe` anywhere (the disc-only
round, 2026-10-12, [`disc-only-install.md`](disc-only-install.md)). A PC
install does not. `grep -rn "BOF3\.exe" src/launcher` counts 40 lines in 7
files (`config.cpp` 7, `headless.cpp` 1, `launcher.cpp` 9, `save_dialog.cpp`
5, `sources_dialog.cpp` 3, `start.cpp` 14, `save/block.cpp` 1); the engine's
reads are the 12 lines of `grep -rnE "\bExeFile_(Present|Read)\(" src`.

| # | Who | Where | What it does with the file | Kind |
|---|---|---|---|---|
| 1 | launcher | `start.cpp:83`, `:89` | Finds the game folder by the file's presence; `exe_present` then steers the start | a test of presence |
| 2 | launcher | `start.cpp:102..105` | Hashes the whole file (SHA-256) at every start and refuses another build | a read, every start |
| 3 | launcher | `start.cpp:124` | Reads the default key table from it and compares it with the engine's rule | a read, every start |
| 4 | launcher | `start.cpp:220..240` (DIV-0212) | With a cache image built from the PC's exe, names the cache as `.data`'s source; with no cache, or a cache a disc built, or `BOF3X_EXEFILE=1`, the file is the source | a choice |
| 5 | engine | `exe_host.cpp:250` `OpenExe`, `CopyData` | Opens the file; copies `.data` from it when it is the source | a read, every start |
| 6 | engine | `exe_image.cpp:617..636` | DIV-0212's check: the cache's image with its table files against the file's `.data`, one byte of difference a `Fatal` | a read, every start |
| 7 | engine | `exe_image.cpp:1311..1352` | A disc-built image after the fills against the file's `.data`, as log lines | a read, when there |
| 8 | engine | `rdata_consts.cpp:46`, `:128` | The engine's copies of `.rdata`'s constants against the file's | a read, when there |
| 9 | engine | `shell.cpp:113..127` | The default key table against the file's | a read, when there |
| 10 | engine | `exe_own.cpp:350..357` | The engine's own `.data` bytes against the file's | a read, when there |
| 11 | engine | `dat_load.cpp:848`; launcher `config.cpp:1139` | "The Chinese text is here": the file's presence, or a `loc\zh-CN` layer | a test of presence |
| 12 | engine | `magic_080.cpp:668` | The disc probe: with the file beside the game the install is the PC's and Capcom's probe decides (DIV-0100) | a test of presence |
| 13 | launcher | `config.cpp:1783..1790` | Three settings refused on "a disc-built install (no BOF3.exe)" | a test of presence |
| 14 | save editor | `save_dialog.cpp:885`, `:895..898`, `:935`, `:971` | Reads the party table, 7 member records, the whelp row, 60 trait rows, the area links, and the chests and passages from the file (`ExePartyTable`, `ExeImageRead`, `ExeAreaLinks`, `ExeAreaFinds`); with no file it offers less (`:746`, `save/block.cpp:724`; HANDOFF, IDEAS I58's next step) | a read, when opened |
| 15 | importer | `player_sources.py:154` `lay_out_pc`, `:1134` `copy_pc` | Lays `BOF3.exe` into the install's game folder with `DAT`, `BGM`, `SND` and the movie | a copy, at install |

Rows 6 to 10 are checks: the game does not need their answer to run. Rows 1,
11, 12 and 13 use the file's presence to mean "this is a PC install". Rows 2
to 5 and 14 are real reads of content.

**What already exists to replace them:** the cache's `base\exe\data.bin` and
its 27 table files hold `.data` in the PC's layout on every install kind
(DIV-0212; `importer.py:36`); `data.toml` names the build (`build = "pc-zh"`
or a disc's); the manifest holds the image's SHA-256
(`importer.py:1533..1535`) and `verify` checks it (`:2409`); the importer's
`identify` already refuses an exe that is not the 2001 release's
(`player_sources.py:242..289`).

## 3. The shape

1. **One source of `.data` for a player: the cache's image.** The launcher
   never names the file. A PC folder with no image is set up first (call 1).
2. **"A PC install" is a fact the install records**, read from the cache's
   manifest or `data.toml`, not inferred from a file beside the game.
3. **The checks move to where the file is still read:** the importer's
   set-up and `verify`, and the developer's runs under `BOF3X_EXEFILE=1`
   and in the hosted build. The start checks the image against its own
   recorded hash instead (call 2).
4. **The save editor reads the image** through one reader for every install
   kind.
5. **The importer stops laying the file into the game folder.** A file a
   player already has there is left alone and not read.

## 4. The groups

No file is in two groups. Line numbers are wave A's and move when it merges.

| Group | What it does | Files | Proof |
|---|---|---|---|
| X1, the install's kind as a recorded fact | The importer writes the kind into the cache (the manifest or `data.toml`: the PC's `DAT` is the base, or a disc is). A call for it in the shared reader (`src/common/cache_manifest.cpp`). Rows 1, 11, 12 and 13 ask that call. An install made before this round has no such line: the reader then answers from `data.toml`'s `build` and the `loc\zh-CN` layer, and says so once in the log | `tools/importer.py` (the manifest's writer), `src/common/cache_manifest.cpp`, `dat_load.cpp:848`, `magic_080.cpp:668`, `config.cpp:1139` and `:1783..1790` | `config_tests` rows for each install kind, old and new manifest; the hosted suite |
| X2, the start without the file | The launcher: rows 2 and 3 go from the player's path; row 4 always names the cache's image, whatever built it, unless `BOF3X_EXEFILE=1`. The engine: row 5's open happens only under that switch or in the hosted build; rows 6 to 10 run only when the file was opened so. In their place at a player's start: the image's own SHA-256 against the manifest's (call 2). `Engine_Main`'s step 11 (`src/hook/exe_host_steps.h`, wave A's) loses its file half for a player | `src/launcher/start.cpp`, `src/hook/exe_host.cpp`, `exe_image.cpp`, `exe_own.cpp`, `rdata_consts.cpp`, `shell.cpp` | The exe host's self-test with and without `BOF3X_EXEFILE=1`: with it, every check line of today; without it, none and no open. The ten routes both ways, identical |
| X3, a PC folder that has no image yet | A game folder that holds the PC's files and a cache with no image, or no cache: the launcher makes the image through the Sources page's set-up before the first start, as its first run does for no install at all (`start.cpp:142`). Under `--no-config` and a self-test it refuses with one sentence that names the set-up command. A PC folder beside a cache that a disc built: the image is the disc's and is used (today the file wins there) | `src/launcher/start.cpp` (after X2), `sources_dialog.cpp`, `tools/player_sources.py` | A PC folder with no cache started through the player's launcher: set-up runs, then the title; the same folder under `--no-config`: the refusal |
| X4, the save editor on the image | `ExePartyTable`, `ExeImageRead`, `ExeAreaLinks` and `ExeAreaFinds` read `base\exe` (the image with its table files laid in, as the engine maps it) through one function, on every install kind. The two "no BOF3.exe" messages go if a disc-built image holds the same rows, or stay for the rows it does not hold, named (section 8) | `src/launcher/save_dialog.cpp`, `src/launcher/exe_image.cpp`, `save/block.cpp` | `save_tests`; `save_editor_drive.py` on a PC install with the file moved away and on a disc-built install: the entrance list and the party table as on a PC install today |
| X5, the importer's layout | `lay_out_pc` and `copy_pc` lay no `BOF3.exe`. `verify` gains rows 6 to 10's comparisons where a PC source is in the source list (the image against the source's `.data`, the constants, the key table), so they still run for every PC owner, once. The source list keeps the path of the player's PC folder, since a later language or mod build reads it again (call 4) | `tools/player_sources.py`, `tools/importer.py`, `tools/exe_tables.py` | `setup` from a PC folder: no `BOF3.exe` in `game\`; `verify` passes; one byte changed in the image: `verify` names it |
| X6, the words and the ledger | Every message a player reads after install that names `BOF3.exe` (the 40 launcher lines and the engine's `Fatal` texts of section 2) read again: gone, or said in terms of the install. ASD-STE100. DIV-0212 amended (its start check moves), DIV-0100's note on the disc probe, one new entry for X3 (a PC folder is set up before its first start) | `docs/DIVERGENCE.md`, `docs/disc-only-install.md`, `docs/install-sources.md`, `docs/launcher-settings.md`, the README | `ledger_check.py`, `ledger_switches.py --check` |
| X7, the proof that nothing opens it | A harness install made from the PC source by the new importer, so `game\` holds no `BOF3.exe`. On it: the launcher's start, the ten routes, `music_heard.py`, the Jukebox's and the save editor's drivers. Then the same with a `BOF3.exe` put into `game\` whose bytes are not the game's: every result the same, which shows nothing read it. The developer's side unchanged: the hosted shadow suite and the routes on `<main checkout>\bof3` under `BOF3X_EXEFILE=1` | `tools/` (a recipe of the above), no source | The routes' state hash on the new install against the references; the wrong-bytes run identical to the clean one |

**Order.** X1 and X5 first, side by side (X1 owns the manifest's writer in
`importer.py`, X5 the layout functions and `verify`: they agree on the
manifest line through the brief, not by editing each other's functions). Then
X2. Then X3 and X4 side by side. X6 as the others hand back. X7 last, by the
coordinator.

## 5. The gate line

The Linux round's Windows line ([`linux-round.md`](linux-round.md), "The 38
files, staged as one round", with `tu_census.py --check --strict` and
`retire_shells.py --check --strict`), and:

- `config_tests` and `save_tests` (`tools/launcher_host`) on Windows and in
  the WSL;
- the importer's `check` and `mods.py selftest`;
- the exe host's self-test and the moved base, each with and without
  `BOF3X_EXEFILE=1`;
- at the coordinator's merge: the ten routes on `<main checkout>\bof3` under
  the switch (the references' conditions), then X7.

## 6. What a player can tell

- **A PC folder used in place, with no cache:** the first start runs the
  set-up (minutes, once) where today the game starts at once. This is the
  one behaviour change, and the new ledger entry.
- **A PC install whose cache holds the image:** nothing, but a start no
  longer reads and hashes the file and no longer stops when the file
  beside the game is another build's.
- **A PC folder beside a disc-built cache:** `.data` is the disc-built
  image where it was the file's. The two differ by the gaps
  `recipes/exe-gaps.tsv` lists; a player of such a mixed install could tell
  where a gap shows. Call 3.
- **The save editor on a disc-built install:** the entrance list and the
  party table where today it offers less.
- **`BOF3.exe` in the install's game folder:** new installs have none. An
  older install keeps its copy; nothing reads it, and the player may delete
  it.

## 7. For the owner

**Decided (the owner, 2026-11-19): all five as proposed.** The round launches
after the Linux round's wave A is on `main` (call 5), at the owner's word.

1. **A PC folder with no image.** Proposed: set it up before the first
   start (X3). The other answer keeps the file as a source for that one
   case, and then the request is not met for it.
2. **What a start checks in place of DIV-0212's comparison.** Proposed: the
   image's SHA-256 against the manifest's, at every start. The image is
   638,976 bytes (0x9C000, `.data`'s size; this staging said 0x58F000, which
   is not the image's size: X2's measure), and the hash costs 1.8 to 2.1 ms
   on this machine (section 8). It catches a damaged or hand-edited image,
   which is what the comparison caught; it no longer catches a wrong
   `BOF3.exe`, which no longer matters.
3. **A PC folder beside a disc-built cache.** Proposed: the cache's image
   is used and the launcher says in one line that the Sources page can
   build the PC's. Or: X3's set-up builds the PC's image there too.
4. **Language and mod builds after install.** They read the source list's
   PC folder again (`importer.py:1392..1397`,
   `disc_patch_build.run_loc`). Proposed: that is the importer reading a
   source, as it reads a disc again, and is not "the player's run". If it
   must go too, the importer would keep what it needs of the exe in the
   cache, which is game data kept twice.
5. **When.** It changes `start.cpp`, `exe_host.cpp` and `exe_image.cpp`,
   which the Linux round's wave B also changes (its start-up group).
   Proposed: after wave A is on `main` and before wave B, since X2 removes
   the file half of the start-up that wave B would otherwise port, and one
   round runs at a time.

## 8. Not measured

- **Whether a disc-built image holds the save editor's rows. Answered by
  X4: the reader, not the data.** The readers measured themselves
  (`BOF3X_ORACLE_TRACE=1`) on a PC-built image and a US disc-built one (the
  i130 cache of 2026-10-29): the party table, the area links (2,003 reads)
  and the chests and passages (11,292 reads) are held on both, 0 refused; the
  seven default records' names (`0x64B390` + `0xA4*k`), the whelp's
  (`0x64B80C`) and the 60 trait names (`0x653200` + `20*r`) are held on the
  PC-built image and not on the disc-built one (a gap and `map` or
  `language` ranges). No read falls outside `.data`. A disc-built image
  built today was not measured.
- **How many installs in use are a PC folder with no cache.** The released
  launcher starts one directly today; X3 changes that.
- **The manifest's present fields. Answered by X1, corrected after X3:**
  an older manifest answers from its `loc/zh-CN` rows. X1 counted any such
  row; X3 found that a disc-built manifest lists that layer's slots with an
  empty source (275 rows on the i130 cache and on X3's `olddisc`, none with
  a source; a PC's has 475, each with one), so every older disc-built
  install read as a PC install. The coordinator's fix (`f5cd4721`): a row
  counts only with a source; `config_tests` has the rows.
- **The cost of the start's hash. Answered by X2 on this machine:** 1.8 ms
  (32-bit, twice), 1.8 and 2.1 ms (64-bit), on a quiet machine; 2.9 to 3.5
  ms with builds beside it. The engine makes it, not the launcher: it hashes
  what it maps, needs no second composer, and covers both launchers and a
  start with `BOF3X_EXEIMAGE` set by hand. **Not measured:** a slow disk.
- **Which of the engine's `Fatal` texts a player can still reach** once the
  file is not opened. **Answered by X6 (by reading):** of the texts that
  name `BOF3.exe`, a player's start reaches only the image's: the data
  toml's size against the engine's `.data` (now said without the file's
  name) and the missing recorded hash (its pointer to `BOF3X_EXEFILE=1`
  removed). Every other `Fatal` that names the file needs it opened (the
  hosted build, `BOF3X_EXEFILE=1`) and is the developer's. The "As run"
  section's X6 part lists them.
- **The tools that read `BOF3.exe`** (`grep -rl "BOF3.exe" tools`): they are
  developer tools and stay; none was read for a use that a player's launcher
  starts, beyond `player_sources.py`, `importer.py` and the mod builders.
- **The Linux side.** The round's files compile for x86-64 Linux as before
  (`linux_tree.py`, each group's run: 831 compile, 0 do not, 11
  Windows-only); nothing links or runs there yet, so no Linux start was
  tried. The reads this round removed are steps wave B would otherwise have
  given a Linux start.

## 9. As run (2026-11-19, the local branch `exe-retire/round`)

Five groups in worktrees, one at a time or two side by side, each merged
`--no-ff` by the coordinator when it handed back: X5 (`40d82a9e`) and X1
(`bb7fe3a3`), then X2 (`fa151c33`), then X4 (`b266355a`) and X3
(`a6da0935`, after the coordinator's fix `f5cd4721`), then X6 alone. Each
group's figures are its own report (the coordinator's condensed hand-backs);
the code is the record of what changed.

**X5, the importer's layout and `verify`** (`bc80bf46`). `setup` from the
developer's PC folder into a home of its own in 9 s: `game\` holds `BGM`,
`DAT` (742 files), `SND`, `capcom.avi`, 0 `BOF3.exe`; the cache has
`loc\zh-CN`. `verify`: rc 0, the PC source's `.data` byte for byte (638,976
bytes compared); one bit flipped in a copy of `data.bin` and of a table file,
each named with file and address, rc 1; the source moved or no
`sources.toml`: one sentence, rc 0. What `verify` cannot check: the `.rdata`
constants, the default key rule and the engine's own `.data` bytes are C++
tables with no Python copy; none was made, and the developer's runs compare
them. Not run: the builds, any game.

**X1, `[build] install_kind`** (`71a524e9`). `"pc"` (the PC game's `DAT\` is a
source) or `"disc"`; calls in `src/common/cache_manifest.h` ("The install's
kind"). Every install kind keeps its answer, but a PC folder beside a
disc-built cache: the Chinese text, `BOF3X_CACHE_DATA=0` and
`BOF3X_EXEIMAGE_STRICT=1` refused there (the owner's decision 3); `BOF3.exe`
with discs and no PC `DAT\` now allows `STRICT=1`. Gate: both presets;
`config_tests` 4,327 (Windows), 4,325 (WSL), 0 failures; `linux_tree` 831 / 0
/ 11; the hosted suite done, 922 `MISMATCHES` lines all 0. **A defect, found
later by X3 and fixed by the coordinator (`f5cd4721`):** an older manifest's
fallback counted any `loc/zh-CN` row, and a disc-built manifest lists that
layer's slots with an empty source (275 rows on the i130 cache and X3's
`olddisc`, none with a source; a PC's has 475, each with one), so every older
disc-built install read as a PC install. A row now counts only with a source.

**X2, the start without the file** (`44d8a831`). A player's start opens,
hashes and tests for no `BOF3.exe`; `.data` is always the cache's image; the
engine hashes it as mapped against the manifest's `[exe] sha256`
(`CheckRecordedHash`): 1.8 ms (32-bit, twice), 1.8 and 2.1 ms (64-bit) on a
quiet machine. The image is 638,976 bytes, not the 0x58F000 of the staging
(that is the span the moved base reserves). A game folder with no image:
`StartRefuseNoImage`. Row 1: the working folder is picked when it holds
`DAT\FIRST.DAT`. Runners: `attract_run.developer_exefile()` sets
`BOF3X_EXEFILE=1` with one line when the game folder holds `BOF3.exe` and no
cache is named; `input_run.py` and `abc_run.py` use it. Gate line 8 on
`<main checkout>\bof3` under the switch against a build of `bb7fe3a3`: the
self-test and the moved base 10,392 lines each, 0 different. Without the
switch on its own install: self-test, moved base and 64-bit done, 0 FATAL;
an 860,000-byte file of its own text as `game\BOF3.exe` gave the same run
line for line; one byte changed in a copy of the image: exit 3 with the
"damaged or was changed" sentence. Not run: the windowless launcher's
program, a live runner, the routes.

**X4, the save editor on the image** (`b70c1863`). The readers measured
themselves (section 8's first item): every row is held on a PC-built image;
on a disc-built one all but the default names. One reader for every install
kind (`ExeImageFromCache`); no `BOF3.exe` opened but under the switch. The
oracle (`BOF3X_ORACLE_EXE`, `BOF3X_ORACLE_CACHE`): the file against the PC
cache, 121 checks, 0 failures at both Windows widths and in the WSL (the party
table, the default names, 1,555 links, 187 chests, 120 passages equal); the
disc cache the same but the default names. `save_tests` 115, 0 failures at
the three. Not run: the fixture and `--layers` runs, `save_editor_drive.py`;
the editor's window is compiled only.

**X3, a game folder with no image** (`a64e5513`, `7b1bc6cb`, `c63f1c9f`).
With the launcher's window, the home's own `game\` gets the first run and any
other folder is set up in place (`setup --game-in-place DIR`, DIV-0241);
never hosted, under the switch, a self-test, `--no-config`, the windowless
launcher, `BOF3X_EXEIMAGE` set or a cache the player chose. Call 3's line is
said when the folder's `BGM\` holds a file and the kind is disc. `stale`:
pins for the records whose only change is the round's (`STALE_SAME`,
`STALE_SAME_DATA`); after them X3's copies of installs made by the base's
importer say `fresh`. The Python mirrors (`language_tags.cache_kind`,
`original_available`, `install_base`) follow X1's rule. Gate: both presets;
`config_tests` 4,357 / 4,355; `linux_tree` 831 / 0 / 11; the hosted suite
done, all `MISMATCHES` lines 0 (reported as 927; its log holds 922); line 8 with the switch 10,392 lines, without it on the in-place
install 10,397, 0 FATAL. Proof from the command line: the set-up in 8 s, all
1,805 files of the folder with the same sha256 afterwards. **The Sources
page's window was not shown or driven.** Found: i130's old cache stops at
the exe host's start ("47 gap ranges and no accepted list"), from before the
round.

**X6, the words, the ledger and the records** (the branch `exe-retire/x6`).
- **The ledger:** DIV-0241 (X3's set-up in place) and DIV-0242 (the file not
  read after install). The coordinator's brief named DIV-0240 and DIV-0241,
  but DIV-0240 is the function keys' entry of 2026-11-18: the two IDs are
  the next free. DIV-0212 amended and brought to the owner's decision 2
  (`Player: never`: no preset sets `BOF3X_EXEFILE=1`; "As shipped" turns off
  93 entries, was 94); DIV-0100 and DIV-0102 amended (the cache's record).
  `ledger_registry.h` and the presets' tables regenerated.
- **D432 fixed:** the engine's log no longer says "the running .data is
  BOF3.exe's" over the cache's PC-built image, nor "no BOF3.exe" where the
  file was not opened. `exe_own.cpp`'s pin moved with it; `stale` on X3's
  `oldcopy` and `olddisc_copy` and on X5's home still says `fresh`.
- **The words:** every message that names `BOF3.exe` read again; three
  player-facing sentences brought to one voice ("game data", "set up"); the
  missing-hash Fatal no longer points a player at the developer's switch;
  the windowless launcher's usage text says how it finds the game folder.
- **X3's limit 4:** a start that names no folder and is not run from the PC
  folder played in place finds it through `sources.toml`'s `play_in_place`
  row (`HomeInPlaceFolder`, asked by `StartFindGame` after the working
  folder and the home's `game\`; `HomeGameDefault` unchanged, so the stale
  check never reaches the folder).
- **Measured:** both presets build, no new warning in a touched file;
  `config_tests` 4,402 checks at both Windows widths and 4,400 in the WSL,
  `save_tests` 115 at the three (no fixtures), 0 failures; `os_census.py
  --check --boundary` ok; `linux_tree.py` 831 / 0 / 11, `--check` ok; gate
  line 5 all ok; `importer.py check` 0 errors; `mods.py selftest` 0
  failures; `language_tags.py selftest` 13 kinds, 0 errors. A set-up in place
  of a copy of the PC's files (7 s; the folder's 3,040 files the same sha256
  after the set-up and the runs). The exe host's self-test and the moved
  base on `<main checkout>\bof3` under `BOF3X_EXEFILE=1`: 10,392 lines each,
  0 different from X3's runs of the same. Without the switch on the in-place
  install: 10,399 lines, 0 FATAL, both bases; against X3's in-place runs only
  the D432 lines, the paths and the folder's own `area4-walls` overlay
  differ. Started with no `--game` from a folder that is not a game folder
  and the home naming the folder in place: the same run as with `--game`
  (only pids and timings differ). The hosted suite (`--host bof3`, `BOF3X_SHADOW='*'`) on `<main checkout>\bof3`: `self-test only: done` in 993 s, 922 `MISMATCHES` lines, all 0, 0 FATAL (X3's run's log holds 922 such lines too; the 927 of the briefs was not found in it).
- **Not run by X6:** the 64-bit self-test, the ten routes, X7, any window.

## 10. Not run yet

- The ten state-hash routes on `<main checkout>\bof3` under
  `BOF3X_EXEFILE=1` (the references' conditions), then on an install with no
  `BOF3.exe`.
- X7: a harness install made from the PC source by the new importer, the
  routes, `music_heard.py`, the Jukebox's and the save editor's drivers on
  it; then the same with a `BOF3.exe` of wrong bytes put into `game\`.
- The Sources page's window driven through a set-up in place (X3 ran the same
  importer commands from the command line).
- The save editor's window on a disc-built install and on a PC install with
  no `BOF3.exe` (`save_editor_drive.py`), and `save_tests` with the owner's
  fixtures and `--layers` (the coordinator's).
- The windowless launcher's program end to end; a live runner
  (`input_run.py`, `attract_run.py`, `abc_run.py`) on `<main checkout>\bof3`
  with its new switch line.
- `tools/launcher_host/dialog_test.py`: its fake caches hold no `base\exe`, so
  by reading `StartSetUpFor` and `StartApply` a start from them meets the
  set-up or the refusal; not run since the round.
- A slow disk's cost of the start's hash.
- The owner's play.
