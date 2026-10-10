# The importer's transforms for a disc-only install

**Status:** IN PROGRESS (verified 2026-10-08, a cloud session: the transforms
and the stand-ins proved on every held build; a disc-only player still lacks the
banks, 14 arenas and every language layer - section 8)

[`unified-data-plan.md`](unified-data-plan.md) step 3, on top of step 2
([`importer.md`](importer.md)). Step 2's importer rebuilt the PC's 742 `DAT/`
containers from a PC install plus discs. From one Western PSX disc alone it
wrote 13 containers. This step makes every PSX disc a source of all of
`base/` except the audio banks and 14 arenas the port edited:

| | Before (step 2) | Now |
|---|---:|---:|
| `base/` chunks from the JP disc alone | 2,171 | **2,392** (2,376 exact + 16 stand-ins) |
| from the US disc alone | 2,146 | **2,392** (2,351 + 41) |
| from the EU-English disc alone | 2,147 | **2,392** (2,352 + 40) |
| from the French disc alone | 2,127 | **2,392** (2,332 + 60) |
| from the German disc alone | 2,125 | **2,392** (2,330 + 62) |
| from PSP-JP / PSP-EU alone | 1,951 / 1,929 | 2,151 / 2,129 (the enemy tables; the rest is step 4) |

`base/` has 3,307 chunks: 2,392 + 901 banks (step 6) + 14 port-edited arenas.
"Exact" is a chunk byte-identical to the PC's after a transform. A "stand-in"
is the disc's own section where the PC kept Japan's language or drew its own
art (section 5). Every number in this file was produced by a run in this
session against the files `fixtures.toml` identifies. The commands are in
section 9.

Everything lives in `tools/importer.py`'s transforms section and in the
regenerated `recipes/pc-zh.toml`. `loc_build.py` is unchanged.

## 1. What changed in the recipe

Each chunk line may now carry:

- **`on = ["build:how", ...]`** with `how` one of `copy`, `type1` (step 2), or
  the new `widen`, `icons`, `ryud` (sections 2 and 4). The generator indexes
  every disc section through each transform and looks up the PC chunk's hash,
  as before. So a recipe names a transform only where its output was hashed
  equal to the PC's chunk.
- **`base` / `names`** on the 200 enemy tables: the hash of the table with its
  names blank, and the layer the names go to (`loc/zh-CN`).
- **`own = ["build:EMI#section:sha256", ...]`, `own_why`**: the stand-ins, each
  with its own hash, and the reason (section 5).

The classes change to match. The 200 `layout+text` chunks become `enemy-table`
in `base`. The 4 `logic-data` become `pc-byte`. FIRST's `art` page becomes
`pc-icons`. The 16 dial pages stay `art`.

Regeneration needs one more input: `region_diff.py pair` for JP against each
PSX disc (section 9). It is what pairs a JP section with the Western disc's own
form of it.

`importer.py check` (CI, no game data) also checks three things:
- every `how` is a transform the importer has;
- every enemy table splits into `base` and the target's language;
- every stand-in is a PSX disc's, in `base`, with a known reason.

## 2. `widen-enemy-names`

**Measured.** Each disc's enemy table was widened (8-byte names to 12, stride
`0x88` to `0x8C`, every name blank) and compared with the PC's table with its
names blanked:

| psx-jp | psx-us | psx-eu-en | psx-fr | psx-de | psp-jp | psp-eu |
|---:|---:|---:|---:|---:|---:|---:|
| 200 of 200 | 200 | 200 | 200 | 200 | 200 | 200 |

So the stats, header and everything past the names are one set of bytes in all
seven builds, as `region-diff.md` said for the four it had.

**The split, decided.** `base/` holds the table in the PC's layout with every
name blank: one hash for every disc, so it is verifiable like any other chunk.
The names go in the language layer, one 12-byte kind-0 chunk per record at
`0xC2048 + k * 0x8C`. The reasons:
- the name bytes are glyph codes of a layer's own font, so a name has no
  language-neutral form. `loc_build.encode_text` maps English onto glyphs it
  appends to the font, and Japanese onto pair codes (DIV-0053, DIV-0057).
- that chunk form is exactly what `loc_build.py` already writes for `en`, `fr`,
  `de` and `ja`, so every non-Chinese layer needs no change.

For the Chinese layer, `loc/zh-CN/dat/AREAnnn.DAT` holds its text slots and then
the names of every record whose PC name field is not all zero. They are split
from the PC's chunk when the PC is a source. **The PC's names have one source,
the PC**, whichever source gave the stats.

