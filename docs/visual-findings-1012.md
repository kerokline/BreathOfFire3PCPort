# The owner's visual findings of 2026-10-12: battle framing, equip icons, the save menu, the PSP's 16:9 data

**Status:** MEASURED (2026-10-12, evening, the PSP-only round's group VF,
branch `psp/visual-fixes` from `psp/round` `8fdd1f36`; the seven held discs,
`BOF3.exe` and the end batch's two caches `C:\Users\kerok\eb\c_pspeu` /
`c_psxeu`, read only; headless self-tests only, **not played, no capture**)

The owner watched the end batch's live disc-only installs on 2026-10-12 (a
PSP-EU `en-150` install and a European PSX disc-only install) and reported
three things, A to C; the coordinator added D the same evening. Which install
showed each is the end batch's to capture; this doc says, per finding, which
images carry the cause. No table value is in this doc, the code or the
recipes (CLAUDE.md rule 1): addresses, counts, deltas and rules only. Game
facts (what a screen is in play) are the owner's; any here are marked as
guesses.

## 0. What came out

| | Cause | Installs | Done here |
|---|---|---|---|
| **A** combatants shifted left, an enemy partly off screen (EB: on both disc-only installs, not on the PC's) | **Three gaps of the encounter mode, on every disc-built image** (section 1.4): `GameMode5_Turn`'s approach pairs `0x656AA4` (the battle's view centre is not stepped toward the enemies: the scene sits lower right, the Gooey off the left edge), `Battle_OpenEnemyNames`' column places `0x656A34` (the name box at the top-left corner), and the turn order's percent row `0x64E328` (the party's ability bonus 0 at level classes 0 and 1: Teepo before Ryu). **Not D56**, not the enemy records (equal to the PC's but the names), not the formation table: the lead's `Formation_SlotVectors`+0x106.. is the zone music sets on the PSP image (1.1)| every disc-only install, PSX and PSP | the three filled (`twin`), with two more battle tables (wake chances, tally steps); the music sets on PSP builds (`msets`), two PSP-EU battle runs. D56 stays an owner's option (1.3) |
| **B** the battle equip screen's slot icons all one glyph | the equipment window's six slot icons **and categories** (`0x66B5B4`, 16 bytes) and the BATE equip screen's copy (`0x64AE18`) are zero on every disc-built image: every slot draws icon 0 and names its item from category 0 | every disc-only install, PSX and PSP | filled (`twin`, from the disc's `BATTLE.EMI#15`), every byte `BOF3.exe`'s |
| **C** the save menu's numbers and "space buffer" | the menu's number formats are zero on every disc-built image: the figure pair `"%3d/   "` / `"   /%3d"`, the `--------` no-next-level text, `"%8d"`, `"%3d/%3d"`, the save slot's play-time colon (`"  :"` - two spaces, the buffer that puts the colon between the hours and the minutes) and the play-time format itself. The leading spaces are **Capcom's** (the PSX's `START.EMI#8` has them byte for byte), not a Latin-text buffer of ours. The empty slot's own text (`0x669CCC`) and the slot number's `"%X"` stay zero: no disc carries either | every disc-only install | filled (`twin`, from `START.EMI#8`) except `0x669CCC` and `"%X"`; options in 3.3 |
| **D** the PSP's data framed for its 480 x 272 screen | of the three widescreen values the PSP keeps in data (WS's survey), only `MsgBox_PlacementTable` entries 3..6 (x -32 / +32, y unchanged) reach a PSP-built image; the tile size entry 11 is already the PC's there, SCENA17's letterbox is code. `Title_Sprites` entry 10 and the title / demo pages are the PSP's own title art, not placement | PSP-only installs | the side placements put back to the 4:3 frame at import (`unwide`); the PSP image now equals the PSX's there |

The gaps per build (`recipes/exe-gaps.tsv`, `gapcount.py`, against
`psp/round` `6c2ae0c9`, which has `main`'s `GAME.EMI` place): ranges / range
bytes / game-value bytes the PC has non-zero

| Build | before | after |
|---|---|---|
| `psx-jp` | 330 / 3,281 / 789 | **307 / 2,998 / 728** |
| `psx-us` | 351 / 4,085 / 1,323 | **329 / 3,819 / 1,262** |
| `psx-eu-en` | 349 / 4,069 / 1,319 | **327 / 3,803 / 1,258** |
| `psx-fr` | 351 / 4,157 / 1,350 | **329 / 3,891 / 1,285** |
| `psx-de` | 352 / 4,173 / 1,351 | **329 / 3,887 / 1,286** |
| `psp-jp` | 316 / 2,897 / 740 | **278 / 2,432 / 664** |
| `psp-eu` | 351 / 3,112 / 791 | **311 / 2,612 / 709** |

## 1. A: the combatants shifted left

### 1.1 The lead, read

The coordinator's lead was `Formation_SlotVectors` +0x106..0x20C, zero on
the PSP-built image where every PSX image fills it. Read against the PC's
bytes and its readers (`imgdiff.py`, `where.py`, scratch), that range is not
the formation table:

- `Formation_SlotVectors` (`0x6698B0`) is read by `Encounter_PlaceMember`
  (`inventory_ops.cpp`) and `Party_PlaceInFormation` (`scena_sx2.cpp`) at
  `8 * ((last^2 + formation) * 3 + member)`: 30 rows of two dwords, to
  `+0xF0`. Rows 30 on hold byte values that are no (dx, dz) pair.
- `0x6699A0` is `battle_e3`'s s8 pairs (`kSpecialOffsets`), equal to the PC's
  on both images.
- `0x6699B8..0x669A48` are byte lists of 3-byte entries and `0x669A48..0x669AC0`
  15 records of (list pointer, count): `kMusicSets`, read by
  `Area_PickMusic` -> `Area_ZoneTrack` (`window_task.cpp`): a zone's
  music track by story flags. The gap's `game-value` rows at `+0x19C..+0x20C`
  are the 15 counts; the pointer words are the layout's.

On a PSP-built image those lists and counts are zero, so a zone with a music
set plays track 0's entry - **a music defect, not a visual one** (not heard;
a guess at what it sounds like is the owner's to make).

The PSP-built image (`c_pspeu`) against the PSX-EU one (`c_psxeu`), every
run where they differ (`imgdiff.py`: 282 runs): none in the encounter
tables (`0x6698B0..0x669CB0` is the PC's byte for byte on PSX-EU; on PSP-EU
only the 95 music-set bytes differ), `Encounter_SlotChance`'s facing offsets
`0x669CB8` equal on both. The battle data containers are the PSX's on the
PSP too: `BATTLE.DAT`, `BATTLE2.DAT` and `BATE.DAT` differ from the PSX-EU
cache's only in the UI page `0x1A080200` (112 bytes; section 4)
(`datdiff.py`). So **no data either install reads places combatants
differently from the PC's install**.

### 1.2 Two PSP-EU battle runs, filled on the way

The PSP-EU image leaves two battle runs zero that every PSX image and the
PSP-JP image place (PSP-JP through `psxmap`, which reads the PSP's file at
PSX-JP's addresses; PSP-EU's battle module sits 0x3000 higher, so it missed):

- `0x64E4E4..0x64E4FC`: `BattleItemCmd`'s six slot categories (`kEquipCats`,
  `kUnequipCats`, `battle_e4`) and the facing step pairs `0x64E4F4`
  (`Escape_Begin`'s run, `BattleFxDash_Rise`'s side, `boss_sg`'s
  `kFacingOffsets`). Zero: an escape and a dash take no side step - small
  (the pairs are four units), not a scene shift.
- `0x64E524..0x64E52A`: `Escape_Chance`'s row. Zero: every escape chance 0.

Both are on the PSP disc verbatim in `BATTLE/BATTLE.EMI#15`; filled by
`twin` (section 5).

### 1.3 D56, the left edge the port does not test (first reading; superseded by 1.4)

`Encounter_Place` places the party and the enemies and then requires every
combatant's footprint on screen through `Encounter_OnScreen` `0x5928F0`
(`inventory_ops.cpp`), or the encounter is retried. The PSX compares each
projected corner as an unsigned short against 319 / 239, so a corner left
of or above the screen fails too; the port compares floats against 320.0 /
240.0 only ([`inventory_ops.md`](inventory_ops.md) 5, known defect **D56**,
"a fight placed partly off the left or top edge is possible on the PC").
So an encounter can start with an enemy partly off the **left** edge and
never off the right - the owner's report, as code. It is the port's on
every install, the PC's included; a disc-only install neither adds nor
removes it. What would make it more frequent on one install (the field
maps' cells, which differ by language in a few areas, `AREA030` chunk 8 for
one) is not measured.

Not done: the fix is a divergence (the PSX's lower bounds restored), a
change of every encounter's placement, so of every recorded battle route and
statehash reference from the first encounter on. The owner's call (the
psx-twin rule says PC-only, so a port repair), drafted in "For the other
files" as DIV E (not a ledger entry: a proposal, D56 and owner-review).

**What only a capture can confirm:** that the owner's fight had a
combatant's projected footprint at x < 0 when it was placed (a
`BOF3X_STATEHASH_DUMP` of the encounter frame, or `mem_watch.py` on the
slots `0x939F00` and the members' spots `0x7E06E0` at the battle's start,
projected), and on which install.

