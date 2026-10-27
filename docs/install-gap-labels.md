# The install-gap round, group LA: the language and layout rows

**Status:** MEASURED (2026-10-18, branch `install-gap/la` off `06a209bd`, `d41273d9` merged in): the
rows read off all seven discs' code, the builder and the DLL changed, the
checks in section 6; captures in section 7. Two ledger drafts and the
defects' new statuses are in section 5, lettered, for the coordinator to
number. Nothing regenerated is committed. Section 9 (2026-10-27, `away1027/lang`): the `language`
rows that are not text, and the name rule's narrowing; its recipes regenerated. Section 9.2
(the same day, `away1027/twins`): the map's tie-break that places the faerie village operand.

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
| `Effect1b_ToggleText+0x28` `0x66A0B0..0x66A0D8` (Super Combo's prompt texts; D393, 2026-10-26) | all 7 | **FIXED under the layer** on every build, the JP ones too | `SuperCombo_DrawText`'s twin in `MAGIC003.EMI` / `BOOT.BIN`; label group 24 (section 4.5) |

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

### 4.4 The village music player's labels (`CommuMusic_CloseSteps+0x28`, FR / DE): read, not built

Handed over from TW (its report): TW placed the two segments FR and DE keep
in the PC's words (`0x65310C..0x65311C`, `0x65313C..0x653142`, from
`COMMU03.EMI#6`); the rest of `0x653104..0x653150` is the discs' own words
or nothing.

- **The words.** Six slots behind the pointer table `0x6530EC` (8, 16, 16,
  8, 8, 8 bytes from `0x653104`; read by `CommuMusic_DrawList` `0x45FA80`
  through `0x6530F0` / `F4` / `F8`). `COMMU03.EMI` holds the same six slots
  at the same sizes on US, FR and DE (US: the PC's words; FR and DE re-word
  four of them in mixed case). They would fit the PC's slots in place.
