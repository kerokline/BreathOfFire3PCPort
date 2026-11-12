# The disc-patch round: a romhack's kept-section changes as a mod

**Status:** IN PROGRESS (issue 216). Staged 2026-11-10; staged again
2026-11-11 against `main` `8d2d987d`, after the image-layer round merged (PR
224), and group M run the same day: its five measurements are in
[`romhack-mods.md`](romhack-mods.md) section 8 and they change sections 1 and 3
below. Groups A, B, E and C are built (5a, 5b, 5c, 5d): the mod builds on the
player's machine, with the overlays' table numbers as cells of the table files
(the image-layer round's group D), and the launcher hands its layers to the
game, with a check box at the left of the mod's row to turn it off. D (the
release) is held by the owner's call 1. Nobody has played it. Step 4 of
romhack-mods.md section 6.
Read that note first: it holds the measurements this plan stands on. `main`
holds the three things the round starts from: `tools/vcdiff.py`,
`tools/disc_patch_census.py`, and `author` / `url` / `credit` in `mod.toml` and
on the Mods tab.

**This round is the gate for the image-layer round's group D** (a romhack's
edits to the rows of a table, [`image-layer-round.md`](image-layer-round.md) 5):
that group is written into this round's builder, so it is group E here and
starts when group B is done.

The owner (2026-11-09, issue 216): a process that turns a romhack into a mod
package, "exactly the kind of mod people would be used to"; the first one is
*Breath of Fire 3 Definitive Edition 1.0* by TheGreatBen, repackaged with his
permission on the condition that he is credited.

## 0. The shape in one paragraph

A new mod kind, `disc-patch`. The zip holds `mod.toml` and the author's
`.xdelta`, byte for byte as he released it. On the player's machine the build
step applies the patch to the player's own disc in a temporary file, runs the
**existing** builders on the original disc and on the patched one, and keeps
the chunks that come out different: that set is the mod's layer,
`cache\opt\<layer>\dat\`, listed in the manifest as the mod's. Nothing about a
section's format is learned again, and the importer's hash gates are not
touched, because the importer never sees the patched disc - the mod's step
does. The text builder does not see the patched disc whole either: it is given
the original disc with the patched sections put in only where the PC keeps the
section (group M, measurement 1). This round carries only what the PC holds as DAT chunks (662 of this
hack's 1,551 edit runs: scripts, text, enemy tables, images). Everything else
is counted and reported to the player, never dropped in silence.

## 1. What is settled, and what is not

Settled by the measurements (romhack-mods.md 1 to 3):

- The patch format is xdelta3's VCDIFF with DJW; `vcdiff.py` applies this one
  in 6 seconds and catches a wrong disc at the first window.
- The base is `psx-us`, track 1: the first 442,938,048 bytes of the
  single-file image, or the redump `(Track 1).bin`. The step must take either.
- `importer.py build` refuses the patched disc at the first changed chunk, and
  with the gate waived stops at `base/exe` (BATTLE section 15 is 8 bytes
  longer). So the round does not go through `importer.py build`.
- 52 base chunks differ: 51 enemy tables (`widen`) and area 63's image.

### 1a. Group M as run (2026-11-11; romhack-mods.md 8 has the numbers)

| | Asked | Answer | What it changes |
|---|---|---|---|
| 1 | Does `loc_build.py` take the patched disc? | **No, and yes.** The patched disc whole is refused twice: the builder finds its label tables in the overlays by Capcom's numbers around them, and the hack changed those numbers (the character records, the formation pairs). Given the original disc with the patched sections put in at `0x80010000`, `0x8001A000` and `0x0E001000` only, it builds with no refusal: 78 area script blocks, 44 pool chunks, 2 corrected copies (kind 19, DIV-0203) and 1 area page differ from the original's layer, and nothing else | step 4 below builds from a **hybrid disc**; the label and name edits in the overlays are group E's |
| 2 | An `opt/` chunk over a `loc/` chunk of the same tag | **The last one walked wins**: `LoadDatFile` walks the file, then the language layer, then each `BOF3X_OPT` layer in order, and a kind-0 chunk is copied to its tag whoever sent it (`dat_load.cpp`, read; not run). So the layer holds whole chunks. One consequence: a language's enemy names are 12-byte chunks laid over the base's table, so the mod's whole table, walked later, blanks them | step 3 writes the names again after each enemy table |
| 3 | What the patch's ADD data is | 567,385 literal bytes: 531,110 are sector addresses and error-correction bytes, 27,607 the author's edits, 6,687 the disc's directory, **1,981 Capcom's bytes** carried as they are. 4,129,805 more bytes are 3,111 runs of one repeated byte (fill) | owner call 2: not zero, so the file stays out of the repository |
| 4 | Area 154's kind-7 section; the two images | The kind-7 section is **the sample body of the area's sound bank**, which the PC keeps as a kind-2 chunk: the census's `dropped` is wrong for it. Seven whole sectors differ and the bank built from it is 161,448 bytes shorter. Area 63's image is a base chunk, copied; area 115's is a language page, built by `loc_build.py` | owner call 5 (new): is the bank change meant |
| 5 | A PC-based install | The same text set, one more area block (79), and one more text slot kept as the PC shipped it (497 against 496) | step 4 runs the way the install's language layer was built; the layer is built per install, not shipped |

The five as they were asked:

1. **Does `loc_build.py` take the patched disc?** `loc_build.py all --disc
   <patched> --cache <a copy of a psx-us cache> --lang en-US`, then its
   `loc\en-US\dat\` against the original's, chunk by chunk. Expected: about
   66 area script blocks and the shared text pool differ and nothing else. A
   refusal (a width check, a control code) is a finding, and decides whether
   the text half is in this round at all.
2. **What the engine does with an `opt/` chunk over a `loc/` chunk of the
   same tag** for a script block (`0x80010000`, kind 0) and for a name-table
   chunk. The reading-row layer is a kind-18 side chunk and area4-walls a cell
   plane, so neither proves it. If the last layer walked wins, the mod's layer
   holds whole chunks. If not, the round needs the answer before group B.
3. **What the patch's ADD data is.** The xdelta's data sections unpack to a
   few megabytes against about 40 KiB of real edits; the rest should be raw
   sector addresses and error-correction bytes, which change when files move.
   Count the ADD bytes by where they land (sector header and ECC, or user
   data; inside a changed section, or not). The number that matters for
   rule 1 is how many ADD bytes are Capcom's content that the encoder did not
   find a copy for. This feeds owner call 2 below.
4. **Area 154's kind-7 section and area 115's image**: the first is `dropped`
   in the census; say what it is. Both images: do they pass the PC's kind-1
   transform as the originals do?
5. **A PC-based install** (the PC's `DAT\` plus the US disc for English): the
   same layer over it, or a different chunk set? The enemy tables are
   language-neutral and `base/`'s are the same bytes whichever source built
   them (importer.py's `widen` note), so those should be. The text is built
   against the PC's containers there (`build_languages`), so run measurement 1
   that way too.

## 2. The format

```toml
[mod]
id = "definitive"                  # the folder; the layer is mod-<id>, so 19 characters at most
name = "Definitive Edition"
version = "1.0"
author = "TheGreatBen"             # required for this kind
url = "https://www.romhacking.net/hacks/10312/"
credit = "..."                     # the author's line, his words
kind = "disc-patch"
requires = ["en-US"]

