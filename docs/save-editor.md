# The save editor in the launcher

**Status:** IN PROGRESS (2026-11-11). Stages 1 to 4 of
[`save-editor-spec.md`](save-editor-spec.md) section 16 are built: the format
core, the page, the advanced fields, and since 2026-11-11 PlayStation memory
card files and conversion between a card and the PC port's save files (section
7). Creation and story points (stages 5, 6) are not built. The format tests
pass on 113 real PC saves and on 46 saves of 45 real card files (section 5).
**Nobody has loaded an edited save in the game yet, and nobody has loaded a
save from the PC game on a PlayStation or an emulator** (section 6).

The spec says why each thing is as it is; [`save-block.md`](save-block.md) and
its range files hold the evidence for each byte. This page says what was built
and where it differs from the spec.

## 1. Where it is

The launcher's **Mods** tab has a third button on its sub-row, after Data and
Extras: **Save Editor...** (the owner's place for it, 2026-11-10). It opens a
dialog of its own, as the CRT options do (`IDD_SAVEEDIT`, `launcher.rc`).

```
src/launcher/save/block.h/.cpp   the block, checksum, field table, checks, the PC file
src/launcher/save/card.h/.cpp    the memory card image, its directory, the PlayStation layout of the block
src/launcher/save/namemap.h/.cpp each release's default names, and a save's names changed between them
src/launcher/save/names.h/.cpp   the lists of names, read from the install and from save-names.tsv
data/save-names.tsv              the reference names; the build copies it beside the launcher
src/launcher/save_dialog.cpp     the window
tools/launcher_host/save_tests.cpp   the format tests
tools/launcher_host/save_editor_drive.py   the window driven through a card and a conversion
```

`save/` has no window code. The window holds no offset of its own: every row it
shows is made from the field table, which is the one source of a field's offset,
width, signedness and class.

## 2. The window

- **Save file:** the window opens on the first slot that holds a good save. The box lists the sixteen slots `BISLPS00..0F.DAT` of the game folder, each
  with the name, level and play time of its save, and "A different file or
  memory card..." for a PC save anywhere else or a memory card file. With a
  card open, the box also lists each save of the game on that card.
  `BISLPS10..1F` are quicksaves and are not listed.
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
- **Write to:** the place to save to, the opened one by default: a slot, a
  save on the memory card (replaced), a new save on the card, or "A memory card
  file..." to select a card, which can be a file that is not there yet.
  **Save** writes it. A save written to the other kind of place is converted
  (section 7).

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
exe tables). An install with no `BOF3.exe` has no entrance list. **The lists
are on the discs too**: each area's is in its own `AREAnnn.EMI`, in the section
that loads at `0x801F2C00`, byte for byte the exe's for 191 of 192 areas on
the JP disc (measured 2026-11-10). Reading them there for a disc-built install
is a task under [`IDEAS.md`](IDEAS.md) I58.

Measured by the research pass (2026-11-10): the static read gives 1,542 links
in 192 areas, with no difference from the running game's own dump of the door
sweep, and on the 1,517 doors that sweep drove the leader stood on exactly
these cells. 64 of 86 real PC saves are within 10 cells of an entrance of
their area. 177 areas have an entrance.

Not in the list: the arrivals that a script makes with literal arguments (357
in our source), which can depend on the story state; and the areas no link
leads to (battlefields, the ending).

### 4.3 Records and finds: what each bit records (2026-11-13)

Each row of the section shows the bit's number and, where the install names it,
what the bit records, as the gene rows do:

| Record | Row | The bit is | The name from |
|---|---|---|---|
| Enemies defeated, Enemies identified (`+0x688`, `+0x6C8`) | `Enemies defeated 12 - <enemy>` | the enemy kind | each `AREAnnn.DAT`'s enemy table, the language layer's names over it |
| Skills learned (`+0x6A8`) | `Skills learned 8 - <ability>` | the ability id's low byte | the ability names of section 4 |
| Chests and field objects (`+0x6EC`) | `Chests and field objects 0 - area024 (100, 21): <find>` | the content object's `+5` | each area's placement script in `BOF3.exe` (op `9x`) |
| Passage finds (`+0x72C`) | `Passage finds 4 - area003, area008 (79, 49): <find>` | the passage entry's `+4` | each area descriptor's passage list in `BOF3.exe` |

