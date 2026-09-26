# Asset sources: building the game from whatever copies the player owns

**Status:** DRAFT (2026-09-26; a plan, not a finding. The facts it rests on are
cited and were verified where they are cited, not re-measured here)

The delivery goal: someone builds this project on their own machine, points it
at the copies of *Breath of Fire III* they own — the 2001 PC port, one or more
PlayStation discs, or a mix — and gets a playable game. Once the code is ours,
**one PSX disc on its own** should be enough.

This is the long-range shape of [`IDEAS.md`](IDEAS.md) I2 (selectable
localisations), which today is the special case "English text and font from the
player's US disc". It is phase-5 work ([`PLAN.md`](PLAN.md) §5). It is written
now because two things on this page get more expensive the longer they wait:
the rule in §5, and naming assets as their loaders are taken over (§8).

---

## 1. Two layers, not one mapping

"A catalogue of game mapping files to disc hashes" is right, but it is two
separate things:

| Layer | Answers | Keyed by | Exists today as |
|---|---|---|---|
| **Identity** | *What has the player given us?* | per-file hashes → a build id | [`fixtures.toml`](../fixtures.toml), `tools/verify_fixtures.py` |
| **Recipes** | *How does logical asset X come out of build Y?* | (asset id, build id) | the pairing in `tools/dat_census.py`, and `tools/loc_build.py` for one language |

### Identity

`fixtures.toml` already names seven builds and states why a receipt that does
not name its build is not evidence. For player-facing use, two changes:

- **Hash files, not images.** One disc ripped as `.bin/.cue`, `.iso` or `.chd`
  gives three image hashes. The ISO9660 files inside hash the same whichever
  way it was ripped. The boot EXE (`SLPS_009.90`, `SLUS_004.22`, …) names the
  build. The other files confirm the dump. `fixtures.toml` has image hashes
  for the JP disc and a boot-EXE hash; it needs a per-file table for each disc
  it catalogues.
- **An unknown build is named as unknown**, never guessed at. The rule is
  already in `fixtures.toml`'s header. The importer (§3) extends it: it says
  which files matched a known build and which did not, and stops.

The PC install needs the same treatment. Hashing the `DAT/` tree into
`fixtures.toml` is an open item in [`DAT_CONTAINER.md`](DAT_CONTAINER.md) §4.

### Recipes

The engine never asks for "`AREA012.EMI` section 4". It asks for a **logical
asset**, for example `area/012/script`, `area/012/enemies`, `font/latin`,
`bgm/012` or `table/weapons`. For every logical asset, each build has a recipe:

```
asset   area/012/enemies
build   psx-us
locate  AREA012.EMI  section <n>  (dest 0x800E4000)
xform   widen-enemy-names 8->12          # the port's layout, DAT_CONTAINER §2
expect  sha256 <hash of the canonical output>
```

- **locate:** file, then section (or byte range, or XA channel and sectors).
- **xform:** a named transform written once in the importer. Examples: `copy`,
  `decompress-type1`, `widen-enemy-names`, `xa-decode`, `vag-decode`.
- **expect:** the hash of what the recipe produces. This is what lets two
  people prove they built the same asset without anyone hosting it. The idea
  is `loc_build.py`'s, which already prints its glyph table's SHA-256 for that
  reason. A hash of Capcom's data is not Capcom's data. `fixtures.toml` already
  publishes hashes of it.

**Most recipes can be generated, not hand-written.** The census pairs every
PC chunk with its PSX section or group, and 2,120 of 2,680 paired sections are
byte-identical ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2). For those, the
recipe is `copy`. The 513 data differences have six known causes, and each
cause is one transform. Hand-written recipes are the exceptions, and each
exception is already listed.

Recipes are per build, and **the regional builds are not address-compatible**
([`STATUS.md`](STATUS.md) "A stated goal worth recording now"). So a Western
disc's recipes are measured on that disc, never derived from the JP disc's by
offset arithmetic.

## 2. What a recipe file is not

It is not game data. It holds file names, section indices, transform names and
hashes. [`LICENSING.md`](LICENSING.md) §3 (the engine/data split) is untouched:
recipes and the importer ship, and the bytes they point at stay on the
player's disc.