[disc-patch]
build = "psx-us"                   # a fixtures.toml build id; a closed list in the tool
patch = "definitive-1.0.xdelta"    # a file at the zip's root
patch_sha256 = "d321b7af..."       # of the author's file
result_sha256 = "830f07f8..."      # of the patched data track
```

- `.xdelta` is a new allowed member extension, for this kind only. The
  existing bounds hold it (1.35 MiB against 4 MiB a member).
- `author` is required here; `mods.py` refuses a `disc-patch` mod without it.
- `result_sha256` makes the build step's output checkable before anything is
  built from it: a patch that applies but not to these bytes stops there.
- A second file, the author's readme, is worth carrying if he wants it shown;
  not in the first cut.

## 3. The build step

`tools/disc_patch_build.py`, run by `mods.py` as `ruby_build.py` is
(`KINDS[kind]["builder"]`; `build_layer` needs its reading-row words made
general first).

1. Find the player's disc of `build` (`<home>\sources.toml`). None held: the
   mod's state is `needs`, with the disc named, as a language is today. A
   source may be a `.zip` (the owner's install holds its discs that way): the
   importer's own unpacking under `<home>\cache\tmp` is used, and the patch
   reads the data track from there.
2. `vcdiff.apply_file` into a temporary folder under the home (443 MiB; say so
   before, remove after, also on failure). Check `result_sha256`.
3. **Base chunks.** For every recipe row the build is a source of, get the
   payload from the original and from the patched disc through the same
   transform (`importer.DiscSource.get`, `TRANSFORMS`); where they differ, the
   patched payload is a chunk of the layer. The experiment's wrapper
   (`build_as_us2.py`, romhack-mods.md 3) is this loop with a counter.
   - **An enemy table is followed by its names.** The layer is walked after
     the language layer, so a whole table from it blanks the names the
     language laid (1a, row 2). After each enemy table the layer holds the
     language's name chunks again, made by `loc_build.convert_enemy_names`
     from the patched table and the patched disc, so a name the hack changed
     is the hack's.
   - **A sound bank** does not go through `TRANSFORMS` (`vag.importer_source`,
     which returns nothing for a bank that is not the recipe's). The step
     builds each bank from both discs and compares. A changed bank is carried
     or reported by owner call 5; until that call it is reported.
4. **Language chunks.** Not the patched disc: `loc_build.py` refuses it (1a,
   row 1). The step gives the builder a **hybrid disc**: the original disc's
   files, with a section of the patched disc put in where the two differ and
   the section's load address is one the PC keeps as text or as a page
   (`0x80010000`, `0x8001A000`, `0x0E001000`; the list is in the tool, with the
   reason for each). `loc_build.py` gets one new input for it (a disc object
   in place of a path; no change to what it builds). It runs the way the
   install's language layer was built (disc-only, or against the PC's files),
   into a scratch folder; every chunk that differs from the install's
   `loc\<tag>\` chunk goes into the layer, a kind-19 corrected copy with the
   kind-0 block it follows. The original's side is already in the cache, so
   this is one run of about 85 seconds.
5. Write the two layers (3a), list every chunk in the manifest as
   `mod:<id>@<version>:<build>` (mods.md 4's rows).
6. **The report.** Run the census's comparison (`disc_patch_census.py` as a
   module) and write `<home>\mods\<id>\report.toml`: edit runs carried, by
   kind; edit runs not carried, by class (relinked addresses: none needed;
   overlay data: needs the image layer; code: needs engine behaviours). The
   Mods tab shows the two totals for the selected mod. This is the round's
   honesty rule: the player sees "carries the enemy and text changes; 429 of
   the hack's changes need parts this release does not have", not a mod that
   looks whole.

### 3a. Two layers (the owner's call 4)

| Layer | Holds | Played over |
|---|---|---|
| `opt\mod-<id>\dat\` | step 3's base chunks: enemy tables with their name fields blank, base images | any text |
| `opt\mod-<id>-<tag>\dat\` | step 4's chunks (script and text blocks, the text pools, a language's pages, corrected copies) and, for every enemy table of the first layer, that language's names again | `<tag>` text alone: the engine refuses a layer whose name ends in another language's tag (`LayerLanguage`) |

The text layer is walked after the first, so its names land on the tables.
`<tag>` is the language of the disc the patch was made against. With the two
names an id is 13 characters at most for a tag of five (`en-US`), since a layer
name holds 23.

**Over another language** (not built in this round): the first layer alone
would blank that language's enemy names, so it needs a names part for that
language - the install's own name chunks for those areas, copied from
`loc\<other>\`. Until that exists the launcher offers the mod over its own
language only.

### 3b. The overlays' data (group E, as built: 5c)

The PC holds the overlays' tables in `.data`, at addresses of its own, and the
image builder (`tools/exe_tables.py`) fills them from the disc by address. A
hack moves bytes inside an overlay, so its sections cannot be handed over as
they are. Each changed overlay section is first put back at the original's
addresses (`unmoved_section`):

| What the two sections show | What is done |
|---|---|
| a run of the same length that differs | taken, where it is now |
| a word that differs only by a distance bytes moved (a pointer, a jump, the low half of an address) | left the original's: it is the patch tool's relinking, and the PC's pointers are its own |
| bytes put in and taken out so that the shift comes back to what it was, inside one table the PC holds at an address of its own, or with nothing in the section relinked by a distance inside it | a rewrite in place (a record with a new name and numbers, a list reshuffled): taken address for address |
| the same, with a pointer relinked by such a distance; or a shift that does not come back | bytes moved: the same-length edits follow them to the original's address; the bytes put in or taken out have no address there and are not placed |

Then `exe_tables.disc_image` builds the image from the original disc and from
that one, the pointer words are masked in both, and the bytes that differ are
the mod's:

- a byte in a table that is a file: a cell of `opt\mod-<id>\exe\tables.toml`,
  in a copy of the install's own table file (image-layer-round.md 3.2);
- any other byte: a raw range of `opt\mod-<id>\exe\place.toml`, where the
  install's image is of a class a layer may write. A PC-built image
  (DIV-0212) is the PC's own outside the table files, so there those bytes
  are not carried, and counted (owner call 6);
- a pointer the hack aims elsewhere, and a length change on bytes the image
  takes: not carried, counted, each length change with its file, section,
  address and lengths.

**The text builder sees the same world.** `loc_build.py` is given the hybrid
disc with these un-moved overlay sections in it as well as the patched text
sections, and reads the image with the mod's bytes laid over it
(`loc_build.image_overlay`). The tables it finds by Capcom's numbers are then
found by the hack's numbers on both sides, and a name the hack changed in an
overlay table is built into the text layer as that language's name chunk.
This is what answers group M's first refusal for good: nothing is waived.

The rule in the third row is a judgement, not a proof: it reads "nothing was
relinked by that distance" as "nothing moved". A hack that moves a table and
relinks nothing would be taken wrong. The self-test has both shapes.

A patched section that passes no transform (a kind-1 image that does not
decode, a text block `loc_build.py` refuses) stops the build with the file,
the section and the builder's sentence. No partial layer (rule 4).

## 4. The launcher

**As built (group C, 5d), where it differs from the proposal below:** the ini
line is `mods_off=<id>,<id>` - the mods to keep off - and a mod is **on when
it is added**. A `table-edit` mod was on when built before this round
(DIV-0213), and one rule for both kinds is the owner's call 3; with the list
of the ones that are off, an ini from before the box still plays every mod it
played. The report's sentences are in the log box, not in the row.

- **On and off.** A built `disc-patch` mod needs a switch. The reading-row
  mod has the *Row above the text* box; this kind has nothing, and neither has
  `table-edit` (the image-layer round: a table mod is on when it is built,
  `ConfigModLayers`). One answer for both kinds. Proposed: a
  check box column on the Mods list for kinds that are simply on or off,
  written as `mods=<id>,<id>` in `bof3x.ini`, and the launcher adds
  `mod-<id>` to `BOF3X_OPT` after the `opt=` layers when the mod is built,
  its disc's language is the one played, and the box is ticked. Off by
  default after an add. (Owner call 3.)
- **Greyed reasons**, as every option has them (the owner's rule of
  2026-10-10): `needs the US PlayStation disc: add it on Data, Sources`;
  `only with en-US text`; `not built`; `built: N changes carried, M not (see
  the status line)`.
- The author line and the kind are there already.
- All text in ASD-STE100.

## 5. Groups

| Group | What | Files | Done when |
|---|---|---|---|
| **M** | Section 1's five measurements, written into romhack-mods.md | scratch only | each has a number or a named refusal |
| **A** | `mods.py`: kind `disc-patch`, its keys and checks, `.xdelta` allowed for it, `author` required, the layer name `mod-<id>`; `build_layer` and the state words made general | `tools/mods.py`, `docs/mods.md` | `mods.py selftest` with a synthetic patch from `vcdiff.make_patch`: accepted, and each new refusal has its sentence |
| **B** | `tools/disc_patch_build.py`: steps 1 to 5 | new tool; `tools/make_release.py` learns it and `vcdiff.py` | self-test on synthetic EMIs for the differ-and-collect half; on the owner's machine the layer holds exactly group M's chunk list |
| **C** | The report (step 6) and the launcher (section 4) | the builder, `sources_dialog.cpp`, `config.cpp`, `dialog_test.py`, `config_tests.cpp` | `dialog_test.py` passes with a fake built `mod-` layer: ticked, unticked, greyed |
| **D** | `make_mod.py definitive` and the release | `tools/make_mod.py`, `release.yml`, `THIRD_PARTY.md` | after owner calls 1 and 2 |

| **E** | The image-layer round's group D: a changed dropped section's data runs placed through `exe_maps` onto table cells (`opt\mod-<id>\exe\tables.toml`) and raw ranges, the map translated where the hack moved bytes (romhack-mods.md 4 item 2); what does not place is refused by file, section, range and reason, and counted in the report | `tools/disc_patch_build.py`; `tools/exe_tables.py` read only, if it can be | the 240 data runs of the census each land in a cell, a raw range, or a named refusal; the mapped `.data` with the layer differs from the plain one in exactly those bytes |

M is done. A before B; C beside B; E after B; D last.

**Group E's first questions**, from group M: (1) the two landmarks that stopped
`loc_build.py` (the character records and the formation pairs) are data runs E
carries, so the same numbers are read by two builders - say which owns each;
(2) a name the hack changed inside a dropped section (a new ability's name) is
text, a language's, and a table cell cannot hold it (the image-layer round's
call 4): it needs the hybrid disc to take that section with the landmark
translated, not waived; (3) `base\exe` is in the table-file form now, so the
scratch caches of 2026-11-10 are not this round's - `D:\bof3x-runs\dp1111`
has fresh ones.

## 5a. Group A as run (2026-11-11)

`tools/mods.py` alone; no engine or launcher file.

- **The kind** `disc-patch`: `[disc-patch]` with `build`, `patch`,
  `patch_sha256`, `result_sha256`; the layer `mod-<id>`. `build` is one of a
  closed list in the tool, each with its language and its name on the Sources
  page: `psx-us` alone, the one disc the builder's parts have been run on.
- **Refused when the zip is added**, each with its sentence: no `author`; a
  disc not in the list; `requires` without the disc's language; a patch that is
  not a `.xdelta`; a patch that is not the file `patch_sha256` names; a hash
  that is not 64 hex digits; a file `vcdiff.py` cannot read (not VCDIFF,
  another secondary compressor, a damaged header); a patch that writes more
  than 800 MiB. `.xdelta` is no new exception in the member rules: a zip holds
  only the files `mod.toml` names, and only this kind's `patch` key takes one.
- **A new state, `needs-disc`**: the language is held and the source list
  (`<home>\sources.toml`) has no disc of `build`. `needs` is the disc's name
  and `problem` the sentence, so the launcher as it is today shows the
  sentence; group C gives it its own row text. The build step skips such a mod
  with a line, as it skips a mod whose language is not held.
- **The build step** calls `disc_patch_build.build(home, cache, layer, the
  mod's folder, its row, the manifest's source word, say)`, which returns the
  manifest rows and raises `disc_patch_build.Bad`. That tool is group B's. Until
  it exists a `disc-patch` mod's build stops with "this release does not have
  it", and `make_release.py` refuses to pack (an import it cannot place): no
  release is cut between A and B.
- **Checked:** `mods.py selftest`, 0 failures, with 12 new zips (one accepted,
  eleven refusals, the patch made by `vcdiff.make_patch` over bytes of its
  own) and the cycle: added with no en-US text (`needs`), with the text and no
  disc (`needs-disc`, the step's skip line), with the disc listed
  (`not-built`), the row in `mods.toml`, the build refused by name where the
  builder is not there, removed. The author's real patch in a zip with the two
  real hashes: `mods.py check` accepts it (the zip is 1,016,112 bytes, in the
  scratch root, never in the repository).
- **Not done:** DIV-0157's amendment waits for the first group that changes
  what a player gets (B with C). `docs/mods.md` has the kind's section.

## 5b. Group B as run (2026-11-11)

**What was built.**

- `tools/disc_patch_build.py` (new): steps 1 to 5 of section 3 and the two
  layers of 3a. `build(home, cache, the mod's folder, its row, the language
  tag, the manifest's source word, say)` returns each layer's manifest rows.
  Its parts: `changed_sections` (the two discs, EMI by EMI), `collect_base`
  (a changed section through the recipe row's own transform, both sides),
  `HybridDisc` and `hybrid_emi`, `run_loc` (the text builder in a child
  process, against a view of the cache's `base\` or a scratch copy of the PC
  game, as the install's own layer was built - the manifest says which),
  `diff_layer`, `write_layer`.
- `tools/loc_build.py`: one line of behaviour, `disc_opener`, the thing
  `open_disc` calls (it was `psx_disc.Disc`, and still is by default).
- `tools/mods.py`: `layers_of` (a `disc-patch` mod's two layers), the id
  bound by the longer one (13 characters for `en-US`), `built` when either
  layer is in the cache, both removed with the mod, `text_layer` in the JSON
  for the launcher.
- CI: `disc_patch_build.py selftest` in `checks.yml` and `release.yml`.

**Where the build differs from section 3 as staged.**

- **The enemy names come from the text builder's child process**, after its
  run, by `loc_build.convert_enemy_names` on the patched table and the patched
  disc's section, while the language's font tables are still set up. A name
  the hack changed is then the hack's. (On this hack all 216 come out as the
  install's own.)
- **The scratch folder is `<home>\mods\.build-<id>`**, removed when the build
  ends, also on a refusal. It holds the patched track (422 MiB for this
  disc), an unpacked `.zip` disc, and the text builder's output.
- **A patch that changes nothing the PC keeps** is refused, by count.
- **The report file (step 6) is group C's.** The build says what it does not
  carry in two lines; nothing writes `report.toml` yet.
- **The sound bank** is compared by its sections, not built: a changed
  section of a bank's header, samples or cues is counted and not carried (the
  owner's call 5).

**Checked.**

| Check | Result |
|---|---|
| `disc_patch_build.py selftest` (synthetic EMIs, recipe rows and containers; no game data) | 25 checks, 0 failures: the hybrid EMI and its two refusals, the changed-section walk, the collect step with its controls (another build, a transform that takes the change away, a transform that refuses, a step the release lacks), the layer diff with its three refusals, the containers read back, the install's source row and layer mode, a listed disc that is gone, no scratch folder left |
| `mods.py selftest` | 0 failures, with the id of 14 characters refused and the text layer alone counted as built |
| The real patch on a US disc-only install (`D:\bof3x-runs\dp1111\home`, `mods.py add` then `build`) | exit 0 in 74 s. `mod-definitive`: **52 chunks in 52 files, the 51 enemy tables and area 63's image - group M's list.** `mod-definitive-en-US`: 341 chunks in 165 files - 78 script and text blocks, 44 pools, 2 corrected copies, area 115's page (group M's 125) and 216 enemy names |
| Built a second time | both layers the same bytes (the sha256 over every file of each) |
| `mods.py remove` | both layers gone, the manifest the bytes it was before the mod, the scratch folder gone |
| The real patch on a PC-based install with the disc as a `.zip` (`home_pc`: the PC game folder and the owner's zipped US disc) | exit 0 in 34 s, the zip unpacked and removed. The first layer is the same bytes as on the disc-only install. The text layer: 342 chunks in 166 files (79 blocks, as M5) |
| The engine's walk, modelled (`tools/cache_walk.py` `State`: base, `loc\en-US`, then the layers), every container | 166 of 742 containers end differently with both layers; **0 bytes change outside the layers' own chunks**; all 51 enemy tables are the patched table with the install's names; with the first layer alone the 216 names are blank, which is why the two go together |
| The exe host's self-test on the disc-only cache, `BOF3X_LANG=en-US` (`run_st.sh`; the image-layer round's build, the same engine as `main`) | no layer, both layers, the first alone: exit 0, no Fatal, each layer taken as `the cache's opt layer`; the mapped `.data` hash the same in all three (the mod has no image part) |

**Not done.**

- **Acceptance's live half.** No route was run: the state hash with the mod
  installed and off, and a save loaded with it on and an enemy table and a
  text change read back from memory. Both need a hands-off machine and, for
  "on", the launcher handing the layers over (group C) or `BOF3X_OPT` by hand.
- **The control "the text layer under another language is refused"** was not
  run: the scratch install holds en-US alone, and the launcher stops a
  disc-only install before the engine is asked. The rule is the engine's own
  (`ReadOptLayers`, DIV-0086).
- **The census's 662 against the build.** The build's own lines say 125 text
  and page sections went through the text builder, 52 base chunks were taken,
  1 sound section and 169 other sections were not carried. That the 169 are
  exactly the census's dropped sections was not cross-checked row by row.
- The launcher does not know the kind's layers, DIV-0157 is not amended, and
  nobody has played it.

## 5c. Group E as run (2026-11-11): the image-layer round's group D

**What was built.**

- `tools/disc_patch_build.py`: `unmoved_section`, `UnmovedDisc`,
  `image_changes`, `write_image_part` (section 3b); `HybridDisc` takes the
  un-moved overlays; the text builder's child lays the mod's bytes over the
  image it reads, from the moment the disc is opened (a PC install's
  `BOF3.exe` is checked before that, as it is). `edits`, `moved_by` and
  `relocation` moved here from `disc_patch_census.py`, which imports them.
- `tools/loc_build.py`: `image_overlay` and `overlaid`, used by both image
  readers (`PcInstall.exe`, `DiscCache.exe`); empty outside this build.
- `tools/mods.py`: a layer with only raw ranges counts as built.
- No engine file: the loader of the image-layer round takes both forms as
  they are.

**Measured on the real patch first** (`e_measure.py`), then built:

| | |
|---|---|
| Image bytes that differ, pointer words masked | **2,472**: 2,408 in seven tables that are files, 64 in 38 runs outside them |
| The seven tables | `exp_table` 2,132 bytes in 618 records; `shop_records` 149 in 10; `abilities` 105 in 73; `weapons` 12 in 6; `armour` 5 in 4; `accessories` 4 in 3; `event_op_lengths` 1 |
| The 64 loose bytes | under 29 symbols: `Char_DefaultRecords` 10, `Ability_EffectIndex` 9, the rest 1 to 3 bytes each in the areas' and effects' own tables |
| Pointer words the hack aims elsewhere | 0, once a relinked word inside a longer run is told from an edit (198 bytes before that) |
| Length changes on bytes the image takes | 0, once a rewrite inside a fixed table is taken in place (13 before, all in tables: the shop records, an ability's record, the character records) |
| Names the hack changed in the overlay tables | 12: 1 consumable, 6 armour, 1 accessory, 4 abilities. Carried in the text layer as four kind-5 name chunks of `FIRST.DAT` |

**Checked.**

| Check | Result |
|---|---|
| `disc_patch_build.py selftest` | 39 checks, 0 failures: the six shapes of section 3b's table, the un-moved disc and the hybrid over it, cells cut at a record's end, raw ranges in order, a must-equal byte kept and counted, a table file that is not the install's refused |
| `mods.py`, `loc_build.py`, `vcdiff.py` self-tests; `importer.py check` | pass |
| The real patch, US disc-only install | exit 0 in about 90 s. `mod-definitive`: 52 chunks, 7 table files with 1,209 cells (2,408 bytes), 38 raw ranges (64 bytes). `mod-definitive-en-US`: 345 chunks (5b's 341 and the four name chunks). Nothing refused. Built three times: the same bytes |
| The real patch, PC-based install (zipped disc) | exit 0 in 50 s. The same 2,408 bytes of cells; **the 64 loose bytes not carried**, said in the build's line; 346 text chunks |
| The exe host's self-test, disc-only, both layers (`run_st.sh`) | exit 0; `7 tables, 1209 cells, 2408 bytes ... 2408 of them differ`; `38 ranges, 64 bytes ... 64 of them differ`; the mapped `.data` hash is another than with no layer. The same counts from Python over `read_image` |
| The exe host's self-test on a PC game folder with the PC-built image (`run_st_pc.sh`) | exit 0; `base/exe (pc-zh) is BOF3.exe's .data, byte for byte`, then `7 tables, 1209 cells, 2408 bytes ... 2408 of them differ` |

**Not carried, and said by the build:** the overlays' code (169 changed
sections in 121 files were read for their tables only), one sound section,
the boot program (5 places: its file table follows the disc's layout, its
other edits are not read), and on a PC-built image the 64 loose bytes.

**Not done.**

- **What each number is.** The cells are bytes that differ, placed by
  address. No table was read back by field against the hack's readme (the
  readme does not give numbers), and no value was looked at in the game.
- **The live half**, as in 5b: no route, no save loaded, nobody has played it.
- **The census against the build**, row by row: the census counts 240 data
  runs by its own alignment; the build places bytes. The two were not
  reconciled run by run.
- The other install kinds (a European or Japanese base with the US disc for
  text) were not built.

## 5d. Group C as run (2026-11-11)

**What was built.**

- **The hand-over** (`src/launcher/config.cpp` `ConfigModLayers`): after the
  ini's layers and the row above the text, in the Mods tab's order, every
  `table-edit` mod's `mod-<id>` and every `disc-patch` mod's `mod-<id>` then
  `mod-<id>-<tag>`. A disc-patch mod goes over whole or not at all: left off
  with a line on stderr when the language played is not its text's (3a: the
  first layer alone would blank the enemy names), when it is not built, or
  when its layers would pass the game's 16. `opt=none` is still no layer at
  all. For the hosted build, which cannot take a layer with an exe part, the
  text layer is dropped with the first (`ConfigModHosted`).
- **`mods_off=`** in `bof3x.ini`: the ids of the mods to keep off, kept as
  written. Empty by default.
- **The check box** (`sources_dialog.cpp`): a column at the left of the Mods
  list; a box on each `table-edit` and `disc-patch` row, none on a
  reading-row mod (that one is chosen in Settings). A click on the box or
  Space on the row changes it; the row says `built: on` or `built: off`. The
  window keeps the new `mods_off=` and saves it on any way out, as it does
  for the *Row above the text* toggle. A greyed row keeps its box and a click
  does not change it. An add turns the mod on (its id leaves `mods_off=`).
- **The rows' words:** `needs the disc PlayStation, USA: add it on Data,
  Sources`; `built; only with en-US text`; `built: on`; `built: off`. After a
  build of an on/off mod the status line is `The mod is ready. It is on.`
- **The report** (section 3 step 6): the build writes
  `<home>\mods\<id>\report.toml`, counts only - `[carried]` (base chunks,
  enemy tables, text chunks and sections, table bytes) and `[not_carried]`
  (overlay sections read for their tables and not for their program, sound
  sections, places in the boot program, table bytes the install's image
  keeps, pointer bytes, length changes, other sections). `mods.py list
  --json` gives two sentences from it (`carries`, `lacks`) and the text's
  language (`language`); the Mods tab writes the two sentences to its log
  when the mod is selected. The census is not run again for it: the counts
  are the build's own.

**Checked.**

| Check | Result |
|---|---|
| `disc_patch_build.py selftest` | 43 checks, 0 failures (the report: none before a build, the two sentences, nothing left out, a file the tool did not write) |
| `mods.py selftest` | 0 failures (`language`, `carries` and `lacks` for a built disc-patch mod) |
| `config_tests.exe` | 3,654 checks; the 12 new ones pass (a mod `mods_off=` names, both layers in order, another language, two layers past 16, the hosted pair, `mods_off=` read and written); the same 2 failures as `main` (the presets' count) |
| `dialog_test.py`, with a made-up built disc-patch mod (no patch read, nothing built) | 89 checks, 0 failures: the box ticked and the row `built: on`; the log's two sentences; a click unticks it and `mods_off=hack` is written; read back unticked, Space ticks it, `mods_off=` written empty; under ja-JP the row greyed with `only with en-US text` and a click changes nothing; with no disc in the source list the row names the disc. Captured: `D:\bof3x-runs\dp1111\shots_c\` |
| The real mod rebuilt on the US disc-only install | exit 0 in 86 s, the layers as 5c; `report.toml`: 51 enemy tables, 1 picture, 125 text sections, 2,472 bytes of table numbers; not carried: the program changes in 169 sections, the boot program, 1 sound section |
| The exe host's self-test started by the new launcher, **no `BOF3X_OPT` in the environment** (`run_c.sh`; `language=en-US`) | exit 0, no Fatal. Box ticked: both layers taken (`optional layer 2, DAT\mod-definitive.*`, `layer 3, DAT\mod-definitive-en-US.*`), `7 tables, 1209 cells, 2408 bytes`, `38 ranges, 64 bytes`, the mapped `.data` hash the one of 5c's run with the layers named by hand. `mods_off=definitive`, and `opt=none`: no mod layer, the mapped `.data` hash the one with no layer |

**Not done.**

- **Nobody has played it**, and no route was run: the launcher now hands the
  layers over, so the live half of section 6 (the state hash with the mod
  off, a save loaded with it on and a table and a text change read back) can
  be run on a hands-off machine.
- The launcher was not started on the PC-based scratch install
  (`home_pc`), and no install with a second language was built, so "left off
  under another language" is checked by `config_tests` and the dialog test,
  not by a start.
- The row's box is drawn by the list (one owner-drawn row); a screen reader
  reads the row's words `built: on` and `built: off`, not a check box.

## 5e. Call 6 as run (2026-11-11): what the 64 loose bytes are

Each of the 38 ranges was read against the install's image map
(`base\exe\data.toml`) and the bytes on both sides (in memory; none here).

| What | Bytes | Evidence | Now |
|---|---|---|---|
| **Padding** after a table of an odd length, or before one at an odd address | 43 in 28 ranges | each run is the last one to three bytes of a mapped run that ends on a 4-byte boundary, or the first of one that starts off it; the original has zeros there (once, two bytes of its own leftovers) and the patched disc has other leftovers | not the hack's numbers. Still laid as raw ranges on a disc-built image, where no reader reads them; not carried on a PC-built one |
| `Ability_EffectIndex`: the effect handler of an ability | 9 in 2 ranges | `symbols.toml`: one byte an ability id, read by `Effect_ApplyResult` | **catalogued** as `ability_effect_index` (228 by 1; the count is the abilities table's, a hypothesis) |
| The masters' ability grants | 2 in 2 ranges: two abilities change places in one master's list | `MasterGrant_Step1Member` 0x5873C0 reads six (levels, ability) pairs at `0x6642A4 + 12 * master`; the data had no name | **named** `Master_AbilityGrants` and **catalogued** as `master_abilities` (17 by 12; 17 by the extent, a hypothesis) |
| `Char_DefaultRecords`: the records a new game starts each character with | 10 in 7 ranges | `NewGame_InitCharacters` 0x437820 | **catalogued** as `char_defaults` (8 by 164), **a table file with holes** (below) |

**The starting records: a table file with holes.** A table file was one run
of `count * stride` bytes with no other owner inside it. The first seven bytes
of the first record are the hero's default name, which the language layer
owns (`recipes/exe-gaps.tsv`: a `gap`, answered `layer`, on every build), and
on the European PSP disc three more records have name bytes no rule places. A
plain file over them is refused by the engine at start (`the accepted row
"layer" 0x64B390..0x64B397 is on no gap range of the image`) - tried first.
So, at the owner's word, a table may say `holes` in `tables.toml`
(DIV-0212 as amended):

- `tools/exe_tables.py` `carve_tables` cuts out only the table's runs of a
  disc's or the exe's bytes, one `file` range of `data.toml` each; a range of
  another class inside the table stays as it is, and the file holds zero
  there. The file is still the whole table.
- The engine (`src/hook/exe_image.cpp` `LayBaseTable`) lays each `file`
  range's own part of the file; a range must lie inside its table's row.
- No disc holds this table whole, so it has no `[[table.psx]]` row: its bytes
  reach the image through the map, as before, and are then cut into the file.
- On a PC-built image the table is one range with no hole. On the US disc it
  is one range from the eighth byte; on the European PSP disc six.

**The other two tables are found on all seven discs** (`tables.py locate`,
the rows in `tables.toml`), so every install kind gets them as files.

**Checked.**

| Check | Result |
|---|---|
| `tables.py check` | 34 tables, 125 fields, 0 errors |
| The image, built again from each of the seven discs and from `BOF3.exe` (`exe_tables.py build`, then `verify`) | 30 table files each (27 before); **every image as mapped is the same bytes as before** (the hash data.toml and the manifest record does not change); `verify`: 0 problems on each |
| The engine's table-files self-test | a table with a hole laid in two ranges, the hole left as the image has it; a range that starts before its table refused |
| `data_diff.py run` on the seven discs, then `check` | `recipes/data-diff.tsv` written again: both new tables compared on every build, 0 holes, 0 differences; 618 rows as before |
| `importer.py check`, `exe_own.py check`, `exe_scan.py --accepted`, `data_census.py`, `reloc_census.py`, `gen_symbols.py`, the ledger checks | pass |
| The mod rebuilt on the US disc-only install and on the PC-based one | **2,429 bytes of 10 tables as cells on both** (2,408 of 7 before): every table number the hack changes. The other 43 bytes, the padding, are raw ranges on the disc-only install and not carried on the PC-based one |
| The exe host's self-test, disc-only (started by the launcher, no `BOF3X_OPT`) and on the PC game folder | exit 0, no Fatal; `10 tables, 1227 cells, 2429 bytes ... 2429 of them differ` on both; the disc-only install's mapped `.data` hash with the mod is the one of 5c and 5d (the same bytes, more of them from table files) |

An install built before these commits has 27 table files and its mod layer
the 7-table form: both still load (nothing reads a table the cache does not
have); the three new tables come with the next build of the install. An
install built after them needs this engine: an older `bof3x.exe` refuses a
disc-built image whose `char_defaults` range is not its whole row.

**Seen in play (the owner, 2026-11-11, the disc-only play copy):** the hack's
text is there, and a save made with the original's numbers showed a character
whose EXP was past the mod's next two levels (the EXP bar drawn about three
times its width); the next battle gave the levels and the bar was right
again. That is the mod's `exp_table` cells read by the game. A save from
before a mod that changes the EXP table can show this once.

## 6a. The behaviours round's foundation, as run (2026-11-12)

On `romhack/behaviours` from `main` `c14b805c` (worktree
`D:\bof3x-runs\de1112\wt_f`, scratch `D:\bof3x-runs\de1112\f`). The
mechanism by which a mod turns on engine behaviours, and two fixes to the
builder. [`mods.md`](mods.md) 11 is the contract. **No game was started**: a
state-hash proof chain was running on the machine.

**What was built.**

- **The engine** (`src/hook/behaviour.h`, `behaviour.cpp`): `bof3::BehaviourOn(
  bof3::behaviours::Id::<Name>)`, `BOF3X_ON` read first in `InjectAll`, a
  `Fatal` on a name that is no behaviour and on an ask before the read, a
  log line a behaviour on, `BOF3X_OFF` winning; the parser's self-test on
  names of its own. `detour.cpp`: `BOF3X_OFF` takes a behaviour name.
- **The generated names**: `tools/ledger_switches.py` class `behaviour`, the
  `bof3::behaviours` block of `src/hook/code_names.h` (empty: no behaviour is
  built yet), a check that `BOF3X_ON` and `on=` are read; `tools/ledger_check.py`:
  every behaviour the `Switch:` of exactly one `Default: off` entry, `ini:on=`
  and `env:BOF3X_ON=` refused as `Switch:` names, and a self-test with a
  behaviour of its own in a copy of the inventory.
- **Mods** (`tools/mods.py`): `needs` in `[mod]` for `disc-patch` and
  `table-edit`, `skip` in `[disc-patch]`, both checked and kept in
  `mods.toml`; `behaviours` and `missing` in the JSON, the release's list
  read from the shipped `src/hook/code_names.h` (`build_reuse.SHIPPED_DATA`).
- **The launcher** (`config.cpp`, `sources_dialog.cpp`): `on=`; a mod that
  misses a behaviour left off and greyed with the names; `BOF3X_ON` from
  `on=` and the needs of the mods whose layers are handed over; `off=` wins,
  said on stderr; the Mods status line names the behaviours a mod turns on.
- **The zero-record rule** (`disc_patch_build.write_image_part`,
  `tables.py out_of_bounds`, `tables.toml` `range`): D415. The first bounds are
  `shop_records`' `level` 0..11 and `category` 0..4, cited to
  `shop_mode.cpp:112`, `shop_trade.cpp:127-129` and `:296-309`. `skip`:
  `parse_skip`.
- **Files outside the directory** (`disc_patch_build.owned_sectors`,
  `outside_files`, `outside_sections`; the census's `where = outside` rows; the
  report's `outside_files`, `outside_bytes`, `outside_sectors`). Not carried.
  The reading said the original holds zeros at those LBAs; measured, 3 of the
  67 sectors hold the original's bytes of another file there (the patched disc
  was rebuilt), so the test is "not the original's sector at that LBA", not
  "zero on the original".
- **Two defects of `main` found on the way and fixed:** `disc_patch_census.py`
  stopped with a `NameError` at its first changed EMI (85f9bda4 moved
  `emi_sections` to the builder and left no import); `tools/launcher_host`
  did not build since the OS seam (C++17, and no `bof3x_os` library).

**Checked.**

| What | Result |
|---|---|
| `disc_patch_build.py selftest` | 61 checks, 0 failures (43 before): carried, refused and skipped zero records, a stocked record carried whatever its value, a later copy of a repeated field, `skip`'s refusals, the report's new sentence, an outside EMI over other bytes of the original, a non-EMI sector, one past the original's end, a sector the original has the same not counted, one file's sections read back |
| `mods.py selftest` | 0 failures: 11 new zips (`needs` and `skip` accepted, and their refusals; `needs` on a table-edit mod accepted, on a reading-row mod refused), the header's behaviours block read, `missing` in order, `needs` and `skip` kept in `mods.toml` |
| `config_tests` (`tools/launcher_host`, clang++ 22) | 3,692 checks, 14 failures, all of `main`: the presets' reference rows lack DIV-0214 and DIV-0215 (`presets_*.tsv`). The new ones pass: needs read; a missing behaviour leaves the mod off; `BOF3X_ON` holds two mods' needs in order, once each; `on=` first; a mod off, under another language, or with a layer cut gives none; `off=` against a need, `*`, `-name`; `on=` read, written, sent to both hosts; an environment's `BOF3X_ON` wins |
| `ledger_check.py` (215 entries, 0 errors; the behaviours self-test's 5 cases), `ledger_switches.py --check`, `ledger_registry.py --check`, `tables.py check` (125 fields), `gen_symbols.py`, the self-tests of `type1.py`, `vcdiff.py`, `table_edit_build.py`, `loc_build.py`, `make_mod.py`; `importer.py check`, `exe_own.py check`, `exe_scan.py --accepted`, `data_diff.py check`, `data_census.py --check` and `--strict`, `reloc_census.py`, `reloc_bare.py`, `width_census.py`, `compile_shaders.py`, `gen_resample.py`, `tu_renames.py`, `tu_census.py --strict`, `retire_shells.py --strict`, `os_census.py --check` (after `analysis/os_census.tsv` was regenerated for the launcher's new sites) and `--boundary` | each exit 0 |
| The engine's parser self-test | **built, not run**: it needs a game start (`BOF3X_SELFTEST_ONLY`) |
| `cmake --build` for `i686` and `x86_64` | both build |
| The real patch, offline, on a copy of the US disc-only scratch install (`de1112\f\home`, 7 table files) | before: `shop_records` 33 cells, 149 bytes; after: 0, and the layer has no `shop_records.bin`. Every other table's cells and the `dat\` chunks the same files; `report.toml` `zero_record_bytes = 149`, `zero_records = 10`, `outside_files = 15`, `outside_bytes = 137216` (67 sectors), `outside_sectors = 0` |
| `disc_patch_census.py` on the real track | 15 outside EMIs, 67 sectors, LBAs `0x2DEE3`..`0x2DF25`: 14 at `0x80010000`, one at `0x80016000` |

**Not run with the game (owed when the machine is free):** the exe host's
self-test with `BOF3X_SELFTEST_ONLY=1` (the parser's cases run; `behaviours:
0 of 0 on`); a start with `BOF3X_ON=NoSuchName` (the `Fatal` sentence); a start
with `BOF3X_OFF` naming a behaviour (none exists yet, so this waits for the
first one); `dialog_test.py` with a mod that has `needs` (the greyed row and
the status line); the state-hash routes with the rebuilt mod layer off and on
(the layer lost only the `shop_records` cells, which no shop of the original
reads).

- Self-tests hold no game data: `vcdiff.py` makes the patch, the EMIs are
  synthetic.
- On the owner's machine, with the real patch: the build takes under five
  minutes; the layer's chunk list equals group M's; building twice gives the
  same bytes; removing the mod takes the layer and its rows out.
- With the mod installed and **off**: a recorded route's state hash is equal
  to the same route without the mod installed (`tools/statehash.py`).
- With it **on**: the game starts and loads a save on the US-built install;
  one enemy table and one text change the census names are read back from
  memory at their addresses (the readme does not say which enemy changed, so
  the census row is the reference, not model memory).
- The owner plays it. Which changes to look for is the owner's to say from
  the hack's readme.

## 6b. The behaviours, as built

**DoubleHitSwords (DIV-0216)**, on `romhack/b1-swords` (worktree
`D:\bof3x-runs\de1112\wt_b1`). The two hooks of BATTLE section 3 were read
from the patched listing itself, not only from the reading, and agree with
it: in `BattleAction_EnterKind` a member's kind-1 command with weapon 6, 9 or
0x18 becomes kind 4 with action id 0x77; in `BattleAction_AbilityCheck` the
cost of ability 0x77 for such a member is 0, stored where both cost paths
meet. The weapon byte the hack reads is the one Capcom's own battle code
reads (`PartyMember::weapon`). Ours: one test (`DoubleHitSword`) and two
guarded sites in `src/game/battle/battle_action.cpp`; off, both functions
are unchanged. The hack has no dragon-form or status test, so automatic,
confused, auto-battle and counter attacks by a member with one of the swords
convert too; mirrored, and written in the entry. `tools/ledger_check.py`'s
behaviours self-test now leaves the build's own behaviours out of its
inventory (with the first real one in, every fixture case read it as "no
ledger entry"). No game was started. Owed: the hosted `battle_actions`
shadow with the behaviour off (it must still pass); a fight under
`BOF3X_ON=DoubleHitSwords` with the mod's data and one of the three swords
(the attack runs as ability 0x77, costs no AP, and - unverified, the data's
or the animation's - hits twice); the same fight off, as before.

## 7. Owner calls (answered 2026-11-11, after group M)

| Call | The owner | So |
|---|---|---|
| 1 | "I agree, no reason to ship a partial hack" | (a): the kind is built, the Definitive Edition zip is not released until the hack is whole. Group D waits |
| 2 | (by its own rule, M3) | the author's `.xdelta` is not committed |
| 3 | "on off switches would be nice, yeah. maybe just a checkbox on the left side of the mod row" | a check box at the left of each mod's row on the Mods list, for every kind that is simply on or off (`disc-patch`, `table-edit`). Group C |
| 4 | "if the language edits are localization targeted ... the single localization layer is edited and the rest of the changes remain applicable to all" | **two layers a mod**: `mod-<id>` holds what no language owns (enemy tables, base images; later the table cells), and `mod-<id>-<tag>` holds the edits to that language's text. The first can be played over any text, the second only over its own (section 3a) |
| 5 | "Hold off on the sound bank"; frost's idea of a more true-to-PSX sequencer goes down as an idea for later | a changed bank is not carried and is counted in the report. IDEAS I64 |

The calls as they were put:

1. **Ship a partial hack?** After this round the mod carries the enemy and
   text edits, and none of the EXP tables, shop stock, battle tables or code.
   Balance that is half applied (stronger enemies after the volcano without
   the faster levels) may play worse than either game. Choices: (a) build the
   kind, keep the Definitive Edition zip unreleased until the image layer
   (step 5) at least; (b) release it marked with the report. I recommend (a):
   the mechanism is proven on his hack, and his name goes on something whole.
2. **Where the repackaged zip lives.** The furigana zip is built from
   `data/` in the repository. The xdelta is the author's file, and group M's
   measurement 3 says how much of it is Capcom's bytes the encoder carried as
   ADD data. If that is not zero, the file stays out of the repository
   (rule 1) and the release step fetches or is handed it; if it is zero, it
   can sit under `data/mods/` with his permission recorded in
   `THIRD_PARTY.md`. Either way the permission (who, when, the condition) is
   written down before the first release that carries it.
   **Measured 2026-11-11: not zero** (1,981 bytes of the game carried as
   they are, 1a row 3). So by this rule the author's file is not committed.
3. **The switch**: a check box on the Mods list (section 4), or something
   else. It now decides for `table-edit` mods too.
4. **Other languages.** The enemy tables are the same in every language; the
   script and text edits are English. Offer the neutral half over other text
   later, or keep the hack English-only as its author made it.

5. **Area 154's sound bank** (new, 1a row 4). Seven whole sectors of the
   bank's sample body differ on the patched disc and the readme names no
   sound change. Three samples decode shorter and the bank loses 161,448
   bytes. It may be meant, or a mark the author's disc tool left. Choices:
   (a) do not carry a changed bank, report it, and ask the author; (b) carry
   it as the disc has it. I recommend (a).

6. **The loose bytes on a PC install** (new, from group E). 64 bytes of this
   hack's table numbers lie outside the tables that are files. A disc-built
   image takes them as raw ranges. A PC-built image (DIV-0212) is class `exe`
   there, which the engine holds equal to `BOF3.exe`, so on a PC install they
   are not carried. Choices: (a) leave it, and catalogue those tables when
   they come up (each catalogued table becomes a file, and then carries on
   every install); (b) let a layer's raw range lie over a PC-built image
   (an engine change, DIV-0111 and DIV-0212 amended). I recommend (a) with
   `Char_DefaultRecords` and `Ability_EffectIndex` catalogued first: it keeps
   one rule for every install and makes the bytes readable.

   **The owner, 2026-11-11: "lets catalog those tables so we can fill them
   in"; then, for the one that could not be a file, "Let's have a table to
   fill the hole". As run (5e):** three tables catalogued, the third as a
   table file with holes; 43 of the 64 bytes are not table numbers at all.

Calls 1, 2 and 4 gate group D alone; call 3 gates group C; call 5 gates one
chunk; call 6 is answered (5e). Groups A, B and E needed none of them.

## 8. Not in this round

- (The image layer and the translated map, step 5's 240 data runs, were out
  of this round as staged. The image layer exists since PR 224, so they are
  group E.)
- Any engine behaviour (step 6; 84 functions). No DIV entry is opened here.
- Other patch formats (PPF, IPS, BPS) and other secondary compressors.
- `make_mod.py emi-patch`, the tool that makes a patch from an edited EMI
  (I58 item 4).

## 9. Ledger

A mod's changes are the modder's and are not DIV entries. The mechanism is:
DIV-0157 (mods) is amended for the new kind, the check box and the
`mods_off=` line (2026-11-11, with group C), and DIV-0213 for the same box
over a `table-edit` mod.

## 10. Where things are

| | |
|---|---|
| Branch | `disc-patch/round`, from `main` `8d2d987d` (worktree `C:\Users\kerok\bof3x-runs\dp1111\wt`). `mods/romhack-216` is merged |
| This round's scratch root | `D:\bof3x-runs\dp1111`: `cache_orig\` a psx-us disc-only cache with en-US built by `main`'s tools (table files in `base\exe`); `cache_hack2\` the same with the hybrid's language layer; `pc_orig\`, `pc_hack\` the PC's files with each language layer; the scripts `m3_add.py` (measurement 3), `loc_hybrid.py` (the hybrid disc, as a wrapper), `loc_diff.py`; the logs |
| The 2026-11-10 scratch root | `D:\bof3x-runs\romhack216`: `dl\` the author's archive unpacked; `out\de_track1.bin` the patched track; `out\census.tsv`; `cache_orig\` a psx-us disc-only cache with en-US (built 2026-11-10); `cache_hack\` the stopped experiment |
| The original disc | `D:\bof3x-runs\discs\Breath of Fire III (USA).cue` |
| The US function map | `D:\bof3x-runs\frost\map` (`--twins`) |
| Experiment wrappers | `D:\bof3x-runs\romhack216\tools\build_as_us2.py` (importer build from the patched disc, the hash gate waived and counted: romhack-mods.md 3) |
omhack216	oolsuild_as_us2.py` (importer build from the patched disc, the hash gate waived and counted: romhack-mods.md 3) |
