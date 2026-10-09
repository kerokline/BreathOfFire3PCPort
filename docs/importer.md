# The importer: recipes, identity, plan, cache, verify

**Status:** IN PROGRESS (2026-10-08, two cloud sessions: the recipe generator, the importer skeleton and the language layers by BCP 47 tag, proved on every catalogued build; the engine's tags done the same day; step 3's transforms and stand-ins in [`importer-transforms.md`](importer-transforms.md) - the per-disc counts and the class table below are superseded by its sections 1, 5 and 6; step 4's `opt/` layers, presets and `install` in [`opt-layers.md`](opt-layers.md), step 6's banks and `base/snd/` in [`sound-import.md`](sound-import.md), step 8's `base/exe/` in [`exe-import.md`](exe-import.md), step 9's `base/bgm/` in [`seq-import.md`](seq-import.md) - the manifest's `bgm` rows, `check` running `seq.py check`; the engine's reading of the cache is not done - `install` copies layers into `DAT/` meanwhile)

[`unified-data-plan.md`](unified-data-plan.md) step 2. `tools/importer.py` is
the importer's skeleton, and `recipes/pc-zh.toml` is its first recipe file,
generated. From any ordered list of the player's sources, the importer writes a
cache of `base/` and `loc/zh-CN/` that **reassembles the PC port's 742 `DAT/`
containers byte for byte**, with every chunk a disc carries taken from the disc.
It also writes `loc/en-US`, `loc/en-150`, `loc/fr-FR`, `loc/de-DE` and `loc/ja-JP` from the player's
discs, and each **equals the overlays `tools/loc_build.py` writes today**, 245
of 245 per language (section 5).

## 1. What a recipe is

`recipes/pc-zh.toml` has one `[[file]]` per container the PC ships, and one
line per chunk, in file order:

```
{ kind, tag, size, sha256, layer, class, [what], [emi = "PATH#section", on = ["build:how", ...]], [alt = [...]] }
```

- `sha256` is the payload's hash: the asset's identity and the check every
  import applies.
- `layer` is `base` or `loc/zh-CN`.
- `class` is why the chunk sits where it does (section 3).
- `emi` / `on` name **every held build that carries the chunk byte for byte**
  and how to get it out: `copy` (the section as it is), `type1` (decoded,
  [`type1-compression.md`](type1-compression.md)), or since step 3 `widen`,
  `icons`, `ryud` (a transform, [`importer-transforms.md`](importer-transforms.md)
  sections 2 and 4). A chunk may also carry `base` / `names` (an enemy table
  split between layers) and `own` / `own_why` (a disc's own section as a
  stand-in where no source carries the exact chunk, section 5 there).
- `alt` is for a build that carries the chunk in a different place. Of the
  2,171 chunks with a disc source, 12 have one: the PSP's `FIRST.EMI` has one
  more section than the PSX's, so its copies sit one index later.
- The PC install is every chunk's last source, implicitly: the container's
  own chunk at the same slot.

**Generated, never edited.** The generator hashes every section of every disc
it is given, a PSX type-1 section both raw and decoded, and looks up every PC
chunk by its hash. So every source in a recipe is a proven match, not an
inferred one: the plan's "the generator keeps it honest" (section 3 item 2),
made literal. A chunk no disc carries gets its class from `region_diff.py pc`,
the PC against JP, and the class decides its layer. The census
(`dat_census.py`) is used only as the tie-break, its own pairing preferred among
identical candidates. The recipe holds names, indices, sizes and hashes - never
bytes - so it is committed (rule 1; `LICENSING.md` section 3). The recipe
format is TOML, as the plan proposed.

To regenerate, from the owner's files (step 3 adds one `region_diff.py pair`
run per PSX disc; [`importer-transforms.md`](importer-transforms.md) section 9
has the full list):

```
python tools/dat_census.py DAT <the JP disc's EMIs, flat> --out analysis/dat_census.json
python tools/region_diff.py pc --census analysis/dat_census.json --dat DAT --disc JP --out analysis/region/psx-jp_vs_pc-zh.json
python tools/importer.py recipes --dat DAT --disc JP --disc US --disc EU --disc FR --disc DE --disc PSPJP --disc PSPEU
python tools/importer.py check
```

`check` needs no game data and runs in CI. It checks that the recipe's files
are the manifest's 742, that every container's chunks add up to the manifest's
size, that every source names a build `fixtures.toml` holds, and that every
layer is `base` or the target's own language layer.