- **Why not built:** `CommuMusic_DrawLabel` `0x45FCE0` (ours, `rest_4e.cpp`)
  draws each byte as a quad of a texture page, `u, v` from `c - 0x20` - not
  a font glyph. So a disc's word is right only with that install's page
  holding the letters it uses: lowercase letters on the PC's page (a PC
  install under `fr-FR`) are not measured, and the French disc's own
  "stereo" slot has `0x20` where its accented letter would be, which this
  draw skips (a gap in the word on the French disc itself, by the bytes; not
  seen). A raw-byte group (group 15's shape) is the way, after a capture of
  the page on each install kind. Left for the coordinator / owner.
- **The glyphs at `+0x68`** (`0x653144`, `0x653148`, `0x65314C`: three
  `Text_DrawFont8` strings of the PC's 8 px font, pushed by the same draw):
  no disc copy (TW); the US routine draws its own controls there. Left as
  a gap, not attempted.

### 4.5 `0x66A0B0`, Super Combo's prompt texts (group 24; 2026-10-26)

Found after the round, by the spell sweep's gap watch on the US + PSP-EU
disc-built install ([`spell-sweep.md`](spell-sweep.md) 6): the `language`
row `0x66A096..0x66A0D8` read 90 times before any write by
`SuperCombo_DrawText` `0x49B420`, and the prompt box at the field's
centre blank where the PC reads "GO!!" (D393). The row past group 20's
slot is four slots behind the pointer table `0x66A0D8`: `0x66A0B0` (12
bytes, room 16), `0x66A0C0` (5, room 8), `0x66A0C8` (4), `0x66A0D0` (4);
the draw takes a byte less `0x3F` as a glyph of the effect's own page
(21 a row, from v `0x24`), `0xFF` as a half space, and centres by the
length table `0x65A51C` (`9, 5, 4, 4`, the overlay's data, placed). Text 1
is drawn under `SuperCombo_Prompt1`, 2 under `_Prompt2`, 0 under
`_ShowCount`; text 3 has no reader in `BOF3.exe`.

**The twin** is in `BMAGIC/MAGIC003.EMI` (Super Combo's effect overlay,
[`magic_s02.md`](magic_s02.md)), section 3 (loaded at `0x801EEC00`): the
routine at `0x801F01E0..` masks `which` with `andi 0xFF`, reads its
length at `L + which` and its record's first byte at `L + 4 + 13 which`
(`sll` / `addu` chain), then takes each byte `- 0x3F` (the `addiu` at
`0x801F0270`) - the PC's draw to the instruction, with the strings as
fixed records instead of a pointer table:

| Build | Where | Lengths `L` | Records | Stride |
|---|---|---|---|---|
| psx-us, eu-en, de, jp | `MAGIC003.EMI#3` `+0x1E9C` | `0x801F0A9C` | `0x801F0AA0..0x801F0AD4` | 13 (the US words) |
| psx-fr | `MAGIC003.EMI#3` `+0x1EE0` | `0x801F0AE0` | `0x801F0AE4..0x801F0B1C` | 14 (its own first text, 13 letters; `- 0x3F` at `0x801F0278`) |
| psp-eu `BOOT.BIN` | the image (first program header) | `0x2F4989` | `0x2F498D..0x2F49C1` | 13 (`- 0x3F` at `0x15394C` and `0x153A78`, one routine) |
| psp-jp `BOOT.BIN` | the same | `0x2F47E9` | `0x2F47ED..0x2F4821` | 13 (`0x1539EC`, `0x153B18`) |

Each record is the text to its NUL and then fill (`01 02 03 ..`, not
NULs), so the records are cut by the stride read off the code, not by
their padding. Both PSP discs' `MAGIC003.EMI` hold the PSX layout too (the
JP build's); the builder reads the PSPs' from `BOOT.BIN` as groups 17 and
20 do.

**The fix** (DIV-0064 amended 2026-10-26): `loc_build.py` `combo_prompts`
follows the values linear in the `andi 0xFF` byte through lui / addiu /
sll / addu / subu from the routine's `addiu $sp` to the `- 0x3F`, and
wants one (lengths, stride) pair whose record read is 4 past its length
read; the four texts are the records to their NULs (each 1..stride - 1
bytes), the lengths each under the stride. Group 24 sends them raw
(`label_chunk`'s `raw`, group 15's and 18's way: the bytes index the
install's page, so no font code of the layer's applies); `labels.cpp`
writes them in place, each slot witnessed by its entry of `0x66A0D8`.
Built for every disc: the Western PSX ones through `convert_labels`, the
PSPs through `convert_labels_psp` (`BOOT.BIN`), and the JP builds beside
group 15 (the one other group a Japanese disc builds; PSP-JP now builds
that path too, for this group alone). Under a Latin layer the PC install
changes: its slot 0's two `0x20` (a full glyph each, 12 px) become the
discs' `0xFF` half spaces, the PlayStation's own spacing; under `fr-FR`
the first text is the French disc's. Checks: section 6's table gains the
2026-10-26 rows below.

| Check (2026-10-26) | Result |
|---|---|
| `loc_build.py selftest` (`combo_prompts` on synthetic code: the US `sll` / `addu` chain found, the chain without its last `addu` refused) | ok |
| `combo_prompts` over the seven discs (`MAGIC003.EMI#3`; the PSPs' `BOOT.BIN`) | `combo prompts 4` on each; US, EU, DE, JP, PSP-EU, PSP-JP the same four texts, FR its own first text |
| `importer.py check`, `exe_tables.py check` | 0 errors each |
| `importer.py build` + `install --base` (US disc + PSP-EU) from this branch; `abc_run.py prepare --only pc-enus` (a PC copy, `en-US` from the US disc) | `DIV-0064: 4 of 4 combo prompts` in both installs' logs |
| `spell_sweep.py run --ids 3` on the disc install (`BOF3X_EXEIMAGE`, `BOF3X_CACHE`, `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the gap watch `gap,unread,language`) | `a003_1.png` READY, `a003_2.png` GO!!, `a003_5.png` O HIT COMBO!! (the before-image's box empty, `spellsweep_disc\probe_gw\a003_1.png`) |
| the same on the PC copy under `en-US`, and under `original` | `en-US`: the disc install's frames to the pixel; `original`: the PC's own bytes, text 0's two spaces a full glyph each |
| `gapwatch.py report` | `0x66A096..0x66A0D8` still "read 720 before a write, writes 0": the parked-page blind spot ([`install-gap-reads.md`](install-gap-reads.md) 9.3); written by the log line and the shots |
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
| self-test `BOF3X_SHADOW='*,-field_c1'` under `fr-FR`, `de-DE` (hosted, the PC copy with this branch's overlays) | `self-test only: done` each, `inject: 10088 ours`, no non-zero MISMATCHES line, no Fatal |
| self-test `'*'` under `BOF3X_LANG=original` (after the `d41273d9` merge) | done, `inject: 10087 ours`, no non-zero MISMATCHES, no Fatal |
| exe-host self-test on the FR and DE disc-only installs (`BOF3X_EXEIMAGE` = the cache) | done each, `inject: 10088 ours`; in the route runs every new chunk logged applied (section 7) |
| Defender | took the local launchers three times during the runs (the known trap); each run was re-checked by its log's `self-test only: done` and `inject:` lines, not its exit code |

## 7. Captures

From root `C:\Users\kerok\g_la` only (launcher copy `L2`, `input_run.py
--no-front --speed 4`).

**After merging `harness/recipe-buttons-by-release` (`d41273d9`, the
coordinator's: a recipe's buttons pressed as the install's release) the
camping route holds on both disc-only installs**, and the D286 screen is
captured there directly:

| Screen | FR disc-only | DE disc-only |
|---|---|---|
| `campingFishing` `fishing_equip` (D286's headings) | `g_la\shots2\fr\campingFishing\fishing_equip.png` | `g_la\shots2\de\campingFishing\fishing_equip.png` |
| `campingFishing` `skill_note` (group 22) | `g_la\shots2\fr\campingFishing\skill_note.png` | `g_la\shots2\de\campingFishing\skill_note.png` |

`fairyVillage` still starts a new game on the DE disc-only install (its
title, inputs recorded under `en-US`; `g_la\shots2\de\fairyVillage\`), so
the faerie names are not seen. The paragraphs below are the run before the
merge, kept for the record.

The changed draws are the same code on both install
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

## 9. The `language` rows that are not text (2026-10-27, away day, agent `lang`)

**Why.** `exe_own.py`'s symbol-name rule classed a segment `language` from
a symbol whose name ends `Messages`, `Format`, `Text` and so on, *up to the
next symbol*. A disc-built image zeroes `language` bytes, expecting a layer
to write them, and the gap watch and the data diff take the class as
"text". On 2026-10-25 that hid area 6's descriptor
([`exe-import-engine.md`](exe-import-engine.md) 7.10). This section reads
every `language` row of `recipes/exe-gaps.tsv` (both `how` = `language`
and the `language-zero` gap kind; all seven builds, from `main` `e84aaf6d`)
to find the others.

**Method.** Scratch scripts in `bof3x-runs\away1027\lang\scripts\` (not in
the repository): `dump_rows.py` reads each row's bytes from the PC's
`.data` (`exe_tables.pc_data`), its readers (`analysis/pc_xref.json`, 64
bytes back, plus the `.data` pointer words into it), and its `src/` names.
`shape.py` sorts each row's bytes into the port's two-byte glyph codes (lead
byte `0x80..0x8F`), GBK pairs, ASCII, zeros and other bytes. `cover.py`
lays the data diff's decoded keys over the rows. `neigh.py` builds each
disc's image (`exe_tables.disc_built`) and reads what the disc holds at the
neighbouring places' displacement. 115 distinct rows over the seven builds.

| Rows | Builds | Class assigned | Evidence | Written by | Gap watch |
|---|---|---|---|---|---|
| `CommuRank_Modes+0x8C..+0x528`: the 60 faerie names (16 bytes at `0x653200 + 20 r`) | all 7 | text (glyph codes) | 2..8 glyph bytes each, the rest zero; read through `CommuSim_AddRecord`'s `[ecx*4 + 0x653200]` | group 16 on a Latin layer; under `ja-JP` none (D280) | read 2026-10-27: rows 3, 12 and 19 through `CommuSim_AddRecord` <- `CommuSim_AreaEnter` on `fairyVillage`, US-only install (the day's validation, `away1027\int\gw_report_fairyVillage.txt`) |
| `ConfigScreen_OpenStates+0x24..` `0x6536F8..0x653808` | all 7 | text with its layout bytes | 17 records of 16: two layout bytes, then glyph codes | `convert_config` / `convert_config_psp` (17 records, PSP 15) | not seen read |
| `EffectKind0F_PanelTitles`, `Equip_SlotIcons+0x8` / `+0x14`, `SharedList_SortSteps+0xC`, `MenuList_GeneWinSteps+0x25` / `+0x27` | FR, DE | text (ASCII) | sections 1, 2 | kind 16 tag 3; groups 21, 22, 23 | not seen read |
| `Cursor_Hidden+0x2C`, `+0x4C` (`0x65DA78`, `0x65DA98`) | all 7 | text (GBK) | the window title and the missing-disc box | ours: DIV-0039 draws English, the bytes are not used | not seen read |
| The string pool `0x669CCC..0x66A450`: `AreaW0b_ItemNameKey`, `BattleE1_GuestHeadOut`, `Battle_CommandLabels`, `Battle_CommandLabelPointers+0x1C`, `BattleActions_TextCounter+..`, `CommuName_RecordNames+0x1C`, `Effect1b_ToggleText+..`, `Menu_Verbs+..`, `Magic080_TextPointers+0x18`, `Pause_LinesGame+0x8` | all 7 | text (glyph codes, ASCII) | each a string or a fixed-width slot behind a pointer table of its reader (`loc_build.py`'s group comments name each address) | groups 1..24 (`loc_build.py`), `char_names.cpp` (`0x669CD8`, `0x669CE0`), the pause lines DIV-0038 (`pause_text.cpp`); the JP builds' pool: D280 | read and written by the layer: `Battle_CommandLabels`, `Effect1b_ToggleText+0xE` / `+0x60`, `Menu_Verbs+0xF4`, `Magic080_TextPointers+0x18` |
| `language-zero` rows (`0x6531F4`, `0x653200` JP, `0x669CCC`, `0x669CE8` JP, `0x66A094`, `0x66A098` JP) | as listed | text | `exe_tables.LANGUAGE_ZERO` | none (sections 3, 4; `0x669CCC` the empty slot's text, read in every sweep) | `0x669CCC` read |
| **`Area08_MessageFormat+0x9FA`, `+0xA04`** (`0x5E1ABA..0x5E1AC0`, `0x5E1AC4..0x5E1AC8`) | PSP-JP, PSP-EU | **game value: area 9's entry script** | area 9's descriptor (`0x5E1AD0`) names `0x5E1AB8` as `entries` (`+0x14`) and `0x5E1ABC` as `scripts` (`+0x18`); no text byte. `BOOT.BIN` packs the entry halfword, so the script sits two bytes before the neighbour's displacement (no word shift of 7.9 reaches it) | nothing: zeroed on both PSP images | not seen read |
| **`Area175_ScriptMessages+0x8C3`, `+0xC1B`, `+0xF73`, `+0x12CB`, `+0x1623`, `+0x1D03`, `+0x207B`** (one byte each) | US, EU, FR, DE | **game value: a talk script's message number** (9.1) | `0xF1` bytes before each of areas 178..182, 184 and 185's descriptors: the low byte of talk script 6's `D0 00 hi lo` in the descriptor's `+0x04` talk table | nothing: the Western discs hold another value there (the JP disc and both PSPs hold the PC's); where a map segment runs over it (areas 177, 183) the image takes the Western value, elsewhere the map splits there and the byte is zero | not seen read |

The sweeps' gap-watch reports (`bof3x-runs\spellsweep_disc\gw_report.tsv`,
`bof3x-runs\ss1026\rv\gw*_report*.txt`, `ss1026\psp\`, `ss1026\sc\`) are
battles only; no row outside the string pool was read.

**Two non-text rows, both from the name rule** (no hand-listed region was
wrong). Every other `language` row is text or a text record's layout byte.
The census of what the rule matches shows why: of 147 symbols it matches,
most are areas' `Choices` (`unsigned long`), `ChoiceMessages` / `Messages`
(`unsigned short` message numbers) and the like: numbers whose own bytes a
disc carries, mostly placed. The name was never wrong about the symbol.
The fault was the reach past it.

**The rule change** (`tools/exe_own.py`, `name_extent` / `name_class` /
`name_cuts`): the `language` name rule speaks only for the symbol's own
bytes. Those are `ctype` x `count` from `symbols.toml` (one element with no
count). A symbol with no `ctype`, or a `char` array with no count, runs to
its PC string's NUL. Segments are also cut at those ends, except inside a
hand-listed `REGIONS` row, where the row decides. Past the end, a segment
gets the class of any unnamed one: unread when nothing reads it, otherwise
disc-unplaced or game-value by the disc search. This is narrower than a
hand-listed range, and it covers the rule's next reach too. The
`port-relaid` name rule is unchanged.

**Before and after** (`recipes/exe-gaps.tsv` rows / bytes by `how`;
`exe.toml`), the seven discs through `exe_own.py measure`, `exe_tables.py
gaps`, `exe_tables.py recipe`, `psp_exe.py recipe`, `exe_tables.py recipe`,
`exe_own.py measure`, `exe_tables.py gaps`:

| Build | `gap` | `language` | `identical` | `placed_differ` |
|---|---|---|---|---|
| `psx-jp` | 21 / 299, unchanged | 88 / 2,736, unchanged | 615,452, unchanged | 3,151, unchanged |
| `psx-us` | 48 / 680 to 55 / 687 | 89 / 2,824 to 82 / 2,817 | 614,996, unchanged | 3,194, unchanged |
| `psx-eu-en` | 49 / 723 to 56 / 730 | 89 / 2,824 to 82 / 2,817 | 614,965, unchanged | 3,191, unchanged |
| `psx-fr` | 52 / 758 to 59 / 765 | 93 / 2,860 to 86 / 2,853 | 614,912, unchanged | 3,193, unchanged |
| `psx-de` | 52 / 758 to 59 / 765 | 94 / 2,862 to 87 / 2,855 | 614,911, unchanged | 3,192, unchanged |
| `psp-jp` | 21 / 267, unchanged | 89 / 2,752 to 87 / 2,742 | 613,246 to 613,250 | 3,380, unchanged |
| `psp-eu` | 20 / 237, unchanged | 89 / 2,798 to 87 / 2,788 | 613,227 to 613,231 | 3,360, unchanged |

`unread` and `keys` are unchanged on every build.

- **Area 9's entry script now places** on both PSP builds. With the row no
  longer `language`, `psp_exe.py`'s `psxmap` rule (which skips `language`
  ranges) reads it from the PSP disc's own `WORLD00/AREA009.EMI#13`, byte
  for byte the PC's: two `exe-psp.tsv` rows a build, 10 bytes. The PSP images'
  hashes change. A PSP-built install needs the rebuild (`importer.py stale`
  names `exe`). What area 9 did with a zeroed script was not seen in play.
- **The faerie village bytes are now `gap` / `game-value`** on the four
  Western PSX builds (7 one-byte rows on each). No rule places them, because
  every rule wants the PC's byte and the Western disc holds another value.
  The image keeps the zero it had. Now the class says so: the gap watch and
  the data diff see a game value, not text. No data diff key decodes these
  bytes, so `recipes/data-known.toml` gets no entry (one would match no row and
  fail `check`). For the owner: which value is right on a Western install?
  It could be the disc's own, as areas 177 and 183 already get it through
  the map, or the PC's (JP's). That depends on what the operand selects
  (it is a message number, 9.1: the Western one goes with the Western text).
- **Text rows no layer writes:** none on a Western build beyond the known
  `language-zero` rows (sections 3, 4: `0x6531F4`, `0x669CCC`, `0x66A094`).
  On the JP builds the whole string pool, the faerie names and the config
  labels wait on D280, as before.

Checks: `exe_tables.py check`, `psp_exe.py check`, `importer.py check`,
`tables.py check`, `ledger_check.py`, `exe_own.py check`, `data_diff.py
check`, all 0 errors. `data_diff.py run`: 628 rows, the same rows as before,
only the two PSP image hashes changed. No ledger entry: a placement of disc
data, DIV-0101's.

### 9.1 What the faerie village operand selects (follow-on, the same day)

**It is a message number: the talk line of one villager, a release difference.**

- **Where the byte is.**
  - Descriptor `+0x04` of each faerie village area (175..185) is the talk scripts' table: 12 pointers (area 178: the table at `0x642DC4`, the scripts at `0x642D1C` and `0x642D40 + 12 (k - 1)`). The pointer words were found by a scan of `.data` for words into the block.
  - [`event-script.md`](event-script.md) 2 describes the table: `MoveScript_SetTurnTarget` `0x517E90` runs the table entry an object's talk word picks (bit 15) through `EventScript_Run`.
  - The bytes previously written as "the `+0x10` block" are this table's scripts (the `+0x10` array holds one pointer, to `0x642D0C`). The 12-byte talk scripts are `D0 00 hi lo`, `D0 08 80 n`, `FF`.
  - `tools/event_scan.py --area 178` decodes the placement script at `+0x00` up to `0x642D09`. That tool does not decode the talk table ("not decoded here"), so the three ops were read with its grammar: `EventScript_OpLengths` gives `Dx` 4 bytes.
- **What `D0` does.** It is `EventOp_Dx` `0x57B500` (`src/game/event_ops.cpp`). With `op[1]` 0 it writes the talked-to object's word `+0x88` (`trigger_word`) = `hi:lo`. With `op[1]` not 0 it writes `+0x86` (the trigger id) = `op[3]`.
- **How the word is read.** After the script, `MoveScript_SetTurnTarget` reads `+0x88` again (`object_kinds.cpp`, PSX `FUN_801A27A8`): bit 13 set opens a system message, otherwise `Msg_OpenScript` opens **script message `hi:lo` of the area's text block**.
- **The values.** The byte is the low byte of talk script 6's message. All eleven areas have the same run of numbers in the same order (a scratch read, `scripts\talk_ids.py`).
  - On the PC, the JP disc and both PSPs, script 6 opens message `0xE1`.
  - On the four Western PSX discs it opens `0x6D`, the same message as script 5.
- **What a Western disc-built image holds** (`scripts\talk_img.py` over the US disc):
  - Areas 175, 176, 177 and 183 hold the Western `0x006D`; the map's runs reach it.
  - Areas 178..182, 184 and 185 hold `0x0000`, so talking to that villager opens message 0 of the area.
  - A disc-only install's area text is the disc's own block, with the PC's slot count (US, EU, JP: [`loc-build-disc-only.md`](loc-build-disc-only.md) 4.2). The Western `0x6D` therefore goes with the text it is built with, and **the disc's value is the right one on a Western disc-built install**. The PC's `0xE1` would open a line the Western release's script 6 never opens.
- **Why no rule places it.** It is not a gap in the existing map rule; the map's tie-break causes it.
  - In area 178 the Western byte differs from the PC's, so the run from `AREA178.EMI` ends before it.
  - The 300 bytes after it (`0x642D80..0x642EAC`, the talk table and the descriptor) are held at the same address by six files. `exe_twins.runs` gives such a group to the first by name (`rank`), `AREA177.EMI`, so the next segment is `AREA177.EMI`'s.
  - Two files cannot be bridged by the map's 64-byte gap rule, so the byte between them stays unplaced.
  - In areas 177 and 183 the run stays in the area's own file, and the bridge takes the Western byte (counted in `placed_differ`).
  - `tools/exe_maps`-based scope (`scripts\tie_scope.py`): exactly 6 such boundaries a Western PSX build (areas 178..182 and 184), none on the JP or PSP builds. Area 185's tail is `AREA183.EMI`'s at another displacement (area 185's layout is `0x64` bytes on).
- **The fix, not built here** (built the same day: section 9.2).
  - It is a tie-break in `tools/exe_twins.py` `survey` / `map`: a run that several files hold equally continues the segment before it when that segment's own file holds the run at its own displacement. The survey must record the group's files, and check the bytes against that file.
  - The map then bridges the one byte from the area's own file, and the image takes the Western `0x6D` in all seven, as it already does in four.
  - This means regenerating `exe_maps/` (survey over the seven discs), then the recipe chain of section 9. `placed_differ` rises by the seven bytes on each Western build, as release differences.
  - It is left for the orchestrator because it moves `exe_maps/` and every recipe after it: better done once after the day's merges than on a branch beside `desc`'s.
  - The data diff decodes no talk table, so no `data-known.toml` entry follows.
- **On the PC install under a Western layer** (not a disc-built image): `.data` keeps the PC's `0xE1`, and the layer's text block is the disc's. Talk script 6 in these eleven areas then opens the Western block's message `0xE1`, not the `0x6D` the Western release shows there. Which line that is was not read (no text is quoted here). This is for the owner: whether a Western layer should carry the operand too.

### 9.2 The map's tie-break: the faerie village operand placed (the same day, agent `twins`)

**What changed.** `tools/exe_twins.py`, the map's choice between equal
holders (its docstring, and [`exe-tables-by-build.md`](exe-tables-by-build.md)
2.2, say the same in the tool's terms):

- **The rule.** Each PC byte goes to the longest run holding it; between runs
  of one length the first by file rank wins (boot, `GAME.EMI`, `BATTLE.EMI`,
  then the file name). When that choice would start a new segment, and the
  segment just before it, inside the 64-byte gap bridge, is a place that
  holds the same run equally, the segment before continues in its own file
  and displacement. The bridge then takes the bytes between them from that
  file.
- **"Holds the run equally"** is one of two measured things, never a name:
  a file of the run's copy group (the survey's run now records, as `also`,
  the other files that hold its seed at the same address *and* agree with
  the PC over the run's whole span, pointer words wild, compared byte for
  byte against each file), or another run with the same span and length.
  The tie-break never overrides the longest run, or the PSP's ELF-first
  order: it replaces only the name order between equals.
- **Area 185** is the second kind. Its tail (`0x644538..0x644664`, 300 bytes)
  is held at one address by `AREA183.EMI` and `AREA184.EMI` (the group the
  name order gave to `AREA183.EMI`), and by `AREA185.EMI` at another
  address. That other address is area 185's own displacement: the segment
  before it (`0x6442F4..`) has the same one. So the same rule covers it. The
  survey found that run by itself, and the map now continues area 185's
  segment with it. Areas 178..182 (the six-file group, `AREA177.EMI` by
  name) and 184 (the two-file group, `AREA183.EMI` by name) are the first
  kind.

**The map's effect** (the survey re-run over the seven discs,
`bof3x-runs\away1027\twins\after\json`; the map diffed against the
original code's map from the same survey, `twins\base\maps_orig`): on each
Western PSX build 14 segments become 7 (`0x642B60..0x642EAC` area 178 to
`0x6442F4..0x644664` area 185, each now one segment of its own area's
`EMI#13`); on `psx-jp`, `psp-jp` and `psp-eu` no segment moves. The tie
fires nowhere else on any build. `lang\scripts\tie_scope.py` after: 0
boundaries on every build (6 before on each Western build).

**What is committed in `exe_maps/`.** Only that effect, applied to the
committed maps. A full re-run of `survey` and `map` today also moves other
segments, with or without the tie-break: 9 to 15 diff lines a build, all
pass-2 single-table places (`search`, `neighbour`, `name`) that come and go
because `symbols.toml` and `src/` have grown since the maps were made
(2026-10-08; 700 candidates now). Run through the recipe chain by itself
(`twins\drift_wt`, `twins\chain_drift.out`), that drift changes the image on
the four Western builds (`placed_differ` +1 each) and the recipe files on
all seven. It is not this rule, so it is left out and reported.

**Before and after** (the recipe chain over the seven discs from this
branch, `twins\chain.out`; *before* is the integration branch's own chain
on the same day, `away1027\int\chain.out`, its recipes kept in
`twins\before_recipes`):

| Build | `gap` rows / bytes | `identical` | `placed_differ` | refused rebuilds | image |
|---|---|---|---|---|---|
| `psx-jp` | 20 / 279, unchanged | 615,454, unchanged | 3,151, unchanged | 121, unchanged | unchanged |
| `psx-us` | 54 / 667 to 47 / 660 | 614,998, unchanged | 3,194 to 3,201 | 191 to 121 | 7 bytes |
| `psx-eu-en` | 55 / 710 to 48 / 703 | 614,967, unchanged | 3,191 to 3,198 | 195 to 125 | 7 bytes |
| `psx-fr` | 58 / 745 to 51 / 738 | 614,914, unchanged | 3,193 to 3,200 | 195 to 125 | 7 bytes |
| `psx-de` | 58 / 745 to 51 / 738 | 614,913, unchanged | 3,192 to 3,199 | 195 to 125 | 7 bytes |
| `psp-jp` | 21 / 267, unchanged | 613,250, unchanged | 3,380, unchanged | 134, unchanged | unchanged |
| `psp-eu` | 20 / 237, unchanged | 613,231, unchanged | 3,360, unchanged | 127, unchanged | unchanged |

`unread`, `language` and `keys` rows are unchanged on every build.

- **The image.** Each build's disc-built `.data` from the before recipes and
  from the after recipes, compared byte for byte (`twins\imgdump.py`): the
  four Western builds differ at exactly `0x642D7F`, `0x6430D7`, `0x64342F`,
  `0x643787`, `0x643ADF`, `0x6441BF`, `0x644537` (talk script 6's message
  number, low byte, in areas 178..182, 184, 185); JP and both PSPs are
  identical. `exe_tables.py build` from the US cue gives the same image
  (`verify`: 0 problems). `lang\scripts\talk_img.py` over it and the other
  six discs: talk script 6 opens `0x006D` in all eleven areas 175..185 on
  US, EU, FR and DE (seven of them `0x0000` before), and `0x00E1`, the PC's,
  on JP and both PSPs, as before.
- **Every other changed row.**
  - `exe-gaps.tsv`: the 28 one-byte `gap` / `game-value` rows of section 9
    (`Area175_ScriptMessages+0x8C3` and six more, on four builds) leave.
  - `exe-places.tsv`: 28 rows change their `file` only (7 a Western build:
    the 8-byte place after each tail, `0x642EAC`, `0x643204`, `0x64355C`,
    `0x6438B4`, `0x643C0C`, `0x6442EC`, `0x644664`), from `AREA177.EMI` /
    `AREA183.EMI` to the area's own file, same range, `agree` and `keys`;
    `0x644664` also takes `AREA185.EMI`'s own address.
  - `exe-rebuild.tsv`: 112 refused-rebuild rows leave (28 a Western build:
    each of the seven areas' talk-table run of 7 words, and three of its
    descriptor's words). Before, the map run backwards read those pointer
    words from another area's file and got that area's addresses, so the
    rebuild refused them and the layout filled them. Now each is read from
    its own area's file and rebuilds to the PC's value, so the image bytes
    there are the same (`exe.toml`, a Western build: `rebuild_refused` -70,
    `rebuild_ambiguous` -72, `layout_words` -142, `rebuilt_equal` +568).
  - `exe.toml` also carries the four new image hashes and the files'
    hashes; `data-diff.tsv` the same 618 rows, only the four Western image
    hashes changed. Nothing else moved.
- **Release difference, not a divergence.** The bytes are the Western
  disc's own message number, placed into a disc-built image from the disc:
  a placement of disc data under DIV-0101, so no ledger entry. The PC
  install's `.data` is not touched; whether a Western language layer should
  carry the operand there too stays the owner's question of 9.1.
- **`data-known.toml`.** No entry: the data diff decodes no talk table, and
  `data_diff.py check` agrees (0 errors, 4 known entries, as before).
- **Install staleness.** The four Western images' hashes change, so
  `importer.py stale` names `exe` on a Western disc-built install built
  before this, and the launcher's prompt brings the rebuild. Not seen in
  play: no game was run.

**Checks** (`twins\chain.out`): `exe_tables.py check` (878 refused
rebuilds, 1,158 before), `psp_exe.py check`, `importer.py check`,
`tables.py check`, `ledger_check.py`, `data_diff.py check`, `exe_own.py
check`: 0 errors each. `src/` is untouched, so `codegen_same.py` and the
self-tests do not apply.

**Chain order after a map change.** The section 9 chain begins with
`exe_own.py measure`, which builds images from the recipes on disk. With
the new maps those recipes are stale (`exe-rebuild.tsv` refuses words the
new maps rebuild to the PC's value), and `measure` stops at `0x642EAC`
("rerun `recipe`"). So after a map change the chain runs `exe_tables.py
recipe`, `psp_exe.py recipe`, `exe_tables.py recipe` once first, then the
section 9 chain.

## For the other files (the coordinator's)

- `docs/install-gap-census.md` 8: the statuses of section 0.
- `tools/exe_tables.py` `LANGUAGE_ZERO`: `0x653200`, `0x669CE8`, `0x66A098`
  are now written by a Latin layer (still zero under `ja-JP`); and
  `tools/exe_own.py`'s classes for `0x653EA0`, `0x663668..`, `0x664288`,
  `0x66B1F0` (`game-value` today) would read `language` if reclassed. Not
  edited here (OT's files); the gap rows stay as measured until then.
- TW / stage B: DE `0x663660..0x663668` (section 2.2).
