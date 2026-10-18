# Which gaps play reads: the read watch

**Status:** MEASURED (2026-10-18, the install-gap round's group RW, branch
`install-gap/rw` off `06a209bd` with `harness/recipe-buttons-by-release`
merged; IDEAS.md I43 step 2). 77 watched runs - the attract and the ten
state-hash routes on each of the seven one-source installs - plus 15 targeted
runs. Nothing fixed; nothing captured; no recipe regenerated.

The census ([`install-gap-census.md`](install-gap-census.md)) gave every row of
`recipes/exe-gaps.tsv` a status by reading its readers. This document answers the
other half: **of the ranges a disc-built image leaves zero where the PC has a
value, which does play actually read?** It is the list the round's staging
([`install-gap-round.md`](install-gap-round.md) 2, RW) asks for first: TW and
stage B take the read rows first, and I43 step 4 (`BOF3X_EXEIMAGE_STRICT=1` as a
disc-only install's default) waits on it. Addresses, classes, symbol names and
counts only (CLAUDE.md rule 1): the watch never reads a value.

## 0. What came out

1. **Ten `gap` items are read in play before anything writes them** - so they
   are read as the image's zero: eight live defects and the two OWNER CALL rows.
   On the seven installs, over the attract, the ten routes and the targeted runs:

   | Item | Read on | Census status | Now |
   |---|---|---|---|
   | `Title_Sprites+0xE` `0x65385E` | psx-us, eu-en, fr, de (every route: the title) | OWNER CALL (D266) | read in play - the owner's call stands, now measured |
   | `Encounter_SlotChance+0x1C` `0x669CCC` | all 7 (every route with the load screen) | OWNER CALL (the empty slot) | read in play - as above |
   | `ChlorineCloud_Offsets` `0x65A608` | psx-jp, psx-us, psx-eu-en, psp-eu (cutsceneAndNue, the Nue fight) | NEEDS FIX P3 | **read in play: a live defect** |
   | `EventScript_OpLengths+0x10` (the default light pair) `0x663B1C` | psp-jp, psp-eu (every route, every area entry) | NEEDS FIX P2 (PSP) | **read in play: a live defect** |
   | `Sprite_ScreenMargins` `0x663B28` | psp-jp, psp-eu (five routes; ~95,000 reads on psp-eu) | NEEDS FIX P2 (PSP) | **read in play: a live defect** |
   | `Char_WhelpSlot+0x3` `0x669739` | psp-jp, psp-eu (six routes) | NEEDS FIX P3 (PSP, one byte) | **read in play: a live defect** |
   | `Fish_Kinds+0x72A` `0x66ADBA` | psx-jp (every route: `Area_ClassifyPending` at each area change) | NEEDS FIX P2 (JP) | **read in play: a live defect** |
   | `EffectKind0F_PanelTitles` `0x653EA0` | psx-fr, psx-de (caughFish, masterAndManillo) | NEEDS FIX (D286, LA's) | **read in play** (D286 confirmed read) |
   | `Equip_SlotIcons` labels `0x663660` | psx-de (shop) | NEEDS FIX P3 (LA's) | **read in play: a live defect** |
   | `ConfigMenu_Steps+0xC` `0x667518` | psp-eu (every save load: `Char_ApplyTraits` from `Char_RecalcStats`; found by a targeted run, 4.2) | NEEDS FIX P4 (no reader found) | **read in play: a live defect, and a reader** |

   `EffectKind0F_PanelTitles` and `Equip_SlotIcons` are the census's FR / DE
   layout rows (LA's in this round).
2. **Three `language` ranges are read before any layer writes them** on every
   install - `Menu_Verbs`, `Menu_Verbs+0xF4`, `CommuName_RecordNames+0x144` - and
   on the two Japanese builds five more: `Battle_CommandLabels`,
   `Battle_CommandLabelPointers+0x1C`, `CommuName_RecordNames+0x1C` / `+0x1CC` and
   `Encounter_SlotChance+0x24`. The census calls the class zero that "a
   language layer's handlers write, or ours read around"; these are read in
   place, through `Text_DrawString`. What that draws was not captured. Handed to
   LA (section 6).
3. **No `unread` range is read before it is written** on these routes (one,
   `Task_StackTop`'s range, is written first by `Task_SetStackBase` and read
   after: harmless). The `unread` class holds for what the routes reach.
4. **Targeted runs** (each range's words held in the debug registers for a whole
   run, 4.2) settle ten hot-page rows for the route they ran: `ConfigMenu_Steps+0xC`
   is read (above), the batch's sampling having missed it; `MasterWin_Requirements+0x44`
   (P1 item 8, four Western installs), `Music_LoadedTrack+0x1E30` (P4),
   `Encounter_SlotChance+0x38`, `CommuName_RecordNames+0xF0`, `GameMode3_Steps+0xC`,
   `EffectKind18Sub64_States+0x1C`, `Equip_SlotIcons+0x10` (FR) and
   `MenuList_GeneWinSteps` (FR, DE) are not.
5. **The other `gap` items are not seen read on these routes.** Of the 137 rows
   the batch never saw read, 62 sit on pages the watch saw every access to in
   every run (exact for these routes); 75 on pages parked at least once, where the
   watch sampled (section 4) - and `ConfigMenu_Steps+0xC` shows the sampling can
   miss a read that happens on a few latches.
   Almost all of them belong to areas, effects, bosses and magic the eleven
   routes never reach (one area-named item's area is reached:
   `Area141_CellEntries+0x7C`, a chapter-14 event's operand, not read in area
   141's field).
6. **I43 step 4's condition is not met.** "No range read in play still zero" is
   false: the eight live rows above are read on the installs listed, the two
   OWNER CALL rows on every Western one (the empty-slot row on all seven).
   `BOF3X_EXEIMAGE_STRICT=1` as the default would refuse every one-source
   install today. And "not read" here is
   route-bound: a clean list from these routes would still not prove a range
   unread in the whole game (section 7).

## 1. The watch: `BOF3X_GAPWATCH`

`src/hook/gapwatch.cpp` (`gapwatch.h`), an instrument in the exe host, off by
default, with no `DIVERGENCE.md` entry: it changes page protections and the main
thread's debug registers, never a byte the game keeps.

- **What it watches.** With a disc-built image mapped (`ExeImage_MappedBuild()`,
  the exe host with `BOF3X_EXEIMAGE`), the image's ranges of the classes in
  `BOF3X_GAPWATCH_CLASSES` (default `gap`; the runs here used
  `gap,unread,language`) - data.toml's ranges as `exe_image.cpp` mapped them,
  the same list the log's `gap` lines print (`ExeImage_MappedRange`, a new
  accessor). Refused in the hosted build and without a disc-built image.
- **How.** Every page holding such a range gets `PAGE_GUARD`. A vectored handler,
  first in the chain (the pattern of `calltrace.cpp`; `crash.cpp`'s reporter,
  last in the chain, never sees a guard fault or a step), takes the fault,
  classifies the access by `ExceptionInformation` (read, write), counts it
  against the range holding the address, records a range's **first read**
  (reader `eip`, four return addresses from a stack scan, `Frame_Counter`, the
  recipe frame, `Game_AreaNumber`, the thread) and **first write** (writer and
  frame), and resumes the instruction with the trap flag set; the single step
  after it puts the guard back. A read before the range's first write is counted
  apart (**reads before a write**: the reads that see the image's zero). An
  access starting up to seven bytes below a range is counted as `edge` (a wider
  read may reach it).
- **Hot pages: the page-granular fallback.** A page that shares a range with
  busy data would fault on every access. Each page has a budget of 16 faults a
  latch (`BOF3X_GAPWATCH_BUDGET`); past it the page is parked - unguarded - until
  the next latch re-arms it, and the file's page lines count faults, faults
  inside ranges, latches parked and the busiest readers (a space-saving count).
- **Hot pages: the debug registers.** On a page parked at least once, the `gap`
  ranges' aligned words take the main thread's debug registers in turn, two words
  a latch, each word in two registers (read-or-write and write-only, so a hit of
  the first alone is a read). A hit is a trap after the access with no guard
  fault: free. They are loaded by an exception of ours raised at the latch, whose
  handler writes them into the thread's context (it works under WoW64; the
  self-test proves it). `BOF3X_GAPWATCH_PIN=<address>` keeps one range's words
  (two at most) in the registers for the whole run: the targeted run of 4.2.
- **The state hash.** It reads every page each latch, and is no reader of the
  game's: the latch unguards the pages and clears the registers around
  `StateHash_Tick` (`GapWatch_Pause`, then `GapWatch_Tick`).
- **The file.** `BOF3X_GAPWATCH=<file>`, or a directory, which gets
  `gapwatch_<build>_<route>.tsv`; rewritten in place every 60 latches (runs end
  by `taskkill`). `tools/gapwatch.py report DIR --dll <the run's bof3x.dll>`
  joins the runs by range and names the readers (llvm-symbolizer on the DLL,
  `symbols.toml`'s `pc` for an owned twin); `pages` prints the page lines.
- **One limit, by how Windows treats guard pages:** an access to a guarded page
  from inside a system call fails that call instead of raising the fault. No
  file buffer is known to live on a watched page; the state-hash check below is
  the evidence that the game behaved the same.

### 1.1 The self-test

`BOF3X_SHADOW=gapwatch` (part of `'*'`): three scratch pages, two watched, one
never guarded. A read of the page outside every range is a fault and no range's;
the planted read reads the planted value, is recorded as the range's first read
at `PlantedRead`'s instruction, and the page is guarded again; a second read is
counted but leaves the first; a write is counted as one with its writer, and a
read after it is not counted as a read of the image's bytes; one unaligned word
across the two pages (two faults, one step, both guards back); a byte-by-byte copy
across both pages through the second range (its four bytes read); past a budget of
four the page is parked, and the latch re-arms it; the unguarded page never
faults; under a debug register a read is caught as a read with no guard fault,
the word's other byte too, a write is told apart by the write-only register, and
nothing once the registers are cleared; the watched bytes are unchanged at the
end. With `BOF3X_GAPWATCH` unset, no page of `.data`'s 156 is guarded.

Measured: passed (`76 faults, 74 steps`); the full `BOF3X_SHADOW='*'` from an
ini-less launcher copy with the module built in passed on the branch's tip (1,085
`MISMATCHES` lines, every one 0; `self-test only: done`, `inject: 10087 ours`).
Negative controls, each built and run once and then reverted: the step's re-guard
removed - refused (`the page is not guarded again after a read`); the debug
registers' enable bits zeroed - refused (`the debug register did not catch the
read`).

### 1.2 Cost and no-change

- **Cost** (the 77 runs, 386,000 latches): 35.1 ms a latch with the watch, against
  33.3 ms for 30 logic frames a second; about 94 guard faults a latch. The attract
  for 3,000 frames on psx-us: 110 s watched, 112 s and 119 s unwatched (the machine
  was shared with other sessions' runs: timing is noise at this size). The hot
  pages are what the faults are spent on: on whelpBoss, `0x66C000` (task records,
  `Math_Sin`, `Field_ObjectMotion`) was parked on 13,139 of 13,140 latches, `0x663000`
  (`DrawLayer_Open`, `AreaMap_HeaderPass`) on 12,275. The budget keeps every page
  to at most 16 faults a latch, so no page can make a run crawl.
- **No-change**: psx-us attract, 3,000 frames, state hash under
  `tools/statehash_skip.txt`, two unwatched runs against one watched
  (`statehash.py check`): the references agree but on one page; against them the
  watched run differs on two pages only, `0x7DE000` (the sound page, 42 ticks)
  and `0x7C9000` (`D3d_ScaleX`, `Font_TexCache`, 7 ticks) - the platform pages
  the state hash already knows to vary with load ([`state-hash.md`](state-hash.md)
  4, the third row). No game page differs.

## 2. The runs

From the harness root `C:\Users\kerok\g_rw` (`abc_run.py prepare --installs
<root>/installs.toml --set rw`: the seven disc builds of
`D:\bof3x-discs\paths.json`, each `install --base` with its own language - ja-JP,
en-US, en-150, fr-FR, de-DE, ja-JP, en-150), the launcher copy `L\` with this
branch's DLL, `BOF3X_GAPWATCH=<root>\gw`, `BOF3X_GAPWATCH_CLASSES=gap,unread,language`,
`BOF3X_DATLOG=1` (which areas each run reached), the harness's own
`BOF3X_OPT=none BOF3X_LAYERING=0`, one game at a time, `--every 100000` (no
frames). Routes: the attract (11,000 frames) and the ten with state-hash pairs
(`combat`, `shop`, `whelpBoss`, `caughFish`, `cutsceneAndNue`, `dragonTransform`,
`field_menu`, `masterAndManillo`, `menu_screens`, `worldMapAndAreaTransition`),
each with its `# save` header.

**Every route run is after the merge of `harness/recipe-buttons-by-release`**
(`d41273d9`, at the coordinator's word): a recipe's buttons pressed as the
install's release. Runs made before it (psx-us attract to whelpBoss) were thrown
away and re-run. The merge's effect, measured by the areas each run loaded
(`BOF3X_DATLOG`): with it, all seven installs follow psx-us's area sequence on 73
of the 77 runs; the four that part are psp-jp `combat` and `dragonTransform`
(no return to the field after the battle), psx-jp `masterAndManillo` (stops after
area 45) and psx-fr `worldMapAndAreaTransition` (areas 7, 33, 7, 14, without 8):
the Japanese and French text timing against routes recorded in English, not a
button map. No PC-plus install was run (none is one-source).

| install | runs | latches | ms a latch (mean) | guard faults | faults a latch |
|---|---:|---:|---:|---:|---:|
| psx-jp | 11 | 56,160 | 35.1 | 5,238,096 | 93 |
| psx-us | 11 | 56,160 | 35.1 | 5,340,165 | 95 |
| psx-eu-en | 11 | 56,160 | 35.1 | 5,340,200 | 95 |
| psx-fr | 11 | 56,160 | 35.1 | 5,397,852 | 96 |
| psx-de | 11 | 56,160 | 35.1 | 5,400,055 | 96 |
| psp-jp | 11 | 44,820 | 35.4 | 4,072,987 | 91 |
| psp-eu | 11 | 56,160 | 35.1 | 5,216,245 | 93 |

The areas the routes reach: 1, 2, 4, 7, 8, 9, 10, 14, 16, 23, 25, 29, 30, 31, 33,
45, 61, 90, 141, and battles on `combat`, `dragonTransform`, `whelpBoss` and
`cutsceneAndNue`.

## 3. Ranges read before a write

Every range read at least once while nothing had written it, by any run. "Reader"
is the function the first read was in (our function; its Capcom twin's address
where `symbols.toml` has one), then the first two return addresses on its stack;
"first read" is the earliest by logic frame, with its area and install.

| Range | Symbol | class | Read before a write on | Reader (first read) <- callers | Routes | First read (frame, area) |
|---|---|---|---|---|---|---|
| `0x65A608..0x65A62C` | `ChlorineCloud_Offsets` | game-value | psx-jp, psx-us, psx-eu-en, psp-eu | ChlorineCloud_Start (0x49DAD0) <- ChlorineChild_Task (0x49DA80); ChlorineCloud_Run (0x49DAA0) | cutsceneAndNue | cutsceneAndNue 5264, area 0x0017 (psx-jp) |
| `0x663B1C..0x663B28` | `EventScript_OpLengths+0x10` | game-value | psp-jp, psp-eu | Area_Enter (0x594E60) <- GameMode_Enter (0x495900) | attract, caughFish, combat, cutsceneAndNue, dragonTransform, field_menu, masterAndManillo, menu_screens, shop, whelpBoss, worldMapAndAreaTransition | cutsceneAndNue 204, area 0x0017 (psp-eu) |
| `0x663B28..0x663B30` | `Sprite_ScreenMargins` | game-value | psp-jp, psp-eu | Sprite_ProjectA (0x57B860) <- Field_Frame (0x517200); Party_ExtraScreens (0x57B780) | attract, caughFish, masterAndManillo, whelpBoss, worldMapAndAreaTransition | whelpBoss 283, area 0x0002 (psp-eu) |
| `0x669739..0x66973C` | `Char_WhelpSlot+0x3` | game-value | psp-jp, psp-eu | Field_MemberSprite (0x533BA0) <- EventOp_6x (0x57AD10); EventScript_Op (0x579F30); Field_PartyLoad (0x533CE0) | caughFish, combat, cutsceneAndNue, shop, whelpBoss, worldMapAndAreaTransition | combat 178, area 0x0000 (psp-eu) |
| `0x669CCC..0x669CD4` | `Encounter_SlotChance+0x1C` | language-zero | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de, psp-jp, psp-eu | Text_DrawString (0x516B70) <- Crt_sprintf (0x5B9380); Gfx_CommitPrim (0x461E50) | caughFish, combat, cutsceneAndNue, dragonTransform, field_menu, masterAndManillo, menu_screens, shop, whelpBoss, worldMapAndAreaTransition | cutsceneAndNue 136, area 0x0000 (psp-eu) |
| `0x669CD4..0x669CE8` | `Encounter_SlotChance+0x24` | language | psx-jp, psp-jp | FieldPanel_DrawKindRow (0x52D140) <- Field_MembersFrame (0x517350); LeaderPanel_S4RowIn (0x529A60) | caughFish, masterAndManillo | masterAndManillo 2299, area 0x001E (psp-jp) |
| `0x669D28..0x669D60` | `Battle_CommandLabels` | language | psx-jp, psp-jp | Text_DrawString (0x516B70) <- BattleWin_DrawCommandLabel (0x4439A0); Text_DrawAt (0x516B30) | combat, cutsceneAndNue, dragonTransform, whelpBoss | combat 499, area 0x001D (psx-jp) |
| `0x669D7C..0x669DE0` | `Battle_CommandLabelPointers+0x1C` | language | psx-jp, psp-jp | BattleBanner_ShowName (0x44A990); Str_CopyN (0x5171A0) <- BattleBanner_SetMessage (0x44A8E0); BattleInput_NextMember (0x42E9E0); BattleTarget_ShowPrompt (0x447120) | combat, cutsceneAndNue, whelpBoss | whelpBoss 700, area 0x0002 (psp-jp) |
| `0x669FC0..0x66A088` | `CommuName_RecordNames+0x1C` | language | psx-jp, psp-jp | EffectKind0F_DrawGlyph (0x469AD0) <- EffectKind0F_LineType (0x466310); GameMode8_Frame (0x52CF00) | caughFish, masterAndManillo | masterAndManillo 715, area 0x001E (psp-jp) |
| `0x66A0E8..0x66A164` | `CommuName_RecordNames+0x144` | language | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de, psp-jp, psp-eu | Text_CharCount (0x57D800); Text_DrawString (0x516B70) <- BattleMenuWin_ItemListRun (0x59CB40); BattleMenu_DrawItemList (0x59CD00); GeneWin_DrawChoices (0x598E90) | combat, cutsceneAndNue, dragonTransform, masterAndManillo, menu_screens | dragonTransform 976, area 0x008D (psp-eu) |
| `0x66A170..0x66A228` | `CommuName_RecordNames+0x1CC` | language | psx-jp | ucrtbase.dll+0x8D0D0 <- BattleMenuWin_SkillListRun (0x59CBC0); BattleMenu_DrawSkillList (0x59D200) | combat, dragonTransform | combat 514, area 0x001D (psx-jp) |
| `0x66A228..0x66A2FC` | `Menu_Verbs` | language | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de, psp-jp, psp-eu | EffectKind0F_DrawGlyph (0x469AD0); Text_CharCount (0x57D800); Text_DrawString (0x516B70) <- EffectKind0F_LineNext (0x466460); GameMode8_Frame (0x52CF00); Gfx_CommitPrim (0x461E50) | caughFish, cutsceneAndNue, masterAndManillo, menu_screens | masterAndManillo 719, area 0x002D (psx-jp) |
| `0x66A31C..0x66A32C` | `Menu_Verbs+0xF4` | language | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de, psp-jp, psp-eu | Text_DrawString (0x516B70) <- Crt_sprintf (0x5B9380); Gfx_CommitPrim (0x461E50) | cutsceneAndNue, dragonTransform, field_menu, masterAndManillo, menu_screens, worldMapAndAreaTransition | worldMapAndAreaTransition 639, area 0x0007 (psp-eu) |
| `0x65385E..0x65385F` | `Title_Sprites+0xE` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | Title_Sprite (0x462560) <- Prim_SetShade (0x462A70); Title_DrawSetA (0x462820) | attract, caughFish, combat, cutsceneAndNue, dragonTransform, field_menu, masterAndManillo, menu_screens, shop, whelpBoss, worldMapAndAreaTransition | cutsceneAndNue 9, area 0x0000 (psx-eu-en) |
| `0x653EA0..0x653EB4` | `EffectKind0F_PanelTitles` | game-value | psx-fr, psx-de | Text_DrawString (0x516B70) <- Gfx_CommitPrim (0x461E50); Menu_DrawBox (0x57CF60) | caughFish, masterAndManillo | masterAndManillo 750, area 0x001E (psx-de) |
| `0x663660..0x663677` | `Equip_SlotIcons` | game-value | psx-de | Menu_DrawEquipPanel (0x573A80) <- ShopWin_EquipRun (0x59B4F0); Shop_Frame (0x517300) | shop | shop 1767, area 0x0007 (psx-de) |
| `0x66ADBA..0x66ADD8` | `Fish_Kinds+0x72A` | disc-unplaced | psx-jp | Area_ClassifyPending (0x595350) <- Field_ChangeArea (0x594E00); GameMode_Start (0x495840); Scena16_Start (0x56B2B0) | attract, caughFish, combat, cutsceneAndNue, dragonTransform, field_menu, masterAndManillo, menu_screens, shop, whelpBoss, worldMapAndAreaTransition | cutsceneAndNue 203, area 0xFFFF (psx-jp) |

The `language-zero` and `game-value` kinds are `recipes/exe-gaps.tsv`'s; the
`language` rows are the class of that name. `Encounter_SlotChance+0x24` and the
three `Battle_*` / `CommuName_RecordNames+0x1C` / `+0x1CC` rows are read before a
write only on the Japanese builds: on the Western ones the language layer
(`CharNames_Apply`, `BattleCommands_Apply`) writes them first.

### 3.1 Written before any read

Ranges a layer or the engine writes at start-up, before the routes read them (so
the zero is never seen): `Char_DefaultRecords` `0x64B390` (gap) and its three
`unread` neighbours, written by `CharNames_Apply`; `ConfigScreen_OpenStates+0x24`
(language), by `ConfigText_Apply`; `Task_StackTop`'s range (unread), by
`Task_SetStackBase`, read 1,125 times after. Under ja-JP (`psx-jp`, `psp-jp`)
`Char_DefaultRecords` is neither written nor read on these routes (its reader,
`NewGame_InitCharacters`, runs at a new game, and every route loads a save).

## 4. Ranges not read

### 4.1 The `gap` rows

137 `gap` rows (keyed by data.toml's name) were never read before a write on any
run. "Parked" counts the runs in which one of the row's pages went over its
budget at least once: in those runs the watch saw the page's first 16 accesses a
latch and, for the row's words, the debug registers on the latches it held them.
A row whose page was never parked was seen whole: **not read on these routes** is
exact for it. 62 rows are of that kind, 75 had a parked page in some run. The
last column says whether an `AreaNNN_` row's area is one the routes reach.

| Range (first build) | Symbol | kind | builds | runs | runs whose page was parked | debug-register latches / run latches | its area reached |
|---|---|---|---|---:|---|---|---|
| `0x604958..0x604965` | `Area65_DriftUV` | port-relaid | all 7 | 77 | 6 | 438 / 381780 | no |
| `0x6126F4..0x6126FE` | `Area87_DriftUV` | port-relaid | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x613070..0x613078` | `Area88_DriftUV` | port-relaid | all 7 | 77 | 7 | 1526 / 381780 | no |
| `0x61478C..0x61478F` | `Area91_RingTurns` | game-value | all 7 | 77 | 7 | 631 / 381780 | no |
| `0x616DE4..0x616DEE` | `Area94_EffectArgsE` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x617EAC..0x617EB8` | `Area96_Handlers+0x184` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x61B4F0..0x61B4F7` | `Area104_PlateAnims` | port-relaid | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x62034C..0x6203A4` | `Area115_Sprites` | disc-unplaced | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x6203A4..0x6203BC` | `Area115_Buttons` | disc-unplaced | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x6203C8..0x6203D8` | `Area115_Directions` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x6203D8..0x6203E0` | `Area115_Record8Anims` | port-relaid | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x6203E8..0x6203F8` | `Area115_DriftUV` | port-relaid | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x6246F4..0x6246FC` | `Area121_Record8Anims` | port-relaid | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x624720..0x624723` | `Area121_BandHeights` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x62F6A4..0x62F6BB` | `Area141_CellEntries+0x7C` | game-value | all 7 | 77 | 21 | 1967 / 381780 | yes |
| `0x635344..0x635358` | `Area146_EffectStates+0x1230` | disc-unplaced | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x635360..0x635364` | `Area146_EffectStates+0x124C` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x637284..0x637286` | `Area151_DriftUV+0x8` | port-relaid | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x63D5D8..0x63D5DC` | `Area169_Rects+0xB9C` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x63FF24..0x63FF2C` | `Area172_EffectStates+0xFF0` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x649750..0x649754` | `Area196_Keys1+0x658` | game-value | all 7 | 77 | 0 | 0 / 381780 | no |
| `0x64B390..0x64B397` | `Char_DefaultRecords` | game-value | all 7 | 77 | 55 | 7681 / 381780 | - |
| `0x64B80C..0x64B813` | `Char_DefaultRecords+0x47C` | game-value | psp-eu | 11 | 11 | 1433 / 56160 | - |
| `0x64D478..0x64D484` | `BossDolphin_Anims` | disc-unplaced | all 7 | 77 | 22 | 1018 / 381780 | - |
| `0x64D484..0x64D48C` | `BossDolphin_Sounds` | game-value | all 7 | 77 | 22 | 753 / 381780 | - |
| `0x64D78E..0x64D798` | `BossGaist_Anims+0x2` | game-value | all 7 | 77 | 22 | 1017 / 381780 | - |
| `0x64D798..0x64D7A0` | `BossGaist_Sounds` | disc-unplaced | all 7 | 77 | 22 | 771 / 381780 | - |
| `0x64D7A0..0x64D7A8` | `BossTorch_Sounds` | game-value | all 7 | 77 | 22 | 771 / 381780 | - |
| `0x653144..0x65314F` | `CommuMusic_CloseSteps+0x68` | game-value | psx-jp, psx-us, psp-jp, psp-eu | 44 | 44 | 9050 / 213300 | - |
| `0x653200..0x653210` | `CommuRank_Modes+0x8C` | language-zero | all 7 | 77 | 77 | 19027 / 381780 | - |
| `0x653815..0x65382D` | `ConfigScreen_OpenStates+0x141` | game-value | all 7 | 77 | 77 | 29596 / 381780 | - |
| `0x6542EC..0x6542F0` | `EffectKind27_States+0xC` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x654328..0x65432C` | `EffectKind29_States+0x18` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x6543F0..0x654400` | `EffectKind33_SignX` | disc-unplaced | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x654400..0x654410` | `EffectKind33_SignZ` | disc-unplaced | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x6549E8..0x6549EC` | `EffectKind65_ShakeSteps` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x654AF8..0x654AFB` | `EffectKind75_States+0xC` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x654D85..0x654D9C` | `EffectKind86_States+0x11` | game-value | psp-jp, psp-eu | 22 | 0 | 0 / 100980 | - |
| `0x6550E0..0x655100` | `EffectKind99_States+0x10` | disc-unplaced | all 7 | 77 | 7 | 5920 / 381780 | - |
| `0x655208..0x655210` | `EffectKindA0_States+0x24` | disc-unplaced | all 7 | 77 | 7 | 2054 / 381780 | - |
| `0x656A80..0x656A84` | `GameMode3_Steps+0xC` | game-value | all 7 | 77 | 77 | 7404 / 381780 | - |
| `0x65A678..0x65A67C` | `SnapWave_FacingPhase` | game-value | all 7 | 77 | 12 | 298 / 381780 | - |
| `0x65A6DC..0x65A6E8` | `Magic020Slash_Animations` | game-value | all 7 | 77 | 12 | 595 / 381780 | - |
| `0x65A919..0x65A924` | `WardMote_Tilts+0x1` | game-value | psp-jp, psp-eu | 22 | 2 | 104 / 100980 | - |
| `0x65AA64..0x65AA67` | `BoneDanceShake_Counts` | game-value | all 7 | 77 | 12 | 297 / 381780 | - |
| `0x65AAAD..0x65AAC4` | `TornadoMote_Phases+0x15` | game-value | psp-jp, psp-eu | 22 | 2 | 156 / 100980 | - |
| `0x65B1D4..0x65B1D7` | `ReviveHalo_MoteCounts` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x65B528..0x65B52C` | `Magic087_RisePhases+0x8` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x65BB24..0x65BB28` | `WhelpBreath_GlowAngles` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x65BE21..0x65BE34` | `InkPuff_Offsets+0x11` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x65BE50..0x65BE74` | `InkInkPuff_Offsets` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x65C1A8..0x65C1BC` | `MagicBallOrb_Shades` | disc-unplaced | all 7 | 77 | 77 | 22196 / 381780 | - |
| `0x65C21C..0x65C224` | `AuraBreathDome_V` | disc-unplaced | all 7 | 77 | 77 | 10488 / 381780 | - |
| `0x65C224..0x65C22C` | `AuraBreathDome_Height` | game-value | all 7 | 77 | 77 | 10482 / 381780 | - |
| `0x65E2DC..0x65E2E0` | `EffectKind18Sub1C_States+0x14` | game-value | all 7 | 77 | 21 | 508 / 381780 | - |
| `0x65E2F4..0x65E2F8` | `EffectKind18Sub1D_States+0x14` | game-value | all 7 | 77 | 21 | 508 / 381780 | - |
| `0x65E330..0x65E333` | `EffectKind18Sub21_States+0x8` | game-value | all 7 | 77 | 21 | 508 / 381780 | - |
| `0x65EB44..0x65EB50` | `EffectKind18Sub2E_States+0x14` | game-value | psx-jp, psx-us, psx-fr, psx-de, psp-jp, psp-eu | 66 | 18 | 838 / 325620 | - |
| `0x65EB64..0x65EB6C` | `EffectKind18Sub2F_States+0x14` | game-value | psx-jp, psx-us, psx-fr, psx-de, psp-jp, psp-eu | 66 | 18 | 631 / 325620 | - |
| `0x65EB80..0x65EB88` | `EffectKind18Sub30_States+0x14` | game-value | psx-jp, psx-eu-en, psx-de, psp-jp, psp-eu | 55 | 15 | 522 / 269460 | - |
| `0x65EB9C..0x65EBA4` | `EffectKind18Sub31_States+0x14` | game-value | all 7 | 77 | 21 | 763 / 381780 | - |
| `0x65EBB8..0x65EBC0` | `EffectKind18Sub32_States+0x14` | game-value | psx-jp, psx-us, psx-eu-en, psx-de, psp-jp, psp-eu | 66 | 18 | 632 / 325620 | - |
| `0x65EBD4..0x65EBD8` | `EffectKind18Sub33_States+0x14` | game-value | all 7 | 77 | 21 | 507 / 381780 | - |
| `0x65EC04..0x65EC08` | `EffectKind18Sub37_States+0x10` | game-value | all 7 | 77 | 21 | 507 / 381780 | - |
| `0x65EC38..0x65EC40` | `EffectKind18Sub38_States+0x10` | game-value | all 7 | 77 | 21 | 762 / 381780 | - |
| `0x65F578..0x65F57C` | `EffectKind18Sub64_States+0x1C` | game-value | all 7 | 77 | 67 | 5660 / 381780 | - |
| `0x660CCC..0x660CD0` | `LinkedObjectD_Amounts` | game-value | all 7 | 77 | 26 | 1479 / 381780 | - |
| `0x660E48..0x660E50` | `Scena02_EffectX` | game-value | all 7 | 77 | 26 | 2094 / 381780 | - |
| `0x660F94..0x660F98` | `Scena03_Bob4` | game-value | all 7 | 77 | 26 | 1478 / 381780 | - |
| `0x660F98..0x660F9C` | `Scena03_Bob2` | game-value | all 7 | 77 | 26 | 1478 / 381780 | - |
| `0x660FFC..0x661000` | `Scena04_Quake` | game-value | all 7 | 77 | 26 | 1478 / 381780 | - |
| `0x661000..0x661004` | `Scena04_TremorSteps` | game-value | all 7 | 77 | 0 | 0 / 381780 | - |
| `0x6636B0..0x6636C0` | `Equip_SlotIcons+0x50` | game-value | all 7 | 77 | 77 | 17180 / 381780 | - |
| `0x6636CC..0x6636DC` | `Equip_SlotIcons+0x6C` | game-value | all 7 | 77 | 77 | 17170 / 381780 | - |
| `0x6636E8..0x6636F8` | `Equip_SlotIcons+0x88` | game-value | all 7 | 77 | 77 | 17165 / 381780 | - |
| `0x663704..0x663714` | `Equip_SlotIcons+0xA4` | game-value | all 7 | 77 | 77 | 17156 / 381780 | - |
| `0x663720..0x663730` | `Equip_SlotIcons+0xC0` | game-value | all 7 | 77 | 77 | 17151 / 381780 | - |
| `0x66373C..0x66374C` | `Equip_SlotIcons+0xDC` | game-value | all 7 | 77 | 77 | 17149 / 381780 | - |
| `0x663758..0x663768` | `Equip_SlotIcons+0xF8` | game-value | all 7 | 77 | 77 | 17140 / 381780 | - |
| `0x663774..0x663784` | `Equip_SlotIcons+0x114` | game-value | all 7 | 77 | 77 | 17133 / 381780 | - |
| `0x663790..0x6637A0` | `Equip_SlotIcons+0x130` | game-value | all 7 | 77 | 77 | 17128 / 381780 | - |
| `0x6637AC..0x6637BC` | `Equip_SlotIcons+0x14C` | game-value | all 7 | 77 | 77 | 17125 / 381780 | - |
| `0x6671EC..0x6671F1` | `Music_LoadedTrack+0x1E30` | game-value | all 7 | 77 | 77 | 9912 / 381780 | - |
| `0x667518..0x66751F` | `ConfigMenu_Steps+0xC` | game-value | psp-jp, psp-eu | 22 | 22 | 2776 / 100980 | - |
| `0x669789..0x6697B0` | `MoveScript_EffectArg+0x45` | disc-unplaced | psp-jp, psp-eu | 22 | 20 | 9942 / 100980 | - |
| `0x669CE8..0x669CED` | `Encounter_SlotChance+0x38` | language-zero | all 7 | 77 | 73 | 10141 / 381780 | - |
| `0x66A094..0x66A096` | `CommuName_RecordNames+0xF0` | language-zero | all 7 | 77 | 77 | 6692 / 381780 | - |
| `0x66A098..0x66A0AC` | `CommuName_RecordNames+0xF4` | language-zero | all 7 | 77 | 77 | 20060 / 381780 | - |
| `0x5EC5EC..0x5EC5F0` | `Area26_EffectKinds+0x4` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x6130E8..0x6130FC` | `Area88_DriftUV+0x78` | port-relaid | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 5 | 1305 / 280800 | no |
| `0x621610..0x62161D` | `Area118_SpawnKinds` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x62390C..0x623910` | `Area121_Cells+0x26C` | disc-unplaced | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x62660C..0x626614` | `Area124_Weights+0x50` | disc-unplaced | psx-eu-en, psx-de | 22 | 0 | 0 / 112320 | no |
| `0x62C168..0x62C175` | `Area135_EffectKinds` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x63A25C..0x63A280` | `Area154_ChoicePairs+0x18C8` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x63A34C..0x63A370` | `Area154_ChoicePairs+0x19B8` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x63CA46..0x63CB03` | `Area169_Rects+0xA` | disc-unplaced | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | no |
| `0x653104..0x653150` | `CommuMusic_CloseSteps+0x28` | disc-unplaced | psx-fr, psx-de | 22 | 22 | 17053 / 112320 | - |
| `0x654344..0x65434C` | `EffectKind2A_States+0x10` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | - |
| `0x654770..0x654798` | `EffectKind66_States+0x18` | disc-unplaced | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x654858..0x654876` | `EffectKind5B_States+0xC` | disc-unplaced | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x654A9C..0x654AA0` | `EffectKind6D_States+0x48` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | - |
| `0x654D84..0x654D9C` | `EffectKind86_States+0x10` | disc-unplaced | psx-de | 11 | 0 | 0 / 56160 | - |
| `0x655168..0x655170` | `EffectKind9A_Branches` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 5 | 1466 / 280800 | - |
| `0x65A698..0x65A69C` | `AirRaidImage_ShadeSteps` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 10 | 246 / 280800 | - |
| `0x65A918..0x65A924` | `WardMote_Tilts` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 10 | 490 / 280800 | - |
| `0x65AAA8..0x65AAC4` | `TornadoMote_Phases+0x10` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 10 | 976 / 280800 | - |
| `0x65B8F4..0x65B918` | `Fx106_ShardTables` | disc-unplaced | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 0 | 0 / 280800 | - |
| `0x65BB78..0x65BB80` | `BreathMote_Angles` | game-value | psx-us, psx-eu-en, psx-de | 33 | 0 | 0 / 168480 | - |
| `0x65DE5E..0x65DE78` | `EffectKind18_08_States+0x8A` | disc-unplaced | psx-us, psx-fr, psx-de | 33 | 33 | 11078 / 168480 | - |
| `0x65EED8..0x65EEDC` | `EffectKind18Sub45_States+0xC` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr, psx-de | 55 | 15 | 395 / 280800 | - |
| `0x65F438..0x65F450` | `EffectKind18Sub5D_States+0x14` | disc-unplaced | psx-jp, psx-us, psx-fr, psx-de | 44 | 40 | 10915 / 224640 | - |
| `0x66219C..0x6621AA` | `Scena17_EndSteps+0xD0` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x6621FC..0x662206` | `Scena17_EndSteps+0x130` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662248..0x662254` | `Scena17_EndSteps+0x17C` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662430..0x662571` | `Scena17_EndSteps+0x364` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662730..0x66273F` | `Scena17_EndSteps+0x664` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662772..0x66277E` | `Scena17_EndSteps+0x6A6` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x6628DC..0x6628EC` | `Scena17_EndSteps+0x810` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662958..0x6629CD` | `Scena17_EndSteps+0x88C` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x6629F1..0x662A01` | `Scena17_EndSteps+0x925` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662A1C..0x662A24` | `Scena17_EndSteps+0x950` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 0 | 0 / 224640 | - |
| `0x662A5C..0x662A7E` | `Scena17_EndSteps+0x990` | game-value | psx-eu-en, psx-fr, psx-de | 33 | 0 | 0 / 168480 | - |
| `0x662AC5..0x662AEA` | `Scena17_EndSteps+0x9F9` | disc-unplaced | psx-eu-en, psx-fr, psx-de | 33 | 0 | 0 / 168480 | - |
| `0x664288..0x664298` | `SharedList_SortSteps+0xC` | game-value | psx-fr, psx-de | 22 | 2 | 95 / 112320 | - |
| `0x66B1F3..0x66B1F8` | `MenuList_GeneWinSteps+0x27` | game-value | psx-de | 11 | 11 | 1158 / 56160 | - |
| `0x66B468..0x66B46E` | `MasterWin_Requirements+0x44` | game-value | psx-us, psx-eu-en, psx-fr, psx-de | 44 | 44 | 6146 / 224640 | - |
| `0x653142..0x653150` | `CommuMusic_CloseSteps+0x66` | game-value | psx-eu-en | 11 | 11 | 3142 / 56160 | - |
| `0x653FA8..0x653FBC` | `EffectKind3C_Op` | disc-unplaced | psx-us, psx-eu-en, psx-fr | 33 | 33 | 9193 / 168480 | - |
| `0x654D84..0x654D9C` | `EffectKind86_States+0x10` | game-value | psx-jp, psx-us, psx-eu-en, psx-fr | 44 | 0 | 0 / 224640 | - |
| `0x663670..0x663678` | `Equip_SlotIcons+0x10` | game-value | psx-fr | 11 | 11 | 1360 / 56160 | - |
| `0x66B1F1..0x66B1F8` | `MenuList_GeneWinSteps+0x25` | game-value | psx-fr | 11 | 11 | 1356 / 56160 | - |
| `0x65DE60..0x65DE78` | `EffectKind18_08_States+0x8C` | disc-unplaced | psx-jp | 11 | 11 | 4287 / 56160 | - |
| `0x662978..0x6629B1` | `Scena17_EndSteps+0x8AC` | game-value | psx-us | 11 | 0 | 0 / 56160 | - |
| `0x6629C6..0x6629CD` | `Scena17_EndSteps+0x8FA` | game-value | psx-us | 11 | 0 | 0 / 56160 | - |
| `0x662A77..0x662A7E` | `Scena17_EndSteps+0x9AB` | game-value | psx-us | 11 | 0 | 0 / 56160 | - |
| `0x662ADC..0x662AEA` | `Scena17_EndSteps+0xA10` | game-value | psx-us | 11 | 0 | 0 / 56160 | - |

### 4.2 The targeted runs

A row of at most two words on a hot page, run once more with
`BOF3X_GAPWATCH_PIN=<its address>`: its words in the main thread's debug
registers on every latch of the run (`dr_latches` equal to the run's latches in
each file), on a route that reaches its reader's screen or scene as far as the
census's reader says. Fifteen runs:

| Row | Install, route | Latches | Result |
|---|---|---:|---|
| `MasterWin_Requirements+0x44` `0x66B468` (P1 item 8) | psx-us, psx-eu-en, psx-fr, psx-de: masterAndManillo | 5,520 each | not read, not written |
| `Music_LoadedTrack+0x1E30` `0x6671EC` (P4) | psx-us: attract; whelpBoss | 10,980; 13,140 | not read |
| `ConfigMenu_Steps+0xC` `0x667518` (P4, PSP) | psp-eu: field_menu; menu_screens | 1,440; 1,740 | **read**, 8 reads each, all before any write: `Char_ApplyTraits` (0x590800) from `Char_RecalcStats` (0x590660) and `TitleFlow_EnterGame` (0x588800), frame 750, area 0 - the save's load |
| `Encounter_SlotChance+0x38` `0x669CE8` | psx-us: cutsceneAndNue (the Equip window in the Nue fight) | 7,380 | not read |
| `CommuName_RecordNames+0xF0` `0x66A094` | psx-us: caughFish | 3,840 | not read |
| `GameMode3_Steps+0xC` `0x656A80` | psx-us: whelpBoss | 13,140 | not read |
| `EffectKind18Sub64_States+0x1C` `0x65F578` | psx-us: worldMapAndAreaTransition | 2,100 | not read |
| `Equip_SlotIcons+0x10` `0x663670` (FR `EXP`) | psx-fr: menu_screens | 1,740 | not read |
| `MenuList_GeneWinSteps+0x25` / `+0x27` | psx-fr / psx-de: dragonTransform | 4,320 each | not read |

`ConfigMenu_Steps+0xC` was on a page parked in all 22 PSP batch runs, its words
under the turn's registers on 2,776 of 100,980 latches, and its eight reads come
at one moment of a run (the load): the sampling missed it. It is the case for
which the pin exists, and the warning for 4.1's 75 sampled rows. The master
route reaches areas 90, 45, 16, 30, 16, 61 on all four Western installs; whether
it opens a master's window, where `MasterWin_Available` reads the list, is not
established here - a recipe that does, with the same pin, would settle the row.
The debug registers watch the main thread only (the game's logic; the sound
thread is not watched by them).

### 4.3 `unread` and `language`

No `unread` range is read before a write on any run (3.1's `Task_StackTop` range
is written first). `language` ranges not in section 3 were not read before a
write. Both classes had parked pages in places (`0x66A000` and `0x66C000` above
all), so the same sampling caveat as 4.1 applies to them there.

## 5. The census's section 8, re-ranked by the watch

A row read in play (section 3) is a live defect on the installs named; a row not
read on these routes can wait for a route that reaches its reader. The census's
own priorities are kept inside each band.

**Read in play - first:**

1. **`ChlorineCloud_Offsets`** (P3 in the census; 4 builds read it: psx-jp,
   psx-us, psx-eu-en, psp-eu - not psx-fr / psx-de / psp-jp, whose cutsceneAndNue
   runs did not reach the read): `ChlorineCloud_Start` reads the offsets at the
   Nue fight (area 0x17, frame ~5,300). Stage B's OF-E row (magic and effects).
2. **The PSP's default light pair** `0x663B1C..0x663B28` (P2 item 12): read by
   `Area_Enter` on every area entry, every route, psp-jp and psp-eu. PB's row.
3. **`Sprite_ScreenMargins`** `0x663B28..0x663B30` (P2 item 12): read by
   `Sprite_ProjectA` from `Field_Frame` - about 95,000 reads on psp-eu's five
   routes. PB's row.
4. **`Fish_Kinds+0x72A`** `0x66ADBA..0x66ADD8` (P2 item 11, JP only): read by
   `Area_ClassifyPending` from `Field_ChangeArea` at every area change on
   psx-jp, not only when fishing. TW's row.
5. **`Char_WhelpSlot+0x3`** `0x669739..0x66973C` (P3, PSP): read by
   `Field_MemberSprite` (from `EventOp_6x`, `Field_PartyLoad`) on six routes,
   psp-jp and psp-eu. The census left it "a single byte, no safe twin"; it is read.
6. **`Equip_SlotIcons` labels** `0x663660..0x663677` (P3, FR / DE): read by
   `Menu_DrawEquipPanel` in the shop on psx-de. psx-fr's row is the `EXP` label
   alone (`0x663670..0x663678`, `field_o`'s `kExpLabel`), which the shop's equip
   panel does not read; not read on psx-fr's runs. LA's row.
7. **`EffectKind0F_PanelTitles`** `0x653EA0..0x653EB4` (D286, FR / DE): read through
   `Text_DrawString` from `Menu_DrawBox` on caughFish and masterAndManillo, psx-fr
   and psx-de. LA's row; D286 confirmed read.
8. **`Title_Sprites+0xE`** (D266, OWNER CALL) and **`Encounter_SlotChance+0x1C`**
   (the empty save slot, OWNER CALL): read on every Western PSX run's title and
   on every load screen of all seven. The owner's calls; the watch only confirms
   that a player meets both.

**Not read on these routes, page seen whole** (62 rows of 4.1 with 0 parked runs):
wait for a route that reaches the reader - they are mostly area tables of areas
the routes never enter (115, 118, 121, 135, 146, 154, 169, 172, 196) and effect,
magic and boss rows whose scenes the routes do not play.

**Not read on these routes, page sampled** (75 rows): the same, with the caveat
that a read on a parked latch outside the two debug-register words could be
missed. `MasterWin_Requirements+0x44` is settled for its route by 4.2.

**P4:** `ConfigMenu_Steps+0xC` (PSP) **is read**, at every save load, by
`Char_ApplyTraits` - promote it to the read band (it reaches the stats
calculation; what a zero does there is not measured). `Music_LoadedTrack+0x1E30`
is not read on the attract or whelpBoss with its words pinned: "likely unread"
holds for those runs.

**P1 item 7** (`Area169_Rects+0xA`): area 169 is on no route; not measured.
**P2 items 11 and 13 other than `Fish_Kinds`**, **P3's other rows**: unchanged,
not reached.

## 6. Handed on

- **TW:** `Fish_Kinds+0x72A` (JP) is read at every area change - first of its P2
  list.
- **PB:** the light pair and `Sprite_ScreenMargins` are read on every area entry
  and every field frame of both PSP installs; `Char_WhelpSlot+0x3` is read on six
  routes (it is not PB's in the staging - the census left it "a listed gap" - so
  it goes to the coordinator as a live P3 that needs a source);
  `ConfigMenu_Steps+0xC`, the census's `BOOT.BIN`-once row, is read at every save
  load on psp-eu (4.2).
- **Stage B, OF-E:** `ChlorineCloud_Offsets` is read at the Nue fight on four
  builds.
- **LA:** `EffectKind0F_PanelTitles` (FR, DE) and `Equip_SlotIcons` labels (DE)
  are read; and three `language` rows are read in place before any layer writes
  them on all seven installs (`Menu_Verbs` by `Menu_DrawButtonRow` /
  `MenuList_ButtonRow` and `Text_CharCount`; `Menu_Verbs+0xF4` as a
  `Crt_sprintf` format; `CommuName_RecordNames+0x144` by `GeneWin_DrawChoices`
  and `BattleMenu_DrawItemList`), five more on the Japanese builds only (3). What
  those draws show was not captured.
- **RD:** both P4 rows measured by pinned runs (4.2): `ConfigMenu_Steps+0xC` has a
  reader, `Char_ApplyTraits` (0x590800) from `Char_RecalcStats` (0x590660) at the
  save's load, eight reads before any write on psp-eu; `Music_LoadedTrack+0x1E30`
  was not read on psx-us's attract and whelpBoss with its words pinned.
- **The coordinator, I43 step 4:** see 7.

No ledger entry is drafted (the watch is an instrument and changes nothing the
game keeps) and no defect is numbered. Defects described here, for numbering at
the merge if the coordinator files them: `ChlorineCloud_Offsets` read as zero at
the Nue fight (four builds); `Char_WhelpSlot+0x3` read as zero (PSP);
`ConfigMenu_Steps+0xC` read as zero by the stats calculation at every save load
(PSP);
`Fish_Kinds+0x72A` read as zero at every area change (JP); `Equip_SlotIcons`
labels read as zero in the shop (DE); the `language` rows read in place (three
on every install, five more under ja-JP).

## 7. For `BOF3X_EXEIMAGE_STRICT=1` (I43 step 4)

The staging makes the default wait on "RW's list shows no range read in play
still zero". **It shows ten** (section 3 and 4.2), on every install kind: each
Western PSX install reads at least `Title_Sprites+0xE` and
`Encounter_SlotChance+0x1C`; psx-jp reads `Fish_Kinds+0x72A`,
`ChlorineCloud_Offsets` and the empty-slot row; the two PSPs read the light pair,
the screen margins, `Char_WhelpSlot+0x3` and the empty-slot row, and psp-eu
`ConfigMenu_Steps+0xC` (measured there; psp-jp has the same row). Strict as the
default would refuse every one-source install at start today. Until the eight
live rows are placed and the owner has made the two calls, step 4 is not ready.

A clean list from this watch would also not be enough on its own: these eleven
routes reach 19 areas and four battles. For step 4 the watch wants routes through
the areas and scenes of the rows still unread - the census's readers name them -
or a long recorded play.

## 8. Running it

```
BOF3X_GAPWATCH=<dir> [BOF3X_GAPWATCH_CLASSES=gap,unread,language] [BOF3X_GAPWATCH_BUDGET=16]
  [BOF3X_GAPWATCH_DR=0] [BOF3X_GAPWATCH_PIN=0x66B468]       # an exe-host run from a disc-only install
python tools/gapwatch.py report <dir> --dll <that run's bof3x.dll> [--tsv out.tsv] [--all]
python tools/gapwatch.py pages <file> --dll <dll>
```

Under the A/B/C harness the variables go in the install's `env` table of an
installs file (the harness passes no `BOF3X_*` of the caller's); this round's was
`<root>\installs.toml`, with `BOF3X_DATLOG=1` beside them to see which areas a run
reached. **Symbolise with the DLL the runs used**: the readers are offsets into
`bof3x.dll`, and a rebuild moves them (a later DLL named this round's readers
wrongly until the run's own was rebuilt).

The targeted runs' scripts and installs files are in the root's `scripts\`
(`pin.sh`, `pin2.sh`, `pin3.sh`).

Traps met: Defender quarantined the launcher of each new local build
(`Trojan:Win32/Bearfoos.A!ml`, `Behavior:Win32/DefenseEvasion.A!ml`), the build's
own copy included; a launcher linked in a second build directory (a different
binary) ran until it too was taken. A run whose launcher was taken mid-run still
finished. The handler must not read a word under its own debug register (it
traps inside the handler and the process dies without a `CRASH` line); hence the
read / write register pairs.

## Appendix: every run

Latches watched, the mean wall time a latch, the guard faults taken, and how many watched ranges (of every class) the run read at all.

| install | route | latches | ms a latch | guard faults | ranges read (any) |
|---|---|---:|---:|---:|---:|
| psx-jp | attract_11000 | 10980 | 34.1 | 894,967 | 2 of 299 |
| psx-jp | caughFish | 3840 | 35.4 | 463,652 | 6 of 299 |
| psx-jp | combat | 2580 | 36.5 | 205,918 | 7 of 299 |
| psx-jp | cutsceneAndNue | 7380 | 34.9 | 608,093 | 9 of 299 |
| psx-jp | dragonTransform | 4320 | 35.4 | 452,734 | 7 of 299 |
| psx-jp | field_menu | 1440 | 41.1 | 81,084 | 4 of 299 |
| psx-jp | masterAndManillo | 5520 | 34.8 | 631,810 | 5 of 299 |
| psx-jp | menu_screens | 1740 | 39.5 | 122,127 | 6 of 299 |
| psx-jp | shop | 3120 | 35.9 | 285,421 | 3 of 299 |
| psx-jp | whelpBoss | 13140 | 34.0 | 1,303,719 | 5 of 299 |
| psx-jp | worldMapAndAreaTransition | 2100 | 37.1 | 188,571 | 4 of 299 |
| psx-us | attract_11000 | 10980 | 34.1 | 925,024 | 2 of 324 |
| psx-us | caughFish | 3840 | 35.4 | 454,924 | 4 of 324 |
| psx-us | combat | 2580 | 36.5 | 209,561 | 4 of 324 |
| psx-us | cutsceneAndNue | 7380 | 34.9 | 618,951 | 8 of 324 |
| psx-us | dragonTransform | 4320 | 35.3 | 457,647 | 6 of 324 |
| psx-us | field_menu | 1440 | 41.1 | 84,815 | 4 of 324 |
| psx-us | masterAndManillo | 5520 | 34.8 | 616,511 | 5 of 324 |
| psx-us | menu_screens | 1740 | 39.6 | 126,044 | 6 of 324 |
| psx-us | shop | 3120 | 35.9 | 288,174 | 3 of 324 |
| psx-us | whelpBoss | 13140 | 34.0 | 1,365,179 | 4 of 324 |
| psx-us | worldMapAndAreaTransition | 2100 | 37.2 | 193,335 | 4 of 324 |
| psx-eu-en | attract_11000 | 10980 | 34.1 | 925,023 | 2 of 320 |
| psx-eu-en | caughFish | 3840 | 35.4 | 454,941 | 4 of 320 |
| psx-eu-en | combat | 2580 | 36.5 | 209,561 | 4 of 320 |
| psx-eu-en | cutsceneAndNue | 7380 | 34.8 | 618,951 | 8 of 320 |
| psx-eu-en | dragonTransform | 4320 | 35.4 | 457,647 | 6 of 320 |
| psx-eu-en | field_menu | 1440 | 41.4 | 84,815 | 4 of 320 |
| psx-eu-en | masterAndManillo | 5520 | 34.8 | 616,512 | 5 of 320 |
| psx-eu-en | menu_screens | 1740 | 39.6 | 126,044 | 6 of 320 |
| psx-eu-en | shop | 3120 | 35.9 | 288,174 | 3 of 320 |
| psx-eu-en | whelpBoss | 13140 | 34.0 | 1,365,197 | 4 of 320 |
| psx-eu-en | worldMapAndAreaTransition | 2100 | 37.2 | 193,335 | 4 of 320 |
| psx-fr | attract_11000 | 10980 | 34.1 | 924,749 | 2 of 324 |
| psx-fr | caughFish | 3840 | 35.4 | 453,160 | 5 of 324 |
| psx-fr | combat | 2580 | 36.5 | 209,596 | 4 of 324 |
| psx-fr | cutsceneAndNue | 7380 | 34.8 | 601,762 | 3 of 324 |
| psx-fr | dragonTransform | 4320 | 35.3 | 457,665 | 6 of 324 |
| psx-fr | field_menu | 1440 | 41.0 | 84,831 | 4 of 324 |
| psx-fr | masterAndManillo | 5520 | 34.8 | 622,195 | 6 of 324 |
| psx-fr | menu_screens | 1740 | 39.5 | 126,059 | 6 of 324 |
| psx-fr | shop | 3120 | 35.9 | 296,819 | 3 of 324 |
| psx-fr | whelpBoss | 13140 | 34.0 | 1,422,629 | 4 of 324 |
| psx-fr | worldMapAndAreaTransition | 2100 | 37.1 | 198,387 | 4 of 324 |
| psx-de | attract_11000 | 10980 | 34.1 | 925,390 | 2 of 327 |
| psx-de | caughFish | 3840 | 35.4 | 454,358 | 5 of 327 |
| psx-de | combat | 2580 | 36.5 | 209,665 | 4 of 327 |
| psx-de | cutsceneAndNue | 7380 | 34.8 | 611,960 | 3 of 327 |
| psx-de | dragonTransform | 4320 | 35.3 | 457,908 | 6 of 327 |
| psx-de | field_menu | 1440 | 41.2 | 84,831 | 4 of 327 |
| psx-de | masterAndManillo | 5520 | 34.9 | 625,590 | 6 of 327 |
| psx-de | menu_screens | 1740 | 39.6 | 126,059 | 6 of 327 |
| psx-de | shop | 3120 | 35.9 | 290,039 | 4 of 327 |
| psx-de | whelpBoss | 13140 | 34.0 | 1,420,905 | 4 of 327 |
| psx-de | worldMapAndAreaTransition | 2100 | 37.1 | 193,350 | 4 of 327 |
| psp-jp | attract_11000 | 10980 | 34.1 | 870,714 | 3 of 284 |
| psp-jp | caughFish | 3840 | 35.4 | 463,907 | 8 of 284 |
| psp-jp | combat | 480 | 50.0 | 22,939 | 4 of 284 |
| psp-jp | cutsceneAndNue | 4440 | 35.5 | 351,327 | 6 of 284 |
| psp-jp | dragonTransform | 840 | 42.8 | 51,526 | 4 of 284 |
| psp-jp | field_menu | 1440 | 41.0 | 81,084 | 4 of 284 |
| psp-jp | masterAndManillo | 5520 | 34.8 | 614,199 | 8 of 284 |
| psp-jp | menu_screens | 1140 | 40.3 | 56,915 | 4 of 284 |
| psp-jp | shop | 900 | 42.2 | 69,963 | 4 of 284 |
| psp-jp | whelpBoss | 13140 | 34.0 | 1,301,913 | 7 of 284 |
| psp-jp | worldMapAndAreaTransition | 2100 | 37.1 | 188,500 | 6 of 284 |
| psp-eu | attract_11000 | 10980 | 34.1 | 878,015 | 3 of 289 |
| psp-eu | caughFish | 3840 | 35.4 | 453,595 | 6 of 289 |
| psp-eu | combat | 2580 | 36.5 | 209,654 | 5 of 289 |
| psp-eu | cutsceneAndNue | 7380 | 34.8 | 602,978 | 9 of 289 |
| psp-eu | dragonTransform | 4320 | 35.4 | 456,419 | 6 of 289 |
| psp-eu | field_menu | 1440 | 41.1 | 82,821 | 4 of 289 |
| psp-eu | masterAndManillo | 5520 | 34.8 | 608,428 | 6 of 289 |
| psp-eu | menu_screens | 1740 | 39.4 | 123,150 | 6 of 289 |
| psp-eu | shop | 3120 | 35.9 | 285,178 | 4 of 289 |
| psp-eu | whelpBoss | 13140 | 34.0 | 1,327,107 | 6 of 289 |
| psp-eu | worldMapAndAreaTransition | 2100 | 37.1 | 188,900 | 6 of 289 |
