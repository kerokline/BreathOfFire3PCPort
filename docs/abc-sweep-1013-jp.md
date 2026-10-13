# The A/B/C sweep in Japanese, 2026-10-13: the PC's ja-JP layer, the Japan disc alone, the PSP-JP disc alone

**Status:** MEASURED (2026-10-13 06:20 .. 10:20, the owner away; branch
`away/1013-jp` off `d0d9162a`). Every claim below is from the run named with
it; the frames, logs, heat maps, dumps and reports stay under
`C:\Users\kerok\abj` (game-derived, rule 1). Not seen by the owner.

The owner asked for a Japanese A/B/C comparison: Japanese is the one language
all three builds carry natively (the PC's `ja-JP` layer, the PlayStation's
Japan disc, the PSP-JP image), where English on the PSP needs the PSX script.
The method is [`abc-sweep-1012.md`](abc-sweep-1012.md)'s (sections 1, 2, 6),
its ten routes, and four more for the spot checks.

## 0. What came out

- **The `jp` set builds and plays.** `prepare --set jp` built all three
  caches and installs with no extra input (1,420 s); every route ran on all
  three installs. The PC's `ja-JP` path passes the hosted `'*'` self-test
  (exit 0, `inject: 10087 ours`, 1,073 `MISMATCHES` lines all 0).
- **The Japanese text itself matches on all three.** Dialogue, the help
  lines, item, skill and enemy names, the area plates, the gene window's
  `AP`, `NEXP をかくとく！` - drawn the same on the PC and both disc-only
  installs wherever a route reached them (sections 2, 4).
- **One crash, both disc-only installs exposed: the fishing banner** (3.1).
  Under `ja-JP` the banner's lines are empty on a disc-built image; Capcom's
  scroller then draws the NUL as glyph `0xFFDA`, which reads 18 MB past the
  glyph table: a garbage glyph at the banner's right end on every disc-only
  run (PSX-JP and PSP-JP), different each run, and once in five PSP-JP runs
  an access violation in `Font_UnpackGlyph` (`caughFish`, recipe frame ~965).
- **The exe's own strings are blank on both disc-only installs and Chinese on
  the PC** (3.2): the Config screen (every label and option), the menu verbs,
  the list titles, the stat labels, the battle command labels, the gene
  window's tabs, the zenny unit, the whelp's default name at New Game (blank:
  the hero has no name), the fishing banner. `loc_build.py` builds none of
  kinds 7..16 for a Japanese disc ("not built for a Japanese disc"), so the
  PC keeps the port's Chinese and a disc-built image keeps its `language`
  ranges zero. This answers item 3's "never seen in Japanese": DIV-0015,
  0018, 0019, 0020, 0064 and 0069 do not apply under `ja-JP`; DIV-0057's
  pair codes do and are right (names fit, item and enemy names identical).
- **The PSX-JP world map is not the others'** (3.3): from the first world-map
  frame the camera sits closer, and the route parts - on all four routes that
  load on the world map (`worldMapAndAreaTransition` 1590, `fairyVillage`
  330, `masterAndManillo` 420, `caughFish` 420). Repeated frame-exact (0 of 71
  frames differ between two PSX-JP runs); PSP-JP and the PC agree. Cause not
  located.
