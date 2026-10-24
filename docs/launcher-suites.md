# The launcher's three suites: graphics, audio, data

**Status:** BUILT (2026-10-12, the PSP-only round's group LT, branch `psp/lt`
off `psp/round` `668a9b67`). Driven by script against three fake manifests
(section 5), and once against a real PSP-only cache built with SI's branch;
**not seen by the owner**. Of the manifest keys it reads, `base_policy` is
written by SI's branch (`psp/si` `1d72d999`), and `exe_tables_from` by
nothing yet (IM's; section 3.1). The audio and data suites' engine half is
TG's, in wave two (section 7). **Engine half BUILT** (2026-10-12, group TG,
branch `psp/tg` off `psp/round` `958b01b9`, section 9): every member is now a
layer; headless self-tests only, **not played and not heard**.

The owner's calls of 2026-10-10 ([`psp-only-build.md`](psp-only-build.md) 6
and 7) asked for three toggles in the launcher, **audio**, **graphics** and
**data**. Each picks the PSP's version or the PlayStation's, as a suite: a
whole set of differences on or off together. Every option is listed, and an
option whose disc is missing is greyed with the discs that would enable it.
For a PSP-only install it works in reverse: the PSP's version is the base and
the PlayStation's is the greyed one. This page replaces the "PSP extras"
boxes ([`launcher-settings.md`](launcher-settings.md) 3.1, DIV-0086).

No game data here or in the code: build ids, layer names and counts only
(CLAUDE.md rule 1).

## 1. What the page is

The settings dialog (`IDD_CONFIG`, `src/launcher/launcher.rc`) is wider: the
left column is unchanged (Settings, Music), and the right column is the group
"Versions: the PlayStation's or the PSP's" ("PlayStation or PSP version" since
2026-10-24, issue 124, when every launcher string was rewritten in ASD-STE100
style). It holds a note and then one block
per suite:

- **The note** says what the cache's base is and which builds it was made
  from, read from its `manifest.toml`. It also covers the other cases: no cache
  named; a folder with no manifest; a manifest the page cannot read.
- **Per suite**, a row of two radio buttons, "PlayStation" and "PSP". The base's
  side is labelled "(the base)". A side the install cannot play is disabled.
- **Up to four member lines** under the row, one per member of the suite
  (`recipes/suites.toml`). A member is greyed when choosing its suite cannot
  play it.
- **A reason line** (four lines high) says why each greyed side or member is
  greyed. A reason that several members share is said once, with their names.

Captures, from `capture.py` in the LT scratch (`PrintWindow` of the real dialog
over the fake caches of section 5): `all_chinese.png` (PC + PSX + PSP, Chinese),
`all_partial.png` (an `opt=` from the PSP extras era), `psx_only.png`,
`psp_only.png`. They are not committed: they hold nothing of the game, but
captures go to the owner, not the tree.

