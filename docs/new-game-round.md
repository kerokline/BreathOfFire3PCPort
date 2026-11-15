# The New Game round: the name entry and the options screen back in the New Game sequence

**Status:** IN PROGRESS, stages A and B built (2026-11-14, on the local
branch `new-game/round` off `main` `651a5ad8`, not pushed; IDEAS I34. Group
M ran on 2026-11-12: section 3a. Stage A: 4a. Stage B, the callers, is
built and merged on the branch: 4b is the owner's word, the 4c sections are
each group as run, 4d is the merged tree as verified and what is owed.
New Game runs the name screen and the options screen, the faerie village
renamer runs the grid, and the launcher has the two boxes; **the ledger has
no entry yet, so the branch is not mergeable to `main` as it is (rule 2):
stage C writes the entries, runs the proof on a quiet machine, and the
owner plays it.** Sections 4 to 9 are as first staged and are amended by
3b, 4a and 4b.)

The owner (2026-11-12): stage a round for the character naming screen and the
options menu at New Game. On the PlayStation, New Game asks for the hero's
name, then shows the options screen, then starts the game (the owner,
2026-09-19, [`save-interchange.md`](save-interchange.md) 2). The port goes
straight to the game, moved the options screen to a third row of the title
menu, and cut the name entry everywhere, the faerie village's renamer
included ([`name-entry-restoration.md`](name-entry-restoration.md), the
research this round builds from; its section 4 is the routine's skeleton).

No grid byte, label, legend string or message of either game appears in this
note (rule 1). The grid and its strings are read from the player's own disc
on the player's machine and live in the language layer, never in the
repository.

## 0. The shape in one paragraph

One name-entry routine of ours (the grid's draw, the legend, the input step),
reimplemented from the PlayStation's and shared by two callers. **New Game**
gets two new steps between `TitleFlow_NewGame` and the game: the name screen
for character record 0, then the options screen the port already has, on the
title path it already has. **The faerie village's renamer** gets its three
cut calls back. The grid, the labels, the legend and the screen's messages
come from a new chunk of the chosen language's layer, built by
`tools/loc_build.py` from that language's disc; a language with no such chunk
(the original Chinese: no disc has a grid for it) skips the name screen. On
by default where the chunk is present, with a switch. Names are 5 as on the
PlayStation. The title menu's third row stays as it is.

## 1. What is known

From [`name-entry-restoration.md`](name-entry-restoration.md) (2026-10-05,
capstone over the sibling's overlay captures and `BOF3.exe`):

- **Two copies of one routine** on the PlayStation: `COMMU02.EMI` (the
  renamer) and `START.EMI` (New Game), with a byte-identical 288-byte grid
  (three pages of 8 x 12 one-byte cells, special cells 1..5 in columns
  10..11 of rows 3..7). Measured on the Japan build's captures; the other
  builds are M2 below.
- **The renamer on the PC:** the six entry steps, the tiled frame
  (`Commu_DrawTiledFrame` `0x45F1A0`) and both commits (`0x45F5A0` member,
  8 bytes; `0x45F650` slot, 5 bytes) are ours. The grid's draw and the legend
  are calls to `BareRet` `0x437CC0` that still pass the PlayStation's
  arguments; the input step is `BareRetZero` `0x43C9F0`. The state cells are
  paired in the research's section 3. The port's entry-in writes `0x20` for a
  blank where the PlayStation writes `0xFF`.
- **DIV-0075** (`CommuNameEntryAbandons`) ends the renamer's entry unanswered,
  where Capcom's port never leaves the step.
- **New Game on the PC has no remnant of the name screen.**
  `TitleFlow_NewGame` `0x5880E0` calls `NewGame_InitCharacters`, starts
  scenario 0 and sets `Game_Step` 5.
- **The New Game copy** names record 0 only, cannot be cancelled into the
  game (special code 4 sets a mode word; where that leads is not traced, M1),
  and on an all-blank name puts record 0's default back.
- **The callees exist on the PC and are paired:** `Menu_DrawBox` `0x57CF60`,
  `Text_DrawAt` `0x516B30`, `Text_DrawSmall` `0x516E70`, `Menu_DrawHand`
  `0x5905D0`, `Menu_DrawPieces` `0x57D910`, `Input_AutoRepeat` `0x461EB0`.

Read today in our own source and `symbols.toml` (no game run):

- **The options screen already has a New Game path.** `TitleFlow_Step`
  `0x587DB0` steps are Begin, Menu, NewGame, Load, the Config screen's
  machine `0x460CB0` (step 4), EnterGame (step 5). At `Game_Step` 4 the
  Config screen's top bar leaves on a confirm or on bit `0x800` with no
  sound, and its closing state does `Transition_Start(0)` and sets
  `Game_Step` 5 (`src/game/core/title.cpp`, `ConfigScreen_TopBar` and the
  closing at original `0x461620`). `TitleFlow_EnterGame` `0x588800` then goes
  back to the menu when the title cursor is 2, and into the game otherwise.
  **Hypothesis:** step 4 into step 5 is the PlayStation's "options, then the
  game", and the cursor-2 test is what the port added to make it a title
  row. M1 checks it against `START.EMI`.
- **The title menu** has three rows on the PC (`TitleMenu_DrawRows`
  `0x588880`; DIV-0014 gives the third its word under a language) and a
  two-row layout of rows 0 and 2 on byte `0x6BDF8E`
  ([`USER_CHECKS.md`](USER_CHECKS.md) 6, still open). `TitleMenu_Leave`
  `0x5880A0` sets `Game_Step` to the cursor + 2.
- **The screen's messages are beside ones the port kept.** The PlayStation's
  New Game screen draws system message `0xBA` or `0xBB`
  (research 4.6); the PC's Config screen draws `0xBC` + the cursor. Whether
  the PC's pool and each layer's still hold `0xBA` / `0xBB` is M4.
- **Default names** under a language are a kind-10 chunk from the disc's
  `START.EMI` records (DIV-0020), at most five for a Latin layer; a `ja-JP`
  name of five is paired from its end until it fits the field (DIV-0020's
  amendment, DIV-0057's rule, `NameCopy` of DIV-0118).
- **Chunk kinds in use by `loc_build.py`:** 5 to 20. The grid's is the next
  free one (21 if still free at launch).

## 2. The owner's answers (2026-11-12, by chat), and what is still open

| # | Question | Answer |
|---|---|---|
| A1 | New Game only, or the renamer too? | **Both.** One shared routine. |
| A2 | The title menu's third row | **Keep it for now**, as a copy of the regular Config screen. The owner intends to make it a custom menu of quality-of-life toggles later, reachable from inside the game too (IDEAS I65). |
| A3 | Default | **On, under the chosen localisation's grid:** Japanese shows the Japan grid, English the English grid, Chinese skips the name screen. |
| A4 | Name length | **5, as the PlayStation.** |
| A5 | Who is named at New Game | Record 0 only, as the PlayStation (research section 8 item 2; proposed, not asked again). |

Still the owner's, none blocks stage A:

1. **Does the original Chinese pass through the options screen at New
   Game?** A3 says it skips the name screen. Proposed: the options step has
   its own switch and is on in every language, the Chinese too, since the
   screen exists there; the `original` preset turns both off. Say if the
   Chinese flow should stay the port's whole.
2. **What the legend's six button pictures show** on a keyboard and on each
   pad mapping. Proposed: the button round's rule
   ([`key-icons-by-mapping.md`](key-icons-by-mapping.md)), decided off a
   capture in stage B (group BT).
3. **Typing a name on the keyboard** beside the grid. Proposed: not in this
   round (section 8).
4. **The French, German and Spanish grids**, once M2 says what each disc
   holds: the disc's own, as A3 reads. Only a question if a disc's grid lacks
   letters its language's default names use.

## 3. Not measured, and first in the round (group M, read-only, no `src/`)

M writes its results into this file's section 3a and stops. Nothing in
stages A and B starts before the owner has read it, because M can change the
group list.

1. **The PlayStation's New Game sub-machine, whole.** The 33-pointer run at
   `START` `0x801ED078` and the sub-step table `0x801ED0A8`: every sub-step
   from the title's New Game to `0x801E6228` (the pair of
   `TitleFlow_EnterGame`), in order. Where the options screen is entered and
   left; what special code 4 does (mode word `0x80143B92`); the name panel
   `0x801E6308`, read to the last instruction; whether `START`'s input step
   differs from `COMMU02`'s beyond the research's 4.6 list. Confirm or refute
   the hypothesis of section 1 (PC step 4 into 5 is that path).
2. **The grid in every build the importer takes.** For each disc build
   (the seven of `exe_maps/`, PSP included): where the grid, the 0x1C-byte
   label block, the eight legend strings and their pointer order sit in
   `START.EMI` and in `COMMU02.EMI`; whether the two copies agree; the page
   count and the holes per page (shape counts only). A build with a
   different shape (a Latin build may have fewer pages) changes the input
   step's clamps, which are written for the Japan shape.
3. **The grid's bytes against the layer's name bytes.** For each language:
   is a grid cell's byte the byte the layer stores in a name and draws with
   `Text_DrawAt`? Compare by shape with the kind-10 default names (the same
   disc's records). For `ja-JP`: what the five one-byte cells become in the
   8-byte field (the pairing rule), and whether the reverse (a stored name
   back into the edit buffer at entry-in) is exact.
4. **The messages.** System messages `0xBA` and `0xBB`, and the renamer's
   `0xF4`, `0xF5`, `0xF7`: present in the PC's pools and in each layer's, or
   to be carried in the new chunk. Bit `0x40` of PSX `0x80143E6C`, which
   picks between the two: what it is, and its PC twin.
5. **Every draw of a character's name at five.** The names a Latin layer has
   today are at most five, so no new width is expected; list any site that
   draws record 0's name with a cap below five in a layer's letters.
6. **The pad bits.** Which physical button each bit of section 4.4 is, on the
   PlayStation and through the PC's mapping, measured (the research assumed
   the standard layout), and what the button round's roles make of them.
7. **The sounds.** `0x100` to `0x107` and `0x105`: all present on the PC
   (`Sound_PlayEffect`), in the title's bank and in the community's.
8. **What starts at New Game in the harness.** Every recipe, self-test and
   state-hash route that presses New Game (`tools/recipes/`, the self-tests,
   the attract cycle): with the round's default on they meet two new screens.
   List them; section 7's trap 1 says what each does about it.
9. **The fade and the actor.** The name screen fades in and out through PSX
   `0x8014EB68` and animates an actor through the scratchpad (research 4.6).
   Their PC twins, and which actor it is by its record, not by a name from
   memory.
10. **The Mednafen reference.** The sibling's savestate row 7 lands on the
    name screen (`../BreathOfFire3Recomp/docs/SAVESTATES.md`). One capture a
    page for the US and Japan builds, kept in the round's root (game-derived,
    never committed), for the layout comparison of stage C.

## 3a. What group M measured

Three read-only agents, 2026-11-12. No game or emulator run. Each part says
what it read; the orchestrating session spot-checked the citations marked
(checked).

### M1. The PlayStation's New Game path, whole

*Group M-A: capstone over the sibling's captures (`START.EMI` `b8cc1561`,
`COMMU02.EMI` `5434a7fa`, `GAME.EMI` `9d00fd19`), the boot executable and
`bof3/BOF3.exe`; scripts and listings in
`C:\Users\kerok\bof3x-runs\ng1112\m-a` (`psxd.py`, `pcd.py`,
`diff_input.py`, `bankscan.py`, `bankhash.py`). "Verified" is read in both
bodies. The PC side's cursor claims were checked against our source and
`tools/pe_xref.py 0x6bdf85`.*

**The two task tables (verified).** The PlayStation's title task
`0x80197F78` dispatches on `Game_Mode` `0x80143B90` (PC `0x66C7E8`) through
three modes: 0 loads the title's file, 1 is `0x801E3CEC` (= PC
`TitleFlow_Step`), **2 is `0x801E1520`, the options screen**. The PC's
`TitleTask_Modes` `0x667294` has two entries and no mode 2. `0x801E3CEC`
jumps through `0x801ED078` by `Game_Step` `0x80143B92` (PC `0x66C7EA`): the
research's "mode word" is `Game_Step`.

**The 33-pointer run at `0x801ED078`** is 5 top-level steps, 7 menu
sub-steps (`0x801ED08C`), 4 New Game sub-steps (`0x801ED0A8`) and 17 load
sub-steps (`0x801ED0B8`), all on the sub-step byte `0x801ED7E0` (PC
`SaveMenu_FlowState` `0x6BDF86`). The PC keeps the 7 menu sub-steps and an
8-entry load table, and has no New Game sub-table.

| Step | PSX extent | What it does | PC |
|---|---|---|---|
| 0 | `0x801E3D28..0x801E3E87` | Begin: the actor (M9), the cursor, the row levels, `Game_Step` + 1 | `TitleFlow_Begin` `0x587DC0`, the same stores; the cursor and `0x6BDF8E` from `Save_ListFiles` |
| 1 | `0x801E3E88..0x801E3EC3` | the menu, sub-steps m0..m6 | `TitleFlow_Menu` `0x587EA0` |
| m2 | `0x801E3F80..0x801E40EB` | the card's status sets `0x801ED7EC` ("a load is offered") and the cursor | `TitleMenu_FrameUp` |
| m5 | `0x801E4280..0x801E442F` | choose: `0x5000` flips the cursor, only when a load is offered; confirm or `0x800`: `Transition_Start(0)`, sound `0x105` | `TitleMenu_Choose` `0x587FA0` |
| m6 | `0x801E4430..0x801E448F` | wait for the fade; `Game_Step` 2 if the cursor is not 0, else 3 | `TitleMenu_Leave` `0x5880A0`: cursor + 2 |
| 2 | `0x801E4490..0x801E44D3` | New Game: sub-steps n0..n3, then `Field_DrawFrame` after each | `TitleFlow_NewGame` `0x5880E0`, no sub-steps |
| n0 | `0x801E44D4..0x801E454F` | open: `NewGame_InitCharacters`; name cursor, row and column 0 (**not** the page, **not** the buffer); `Draw_PassFlags` 1; the draw-all; `Transition_Start(1)` | - |
| n1 | `0x801E4550..0x801E459B` | the draw-all until the fade ends | - |
| n2 | `0x801E459C..0x801E4E0B` | the input step | - |
| n3 | `0x801E4E0C..0x801E4E53` | the draw-all while the fade runs; then sub-step 0, `Game_Step` 4 | - |
| 3 | `0x801E4E54..0x801E4E8F` | load, 17 sub-steps; ends at `Game_Step` 4, or back to 1 | `TitleFlow_Load` `0x588130`, 8 sub-steps |
| 4 | `0x801E6228..0x801E6307` | EnterGame: growth rolled for 8 characters; **cursor not 0: `Game_Mode` + 1** (to the options); cursor 0: the field task | `TitleFlow_EnterGame` `0x588800`: **cursor 2: `Game_Step` 1**; else the growth loop and the field task |
| mode 2 | `0x801E1520` | the options at New Game | `ConfigScreen_Run` `0x460CB0`, as `Game_Step` 4 |

(On the PlayStation the cursor is 1 on New Game and 0 on the load row; the
research's `0x801E4430` is the menu's last sub-step, not a name-screen
step.)

**The options screen at New Game (verified).** The PlayStation's mode-2
machine and the PC's `ConfigScreen_States` `0x6536C0` have the same five
states and three open sub-steps, and **every PlayStation test of "mode is 2"
is a PC test of "`Game_Step` is 4", branch for branch**: the top bar's silent
confirm (PSX `0x801E191C`, PC `0x460F3F`), its `0x800`-only leave with the
cancel word not tested (PSX `0x801E1A28`, PC `0x461016`), and the closing
(PSX `0x801E22E4`, PC `0x461620`). They part after the closing's fade:

- The PlayStation counts 30 frames, then clears a byte, sets the area number
  `0xFFFF`, calls `Scenario_Start(0)`, clears 20 words and starts the field
  task.
- The PC sets `Game_Step` 5. That tail (the byte `0x9039F2`, the area
  number, `Scenario_Start(0)`, the 20 `Cond_Flags` words) is in the PC's
  `TitleFlow_NewGame`.

**Section 1's hypothesis: confirmed, with two corrections.** (a) On the
PlayStation the options come *after* EnterGame's growth roll, and the
options' own closing starts the scenario; the port moved that tail into
`TitleFlow_NewGame` and pointed the options' end at EnterGame. (b) The
cursor-2 test is the port's retarget of EnterGame's other branch, which on
the PlayStation is "New Game: on to the options".

**The PC's test already tells New Game's options from the third row.**
`SaveMenu_Cursor` `0x6BDF85` is 0 at New Game and 2 on the third row; its
writers are `TitleFlow_Begin` and `TitleMenu_Choose` only (12 references,
none in the Config screen; checked). New Game, the name screen, the Config
screen at step 4, step 5: the game starts, with no new test, as long as no
new step writes `0x6BDF85`.

**Special code 4 in START (verified).** Sound `0x103`, sub-step 0,
`Game_Step` 1, no fade: back to the title menu from its first sub-step. The
buffer and the page are left as they were; nothing in START resets them, so
they hold until the overlay is loaded again. `NewGame_InitCharacters` runs
again at the next open.

**The name panel `0x801E6308..0x801E6417`** (x, y), called with (`0x20`,
`0x2B`): `Menu_DrawBox(x+3, y+3, 0x7D, 0x2F, 0, window colour)`; a `0x28` x
`0x30` portrait SPRT, entry 0 of a 4-byte image table (the shape of PC
`CommuMember_DrawPortrait` `0x45EEB0`); `Text_DrawAt(x+0x3B, y+0x19, colour,
5, buffer)`, the colour always 0 in effect; the underline at (x + `0x3B` +
12 * name cursor, y + `0x25`, `0xC`) through PC `Menu_DrawGreyHLine`
`0x57CEF0`; `Menu_DrawPieces(x, y, list, 0)`. It is the PC's
`CommuMember_DrawPanel` `0x45EE10` with a box height of `0x2F` (the PC's is
`0x2D`), portrait entry 0 and the underline.

**The draw-all `0x801E6F80`** is as research 4.6 has it, with one call the
research left out, first: the backdrop (= PC `Menu_DrawBackdrop` `0x575690`
with `Config_Background`). The title box is PC `Menu_DrawTitleBox`
`0x574AB0` at y `0x12`. The word that picks message `0xBA` or `0xBB` is the
main loop's frame counter: the two alternate every 64 frames. Its PC twin
`Frame_Counter` `0x937F94` is a hypothesis.

**START's input step against COMMU02's** (both read whole, 351 operations
against 378). The moves, the page-2 clamps, codes 1, 2, 3 and 5, triangle,
square / R, L and the backspace's cursor rule are identical. The
differences, all of them:

1. START draws first (research 4.6).
2. **Code 4.** COMMU02: sub-step + 1, returns 0, no sound. START: sound
   `0x103`, sub-step 0, `Game_Step` 1.
3. START writes 0 to buffer byte 5 on every glyph entry and every backspace.
4. **The finish.** Sound `0x104` against `0x105`. COMMU02 turns the trailing
   `0xFF`s into 0, **an all-blank name included** (five 0 bytes; its caller
   tests byte 0), which corrects research 4.4. START on an all-blank name
   copies record 0's five name bytes into the buffer, else the same rule;
   then writes record 0, `Transition_Start(0)`, sub-step + 1.
5. COMMU02 returns the answer; START's step returns nothing.

**The title rows (verified).** The PlayStation draws two images when a load
is offered and one, New Game alone, when not. PC `0x6BDF8E`
(`SaveMenu_AnySave`) has one writer, `Save_ListFiles() != 0` in
`TitleFlow_Begin`, and is the twin of the PlayStation's "a load is offered"
byte. The PC's two-row layout is the PlayStation's one-row layout plus the
third row.

### M2. The grid, the labels and the legend in every build

*Group M-B: capstone MIPS over the seven held discs
(`D:\bof3x-runs\discs`, through `loc_build.open_disc`), `tools/pe_xref.py`,
our source, and `tools/dat.py` over the built layers in `game\cache\loc\`
(read only). Scripts and outputs in `C:\Users\kerok\bof3x-runs\ng1112\m-b`
(`grid.py`, `page3.py`, `west2.py`, `funcs.py`, `labels.py`, `legend.py`,
`psp.py`, `hashes.py`, `m3.py`, `m3b.py`, `m4.py`, `flagscan.py`,
`draw_sites.txt`, and the read-off listings). The hashes below are in
`hashes.out` (checked).*

The seven builds are `psx-jp`, `psx-us`, `psx-eu-en`, `psx-fr`, `psx-de`,
`psp-jp` and `psp-eu`. No build is Spanish; no disc holds a Chinese grid.

**The research's shape is the Japan build's only.** Every build carries a
three-page, 288-byte block, equal in `START`, `COMMU02` and `COMMU02B` in
every build. The Western builds use **one page, page 2**, of another shape:
their open step and both entry-ins write the constant 2 to the page cell and
nothing else writes it.

| Build | `START.EMI` grid (VA) | `COMMU02.EMI` grid (VA) | Pages in use | md5 of the 288 bytes |
|---|---|---|---|---|
| psx-jp, psp-jp | `0x801ECD2C` | `0x801DACA4` | 0, 1, 2 | `81ef2ad0...` |
| psx-us, psx-eu-en | `0x801ECDBC` | `0x801DAAAC` | 2 | `822152b5...` |
| psx-fr | `0x801ECC1C` | `0x801DAA08` | 2 | `40b6c696...` |
| psx-de | `0x801ECC74` | `0x801DA9C8` | 2 | `5f9b0163...` |
| psp-eu | its EMIs are the Japan build's; `BOOT.BIN` holds the PSX US grid (image `0x2EC7A8`, `0x2EF1C9`) | | not read (its code is Allegrex) | `822152b5...` |

| Page | Builds | Specials (columns 10..11) | Glyph cells | Empty cells |
|---|---|---|---|---|
| 0, 1 | JP | codes 1..5, rows 3..7 | 86 each | none |
| 2 | JP | codes 1..5, rows 3..7 | 42 | 44 |
| 2 | US, EU, PSP-EU | **codes 1..4, rows 3..6** | 68 | 20: row 2 cols 6..9; row 5 cols 6..9; row 7 whole |
| 2 | FR | codes 1..4, rows 3..6 | 82 | 6: row 2 col 9; row 5 cols 7..9; row 7 cols 10..11 |
| 2 | DE | codes 1..4, rows 3..6 | 75 | 13: row 2 col 9; row 7 whole |

The Western pages 0 and 1 have the Japan pattern and nothing reads them: **a
shape search for the Japan pattern alone finds the wrong pages in the West.**

**The Western input step is another routine** (US `START`
`0x801E4744..0x801E4E8C` read to the last instruction; the four Western
copies share one normalised code hash, `funcs.py`):

| | Japan | West |
|---|---|---|
| Page change, clamps | Triangle and code 3; page-2 clamps | **none** |
| Confirm bit | `0x20` | **`0x40`** |
| Code 3 | page | **quit** |
| Code 4 | quit | **finish** |
| Code 5 | finish | does not exist |
| Name cursor + 1 | `0x80`, or repeated `0xA` | `0x20`, or repeated `0xA` |
| Name cursor - 1 | repeated `0x5` | `0x80`, or repeated `0x5` |
| Backspace | `0x40` | `0x10` |
| Left / Right from a special | column 9 / 0 | the same, **with no skip over an empty cell** |

**A reachable empty cell in the West (from the code, not run).** Left from
the row-5 special sets column 9, which is empty on the US / EU and French
pages; a confirm there writes byte 0 into the name. Hypothesis: a defect of
Capcom's Western releases. The PlayStation itself decides (M10's capture
session can try it).

**The labels.** Japan: `0x1C` bytes, 7 slots of 4 (code 3's label by page).
US / EU: `0x14` bytes, 4 slots of 5. French, German: `0x1C` bytes, 4 slots of
7. `START`'s and `COMMU02`'s blocks differ in every build, in the finish
code's slot (and the quit code's in German).

**The legend.** An 8-pointer table in every build; which entries are drawn
differs (Japan: 0..3, 4 + page, 7 as leaders; US / EU: 0..3 or 0..4, 7 as
leaders, and in `START` four more 8 x 8 tiles; French, German: 0..4, no
leaders). The box is `0x78` wide in `START`, `0x4D` in `COMMU02`. The six
button tiles are the same in every build, in another order. **The PSP
builds' legends were not found** in either `BOOT.BIN`.

### M3. Grid bytes against the layer's bytes

| Build | Distinct cells in use | Same byte in the layer | Another single byte | Two-byte code | No glyph | Default-name bytes off the grid |
|---|---|---|---|---|---|---|
| US, EU, PSP-EU | 68 | 62 | 5 | 1 | 0 | 0 |
| FR | 82 | 62 | 5 | 15 | 0 | 0 |
| DE | 75 | 62 | 5 | 8 | 0 | 0 |
| JP | 204 | 0 | 1 | 203 | 0 | 0 |

- **A cell's byte is the layer's byte for letters and digits only.** The
  draw and the commit go through the layer's encoding (`loc_build`'s
  `encode_char` / `ja_encode`). The encoding is injective over every grid, so
  a stored name decodes back to its cells exactly.
- **`0xFF` is a glyph cell in every build (the space)** as well as the
  routine's blank marker: a typed space is a blank, and only trailing ones
  are cut.
- **Field room, Latin:** a five-cell name is 5 + k bytes for k two-byte
  cells, and the record's field is 8 and a NUL: k <= 3. A French or German
  name with four or five two-byte cells does not fit. The faerie slot's 5
  bytes hold a five-cell name only when every cell is a single byte.
- **`ja-JP`:** five cells are 10 bytes. They fit 8 only through one pair code
  for the last two cells (DIV-0057), and pair codes come from a table fixed
  when the layer is built: 227 pairs in the owner's layer, 525 glyph slots
  left, 41,209 possible last pairs. **So most five-cell names cannot be
  stored as the layer is built today.** A four-cell name is 8 bytes with no
  pair. A faerie slot holds at most two cells without pairs; a five-cell name
  cannot be stored there at all.
- The reverse (stored name to cells) is exact given the same pair table.

### M4. The messages

`0xBA`, `0xBB` (system pool, `FIRST.DAT`) and `0xF4`, `0xF5`, `0xF7` (areas
175..185) are present and non-empty in the PC's pools and in every built
layer (`en-US`, `en-150`, `fr-FR`, `de-DE`, `ja-JP`), each the layer's own.
**Nothing needs carrying in the chunk.** The word that picks `0xBA` or
`0xBB` is the frame counter; its PC twin is `Frame_Counter` `0x937F94`,
paired through `Menu_DrawMemberStatus` `0x573560`, which tests bit `0x20` of
it where its PlayStation twin tests bit `0x20` of `0x80143E6C` (this settles
M-A's hypothesis). The save block's delta does not apply to that word.

### M5. Every draw of a character's name

Eleven fixed-cap draws (the status, equip, reserve, party, shared-list,
renamer, save-slot and two battle windows; the list with file and line is in
`draw_sites.txt`) and seven fixed-length copies. **No cap is below five
characters.** What cuts is bytes:

- The renamer's entry-ins copy the stored name's first 5 bytes into the edit
  buffer (`src/game/community/commu_name.cpp:700-708`, checked; `:961-967`),
  and the slot commit writes 5.
- Both commits copy 8 bytes to the text insert record with no NUL after
  them.
- `BattleWin_DrawPartyStatus` `0x442FA0` and `BattleWin_DrawTargetMember`
  `0x443F60` draw through `Text_DrawSmall`, which does not expand pair codes:
  a `ja-JP` name with a pair would show the pair's placeholder there. From
  the source, not run; it would already apply to the one default name that
  has a pair.

### M6. The pad bits

**The PlayStation game does not remap the pad (verified):** held, previous
and pressed words straight from the Psy-Q `PadRead`; the PC's `Input_Latch`
`0x4FC6A0` builds its three words by the same formula. `Input_AutoRepeat`
returns the held word after a 12-frame wait and then every 3 frames; the PC
returns a dword whose low half is the held word.

| Bit | Test | Psy-Q layout | PC keys, pad | The button round's role (Japan) |
|---|---|---|---|---|
| `0x1000` `0x4000` `0x8000` `0x2000` | repeated, mask `0xF00F`: the hand | Up Down Left Right | arrows; d-pad | d-pad, passed through |
| `0x20` | pressed: the cell | Circle | V, Space; east | confirm |
| `0x10` | pressed: next page | Triangle | Z; north | Menu |
| `0x80` | pressed: name cursor + 1 | Square | C; west | Move |
| `0xA` | repeated: name cursor + 1 | R1, R2 | S, W; RB, RT | View (R1) |
| `0x5` | repeated: name cursor - 1 | L1, L2 | A, Q; LB, LT | Change (L1) |
| `0x40` | pressed: backspace | Cross | X, Esc; south | cancel |
| `0x800` | pressed: finish | Start | Enter; start | none |

- **Every face test is a constant**; none reads the Config words. Under
  DIV-0139's rule each face and shoulder test goes through `Sigma`, as
  [`button-tests.md`](button-tests.md) 2.3 deferred to this round.
- The physical identity of each bit rests on the Psy-Q layout; only Down and
  the confirm bit are corroborated by the sibling's live Mednafen notes.
- The Western discs' copies drop one triangle test and change the face tests
  ([`button-roles-by-release.md`](button-roles-by-release.md) 3); not read
  by M-A.

### M7. The sounds

The routine plays `0x100` (moves), `0x101` (the name cursor), `0x103` (a
glyph, codes 1 and 2, the page, START's code 4), `0x104` / `0x105` (the
finish, COMMU02 / START), `0x106` (backspace), `0x107` (codes 1 and 2 at the
limit). All are bank 1, cues 0..7. **61 PC data files carry the same bank-1
payload** (hashed whole), among them `START`, `FIRST` and every `COMMU*`
file, with cues 0..7 present. `0x107` is the one id the PC title does not
play already. From the shipped PC data; not listened to, and not checked on
a disc-built or PSP-only install.

### M9. The twins, and the actor

| PSX | PC | How |
|---|---|---|
| `0x8014EB68` | `Transition_Start` `0x495040` | `symbols.toml`; by position in the title's confirm |
| `0x80143C40` | `Transition_Busy` `0x66C810` | `symbols.toml` |
| `0x801D15F4` | `NewGame_InitCharacters` `0x437820` | read whole |
| `0x8014B948` | `Field_DrawFrame` `0x592F00` | `symbols.toml` |
| `0x8014D5AC` | `Sprite_SetAnimation` `0x5891F0` | `symbols.toml` |
| `0x8014D3D4` | `Sprite_QueueOverlay` `0x5890E0` | `symbols.toml` |
| `0x8014D86C` | `Sprite_ScriptTick` `0x5893A0` | read |
| `0x8014832A` | `Draw_PassFlags` `0x7E0918` | by position in `GameMode_Start` |
| `0x1F800044` | `Sprite_Current` `0x937F88` | both Begins store the actor there |
| `0x8015E908` | `Sound_PlayEffect` `0x587740` | by position |
| `0x8014B848` | `Task_Restart` `0x5A9976` | both EnterGames |

**The actor is `ObjTrio` record 0** (PC `0x802D40`), set up by Begin as
party member 0's field sprite in slot 0 (`Field_MemberSprite(0, 0)`), with
the same stores on both sides. Every 10 frames the draw-all steps its facing
through 0..7 and sets animation 8..15; every frame it queues and ticks it;
step 2 draws it with `Field_DrawFrame`. **On the PC `TitleFlow_Begin` still
sets it up and nothing draws it**: a remnant of the cut screen. That member
0 is character record 0 is a hypothesis.

### M8. What starts at New Game in the harness

*Group M-C: source, recipes and tools read; scripts in
`C:\Users\kerok\bof3x-runs\ng1112\m-c` (`recipe_starts.py`, its output
`recipe_starts.txt`).*

**How the title chooses a row** (our source):

- `TitleFlow_Begin` puts "any save listed" into both the three-row byte
  `0x6BDF8E` and the title cursor `0x6BDF85`: the cursor starts on LOAD GAME
  (1) when slot files exist, on NEW GAME (0) when none do
  (`src/game/core/sound.cpp:814-819`, checked). So `0x6BDF8E` is "a save
  exists", which answers [`USER_CHECKS.md`](USER_CHECKS.md) 6's question from
  the source.
- `TitleMenu_Choose`: with saves, up and down move over 0..2 and wrap;
  without, either toggles 0 and 2 (`sound.cpp:896-909`). `TitleMenu_Leave`
  sets `Game_Step` to the cursor + 2; `TitleFlow_EnterGame` returns to the
  menu when the cursor is 2, else starts the game, after a load too
  (`sound.cpp:1205-1221`).
- **Every route that confirms on the title with no slot file present starts a
  New Game**, whatever it meant to do. The `# save` swap
  (`tools/input_run.py:216-223`) is what puts the cursor on LOAD GAME.
- The attract demo never runs the title flow.

**Which language a run gets:** `--lang`, else the launcher's `bof3x.ini`,
else the original Chinese. The gates use ini-less launcher copies
(`tools/abc_run.py:282-284` refuses one with an ini, checked); `abc_run`
clears the caller's `BOF3X_*` and passes each install's own.

| Route or test | How it starts | Affected | Remedy |
|---|---|---|---|
| `battle_commands.txt` | **New Game**: `press up` from LOAD GAME, confirm (lines 9-13, checked); no `# save` header | **Yes**, where the layer has the chunk | `seek up 0x6BDF85 u8 == 0 max 3` in place of `press up`; the switches off in a gate |
| `config_screen.txt`, `config_controller.txt` | The title's third row; leave through EnterGame's cursor-2 branch | Only if TF changes that branch or the Config screen's closing | None; they are the check that the third row is unchanged |
| 104 recipes with a `# save` header | Load slot 0 | No | - |
| 12 recorded routes with no header (`bridgeWalk`, `deisCave`, `fairyVillage`, `gaistFullRoute`, `garrAbility`, `identify`, `liftScene`, `maekyssGorge`, `ninaWalkBehindBlock`, `overheatAndDeepSea`, `tarmanLogic`, `vaccinationAndFireScene`) | Load, given `--save NAME` | No when given `--save`; without it and with no slot file they press New Game, wrong today already | Add the `# save NAME` header |
| `load_list.txt`, `title_timeline.txt`, `attract_cycle.txt`, the six `quick_*_cross.txt` | The load list, no input, or a quick-load at frame 0 | No | - |
| The ten state-hash routes (`caughFish`, `combat`, `cutsceneAndNue`, `dragonTransform`, `field_menu`, `masterAndManillo`, `menu_screens`, `shop`, `whelpBoss`, `worldMapAndAreaTransition`) | All load their save, under the original Chinese | No | None; no new pairs are needed for this round |
| The self-tests | The game never starts | **Through the shadow fuzz**, which compares ours with Capcom's for `TitleFlow_Step`, `TitleMenu_Leave`, `TitleFlow_NewGame`, `TitleFlow_EnterGame` (`src/game/core/save_menu_fuzz.cpp:547-568`) and the Config screen's `0x460CB0`, `0x461620` (`src/game/effect/rest_4f_fuzz.cpp:162,171`) | TF leaves those bodies Capcom's with the switches off, and the fuzz pins the switches off |
| Spell sweep, boss sweep, every-door sweep | Saves, or crafted saves | No | - |
| CI | Never runs the game | Through the ledger checks only (`ledger_switches.py --check`, `ledger_registry.py --check`) | Stage C |
| Launcher tests | No game flow | `presets_en-US.tsv` / `presets_original.tsv` are generated from the ledger | Regenerate at stage C |

**Where a harness-wide "these three switches off" goes.**

- An environment `BOF3X_OFF` **replaces** the ini's `off=` and the preset's
  names whole; it does not merge (`src/launcher/config.cpp:561-565`, `:1625`).
- `BOF3X_OFF` with a name that is not a code name is a Fatal at start
  (`src/hook/detour.cpp:467-469`, checked); a known name that is not active in
  the start is only logged. **So the line cannot land before the three names
  are in `src/hook/code_names.h`.**
- Every runner launches through `tools/input_run.py`. **Proposed place:** its
  `main()`, after the `--env` loop (`input_run.py:210-212`, checked): merge
  the three names into `env['BOF3X_OFF']`, keeping any a caller passed,
  unless a new flag asks for the screens. Cost: a hand run under the play ini
  then loses the ini's own `off=` (empty today); the flag can read and merge
  it.

### M8b. How a recipe sees the title flow

- `until`, `peek`, `seek` and `watch` take any mapped address
  ([`input-script.md`](input-script.md)). The words: `Game_Mode` `0x66C7E8`
  u16, `Game_Step` `0x66C7EA` u16, the title menu's state `0x6BDF86` u8 and
  cursor `0x6BDF85` s8. No recipe waits on `Game_Step` today.
- **For the name screen to be waitable, it wants a `Game_Step` value of its
  own**, or a word in Capcom's span. A static of ours is not a stable recipe
  address.
- `tools/recipe_shots.py` takes recorded recipes only (`wait`, `hold`,
  one-frame `shot`, zero-frame `key`, `quit`; `tools/recipe_shots.py:31-44`,
  checked). Shots on the new screens come from an owner's recording put
  through it, or from `shot` lines in a hand-written route, not both in one
  file.

