# The save editor in the launcher

**Status:** IN PROGRESS (2026-11-10). Stages 1, 3 and 4 of
[`save-editor-spec.md`](save-editor-spec.md) section 16 are built for the PC
port's save files: the format core, the page, and the advanced fields. Memory
cards, conversion, creation and story points (stages 2, 5, 6) are not built.
The format tests pass on 114 real PC saves (section 5). **Nobody has loaded an
edited save in the game yet** (section 6).

The spec says why each thing is as it is; [`save-block.md`](save-block.md) and
its range files hold the evidence for each byte. This page says what was built
and where it differs from the spec.

## 1. Where it is

The launcher's **Mods** tab has a third button on its sub-row, after Data and
Extras: **Save Editor...** (the owner's place for it, 2026-11-10). It opens a
dialog of its own, as the CRT options do (`IDD_SAVEEDIT`, `launcher.rc`).

```
src/launcher/save/block.h/.cpp   the block, checksum, field table, checks, the PC file
src/launcher/save/names.h/.cpp   the lists of names, read from the install and from save-names.tsv
data/save-names.tsv              the reference names; the build copies it beside the launcher
src/launcher/save_dialog.cpp     the window
tools/launcher_host/save_tests.cpp   the format tests
```

`save/` has no window code. The window holds no offset of its own: every row it
shows is made from the field table, which is the one source of a field's offset,
width, signedness and class.

## 2. The window

- **Save file:** the window opens on the first slot that holds a good save. The box lists the sixteen slots `BISLPS00..0F.DAT` of the game folder, each
  with the name, level and play time of its save, and "A different file..." for
  a PC save anywhere else. `BISLPS10..1F` are quicksaves and are not listed.
- **The sections**, on the left: Game, Party, Characters, Inventory, Key items,
  Party skills, Dragon genes, Masters, Fishing. A section with several records
  (the eight characters, the four inventory categories) has a box to select the
  record. The faerie village's three sections and Options are behind the
  advanced box (the owner, 2026-11-10).
- **Area and Position** (Game): the area shows its name beside its number and
  offers the named areas. Position is one field, `x, z` in cells (`47, 15.5`),
  and its list is the known entrances of the area the save is in, each as
  `x, z - from <the areas it comes from>`. A pick from the list sets the
  position and the entry flags; typing two numbers sets the position alone.
- **The fields** of the section: the field, its value, and the value in the
  file when the player changed it.
- **Value:** one box for the field selected. A field with a list (an item, an
  ability, a party member, a switch) offers the list, and a pick from it is set
  at once. Enter, or Set, sets what was typed: a number, a name from the list,
  `12 x 3` for an item and a count. A double-click changes a switch.
- **Show advanced fields** adds the spec's A, D and P classes: ten more
  sections (Faerie village, Faeries, Faerie buildings, Options, Story state,
  Chapter flags, Event flags, Records and finds, Counters, Other bytes) and the
  advanced fields of the others. The chapter, an advanced field of Game, shows
  its label. A derived or
  preserved field is shown and cannot be changed.
- **Write to:** the slot to save to, the opened one by default. **Save** writes
  it.

Nothing is written until Save. A change of file, Reload and Close ask before
they discard a change.

## 3. What a save does

1. Refuses while the game runs (a window of the class the game registers,
   `Bof3`).
2. Sets the fields that the editor owns, **each only where its source changed**,
   so a save nobody edited is written byte for byte:
   - the slot summary from record 1 (name, level, EXP), the play-time copy, the
     two option copies, the cleared byte from event flag 146, the portraits
     from the field order;
   - the battle order, when its members are no longer the field order's;
   - the party row `+0x74C`, by `PartySet_Find`'s rule, from the 19-row table
     read out of the install's `BOF3.exe` (`ExePartyTable`, `exe_image.cpp`);
   - every faerie stamp in use, moved by the amount the battle count changed;
   - the checksum, last.
3. Checks the result (the spec's section 13). An error refuses the save; a
   warning asks. **Only what the edit made wrong is reported**: an issue that
   the file already had is not, so a save the game wrote odd still saves.
4. Writes beside the file, reads the bytes back, copies the file it replaces to
   `<game folder>\save-backups\BISLPS0X-<date>-<time>.DAT`, then replaces. The
   backups are in a folder of their own because the game's list matches
   `BISLPS??.DAT` and holds sixteen entries with no bound
   ([`save-files.md`](save-files.md) section 2).

**Refused:** a chapter that is not 0..19 or is 16; message speed, window colour
and background outside their range; an all-zero play time; a field order that
is not 1 to 3 different members packed from the start, or names a member no
record holds; new members with no party row (or with no `BOF3.exe` to read the
table from: the order can change then, the members cannot); a formation that is
locked or too large for the party; a gene row that is not packed or half empty;
a non-zero byte after the gene bits; the last bit of either find bank; a stamp
above the battle count; an item count outside 1..99.

**Warned:** a party member without the joined bit; the field tail and script
flag `0x40` not agreeing; a level at apprenticing above the level, or with no
master; AP above its maximum; fatigue above 9; an item in two places of one
category; all 32 key item places full.

## 4. Names

Read from the player's install, never from the repository
([`save-editor-spec.md`](save-editor-spec.md) section 10).

- **Item and ability names** come from the name chunks (kind 5, DIV-0008) of
  the language layer's `FIRST.DAT`: `<game>\DAT\<tag>.FIRST.DAT`, else
  `<cache>\loc\<tag>\dat\FIRST.DAT`, for the language the game plays now. The
  chunk tags are the tables' addresses (`ExeNameTableTags`).
- **Only the Latin layers** (`en-*`, `fr-FR`, `de-DE`) give names, where a
  letter is its ASCII byte (`tools/loc_build.py` `ASCII_OF`). An accented
  letter is a two-byte glyph code and shows as `?` in a list.
- **Character and faerie names** are edited as letters on a Latin install: the
  letters, digits, space and `( ) , - . / = ? & ' : ; "` that the font has. Any
  other name, and every name of the Chinese and Japanese installs, is shown and
  written as its glyph codes: `hex:8A 88 8A 89`.
- On an install with no Latin layer the lists show numbers. The window says so.

### 4.1 The reference names: `data/save-names.tsv`

Checked in, as the Jukebox's `music-names.tsv` is, and copied beside the
launcher by the build and the release; the editor reads that copy. UTF-8,
tab-separated: `kind id name x z flags status source`. The editor reads the
first six columns; `status` and `source` are the record of where each name
comes from. A row can be edited or added by hand.

| Kind | Rows | From (2026-11-10) |
|---|--:|---|
| `master` | 17 | The US text, read by index from an install's English layer: the masters window draws system message `0x110 + (bit + 1)` (`MasterWin_DrawList`) |
| `gene` | 18 | The same way: the gene window draws message `0x4171 + (bit + 1)` (`MenuList_GeneWinDraw`). Gene 13's name is `???` in the game's own text |
| `formation` | 6 | The tactics wheel's entry for cell n (`TacticsFormation_Build`), checked against the help lines `0x77 + n`: Normal, Attack, Defense, Chain, Magic, Refuge |
| `fish` | 23 | Kind k is consumable item `0x38 + k` (`FieldPanel_DrawKindRow`); kind 22 copies the fish merchant's name. Where the install gives item names, the editor takes kinds 0..21 from them, in the install's language |
| `chapter` | 20 | Our labels, from [`boss-fights.md`](boss-fights.md) and each chapter file's end: "Ends with <boss>" only where the fight is in the run that ends the chapter |
| `area` | 128 of 200 | 20 from the US text (area captions, region names); 107 cited from one source each (the sibling's `names/areas.toml` and `places.toml`, our docs); 72 areas have no name and show as a number |

The two countdowns are labelled by the names of abilities `0x3E` and `0x15`
from the install's ability names (Celerity and Bonebreak on the US text):
`Skill_CanUse` refuses each while its countdown runs.

Six cited area names do not appear in the US text and may not be the game's
words: Magus's Grove (61), Eygnock Road (69), Mt. Levett (70), Boumou Mountain
(71, 99) and the "Gorge" of Maekyss Gorge (67).

### 4.2 Entrances: read from the install, not shipped

A save's area, position and entry flags are exactly what a door hands
`Field_ChangeArea`: the load passes them to it unchanged. The doors are the
link lists of `Area_Descriptors` in `BOF3.exe` (a descriptor's `+0x20`, 12
bytes a link, `Area_LinkAt`; [`area-entry.md`](area-entry.md) section 1), so
**an entrance into an area is every link of any area that leads to it**.

`ExeAreaLinks` (`exe_image.cpp`) reads them from the player's file with
`Area_LinkAt`'s arithmetic (`(w & 0xFF80) << 8` is the save's 16.16 position,
in half cells; flags `& 0x8F`), indirect links' alternatives included. It
leaves out a link to cell 0, 0 and the one list that eleven areas share as a
placeholder. The editor groups the links of one position and names the group
by the areas it comes from. The values are an exe table's, so they are read
at run time and are not in the repository (the owner's call of 2026-11-09 for
exe tables). An install with no `BOF3.exe` has no entrance list.

Measured by the research pass (2026-11-10): the static read gives 1,542 links
in 192 areas, with no difference from the running game's own dump of the door
sweep, and on the 1,517 doors that sweep drove the leader stood on exactly
these cells. 64 of 86 real PC saves are within 10 cells of an entrance of
their area. 177 areas have an entrance.

Not in the list: the arrivals that a script makes with literal arguments (357
in our source), which can depend on the story state; and the areas no link
leads to (battlefields, the ending).

## 5. Tests

`tools/launcher_host/save_tests.cpp`, built by `tools/launcher_host/` or alone
(`clang++ -std=c++17 -Isrc save_tests.cpp src/launcher/save/block.cpp`). The
fixtures are the player's own saves, given as folders; none is committed.

Run 2026-11-10 on `tools/recipe_saves`, `game\game` and `game\backups`: **939
checks, 0 failures, 114 fixtures.**

| Test | Passes when |
|---|---|
| The table | Every field is inside the block; no two editable fields share a bit; the checksum is no field |
| Text | Names, lists, signed bytes, 16.16 numbers, clocks and item stacks parse and print; out-of-range entries are refused |
| The file | A write, a read back, a second write that keeps the first as a backup, no temporary file left |
| Round trip, each fixture | An unedited block is written byte for byte and has no new issue |
| Own text, each fixture | Every editable field's text set back into it changes nothing |
| One edit, each fixture | Zenny changes itself and the checksum only; a level and EXP edit changes those, the summary and the checksum only |
| Battle count, each fixture | A change leaves no stamp above it |

Measured on the way: one character record holds level 0 in every PC save (the
record that holds nobody yet), so the level field takes 0..99.

## 6. Not done, and where this differs from the spec

- **Not played.** No edited save has been loaded in the game. That is the
  spec's last test and is owed before this is called done.
- **No memory cards, no conversion** (stage 2; `tools/save_convert.py` still
  does that), **no creation, no story points** (stages 5 and 6). A card file
  is refused with a sentence that says so.
- **The page is its own dialog**, not a page of the launcher's window (the
  spec's section 13 "Layout"): the owner's call, 2026-11-10.
- **Equipment does not move the inventory.** The spec's section 13 takes an
  equipped item out of the inventory count. Here a weapon set on a character is
  written to the record and the inventory is left as it is.
- **EXP past the level** is not warned: it needs the EXP curve from the
  install.
- **The button words** are shown and cannot be changed.
- **An entrance is a place, not a promise.** The list does not know the story
  state: an indirect link's alternative is offered whether or not its flag
  holds, and the editor does not check that the area fits the chapter.
- **A bad checksum** asks once, then opens the save for editing; the spec opens
  it read-only until "Open anyway".
- **The pad** does not move through this dialog; the keyboard and the mouse do.