`verify` lays those names back over the base table before hashing the
container. It also refuses a chunk past a layer's slots that is not an enemy
name. Planted corruption tests:
- a flipped byte in `AREA010`'s last Chinese name: `AREA010.DAT: composed, it
  is not the PC's`, exit code 1;
- a flipped byte in a text slot: caught by the manifest check as well.

**Check against `loc_build.py`.** For each of the 200 tables, each layer's name
chunks (built this session) were laid over two tables: the PC's table, which is
what the engine reads today with the PC's `DAT/` and an overlay, and the cache's
blank-named base. The two were compared.

| Layer | Tables equal | Records that differ |
|---|---:|---:|
| `en-US` | **200 of 200** | 0 |
| `en-150` | 200 of 200 | 0 |
| `ja-JP` | 200 of 200 | 0 |
| `fr-FR` | 167 | 40, in 33 tables |
| `de-DE` | 163 | 54, in 37 tables |

The French and German differences are exactly the records `loc_build.py` keeps
("enemy names: 408 (40 kept)", "394 (54 kept)"; the US: "448 (0 kept)"). A kept
name is one that does not encode in the 8 bytes the banner draws, because its
accented letters take two bytes each. On the PC with an overlay those enemies
keep the port's Chinese name. Over the cache's base they have no name. Neither
is right. Fixing it is `loc_build.py`'s job: an abbreviation or a wider draw.
That is the owner's call, below.

## 3. `remap-dest`: not needed

The plan expected a transform that rewrites the Western discs' shifted
destinations. **It gains nothing, measured.** The recipe matches a chunk by
its payload's hash, never by the destination. The cache writes the chunk under
the PC's tag, taken from the recipe. So a section that differs only in where it
loads already counts as a source.

Recipe sources whose disc section loads at a different destination than JP's
(the recipe joined with `region_diff.py pair`):

| Build | Sources | at JP + `0x8000` (kind 0) | at JP's (kind 0 / kind 1) |
|---|---:|---:|---:|
| psx-us | 2,146 | **934** | 668 / 544 |
| psx-eu-en | 2,147 | 934 | 668 / 545 |
| psx-fr | 2,127 | 930 | 668 / 529 |
| psx-de | 2,125 | 918 | 667 / 528 |

The PSP's are the same sections with the top bit cleared: 1,498 of PSP-JP's,
and 820 of PSP-EU's at `+0x8000` as well.

None of the chunks a Western disc lacks is a relocation. Each is a content
difference, listed in section 5. The other two bands the plan names are not
`DAT/` chunks:
- the pool's `+0x6000` is the system pool, which is text: the language layer,
  with its relocation DIV-0007;
- the boot band's `+0x3000` is the executable: step 8's `base/exe/`.

