# A PSP-only install's base: the PSP's own sections standing in

**Status:** MEASURED (2026-10-12, the PSP-only round's group SI,
[`psp-only-round.md`](psp-only-round.md); both PSP discs, the JP PSX disc and
the PC's `DAT/` checked; offline only: not rendered, not played)

The owner's call of 2026-10-10 ([`psp-only-build.md`](psp-only-build.md) 7,
call 1): **a PSP-only install uses the PSP's art and data as its base.** This
builds that. With only PSP discs among the sources, the importer now fills
every `base/` chunk the PSP does not carry in the PC's form from the PSP's own
section at the same place. A PSP disc alone now writes **725 of 741 `base/`
containers** (519 before). The 16 it does not write are the containers whose
bank lost its second tone, which are MB's.

Hard rule 1: nothing here or in the recipe is a byte of a disc. Names,
indices, counts, sizes and hashes only.

## 1. The base policy

`tools/psp_standins.py` decides the policy from the sources that give
`base/` chunks (the PC's `BOF3.exe` gives none):

| Sources | `base_policy` | What stands in where no source carries a chunk |
|---|---|---|
| only `psp-jp` and/or `psp-eu` | **`psp`** | the PSP's own section (this doc) |
| anything else: the PC's `DAT/`, any PSX disc, with or without a PSP disc | `psx` | a PSX disc's own section, as before ([`importer-transforms.md`](importer-transforms.md) 5, [`arena-edits.md`](arena-edits.md) 4); nothing new |

So a PSX disc beside a PSP disc keeps the PlayStation's base, and the PC and
PSX paths cannot change (section 5 measures it). The manifest records the
policy in a new table, the contract with the launcher (LT):

```
[build]
base_policy = "psp"                 # or "psx"
standins_sha256 = "..."             # psp only: the stand-in recipe the build used
```

It sits after `[meta]` in every manifest the importer writes, `psx` ones
included.

## 2. The recipe and the build

`recipes/psp-standins.toml` is generated, never edited:

```
python tools/psp_standins.py recipes --disc JP --disc PSPJP --disc PSPEU
```

For every `base/` chunk of `recipes/pc-zh.toml` in the classes below that a
PSP build does not carry byte for byte, it finds the chunk's place on the JP
PSX disc (the recipe's exact source there, or JP's own stand-in), aligns that
EMI with the PSP's (`region_diff.align`), applies the how, and records
`build:how:EMI#section:sha256` of the output and a reason per build. One row
per chunk, 277 rows. `recipes/pc-zh.toml` is **not** regenerated: none of
this needs its rows.

At build time `importer.get_chunk`, after every exact source, bank and PSX
stand-in has failed, asks `psp_standins.stand_in`. Under the `psp` policy that
takes the first PSP source in the player's order that the row lists, applies
the how and **hashes the output against the row** before it is written. The
manifest records it as `build:own:EMI#section` with its own hash, so
`verify` checks it like any stand-in. Where the output is the PC's chunk byte
for byte (two rows, section 3), it is recorded under its how (`hold`,
`icons`) with the recipe's hash.

`importer.py check` (CI) runs `psp_standins.check`: every row names a `base`
chunk of a class the policy fills, of the same class as `pc-zh.toml` says,
that its build does not carry; every build, how and reason known; every hash
a sha256; one row per chunk. No game data.

Two placement rules the generator needed, both measured on the discs:
- **The PSP sets a page tag's low byte** (for example `0x080F0202` where the
  PSX has `0x080F0201`, `0x1A080401` for `0x1A080400`). The PC's loader
  never reads that byte: a kind-1 tag is x tile, y tile, width, and an unused
  low byte ([`asset-loading-path.md`](asset-loading-path.md) 2). So a page is
  matched on the top three bytes; the chunk keeps the PC's tag.
- **`FIRST`'s 64 KB page at `0x1A080400`** has no pair: the PSP's `FIRST.EMI`
  carries a 32 KB page at `0x1A080200` in a new slot 3 and drops this one
  (`region_diff.align` on `ETC/FIRST.EMI`, 14 sections on JP, 15 on PSP-JP).
  The same JP page sits in `DEMO.EMI` 3 and `AREA024.EMI` 9 as well, and the
  PSP gives both of those one and the same page. The generator takes it (from
  `AREA024.EMI` 9, the same rectangle) only because every other place agrees.