A find's place is the area and the cell (x, z); after it, what the find
gives: the item's name, or `weapon 12` (its category and number) where the
install has no item names, or the zenny. One cell and find in several areas
(a town's chapter versions of one map) is one entry with the areas listed;
different places of one bit are joined by `; `. A bit with no name keeps its
number alone. The layouts are in [`story-flags.md`](story-flags.md) "Other
flag banks"; `src/launcher/save/finds.cpp` reads them, `ExeAreaFinds`
(`exe_image.cpp`) hands it the exe, and `tools/bank_names.py` is the same
reader in Python (for a watcher: `bank_names.load(game_dir)`).

Like the other names, these are read at run time and are not in the
repository. Enemy and item names need a Latin layer (section 4); the places and
the zenny do not, and come from the exe alone. The enemy table of an area is
read as the game reads it: the cache's `base\dat` and `loc\zh-CN\dat` (or the
install's `DAT\`), then `<cache>\loc\<tag>\dat` (or `DAT\<tag>.AREAnnn.DAT`).
An optional layer (`opt=`, such as `psp-names-*`) is not read: the editor
names an enemy as the language layer has it.

MEASURED 2026-11-13 on the owner's install (`game\`, English, cache built
2026-11-12): **175 enemy kinds, 227 abilities, 170 chest bits and 104 passage
bits named**; each enemy kind carries one name. Under `fr-FR` 165 kinds and
under `de-DE` 164 are named: the language build keeps the PC's name where the
disc's does not fit (`convert_enemy_names`), and that name is in Chinese glyph
codes, which the editor does not show. On the plain PC install (`bof3\`, no
`bof3x.ini`) the same counts come with `--language en-US`, from the
`DAT\en-US.*` layers. On 88 PC saves (the 72 of `tools/recipe_saves` and the 16
slots of `game\game`) every set bit of the five banks has a name: 5,808 enemy,
2,667 skill, 5,813 chest and 3,738 passage bits, summed over the saves (`python
tools/bank_names.py --game game --save ...`; `Enemies identified` is clear in
all of them). The launcher's readers and the
Python tool give the same text for every bit on `game\` (en-US, fr-FR) and on
`bof3\` (en-US), compared with a scratch build of the readers.

Sanity checks of the layout (MEASURED): `start.DAT` (area 24) has no chest bit,
`start_normal.DAT` one, bit 0, named as a find of area 24 - the area
`start.DAT` stands in; the four enemy bits of `start_normal.DAT` (25, 26, 29,
30) are four of the five kinds in area 24's table.

## 5. Tests

`tools/launcher_host/save_tests.cpp`, built by `tools/launcher_host/` or alone
(`clang++ -std=c++17 -Isrc save_tests.cpp src/launcher/save/block.cpp`). The
fixtures are the player's own saves, given as folders; none is committed.

Run 2026-11-11 on `tools/recipe_saves`, `game\game`, `game\backups` and the
card files of section 7, with `--layers` on a `DAT\` folder that holds the five
PlayStation languages built by this version's `tools/loc_build.py`: **1,557
checks, 0 failures, 113 PC fixtures, 45 cards with 46 saves, 15 empty cards compared**
(2026-11-10, before the cards: 939 checks on 114 fixtures).

| Test | Passes when |
|---|---|
| The table | Every field is inside the block; no two editable fields share a bit; the checksum is no field |
| Text | Names, lists, signed bytes, 16.16 numbers, clocks and item stacks parse and print; out-of-range entries are refused |
| The file | A write, a read back, a second write that keeps the first as a backup, no temporary file left |
| Round trip, each fixture | An unedited block is written byte for byte and has no new issue |
| Own text, each fixture | Every editable field's text set back into it changes nothing |
| One edit, each fixture | Zenny changes itself and the checksum only; a level and EXP edit changes those, the summary and the checksum only |
| Battle count, each fixture | A change leaves no stamp above it |
| The card, no fixture | An empty card, a save added (directory frame, check byte, title), read back, replaced (the block's bytes only), a second save with the first's icon, the card file written and kept; a name of six bytes refused; a card file is not a PC file and the reverse |
| Each save of each card | It opens; the two layouts go there and back byte for byte; unedited, the block and the whole card are byte for byte after a put; a zenny edit changes zenny and the checksum on the card and nothing else; the save moves to a new card and reads back the same |
| Each empty card | `EmptyCard` is that card, byte for byte (the emulator's cards whose blocks are zero) |
| The name map, no fixture | Default names take the other set's; the player's are kept and reported; a faerie record nobody is in is not touched; the names go back; a layer's chunks (its disc's set and card file name, the names as the PC plays them); the exe's tables |
| The name map, each built layer | It has its disc's eight and sixty names and its card file name, and the names as the PC plays them |
| Records and finds, no fixture | The script walk finds op `9x` in both arms of an if and in a switch case, skips a bit of `0xFF`, and stops at a byte that is no op; the two find layouts and the passage entries that are no find; one place in two areas is one entry; an area file's enemy table with a layer's names over it, a name with no letter left out, a second name joined; the five banks' rows with a name and without |
| The name map, each card save | The eight names become the other release's and fit a card; they come back; a second trip changes nothing (46 of 46 saves hold default names only: US, Japan, Europe, France, Germany) |

**Against `tools/save_convert.py`** (2026-11-11, `--dump` and a scratch
script): the editor's card-to-PC block is the tool's `psx2pc` block byte for
byte on 32 of 32 saves.

**The window, driven** (`tools/launcher_host/save_editor_drive.py`, 2026-11-11,
a scratch install that plays English and has the US, Japan and French
languages, and copies of two RetroArch cards, Japan and US; 33 checks each, 0
failures): the card opened from the file box and its saves
listed; the first written to slot F, the file equal to `save_convert.py`'s but
for the names, which are the install's (`Ryu` ... `Whelp` from the Japan
card); slot F written back to the card as a new save of the same release,
whose block is the first save's block byte for byte, names too, with nothing
else on the card changed and the card from before in `save-backups`; the same
save written to a card file that did not exist, which holds that one save.

Measured on the way: one character record holds level 0 in every PC save (the
record that holds nobody yet), so the level field takes 0..99.

## 6. Not done, and where this differs from the spec

- **Not played.** No edited save has been loaded in the game. That is the
  spec's last test and is owed before this is called done.
- **A save from the PC game has not been loaded on a PlayStation or an
  emulator.** The card the editor writes is checked for structure only
  (section 7). The other direction, a card save in the PC game, is the path
  `save_convert.py` proved in play on 2026-09-19
  ([`save-interchange.md`](save-interchange.md) section 4), and the editor's
  bytes are that tool's.
- **No creation, no story points** (stages 5 and 6).
- **The named Records and finds rows (section 4.3) are not seen in the
  window.** The readers are checked by the host tests and against
  `tools/bank_names.py` on the owner's install; nobody has opened the page with
  them yet. A disc-built install with no `BOF3.exe` has no chest or passage
  names (the scripts and lists are on the discs too, as the entrances are: I58).
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

## 7. Memory cards and conversion (2026-11-11)

Stage 2 of the spec (its sections 5.2 and 11). `src/launcher/save/card.h/.cpp`
has no window code; the tests of section 5 use it directly.

### 7.1 What opens

A **raw memory card image**: 131,072 bytes that start `MC`. That is the file
of every emulator measured: RetroArch's `.srm` and `.mcr`, Mednafen's `.mcr`,
`.mc1`, `.mc2`, the sibling's psxrecomp `.mcd` (45 files: the sets of the
spec's section 15, and the owner's three European cards of 2026-11-11, one
each from the European, French and German discs, five saves). The extension does not matter; the size and the first two
bytes do. A gzip file (a Mednafen save state) and any other size are refused
with a sentence that says why.

A save of the game is a directory frame in state `0x51`, one card block long,
whose file name ends `BOF3` and two digits, with `SC` at the start of its
block. Other games' saves and deleted entries are not listed and are not
touched. The release is the name's prefix, which each disc holds in its
`START.EMI` (read from the five discs, 2026-11-11): `BASLUS-00422BOF3` (US),
`BISLPS-00990BOF3` (Japan), `BESLES-01304BOF3` (Europe), `BESLES-01319BOF3`
(France), `BESLES-01320BOF3` (Germany). The owner's three European cards hold
exactly these names (2026-11-11). Any save of the game opens and can be
replaced. **A new save is offered for the releases the player has installed
as a language** (section 8.3: the Language box's list), and for the release
of the card save that is open.

### 7.2 One layout in the editor

The editor's block is always the PC layout. A card save is changed into it
when it is read (`PsxToPc`) and back when it is written (`PcToPsx`), so the
field table has one set of offsets. The one difference is the character
record ([`save-interchange.md`](save-interchange.md) section 2): the name is
5 bytes on the PlayStation and 9 on the PC, and the rest of the record is 4
bytes later on the PC.

- `PsxToPc` refuses a record that uses its last four bytes, which have no
  place in a PC record. No save held does (46 of 46).
- `PcToPsx` refuses a name of more than five bytes, with a sentence that
  names the character.
- The checksum's value is the same in both layouts: the bytes that move are
  the same bytes and the bytes that go are zero. Measured on 46 saves: there
  and back is byte for byte.

### 7.3 What a write to a card does

The whole card file is written beside itself, read back, and then replaces
the file; the card from before goes to `<game folder>\save-backups\`, as a
PC save does (section 3). The check that the game is not running applies to
the PC's files only.

- **A save replaced:** bytes `0x200..0x12AF` of its card block change and
  nothing else. The title in the block's header is written again only when
  the hours, the minutes or the level it shows changed.
- **A new save:** the first free directory frame (state `0xA0..0xA3`), the
  first of the numbers `00..02` that the release has free. The game lists
  three saves a release on one card (the spec's 5.2), so the editor offers
  no fourth. The frame gets `51 00 00 00`, the size `0x2000`, next `0xFFFF`,
  the name, and the XOR of its first 127 bytes. The card block is zero past
  the game's block.
- **The icon of a new save:** the header of the save that is open when it
  came from a card; else the header of a save of the game on the target
  card; else an icon the editor draws (one frame, a flame in a ring). None
  of the game's pixels is in the launcher.
- **The title** is what a console's card screen shows; the game does not
  read it. The game writes one of two forms, and the editor writes the same
  bytes: full-width `Breath of Fire 3(n) HHhMMm Lv.LL` on the US release (all
  64 bytes) and the name in katakana with the hours, minutes and level words
  on the Japan release (58 bytes). `n` is the name's number + 1; the level is
  the slot summary's (`+0xCA8`). The European, French and German releases
  write the US form (measured on five saves, 2026-11-11), which is what the
  editor writes for every release but Japan's.
- **The icon flag** at `+0x02` of the block's header is `0x13` (three frames)
  on the US and Japan saves and `0x11` (one frame) on the five European ones.
  The editor copies a header whole and never changes the flag.
- **A card file that is not there yet** is an empty card until Save:
  `MC` and `0x0E` at `0x7F` in frame 0, fifteen free directory frames
  (`A0`, next `0xFFFF`, check byte `A0`), twenty frames of the replaced-sector
  list (`FF FF FF FF`, next `0xFFFF`), frame 63 a copy of frame 0, the rest
  zero. That is byte for byte the 12 empty cards RetroArch made.

### 7.4 Conversion

A save converts when the place it is written to is the other kind from the
place it was opened from. The editor lists what changes and asks first.

- **The names the game gave become the other game's** (section 8): the eight
  characters' and the faeries' default names are written as the destination
  release's. A name the player wrote goes as it is, and the question lists
  it: each release has its own letters, so it can show incorrectly in the
  other game, and the player can change it before the save.
- **The button words `+0xCBC` and the options go with the save.**
- **To a card only:** `+0x1040` (ours, DIV-0191) and the options `+0x7C`,
  `+0x7D`, which no PC screen sets, are cleared (the spec's section 13),
  then the checksum.
- Everything else is the same bytes: the block is one format.

But for the names, the card-to-PC block is byte for byte what
`tools/save_convert.py psx2pc` makes with no name donor (32 of 32, measured
before the name map; the driven run checks it with the names left out). That
tool stays as the reference.

### 7.5 Where this differs from the spec

- **Names:** the spec's 11.3 gives every record the destination's default or
  what the player types. Here only a name that is some release's default is
  replaced (section 8); the player's own names stay.
- **No "keep or use this target's default" choice for the buttons** (11.6):
  they are kept.
- **No name codec per release** (the spec's section 9): a name the player
  wrote on a Japan card shows as its bytes (`hex:D2 E0 AD`) or as the
  install's glyphs for those bytes.
- **One card at a time.** The two file boxes show the saves of one card
  file. A save can go from one card to another: open it, then select the
  other card in "Write to".

### 7.6 Not proved

- **A save from the PC game on a PlayStation or an emulator.** The card is
  right in structure (the directory, the check bytes, the checksum, and a
  block that is a real card save's block when it came from one). Whether the
  PlayStation game reads a PC game's button words and option bytes as the PC
  does is not known. The editor's question says so. This is the test owed
  before the path is called done: Mednafen, or the sibling's psxrecomp.
- The sibling's verifier (`save_tool.py verify`) was not run on a card that
  the editor wrote.
- DexDrive `.gme` and single-save files (`.mcs`, `.psv`) are not opened.

## 8. The name map: default names between releases (2026-11-11)

`src/launcher/save/namemap.h/.cpp`. Every name in it is read from what the
player installed: none is in the repository or the launcher.

### 8.1 The text a save holds

Three places, and no other ([`save-block.md`](save-block.md); every other
name in the game is an id in the save):

| Where | Size | What |
|---|---|---|
| The eight character records, `+0x90 + n * 0xA4` | 9 bytes on the PC, 5 on a card | The character's name |
| The slot summary, `+0xCA0` (5) and `+0xCB6` (4) | 9 | A copy of record 1's name; the editor writes it from the record (section 3) |
| The faerie names, `+0xF10 + 5 r` | 5 bytes, 60 of them | Each faerie's name |

Each release writes these in its own letters: the US release a letter as its
ASCII byte but a full stop as `0x3E` (measured: `Dr.K` is `44 72 3E 4B` in 10
saves); the Japan release one-byte kana; the port two-byte glyph codes; an
English PC install a letter as its ASCII byte and the full stop as `0x2E`.
The filler differs too: a card pads a faerie name with `0xFF`, the PC with
zero.

### 8.2 A name the game gave can be told

Both kinds of name start from a table of the release:

- **Characters:** New Game copies eight default records, the name first
  (`Char_DefaultRecords` `0x64B390` and the whelp's `0x64B80C` on the PC,
  [B](save-block/range-b-characters.md); the same records in the discs'
  `START.EMI`, DIV-0020). The PC has no name entry at New Game, and the
  PlayStation's is for the first character only.
- **Faeries:** when a faerie arrives, `CommuSim_AddRecord` copies the name of
  her trait record into the save (`0x653200 + 20 r` on the PC,
  [E](save-block/range-e-tail.md); sixty 9-byte records in the discs'
  `COMMU00.EMI`). Record `r` is a fixed individual, so the default of record
  `r` is one name in each release.

So a name that equals a release's table entry **for the same record** is a
name the game gave, and the other release's entry for that record is what
that game would have given. A name the player wrote is no table's entry and
cannot be changed.

### 8.3 The sets: from the languages the player installed

A language of the Language box is a layer the player built from their own
disc (`tools/loc_build.py`, DIV-0005). Since 2026-11-11 the builder also
writes into the layer's `FIRST.DAT` **what that disc itself stores in a
save**: one chunk of kind 20, tag 0 (`save_names_chunk`), 361 bytes:

| Bytes | What |
|---|---|
| 1 | Version, 1 |
| 20 | The card file name the release gives its saves, without the two digits, NUL-filled: the first `B?S???-nnnnnBOF3` in the disc's `START.EMI` |
| 8 x 5 | The eight default names, as in `START.EMI`'s default records (the bytes `convert_char_names` reads for DIV-0020) |
| 60 x 5 | The sixty faerie names, as in the trait records of the community overlay, `0xFF`-filled (the bytes the labels' group 16 reads) |

It is written only when all three were found, so a PSP disc's layer has none
(the PSP has no card saves). The engine passes over the chunk
(`dat_load.cpp`: kind 20 is the launcher's); `tools/dat.py` knows it.

The editor makes these sets (`SaveEditorRun`):

| Set | From |
|---|---|
| A disc's own, by language tag, with its card file name | The kind-20 chunk of each installed language's `FIRST.DAT` (`<game>\DAT\<tag>.FIRST.DAT`, else the cache's `loc\<tag>\dat\FIRST.DAT`) |
| `pc`: the install as it plays now | The played language's layer: its default names chunk (kind 10, DIV-0020) and faerie names (kind 15, group 16; an empty entry is the exe's). With no language played, the exe's |
| `pc-<tag>`: each other installed language as the PC plays it | The same two chunks of that layer |
| `pc-original`: the port's own | The exe's two tables, read from the install's `BOF3.exe` |

**The releases offered for a new card save are the disc sets**: one for each
installed language whose layer has the chunk, in the Language box's order.
With none, a card save can still be opened, edited and replaced, and the
editor says how to get a release.

**A layer built before 2026-11-11 has no chunk.** Its language gives no card
release and no disc names until the language is built again with this
version (the setup's rebuild). The names as the PC plays it still come from
that layer.

Built from the five PlayStation discs on this machine (2026-11-11): each
layer has the chunk; 12 of 12 US card saves, 29 of 29 Japan card saves and
the five European ones (one English, two French with `Drago`, two German with
`Welpe` for the eighth name) hold their disc set's eight names; the 103 PC saves made under the English
layer hold the `pc` set's, the 10 made by the original the exe's.

### 8.4 What a conversion does

When a save goes from a card to the PC or the reverse (section 7.4), before
anything else is set:

- The destination set is the disc set whose card file name is the card
  save's, or `pc`. A card save of a release that is not installed keeps its
  names as they are, and the question says so.
- Each character record's name, and the name of each faerie record **in
  use**, that equals the entry of **any** set for that record is written as
  the destination's entry: a character's with zeros after it, a faerie's
  with the destination's filler. A save of mixed names (the sibling's import
  holds Japan's character names and the US faerie names) comes out in one
  release.
- A PC faerie name of the original port can be two glyphs and half of a
  third in its five bytes; our build does not store the half (DIV-0200).
  Both forms are that name.
- Any other name stays as it is, and the question before the save lists
  them ("Character 3", "Faerie 12").
- The summary's name follows record 1, as after any edit.

Measured: all 46 card saves go to another release and back with the same
eight names, and a second trip changes nothing. Driven in the window: a Japan
card save written to an English install's slot F holds `Ryu` ... `Whelp`; a
US save with 20 faeries goes card to PC to card byte for byte; a German, a
French and a European card save each reach slot F as `Ryu` ... `Whelp` and go
back to their card byte for byte. In the list of
a card's saves, the line shows the names as the install writes them, so a
Japan save reads `Ryu`; the save itself is not changed by that.

### 8.5 Not done

- A name codec per release, for names the player wrote.
- The guest's name in record 8 (fight 26's battle extra writes it there and
  takes it away, DIV-0064): it is no set's, so a save made with it would keep
  it. No save held has it.
- A PC save moved between languages on the PC (a save of the original port
  opened in an English install): the sets can do it, the window does not
  offer it.