### Quicksave

- **Nothing refuses a quicksave by game state today.** A take on the title
  menu or the title's Config screen is taken and a load there is restored;
  the title's state is all in Capcom's `.data` / `.bss`, which the snapshot
  copies. From the source, not from a run: no title take was ever measured.
- **What a title step with statics of ours needs:**
  1. Its cells are outside the span: carry them in a snapshot participant
     (`Snapshot_Register`, `src/hook/snapshot.h:95-96`), or refuse F6 while
     the step is active and say so on the overlay.
  2. `tools/quicksave_census.py check` fails on a new mutable static with no
     rule: `name_grid.cpp`'s cells need rows.
  3. **The switch is not in the build id.** A take on the name step, loaded
     with the switch off, restores a `Game_Step` the dispatcher does not
     have. Through Capcom's table, entry 6 of `0x6671F4` is the first word of
     the title menu's state table: no crash, a wrong state. The participant's
     check refuses a record whose switch state differs from the live one.
  4. The cells are reset at the step's entry, so nothing reads stale ones.
- PSX `0x80143B92`, the word special code 4 writes, is `Game_Step`'s twin
  (`symbols.toml:4725-4728`).

### M10. The Mednafen reference

- **Section 3's item 10 was wrong about what exists.** The sibling's
  savestate row 7 is the recompiled runtime's `.pst`, Japan only, and it is
  stale (`tools/scene.py preflight`: a codegen hash of another version).
  Mednafen cannot read it. No Mednafen state of either build is on the name
  screen (the Japan states read at `Game_Mode` / `Game_Step` are in play or
  on the logo).
- **One capture exists:** the Japan build's first grid page,
  `../BreathOfFire3Recomp/mednafen/snaps/Breath of Fire III (Japan)-0013.png`
  to `-0016.png` (2026-09-05). No second or third page, nothing of the US
  build.
- **The reference is a cold boot in Mednafen** for each build, driven by the
  sibling's `tools/mednafen_ctl.py`, which types into the foreground window:
  the machine hands-off for the whole capture. The discs, the BIOSes and a
  card with saves for each build are on disk. The command list, with output
  under `C:\Users\kerok\bof3x-runs\ng1112\ref\<jp|us>`, is in
  `C:\Users\kerok\bof3x-runs\ng1112\m-c\REPORT.md` (written from the
  sibling's docs, not run). Two cautions in it: the US launch takes no
  `--card` (it would copy the Japan card over the US one), and each title
  capture is read before the confirm, since a cursor on LOAD GAME opens the
  load list.

### What M-A's results change

- **TF needs no new test in `TitleFlow_EnterGame`,** and must not write
  `0x6BDF85`.
- **The name screen is sub-steps of step 2, as on the PlayStation,** on
  `SaveMenu_FlowState` `0x6BDF86`: open (its own `InitCharacters`), fade in,
  input, fade out; then the rest of `TitleFlow_NewGame` without a second
  `InitCharacters` (it would overwrite the entered name), then `Game_Step`
  4, then 5. The step's identity is then in Capcom's span, waitable by a
  recipe (`0x66C7EA` == 2 and `0x6BDF86`), and M-C's "a `Game_Step` value of
  its own" is not needed. The quicksave switch check still is: with the
  switch off, step 2 at a sub-step other than 0 runs the port's
  `TitleFlow_NewGame` whole.
- **Code 4 goes back to the title menu** (sound `0x103`, `Game_Step` 1,
  sub-step 0, no fade), before `Scenario_Start(0)` has run.
- **The order differs from the PlayStation's in one place, by the port's
  layout:** growth is rolled after the options here and before them there,
  and the PlayStation waits 30 frames after the options' fade. Whether
  either matters (the random number sequence, the first frame of the
  opening) is TF's to measure; a call for the owner only if it shows.
- **The screen's draw:** `Field_DrawFrame` after each sub-step,
  `Draw_PassFlags` 1 at the open, the draw-all starting with the backdrop
  and queueing and ticking `Sprite_Current`, which `TitleFlow_Begin` has set
  up already. The name panel is new code of section M1's shape.
- **NG takes both copies' differences as parameters:** the code-4 result,
  the finish sound, the all-blank rule (five 0 bytes for the renamer, the
  default name for New Game), buffer byte 5 cleared. The New Game buffer and
  page are reset where the PlayStation reloads START (the title task's
  start).
- **NG and BT:** every face and shoulder test through `Sigma`; the low half
  of `Input_AutoRepeat`'s result.
- **No sound-bank work.**
- **Corrections owed elsewhere** (stage C): the research's 4.4 (the
  all-blank finish) and 4.6 (`0x801E4430`, the "mode word", the open's
  resets); `symbols.toml`'s `PartyWorkingRecords` pair is `0x10` off one of
  the two Begins' stores (`0x802D40` against `0x802D50`), to be read before
  anyone changes it (rule 3).
- **Not established by M-A:** the buttons beyond the Psy-Q layout; the
  Western builds' input step; whether the PC's portrait table entry 0 is the
  PlayStation's picture; member 0 = record 0; `Frame_Counter` as the
  alternation's twin; the sounds by ear.

### What M-B's results change

- **The chunk (GD) carries, per build:** the pages in use; each page's 96
  cells as the disc's codes with each cell's encoding in the layer; the
  special-code map; the label block with its slot layout, **one per copy**;
  the legend's strings per copy and which entries are drawn; the box width;
  the button-tile order. One grid (the copies are equal). No message.
- **GD locates the grid by the drawer's reference and the open step's page
  constant,** or by both patterns, never by the Japan pattern alone.
- **`psp-eu`:** the grid and labels are in `BOOT.BIN`; the legend was not
  found and the input step not read. GD reads them or refuses the build, and
  a refused build skips the name screen as the Chinese does.
- **NG is one body over a parameter set:** the code map, the pages, the bits
  for confirm, backspace and the cursor moves, the finish sound, and the
  clamp written as "up to the first cell that is not empty" (equal to the
  Japan constants on its page 2).
- **The contract of section 5 is wrong where it says a Latin layer's bytes go
  in as they are.** The commit encodes each cell; an entry that would pass
  the field's bytes is refused at the keypress (sound `0x107`, as the limit
  codes play).
- **CR:** the entry-ins decode the stored name to cells; the slot path holds
  5 bytes; the commits write the NUL.
- **The two battle windows** want the pair expansion the enemy windows have,
  for `ja-JP`.
- **M6's table is the Japan build's.** The Western bits are M2's table.
- **Not established by M-B:** the PSP builds' own code; the Western
  `COMMU02` input step instruction by instruction (its hash equals across
  the four); whether the empty cell is reached in play; which disc the
  owner's `en-150` layer was built from.

### What M-C's results change

- **RT moves to stage B**, after TF and CR have put the three names in
  `code_names.h`; its stage-A "done when" could not be met. What it can do
  in stage A: `battle_commands.txt`'s `seek`, and the twelve `# save`
  headers.
- **TF:** a `Game_Step` value of its own for the name screen (and for New
  Game's options step, so a load never enters it), dispatched by our
  `TitleFlow_Step`, not past the end of Capcom's table; the fuzz rows pinned
  with the switches off; the quicksave rule decided (participant or refusal)
  with the switch check.
- **NG:** census rows for its statics; a fuzz that does not depend on
  whether a layer is loaded.
- **Stage C:** regenerate the launcher's two preset TSVs, `code_names.h` and
  the registry. No new state-hash pairs for the ten routes.
- **Not established by M-C:** whether `ledger_switches.py --check` lets a
  code name be committed before its ledger entry (stage B before C); the US
  build's addresses for `Game_Mode` / `Game_Step`; a title quicksave in a
  run.

## 3b. After group M: the plan as amended, and the calls it raises

Sections 4 to 9 below are as staged. Where 3a differs, 3a and this section
hold.

**The plan's changes:**

- Stage A is **GD** and **NG**, with RT's two small edits
  (`battle_commands.txt`'s `seek`, the twelve `# save` headers). **RT's
  switch-off line and new routes move to stage B.**
- NG is one body with a parameter set per build family (Japan, West), fed
  by the chunk.
- TF builds the name screen as sub-steps of step 2 and reuses the Config
  screen's step-4 path untouched.
- A reference capture session in Mednafen (M10) is owed before stage C, on
  a hands-off machine; it also tries the West's empty cell.

**The calls group M raised, and the owner's answers (2026-11-12, by chat,
with a side-by-side picture of the Japan and US name screens running in the
sibling's recompiled builds; the picture is game-derived and is not in the
repository):**

| # | Call | Answer |
|---|---|---|
| 5 | `ja-JP` names of five: the PC's 8-byte field holds five kana only through a pair code the layer may not have | **"For now", up to 4 kana** for the character's name. Five is later work on the encoding. |
| 6 | The faerie slot names under `ja-JP` (5 bytes hold two kana) | **As proposed:** the member path gets the grid with call 5's cap; the slot path keeps DIV-0075's end under `ja-JP`, said in the ledger. |
| 7 | The West's reachable empty cell (M2, read from the code, not run) | **The owner: it is not possible to move back onto an empty cell on the real screen.** So ours never lands on one (the skip the Japan build has), and M2's reading of the Western step is refuted by observation at that point: NG reads the Western step's Left / Right from a special again before it writes that branch, and says what the first reading missed. No ledger line for a defect that is not there. |
| 8 | `psp-eu` installs (the legend not found, the code not read) | Not answered. Proposed as before: the name screen is skipped on that install for now, ledgered as a gap. Does not block stage A. |

**What the owner's picture adds** (the owner's observation, for NG, BT and
the stage C comparison; nothing here is decoded from a disc):

- The Western screen draws its grid in the narrow 8 x 12 letters, so the
  grid does not fill its box as the Japan one does. The box, the frame and
  the cell pitch look the same on both.
- Both screens show the title box, the name panel with a portrait and the
  name, the legend with the button pictures and a small animated figure at
  its right, and the grid box with the special cells' labels in two colours.
- The Western legend's rows follow the Western bits of M2 (confirm, delete,
  next, back, and the finish on Start), in another order from Japan's.
- The Western page as seen agrees with M2's shape: seven rows in use, the
  eighth empty, four labels in rows 3 to 6.

The Mednafen capture session of M10 is still wanted for the pixel
comparison of stage C, but it no longer has to settle call 7.

Call 4 of section 2 is answered by the discs: the French and German grids
carry their own extra cells, and every default name's bytes are on its
build's grid. What is left of it is the byte limit (a name with four or
five two-byte cells), handled by the refusal above.

## 4. The groups

Worker agents on Opus, each in its own worktree reset onto `new-game/round`,
one group merged at a time, every commit signed off (rule 7). Files are
disjoint inside a stage.

### Stage A (after M is read; three groups in parallel)

| Group | Files | What | Done when |
|---|---|---|---|
| **GD**, the grid's data | `tools/loc_build.py`, `tools/importer.py`, their checks | A new chunk kind in the language's `FIRST.DAT` layer: the grid's pages, the label block, the legend strings in pointer order, the screen's messages if M4 says they are not in the pools. Built from the language's own disc, located by shape (the special-cell pattern), checked against the second copy where M2 found them equal. Refused loudly on a disc where the shape is not found. | `loc_build.py all` writes the chunk for every held language; `importer.py check` holds it on a synthetic layer; a layer with no chunk builds as today |
| **NG**, the routine | new `src/game/menu/name_grid.{h,cpp}`, its fuzz file | `NameGrid_Draw`, `NameGrid_DrawLegend`, `NameGrid_Input` from the research's 4.1, 4.2 and 4.4, over a caller's cell block (buffer, name cursor, column, row, page, answer), so both callers share one body. The chunk's reader; `NameGrid_Available()` true only with a chunk whose shape passes. No stub (rule 4): a call without a chunk aborts. | Builds at both widths; the fuzz file drives every branch of the input step against a synthetic grid (the page clamps, the skips over empty cells, the five special codes, backspace, an all-blank finish) |
| **RT**, the routes | `tools/recipes/`, `tools/` | The switch-off prefix for every route M8 listed, a route that reaches the title's New Game, and the renamer's route (the faerie village save, `fairyVillage.txt`'s). Recording the entry itself waits for stage B. | The existing gates pass unchanged with the round's switches off |

### Stage B (after NG merges; three groups in parallel)

| Group | Files | What | Done when |
|---|---|---|---|
| **TF**, the New Game sequence | the title flow's unit file, `src/hook/inject_all.cpp` | Two new steps of ours after `TitleFlow_NewGame`: the name screen (open, fade in, the draw-all of research 4.6 in its order, the input step, the commit to record 0 with the layer's encoding, fade out), then the Config screen on its `Game_Step` 4 path, then EnterGame. The cursor-2 test in `TitleFlow_EnterGame` must tell "the title's third row" from "New Game's options"; M1 says how. A step list a later screen can join (IDEAS I56's champion screen). | New Game under `en-US` and `ja-JP`: name, options, the opening scene, the name in the first box that prints it; under the original Chinese per call 1; with the switches off, the port's flow frame for frame |
| **CR**, the renamer | `src/game/community/commu_name.cpp`, its fuzz file | The two `BareRet` calls and the `BareRetZero` call wired to NG's routine where `NameGrid_Available()`; the blank byte (`0x20` on the PC's path, `0xFF` in the routine) converted at entry-in and at the commits; the member path's 8 bytes and the slot path's 5. DIV-0075 keeps the entry's end where there is no grid. | The renamer's route: a slot and a member renamed, the name shown by message `0xF5`, saved, loaded, shown again |
| **BT**, the legend's buttons and the wide modes | the legend's part of `name_grid.cpp` (after NG), the wide tables | The six pictures by the button round's rule (call 2); the three fixed boxes under `BOF3X_WIDE`, as the menus are. | A capture a mapping and a width, sent to the owner |

### Stage C (the proof, then the owner's play)

