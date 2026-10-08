# The unified game data: a detailed plan for the importer and the cache

**Status:** PROPOSAL (2026-10-06, evening) - the detailed plan
[`ASSET_SOURCES.md`](ASSET_SOURCES.md) deferred until its phase 4 measurement
had run. That measurement ran today ([`region-diff.md`](region-diff.md)),
with the music pairing ([`bgm-comparison.md`](bgm-comparison.md)) and the
PSP's executables read ([`psp-stallion.md`](psp-stallion.md)); this plan is
built on those numbers. `ASSET_SOURCES.md` stays the statement of the goal
and the rules; this file is how to get there, step by step, with what each
step costs and what it waits on. Nothing here is scheduled until the owner
picks a step; a step that lands moves to [`STATUS.md`](STATUS.md).

## 0. The goal, restated in one paragraph

A player points the build at the copies of *Breath of Fire III* they own -
the 2001 PC port, any of the PSX discs (JP, US, FR, DE), either PSP disc, or
a mix - and one importer turns them into one cache the engine reads: a
language-neutral `base/`, per-language `loc/<lang>/` layers, and optional
layers the player can turn on. The engine never reads an original format at
run time and never carries a game table in its source
([`ASSET_SOURCES.md`](ASSET_SOURCES.md) sections 3 and 5). Every asset in the
cache records which build and recipe produced it.

## 1. What today's measurements settled

| Question the old plan left open | Answer | Where |
|---|---|---|
| Do the regional PSX builds differ beyond text? | Yes, in **83 rows of four kinds**, none language: one area's map band and its placement map (a collision fix every later build made), one cue byte in 65 voice banks (a PSX sound priority), one sample dropped from 8 PAL area banks. Enemy stats identical in 200 of 200 tables; no non-language image differs except CLUTs. | `region-diff.md` 4.1, 7, 8 |
| So is `base/` one tree? | **Yes**, JP's (the census's pair), with a short per-build exception list; the two post-release fixes are better taken as our own code than as per-build bases (DIV-0080 is the first). | `region-diff.md` 7 (1), 10 |
| Is the PSP a different game's data? | **No.** Its data carries JP's code byte for byte; PSP-EU is PSP-JP plus a language layer; audio is the PSX's samples re-containered; 65 compressed arenas ship decompressed, 37 byte-identical to the PC's. Its content changes are art (653 tiles blanked, 301 redrawn, 53 palettes), text, and 11 map bands. | `region-diff.md` 5 |
| Does the PSP change game logic? | Not where read: the boss module and the encounter paths are identical compiles of the PSX's; the one difference (a render-load guard removed) the PC never trips. P4 refuted, P6 and P7 confirmed as palettes and a name. | `psp-stallion.md` 3, 5 |
| How does the PC's music map to the disc's? | **File N is song N** (164 pair; song 21 has no MP3; `165` the victory sub-song; `166` unpaired). The MP3s are MPEG-1 Layer III 44.1 kHz 128 kbit/s CBR plain stereo, no tags. Loop points are in the sequences; the PC replays every intro. | `bgm-comparison.md` 6, 7, 10 |
| What is held? | JP, US, FR, DE PSX and both PSPs, verified with per-file manifests (`fixtures/*.files.tsv`); the PC's 742 `DAT/` files too. Only `psx-eu-en` is missing. | `fixtures.toml`, `region-diff.md` 1 |

Still open, and carried into the steps below (the type-1 decompressor is
written: step 1, [`type1-compression.md`](type1-compression.md)), the `SND/` cut table, the recipe file format, the canonical
font format, the PSP's instrument (`PPHD`) and sequence (`pPMS`) readers, and
every exe-resident table's address on each SKU beyond the item and ability
tables already in [`tables.toml`](../tables.toml).

## 2. The cache's shape