The page reads the cache the box names **as it stands**. Typing or browsing
to another folder redraws the suites. When the environment sets
`BOF3X_CACHE`, that cache is read instead, because it wins at start-up
(`ConfigCacheDataRoot`). A layer counts as present the way the DLL counts
it (`ConfigOptInstalled`): `DAT\<layer>.*.DAT`, or the cache's
`opt\<layer>\dat\`, the latter not under `BOF3X_CACHE_DATA=0`.

## 2. The catalogue: `recipes/suites.toml`

The catalogue is hand-written. `tools/suites.py check` holds it to the
owner's calls and to the other recipes, and `importer.py check` runs that
check (CI). `tools/suites.py gen` turns it into the launcher's table
`launcher/suites.gen.h`. This is a build step in both `CMakeLists.txt` and
`tools/launcher_host/CMakeLists.txt`, so the toml is the only place to edit.

Each `[[suite]]` has an `id`, a `label` and a `what`. Each `[[suite.member]]`
has:

| Key | What |
|---|---|
| `id`, `label` | the member's name, and its line on the page |
| `kind` | where it lives in the cache: `opt` (`opt/<layer>/`), `bgm` (`base/bgm/`), `snd` (`base/snd/`), `exe` (`base/exe/`'s tables), `base` (`base/` chunks: SI's stand-ins), `code` (a rule in our code, PB's battle switches: no chunk, the ini's `psp=`) |
| `layer` | for `opt` only: the `recipes/opt.toml` layer |
| `code` | for `code` only: the switch's name in `psp=` (`duel-loss`, `myria-item`) |
| `from` | the builds it comes from (fixtures.toml ids; PSP discs only) |
| `over_psx` | how it reaches the game beside a PlayStation base: `layer` (an `opt/` layer, selectable today), `engine` (its engine half is TG's; greyed), `switch` (a `code` member: a `psp=` switch, selectable beside any base), `none` (no layer exists or is planned; greyed) |
| `why` | the greyed reason, exactly when `over_psx` is not `layer` |

`check` refuses a catalogue that breaks any of these:

- the three suites are exactly `graphics`, `audio`, `data`, in that order;
- every `from` is a PSP disc in fixtures.toml;
- an `opt` member's `from` is exactly the builds that its layer's chunks are
  `on` in `opt.toml`, so the page never names a disc that cannot make the
  layer;
- every `opt.toml` layer but `area4-walls` is in exactly one suite.
  `area4-walls` is a repair, on by default (DIV-0080), and in no suite;
- a suite has 1 to 4 members (the dialog's places);
- the text is printable ASCII.

Measured: `python tools/suites.py check` gives 0 errors, and
`python tools/importer.py check` exits 0 with the suites in it.

The catalogue as built:

| Suite | Member | Kind | From | Beside a PlayStation base |
|---|---|---|---|---|
| graphics | Stallion's palettes | `opt` `psp-art` | psp-jp, psp-eu | a layer, today |
| | area tiles and palettes | `opt` `psp-tiles` | psp-jp, psp-eu | a layer, today |
| | map bands | `opt` `psp-maps` | psp-jp, psp-eu | a layer, today |
| audio | music | `bgm` | psp-jp, psp-eu | the engine half (TG) |
| | effects, voices, jingles | `snd` | psp-jp, psp-eu | the engine half (TG) |
| data | level table and item 87 | `exe` | psp-jp, psp-eu | the engine half (TG) |
| | English renames | `opt` `psp-names-en-150` | psp-eu | a layer, today |
| | Japanese renames | `opt` `psp-names-ja-JP` | psp-jp | a layer, today |
| | Ryu's forms (P8) | `base` | psp-jp, psp-eu | none: no layer yet (P8 unrendered) |
| | fight 27: a loss goes on | `code` `duel-loss` | psp-jp, psp-eu | a switch, `psp=` (DIV-0108) |
| | fight 55: Myria's item | `code` `myria-item` | psp-jp, psp-eu | a switch, `psp=` (DIV-0108) |

The names layers, P8 and the button page are placed by the proposal of
section 6, which is the owner's to settle.

## 3. Availability

### 3.1 What the page reads from the manifest

`ManifestParse` (`src/launcher/suites.cpp`) reads the subset of TOML that
`importer.py write_manifest` writes:

- `[[source]] build`: the builds the cache was made from, distinct, in order;
- `[build] base_policy`: `"psp"` or `"psx"`. SI writes it (the round's
  contract, [`psp-only-round.md`](psp-only-round.md) 4): on `psp/si`
  `1d72d999`, `psp_standins.manifest_lines` puts the table right after
  `[meta]` in every cache, with `standins_sha256` under `psp` (not read
  here). **Absent means `psx`**: a cache from before this round has no
  `[build]` table;
- `[build] exe_tables_from`: one build id or a one-line list of them. IM
  writes it. When absent, the page uses `[exe] build`;
- the `bgm` rows' first source build (`base/bgm/`), and `[snd] build`
  (`base/snd/`): what a PSP base's audio members show.

A `base_policy` that is neither string, or an `exe_tables_from` that is
neither a build nor a list of them, **is not guessed**. The note names the
key and its value, and every side of every suite is greyed. The page reads
the keys, never the chunk rows, as the contract says.

### 3.2 Beside a PlayStation base (`base_policy` absent or `psx`)

- **The PlayStation's side** is the base: always enabled. With no PSP layer in
  `opt=` it is the side shown checked.
- **A member** is available when all of these hold:
  - the manifest has one of its `from` builds;
  - `over_psx` is `layer`;
  - its layer is present.

  Otherwise it is greyed, with the first reason that applies:
  - `needs psp-jp or psp-eu` (no such build in the cache);
  - its `why` (the engine half, or no layer);
  - `not in the cache (build with --opt <layer>)`.

  With no cache named, an installed layer is its own evidence of the disc.
  That is the PSP extras' install path (`importer.py install --opt`), and it
  still works.
- **A text layer** available under another language (the names) stays
  available, with the note `plays under English text only` (or Japanese).
  This is the DLL's test, `SamePrimaryLanguage` on the primary subtag: the
  English renames play under `en-US` and `en-150` alike. Choosing the suite
  still writes the layer, and `ConfigOptPlayable` leaves it off at start while
  the language differs, as it did for the PSP extras' hidden box.
- **The PSP's side** is enabled when at least one member is available. When
  it is greyed, the reason is one of these:
  - "The PSP's version needs a PSP disc (`psp-jp or psp-eu`)", when no
    member's build is in the cache. With no cache named it adds "in a cache
    named in Cache folder".
  - "Nothing of the PSP's plays beside this base yet", when the disc is
    there but every member is greyed. This is audio's case on PC + PSX +
    PSP until TG.

### 3.3 On a PSP base (`base_policy = "psp"`): the inverse

- **The PSP's side** is the base, labelled "PSP (the base)", checked and
  enabled.
- **The PlayStation's side** is greyed for every suite. The reason is "The
  PlayStation's version needs a PlayStation disc (`psx-jp, psx-us, psx-fr,
  psx-de or psx-eu-en`)". If the cache somehow has a PlayStation or PC source
  as well, the reason is "The PlayStation's version over a PSP base is not
  built yet." No layer of PlayStation data over a PSP base exists or is
  planned in this round.
- **Members** show whether the cache holds the PSP's version:
  - a member whose `from` builds are not among the sources is not this
    base's: "needs psp-eu" for the English renames on a PSP-JP base;
  - music needs `bgm` rows from a PSP build ("no songs in this cache" before
    MS stage B);
  - effects need `[snd]` from a PSP build;
  - the tables need `exe_tables_from` naming a PSP build;
  - the rest are the base's.
- **Nothing is written.** A click on a greyed side does nothing. The PSP's
  side changes nothing either, because it is already the base.

## 4. The ini: a suite is its layers in `opt=`

The suites have **no line of their own** in `bof3x.ini`. A suite is the
`opt/` layers it moves, so the page writes them as today's `opt=`
(DIV-0086). It does this through the same `ConfigOptEdit` that the boxes used
(`SuitesOptEdit`, `suites.cpp`):

| On Play | `opt=` becomes |
|---|---|
| no suite's choice changed | the line byte for byte, valid or partial |
| the PSP's chosen, beside a PlayStation base | the line plus every member layer present (any language) |
| the PlayStation's chosen | the line less every member layer of the suite, present or not |
| anything on a PSP base, or a greyed side | unchanged |

The written list orders the suites' layers in `suites.toml`'s order
(`SuiteLayers`), then `kOptDefault` (`area4-walls`), then any other name the
line had, such as a mod's layer. An empty `opt=` contributes the default
layers, so the walls stay on. `none` is written when nothing is left. These
are the PSP extras' rules unchanged: `kPspLayers` is gone, and the suites'
order is the same five names in the same order.

**What a suite shows** is read from the line:
- the PSP's, when every member layer present is in it;
- the PlayStation's, when no member layer is;
- **neither radio** (partial) otherwise. An `opt=psp-art` from the PSP extras
  era is partial for graphics, and the reason line says "kept as written
  until a version is chosen".

**`music=`** is not a suite. It stays the choice between the cache's songs
(`seq`, the default) and the PC's MP3s (`mp3`), DIV-0087. On a PSP base the
cache's songs are the PSP's (MS stage B writes `base/bgm/` from a PSP source),
so `music=seq` plays the PSP's music with no new value.

**Since 2026-10-21 (DIV-0150)** `music=` names a source - `psx`, `psp` or
`pc` - and the music is no longer the audio suite's: `psp-music` left the
catalogue (`recipes/psp-layers.toml` marks it `suite = "music"`, the Music
box's), and the engine reads it by the chosen source, not through `opt=`. The
audio suite is the PSP's effects and voices (`psp-snd`); the 11 recorded
streams in that layer follow the Music box too.

**`psp=`** carries the data suite's two `code` members (DIV-0108):
`SuitesPspEdit` writes it on `SuitesOptEdit`'s terms, and a suite's side
reads both lines. Unset, both switches are on for a PSP-only cache and off
otherwise ([`psp-battle-changes.md`](psp-battle-changes.md)). Beside a PlayStation
base, the audio suite's PSP side will be TG's `opt/` layers for `base/bgm/`
and `base/snd/` in `opt=`, not a third `music=` value. So the suites map onto
today's two lines, and an ini written by this launcher is read by the last
one unchanged.

## 5. Measured

All without a game run (the brief's rule), on this machine, 2026-10-12.

| What | Command | Result |
|---|---|---|
| the catalogue | `python tools/suites.py check`; `python tools/importer.py check` | 0 errors; exit 0 (742 files, 3,582 chunks, `opt.toml` 6 layers, 0 errors) |
| the i686 build | `cmake --preset i686 && cmake --build build` | clean, no warning in `src/launcher/` |
| host suite | `cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++`, `config_tests.exe` | **114 checks, 0 failures** (at `668a9b67`: 50 checks, 2 failures, below) |
| the dialog | `python tools/launcher_host/dialog_test.py build/bof3x-launcher.exe <install>/BOF3.exe` | **35 checks, all ok** (at `668a9b67`: no dialog, below) |
| the checker refuses | `neg_suites.py` (LT scratch): seven mutations of the toml (a fourth suite, a PSX build in `from`, `from` not the layer's `on`, a layer in no suite, `engine` without `why`, an `opt` member not `layer`, the default layer in a suite) | 7 of 7 refused; the toml as committed 0 errors |
| PC and PSX paths | `refs_lt.sh` (LT scratch): JP + PC, US alone and European alone built with this branch's tools against the coordinator's `ref_*` (built with `668a9b67`'s); every file but `manifest.toml` hashed, and the manifest diffed without its `built =` line | JP + PC 2,126 files, US 1,873, European 1,865: every tree identical, 0 manifest lines differ; `recipes/exe-gaps.tsv` untouched (`git diff 668a9b67 -- recipes/exe-gaps.tsv` empty) |
| hosted self-test | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' <copy>/bof3x-launcher.exe --game <install> --no-config`, an ini-less copy | exit 0 (the DLL is unchanged; the launcher's path is exercised) |
| exe host self-test, PSX disc-only | `importer.py install --base --cache ref_us --game <scratch>/g_us`, then `BOF3X_SELFTEST_ONLY=1`, `BOF3X_CACHE` = `BOF3X_EXEIMAGE` = `ref_us`, the copy with `--no-config` | exit 0; `inject: 10087 ours`; `self-test only: done`; `exe_own: 17 regions ... filled over a disc-built image` |
| ledger | `python tools/ledger_check.py` | 101 entries, 0 errors, 2 notes (DIV-0107 is not in the ledger yet) |

The host suite's new checks (`config_tests.cpp`, `TestManifest` and
`TestSuites`) run the three fake manifests in memory:

- **PSP-only:** every suite is the PSP's with the PlayStation's greyed, and
  the reason names the five PlayStation discs. A PSP-only cache without
  `base/bgm` says "no songs in this cache". A choice writes nothing.
- **PSX-only:** every PSP side is greyed, naming `psp-jp or psp-eu`. The
  English renames need `psp-eu`. A greyed choice writes nothing.
- **PC + PSX + PSP**, holding `psp-art`, `psp-tiles` and the English renames:
  - graphics is offered for the two layers built, and `psp-maps` is greyed
    as not built;
  - audio is greyed, with the engine-half reason;
  - data is offered for the renames alone, with the tables (TG) and P8 (no
    layer) greyed;
  - the `opt=` edits: untouched; the PSP's graphics over an empty line
    (`psp-art,psp-tiles,area4-walls`); graphics and data; back to the
    PlayStation's (the walls kept; `none` over a list without them); a
    partial line kept byte for byte, and made whole by a choice; an
    uninstalled member and a mod's name.