The tag-not-destination rule is also what places a stand-in. A Western
palette at `0x80035800` lands at the PC's tag `0xA000`, where the PC's code
(compiled from JP's) reads it.

## 4. Two port changes as rules

- **`respace-icons`**. `FIRST`'s page `0x080F0201` holds the seven item-type
  icons (weapon, staff, pot, eye, shield, crossed swords, boot). They are 24 x
  24 at 4 bpp, on a 24-px pitch on the PSX. The PC moved them to a 32-px pitch
  and changed no pixel: rendered, then rebuilt with **0 pixels different**; the
  page differs only in rows 0..23. The rule moves six-halfword columns from
  `6i` to `8i`. It is coordinates only and yields the PC's chunk from all five
  PSX discs (their pages are JP's).
- **`ryud`**. `RYUD00..03`'s section 3 holds `0xF7` at `+0x7ACE` on every PSX
  disc and `0xFF` on the PC (`region-diff.md` 9, still unexplained). The rule
  sets the byte only where the disc holds `0xF7`, and yields the PC's chunk from
  all five PSX discs. `base/` is the PC's bytes whatever they mean, so the rule
  carries them.

The PSP's copies of both differ (`region-diff.md` 5); they are step 4's.

## 5. The stand-ins: what a Western disc carries in its own form

`importer.md` section 3 listed 25 chunks the US disc lacks. The PC kept the
Japanese bytes in each. The generator now records, per PSX disc, that disc's
own section at the JP section's place, as `region_diff.py pair` aligns them,
with its hash. The build uses a stand-in **only when no source the player gave
carries the chunk itself**: every exact source first, the PC included, then the
stand-ins, each in the player's order. So with the PC or the JP disc among the
sources nothing changes. Each stand-in is hashed against the recipe before it
is written, and `verify` checks it against the manifest.

Four reasons are allowed (`OWN_WHY`); nothing else stands in:

| Why | US | EU-en | FR | DE | JP | What |
|---|---:|---:|---:|---:|---:|---|
| `language` | 23 | 22 | 38 | 40 | - | world-map plate data, painted area pages, `DEMO`'s language page, glyph atlases: what `region_diff.py pair` classes text |
| `page-clut` | - | - | 4 | 4 | - | the palette a stand-in area page is drawn with (French and German only), allowed only beside that page |
| `div-0080` | 2 | 2 | 2 | 2 | - | `AREA004`'s map band and placement map |
| `port-art` | 16 | 16 | 16 | 16 | 16 | the world map's dial page |

**The plates (21 of the US's 25; 22 on the German disc).** These are the plate
data in `loc_build.py`'s ten `PLATE_AREAS`: the frame sections `0xB0000` and
`0xC0000` and the palette `0xA000`. Each disc's stand-in is **byte for byte the
chunk its own language layer lays at the same tag**. Compared this session,
with no difference:
- US 21 of 21 against `loc/en-US`;
- EU 21 of 21 against `loc/en-150`;
- French 21 of 21 against `loc/fr-FR`;
- German 22 of 22 against `loc/de-DE`.

`dat_load.cpp`'s `LoadDatFile` walks the base file, then the overlay, and lays
each overlay chunk over the arena or VRAM whatever the base held. A base file
that does not exist is skipped (`File_Open` gives -1) and the overlay still
walks. So under the layer, **the base's plate chunks are not read at run time**.
They are needed only because a container is written whole or not at all, and
a base without them would also show nothing if played under no layer. A
stand-in costs nothing and keeps `base/` complete. The alternative, moving the
plate data to `loc/`, was weighed. It would make the Chinese layer carry JP's
plate data, and change step 2's layer split for no runtime gain.

**`AREA128`'s painted page and `DEMO`'s language page (US).** No language
layer carries them (`loc_build.py` builds neither). On the PC, and on the PC
with the English overlay today, they show Japan's painted words. From a US disc
alone, `base/` shows the US's English: 2 of `AREA128`'s 128 tiles. The French
and German discs add 12 glyph atlases in `MAGIC*` files, 3 more painted pages
with their palettes, and (German) `SCENA17`'s copy of the `DEMO` page. These
are the disc's own language where the PC kept Japan's. It is a note, not a
ledger entry, until the engine reads the cache (for the other files, below).

**`AREA004`'s two rows (DIV-0080).** *Revised 2026-10-10: the walls are no
longer written by our code from a coordinate table; they are the
`area4-walls` optional layer, cut from a Western disc ([`opt-layers.md`](opt-layers.md)
section 1, [`region-diff.md`](region-diff.md) 10).*
- From the JP disc, or the PC: JP's bytes, the open map. With a Western disc
  among the sources as well, the build makes the layer by default (the
  Western disc's whole cell plane and placement map over JP's; `--no-opt
  area4-walls` leaves it out), `install` installs it by default, and the
  launcher plays it whenever it is installed unless the ini says `opt=none`
  (2026-10-10). Without one, no walls.
- From a Western disc alone: that disc's bytes in `base/`, which already
  have the 72 walls and the 8 cleared placement cells; no layer is needed.

The difference is what the layer does not take: the Western map band's
**30-cell re-texture** (920 bytes) comes with a Western-only `base/`. Two
consequences for a Western-only cache:
- the open map cannot be had from it, because no JP bytes are held;
- the strip's textures are the later discs'.

That is a difference in game behaviour by source, and DIV-0080 records it.

**The dial page (port art, 16 world-map areas).** No disc carries the port's
keyboard legend. Every PSX disc, JP included, stands in its own page: the
PlayStation's buttons and that disc's words. Rendered `AREA016`'s from the PC,
JP and US: the legend panel, the gauge bar and the ENGINE / OVER HEAT frames
sit in the same cells on all three. The US page differs from JP's in 5 of its
128 tiles (its words). So the PC's code, drawing from the PC's coordinates,
draws the PlayStation's legend in the port's place. A disc-only player sees
○ / × / START where the PC shows SPACE / X / ENTER. Not seen live.
Whether to accept that or draw a replacement layer is the owner's call. It is a
note, not a divergence: no rule changes the game. The port's art is simply
absent with the port.

## 6. Measured

Every build and verify below ran this session on the files under
`/workspace/scratch/game/`.

| Sources, in the player's order | Exact from discs | Stand-ins | Containers written | Verify |
|---|---:|---:|---|---|
| PC `DAT/` | 0 | 0 | base 741, loc/zh-CN 254 | **742 of 742** byte-identical |
| JP disc, PC | 2,376 psx-jp | 0 | base 741, loc/zh-CN 254 | **742 of 742** |
| US disc, PC | 2,351 psx-us | 0 | base 741, loc/zh-CN 254 | **742 of 742** |
| EU-English disc, PC | 2,352 | 0 | 741 / 254 | 742 of 742 |
| French disc, PC | 2,332 | 0 | 741 / 254 | 742 of 742 |
| German disc, PC | 2,330 | 0 | 741 / 254 | 742 of 742 |
| JP, US, EU, FR, DE, PC, `BOF3.exe`, all five `--lang` | 2,376 psx-jp | 0 | 741 / 254 + 5 x 245 | 742 of 742; the five layers **byte-identical to those built at `24bd7cb`** from the same discs, 245 of 245 each |
| US disc alone | 2,351 | 41 | base 13 | 0 problems; 12 composed equal to the PC's |
| EU-English disc alone | 2,352 | 40 | base 13 | 0 problems; 12 |
| French disc alone | 2,332 | 60 | base 13 | 0 problems; 11, and 1 holding its own sections |
| German disc alone | 2,330 | 62 | base 13 | 0 problems; 10, and 2 holding their own |
| JP disc alone | 2,376 | 16 | base 13 | 0 problems; 12 |
| PSP-JP alone | 2,151 | 0 | base 11 | the PSX-only transforms and 206 differing sections missing: step 4 |

What each disc alone still lacks is the same for all five PSX discs:

| Layer | Class | Chunks | Why (the build's own report) |
|---|---|---:|---|
| base | `bank` | 901 | the disc's VAG samples as WAV: step 6 |
| base | `pc-edit` | 14 | the port's edits to type-1 arenas, unread: the PC install only (section 8) |
| loc/zh-CN | `text`, `font`, `enemy-names` | 274 + 1 + 200 | the Chinese layer: the PC install only, not needed unless `zh-CN` is played |

By container, the `base/` files not written lack:
- **714 only their bank**: every one complete once step 6 lands;
- 14 a bank and a port-edited arena.

## 7. `en-US` is the default English

The owner's decision of 2026-10-08. `importer.py`'s `DEFAULT_TAG = {"en":
"en-US"}`:
- a bare `--lang en` means the US disc when it is among the sources, whatever
  the order, and otherwise the first English disc in the player's order (the
  EU-English disc alone gives `en-150`);
- a full tag still means exactly that tag.

Checked through `resolve_languages`:

| Sources | `--lang en` gives |
|---|---|
| EU, US | `en-US` |
| US, EU | `en-US` |
| EU only | `en-150` |
| US only | `en-US` |

`verify --overlays` compares a layer with an install's bare-coded overlays
(`en.*`) only for the language's default tag. The install's `en.*` are en-US's,
so `loc/en-150` is reported "not compared" instead of failing 179 of 245.
`loc_build.py`'s own default, the bare `en`, is unchanged. It names the files
the engine reads today, and for English the US disc is what has always been
given to it.

## 8. What a disc-only player still cannot have, and why

1. **The banks (901 chunks), step 6.** Every area, battle and form container
   holds one. They are the only thing between 714 `base/` containers and
   complete.
2. **The 14 port-edited arenas.** The edits are listed in
   [`type1-compression.md`](type1-compression.md) 3:
   - member 2's block in eight `PL` arenas, 352 bytes shorter, with 480 changed
     windows;
   - `START` given `PL012`'s PC arena;
   - 1,949 words in three `BPLD01x`;
   - two words in the `27A` pair.

   They are game bytes, so no recipe can carry them (rule 1). They are not a
   rule either until someone reads what they fix in the PC's executable, which
   consumes the arenas. The disc's decoded arena would load, and its block
   offsets are self-describing. Whether the PC's code wants the edited form is
   unknown, so the importer stops: "the PC install only". This is the reading
   `type1-compression.md` 3 already names.
3. **Every language layer.** This is the larger gap.
   - `loc_build.py` builds `loc/<tag>/` against the PC's own containers and
     `BOF3.exe`: the font it appends Latin glyphs to is the port's Chinese
     table (the kind-3 chunk, `loc/zh-CN`), the text blocks are converted
     against the PC's blocks, and the name, verb and config tables are read out
     of the exe. `importer.py build --lang` refuses without the PC's `DAT/` and
     `BOF3.exe`.
   - So **a disc-only cache has no font and no text**, though its `base/`
     becomes complete with step 6.
   - Closing that is its own piece of work: a font built from the disc's glyph
     sheets alone, `loc_build.py` reading `base/` and the disc instead of
     `DAT/`, and step 8's `base/exe/`. It is not a transform of this step.
4. **The Chinese layer**, which a disc-only player does not need.

## For the other files

- **`docs/importer.md`**:
  - section 1: `how` gains `widen`, `icons`, `ryud`. The recipe gains `base` /
    `names` and `own` / `own_why`. The regeneration commands gain one
    `region_diff.py pair` run per PSX disc (section 9 here). `check`'s list
    grows as in section 1 here.
  - section 2 item 3: transforms per this doc; stand-ins after every exact
    source.
  - section 3's class table:
    - `layout+text` 200 becomes `enemy-table` 200 in `base` (the names in
      `loc/zh-CN`);
    - `logic-data` 4 becomes `pc-byte` 4, with a disc source;
    - `art` 17 becomes `art` 16 + `pc-icons` 1, with a disc source.

    The per-disc table becomes 2,376 / 2,351 / 2,352 / 2,332 / 2,330 / 2,151 /
    2,129 exact, plus stand-ins 16 / 41 / 40 / 60 / 62 / 0 / 0. "The 25 the US
    disc lacks" closes: they are stand-ins, section 5 here.
  - section 4's last row: base 13 written, 714 lacking only their bank, 14 a
    bank and a port-edited arena.
  - section 5: "Which English is the player's call; the order of the sources
    says it" is replaced by section 7 here.
  - the manifest's assets now sit under `[cache]`. They had been parsed by
    TOML as part of the last `[[source]]`; nothing read them until `verify`
    did.
- **`docs/unified-data-plan.md`**:
  - section 3 item 3: `remap-dest` is not needed (section 3 here).
    `widen-enemy-names` is done, split as section 2. Add the two rules and the
    stand-ins.
  - section 8 row 3: done for the PSX discs, as measured; the FR / DE / JP text
    encodings were already `loc_build.py`'s.
  - section 10: the RYUD byte is carried by rule, still unexplained.
- **`docs/STATUS.md`**: unified-data step 3 landed. A PSX disc alone gives all
  of `base/` but the banks and 14 arenas. A disc-only install still has no
  language layer (section 8 item 3).
- **`docs/HANDOFF.md`**: the next disc-only items are, in order:
  - step 6 (the banks);
  - the reading of the 14 arena edits;
  - a disc-only `loc_build` (font, text, exe tables; with step 8).
- **`docs/DIVERGENCE.md`** (not in this step's lane; proposed text):
  - DIV-0080, a line: from a Western disc alone the cache holds that disc's
    `AREA004` rows. The walls are already there, and the 30-cell re-texture
    comes with them; the open map cannot be had from such a cache. (Written
    into DIV-0080; since 2026-10-10 the walls on JP's map are the
    `area4-walls` layer, not code.)
  - When the engine reads the cache, one entry for the stand-ins as a whole:
    the dial page's PlayStation legend, the Western pages' own words.
- **`docs/owner-review.md`**: the dial page stand-in, to look at once the
  engine reads the cache. Also the French and German enemies with no name over
  the cache's base (section 2).
- **`fixtures.toml`**: nothing.

### The owner's calls

1. **The dial page for a disc-only player.** Accept the disc's own page
   (PlayStation buttons in the port's place, as built now), or draw a
   keyboard-legend layer that does not depend on the PC's art.
2. **The 40 French and 54 German enemy names that do not fit 8 bytes.** On the
   PC with an overlay they stay Chinese today; over the cache they are blank.
   Abbreviate them in `loc_build.py`, or widen the draw (DIV-0053 draws 8).
3. **The 14 port-edited arenas.** Read what the edits fix before a disc-only
   install can have them (`type1-compression.md` 3), or accept the disc's
   decode with a ledger entry.
4. **`AREA004` from a Western disc**: the re-texture comes with it. Fine as a
   by-source difference, or should the importer refuse a Western-only
   `AREA004`?

## 9. Commands

```
python tools/dat_census.py DAT <the JP disc's EMIs, flat> --out analysis/dat_census.json
python tools/region_diff.py pc --census analysis/dat_census.json --dat DAT --disc JP --out analysis/region/psx-jp_vs_pc-zh.json
python tools/region_diff.py pair psx-jp=JP psx-us=US     --out analysis/region/psx-jp_vs_psx-us.json      # and psx-eu-en, psx-fr, psx-de
python tools/importer.py recipes --dat DAT --disc JP --disc US --disc EU --disc FR --disc DE --disc PSPJP --disc PSPEU
python tools/importer.py check
python tools/importer.py build --source US --out CACHE        # a disc alone: the report of section 6
python tools/importer.py verify --cache CACHE
```

The regenerated recipe was generated twice from the same inputs, and the two
runs are identical.
