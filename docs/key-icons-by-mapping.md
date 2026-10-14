# Key icons tied to the mapping: every place the game draws a button, and what it would take

**Status:** MEASURED (2026-10-13, the owner away; branch `away/1013-keyicons` off
`0a6ecc1b`). A proposal: nothing is built, no `src/` change. Every claim cites the
measurement that produced it; the scripts and their outputs are in
`analysis/shots/keyicons-1013/tools/` (game-derived, never committed), the captures in
`analysis/shots/keyicons-1013/` with a README. Not seen by the owner.

The owner, on the dock's button legend ([`disc-data-inventory.md`](disc-data-inventory.md)
4.2, Q5): "US discs changed the default keys, so they also changed the default icons. The
proper fix is to make all the renders of the key icons tied to mapping instead of their
original draw - can you investigate how difficult that would be?"

## 0. The answer in short

- **Six places draw a button** (section 2): the Config screen's Controller panel, the
  world map's legend (eleven copies of one draw), the fishing banner's line labels, the
  dock's parts-menu legend, the item lists' L1 / R1 corner art, and the dialogue's button
  mentions in nine minigame areas. All of their code is ours already.
- **Two of them already follow the mapping**: the Config panel (since DIV-0051) and the
  world-map legend, which on the PlayStation draws the glyph of whatever button the save's
  word holds. The port painted that legend over with static keyboard words and removed its
  glyph sheet. The other four are baked: art, a fixed label byte, or text.
- **The US release did not "change the icons"; it permuted the four face buttons
  everywhere at once** (section 3): the default words, every hard-coded test that names a
  face button, the art and the text, by one permutation P - JP circle becomes US cross,
  cross becomes triangle, triangle becomes square, square becomes circle. Measured: 44 of 44
  changed face-button tests in the overlays follow P, none goes another way; the dock's and
  the fishing's tests, art and text agree with it.
- So the PC (Japan's code with Japan's icons) is **consistent today on its own data**. Its
  icons go wrong in four situations: (a) a keyboard player (the icons name PlayStation
  buttons, the world map names keys that may be rebound); (b) a rebound pad; (c) a Western
  language layer or disc install, whose text and art name the US buttons over Japan's code
  - **a defect of ours today in nine areas' dialogue** (section 9); (d) a Config swap, for the
  sites whose code reads the save's words while their icon is baked.
- **The design** (section 5): one function, the glyph of a PlayStation bit under the live
  physical bindings and the device last used, and every draw site asking it for the bit its
  own code tests. Cost: **about 6 to 9 working days for everything**, of which the core and
  the glyph sets are 2.5 to 3.5 and the three sites a route reaches (Config, world map,
  fishing) another 1.5 to 2.5 (section 6).
- **A "US button layout" is then free of game logic**: P applied in the physical layer
  (a launcher preset), and every icon follows. It gives a Western player the US feel on
  Japan's code without one changed test.

## 1. Method

All static, on `bof3/BOF3.exe`, the shipped DAT files, the JP and US PlayStation discs
(`CDImage/`) and the A/B/C harness's caches (`C:\Users\kerok\ab\c_psxus`, `c_pspeu`). The
tools, run from the worktree (copies in `analysis/shots/keyicons-1013/tools/`):

| Script | What it measured |
|---|---|
| `input_census.py EXE pc_funcs.json symbols.toml OUT` | every catalog function reading `Input_Held` / `Input_Pressed` / the repeat word `0x7E01B8` or the nine action words `0x903580..0x903592`, with the immediate masks the code tests: 118 functions, 77 of them reading the words (`input_census.tsv`) |
| `input_sites.py EXE symbols.toml LO HI` | the same per instruction in a range, labelled by function (the fishing machine below) |
| `callargs.py EXE pc_funcs.json symbols.toml 57D860` | every call of `Menu_DrawPiece` with its immediate arguments |
| `piece_users.py`, `pieces.py` | the piece lists and the piece table `0x663C8C` (the UI page) |
| `page.py`, `pix.py` | kind-1 pages rendered as grey indices (DAT chunks and EMI sections) and pixel dumps |
| `mips.py`, `emi_find.py` | the PlayStation twins (MIPS32, capstone), found by section hash and by constants |
| `perm_scan.py JP.cue US.cue` | every face-button test in every EMI section the two discs share, JP against US (`perm_scan.txt`) |
| `text_buttons.py pc\|us DIR` | the button mentions in the area text blocks (an icon glyph after a colour control) |

