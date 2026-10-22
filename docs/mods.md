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
| an `id` that is not lowercase letters, digits and single hyphens, or whose layer `ruby-<id>-<base>` is over 23 characters | the DLL's layer-name limit (`ConfigOptValid`) |
| a retired bare language code (`ja`), a tag the game does not know | DIV-0005 |
| `reading-row.base` not among `requires`, or a base the builder cannot read (today: only `ja-JP`, furigana) | |
| `readings` not a plain `.toml` file name, or not valid UTF-8 TOML | |

Adding a mod whose `id` is installed replaces it (a re-issued readings list
is a newer zip of the same id); `add --json` says which version it replaced.

## 3. The tools

| Command | Output | Exit |
|---|---|---|
| `mods.py add ZIP --home H [--json]` | one flat object: `path, ok, id, name, version, kind, requires, base, layer, state, needs, problem, replaced` | 0 added, 1 refused |
| `mods.py remove ID --home H` | `removed: ID` / `not installed: ID`; the folder, the row, and the layer built from it (`cache\opt\<layer>\` and its manifest rows) | 0 / 1 |
| `mods.py list --home H [--json]` | one object a mod, in the list's order; `state` is `built`, `not-built`, `needs` (`needs` names the tags) or `problem` | 0 |
| `mods.py build [ID] --home H` | the step's lines; `error <sentence>` on a failure | 0 built, 1 failed, 2 no install built yet |
| `mods.py check ZIP [--json]` | `add`'s checks alone | 0 / 1 |
| `mods.py selftest [--keep DIR]` | every refusal and the add / list / build / remove cycle (no game data) | 0 / 1 |
| `make_mod.py furigana [--src DIR] [--out ZIP]` | the furigana zip from `data/furigana/`: `mod.toml` (written from `MODS` there) and `readings.toml`, nothing else; deterministic; checked by `mods.py`'s reader | refuses when the folder lacks `readings.toml` |
| `make_mod.py --selftest` | against a temporary folder, never `data/furigana/` | 0 / 1 |

`<home>\mods.toml` is written only by `mods.py`, in `sources.toml`'s shape: a
`[[mod]]` table a mod, a `key = value` line each, the values JSON-quoted
(`id, name, version, kind, requires, base, readings, files, zip, sha256,
added`). The launcher reads it (`ConfigModsInstalled`) for the Settings box.

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
IDEAS I33), the list - name, version, kind, state - owner-drawn so a mod the
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
