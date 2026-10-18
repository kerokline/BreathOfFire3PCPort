# The install-gap round, group LA: the language and layout rows

**Status:** MEASURED (2026-10-18, branch `install-gap/la` off `06a209bd`): the
rows read off all seven discs' code, the builder and the DLL changed, the
checks in section 6; captures in section 7. Two ledger drafts and the
defects' new statuses are in section 5, lettered, for the coordinator to
number. Nothing regenerated is committed.

Group LA of [`install-gap-round.md`](install-gap-round.md) 2: the P3 rows of
[`install-gap-census.md`](install-gap-census.md) 4 that are a disc's words
rather than game values. Each is a slot the PC's `.data` holds a string in;
a disc-built image has no bytes there because the disc lays its own words
out elsewhere or at other widths. The fix is the language layer's shape
throughout (DIV-0064's label groups, DIV-0069's fishing chunk): the words
come from the player's disc at build time, read where the disc's own code
reads them, never typed into `src/` (the owner's stance, 2026-10-10).
Addresses, sizes, counts, symbol and file names only (CLAUDE.md rule 1).

## 0. What came out

| Row (census 4) | Builds | Status now | By |
|---|---|---|---|
| `EffectKind0F_PanelTitles` `0x653EA0..0x653EB4` (D286) | FR, DE | **FIXED under the layer** | kind 16 tag 3 (`fishing_headings`), drawn by ours centred (section 1) |
| `Equip_SlotIcons` labels `0x663668..0x66367C` | FR, DE | **FIXED under the layer** | label group 21 (section 2.2) |
| `Equip_SlotIcons` `0x663660..0x663668` (the slot-icon table before the labels) | DE | **handed to TW**: a game value, verbatim in DE's `START.EMI` / `STATUS.EMI` | section 2.2 |
| `SharedList_SortSteps+0xC` `0x664288..0x664290` | FR, DE | **FIXED under the layer** | label group 22 |
| `SharedList_SortSteps+0xC` `0x664290..0x664298` (`SKILL`) | FR, DE | **HARMLESS**: nothing in `BOF3.exe` reads it | section 2.3 |
| `MenuList_GeneWinSteps+0x25` / `+0x27` `0x66B1F1..0x66B1F8` | FR, DE | **FIXED under the layer** | label group 23 |
| `CommuRank_Modes+0x8C` `0x653200..0x653210` (D289) | all 7 | **FIXED** on every Latin build; JP builds D280's | group 16's slots (section 3) |
| `Encounter_SlotChance+0x38` `0x669CE8..0x669CED` | all 7 | **FIXED since DIV-0118** on the four Western PSX builds (the census missed it); **now PSP-EU**; JP builds D280's | group 17, PSP-EU from `BOOT.BIN` (section 4.1) |
| `CommuName_RecordNames+0xF4` `0x66A098..0x66A0AC` | all 7 | **FIXED under the layer** (Western PSX, PSP-EU); JP builds D280's | `EffectKind5E`'s twin found: `SCENA10.EMI`; label group 20 (section 4.2) |
| `CommuName_RecordNames+0xF0` `0x66A094..0x66A096` | all 7 | **NEEDS FIX stays, no Western source**: the Western routine draws no word; JP's word is in `AREA008.EMI` (D280's) | section 4.3 |