- The ledger: one entry each for the name entry at New Game, the options
  step at New Game, and the renamer's grid; DIV-0075 and DIV-0020 amended;
  `ledger_check.py`, `ledger_registry.py --check`. Numbers are taken at the
  build (the ledger's last entry today is DIV-0227).
- `save-interchange.md` 2 and 2a, `USER_CHECKS.md` 4, the research's status,
  `button-tests.md`'s name-entry lines, STATUS, HANDOFF, `owner-review.md`.
- The state-hash routes with the switches off (identical to the pairs
  recorded at the tip before the round), then the new routes recorded as
  their own pairs. On a quiet, hands-off machine.
- The layout against M10's captures, by eye, the owner's.

## 4a. Stage A as run (2026-11-14)

Three worker agents, one worktree each under `D:\bof3x-runs\ng1114`
(`ng`, `gd`, `rt`; the integration worktree is `round\wt`), merged one at a
time into `new-game/round`. NG wrote the chunk's layout first (5a) and
stopped; GD and the routine then ran side by side from it. No state-hash
chain was run: another session's builds were running, and a proof wants a
quiet machine.

**What differs from the staged plan.**

- **The chunk is kind 22.** Kind 21 was taken by DIV-0225's town warp after
  the round was staged (`dat_load.cpp` `TakeChunk`, `disc_patch_build.py`
  `WARP_KIND`).
- **5a was amended twice while GD ran.** Its CLUT rule read "bits 15 and 14
  clear" against reference values that have bit 14 set; it is "bit 15
  clear" (GD and NG found it independently). The legend's draw mode became
  an operation (op 5), because the tiles' texture page differs per copy and
  per build; GD's first chunks lacked it and were rebuilt.
- **`tools/dat.py`** takes kind 22 in `KINDS` (one entry; `write_overlay`
  refuses an unknown kind).
- **`loc_build.py` reads the disc's code by running it**: each copy's
  drawer, input step and legend are told apart by a normalised hash of
  their code and run in a small R3000 interpreter with each callee answered
  by its role. No value is keyed on a build's name. The hash is the tool's
  own (no capstone); it groups the seven discs as M-B's did.

**RT** (`b2ae08db`). `battle_commands.txt` seeks NEW GAME on the title
cursor (`seek up 0x6BDF85 u8 == 0 max 3`; the recipe language has no s8,
and u8 `== 0` is exact for 0). With saves, before against after: 67 of 67
captured frames identical. With no slot file it reaches the battle with no
press, where the old `press up` chose the third row. Ten routes carry a
`# save` header from evidence in the docs and tools (`bridgeWalk` had one
already); three were replayed against the old file with `--save`
(`ninaWalkBehindBlock`, `identify`, `deisCave`: every frame identical), the
other seven were not replayed. **`garrAbility` has no header and gets none:
the owner (2026-11-14) does not remember its save or its purpose.** M8's
"lines 9-13" of `battle_commands.txt` are lines 9 to 18 now. No CI step
reads `tools/recipes/`.

**GD** (`9bb8777e`, `0973ff12`). `loc_build.py all` writes the chunk as the
last chunk of each layer's `FIRST.DAT`:

| Layer (build) | Chunk | Legend operations, `START` / `COMMU02` |
|---|---|---|
| `en-US` (psx-us), `en-150` (psx-eu-en) | written | 18 / 13 |
| `fr-FR` (psx-fr), `de-DE` (psx-de) | written | 13 / 11 |
| `ja-JP` (psx-jp) | written | 15 / 13 |
| psp-jp | the Japan chunk, from its EMIs | 15 / 13 |
| psp-eu | none (call 8) | - |
| the original Chinese | none (no disc has a grid) | - |

The shape counts are M2's and M3's; every list equals 5a's reference;
every other chunk of the five layers is byte for byte what the tree before
the change builds. `loc_build.py selftest` and `importer.py check` hold
the chunk on a synthetic grid of each family, with 28 broken variants
refused. A layer built before this change is offered a rebuild by the
stale check (`build_all`'s digest changes), read from the tool, the
launcher not run. The two PSP rows were checked by calling the converter
on the opened disc only: a whole PSP layer build fails in `check_relaid`
with and without this change.

**NG** (`00d8edd6`, with 5a's amendments and 5b). The routine, the reader
(`dat_load.cpp` kind 22) and the self-test. Builds at both widths. The
self-test: 150 checks over two synthetic grids, 0 mismatches; the full
shadow suite ran with no mismatch on the build before the two frame
addresses were named. The five real chunks pass the reader, every legend
operation is consumed through the recorder, and a random walk over each
real grid never puts the hand on an empty cell (a temporary edit, not
committed). **One real take each under `en-US` and `ja-JP`** through the
exe host, to the title: the grid is taken, the piece-list check against the
running image passes, no Fatal. Where the legend frames' piece lists
(`0x667148`, `0x653000`) are not what the routine knows - a disc-built
image may differ - `NameGrid_Available()` is false with the reason logged,
and the install skips the screen as one with no chunk does.

**The merged tree** (`new-game/round`): the 32-bit build, `loc_build.py
selftest` and `importer.py check` pass. `tools/quicksave_census.py check`
exits 1 on 675 statics with no rule, none of them `name_grid`'s; that
backlog is on `main` and the check is not in CI.

**Owed on stage A.**

- A hosted (`BOF3.exe`) run with a layer loaded; `fr-FR`, `de-DE` and
  `en-150` in a real game; the 64-bit build of the merged tree (NG's
  worktree built it before the last merges); the full shadow suite on the
  final tree.
- Nothing has been drawn on a screen. The layout, the texture pages and
  the look are stage B's (BT) and stage C's.
- `tools/recipe_saves.py:220` raises when the worktree and the saves
  folder are on different drives, after slot 0 is swapped (RT worked
  round it; one line to fix).
- GD's commits carry the co-author line of the model that wrote them, the
  others the orchestrating session's.

**Calls for the owner raised by stage A.**

| # | Call | Stands at |
|---|---|---|
| 7, again | NG read the Western step's Left and Right from a special a second time, in `START` and in `COMMU02`: the code stores column 9 or 0 with no test of the new cell, and row 5's column 9 is empty on the US / EU and French pages. The owner's observation and the code disagree at that one move. Ours never lands there either way. | One press on a real US build settles what the ledger says (5b has the text for both). |
| 9 | The `ja-JP` legend's small text: the layer has no 8 x 8 kana set, so the strings use kind 7's encoding (the 12-pixel kana at two thirds, as the Config screen). | Built so. The alternative is no `ja-JP` chunk until the layer has an 8 x 8 set. |
| 10 | psp-jp takes the Japan chunk from its EMIs; its own code was not read. | Built so. |
| 8 | psp-eu: no chunk, the screen skipped. | Still not answered; built as proposed. |

## 4b. Stage B: the owner's word (2026-11-14, by chat) and the groups as launched

**The owner's answers.**

- **Stage B goes ahead.**
- **Call 8, `psp-eu`.** The owner read "the screen is skipped on `psp-eu`"
  as the PSP release leaving the naming out, and asked for the name screen
  there as a change towards the PlayStation, with a toggle in the launcher.
  **What is measured is narrower** (M2): `psp-eu`'s `BOOT.BIN` holds the
  US grid and labels; its legend was not found and its code not read. Whether
  the PSP release shows a name screen is not established by anything in
  this repository. What the answer changes: `psp-eu` is no longer left
  without the screen. Group PK tries to give it a chunk (its own legend if
  found; otherwise it reports what a chunk would have to borrow, and the
  owner decides), and the name entry gets a launcher toggle (group LT).
- **Call 9, the `ja-JP` legend's small text.** The owner: the Japanese font
  has an 8-pixel kana set, and it should already be in the font files. Group
  PK finds it and moves the `ja-JP` legend's strings to it; the 12-pixel
  kana at two thirds goes if the set is there.
- **Call 8, after PK (the same day).** The owner ran the PSP release and
  sent a picture of its name screen (game-derived, not in the repository):
  the legend names cross for confirm, circle for delete, R and L for the
  name cursor, START for the finish, with R and L drawn as letters. It
  agrees with PK's reading of the PSP step (4c-PK). A second picture, of
  the Japan PSP release: circle confirm, cross delete, square the name
  cursor, triangle the page, START the finish, all five as button shapes,
  and five labels on rows 3 to 7. That is the Japan family's legend as M2
  has it, which supports call 10 by the look; `psp-jp`'s own code is still
  not read. Asked which style a `psp-eu` install shows, the owner: **"lets
  have the buttons match the mappings, like we try to do in game, and use
  the jpn mappings as defaults (if multiple sources are used)."** Read
  here as: the legend's pictures and the step's buttons follow the
  player's mapping on every install (the button round's rule, call 2,
  BT's work), so no install gets a style fixed by its disc; and where an
  install is built from more than one source, the starting mapping is the
  Japan one. What that leaves for `psp-eu` is a chunk (its grid, labels and
  strings are on its disc, 4c-PK) and the legend's rows drawn from the
  mapping rather than from the PSP's own tiles and letters (groups BT and
  PE, after TF merges). The owner's recollection of the order the starting
  mapping is taken in today: Japan PlayStation, then West PlayStation, then
  Japan PSP, then English PSP, then the PC. **Not checked against the
  source yet**; BT reads the launcher's and the game's rule first and
  changes nothing there if it is already so.

- **Call 7, settled (the same day).** The owner ran the US PlayStation
  release: with the hand on the quit label, Left puts the hand on the empty
  cell beside it; Right from the last letter of that row goes to the label.
  A confirm on the empty cell does, as far as the owner can tell, what the
  cell beside the full stop on row 2 does. So the code's reading (M2, 5b)
  is right, the original's Western step has the defect, and the owner's
  earlier observation is withdrawn. Ours never lands there (5b); the ledger
  takes 5b's "the hand" line as written. Not measured: what byte the cell
  beside the full stop holds, against the 0 the empty cell enters.

- **The skip's direction (the owner, the same day).** Where a move would
  put the hand on an empty cell, ours is to go on **in the direction of the
  move** to the next cell that is not empty (Left from the quit label goes
  to that row's last letter), not up to the row above as NG built it (5b).
  BT changes the step. The Japan family's page-2 clamps are that disc's own
  behaviour; BT says what the rule does to them before it changes them.

**The groups.** TF and CR as section 4 has them. RT's switch-off line and
new routes, and BT, run after TF merges (both need the screen reachable).
Two groups are new:

| Group | Files | What |
|---|---|---|
| **PK** | `tools/loc_build.py`, `tools/importer.py`, their checks; the small-text path only if the kana set needs it | `psp-eu`'s chunk (call 8) and the 8-pixel kana for the `ja-JP` legend (call 9) |
| **LT** | `src/launcher/`, the switch registry's launcher side | A launcher toggle for the name entry, in the form the launcher's other toggles towards the PlayStation have; its text in ASD-STE100 |

## 4c-LT. Group LT as run

*Group LT, 2026-11-14, on the local branch `new-game/lt` off `new-game/round`
`2207d9ce`. Launcher side only; TF's and CR's names are not in this tree.*

**How the launcher shows such switches today.** No control is made per
ledger entry or per registry row. The ledger's switch fields reach the
launcher only as the generated `src/launcher/ledger_registry.h`, which the
**presets** read (`ConfigPresetPlan`, Settings, Preset); a code name is
otherwise turned off by hand in `off=` (`docs/launcher-settings.md` 3.2). The
boxes the player sees are **hand-written**, one per behaviour, with a label,
a sentence under it and, where the install decides, a grey with a line that
says why: Mods, Extras (`desert_faerie=`, DIV-0204, and its neighbours, ini
keys), Game Options, Solo character (`SoloCharacter` in `on=`, edited by
`ConfigBehaviourEdit`, grey by `ConfigSoloState`), the suites and the
Jukebox's chord (`opt=` layers). The restorations towards the PlayStation
that are code names (`SunderLoop`, `MountainPasses`, ...) have no box; the
PlayStation preset is their control. So the registry text alone would show
nothing, and the toggle the owner asked for is a hand-written box.

**What is built.** Extras is on two pages, as the Cheats sub-tab (Page 1 /
Page 2 on the third row); Page 2 (`IDD_PAGE_EXTRA2`) holds two boxes over
code names in `off=`:

- **The name entry, one box** for `NewGameNameEntry` and `CommuNameGrid`:
  one routine, one grid chunk, one rule for when it plays, and to the
  player one screen (A1). Two boxes would only offer a half that no answer
  of the owner's asked for; `off=` can still turn either off alone, and the
  box then shows cleared.
- **The options step, a second box** for `NewGameOptions`: another screen,
  with its own switch by call 1, which every language can play. The
  integrator may drop it if the owner wants only the name entry.

Each box is ticked unless `off=` turns a name off, and is written back only
once clicked (`ConfigNewGameOffEdit`, `ConfigBehaviourEdit`'s rule), so an
untouched page writes `off=` byte for byte. It reaches the game as
`BOF3X_OFF`, as every `off=` does. **Grey, with the reason**
(`ConfigNewGameState`): a box whose names the release lacks
(`src/hook/code_names.h` through `ConfigCodeNamesKnown`; the game stops on
a name it lacks, so the box must not write one) - in this tree both boxes,
until TF and CR land their code names and `ledger_switches.py` regenerates
the header; a box the chosen preset turns off (`PresetPlan::off`, which
holds these names once stage C writes their ledger entries and regenerates
`ledger_registry.h`); and the name entry when the language played has no
grid.

**How the launcher knows what a layer holds.** It did not, before: it asks
only whether a layer's files exist. `ConfigLayerHasNameGrid` now reads the
chunk headers of the language layer's `FIRST.DAT` (16 bytes each, as
`dat_cache.cpp` `CountByWin32` walks them) for a kind-22 chunk of tag 0
(section 5a), in the file the game walks (DIV-0089: the cache's
`loc\<tag>\dat\FIRST.DAT` when the cache has the layer, else the install's
`DAT\<tag>.FIRST.DAT`). The original Chinese has no layer; `psp-eu` has a
layer without the chunk until PK writes one, and then lights up with no
launcher change. **Not seen by the launcher:** the game's second test, the
PC's legend frames in the running image (`NameGrid_Why`). An install whose
image lacks them shows the box enabled and the game skips the screen and
logs why. To show that too, the launcher would read the two lists out of
`BOF3.exe` (`ExeImageRead`) against the hashes in `name_grid.cpp`, and would
need an answer for a disc-built install with no `BOF3.exe`.

**The text** (ASD-STE100; no game text):

- *Name box:* "Show the name entry screen at New Game and in the faerie
  village". Under it: "You select the letters of a name on a grid, as in
  the PlayStation version. At New Game, you give a name to the hero. In the
  faerie village, you can change a name. Off: New Game does not ask for a
  name, and a name in the faerie village does not change."
- *Options box:* "Show the options screen at New Game, before the game
  starts". Under it: "As in the PlayStation version. You can also change
  these settings from the title menu. Off: New Game starts the game
  immediately."
- *The reasons:* "This version of the game does not have the name entry
  screen." / "... the options screen at New Game."; "The preset (Settings,
  Preset) turns off the name entry screen when the game starts." (and the
  options screen's); "The Chinese (PC script) text has no name entry
  screen. No copy of the game has its letters."; "The files of this
  install for <language> do not have the name entry screen. The importer
  adds it only from a copy of the game that has it (Data, Sources)."

**Run.** The main build (`cmake --preset i686 && cmake --build build`)
with no new warning; the launcher's host tests
(`tools/launcher_host/config_tests.exe`): 4,021 checks, the 35 new ones
(`TestNewGame`: the chunk test against made-up containers, install and
cache; the page's state and greys; eleven edits of `off=`; the ini's
`off=NewGameNameEntry,CommuNameGrid` and `BOF3X_OFF` for both hosts) pass;
2 failures, both in `TestPresetPlan` and the same on `2207d9ce` without
these changes. The page templates measured in hidden dialogs of the
measuring process: Extras under the third row ends at 475 of the window's
480 pixels at 100 %, and every text of Page 2 fits its control.
`ledger_switches.py --check` and `ledger_registry.py --check` pass.
**Not run:** `tools/launcher_host/dialog_test.py` (it finds the dialog by
its title, and another session uses the launcher on this machine); its
expectations are updated for the third row over Extras and Page 2's boxes.
No capture of the page; the owner has not seen it.

## 4c-PK. Group PK as run

*Group PK, 2026-11-14, worktree `D:\bof3x-runs\ng1114\pk\wt` (branch
`new-game/pk` off `new-game/round` `2207d9ce`). Capstone over the held
discs through `loc_build.open_disc`; scratch scripts and every read-off in
`D:\bof3x-runs\ng1114\pk\s`. No game byte below: addresses, offsets, counts
and code constants only.*

**Call 9: the 8-pixel kana are on the Japan disc, in the layer's own font
file.** The Japan disc's small draw (`SLPS_009.90` `0x8014FD78`, the twin
of `Text_DrawSmall`) samples texture page `0x2F`, which is `ENDKANJI.EMI`
section 0 - the sheet `build_table_ja` already puts into the `ja-JP` font
whole. For colour 0 its table `0x8017FF18` gives v base 0 and height 8; a
byte below `0x5B` is cell `b - 0x20` from y 0, a kana byte (`0x5B..0xFE`)
cell `b - 0x5B` from v = base - `0x58` as a byte (y `0xA8`), a `0x15 nn`
code cell `nn + 0x40` from y 0, 32 cells of 8 x 8 to a row; `0xFF` advances
8. All 164 kana cells have ink (counted on `psx-jp`). The set was in the
layer already, but as 12-pixel cells: an 8 x 8 cell is never on a 12-pixel
cell's boundary of its own, so no glyph index named one.

What changed (`tools/loc_build.py` only, no `src/`): a Japanese layer's
legend strings are encoded by `ja_small_encode` - each character the
two-byte code of a glyph made from its 8 x 8 cell tripled to 24 x 24 (as
DIV-0016's Latin set: `Text_DrawSmall` shows the whole glyph at 16 x 16),
allocated on first use after the pair codes, an index whose low byte is 0
skipped (its code would carry a NUL), advance 8. `ja_small_chunks` appends
those glyphs to kinds 3 and 4 after `ja_pair_chunks`; a refused chunk
allocates none. `Text_DrawSmall` has no glyph bound and the string draw's
bound follows the table (DIV-0016), so nothing in `src/` changes; the
reader is encoding-blind.

Measured (`loc_build.py all --discs` with the tree before and after, over a
PC game folder): 1,233 files each; 1,232 byte-identical. `ja-JP.FIRST.DAT`
differs in three chunks only: kind 3 grows by 27 glyphs (`0xDF3..0xE0D`, 26
cells and the skipped `0xE00`) with its first 3,571 glyphs unchanged, kind 4
by 27 bytes likewise, and kind 22 by its legend entries alone (all 8 in each
copy, the same byte lengths, so the same size, 1,710 bytes, and the same
rooms); grid, code map, labels and operations unchanged; the chunk passes
`name_grid_check`. `psp-jp` (its EMIs, converter called on the opened disc):
the same chunk and the same 26 cells, pixel for pixel. The glyphs were
rendered in scratch and read as the disc's kana; nothing has been drawn on a
screen. Kind 7's Japanese strings (the Config screen) still use the
12-pixel kana at two thirds: moving them is the same encoding, not done.

**Call 8: `psp-eu`'s parts are all on its disc; its input step is a third
shape.** Read in `BOOT.BIN` (image offsets; its pointers are link-time
offsets, a `.text`-range value used as data is `.bss`, `tools/psp_elf.py`):

| Part | `START` | `COMMU02` | Against `psx-us` |
|---|---|---|---|
| grid (288 bytes) | `0x2EC7A8` | `0x2EF1C9` | equal (M2's md5) |
| labels (4 x 5) | `0x2ECB18` | `0x2EF78F` | equal |
| legend table (8 pointers) | `0x2ECB44` | `0x2EF6FC` | entries 0..4 and 7 equal; 5 and 6 one byte each, empty on the US disc |
| drawer | `0xC3CD0` | `0x11645C` | West: codes 1..4 at slots 0..3, colours 4, 4, 2, 2, count 5, the same pitches |
| legend | `0xC3FD0` | `0x1149BC` | as the US ops except rows 1..3 of the button column (below) |
| input step | `0xC36C0` | not read | not West (below) |
| page byte | `.bss` + `0x8261B`, one store, constant 2 | `0x2EF1C8`, no store, static 2 | page 2 |

The PSP's code passes arguments 5..8 in `$t0..$t3`, uses branch-likely
instructions and copies the label block with a call: `MipsRun` reads none
of that today, and `loc_build.py` reads no `BOOT.BIN` function.

The difference that matters: the PSP `START` input step's backspace is
pressed bit `0x20` (West `0x10`), the name cursor moves on repeated `0xA` /
`0x5` only (West adds pressed `0x20` / `0x80`); confirm `0x40`, finish
`0x800`, the codes' actions and Left / Right from a special are the West's.
Both PSP legends match that step, not the West's: the button column keeps
tile u `0x1D` on row 0, has u `0x1B` (US: row 2's) on row 1, and draws
entries 6 and 5 (count 1) on rows 2 and 3 where the US draws tiles u `0x1B`
and `0x1E`. A chunk with family 1 and the PSP's legend would put the West
routine's cursor + 1 button beside the backspace text. No chunk is written;
the refusal now says this (`read_name_grid`). The owner's choice, each
still to build: (a) a third family in 5a with the PSP's bits and its own
legend (the reader and NG's routine change, `name_grid.cpp`); (b) family 1,
the PlayStation West's routine, with the US legend's operations borrowed
from a held PlayStation disc and the PSP's strings and grid (the strings
used are equal); (c) family 1 with the PSP's legend, row 1 mislabelled.
Any of them needs `BOOT.BIN`'s functions read (the conventions above).

**4a's failing PSP build.** `loc_build.py all --game` (over a PC install)
with a PSP disc stops in `check_relaid` because the PSP's glyph atlas
(`STATUS` `0x1C080200`) differs from the port's in one 16 x 16 icon, 121
pixels, inside the rule's strip (measured in `loc-build-psp.md` 4). That is
by design: the importer builds a PSP layer disc-only (`--cache`,
`resolve_languages(psp=True)` only there), where `check_relaid` does not
run. Not the discs held, not a regression; nothing changed.

**Run:** `loc_build.py selftest` (with a new check of the 8 x 8 path on a
made-up sheet), `importer.py check`, both pass. **Not run:** the game; a
disc-only `ja-JP` or `psp-jp` layer build end to end; the 64-bit build
(no `src/` change).

**Follow-up as run** (after `new-game/round` `037a316b` was merged in).

*The finish label's white block under `ja-JP` (BT's report) is D18,
Capcom's glyph-cache overrun, reached.* A temporary log in
`Font_GlyphTexture` on the `ja-JP` name screen named four lookups a frame
that found all 128 entries used and no hit: the finish label's first two
kana (colour 2) and two colour-0 glyphs. The katakana page, the labels in
two colours, the message and the legend's 26 new 8 x 8 glyphs pass 128
distinct textures a frame; GD's layer drew the legend from glyphs the
screen's other text shares and stayed under. Nothing in the layer collides:
the label bytes are the same in both layers. **Fixed in the renderer**
(`src/game/platform/d3d.cpp` `Font_GlyphTexture`): with every entry used
and no hit, ours rebuilds an entry in turn (`g_reuse_next`) where Capcom's
index runs one past the table. The draws already made from that entry keep
their texture: the render shim snapshots a texture written with draws
pending (`render_shim.cpp` `BeforeWrite`). The switch is the code name
**`GlyphCacheReuse`** (the owner's word, 2026-11-14): a byte
(`glyph_draw::g_full_cache_reuse`) that `GlyphDraw_Inject` sets through
`PatchBytes`, on by default, off under `BOF3X_OFF=GlyphCacheReuse` (or
`BOF3X_ORIGINAL`), which gives Capcom's overrun exactly; regenerated into
`src/hook/code_names.h` and `analysis/ledger_switches.tsv`. The fuzz
compares Capcom's overrun byte for byte with it off and, in each of the
4,965 overrun rounds, checks the reuse with it on (returns the entry,
builds it, marks it used, leaves the vertex block alone): that is the check
that would have caught it. Captured, `ja-JP` `newGameName.txt`: on, the
label draws whole (`pk\cap\ja_reuse_on`); with `BOF3X_OFF=GlyphCacheReuse`
the white block returns (`pk\cap\ja_reuse_off`), so the switch is live. **Ledger text (a new
entry, D18):** "Original: `Font_GlyphTexture` `0x5A2BC0`, with all 128
glyph textures used in a frame and none a hit, returns 128 and binds the
vertex block's bytes as a texture (a garbage glyph, seen as a white block).
New: the entry after the last one rebuilt, in turn, is rebuilt for the new
glyph; earlier draws keep their texture through the render shim's
snapshots. Why: the `ja-JP` name screen draws more than 128 glyph
textures. Switch: `GlyphCacheReuse` (code name, default on; Player: yes).
Toward: neither (the PlayStation draws glyphs from VRAM pages and has no
such cache)."

*The pair codes skip an index whose low byte is 0* (`ja_new_pair`): the
241st pair would be `0xE00`, whose code carries a NUL. The slot stays in
the font (blank) and in kind 13 (the pair `(0, 0)`), so kind 13 stays one
entry a glyph from `PAIR_AT` and the engine reads it unchanged; below 240
pairs nothing changes, and the owner's layer has 227, so no built layer and
no save changes.

*The `START` row's word.* Layers already carry images as kind-1 chunks
(the title page, the village sheets, the dock page, the plates), and
`TakeChunk` applies a kind 1 from any overlay, so the strip goes into the
layer's `FIRST.DAT` as one: `grid_page_chunks` finds every op 3 a copy draws
on page `0x2F` (after an op 5 of that tpage, or after a text or the leaders
with no op 5 between - the line added to 5a's op 5 row), compares each
8 x 8 cell of the disc's `FIRST.EMI` sheet `0x1E000200` with the base's
(the install's `FIRST.DAT`, or on a disc-only build the layer's own
re-laid stand-in), and writes each 32 x 32-word tile with a differing cell
as the base's tile with the disc's cells pasted in. No `src/` change: only
`FIRST.DAT` and `ENDKANJI.DAT` load that VRAM, so the layer's tile stands
until the ending. Measured, every held disc: `psx-us` and `psx-eu-en` 4 of
their 10 page-`0x2F` cells, one tile, `0x1E040100`; `psx-fr`, `psx-de`,
`psx-jp`, `psp-jp` none (their cells, the button sheet, equal the PC's;
the French and German discs hold the word but draw no tile on it, the
Japan disc holds other art); `psp-eu` (disc-only, against its stand-in) 4
of 8, the same tile. Layers over the PC game folder against the set before
the follow-up: 1,231 of 1,233 files byte-identical; `en-US.FIRST.DAT` and
`en-150.FIRST.DAT` each gain that one chunk (2,048 bytes); `ja-JP` is
unchanged. Captured: the `en-US` legend's `START` row draws its word.
**Ledger text (a new entry, or an amendment to the name entry's):** "Original: the PC port cleared a rectangle of the
font page `0x1E000200`, so the four row-4 tiles of the Western New Game
legend sample blank texels. New: a Western layer carries the disc's own
texels for the cells its legend draws on page `0x2F` (one 32 x 32-word
tile, kind 1 in `FIRST.DAT`), where the install's differ. Why: the
PlayStation draws a word there. Toward: psx."

**Run (follow-up):** the 32- and 64-bit builds; `BOF3X_SHADOW=glyph_draw,
name_grid,new_game` (self-test only, hosted): glyph_draw's 40,000 lookup
rounds with 4,965 reuse checks, name_grid 145 checks and new_game 9, 0
mismatches; `loc_build.py selftest` (new: the pair skip, the page-0x2F
cells) and `importer.py check`, both pass; `tools/recipes/newGameName.txt`
under `en-US` and `ja-JP` from `D:\bof3x-runs\ng1114\pk` (captures in
`pk\cap\en-US` and `pk\cap\ja-JP`). **Not run:** the whole shadow suite;
the 64-bit build in a game; `psp-eu` and `en-150` in a game; a disc-only
build end to end (`grid_page_chunks` called on PE's disc-only cache and
the opened disc only).

## 4c-TF. Group TF as run (2026-11-14)

*Worker agent on its own worktree `D:\bof3x-runs\ng1114\tf\wt`, branch
`new-game/tf` off `new-game/round` `2207d9ce`, local commits only. Runs,
captures and scratch scripts in `D:\bof3x-runs\ng1114\tf` (game-derived,
never committed). No state-hash chain: the machine was not quiet.*

**What was built.**

- `src/game/core/new_game.{h,cpp}`: the sequence. The screens are sub-steps
  of `Game_Step` 2 on `SaveMenu_FlowState` `0x6BDF86`: 0 the open
  (`NewGame_InitCharacters` and DIV-0228's stamp), 1..4 the name screen (the
  PlayStation's n0..n3: open, fade in, input, fade out). A recipe waits on
  `0x66C7EA u16 == 2` and `0x6BDF86 u8 == 3` for the input step. The step
  list `kScreens` is where IDEAS I56's champion screen joins (its sub-steps
  after 4, its own test). After the last screen, the port's rest of
  `TitleFlow_NewGame` (0x9039F2, the area number, `Scenario_Start(0)`, the
  `Cond_Flags` words) and `Game_Step` 4 with no fade - the screen black and
  no transition running, as the title's third row enters it - or, with the
  options off, the port's `Transition_Start(0)` and `Game_Step` 5.
- `TitleFlow_NewGame` (`sound.cpp`) asks the sequence first and otherwise
  runs its body unchanged, cut into `TitleFlow_NewGameRecords` and
  `TitleFlow_NewGameRest`. `TitleFlow_Begin` tells the sequence the title
  task started (the buffer and page reset, M1); it writes no Capcom byte.
- **The cursor-2 test** in `TitleFlow_EnterGame` is untouched: nothing of
  ours writes `0x6BDF85`, so it is 0 through the name screen, the Config
  screen and step 5 (M1). Measured: the `en-US` route below enters the game
  after the Config screen; `config_screen.txt` (the third row) was not run.
- The draw-all is START's `0x801E6F80` call for call, read from the capture
  (scratch, M-A's lib): backdrop, title box `(0x14, 0x12, 0x118, 0x13)`,
  message `0xBA` / `0xBB` on bit `0x40` of `Frame_Counter` at `(0x1C,
  0x15)` count `0xFF`, the name panel, `NameGrid_DrawLegend` as NG built it,
  the grid's frame, `NameGrid_Draw`, then the actor (every 10 frames `+6`
  steps through 0..7 with animation `+6 + 8`; queued and ticked each frame).
  `Draw_PassFlags` 1 at the open; `Field_DrawFrame` after every sub-step.
  n1 draws then tests the fade; n3 draws only while the fade runs (as
  `0x801E4550` and `0x801E4E0C`).
- **The name panel** `0x801E6308`, read whole: box `(x+3, y+3, 0x7D, 0x2F)`,
  portrait `0x801DC37C(x+6, y+2, 0, 0)`, the name at `(x+0x3B, y+0x19)`
  count 5 in colour `buffer[5] != 0 ? 2 : 0`, the underline at `x + 0x3B +
  12 * cursor, y + 0x25`, width `0xC`, then `Menu_DrawPieces(x, y,
  0x801ECF28, 0)`. Measured against `BOF3.exe` (`panel_cmp.py`,
  `panel_cmp2.py`): START's piece list equals the PC's `0x652F5C` through
  its end marker (162 bytes) and START's portrait table entry 0 equals the
  PC's `0x652F38` entry 0, so the PC's list and `CommuMember_DrawPortrait`
  draw them. The name is drawn as the layer stores it (`NameGrid_Encode`).
- **The commit:** record 0's 8-byte name through `NameGrid_Encode`; when the
  finished cells equal record 0's own name decoded (the all-blank default,
  or the default typed in), the record is left as it is.
- **Switches** (code names, `PatchBytes` flags of ours, set after every
  module's self-test): `NewGameNameEntry` (shows where
  `NameGrid_Available()`), `NewGameOptions` (call 1's proposal: every
  language, the original Chinese included). With both off, or the name
  entry on where there is no grid and the options off, the port's body runs
  whole. `code_names.h` regenerated (`analysis/ledger_switches.tsv` left as it was: it was already stale on the branch and is not checked).
  **The ledger checks pass with no entry yet:** `ledger_switches.py
  --check` (0 errors), `ledger_registry.py --check`, `ledger_check.py` (0
  errors). The `original` preset turns them off only once stage C's entries
  carry the fields.
- **Quicksave: a participant, `NEWGAME`** (the cells and the reset flag).
  Its check refuses a take made on the name screen when this start has
  `NewGameNameEntry` off or no grid of the same family ("name screen,
  switch off" / "name screen, no grid"); every other take loads. Another
  language is ST's refusal already. Self-test under
  `BOF3X_SHADOW=new_game`. Census rows in `tools/quicksave_census.py`. No
  take or load on the screen was run in a game.

**A defect found on the way (the grid's frame).** The PC's
`Commu_DrawTiledFrame` `0x45F1A0` passes `Gpu_SetDrawMode` a texture-window
RECT on its own stack, and the PC's draw mode keeps the pointer (dword +8,
read at the present). Drawn at New Game it crashed: `BOF3X_TEXPAGELOG=1`
logged `build page 29 ... key 7492,126 13064x130`, then an access violation
in `D3d_BuildPageTexture` (a draw-skip bisection over six runs put it on
that call alone). The PC never ran the function in play: the renamer's
entry steps that call it are the cut ones (research 5.1). Ours draws the
frame with each RECT in the packet pool (`DrawGridFrame`, as
`Menu_DrawTitleBox` does). **Group CR meets the same crash** when the
renamer's steps run `Commu_DrawTiledFrame` (`commu.cpp`, not TF's file).
It wants a known-defects number and the same fix there.

**What was run** (32-bit build, exe host, my game copy: GD's `BOF3.exe` and
`DAT\` with its layers, the PC's `BGM\`, `SND\`, movies):

| Run | Result |
|---|---|
| `en-US`, scratch `ng_name.txt`: New Game, name screen, three letters, Start, Config, Start, the opening | `done` at 3,234; record 0 `0x444241` (3 bytes) where it held `0x757952`; Config shown; the opening scene and its battle run, no Fatal |
| `ja-JP`, the same route | `done` at 3,234; Japan grid, 3 pages; record 0 named in 6 bytes; no Fatal |
| the original Chinese, scratch `ng_orig.txt` | no grid ("no kind-22 chunk taken"): no name screen; Config, then the opening; `done` at 1,367 |
| `battle_commands.txt`, `en-US`: the tip `2207d9ce` build against this one with `BOF3X_OFF=NewGameNameEntry,NewGameOptions` | **67 of 67 frames identical** (`BOF3X_SHOT_EVERY=60` and the recipe's shots) |
| `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'`, hosted, ini-less launcher folder | "self-test only: done"; 917 self-test lines with 0 mismatches and none with more, among them `save_menu`'s (66,000 rounds, `TitleFlow_NewGame` included), `rest_4f`'s, `name_grid`'s and `new_game`'s (9 checks); no Fatal. The launcher was ended by my wrapper's 15-minute timeout while the game process ran on to the end, so the exit code was read from the log, not from the launcher |
| `BOF3X_SELFTEST_ONLY=1`, exe host | game process exit code 0 |
| 64-bit preset (`cmake --preset x86_64`, `build-x86_64`) | builds; not run |

Captures kept in the root: `shots\en_new\name_empty.png`,
`name_typed.png`, `config.png`; `shots\ja_new\` the same.

**Not run, or not reached.** No route reaches a box that prints record 0's
name: the opening's boxes up to frame 3,234 are other speakers'. Not run:
the hosted `BOF3.exe`, `fr-FR`, `de-DE`, `en-150`, `psp-eu`; the 64-bit
build in a game; the special code 4 (quit to the title) and an all-blank
finish in a game (only through NG's self-test); a quicksave on the name
screen; `config_screen.txt`; the wide modes. Whether the growth roll after
the options instead of before (M-A's "order" note) moves anything was not
measured.

**Ledger text for stage C** (numbers are stage C's).

- *NewGameNameEntry* (Tier: Intent; Toward: psx; Default: on where the
  layer has the grid). Original (the PC): New Game goes straight to the
  game; record 0 keeps its default name. PlayStation: New Game opens a name
  screen for record 0, then the options screen. This: the PlayStation's
  name screen, START's sub-steps n0..n3 as sub-steps 1..4 of `Game_Step` 2,
  drawn and run by the shared routine; Start with no name keeps the
  default (DIV-0020's). Why: the owner (2026-11-12, A1, A3). Switch:
  `NewGameNameEntry` (code). Fold in 5b's lines (the hand, the field's
  room, four kana, the buttons, the pad word, the legend's piece) and: the
  grid's frame drawn with its texture windows in the packet pool, where the
  PC's `0x45F1A0` leaves them on its stack (the defect above); the default
  left in record 0 rather than copied back through the grid.
- *NewGameOptions* (Tier: Intent; Toward: psx; Default: on, every language).
  Original (the PC): the Config screen is the title's third row only.
  PlayStation: the options screen after the name, before the game. This:
  after New Game (and its name screen) the Config screen on the title's
  `Game_Step` 4 path, unchanged, then the game. The scenario's start and
  the growth roll keep the port's order (before / after the options; the
  PlayStation rolls before and starts after, M1). Why: the owner
  (2026-11-12) and call 1's proposal. Switch: `NewGameOptions` (code).

## 4c-CR. Group CR as run (2026-11-14)

*Worker agent, worktree `D:\bof3x-runs\ng1114\cr\wt`, branch `new-game/cr`
off `new-game/round` 2207d9ce. Harness root `D:\bof3x-runs\ng1114\cr` (its
own game folder, launcher copy and captures). No state-hash chain.*

**What was built.**

- `src/game/community/commu_name.{h,cpp}`: the switch `CommuNameGrid`
  (`PatchBytes`, on by default, set after `rest_4d`'s self-test). Where it is
  on, a grid is loaded (`NameGrid_Available()`) and the entry is not a
  faerie slot under the Japan family (call 6), the two `BareRet` calls are
  NG's legend and grid draws and `BareRetZero` its input step
  (`Copy::kRenamer`), over the PC's own cells (`0x675F98`, `0x675F94`,
  `0x675FCC` column, `0x675FCA` row, `0x675FCB` page). Elsewhere the six
  steps are as before (DIV-0075's end).
- **Entry-in** decodes the stored field (slot 5 bytes, record 8) to cells:
  each one-byte `0x20` a blank (`0xFF`), the trailing ones dropped, a NUL
  the end; then cut to the family's cells (4 under Japan, call 5) and to the
  field's bytes. A name the grid cannot decode opens blank, logged. The page
  opens on `NameGrid_FirstPage()`.
- **The input step** answers 1 on a finish and moves the step on at a finish
  or a quit, as the PlayStation's does.
- **The answered end** encodes the cells into `0x675F98` and NUL-fills nine
  bytes, so the commits (R4E's, unchanged) and their text record take the
  layer's bytes; the member step after the commit writes the NUL at the
  record's `+8` and at `Text_Records + 0x28` (an eight-byte name has none).
- **The panels** draw the buffer as text: while the cells are in it they are
  handed the encoded name for the draw, and the cells are put back after.
- **Draws before the first entry-in** use the open's cells (first page, row
  and column 0) while the PC's bytes are off the grid.
- `src/game/community/commu.cpp`: `Commu_DrawTiledFrame` puts each texture
  window in the packet pool, always (the defect below; the orchestrator's
  instruction, from TF's finding; first built as a switch,
  `CommuFrameWindowInPool`, and made unconditional at the owner's word, 2026-11-14,
  follow-up below).
- `rest_4d_fuzz.cpp`: `GridTest` (below); `EntryAbandonTest` now runs with
  a "no grid" API, since the grid's byte is set by then.
- `src/hook/code_names.h` and `analysis/ledger_switches.tsv`: regenerated
  with `tools/ledger_switches.py` on the merged tree (one code name of CR's,
  `CommuNameGrid`).

**What was run.**

- Builds: `i686` and `x86_64`, clean.
- `BOF3X_SHADOW='*'` headless from the root's ini-less launcher copy: exit
  0, 922 lines of `0 MISMATCHES` and none other (before the frame fix);
  `rest_4e,rest_4d` again after it: exit 0, 9 of 9.
- `GridTest`: the seven entry steps (the six and the member's commit step)
  with the switch on and no grid against Capcom's with DIV-0075's step,
  28,000 rounds; the slot's three under a Japan grid, 12,000 rounds; then
  375,847 checks of the grid's paths on two synthetic grids (each helper's
  rule, and a random walk: 1,621 finished, 1,556 read back through the slot
  commit's bytes, 179 quit). A planted defect (page 0 at entry-in) was
  caught.
- `ledger_switches.py --check`, `ledger_registry.py --check`,
  `ledger_check.py`: pass. A code name with no ledger entry is not refused.
- **Live, `en-US`, the faerie village save** (`fairyVillage`), in the root's
  own game folder with GD's layers: the renamer entered by a **harness
  step** - `poke` of `Field_ObjectTriggers` id 8's stores (tail kind `0x17`,
  sub-kind 3, the object pointer, the script flag), not a player's route; no
  recorded route reaches the renamer. A member renamed ("Ryu" to "llu") and
  a slot ("Candy" to "Kit"), each shown by message `0xF5`; F12's save; a
  second and a third run loaded it and the panels showed "llu" and "Kit";
  the file holds them at `+0x90`, `+0xCA0` and `+0xF1A` (space-padded).
- **Live, switch off** (`BOF3X_OFF=CommuNameGrid`): the entry ends as
  DIV-0075 has it (the buffer `0x20`-padded, the step on the next frame,
  unanswered, state 4), the record unchanged.
- **Live, `ja-JP`, the member path**: the grid (three pages) and the kana
  legend drawn; the save's name, made under `en-US`, is off the Japan grid
  and opened blank (logged); one kana entered, finished, shown by `0xF5`.
  Not saved or reloaded.

**Not run.** The slot path under `ja-JP` live (DIV-0075's end there is held
by the harness rows only); a four-kana name; `fr-FR`, `de-DE`, `en-150`; the
wide modes; the 64-bit exe in play (built only; the runs used the 32-bit exe
host and the hosted build). The pool placement of the frame's windows has no
original to compare with (follow-up below).

**Owed.** The legend's button pictures do not match the keys that act on
this install: it shows the PlayStation's (cross OK, triangle Del), and the
PC's circle entered a letter, cross erased, triangle moved the name cursor
back - BT's call 2. The renamer has no recorded route; RT's could start from
the poke above or from the village object whose trigger is id 8.

**Defect for `known-defects.md` (stage C to number).** `Commu_DrawTiledFrame`
(`0x45F1A0`) passes `Gpu_SetDrawMode` a texture-window RECT on its own
stack; the PC's draw mode keeps the pointer and reads it at the present,
after the function has returned. The renamer's entry steps, its only
callers, were cut, so it never ran in play; with the grid wired the game
ended silently on the entry's first frame (no Fatal, no crash report), and
with the grid switch off too (the crash reporter left a dump of 0 bytes).
Fixed unconditionally: a correction of a PC-only defect, no switch.

**The ledger text for stage C.**

- *The renamer's grid* (new entry; switch `CommuNameGrid`, `PatchBytes`,
  default on, player yes, toward psx, languages: every layer with a grid).
  *Original:* the port cut the entry's legend, grid and input step (two
  `BareRet`, one `BareRetZero`); DIV-0075 ends the entry unanswered. *New:*
  NG's routine in their place, `COMMU02`'s copy, over the PC's cells; the
  stored name decoded to cells at entry-in (`0x20` the PC's blank, `0xFF`
  the routine's; a name off the grid opens blank) and encoded at the
  answered end, NUL-filled; the member commit's NUL written after it; the
  panels drawn with the encoded name. 5 bytes for a slot, 8 for a record.
  Under the Japan family the member path has four cells (call 5) and the
  slot path keeps DIV-0075's end (call 6). With 5b's items (the hand, the
  field's room, four kana, the buttons, the pad word) for this copy.
  *Also:* an all-blank member name takes the record's default from
  `Char_DefaultRecords` (`0x64B390`, the layer's under DIV-0020), where the
  port's code reads `CommuName_RecordNames` (`0x669FA4`), the exe's own
  text, which no layer replaces; the PlayStation takes `COMMU02`'s roster
  names. *Why:* the owner's A1 (both screens) and call 6. *Off:* DIV-0075's
  end. `original` preset: off.
- *DIV-0075 amended:* where `CommuNameGrid` runs, the entry is the grid's
  and this entry's end does not apply; it holds with the switch off, with
  no grid (the original Chinese; any layer without the chunk) and for a
  faerie slot under the Japan family (call 6).
- *The tiled frame's windows* (a new entry or a line of the renamer's; no
  switch: the owner, 2026-11-14, "go ahead and make it unconditional").
  *Original:* `Commu_DrawTiledFrame` `0x45F1A0` hands `Gpu_SetDrawMode` a
  texture-window RECT on its stack, which the PC's present reads after the
  return (the defect above). *New:* each RECT in the packet pool, as
  `Menu_DrawTitleBox` and TF's `DrawGridFrame` do. *Also in the PSX
  version?* No: its `SetDrawMode` copies the window into the packet at the
  call. *Reversible?* No switch; `BOF3X_ORIGINAL=Commu_DrawTiledFrame` runs
  Capcom's, with the stack RECT (the defect).

**The owner's answers (2026-11-14, by chat).** (1) An all-blank member name
takes the default name, as built: no change. (2) The frame fix: "go ahead
and make it unconditional".

**Follow-up as run (2026-11-14, on the merged tree 3930b821).**
`CommuFrameWindowInPool` is gone (its `PatchBytes` byte and its code name).
`Commu_DrawTiledFrame` is one body, `TiledFrame`, taking where a window goes:
the game's entry passes the pool placement (eight bytes taken at
`Gfx_PacketNext`, the cursor moved past them), and
`Commu_DrawTiledFrameStackWindow` passes the original's stack RECT. `rest_4e`'s
row for `0x45F1A0` now runs the second against Capcom's clone: the same body,
every other byte, call and argument compared as before, the windows by their
eight bytes (the draw mode's recorder logs the RECT's bytes, not its address).
What that row no longer covers is the pool placement itself - the eight bytes
taken in the pool and the cursor moved past them - which has no original to
compare with: it is three lines, seen live. No row dropped.
Ran: `ledger_switches.py` (regenerated) and `--check`, `ledger_registry.py --check`, `ledger_check.py`: pass. Builds `i686` and `x86_64`: clean. The whole `BOF3X_SHADOW='*'` suite headless from the root's ini-less launcher copy: exit 0, 922 lines of `0 MISMATCHES` and none other; `rest_4e`'s 48 functions, 288,000 rounds, 0 mismatches. This is also the full suite owed after the frame fix.
Live, `en-US`, the member path of the `fairyVillage` save (the same harness poke): the grid's frame drawn, no crash, "Ryu" edited to "llu" and shown by `0xF5`. Not run again: the slot path, the save and load, `ja-JP`.


## 4c-PE. Group PE as run

*Group PE, 2026-11-14, worktree `D:\bof3x-runs\ng1114\pe\wt` (branch
`new-game/pe` off `new-game/round` `768e9f74`). Capstone and the reader over
the held discs through `loc_build.open_disc`; scratch scripts and read-offs
in `D:\bof3x-runs\ng1114\pe\s`. No game byte below: addresses, offsets,
counts, op kinds and code constants only.*

**`psp-eu` has a kind-22 chunk, read from its own `BOOT.BIN`.** By the
owner's word on call 8 (4b) it carries family 1, the West's step; its grid,
code map, page, labels and legend are the PSP disc's. Nothing is borrowed
from a PlayStation disc and no table is in the source. What was built
(`tools/loc_build.py` only, no `src/`):

- `AllegrexRun`, `MipsRun` for a `BOOT.BIN` image: code is fetched from
  `.text` only; a data access inside `.text`'s range is `.bss`
  (`tools/psp_elf.py`'s rule) and reads as unwritten memory, 0, as an
  address outside an overlay's sections does on the PlayStation; arguments
  5..8 come from `$t0..$t3`; the branch-likely instructions, a likely
  branch not taken skipping its delay slot; and a new role, `copy`, the
  `memcpy` the drawers copy their label block with, answered in place with
  its bytes kept as an `lwl` / `lwr` copy's are. Nothing else of the
  Allegrex was needed; anything else stops the run as an unknown opcode.
  `MipsRun` itself changed only in naming its fetch and its executable test
  and in honouring a "nullify" that only the subclass returns.
- `GridBoot`, the reader's module over `BOOT.BIN` (`GridEmi` is the same
  over an overlay, the PlayStation path as it was): `.text`'s functions by
  the overlays' rule, shapes by `allegrex_shape_hash` (`mips_shape_hash`
  with every `lui` immediate masked too, since link offsets have small high
  halves), and the PSP's tables: two drawers, two input steps, two legends,
  the tile helper. Each hash occurs once in `.text`, the helper's seven
  times.
- The parts are **run, not read at offsets**: the drawers and legends run as
  the PlayStation's do, and every offset 4c-PK lists came out of the run:
  grid `0x2EC7A8` / `0x2EF1C9`, labels `0x2ECB18` / `0x2EF78F` (20 bytes,
  read by the copy), legend tables `0x2ECB44` / `0x2EF6FC`; drawers
  `0xC3CD0` / `0x11645C`, legends `0xC3FD0` / `0x1149BC`, input steps
  `0xC36C0` / `0x116BE0`.
- The page byte: every call of the drawer passes it, and each function is
  walked in order with what its registers hold (`grid_track`), because the
  PSP's code keeps a `lui` base in a saved register for a whole function.
  `START`'s is `.bss` (link `0x8261B`, loaded `0x3F5DBB`), one store, the
  constant 2. **`COMMU02`'s (`0x2EF1C8`) is stored after all**, against
  4c-PK's "no store": two `sb` of the constant 2 (`0x112508`, `0x11356C`)
  through `$s7`, whose `lui` is 82 words up the function, past the 40-word
  window PK's scan used. So the rule is the PlayStation's unchanged: every
  store one constant. A byte store at the cell's low half through a base the
  walk does not know, or a wider store over the cell, refuses (none found).

**Against `psx-us`, part by part** (the two chunks decoded; the converter
called on each opened disc): version, family, page count, first page, all 96
cells and the code map **equal**; both copies' label blocks (slots, draw
count, room, bytes) and legend boxes **equal**; the header's build id
differs (`psp-eu`, `psx-us`). The legends: the same 18 and 13 operations
and the same room; entries 0..4 and 7 equal, entries 5 and 6 differ (one
character each on the PSP and named by an operation; empty on the US disc
and named by none). In each copy exactly **three operations differ**, rows
1..3 of the button column: the PSP's kinds are 3, 1, 1 where the US's are 3,
3, 3. Row 1 is a tile of another u (the one the US draws on row 2); rows 2
and 3 are op 1, text, of entry 6 and entry 5, count 1, colour 0, at the
button column's place (dx `0xE`, dy `0x16` and `0x1E`). 5a expresses
them as they stand; nothing was added to 5a's ops.

**What BT's routine must know.** In both `psp-eu` copies the six button
pictures are not all op 3: rows 0 and 1 are op 3 (dx `0xE`, dy 6 and `0xE`),
rows 2 and 3 op 1 (dx `0xE`, dy `0x16` and `0x1E`, one letter), row 4's two
op 3 (dx `0xA` and `0x12`, dy `0x26`). A routine that finds the button
pictures as the op 3s after the button page's op 5 finds four on `psp-eu`
and six elsewhere; to draw every row's button from the mapping it has to
take the column by place (dx `0xE`, dy 6 + 8i for rows 0..3, and the two
at dy `0x26`) whatever the op's kind, and replace an op 1 there as it does an
op 3. The rows' labels (entries 0..3 at dx `0x2A`) are the US strings,
entry for entry, so each row names what the US's row names.

**`COMMU02`'s input step is the same third shape as `START`'s** (read to its
second `jr $ra`; `0x116BE0..0x117190`): the hand on the repeated word's
`0xF00F`, confirm pressed `0x40`, backspace pressed `0x20`, the name cursor
+ 1 on the repeated `0xA` and - 1 on the repeated `0x5` with no pressed bit,
finish `0x800`, codes 1..4 the West's actions (code 3 quits: the renamer's
state byte + 1, result 0). It differs from `START`'s step only in M1's copy
differences: the finish sound (`0x104`, `START` `0x105`), no default name
for an all-blank entry, no clear of buffer byte 5, and the quit's result.

**`psp-jp`, for the record (call 10).** Its `BOOT.BIN` also holds the Japan
disc's 288-byte block, twice (image `0x2ECA60`, `0x2EF440`), and its own
input steps (`0xC38C8`, `0x116EFC`) carry the **Japan family's bits** (M2):
confirm pressed `0x20`, code 3 sets the page bit `0x10`, the page on pressed
`0x10` with the page-2 clamps, the name cursor + 1 on pressed `0x80` or the
repeated `0xA`, - 1 on the repeated `0x5`, backspace pressed `0x40`, finish
`0x800`. Read by eye from the disassembly, not run. Call 10 stands: its
chunk is its EMIs', unchanged.

**Measured.** `loc_build.py all --discs` over a PC game folder with the tree
before (`768e9f74`, archived) and after: 1,233 files each, **all
byte-identical** (no PlayStation layer changes). A disc-only `psp-eu` cache
(`importer.py build --source <psp-eu> --lang en-150 --out
D:\bof3x-runs\ng1114\pe\out`, the PSP-only install's path): its
`loc/en-150/dat/FIRST.DAT` holds one kind-22 chunk, 880 bytes, which passes
`name_grid_check` and equals the chunk the converter makes from the opened
disc. `loc_build.py selftest` (a new check, `allegrex_selfcheck`: the
interpreter on made-up code and the page-cell walk's four refusals) and
`importer.py check` pass.

**Not run:** the game (no `src/` change; whether the screen draws right on a
`psp-eu` install waits on BT's routine and the owner's look); a `psp-jp`
`BOOT.BIN` chunk (not wanted, call 10); the 64-bit build.

## 4c-RT. Group RT as run (stage B)

*Worker agent, worktree `D:\bof3x-runs\ng1114\rt2\wt`, branch `new-game/rt2`
off `new-game/round` `768e9f74`, local commits only. Root, game copy (TF's,
copied), launcher folders and captures in `D:\bof3x-runs\ng1114\rt2`
(game-derived, never committed). No state-hash chain: the machine was not
quiet.*

**The switch-off line is in one place: `tools/input_run.py`.** Every runner
(`abc_run`, the state-hash chains, `solo_harness`, the sweeps,
`quicksave_check`, `music_heard`, `eye_sheet`, `build_reuse`) launches
through it; the probes that launch directly (`close_probe`, `ini_probe`,
`key_probe`) stop at the title, and the attract demo never runs the title
flow. After the `--env` loop, `screens_off` adds `NewGameNameEntry` and
`NewGameOptions` to `BOF3X_OFF`, as M8 proposed:

- It keeps a caller's `BOF3X_OFF`, or else the launcher folder's
  `bof3x.ini` `off=` (an environment `BOF3X_OFF` replaces the ini's whole).
  A name the caller lists already, as off or as `-Name`, is left as written.
- It adds only the names the launcher folder's build knows, read as the
  NUL-ended string in its `bof3x.dll` or `bof3x.exe`: a build from before
  the round stops at start on an unknown name (`detour.cpp`), so an A/B
  against the old tip gets nothing added on the old side.
- A route about the screens says so with a header line `# screens new-game`
  (the game's parser reads it as a comment), or `--new-game-screens`.
- `CommuNameGrid` is not in the list: it is not a code name in this tree
  (CR's). If CR's grid changes a route that enters the renamer, the
  integrator adds it to `NEW_GAME_SCREENS` once the name exists.
- An environment `BOF3X_OFF` also replaces a preset's names (M8). The
  harness launches ini-less with `--no-config`; the A/Bs below show no
  difference, so the default plan adds none today.

**Measured** (32-bit build of this tree in an ini-less `L_new`; TF's
`L_base`, the `2207d9ce` build, copied to `L_base`; my game copy, `en-US`,
`BOF3X_SHOT_EVERY=60`, no slot file):

| Run | Result |
|---|---|
| `battle_commands.txt`, base against new | **67 of 67 frames identical**; the new side's log: both patches "left alone (BOF3X_OFF)"; the base side: nothing added |
| `config_screen.txt`, base against new | 23 of 23 identical |
| `config_controller.txt`, base against new | 18 of 18 identical |
| `field_menu.txt` (a `# save` route), new, the save read from the main checkout on C: with the worktree on D: | `done` at 1,454; slot 0 swapped and handed back (the case that raised before the fix below) |

Not replayed: the ten state-hash routes and the other save-loading routes
(they load slot 0 and never reach New Game; the line only adds two names
they do not reach), the self-test (the game never starts; TF ran it with
the fuzz pinning the switches), the sweeps.

**The new routes** (`tools/recipes/`, each with a header saying what it
proves and what it needs; they wait on `Game_Step` `0x66C7EA` and
`SaveMenu_FlowState` `0x6BDF86`, and reach the battle on `Game_Mode`
`0x66C7E8` == 5 as `battle_commands.txt` does, by `seek`, not by frames):

| Recipe | Language | Result |
|---|---|---|
| `newGameName.txt`: name screen, A B D, Start, Config, the opening's battle | `en-US` | `done` at 3,053; record 0 `0x757952` to `0x444241`, "named, 3 byte(s)"; battle after 184 presses |
| the same | `ja-JP` | `done` at 2,897; "named, 6 byte(s)"; battle after 171 presses |
| `newGameOriginal.txt`: no name screen, Config, the opening's battle | the original Chinese | `done` at 2,718; "no name screen here (no kind-22 chunk taken)"; battle after 167 presses |
| `newGameBlankName.txt`: Start with no letter, Config, `Game_Step` 5 | `en-US` | `done` at 738; record 0 equal before and after, "keeps its default name" |
| the same | `ja-JP` | `done` at 738; the same |
| `newGameQuit.txt`: down x5, left (the quit label on the US page), confirm, the title, New Game, the name screen again | `en-US` | `done` at 791; `Game_Step` 1 at 675, the name screen again at 770 |

`newGameQuit.txt` is for the US grid (`en-US`, `en-150`; `en-150` not run):
the Japan page puts its labels elsewhere. Not run: `fr-FR`, `de-DE`,
`en-150`, `psp-eu`, the hosted `BOF3.exe`, the wide modes. These routes'
state-hash pairs are stage C's.

**`tools/recipe_saves.py`.** `os.path.relpath` raised across drives in
`Slot0.__enter__` after the swap and the lock, so the exception left slot 0
swapped with no hand-back (shown in scratch on the old file: the recipe's
bytes in slot 0, the lock left). Every message path now goes through
`shown()` (relative when it can be, else as it is), as `input_run.py`'s
already did. `python tools/recipe_saves.py check` swaps and hands back made-up
files in a temporary game folder, with the save on another drive than the
checkout when the temporary folder is on one: passes here (C: against D:).

**The renamer's route.** Nothing added: `fairyVillage.txt` already has
`# save fairyVillage` (stage A), and that save is in the main checkout's
`tools/recipe_saves/`. CR records the renamer's route from it.

**For the integrator.** `docs/input-script.md` (section 1a or 3) and
`docs/abc-harness.md` do not yet say that every run gets the two names, or
name the `# screens new-game` header; `abc_run`'s run meta records the
environment before `input_run.py` adds the names (`input_run`'s own run-times
line has them).

## 4c-BT. Group BT as run

*Worker agent, 2026-11-14, worktree `D:\bof3x-runs\ng1114\bt\wt`, branch
`new-game/bt` off `new-game/round` `768e9f74`, local commits only. Runs,
captures and scratch scripts in `D:\bof3x-runs\ng1114\bt` (game-derived,
never committed): `s\` holds the scripts (`chunk22.py`, `page.py`,
`cmpwin.py`, `mdis.py`, `exedis.py`, `kinds.py`). Capstone over the held
discs through `loc_build.open_disc`. No state-hash chain: the machine was
not quiet.*

**Why the `START` row of the `en-US` legend showed odd tiles.** The four
row-4 tiles of the Western `START` legends (u 0..3, v `0x13`) come after the
legend's texts. The PlayStation's small text draw `0x8014FC90` commits, before
its glyphs, `0x8014FC00(0)`'s `SetDrawMode(p, 0, 0, 0x2F, 0)` (US exe, read),
so on the PlayStation those tiles sample page `0x2F`, the FIRST image
`0x1E000200` (VRAM (960, 0)), where every Western disc's `FIRST.EMI` holds a
32 x 8 word at u 0..31, v 152..159 (rendered, `page.py`; the Japan disc holds
other art there). The PC's `Text_DrawSmall` `0x516E70` commits no draw mode
(symbols.toml; its glyphs carry their own texture), so ours drew them on the
page the piece's op 5 left, `0x1D`: the UI page, whose texels at that place
are frame edges, identical in the PC's `START.DAT` and in the US, Japan and
French `START.EMI` (rendered, `uipage` window). The same cause drew the
French and German legends' six button tiles from the UI page (they set no
page of their own after the piece). **Fixed in the routine:** before a tile
or the piece, the PC is given the draw mode the PlayStation's small draw left
after a text or the leaders, where it differs from the last committed one
(`DrawLegend`, `settle`). **What is left:** the PC's `FIRST.DAT` has that
32 x 8 strip blank (its whole tile u 0..127, v 128..159 has no ink, counted),
and no layer carries an image, so on a PC install the four tiles now draw
nothing (the row reads START and the leaders). A chunk or layer change is
needed to show the word; it is PE's file (below).

**The legend's pictures follow the mapping.** Measured: the button sheet on
page `0x2F` - tile v 0, u `0x1B` circle, `0x1C` triangle, `0x1D` cross,
`0x1E` square; tile v 2, u 0..3 L1, L2, R1, R2, u 4, 5 START, u 6, 7 SELECT -
is identical in the PC's `FIRST.DAT` and in all seven held discs'
`FIRST.EMI` (`cmpwin.py`). The legend's button column is taken **by place**:
rows 0..3 at dx `0xE`, dy 6 + 8i, in every copy 5a lists and in PE's
`psp-eu` chunk, whatever the op kind (`psp-eu` draws rows 2 and 3 as
one-letter texts). Each row names one action, in the same order in both
families: confirm, erase, next, then the page (Japan) or back (West) - the
action texts beside them, and on every PlayStation disc a tile of that
action's bit in the family's step (Japan circle, cross, square, triangle;
West cross, triangle, circle, square: M2's tables against 5a's tile lists).
So no chunk field is needed. A row draws the sheet's tile of
`map(the family's bit)`, `map` the input step's own (`Sigma` for Japan,
`SigmaFrom(West)` for the West), on page `0x2F`, in the CLUT the copy draws
its shape tiles with; a button moved to a shoulder draws the sheet's L1 or
R1 label in the CLUT the copy draws START with. START is never moved by `σ`.
At a PlayStation disc's own defaults every legend draws its own tiles; on a
PC install with a Western layer (Japan's starting words, DIV-0134) the
Western legend now shows circle, cross, square, triangle, the buttons the
West's step tests there through `SigmaFrom(West)` - TF's and CR's captures
showed the disc's cross for OK while circle entered a letter. `psp-eu`'s
own tiles follow the PSP's step, which ours does not run (the West family's
step, as 4b has it), so there the row's action decides, not its tile; its
rows 2 and 3 show the pressed face buttons of next and back, the shoulders
still work as the West's.

**The starting mapping's order (call: the owner's recollection, 4b).** The
game takes the install's release in `src/input/sigma.cpp:388-413`
(`ReleaseOfSources`): Japan if any source is `psx-jp` **or the PC's own data
(`pc-zh`)** (`kPcIsJapanSource = true`, `:73`), else the PlayStation West,
else `psp-jp`, else `psp-eu`; no cache or no manifest is Japan
(`:141-151`). The sources are the cache manifest's `[[source]]` builds,
written by `tools/importer.py:1473-1474`; the launcher only passes the cache
(`src/launcher/config.cpp:620-624`) and holds no rule of its own. So the
order is Japan PlayStation and the PC first, together, then West
PlayStation, Japan PSP, English PSP. **It differs from the recollection in
one place: the PC is first, not last.** A PC install with a Western disc
(`pc-zh` + `psx-us`) starts with Japan's buttons today, where "the PC last"
would give the West's; the same for the PC with a PSP disc. The PC's
defaults are Japan's, so nothing else differs. Also not as "the Japanese
mapping when several sources are used": `psx-us` + `psp-jp` starts with
the West's. Nothing changed (DIV-0134's `kPcIsJapanSource` is the owner's
2026-10-17 word); the owner's call.

**The skip's direction (4b).** Both discs already skip an empty cell in the
direction of a move from a letter (do-while with a wrap: Japan `START`
`0x801E4614..0x801E48CC`, read whole); only Left / Right from a special
store column 9 / 0 with no test. The Japan step then clamps, after every
frame's moves, rows 4..7 of columns 0..9 up to row 3 on page 2
(`0x801E48D0..0x801E4918`), and after a page change also row 2 of columns
6..9 up to row 1. **What the owner's rule would do to the Japan family:**
the two agree everywhere but Left / Right from a special in rows 4..7 of
page 2, whose columns 0..9 are all empty: the rule would find no letter in
the row and leave the hand on the special (or its twin), where the disc
puts it on row 3, column 9 (Left) or column 0 (Right). A page change has no
direction, so the rule says nothing there. **Built:** the West goes on in
the direction of the move from a special (Left from the quit label to that
row's last letter); the Japan family keeps its disc's clamps, every move
and page change as before. Measured in a game (`en-US`, after the swap
below): the hand on Quit, Left puts it on the row's last letter.

**The cell beside the full stop (call 7's record).** On the US / EU page,
row 2 column 11, beside the full stop in column 10, is a glyph cell of code
`0xFF`: the space, which the routine also uses as the blank marker. An
empty cell is code 0 (no glyph). So on the original a confirm there enters a
blank, and on the empty cell a 0, which ends the name at that cell; both
draw nothing, which is what the owner saw.

**The wide modes.** Nothing to change: under `BOF3X_WIDE` every primitive
is shifted right by the columns and `Menu_DrawBackdrop` (ours) widens the
backdrop by `Widescreen_BackdropPairs`, as every menu. Captured at 16:9
(`en-US`) and 21:9 (`ja-JP`): the screen centred, the wood to both edges,
no band.

**The legend's piece (5b's last line).** Compared statically: START's call
`0x801AF270(0x2C, 1)` (the US twin of `Menu_PieceRect`, in `GAME.EMI`) reads
the table `0x801CCF7C`, whose entry `0x2C` is u 0, v `0xA8`, 40 x 48, the
same four bytes as the PC's `0x663C8C` entry `0x2C`; the UI page
`0x1A080200` is identical whole in the PC's `START.DAT` and in the US,
Japan and French `START.EMI`. With the disc's CLUT the sprite samples the
same texels; on the captures it is the frame behind the actor. Not compared
against a PlayStation picture (M10's captures are not taken).

**Captures** (`D:\bof3x-runs\ng1114\bt\captures\`, each
`name_screen.png`): `en-US_default_4x3`, `en-US_swapped_4x3`,
`ja-JP_default_4x3`, `ja-JP_swapped_4x3`, `en-US_default_wide1`,
`ja-JP_default_wide2`, and `en-US_swapped_moves\` (cross types a letter,
the hand on Quit, Left from it, circle erases). "Swapped" is the title's
Config, Controller, cross on the Speak line (Speak and Action trade:
words `0x903580` = `0x43`, `0x90358C` = `0x20`, peeked), then New Game.
The `ja-JP` captures use PK's `ja-JP.FIRST.DAT`; with it the finish
label's second kana draws as a white block, which GD's layer with the same
build does not (`D:\bof3x-runs\ng1114\bt\shots\ja_default_gdlayer\name_screen.png`).

**Run.** The 32-bit build. `BOF3X_SHADOW=name_grid,new_game`
(self-test only, hosted, an ini-less launcher folder): `name_grid` 145
checks, 0 mismatches; `new_game` 9, 0. The whole suite
(`BOF3X_SHADOW='*'`, self-test only, hosted, ini-less): "self-test only:
done", game exit code 0, 922 self-test lines naming mismatches, every one
0. The 64-bit preset builds (not run). `battle_commands.txt` under
`BOF3X_OFF=NewGameNameEntry,NewGameOptions`, the tip's build against this
one, `BOF3X_SHOT_EVERY=60`: 67 of 67 frames identical. **Not run:** the
64-bit build in a game; `fr-FR`, `de-DE`, `en-150`, `psp-eu` in a game (the French
fix is from the code and the ops, not seen); a disc-built install.

**Ledger text for stage C** (5b's "the hand" line, amended, and two lines
to fold into the name-entry entries):

- *The hand.* The original Western step leaves the hand on an empty cell
  after Left from row 5's special (US / EU, French); a confirm there enters
  byte 0. Ours, in the West, goes on in the direction of the move to the
  next cell that is not empty, Left from the quit label to that row's last
  letter (and Right from a special past an empty column 0, where a grid has
  one). The Japan family keeps its disc's clamps: after a move, rows 4..7 of
  columns 0..9 up to row 3 on page 2; after a page change, also row 2 of
  columns 6..9 up to row 1. Why: the owner, 2026-11-14 (call 7 settled, the
  skip's direction).
- *The legend's buttons.* The original's legend draws its release's
  default buttons as fixed tiles. Ours draws, in each row of the button
  column, the sheet's tile of the button the step tests for that row's
  action under the Config now (`Sigma` / `SigmaFrom(West)`), a shoulder as
  the sheet's L1 / R1 label; at a disc's own defaults its own tiles. On
  `psp-eu` the rows follow the West family's step, not the PSP's tiles and
  letters. Why: the owner, 2026-11-14 ("the buttons match the mappings").
- *The legend's pages.* A tile or the piece after a text is drawn on the
  page the PlayStation's small draw leaves (`0x2F`), which the PC's small
  draw does not set. Not a divergence in intent: it restores the disc's
  pages (the French and German button tiles, the Western START row). The
  Western START row's word stays blank on a PC install until the layer
  carries it.

## 4d. Stage B merged: what was verified, and what is owed (2026-11-14)

Merged one group at a time into `new-game/round`: LT, PK, TF, CR, PE, RT,
CR's follow-up (the tiled frame's fix with no switch), BT, PK's follow-up
(D18's reuse, the START row's word). One fix at the merge: `rest_4d`'s
synthetic legend passes BT's map argument. Group VF then ran the merged
tree and changed nothing, twice: at `037a316b` and at the final tip
`2068bfdd` (logs and captures under `D:of3x-runs
g1114f`).

**Verified on `2068bfdd` (VF's second pass unless said).**

- Both widths build. The whole 32-bit shadow suite: 922 result lines, all
  0 mismatches (`new_game` 9 checks, `name_grid` 145, `glyph_draw` with
  D18's reuse checked 4,965 times). The 64-bit build cannot run the shadow
  suite by design (`detour.h` `WantsShadow`); its plain self-test passed at
  `037a316b` and it was not run in play.
- With the two New Game switches off, against the build before stage B
  (`2207d9ce`): `battle_commands` 67 of 67 frames, `config_screen` 23 of
  23, `config_controller` 18 of 18. Because D18's reuse is not behind those
  switches, three text-heavy routes too: `field_menu` 31 of 31,
  `menu_screens` 34 of 34, `shop` 52 of 52 on the repeat (the first base
  run alone was one frame ahead in a typing message; four repeats agree).
- `newGameName` under `en-US` and `ja-JP` with PK's layers: the START row
  draws its word; the `ja-JP` finish label draws whole and the legend is in
  the 8-pixel kana. `newGameOriginal`, `newGameBlankName`, `newGameQuit`
  ran at `037a316b`.
- At `037a316b`: the name screen under `fr-FR`, `de-DE`, `en-150` and over
  PE's disc-only `psp-eu` cache (a name typed, the options, the opening);
  the renamer's member path by CR's poke; a quicksave taken on the name
  screen and loaded back to it; 16:9 with a swapped mapping; the tool
  checks; the launcher's host tests (the two `TestPresetPlan` failures of
  the base and no other).

**Owed, for stage C.**

- **The ledger.** Entries for the name entry at New Game, the options at
  New Game, the renamer's grid, D18's reuse (a renderer change to a Capcom
  defect, on by default, a plain flag today: whether it is a named switch is
  the owner's), the tiled frame's windows (PC-only defect, no switch); the
  START row's word in the layer and the `ja-JP` 8 x 8 glyphs (DIV-0016);
  DIV-0075 and DIV-0020 amended. The text is in the 4c sections and 5b.
  Then `ledger_registry.h` and the preset TSVs regenerated (the two
  `TestPresetPlan` failures are probably the stale TSVs), and
  `known-defects.md` for the tiled frame and D18.
- **The proof**: the state-hash routes with the switches off, on a quiet,
  hands-off machine; the new routes' own pairs.
- **Not run by anyone:** the 64-bit build in play; a PSP-only home laid out
  by the launcher's Sources page (VF ran the PSP cache over a PC game
  folder); the renamer's slot path with save and reload on the final tree,
  and any player's route into the renamer (it is entered by a harness
  poke); `tools/launcher_host/dialog_test.py`; the `psp-eu` layer rebuilt
  with the START row's word; a name carried through `save_convert.py`
  (acceptance 6); M10's reference captures.
- **Docs:** `input-script.md` and `abc-harness.md` do not describe the
  harness's default switch-off or the `# screens new-game` header;
  `launcher-settings.md` has LT's row only.
- **For the owner's eye:** the German and French labels look cut to their
  room (not compared with the discs); the hand at column 0 sits over the
  grid frame's edge; the options box LT added beside the name-entry box.
- **Open with the owner:** the starting mapping's order (`sigma.cpp`
  `ReleaseOfSources`: the PC's own data counts as a Japan source and is
  first, not last as recalled; US PlayStation with Japan PSP starts with
  the West's); call 1 for the original Chinese (built: the options screen
  is shown); the growth roll's order against the options (TF kept the
  port's).

## 5. The contracts

- **Switches** (names proposed; the registry's form): `NewGameNameEntry`
  (on where the layer has the chunk), `NewGameOptions` (call 1),
  `CommuNameGrid` (on where the layer has the chunk; off gives DIV-0075's
  end). All three in `BOF3X_OFF` and off under the `original` preset.
- **The chunk** is the only source of the grid. No fallback table in the
  source, in any language.
- **Names:** five cells in the edit buffer, `0xFF` blank inside the routine.
  The commit converts to what the layer stores (M3); a Latin layer's five
  bytes go in as they are with the field's remainder cleared.
- **The cells:** New Game's are ours and new (static, in our image); the
  renamer's are the PC's own (`0x675F98` and its neighbours, research 3).
- **Quicksave:** the name screen is a title-flow state; a snapshot taken
  there must restore it or be refused, as the other title states are
  ([`quicksave-round.md`](quicksave-round.md)). TF says which.

## 5a. The grid chunk's layout

*Group NG, 2026-11-14: the contract between GD (`tools/loc_build.py`
writes it) and NG (`src/game/menu/name_grid.cpp` reads it). Layout only: no
grid byte, label, legend string or message of either game is in this
section (rule 1). The per-build values below are code constants read with
capstone over the held discs (`D:\bof3x-runs\discs`, through
`loc_build.open_disc`; the drawers, legends and input steps at the
addresses of M2's `funcs.out`, read whole for `psx-jp`, `psx-us`, `psx-fr`
and `psx-de`, and `psx-de` against `psx-fr` instruction by instruction).*

**Where.** One chunk, **kind 22, tag 0**, in the language layer's
`FIRST.DAT`. Kind 21 is taken (DIV-0225's town warp, `dat_load.cpp`
`TakeChunk`, `tools/disc_patch_build.py` `WARP_KIND`), so 22 is the next
free. `FIRST.DAT` is walked at start and again on a return to the title; the
reader takes the chunk each time, replacing what it holds.

**No chunk** is written for a build GD cannot read whole: every part below
(the grid, the code map, both copies' labels and both copies' legends) is
found and encodes, or the build gets no kind-22 chunk and the layer's report
says why. `psp-eu`'s parts are all in `BOOT.BIN` and are read there by
the PSP's own shapes (4c-PE); its input step is the PSP's own, neither
family below, and its chunk carries family 1, the West's step, by the
owner's word on call 8 (4b: the step's buttons and the legend's pictures
follow the player's mapping on every install). There is no partial chunk.

All numbers are little-endian. "Text encoding" is the layer's encoding for
`Text_DrawAt` (`encode_text` / `encode_char`, `ja_encode` for a Japanese
disc: the bytes a name is stored in). "Small encoding" is the layer's
encoding for `Text_DrawSmall`: on a Latin layer as its other 8 x 8 strings
are (kind 7's, the 8 x 8 set at glyph `0xA00`, DIV-0016); on a Japanese
layer the Japan disc's own 8 x 8 set, each character a two-byte code of a
glyph appended to the layer's font after the pair codes (`ja_small_glyph`;
call 9, 4c-PK), and the word separator `0x20`. Kind 7's Japanese strings
still draw the 12-pixel kana at two thirds.

### The header, 0x14 bytes

| Off | Width | Field | Valid |
|---|---|---|---|
| `0x00` | 1 | version | 1 |
| `0x01` | 1 | family: 0 Japan (`psx-jp`, `psp-jp`), 1 West (`psx-us`, `psx-eu-en`, `psx-fr`, `psx-de`; `psp-eu` by call 8, 4c-PE) | 0, 1 |
| `0x02` | 1 | P, the pages in use | Japan 3, West 1 |
| `0x03` | 1 | F, the disc's number of the first page in use | Japan 0, West 2; F + P <= 3 |
| `0x04` | 1 | C, the special codes in the map | Japan 5, West 4 |
| `0x05` | 3 | zero | 0 |
| `0x08` | 12 | the build the chunk was read from, ASCII, NUL-padded (`exe_maps` id, e.g. `psx-eu-en`); for the log only | printable ASCII, then NULs |

### The grid, P x 96 x 4 bytes, at `0x14`

Pages F .. F + P - 1 in order; in each, rows 0..7, in each row columns 0..11.
The routine's page cell holds the disc's page number (F + k), as on the
PlayStation: a Western page is 2. Pages the disc carries but nothing reads
(the Western 0 and 1) are not in the chunk. One record a cell:

| Off | Field |
|---|---|
| +0 | the disc's code: what the PlayStation's buffer would hold for this cell |
| +1 | kind: 0 empty, 1 glyph of one byte, 2 glyph of two bytes, 3 special |
| +2 | the cell's first byte in text encoding (0 for kinds 0 and 3) |
| +3 | its second byte (0 for kinds 0, 1 and 3) |

Per kind: **0** code 0, both bytes 0. **1** code 6..0xFF, byte +2 in
`0x01..0x7F`. **2** code 6..0xFF, byte +2 in `0x80..0xFF`. **3** code 1..C,
the special code. A glyph's code is never below 6, so the input step's
"is this a special" test (code non-zero and below 6) and the position test
agree.

### The code map, C x 4 bytes, after the grid

Code 1 first. One record a code, the same for both copies:

| Off | Field | Valid |
|---|---|---|
| +0 | action: 1 name cursor + 1 (a limit code), 2 name cursor - 1 (a limit code), 3 next page, 4 quit, 5 finish | each of 1, 2, 4, 5 exactly once; 3 once if P > 1, never if P = 1 |
| +1 | label slot | below the copies' slot count S (with flag bit 0: slot + the last page's number below S) |
| +2 | flags: bit 0, the label is slot + the page's number (Japan code 3: its label names the page) | bit 0 only |
| +3 | the label's colour, `Text_DrawAt`'s third argument | 0..0xFF |

Reference values, read from the drawers (US `START` `0x801E6490..0x801E6784`,
French `0x801E6458..0x801E6770`, Japan `0x801E6418..0x801E675C` and research
4.1) and the input steps (US `START` `0x801E4744..0x801E4E8C`; Japan M1):

| Family | Code 1 | Code 2 | Code 3 | Code 4 | Code 5 |
|---|---|---|---|---|---|
| Japan | 1, slot 0, colour 4 | 2, slot 1, colour 4 | 3, slot 4, flag 1, colour 4 | 4, slot 2, colour 2 | 5, slot 3, colour 2 |
| West | 1, slot 0, colour 4 | 2, slot 1, colour 4 | 4, slot 2, colour 2 | 5, slot 3, colour 2 | - |

### Then the two copies, `START` (New Game) first, then `COMMU02` (the renamer)

Each copy is a label block followed by a legend block.

**The label block**, 4 + S x R bytes:

| Off | Width | Field | Valid |
|---|---|---|---|
| +0 | 1 | S, the slots | Japan 7, US / EU 4, French / German 4; the same in both copies |
| +1 | 1 | the draw count: `Text_DrawAt`'s fourth argument, characters | Japan 4, US / EU 5, French / German 7 (the disc's slot width); 1..15 |
| +2 | 1 | R, the bytes a slot takes here, its NUL included | 2..32 |
| +3 | 1 | zero | 0 |
| +4 | S x R | each slot's string in text encoding, NUL-terminated within R, NUL-filled after | every slot the code map names is non-empty |

The disc's slot is a fixed width; GD takes its bytes to the width or to a
NUL, whichever is first, and encodes them.

**The legend block**, 8 + E x R2 + N x 8 bytes:

| Off | Width | Field | Valid |
|---|---|---|---|
| +0 | 1 | the box's width: `Menu_DrawBox(x + 3, y + 3, w, h, 0, window colour)` | `START` `0x78`, `COMMU02` `0x4D` in every build read |
| +1 | 1 | the box's height | `0x2F` |
| +2 | 1 | E, the legend's entries: the disc's 8-pointer table in pointer order | 8 |
| +3 | 1 | R2, the bytes an entry takes here, its NUL included | 2..32 |
| +4 | 1 | N, the draw operations | 1..32 |
| +5 | 3 | zero | 0 |
| +8 | E x R2 | each entry in small encoding, NUL-terminated within R2 | an entry an operation names is non-empty; an entry none names may be empty |
| then | N x 8 | the operations, in the disc's draw order | below |

**An operation**, 8 bytes; (x, y) is the legend's origin, the caller's:

| +0 op | +1 | +2 | +3 | +4 | +5 | +6 | +7 | Draws |
|---|---|---|---|---|---|---|---|---|
| 1 text | dx | dy | entry | flags: bit 0, entry + the page's number | count | colour | 0 | `Text_DrawSmall(x + dx, y + dy, colour, count, entry)` |
| 2 leaders | dx | dy | entry | rows | columns | x step | y step | `Text_DrawSmall(x + dx + xs * j, y + dy + ys * i, 0, 1, entry)` for each row i and column j |
| 3 tile | dx | dy | u tile | v tile | CLUT, low | CLUT, high | 0 | an 8 x 8 sprite of texture (8u, 8v) at (x + dx, y + dy), shade `0x7F`; the CLUT is the PlayStation's `GetClut` word, `cy * 64 + cx / 16` |
| 4 piece | dx | dy | piece id | the rect query's flags | CLUT, low | CLUT, high | 0 | a sprite of the menu piece's rectangle (`Menu_PieceRect`, PSX `0x801B0270`) at (x + dx, y + dy), shade `0x80` |
| 5 draw mode | dtd | tpage, low | tpage, high | 0 | 0 | 0 | 0 | `Gpu_SetDrawMode(p, 0, dtd, tpage, 0)`, committed: the texture page the tiles and the piece after it use. A text or the leaders (ops 1, 2) also leave page `0x2F`: the PlayStation's small draw commits it before its glyphs (US `0x8014FC90`, group BT) |

Every op's entry is below E; ops 1 and 2 have a count or rows and columns
of at least 1; op 3's and op 4's CLUT has bit 15 clear (bits 6..14 are its row); op 5's dtd
is 0 or 1. **Op 5 is the amendment of 2026-11-14 (NG, step 2):** the
legends set the tiles' texture page in some copies and not in others (US
`START`'s four row-4 tiles are drawn on the piece's page, its six button
tiles on another; the French and German legends set no page before their
tiles), so the page is the disc's, an operation, not a constant of the
routine. GD writes one op 5 where the disc's legend calls `SetDrawMode`
after its box, with the tpage word the PlayStation computes (`GetTPage`'s
word; where the disc picks `0x8F` or `0x2F` by `GetGraphType`, the `0x2F`
the PC takes, as `WorldMap_DrawFrame` does). `START`'s draw mode before its
box (dtd 1, tpage `0xF`, every build) is the routine's, not an op.

Reference values (the legends read: Japan `START` `0x801E675C..0x801E6BE8`,
Japan `COMMU02` `0x801D9810..0x801D9B8C`, US `START` `0x801E6784..0x801E6C68`,
US `COMMU02` `0x801D97C8..0x801D9AC8`, French `START` `0x801E6770..0x801E6AB0`
and `COMMU02` `0x801D97F4..0x801D9A24`, German the French with another x).
Every text op is colour 0, count `0x10`; every leader op is entry 7, 5 rows,
3 columns, steps 4 and 8; "the six tiles" are, in this order, four tiles at
dx `0xE` and dy 6, `0xE`, `0x16`, `0x1E`, CLUT `0x7803`, then (4, 2) at
(`0xA`, `0x26`) and (5, 2) at (`0x12`, `0x26`), CLUT `0x7800`; "the
piece" is op 5 with dtd 0 and tpage `0x1D` (`GetTPage(0, 0, 0x340, 0x100)`),
then piece `0x2C` flags 1 at (`0x50`, 5), CLUT `0x784C`; "the button page" is
op 5 with dtd 0 and tpage `0x2F`:

| Copy | Ops, in order |
|---|---|
| Japan `START` | the piece; text entries 0, 1, 2 at (`0x2A`, 6 + 8i); text entry 3 at (`0x2A`, `0x26`); text entry 4 + page at (`0x2A`, `0x1E`); leaders at (`0x1A`, 6); the button page; the six tiles, the four with u `0x1B`, `0x1D`, `0x1E`, `0x1C`, v 0 |
| Japan `COMMU02` | the same without the piece |
| US / EU `START` | the piece; text entries 0..3 at (`0x2A`, 6 + 8i); tiles u 0..3, v `0x13` at (`0x2A` + 8k, `0x26`), CLUT `0x7800`; leaders at (`0x1A`, 6); the button page; the six tiles, the four with u `0x1D`, `0x1C`, `0x1B`, `0x1E`, v 0 |
| US / EU `COMMU02` | text entries 0..4 at (`0x2A`, 6 + 8i); leaders at (`0x1A`, 6); the button page; the six tiles as US `START`'s |
| French `START` | the piece; text entries 0..4 at (`0x1B`, 6 + 8i); the six tiles as US `START`'s; no leaders, no button page |
| French `COMMU02` | text entries 0..4 at (`0x1B`, 6 + 8i); the six tiles as US `START`'s; no button page |
| German | the French, with text dx `0x22` |
| `psp-eu` `START`, `COMMU02` | US / EU `START`'s and `COMMU02`'s, but for rows 1..3 of the six tiles' four: tile u `0x1B` at dy `0xE`, then text entry 6 at (`0xE`, `0x16`) and text entry 5 at (`0xE`, `0x1E`), each count 1, colour 0 (4c-PE) |

GD reads which shape a copy has off its own legend (M2's normalised code
hashes in `funcs.py` tell the six apart; `psp-eu`'s two by
`allegrex_shape_hash`, 4c-PE) and the text dx off the code, not off the
build's name.

**The end.** The chunk ends exactly after the `COMMU02` legend block.

### What is not in the chunk: constants of the routine

- **The input bits, keyed on the family byte** (M2's table and M6's; each
  test goes through `Sigma`, DIV-0139): Japan confirm `0x20`, next page
  `0x10`, name cursor + 1 pressed `0x80` or repeated `0xA`, name cursor - 1
  repeated `0x5`, backspace `0x40`, finish `0x800`; West confirm `0x40`, no
  page bit, name cursor + 1 pressed `0x20` or repeated `0xA`, name cursor - 1
  pressed `0x80` or repeated `0x5`, backspace `0x10`, finish `0x800`. The
  hand is the repeated word's `0xF00F` in both. `psp-eu`'s own step's bits
  (4c-PK) are not keyed: its chunk is family 1 (call 8).
- **The copy's differences** (M1's list): the quit's result and sound, the
  finish sound, the all-blank rule, buffer byte 5 cleared. The caller says
  which copy it is.
- **The `ja-JP` cap of 4 cells** (call 5) on the Japan family, and the
  field's byte room, which the caller passes (8 for a record's name, 5 for a
  faerie slot); the refusal counts each cell's bytes from its kind (1 or 2).
- **The grid box, the pitches and the hand** (research 4.1: `0x110` x `0x7B`,
  x by `0x14` or `0x1C`, y by 14, the hand's offsets): the same in every
  drawer read.
- **The legend's frame**: `Menu_DrawPieces(x, y, list, 0)` with the PC's own
  lists, `0x667148` for `START`'s and `0x653000` for `COMMU02`'s: each
  disc's list is byte-identical in all four builds read and occurs once in
  `BOF3.exe`'s `.data`, at those addresses (hashed, 53 and 34 pieces).
- **No message.** `0xBA`, `0xBB`, `0xF4`, `0xF5`, `0xF7` are in every
  layer's pools (M4).

### What makes a chunk pass

`NameGrid_Available()` is true only after a kind-22 chunk has been taken and
every rule above holds, plus these on the grid as a whole:

- the cells in columns 10 and 11 of rows 3..7 are each special or empty, and
  the two of a row are the same record; every other cell is a glyph or empty;
- each page in use holds every code of the map in exactly one row (its
  columns 10 and 11), and no other special code;
- row 0 of each page has no empty cell (so the clamp "up to the first cell
  that is not empty" ends, and the hand's start, row 0 column 0, is a glyph);
- one disc code has one encoding wherever it is, and two codes never share
  one (the reverse, stored bytes to cells, is then exact: M3);
- a glyph of code `0xFF` exists (the space, which is also the blank marker:
  M3).

A chunk that is present and fails a rule is a Fatal at the take, naming the
rule: it is a fault of our builder, not the player's disc. No chunk leaves
`NameGrid_Available()` false, and a call of the draw or the input step then
aborts loudly (rule 4).

## 5b. NG's routine, built (stage A)

*Group NG, 2026-11-14, on `new-game/ng`. Nothing calls it yet; nothing a
player sees changes.*

**Where.** `src/game/menu/name_grid.h` / `.cpp`, the self-test
`name_grid_fuzz.cpp` (`BOF3X_SHADOW=name_grid`), `dat_load.cpp`'s kind 22.
The chunk's layout is known in two places only: section 5a and
`ParseChunk`.

**For stage B (TF, CR, BT).**

- The caller owns a `name_grid::Cells` block (buffer of six, name cursor,
  column, row, page) and opens it as the PlayStation's entry does: cursor,
  column and row 0, the page `NameGrid_FirstPage()`, the buffer 0xFF or
  `NameGrid_Decode` of the stored name. The hand must start on a cell that
  is not empty (row 0 always is); the step stops with a Fatal otherwise.
- `NameGrid_Input(copy, cells, room, fallback)` returns bits: `kFinished`,
  `kQuit`. For `kNewGame` both can come in one frame (START's quit does not
  leave the step); TF does the rest of each (`Game_Step` 1 and sub-step 0;
  the commit to record 0 with `NameGrid_Encode`, `Transition_Start(0)`, the
  next sub-step). `fallback` is record 0's name as cells, for the all-blank
  finish. For `kRenamer`, `kQuit` is the unanswered end; CR advances.
- `room` is the field's bytes: 8 for a record's name, 5 for a slot. A
  glyph that would pass it is refused with `0x107`. `NameGrid_CellCount()`
  is 4 for the Japan family (call 5), 5 for the West.
- The draws are `NameGrid_Draw(copy, cells, x, y)` and
  `NameGrid_DrawLegend(copy, page, x, y)`; the caller draws first in
  `kNewGame`, as START does. The name panel and its underline are the
  caller's (M1's shape).
- **The legend frames' lists are the PC's own** (`0x667148`, `0x653000`,
  section 5a). The take checks both by length and SHA-256 in the running
  image. When either is missing (a disc-built or exe-less image),
  `NameGrid_Available()` is false and the reason is logged once and given
  by `NameGrid_Why()`: the caller skips the screen as it does with no
  chunk. Chosen over a Fatal so that such an install still plays.

**Call 7, read again.** US `START`'s Left (`0x801E48EC`) and Right
(`0x801E49B4`) and US `COMMU02`'s (`0x801DA2C8`) read the cell under the
hand before the move; when it is a special (non-zero, below 6) they store
column 9 (Left) or 0 (Right) and jump past the other moves with no test of
the new cell. No other code in `START` writes the column or the row (every
store to `0x801ED7E0` / `0x801ED7E4`: the open at `0x801E46AC` / `B4` and
the four moves), and the step has no clamp. **The first reading missed
nothing that the code shows:** on the US / EU page and the French page,
row 5's column 9 is empty, so Left from row 5's special (the quit label)
lands the hand on it, and a confirm there stores byte 0. The owner's
observation and the code disagree at that one move. Ours never lands on an
empty cell: after every move and every page change the hand goes up to the
first cell that is not empty (Japan's page-2 constants are this rule), so
that Left goes to row 4's column 9. A one-press check on the sibling's US
build (the hand on row 5's special, then Left) would settle whether the
original does it. *(Superseded by 4b and 4c-BT: the owner's press settled
it for the code, and the West now goes on in the direction of the move -
Left from the quit label to row 5's last letter; the Japan family keeps
the clamp above.)*

**The ledger text for stage C** (to fold into the entries for the name
entry at New Game and the renamer's grid):

- *The hand.* As amended in 4c-BT: in the West the hand goes on in the
  direction of the move to the next cell that is not empty; the Japan
  family keeps its disc's page-2 clamps.
- *The field's room.* The original enters any cell; its names are one byte
  a cell. Ours refuses, with `0x107`, a cell whose entry would make the
  name's stored bytes pass the field (8 for a character, 5 for a faerie
  slot). Why: the layer stores some cells in two bytes (M3).
- *Four kana.* The original Japan name has five cells; ours four under the
  Japan family. Why: call 5.
- *The buttons.* Every face and shoulder test through `Sigma`
  (DIV-0139's rule): Japan's bits by `Sigma`, the West's by
  `SigmaFrom(West)`. At the install's starting Config the original's.
- *The pad word.* The page and finish codes take effect in the same frame
  as on the PlayStation, by a flag of ours rather than a write of the bit
  into `Input_Pressed`. Nothing else in the frame reads the bit.
- *The legend's piece.* The menu piece SPRT of `START`'s legend is drawn
  with the PC's `Menu_PieceRect` rectangle and the disc's CLUT; the
  rectangle and the UI page are the US disc's byte for byte (4c-BT, static);
  not compared against a PlayStation picture.
- *The legend's buttons and pages.* 4c-BT's two lines.

## 6. Acceptance

1. With the three switches off: every existing gate identical (self-tests,
   the ten state-hash routes, the fuzz).
2. `en-US`, `fr-FR`, `de-DE`, `ja-JP` (and each other held language with a
   grid): New Game shows that disc's grid; a five-cell name is entered,
   shown in the game, saved and loaded.
3. The original Chinese: no name screen; the options step per call 1.
4. A blank name gives the layer's default name (DIV-0020).
5. The renamer: a slot and a member renamed and kept through a save.
6. A PC save with an entered name converts to PlayStation form with the name
   kept, where the language's bytes allow it (`save_convert.py`; M3 says
   which languages).
7. Both widths, 4:3 and the wide modes.
8. The owner's play of all of it.

## 7. Traps already visible

1. **Every route that presses New Game changes** when the default is on.
   The harness's launcher dir gets the switches off unless a route is about
   the new screens (the pattern of
   `self-tests-need-a-clean-launcher-dir`: the play ini must not leak into a
   gate).
2. **The sibling's `STRING_TRANSLATION.md` name-entry notes are another
   game's** (research section 6). None of its addresses applies.
3. **`pairs_propagated.json` pairs the legend with `0x585DC0`, and that is
   wrong** (research 4.2).
4. **The input step writes the pad word back** for special codes 3 and 5.
   Ours does the same through the PC's word `0x7E1BEC` or keeps the two as
   local flags; either way the page change and the finish happen in the same
   frame as the press, as on the PlayStation.
5. **The Japan shape's clamps are constants in the PlayStation code.** If M2
   finds another shape on a Latin disc, the clamps come from the chunk's
   holes, not from the constants.
6. **No gameplay fact from memory.** What the screens say and which actor
   stands on the name screen come from the disc and from the owner.

## 8. Not in this round

- The title's third row as a custom menu of toggles (IDEAS I65, the owner's
  intention, A2).
- The champion screen (IDEAS I56); TF leaves the step list it will join.
- Typing a name on the keyboard (call 3).
- A grid for the original Chinese, or any grid not on a disc.
- Names longer than five (A4).
- Naming anyone but record 0 at New Game (A5).

## 9. The brief, per group

Each brief is this file's section for the group, plus:

- Read `CLAUDE.md`, [`STATUS.md`](STATUS.md), the research
  ([`name-entry-restoration.md`](name-entry-restoration.md)) and section 3a
  of this file once M has written it.
- The harness root is the group's own, under
  `C:\Users\kerok\bof3x-runs\ng1112\<group>` (prepared by the group, never
  another's); the game's captures and anything read off a disc stay there.
- Reimplement from the description. Vendor nothing (rule 5). An address is
  never changed without knowing what it points at (rule 3).
- A divergence a group makes is written for the ledger in its report;
  stage C numbers it.
- Commit with `git commit -s`. Push nothing; the PR is opened at the owner's
  word.