### 1.4 After the end batch's captures: the cause

EB's `pairs/battle_three.png` (the combat route, frames 480 and 1740: the
PC install, PSP-EU and the European PSX disc-only install) and AB's
`eu_combat` sheets (the PC install under `en-150` too, so not the language):
on both disc-only installs the party sits lower right, the Gooey is never
drawn (off the left edge), its name box is at the top-left corner, and Teepo
acts before Ryu (the owner: Flare is Teepo's, Heal is Ryu's - the turn
order, not the enemy's action). The PC install has none of it. Read and
measured:

- **The view centre.** AB's report puts the battle in mode 5, area 29:
  `GameMode5` is the encounter mode (`GameMode5_Steps` `0x656A84`).
  `GameMode5_Turn` (`mode_rest.cpp`, original `0x495EC0`) steps the upper
  words of `Field_Kind2X` / `_Kind2Z` - the fight's centre, which
  `Encounter_AimCamera` aims at - by the s8 pair `0x656AA4[formation * 2]`
  (`kApproach`). Those 8 bytes (`GameMode5_Steps+0x20` in `exe-gaps.tsv`,
  `game-value`) were zero on all seven builds, so the centre stays at the
  party: the camera aims short of the enemies, the party lands lower right
  and the Gooey off the left edge, where D56 lets it stand.
- **The name box.** `Battle_OpenEnemyNames` (`battle_sprites.cpp`, original
  `0x494A80`) takes the name column's x, y from `0x656A34` by the layout
  byte (`kNamePlaces`). Those 16 bytes (`Boss_SetupTable+0xE0`,
  `disc-unplaced`) were zero on all seven builds: the box at (0, 0).
- **The turn order.** `Battle_BuildTurnOrder` (`battle_damage.md` 4): party
  value = agility + bonus x percent / 100, the percent from the s16 row
  `0x64E328` by level class (`kTurnPercent`: 100, 125, 150, 200, 250 in
  shape). Its first two words (classes 0 and 1) were zero on all seven
  builds (`Battle_CommandBoxes+0x60`, `disc-unplaced`): a low-level party
  member's ability bonus counts for nothing, so a member with a higher bare
  agility goes first. Already filled by this branch's first `twin` set.

**Measured against the coordinator's questions:**
- (1) The actors' stats: the enemy records of area 29 (`0xC2000`) in a US,
  a European and a PSP-EU disc-built cache equal the PC's `DAT\AREA029.DAT`
  except the name bytes (+0x00..+0x09 of each 0x8C record; `enemycmp.py`,
  `locchunks.py`; the language layer's name chunks sit at `0xC2048 + k x
  0x8C`, the PC's stride). So no record is read at a wrong offset, no
  ability list or other index differs - in particular **no off-by-one on
  our side**; the sibling's keying question (`enemy_ai_data.md` 3) was not
  met here and is not settled by this. The party's stats come from the
  save and `Char_RecalcStats` over the item tables (placed, the PC's).
- (2) `Char_DefaultRecords`: the bytes the disc-built images differ in are
  record offsets 0..8 only - the names (58 bytes on PSX-EU and US, 37 on
  PSP-EU; `x_*` images against `BOF3.exe`). `NewGame_InitCharacters` alone
  copies the records (a new game); a loaded save does not read them.
- (3) What else the sort reads: the agility words, the level-class rows
  `0x64E374` and the enemies' jitter `0x64E334` (placed, the PC's), and
  `Rand`.
- Also equal to the PC's on both images: every encounter table
  (`0x6698B0..0x669CC0`, `0x660B00..0x660B60`).

**Filled** (`twin`): `0x656AA4..0x656AAC` from `ETC/GAME.EMI#0` (the PSX's
`GameMode5_Steps` + 0x20 there: `0x801C8B6C` on JP, the PSX twin's table
`0x801C8B4C` + 8 words) and `0x656A34..0x656A44` from `BATTLE/BATTLE.EMI#15`,
every byte `BOF3.exe`'s, on all seven builds. With them, two more battle
tables zero on every build: `Battle_WakeRoll`'s chances `0x64E3E8`
(`kWakeChance`: a sleeper never woke by the roll) from `BATTLE.EMI#3`, and
the result tally's steps `0x64ADD0` (`kTallyStep`) from `ETC/BATE.EMI#0`.
Still zero: `GameMode3_Steps+0xC` `0x656A80`, the facing jitter
`GameMode3_Leave` reads (`Rand & 3`): its PC bytes are in no `GAME.EMI`.

**What only a capture can confirm:** the combat route's battle on a
rebuilt disc-only cache - the Gooey in view, its name box beside it, Ryu
before Teepo - matching the PC install frame for frame (AB's harness).
D56 stays an owner's option for the left edge in general.

## 2. B: the battle equip screen's slot icons

### 2.1 What draws them

Two screens draw an equipment slot list in battle:

- **The battle menu's equipment window** (`BattleEquipWin_Draw` `0x59D640`,
  `battle_e7.cpp`, kind 3 of `Window_Handler8Kinds`): six rows, the
  member's equipped items `+0x12..+0x17`; per row `Menu_DrawIcon8(x + 6, y,
  B(0x66B5B4 + r))` and the item's name `Item_NamePtr(B(0x66B5BC + r),
  item)`. So `0x66B5B4` is six slot icons and `0x66B5BC` six slot
  categories (each followed by two zero bytes).
- **BATE's equipment screen** (`BattleExtra_Equip*` `0x42DC00..`,
  `battle_e1.cpp`, record 7's equipment): the same 16 bytes' copy at
  `0x64AE18` (the gap row named `BattleExtra_EquipLeaveSteps+0x18`). Its
  commit `BattleExtra_EquipCommit` `0x42E250` (`rest_3a.cpp`) reads the
  categories half `0x64AE20` for `Inventory_Remove` / `Inventory_Add`; the
  reader of the icons half was not located here (by the shape, the screen's
  list draw).

`Menu_DrawIcon8` (`menu_windows.cpp`) draws an 8 x 8 cell at u =
`(icon + 7) << 3` of the UI page: icon 0 is one fixed cell, whatever its
picture. Which cell is a flame is not established here (the UI page is not
rendered); the owner's "flames" is consistent with every slot drawing the
same cell.

### 2.2 The bytes per build

`recipes/exe-gaps.tsv` before this branch: `0x66B5B4..0x66B5C4` a gap
(`disc-unplaced`, named `BattleMenuWin_EquipItemsSteps+0x38`) and
`0x64AE18..0x64AE28` a gap (`disc-unplaced`) on **all seven builds**; the
images (`c_pspeu`, `c_psxeu`) hold 16 zero bytes at each. On the PC: six
distinct non-zero icons and the categories 1, 2, 2, 2, 3, 3 in shape (the
values are not copied here). So on every disc-only install:
- every slot draws icon 0 - **all six the same glyph**, the owner's flames;
- every item's name is looked up in category 0 - **the names are likely
  wrong too** (a guess; the owner reported only the icons);
- `Item_CanUse(2, member, 0, item)` asks about the wrong category;
- in the BATE screen, a change of equipment removes the new item from, and
  returns the old one to, category 0's inventory list instead of the
  slot's (read from `BattleExtra_EquipCommit`; not played) - a gameplay
  defect, not only a visual one.

The field's equipment panel reads its own `Equip_SlotIcons` `0x663660`,
placed on every build, so the field menu is right; only the battle's copies
were missed.

### 2.3 Where the disc has them, and the fill

The PlayStation's battle module carries the 16 bytes verbatim in
`BATTLE/BATTLE.EMI` section 15 (`0x800B6EF0` US, `0x800B3E30` JP,
`0x800B6E30` PSP-EU: `where.py`), one copy, shared by both screens'
twins; the PC duplicated it. The places missed it because its neighbours on
the PC are pointer words the disc does not share (no 16-byte window crosses
into a placed segment). Filled by the new `twin` rule (section 5) on all
seven builds, both PC copies from that one disc copy.

Also filled with them, the same class of item-icon table:
- `0x66AF24..0x66AF34`, the battle result window's icon by item kind
  (`BattleResultWin_DrawItem` `0x598750`, `battle_e7.cpp`, `kIconKinds`):
  zero from `+2` on **every** build (`Win2_TitleBoxStates+0x26`,
  `game-value`), so every dropped item but kinds 0 and 1 drew icon 0. From
  `ETC/GAME.EMI#0`.