"Under the layer": the image's bytes stay zero on a disc-built image; the
overlay every disc-only install builds from its own disc writes the slot
(in place, groups 16 and 20..23) or our draw reads its buffer (groups 17,
fishing tag 3) when the language is loaded. On the PC install the same
chunks replace the PC's English (or, for groups 16 and 17, Chinese) words
under that layer. The JP builds (`psx-jp`, `psp-jp`) build no label groups
under `ja-JP` but group 15 (`loc_build.py`: "not built for a Japanese
disc"): every row above that is "D280's" on them is that owner's call
(the exe's strings under `ja-JP`), not attempted here.

## 1. D286: the fishing equip headings on FR and DE

The PC draws four fixed fields: `0x653EA0` (count 3, at `x + 0x35`),
`0x653EA4` (4, `x + 0x2F`), `0x653EA8` (5, `x + 0x2A`), `0x653EAD` (5,
`x + 0x2A`, row `y + 0x47`), in `EffectKind0F_DrawItemsB`, `_DrawItemsA`
and `_DrawEquipped` (ours, `effect_1b.cpp`). D286's remainder
([`fishing-rule-pages.md`](fishing-rule-pages.md) 4): the French and German
fishing modules lay their words out after the tabs at their own widths, so
no byte-equal run exists.

**The module's widths, read off its code** (capstone, scratch `headrefs.py`):
each heading is drawn by a `lui` / `addiu` of its address with
`addiu $a3, $zero, n` (the characters drawn) and `addiu $a1, r, y` (the row)
beside it.

| Build | Addresses (past the tabs) | Counts | x | Rows |
|---|---|---|---|---|
| psx-us (`AREA030.EMI#4`) | `+0xC`, `+0x10`, `+0x14`, `+0x19` | 3, 4, 5, 5 | `+0x3A`, `+0x37`, `+0x33`, `+0x33` | 7, 7, 7, `0x47` |
| psx-fr | `+0x24`, `+0x2C`, `+0x34`, `+0x3B` | 5, 5, 7, 7 | `+0x32`, `+0x33`, `+0x2B`, `+0x33` | 7, 7, 7, `0x47` |
| psx-de | `+0xC`, `+0x10`, `+0x18`, `+0x1F` | 4, 5, 7, 6 | `+0x36`, `+0x33`, `+0x2B`, `+0x2F` | 7, 7, 7, `0x47` |

The readers (US `0x801D81E4`, `0x801D8878`, `0x801D8948`, `0x801D8CEC`; FR
`0x801D8464`, `0x801D8AF8`, `0x801D8BC8`, `0x801D8F6C`; DE `0x801D81B0`,
`0x801D8844`, `0x801D8914`, `0x801D8CB8`) are the twins of the PC's three
draws: the scroll mark's call (`0x80`, `0x1C`, `0x72`) before the first two,
the row `0x47` the fourth's, as the PC's.

**The fix** (DIV-0069's way, ledger draft DIV-A):

- `loc_build.py` `fishing_headings`: the four refs past the tabs with a
  count and a row nearest them, exactly four, rows `7, 7, 7, 0x47`, in
  address order (the PC's order); each word the disc's bytes to its count or
  NUL. Chunk kind 16 tag 3. Found on US, EU, FR, DE; not on PSP-EU (its
  `BOOT.BIN` code is the PSP's own; its image has the PC's words by the
  `twin` row) nor JP (no fishing chunk for a Japanese disc).
- `fishing_text.cpp`: tag 3 into four 16-byte buffers of ours; nothing in
  `.data` is re-aimed. `FishingText_Heading(i)` gives a buffer once armed
  under a Latin overlay.
- `effect_1b.cpp` `DrawHeading`: with a heading given, the disc's word drawn
  whole, centred on the original's centre `x + 6 * count` by its real width
  (`TextAdvance_Width`); for a 12-unit word of `count` glyphs that is the
  original's x. The US module's own x for its 8-unit words put the centre
  within a unit of it (`0x3A + 12 = 0x35 + 18 - 1`). Without one, the
  original draw.

## 2. The field menu's words (groups 21, 22, 23)

All three are slots of the PC's English the French and German discs re-word.
Every slot is written in place and witnessed by the operand that names it
(`pc_xref.json`, and a scan of `BOF3.exe` for the dword: each address has
exactly the readers listed, no pointer to it in `.data`).

### 2.1 What each disc holds

| PC slot | Reader (operand) | US | FR | DE | Where on the disc |
|---|---|---|---|---|---|
| `0x663668` (8) | `Menu_DrawEquipPanel` `0x573A80` (`push` `0x573A9F`) | the PC's | the PC's | re-worded (4 letters) | `START.EMI`, right after the label tail groups 1 and 2 already anchor on |
| `0x663670` (4) | `Menu_DrawExpPanel` `0x573BF0` (`mov eax` `0x573C2E`) | the PC's | the PC's | the PC's | the same, next slot |
| `0x663674` (8) | the same (`mov eax` `0x573C3C`) | the PC's | re-worded (5) | re-worded (4) | the same, next slot |
| `0x664288` (8) | `SharedList_DrawList` `0x585090` (`push` `0x5852A4`, `0x5852AE`) | the PC's | re-worded (5) | re-worded (5) | `SHOP.EMI`, the slot before group 9's `SKILL` slot |
| `0x66B1F0` (8) | `MenuList_GeneWinDraw` `0x59AE00` (`push` `0x59B046`, `0x59B050`) | the PC's | re-worded (4, an accent) | re-worded (3) | `STATUS.EMI`, after the 16 bytes the PC has at `0x66B1E0`; its twin reads it (US `0x801EB250`, FR `0x801EB09C`, DE `0x801EB108`, a call with the title in `$a0`, the PC's `Text_CharCount` for its centring) |

Decided row by row: a layer entry for each, not the module's widths. Every
reader draws to the NUL (count `0xFF`) or centres by `Text_CharCount`, so
the disc's word needs no layout of ours, and each fits the PC's slot.

### 2.2 `Equip_SlotIcons` on DE: two rows in one

The DE gap `0x663660..0x663677` is two things. `0x663668..` are the labels
above (group 21). `0x663660..0x663668` is not text: the 8 bytes
`Menu_DrawEquipPanel` reads as its six slot icons (`mov ebp, 0x663660` at
`0x573B79`), the "label tail" groups 1 and 2 anchor on. The DE disc holds
them verbatim in `START.EMI` and `STATUS.EMI` right before its labels; the
map's segment stopped there because the next bytes are German, and 8 bytes
is under the 16-byte window. **A `twin` row for TW** (stage B): `0x663660`,
8 bytes, DE, `ETC/START.EMI` (or `STATUS.EMI`) where the head and the
status words are. Until then a DE disc-only install draws the equipment
panel's six slot icons from zeros (not captured).

### 2.3 `SKILL` at `0x664290`: nothing reads it

`0x664290..0x664298` (`SKILL`, 5 bytes) has no reader in `BOF3.exe`: no
operand in `pc_xref.json` and no dword `0x664290` anywhere in the file. The
French and German discs hold the same word in the slot. HARMLESS; the row
stays a gap only because the map's segment stopped at `0x664288`.

## 3. D289: group 16's one-record name shift

**Established again** (2026-10-18): the PC's trait record is the name
first. `CommuSim_AddRecord` copies five bytes from `0x653200 + 20 r`
(`rest_4a.cpp` `kRecordTraits`), the name entry's default reads `0x653200 +
20 slot` (`rest_4d.cpp` `kSlotDefaults`), the ticks and bars read the stats
at `0x653210 + 20 r`. All sixty PC name fields hold a glyph string (first
byte `>= 0x80`), so the "unnamed sixtieth record" was record 59's stats read
as a name, and the slot labels.cpp wrote it to was `0x6536B0`, the area
limit table.

**The fix** (ledger draft DIV-B): `labels.cpp` group 16's sixty slots are
`0x653200 + 20 r`; `loc_build.py` measures the name fields at
`FAERIE_NAMES` (`0x653200`), `FAERIE_UNNAMED` is `()` (checked against the
PC by `loc_build.py anchors`), so all sixty disc names are sent. The disc's
record r is the PC's record r (the stats anchor matches all sixty in
order). An overlay built before the fix sends record 59 empty: under the
new DLL its fifty-nine names land in their own records and 59 keeps the
shipped name.

## 4. The string pool's three language-zero rows

### 4.1 `0x669CE8`, the guest's name (group 17)

DIV-0118 (2026-10-13) built group 17 from `BATE.EMI` (`Beyd`, by the copy
of 5 two calls after the sound `0x102`); `loc_build.py all` reports
`guest name 1` on US, EU, FR and DE. The census (2026-10-17) still lists the
row as NEEDS FIX: the image byte stays zero by design (our
`BattleExtra_EquipOpen` reads the overlay's buffer). **PSP-EU** had no
group 17 ("BATE's PSP copy is the Japanese build's"); its own code is in
`BOOT.BIN`: the same copy of 5 at `0x200DA8`, the sound `0x102` two calls
before, the name right after `Whelp` - the "label group anchored on the
whelp slot" the census asked for, by the twin's code. `guest_name` takes
`BOOT.BIN`'s code and reads the string from the whole file. JP's name is in
JP codes (D280).

### 4.2 `0x66A098`, kind 0x5E's menu title (group 20)

**`EffectKind5E`'s PSX twin is in `SCENARIO/SCENA10.EMI`** (found by
`EffectKind5B_States`' one window on the disc, `0x801FE7CC`, which its
readers `EffectKind5D_Show` / `5E_DrawMenu` / `5E_Wait` share): US
`0x801F7DF0..0x801F7E7C` draws the box, prints the title through `"%s"`
(`0x801F7E40`, the title at `0x801FE874`) and draws it with count 12 at the
box's x + 8 - the PC's `EffectKind5E_DrawMenu` `0x47F9E0` with the PC's
count `0xF` at x + 4. The same routine on every build:

| Build | Title | Count | x |
|---|---|---|---|
| psx-us, eu-en | 12 bytes, no NUL (the state table follows) | 12 | +8 |
| psx-fr | 17 letters and a NUL | 17 | +8 |
| psx-de | 12 bytes, no NUL | 12 | +8 |
| psx-jp, psp-jp (`SCENA10.EMI`) | 16 bytes of JP codes | 15 | +4 |
| psp-eu `BOOT.BIN` `0x41938` | 12 bytes, no NUL | 12 (`ori`) | +8 |

`loc_build.py` `parts_title`: the one call whose second argument is a
`"%s"` and third a string, the next call's fourth the count; the title is
the bytes to that count. Group 20 writes it in place at `0x66A098` (room
20, the PC's NUL at `0x66A0AC`; witness the `push` at `0x47FA0D`, the slot's
one reader). `rest_3e.cpp`'s `EffectKind5E_DrawMenu` draws to the NUL once
the group is written (the French 17 letters past the count 15); x stays the
PC's. Ledger draft DIV-B.

### 4.3 `0x66A094`, the word beside kind 0x55's count: no Western source

The reader is `EffectKind55_DrawCount` `0x47EC30` alone (`push` at
`0x47EC72`); the census's second reader `EffectKind0F_DrawToggles` is the
tab table `0x66A088`'s end, not a read of `0x66A094`. The PSX twin is in
`WORLD00/AREA008.EMI` (area 8's thirty-second count, kind 0x55):

- **JP and both PSPs' `AREA008.EMI`**: the box `(0xC8, 0xA0, 0x2E, 0x14)`,
  the count at `0xCD`, then a one-glyph JP word (a two-byte code) at
  `(0xE5, 0xA4)` - the PC's layout. The JP word's source is there (D280's).
- **US, EU, FR, DE, and PSP-EU's `BOOT.BIN`** (`0x3FCF4`): the box
  `(0xC8, 0xA0, 0x22, 0x14)`, the count at `0xD1`, **no word**: the West
  dropped it and narrowed the box.

So no Western disc has a word for this slot, and none is invented. What a
fix could be is a layout, not a string: under a Western layer, the West's
narrower box and the count's x, read off the routine as group 18's layout
is. Not built: it changes what the PC draws under every Latin layer, which
is the owner's call (section 8).

## 5. Ledger and defect drafts (lettered; the coordinator numbers)

### DIV-A - DIV-0069 amended: the fishing equip headings from the disc

- **Subsystem:** the fishing spot's equip panels (`src/game/fishing_text.cpp`
  tag 3, `src/game/effect_1b.cpp` `DrawHeading`; `tools/loc_build.py`
  `fishing_headings`, chunk kind 16 tag 3).
- **Original behaviour:** `EffectKind0F_DrawItemsB` / `_DrawItemsA` /
  `_DrawEquipped` draw the four fixed fields at `0x653EA0` / `A4` / `A8` /
  `AD` with the counts 3, 4, 5, 5 at `x + 0x35`, `0x2F`, `0x2A`, `0x2A` - the
  PC's English words under every layer; on a FR or DE disc-built image the
  fields are zero and nothing is drawn (D286).
- **New behaviour:** under a Latin overlay that carries the headings (built
  from US, EU, FR and DE discs), the disc's own four words, whole, centred
  on the original's centre `x + 6 * count` by their real width. Without the
  chunk, or not armed, the original draw.
- **Rationale:** D286's remainder: the French and German words exist only
  at their discs' widths; the US module's own x put its words within a unit
  of the same centre.
- **Also in the PSX version?** The words and counts are each disc's own;
  the x is the PC's centre, not the disc's x.
- **Verification:** section 6; captures section 7.
- **Reversible?** play without `BOF3X_LANG`, or an overlay built before.

### DIV-B - DIV-0064 amended: group 16's slots, groups 20..23, group 17 on PSP-EU

- **Subsystem:** `src/game/labels.cpp`, `src/game/rest_3e.cpp`
  (`EffectKind5E_DrawMenu`); `tools/loc_build.py` groups 16, 17 (PSP), 20..23.
- **Original behaviour:** the PC's own words in five slots under every
  layer - `0x66A098` (Chinese), `0x663668` / `70` / `74`, `0x664288`,
  `0x66B1F0` (English) - and zero on a disc-built image where the disc
  re-words them; and DIV-0064's group 16 wrote faerie r's name into record r
  + 1's field (D289), so faerie 0 showed the shipped name and every other its
  neighbour's, on every Latin layer, the PC included.
- **New behaviour:** each slot holds the disc's word from the language
  overlay (kind 0x5E's menu title from `SCENA10.EMI`'s twin, cut at its
  draw's count, drawn to its NUL; the field panels' three labels; the shared
  list's title; the gene list's title), in place; the faerie names in their
  own records; PSP-EU's guest name from `BOOT.BIN`.
- **Rationale:** the stage-2 text swap (DIV-0005) for slots no group reached;
  D289's fix as [`loc-build-disc-only.md`](loc-build-disc-only.md) 5.4 and
  `owner-review.md` describe it. **It changes the PC path's output** under
  every Latin layer (each faerie now shows its own name; FR / DE see their
  disc's words where the PC's English was): the owner's word is asked
  (section 8).
- **Also in the PSX version?** The words are the discs'; the PSX draws the
  menu title at its own count and x + 8, ours to the NUL at the PC's x + 4.
- **Verification:** section 6.
- **Reversible?** play without `BOF3X_LANG`, or an overlay built before
  (group 16 then sends record 59 empty and it keeps the shipped name).

### Defects

- **D289**: fixed by DIV-B (the DLL and the builder).
- **D286**: the FR / DE remainder fixed by DIV-A.
- **A new one, for the coordinator to number if wanted:** on a DE
  disc-only install the equipment panel's six slot icons read `0x663660..`
  as zeros (section 2.2): ours (the map's segment), not measured, a TW row.

## 6. Checks

| Check | Result |
|---|---|
| `loc_build.py selftest` (with the two new readers on synthetic code, a refusal each) | ok |
| `loc_build.py anchors --game <copy of the PC install>` | PC_SHA (with `gene list head`) and FAERIE_UNNAMED the exe's |
| `loc_build.py all` over the seven discs into a copy of the PC install (`C:\Users\kerok\g_la\pc`) | US, EU, FR, DE: `4 headings`; `panel labels 3`, `list title 1`, `parts title 1`, `gene title 1`, `guest name 1`, `faerie names 60`; JP `identify words 6` as before; the PSPs skipped (not PlayStation discs) |
| PSP-EU through `convert_labels_psp` (scratch `t_psp.py`) | `guest name 1`, `parts title 1`, `faerie names 60`; fishing `no headings found` |
| `importer.py check` | 742 files, 3582 chunks, 0 errors; `exe_tables` check 0 errors |
| FR and DE disc-only caches built with this branch (`importer.py build`, `install --base`) | exit 0 each; `loc/fr-FR` and `loc/de-DE` carry fishing tag 3 and groups 17, 20..23 |
| self-test `BOF3X_SHADOW='*,-field_c1'` under `fr-FR`, `de-DE`; `'*'` under `BOF3X_LANG=original` | (filled in below) |

## 7. Captures

From root `C:\Users\kerok\g_la` only (launcher copy `L2`, `input_run.py
--no-front --speed 4`). The changed draws are the same code on both install
kinds. The FR / DE disc-only installs (built here with `importer.py build
--source <FR / DE cue> --lang fr-FR / de-DE`, `install --base`) logged every
chunk applied in the exe host (`4 of 4 fishing headings (D286)`, `60 of 60
faerie names`, `1 of 1 parts title`, `3 of 3 panel labels`, `1 of 1 list
title`, `1 of 1 gene title`; no Fatal), but **the recorded routes do not
drive them**: on both, the camping route's first shot is the Config screen
and its later ones the game's opening scene - the route starts a new game,
the French / German title taking inputs recorded under `en-US` otherwise
(the `fairyVillage` route does the same on the PC under `de-DE`). So the
screens were captured on a copy of the PC install with this branch's
`fr-FR` / `de-DE` overlays, where the routes hold:

| Screen | FR | DE | What it shows |
|---|---|---|---|
| `campingFishing` `fishing_equip` | `g_la\shots\pcfr\campingFishing\fishing_equip.png` | `g_la\shots\pcde\campingFishing\fishing_equip.png` | D286: the three headings in view (the equipped panel's, the bait list's, the guide's) in the disc's words, centred in their boxes |
| `campingFishing` `skill_note` | `g_la\shots\pcfr\campingFishing\skill_note.png` | `g_la\shots\pcde\campingFishing\skill_note.png` | group 22: the shared list's title, the disc's word |
| disc-only `campingFishing` (desynced) | `g_la\shots\fr\campingFishing\` | `g_la\shots\de\campingFishing\` | the routes' failure above |

Not captured (no route reaches them): the field panels' three labels (group
21; neither `menu_screens` nor `field_menu` opens those panels), the gene
list's title (group 23; `dragonMenu` shows the battle's gene window, group
12), kind 0x5E's menu title (group 20, area 128), the guest's name (no route
reaches the duel's equipment screen, as DIV-0118 said) and the faerie names
(group 16; the village route starts a new game under `de-DE`). Seen in
passing on the French fishing screen: the three tabs overrun their 0x28
boxes (DIV-0069's centring, unchanged here) - for the owner's eye.

## 8. For the owner

- DIV-B changes the PC path under every Latin layer: each faerie shows its
  own name (D289's fix), and the French and German layers show their discs'
  words for the panels' EQUIP / NEXT, the shared list's and the gene list's
  titles, and kind 0x5E's menu title. Keep?
- `0x66A094` under a Western layer: draw nothing (as now on a disc-only
  install, the PC's glyph on the PC) or take the West's layout (the narrower
  box, the count at `0xD1`)?
- The JP builds' rows (group 16, 17, 20, the `0x66A094` word) wait on D280.

## For the other files (the coordinator's)

- `docs/install-gap-census.md` 8: the statuses of section 0.
- `tools/exe_tables.py` `LANGUAGE_ZERO`: `0x653200`, `0x669CE8`, `0x66A098`
  are now written by a Latin layer (still zero under `ja-JP`); and
  `tools/exe_own.py`'s classes for `0x653EA0`, `0x663668..`, `0x664288`,
  `0x66B1F0` (`game-value` today) would read `language` if reclassed. Not
  edited here (OT's files); the gap rows stay as measured until then.
- TW / stage B: DE `0x663660..0x663668` (section 2.2).