## 2. The sites

### 2.1 The Config screen's Controller panel (the one already done)

- **Draw:** `Config_DrawControllerCell` (original `0x461C00`, ours in
  `src/game/config_text.cpp`). The original drew a key letter from `0x653818..0x65382C`
  (`S A C Z X V`, read with `hexat.py`) and a glyph through `0x66A2FC` (eight one-byte
  strings `b c d X f g o p`; `b..g` are the port's circled numerals 1..6 and `X` an X letter,
  rendered from `FIRST.DAT`'s table: `pc_icons.png`). Capture
  `01_config_original_cn.png`: `V (1)`, `C X`, `X (2)`, `Z (3)`, `S (6)`, `A (5)`.
- **What decides the icon:** the action's save word (`0x903580 + 2k`), the first of circle,
  cross, triangle, square, L1, R1 set in it. **The mapping is consulted** - the swap layer
  (DIV-0051's icon column, `01_config_ours_en.png`). The keyboard letters were the default
  table's, never the live one (D58); DIV-0051 dropped them under an overlay.
- **Physical layer:** not consulted. A player who rebinds keys or pad buttons in the launcher
  sees the PlayStation button the action is on, not the key or pad button that sends it.
- **Route:** `tools/recipes/config_controller.txt`.

### 2.2 The world map's legend (eleven copies)

- **Draw:** `WorldMap_DrawFrame` (`0x404390`) and its ten area copies `Area16_`, `45_`,
  `65_`, `87_`, `88_`, `104_`, `115_`, `121_`, `151_`, `152_DrawFrame`
  (symbols.toml; ours in `world_map.cpp` and the `area_w*.cpp` groups). Three label sprites
  (1, 2, 3 of `0x5EF618`) and, for rows 1 and 2, a key glyph.
- **What decides the icon** ([`world-map-hud.md`](world-map-hud.md) 1.2): row 1 the first of
  the six `(mask, sprite)` entries of `0x5EF670` with a bit in **word 0** (Speak), row 2 the
  same over **word 6** (Action); row 3 (START, Camp) has no search. The table
  (`hexat.py 5EF670`): triangle sprite 6, square 8, circle 10, cross 12, R1 14, L1 18 - 8 x 8
  cells at u 64..95, v 80..111 of the dial page in dim / lit pairs. **The PlayStation's
  legend follows the mapping.**
- **The art:** on the JP and US discs the labels carry the default button baked in
  (`06_dial_page_pc_jp_us.png`: JP circle / cross / START, US cross / triangle / START, the
  US words Enter / Guide / Camp) and the key-glyph sheet sits beside them. **The port's page
  replaces the labels with `SPACE` / `X` / `ENTER` and pictograms, and blanks the glyph
  sheet** - the same block in all 16 world-map areas ([`importer.md`](importer.md),
  "the dial page is port art"). The PC's code still draws the glyph cell; it is empty.
  So the PC's legend is static keyboard text: it lies when a key is rebound, as D58's
  column did.
- **On a Western disc-only install** the PC's code (Japan's words) draws the live glyph over
  the disc's baked US icon: `02_worldmap_legend_psx-eu.png` shows a red circle over the baked
  cross (Enter) and a cross over the baked triangle (Guide) - a defect (section 9).
- **Route:** `worldMapAndAreaTransition.txt`, `caughFish.txt` (area 16, frames 420..750).

### 2.3 The fishing banner's line labels

- **Draw:** the banner's line records `0x653B98` carry a label byte (`+4`); the label is the
  string `0x66A2FC[label]` (`b c d`, circled 1 2 3) in colour `0x653C00[label]` (2, 1, 6)
  ([`fishing-text.md`](fishing-text.md) 2). Ours: the kind-0xF line states and, under a
  Latin layer, `FishingText_Label` (`src/game/fishing_text.cpp`, DIV-0069's icon).
  Capture `03_fishing_banner_*.png` ("Wind reel... (circle)").
- **What decides it:** the record's label byte - **baked**.
- **What the code tests** (`input_sites.py 528000 52B600`): stage 1's buttons
  (`LeaderPanel_S1Buttons` `0x528A90`) are hard-coded, `Input_Pressed & 0x74` exactly -
  triangle 0x10 the gear menu (stage 9), circle 0x20 cast (stage 2), cross 0x40 quit, L1 0x04
  look (stage 10); the cast's confirm in `LeaderPanel_S2Wait` reads **word 7**
  (`Field_ConfirmButtons`), its cancel word 8; the reel and the catch read `0x20`, `0x40`,
  `0x60`, `0xA000`, `0xC0` hard-coded; the gear menus (`LeaderPanel_S9Menu`,
  `ExtraSlots_*`, `ChoiceMenu_*Page`) words 7 and 8. A mix: the labels are right for the
  hard-coded stage-1 tests whatever the Config says, and would be wrong for a word-read step
  after a Config swap.
- **Route:** `caughFish.txt` (area 30, mode 8).

### 2.4 The dock's parts-menu legend (area 128)

- **Draw:** `EffectKind5E_DrawPanel` (`0x47FBE0`, ours in `src/game/rest_3e.cpp`): one FT4 of
  the area page at u `0xC0`, v `0x50` - the legend is **baked art** (`05_dock_legend_page_pc_jp_us.png`).
- **What the code tests:** `EffectKind5E_Pick` (`0x47F610`): triangle 0x10 the memo board,
  cross 0x40 quit, circle 0x20 place the part - hard-coded. The JP twin `0x801F7590`
  (`SCENA10.EMI` section 0, found by its section hash with `emi_find.py`) tests the same three
  bits; the US module tests **square 0x80** the memo, **triangle 0x10** End, **cross 0x40**
  place (`mips.py`, US `0x801F7660..0x801F769C`). Each disc's legend names its own code's
  buttons.
- **Correction:** [`disc-data-inventory.md`](disc-data-inventory.md) 4.2 reads the PC / JP
  legend's first icon as a square. It is a **circle** (`pix.py` on the page: rounded corners,
  where the US square's are sharp) - and the code's confirm is circle 0x20.
- **On a Western disc-only install** the US page is drawn over Japan's code (cross OK where
  the code wants circle): a defect (section 9). On the PC under English the legend stays
  Japanese (the open Q5).
- **Route:** none (`rest_3e.md`: fuzz only).

### 2.5 The item lists' L1 / R1 art

- **Draw:** `Menu_DrawPiece` (`0x57D860`) with flag 1 takes `(u, v, w, h)` from
  `0x663C8C` on the UI page `0x1A080200`. Pieces `0x15` (144..159, 128..151: the category
  arrows with `L1` under them), `0x18` (176..191: the corner with `R1`), `0x2E` and `0x33`
  (two more arrow pieces with `L1`) carry the labels (`pieces.py`, `pix.py`,
  `07_ui_page_pc_pspeu.png`). `callargs.py`: `0x18` and `0x33` are drawn with flag 1 by ten
  list draws - `Menu_DrawItemList`, `Menu_DrawItemPanel`, `SharedList_DrawList`,
  `Win2_DrawItemList`, `GeneWin_DrawListFrame`, `MenuList_GeneWinDraw`,
  `MasterWin_DrawList`, `BattleMenu_DrawItemList`, `BattleMenu_DrawSkillList`,
  `BattleEquipWin_DrawItems`; `0x15` through the piece lists `0x663490`, `0x6641F0`,
  `0x66AEA4`, `0x66AF58`, `0x66B1E4`, `0x66B3C4`, `0x66B4BC` (`piece_users.py`).
- **What decides it:** baked art. The lists' page and category keys are hard-coded
  (`0x500C` / `0xF00C`: the directions with L1 0x04 and R1 0x08, `input_census.tsv`).
- **The PSP's page** redraws these two tiles as `L` / `R` (59 containers,
  [`region-diff.md`](region-diff.md) 5.2; `04_itemlist_corner_psp-eu.png`).
