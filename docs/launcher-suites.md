# The launcher's three suites: graphics, audio, data

**Status:** BUILT (2026-10-12, the PSP-only round's group LT, branch `psp/lt`
off `psp/round` `668a9b67`). Driven by script against three fake manifests
(section 5), and once against a real PSP-only cache built with SI's branch;
**not seen by the owner**. Of the manifest keys it reads, `base_policy` is
written by SI's branch (`psp/si` `1d72d999`), and `exe_tables_from` by
nothing yet (IM's; section 3.1). The audio and data suites' engine half is
TG's, in wave two (section 7).

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
"Versions: the PlayStation's or the PSP's". It holds a note and then one block
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
| `kind` | where it lives in the cache: `opt` (`opt/<layer>/`), `bgm` (`base/bgm/`), `snd` (`base/snd/`), `exe` (`base/exe/`'s tables), `base` (`base/` chunks: SI's stand-ins) |
| `layer` | for `opt` only: the `recipes/opt.toml` layer |
| `from` | the builds it comes from (fixtures.toml ids; PSP discs only) |
| `over_psx` | how it reaches the game beside a PlayStation base: `layer` (an `opt/` layer, selectable today), `engine` (its engine half is TG's; greyed), `none` (no layer exists or is planned; greyed) |
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
so `music=seq` plays the PSP's music with no new value. Beside a PlayStation
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
| ledger | `python tools/ledger_check.py` | 101 entries, 0 errors, 2 notes (DIV E is not in the ledger yet) |

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

## For the other files

For the coordinator to fold in. These edits are not made here.

**DIV E** (amends DIV-0086's launcher box; number it at merge):

> ### The launcher offers three suites, the PSP's or the PlayStation's, in place of the PSP extras
>
> - **ID:** DIV E
> - **Date:** 2026-10-12
> - **Subsystem:** launcher (`src/launcher/suites.cpp`, `config_dialog.cpp`, `launcher.rc`; `recipes/suites.toml`, `tools/suites.py`)
> - **Tier:** Sensible - a choice of which release's presentation and data to play, made before the game starts; what the suites move is DIV-0086's layers (and, once TG lands, its own entries), off beside a PlayStation base unless chosen.
> - **Original behaviour:** the 2001 port has no settings program (`START.EXE` is an autorun shell, [`launcher-settings.md`](launcher-settings.md) 1) and carries no PSP content. Until this entry, our launcher offered DIV-0086's layers as a "PSP extras" box per installed layer.
> - **New behaviour:** the settings dialog lists three suites from `recipes/suites.toml`: graphics (`psp-art`, `psp-tiles`, `psp-maps`), audio (the PSP's music; its effects, voices and jingles) and data (the level table and item 87; the English and Japanese renames; Ryu's forms, P8). Each is the PlayStation's version or the PSP's, chosen whole, whatever the install. A side the cache's `manifest.toml` cannot provide is greyed, naming the discs that would enable it. A member whose engine half is not built (audio and the tables beside a PlayStation base, until TG) or that has no layer (P8) is greyed with that reason and cannot be selected. Under the manifest's `base_policy = "psp"` it inverts: the PSP's is the base, and the PlayStation's is greyed, naming a PlayStation disc. The choice is written as `opt=`'s layers (no new ini key); an untouched page writes `opt=` back byte for byte, and `music=` keeps its meaning.
> - **Rationale:** the owner's calls of 2026-10-10 ([`psp-only-build.md`](psp-only-build.md) 7): three toggles, every option listed, greyed by source; a PSP-only install the other way round. [`launcher-suites.md`](launcher-suites.md).
> - **Also in the PSX version?** no - the PlayStation release has no launcher and no PSP content.
> - **Reversible?** yes: each suite's PlayStation side, or `opt=none` / an `opt=` list in `bof3x.ini`; a PSP base has no PlayStation side to choose until one is built.

**DIV-0086:** an "Amended 2026-10-12 (DIV E)" line. The launcher's "PSP
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
- Section 5's launcher bullet: "the dialog's three suites (DIV E,
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

> | [`launcher-suites.md`](launcher-suites.md) | The launcher's three suites - graphics, audio, data, each the PlayStation's version or the PSP's (the owner's calls of 2026-10-10): `recipes/suites.toml` and `tools/suites.py` (checked in `importer.py check`, generated into the launcher), availability from the cache's `manifest.toml` (`base_policy`, `exe_tables_from`), greyed sides naming the discs, the PSP-only inverse, `opt=` as the ini's record; the membership proposals; what TG lifts. DIV E. BUILT. |

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