- `0x66A49C..0x66A4AC`, `Item_IconByKind` (the item-trade screen,
  `effect_1g.cpp`): a gap on JP and German only. From `ETC/GAME.EMI#0`.

**EB's capture** (`pairs/nue_equip.png`, cutsceneAndNue's equip window,
both disc-only installs): six flames, and the names wrong as 2.2 read them
- the weapon slot shows "Sea Bream" (the weapon's id read as a consumable),
the other five "Nothing". That is the window this fix covers
(`0x66B5B4`): icons and categories both. **Only a capture confirms** the
fix on a rebuilt cache.

## 3. C: the save menu's numbers and the space buffer

### 3.1 What the save slot draws

`Menu_DrawSaveSlot` `0x576960` (`field_o.cpp`): the box; the slot number
`sprintf(0x6639D4, slot)` in the 8-pixel font at (x + 9, y + 5); with a
summary, the name (DIV-0029's inset), the level (`0x64D3EC`), the play time
`sprintf(0x654830, h, m)` at x + 0x27 and **the colon text `0x6639D0` drawn
at the same x** (twice, y + 0x24 and + 0x27); without one, the empty slot's
text `0x669CCC` at (x + 0x13, y + 2).

The time format is the hours, a **space**, the minutes; the colon text is
**two spaces and a colon**. Drawn at one x in a monospaced font, the colon
lands in the format's space. That is the space buffer: without it the colon
sits on the hours' first digit, to the left. The same device makes the HP /
AP figure pair: `"%3d/   "` and `"   /%3d"` drawn at one x
(`field_s`, `rest_2c`, `rest_2h`, `map_field_objects`).

### 3.2 The bytes per build

Before this branch, on **all seven** builds (`exe-gaps.tsv`, `game-value`,
named `Menu_ButtonSets+0x174..`): `0x6639B0..0x6639C4` (the current
figure's format, the `--------` text `Char_ExpForLevel`'s -1 draws, the
EXP format's first byte), `0x6639C8..0x6639CC` (the count format's first
half) and `0x6639D0..0x6639D8` (the colon text and the slot number's
`"%X"`), and `0x654830..` (the play-time format, named
`EffectKind55_States+0xC`; 9 to 12 bytes by build). Read in the images: the
colon text, `"%X"`, `"%3d/   "`, the dashes and the time format all zero on
PSX-EU and PSP-EU. So on a disc-only install a save slot shows **no slot
number, no play time and no colon**, the figure pairs and counts draw
nothing, and the EXP panel draws nothing where the PC writes dashes.

What the owner saw - "the 0 or - drawn offset to the left" - is not matched
to one of these by measurement (no capture). A guess: a slot's figures or
the EXP panel's no-next-level text drawn without their spaces or not at all.
The capture is the check.

### 3.3 Where the disc has them, and what stays

The PlayStation's menu overlay `ETC/START.EMI` section 8 carries each string
byte for byte - `"%3d/   "`, `"   /%3d"`, `"%8d"`, the dashes, `"%3d/%3d"`,
the time format and the colon text beside it (`0x801D0DB0` / `0x801D0DBC`
on every PSX build; `where.py`) - in another order than the PC's, with
nothing around them the PC shares, so no 16-byte window placed them. The
spaces are Capcom's. Filled by `twin` from `START.EMI#8` on all seven
builds (section 5).

Two stay zero, with no disc copy found:
- **`"%X"` at `0x6639D4`**, the slot number's format: in no `ETC/` overlay
  of any disc (`where.py`: the only hits are three-byte coincidences in
  `BMAGIC/` and `SCENARIO/` files and in `BOOT.BIN`'s `.text`). The PSX draws
  its slot number another way. Options: (a) leave it (no slot number on a
  disc-only install); (b) the engine's own format string, as `exe_own.cpp`
  holds the port's other platform strings (`exe-engine-own.md`): it is a
  printf format of the port's choosing, not a Capcom value - recommended.
- **`0x669CCC`**, the empty slot's text: the port's Chinese (空记录); no
  disc's `START.EMI` holds an `Empty` / `No data` ([`village-text-scan.md`](village-text-scan.md)
  3); `loc-build-disc-only.md` 5.4 lists it among the eight a language layer
  cannot reach. The staged next localisation PR (the empty slot blank as the
  US disc draws it) is the place for it; nothing here.

No language path carries a Latin "space buffer" of ours for these: the
ledger's Latin layout entries (DIV-0029's save-name inset, DIV-0027's
chooser) are in code and apply to every install with a language
overlay, disc-only included. The buffer the owner remembers is in these
format strings, which a disc-only image lacked.

**EB's capture** (`pairs/battle_three.png`, frame 1740): the battle item
list's counts draw as stray glyphs - "Jr" where the PC draws "× 2", "JK"
where it draws "5/128". Those are three formats: the item row's count
`"*%2d"` (`0x653EC0`, `Menu_DrawItemRow`'s `kFmtCount`; the `*` is the
font's ×), the list's `"%3d/%3d"` (`0x6639C8`) and the battle windows'
`/` and `:` (`0x64E31C`). All three were gaps on every build; the last two
are 3.3's fills, and `0x653EC0..0x653EC5` is a `twin` from `ETC/GAME.EMI`
since (the rest of that gap row, `0x653EA0..0x653EC0`, stays: a fishing
label set). Why a zero format drew "Jr" rather than nothing was not read.

**What only a capture can confirm:** the save menu and the item counts on a
rebuilt disc-only cache: slot numbers absent (until 3.3's `"%X"` is
decided), times with their colons, figures and counts drawn.

## 4. D: the PSP's data framed for 480 x 272

### 4.1 How the PSP frames, and what reaches our install

The PSP draws in the PSX's 320 x 240 space and shifts **every primitive's x
by +32** in its converters, presenting a 384 x 216 window (rows 12..228)
scaled 1.25x to 480 x 272 ([`psp-widescreen.md`](psp-widescreen.md) 2.3,
2.4). That shift is code; our install runs the PC's code, so it does not
happen here. What does reach a PSP-only install is the PSP's **data**
re-authored for that 384-wide frame: anything Capcom moved by 32 to keep its
distance from a screen edge that is now 32 further out.

### 4.2 Every PSP-supplied byte or chunk found carrying framing

| Item | What it positions | PSP against PSX (rule) | In our 4:3 (320 x 240) | In our wide mode (DIV-0041, 426 x 240, +53) |
|---|---|---|---|---|
| `MsgBox_PlacementTable` `0x66AE10` entries 3..6 (PSP image, from `BOOT.BIN`; IM's placed difference) | the dialogue box's side placements (u16 x, y; `(x + 0x23) << 4`) | **x -32** on entries 3 and 5, **x +32** on 4 and 6; **y unchanged** (not "y 32 more" as `psp-image.md` 1 has it; WS agrees); entries 0..2, 7..11 equal (`imgdiff`, `msgbox` deltas) | **before the fix** the left boxes 32 px nearer the left edge, the right boxes 32 px nearer (or past) the right edge - "text at odd places". Whether a right box is cut depends on its width, not measured | the boxes sit 21 px inside the band (the PSP anchored them 32 past the old edge, ours widens by 53): what Capcom intended, less 21. `widescreen.md` 3d planned 92 / 279 for wide mode on the PSX's values |
| `Title_Sprites` `0x653850` entry 10, the u16 at `+0x68` (PSP image) | a title-screen sprite's source rectangle in the title page (`Title_Sprite`: u, v, w, h, clut) | **+32** in that one field (`imgdiff`: the one differing byte) | texture layout, not screen placement (WS's reading, `psp-widescreen-placements.md` 2.8, agrees): the PSP's rectangle into the PSP's own title page, which a PSP-only install also takes; normalising it alone would cut the PSP's art wrongly. Kept | the same |
| The battle window's tile size table `0x64E268` entry 11 (WS: 321 on the PC, **385** in `BOOT.BIN`) | `BattleWin_DrawTile`'s full-frame tile | not in the PSP image: the PSP map takes that entry's low byte from `BATTLE/BATTLE.EMI#3` (`data.toml` of `c_pspeu`: `0x64E294` one byte from the overlay, the rest from `BOOT.BIN`), so the image holds **321**, the PC's and the PSX's (read in `c_pspeu` / `c_psxeu`) | nothing to undo | nothing to undo (WS's code-side rule) |
| SCENA17's letterbox (WS: the PSP draws a crop instead) | the ending's bars | code (`Scena17_DrawLetterbox`, ours); no data byte. The three `Scena17_EndSteps`+0x7DC bytes the PSP image differs in are text (letters), not a rectangle | nothing to undo | WS's (a) |
| The title page `FIRST.DAT` 6 (`0x1A080400`, SI's stand-in from `AREA024.EMI` 9) and `DEMO.DAT` 0..2 (`0x12/16/1A080400`) | the title and demo screens' art | different art: 72 of 128 rows of `FIRST` 6 differ, full width; `DEMO` 0 and 1 most of their bytes (`datdiff.py`, `pagediff.py`) | the PSP's title art read through the PC's sprite rectangles: anything Capcom moved within the page for the PSP is cut or misplaced. Needs a render | as 4:3, centred |
| The UI page `0x1A080200` (in `BATTLE`, `BATTLE2`, `BATE`, `START`, `COMMU02`, `AREA030` ...) | menu glyphs and icons | 112 bytes in rows 72..95 of the page (stride 256): small glyph changes, not a layout (presumably the PSP's button glyphs - a guess) | not framing | not framing |
| `SCENA17.DAT` 1, `COMMU02.DAT` 0, area pages (SI's `psp-base` 206) | the ending's page, the village board, area art | content changes (`datdiff`: 757 / 22 bytes; areas larger) | not measured for framing; none is a coordinate table | - |

Not data, so not in a PSP-only install: the PSP's cull bounds, its
full-frame fills at `(-32, 0, 384, 240)`, `AREA172`'s sprite at (256, 190)
and SHOP's clamps (-202 / -152) are all code (`psp-widescreen.md` 3a rows 1..4,
6, 8..12); our code is the PC's.

`Formation_SlotVectors` (finding A) is **not** widescreen-framed: the
PSP's `BOOT.BIN` holds rows 0..2 verbatim and the image's first 30 rows
equal the PC's; the PSP gap past it is the music sets (1.1).

### 4.3 The data-side answer: three values found, one reaches the image, undone at import

WS's code-side survey (`psp/widescreen-survey` `213ed805`,
`psp-widescreen-placements.md` 4..6) found the PSP's widescreen values in
data to be three: the placement table's side x values, the battle tile
size entry 11, and the SCENA17 rectangles. Measured on the data side:
only the first reaches a PSP-only install (4.2). So the normalisation is
one rule, built (WS's proposal (c), the coordinator's instruction):

- **`unwide`** (`psp_exe.py`, applied in `exe_tables.disc_image` after the
  fills): on a PSP build, `MsgBox_PlacementTable` entries 3..6's x go back
  to the 4:3 frame by the PSP's own rule undone - x + 32 for a placement
  left of the centre (160), x - 32 right of it. The values come from the
  player's `BOOT.BIN`; the rule holds addresses and the 32. The recipe
  writes a build's `unwide` row only when the result equals `BOF3.exe`'s
  bytes (both PSP builds: section 6).
- **The byte check:** after it, the PSP-built image equals the PSX-built
  one in `0x66AE1C..0x66AE2A` (section 6), as it already did at
  `0x64E294`.

With that, a PSP-only base is framed for 4:3 like a PSX or PC base, and
any widescreen anchoring is our code's (DIV-0041, WS's (a)). The other
options the coordinator asked to cost, for the record:
- *A PSP base defaulting to our wide mode with the PSP's values scaled*
  needs the same eight bytes undone first anyway (113 / 258 are right for
  32 columns, not for 53, and would move twice under a wide rule): (b)
  is a prerequisite of (a), not an alternative. The wide default itself
  is DIV-0041's open call (`widescreen.md` 5).
- The title page's art (different pixels) cannot be undone by any rule;
  it stays the PSP's, with the PSP's own `Title_Sprites` rectangles into
  it. What it looks like in 4:3 is the capture's to show.

## 5. The fills: three rules in `tools/psp_exe.py`

- **`twin`**: a short run the disc carries verbatim in the overlay whose
  code reads it. `psp_exe.TWINS` names each PC range, the disc file and what
  it is (addresses and names only); `recipe` finds the first copy of
  `BOF3.exe`'s bytes in that file's sections, as the formation wheel's place
  is found (`psp-image.md` 2), and the import copies the player's disc bytes
  from there. Every disc build. Since the end batch's findings also the item
  row's count format, the world maps' tables (6.1) and the encounter mode's
  and the battle's tables (1.4); 28 rows per PSX build, 30 per PSP build.
- **`msets`**: the zone music sets on a PSP disc. `BOOT.BIN`'s `.data` holds
  the 15 records as relocated list pointers with counts, the lists packed
  (3-byte entries, no padding); found by its relocations alone (the one run
  of as many records as the PC's layout words at `0x669A48`, each +0 a
  relocated pointer below the run, rising, each +4 a count that fits before
  the next list). The counts and the lists are copied to the PC's places,
  which the layout's pointer words name; a list is copied with the bytes up
  to the next list (`Area_ZoneTrack` reads the entry past the last as the
  fallback), as far as the PC's room reaches. PSP builds only (the PSX's
  boot EXE is placed by the map).
