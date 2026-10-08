# The PSP's Stallion and Holy Mantle changes, and whether they are toggle-sized

**Status:** MEASURED (2026-10-06) - read-only; nothing in `src/` changed. The
data side is settled to the row and rendered; the code side compared function
by function for the boss's own module and the encounter path, not for the
shared battle engine (section 6). Nothing here has been seen in play.

The owner's question (2026-10-06): can the Stallion sprite and boss-logic
differences of the PSP release be taken as a toggleable option? Stallion is
Balio and Sunder's fused form, fight 24 in area 67 (`boss_se.md`, kind 29).
The leads are [`IDEAS.md`](IDEAS.md) I32's P6 (recoloured brown and blue) and
P7 (its signature attack renamed); P4 (the Holy Mantle) was added to this
assessment by the coordinator the same day (section 5). The method is
[`region-diff.md`](region-diff.md)'s for data and
[`psp-widescreen.md`](psp-widescreen.md)'s for the PSP ELF, with the
[`psx-twin-check.md`](psx-twin-check.md) rule for pairing functions.

No game content is reproduced here: palettes are described by counts, hue
families and brightness; names by table id. The decoded entries, the
renders and the name records are in the gitignored `analysis/stallion/`.

**Short answer.** Stallion's PSP change is **data only, palettes only**: two
16-colour rows in area 67 and the same two (plus six more for its three
palette-swapped kin) in area 166. No sprite pixel, no enemy number, no AI row
and no line of the boss's code differs. A toggle is a palette layer built
from the player's own PSP disc - **about a day**, plus a small loader change
(section 4). The renamed attack (P7) is a one-record text change in the PSP
English executable; Japanese PSP keeps its name table in its executable too.
The Holy Mantle (P4) is **refuted**: its record and every function that reads
it are the PSX's; the encounter test lost one unrelated condition on the PSP,
which the PC already lacks in effect (section 5).

## 1. Sources and tools