```
cache/
  manifest.toml            what was imported from which build, per asset (provenance; the bug report's attachment)
  base/                    language-neutral, JP's layout as the PC port reads it (ASSET_SOURCES 3)
    dat/<NAME>.DAT         one container per original file, the port's kinds (0 data, 1 image, 2 bank, 4/5 overlays)
    exe/                   the exe-resident tables, laid out as BOF3.exe's .data has them (section 5)
    bgm/NNN.DAT            the music, as the engine's player wants it (section 6)
    snd/                   the effect waves
  loc/<lang>/              per-language layers: text blocks, pools, names, labels, the font, language images
  opt/<name>/              optional layers the player turns on: psp-art (P6's palettes, the tiles), psp-names (P7 and the other renames), ...
```

- **One container per original file** keeps `LoadDatFile` and every loader
  of ours unchanged: they already walk kind-0/1/2 chunks by tag, and the
  language overlays already prove the layer mechanism (DIV-0005: a
  `DAT\<lang>.<NAME>.DAT` beside the base file, walked after it). `opt/`
  layers use the same mechanism with a second prefix - the one loader change
  [`psp-stallion.md`](psp-stallion.md) section 4 costs at hours.
- **Layers override by chunk**, never by byte: a layer carries whole chunks
  (a palette row's section, a text block), which is how the overlays work
  today and what keeps provenance per asset.
- **Fixes by rule are code, not layers.** DIV-0080's walls are applied by
  coordinate at load; the cue byte would be the same shape if the PSX banks
  were ever played. A layer carries a later build's bytes only when a rule
  cannot express them (the 30-cell re-texture, `region-diff.md` 10.2).

## 3. The importer's pipeline

```
sources  ─▶ identity ─▶ plan ─▶ transforms ─▶ cache + manifest ─▶ verify
```

1. **Identity.** Hash what the player gives (`verify_fixtures.py --tree`
   does this today for every held build): the build id, or "unknown" with
   the nearest match. Unknown builds import nothing.
2. **Plan.** From the player's ordered source list (or a preset: "PC
   install", "US disc only", "PC + US text"), resolve every logical asset to
   (build, recipe). The recipe table is **generated** from the census and the
   region diff, not hand-written: `dat_census.py`'s pairing is already the
   JP recipe set; `region_diff.py`'s classification adds each other build's
   rows (identical: the base's; text: `loc/`; layout: a transform; logic-data:
   a per-build exception or a rule).
3. **Transforms**, per build (`region-diff.md` 7 (3), made concrete):
   - `copy` - most sections.
   - `decompress-type1` - the 65 arenas (section 4).
   - `widen-enemy-names` - 8 to 12, stride `0x88` to `0x8C`, stats untouched.
   - `remap-dest` - the Western discs' bands (`+0x8000` on three image bands,
     `+0x6000` on the pool, `+0x3000` on the boot band;
     [`regional-builds.md`](regional-builds.md)).
   - `split-language` - message blocks, pools, enemy-name fields, language
     images and plate CLUTs into `loc/<lang>/`; the text encoding per disc
     (US done, FR / DE / JP to add as `loc_build.py` grows).
   - `psp-unwrap` - the header bits, the `pBVC` strip to the PSX sample
     body; `PPHD` / `pPMS` only once readers exist, else the instrument and
     sequence data come from a PSX source in the same install.
   - `exe-tables` - section 5.
   - `wave-from-vag` / `wave-from-xa` - the banks and `SND/` (the VAG decoder
     is small; XA needs the cut table).
4. **Cache + manifest.** Write the containers, hash each, record
   provenance per asset.
5. **Verify.** Against a PC install the cache's `base/` + `loc/zh/` must
   reproduce the port's `DAT/` byte for byte except the port's own art and
   the four logic-data rows the port lacks; `fixtures/pc-zh.DAT.files.tsv` is
   the oracle. Against the US disc, `loc/en/` must reproduce what
   `loc_build.py` builds today. These two checks are the importer's
   self-test, headless, run in CI with the player's files absent (skipped)
   and on the owner's machine (run).

