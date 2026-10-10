# The faerie village's and the gene window's text, and a scan for the rest

**Status:** DONE (2026-10-10) - five more label groups of DIV-0064 built and
seen in game on the owner's four routes, the board's buttons from the disc's
paint (DIV-0088), the faeries' names in the saves; the exe's remaining
Chinese strings listed below with what each needs. Owed the owner's eye in
play.

The owner reached the faerie village (the community band, areas 175..185)
and recorded `tools/recipes/fairyVillage.txt`, `dragonMenu.txt` and
`fairyVillageNews.txt` (the save `fairyVillage.DAT`, slot 0 via
`input_run.py --save`); their earlier `dragonTransform.txt` sees the gene
window too. Under `BOF3X_LANG=en-US` the dialogue, the panel help lines and
the shop were English; the village's board and the gene window's tabs were
not. The owner also asked for a scan of the text no route had reached yet.

## 1. What was found, and where it lives

`BOF3X_TEXTLOG=1` on each route named every `.data` string drawn
(`docs/dialogue-localisation.md` section 8's method); `tools/text_scan.py`
(new) then listed every pair-code string in `BOF3.exe`'s `.data` that a
pointer word or a code immediate reaches - 298 in the table region
`0x640000..0x670000`, rendered with the port's own font so the Chinese can be
read - and the ones no overlay wrote were sorted by what reads them.

| Address | Shipped | Drawn by | US disc | Now |
|---|---|---|---|---|
| `0x66A14C` / `54` / `5C` | 资料 选择 最佳化 | `GeneWin_DrawChoices` `0x598E90` through the pointer table `0x66A164` | `BATTLE.EMI`: `Data` `Pick` `Best`, 6-byte slots between the `AP` / `1` / `0` bytes the PC has at `0x66AF38` and the pieces and `DATA` / `BEST` it has at `0x66AF4E` | **group 12** |
| `0x669E18..0x669E60` (10 x 8) | 学者 商人 宿屋 礼品屋 预言屋 探险家 古品屋 音乐家 游戏屋 复制 | `CommuBoard_DrawListBox` through `0x669E68` | `COMMU01.EMI`: Scholar, Merchant, Inn, Gift, Fortune, Explorer, Antiques, Music, Casino, Copy | **group 13** (repointed: four are eight letters) |
| `0x669E90..0x669ED8` (10 x 8) | 武具 道具 任何 / 速度 性能 / 仕事 文化 / 日归还 近场 远场 | `CommuBoard_DrawListBoxB` through `0x669EE0`, the rows `{first, count}` at `0x652C6C` | the same file: Weapons, Items, Handyman, Speed, Ability, Job, Culture, Daytrip, Nearby, Distant - the twenty 8-byte slots end right at the rows and record offsets the PC has at `0x652C6C` | **group 13** |
| `0x669E10` | 文化 | `CommuBoard_DrawPanel` (`push` `0x459143`), the number after it at `0x98` | the list's own `Culture` | **group 14**; the number moves to `0xBE` (the owner's web reference shows `Culture 7` with the number clear of the word) |
| `0x669F08` | 名 (one glyph after a count) | the ranked lists' headings (`rest_4e.cpp` `ListHeading`, `push` `0x4607B7` / `0x4609A5`) | `COMMU05.EMI`: `faeries` / `faery`, two copies of the pair of 8-byte slots right before the kind table the PC has at `0x653180` | **group 14**; the heading draws the plural, or the singular for a count of one (an inference from the pair - the US routine was not read) |
| `0x669F60` / `0x669F68` | 所持金 赌金 | the hi-lo game's money and stake boxes (`rest_4c.cpp`, `push` `0x45B4BD` / `0x45B61D`) | `COMMU02.EMI`: `Cash` and `Pot`, each with the disc's zenny code `0x60` after it, right after the 72 bytes of sprite rectangles the PC has at `0x652D04` | **group 14** (not on any route yet) |