The line to watch is `xform`. A transform must be an algorithm, not a patch
that carries replacement bytes. "Widen names 8→12" is an algorithm. "Replace
these 1,798 bytes with these" is shipping data, and belongs in the player's
own sources, or in content we authored ourselves.

## 3. Import once into a canonical format; the engine reads one format

```
player's sources ──(importer: identity → recipes → transforms)──▶ asset cache ──▶ engine
  BOF3 install                                                    (local, hashed,
  PSX disc(s)                                                      per-asset provenance)
```

The alternative — an engine that reads every source format at runtime — puts
per-region branches into game code, and they multiply. The import step keeps
all of them in one tool.

- **The canonical format is whatever our engine code reads.** Our engine is
  descended from the PC port, so where the port changed a layout (enemy names
  widened, compressed arenas shipped decompressed, kind-0 tags as arena
  offsets), the canonical form is the port's. The PSX importer converts to it.
  Where the port *lost* something (the SEQ music, §4), the canonical format
  gains a form the port never had.
- **Build and install are separate.** Compiling the engine needs no game data,
  so it can run in CI and be distributed as a binary — the OpenRCT2 /
  DevilutionX model ([`LICENSING.md`](LICENSING.md) §3). Importing runs only on
  the player's machine. "Build the game locally" means both steps, but neither
  depends on the other.
- **`tools/loc_build.py` is the prototype.** It already reads the player's
  disc, transforms sections, writes overlay DATs of our own kinds 4 and 5, and
  hashes what it made. The general importer is that tool, widened from
  language assets to every asset.

## 4. The inventory: what a PSX disc can supply

Built from measurements already on record. "Work" is what the importer or the
engine still needs.

| Asset | From a PSX disc | Work | Evidence |
|---|---|---|---|
| Images (kind 1) | yes; the tag is the PSX VRAM word | none: 546 of 593 identical, the 47 others are the port's replacement art | `DAT_CONTAINER.md` §2 |
| Data (kind 0) | yes | `copy` for 1,574; transforms for the six causes of the 513 others | same |
| — compressed arenas (65) | yes | **a type-1 decompressor, not written** | `DAT_CONTAINER.md` §2, §4 |
| — enemy tables (200) | yes | widen names 8→12, stride `0x88`→`0x8C` | same |
| — script and text blocks (199) | yes: script bytecode + the disc's language | each disc's text encoding (US done) | same; [`dialogue-localisation.md`](dialogue-localisation.md) |
| Sound banks (kind 2) | yes, as VAG ADPCM | a VAG decoder (small) | `DAT_CONTAINER.md` §2: every bank pairs exactly |
| Sound effects (`SND/`) | yes: cut from `MAGIC00.STR`, `VOICE.STR` | an XA decoder, and **the cut table**, not yet located | `DAT_CONTAINER.md` §5: 99.6% of sectors accounted for |
| Music (`BGM/`) | **as sequences, not recordings** | a SEQ player and an SPU-accurate synth; the track mapping | `DAT_CONTAINER.md` §5; [`bgm-comparison.md`](bgm-comparison.md) |
| FMV | as STR / MDEC | an MDEC decoder | PC ships AVIs; DIV-0001 |
| Latin font | yes | built today by `loc_build.py` | [`dialogue-localisation.md`](dialogue-localisation.md) |
| Chinese font (kind 3), 12 PC-only art sections | **PC only** | none; needed only for Chinese | `DAT_CONTAINER.md` §2, §4 |
| Tables compiled into `BOF3.exe` | from the boot EXE and overlays | **per-SKU address maps**, §5 | [`exe-table-audit.md`](exe-table-audit.md) |

"Scenario logic" divides cleanly. The event scripts are bytecode in each area's
`0x80010000` block, which is data and comes from the disc. The interpreter
that runs them is code, and becomes ours.

The two big items are engine work, not importer work: **the music player** and
**the MDEC decoder**. Everything else is decoders and transforms whose inputs
are already understood.

## 5. The exe is a data source too — and the rule that follows

Taking over all the code does not make `BOF3.exe` irrelevant. Its data sections
hold game tables. The item and ability names DIV-0008 overwrites are
fixed-stride records in the exe's `.data` ([`name_tables.cpp`](../src/game/name_tables.cpp)),
the PSX `GAME.EMI` tables with the name field widened. On the disc, the same
tables live in the boot EXE and in overlay sections: some of the 542 sections
the port dropped are data, not code (`DAT_CONTAINER.md` §2, "not all code").

