# The exe's remaining Chinese: the definite list, and what replaces each string

**Status:** IN PROGRESS (2026-10-13, branch `away/1013-chinese`, a reading and
tooling pass - no game run, no `src/` change. The list is complete and
regenerates with `tools/text_scan.py --ledger`; the fills it proposes in
section 7 are not built)

[`HANDOFF.md`](HANDOFF.md) item 6 asked for three no-owner readings: the exe's
remaining Chinese (`tools/text_scan.py --all` finds 424 strings, 241 at
addresses `src/` names) cross-checked against DIV-0064's label tables into a
definite list; the reading parts of [`exe-import-engine.md`](exe-import-engine.md)
section 5 (the language addresses, the effect state tables' twins, a second
map of the PSP ELF's own copies); and the gaps [`loc-build-disc-only.md`](loc-build-disc-only.md)
still listed. This doc is all three. No game data is in it: addresses,
counts, the discs' own English words, and the port's Chinese only as the
labels earlier docs already name.

## 0. What came out

1. **The 424 strings, every one classed** (section 2): 141 a language overlay
   replaces, 13 real text nothing replaces, and 270 that are not text at all -
   183 pointer words, 79 bytes equal to a PlayStation disc's own data at the
   same address, 8 more data read one by one. **No string is unclassed**; the
   tool exits 1 if one ever is.
2. **What is really left is nine pieces of text** (section 3): the empty save
   slot's word, the name the battle extra copies over the whelp's, fight 26's
   turn counter, the hi-lo game's three choices, the community's seven member
   names, the parts menu's title, a one-glyph count word, and two six-glyph
   sets drawn as quads - plus faerie record 0's name, which group 16 misses by
   one record (a known defect, re-confirmed here by disassembly). For four of
   them the US disc's own string is found: **`Beyd`** (`BATE.EMI`), **`turns
   left`** (`BOSS025.EMI` / `BOSS027.EMI`), **`HIGH` / `LOW` / `SET`**
   (`COMMU02.EMI`), and the faerie name (`COMMU00.EMI`, group 16's own source).
3. **A finding against DIV-0064 group 15** (section 4.1): the Identify panel's
   overlay is `MAGIC060.EMI`, and on every PlayStation disc its draw routine
   names `WEAK` and `POINT` (small font, the heading row) and `ITEM` - the
   French disc `OBJET`, the German `OBJEKT` - where group 15 rests on "no disc
   carries a word for them" and sends spaces. Under the French and German
   overlays the port draws the English `ITEM`.
4. **`exe-import-engine.md` 5** (section 5): the language addresses are **182**
   on every build today (119 in the string pool, 63 in the community block),
   every one accounted for by a handler or listed in section 3;
   `exe_tables.group_of` now classes by region (item 6, built); the "effect
   state tables" are numbers beside 54 code-pointer tables whose 216 pointers
   have 4 PSX twins between them - nothing placeable until the function twins
   exist; the PSP ELF's second map would newly place **187 bytes** on PSP-JP
   (191 on PSP-EU) - measured, recommended against.
5. **`loc-build-disc-only.md`'s gaps** (section 6): measured on four
   disc-only caches built today.

## 1. The method

```
python tools/text_scan.py <out> --all --ledger --game bof3 --analysis analysis \
    --disc "CDImage/Breath of Fire III (Japan).cue" --disc "CDImage/Breath of Fire III (USA).cue" \
    --disc "CDImage/Breath of Fire III (Europe).cue" --disc "CDImage/Breath of Fire III (France).cue" \
    --disc "CDImage/Breath of Fire III (Germany).cue"            [--singles] [--no-sheets]
```

The scan is the one of [`village-text-scan.md`](village-text-scan.md): every
NUL-ended run of glyph codes with at least one pair code that a `.data`
pointer word or a `.text` immediate reaches, two glyphs or more (`--singles`:
one or more). `--ledger` adds, per string:

- **its readers**: the function holding each `.text` referrer (`symbols.toml`'s
  starts, else `analysis/pc_funcs.json` / `pc_hidden.json`'s); for a `.data`
  referrer, the walk back through the table that holds it to the first word
  code reaches, stopping at a code pointer (another table's). A heuristic: the
  battle stats' table `0x64AE08` is read from no base the walk finds and is
  listed with no reader;
- **the `src/` files that name its address** (a 6-digit hex constant);
- **one class**, in this order: `left` (the `LEFT` table, section 3), `cover`
  (a language overlay writes or repoints it: kind 15's groups parsed out of
  `src/game/labels.cpp`'s `kTables`, kind 5's six name tables from
  `name_tables.cpp` over `symbols.toml`'s `NameTable_*`, and `COVER` - kinds 7
  to 12, 14, 16 and DIV-0083 - each row carrying a constant its source must
  still hold, or the run stops), `pointer` (the bytes and the NUL are an
  address in the image), `disc` (byte-equal to a disc's own image of `.data`
  at that address, `exe_tables.disc_image` with the places), `data` (the
  `NOT_TEXT` table), else `NEW` and exit 1.

It writes `<out>/ledger.tsv` (address, bytes, glyphs, class, key or reading,
readers, `src/` files, referrers) and the sheets of the old scan. Run
2026-10-13 at `d0d9162a`: `analysis/`-style output in the session's scratch
(`scan/`, `scan1/` with `--singles`).

## 2. The counts

| Class | 424 strings | of them `src/` names | `--singles`: 504 |
|---|---:|---:|---:|
| `cover` - an overlay replaces it | 141 | 115 | 149 |
| `left` - real text nothing replaces | 13 | 4 | 29 |
| `pointer` - an address word | 183 | 100 | 183 |
| `disc` - a disc's own bytes at that address | 79 (JP 78, EU 1) | 19 | 127 |
| `data` - read and recorded | 8 | 3 | 16 |
| `NEW` | **0** | | **0** |

So of the 241 strings at addresses `src/` names, 115 are covered, 4 are left,
and 122 are not text. The 183 pointer words are mostly little-endian code
addresses `0x4xxx80` (`80 xx 4x 00` reads as a pair code and a letter) at
the heads of the effect and boss jump tables; the 79 `disc` strings are area,
boss and effect data the JP disc carries byte for byte (map cells, animation
byte tables, the `Im HP D LG` record table of `EffectKind18Sub39_Draw`, which is
letters, not words). The eight `data` rows: `BossGarr_Enter`'s byte table
`0x64D158` (the shape of the other bosses' tables, which are `disc`), a run of
`0x80` shade bytes before `EffectKind18Sub59`'s code pointers `0x65EF0C..`
(six overlapping starts), and the C runtime's code-page 932 ranges
`0x675310..` (`0x8260..0x8279` the full-width A..Z, `0xA6..0xDF`, the
lead-byte ranges `0x81..0x9F`, `0xE0..0xFC`), read at `0x5C13CE`.

**The 141 covered, by key** (languages: "Latin" = a layer from a Western
PlayStation disc - en-US, en-150, fr-FR, de-DE; `loc_build.py build_all`
builds kinds 7..12, 15 and 16 from no Japanese disc and, from a PSP disc, no
Config and kind 15 by its own reader):

| Key | Strings | Languages |
|---|---:|---|
| kind 5 item / ability names (DIV-0008) - `0x656FF8` is consumable 56's name field, read by `FieldPanel_DrawKindRow`; `0x65D008` an ability's, read by `Boss07_Event` for its banner | 2 | all |
| kind 7 Config: row labels, controller names (DIV-0015, DIV-0026) | 12 | Latin |
| kind 8 menu verbs (DIV-0018) | 22 | Latin |
| kind 9 battle commands (DIV-0019) | 7 | Latin |
| kind 10 default names, the whelp's slot `0x669CE0`; kind 11 Manillo (DIV-0020) | 3 | Latin |
| kind 12 battle banner messages (DIV-0052) | 12 | Latin |
| kind 14 F9 pause lines (DIV-0038, the port's own) | 4 | all |
| kind 15 groups 1..10, 12..15 (DIV-0064) | 63 | Latin |
| kind 16 fishing lines and tabs (DIV-0069) | 14 | Latin |
| `0x66A1F8` not drawn under a Latin overlay (DIV-0083) | 1 | Latin |

So under a Japanese layer 135 of the 141 still draw the port's Chinese - the
exe-side strings of DIV-0056's open end ([`dialogue-localisation.md`](dialogue-localisation.md)
6 item 6).

## 3. The definite list: real text nothing replaces

| Address | What it is | Read by | An overlay? | What the discs have |
|---|---|---|---|---|
| `0x653200` | faerie trait record 0's name field (16 bytes, then the stats at `+0x10`) | `CommuSim_AddRecord`, `lea esi, [ecx*4 + 0x653200]` at `0x455FB3`, five bytes copied | group 16 means to, and misses: it writes name r at `0x653214 + 20 r`, record r + 1's field | US `COMMU00.EMI` record 0's name (group 16's own source). The defect is [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4's, on [`owner-review.md`](owner-review.md); re-confirmed here: the bars read the stats at `0x653210 + 20 k` (`movzx ax, byte [esi + 0x653210]`, `0x45E941`) for the same k the birth reads the name at `0x653200 + 20 k` |
| `0x669CCC` | the empty save slot's word | `Menu_DrawSaveSlot` | none | no word: the US slot is blank (the staged localisation PR, HANDOFF item 5; D254) |
| `0x669CE8` | the 5-byte name `BattleExtra_EquipOpen` copies over character record 7's; `EquipLeave` restores the whelp's from `0x669CE0` (kind 10) | `BattleExtra_EquipOpen` `0x42DC20` | none | **`Beyd`**: US `BATE.EMI` `0x801D86F8`, right after `Whelp` at `0x801D86F0` (code `0x801D0F34` and `0x801D1608` load them); the European disc the same place, the French at `0x801D8710` / `0x801D8708`, the German at `0x801D8740` / `0x801D8738` (all `Whelp` / `Beyd`) |
| `0x669D10`, `0x669D18` | fight 26's turn counter: a word before the count and a word after | `Boss26Fx_DrawCount` `0x43C780` through `0x669D20` / `0x669D24` | none | **`turns left`**: US `BOSS025.EMI` and `BOSS027.EMI`, section at `0x800C1800`, string `0x800C32A8`; the routine draws `%2d` (`0x800C184C`, code `0x800C30B0`) then `turns left` (code `0x800C30F0`) - one word after the count, none before. The European disc adds the singular `turn  left` (`0x800C32E0`, code `0x800C3108`); the French has `Rest   tours` / `Rest   tour` (the count between the words, by the spaces) |
| `0x669F44`, `0x669F48`, `0x669F4C` | the hi-lo game's three choices (one glyph, one glyph, two) | `CommuHiLo_DrawChoice` `0x45B1E0` through `0x669F54`, `Text_DrawAt` count 2 | none | **`HIGH` `LOW` `SET`**: US `COMMU02.EMI` `0x801DAC58`, three 4-byte slots (`HIGH` with no NUL) right before the 72 bytes of sprite rectangles the PC has at `0x652D04` (group 14 found `Cash` / `Pot` right after them); the French disc `Haut` `Bas` `Mise`, the German `Hoch` `Tief` `Setz`, in the same three slots. The count 2 would cut each word |
| `0x669F70..0x669F9C` | the community's seven member names (one is one glyph) | `CommuName_MemberEntryOut` through `0x669FA4` | none | read only by the renamer the port cut (DIV-0075). No such table: `Nina` is in no `COMMU*.EMI` of the US disc (a search of every EMI: `START`, `STATUS` - DIV-0020's records - and two area texts) |
| `0x66A094` | a one-glyph word after a count | `EffectKind55_DrawCount` `0x47EC30` | none | not looked for; no route enters it |
| `0x66A098` | the parts menu's title (ten glyphs) | `EffectKind5E_DrawMenu` `0x47F9E0` (`Crt_sprintf` `%s`) | none | **not found**: no string any US code references names parts or Momo (a lui / addiu scan of every EMI: `GAME.EMI`'s `Part A..G` are the items); the reader has no PSX twin |
| `0x66A380..0x66A394` | six element glyphs, one each | `Identify_DrawElements` `0x4B1520` through `0x66A398`: 12 x 12 quads by glyph code, not a text draw | none - and no letter slot, so a Latin font does not repaint them | not read: `MAGIC060.EMI`'s element draw, beside the `WEAK` / `POINT` draw of section 4.1 |
| `0x66A3B0..0x66A3C4` | six glyphs, one each | `Magic080_DrawText` `0x4FC420` through `0x66A3C8`, quads by glyph code | none | not read |

The last three rows and `0x669F44` / `0x669F48` / `0x669F70` / `0x66A094` are
one glyph each: they appear with `--singles` (the 16 extra `left` rows there).

## 4. Findings on the way

### 4.1 The Identify panel draws `WEAK` `POINT` and `ITEM` on the PlayStation (DIV-0064 group 15)

`symbols.toml` places the Identify children in `MAGIC060` (`IdentifyChild_Task`:
"MAGIC060 (row 39, Identify read one id down)"); DIV-0064's group 15 looked
in `MAGIC059.EMI` and `BATTLE.EMI`, found no word, and on the owner's wiki
capture of the US panel (a small `ITEM` label, no headings) sends a space
for 弱点 and 持有物. `MAGIC060.EMI` on every held PlayStation disc:

| Disc | Strings the routine names (lui / addiu scan) |
|---|---|
| JP | `WEAK`, `EXP`, `ITEM`, `POINT` |
| US, EU | `WEAK`, `EXP`, `ITEM`, `POINT` (`0x801F0B64..`; the run also holds the zenny code `0x60` and `GET`) |
| FR | `WEAK`, `EXP`, `OBJET`, `POINT` |
| DE | `WEAK`, `EP`, `OBJEKT`, `POINT` |

Read in the US routine (capstone, `0x801EF180..0x801EF2C0`): a call
`0x801EF76C(WEAK, 0x5A, 0x44)`, then `0x801EF76C(POINT, 0x80, 0x44)`, then
the EXP and zenny lines through `0x801EF590`, then `0x801EF76C(ITEM, 0x5A,
0x8A)` - no branch between them. `0x801EF76C` draws `ITEM`, which the owner's
capture shows in the small font, so it is the small draw (an inference), and
`WEAK POINT` is the panel's small heading on the weakness row. What the owner's
capture showed there was not re-read; the capture is not in the tree.

Two consequences, for the coordinator to number and the owner to see:
- **Group 15's premise does not hold**: the PlayStation has a heading for
  the weakness row, `WEAK POINT` in the small font (all five discs keep it in
  English). Whether to draw it is the owner's call (an amendment to DIV-0064).
- **A defect of the French and German layers**: the item label the port
  draws under any Latin layer is `0x65AAB4`, the PC's own English `ITEM`
  (`magic_s12.cpp` `kItemWord`); the French and German discs have `OBJET` and
  `OBJEKT` there.

### 4.2 Where the remaining strings live

Twelve of the thirteen `left` strings of the default scan (all but faerie
record 0's name) and all the covered ones but the name tables, the default
names and the formation records are in one pool,
`0x669CCC..0x66A450`, the port's: no disc maps any of it (`exe_maps/psx-us.tsv`:
the segment before ends at `0x669CCC`, `GAME.EMI`; the next begins at
`0x66A450`, the boot EXE). Each disc's string is therefore beside its reader's
data in that reader's own overlay - `BATE`, `BOSS025` / `027`, `COMMU02`,
`MAGIC060` above - and is found by the anchor the PC still shares (the 72
bytes at `0x652D04`, the `Whelp` slot's twin) or by the overlay's own code
(the lui / addiu scan), as DIV-0064's groups were.

## 5. `exe-import-engine.md` section 5: the reading parts

### 5.1 The language addresses (item 5's first half)

`exe_tables.py xref` at today's tip with the JP, US, PSP-JP and PSP-EU discs
(`symbol` / `raw` counts, `none` + `partly`): the two language regions hold
**182 uncarried addresses on every build** - 119 in the string pool
`0x669CCC..0x66A450`, 63 in the community block `0x6531F4..0x6536E0`
(section 3.2's 183 was JP's of 2026-10-10; [`loc-build-disc-only.md`](loc-build-disc-only.md)
5.4 counted 182 on US). Joined with the ledger's tables (a session script,
`lang_none.py`), US:

| What fills it under a language layer | Addresses |
|---|---:|
| kind 15 groups (16's sixty slots counted whole: the area-limit table `0x6536B0..` is its "unnamed" sixtieth, never written) | 126 |
| kind 16 fishing 15, kind 12 banner messages 12, kind 7 Config 12, kind 14 pause 4, kinds 8 / 9 / 10 / 11 five, DIV-0083 one | 49 |
| section 3's `left` rows (`0x653200`, `0x669CCC`, `0x669CE8`, `0x66A094`, `0x66A098`) | 5 |
| nothing, not text: `0x6531F4` (the `%d/%d` format, the engine's own) and `0x653210` (the faerie stats, game values no chunk kind carries - D264) | 2 |

That is 5.4's reading again, at today's tip: no language address is
unaccounted for, and the eight that stay zero on a disc-only image are the
five `left` rows, the format, the stats and the area limits.

### 5.2 `group_of` by region (item 6, built)

`exe_tables.group_of` now asks where an address lies before its symbol's
name: the two language regions, and a raw address named after a
`States` / `Steps` / `Kinds` / `Handlers` table. `xref` on psx-jp, `none`:

| Group | Before | After |
|---|---:|---:|
| language: the exe's string pool | - | 119 |
| language: the community block | - | 63 |
| language: text, names, formats (by name) | 76 | 4 |
| beside a code-pointer table (want function twins) | - | 85 |
| mixed: within 16 bytes of a pointer word | 91 | 18 |
| other | 123 | 5 |
| small, platform, re-laid | 89 | 85 |

The printed groups only; nothing `xref` writes into a recipe changes
(`exe_tables.py check`: 0 errors).

### 5.3 The effect state tables' twins (item 5's second half)

What `src/` reads there is not the code pointers but the numbers beside
them: every uncarried address named after a state / step / kind table is a
raw `+offset` past the table's words (a session script, `codetabs2.py`, over
the xref and `symbols.toml`). Fifty-four tables; before the uncarried
addresses they hold **216 code pointers, of which 4 point at a function with
a `psx` twin** (`EffectKind0F_Child5Steps` 2, `GameMode3_Steps` 2). So 3.3's
conclusion stands and is sharper: the method needs the effect functions'
PSX twins, almost none of which exist; no `exe-places.tsv` row can be keyed
by them today. Candidate rows: none. The largest by uncarried count:
`BattleMenuWin_EquipItemsSteps` (8; 4 code pointers), `EffectKind0F_Child5Steps`
(6; 3), `EffectKind18Sub5D_States` (5; 5), `EffectKind18Sub3B_States` (4; 5),
`CommuName_MemberEntrySteps` (4; 5). Five bases hold no code pointer at all
before the address (`Fish_Kinds`, `Area118_SpawnKinds`, `Area26_EffectKinds`,
`AirRaidImage_ShadeSteps`, `Scena04_TremorSteps`): their uncarried address is
the table's own first word, a number - the name rule over-reaches there.

### 5.4 A second map of the PSP ELF's own copies (item 7)

Measured on both PSP discs (session scripts `elfcopy.py`, `elfptr.py`; the
map rows and `exe-places.tsv` as `disc_image` applies them). Of the PSP-JP
image's data-pointer words:

| Word placed from | Its value points into | Its PC target is | Words |
|---|---|---|---:|
| the ELF | the ELF | placed from the ELF | 2,404 |
| the ELF | the ELF | **placed from an EMI** | **1,194** |
| the ELF | the ELF | unplaced | 287 |
| an EMI | an EMI section | placed from an EMI / from the ELF / unplaced | 2,155 / 1,451 / 99 |
| not placed | | | 2,333 |

So the ELF's own pointers point at the ELF's copies of 1,194 targets the map
places from an EMI: the rule maps those targets forward to the EMI address
and refuses the word. A second map would pair them. Grown from the pointer
pairs whose first 16 bytes agree (595 seeds) into byte-agreeing stretches of
16 bytes or more: **176 candidate segments, 6,924 bytes; they would newly
place 187 bytes (76 nonzero)** of the 95,109 the PSP-JP image leaves
unplaced, and hold the targets of 202 of the 1,481 ELF-valued words. PSP-EU:
181 segments, 7,032 bytes, 191 new (76 nonzero), 208 of 1,501. Conversely,
of the map's EMI segments with a 16-byte pointer-free key, 20 have an ELF
copy (526 bytes), 777 have none.

**Recommendation: not worth building.** Since section 7 every pointer word
is filled from `recipes/exe-layout.tsv`, so the rebuild the second map would
help is no longer needed for the image, and the bytes it adds are 76. The
candidate rows (`pc`, `pc_end`, `BOOT.BIN`, ELF address) are in the session's
scratch (`elfsegs_psp-jp.tsv`, `elfsegs_psp-eu.tsv`); they are agent TWINS'
to take or drop (`tools/psp_exe.py` is theirs today; it was not touched).

## 6. `loc-build-disc-only.md`'s gaps, at today's tip

FILLED BY THE CACHE RUN BELOW.

## 7. What would fill the rest (for the coordinator)

Each is a `src/` change and an overlay change, so a ledger entry and the
owner's eye; none is built here.

- **A new label group for `0x669CE8`** from `BATE.EMI`'s `Beyd`, anchored on
  the `Whelp` slot DIV-0020 already finds; written in place (5 bytes copied:
  `Beyd` fits).
- **Fight 26's counter**: `turns left` after the count and nothing before
  (the European singular, the French count between its words) - the count's
  x and the first word's draw change with the language, so a layout entry
  with the strings (`BOSS025.EMI` / `BOSS027.EMI`).
- **The hi-lo choices**: the three 4-byte slots before the 72 bytes at
  `0x652D04` in `COMMU02.EMI`, repointed into 16-byte buffers (the four-letter
  words carry no NUL), the draw's count 2 widened as group 14's are
  (`Labels_Written`).
- **Group 15 revisited** (4.1): `WEAK POINT` small on the weakness row, and the
  disc's own item word (`OBJET`, `OBJEKT`) instead of the PC's `ITEM` under
  French and German - the second a defect to number.
- **Group 16's one-record shift** (already on `owner-review.md`): slots at
  `0x653200 + 20 r`; it changes the PC path's output.
- **Left open:** the parts menu's title (find `EffectKind5E`'s PSX twin), the
  one-glyph `0x66A094`, the element and `Magic080` glyph sets (both drawn as
  quads by glyph code: a disc-only image's blank Chinese glyphs leave them
  blank, a PC-built Latin layer leaves them Chinese), the empty save slot
  (the staged PR), the member names (I34).

## 8. Commands

```
python tools/text_scan.py <out> --all --ledger --game bof3 --analysis analysis --disc <each PSX cue> [--singles]
python tools/exe_tables.py xref --game bof3 --out <dir> --disc <JP cue> --disc <US cue> --disc <PSP-JP iso> --disc <PSP-EU iso>
python tools/exe_tables.py check
python tools/importer.py build --source <cue> --lang <tag> --out <cache>; python tools/loc_build.py cover --cache <cache> --lang <tag>
```

The disc reads of sections 3 and 4 were session scripts over `psx_disc.Disc`
and `exe_twins.emi_sections`: a hex dump around a pattern in a file's
sections, a lui / addiu (or ori) pair scan of every EMI's code for addresses
that land on a NUL-ended printable string, and capstone MIPS32 over one
routine; with sections 5.1..5.4's `lang_none.py`, `codetabs2.py`,
`elfcopy.py` and `elfptr.py`, none committed (each a few dozen lines over the
tools named).