- **Route:** `fairyVillage.txt` (t004230..t004320), `shop.txt`, `menu_screens.txt`.

### 2.6 The dialogue's button mentions

- **Draw:** the text draw (`MsgBox_Step`, `Text_DrawString`, ours in `src/game/msgbox.cpp`
  and `text_draw.cpp`): a glyph like any other.
- **Census** (`text_buttons.py`; an icon glyph right after the colour control `0x05`, the
  shape both releases use for a button in running text):
  - **PC Chinese** (`bof3/DAT`): 10 areas, 31 mentions - circled 1 in the circle colour 25,
    `X` in the cross colour 4, circled 3 in the circle colour 1 (area 1) and in the square
    colour 1 (area 121). The port's dialogue does not use the Config's code-to-button
    assignment (there circled 3 is triangle); the colour is the reliable cue.
  - **en-US layer** (`analysis/cache/pc-plus-us/loc/en-US`; the US font's icons are codes
    0x80..0x83 circle, cross, triangle, square, read off `analysis/font/us_sheet0.png`, laid at
    glyphs `0x9E3..0x9E6` by `loc_build.encode_char`): 9 areas, 32 mentions - cross 27,
    triangle 4, circle 1.
  - The areas: 8 (chopping wood), 30 / 89 / 129 (the fishing help lines, five each), 68 (the
    well), 75 (the rope pull), 111 (the antenna), 121 (ramming the Black Ship), 135 (the
    crane); area 1 on the PC only. Not buttons: area 78 (a censored word of crosses) and the
    faerie village 175..185 (cross as a times sign).