## 3. What stands in, by class

`python tools/psp_standins.py recipes ...` (the run that wrote the committed
recipe):

| `pc-zh.toml` class | Reason (`why`) | How | PSP-JP | PSP-EU | Output |
|---|---|---|---:|---:|---|
| `disc` | `psp-base`: the PSP's own content change (area pages, palettes, map bands, the UI page, `DEMO` / `FIRST` / `SCENA17` / `COMMU02`, [`psp-only-build.md`](psp-only-build.md) 2.2) | copy | 206 | 206 | the PSP's section |
| `disc` | `language`: PSP-EU's own plates and pages where the PC kept Japan's (PSP-JP carries them) | copy | - | 22 | the PSP-EU's section |
| `type1` | `psp-base`: P8, Ryu's form data in 14 arenas | copy | 14 | 14 | the PSP's arena |
| `pc-byte` | `psp-base`: P8, `RYUD00..03` | `ryud` | 4 | 4 | the PSP's section with the port's byte |
| `pc-edit` | `arena`: the 11 port-edited arenas | copy | 11 | 11 | the PSP's arena |
| `pc-edit` | `start-arena`: `START` as the PSP's `PL012` | copy | 1 | 1 | the PSP's `PL012` arena |
| `pc-hold` | `hold`: the 27A rule on the PSP's arena | `hold` | 2 | 2 | `BPLU27A` **the PC's chunk**; `BPLD27A` P8 plus the port's hold |
| `pc-icons` | `icons`: the respace rule on the PSP's page | `icons` | 1 | 1 | **the PC's chunk** |
| `art` | `port-art`: the dial page, the PSP's own | copy | 16 | 16 | the PSP's page |
| | | | **255** | **277** | 253 / 275 stand-ins, 2 the PC's bytes |

`bank` (16) is not filled: the dropped tone is MB's
([`psp-only-build.md`](psp-only-build.md) 2.1). Everything a PSP build lacked
besides the banks is now filled; the generator reported no chunk it could not
place.

**PSP-JP against PSP-EU, row by row:** 240 of the 255 rows they share give the
same output. The 15 that differ are, by place, language (presumed: not
rendered): 10 of the 16 dial pages, the `0x1A080400` page in `AREA004`,
`AREA024` and `FIRST` (3), `DEMO`'s `0x12080400` page, and `FIRST`'s 512-byte
`0xB400` section. The 22 PSP-EU-only rows are the
plates of 11 world maps (`0xB0000`, `0xA000`; `AREA065` also `0xC0000`) and
`AREA128`'s painted page, the same chunks a Western PSX disc stands in for as
`language`.

### 3.1 The icons and dial rules on the PSP's section

- **Icons:** the PSP's `FIRST` icon page is byte-identical to the JP PSX disc's
  (`ETC/FIRST.EMI` 7 on PSP-JP against 6 on JP, the same sha256), so
  `respace-icons` gives **the PC's chunk exactly** from either PSP disc. The
  recipe generator missed it only because it asks for a destination equal to
  `0x080F0201`, and the PSP's tag ends `02`.
- **Dial page:** as on every PSX disc, no PSP carries the port's keyboard
  legend, and the PSP's own page stands in (`port-art`). Rendered: no. On a
  PSX disc that page shows the PlayStation's buttons
  ([`importer-transforms.md`](importer-transforms.md) 5); on a PSP disc it
  presumably shows the PSP's (a guess: not rendered). Six of the 16 are the
  same page on both PSPs, ten differ.

## 4. The 14 arenas from a PSP source

### 4.1 The decompressed form is the form the engine reads

The PSP ships the type-1 arenas decompressed. Nothing needs compressing, and
`tools/type1.py` is unchanged:
- the PC's own arena chunks are the decompressed form: 51 of its 65 are the
  PSX section decoded, byte for byte ([`type1-compression.md`](type1-compression.md) 2);
- a kind-0 chunk is `memcpy`'d to the arena as it is, with no decompression
  ([`asset-loading-path.md`](asset-loading-path.md), the kind table);
- the PSP's sections are that form: `python tools/type1.py check --disc JP --psp PSPJP`
  gives **50 of 65 byte-identical** to the PSX decode; the 15 that differ are
  `BPLD247 257 267 278 27A 457 467 478 567 578 678` and `BRTD247 457 467 478`.