- **No cache:** a `DAT\` layer offers its suite.
- **A bad `base_policy`:** everything greyed.
- **The manifest reader:** lists and strings, an older manifest without
  `[build]`, three malformed values, `[exe]` and `[snd]` builds not counted
  as sources.

`dialog_test.py` drives the real dialog over the three fake caches on disk:

- **PC + PSX + PSP** (with the player's `BOF3.exe` copied):
  - an untouched dialog leaves the ini byte-identical;
  - the radios, member lines and reasons are as above;
  - clicking the PSP's graphics writes `psp-art,psp-tiles,area4-walls`, and
    clicking back writes `area4-walls`;
  - a PSP extras `opt=` shows partial and is kept;
  - `en-US` with the PSP's data over `opt=none` writes `psp-names-en-150`,
    the renames' language note goes, and the greyed audio click writes
    nothing;
  - the Music and Cache folder boxes behave as before, and a cache typed in
    redraws the suites ("No manifest.toml");
  - the PSX-only cache named greys every PSP side.
- **PSX-only install, no `BOF3.exe`:** the dialog opens, every PSP side is
  greyed, and the launcher takes `.data` from the cache.
- **PSP-only install, no `BOF3.exe`:** the note says the PSP's base, every
  suite is the PSP's with the PlayStation's greyed and the five discs named,
  every member is present but the English renames ("needs psp-eu": the fake cache is PSP-JP's), and clicks write nothing.
- **No cache, `psp-art` in `DAT\`:** graphics is offered, and choosing it
  writes `psp-art,area4-walls`.

Two defects in the base, found and fixed here because they are the launcher
host suite's own:

- **`dialog_test.py` could not run at `668a9b67`.** Since state 3 the
  launcher starts `bof3x.exe` by default, and the test copied only the
  launcher and a dummy DLL. The launcher stopped on "bof3x.exe is missing"
  before any dialog: `AssertionError: no dialog`, measured with the base
  build. The test now places a copy of Windows' `hostname.exe` as
  `bof3x.exe`, so Play starts nothing of the game. It also kills its own
  launcher by handle when a step fails, so a message box is never left
  open.
- **`config_tests.exe` failed 2 of 50 at `668a9b67`** (built from `git
  archive 668a9b67 src tools/launcher_host`). The code-page test set ANSI
  bytes straight into `Config::cache`, which state 3 made UTF-8. It now
  goes through `ConfigCacheFromIni`, as `ConfigLoad` does.

## 6. The membership proposals (the owner's calls, `psp-only-round.md` 7 item 3)

1. **The PSP's names layers: under data, as proposed, or the language
   choice?** Proposed: **data**. The renames are rows of the PSP's own item
   and ability tables, the same tables the data suite's level table and
   item 87 come from, and they are already `opt/` layers that play today.
   Under the language choice they would need a fourth control, or language
   entries multiplied by builds. The cost is visible: under the Chinese the
   member reads "plays under English text only", and the layer waits in
   `opt=` until an English language is chosen. The other answer is to take
   both members out of `suites.toml` and offer them beside the Language box;
   `check` then wants them in no suite, which is one edit to its rule.
2. **The PSP's button-label page: graphics, or left out?** Proposed: **left
   out** beside a PlayStation base. It labels the PSP's shoulder buttons
   ([`psp-only-build.md`](psp-only-build.md) 4, "probably not wanted"), and
   no layer exists for it. On a PSP base it is part of SI's stand-ins, so the
   base has it whatever the suite says. It is listed nowhere on the page.
   If the owner wants it in graphics, it is a fifth graphics member (`check`
   allows four; the dialog has four places) or a new layer that replaces
   one.
3. **P8, Ryu's forms: data or graphics?** Proposed: **data**, as listed
   ("Ryu's forms (P8)", greyed: no layer yet). It is form data, `RYUD` and
   the arenas carrying it, and the 13,770 bytes are unrendered
   ([`opt-layers.md`](opt-layers.md) 8). If a render shows it is only how
   the forms look, it moves to graphics: one `suite` line in the toml.

## 7. What TG lifts (wave two)

*Done 2026-10-12: section 9. The catalogue table in section 2 and the
expectations in section 5 are as LT built them; section 9.5 is the catalogue
now.*

TG builds the engine half: `opt/` layers for `base/bgm/` and `base/snd/`, and
a chunk kind for the exe tables. When each exists:

- the member's `over_psx` becomes `layer`, with its `layer` named, and
  `kind` stays as it is (or becomes `opt`, if the layer is an ordinary
  `opt/` container);
- `why` goes;
- `opt.toml`, or TG's own recipe, carries the layer, and `check`'s rule that
  `from` equals the layer's `on` builds applies to it.

The page then offers it with no code change: `SuitesEvaluate` reads only the
catalogue and the manifest. If TG's layers live outside `opt.toml`, `check`
needs to read that recipe too. That is a few lines in `suites.py`, and TG's
to add.

## 8. Not verified

- **`exe_tables_from` as IM writes it.** It is read as the round doc fixes
  it: a key of the `[build]` table, a build id or a list. If IM writes
  another shape, the page greys everything and names the key: loud, not
  wrong, but a one-line change in `ManifestParse`. Until IM, the tables
  member falls back to `[exe] build`.
- **A complete PSP-only cache.** Checked once against SI's real cache:
  `importer.py build --source <PSP-JP iso>` run from a `git archive` of
  `psp/si` `1d72d999` (`real_si.py`, LT scratch). The note read "This
  install's base is the PSP's; the cache is built from psp-jp". Every suite
  was the PSP's with the PlayStation's greyed and the five discs named.
  Music read "no songs in this cache" (no `base/bgm` before MS stage B). The
  English renames read "needs psp-eu". The rest were the base's, and an
  untouched Play left `opt=` empty. The `bgm` row form for a PSP source
  (`psp-jp:<how>:...`) is still assumed from the PSX's; MS writes it. The
  fake manifests are modelled on the owner's
  `analysis/cache/pc-plus-us/manifest.toml`, on `write_manifest`, and on
  SI's `[build]` table.
- **The owner's eye.** The layout: the dialog is now 560 x 309 dialog units,
  about 996 x 618 pixels at 96 DPI as captured. Also the wording. The left
  column's two longest notes ("...draws with Direct3D 11.", "...stretches to
  the window's height.") are clipped by their own control widths. This is
  older than this page: the same capture of the `668a9b67` launcher shows
  both clipped. It is not changed here.
- **The pad.** The radios carry `WS_TABSTOP`, so the pad's up and down
  reach them and cross clicks them (`padnav`), but this was not driven with a
  pad.
- **High DPI** and a narrow screen: not tried.
- Whether the owner wants the suites to carry anything beyond the layers that
  exist (P13's logo, P14's title menu, P15's wording, `SCENA17`): none has a
  layer, so none is listed.

## 9. The engine half (TG, wave two)

The PSP-only round's group TG, branch `psp/tg` off the round tip `958b01b9`
(wave one merged). Every member section 7 left greyed is now an `opt/` layer,
so beside a PlayStation base the page offers it as it offers the graphics:
`recipes/suites.toml` changed only in the four members' `over_psx` / `layer`
/ `why` fields, and `SuitesEvaluate` is unchanged. The owner's later call of
the same day adds a fifth layer, the battle chord as the PSP plays it, which
is not a suite's but the Jukebox's (9.6). No game data here, in the recipe or
in the code: names, counts, addresses and hashes (CLAUDE.md rule 1).

### 9.1 The layers: `recipes/psp-layers.toml`, `tools/psp_layers.py`

`recipes/opt.toml` is generated and holds DAT chunks cut sub-range by
sub-range, so these layers have a hand-written recipe of their own. It holds
names, parts and rules only. A layer's **part** is the tree under
`opt/<name>/` and the engine seam that reads it:

| Layer | Part | What it holds | Rule (no copied value) | Built from |
|---|---|---|---|---|
| `psp-music` | `bgm` | `opt/psp-music/bgm/NNN.DAT` and `bgm/bank/NAME.DAT`, `base/bgm`'s form | `ppms.build` (MS stage B; MB's banks), moved under the layer | the first PSP disc among the sources |
| `psp-snd` | `snd` | `opt/psp-snd/snd/NAME.DAT`: 880 effects and voices as WAVs, the 11 jingles as MP3s | `at3.build`, the player's ffmpeg (PN) | the same; no ffmpeg, or a clip skipped: refused (a layer is whole or not at all) |
| `psp-tables` | `exe` | `opt/psp-tables/exe/place.toml` (ranges, sha256) and `place.bin` (their bytes) | every record of `tables.toml`'s `exp_table` and `consumables`, by `symbols.toml`'s PC address, count and stride, less a record's name field (a language's glyph codes); bytes from the PSP-built image (`exe_tables.build_from`), each one a class a disc gave (`map`, `place`, `table`, `widen`) | the same |
| `psp-ryu-forms` | `dat` | 19 containers of one chunk each: `RYUD00..03` and the 15 `BPLD` / `BRTD` arenas (P8) | `recipes/psp-standins.toml`'s rows of the classes `type1`, `pc-byte`, `pc-hold` whose output is not the PC's chunk (SI's stand-ins, hashed against SI's rows), at the PC's kind, tag and size | the same |
| `psp-chord-one-tone` | `dat` | 16 containers of one kind-2 chunk: the shared battle effects bank in the PSP's own one-tone form (9.6) | every kind-2 chunk whose PSP group is the case `vag.psp_dropped_tone` restores, built by `vag.psp_vab` without the cue table; the restored form is held to the PC's chunk's hash, the one-tone form must differ | built **whenever a PSP disc is a source, on any base** (`auto`) |

`importer.py build --opt NAME` builds the first four, beside a PlayStation
base only: under `base_policy = "psp"` the base is the PSP's already, and a
`bgm` or `snd` layer also wants a PSX disc among the sources (without one
`base/bgm` and `base/snd` are the PSP's, or absent). Each refusal names why.
The manifest gets a `[psp_layers]` table after everything else the importer
writes (`[fmv]` included): `recipe_sha256` and one row a file, `[layer, path,
source, sha256]`. Neither the launcher's `ManifestParse` nor `dat_cache`'s
parser reads it (the first has no key it knows there; the second stops at
`[cache] assets`' closing bracket).

The importer's call sites (`tools/importer.py`, all one-liners into
`psp_layers`): `build` splits `--opt` (`take`) and builds after the
manifest; `verify_opt` hands a TG layer to `verify_layer`; `install`
refuses `--opt` of a `bgm`, `snd` or `exe` layer (`refuse_install`: the
engine reads those from the cache only); the `--opt` help names them.
`verify_layer` checks every file against the manifest's row, then by part:
a `bgm` layer against `fixtures/bgm-psp.tsv` (what `ppms.py` makes of that
build), an `exe` layer's ranges against the rule and its image against
`recipes/exe.toml`'s hash for the build, a P8 container's one chunk against
the PC's kind, tag and size and SI's hash, a chord container's one chunk
against a bank tag of that container. `psp_layers.check` (run by `suites.py
check`, so by `importer.py check`): known parts, PSP builds, names the
engine takes and `opt.toml` does not have, the exe rule's ranges in order
inside `.data`, P8's rule selecting exactly 19 rows.

`tools/suites.py check` reads the recipe beside `opt.toml`: a member whose
layer is TG's has `over_psx = "layer"`, its `from` the layer's, its kind the
layer's part (`bgm`, `snd`, `exe`; a `dat` layer's member is `opt`, or `base`
where a PSP base stands it in, P8); every TG layer is in exactly one suite
but the Jukebox's (`suite = "jukebox"`), which must be in none.

### 9.2 The engine's seams (DIV-0109 to DIV-0113)

All are reached only when `BOF3X_OPT` (the ini's `opt=`) names the layer and
the cache (`BOF3X_CACHE`, not under `BOF3X_CACHE_DATA=0`) holds its part:
`opt=` off is today's behaviour, byte for byte.

- **Which layers exist** (`dat_load.cpp` `ReadOptLayers`): a layer counts
  when `DAT\<layer>.*.DAT` exists, or the cache has `opt\<layer>\dat\`, or
  (new) `opt\<layer>\bgm\*.DAT`, `snd\*.DAT` or `exe\place.toml`
  (`dat_cache::HasLayerPart`); the launcher's `CacheHasLayer` asks the same
  (`config.cpp`), so the page and the DLL agree. After the check,
  `dat_cache::SetOptLayers` hands the list to the seams below.
  **`kOptMax` is 16** (it was 8): all three suites' PSP sides with the walls
  name nine layers, eleven with the other renames and the chord;
  `ConfigOptValid` follows (`more than 16 layers`).
- **DIV-0109, the music** (`music_seq.cpp`, DIV-0087's `Music_LoadFile` seam):
  `Arm` takes the named layers with a `bgm` part, checks every song and bank
  of each at start-up as it checks `base\bgm`, and `LoadFile` looks for a
  track in the last named layer first, then the earlier ones, then
  `base\bgm`, then the MP3. A song's bank comes from **its own directory's**
  `bank\`, and the bank held is now known by its file path, not its name:
  the PSP's banks are named as the PSX's, and keyed by name a base song
  would have played on the layer's bank (the self-test's control case).
  `BOF3X_MUSIC=mp3` still plays every MP3, with a line saying the layer's
  songs are not played either. Arming no longer needs `base\bgm` when a
  named layer has songs. A root too long for `<root>\opt\<layer>\bgm\bank\`
  + 15 + `.DAT` is a Fatal naming the layer.
- **DIV-0110, effects, voices, jingles** (`dat_cache.cpp` `SoundPath`, DIV-0089's
  `Sound_LoadStream` seam): armed, the named layers with a `snd` part, last
  first, then `base\snd`, then `SND\`. A layer without a file leaves it to
  the one under it. Over a PSX-only install this brings the 11 jingles a
  PSX-only install lacks (`psp-install.md` 3; the round's section 7 item 5).
- **DIV-0111, the exe tables** (`exe_image.cpp` `ExeImage_LayOptTables`, called
  by the exe host's `CopyData` after `.data` is in place - the file's, or
  `BOF3X_EXEIMAGE`'s and filled - and before `ExeImage_CompareWithFile`):
  each named layer with `exe\place.toml`, in `BOF3X_OPT`'s order, is laid
  over `.data`. Refused (Fatal, naming the file): a `place.toml` not in
  `psp_layers.py`'s shape, ranges out of order or outside `.data`, a byte
  count that differs, a `place.bin` whose sha256 is not the toml's, and,
  over a disc-built image, a range on a class that must equal the PC's
  (`layout`, `own`, `keys`, ...). The log names the bytes that changed.
  **The hosted build refuses it**: `ReadOptLayers` stops with "only
  bof3x.exe lays (the exe host); the hosted build (--host bof3) runs
  Capcom's .data as loaded". The hosted build is the A/B and shadow
  reference, and its `.data` stays the loader's.
- **DIV-0112, P8** (no new seam): an ordinary DAT layer through DIV-0086's walk.
  Each chunk is kind 0 at the arena's tag, the whole arena; the arenas' tag
  `0x10000` resets `Gfx_UploadQueueCount` again, which changes nothing,
  since in each of the 19 containers the only chunks after the arena are
  banks (`recipes/pc-zh.toml`; a bank queues no upload).
- **DIV-0113, a layer's bank** (`save_menu.cpp` `Snd_LoadBankFile`, through
  `dat_load.cpp` `DatLoad_OptLayerChunks`): the original loads a
  container's kind-2 chunks from the shipped file alone, so a layer's bank
  would apply when `LoadDatFile` loads the container and not when a scene
  loader asks for its banks. Now, after the shipped file, each named layer's
  file for that container (`LoadDatFile`'s precedence) gives its kind-2
  chunks too; `Snd_LoadBank` frees and replaces a slot's bank, as when the
  original loads one over another. Only when a named layer carries a bank at
  all (read from its files' headers at injection, logged): with none, the
  function is the original's, and its shadow comparison stays like for
  like. (A first build without that gate crashed the hosted `'*'` run in
  `Snd_LoadBankFile`'s shadow fuzz, `area4-walls` being named by default:
  the fuzz's names walked the real file layer.)

**The PSX's banks need nothing else.** MB measured the PSP's 901 effects
banks equal to the PSX-JP chunks (901 of 901 on each PSP, with the
dropped-tone rule; 885 without), and the PSX-JP chunks are the PC's
(`psp-music-banks.md` 5): there is no other PSP bank to lay. The one
difference is the chord, which the owner made a toggle (9.6).

### 9.3 The Jukebox's PSP column

`music_player.cpp` `ListPsp`: when the cache's `base\bgm` is not the PSP's,
the PSP source lists `<cache>\opt\<music layer>\bgm\` (the layer named by
`suites.toml`'s `bgm` member), and every song plays on its own directory's
`bank\` (`Entry::bank_dir`), so the PSX and PSP columns sit side by side on
a PC + PSX + PSP cache. A PSP-only cache is as before (`base\bgm`).

### 9.4 Measured

All on this machine, 2026-10-12, headless (no game window). Scripts in
`<scratchpad>/psp_tg/`.

| What | Command | Result |
|---|---|---|
| the recipes | `python tools/psp_layers.py check`; `python tools/suites.py check`; `python tools/importer.py check` | 5 layers, 0 errors; 0 errors; exit 0 (742 files, 3,582 chunks, `opt.toml` 6 layers, 0 errors) |
| the checkers refuse | `neg_tg.py`: 12 mutations (a member back to `engine`, the wrong kind, `from` not the layer's, a `bgm` member on an `opt.toml` layer, the chord layer in a suite, a TG layer in no suite, one from a PSX disc, an unknown part, P8's count, an unknown table, a TG name that is an `opt.toml` one, a banks rule) | 12 of 12 refused; the committed files 0 errors |
| PC + US + PSP-EU | `build` with `--opt psp-music --opt psp-snd --opt psp-tables --opt psp-ryu-forms`, against the same build by `958b01b9`'s tools (`treecmp.py`) | 2,126 files identical; 1,175 new, all under `opt/`: `psp-music` 247 (166 songs, 81 banks), `psp-snd` 891, `psp-tables` 2, `psp-ryu-forms` 19, `psp-chord-one-tone` 16; the manifest: 0 lines removed, 1,182 added (the `[psp_layers]` table); 218 s against 150 s |
| `verify` on it | `importer.py verify --cache` | 742 of 742 composed, 0 problems; each of the five TG layers 0 problems, `area4-walls` as before |
| PSP-EU alone | `build`, against `958b01b9`'s tools | 1,882 files identical; 16 new (`psp-chord-one-tone`, built on every base); manifest 0 removed, 23 added; `verify` 0 problems |
| the PC and PSX paths | JP + PC, US alone, European alone against the coordinator's `ref_*` (built with `668a9b67`'s tools) | 2,125, 1,872 and 1,864 files: every one identical; the manifests differ only by the `[build]` table (`base_policy`, `exe_tables_from`) that wave one (SI, IM) added, no line of TG's; `recipes/exe-gaps.tsv` untouched |
| the exe tables, by bytes | `exe_cmp.py`: `place.bin` laid over each image at `place.toml`'s ranges, as `ExeImage_LayOptTables` lays it, against the PSP-built image | over the PC's, the US disc's and the European disc's images: **250 bytes change**, 0 outside the ranges (249 in `exp_table` roster index 6, 1 in consumable 87); in the ranges every byte equals the PSP image's; the layer is 93 ranges, 6,096 bytes |
| PSP-JP against PSP-EU | the same layers cut from each disc (`layers_only.py`) | `psp-tables` and `psp-ryu-forms` byte-identical |
| refusals at build | `--opt psp-music` from PSP-EU alone; `--opt psp-tables` from the US disc alone | each refused in a second, naming why (a PSP base; no PSP disc) |
| install | `install --opt psp-music`; `install --opt psp-ryu-forms --opt psp-chord-one-tone` | refused, naming `cache=` / `opt=`; 35 files as `DAT\<layer>.*.DAT` |
| the i686 build | `cmake --preset i686 && cmake --build build` | clean, no warning in a file changed here |
| hosted self-test | `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, an ini-less copy, the PC install (which names `area4-walls` by default) | exit 0, `inject: 10087 ours`; the DIV-0109, DIV-0110 and DIV-0111 self-tests logged |
| hosted, the tables named | `--host bof3`, `BOF3X_OPT=psp-tables` | exit 3: "DIV-0111: ... only bof3x.exe lays (the exe host)" |
| hosted, the other layers | `--host bof3`, `BOF3X_OPT=psp-music,psp-snd,psp-ryu-forms,psp-chord-one-tone`, the PC + US + PSP-EU cache | exit 0 |
| exe host, PSX disc-only install | `install --base` from a US + PSP-EU cache; `BOF3X_CACHE` = that cache (the launcher sets `BOF3X_EXEIMAGE`); `BOF3X_OPT` each of `none`, `psp-music`, `psp-snd`, `psp-tables`, `psp-ryu-forms`, `psp-chord-one-tone`, then all five with `area4-walls` | exit 0 every time; `exe_own: 17 regions ... filled over a disc-built image`; `inject: 10087 ours`; with the tables: `DIV-0111 ... 250 of them differ` over `psx-us`; with the music: the layer's 166 songs and 81 banks checked, then base's |
| exe host, a PC install | the PC install, the PC + US + PSP-EU cache, all five layers | exit 0; `DIV-0111 ... 250 of them differ` over the file's `.data` |
| exe host, a damaged layer | one byte of `place.bin` flipped (restored after) | exit 3: "place.bin is not the file place.toml's sha256 names" |
| launcher host suite | `cmake -S tools/launcher_host -B build/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++`, `config_tests.exe` | **136 checks, 0 failures** (114 before): the parts counted as the DLL counts them, every suite offered with TG's layers, `opt=` of nine and eleven layers valid and seventeen refused, the chord box's six states |
| ledger | `python tools/ledger_check.py` | 101 entries, 0 errors, 2 notes (DIV-0109..0113 and DIV-0107 are not in the ledger yet) |