- **What decides it:** the text - **baked**, per language. What the code tests is per
  minigame: the rope pull is `Input_Pressed & 0x20`, circle, hard-coded
  ([`area_w1f.md`](area_w1f.md) 1.1) - the US text says cross; the fishing is section 2.3;
  the chopping, the well, the antenna and the ram were not traced.
- **Route:** none of the routes is known to open these messages (not searched frame by frame).

### 2.7 Not sites

- **The battle command cross** draws command boxes by position (`analysis/shots/labels_cmd/cmd_l1.png`);
  `Battle_CommandPadMasks` `0x64AE3C` maps directions and L1 / R1 to commands, no button glyph.
- **`Menu_ButtonSets`** (`0x66383C`) are verb boxes (Quit, Use, ...), not controller buttons.
- **The F9 pause lines** name F9, a key `WndProc` reads directly (DIV-0038), outside both layers.
- **The name-entry legend** was cut by the port ([`name-entry-restoration.md`](name-entry-restoration.md)).
- **`text_scan.py --ledger`** (run `--all --singles`) has no button-glyph class: the label
  strings `0x66A2DC..0x66A2F8` are reached only through `0x66A2FC` and the scan does not list
  them (`textscan.log`).
- **Not searched:** area pages' own art beyond the dock. Evidence that there is no more:
  [`region-diff.md`](region-diff.md) finds no JP-US image difference that is not language or a
  CLUT, and the US changed every button icon it touched (section 3) - so a further baked
  button icon would have shown up as a language-image difference; the dock's is the one the
  inventory found.

## 3. What the US release changed: one permutation

| JP | US |
|---|---|
| circle 0x20 | cross 0x40 |
| cross 0x40 | triangle 0x10 |
| triangle 0x10 | square 0x80 |
| square 0x80 | circle 0x20 |

- **The defaults:** JP Speak circle, Move square, Menu triangle, Action cross, confirm
  circle, cancel cross; US cross, circle, square, triangle, cross, triangle
  ([`controls.md`](controls.md) 2.1, both boot executables read). That is P on every word;
  L1, R1, select, L2 / R2 unchanged.
