# `BOF3.exe` out of the player's run: staging

**Status:** STAGED (2026-11-19; not launched; nothing built; the five calls of
section 7 answered the same day, all as proposed). Asked by the owner
on 2026-11-19: "remove the original exe from any post-install player usage".
Measured on the Linux round's wave A merge (`linux/lcA` `33de9f1b`, which holds
`start.cpp`, `exe_file.cpp` and `host_win32.cpp`); each figure has its command
beside it. What was not measured is in section 8. IDEAS I70.

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
   image's SHA-256 against the manifest's, at every start (the image is
   0x58F000 bytes; the cost is in section 8). It catches a damaged or
   hand-edited image, which is what the comparison caught; it no longer
   catches a wrong `BOF3.exe`, which no longer matters.
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

- **Whether a disc-built image holds the save editor's rows.** HANDOFF says
  an install with no `BOF3.exe` "gets no chest or passage places"; whether
  that is the reader (it asks for a file) or the data (a gap of
  `recipes/exe-gaps.tsv` at `0x64B390`, `0x64B80C`, `0x653200` or the link
  and find tables) was not read. X4 measures it first.
- **How many installs in use are a PC folder with no cache.** The released
  launcher starts one directly today; X3 changes that.
- **The manifest's present fields.** That `data.toml`'s `build` and the
  layer list are enough for X1's fallback on an old install was read from
  `CacheImageIsPcBuilt` (`start.cpp:40`) and `config.cpp:1139`, not tried.
- **The cost of the start's hash** on a slow disk; and whether the engine
  or the launcher should make it.
- **Which of the engine's `Fatal` texts a player can still reach** once the
  file is not opened; X6 lists them.
- **The tools that read `BOF3.exe`** (`grep -rl "BOF3.exe" tools`): they are
  developer tools and stay; none was read for a use that a player's launcher
  starts, beyond `player_sources.py`, `importer.py` and the mod builders.
- **The Linux side.** Nothing here was compiled for Linux; the reads this
  round removes are steps wave B would otherwise give a Linux start.