## 2. The pipeline

```
identify ─▶ plan ─▶ copy / type1 ─▶ cache + manifest ─▶ verify
```

1. **Identity.** Each source is hashed file by file against `fixtures.toml`'s
   per-file manifests (`region_diff.manifest`, as `verify_fixtures.py --tree`
   does). Only a whole match counts. **An unknown build imports nothing**: a
   loose folder of extracted EMIs is refused, as is a modified disc.
2. **Plan.** For each chunk, the first source in the **player's** order that
   the recipe lists for it. The recipe's order of builds is not a preference.
3. **Transforms.** `copy` and `type1` today. Every result is hashed against
   the recipe before it is written; a mismatch stops the import.
4. **Cache.** `base/dat/NAME.DAT` and `loc/zh-CN/dat/NAME.DAT`, ordinary DAT
   containers, each holding its layer's chunks in file order. **A container is
   written whole or not at all.** `manifest.toml` records, for every chunk, its
   container, slot, layer, the source (`build:how:EMI#section`) and its hash.
   Each source is recorded by its file name only, never the player's path.
5. **Verify.** Interleave `base/` and `loc/zh-CN/` back into the PC's containers
   in the recipe's slot order, and hash each against
   `fixtures/pc-zh.DAT.files.tsv`. No PC install is needed: the hash list is
   the oracle. Since step 8 ([`exe-import.md`](exe-import.md)) `build` also
   writes `base/exe/` from the first source that can (the PC's exe, else a
   disc) with `[exe]` in the manifest, `verify` checks it against
   `recipes/exe.toml`'s per-build hash, and `check` runs `exe_tables.py check`.

## 3. What the chunks are (the recipe's classes)

| Layer | Class | Chunks | Source | What |
|---|---|---:|---|---|
| base | `disc` | 2,120 | a disc | byte-identical to a disc section |
| base | `type1` | 51 | a disc | a PSX type-1 section decoded, or the PSP's decompressed copy |
| base | `bank` | 901 | PC | audio banks: the disc's VAG samples as WAV. **Step 6: from any PSX disc by `wave-from-vag`**, 901 of 901 byte-identical from JP or US, 893 from a PAL disc ([`sound-import.md`](sound-import.md)); the PC's `SND/` likewise, `recipes/pc-zh.snd.toml` |
| base | `pc-edit` | 14 | PC | the port's edits to type-1 arenas ([`type1-compression.md`](type1-compression.md) 3) |
| base | `logic-data` | 4 | PC | the `RYUD00..03` byte (`region-diff.md` 9). **Step 3: `pc-byte`**, the disc's section with the byte set by rule (`ryud`) |
| base | `art` | 17 | PC | an image page the port redrew (`FIRST`), and the world map's dial page in 16 areas (below). **Step 3: `art` 16 + `pc-icons` 1** - `FIRST`'s page is the item-type icons respaced 24 to 32 px, rebuilt from any disc (`icons`); the dial page a stand-in from the disc's own page when the PC is absent |
| loc/zh-CN | `text` | 274 | PC | area message blocks (199), system pools (44), language images (glyph atlases, the 14 place-plate pages, the title page, the ending sheet), the text CLUT strip (DIV-0013) |
| loc/zh-CN | `layout+text` | 200 | PC | the enemy tables: Chinese names in fields widened 8 to 12, stats as JP's. **Split by step 3**: `enemy-table` 200 in `base/` (the PC layout, names blank, from any disc by `widen`), the names 12-byte chunks in each language layer ([`importer-transforms.md`](importer-transforms.md) section 2) |
| loc/zh-CN | `font` | 1 | PC | the port's Chinese font, the kind-3 chunk |
| | | **3,582** | | |

Each disc's share of the 2,171 with a disc source:

| psx-jp | psx-us | psx-eu-en | psx-fr | psx-de | psp-jp | psp-eu |
|---:|---:|---:|---:|---:|---:|---:|
| 2,171 | 2,146 | 2,147 | 2,127 | 2,125 | 1,951 | 1,929 |

**The dial page is port art, not text** (2026-10-08). `region_diff.py` calls
the PC's change to the world map's dial page (kind 1, `0x0A081000`) "words
painted on", by its destination. The facts:
- the change is one 14-tile block, byte-identical in all 16 world-map areas;
- rendered and read against the JP, US and FR pages, it holds no Chinese;
- it is the port's keyboard controls legend: SPACE, X and ENTER keys with
  pictograms, where the PlayStation draws ○ / × / START with words (JP
  Japanese, US `Enter` / `Guide` / `Camp`);