### 9.5 The suites now

| Suite | Member | Beside a PlayStation base | On a PSP base |
|---|---|---|---|
| audio | music | layer `psp-music` | `base/bgm` |
| | effects, voices, jingles | layer `psp-snd` | `base/snd` |
| data | level table and item 87 | layer `psp-tables` (the exe host only) | `base/exe` (`exe_tables_from`) |
| | Ryu's forms (P8) | layer `psp-ryu-forms` | the base's stand-ins (kind `base`) |

The owner's calls of 2026-10-12 (relayed by the coordinator): LT's three
membership proposals stand as built (section 6: the renames and P8 under
data, the button page left out), and the PSP music's restored stereo width
is fine (no narrow-pan bank form).

### 9.6 The battle chord (the owner, 2026-10-12)

One effects bank, shared by 16 containers (`BPLD` / `BPLU` 015, 025, 056,
257, 457, 567, 578 and `BRTD` / `BRTU` 457), keys a chord whose second tone
the PSP's bank dropped; MB's rule restores it from the PSP's own `pBVC`, so
every base plays the PlayStation's chord (`psp-music-banks.md` 5). The owner
asked for it to be toggle-able, on the Jukebox screen:

- **The layer** `psp-chord-one-tone`: those 16 bank chunks as the PSP plays
  them, one tone. Built on every cache with a PSP source, PSX or PSP base.
