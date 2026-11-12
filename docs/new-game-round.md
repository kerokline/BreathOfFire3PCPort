# The New Game round: the name entry and the options screen back in the New Game sequence

**Status:** STAGED (2026-11-12, on `new-game/staging` off `main` `01a67500`;
IDEAS I34; nothing built, no agent launched, no game run. Section 1 is what
earlier notes measured plus one reading of our own source made today; section
3 is what nobody has measured, and runs first.)

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
  build (the next free today is DIV-0228).
- `save-interchange.md` 2 and 2a, `USER_CHECKS.md` 4, the research's status,
  `button-tests.md`'s name-entry lines, STATUS, HANDOFF, `owner-review.md`.
- The state-hash routes with the switches off (identical to the pairs
  recorded at the tip before the round), then the new routes recorded as
  their own pairs. On a quiet, hands-off machine.
- The layout against M10's captures, by eye, the owner's.

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