- the button-glyph sheet is removed;
- the gauge bar and the ENGINE / OVER HEAT frames are restyled, their words
  still English, as on the JP disc.

Every language on the PC wants it, so the generator classes it `art` in
`base/` (`importer.DIAL_PAGE`), not `loc/zh-CN`. That is why a language layer
need not and should not replace the page: a disc's page would bring the
PlayStation's buttons back. The one thing on it that is a language's is the
gauge words, which the French and German discs translate (MOTEUR /
SURCHAUFFE). Splicing those rows into the port's frames is a proposal on
`owner-review.md`, not done.

The other 14 "painted" pages are text, as classed: the place-name plates at
`0x0E001000`, Chinese over Japanese, rendered. Ten are DIV-0055's world maps.
In the other four (`AREA104`, `127`, `134`, `164`) the US disc keeps Japan's
plate page, and `loc_build.py`'s plate list does not include them. **That is
not a gap** (rendered JP / US / PC and measured, 2026-10-08):
- `127`, `134` and `164` are not world maps. `WorldMap_Records` holds eleven
  (16, 33, 45, 65, 87, 88, 104, 115, 121, 151, 152), and their sprite-frame
  sections are 30-37 KB of other data. Their pages carry a leftover copy of
  world 0's strip (`AREA016`'s names) that nothing draws.
- `104` is a world map whose whole strip is byte-identical to one in
  `AREA121`'s page, and its plate-start routine is in `121`'s code. Its
  plate-sizing section is unchanged on the US disc, where every localised
  map's grew or shrank with the English names. The owner's reading: its
  places are among those the game labels with the shared `?` plate, which
  needs no translation.
- The `?` plate itself is the one plate the port never repainted: the first
  84 texels of every strip (the `?!` icons and `?`) are JP's on the PC. The
  US redrew it only as part of restyling a whole strip, on the maps it
  localised.