### 4.2 `BPLD012`, `015`, `016` do not carry the PSP's change

The round plan and [`arena-edits.md`](arena-edits.md) 6 say that `BPLD012` /
`015` / `016` "also carry the PSP's own change". **Measured, they do not.**
They are not among the 15 in 4.1, and each PSP's section hashes equal to the
PSX disc's decoded section that `pc-zh.toml` already lists as its `psx-jp`
stand-in. The same holds for the eight `PL` arenas and for `PL012` (so for
`START`): **all 12 port-edited arena stand-ins from a PSP disc are byte for
byte the PSX disc-only cache's.** The PSP's own change sits in the 14 `type1`
arenas and `BPLD27A`, which section 4.4 reads.

So under the PSP base the 11 arenas and `START` are DIV-0096's case unchanged
(the port's script edits absent), not a new one.

### 4.3 AR's rules on a PSP source

- **`hold` (27A):** `importer.hold_arena`, the rule's body split out of
  `hold_loop` so that it takes an arena already decompressed (the PSX path
  decodes first, then calls it; its outputs are unchanged, section 5). On
  `BPLU27A` it gives **the PC's chunk exactly** from either PSP. On `BPLD27A`
  it gives the PSP's arena, P8 change included, with the port's hold: the
  rule found its two identical four-step loops in the PSP's member A bank as
  on the PSX.
- **`start-arena`:** `START` is the PSP's `PL012` section, which is the PSX
  decode of `PL012`, so `START` is byte-identical to the PSX disc-only cache's.

### 4.4 P8 as stand-ins: what the PSP changed in the arenas

`arena_struct.py` (scratch; the levels of [`arena-edits.md`](arena-edits.md)
1) on PSP-JP's 15 changed arenas against the JP decode:
- each arena is **the same size**, and **one bank of three** differs: the bank
  of the member whose file-name digit is `7` (`BPLD247` bank 2, `BPLD278`
  bank 1, `BPLD27A` bank 1, ...), 110,592 bytes on both;
- in that bank the palette is unchanged; the images, the scripts and the
  frame entries differ, and the four level offsets move;
- the script count (20) and the step count (266) are the same, and on both
  sides **0 steps name a frame past their script's frame count** and no
  offset leaves its bank.

`RYUD00..03`: the PSP's sections hold `0xF7` at `+0x7ACE` as every PSX disc
does, so the `ryud` rule applies and the stand-in is the PSP's section with
the port's byte. Their other differences (P8) were not read here.

Whether our code runs these is **CD's to say** (the round's section 4
contract). The consumers read every arena offset from the data
([`arena-edits.md`](arena-edits.md) 1, 3), and the layout above stays inside
what they read. If CD reports otherwise, the matching rows become a refusal
with the reason: `stand_in` returns nothing for a row that is not in the
recipe, so deleting the rows and regenerating is enough.

## 5. Measured

Builds from `668a9b67`'s tools (the reference) and this branch's, with
`importer.py build` and `verify`, compared file by file with the manifests set
aside.