| what | where |
|---|---|
| PSX JP / US discs, PSP JP / EU ISOs | the main checkout's `CDImage/` (identified in [`region-diff.md`](region-diff.md) section 1) |
| the per-section diff | `analysis/region/psx-jp_vs_psp-jp.json` (region-diff's run) |
| palettes, renders, tables | [`tools/psp_stallion.py`](../tools/psp_stallion.py) `clut`, `render`, `tables`, `boot` (new) |
| PSP code | `PSP_GAME/SYSDIR/BOOT.BIN` of both PSP discs: plain MIPS ELFs (`psp_stallion.py boot`: JP 5,077,621 bytes, EU 5,075,540; `EBOOT.BIN` beside it is the encrypted `~PSP` wrapper and is not read). Capstone MIPS32 LE over PH0 at base 0, as `psp-widescreen.md` |
| PSX code | the boss module `BOSS024.EMI` section 16 (load `0x800C1800`, 976 bytes; the sibling's `names/boss_records.toml`: entries `0x800C19E8` fight 24, `0x800C1AB0` fight 48); the field code in `GAME.EMI` section 0 (JP load `0x80196800`); `AREA189.EMI` section 13 (`0x801F2C00`) |
| names | the sibling's `names/abilities.toml`, `items.toml`, `enemies.toml` (reference only) |
| the fight's AI rows | `tools/enemy_ai.py --area 67` over the PC's `DAT/` (output not kept: game data) |

`tools/psp_elf.py` assumes the EU ELF's `.text` size; the JP ELF's is
`0x2CF798` (EU `0x2CF710`), so the scans below used scratch readers over the
section table rather than that tool.

## 2. Every PSP data difference that touches the fight

From `analysis/region/psx-jp_vs_psp-jp.json`, the three files the fight
loads, every section:

| file | section | PSP vs JP | what |
|---|---|---|---|
| `AREA067` (the arena) | 6, `0x8002D800`, 2,048 bytes | **2 rows of 64 differ** (rows 6 and 7) | the area's sprite palettes - **Stallion's** (below) |
| `AREA067` | 7, `0x0A081000` page | 4 tiles of 128 differ | the area's own textures: a 16 x 16-texel block blanked (tile 8, VRAM halfwords x 576..583, y 256..271) and three small redraws of 3, 8 and 62 halfwords (tiles 59, 75, 89). Field scenery, not the boss; their content not identified |
| `AREA067` | 5, `0x0E001000` sprite sheet | identical | the boss's cells (below) |
| `AREA067` | 10, `0x800E4000` enemy table | identical | Stallion's stats and AI rows, and the three Bullies' |
| `AREA067` | 8, 9, 11..14 | identical | the other palettes, map data, messages, area code |
| `BOSS024` (the boss module) | 3, 5, 6, 10, 15, 16 | identical | the battle engine copy, `0x800F0800` data, its palettes, the boss code |
| `BOSS024` | 4, `0x1A080200` | 2 tiles | the shoulder-button labels, in all 59 battle and boss modules (region-diff 5.2) |
| `BOSS024` | 11, `0x80014000` | differs | the battle pool's empty-slot fix, in all 42 battle and boss modules (region-diff 5.2) |
| `AREA166` (fight 48, kind 55's area) | 6, `0x8002D800`, 1,024 bytes | **8 rows of 32 differ** (rows 0..7) | Stallion's two rows again, and its three palette-swapped kin |
| `AREA166` | 8, `0x8002BE00` | 1 row | row 5: sixteen black entries become a grey ramp with the STP bit set; not this sprite (not rendered) |

The fight's music and sound are re-authored in PSP formats like every bank
(region-diff 5.1); not compared. The attack's spell module is not among the
PSP's changed files (no `BMAGIC` row in the PSP pair).

### 2.1 Whose palette rows they are

The decisive measurement is the render (section 2.3). Two corroborations:
`AREA067` and `AREA166` change **the same two rows identically** (row 6, 7
in one; row 0, 1 in the other), and the boss code gives kind 29 (Stallion,
area 67) and kind 55 (area 166) the same animation and sound tables
(`boss_se.md`; the PSX module's `0x800C1B44` / `0x800C1B50`, read both
times by the two entries).

A sprite palette is 32 entries, two of these 16-entry rows
(`Sprite_RestoreClut`, [`sprite-draw-order.md`](sprite-draw-order.md): "32
words to a number"). So the change is **palette number 3 of area 67** and
**palette numbers 0..3 of area 166**. The sprite sheet uses indices below 32
almost exclusively (a census of `AREA067`'s sheet: 57 distinct values, all
but 721 of 262,144 texels below 32), which fits.

### 2.2 The decode, in words

`tools/psp_stallion.py clut` (per entry in `analysis/stallion/clut_decode.txt`):

| palette | entries moved | JP (PSX) | PSP |
|---|---|---|---|
| area 67 no. 3 = area 166 no. 0 (Stallion), first row | 15 of 16 (entry 0, transparent, kept) | a pale ramp: near-white and pale yellow through cool greys to grey-purple (luminance mean 125), with four reds and two oranges at the end | a brown ramp (eight browns, mean 68) - the body - and the four reds become blue-greys and slate blues |
| the same, second row | 16 of 16 | dark reds, a yellow-to-tan trio, a blue ramp, a teal ramp (mean 84) | the dark reds turn dark teal, the tan trio turns orange-red, the blue ramp shifts to cyan, the teal ramp turns brown (mean 53) |
| area 166 no. 1 | 15 + 15 | the pale ramp with violet accents | blues and steel |
| area 166 no. 2 | 15 + 15 | the pale ramp with red accents | golds and yellows |
| area 166 no. 3 | 9 + 13 | dark greys with red / cyan accents | navy greys with magenta |

In RGB555 terms (the light and dark ends of Stallion's first row): the
lightest moved entry goes from `0x63DF` to `0x1970`, the darkest from
`0x24C8` to `0x0422`; the red accent run `0x2558..0x188E` becomes
`0x39AA..0x24E5`. The fan wiki's "brown and blue" is a fair summary of the
first row and the PSP's overall darkening (mean luminance 125 to 68, 84 to
53); every one of area 166's swapped palettes is recoloured too, which no
lead listed.

### 2.3 The render (P6 settled)

`tools/psp_stallion.py render` and the scratch renders, all under
`analysis/stallion/`:

- `AREA067_stallion_cells_jp_left_psp_right.png` - texel columns 512..767 of
  the area's sprite sheet (`0x0E001000`, 8 bits a texel, 16 tiles across)
  through palette 3, JP on the left, PSP on the right, 2x. The large cells
  (head, neck, limbs, a holed blade) are white-grey with red and teal on the
  JP disc and brown with blue-grey on the PSP; the smaller cells below them
  in the same columns change the same way, so they share the palette
  (which actor's frames use them is not read - section 6).
- `AREA166_palettes0to3_jp_left_psp_right.png` - the same cells in area 166
  under its four palettes, JP left, PSP right: four coherent creatures on
  each side, all four recoloured.
- `AREA0nn_<build>_sheet_palettes0to3.png` - what the committed tool
  writes: the whole sheet of each area under palettes 0..3, stacked, one
  file per disc (the two crops above were cut from the same renders).
- `AREA067_changed_tiles_jp_top_psp_bottom.png` - the four scenery tiles of
  the `0x0A081000` page, index-grey, JP above PSP.

That the cells read as whole, plausibly-coloured figures under palette 3 and
not under the area's other palettes is the evidence that this is the
palette they are drawn with. The owner's eye on the two PNGs is the check.

### 2.4 The fight's numbers and logic data

Area 67's enemy table is identical (region-diff, and above); `enemy_ai.py
--area 67` reads Stallion's AI from it: a base row and three conditional rows
over three abilities, one of which is ability 115 in the sibling's
`abilities.toml` (the US name the sibling records; the PC doc
[`magic_c2.md`](magic_c2.md) has its effect, `MAGIC116`, row 59). So the
fight's stats, HP, AI conditions and odds, and the attack's effect module
are the PSX's on the PSP.

## 3. The code side

### 3.1 The PSP runs its own compile of the boss module

The PSP keeps every EMI's PSX code section byte for byte (region-diff 5),
but the ELF holds a native compile of the same functions: `BOSS024`'s module
is at EU `0x1E5198..0x1E552C` and JP `0x1E54BC..0x1E5850` (the same layout,
JP 0x324 later), its tables in `.data` (EU `0x2F73B0`, JP `0x2F7220`). Found
by `Boss24_End`'s signature - a bit-1 test of a byte, then 5 stored, `| 8`
and 0x44 stored (`0x904AE8` and `Music_Track` on the PC) - which occurs once
in each ELF; the neighbours line up with the PSX module entry for entry.

| function (PC name, `boss_se.md`) | PC | PSX (`BOSS024` #16) | PSP-EU | PSP-JP | verdict |
|---|---|---|---|---|---|
| `BossStallion_Dispatch` | `0x43B770` | `0x800C1804` | `0x1E5198` | `0x1E54BC` | same: index the 12-entry state table by `+1` |
| `BossStallion_Enter` | `0x43B790` | `0x800C1848` | `0x1E51D4` | `0x1E54F8` | same: animations, hook, sounds stored, `+1` = 2, tail tick |
| `BossStallion_Hook` + 3 `BareRet` | `0x43B7D0` | `0x800C18A0..0x800C18EC` | `0x1E5230..0x1E527C` | `0x1E5554..0x1E55A0` | same |
| `BossSample10_Dispatch` / `_Enter` / `_Hook` | `0x43B7E0` / `0x43B800` / `0x43B860` | `0x800C18F0` / `0x800C1934` / `0x800C1998` | `0x1E527C` / `0x1E52B8` / `0x1E5324` | JP +0x324 | same, `+0x104 \|= 8` included |
| `Boss24_Setup` | `0x43B870` | `0x800C19E8` | `0x1E5370` | `0x1E5694` | same: three hooks stored |
| fight 24's event hook (`BareRetZero` on the PC) | - | `0x800C1A20` | `0x1E53A0` | `0x1E56C4` | equivalent: the PSP's is a compiled switch whose six cases all return 0 |
| `Boss24_End` | `0x43B890` | `0x800C1A28` | `0x1E53E4` | `0x1E5708` | same: won - variable 3 = 5, `\| 8`, music 0x44, step 1; else step 2 |
| `Boss24_Exit` | `0x43B8C0` | `0x800C1A90` | `0x1E5448` | `0x1E576C` | same: actor 3 cleared |
| `Boss48_Setup` / end / exit | `0x43B8D0` | `0x800C1AB0` / `0x800C1AF0` / `0x800C1B3C` | `0x1E5464` / `0x1E54D8` / `0x1E5528` | JP +0x324 | same shape (the end's stored byte is a different global on each side, which is relocation) |
| the tables | `BossStallion_Anims` / `_Sounds` / `_States` / `_Hooks` | `0x800C1B44` / `0x800C1B50` / `0x800C1B58` / `0x800C1B88` | `0x2F73B0` / `0x2F73BA` / `0x2F73C4` / `0x2F73F4` | `0x2F7220`.. | the animation and sound bytes equal; the state tables point at the generic enemy states in the same order (entries 1 and 10 the same function on both) |

PSP-EU against PSP-JP was matched by address-masked instruction sequences
(j/jal targets, `lui` and register-relative low halves masked): every
function above has exactly one JP twin with the same masked words.

**Verdict: no boss-logic difference.** Stallion's own code is the PSX's on
both PSP releases. The generic battle engine (the enemy states the tables
point at, the AI interpreter) is shared by every fight and was not compared
function by function; nothing in the leads points there, and the fight's AI
data is identical (2.4).

### 3.2 P7: the renamed attack

The PSP-EU ELF carries the US disc's item and ability tables in the US
layout (`psp_stallion.py tables`; space stored as `0xFF`): ability table at
`0x2F343C`, 227 records of 20 bytes. Against the US disc's `GAME.EMI`
(`0x801CA718`): **numbers equal in all 227 records; four names differ** -
ids 45 and 46 respaced (one word to two), **id 115 replaced** (the US
twelve-letter name becomes a different two-word name), id 217 renamed (a
dragon spell). Id 115 is the ability Stallion's AI rows pick (2.4). So **P7
is confirmed for the English PSP release**: the record's numbers are
unchanged, only the name.

The item tables tell the same story, unlisted by any lead: names of weapon
14, armour 9 and accessories 29 and 45 changed, numbers equal, except
consumable 87, whose first byte (`flags` in the sibling's `items.toml`) loses
bit 0. The records are in `analysis/stallion/psp_eu_name_table_diffs.txt`.

Japanese: the PSP-JP ELF has its own name tables (the PSP does not read
names from `GAME.EMI`, region-diff 5.4); whether the Japanese name of id 115
changed on PSP-JP was **not read** (section 6).

**Where the PC's name lives:** the ability table `0x65C4C8` in `BOF3.exe`'s
`.data`, 0x18 bytes a record with a 16-byte name
([`battle_window_draw.md`](battle_window_draw.md) section 2); an English game
overwrites the names from the player's US disc through a kind-5 overlay chunk
(DIV-0008, `NameTables_Apply`).

## 4. The toggles, costed

| change | kind | needs | cost | live check |
|---|---|---|---|---|
| **P6 Stallion's palettes** (area 67 rows 6-7; area 166 rows 0-7) | data layer | the player's PSP disc (JP or EU: the two ship these sections identically, region-diff 5) | **easy - about a day** | a new route: neither of the owner's recipes reaches fight 24 (below) |
| **P7 the attack's name** (ability 115, English) | text layer | the player's PSP-EU disc | **easy - hours**, on top of P6's loader change | the same route, the attack's banner |
| the other three ability and five item renames | text layer | the PSP-EU disc | free once P7's builder exists; whether to take them is the owner's call | menus |
| Stallion's code | - | - | **nothing to take**: identical | - |

**P6, how.** The PC loads `AREA067.DAT`'s palette chunk at tag `0xA000`
(`0x8002D800 - 0x80023800`, [`DAT_CONTAINER.md`](DAT_CONTAINER.md) section 2;
region-diff's PC pair shows the chunk is JP's byte for byte in both areas).
A kind-0 chunk in an overlay lands on the shipped one (DIV-0005), so the
data side is a builder that reads the player's PSP ISO with
`tools/psx_disc.py` (it reads ISOs already) and writes `AREA067` and
`AREA166` overlays carrying that one section - `loc_build.py`'s shape,
`ASSET_SOURCES.md` section 3's "layer". The engine side is small but real:
`LoadDatFile` walks exactly one overlay prefix today, the language's
(`src/game/dat_load.cpp`, `DAT\<lang>.<name>`); a PSP layer needs a second
walk (`DAT\psp.<name>` under one `BOF3X_` option and a launcher box), and
the ledger entry for the option. Taking only rows 6-7 / 0-7 (not the whole
section) keeps the layer to exactly the measured change; the area-166
`0x8002BE00` row is a separate decision (it is not this sprite). Whether
area 166's three kin follow Stallion's toggle or get their own is the
owner's call - they are one section, so one toggle is natural.

**P7, how.** `loc_build.py` already finds a donor table by the PC table's
numeric bytes and writes the names as a kind-5 chunk (DIV-0008); a PSP donor
is the ELF's US-layout table instead of `GAME.EMI`, with `0xFF` read as a
space. Under the English layer only; meaningless under Chinese or the
JP-text paths.

**The live check.** `balioAndSunder_1` and `_2` reach fights 13 and 16
(`BOSS013`: Balio, Sunder and Nina - [`cheats.md`](cheats.md) section 2a);
Stallion is fight 24 in area 67, later. A recorded route into area 67's
fight is needed: the owner records it, then the check is two captures of the
fight, layer on and off.

**Recommendation.** P6 is worth taking as an option: one section from the
player's disc, no code of the fight touched, an obvious visual. P7 rides on
the same work. Neither should be a default (the PSX colours are the
original's, rule 6); if the owner wants the PSP set as a preset, the I33
"presets from the ledger" idea is where it fits.

## 5. The Holy Mantle (P4): refuted

The lead: on the PSP the Holy Mantle suppresses encounters better while
walking straight, and a turn after a long walk brings one on.

### 5.1 The item's record

The Holy Mantle is accessory 21 (`0x15`) in the sibling's `items.toml`. Its
eight numeric bytes are **equal** on PSX-JP (`GAME.EMI` `0x801CAA68` +
21 x 16), PSX-US (`0x801C9E7C` + 21 x 20) and the PSP-EU ELF's copy
(`0x2E6ED8` + 21 x 20) - `psp_stallion.py tables`. No field of the record
marks encounter suppression: the effect is in code, by the accessory's id
(5.2). PSP-JP's `GAME.EMI` is byte-identical to PSX-JP's; its ELF's own copy
was not compared (section 6).

### 5.2 The code that reads it

Every place found that asks "does a member wear accessory 0x15", by the call
`Actor_EquipCount(member, 3, 0x15)` (PC `0x535310`, PSX `0x801C48EC`, PSP-EU
`0xDA9CC`): one site in the PSX's `GAME.EMI`, one in the desert area's
overlay, and the same two on the PSP (a scan of every call to the function
for the constant 0x15 in a 7-instruction window).

| function | PC | PSX-JP | PSP-EU | PSP-JP | verdict |
|---|---|---|---|---|---|
| `Field_ZoneCounterRoll` - the counter set on entering a zone: base + min(Rand & 0x1F, limit), **doubled with accessory 0x15**, halved with 0x14 | `0x52FEB0` | `0x801B4CC4` | `0xC9DDC` | `0xCA0C8` | **same**; the zone table (7 base / limit pairs, PSX `0x801CD944`, PSP-EU `0x2ED0D0`, PSP-JP `0x2ED388`) equal; the PSP halves with `sra` where the PSX uses `srl`, on a value already masked to 8 bits: equal |
| the desert's counter (`AREA189`: a desert by the sibling's `places.toml` name for area 189) - (Rand & 0x1F) + 0x96, doubled with 0x15, halved with 0x14, doubled again on a flag | (area overlay) | `0x801F4DE0` | `0x26B00C` | `0x26B1D0` | **same** |
| `Field_LeaderStepLands` - the step count: + 1, + 2 on directions 2 and 6 | `0x52E580` | `0x801B28D0..` | `0xCC2E8..` | `0xCC5D4..` | **same** |
| `Field_EncounterDue` - the counter against the steps (7/10 of them walking) | `0x530030` | `0x801B4EA8` | `0xCCD94` | `0xCD080` | **one condition removed on the PSP** (5.3) |

PSP-JP against PSP-EU: one masked twin each, as in 3.1.

### 5.3 The one difference: a frame-load guard

The PSX's `Field_EncounterDue`, after the counter and flag tests and unless
bit 0 of `0x80143F02` (`Field_InputFlags` on the PC) is set, refuses an
encounter when the word `0x80143EFC` is above `0xF0`
(`0x801B4FAC..0x801B4FC0`). That word has one writer in the PSX boot EXE:
the DrawSync callback `0x8014B0F8` stores `VSync(1)` into it
(`0x8014B100..0x8014B10C`; `VSync` identified by the sibling's Psy-Q
signature match). In Psy-Q's documented meaning `VSync(1)` is the time since
the last vertical blank in horizontal-line units, so the PSX postpones an
encounter on a step whose last frame finished drawing more than 240 lines
after the blank - a load guard, presumably so a battle does not start on a
frame that has already run over.

**The PSP's `Field_EncounterDue` has no such test** (EU `0xCCD94..0xCCFB4`:
the counter, the walking 7/10 by a `div` by 10, the `+0x12C` bit, the
members' bit 6, then the placement - nothing compared with 0xF0). It has no
DrawSync callback to feed one.

**The PC already behaves like the PSP here.** Its twin keeps the test
(`kScreenCounter` `0x929EE0`, `src/game/event_ops.cpp`), but `0x929EE0` lies
in `.data`'s zero-filled tail and the only instruction in `BOF3.exe` that
names it is that read (`0x5300F4`, one match of the address in the whole
image); our code only reads it. So on the PC the guard never fires.

### 5.4 Verdict

**P4 refuted as a Holy Mantle change**: the item's record, the two
counter rolls that double for it, and the step count are the PSX's on the
PSP, byte for byte in data and instruction for instruction in code. The only
encounter-path difference found removes a render-load guard that applies to
every encounter, worn mantle or not, and it is not a direction or
turning test. Whether that guard is what players felt on the PSX is a
guess not made here. Nothing to toggle: the PC already matches the PSP;
restoring the PSX's guard would need a frame-time measure of our own and is
a separate "what it was" question, recorded for the coordinator.

The "+2 on directions 2 and 6" rule of the step count is a heading-dependent
behaviour that every release shares; it is the nearest thing in the code to
the lead's "walking straight", and it is the same on all three.

## 6. What could not be settled

> **2026-10-08, unified-data step 5** ([`exe-tables-by-build.md`](exe-tables-by-build.md) 4.2): this doc's ELF
> addresses are **file offsets**, 0x80 more than the catalogue's link-time
> offsets (the accessories' `0x2E6ED8` is `0x2E6E58` there); its ability ids
> frame a record parameters-first, so its ids 45, 46, 115, 217 are
> `Ability_Records` 46, 47, 116, 218. The PSP-JP items below are **closed**:
> the PSP-JP ELF's accessory records equal the PC's numbers in all 52 (the
> Holy Mantle's, 21, included), and it renames one ability, id 116 - the
> record the English PSP renames - and key items 2, 5, 7 and 9. Consumable 87
> differs on both PSP ELFs, a PSP change, not an English one.

- **Which frames use palette 3's cells.** The render shows the recolour on
  the cells drawn with that palette; the frame tables that assemble Stallion
  (and whether Balio or Sunder share palette 3 in area 67) were not read.
- **The `0x0A081000` tiles of area 67**: located and sized, content not
  identified (they are 4-bit field textures whose palette was not paired).
- **The PSP-JP ELF's name and item tables**: not located; P7 is confirmed
  for the English PSP only, and the Holy Mantle's record on PSP-JP rests on
  its `GAME.EMI` (identical to PSX-JP) and the code's sameness, not on the
  ELF's data copy.
- **The shared battle engine** on the PSP against the PSX's: not compared;
  only the boss module and the fight's data were.
- **Area 166's `0x8002BE00` row 5** (black to a grey ramp): not rendered,
  not assigned.
- **The PSP-EU consumable 87 flag bit**: found, meaning not read.
- Nothing was seen in play; the live checks of section 4 need a route the
  owner has not recorded.
