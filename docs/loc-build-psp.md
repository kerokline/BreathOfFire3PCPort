# A language layer from a PSP disc alone: `loc_build.py` on PSP-JP and PSP-EU

**Status:** IN PROGRESS (2026-10-12, the PSP-only round's LG group, branch
`psp/lg`; a local session without the game running). Built and diffed for
both PSP discs; not played (the engine reads a disc-only cache since state 3,
PSP-only installs started headless in the end batch's V2 / V3, [`psp-end-batch.md`](psp-end-batch.md); not played).
**Updated 2026-10-17** (D246, branch `fix/psp-config-text`): the Config
screen's text is built from PSP-EU now (section 3.3).

The disc-only round made `loc_build.py all --cache` build a language layer
from a PlayStation disc alone ([`loc-build-disc-only.md`](loc-build-disc-only.md)).
This doc runs the same build against a PSP disc
([`psp-only-round.md`](psp-only-round.md) section 2 row 5): `ja-JP` from
PSP-JP, `en-150` from PSP-EU, the names and the exe's strings from
`BOOT.BIN`, and every container diffed against the `psx-jp` and `psx-eu-en`
disc-only layers. No game bytes appear here: counts, offsets, record indices
and hashes only (`CLAUDE.md` rule 1).

**The result in one paragraph.** Both PSP discs give a layer of **254 of
254** containers, the same set as a PSX disc's, and replace the same **381
of 475** `loc/zh-CN` chunks. `ja-JP` from PSP-JP differs from the PSX-JP
layer only where region-diff 5.2 says the PSP changed Japanese text, names
or one font tile. `en-150` from PSP-EU differs from the PSX-EU layer in
region-diff 5.3's English edits (P9 among them), in the PSX-EU release's own
edits that the PSP does not carry, and in three PSP-only cells of its font.
Two of its chunks are **not built**, each with the reason: the Config
screen's text (the PSP's screen has other rows) and label group 14 (one of
its five words is not where any rule finds it). So on a PSP-EU image **24**
of the 182 language addresses stay zero, against 8 from a PSX disc.
Both are built since: group 14 on 2026-10-13 (D247), the Config screen on
2026-10-17 (D246, section 3.3), so a PSP-EU image leaves the same 8 as a
PSX disc.

## 1. What was built

All of it in `tools/loc_build.py` (this round, LG's alone), plus one call
site in `tools/importer.py`.

- **`PspDisc`** (`open_disc`): a PSP disc read as a PlayStation one. Its EMIs
  sit under `PSP_GAME/USRDIR/<region>/` (`JPN`, `USA`), the same container
  form as the PSX's `BIN/`, with two header changes undone on read
  (`psp_emi`): bit 31 of a type-0 or type-1 RAM destination, and bit 0 of
  eight type-3 image words. Measured over every EMI of PSP-JP against PSX-JP
  (scratch `probe`, 2026-10-12): 2,683 bit-31 words, 8 image words, every
  other destination equal, and only `FIRST.EMI`'s section count differs
  (region-diff 5.1). The build, `is_japanese` and the tag come from
  `UMD_DATA.BIN`'s serial against `fixtures.toml` (`psp-jp` ULJM-05029,
  `psp-eu` ULES-00193).
