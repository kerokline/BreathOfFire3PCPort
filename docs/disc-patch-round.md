# The disc-patch round: a romhack's kept-section changes as a mod

**Status:** STAGED 2026-11-10 (issue 216; nothing of the round built). Step 4
of [`romhack-mods.md`](romhack-mods.md) section 6, staged for another session.
Read that note first: it holds the measurements this plan stands on. Branch
`mods/romhack-216` holds the three things the round starts from:
`tools/vcdiff.py`, `tools/disc_patch_census.py`, and `author` / `url` /
`credit` in `mod.toml` and on the Mods tab.

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
does. This round carries only what the PC holds as DAT chunks (662 of this
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

Not measured, and first in the round (group M):

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
   mod's state is `needs`, with the disc named, as a language is today.
2. `vcdiff.apply_file` into a temporary folder under the home (443 MiB; say so
   before, remove after, also on failure). Check `result_sha256`.
3. **Base chunks.** For every recipe row the build is a source of, get the
   payload from the original and from the patched disc through the same
   transform (`importer.DiscSource.get`, `TRANSFORMS`); where they differ, the
   patched payload is a chunk of the layer. The experiment's wrapper
   (`build_as_us2.py`, romhack-mods.md 3) is this loop with a counter.
4. **Language chunks.** Run `loc_build.py` on the patched disc the way the
   install's language layer was built (disc-only, or against the PC's files),
   into a scratch folder; every chunk that differs from the install's
   `loc\<tag>\` chunk goes into the layer. The original's side is already in
   the cache, so this is one run of about 80 seconds.
5. Write `opt\mod-<id>\dat\`, list every chunk in the manifest as
   `mod:<id>@<version>:<build>` (mods.md 4's rows).
6. **The report.** Run the census's comparison (`disc_patch_census.py` as a
   module) and write `<home>\mods\<id>\report.toml`: edit runs carried, by
   kind; edit runs not carried, by class (relinked addresses: none needed;
   overlay data: needs the image layer; code: needs engine behaviours). The
   Mods tab shows the two totals for the selected mod. This is the round's
   honesty rule: the player sees "carries the enemy and text changes; 429 of
   the hack's changes need parts this release does not have", not a mod that
   looks whole.

A patched section that passes no transform (a kind-1 image that does not
decode, a text block `loc_build.py` refuses) stops the build with the file,
the section and the builder's sentence. No partial layer (rule 4).

## 4. The launcher

- **On and off.** A built `disc-patch` mod needs a switch. The reading-row
  mod has the *Row above the text* box; this kind has nothing. Proposed: a
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

A before B; C beside B; M before all of them; D last.

## 6. Acceptance

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

## 7. Owner calls

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
3. **The switch**: a check box on the Mods list (section 4), or something else.
4. **Other languages.** The enemy tables are the same in every language; the
   script and text edits are English. Offer the neutral half over other text
   later, or keep the hack English-only as its author made it.

## 8. Not in this round

- The image layer and the translated map (step 5; 240 data runs).
- Any engine behaviour (step 6; 84 functions). No DIV entry is opened here.
- Other patch formats (PPF, IPS, BPS) and other secondary compressors.
- `make_mod.py emi-patch`, the tool that makes a patch from an edited EMI
  (I58 item 4).

## 9. Ledger

A mod's changes are the modder's and are not DIV entries. The mechanism is:
DIV-0157 (mods) is amended for the new kind and the `mods=` line when group A
and C land. `docs/DIVERGENCE.md` is read before either.

## 10. Where things are

| | |
|---|---|
| Branch | `mods/romhack-216` (worktree `C:\Users\kerok\bof3x-runs\rh216\wt`) |
| Scratch root | `D:\bof3x-runs\romhack216`: `dl\` the author's archive unpacked; `out\de_track1.bin` the patched track; `out\census.tsv`; `cache_orig\` a psx-us disc-only cache with en-US (built 2026-11-10); `cache_hack\` the stopped experiment |
| The original disc | `D:\bof3x-runs\discs\Breath of Fire III (USA).cue` |
| The US function map | `D:\bof3x-runs\frost\map` (`--twins`) |
| Experiment wrappers | `D:of3x-runsomhack216	oolsuild_as_us2.py` (importer build from the patched disc, the hash gate waived and counted: romhack-mods.md 3) |