- **`unwide`**: section 4.3. Not a fill: applied to the placed bytes after
  the fills (`exe_tables.disc_image`, one call), only for a build whose
  recipe has the row, which `recipe` writes only when the rule gives
  `BOF3.exe`'s bytes.

Every row is kept only when every byte it writes equals `BOF3.exe`'s
(`psp_exe.py recipe`: "0 not BOF3.exe's" on every row). `fill` writes only
bytes nothing else filled, so a PSX image changes only inside its gap rows.
Two runs (`0x64E4E4`, `0x64E524`) are `twin` rows on the PSP builds only:
the PSX builds place them through `exe-places.tsv`, and a twin there split
that place and left one byte of `Battle_HolyAffinityTable` unplaced (seen in
a first regeneration, then limited).

`recipes/exe-psp.tsv` gains 13 `twin` rows per PSX build (135 bytes) and 15
per PSP build (165), one `msets` row per PSP build (173) and one `unwide`
row (8). `exe-layout.tsv` loses seven words that are no pointers or are
numbers now placed: `0x6639AC`, `0x6639C4`, `0x6639CC` (the format strings
`"%3d"` / `"%8d"`, whose bytes looked like pointers into `.data`), `0x64E328`
and `0x66AE1C..0x66AE28`'s four (numbers the image now holds as the PC's);
`exe-pointers.tsv` the same three words. `exe-places.tsv` is unchanged.