- **The hard-coded tests** (`perm_scan.py`, every EMI section both discs carry; a test is an
  `andi` within four instructions of a `lhu` of the pad words): 139 tests compared; **44
  changed, every one by P**; 94 equal, none of them naming some but not all of the face
  buttons; 1 other (`GAME.EMI` section 1, `0x800` start became `0x9FF`). Four sections'
  counts differ: the name entry (`COMMU02`, `COMMU02B`, `START`, `STATUS`) drops a triangle
  test in the US - a rework the PC cut anyway. The P changes: the fishing module (11 each in
  `AREA030`, `089`, `129`), the crane (`AREA135`, 5), the rope pull (`AREA075`, 2), the
  dock (`SCENA10`), `SCENA13` and `SCENA17` (the ending). A lower bound: tests reusing a
  register loaded earlier are not counted (the dock's other two are such).
- **The fishing's stage 1, read:** JP `andi 0x74` with triangle menu, circle cast, cross
  quit; US `andi 0xD4` with square menu, cross cast, triangle quit (`mips.py`, `AREA030`
  section 4, JP `0x801D9ED8`, US `0x801D9E50`). Its labels: US cross, triangle, square
  ([`fishing-text.md`](fishing-text.md) 3). P.
- **The dock:** section 2.4. P.
- **The text:** the PC's colours against the US glyphs, mention for mention (section 2.6):
  circle-coloured becomes cross, cross-coloured becomes triangle, the area-121 square becomes
  circle. P.

So the US release is Japan's game played with the face buttons relabelled. A physical remap
by P on the PC reproduces its controls exactly, everywhere except the name entry and the one
start test, and without touching a test.

## 4. Two layers, and when an icon is wrong today

- **The physical layer** (key or pad input to a PlayStation bit): `Key_Table` `0x7DE7A8`
  (from `BOF3.CFG` or `BOF3X_KEYS`), the pad's `BOF3X_PAD` table and layout
  (`src/input/bindings.h`, DIV-0050). Shared by every test.
- **The swap layer** (an action to a PlayStation bit): the nine save words. Read only by the
  code that reads them: the field's actions, the menus' confirm and cancel (77 functions,
  `input_census.tsv`); not by the minigames' hard-coded tests.

| Situation | Config | World map | Fishing labels | Dock | List L1 / R1 | Dialogue |
|---|---|---|---|---|---|---|
| PC, its own data, pad on defaults | right | names keys | right | right (Japanese) | right | right (Chinese) |
| keyboard player | names PS buttons | right until a key is rebound | names PS buttons | names PS buttons | names PS buttons | names PS buttons |
| rebound pad | names the PS button, not the pad's | names keys | wrong | wrong | wrong | wrong |
| Config swap | right | names keys | the word-read steps wrong | right | right | wrong where the code reads words |
| Western language layer | right | names keys | right (port's labels) | Japanese | right | **wrong in the minigames with hard-coded tests** (rope, crane, fishing; the US text names P of the PC's button) |
| Western disc-only install | right | **two icons overlapped** | right | **wrong** (US art) | right | wrong, as above |
| PSP-only install | right | the PSP's own page (not read) | right | the PSP's page (not read) | `L` / `R` | the PSP's text, as above |

## 5. The design: one table, every site asks it

**The table.** `input::GlyphFor(bit)` (a new `src/input/glyphs.{h,cpp}`) answers, for one
PlayStation bit, the glyph a player should see:

1. the device last used - `pad_read.cpp` records which device produced the last press
   (keyboard bytes or the SDL pad), defaulting to the pad when one is open;
2. on the keyboard, the first key of the live `Key_Table` whose word has the bit (the table
   the original's loop reads, so a hand-edited `BOF3.CFG` is honoured); on the pad, the first
   pad input bound to it in `BOF3X_PAD`'s table;
3. the glyph set by device: keyboard key names; the pad's own family by `SDL_GetGamepadType`
   (already logged by `pad_sdl.cpp`): PlayStation shapes, Xbox letters, Nintendo letters;
4. none bound: the PlayStation shape, as now.

A site that tests a save word asks for the word's bit (the Config panel and the world map
already compute it); a site that tests a constant asks for the constant. Nothing else
changes: no test, no save, no table of Capcom's.

**The glyphs.** PlayStation shapes exist (DIV-0051's six, from the player's disc; the world
map's 8 x 8 sheet on every disc). Key names and Xbox / Nintendo letters are new art of ours,
drawn as glyphs in the overlay table's free range (as DIV-0051's six) - composed from the
dialogue and UI Latin cells the overlay already builds (`Z`, `X`, `Esc`, `Ent`, `Spc`,
arrows), so no third-party art. A disc-only or PC install without a Latin overlay needs the
same cells: built from the player's disc's atlas (every Western disc carries it; a JP-only or
Chinese-only install would need a JP-disc build or our own pixel font - an open point).

**Option B, a "US button layout".** A launcher preset that binds the pad's (and the keyboard's)
face buttons through P, so the south button sends the PC's circle bit and so on. With the
table above every icon then shows the US buttons on Japan's code, and the Western text's
icons can be resolved by P instead of rewritten. A preset of the existing Controls dialog;
no game logic.