- **The yes / no prompts in Japanese** (4.1): the hand sits just before
  `はい` on all three sides at every prompt captured (the shop's four, the
  master's, the inn's and Manillo's vertical ones). DIV-0027's re-spacing is
  Latin-only and rightly not applied; nothing to fix.
- **Known, unchanged in Japanese:** the load screen's slot number and empty
  slots on disc-only installs (D254's `"%X"`: `) 57`), the inn's black
  screen on the PSX disc alone (D255), the PSP's `L` / `R` shoulder labels,
  its title and area art (DIV-0103), its sixth menu tab icon.
- **Harness:** `input_run.py`'s stale-log read (the 1012 trap) fixed - it
  cost three of the first fourteen route-sides; `psp-attract-logo` added to
  `recipes/abc-known.toml` (attract A-C unexplained 72 to 8).

## 1. Build, root, method

- **Build:** this worktree's `build/` at `d0d9162a` (the launcher, DLL and
  exe copied into `C:\Users\kerok\abj\L` by `prepare`; SHA-1 prefixes
  `bof3x.dll` `83dd170e39fc`, `bof3x-launcher.exe` `38921e7b73bd`,
  `bof3x.exe` `d8372d11fe9b`).
- **The set** (`recipes/abc-installs.toml` `[set.jp]`): `pc-jajp` - a copy of
  the PC install with `c_pc_jp` built from the PC's `DAT\`, `BOF3.exe` and
  `Breath of Fire III (Japan).cue`, `--lang ja-JP` (273 s); `psx-jp` - the
  Japan disc alone, `install --base --lang ja-JP` (407 + 37 s); `psp-jp` - the
  PSP-JP image alone (535 + 76 s). The importer needed nothing beyond the
  sources: the `psp-names-ja-JP` layer (and its `analysis/region` pair) is an
  `--opt` layer the set does not ask for, so none was missing. Logs:
  `abj\logs\build_c_*.log`, `install_g_*.log`.
- **Root:** `C:\Users\kerok\abj`, this agent's alone. Other agents ran games
  from their own roots at the same time: the machine was not quiet.
- **Self-test:** `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*' BOF3X_OPT=none
  BOF3X_LANG=ja-JP BOF3X_CACHE=<c_pc_jp> L\bof3x-launcher.exe --game g_pc
  --no-config`: exit 0, `inject: 10087 ours, 0 left original`, every
  `MISMATCHES` line 0 (log kept as the session's `selftest_pc_jajp.log`).
- **Runs:** `python tools/abc_run.py run --set jp --tag jp1013 --route R
  --root C:/Users/kerok/abj --statehash --statehash-skip <skip>`, one game at a
  time, `--every 30`; `<skip>` the union of both disc caches' `exe_tables.py
  gate-skip` lists (1,300 lines). Reports `report_jp1013_<route>.md`.
- **Recipes:** `fairyVillage` from a scratch copy with `# save fairyVillage`
  (the 1012 trap). `battle_commands` first ran as committed and stuck in the
  Config screen on all three sides (the title cursor: `abc-harness.md`'s new
  trap), then from a scratch copy with its `press up` made `wait 1` (tag
  `jp1013nb`). Every recipe was recorded under `en-US`: Japanese text timing
  differs, so where a route depends on text length it can drift from the
  owner's run - but the three sides are compared like for like.
- **The stale log.** The first `combat` ended both disc-only sides in 2 s
  with no frames and the first `shop` the PC side in 1 s ("recipe done at
  recipe frame 2620" read from the previous run's log). `input_run.py` now
  removes `bof3x.log` before the launch (section 7); both routes were re-run
  after it, and no run since has ended early (the shop's PC side had started
  before the edit was saved).
- **Repeats:** `worldMapAndAreaTransition` on PSX-JP and the PC (tag
  `jp1013rep`); `caughFish` on PSP-JP four more times (`jp1013rep`,
  `jp1013tl` with `BOF3X_TEXTLOG=1`, `jp1013sh1`, `jp1013sh2` with the state
  hash as the first run).

## 2. The sweep, route by pair

Per pair: frames compared / differing / unexplained / first differing tick /
first unexplained tick / where the scene key parts. A = `pc-jajp`, B =
`psx-jp`, C = `psp-jp`.

| Route | A-B | A-C | B-C | Runs |
|---|---|---|---|---|
| attract | 366 / 1 / 1 / 9570 / 9570 / - | 366 / 283 / 8 / 420 / 4110 / - | 366 / 283 / 7 / 420 / 4110 / - | all done |
| combat | 87 / 72 / 72 / 240 / 240 / - | 87 / 76 / 73 / 90 / 210 / - | 87 / 15 / 12 / 90 / 210 / - | all done (second run) |
| shop | 105 / 53 / 53 / 240 / 240 / - | 105 / 87 / 71 / 90 / 180 / - | 105 / 84 / 68 / 90 / 180 / - | all done (second run) |
| cutsceneAndNue | 252 / 138 / 138 / 150 / 150 / - | 252 / 142 / 139 / 30 / 120 / - | 252 / 13 / 10 / 30 / 120 / - | all done |
| worldMapAndAreaTransition | 71 / 26 / 26 / 240 / 240 / **1590** | 71 / 32 / 13 / 90 / 180 / - | 71 / 26 / 8 / 90 / 180 / **1590** | all done |
| menu_screens | 64 / 33 / 33 / 570 / 570 / - | 64 / 38 / 35 / 420 / 510 / - | 64 / 16 / 13 / 420 / 510 / - | all done |
| field_menu | 55 / 24 / 24 / 570 / 570 / - | 55 / 29 / 26 / 420 / 510 / - | 55 / 23 / 20 / 420 / 510 / - | all done |
| dragonTransform | 145 / 116 / 116 / 270 / 270 / - | 145 / 122 / 120 / 60 / 150 / - | 145 / 27 / 25 / 60 / 150 / - | all done |
| bossAndFlash | 184 / 121 / 121 / 210 / 210 / - | 184 / 156 / 153 / 60 / 150 / - | 184 / 147 / 144 / 60 / 150 / - | all done |
| fairyVillage | 157 / 85 / 85 / 210 / 210 / 330 | 157 / 89 / 82 / 60 / 150 / - | 157 / 17 / 10 / 60 / 150 / 330 | all done |
| config_screen | 38 / 18 / 18 / 600 / 600 / - | 38 / 25 / 22 / 420 / 510 / - | 38 / 7 / 4 / 420 / 510 / - | all done |
| battle_commands (`jp1013nb`) | see 4.2 | | | all done |
| masterAndManillo | 183 / 176 / 176 / 180 / 180 / **420** | 183 / 98 / 74 / 60 / 150 / - | 183 / 177 / 172 / 60 / 150 / **420** | all done |
| caughFish | 129 / 94 / 94 / 270 / 270 / **420** | 32 / 28 / 13 / 90 / 210 / - (97 missing) | 32 / 25 / 10 / 90 / 210 / 420 (97 missing) | **psp-jp FATAL at ~965** |

The attract row is the re-compare with `psp-attract-logo` (section 5). Read
together: **A-B is nearly equal where only Japanese data is drawn** (the
attract 365 of 366 frames equal; its one frame, 9570, is the camera a frame
apart, `moved -3,+0` over the whole screen), and **B-C is small wherever the
route stays together** (cutsceneAndNue 13, fairyVillage 17, combat 15: the
PSP's `L` / `R` labels and title). The large A-B and A-C counts are two
things only: the exe strings of 3.2 (every menu, battle and Config frame) and
the PSX-JP world map of 3.3 (the routes that part).

The unexplained differences, each opened and looked at, by kind (data,
layout, timing as in the 1012 sweep). Descriptions are of what is drawn.

**On every route:**
- The title (mode 0/1): the PSP's own title page (C only; `psp-title`).
  The PC's `®` is not a difference here: the Japan disc's title has it too.
- The load screen (mode 1, 150..300): on both disc-only installs the slot's
  number gone, the name a lone `R`, the empty slots `) 57` (D254's `"%X"`);
  on the PC the empty slots draw a four-glyph word (the port's Chinese
  empty-record word through the Japanese sheet, `一覧ネー` as drawn).
- **Names from the saves:** the recipe saves were begun in English or
  Chinese; under `ja-JP` the PC draws them through the Japanese sheet
  (`R n Ⅲ`, `T④④↘↖`, the Chinese `龙`) and the disc-only installs draw the
  first letter or nothing. A save keeps the names it was begun with (the
  owner's 2026-09-21 acceptance, [`dialogue-localisation.md`](dialogue-localisation.md)
  8); the two builds differ only in how they render the foreign bytes.
- The exe strings of 3.2: every menu, shop, Config, battle-command and gene
  window frame; the regions are the labels' boxes (e.g. menu_screens
  `items`: the four verbs, the list title; `equip`: the four stat labels).

**A-B (the PC against the Japan disc alone), beyond those:**
- The world map from its first frame (3.3), and everything after the parting
  on the four routes that load there.
- shop: the inn, 2730..3150, 80% (D255: the PSX disc alone draws it black;
  the PSP-JP and PC draw `おはようございます / セーブしますか？` and the
  vertical `はい / いいえ`).
- bossAndFlash and cutsceneAndNue: the Teepo / Ryu name banners (saves) and
  the battle command label (3.2); one frame of each a moment apart (3810
  cutsceneAndNue, a field fade).
- worldMapAndAreaTransition: the legend panel (the PC's keyboard legend
  against the disc's `○ 決定 / × ガイド / START キャンプ`, the port's art,
  as in the 1012 sweep).

**A-C and B-C (the PSP-JP disc alone), beyond those:**
- The attract's dialogue scenes: the PSP's logo at the top right (now
  `psp-attract-logo`); 3900..4170 and 9450..9690 also a lit window at the
  left (`0,0-48,112`) drawn dark - the PSP's area art in a mode-4 scene,
  which `psp-area-art` (mode 2 only) does not cover.
- The shop's area art behind its windows (the 1012 sweep's `moved` pieces
  again: `304,120` +8,-7 and `0,156` -8,-8, art, not layout).
- The battles' `L` / `R` labels at the scroll bars; the field menu's sixth
  tab icon.
- caughFish: the crash (3.1).

## 3. Findings

### 3.1 The fishing banner draws glyph `0xFFDA` on disc-only Japanese installs; once a crash

**The crash.** `caughFish`, PSP-JP, the first run (`runs\jp1013_caughFish\psp-jp`):
the last timed frame `t000960` (mode 8, area 30), then `CRASH 0: exception
0xC0000005 at 0x0F61B0B8 (bof3x.dll+0x12B0B8)`, `reading 0x1CD08560`.
`llvm-symbolizer` on `L\bof3x.dll` (ImageBase `0x10000000`): eip is
`tex_cells::Long` in `Font_UnpackGlyph` (`tex_cells.cpp` 240) <-
`Font_BuildGlyphTexture` (367) <- `Font_GlyphTexture` (`glyph_draw.cpp` 186)
<- `D3d_DrawGlyph` (146) <- `d3d_list` `Dispatch` <- `Gfx_DrawOTag` <-
`Game_WinMain` <- `Engine_Main`. The minidump (`bof3x.crash-16352-0.dmp`,
copied beside the run) read with `tools/crash_report.py --read 0x1AFB58
0x200`: `Font_GlyphTexture`'s arguments on the stack are glyph `0xFFDA`, CLUT
`0x7800`. `0xFFDA` is `-0x26`: by the offset the 1012 sweep's `0xFFDE` had
for a byte `0x04`, the glyph a byte `0x00` gives - a NUL drawn as a
character. Its 288-byte cell lies `0xFFDA * 288` (18.9 MB) past the glyph
table, so what it reads is whatever is mapped there.

**Not once: every disc-only run draws it.** The banner's right end at
`t000840` (the first character entering the scroll), cropped from seven
runs: the PC draws `钓` (the port's Chinese banner); PSX-JP and all five
PSP-JP runs draw a striped cell - orange bars in two runs, two thin red bars
in three, a mixed pattern in one - **different from run to run**, which is
memory outside the table, not a glyph. Four PSP-JP repeats finished
(`jp1013rep`, `jp1013tl`, and `jp1013sh1` / `jp1013sh2` with the state hash
on, as the crashing run had): one crash in five runs. PSX-JP reaches the
same banner (its `caughFish` frames 840..) and draws the same garbage; it did
not crash in its one run.

**Why the NUL.** The banner's thirteen lines are strings `0x669FC0..0x66A06D`
through the records at `0x653B98` ([`fishing-text.md`](fishing-text.md) 1).
A disc-built image maps `0x669FC0..0x66A088` as `language`
(`CommuName_RecordNames+0x1C`, `c_pspjp\base\exe\data.toml`; the same on
`c_psxjp`), left zero for the language layer, and `loc_build.py` builds the
fishing text (kind 16, DIV-0069) only for a Latin disc. So every line is
empty. `EffectKind0F_LineType` (`0x466310`, ours, `effect_1a.cpp`) draws the
character under its cursor through `0x469AD0` while the flip runs, before it
counts the string - with an empty string that character is the NUL. The
textlog run confirms the empty lines (`text 0x00669FC0 frame 971:` with no
bytes, `runs\jp1013tl_caughFish\psp-jp\bof3x.log`). The PC never has an
empty line (the port's Chinese, or DIV-0069's Latin text), so the PC path is
not affected; an English, French or German disc-only install has kind 16.

Classification: data (a `language` range no Japanese layer fills) reaching
Capcom's scroller, which assumes a non-empty line - a crash a player can
reach on a disc-only Japanese install, and a visible garbage glyph on every
fishing visit. **Not a `twin` fill:** the bytes are the port's text, not a
copy of a disc overlay's (3.2). For the coordinator to number.

### 3.2 The exe's own strings under `ja-JP`: Chinese on the PC, blank on the discs

`loc_build.py` (the `donor_ja` branch, "config, verbs, character names,
merchant, battle labels and messages: not built for a Japanese disc") skips
kinds 7..12 for a Japanese donor, and kinds 15 and 16 are built from the
Latin anchors only. What that leaves, captured on all three sides:

| What | Ledger (Latin) | Route, frame | PC (`ja-JP`) | PSX-JP, PSP-JP |
|---|---|---|---|---|
| Config: labels, options, the two buttons | DIV-0015 | config_screen `config_row1`..`7` | the port's Chinese (`终了` on the first button, as drawn; six row labels and their options) | **every label and option blank**; the hand and help line only |
| Menu verbs | DIV-0018 | menu_screens `items`, `equip`, `ability` | `使用 整理 丢弃 重要`, `装备 最强 ...` | four blank buttons |
| List titles | DIV-0064 | menu_screens `items` / `ability`; dragonTransform 930 | `物品`, `治疗`, `辅助` | blank title box |
| Stat labels (menu, shop, master, status) | DIV-0064 | menu_screens `equip`, `status`; shop 1590; masterAndManillo 3930 | `攻击 防御 智力 速度` | blank; the numbers drawn |
| Battle command labels | DIV-0019 | combat 1500; cutsceneAndNue 6240; battle_commands `jp1013nb` 2910..3150 | `攻击`, `観察`, `特能`, `逃走` | blank label box |
| Skill-type title in battle | DIV-0064 | battle_commands `jp1013nb` 3270 | `龙技` | blank |
| Gene window tabs | DIV-0064 (groups 12..14) | dragonTransform 990 | `资料 选择 最佳化` | blank |
| New Game's default names | DIV-0020 | battle_commands `jp1013nb` 2790..3270 | the whelp `巴比` in the banner and status | **no name at all** |
| Zenny unit | DIV-0064 | field_menu `menu_top*` | the port's `ｚ` glyph | blank |
| Fishing banner | DIV-0069 | masterAndManillo 1080, caughFish | `起...① 钓竿移动...` | empty lines, and 3.1 |
| The faerie board's member names | - | fairyVillage 840, 1710 | Chinese names (`巴兰莉`, `杜尔`) | blank |

The Japan disc carries its own form of each (the PlayStation draws these
screens in Japanese), so the gap is the language layer's, not the image's: a
Japanese layer would have to find each table on the Japan disc (the Latin
anchors are the US layouts) and write the disc's two-byte codes through the
`ja-JP` sheet's mapping, with the PC's slot widths (DIV-0057's pair codes
were built for names, not these slots). That is `loc_build` work and changes
what the PC's `ja-JP` install shows (Chinese to Japanese), so it wants a
ledger entry (DIV-0015 / 0018 / 0019 / 0020 / 0064 / 0069 extended) and the
owner's word on scope; not attempted here. **Not `twin` fills:** these
ranges are `language` in `data.toml` by design (`exe-engine-own.md` 4.2: "Not
filled"), and a verbatim copy of the disc's bytes would draw the wrong
glyphs on the PC's sheet.

### 3.3 The PSX-JP world map: a closer camera, and the routes part

`worldMapAndAreaTransition` `t001140`..`t001770` (mode 2, area 33): PSX-JP
draws the world map larger - the houses, trees and the party about a third
bigger by eye, the view centred on the party - where the PC and PSP-JP draw the
same wider view (A-C differs there only by the legend panel `64,20-124,56`).
The same on `fairyVillage` (390, area 65), `masterAndManillo` (360..720, area
45) and `caughFish` (from 420). In `masterAndManillo` the camera is wide at
330 (the fade-in) and close from 360; the party then barely moves and the
route's next presses open the camp menu (mode 3), so the run never reaches
the master. The 1012 sweep's PSX-EU world map was the PC's
(`n1012_worldMapAndAreaTransition` 1200, checked side by side), so this is
the Japan disc's alone.

- **Deterministic:** a second PSX-JP run (`jp1013rep`) equals the first on
  all 71 frames (`abc_compare.py`, `runs\jp1013rep_wm_psx_vs_first`).
- **State hash:** A-B from tick 1095 (the world map's load) the pages
  `0x905000` (`CameraTurn_Steps`, `CameraTurn_Angles`, ...), `0x92A000`,
  `0x938000`, `0x939000` and `0x66C000` differ; A-C has none of them
  (`runs\jp1013_worldMapAndAreaTransition\compare\statehash_A-B.txt` /
  `_A-C.txt`). The camera state differs from the load on.
- **Not found in the data** (scratch scripts `imgdiff*.py`, `mancmp2.py`):
  no base chunk differs on PSX-JP alone (of 3,307: 3,054 equal on all three,
  225 PSP-only, 16 different on both discs - the world-map dial pages,
  `port-art` - and 12 equal on both discs); in `.data`, the runs where
  PSX-JP's image differs from the PC's while PSP-JP's or PSX-EU's equals it
  are an area-26 map run, two bytes in `START.EMI#8`'s run after
  `Char_DefaultRecords`, one `GAME.EMI#0` table byte (`0x663B1B`) and
  `gap` / `language` ranges with no world-map symbol. None is named for
  the world map's camera.

Cause not located; the next read is the world map's camera set-up on load
(what writes `0x905000`'s page at tick 1095) and a raw dump of that page on
PSX-JP against PSP-JP. Whether the Japanese PlayStation's world map really
sits closer is not known here (the PSX twin, by `psx-twin-check.md`, or the
owner) - if it does, this is the disc's own behaviour and the parting is
only the recipes', recorded on the PC.

## 4. The spot checks the owner named

### 4.1 The yes / no prompts

Captured in Japanese (frames named are the timed ones):

| Prompt | Where | PC | PSX-JP | PSP-JP |
|---|---|---|---|---|
| Shop, "sell?" / "buy?" (`ShopWin_TitleRun`'s line, the hand a window record) | shop 1050, 1680, 1980 | hand just before `はい`, `いいえ` after it | same | same |
| Master, `この人で よろしいですか？ はい いいえ` (`0x586D20`) | masterAndManillo 3960 | hand just before `はい` | not reached (3.3) | same as the PC |
| Inn, `セーブしますか？` with `はい` / `いいえ` on rows | shop 2790 | hand on the `はい` row | black (D255) | same as the PC |
| Master, `弟子を やめたいのか？` and `あらたに 誰かが...` with `はい` / `いいえ` on rows | masterAndManillo 4620, 4800, 4830 | hand on the `はい` row | not reached | same |

Every capture has the hand on the first answer: none shows the hand on
`いいえ`, which the routes never select. On `はい` the hand's tip touches the
word on all three - the Japanese line has the layout Capcom fitted the stops
to, and DIV-0027 (Latin only, `Lang_Latin()`) is rightly off. **Same on all
three; nothing to fix.** The hand on `いいえ` is not captured.

### 4.2 The DIV-0064 / DIV-0057 labels "never seen in Japanese"

- **DIV-0064 (and 0015, 0018, 0019, 0020, 0069)**: not built for `ja-JP`
  (3.2) - so under Japanese they are not "seen" because they do not apply:
  the PC shows the port's Chinese, the disc-only installs nothing. The
  `jp1013nb` battle (New Game, the whelp's first fight) shows the command
  cross's labels and the `龙技` title Chinese on the PC and blank on both
  discs, the whelp named `巴比` on the PC and unnamed on both discs.
- **DIV-0057 (pair codes)**: seen and right. Item names longer than eight
  kana (`純げんきだま`, `ドラゴンシールド`, `フォースアーマー`), skill names
  (`リフレスト`, `ミカテクト`), enemy names in the banner (`ヌイグルミ`,
  `モーグ`, `ギリー`) and the boss's (`毒撃` as a skill banner) are drawn the
  same on the PC and both discs, nothing cut, no placeholder glyph
  (menu_screens `items` / `equip` / `ability`, shop 1590, combat 1500,
  battle_commands `jp1013nb` 2910).

## 5. `recipes/abc-known.toml`

`psp-attract-logo` added (DIV-0103's amendment, D256): a PSP side against a
non-PSP side, the attract route only, modes 1 and 4, the box
`196,12-320,84` - the title logo the attract's dialogue scenes draw at the
top right. Seen on the eu triple (the 1012 sweep's 123 frames) and here.
Checked by re-comparing `jp1013_attract`: A-C unexplained 72 to 8, B-C 72 to
7; what is left is the PSP's mode-4 area art at the left of the mine scene
(3900..4170, 9450..9690), the one-frame camera at 9570 and a corner trim at
9240 (`moved -2,+6`, the PSP's art). Not added: the exe strings (3.2) and
the PSX-JP world map (3.3) are defects, not intended differences.

## 6. The state hash

Every run recorded `BOF3X_STATEHASH` (`--statehash`, the 1,300-line skip
union). The pairs are different installs, so pages differ by design; read as
context:

- **A-B (the PC against PSX-JP), where the scenes stay together:** the
  `Gfx_ClutStrip` pages from the first area load on every route (the PC's
  font and CLUT rows are its own), the sound page, and the window / field
  pages (`0x803000`, `0x903000`, `0x904000`, `0x929000`) from the save load -
  the save's names and the window records' text pointers into different
  pools. Music pages (`Music_LoadedTrack`, `0x6BD000`) differ in the
  bossAndFlash fight (1755..4961). Not read to the byte.
- **B-C:** from tick 1 `0x657000` (the name tables: DIV-0104's
  `NameTable_*`, the PSP-JP's renames) and `0x65C000`, the VRAM shadow.
- **Run to run, one install:** PSP-JP `caughFish` `jp1013sh1` against
  `jp1013sh2`: 3 pages over 3,841 ticks - the sound page at five ticks and
  two pages at one tick (1793): noise, the machine loaded.

No PC-path reference exists in Japanese (the `cn_*_orig2` pairs are the
port's own language), so section 6 of the 1012 sweep has no `ja-JP` twin;
not recorded here.

## 7. Fills the twin way, and the harness fix

**No `twin` row fits what this sweep found.** The 1012 fixes' class (D252,
D260..D263: a short `.data` run no map places, carried verbatim by the
overlay that reads it) does not appear in Japanese: every disc-only
difference from the PC in sections 3.1 and 3.2 lies in `language` ranges,
which hold the PC's text, not a disc's bytes, and 3.3 has no data
difference found. The twin rows the 1012 round added (D260..D263) are in
effect here - the faerie board's help line and limits work on both
Japanese discs (`fairyVillage` B-C 17 frames, no crash; the board's slots
drawn), and the gene window's `AP` label is drawn on all three.

**`tools/input_run.py`:** removes `bof3x.log` before the launch, retrying
for ten seconds and stopping if it cannot (a game of that launcher folder
still running). The trap is in `abc-harness.md` (2026-10-13); proof: the
first `combat` and `shop` lost three sides to it (the shop's PC side started
before the edit was saved); every run after the change - the rest of the ten
routes and four extras, the re-runs of `combat` and `shop`, `jp1013nb` and the
six repeats - produced its frames, none ended early.

## 8. What is left

- **3.1, the fishing crash:** for the coordinator to number. Two ways out,
  both beyond a sweep: a Japanese kind-16 layer from the Japan disc (the
  banner in Japanese; the PC's `ja-JP` install changes too), or the
  scroller declining an empty line (a code change to Capcom's behaviour,
  needing a ledger entry). Until one lands, a disc-only Japanese install can
  crash at the fishing spot.
- **3.2, the exe strings:** for the owner - whether the `ja-JP` layer should
  carry kinds 7..16 from the Japan disc (the PC's Chinese would become
  Japanese, and the disc-only installs would gain a usable Config screen and
  a named hero). Until then, a disc-only Japanese install has a blank Config
  screen, blank menu verbs and an unnamed hero at New Game.
- **3.3, the PSX-JP world map:** cause not located (the reads named there);
  the PSX twin decides whether it is the disc's own.
- Not reached: the hand on `いいえ`; the master's prompt on PSX-JP (its
  route parts first); the merchant's name (DIV-0020's kind 11, Manillo), as
  `caughFish` and `masterAndManillo` part on PSX-JP before it and the PC /
  PSP-JP frames do not show his name box at a timed frame.
- The recipes are English recordings; a Japanese recording of the routes
  would remove the text-timing drift (none seen to matter here besides the
  world map's, which is 3.3's).

## 9. For the other files

Not edited here except as listed; the coordinator folds them in.

- **`docs/known-defects.md`**, three for numbering: 3.1 (both disc-only
  Japanese installs: the fishing banner draws glyph `0xFFDA` from an empty
  line, a crash when its cell lands on unmapped memory - PSP-JP, 1 in 5
  runs); 3.2 (disc-only Japanese installs: the exe's own strings blank -
  Config, verbs, titles, stats, battle commands, default names, the zenny
  unit, the village board's names); 3.3 (PSX-JP disc alone: the world map's
  camera closer from the load, the routes part).
- **`docs/abc-harness.md`** (edited): the `jp` set in section 2's table,
  `psp-attract-logo` in section 5's, the stale-log trap marked fixed, and the
  title-cursor trap for recipes with no `# save`.
- **`docs/dialogue-localisation.md`** 8: a sentence that under `ja-JP` none
  of kinds 7..12, 15, 16 is built, with this doc's 3.2 table as what that
  shows.
- **`docs/owner-review.md`**: 3.2's question (a Japanese layer for the exe
  strings) and 3.3 (is the Japanese PlayStation's world map closer?).