- **The box** (`IDD_MUSIC`, under the song list): "Battle chord as the PSP
  plays it (one tone)", in a group "In the game (a sound effect, not a
  song)". Its state is `opt=`'s: checking it adds the layer through
  `ConfigOptEdit`, unchecking removes it, and the line is written with the
  rest on Play. **Off by default**: the restored chord on every base. Greyed,
  with the reason under it, when no cache is named, when the cache has no
  PSP source (naming `psp-jp or psp-eu`), or when it has one but not the
  layer (built before this round). `ChordEvaluate` (`suites.cpp`) decides,
  pure, for the host tests.
- **The engine**: the layer's banks reach `Snd_LoadBank` from both loaders
  (DIV-0113).
- **Not heard**, and the Jukebox does not play it: it is an effect, not a
  song. The box only sets what the game plays.

### 9.7 Not verified

- **Nothing played or heard.** No live run: the PSP's songs and effects
  under a PSX base, the chord, the level table at a level-up of roster index
  6, consumable 87, Ryu's forms in battle. The self-tests drive the seams on
  synthetic files; the exe host's self-test lays the real tables.
- **The Jukebox and the dialog by script** (`dialog_test.py`): the
  coordinator's end batch. The new controls compile and the host suite
  checks `ChordEvaluate`; the dialog itself was not opened. One expectation
  of `dialog_test.py` was changed to match: audio's greyed reason on the PC +
  PSX + PSP fake cache is now `--opt psp-music` (not built), not "the engine
  half (TG)". It does not drive the Jukebox.