The draws pass `Text_DrawAt` a glyph count sized for the Chinese word - 2 for
the label, 3 for the list lines and the hi-lo titles, 1 for the glyph - which
would cut `Merchant` to `Mer`; once the group is written they pass `0xFF`
(`Labels_Written`), the strings being NUL-ended. Group 14 is read by our own
draws through `Labels_Slot`, so its five strings stay in the DLL's buffers and
the shipped slots are untouched (the 4-byte slot at `0x669F08` could not hold
`faeries` anyway).

**The heading's place.** `Population change` from the shipped `x + 0x58` ran
under the page counter the 8 px draw puts at `0xEE` (the first capture,
`analysis/shots/fairy_loc1/f00840.png`: `chan` and `1/2` over each other).
The shipped heading is four glyphs, so its middle is `x + 0x70`; under a
written group the heading is centred there (`TextAdvance_Width`), which puts
the US title within a few pixels of where the owner's web reference has it.

## 2. Done the same day, at the owner's word

- **The board's three side buttons** 狩り / 開拓 / 建築 are paint on the
  community's sprite sheet (`CommuBoard_DrawSprite` kinds 3..5 off the
  kind-1 chunk `0x1C080200` of `COMMU01.DAT` / `COMMU05.DAT`), and each disc
  repainted them: `Hunt` / `Clear` / `Build` on the US (the owner's web
  reference), their own words on the French and German. **DIV-0088**:
  `loc_build.py` writes `<tag>.COMMU01.DAT` / `.COMMU05.DAT` with the port's
  sheet and the disc's bytes in the label rows 224..255 (section 4).
- **The list boxes** were `0x30` wide, three Chinese glyphs and a margin,
  so `Merchant` and `Handyman` ran past the edge. The discs sized the box
  per language - US and German `0x50`, French `0x60`, JP `0x30` (the two
  `addiu $a2` constants that vary across the four `COMMU01.EMI`s) - the
  widest word plus 16. Under a written group 13 the box is that
  (`Labels_MaxWidth`), and the frame a column per 8.
- **The faeries' names.** The owner asked whether the port rolls random
  names and stats: it does not. The faeries are the PlayStation's sixty, a
  table of 20-byte trait records at `0x653210` (four stats, a name of 16),
  and a birth (`CommuSim_AddRecord`) copies the record's stats and five
  bytes of its name into the save - whole for the US's five-letter names,
  two and a half glyphs of a Chinese one. Only the renamer's suggestion
  (`CommuName_MakeRandom`, two random halves of messages `0x1F0..` /
  `0x230..`, which the US disc has as `NEW TITLE` placeholders) is random,
  and the port cut that screen (DIV-0075). **Group 16** writes the US
  disc's sixty names (`COMMU00.EMI`, 9-byte records found by the stats,
  the French and German discs carrying the same list) into the name
  fields; `tools/faerie_names.py` rewrites the five bytes of every in-use
  entry in a save from the list (entry r is record r) and recomputes the
  checksum - run on `fairyVillage.DAT`, `BISLPS00` and `BISLPS0C`.
- **The Identify panel's** 弱点 / 持有物: no disc carries a word for them
  (the ability's own overlay `MAGIC059.EMI` and `BATTLE.EMI` hold none)
  because the US panel draws none - the owner's wiki capture: the name, the
  EXP and zenny lines, a small `ITEM` label, the items. **Group 15** sends a
  space for each, so under a Latin overlay the headings vanish and the
  port's unread `ITEM` (`0x65AAB4`) is drawn small above the items as the
  US has it; the target's name is drawn with count 8 instead of the five
  glyphs' 5 (`Fly M` -> `Fly Man`).

- **The French and German check** (an agent, the same day; captures
  `analysis/shots/{fairy,news,dragonmenu}_{fr,de}`): every group full on
  both discs, both sheets repainted (`Chasse / Reclam. / Bâtir`, `Jagen /
  Bauen / Roden` - the German disc paints build on the clear icon and clear
  on the build icon, its own art), the tabs `Data / Pren / Best` and `Data /
  Aufn / Best`, the lists fitting their boxes (`Marchand`, `Capacite` with a
  glyph to spare), `Trvail` / `Objete` / `Capacite` the French disc's own
  spellings. One defect, fixed the same day: the count column of the ranked
  lists' heading ran under the French row label `Taux de natalité` and
  touched the German `Geburtenrate`; it now moves right of the label. The
  French news route desyncs at the tiara prompt (text timing); the village
  route reaches the same board.

## 3. Left as found - each with what it needs

- **The parts menu's title** 选择要交给小桃的零件 (`0x66A098`,
  `EffectKind5E_DrawMenu`, `rest_3e.cpp`): not in `GAME.EMI` (`Part A..G`
  are there), `START.EMI` or `SHOP.EMI`; an area overlay most likely.