**The recipe file format: decide TOML, generated.** It matches
`fixtures.toml`, `symbols.toml` and `tables.toml`, diffs readably in review,
and the generator keeps it honest; a JSON twin for tools that want it is a
one-line export. (ASSET_SOURCES section 9's open item, proposed closed.)

## 4. The decoders and the engine items, sized

| Item | Needed for | Size | Oracle |
|---|---|---|---|
| Type-1 decompressor | every disc source (65 arenas) | **done 2026-10-08** (`tools/type1.py`): 50 of 65 equal the PSP's, 51 the PC's, the 65th confirmed byte by byte; 14 PC edits found | [`type1-compression.md`](type1-compression.md) |
| VAG (ADPCM) decoder | the banks from a disc | small, well known | the PC's converted WAVs pair every bank |
| XA decoder + the `SND/` cut table | the effects from a disc | medium; the cut table is still unlocated (beside the `SND` name strings in the exe, presumably) | the PC's `SND/` files |
| MDEC + STR | FMV from a disc | large; out of this plan (I7 for the player; the disc path waits) | the AVIs |
| SEQ / VAB player on an SPU synth | sequenced music | large; **gated on the owner's ear** (`bgm-comparison.md` 10) | the three Mednafen renders, and the method for more |
| MP3 loop-point table | correct loops on the PC's music without a synth | a day of tooling plus ~10 h of unattended measuring (156 songs at ~4 min); a DIV | the sequences' loop markers |
| `LoadDatFile`'s second prefix | `opt/` layers | hours | the language overlays' tests |
| State hash address-independence | testing after the cutover and with layers in play | small | `state-hash.md` section 6 |

## 5. The exe-resident tables

The engine reads the PC's tables from `.data` at fixed addresses, and the
rule is that it never transcribes them ([`ASSET_SOURCES.md`](ASSET_SOURCES.md)
section 5; [`tables.toml`](../tables.toml) catalogues the item and ability
tables so far). For a disc-only install the importer must produce the PC's
`.data` layout of each table from the disc's boot EXE or overlay:

1. **Finish the catalogue**: every table our code reads by address
   (`grep` the `kTable`-style constants and `name_tables.cpp`'s pattern across
   `src/game`; the audit [`exe-table-audit.md`](exe-table-audit.md)), each
   with its PC address, count, stride and fields, and its PSX-JP address
   (the sibling's `symbols.toml` and the twin method). Today's finds extend
   it: the PSX song table at `0x80182830` in `SLPS_009.90` (165 entries;
   `bgm-comparison.md` 6), the ability table in the PSP executables with
   ability 115's row (`psp-stallion.md` 3.2), the Holy Mantle's accessory
   record `0x15` (5.1).
2. **Per-SKU address maps**: US, FR, DE boot EXEs and both PSP `BOOT.BIN`s
   (plain ELFs - the correction in `psp-stallion.md` 3.1), found by the same
   twin method; `tools/tables.py dump` reads values from the player's file
   into `analysis/`, never into the repo.
3. **The transform**: read each table from the source, widen or re-stride
   where the port did (names 8 to 12 is the known case), write `base/exe/`
   in the PC layout; the engine maps `base/exe/` where `.data` was. This is
   the step that makes state 3 of the cutover
   ([`platform-layers-plan.md`](platform-layers-plan.md) section 3) possible
   without the PC's executable: the engine's data comes from the cache
   instead of the mapped `BOF3.exe` sections.

Until step 3, a PC install is required for the tables and everything else
is optional - which is already more than today.

## 6. Music and sound in the cache

- `base/bgm/` holds the PC's MP3s as they are (file N = song N), with a
  **loop table** if the owner hears the replayed intros (H1) and wants them
  fixed - the cheap divergence `bgm-comparison.md` 10 describes.
- The disc's sequences and instrument banks are imported into `base/seq/`
  regardless (small, and they are the synth's input and test fixture); the
  synth itself waits on the owner's ear and is phase-5 sized.
- Song 21 (no MP3) and `166` (no sequence) are recorded as the two
  irregulars; whether the game plays song 21 is a trace question for a route.
- The cue byte (`region-diff.md` 8.3) becomes a by-rule fix the day the PSX
  banks are played directly, not before.

## 7. Divergence policy for data

- **A later build's fix made the default is a ledger entry applied by rule
  in code** (DIV-0080's form), so every source gets it. The owner's rule of
  2026-10-06: a bug fix is worth keeping as the default.
- **A later build's content change is an `opt/` layer** from the player's own
  disc, off by default, recorded in the ledger as existing (Stallion's
  palettes, the renames). A content change made the default would be a
  ledger entry with the layer required.
- **A regional accommodation stays with its region** (the PAL sample swap
  follows the FR / DE `loc/` layers' build, or is dropped; `region-diff.md`
  8.4 recommends dropping).
- The archival record of "what each build was" stays in `region-diff.md`
  and `fixtures/`; the cache never overwrites it (rule 6).

## 8. Steps, in order, each a round or less

| Step | What | Size | Gate | Needs the owner |
|---|---|---|---|---|
| 1 | ~~The type-1 decompressor, from the 37 PSP oracles, then all 65 against the PC~~ **done 2026-10-08**, [`type1-compression.md`](type1-compression.md); the port's 14 arena edits go to step 2's recipes | a group | none | no |
| 2 | The recipe generator: `dat_census.py` + `region_diff.py` output to `recipes/*.toml`; the importer skeleton with identity, plan, `copy`, `split-language` (US, as `loc_build.py` does), verify against the PC install and the US overlays | a round of 2-3 agents | none | the recipe format (proposed TOML) |
| 3 | The transforms for every PSX disc: `widen-enemy-names`, `remap-dest`, the FR / DE / JP text encodings, the four per-build exceptions handled as section 7 says; `loc/fr`, `loc/de`, `loc/ja` land | a round | step 2 | which languages first |
| 4 | `opt/` layers and the loader's second prefix: `psp-art` (P6) and `psp-names` (P7 + the other renames) from the player's PSP disc; the PSP unwrap transforms | a group (a day for P6 by `psp-stallion.md`'s estimate) | step 2 | option or default (recommended: option) |
| 5 | The exe-table catalogue completed and the per-SKU maps (section 5, steps 1-2) | a reading round | none | no |
| 6 | VAG and the banks; the `SND/` cut table found and XA | a group | step 2 | no |
| 7 | The MP3 loop table, if H1 is heard | a day + 10 h unattended | the owner's ear | yes: the listening session |
| 8 | `base/exe/` produced and the engine reading it (section 5 step 3) | a round, with the biggest live check | steps 2, 5; the cutover's state 3 design | no |
| 9 | The SEQ / VAB player and the SPU synth | phase 5 | the owner's ear; the disc-only goal | yes |
| 10 | MDEC / STR for FMV from a disc | phase 5 | I7 | no |

Steps 1, 2 and 5 need nothing from the owner and can start together; 3, 4
and 6 follow 2. After 8, a player with a US disc and no PC install has
everything but music and video; after 9 and 10, everything.

## 9. What it changes elsewhere

- **The state hash** gains layers to skip or to hash by content (the
  language overlay's cells are the known case); its address-independence is
  step 4's companion.
- **The launcher** grows a Sources page: the player's files, their
  identities, the language and option layers found, the ordered source list.
  The ini already carries `language=`.
- **CI** builds the engine and the importer and runs the importer's
  self-test with no files (skipped); the owner's machine runs it for real.
- **Licensing** is unchanged by design: the repo holds recipes (where and
  how), manifests (hashes), and code; every byte of game data stays the
  player's ([`LICENSING.md`](LICENSING.md) section 3).

## 10. Open

- Whether the PSP's `PPHD` / `pPMS` want readers at all, or PSP installs
  simply take sequences from a PSX source (recommended until a PSP-only
  install is a goal).
- Whether `base/` should carry the PSP's 11 map-band changes as a rule or a
  layer: unread (region-diff 6); read them before deciding, as section 8 did
  for the Western rows.
- The four unexplained logic-data bytes of the PC against JP (`RYUD00..03`
  at `+0x7ACE`, region-diff 9).
- `psx-eu-en`, the one release not held.