- **`psp-snd` over a PC install's `SND\`**: the cache's files win where both
  exist (DIV-0089), so the PC's own MP3 jingles are replaced by the PSP's
  re-encoded ones when the layer is on. Measured only as paths.
- **Start-up time** with a `bgm` layer: Arm parses another 247 files.
- **A cache built by `--opt` with PSP-JP and PSP-EU both**: the first PSP
  disc in the player's order is used; JP and EU gave identical `psp-tables`
  and `psp-ryu-forms` layers (9.4).

## 10. The code review's fixes (2026-10-12, group RF)

The owner's code review of the round tip (`psp/round` `88817a07`), fixed on
`psp/review-fixes`. Headless only; scripts in `<scratchpad>/psp_rf/`. No game
data: names, counts and hashes.

| # | Finding | What changed | The check that catches it | Result |
|---|---|---|---|---|
| 1 | Hosted runs stopped when the data suite was the PSP's (`psp-tables` in `opt=`, refused by the hosted build at start) | `ConfigApplyEnvironment(..., hosted)`: the launcher leaves out of the `BOF3X_OPT` it sets every layer whose part the hosted build refuses (`kHostedRefusesParts` = `exe`), by the recipe's part (`suites.gen.h` `kLayerParts`, from `recipes/psp-layers.toml`) or the cache tree's (`ConfigOptHosted`), with one stderr line naming them. A hand-set `BOF3X_OPT` is untouched: the engine's refusal stays | host suite `TestHostedOpt`: the data suite's PSP side written over an empty `opt=` (`psp-tables,psp-names-en-150,psp-ryu-forms`), then `BOF3X_OPT` for bof3x.exe (`psp-tables,psp-ryu-forms`) and for the hosted build (`psp-ryu-forms`); a mod's layer with `exe\place.toml` dropped by its tree; `run1.sh`: the hosted `'*'` self-test through the launcher with that ini | host suite passes; `run1.sh` (the PC install, the PC + US + PSP-EU cache): exit 0, `inject: 10087 ours`, the launcher's line "cannot take psp-tables ... left out of BOF3X_OPT", `psp-ryu-forms` the one layer (the renames not in that cache). At the tip the same ini passes `psp-tables` and the hosted build stops at start (9.4, "hosted, the tables named": exit 3) |
| 2 | A failed import left layers on disk without manifest rows, offered and played | `psp_layers.build`: each layer built in `out/.psp_layer_<name>_*` and renamed into `opt/` whole; a failure removes it, and the `[psp_layers]` table is written whatever happens with the rows of every layer in place. The engine (`dat_cache::HasLayer`, `HasLayerPart`), the launcher (`CacheHasLayer`) and the Jukebox's PSP column offer an `opt/` layer only when the manifest lists it (`[cache] opt` or `[psp_layers]` rows; no manifest: the tree, as before). `verify` reports an unlisted `opt/` folder as a problem | `rf_inject.py`: a US + PSP-EU build with `--opt psp-music --opt psp-snd`, `psp-snd`'s builder failing; `run2.sh` over both caches; host suite `TestListedLayers`; the `dat_cache` self-test's listing case and its control | the tip's tools: `opt/psp-music` (247 files) on disk, no `[psp_layers]` table; this branch's: the same tree, listed, `verify` 0 problems for it. Over the tip's cache: `verify` "opt/psp-music: on disk, not listed"; the launcher drops `psp-music` ("not there, nor in the cache's opt\"), a hand-set `BOF3X_OPT=psp-music` exits 3 naming the manifest; over this branch's cache both play it (`songs from ...opt\psp-music\bgm first`) |
| 3 | `BOF3X_CACHE_DATA=0` could not play the music layer | a layer's `bgm` part is read whenever the cache is the music's: `dat_cache` keeps the root as the music's under `0` (and its manifest's listing), `HasLayerPart(bgm)` answers there; the launcher asks `bgm` of `ConfigCacheRoot`. `snd` (effects, voices, jingles) and the chord's `dat` banks stay on the data side ([`cache-read.md`](cache-read.md) 6) | the `dat_cache` self-test's music-only case (bgm found, snd and exe not; the rule before refused as its control), under `BOF3X_SHADOW=sound,dat_cache` with `BOF3X_CACHE_DATA` 0 and 1 (`run3.sh`), and the music seam's own self-test in the same runs; host suite `TestMusicOnly` | both settings exit 0 with `DIV-0109 ... songs from ...opt\psp-music\bgm first`; the exe host through the launcher (`run3b.sh`, `opt=psp-music,psp-snd`): under 0 `BOF3X_OPT` is `psp-music` alone, under 1 both |
| 4 | The game and the launcher read `base_policy` by different rules (and the Jukebox by a third) | one reader, `src/common/cache_manifest.*`, linked by the DLL and the launcher: `base_policy` (exactly `psp` / `psx`, absent `psx`), `exe_tables_from`, the bgm row's build, `[snd]`, the listing. Anything else is one sentence (`ManifestRefusal`): the launcher stops on it before the game starts, the DLL (`psp_code.cpp`) Fatals with it | host suite `TestOneReader` (good, absent, seven misspellings); `run4.sh`: `bof3x.exe` started alone and the launcher, over six one-file caches | `psp`, `psx`, absent: exit 0 both, the DLL's default as the policy says; `"PSP"`, `psp` unquoted, `exe_tables_from = 3`: the DLL exit 3 and the launcher exit 1, the same sentence |
| 5 | The Jukebox built a broken path for a second music member | `suites.py check` refuses a second `bgm` member, naming the Jukebox's one column; `ListPsp` takes the first member and stops | `neg_bgm.py`: a second `bgm` member added to audio | refused with the reason; the committed toml 0 errors |
| 6 | Ticking and unticking the chord box wrote a default `opt=` where the line was empty | the box records a choice (`-1` / `0` / `1`); `opt=` is made once on Play from the line as read with the suites' choices (`SuitesOptEdit`'s `extra`), so a choice back where it started writes the line byte for byte | host suite `TestChordRoundTrip`: the old two edits give `area4-walls` over an empty line; now `""`, `none`, `area4-walls`, `psp-art,area4-walls` each come back byte for byte | passes |
| 7 | A mistyped `psp=` started the game and failed inside it | `ConfigPspValid` (the DLL's rule: empty, `none`, or distinct items of the engine's) checked up front as `opt=` is; the DLL keeps its check | host suite `TestPspValid`; `run7.sh` | `duel-loss` exit 0 (`duel-loss on`); `duel-los`, `duel-loss,duel-loss`, `none,myria-item` exit 1 naming the fault |

