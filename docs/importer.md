# The importer: recipes, identity, plan, cache, verify

**Status:** IN PROGRESS (2026-10-08, a cloud session: the recipe generator, the importer skeleton and the language layers, proved on every catalogued build; presets and the engine's reading of the cache are not done)

[`unified-data-plan.md`](unified-data-plan.md) step 2. `tools/importer.py` is
the importer's skeleton, and `recipes/pc-zh.toml` is its first recipe file,
generated. From any ordered list of the player's sources, the importer writes a
cache of `base/` and `loc/zh/` that **reassembles the PC port's 742 `DAT/`
containers byte for byte**, with every chunk a disc carries taken from the disc.
It also writes `loc/en`, `loc/fr`, `loc/de` and `loc/ja` from the player's
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
- `layer` is `base` or `loc/zh`.
- `class` is why the chunk sits where it does (section 3).
- `emi` / `on` name **every held build that carries the chunk byte for byte**
  and how to get it out: `copy` (the section as it is) or `type1` (decoded,
  [`type1-compression.md`](type1-compression.md)).
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

To regenerate, from the owner's files:

```
python tools/dat_census.py DAT <the JP disc's EMIs, flat> --out analysis/dat_census.json
python tools/region_diff.py pc --census analysis/dat_census.json --dat DAT --disc JP --out analysis/region/psx-jp_vs_pc-zh.json
python tools/importer.py recipes --dat DAT --disc JP --disc US --disc EU --disc FR --disc DE --disc PSPJP --disc PSPEU
python tools/importer.py check
```

`check` needs no game data and runs in CI. It checks that the recipe's files
are the manifest's 742, that every container's chunks add up to the manifest's
size, that every source names a build `fixtures.toml` holds, and that every
layer is `base` or `loc/<lang>`.

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
4. **Cache.** `base/dat/NAME.DAT` and `loc/zh/dat/NAME.DAT`, ordinary DAT
   containers, each holding its layer's chunks in file order. **A container is
   written whole or not at all.** `manifest.toml` records, for every chunk, its
   container, slot, layer, the source (`build:how:EMI#section`) and its hash.
   Each source is recorded by its file name only, never the player's path.
5. **Verify.** Interleave `base/` and `loc/zh/` back into the PC's containers
   in the recipe's slot order, and hash each against
   `fixtures/pc-zh.DAT.files.tsv`. No PC install is needed: the hash list is
   the oracle.

## 3. What the chunks are (the recipe's classes)

| Layer | Class | Chunks | Source | What |
|---|---|---:|---|---|
| base | `disc` | 2,120 | a disc | byte-identical to a disc section |
| base | `type1` | 51 | a disc | a PSX type-1 section decoded, or the PSP's decompressed copy |
| base | `bank` | 901 | PC | audio banks: the disc's VAG samples as WAV, step 6 |
| base | `pc-edit` | 14 | PC | the port's edits to type-1 arenas ([`type1-compression.md`](type1-compression.md) 3) |
| base | `logic-data` | 4 | PC | the `RYUD00..03` byte (`region-diff.md` 9) |
| base | `art` | 1 | PC | an image page the port redrew |
| loc/zh | `text` | 290 | PC | area message blocks (199), system pools (44), language images (glyph atlases, painted area pages, the title page, the ending sheet), the text CLUT strip (DIV-0013) |
| loc/zh | `layout+text` | 200 | PC | the enemy tables: Chinese names in fields widened 8 to 12, stats as JP's; step 3's `widen-enemy-names` will split them |
| loc/zh | `font` | 1 | PC | the port's Chinese font, the kind-3 chunk |
| | | **3,582** | | |

Each disc's share of the 2,171 with a disc source:

| psx-jp | psx-us | psx-eu-en | psx-fr | psx-de | psp-jp | psp-eu |
|---:|---:|---:|---:|---:|---:|---:|
| 2,171 | 2,146 | 2,147 | 2,127 | 2,125 | 1,951 | 1,929 |

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
| PC `DAT/` | 0 | base 741, loc/zh 254 | **742 of 742** byte-identical |
| JP disc, PC | **2,171** from psx-jp | base 741, loc/zh 254 | **742 of 742** |
| US disc, PSP-EU, PC | 2,146 psx-us + 1 psp-eu | base 741, loc/zh 254 | **742 of 742** |
| EU-English disc, PC | 2,147 from psx-eu-en | base 741, loc/zh 254 | **742 of 742** |
| US disc alone | 2,146 | 13 | 12 complete; the rest named as missing: banks 901, `loc/zh` 491, PC edits 14, logic-data 4, art 1, and the 25 above |

The fourth row is the importer telling a disc-only player what it cannot build
yet, and why. That list is steps 3 and 6's work. `AFLDKWA.DAT` is the one
container with no `base/` file: its only chunk is the system pool, which is
text.

## 5. The language layers

```
python tools/importer.py build --source JP --source US --source FR --source DE --source DAT --source BOF3.exe \
    --lang en --lang fr --lang de --lang ja --out CACHE
python tools/importer.py verify --cache CACHE --overlays <an install's DAT/>
```

`--lang L` writes `loc/L/dat/NAME.DAT` from the first disc in the player's
order that can give L. English comes from `psx-us` or `psx-eu-en`, French from
`psx-fr`, German from `psx-de`, Japanese from `psx-jp`. **It is
`tools/loc_build.py all`, run unchanged** on a scratch game directory: links to
the PC's 742 shipped containers and `BOF3.exe`, both identified sources, and
none of the install's own overlays, so nothing stale is read. Its
`<lang>.NAME.DAT` files are taken as they are. They are the plan's
`split-language`: what the player's disc says in that language, as overlays the
engine already reads (DIV-0005). The importer reimplements none of it.
`BOF3.exe` is a source because `loc_build` reads the exe's name, verb and
config tables. Without it, `--lang` is refused with that reason.

**English goes first.** The French and German title menus borrow the CONFIG
row from `en.START.DAT` (`loc_build.build_title`). Built alone, they leave the
title as shipped, which is 244 of 245. The importer orders the layers so this
cannot happen.

Measured 2026-10-08 against the owner's install, whose overlays were built on
2026-10-07 and 2026-09-25:

| Layer | From | Against the install's overlays |
|---|---|---|
| `loc/en` | psx-us | **245 of 245** byte-identical |
| `loc/fr` | psx-fr | **245 of 245** |
| `loc/de` | psx-de | **245 of 245** |
| `loc/ja` | psx-jp | **245 of 245** |
| `loc/en` | psx-eu-en | 179 of 245: **the EU-English release is not the US text** |

**`psx-eu-en` (held from 2026-10-08) differs from `psx-us`.** By
`region_diff.py pair` US / EU-English, besides relocated code:
- 44 system message pools and 22 area message blocks, all edited within
  English;
- `DEMO`'s language page, where this release ships the Japanese asset (the
  sibling's note); that is the one more base chunk it carries;
- 8 sound-bank pairs, the PAL sample swap (`region-diff.md` 8.4).

Its English layer differs from the US one in 66 containers: the edited pools
and blocks. Which English is "the" English is the owner's call. The importer
uses whichever English disc comes first in the player's order.

## 6. Not done in step 2

- **Presets** ("PC install", "US disc only", "PC + US text"). These are only
  source orders today, given on the command line.
- **The engine reading the cache.** `LoadDatFile` reads `DAT/` as before. The
  cache is shaped so that `base/` plus a `loc/` layer can be read with the
  overlay mechanism unchanged; the second prefix is step 4.