## 6. Checks

| Check | Result |
|---|---|
| `importer.py check`, `exe_tables.py check`, `psp_exe.py check` | 0 errors (`exe_tables`: 18,837 pointer words, the layout 19,278 words, 441 numbers) |
| `ledger_check.py` | 113 entries, 0 errors, 2 notes (the drafts below are not in the ledger) |
| `psp_exe.py recipe`, seven discs | every row "0 not BOF3.exe's"; `msets` 173 bytes on both PSP builds; `unwide` "4 side placements back to the 4:3 frame" on both |
| The PC path: `exe_tables.py build --source bof3/BOF3.exe` against `psp_coord/ref_jppc` (rebuilt at `d2afc98f`), after the merge | `data.bin` and `data.toml` byte-identical |
| The PSX-EU image (`exe_tables.py build`, the tools before the merge) against the end batch's `c_psxeu` (`bytecheck.py`) | 74 bytes changed, **all inside the old gap rows**, all now `BOF3.exe`'s; 613,051 to 613,125 bytes equal to the PC's |
| The PSP-EU image against `c_pspeu` (the same) | 197 bytes changed, all now `BOF3.exe`'s: 191 inside the old gap rows and 6 the `unwide` bytes; 611,575 to 611,772 equal. (The end batch's caches are gone since; not repeated after the merge) |
| D's byte check: PSP-EU against PSX-EU, new images | equal at `0x66AE1C..0x66AE2A` and at `0x64E294..0x64E296` |
| `recipes/exe-gaps.tsv`'s PSX rows | only removed or shrunk in the filled ranges; the one row that changes shape is `0x6639D0..0x6639D8` to `0x6639D4..0x6639D8` (`"%X"`, still a gap); no new row |
| After the merge (`6c2ae0c9`, the merged build): exe host self-test, PSX disc-only scratch install `C:\b3g` (IM2's US cache, the new US image) | exit 0; `inject: 10088 ours`; `self-test only: done`; `place 675 ranges 6424 bytes`, `gap 329 ranges 3819 bytes` |
| exe host self-test, the real install, the new US image | exit 0; `628157 of 638976 bytes equal; every class that must equal does` (IM2 after main: 627,996); no `place` range differs from `BOF3.exe`; `inject: 10087 ours` |
| exe host self-test, PSP-EU-only scratch install `C:\b3p` (TG's `new_pspeu` cache; the new PSP-EU image; `BOF3X_LANG=en-150`) | exit 0; `inject: 10088 ours`; `self-test only: done`; `place 587 ranges 5777 bytes`, `gap 311 ranges 2612 bytes` |
| exe host self-test, the real install, the new PSP-EU image | exit 0; `628811 of 638976 bytes equal; every class that must equal does`; no `place` range differs |

Commands: `regen.sh` (`exe_tables.py recipe`, `psp_exe.py recipe`,
`exe_tables.py recipe`, `exe_own.py measure`, `exe_tables.py gaps` over the
seven discs), `images.sh`, `selftests.sh` (`st.sh`: a fresh ini-less copy of
this worktree's build per run), `bytecheck.py`, `gapcount.py`, `pcimg.sh`,
and the readers `imgdiff.py`, `where.py`, `datdiff.py`, `pagediff.py`,
`sweep.py`, in the session scratch `psp_vf/`. They print values from the
discs and `BOF3.exe`; none of that output is in the tree. Not run: a full
`importer.py build` of each cache (only `base/exe` changes, and that was
built and compared directly), the launcher host suite and `dialog_test`
(no launcher change), `BOF3X_SHADOW='*'` (no `src/` change).

## 6.1 Areas 16 and 45: the crash on entry (EB's defect 2)

EB: `caughFish` (area 16) and `masterAndManillo` (area 45) stop on both
disc-only installs at area entry - `MoveCmd_AttachOffset`
(`field_objects.cpp` 265) reading a null `+0x0C` vector table of the area's
descriptor (`psp_eb\rt\caughFish_b1.log`: `CRASH 0: reading 0x00000000`,
area `0x10`, task 0 state 2; `symstack.py` names the frames, roughly:
`Field_Frame` <- `Field_WaitTransition` <- `GameMode_Enter`).

Read: `Area_Descriptors[16]` (`0x5E5DE8`) and `[45]` (`0x5F74A0`) have a
null `+0x0C` **in `BOF3.exe` too** (`Area_Descriptors[4]` has one), so the
PC never calls `MoveCmd_AttachOffset` in those areas; on a disc-only
install something there does (an attached object: `Field_ObjectFollow`, or
an effect's `+0x1C` attachment, `effect_1a` / `effect_2f` / `field_c3`).
What the two areas share on every disc-built image are gaps in their own
tables: `Area16_Buttons` / `Area45_Buttons` (`disc-unplaced`, the legend's
key search), `Area16_DriftUV` / `Area45_DriftUV` (`port-relaid`) and, on
some builds, `Area*_Record8Anims` (`port-relaid`: an animation and `+0x2A`
per record-8 object - zero picks animation 0 of bank `0x46`). The same
tables of areas 65, 87 and 88 (world maps the routes do not enter) were
gaps too.

**Filled** where the area's own overlay carries them verbatim (`twin`,
pinned to `WORLDnn/AREAnnn.EMI#13`): `Area16_Buttons`, `_Record8Anims`,
`_DriftUV`; `Area45_Buttons`, `_Record8Anims`, `_DriftUV`; `Area65_Buttons`,
`_Record8Anims`; `Area87_Record8Anims`; `Area88_Record8Anims` - every byte
`BOF3.exe`'s, on every build where the overlay holds them (the recipe run:
"0 not BOF3.exe's"). Not on any disc in their PC shape, so still gaps:
`WorldMap33_DriftUV+0xA`, `Area65_DriftUV`, `Area87_DriftUV`,
`Area88_DriftUV` and its `+0x78` (the port's re-laid drift textures,
DIV-0055's class).

**Not verified:** that these fills stop the crash. The headless self-test
does not enter an area, and no live run was allowed. A zero
`Record8Anims` picking an animation that attaches is the likeliest of the
three (its object is placed at the leader on entry, `Area16_Record8Place`);
if the crash survives, the next step is a probe on `MoveCmd_AttachOffset`'s
callers in area 16 (which object, its type and `+0x1C`) on a disc-only
install, against the PC's.

**Superseded (section 8):** the crash was not these tables but
`WorldMap_Records`' zero area words; `wmrec` fixed it (AB: both routes run
to their end on PSX-EU). The table fills above stay: every byte the PC's.

## 7. Not verified

- **No capture, no play.** Which install showed A, B and C; what icon 0
  looks like; whether the battle equip names were wrong; what the owner's
  "0 or -" was. Each section's last paragraph says what to capture.
- A's cause (1.4) is read from the code and the bytes and matches EB's and
  AB's captures point for point; the fix itself is not yet seen in a run.
- The music sets' effect on a PSP-only install (track 0's entry in zones
  with a set) is read from `Area_ZoneTrack`, not heard.
- D's framing is measured as bytes and deltas, and the side placements are
  undone by rule; the title page and a side box were not rendered in
  either view. Whether the PSP's title art reads right through the PSP's
  own `Title_Sprites` rectangles in our 4:3 frame is the capture's.
- Not looked at: the PSP-only gaps outside these findings (`Sprite_ScreenMargins`,
  `MapCell_Upright*`, `Area_CellHooks`, `EventScript_OpLengths`+0x10,
  `ItemUse_HandlerIndex`+0xD0, `ConfigMenu_Steps`, `Char_WhelpSlot`, ...);
  `sweep.py` (scratch) counts which gap rows' bytes appear verbatim on a
  disc - many are short coincidences, so a sweep is no substitute for a
  reader-pinned `TWINS` entry.

## 8. The second batch (AB's reports, 2026-10-12 late; the owner away, live runs allowed)

Built on `psp/round` `05fccd7d` (the harness merged), committed as
`cfc307a6`, `3f472427`, `5d1b32c1`, `133d72fd`:

- **`wmrec`**: `WorldMap_Records`' area words (+0x18 of eleven 0x1C-byte
  records, `0x653910`), zero on every disc build, so `WorldMap_RecordIndex`
  never found a world map and answered 11 - the "none" index, whose dwords
  are `EffectKind01_States`: the world map ran effect kind 1, whose draw
  calls `MoveCmd_AttachOffset` with the area descriptor's null `+0x0C`.
  **That was the area 16 / 45 crash** (`llvm-symbolizer` on AB's
  `L\bof3x.dll`: `MoveCmd_AttachOffset` <- `EffectKind01_Draw` (`effect_1a.cpp`
  368) <- `Field_Frame` <- `Field_WaitTransition` <- `GameMode_Enter`). The
  words come from `GAME.EMI#0`'s own table of the same shape, found by them.
  AB after it: caughFish and masterAndManillo run to their end on PSX-EU.