| Sources | Before | Now | `verify` |
|---|---|---|---|
| PSP-JP alone | 519 containers | **725**; 3,038 chunks in their exact form (885 of them banks by `wave-from-vag`), **253 stand-ins** in 211 containers | 0 problems; `base/exe` the image `exe.toml` records |
| PSP-EU alone | 519 | **725**; 3,016 exact, 275 stand-ins in 211 containers | 0 problems |
| PSP-JP against PSP-EU | equal but for `base/exe` (2 files) | 1,601 of 1,618 files equal: the 15 language containers of section 3 and `base/exe` (IM's) | |
| PSP-JP and PSP-EU, each before against now | | the 1,412 files written before: **all byte-identical**; 206 containers new | |

The 16 containers left are each one bank short; nothing else is missing from
a PSP-only `base/`.

The PC and PSX paths, each built from both trees and compared file by file
(`cmp.py`, scratch), the manifests aside:

| Sources | Files | Result |
|---|---:|---|
| JP disc + PC `DAT/` + `BOF3.exe` | 2,125 | all byte-identical |
| US disc alone | 1,872 | all byte-identical |
| European disc alone | 1,864 | all byte-identical |
| PC `DAT/` + `BOF3.exe` + US + PSP-EU + PSP-JP | 2,126 (the round brief's 2,125 plus `base/fmv`) | all byte-identical |
| PSP-EU then PSP-JP (both, `psp`) | | 725 containers: the 22 language chunks come exact from PSP-JP, 253 stand-ins from PSP-EU |

Each of those manifests now has `[build] base_policy = "psx"` after `[meta]`;
otherwise only the build time moves. `importer.py check` exits 0 (742 files,
3,582 chunks, 0 errors; `psp_standins` 0 errors). `recipes/exe-gaps.tsv` is
untouched.

## 6. Not verified

- **Nothing was rendered or played.** The PSP's look in a PSP-only base (its
  blanked and redrawn area tiles, its UI page with the PSP's shoulder-button
  labels, its dial page, its title pages) is in `base/` as the PSP ships it.
  Whether our engine draws all of it sensibly is V3's: in particular whether a
  tile the PSP blanked (653 of them, [`psp-only-build.md`](psp-only-build.md)
  2.2) was drawn from somewhere else on the PSP, and so shows as a hole here.
- **Whether our code runs the PSP's P8 arenas and `RYUD`** (CD).
- What the PSP's `RYUD` change is.
- The engine still reads `DAT/`, not the cache; a PSP-only `install --base` is
  PN's.
- `AREA004` under the PSP base: the PSP's map band carries the Western walls'
  cell plane but its placement map is JP's ([`opt-layers.md`](opt-layers.md)
  4), so a PSP-only base has DIV-0080's walls without the placement half. The
  `area4-walls` layer needs a Western PSX disc, so the policy changes nothing
  there. Not played.

## For the other files

### DIV A (draft): a PSP-only install's base is the PSP's

- **Subsystem:** the importer's base (`tools/psp_standins.py`,
  `recipes/psp-standins.toml`, the one call in `tools/importer.py`
  `get_chunk`); the manifest's `[build] base_policy`.
- **Original behaviour:** the PC port ships the PlayStation's data in `DAT/`
  with its own edits; a cache built from a PSP disc alone had no source for
  about 270 `base/` chunks and wrote 519 of 741 containers.
- **New behaviour:** when every source is a PSP disc, each `base/` chunk no
  source carries is the PSP's own section at the same place: its area pages,
  palettes, map bands and UI page (206), Ryu's form data (P8: `RYUD00..03` with
  the port's `0x7ACE` byte, 14 arenas, and `BPLD27A` with the port's hold),
  PSP-EU's own language plates and pages (22), the PSP's dial page where the
  port drew its legend (16), and the 11 port-edited arenas and `START` as the
  PSP ships them. `BPLU27A` and `FIRST`'s icons are the PC's bytes, by the
  port's rules. 725 of 741 containers. With the PC's `DAT/` or a PSX disc
  among the sources nothing changes.
- **Why:** the owner's call of 2026-10-10 (`psp-only-build.md` 7, call 1):
  for a PSP-only player there is no PlayStation original to fall back on, so
  the PSP's content changes are the base, and the PlayStation look becomes
  the option a PSX disc enables. This inverts the plan's data policy
  (`unified-data-plan.md` 7) for PSP-only installs only.
- **Visible?** Yes, presumably: the PSP's area art, its UI page (its
  shoulder-button labels), its title pages and dial page, and Ryu's forms as
  the PSP draws them. Not seen; V3.3's captures are the check.
- **Reversible?** Add a PSX disc to the sources (the `psx` policy), or the
  PC's `DAT/`.

DIV-0096 needs no amendment: the PSP's 11 arenas and `START` are the PSX
disc's decode (section 4.2), so its entry already describes them. Its doc's
section 6 sentence on `BPLD012` / `015` / `016` is wrong (section 4.2); the
coordinator may want a correction there and in `psp-only-round.md` section 1's
arena row.

### For `docs/README.md`

| [`psp-stand-ins.md`](psp-stand-ins.md) | A PSP-only install's base: the PSP's own sections stand in for the ~270 chunks it lacks (`base_policy = "psp"`); 725 of 741 containers; the 14 arenas from a PSP source | MEASURED |

### For `docs/psp-only-build.md` section 5

Step 2 done: `base/` complete from a PSP disc except the 16 banks.

### For `docs/STATUS.md`

A PSP disc alone builds 725 of 741 `base/` containers (the PSP's own sections
as base, DIV A); the 16 left wait on MB's dropped tone.