**The Western text and art.** Two ways, the owner's call (section 10): translate each US
icon to the PC's bit by P at build time (`loc_build.py`: a text icon becomes a button code
the draw resolves through the table), or keep the disc's icon as drawn and offer Option B
as the matching layout.

## 6. Per site: what it needs, hours, value

Ranked by value (how often a player sees it, and how wrong it is now).

| Rank | Site | What it needs | Hours | Route |
|---|---|---|--:|---|
| 0 | **the core** | `GlyphFor`, the last-device flag in `pad_read.cpp`, the inverse of `Key_Table` and the pad table, a self-test (every default binding gives today's icon) | 8-12 | - |
| 0 | **the glyph sets** | key names and Xbox / Nintendo letters as overlay glyphs (`loc_build.py`), the PlayStation shapes kept; disc-only builds from the disc's atlas | 10-16 | - |
| 1 | **the world map's legend** | draw `GlyphFor(word 0 / word 6)` at the key position (one helper the eleven copies call); a START glyph for row 3 (`GlyphFor(0x800)`); labels without key names - the disc's words with their baked icon blanked, or our own label text; on a disc install blank the baked icons | 6-10 | yes |
| 2 | **the dialogue** | `loc_build.py`: a button control in place of the icon glyph (PC Chinese by its colour, US by P), the text draw resolving it; key names wider than a glyph re-flow the line | 12-20 | no |
| 3 | **the fishing labels** | `FishingText_Label` by bit: stage 1's lines the constants, the word-read lines (cast's confirm) the word | 3-5 | yes |
| 4 | **the Config panel** | `Config_DrawControllerCell`: the word's bit through `GlyphFor` (a cell swap) | 1-2 | yes |
| 5 | **the dock's legend** | blank the panel's three baked icons (u `0xC0` panel) and draw `GlyphFor(0x20 / 0x40 / 0x10)` at their places in `EffectKind5E_DrawPanel`; English words are Q5's | 3-5 | no |
| 6 | **the list L1 / R1** | blank the labels in pieces 0x15 / 0x18 / 0x2E / 0x33 and draw the glyph in `Menu_DrawPiece` (one place for all ten lists) | 3-6 | yes |

Total 46 to 76 hours, about 6 to 9 days. The core, the glyph sets and the three reached
sites (world map, fishing, Config) are 28 to 45 hours and give the owner something to look
at on existing routes; the dialogue is the largest piece and is the only one that fixes a
defect present today under every Western layer (section 9).

## 7. Installs

- **PC install, its own data:** the port's world-map art must go (or be covered) for the
  legend to show glyphs; the dock is Japan's art over Japan's code (consistent); the dialogue
  is the port's Chinese.
- **PC install with a Western layer:** the dialogue icons are the US's (P of the PC's);
  the fishing labels are the port's (DIV-0069); the dock stays Japanese.
