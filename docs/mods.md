# Mods: a zip of data files, the Mods tab, and the row above the text

**Status:** BUILT 2026-10-22 (the reading-row round, group ML; DIV-0157).
The mechanism was built and tested end to end with a stand-in builder on
`reading-row/ml`; on `localization/second-script` it meets FB's builder,
`tools/ruby_build.py` (`--readings`, `--out` the `dat\` folder: commit
`5ad20bb2`), and the row is drawn by RR's renderer (DIV-0155). The real
builder from an installed zip needs SudachiPy, which the release does not
bundle (section 8). Not yet played by the owner. Sections 6 and 8 below are
as ML wrote them before FB merged.

The owner's call 2 of the round (section 6 there), decided 2026-10-22 in
chat after a brief reversal: **furigana ships as a mod zip**, the project's
first, so the mod path is proven end to end in this round.

## 1. The shape

A **mod** is a `.zip` holding `mod.toml` at its root and the data files it
names - never code. The player adds it on the launcher's **Mods** tab the way
a disc is added on the Sources page: `tools/mods.py` checks it, unpacks it to
`<home>\mods\<id>\` and lists it in `<home>\mods.toml` (the home folder,
[`install-sources.md`](install-sources.md) section 2). Each mod's **build
step** then turns it into an optional layer of the install's cache, on the
player's machine, from the player's own copies of the game; the launcher
offers what was built.

Today one kind exists, `reading-row`: a row of small glyphs above each text
row of the dialogue box ([`reading-row.md`](reading-row.md) is the layer's
byte contract). Its step runs `tools/ruby_build.py` with the mod's readings
and writes the optional text layer `ruby-<id>-<base>` (DIV-0086's layers;
`ruby-furigana-ja-JP` for the furigana mod). The Settings page's **Row above
the text** box chooses it, and the launcher adds the layer to `BOF3X_OPT`.

## 2. The format

```toml
[mod]
id = "furigana"            # [a-z0-9]+(-[a-z0-9]+)*: the folder name and the layer's middle
name = "Japanese (Furigana)"
version = "0.1.0"          # 1.2.3, optionally -beta / +build
author = "Who made it"     # optional, up to 64 characters: shown on the Mods tab (issue 216)
url = "https://..."        # optional: the author's page; http or https, shown as text, never opened
credit = "One line"        # optional, up to 200 characters: the author's own line, shown when the mod is selected
kind = "reading-row"       # a kind the tools know (KINDS in tools/mods.py); anything else is refused
requires = ["ja-JP"]       # the language layers the install must hold (src/game/language_tags.h tags)

[reading-row]
base = "ja-JP"             # the text the row sits over; among `requires`
readings = "readings.toml" # a file at the zip's root
```

What `mods.py add` / `check` refuse, each with one sentence naming the
member or key (the self-test, `mods.py selftest`, makes a zip for every
case below and checks the sentence):

| Case | Why |
|---|---|
| a `kind` the tools do not know, or none | the kinds are a closed set; the sentence names the kind and lists the known |
| a `.DAT` (or `.EMI`, `.BIN`, `.ISO`, `.STR`, `.AVI`, `.MP3`, ...) member, a text layer included | rule 1: a mod carries its own data; game data is built from the player's discs |
| a program or script (`.exe`, `.dll`, `.py`, `.bat`, `.ps1`, `.js`, ...) | data only; the tools never run, import or evaluate anything from a zip |
| a member `mod.toml` does not name, or a named file missing | the zip is exactly what `mod.toml` says |
| a path with `..`, a drive, a leading `/` or `\`, a backslash, a folder, a file in a folder, a link | nothing can land outside `mods\<id>\` |
| a table or key `mod.toml`'s kind does not have | no hook, no command key, nothing for a later tool to misread |
| more than 16 members, a member over 4 MiB unpacked, over 8 MiB in all, a zip over 8 MiB | the bounds; a member is read at most its header's size and the CRC must hold, so a lying header is refused as damaged |
| an encrypted member, a name twice, not a zip, no `mod.toml` | |
| `author` or `credit` empty, too long or holding a control character; `url` not an `http://` or `https://` address, or with a space | the three are text the launcher shows; nothing is opened, followed or run |
| an `id` that is not lowercase letters, digits and single hyphens, or whose layer `ruby-<id>-<base>` is over 23 characters | the DLL's layer-name limit (`ConfigOptValid`) |
| a retired bare language code (`ja`), a tag the game does not know | DIV-0005 |
| `reading-row.base` not among `requires`, or a base the builder cannot read (today: only `ja-JP`, furigana) | |
| `readings` not a plain `.toml` file name, or not valid UTF-8 TOML | |

Adding a mod whose `id` is installed replaces it (a re-issued readings list
is a newer zip of the same id); `add --json` says which version it replaced.

## 3. The tools

| Command | Output | Exit |
|---|---|---|
| `mods.py add ZIP --home H [--json]` | one flat object: `path, ok, id, name, version, kind, requires, base, layer, state, needs, problem, replaced`, and `behaviours`, `missing` (section 11) | 0 added, 1 refused |
| `mods.py remove ID --home H` | `removed: ID` / `not installed: ID`; the folder, the row, and the layer built from it (`cache\opt\<layer>\` and its manifest rows) | 0 / 1 |
| `mods.py list --home H [--json]` | one object a mod, in the list's order; `state` is `built`, `not-built`, `needs` (`needs` names the tags) or `problem` | 0 |
| `mods.py build [ID] --home H` | the step's lines; `error <sentence>` on a failure | 0 built, 1 failed, 2 no install built yet |
| `mods.py check ZIP [--json]` | `add`'s checks alone | 0 / 1 |
| `mods.py selftest [--keep DIR]` | every refusal and the add / list / build / remove cycle (no game data) | 0 / 1 |
| `make_mod.py furigana [--src DIR] [--out ZIP]` | the furigana zip from `data/furigana/`: `mod.toml` (written from `MODS` there) and `readings.toml`, nothing else; deterministic; checked by `mods.py`'s reader | refuses when the folder lacks `readings.toml` |
| `make_mod.py --selftest` | against a temporary folder, never `data/furigana/` | 0 / 1 |

`<home>\mods.toml` is written only by `mods.py`, in `sources.toml`'s shape: a
`[[mod]]` table a mod, a `key = value` line each, the values JSON-quoted
(`id, name, version, author, url, credit, kind, requires, base, readings,
files, zip, sha256, added`; the three credit keys only when the mod gives
them). The launcher reads it (`ConfigModsInstalled`) for the Settings box.

**Held** means the install holds a language layer: `cache\loc\<tag>\dat\` has
a `.DAT`, or `game\DAT\<tag>.*.DAT` exists - the launcher's
`ConfigLanguagesAvailable`. **Built** means `cache\opt\<layer>\dat\` has a
`.DAT` and the cache's manifest lists `opt/<layer>` (`ManifestOffersLayer`,
the DLL's own rule: an unlisted layer is never offered or played).

## 4. The build step, and `setup`

`importer.py setup` (`tools/player_sources.py run_setup`) calls
`mods.setup_step` after `importer.py build` has made the new cache with its
language layers, and before the game folder is laid out. For each listed mod:

- a mod whose `requires` the new cache lacks is **skipped** with a progress
  line naming the language layer ("needs the ja-JP language layer, which this
  install does not hold (add a disc with that language on the Sources page)");
- otherwise its builder runs:
  `python tools/ruby_build.py furigana --cache C --lang <base> --readings <home>\mods\<id>\<readings> --out C\opt\.<layer>.new`
  - `--out` is the layer's folder (the builder writes `dat\<NAME>.DAT` under
  it, as [`reading-row.md`](reading-row.md) 1.1 lays a cache layer out); the
  result must hold at least one `.DAT`; it is then renamed to
  `C\opt\<layer>\` and every chunk of every container is listed in the
  manifest's `[cache] opt` array as `[NAME.DAT, "opt/<layer>", i,
  "mod:<id>@<version>:<base>", sha256]` (the rows `ManifestOffersLayer`
  reads; the container walk accepts any chunk kind, so kind 18 too);
- **the builder absent, its readings absent, a non-zero exit or no `.DAT`
  written stops the setup** with `error` naming the cause - the install as
  it was is kept, as for any setup error ([`install-sources.md`](install-sources.md) 5).
  Until FB merges, a home with a mod therefore cannot set up: the sentence
  says the builder is not in this release and to remove the mod. That is the
  rule-4 refusal; it is not reachable from a release, which will carry the
  builder.

`mods.py build ID` runs the same step against the home's current cache in
place, so a mod added on the Mods tab is built without a whole setup.
`mods.py remove` takes the layer and its manifest rows out of the home's cache.

## 5. The launcher

**The Mods tab** (`IDD_PAGE_MODS`, `sources_dialog.cpp ModsPageProc`): a
sentence for what a mod is (and that side-loaded DAT / EMI files come later,
IDEAS I33), the list - name, author (`by ...`), version, state (2026-11-10,
issue 216: the author in the row; the kind moved to the status line, which
for the selected mod reads `<name> <version>, by <author>. <credit> <url>
Kind: <kind>.`, or `The mod does not give its author.`) - owner-drawn so a mod the
install cannot play is greyed with its reason (`needs ja-JP text: add that
disc`, `not built: Data, Sources..., Build`, `built; over ja-JP text only`
when another language is chosen in Settings, `problem: ...`), **Remove**, a
path box with **Add zip...** (a file dialog, then the add) and **Add**, a
status line and a log box. The children are `mods.py` beside the importer on
the importer's Python (`FindRuntime`), one at a time, as the Sources page
runs `importer.py`: `list --json` when the tab is first shown; `add --json`,
then `build ID` when it was added and its language is held; `remove ID`; the
list again after each. The window is told (`kMsgModsChanged`) and redraws
the Settings box. Since 2026-10-22 this page is the tab's **Data** button,
beside **Extras** on the tab's sub-row
([`launcher-settings.md`](launcher-settings.md) 2a).

**The toggle on Extras** (the owner's call, 2026-10-22: "hide the second
script dropdown by default, and show it with a toggle"): "Show Row above the
text on Settings (the second script: furigana or another language)",
`ruby_ui=0|1` in `bof3x.ini`, off by default. Off, Settings hides the Row
above the text box and its label; on, shows them. Turned on by hand, or by
an add of a reading-row mod that succeeds (the add's own `--json` line
names the kind; built or not, the box is where the mod is chosen); a remove
leaves it as it is; an ini older than the toggle (no `ruby_ui=` line) comes
up on at start when `<home>\mods.toml` lists a reading-row mod. **Display
only**: hidden, `ruby=` keeps whatever it holds and the layer is composed
into `BOF3X_OPT` exactly as below - a non-empty `ruby=` under a hidden box
is not treated as none, since the owner's words were "hide the dropdown",
and a player who hides it after choosing furigana keeps the furigana.

**Row above the text** (Settings, under Language; `IDC_RUBY`): owner-drawn,
every entry listed and the ones this install cannot play greyed with the
reason (the owner's rule of 2026-10-10); a greyed entry cannot be picked.
Built from what exists, not from a table (`ConfigRubyChoices`):

1. **None** - `ruby=` empty.
2. Every installed `reading-row` mod (`<home>\mods.toml`): playable when its
   `base` is the language chosen and `ruby-<id>-<base>` is built; else greyed
   with "needs <tag> text", "over <base> text only: choose that language" or
   "not built".
3. Every other held language: `ruby-<tag>-<language>`, the second-language
   layer group SB's step builds in stage B; greyed "its second-language layer
   is not built in this install" until it is.
4. Any other `ruby-*-<language>` layer the cache's `opt\` or `DAT\` holds
   and the manifest lists, under its own name.

Under the Chinese text every entry but None is greyed. The box redraws when
the language, the cache folder or the mods change.

**`ruby=` in `bof3x.ini`** holds the entry's id - a mod's id or a language
tag - not a layer name, so it survives a change of language as the player's
intent and names the layer `ruby-<ruby>-<language played>` at each start.
An untouched box writes `ruby=` back as it was read. At start
(`ConfigApplyEnvironment` -> `ConfigRubyPlayable`) the layer is added to the
`BOF3X_OPT` the launcher composes, after the `opt=` layers (once; not past
the DLL's 16), and `ConfigOptPlayable` then applies the DLL's own tests; a
`ruby=` that is not a name, a Chinese-text start, or a layer not built is one
`stderr` line and no row, as `config.cpp` does for a missing language. A
`BOF3X_OPT` already in the environment wins, untouched.

| Choice (language ja-JP, the default `opt=`, area4-walls installed) | `bof3x.ini` | `BOF3X_OPT` |
|---|---|---|
| None | `ruby=` | `area4-walls` |
| Japanese (Furigana) (mod 0.1.0) | `ruby=furigana` | `area4-walls,ruby-furigana-ja-JP` |
| English (US PlayStation script), its layer built | `ruby=en-US` | `area4-walls,ruby-en-US-ja-JP` |
| German, its layer not built | (greyed: not taken) | - |
| `ruby=furigana` with `opt=none` | `ruby=furigana` | `ruby-furigana-ja-JP` |
| `ruby=furigana`, language en-US | `ruby=furigana` (kept) | `area4-walls`; stderr: layer ruby-furigana-en-US is not there |

The table is `config_tests.cpp TestRuby`'s cases; `dialog_test.py` checks the
`ini` column through the dialog. Every row holds with `ruby_ui=0` too
(`TestRuby` plays `ruby=furigana` both ways; `dialog_test.py`'s en-US run
has the box hidden and still gets the stderr line).

## 6. The release

`release.yml` runs both self-tests after laying out the release zip, then,
**when `data/furigana/readings.toml` is in the commit**, makes
`furigana-<version>.zip` with `make_mod.py` and attaches it beside the
release zip (the draft `v*` release and `latest`). Until FB merges, the step
says the file is not there and attaches nothing. The release zip carries
`mods.py` (reached from `player_sources.py`); `make_mod.py` stays in the
repository.

## 7. Checked (2026-10-22, this machine; no game data in any test)

- `mods.py selftest`: 28 zips, each refused with its sentence or accepted;
  the cycle in a temporary home: added into a home without ja-JP (state
  `needs`), a refused zip leaves the list alone, the step skips with the
  layer named, with `loc\ja-JP` the state is `not-built`, the builder absent
  is refused naming `tools/ruby_build.py`, a stand-in builder (one kind-18
  chunk) builds the layer and lists it, building again leaves the manifest
  byte for byte, adding again replaces, remove takes the folder, row, layer
  and rows. `make_mod.py --selftest`: the zip deterministic, two members,
  read back by `mods.py`; a folder without the readings refused.
- The setup hook, with the importer's build and install replaced by
  stand-ins (scratch `setup_hook_check.py`): without the builder setup ends
  `error The mod furigana (Japanese (Furigana)) needs the reading-row
  builder, tools/ruby_build.py, ...` and leaves no cache; with the stand-in
  builder the new cache holds `opt\ruby-furigana-ja-JP\` listed in its
  manifest.
- `config_tests` (host build): 278 checks, `TestRuby` among them (322 at group LX's end, the Extras toggle's among them).
- `dialog_test.py`: all ok, 58 checks with the Mods tab and the box
  (section 5's table, the Mods tab's add / refusal / list / greyed rows /
  remove). It had stopped on main at the disc-only step (DIV-0102) and failed
  two DIV-0150 expectations; its fake disc-only caches now hold a language
  layer and the expectations follow DIV-0150.

## 8. Not done, and for the other groups

- **FB**: `ruby_build.py furigana` must take `--readings FILE` (the mod's
  list in place of `data/furigana/readings.toml`) and treat `--out` as the
  layer's folder, writing `dat\` under it; FB's brief named `--out`'s default
  as the `dat\` folder itself. When the tool lands, `make_release.py` walks
  its imports - SudachiPy is third-party and must be bundled or listed in
  `NOT_BUNDLED`, or the release script stops.
- `importer.py verify` / `verify_opt` does not know a mod's layer (it is in
  no recipe) and will report it; `stale` and `--reuse` do not record it, so
  every setup rebuilds it.
- A mod is built into the **home's** cache; a `cache=` naming another cache
  is not built into.
- Mods of any other kind (art, scenario; IDEAS I33) - the `kind` field is
  open, the tools refuse what they do not know.

## 9. A second kind: `table-edit` (2026-11-11, DIV-0213)

The image-layer round's group C ([`image-layer-round.md`](image-layer-round.md)
4 and 5c). A mod that changes numbers of the game's tables.

```toml
# mod.toml
[mod]
id = "example"            # the layer is mod-<id>: 19 characters at most
name = "..."
version = "1.0"
author = "..."
kind = "table-edit"
requires = []

[table-edit]
edits = "edits.toml"
```

```toml
# edits.toml
[[set]]
table = "weapons"         # a tables.toml key
record = 12               # the CSV's id column
field = "attack"          # a column of the CSV
value = 40

[[set]]                   # a byte no field names yet
table = "weapons"
record = 12
at = 0x11
type = "u8"
value = 3
```

- The zip holds the author's numbers only. It holds no table and no record of
  the game's, and a `.bin` in it is refused as game data like any other.
- **To make one:** `python tools/tables.py export --cache <home>\cache --csv
  <a folder>` writes one CSV a table, with the language's names as a `name`
  column for reading. Copy the folder, change numbers in the copy, then
  `python tools/make_mod.py table-edit --from <the export> --to <the copy>
  --id <id> --title "<name>" --out <zip>`. A changed `name` is left out and
  said: a rename is a text mod. The CSVs are game data; they stay on the
  machine that made them.
- **What `mods.py add` refuses:** a table the catalogue does not have, or one
  that stays in the image; a record past the table; a field the table does
  not have; a value the type does not hold; `name`, or a byte in a name
  field; one cell set twice.
- **The build step** (`tools/table_edit_build.py`) copies the install's
  `base\exe\tables\<key>.bin`, writes the numbers, and emits
  `cache\opt\mod-<id>\exe\` with `cells` naming exactly the edited bytes. An
  install built before the table files is refused with the sentence that says
  to build it again.
- **The launcher** adds each built table mod's layer after the other layers,
  in this list's order; where two mods set one byte the later wins and the
  game's log says so. `opt=none` leaves them off. The hosted build takes none.
- **Not yet:** a row's name beside its number on the Mods tab; a switch to
  turn one mod off without removing it; a romhack's edits converted to rows
  (the round's group D).

## 10. A third kind: `disc-patch` (2026-11-11)

The disc-patch round's group A ([`disc-patch-round.md`](disc-patch-round.md) 2
and 5a). A romhack as its author released it: the `.xdelta` of a disc's data
track, untouched. `tools/disc_patch_build.py` builds it into two layers
(5b there): `mod-<id>`, what no language owns, and `mod-<id>-<tag>`, its
edits to the text of its disc's language. The first also holds the numbers
the hack changed in the tables the PC keeps in its `.data` (3b there), as
cells of the table files. The launcher hands both to the game when the
language played is that disc's (5d there), and the build's `report.toml` says
what of the patch the mod does not have.

**On and off.** A `disc-patch` or `table-edit` mod has a check box at the left
of its row on the Mods tab. It is ticked when the mod is added. Not ticked,
the mod stays installed and built and its layers are not handed to the game
(`mods_off=<id>` in `bof3x.ini`). A reading-row mod has no box: Settings'
*Row above the text* chooses it.

```toml
# mod.toml
[mod]
id = "example"            # the layers are mod-<id> and mod-<id>-<tag>: 13 characters at most for en-US
name = "..."
version = "1.0"
author = "..."            # required for this kind
kind = "disc-patch"
requires = ["en-US"]      # the language of the disc's text

[disc-patch]
build = "psx-us"          # the disc the patch was made against
patch = "example.xdelta"  # the author's file, at the zip's root
patch_sha256 = "..."      # of that file
result_sha256 = "..."     # of the data track the patch makes
```

- The zip holds `mod.toml` and the patch. The patch is checked when the zip is
  added: its hash, that `tools/vcdiff.py` reads it, and what it writes (800 MiB
  at most).
- States: `needs` (the language is not held), `needs-disc` (the language is
  held and the source list has no disc of `build`), `not-built`, `built`.
- The author's patch is not committed to this repository: it holds a few
  bytes of the game ([`romhack-mods.md`](romhack-mods.md) 8, M3).

## 11. Behaviours a mod needs, `BOF3X_ON`, and what a build does not carry (2026-11-12)

The design of [`romhack-mods.md`](romhack-mods.md) 4 item 4, built as the
foundation of the behaviours round ([`disc-patch-round.md`](disc-patch-round.md)
6a). A romhack's code edits are the PC's to reimplement; each one becomes a
**behaviour**: a change of ours under its own ledger entry, **off by
default**, that a mod turns on by naming it. The mod stays data; the
behaviours are reviewed code of ours.

### 11.1 The engine

| | |
|---|---|
| The call | `bof3::BehaviourOn(bof3::behaviours::Id::<Name>)` (`src/hook/behaviour.h`): an array read, cheap in any frame |
| The names | generated: `tools/ledger_switches.py` reads every such call in `src/` (class `behaviour`) and writes the enum `bof3::behaviours::Id` and its `kNames` into `src/hook/code_names.h`, beside the code names. `--check` fails on a stale header; the compiler fails on a name the header does not have |
| The input | `BOF3X_ON=<names>`, `BOF3X_OFF`'s list syntax (a name, `*` for every behaviour, `-Name` to take one out; an exclusion wins wherever it stands). Read once by both hosts, first in `InjectAll` (`Behaviour_Read`) |
| A name that is no behaviour of this build | a `Fatal` naming it: `BOF3X_ON names "X", which is not a behaviour of this release` |
| A name on | a log line, `behaviour ON  <Name> (BOF3X_ON)`, then `behaviours: N of M on` |
| `BOF3X_OFF` names it too | it stays off, with a log line (`off wins`). `BOF3X_OFF` takes a behaviour name without the "not a code name" stop (`detour.cpp InjectReport`) |
| An ask before the list is read | a `Fatal` naming the behaviour: the answer would be "off" whatever the list says (the hook-order trap PR 217's `IsEnabled` guard closes for detours) |
| The self-test | `Behaviour_SelfTest`, under the exe host's `BOF3X_SELFTEST_ONLY` or `BOF3X_SHADOW=behaviours` hosted: the parser on a table of names of its own (`SelfTestAlpha`, `SelfTestBeta`, `SelfTestGamma`), never the generated list, so they need no ledger entry; a `Fatal` on a case that does not hold. `tools/ledger_check.py`'s own self-test puts a `SelfTestBehaviour` into a copy of the inventory to prove its checks |

### 11.2 `mod.toml`

```toml
[mod]
# ... as in sections 2, 9 and 10
needs = ["SmokeSkill", "CampMasters"]   # disc-patch and table-edit only: the behaviours it turns on

[disc-patch]
# ... as in section 10
skip = [{table = "shop_records", records = [40, 49]}]   # optional: records the build must not carry
```

`mods.py add` / `check` refuse, each with its sentence: `needs` on a
`reading-row` mod; `needs` that is not a list of names (a capital letter, then
letters, digits or `_`, 48 at most), more than 32 of them, or a name twice; a
`skip` that is not a list of at most 64 `{table, records}` entries, a table
the catalogue does not have or that is no table file of the PC's data, or
records that are not `[first, last]` inside the table. A name this release
does not have is **not** a refusal - a newer release may have it - but the
mod's `missing`.

### 11.3 What `mods.py` gives

- `<home>\mods.toml` keeps `needs` (a list of names) and `skip` (as
  `[["shop_records", 40, 49]]`, the form a JSON list writes as TOML).
- `list --json` and `add --json` give `behaviours` (the needs, in order) and
  `missing` (the ones this release does not have). The release's list is the
  behaviours block of `src/hook/code_names.h`, the header the game and the
  launcher are built with; the release ships it beside the tools
  (`build_reuse.SHIPPED_DATA`). `list` without `--json` names the missing ones
  on the mod's line.

### 11.4 The launcher

- **The hand-over.** `ConfigModLayers` leaves off a mod that needs a
  behaviour this release does not have, with a line on stderr naming them,
  and records each mod it hands over with its needs (`ModAsk`). After the
  layers are final (`ConfigOptPlayable`, and the hosted build's cut),
  `ConfigModOn` writes `BOF3X_ON`: the ini's `on=` as written, then the needs
  of each mod whose layers all reached `BOF3X_OPT`, in the Mods tab's order,
  each name once. So a mod's behaviours go with its layers, both or neither:
  unticked (`mods_off=`), not built, under another language or past the 16
  layers, it gives none. An environment's `BOF3X_ON` wins, as for the others.
- **`on=`** in `bof3x.ini`: the player's own list, kept as written, empty by
  default. The game checks the names.
- **`off=` against a need: `off=` wins.** A need that `off=` (or a preset's
  `off` list, or an environment's `BOF3X_OFF`) turns off is not put in
  `BOF3X_ON`, the mod's layers still go, and stderr says `the mod <id>
  (<name>) is not complete: off= turns off <names>, which it needs. Its data
  is on`. The engine keeps the same rule for lists set by hand.
- **Which mod asked.** For each behaviour a mod turns on, a stderr line at
  start: `the mod <id> (<name>) turns on the behaviour <Name>`. On the Mods
  tab, the selected mod's status line ends `When it is on, it turns on these
  behaviours of the game: <names>.` This is the smallest honest form of
  romhack-mods.md 5's "the Settings page says which mod asked": the Mods tab is
  where a mod is turned on and off, so the answer is there. A line on the
  Settings page is not built (the owner's call, if wanted).
- **Greyed.** A mod with a `missing` behaviour is greyed on the Mods list,
  `needs <names>: this release does not have it` (or `them`), its box does
  not change, and its status line says `It needs these behaviours of the
  game: <names>. This release does not have all of them, so the mod stays
  off.`

### 11.5 What a `disc-patch` build does not carry

| `report.toml` `[not_carried]` key | What | The Mods tab's `lacks` part |
|---|---|---|
| `zero_record_bytes`, `zero_records` | bytes the patch writes into records of a table file that the install's own table leaves **all zero**, where the filled record is outside the table's field bounds | `N bytes that the patch writes into M empty records of a table (they are not table numbers)` |
| `skipped_bytes` | bytes of the records `skip` names | `N bytes of table records that the mod skips` |
| `outside_files`, `outside_bytes` | EMIs the patched disc holds in sectors its directory does not list (and where the original does not hold the same bytes) | `N files that the disc's directory does not list` |
| `outside_sectors` | such sectors that are no EMI | `N other sectors that the disc's directory does not list` |

**The zero-record rule.** A record the original leaves entirely zero is not
the mod's to fill unless every field of the patched record is inside its
`range` (`tables.toml` `[[table.field]] range = [lo, hi]`, a reader's bound,
cited, status `evidence`; `tables.py out_of_bounds`). A field with no range
takes any value, so a table with no bounds is carried as before. A record the
original stocks is the mod's whatever its values. The rule is
`disc_patch_build.write_image_part`'s; the first table with bounds is
`shop_records` (`level` 0..11, `category` 0..4; D415).

**Files outside the directory.** `disc_patch_build.outside_files(orig,
patched)` lists them (LBA, sectors, sections), `outside_sections(disc, lba)`
reads one file's sections for the code that carries such a file later, and
`disc_patch_census.py` gives each section a row with `where = outside`. None is
carried into a layer.

### 11.6 Building a behaviour: what its engineer adds

Write the change in the game code behind `if
(bof3::BehaviourOn(bof3::behaviours::Id::<Name>))` (include
`hook/behaviour.h`; ask from game code that runs after `InjectAll` has
started, never from a static constructor or an `Inject` that runs before
`Behaviour_Read`), and keep the original's path in the `else`. Run `python
tools/ledger_switches.py` and commit the regenerated `src/hook/code_names.h`
(the enumerator does not exist until then, so the build fails first). Write
the ledger entry with `- **Switch:** <Name>`, `- **Default:** off - a mod's
needs or on= turns it on (docs/mods.md 11)` and `- **Player:** yes -
BOF3X_ON, on=`, the other fields as for any entry. `tools/ledger_check.py`
then checks both ways: a behaviour asked in `src/` that no entry's `Switch:`
names is an error (`has no ledger entry`), as is one named by two entries or
by an entry whose `Default` is not `off`; an entry whose `Switch:` names a
behaviour `src/` does not ask for is `nothing in src/ reads the name`.
Nothing in the launcher or in `mods.py` changes: both read the generated
header. Prove it with a route under `BOF3X_ON=<Name>`, and the mod with its
`needs` naming it.