**The 25 the US disc lacks** (measured against `region_diff.py pair` JP / US):
21 world-map place-name plates (`loc_build.py`'s `PLATE_DATA`), one painted
area page, `DEMO`'s language page, and the two `AREA004` minecart rows
(`region-diff.md` 4.1, the collision fix DIV-0080 applies by rule). The PC kept
the Japanese bytes in each, so only JP (and for most of them PSP-JP) carry
them. For step 3: from a Western disc, these come from that disc's `loc/`
layer and from DIV-0080, not from `base/`.

## 4. Measured (2026-10-08, every source verified against `fixtures.toml` first)

| Sources, in the player's order | From discs | Containers written | Verify |
|---|---|---|---|
| PC `DAT/` | 0 | base 741, loc/zh-CN 254 | **742 of 742** byte-identical |
| JP disc, PC | **2,171** from psx-jp | base 741, loc/zh-CN 254 | **742 of 742** |
| US disc, PSP-EU, PC | 2,146 psx-us + 1 psp-eu | base 741, loc/zh-CN 254 | **742 of 742** |
| EU-English disc, PC | 2,147 from psx-eu-en | base 741, loc/zh-CN 254 | **742 of 742** |
| US disc alone | 2,146 | 13 | 12 complete; the rest named as missing: banks 901, `loc/zh-CN` 491, PC edits 14, logic-data 4, art 1, and the 25 above |

The fourth row is the importer telling a disc-only player what it cannot build
yet, and why. **After step 3** ([`importer-transforms.md`](importer-transforms.md)
section 6): any PSX disc alone gives 2,392 of `base/`'s 3,307 chunks - 714
containers lack only their bank (step 6), 14 a bank and a port-edited arena;
the 25 are stand-ins from the disc's own sections. `AFLDKWA.DAT` is the one
container with no `base/` file: its only chunk is the system pool, which is
text.

## 5. The language layers

```
python tools/importer.py build --source JP --source US --source EU --source FR --source DE --source DAT --source BOF3.exe \
    --lang en-US --lang en-150 --lang fr-FR --lang de-DE --lang ja --out CACHE
python tools/importer.py verify --cache CACHE --overlays <an install's DAT/>
```

**Language tags.** A layer is named by its text's BCP 47 tag, `fixtures.toml`'s
`tag` per build:

| Build | Tag |
|---|---|
| pc-zh | `zh-CN` |
| psx-us | `en-US` |
| psx-eu-en | `en-150` |
| psx-fr | `fr-FR` |
| psx-de | `de-DE` |
| psx-jp | `ja-JP` |
| psp-jp | `ja-JP` |
| psp-eu | `en-150` |

The region subtag is the release's, never a dialect claim. Why the Chinese is
`zh-CN`:
- the exe's strings are GBK, the mainland encoding;
- they use Simplified forms (the game's name 龙战士Ⅲ, the error 错误, the
  master list's 师匠);
- the installer's language picker is LCID `0804`.

Why the European English is `en-150` ("English, Europe") and not `en-GB` (the
owner's decision, 2026-10-08): ten British / American spelling pairs counted
over both English discs' text give identical counts, all American. Its edits
are Sony Europe's terminology (`Memory Card`) and two renamed items. The
PSP-EU text has the European renames too (`Hourglass` in 55 places and no
`Quicksilver`, as the European PSX disc).

`--lang` takes a tag, or a bare language (`en`). **A bare `en` is `en-US`
when the US disc is given, in any source order, else the first English disc**
(the owner's decision, 2026-10-08: `en-US` is the default English;
[`importer-transforms.md`](importer-transforms.md) section 7); any other bare
language is the first PSX disc in the player's order with that primary subtag. `--lang T` writes
`loc/T/dat/NAME.DAT`. **It is `tools/loc_build.py all --lang T`, run
unchanged** on a scratch game directory: links to the PC's 742 shipped
containers and `BOF3.exe`, both identified sources, and none of the install's
own overlays, so nothing stale is read. Its `T.NAME.DAT` files are taken as
they are. They are the plan's `split-language`: what the player's disc says in
that language, as overlays the engine reads (DIV-0005). The importer
reimplements none of it. `loc_build.py --lang` itself accepts a tag now; what
it does by language goes by the primary subtag. Its default is the disc's own
tag (`loc_build.disc_tag`, by the importer's identity check; an unheld disc
wants `--lang`), since **the engine and the launcher read the tag** (2026-10-08:
`DAT\<tag>.*`, `Lang_FullWidth` by the primary subtag, `kLanguages` the five
tags; an install's old `en.*` files are not read and go). `verify --overlays`
still compares a layer with an install's bare-coded overlays when the install
has none under the full tag.
`BOF3.exe` is a source because `loc_build` reads the exe's name, verb and
config tables. Without it, `--lang` is refused with that reason.

**English goes first.** The French and German title menus borrow the CONFIG
row from an English `START.DAT` (`loc_build.build_title`). The US and EU-English
pages are byte-identical; two English pages that differed would be refused. Built alone, they leave the
title as shipped, which is 244 of 245. The importer orders the layers so this
cannot happen.

Measured 2026-10-08 against the owner's install, whose overlays were built on
2026-10-07 and 2026-09-25:

| Layer | From | Against the install's overlays |
|---|---|---|
| `loc/en-US` | psx-us | **245 of 245** byte-identical |
| `loc/fr-FR` | psx-fr | **245 of 245** |
| `loc/de-DE` | psx-de | **245 of 245** |
| `loc/ja-JP` | psx-jp | **245 of 245** |
| `loc/en-150` | psx-eu-en | 179 of 245 against the install's US-built `en.*`: **the EU-English release is not the US text** |

**`psx-eu-en` (held from 2026-10-08) differs from `psx-us`.** By
`region_diff.py pair` US / EU-English, besides relocated code:
- 44 system message pools and 22 area message blocks, all edited within
  English;
- `DEMO`'s language page, where this release ships the Japanese asset (the
  sibling's note); that is the one more base chunk it carries;
- 8 sound-bank pairs, the PAL sample swap (`region-diff.md` 8.4).

Its English layer differs from the US one in 66 containers: the edited pools
and blocks. The default English is `en-US` (the owner, 2026-10-08); `en-150`
by its full tag.

## 6. Not done in step 2

- ~~**Presets**~~ - done in step 4 (`--preset`, [`opt-layers.md`](opt-layers.md)
  section 7).
- **The engine reading the cache.** `LoadDatFile` reads `DAT/` as before. The
  cache is shaped so that `base/` plus a `loc/` layer can be read with the
  overlay mechanism unchanged; the second prefix is step 4.