Also changed: the launcher's `Die` shows no message box under
`BOF3X_SELFTEST_ONLY` (as the DLL's `Fatal`), so its up-front refusals can be
run headless; `dialog_test.py`'s PC + PSX + PSP fake manifest lists the four
layers its tree holds (it listed `psp-art` alone, and finding 2 would have
greyed the others). The message for a layer the launcher drops now names the
manifest, or under `BOF3X_CACHE_DATA=0` that only a music part is read.

**Checks** (this machine, 2026-10-12):

| Check | Result |
|---|---|
| the i686 build | clean, no warning |
| launcher host suite (`lh.sh`) | **215 checks, 0 failures** |
| `importer.py check`; `suites.py check`; `psp_layers.py check` | exit 0; 0 errors; 5 layers, 0 errors |
| `ledger_check.py` | 101 entries, 0 errors, 2 notes (as at the tip) |
| hosted `'*'`, the PC install, an ini-less copy (`run_checks.sh`) | exit 0, `inject: 10087 ours`, no shadow reporting a mismatch |
| exe host, PSX disc-only (a copy of PN's `g_us`, the coordinator's `ref_us`, `en-US`) | exit 0, `inject: 10088 ours`, `exe_own: 17 regions ... filled over a disc-built image`, the battle rules off (`base_policy is psx`) |
| exe host, PSP-only (a copy of PN's `g_psp`, TG's PSP-EU cache, `en-150`) | exit 0, `inject: 10088 ours`, the battle rules on (`base_policy is psp`) |
| the PC and PSX paths (`refs.sh`): JP + PC, US alone, Europe alone against `psp_coord/ref_*` | 2,126, 1,873 and 1,865 files, every file identical; the JP + PC manifest differs only by wave one's `[build]` table (its reference predates IM2; US and Europe were rebuilt after it), not a line of this branch's; `recipes/exe-gaps.tsv` untouched |

`run1.sh` ran on a build whose launcher line still said "TG-3"; the final
build's says the same without the placeholder, and the code is otherwise the
same.

**Found, not fixed:** `importer.py verify` on a US + PSP-EU cache reports
`opt/area4-walls`: "AREA004.DAT slot 6: composed with the layer, not the
recipe's" (1 of 2 base chunks), with the round tip's own tools on the tip's
build as well (`tipverify.sh`): older than these fixes, not looked into.

**Not verified:** the dialog and the Jukebox by script (`dialog_test.py` is
the end batch's; its fake manifest changed here and was not run); anything
played or heard; a hosted run whose ini names a mod's exe layer (the host
suite covers the rule only).

## 11. `main`'s DIV-0102 beside the suites (the merge of 2026-10-12)

`main` (PR #55) changed the same dialog: on an install with neither
`BOF3.exe` nor the cache's `loc\zh-CN`, the Language box drops "Chinese (PC
script)" and its note says why (DIV-0102). The merge (`psp/main-merge`)
keeps both. `DialogState` holds the suites, the player's choices and the
chord as before, and also `original` (`ConfigOriginalAvailable`). `Populate`,
`LanguageChosen` and `ReadBack` count the box's entries from 1 when the
Chinese is not offered. The `installed[]` flags of `main`'s PSP-extras boxes
are gone with the boxes, which the suites replaced.

Measured on the merge (headless, an ini-less copy of the launcher, a US
disc-only install at `C:\b3g` built with `--lang en-US`):
- `BOF3X_LANG` unset: `a disc-only install, no Chinese text: language en-US,
  the first it holds (DIV-0102)`, then exit 0.
- `BOF3X_LANG=fr-FR` (no layer held): the launcher refuses with "The
  languages it holds: en-US.", exit 1.

So on a PSP-only install the Language box defaults to the first language it
holds, not the Chinese (D250's note); the launcher host suite has 215
checks, 0 failures. The box itself was not driven (no `dialog_test.py`: the
end batch was playing live).

## For the other files

**Folded in 2026-10-12** (the coordinator's pass, branch `psp/ledger`): DIV E is **DIV-0107**, written with TG's amendment in it; DIV-0086 amended; the README row, STATUS, HANDOFF and owner-review are in. `launcher-settings.md` and `opt-layers.md` got pointers to this doc.

For the coordinator to fold in. These edits are not made here.

**DIV-0107** (amends DIV-0086's launcher box; drafted as DIV E, numbered 2026-10-12):

> ### The launcher offers three suites, the PSP's or the PlayStation's, in place of the PSP extras
>
> - **ID:** DIV-0107
> - **Date:** 2026-10-12
> - **Subsystem:** launcher (`src/launcher/suites.cpp`, `config_dialog.cpp`, `launcher.rc`; `recipes/suites.toml`, `tools/suites.py`)
> - **Tier:** Sensible - a choice of which release's presentation and data to play, made before the game starts; what the suites move is DIV-0086's layers (and, once TG lands, its own entries), off beside a PlayStation base unless chosen.
> - **Original behaviour:** the 2001 port has no settings program (`START.EXE` is an autorun shell, [`launcher-settings.md`](launcher-settings.md) 1) and carries no PSP content. Until this entry, our launcher offered DIV-0086's layers as a "PSP extras" box per installed layer.
> - **New behaviour:** the settings dialog lists three suites from `recipes/suites.toml`: graphics (`psp-art`, `psp-tiles`, `psp-maps`), audio (the PSP's music; its effects, voices and jingles) and data (the level table and item 87; the English and Japanese renames; Ryu's forms, P8). Each is the PlayStation's version or the PSP's, chosen whole, whatever the install. A side the cache's `manifest.toml` cannot provide is greyed, naming the discs that would enable it. A member whose engine half is not built (audio and the tables beside a PlayStation base, until TG) or that has no layer (P8) is greyed with that reason and cannot be selected. Under the manifest's `base_policy = "psp"` it inverts: the PSP's is the base, and the PlayStation's is greyed, naming a PlayStation disc. The choice is written as `opt=`'s layers (no new ini key); an untouched page writes `opt=` back byte for byte, and `music=` keeps its meaning.
> - **Rationale:** the owner's calls of 2026-10-10 ([`psp-only-build.md`](psp-only-build.md) 7): three toggles, every option listed, greyed by source; a PSP-only install the other way round. [`launcher-suites.md`](launcher-suites.md).
> - **Also in the PSX version?** no - the PlayStation release has no launcher and no PSP content.
> - **Reversible?** yes: each suite's PlayStation side, or `opt=none` / an `opt=` list in `bof3x.ini`; a PSP base has no PlayStation side to choose until one is built.

**DIV-0086:** an "Amended 2026-10-12 (DIV-0107)" line. The launcher's "PSP
extras" boxes are replaced by the three suites. `opt=` is unchanged, and an
`opt=` written by the boxes reads as a partial suite, kept as written.

**`docs/launcher-settings.md`:**
- The status line: 2026-10-12, the PSP extras boxes replaced by the suites
  ([`launcher-suites.md`](launcher-suites.md)).
- Section 3's table: the "Optional layers" row now reads "since 2026-10-12
  the dialog's three suites (`launcher-suites.md`)".
- Section 3.1: the title becomes "The Music, Cache folder and suites boxes";
  replace the **PSP extras** item with one sentence pointing to
  `launcher-suites.md` 1 and 4; the verification record stays as history,
  with a line that `dialog_test.py` was rewritten on 2026-10-12
  (`launcher-suites.md` 5).
- Section 4's ini comment for `opt=`: the two lines `ConfigSave` now adds
  (the suites write their layers here).

**`docs/opt-layers.md`:**
- Section 5's launcher bullet: "the dialog's three suites (DIV-0107,
  `launcher-suites.md`) in place of the PSP extras group".
- "The owner's calls", item 3: superseded by the suites.

**`docs/psp-only-build.md`:**
- Section 5, step 4: struck through, done 2026-10-12
  (`launcher-suites.md`), with the engine half TG's.
- Section 6: a pointer to `launcher-suites.md`.

**`docs/psp-only-round.md`:**
- Section 7 item 3: the proposals are in `launcher-suites.md` 6.
- Section 6 V3.4: the page is scriptable now with `dialog_test.py`'s three
  caches, and the real caches replace the fake ones.

**`docs/README.md`:** a row:

> | [`launcher-suites.md`](launcher-suites.md) | The launcher's three suites - graphics, audio, data, each the PlayStation's version or the PSP's (the owner's calls of 2026-10-10): `recipes/suites.toml` and `tools/suites.py` (checked in `importer.py check`, generated into the launcher), availability from the cache's `manifest.toml` (`base_policy`, `exe_tables_from`), greyed sides naming the discs, the PSP-only inverse, `opt=` as the ini's record; the membership proposals; what TG lifts. DIV-0107. BUILT. |

**`docs/STATUS.md`:** the PSP-only round's LT: the launcher's three suites
built and scripted (114 host checks, 35 dialog checks); the owner's eye on
the page and the membership calls (`launcher-suites.md` 6) owed.

**`docs/HANDOFF.md`:**
- When SI's and IM's keys land, re-run `dialog_test.py` against a real
  PSP-only cache.
- TG: section 7 here.
- Trap: `dialog_test.py` needs a `bof3x.exe` beside the launcher since
  state 3, and a stand-in is placed for it.

**`docs/owner-review.md`, one entry:**
- **Eye:** the options page from `capture.py`'s four captures, or the
  launcher on the owner's display: the layout, the wording, and the partial
  state.
- **Call:** the three membership questions, section 6.

## For the other files (TG, the engine half)

For the coordinator to fold in; not edited here. The ledger drafts were
TG-1 to TG-5, numbered DIV-0109 to DIV-0113 on 2026-10-12, and the code's
citations renamed with them (`dat_cache.h`, `dat_cache.cpp`, `dat_load.cpp`, `dat_load.h`,
`music_seq.cpp`, `save_menu.cpp`, `exe_image.h`, `exe_image.cpp`,
`exe_host.cpp`, `suites.h`, `resource.h`).

**DIV-0107, amended** (the suites; LT's draft above): the "New behaviour"
sentence "A member whose engine half is not built (audio and the tables
beside a PlayStation base, until TG) or that has no layer (P8) is greyed with
that reason" becomes: "Every member is a layer beside a PlayStation base
(`recipes/opt.toml`'s, or `recipes/psp-layers.toml`'s: the music, the
effects, the exe tables, P8 - DIV-0109 to DIV-0112); one not in the cache is greyed
with `build with --opt NAME`." Add: "`opt=` takes up to 16 layers (8
before)."

**DIV-0109 - The PSP's music as a layer beside a PlayStation disc's**
- **Subsystem:** music (`src/game/music_seq.cpp`, DIV-0087's seam; `tools/psp_layers.py`, `recipes/psp-layers.toml`)
- **Tier:** Sensible - a later release's content, off unless chosen (DIV-0086's rule for layers).
- **Original behaviour:** the port plays its MP3s; with DIV-0087, the cache's `base\bgm` (one disc's songs).
- **New behaviour:** with `opt=` naming `psp-music` (the audio suite's PSP side), a track is the layer's `opt\psp-music\bgm\NNN.DAT` on its own `bank\` when it has one, else `base\bgm`'s, else the MP3. A song's bank is held by its file, not its name. `BOF3X_MUSIC=mp3` still plays the MP3s.
- **Rationale:** the owner's calls of 2026-10-10 (three suites, the PSP's audio a choice beside a PSX base); `docs/launcher-suites.md` 9.
- **Also in the PSX version?** no - the PSP's own sequences (`psp-music.md`: 93 of 166 differ).
- **Reversible?** yes: the audio suite's PlayStation side, or `opt=` without the layer.

**DIV-0110 - The PSP's effects, voices and jingles as a layer**
- **Subsystem:** sound streams (`dat_cache.cpp` `SoundPath`, DIV-0089's `Sound_LoadStream` seam)
- **Tier:** Sensible.
- **Original behaviour:** `SND\NAME.DAT` (with DIV-0089, `base\snd` first).
- **New behaviour:** with `opt=` naming `psp-snd`, the layer's `opt\psp-snd\snd\NAME.DAT` before `base\snd` and `SND\`: the PSP's ATRAC3plus clips as decoded by the player's ffmpeg, and the 11 jingles - which a PSX-only install otherwise lacks.
- **Rationale:** as DIV-0109; DIV-0106 (PN) is the same audio as a PSP-only base.
- **Also in the PSX version?** no.
- **Reversible?** yes, as DIV-0109.

**DIV-0111 - The PSP's level table and consumable 87 laid over .data**
- **Subsystem:** the exe host's data mapping (`src/hook/exe_image.cpp` `ExeImage_LayOptTables`, `exe_host.cpp` `CopyData`)
- **Tier:** Extension - a balance change (roster index 6's level table, `opt-layers.md` 3), chosen by the player; IM's DIV-0104 makes it a PSP-only install's base.
- **Original behaviour:** `.data` as `BOF3.exe` (or the disc-built image) holds it: the PSX's tables.
- **New behaviour:** with `opt=` naming `psp-tables`, the exe host lays `opt\psp-tables\exe\place.bin` over `.data` at map time: every record of `exp_table` and `consumables`, names left out, from the PSP-built image. Over the PC's, the US disc's and the European disc's images it changes exactly 250 bytes: 249 of roster index 6's rows and 1 of consumable 87. The hosted build refuses the layer (its `.data` is Capcom's, the reference).
- **Rationale:** the owner's data suite; `docs/launcher-suites.md` 9, `psp-image.md` (DIV-0104).
- **Also in the PSX version?** no - the PSP's changes.
- **Reversible?** yes: the data suite's PlayStation side.

**DIV-0112 - Ryu's forms (P8) as a layer**
- **Subsystem:** data (`LoadDatFile`, DIV-0086's walk; no new code)
- **Tier:** Sensible (presentation and form data the PSP changed; CD settled that our code runs it, `psp-code-diff.md`).
- **Original behaviour:** the PSX's arenas and `RYUD` sections (the PC's chunks).
- **New behaviour:** with `opt=` naming `psp-ryu-forms`, the 19 chunks a PSP-only base stands in (SI): `RYUD00..03` with the port's byte, 14 type-1 arenas, `BPLD27A` with the port's hold.
- **Rationale:** the owner's data suite (P8 under data, 2026-10-12).
- **Also in the PSX version?** no.
- **Reversible?** yes.

**DIV-0113 - The battle chord as the PSP plays it, and a layer's banks in `Snd_LoadBankFile`**
- **Subsystem:** sound banks (`save_menu.cpp` `Snd_LoadBankFile`, `dat_load.cpp` `DatLoad_OptLayerChunks`); the launcher's Jukebox (`music_player.cpp`, `suites.cpp` `ChordEvaluate`, `launcher.rc`)
- **Tier:** Sensible - a choice, off by default.
- **Original behaviour:** `Snd_LoadBankFile` loads a container's banks from the shipped file only. The PSX's (and the PC's) battle effects bank in 16 containers keys a chord of two tones; the PSP's drops the second, and every base plays the restored chord (MB's rule).
- **New behaviour:** `Snd_LoadBankFile` also loads the kind-2 chunks of each named layer's file for the container, when a named layer carries a bank at all. The layer `psp-chord-one-tone` (built whenever a PSP disc is a source) holds those 16 banks as the PSP plays them; the Jukebox's box "Battle chord as the PSP plays it (one tone)" puts it in `opt=`. Off by default.
- **Rationale:** the owner, 2026-10-12: the dropped tone toggle-able, on the Jukebox.
- **Also in the PSX version?** no - the PSX plays two tones.
- **Reversible?** yes: the box, or `opt=` without the layer.

**DIV-0086:** an "Amended 2026-10-12 (TG)" line: a layer may hold `bgm\`,
`snd\` or `exe\` trees as well as `dat\` (DIV-0109..0111), read from the cache only
(`install --opt` refuses them); `kOptMax` 16.

**DIV-0087 / DIV-0089:** an "Amended 2026-10-12 (DIV-0109 / DIV-0110)" line each,
pointing here.

**`docs/opt-layers.md`:** section 3's last paragraph (the level table and
consumable 87 "wait on a chunk kind") and owner's call 5 (P8, the tables):
built, `launcher-suites.md` 9. Section 5: the list's limit is 383
characters and 16 layers.

**`docs/music-player.md`:** section 3 and 6.3's last line: a PC + PSX + PSP
cache shows the PSP's songs from the `psp-music` layer (9.3); the chord box
(9.6).

**`docs/cache-read.md`:** section 5: `Sound_LoadStream` takes a named
layer's `snd\` first (DIV-0110); `Snd_LoadBankFile` a named layer's banks
(DIV-0113).

**`docs/psp-music-banks.md`:** section 5's last paragraph: the dropped tone
is now the owner's toggle (DIV-0113), default the restored chord.

**`docs/README.md`:** `launcher-suites.md`'s row: "... what TG lifts; the
engine half built (section 9: `recipes/psp-layers.toml`, DIV-0109..0113, the
Jukebox's chord box)".

**`docs/STATUS.md`:** the PSP-only round's TG: the suites' engine half -
`psp-music`, `psp-snd`, `psp-tables` (exe host only), `psp-ryu-forms`, and
the Jukebox's battle-chord toggle `psp-chord-one-tone`; self-tested, not
played or heard.

**`docs/HANDOFF.md`:** V3/V4 additions: a level-up of roster index 6 with
`psp-tables` on; the audio suite on a PC + PSX + PSP install by ear; the
chord box on and off in a fight with one of the 16 characters' forms. Trap:
`dialog_test.py`'s Jukebox (IDD_MUSIC grew 36 units; its buttons moved).

**`docs/owner-review.md`, one entry:** ear - the PSP's songs and effects
beside the PlayStation's (the audio suite), the chord one tone against two;
eye - the Jukebox's new box and its wording.