- **The exe's strings from `BOOT.BIN`** (`PspDisc.boot`: the ELF's first
  program header, `.text`, `.rodata` and `.data` at address 0). The PSP's
  EMIs keep the Japanese build's overlays on both discs (`START.EMI` section 8
  of PSP-EU hashes as PSX-JP's), so a PSP-EU `START.EMI` holds Japanese
  strings: the first run stopped on them ("verb 0 holds a code English does
  not have"). The PSP's own code reads its names, verbs and labels out of
  `BOOT.BIN`; every converter now takes it as its donor on a PSP disc, with
  a pointer word there an offset past `.text` (`is_pointer`,
  `PspDisc.pointers`; relocations are applied at load, `tools/psp_elf.py`).
  Section 3 says what each found.
- **`importer.py`**: `resolve_languages(..., psp=True)` from
  `build_languages_disc_only` only, so `importer.py build --source <PSP iso>
  --lang <tag>` builds the layer; a PSX disc of the same tag still wins, and
  the PC path is unchanged (PSX donors only). Measured: `importer.py build
  --lang ja-JP` from PSP-JP and `--lang en-150` from PSP-EU write
  `loc/<tag>/` byte-identical to `loc_build.py all --cache` (254 containers
  each), and `importer.py verify` on the PSP-EU cache reports 0 problems.
- **`loc_build.py selftest`** gains the PSP rules on synthetic bytes
  (`selftest_psp`).

## 2. Measured: the two layers

```
python tools/importer.py build --source "CDImage/Breath of Fire III (PSP) (Japan).iso" --lang ja-JP --out <scratch>/i_pspjp
python tools/importer.py build --source "CDImage/Breath of Fire III (PSP) (Europe).iso" --lang en-150 --out <scratch>/i_pspeu
python tools/loc_build.py cover --cache <cache> --lang <tag>
```

| | PSP-JP `ja-JP` | PSP-EU `en-150` | PSX-EU `en-150` (reference) |
|---|---:|---:|---:|
| containers | **254** | **254** | 254 |
| `loc/zh-CN` chunks replaced (`cover`) | 381 of 475 | 381 of 475 | 381 of 475 |
| area texts, pools | 200, 44 | 200, 44 | 200, 44 |
| messages left empty (no glyph) | 0 | 496 | 496 |
| pool slots left empty | 0 | 0 | 0 (PSX-JP: 42) |
| enemy names | 448 | 448 | 448 |
| font glyphs | 3,570 | 2,723 | 2,671 |

The 496 are the PSX-EU layer's too (sixteen Japanese template messages in 31
areas, `loc-build-disc-only.md` 5.2). PSP-JP has no empty pool slot where
PSX-JP has 42: the PSP fills the battle pool's slot 94 (section 5.2).

## 3. The exe's strings from `BOOT.BIN`

`BOOT.BIN` holds the data of the modules the PSX loads as overlays (`START`,
`STATUS`, `BATE`, `SHOP`, `BATTLE`, `COMMU`, a fishing module), several in
up to four copies, as a native build: the same records and strings, but
packed at byte alignment where an overlay pads to four, some fields resized
to their English words, and a few blocks moved apart. Every anchor the image
carries was searched in it whole and without its leading and trailing NULs
(scratch `probe_anchors.py`, `probe_strip.py`); the hashed anchors whose PC
bytes include alignment padding (the battle tail, the gene head, the village
rows) match nowhere in it.

### 3.1 What each converter found (PSP-EU)

| Chunk | From `BOOT.BIN` | How | Against PSX-EU's |
|---|---|---|---|
| names (6 tables) | all, 0 kept | `convert_names` as is; the image's anchors are the PSP's own (consumable 87's flag) | section 5.3 |
| verbs | 22 | `convert_verbs` as is (its strings packed, the pointer table found) | identical |
| character names | 8 | as is | identical |
| merchant | found | as is, over `BOOT.BIN` instead of the `WORLD` EMIs | identical |
| fishing lines | 13 | the records end 9 bytes before the rows, not 12 (`gaps`) | line 4 differs (5.3) |
| fishing tabs | 3 | no code pattern: the run of text after the quads, over three (PSP-EU 4 wide) | identical |
| battle commands | 7 | the slots end one NUL before the box table (`slots_fit`) | identical |
| battle messages | 12 | as is | identical |
| labels | 15 of 16 groups | `convert_labels_psp` (3.2) | identical but group 14, absent |
| Config screen | **not built**; since 2026-10-17 25 of 29 slots, 4 ours | `convert_config_psp` (3.3) | identical (since 2026-10-17) |

`ja-JP` builds none of the exe's strings but the names, from either disc
("not built for a Japanese disc", unchanged): from PSP-JP all six name
tables, 0 kept.

The names are checked independently: `importer.py`'s `psp-names-en-150`
and `psp-names-ja-JP` layers read the same records out of `BOOT.BIN` by
`tables.toml`'s addresses, and their recorded hashes equal this layer's
records **8 of 8** and **5 of 5** (scratch `check_names.py`).

### 3.2 The labels (`convert_labels_psp`)

The PSX rule of each group (`convert_labels`) assumes the overlay's layout;
in `BOOT.BIN` six of them hold as they are and the rest moved. Each rule
below is measured on PSP-EU's `BOOT.BIN`, the only PSP build with Latin
text; a group whose rule finds nothing is reported and left out.

| Group | Rule in `BOOT.BIN` |
|---|---|
| 1 status words | the first two words after the verb sets |
| 2 menu stats | four 4-byte slots ending at the label tail's core (its 6 bytes without the 2 pad NULs): the PSX's form |
| 3 item types | the five strings whose pointer table ends where group 4's strings begin |
| 4, 5 skill types, battle stats | five strings, their five pointers and four 6-byte slots ending at a label tail's core: the PSX's form with the tail's core for its 16 bytes |
| 6 master list | the first two words after group 8's pointer table (the PC's 24-byte head is elsewhere) |
| 7, 8 sort menus, note sort | after each of the two places the sort head's core occurs: eleven strings and their table (7), or four and theirs (8) |
| 9 ink label | the word that ends just before SHOP's `NOTE` (`PSP_NOTE`, a literal like `LABEL_NOTE_HEAD`) |
| 10 formations | `convert_labels`' rule as is (records of 20, the pairs by hash) |
| 11 zenny unit | the word two before the first verb, with the full stop between (the PSX keeps them after the wheel triangle) |
| 12 gene tabs | three slots ending up to three NULs before the gene tail (6 wide) |
| 13 village lists | the one run in the image of exactly twenty 8-byte slots |
| 14 village words | **not built**: the plural / singular pairs (7-byte slots) end at the kind table and the money title follows the rectangles, as on the PSX, but the stake title is not after the money title; it follows another screen's table of words, and no rule was found |
| 15 identify words | the constant, as for every Latin disc |
| 16 faerie names | `convert_labels`' rule as is (records of 9, the stats by hash) |