- **The empty save slot's** —空记录— (`0x669CCC`, `field_o.cpp`'s slot
  list): no `Empty` / `No data` in `START.EMI`; the PSX draw to read.
- **The community's seven member names** (`0x669F70..0x669F9C` behind
  `CommuName_RecordNames` `0x669FA4`): read only by the renamer the port cut
  (DIV-0075); nothing to do until I34.
- Known and recorded elsewhere: 弟子 `0x66A1F8` (DIV-0064), 残留 / 回合
  `0x669D10` / `18` (section 8 of `dialogue-localisation.md`), the F9 prompts
  `0x66A3F0..` (`pause_text.cpp`, the port's own).

## 4. The board sheet, and what the discs changed in it

The sheet is 256 x 256 at 4 bits (128 bytes a row), VRAM (896, 256), the
same chunk in `COMMU01.DAT` and `COMMU05.DAT` (`COMMU02.DAT`'s differs).
Against the disc sections of the same dest, the port's sheet differs in:

| Rows | US | French | German | What |
|---|---|---|---|---|
| 96..151, 160..175 | yes | yes | yes | the board's frame pieces - the port's draws place them, so the port's stay |
| 225..230, 233..238, 249..254 | yes | yes | yes (248..254) | the three labels - `Hunt` / `Clear` / `Build` and their French and German |
| 56..61, 178..199, 212..215 | - | yes | yes (57..61, 160..198) | other repainted tiles, not looked at |

The sprite table (fourteen 6-byte records) is byte for byte the PC's on the
US disc, so the rectangles are the same. The overlay chunk takes only rows
224..255's differing bytes.

## 5. Verification

- `loc_build.py all` on the US disc: `gene tabs 3, village lists 20, village
  words 5, identify words 2, faerie names 60`, `village board sheet:
  COMMU01.DAT, COMMU05.DAT`; the log `3 of 3 gene tabs`, `20 of 20 village
  lists`, `5 of 5 village words`, `2 of 2 identify words`, `59 of 60 faerie
  names` (the sixtieth kept as shipped).
- `BOF3X_SHADOW=magic_s12,rest_4a,rest_4b,rest_4c,rest_4e` headless from an
  ini-less launcher copy: 0 mismatches each - the fuzz runs before any
  overlay loads, so every changed draw is the original there (the first
  `magic_s12` run caught a changed call order, put back).
- The four routes replayed with a shot every 60 frames:
  `analysis/shots/dragonmenu_loc2` (`Data` / `Pick` / `Best` on the tabs, the
  picked tab in colour 7), `fairy_loc4` (`Scholar` / `Merchant` / `Inn`,
  `Weapons` / `Items` / `Handyman` in a box sized to them, `Culture 0` with
  the number clear, `Hunt` / `Clear` / `Build`, the faerie `Coo` in its
  popup), `news_loc4` (`Population change` centred, `1/2` clear of it,
  `Birth 3 faeries`, `Coo Pan Candy`), `identify_loc4` (no headings, `ITEM`
  small above the items, `Fly Man`).
- `ledger_check.py`: 0 errors.

The owner's web references (two screenshots of the US board and population
list, 2026-10-10) are not theirs and are not kept in the repository.