- **`boxxy`**: the dialogue box tables `kBoxXY` `0x66ADD8` / `kBoxAltXY`
  `0x66ADE8` (WA's lead): a gap on PSP builds (zero: every box at the top
  left - AB's PSP message boxes). `BOOT.BIN` holds them as s16 pairs with
  entries 3..6 framed for the 384 view; read back by the rule undone (3 and 5
  x + 32, 4 and 6 x - 32) into the PC's u8 layout, every byte `BOF3.exe`'s
  ("0 not BOF3.exe's" on both PSP builds). Not re-run on a PSP install (below).
- **Link lists** (`twin` rows from `link_rows`): the first three bytes of the
  exit list (`Area_LinkAt`, descriptor `+0x20`) of 30 areas were zero on
  every disc build (`links.py`); each list is in its area's own
  `AREAnnn.EMI#13` verbatim. Area 112's stays short on PSX-EU (open).
- **Walk poses and the world-map area list** `0x66ADBC..0x66ADD8`
  (`kWalkPoses`, `Area_ClassifyPending`'s eleven areas): zero on every disc
  build, so entering a world map took transition 1 where the PC takes 11
  (the swing-in): the dumps at ticks 310..330 (`dumps/`, `dumpdiff.py`) show
  `0x937F98` 0x0B on the PC and 0x01 on PSX-EU. From `GAME.EMI#0`.
- **Formats** (`twin`): `"%d"` `0x5E10C0` (the result's EXP and the zenny,
  `BATL_END.EMI`), `"AP%2d"` `0x66AF8C` (the gene costs, `BATTLE.EMI`),
  `"%3dZ"` `0x6641DC` (the shop's cost, `SHOP.EMI`), `"*%2d"` `0x653EC0`;
  `"%s"` `0x654900` only on the PSP builds (no PSX copy in `BATL_END.EMI`).
- **`abc-known`**: `pc-title-mark`, the PC port's (R) on the title (the
  owner: the PSX has none); the PC's title art is `DEMO.DAT` / `FIRST.DAT`'s
  page `0x1A080400` (5,062 bytes differ from the US disc's section,
  `pcvsdisc.py`) and `Title_Sprites` entry 1's width (the port 10, the discs
  12; a gap, so the disc-only title draws no mark). Not ledgered: the port's
  title art wants a DIV (draft below).

Checks at `133d72fd`: `importer.py check`, `exe_tables.py check`,
`psp_exe.py check` 0 errors; `ledger_check` 0 errors; every rule row
"0 not BOF3.exe's"; the PC path's image identical to `ref_jppc`; the new
images against the previous ones lose no byte that was `BOF3.exe`'s
(`prevcheck.py`: PSX-US +216, PSX-EU +207, PSP-EU +143, 0 lost); the exe
host on `C:\b3g`, `C:\b3p` and the real install exit 0, no `place` range
differing (628,373 / 628,954 bytes equal). Gaps against `6c2ae0c9`:
PSX-US 351 to **285** ranges (game-value 1,323 to 1,141), PSX-EU 349 to 286,
PSP-EU 351 to 286 (791 to 626).

AB at `5d1b32c1` (PC against PSX-EU, `report_vf2_*.md`): combat 81 of 87
frames equal, the rest the load screen's `"%X"` and the title; caughFish
and masterAndManillo no longer crash; masterAndManillo still parts at tick
322 (the swing-in, fixed at `133d72fd`, not re-run).

## 9. Open, for the follow-up round

- **Areas 16 / 45 crash:** fixed (`wmrec`, 8); masterAndManillo's world-map
  entry fixed at `133d72fd` by the area list but **not re-run** - next: AB
  masterAndManillo on PC / PSX-EU (and PSP-EU).
- **Load screen (`"%X"`, ") 16", the stray glyph after "Ryu"):** all three
  are the missing `"%X"` `0x6639D4`: `Crt_sprintf` writes only a NUL, and
  `Text_DrawFont8` (Capcom's quirk: it stops on the byte *after* a NUL) draws
  the stale tail of the previous slot's "00 16" (`dumps/*.174.bin`:
  `0x904BA0` "2\0" on the PC, "\0" then "3.." on PSX-EU). Next: the owner's
  call on the engine-own string (recommended).
- **"You gain 3 EXP!":** the number's `"%d"` `0x5E10C0` filled (8); not yet
  seen in a run - next: AB combat frame `t002610`.
- **The PC en-150 empty-record word:** `0x669CCC` is the port's Chinese on
  the PC and no layer writes it (`loc-build-disc-only.md` 5.4's eight); the
  staged localisation PR (the empty slot blank, as the US disc draws it)
  covers it. Not touched.
- **PSP message boxes at the top left:** `boxxy` (8); **not re-run** on
  PSP-EU - next: AB with the PSP side once `C:\Users\kerok\ab\c_pspeu` is
  rebuilt (see the note below).
- **PSP title framing:** the PSP's own title page and `Title_Sprites`
  rectangles (4.2), kept; next: the owner's look at a PSP-EU title capture.
- **The Nue divergence (PSP, from tick ~4530):** not investigated past
  reading: candidates are P8 (Ryu's PSP forms in the arenas, `psp-stand-ins.md`
  4.4) and DIV-0104's level table changing timing and so `Rand`; next: AB
  cutsceneAndNue with `--statehash` PSP against PSX, the first differing
  battle page.
- **`kBoxXY` / `kBoxAltXY`:** done for the PSP builds (`boxxy`); on PSX they
  are placed.
- **The shop drift (PSX-EU, tick 2880..3150):** not data in `.data`: the inn
  opens `SND\OYASUMI.DAT` (the kind-0 jingle, the port's MP3 of
  `S_XA00.STR`), which a PSX disc-only install lacks (`open ... FAILED` in
  `runs/vf2_shop/psx-eu/bof3x.log`), and the screen stays black longer than
  on the PC. Next: the jingles on a disc-only install (`sound-import.md` 7,
  the owner's open call), or the stream's failure made to end the wait
  (a DIV).
- **Shop t420..450 (PSX-EU walking ahead):** seen at `5d1b32c1` with the
  walk poses still zero; expected fixed at `133d72fd`; not re-run.
- **More disc-unplaced runs** one overlay alone carries (twelve, listed by
  `unplaced.py`: `Area42_Placements+0x28`, `Area72_Weights+0x50` (area 73's),
  `Area94_EffectArgsD`, `BossGolem_Hooks+0xC`, `EffectKind36_Frames`,
  `EffectKind2C_States+0xC`, `EffectKind44_SparkColours`, `EffectKind49_V3/V4`,
  `EffectKind8A_States+0x24`, `EffectKind18_08_States+0x8B`,
  `EffectKind18Sub3B_States+0x14`): drafted as `TWINS` rows, not committed
  (not regenerated or checked).

**Left outside the worktree:** the AB caches and installs under
`C:\Users\kerok\ab` were rebuilt by `abc_run.py prepare` with this branch's
tools (`c_pc_eu`, `c_psxeu`, `g_psxeu`); `c_pspeu` is a partial build (a
`prepare` interrupted by me) without its stamp, so `prepare` refuses it -
it wants removing (not allowed to this session) before the next PSP run;
`g_pc\BISLPS00.DAT` is the masterAndManillo recipe save from the run I
stopped (the owner's save was absent there, so nothing of the owner's was
overwritten). The owner's own installs, `C:\b3g`, `C:\b3p` and the scratch
caches were read only.

## For the other files

**Folded in 2026-10-12, night** (the coordinator's second pass, branch `psp/ledger2`): DIV-0101's two amendments and DIV-0103's (unwide, boxxy) are in the ledger; DIV E is **not** a ledger entry - a proposal, recorded in D56 and owner-review; DIV F is **DIV-0114**. The fixed defect is **D251** (with **D252** the icons and formats, **D253** the areas 16 / 45 crash); section 9's load screen, inn and title are **D254**, **D255**, **D256**. `psp-image.md`'s row corrected; `disc-only-install.md` 3 was already updated; README row, owner-review and HANDOFF (the follow-up round, section 9 one line each) are in.

Not edited here; the coordinator folds them in and numbers them.

**DIV-0101, amended again** (the gaps entry). Add after IM2's amendment:
- Amended 2026-10-12, evening (the owner's visual findings; group VF,
  [`visual-findings-1012.md`](visual-findings-1012.md)): more runs no
  longer zero on any disc-built image, each read verbatim from the overlay
  whose code reads it (`twin`, `psp_exe.TWINS`): the battle equipment
  window's slot icons and categories and the BATE screen's copy
  (`BATTLE.EMI#15`), the battle windows' `/` and `:` texts and the party's
  ability-bonus row (`BATTLE.EMI#3`), the menus' figure, count, EXP and
  play-time formats and the play-time colon (`START.EMI#8`), the result
  window's and the trade screen's icon-by-kind tables and the item row's
  count format (`GAME.EMI#0`), the encounter mode's approach pairs
  (`GAME.EMI#0`), the enemy name column's places and the wake chances
  (`BATTLE.EMI`), the result tally's steps (`BATE.EMI#0`), the world maps'
  buttons, drift and record-8 animation tables (each area's own
  `AREAnnn.EMI#13`); on
  PSP builds also the zone music sets (`msets`, from `BOOT.BIN` by its
  relocations) and, on PSP-EU, the battle item command's categories, the
  facing step pairs and `Escape_Chance`'s row. Every filled byte equals
  `BOF3.exe`'s. Still zero: the save slot number's `"%X"` and the empty
  slot's text (no disc carries either).
- Verification: as section 6 of that doc.

**Known defect, for numbering (fixed here):** on every disc-only install
the encounter's view centre was not stepped (`0x656AA4` zero), the enemy
name box sat at the top-left (`0x656A34`), the turn order dropped a low
level party member's ability bonus (`0x64E328`), a sleeper never woke by
the roll (`0x64E3E8`) and the result tally had no steps (`0x64ADD0`) -
the owner's "combatants shifted left" and EB's defect 3. D56 is unchanged
and stays the owner's call (DIV E: D56 updated, not ledgered). This defect is D251.

**DIV E (proposed, not built; the owner's call)** - *An encounter refuses a
combatant left of or above the screen, as the PlayStation does.*
- Original: `Encounter_OnScreen` `0x5928F0` refuses a footprint corner
  projected at x >= 320.0 or y >= 240.0 only; a corner at x < 0 or y < 0
  passes, so a fight can start with a combatant partly off the left or top
  edge (D56).
- New: the PSX's test, the projected corner as unsigned shorts against
  319 / 239 (`FUN_801C805C`), so the left and top edges refuse too.
- Why: a port defect against the PSX twin (the psx-twin rule: PC-only, a
  port repair); the owner's 2026-10-12 report.
- Also in the PSX version? No: the PSX refuses.
- Cost: every encounter placement can change from the first battle on, so
  every battle route and statehash reference after it is re-recorded; the
  fuzz compares the original's bounds, so the new test rides behind a flag
  set after the self-test (DIV-0065's pattern).

**DIV-0103, amended** (a PSP-only install's base is the PSP's). Add after
its "New":
- Amended 2026-10-12, evening (group VF, [`visual-findings-1012.md`](visual-findings-1012.md)
  4; WS's survey `psp-widescreen-placements.md` 5 (c)): the PSP's base
  without its 16:9 re-framing. Of the three widescreen values the PSP keeps
  in data, one reaches a PSP-built image of `.data`: `MsgBox_PlacementTable`
  entries 3..6, the side placements, which the PSP moved 32 toward its
  wider frame's edges. The importer puts them back by that rule undone
  (`unwide`: x + 32 left of the centre, x - 32 right of it; the values read
  from the player's `BOOT.BIN`, the recipe's row written only when the
  result equals `BOF3.exe`'s), so the image equals a PSX-built one there and
  any widescreen anchoring is DIV-0041's, in code. The battle tile size
  entry 11 is already the PC's in the PSP image (the map takes that byte
  from `BATTLE.EMI#3`); the ending's letterbox is code. Kept as the PSP's:
  `Title_Sprites` entry 10 (a source rectangle into the PSP's own title
  page, which the install also takes).
- Verification: `psp_exe.py recipe` ("4 side placements back to the 4:3
  frame, 0 not BOF3.exe's" on both PSP builds); the new PSP-EU image equals
  the PSX-EU one at `0x66AE1C..0x66AE2A`; the exe host self-test over the
  real install with the PSP-EU image (every class that must equal does).
  Not rendered.

**DIV-0104**: unchanged (its two tables are the PSP's game data, not
framing); its text's "the PSP's" image now also carries the `twin` /
`msets` fills, every byte the PC's, which DIV-0101's amendment covers.

**`docs/psp-image.md`** section 1's `MsgBox_PlacementTable` row: "each x 32
less and y 32 more" is wrong; entries 3 and 5 are x -32, 4 and 6 x +32, y
unchanged (`imgdiff.py`; WS agrees), and since this branch the import undoes
them.

**DIV-0101, the second batch (add to the amendment above):** also no
longer zero on any disc-built image: the world maps' area words
(`WorldMap_Records`, `wmrec`, `GAME.EMI#0`), the 30 areas' exit lists (each
area's own overlay), the walk poses and the world-map area list, and the
`"%d"`, `"AP%2d"`, `"%3dZ"` formats; on PSP builds the dialogue box tables
(`boxxy`, below). Still zero: `"%X"`, the empty slot's text.

**DIV-0103, the dialogue boxes (add to its amendment):** a PSP base's
`kBoxXY` / `kBoxAltXY` come from `BOOT.BIN`'s s16 pairs with entries 3..6
put back from the 384-wide frame (x -/+ 32 undone) into the PC's u8 layout,
every byte `BOF3.exe`'s.

**DIV F (proposed; numbered DIV-0114, worded as the disc-only install's divergence from the port's title):** *The PC port's title page.*
- Original: the PlayStation's title page and `Title_Sprites` (the owner: no
  (R) mark).
- New (the port, 2001): its own title page `0x1A080400` (`DEMO.DAT` /
  `FIRST.DAT`) with a registered mark, and `Title_Sprites` entry 1's width
  10 where the discs have 12. A disc-only install draws the disc's title.
- Why: the port's change, recorded so the harness's `pc-title-mark` rule
  has a ledger entry; whether a PC install should draw the PlayStation's
  title is the owner's call (the owner prefers the disc's data where both
  exist; the policy is the coordinator's to take up).

**`docs/disc-only-install.md`** section 3: the list of zeros loses the
battle equip window's icons and categories, the menu formats and colon, the
result window's icons; the save slot's number (`"%X"`) joins the empty
slot's text as what stays zero.

**`docs/owner-review.md`**: two calls - D56's fix (DIV E); the slot
number's `"%X"` (leave, or the engine's own format string). D's
normalisation is done (DIV-0103's amendment); whether a PSP base defaults to
wide mode stays with DIV-0041's open call.

**`docs/README.md`**: a row for `visual-findings-1012.md`.