- **Western PSX disc-only:** the dial page and the dock are the disc's (US art over Japan's
  code: both wrong today); the text is the disc's.
- **JP PSX disc-only:** consistent everywhere (Japan's art, text and code) - only the
  physical layer can make it wrong.
- **PSP-only:** the UI page's `L` / `R`, the PSP's own dial page (not read here), the PSP
  text's button words ([`visual-findings-1012.md`](visual-findings-1012.md) 10.4).

## 8. The ledger entries it would take

Drafts for the coordinator to number; none is written to `DIVERGENCE.md`, since nothing is built.

1. **Icons follow the live bindings** - the core and the Config panel (amends DIV-0051:
   the icon is the bound input's, not the PlayStation button's).
2. **The world map's legend from the bindings** - replaces the port's static keyboard art
   (a port change, PC only) and blanks the disc page's baked icons.
3. **The fishing labels from the bindings** (amends DIV-0069).
4. **The dock legend's icons from the bindings.**
5. **The list shoulder labels from the bindings** (and the PSP's `L` / `R` page then only
   art).
6. **Dialogue button codes** - the language layer's icons become button codes resolved at
   draw time (amends the dialogue layer's DIV-0005 family; fixes section 9's first defect).
7. Option B: **a US button layout preset** - a launcher binding preset, no game change (a
   ledger line only if it becomes a default).

## 9. Defects found (for the coordinator to number)

- **The Western layers name the US buttons over the PC's code** (ours: `loc_build.py` copies
  the disc's icon glyphs). Nine areas' dialogue (section 2.6). Where the code is hard-coded the
  text is wrong whatever the save: area 75 says "hit the cross button", the code takes circle
  0x20; the fishing help lines (areas 30, 89, 129) say cross where the PC's code tests circle
  (stage 1's cast `0x20`, `LeaderPanel_S2Steer`'s `0x20`); the crane (135) likewise, its five
  tests P-changed in the US module. Read, not played.
- **A Western disc-only install's world-map legend overlaps two icons**: the disc's baked
  default (US) and the live glyph from Japan's words (`02_worldmap_legend_psx-eu.png`,
  `vf2_caughFish` t000630).
- **A Western disc-only install's dock legend names the US buttons over Japan's code**
  (cross OK where `EffectKind5E_Pick` places on circle). Not seen: no route.
- **The PC's world-map legend names keys statically** (SPACE / X / ENTER, the port's art) -
  D58's class: wrong once a key is rebound. PC only.
- **Correction to `disc-data-inventory.md` 4.2:** the PC / JP dock legend reads circle /
  cross / triangle, not square / cross / triangle.

## 10. For the owner

1. **Is the target "icons always tell the truth"** (section 5 as written: the glyph of what the
   player must press, keyboard included), or "the US release's icons" (Option B: a layout
   preset that makes the US icons true)? They combine: the table first, the preset after.
2. **The Western text's icons:** translate them to the PC's buttons (by P), or keep them and
   pair them with the US layout?
3. **The world map's labels on the PC:** the disc's words (Enter / Guide / Camp) with live
   glyphs, the PlayStation's design, in place of the port's keyboard art?
4. **Keyboard glyphs:** key names in the game's own lettering, or the PlayStation shapes kept
   for keyboard players as now?
5. **The order:** the reached sites first (world map, fishing, Config), the dialogue next?

## 11. Not reached

- No run was made: the captures are earlier runs' frames (README in the shots folder), and
  two sites (the dock, the dialogue) have no route.
- The chopping (area 8), the well (68), the antenna (111) and the ram (121): which bits their
  code tests was not traced; the text's colour and P place them.
- `SCENA13`'s minigame was not identified; the PSP's dial and dock pages were not rendered.
- `perm_scan.py` counts tests reached by its one pattern; tests on a register loaded earlier
  or through the action words are not in its 139.
- Area pages other than the dock were not searched pixel by pixel for baked icons (2.7 says
  why none is expected).