So a disc-only build needs, per SKU, a map from each exe-resident table to its
place on that disc. The map has the same shape as a recipe (§1), with the
PC-exe address in place of the logical id.

That is only possible if our code never absorbs the tables themselves.
**Proposed rule:**

> A taken-over function reads game tables from loaded data — the image by
> address today, the asset cache later. It does not transcribe their values
> into C++.

Layout is fine: strides, counts, field offsets and addresses describe where
data is. `name_tables.cpp` is the pattern to follow: its table has addresses,
strides and counts, and every name and number is read from the image.
Instruction immediates are fine too, since they are code. The line falls
between an array copied out of `.rdata` and a constant that was a `mov`'s
operand.

Why now: a transcribed table is rule 1 in a `.cpp` file, and it silently binds
that function to one build. Both are cheap to prevent at takeover time and
expensive to find later. The audit of the ~400 files taken over so far is
[`exe-table-audit.md`](exe-table-audit.md).

**The table catalog** is [`tables.toml`](../tables.toml) (started 2026-09-26
with the item and ability tables). It records each table's symbol, count, record
size, each disc build's recorded address, and what every field means, with
the reader that proves it. It holds no values: `tools/tables.py dump` reads
them from the player's own `BOF3.exe` or disc, into `analysis/`. That is this
section's map in the form an importer can use.

The rule is written here as a proposal. Promoting it to a `CLAUDE.md` hard rule
is the owner's decision.

## 6. Mixing sources

A player with the PC install and a US disc should be able to choose per
asset: US text, PC music, JP art. The importer resolves each logical asset
from an ordered source list, the player's or a preset's. It records in the
cache, **per asset, which build and recipe produced it** — otherwise
"AREA012 crashes" cannot be reproduced. That record is the per-install
counterpart of `fixtures.toml`, and is what a bug report attaches.

Choosing a source is a behavioural choice. A default that differs from the PC
port's (disc-sequenced music in place of the MP3s, say) is a
[`DIVERGENCE.md`](DIVERGENCE.md) entry, like any other. An option the player
turns on is not, but the ledger records that the option exists.

## 7. Relationship to the rules

- **Rule 1** — the importer ships; recipes, hashes and transforms are not game
  data (§2). What the importer produces lives outside the repo, as
  `loc_build.py`'s output already does.
- **Rule 6** — the disc answers what the game *was*. The canonical format and
  the source defaults answer what it *should be*. A recipe records the first,
  and any transform that changes behaviour, not just layout, is ledgered.
- **Evidence rule** — every `expect` hash is a measurement. A recipe without
  one is a hypothesis.

## 8. Staging

**Now, cheap, during phase 3:**

1. Adopt §5's rule, and settle the audit's findings.
2. Hash the `DAT/` tree into `fixtures.toml` (`DAT_CONTAINER.md` §4).
3. Give assets logical ids as their loaders are taken over. Where a loader
   asks for a file and a chunk, record the id it *means*: a comment, or a
   `symbols.toml` field.

**Groundwork, whenever convenient:**

4. Per-file hash tables for each held disc in `fixtures.toml`.
5. The type-1 decompressor; find the `SND/` cut table and the BGM track map
   (both presumably in the exe beside their name strings).
6. The music comparison in [`bgm-comparison.md`](bgm-comparison.md) (I23),
   which decides whether sequenced music is worth its cost.

**Phase 5:**

7. The general importer, recipes generated from the census, one build at a
   time: JP first (the census's own pair), then US.
8. A SEQ/VAB player on an SPU-accurate synth, and an MDEC decoder.
9. Exe-resident tables from each SKU's boot EXE and overlays.
10. Disc-only boot: the PC install becomes one optional source.

## 9. Open

- **The recipe file format** — TOML to match `fixtures.toml` and
  `symbols.toml`, or generated JSON. Undecided; it matters less than keeping
  it generated.
- **Where canonical differs from both originals.** Any layout that is neither
  the PC's nor the PSX's is ours, and ledgered.
- **Region differences in code, not data.** Where a Western PSX build's
  *code* behaves differently from the JP build, a disc-only build from a
  Western disc still runs our code, which descends from the PC port, which
  descends from JP. Whether any such difference matters to play is unmeasured.