Every built group is **byte-identical** to the PSX-EU layer's (scratch
`first_diff.py`), which is the check that each rule found the right words.

### 3.3 The Config screen is the PSP's own

The PC draws the PlayStation's Config screen: 6 labels, 17 options, 6
controller names (`convert_config`). `BOOT.BIN`'s is another screen: 4
labels where the PSX has 6 (Window and Background kept; Message Speed, Sound
and Autorun gone; Button Config and Delete Data new), 15 option records (the
Sound row's two gone), the same 6 controller names, and none of the
row tables `convert_config` locates the block by (`CONFIG_TABLES`; the
PSP's are shorter). So nothing maps it onto the PC's rows by rule, and the
layer has no Config chunk: `loc_build` prints "not built from a PSP disc".
The screen then draws the image's own slots, which no disc carries (section
6). Which way to go is the owner's (section 9).

**Read row by row, 2026-10-17** (D246; scratch `probe_cfg.py`, `probe2.py`
over `PspDisc.boot()`, offsets in that image, capstone over the label
routine; the PSX side from the US disc's `START.EMI` through
`convert_config`, and PSX-EU's chunk is byte-identical to the US's). The
paragraph above miscounted the labels: **the PSP has six**, not four. Four
are in `.data` before the records (`0x2EC650`..`0x2EC680`); the other two,
Msg Speed and Autorun, are not strings in the image at all - the label
routine (prologue `0xC056C`) builds them on its stack, one `ori` / `sb` pair
a byte, beside the four it copies (`jal 0x2C5620` at `0xC05FC`..`0xC067C`,
`$a1` each `.data` label). Its six-entry pointer array on the stack orders
them Msg Speed, Window Clr, Background, Autorun, Button Config., Delete
Data. The option records are 8 bytes as on the PSX (`0x2EC681`..`0x2EC6F8`),
then the row tables at `0x2EC6F9` (`00 03 07 0B 0D 0F` / `03 04 04 02 00
00`, unique in the image), then the six 21-byte controller records
(`0x2EC705`).

| PSX row | PSX label | PSX options | PSP | PSP label | PSP options |
|---|---|---|---|---|---|
| 0 | Msg Speed | Fast / Med. / Slow | present, row 0 | Msg Speed (stack) | Fast / Med. / Slow |
| 1 | Window Clr | 1 / 2 / 3 / 4 | present, row 1 | Window Clr | 1 / 2 / 3 / 4 |
| 2 | Background | 1 / 2 / 3 / 4 | present, row 2 | Background | 1 / 2 / 3 / 4 |
| 3 | Sound | Stereo / Mono | **absent** | - | - (no record) |
| 4 | Autorun | Off / On | present, row 3 | Autorun (stack) | Off / On |
| 5 | Controller | none; the panel: Speak / Move / Action / Menu / View / Change | the same panel, row 4 | **Button Config.** | none; the panel's six names the same |
| - | - | - | **added**, row 5 | **Delete Data** | none |
| unused | records 15, 16 | Manual / Auto | unused (row 4's first, count 0) | - | Manual / Auto |

So: the PSP keeps every PSX option but Stereo / Mono (15 records = the
PSX's 17 less those two, the counts and x bytes equal record for record),
renames the Controller row Button Config., and adds one row, Delete Data,
with no options. **No vibration row or word**: none of `Vibra`, `vibra`,
`Rumble`, `Analog` occurs in `BOOT.BIN`. The trailing `.` of Button Config.
is code `0x3E`, the code of Med.'s full stop.

**Built since 2026-10-17** (`convert_config_psp`; the owner's decision: fill
the slots with the words, each a one-word name of a function). The PSP's
row tables locate the block as `CONFIG_TABLES` does on the PSX; the four
`.data` labels end at the records; the two stack labels are read off the
routine whose calls pass the first and the last `.data` label
(`psp_config_code_labels`). Each of the PC's 29 slots, and its source:

| PC slots | Source |
|---|---|
| labels 0, 1, 2, 4 (Msg Speed, Window Clr, Background, Autorun) | `BOOT.BIN` (0 and 4 off the stack) |
| label 3 (Sound) | ours, `CONFIG_BUILTIN_LABELS` - the PSP has no Sound row |
| label 5 (Controller) | ours, `CONFIG_BUILTIN_LABELS` - the PSP's word for the row is Button Config., fourteen letters where DIV-0015's column was set for ten; not taken |
| options 0..10, 13..16 | `BOOT.BIN`, records 0..14, count and x verbatim |
| options 11, 12 (Stereo, Mono) | ours, `CONFIG_BUILTIN_OPTIONS`, at the count and x of options 13, 14 (`06 26`, `04 66`, the same pair on the PSX) |
| controller names 0..5 | `BOOT.BIN` |

The owner's second source, the PSX-EU layer's words, is never reachable on
this path: a PSX-EU disc among the sources builds the whole `en-150` layer
itself (`importer.resolve_languages`: a PSX disc of the tag wins), so a
PSP-EU layer is built only where none is held. It would give the same bytes:
the chunk built from PSP-EU is **byte-identical** to the PSX-EU layer's
(316 bytes; and to the PSX-US layer's), the four ours included.

**The layers before and after** (scratch `layerdiff.py`, every file, chunks
keyed by kind, tag and occurrence): `loc_build.py all --discs` over a copy
of the PC install (the five PSX tags, 1,233 overlay files; both PSP images
skipped as before) **1,233 of 1,233** identical; the disc-only PSX-US cache
254 of 254 and the PSP-JP cache 254 of 254 identical; the PSP-EU cache 253
of 254 identical, its `FIRST.DAT` gaining the one kind-7 chunk (sixth of 37,
the rest unchanged and in order).

### 3.4 The font's three cells

PSP-EU's Latin sheet (`ENDKANJI.EMI` section 0) adds three cells past the
US set, codes 0x94..0x96, in the dialogue set only (no 8 x 8 cell). Its
ending text uses them (`AREA199` slots 0 and 1). `latin_extension` counted a
cell only with ink in both sets, so the two messages went empty; it now
counts ink in the dialogue set. On all five PlayStation sheets the run ends
at a cell empty in both sets (scratch `cells.py`: US and EU at 0x94, FR at
0xAB, DE at 0xA8), so their run is the same by either rule, and the PSX
layers are unchanged (5.1). The three UI cells are blank. Over the PC
install too the rule now takes such a cell; none of the five PSX discs has
one.

## 4. `RELAID`, the stand-ins and CONFIG on the PSP's sheets

Every stand-in of the PSX build is built from both PSP discs: 16 (10
atlases, 2 kanji sheets, 4 area pages) and the 2 village sheets, `FIRST`'s
covered atlas left out as on the PSX. Where the PSP's sheets differ from the
PSX's (scratch `relaid_psp.py`, `img_diff.py`):

- **The glyph atlas** (`0x1C080200`, both PSPs): 121 pixels, all in one 16
  x 16 icon of the row the rule's strip copies from (the region-diff 5.2 "1
  tile"). The re-laid sheet differs in 242: those 121 and their copy in the
  laid strip, and nowhere else. **The rule holds**: its rectangles and strip
  still fall on the PSP's sheet as on the PSX's, and lay the PSP's own icon.
  Which icon it is was not looked at.
- **The kanji sheet** (`0x1E000200`): PSP-JP against PSX-JP, 96 pixels in
  rows 60..71, outside every rectangle (one glyph; it is also the one font
  glyph that differs, 0xA0E). PSP-EU against PSX-EU, 167 pixels, all inside
  the rule's cleared rectangles (the three new cells of 3.4), so the re-laid
  sheets are identical. **The rule holds** on both.
- **The village sheets** (`COMMU01`, `COMMU05`) are the atlas: the same 242.
- **The four area pages** are identical.
- **CONFIG from the letters**: both PSP title sheets carry NEW GAME and LOAD
  GAME in the US bands, so `build_title` cuts CONFIG from each disc's own
  sheet ("130 and 140 wide ... 0 rows down"), and the title page is
  byte-identical to the PSX layers'. The PSP's own title row (P14) is in
  rows the build does not take. **The rule holds.**

## 5. The diff, container by container

Every layer in scratch, each from one cache (`importer.py build --source
<disc>`, then `loc_build.py all --cache`); the PSX references built with the
round base's `loc_build.py` (`668a9b67`). Compared with `cmp_layers.py`
(containers, then chunks by kind), `slot_diff.py` (text and pools slot by
slot, with a third layer to name a slot), `first_diff.py` (FIRST's strings),
`img_diff.py` and `font_diff.py`.

### 5.1 The PSX and PC paths are unchanged

- **Disc-only:** this branch's `loc_build.py all --cache` against the same
  caches as the base's: PSX-EU `en-150` and PSX-JP `ja-JP` **254 / 254
  byte-identical** each.
- **Over the PC install:** `loc_build.py all --game` against two scratch
  games hard-linked to `bof3/`, the base's tree and this one, each tag's disc
  in turn (scratch `pc_path.py`): **en-US 247/247, en-150 247/247, fr-FR
  247/247, de-DE 247/247, ja-JP 245/245 byte-identical**; this tree's runs
  also pass `check_anchors` and `check_relaid` on all five discs.
- **Through the importer:** see section 5.4.

### 5.2 `ja-JP`: PSP-JP against PSX-JP

**188 of 254** containers byte-identical. Every difference:

| Containers | Chunk | What | Named by |
|---:|---|---|---|
| 42 | the battle pool (`BATTLE`, `BATTLE2`, 40 `BOSS`) | slot 94: empty in the PSX layer (its slot points past the block's end), a message on the PSP | region-diff 5.2, "the battle pool's filled slot" |
| 2 | the field pool (`FIRST`, `AFLDKWA`) | 42 slots of block 0 | region-diff 5.2, the field pool's growth (P15 candidate) |
| 9 | area text (`AREA035`, `042`, `045`, `057`, `079`, `080`, `081`, `128`, `199`) | 11 slots | region-diff 5.2's nine message edits, the same nine areas |
| 1 | `FIRST` font | 1 glyph (0xA0E) | the kanji sheet's one tile (section 4) |
| 1 | `FIRST` names | key items 2, 5, 7, 9; ability record 115 (the opt layer's 116) | region-diff 5.4 P7 and the PSP-JP renames; `psp-names-ja-JP` 5 of 5 |
| 12 | the glyph atlas, the two village sheets | 242 pixels | section 4 |
| 2 | the kanji sheet (`ENDKANJI`, `FIRST`) | 96 pixels | section 4 |

Unexplained: none.

### 5.3 `en-150`: PSP-EU against PSX-EU

**167 of 254** containers byte-identical. PSX-EU is itself the US text with
its own edits (`fixtures.toml`: 44 pools and 22 area blocks), and region-diff
5.3 measures the PSP against the US, so every differing slot is also read
against the PSX-US layer: a slot where the PSP equals the US is the PSX-EU
release's own edit, which the PSP does not carry.

| Containers | Chunk | What | Named by |
|---:|---|---|---|
| 44 | every system pool | 92 slots where the PSP has the US text (PSX-EU's edits); 110 where it differs from both; 2 slots added to the field pool (309 to 311, `FIRST` and `AFLDKWA`) | region-diff 5.3: the two item descriptions (block 1 slots 133 and 452 in all 44), the save and discard prompts and the added button legend (the field pool) |
| 34 | area text | 3 slots the US text (PSX-EU's edits); 83 differ from both | region-diff 5.3: the renames across `AREA175`..`185`, the boss name, the item word, the title, the button plural, P9 |
| 3 | area text (`AREA030`, `089`, `129`) | slots 148..255 empty: the PSP's table points them at the block's end, the PSX's at its first message (107 slots each, 321) | unused filler slots; the PSP's table, not text |
| 1 | `FIRST` names | 4 tables: the 8 PSP renames (weapon 14, armour 9, accessories 29 and 45, ability records 45, 46, 115, 217); 11 more ability records where PSX-EU corrected a spelling that the US and the PSP keep | region-diff 5.4 P7 and 5.3; `psp-names-en-150` 8 of 8; against the PSX-US layer only the 8 differ |
| 1 | `FIRST` fishing line 4 | one word | region-diff 5.3's button plural |
| 1 | `FIRST` font and advances | three cells added (2,671 to 2,723 glyphs) | section 3.4 |
| 1 | `FIRST` Config, label group 14 | absent (both built since: group 14 2026-10-13, D247; Config 2026-10-17, byte-identical to PSX-EU's, D246) | sections 3.2, 3.3 |
| 12 | the glyph atlas, the two village sheets | 242 pixels | section 4 |

Against region_diff's own pair (`region_diff.py pair psx-us=US
psp-eu=PSPEU`, scratch `rd_text.py`): its 89 text rows are 43 area blocks,
44 pools and two test modules (`MTEST`, `RTEST`, which have no PC
container); the PSP-EU layer differs from the PSX-US layer's text in
exactly those 87 containers, and in no other. Unexplained: none.

### 5.4 Through the importer

The round's merge check (`analysis/psp_round/merge_psp.sh`), and the same
with the language layer, this branch's tools against the base's (scratch
`merge_check.sh`, `refs2.sh`, `tree_diff.py`; every file but the manifest
hashed):

| Build | Files | Differ | Manifest |
|---|---:|---:|---|
| JP + PC (`bof3/DAT`, `bof3/BOF3.exe`), against the coordinator's `ref_jppc` | all | 0 | the build time only |
| US disc alone, `--lang en-US` | 2,126 | 0 | the build time only |
| European disc alone, `--lang en-150` | 2,118 | 0 | the build time only |

`recipes/` is untouched, so `exe-gaps.tsv`'s PSX rows cannot move.
`importer.py check` and `ledger_check.py` (101 entries, 0 errors) pass.

## 6. The exe image's language bytes on a PSP image

`exe_tables.py xref` with the PC's exe and four discs (scratch, `xref/`),
restricted to the two regions of `loc-build-disc-only.md` 5.4
(`0x669CCC..0x66A450`, `0x6531F4..0x6536E0`): PSX-US **182** none, PSX-EU
183, PSP-JP **182**, PSP-EU **182**, the same addresses (the one that
differs, `0x6536B0`, is reproduced on all but PSX-EU). `BOOT.BIN` fills none
of them. Which stay zero under each layer, by the disc-only round's
per-address table (its scratch `lang182_class.tsv`: A written by a handler,
B read through ours, C no handler; a handler's chunk absent leaves its
addresses zero; scratch `zero_count.py`):

| Layer | Stay zero | Which |
|---|---:|---|
| PSX-US `en-US`, PSX-EU `en-150` | **8** | the eight of 5.4 there (the check of the method) |
| PSP-EU `en-150` | **24** | those 8; the Config screen's 6 row labels (`0x669F0C`..`0x669F3C`) and 6 controller names (`0x66A338`..`0x66A360`); group 14's 4 (`0x669E10`, `0x669F08`, `0x669F60`, `0x669F68`). Since D247's and D246's fixes (2026-10-13, 2026-10-17): the same 8 as a PSX disc, by the same table (not re-run through `exe_tables.py xref`) |
| PSP-JP `ja-JP` | **177** | the same as PSX-JP `ja-JP`: a Japanese layer builds none of the exe's strings but the names, and over a disc-only image their slots are zero |

The 177 is not the PSP's: PSX-JP's disc-only layer has the same (measured
here, not in the disc-only round, which counted US). Not counted: `0x66A1F8`
(DIV-0083 draws it only under a Latin layer), and the 17 Config option
records at `0x6536F8`, outside both regions.

## 7. What SI's merge will change

The PSP caches here hold 519 of 741 `base/` containers. The layer reads
`base/` for the enemy tables only (their numbers, against the donor's); a
container not in `base/` is built from the donor's own table widened as the
importer widens it, so all 448 names are built today. When SI fills `base/`
from the PSP's sections, the enemy tables are the same source, so the
layer should not change. What would change it is SI's regeneration of
`recipes/pc-zh.toml`: the layer takes its container list, its `loc/zh-CN`
chunks and the stand-ins' sizes and slot order from it. Re-run 5.2 and 5.3
after SI merges; not done here.

## 8. Known defects (for the coordinator to number)

1. **PSP-EU `en-150`: no Config screen text.** The 12 addresses of section
   6 stay zero, so the in-game Config screen's labels and controller names
   draw blank on a PSP-only English install (and its 17 option records, not
   counted). Not a crash (the disc-only round's class B: read through ours,
   the bytes zero). **D246, fixed 2026-10-17** (section 3.3).
2. **PSP-EU `en-150`: no village words (label group 14).** The board panel's
   culture label, the ranked lists' heading word, and the hi-lo game's money
   and stake titles draw blank.
3. **PSP-JP and PSX-JP `ja-JP` disc-only: 177 language addresses stay
   zero.** Not new in this round and not the PSP's: the Japanese layer has
   never built the exe's strings (verbs, labels, battle text, Config), which
   over the PC install show the port's Chinese. A disc-only Japanese install
   shows them blank. Found while counting; the disc-only round counted US
   only.

## 9. For the owner

1. **The PSP's Config screen** (defect 1): leave a PSP-only English install's
   Config text blank; take the words that match from the PSP's screen
   (Window, Background, the options and the controller names) and leave the
   other rows blank; or draw the PC's Config rows from a built-in English
   set (ours, not the disc's). No layer change until the owner picks.
   **Decided 2026-10-17:** fill every slot with the words - the PSP's where
   its screen has one, ours for the other four (section 3.3). Open for the
   owner: label 5 is ours ("Controller") where the PSP names that row
   Button Config.; taking the PSP's word is one entry of
   `PSP_CONFIG_LABEL_OF`, and wants a look at its fourteen letters against
   the label column.
2. **Label group 16's one-late names** stay as the disc-only round left
   them: an open owner's call (`loc-build-disc-only.md` 5.4); the PSP build
   inherits it unchanged.

## 10. Not verified

- **Not played.** No PSP-only install was started; V2 and V3 of the round
  do that.
- **The label rules are fitted to one build.** PSP-EU is the only PSP build
  with Latin text, so each rule of 3.2 was found and checked on the same
  `BOOT.BIN`; the check is that every built group equals the PSX-EU layer's.
  A second Latin PSP image would test them; none is held.
- **Which words the PSP changed** was counted by slot and tied to
  region-diff 5.3's word-level classes by container, not read slot by slot
  against them.
- **The PSP's changed atlas icon** and **the three new font cells** were not
  looked at, nor whether the blank UI cells of the three are ever drawn.
- **SI's merge** (section 7).
- `0x66A1F8` under `ja-JP` and the Config option records were not classed.

## 11. Commands

```
# a PSP-only cache with its language layer
python tools/importer.py build --source "CDImage/Breath of Fire III (PSP) (Europe).iso" --lang en-150 --out <cache>
# or against an existing PSP cache
python tools/loc_build.py all --disc "CDImage/Breath of Fire III (PSP) (Japan).iso" --cache <cache> --lang ja-JP
python tools/loc_build.py cover --cache <cache> --lang en-150
python tools/loc_build.py selftest
```

The scratch scripts (`probe_anchors.py`, `probe_strip.py`, `cmp_layers.py`,
`slot_diff.py`, `first_diff.py`, `img_diff.py`, `font_diff.py`,
`relaid_psp.py`, `check_names.py`, `rd_text.py`, `zero_count.py`,
`cells.py`, `pc_path.py`) are not committed; each is a few lines over
`tools/dat.py` and `loc_build`'s own functions, and the tables above say what
each counted.

## For the other files

**Folded in 2026-10-12** (the coordinator's pass, branch `psp/ledger`): DIV-0090 and DIV-0097 amended, the font rule folded into DIV-0090's amendment; section 8's defects are **D246**, **D247** and **D248**; the README row, STATUS, HANDOFF and owner-review (the Config screen: deferred to an emulator, the owner) are in.

**DIV-0090, amend** (a PSP disc as the donor):

> **Amended 2026-10-12 (the PSP-only round, [`loc-build-psp.md`](loc-build-psp.md)):**
> a PSP disc alone builds the layer too (`importer.py build --source <PSP iso>
> --lang ja-JP` or `en-150`), its names and the exe's strings from
> `BOOT.BIN`. 254 containers from either PSP disc. Against the PSX layer of
> the same tag it differs only in the PSP's own text, names and font tile
> (sections 5.2, 5.3). Two `en-150` chunks are not built from PSP-EU: the
> Config screen's text (the PSP's screen has other rows) and label group 14;
> their 16 addresses draw blank (section 8). Over the PC install nothing
> changes.

**DIV-0097, amend** (the stand-ins on the PSP's sheets):

> **Amended 2026-10-12 (the PSP-only round, [`loc-build-psp.md`](loc-build-psp.md) 4):**
> from a PSP disc the same 16 stand-ins and 2 village sheets are the PSP's
> sections re-laid by the same rule: the rule's rectangles and strip fall on
> the PSP's sheets as on the PSX's. The PSP repainted one icon of the strip's
> source row, which the laid strip then carries (242 pixels against the PSX
> layer), and, on PSP-JP, one kanji-sheet glyph outside the rule. Not seen.

**DIV draft (no letter given; the coordinator decides whether it is its own
entry or part of DIV-0090's amendment): the font run takes dialogue-only
cells.** Original: `latin_extension` counted an accented cell past 0x93
only with ink in both the 12 px and 8 x 8 sets. New: ink in the 12 px set is
enough; PSP-EU's three cells 0x94..0x96 are appended, their UI cells blank.
Why: PSP-EU's ending text uses them; without them two messages of `AREA199`
were empty. Also in the PSX version? No PSX sheet has such a cell; every PSX
and PC-path layer is byte-identical (section 5.1). Not played.

**`docs/psp-only-build.md`** section 5 / 2.4: "A disc-only language layer
does not exist for any disc yet ... `loc_build all` has never been validated
against a PSP disc": validated 2026-10-12, `loc-build-psp.md`.

**`docs/psp-only-round.md`** section 2 row 5: done, with the two `en-150`
chunks not built.

**`docs/README.md`**: one row for `loc-build-psp.md` (text; the PSP's
language layers).

**`docs/STATUS.md` / `docs/HANDOFF.md`**: a PSP disc alone builds both
language layers; the owner's call on the PSP's Config screen (section 9);
the `ja-JP` disc-only zero slots (defect 3) as a follow-up for whoever takes
the Japanese exe strings.

**`docs/owner-review.md`**: section 9 item 1.

**Added 2026-10-17 (D246, branch `fix/psp-config-text`); numbered at the
button round's end as its own entry, DIVERGENCE DIV-0141:**

**DIV-0141 (lettered draft A here): a PSP-EU layer's Config screen, the
PSP's words and four of ours.**

- **Subsystem:** text / menu (`tools/loc_build.py` `convert_config_psp`; the
  kind-7 chunk DIV-0015 applies).
- **Tier:** Sensible - the screen's words; nothing in play moves.
- **Original behaviour:** a layer built from PSP-EU alone had no Config
  chunk (the PSP's own screen has other rows), so on a PSP-only English
  install the Config screen's six labels and six controller names drew blank
  and its seventeen records kept the image's zeros (D246).
- **New behaviour:** the chunk is built from `BOOT.BIN`'s screen (section
  3.3): 25 of the 29 slots take the PSP's word; the four its screen has no
  word for take ours, a table of four one-word names in `loc_build.py`
  (`CONFIG_BUILTIN_LABELS`: Sound, Controller; `CONFIG_BUILTIN_OPTIONS`:
  Stereo, Mono), the Sound row's two records at the Autorun row's count and
  x. The chunk is byte-identical to the PSX-EU layer's.
- **Rationale:** the owner's decision, 2026-10-17: fill the slots with the
  words, none more than a one-word name of a function; quoted labels naming
  a game function are fine in code (the owner's stance, 2026-10-10). The
  PSP's word first, the PSX-EU layer's where a PSX-EU disc is held (never on
  this path: that disc builds the layer itself), then ours.
- **Also in the PSX version?** The words are the PlayStation's (the PSX-EU
  and PSX-US screens read the same). The PSP itself has no Sound row and
  names the Controller row Button Config.
- **Reversible?** No toggle; a layer built from a PSX disc, or from the PC
  install, is unchanged.
- **Checked:** `loc_build.py selftest` (`selftest_config_psp`, synthetic);
  every other layer byte-identical before and after (section 3.3); the
  Config screen and its Controller panel on a PSP-only install, headless on
  `sigma_config.txt`: blank before, filled after and pixel-equal to the
  PSX-US install's; PSX-US and PC + `en-US` frame-equal before and after
  (D246). Not seen by the owner.
