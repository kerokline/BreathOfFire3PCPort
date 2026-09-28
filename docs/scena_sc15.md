# Scenario chapters 15..19: chapter 15's bank, chapter 16's object hook, the staff roll and chapters 18 and 19

**Status:** IN PROGRESS (2026-09-28) - eighty-nine functions ours
(`src/game/scena_sc15.cpp`, shadow name `scena_sc15`), fuzzed headless
through the scenario harness ([`scenario_harness.md`](scenario_harness.md))
in five `Run`s, one per chapter byte: **0 mismatches in 516,000 rounds**;
**96 controls planted, 96 refused** (94 by a count, 2 by a fault whose near variants are refused by a count); `BOF3X_SHADOW='*'` exit 0; `ledger_check` 0 errors. Fuzz
only: no recorded route plays chapters 15, 17, 18 or 19, and the attract
cycle does not reach chapter 16's slot 1 (section 8).

Group SC15 of round ten's third wave ([`takeover-queue-round10.md`](takeover-queue-round10.md)
§9; the plan [`takeover-queue-scenario.md`](takeover-queue-scenario.md) §3).

## 0. The block `0x56C130..0x56D5E0` (the tool's unit SC17) is chapter code

The plan (and [`scenario-roots.md`](scenario-roots.md) §2) had chapters
17..19 as `Scenario_NoHook` vtables with one-entry call tables, so what the
tool's 60 starts between chapter 16's block and `Scenario_Start` (`0x56D5E0`)
were was open. **They are chapters 17, 18 and 19's code, all 60 of them**,
reached through the chapters' own vtables:

- **The vtables point into the block.** `Scena17_Hooks` `0x661FD8` slot 0 is
  `0x56C130` and slot 1 `0x56CB80` (only slots 2 and 3 are the shared
  `Scenario_NoHook` / `0x43C9F0`); `Scena18_Hooks` `0x662C30` is `0x56D510`,
  `0x56D530`; `Scena19_Hooks` `0x662C58` is `0x56D550`, `0x56D5C0`. The
  frames jump through state tables in `.data` right after each vtable
  (`0x661FEC`, `0x662C44`, `0x662C6C`), and those through parts, runs and
  steps (section 2). Every one of the 60 starts is a table entry, a direct
  call (`E8` / `E9`) from another of them, or - one - a task entry pushed to
  `Task_Restart` (`0x56D3B0`, pushed at `0x56CB63`): a dword scan of the
  image and an `E8` / `E9` scan of `.text` find no reference from anywhere
  else.
- **The PSX pairs agree.** `analysis/pairs_propagated.json` pairs 48 of the
  block's starts, every one into `SCENA17.EMI`'s section (`8a1ff74a`,
  table-anchored from the vtable record `0x801F9770`: slot 0 `0x801F775C`,
  slot 1 `0x801F88C4`); the sibling's `names/scenario_records.toml` gives
  SCENA18's slots `0x801F6C04` / `0x801F6CAC` and SCENA19's `0x801F6C04` /
  `0x801F6D10`, the twins by slot of `0x56D510` / `0x56D530` and `0x56D550`
  / `0x56D5C0`. The sibling's `DATA_ISLANDS.md` has SCENA17 as the staff
  roll ([`field-modes.md`](field-modes.md) §0); the code agrees: a scrolling
  roll of 351 text lines in a font of its own (section 3.3).
- **Why the walk reached none of them.** `analysis/remaining_catalog.tsv`
  labels everything from `0x56AD80` to `0x56D5DF` "Event script" by address
  range (`how` = `range`), a label outside the scenario family the walk
  expands ([`scenario-roots.md`](scenario-roots.md) §3), so the walk stopped
  at every root there. The same label is why fourteen of chapter 15's
  twenty-eight starts were not walked (section 1).
- **Not engine code.** Nothing before `Field_ModeDispatch` `0x56D690` in the
  block is engine-side: `Scenario_Start` `0x56D5E0` (ours, title-states) is
  the first engine function after it.

So the block is taken here, in this module, with its own `Run`s (chapter
bytes 17, 18 and 19).

## 1. The bands, the roots and the starts

| Chapter | Vtable | Slot 0 | Slot 1 | Slot 2 | Slot 3 | Slot 4 | PSX record (sibling) |
|--:|---|---|---|---|---|---|---|
| 15 | `Scena15_Hooks` `0x6618A8` | `Scena15_Frame` | `Scena15_ObjectTrigger` | `Scena15_StepHook` | `Scena15_ArriveHook` | none | `0x801FE5F4`: `0x801F9CAC`, `0x801FDD80`, `0x801FE024`, `0x801FE18C` |
| 16 | `Scena16_Hooks` `0x6619E8` | `Scena16_Frame` (ours) | `Scena16_ObjectTrigger` | `0x43C9F0` | `0x43C9F0` | none | `0x801F8524`: slot 1 `0x801F8344` |
| 17 | `Scena17_Hooks` `0x661FD8` | `Scena17_Frame` | `Scena17_ObjectTrigger` | `Scenario_NoHook` | `0x43C9F0` | none | `0x801F9770`: `0x801F775C`, `0x801F88C4` |
| 18 | `Scena18_Hooks` `0x662C30` | `Scena18_Frame` | `Scena18_ObjectTrigger` | `Scenario_NoHook` | `Scenario_NoHook` | none | `0x801F6D58`: `0x801F6C04`, `0x801F6CAC` |
| 19 | `Scena19_Hooks` `0x662C58` | `Scena19_Frame` | `Scena19_ObjectTrigger` | `Scenario_NoHook` | `Scenario_NoHook` | none | `0x801F6DBC`: `0x801F6C04`, `0x801F6D10` |

The PSX twins are by slot (a hypothesis each); chapter 17's are also
pairs_propagated's. `0x43C9F0` is `xor al, al; ret`.

**The call tables** are not in these bands: chapter 15's A (`Scena15_CallA`,
four entries) and every chapter's B are CALLS' block (`0x51AB50`,
`0x51ABC0`, `0x51ABE0`, `0x51AA40`; ours, [`scena_calls.md`](scena_calls.md));
17..19's A is one entry, `ScenaCall_Party034`.

**The starts.** `tools/scenario_rows.py --unit SC15` lists 28 starts in
`0x567DC0..0x56B2A0` plus `0x537580`, `--unit SC16` chapter 16's slot 1
`0x56C080` (the rest of that block is ours since the title-states work,
`Scena16_*`, [`field-modes.md`](field-modes.md)), `--unit SC17` 60 in
`0x56C130..0x56D5E0`: **89, every one a function**, with the extents the
tool prints, each held against the group's own reading to the last
instruction. Six catalogue starts in the bands are **not functions**, and the
tool dropped them already: `0x5687A0` (case 5 of `Scena15_Run1`'s switch,
its table entry at `0x568948`), `0x56A0E0`, `0x56A580`, `0x56A770`,
`0x56AB10` (cases 6, 0x19, 0x21 and 0x31 of `Scena15_Run6`'s switch, entries
at `0x56ACB0`, `0x56ACFC`, `0x56AD1C`, `0x56AD5C`) and `0x56D240` (case 3,
the logo, of `Scena17_DrawLine`'s switch, `0x56D2A4`) - `pe_hidden.py` read
them from `.text` jump tables. None is paired by `pairs_propagated.json`. No
function is missing from the lists.

**The fourteen of chapter 15 the walk did not reach** are the "Event
script"-labelled range above: `Scena15_RandomPause` `0x56AD80` and
`Scena15_Shake` `0x56ADC0` (called only by `Scena15_Run6`),
`Scena15_SpawnMarker` `0x56ADF0` (by `Scena15_EnterArea` and
`Scena15_Run3`), the slot-1 root `Scena15_ObjectTrigger` `0x56AE80` and its
table's eight distinct handlers `0x56AEA0..0x56B010`, and the slot-2 / 3
roots `Scena15_StepHook` `0x56B030` and `Scena15_ArriveHook` `0x56B100`.

**`0x537580`**, before chapter 0's block: `Scena15_RecordWord`, the word at
`[0x7E0880] + index * 8`. Its callers are `Scena15_Run5` and `0x48A57A` /
`0x48A59D`, inside `0x48A550`, which pairs_propagated puts in SCENA15's own
section (PSX `0x801F8958`, by callers) - chapter 15's by both.

## 2. The tables

| Table | Named | Entries | Read by |
|---|---|--:|---|
| `0x6618C0` | `Scena15_Battles` | 10 records of 10 bytes | `Scena15_BattleSetup` (n), `Scena15_Run2` (effect +0xB) |
| `0x661924` | `Scena15_States` | 3 (0 is `0x5646B0`, SC13's band) | `Scena15_Frame` |
| `0x661930` | `Scena15_Runs` | 7 (0 a bare ret) | `Scena15_Run` |
| `0x661950` | `Scena15_EventOps` | three `EventOp_0x` records, two `EventOp_6x` records at `0x661988` | `Scena15_EventObjects` |
| `0x6619A8` | `Scena15_ShakeSteps` | 4 (s8) | `Scena15_Shake` |
| `0x6619AC` | `Scena15_Objects` | 12 | `Scena15_ObjectTrigger` |
| `0x6619DC` | `Scena15_TalkWho` | 5 bytes | `Scena15_ObjectTalk` |
| `0x661A1C` | `Scena16_Objects` | 1 (`0x43C9F0`) | `Scena16_ObjectTrigger` |
| `0x661A20` | `Scena17_RollLines` | 351 lines and -1; 367 read (section 7) | `Scena17_ScrollRoll` |
| `0x661FEC` | `Scena17_States` | 3 | `Scena17_Frame` |
| `0x661FF8` | `Scena17_Parts` | 4 (0 a bare ret) | `Scena17_Run` on `0x8034E3` |
| `0x662008` | `Scena17_Part1Steps` | 16 | `Scena17_Intro` |
| `0x662048` | `Scena17_Part2Steps` | 3 | `Scena17_Roll` |
| `0x662054` | `Scena17_Part3Steps` | 13 | `Scena17_Outro` |
| `0x662088` | `Scena17_Objects` | 1 (`0x43C9F0`) | `Scena17_ObjectTrigger` |
| `0x66208C` | `Scena17_GlyphWidths` | 64 bytes | `Scena17_DrawLine` |
| `0x6620CC` | `Scena17_EndSteps` | 3 | `Scena17_EndTask` on `Game_Step` |
| `0x662C44` / `0x662C50` / `0x662C54` | `Scena18_States` / `Runs` / `Objects` | 3 / 1 / 1 | chapter 18 |
| `0x662C6C` / `0x662C78` / `0x662C7C` | `Scena19_States` / `Runs` / `Objects` | 3 / 1 / 1 | chapter 19 |
| `0x6BC740` | `Scena15_Bytes` | 8 bytes, read by no other code | the shake, the kept party byte / pause count, the effect slot; the roll's line, scroll and skip |

The states and the object tables of 18 and 19 share entries: state 0 of 15,
18 and 19 is `0x5646B0` (state 1; SC13's band), state 1 of 18 and 19 is
`Scena18_EnterArea`. Each `[[data]]` is in `symbols.toml`.

## 3. The functions

Extents by recursive descent, jump tables and case-byte tables included.
The fuzz's call shape in brackets.

### 3.1 Chapter 15

| Address | Name | Bytes | Does |
|---|---|--:|---|
| `0x537580` | `Scena15_RecordWord` | 0x14 | `[0x7E0880] + (index & 0xFFFF) * 8`'s word, zero-extended (kEntry, ax compared) |
| `0x567DC0` | `Scena15_Frame` | 0xE | `jmp [Scena15_States + s8 0x8034E2 * 4]` (kSlot) |
| `0x567DD0` | `Scena15_EnterArea` | 0x73B | state 1, 69 calls: by `Game_AreaNumber` (read afresh at every test), `Cond_ByteFD` (likewise) and the chapter's flags - the camera distance in areas 0x2D / 0x98, an effect in 0x95, event battles 0..9 or a flag in 0x9E..0xA6 (0xA4 and 0xA6 with `Cond_ByteFD` not 0 end at once), the pass flags in 2 / 0xAC, 0xAD only on `Cond_ByteFD` 2 (flag 0, the party, a marker, call-table entry 3), markers / music / call-table entry 2 in 0xAE, `Effect_Objects[0]` bit 0x40 in 0xBD (row 14 flag 1), two effects and the shake in 0xC6; state 2 |
| `0x568510` | `Scena15_BattleSetup` | 0xBE | event battle n's set-up: a kind-0x89 effect remembering n, the kind-2 sprite and the party (`0x532ED0`) placed from `Scena15_Battles[n]`, run 2 at step 0 (kEntry) |
| `0x5685D0` | `Scena15_PlaceEffect` | 0x63 | an effect of a kind at three s16s with two more fields, its slot in `0x6BC743`, al 1 / 0 (kEntry, al) |
| `0x568640` | `Scena15_Run` | 0xE | `jmp [Scena15_Runs + s8 MoveScript_Var7 * 4]` |
| `0x568650` | `Scena15_Run1` | 0x326 | steps 0..7, 0x14, 0x19 (a case-byte table): area 0xBD, three messages with the hand drawn while each is open, area 0xC1 |
| `0x568980` | `Scena15_Run2` | 0xF0 | steps 0..5: the event battle started from the effect's record and its flag set |
| `0x568A70` | `Scena15_Run3` | 0x641 | 28 cases over steps 0..0x34 (a case-byte table) |
| `0x5690C0` | `Scena15_Run4` | 0x670 | steps 0..0x18: the second party set with the party byte kept, then the first, the event objects |
| `0x569730` | `Scena15_EventObjects` | 0x61 | five event objects placed in the first free sprite records (`0x57CD90`) |
| `0x5697A0` | `Scena15_Run5` | 0x5F8 | 30 cases over steps 0..0x24: the strip to grey, a restart to `Boot_Task` on a button, an object handed on |
| `0x569DA0` | `Scena15_ClutToGrey` | 0x172 | the CLUT strip (32 rows but 0xB) a step toward, or at, each colour's grey |
| `0x569F20` | `Scena15_Run6` | 0xE54 | steps 0..0x36, 118 calls, the camera shake after every step (below) |
| `0x56AD80` | `Scena15_RandomPause` | 0x34 | a pause of `((Rand % 0x26) + 0x2A) / 2` frames in `0x6BC742` |
| `0x56ADC0` | `Scena15_Shake` | 0x2E | with `0x6BC741` set, `Camera_ShiftY += Scena15_ShakeSteps[Frame_Counter & 3] << shift` |
| `0x56ADF0` | `Scena15_SpawnMarker` | 0x87 | a kind-0x9E marker on the ground at (x, z), al 1 / 0 (kEntry, al) |
| `0x56AE80` | `Scena15_ObjectTrigger` | 0x1F | slot 1: `Scena15_Objects[object +0x86](object, row)` (kObject) |
| `0x56AEA0` | `Scena15_ObjectTalk` | 0x57 | entries 0..4: a message whose id `0x42C0A0` (area 0xC0) or `0x42BA90` answers for `Scena15_TalkWho[+0x86]` |
| `0x56AF00` | `Scena15_Object05` | 0x14 | run 3 at step 0x32 |
| `0x56AF20`.. `0x56AFE0` | `Scena15_Object06`..`10` | 0x21..0x28 | a flag, a party member dropped in (not 8), the object's `+0x8A` counted |
| `0x56B010` | `Scena15_Object11` | 0x11 | the object's `+0x8A` counted |
| `0x56B030` | `Scena15_StepHook` | 0xCF | slot 2: area 0xAD starts run 3 (step 0xF or 0) on rectangles, area 0xAE run 4 on x 0x418000 and answers 1 (kHook) |
| `0x56B100` | `Scena15_ArriveHook` | 0x19F | slot 3: area 0xAE, by the first of flags 2 / 4 / 5 clear, run 3 at 0x14 / 0x1E / 0x28 on a rectangle each; all set, run 5 at 5 or 0; al always 0 (kHook) |

Every run is the chapters' usual step machine: wait on a script counter
(`0x903848`, and `0x90384B` in run 4), the timer `0x8034E6`, a file
(`File_LoadDone`), the message request, the wait word or the effect in
`0x6BC743` going out; then place effects, load files and music, drop party
members in, set flags, change the area; end with `MoveScript_Var7` and the
step 0. Run 5 ends the task with `Task_Restart(Boot_Task)` after
`0x587860` (every sound channel stopped) once a button is held (`Input_Held`
bit 11, read as a dword); run 6 ends in area 0xC7 at (0xB60000, 0x128000)
facing 7 with `Field_StatusBits |= 0x80` (`0x56D6F0`) - the area chapter
17's start changes to as well. What these runs are in the story is not read
here; the code names areas, flags, messages, files and effect kinds only.

### 3.2 Chapter 16's slot 1

`0x56C080` `Scena16_ObjectTrigger` (0x1F): `Scena16_Objects[object +0x86]
(object, row)`; the table holds one entry (`0x43C9F0`) and a 0 after it.

### 3.3 Chapter 17: the roll

The frame's states: `Scena17_Start` `0x56C140` (the CLUT strip restored, the
pass flags 0x1F, area 0xC7 facing 7, the counters 0), `Scena17_EnterArea`
`0x56C180` (in area 0xC7: the camera set, one field frame drawn by hand and a
DR_MOVE of it to (0x2C0, 0); then part 1) and `Scena17_Run` `0x56C240`, a jump
through `Scena17_Parts` on the byte `0x8034E3`:

- **Part 1, `Scena17_Intro` `0x56C250`**: `Scena17_Part1Steps[MoveScript_Var7]`,
  then the letterbox. Sixteen steps `Scena17_Intro00`..`15`
  (`0x56C270`..`0x56C670`): the kind-2 sprite moved along x while
  `Field_Kind2Hold` holds, the camera angle, distance and `MapView_Elevation`
  moved each frame, `MoveScript_FAWord` set from the ground's elevation, the
  fade drawn on and off (`Scena17_DrawFade`), track 0x96, then part 2.
- **Part 2, `Scena17_Roll` `0x56C6B0`**: stream 8 loaded, the roll drawn and
  scrolled every frame (`Scena17_ScrollRoll`) until the stream is done and
  the roll at its end, then part 3.
- **Part 3, `Scena17_Outro` `0x56C750`**: thirteen steps
  `Scena17_Outro00`..`12` (`0x56C760`..`0x56CB60`): a disc of rays growing
  and fading (`Scena17_DrawRays`), a panel (`Scena17_DrawPanel`), file 0x15C,
  a skip on `Input_Pressed` bit 0x20, script message 1, then either a restart
  to `Boot_Task` or - step 12 - the task restarted at `Scena17_EndTask`.
- **The draws**: `Scena17_DrawFade` `0x56CBA0` (a tile and two sprites of the
  frame's copy), `Scena17_DrawRays` `0x56CCF0` (GTE: 32 triangles and quads),
  `Scena17_DrawLetterbox` `0x56CFF0` (two black bands), `Scena17_DrawPanel`
  `0x56D070`, `Scena17_ScrollRoll` `0x56D110` (sixteen lines 22 pixels apart
  from `Scena17_RollLines`, a line every 22 frames, al 1 at the -1),
  `Scena17_DrawLine` `0x56D1A0` (a string: space, `!` and a digit for the
  palette, `#` and a byte for a glyph + 4, `@` for the logo, `A`..`Z`,
  anything else c - 0x47), `Scena17_DrawGlyph` `0x56D2D0` (a 16 x 16 sprite
  of the font page, CLUT `0x7B80 | palette`), `Scena17_DrawLogo` `0x56D350`
  (88 x 18).
- **After the roll**: `Scena17_EndTask` `0x56D3B0`, a task entry that never
  returns: `Game_Step` 0, the task's words cleared, the windows reset, then
  every frame `Scena17_EndSteps[Game_Step]` and a sleep.
  `Scena17_EndReturn` `0x56D3E0` sets story flag 0x92, clears flags 9..0x16
  of chapter 15's row, puts the party at area 0x8F with `Cond_ByteFA` = 15,
  loads file 0x31B a frame at a time and moves on; `Scena17_EndField`
  `0x56D4D0` runs the field's task records and the menu mode's state
  (`ShopMode_Dispatch`); `Scena17_EndRestart` `0x56D4E0` clears the counters,
  stops the sound and restarts at `Boot_Task`.
- `Scena17_ObjectTrigger` `0x56CB80`: `Scena17_Objects` (one entry, `0x43C9F0`).

### 3.4 Chapters 18 and 19

`Scena18_Frame` `0x56D510` / `Scena19_Frame` `0x56D550` (their state tables),
`Scena18_EnterArea` `0x56D560` (state 2; both chapters' state 1),
`Scena18_Run` `0x56D520` (one entry, a bare ret), `Scena19_Run` `0x56D570`
(one entry, `Scena19_Run0` `0x56D580`: on `Input_Pressed` bit 8 an effect of
kind 0x2F), the object hooks `0x56D530` / `0x56D5C0` (one bare-ret entry
each). Two chapters that do almost nothing.

## 4. The fuzz

`BOF3X_SHADOW=scena_sc15`, `scena_sc15_fuzz.cpp`: five `scenario_harness::Run`
calls under the one shadow name, `chapter` 15, 16, 17, 18 and 19 (the flag
row pointer each chapter's), 6,000 rounds a function for 15 and 17, 4,000
for 16, 18, 19.

- **The clones**: the 89, as `scenario_rows.py --clones` printed them, each
  with its call shape. **Three are not the harness's clones**, and the file
  says why: `Scena15_EnterArea` (69 call sites) and `Scena15_Run6` (118) are
  over the harness's 64, so the fuzz file copies them itself
  (`bof3::CloneOriginal`, every site re-aimed at a trampoline that calls the
  recorder the harness stands in for the callee, the jump table relocated)
  and hands the harness a six-byte `jmp [copy]` as each one's original
  ([`scena_sc12.md`](scena_sc12.md)'s way). `Scena17_EndTask` never returns,
  so both its sides are called through wrappers that `__builtin_setjmp`, the
  original's through a copy of the fuzz file's own (three sites re-aimed);
  the fuzz's `Task_Sleep` stand-in (`custom`, logging its argument)
  `__builtin_longjmp`s back after one to three sleeps, the seed's number.
- **The callees**: every one the 89 call, listed by the group - the group's
  own called directly (logged with their arguments by what each reads: the
  draws' x / y as u16, bytes as u8), the draws and the GTE (the vectors and
  matrices the GTE calls read noted by content, the pads left out, their
  outputs filled from the recorders' stream; each `Gfx_CommitPrim` notes the
  primitive it links), `Party_DropIn` a flag (run 3 loops while its al
  answers), `Scena15_RecordWord` answering 0x308 a third of the time (run 5
  compares it), `PartySet_LoadFirst` / `LoadSecond` moving the party byte or
  its kept copy half the time (run 4 reads both again after each),
  `0x57CD90` a sprite record 0..0x1D or 0xFF, the object tables' typed
  stand-in (below), and the raw addresses of section 6.
- **The `.data` tables** swapped for recorders: every state, run, part, step
  and object table of section 2. While an object hook is fuzzed its table's
  cells hold a stand-in of the entries' own type (object, row), one per
  entry, logging the entry and both arguments (SC0's `ObjectEntry`, SC11's).
- **Regions** beyond the standard 22: chapter 15 - `Scena15_Bytes`,
  `Gfx_ClutStrip` (0x4000 bytes), `Camera_ShiftY`, `0x904EF0`,
  `Field_ScriptFlags2`, `Draw_OtSlot`, `Music_Track`, `0x904152`, the
  area-change track byte `0x904CD0`, `0x904EE0`, the party byte `0x90412C`,
  `0x929F10`, `Cond_ByteFE` (26,404 bytes in 35 regions); chapter 17 -
  `Scena15_Bytes`, `Gfx_PacketNext` and a primitive buffer of the fuzz's own
  it points into, a text buffer for `Scena17_DrawLine`, `Prim_VertexScratch`,
  `Gfx_BufferIndex`, `Camera_Matrix`, `Cond_ByteFE`, `Music_Track`, the menu
  bytes `0x929F00` / `0x929F0C`, `0x929F10..13` (`Field_Kind2Hold`),
  `Game_Step`, `0x66C7DA` (10,396 bytes in 36 regions). `Scena15_RecordWord`'s
  pointer `0x7E0880` is set to a table of the fuzz's own every round (0x308 at
  every seventh record) and put back after the runs.
- **The seed**: every switch's steps and a few beside them (any step one
  round in ten), the counters at each value a step compares (run 3's, run
  4's, run 5's, run 6's lists), the timer at 1 half the time (the count-downs
  end) and at 0x1F / 0x20 / 0x21 (run 5's fade), the kept slot a record
  0..19 with its live bit random, the effect's `+0xB` 0..11, the areas each
  test names, `Cond_ByteFD` 0..3, the request 0 / 1 / 2 / 6, the wait word 0,
  the shake bytes, the kept party byte 0 / 0xFF / equal to the party byte,
  the camera distance at 0x780 either side; for chapter 17 the part, the
  counts each step compares (0x31 / 0x32, 0x44..0x46, 0x7C..0x81, 1),
  `MapView_Elevation` at 0x540 and 0xCC0 either side, `Field_Kind2Hold` 0,
  counter 3 0x14 / 0x64, `Input_Pressed` bits 0x20 / 8, `Game_Step` 0..2, the
  roll's line near its end half the time and its scroll at 0x15, a string of
  every character class; the hooks' x and z on each test's constant, one
  either side, and inside and beside each rectangle.
- **The disturbance** (the group's, and `settle` again half the time from the
  recorders' stream): chapter 15 - the area, `Cond_ByteFD`, the kept slot,
  its live bit, the party byte and its kept copy, the shake bytes, counter 0;
  chapter 17 - `Field_Kind2Hold`, `MapView_Elevation`, the part, the roll's
  line and scroll, counter 3, `Field_Kind2Z`, the skip byte. `settle` keeps
  the slot inside the twenty records (the runs write through it) and
  `Game_Step` inside its table (the end task indexes it every frame).

**Result** (2026-09-28, this worktree):

    shadow      scena_sc15 self-test: 168000 rounds over 28 functions (6000 each), 279651 calls to the stand-ins, 0 MISMATCHES; 26404 bytes of state (35 regions)
    shadow      scena_sc15 self-test: 4000 rounds over 1 functions (4000 each), 4000 calls to the stand-ins, 0 MISMATCHES; 10004 bytes of state (23 regions)
    shadow      scena_sc15 self-test: 312000 rounds over 52 functions (6000 each), 4306961 calls to the stand-ins, 0 MISMATCHES; 10396 bytes of state (36 regions)
    shadow      scena_sc15 self-test: 16000 rounds over 4 functions (4000 each), 12000 calls to the stand-ins, 0 MISMATCHES; 10004 bytes of state (23 regions)
    shadow      scena_sc15 self-test: 16000 rounds over 4 functions (4000 each), 14012 calls to the stand-ins, 0 MISMATCHES; 10004 bytes of state (23 regions)

Every callee and every table handler is reached; the thinnest are
`PartySet_LoadFirst` (4 calls: run 4's step 0xB with the timer 0, the file
in and a kept party byte), `Scena15_EventObjects` 8, `PartySet_LoadSecond`
17, `Scena15_ClutToGrey` 23, `Scena15_RecordWord` 44 - each still enough to
refuse its controls (section 5). `BOF3X_SHADOW='*'`: exit 0.

## 5. The controls

One planted bug per function (C1..C89), plus five on orders the original keeps across a call (X1..X5, two of
them the two mismatches the first fuzz run found) and near variants of the two refused by a fault, each
planted in `scena_sc15.cpp` by a script (the anchor a unique string), rebuilt, run headless
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=scena_sc15`), restored and rebuilt. **96 planted, 96 refused: 94 by a
count of mismatching rounds, 2 by a fault** - C83 and C87 (the one-entry run table called twice: the
recorder moves `MoveScript_Var7` between the calls and the second reads past the table), whose near
variants C83b / C87b (the index held across both calls) are refused by a count in every round.

| | Function | Planted | Refused (rounds) |
|---|---|---|--:|
| C1 | `Scena15_RecordWord` | record +2 | 6,000 |
| C2 | `Scena15_Frame` | the next state's entry | 6,000 |
| C3 | `Scena15_EnterArea` | area 0x2D tests flag 0xB for 0xA | 177 |
| C4 | `Scena15_BattleSetup` | effect kind 0x8A | 5,727 |
| C5 | `Scena15_PlaceEffect` | y taken whole | 5,680 |
| C6 | `Scena15_Run` | the next run's entry | 6,000 |
| C7 | `Scena15_Run1` | status bit 2 | 144 |
| C8 | `Scena15_Run2` | the battle from +8 | 744 |
| C9 | `Scena15_Run3` | step 0x1A on counter 4 | 27 |
| C10 | `Scena15_Run4` | counter 3 0x19 | 147 |
| C11 | `Scena15_EventObjects` | 6x records 0x11 apart | 5,795 |
| C12 | `Scena15_Run5` | the ground effect's z + 1 | 9 |
| C13 | `Scena15_ClutToGrey` | row 0xC skipped | 6,000 |
| C14 | `Scena15_Run6` | effect +0xC 0xB01 | 9 |
| C15 | `Scena15_RandomPause` | Rand % 0x25 | 1,400 |
| C16 | `Scena15_Shake` | redraw 3 | 2,933 |
| C17 | `Scena15_SpawnMarker` | elevation + 0x201 | 5,703 |
| C18 | `Scena15_ObjectTrigger` | entry 11 - index | 6,000 |
| C19 | `Scena15_ObjectTalk` | area 0xC1's table | 3,076 |
| C20 | `Scena15_Object05` | step 0x33 | 6,000 |
| C21 | `Scena15_Object06` | member 0xC | 6,000 |
| C22 | `Scena15_Object07` | flag 0xF | 6,000 |
| C23 | `Scena15_Object08` | a member dropped in | 6,000 |
| C24 | `Scena15_Object09` | member 0xD | 6,000 |
| C25 | `Scena15_Object10` | flag 0xE | 6,000 |
| C26 | `Scena15_Object11` | +0x8A + 2 | 6,000 |
| C27 | `Scena15_StepHook` | x 0x418001 | 39 |
| C28 | `Scena15_ArriveHook` | x at most 0x52FFFF | 22 |
| C29 | `Scena16_ObjectTrigger` | object and row swapped | 4,000 |
| C30 | `Scena17_Frame` | the next state's entry | 6,000 |
| C31 | `Scena17_Start` | facing 6 | 6,000 |
| C32 | `Scena17_EnterArea` | part 2 | 6,000 |
| C33 | `Scena17_Run` | the next part's entry | 6,000 |
| C34 | `Scena17_Intro` | the next step's entry | 6,000 |
| C35 | `Scena17_Intro00` | Cond_ByteFE 5 | 3,031 |
| C36 | `Scena17_Intro01` | 0x8034E5 = 0x5B | 1,870 |
| C37 | `Scena17_Intro02` | track played over 9 | 767 |
| C38 | `Scena17_Intro03` | angle 0xFE97 | 781 |
| C39 | `Scena17_Intro04` | the fade at count + 1 | 6,000 |
| C40 | `Scena17_Intro05` | x 0x8C0001 | 2,981 |
| C41 | `Scena17_Intro06` | turn by -4 | 6,000 |
| C42 | `Scena17_Intro07` | counter 3 0x15 | 580 |
| C43 | `Scena17_Intro08` | down on frame bit 1 | 1,486 |
| C44 | `Scena17_Intro09` | the halving floored (sar 7) | 1,603 |
| C45 | `Scena17_Intro10` | distance - 5 | 2,943 |
| C46 | `Scena17_Intro11` | clamped above 0x541 | 323 |
| C47 | `Scena17_Intro12` | counter 3 0x63 | 1,196 |
| C48 | `Scena17_Intro13` | at 0xCC0 too | 663 |
| C49 | `Scena17_Intro14` | transition 0xE | 727 |
| C50 | `Scena17_Intro15` | Cond_ByteFE 7 | 2,840 |
| C51 | `Scena17_Roll` | the next step's entry | 6,000 |
| C52 | `Scena17_Roll0` | stream 9 | 6,000 |
| C53 | `Scena17_Roll1` | the roll's end not waited for | 1,286 |
| C54 | `Scena17_Roll2` | part + 2 | 3,019 |
| C55 | `Scena17_Outro` | the next step's entry | 6,000 |
| C56 | `Scena17_Outro00` | track 0xA1 | 6,000 |
| C57 | `Scena17_Outro01` | rays at radius 1 | 6,000 |
| C58 | `Scena17_Outro02` | radius 11 times | 5,663 |
| C59 | `Scena17_Outro03` | counter 3 0x6F | 724 |
| C60 | `Scena17_Outro04` | radius 0x1F5 | 6,000 |
| C61 | `Scena17_Outro05` | radius (count + 0x33) * 10 | 2,632 |
| C62 | `Scena17_Outro06` | level 0xFE - count | 4,090 |
| C63 | `Scena17_Outro07` | file 0x15D | 1,101 |
| C64 | `Scena17_Outro08` | skip on bit 8 | 2,521 |
| C65 | `Scena17_Outro09` | faded over 0x11 | 4,518 |
| C66 | `Scena17_Outro10` | message 2 | 2,956 |
| C67 | `Scena17_Outro11` | the step test turned | 4,481 |
| C68 | `Scena17_Outro12` | part 1 | 5,725 |
| C69 | `Scena17_ObjectTrigger` | object and row swapped | 6,000 |
| C70 | `Scena17_DrawFade` | tile level * 4 | 2,903 |
| C71 | `Scena17_DrawRays` | the quad's +0x18 copied before its +8 | 6,000 |
| C72 | `Scena17_DrawLetterbox` | the lower band at 211.0 | 6,000 |
| C73 | `Scena17_DrawPanel` | CLUT 0x7B41 | 5,981 |
| C74 | `Scena17_ScrollRoll` | lines 23 apart | 6,000 |
| C75 | `Scena17_DrawLine` | a space 13 pixels | 2,327 |
| C76 | `Scena17_DrawGlyph` | palette & 0x7F | 3,022 |
| C77 | `Scena17_DrawLogo` | height 0x13 | 6,000 |
| C78 | `Scena17_EndTask` | Game_Step 1 at the start | 6,000 |
| C79 | `Scena17_EndReturn` | flag 0x16 left | 6,000 |
| C80 | `Scena17_EndField` | the two calls swapped | 6,000 |
| C81 | `Scena17_EndRestart` | counter 3 kept | 4,341 |
| C82 | `Scena18_Frame` | the next state's entry | 4,000 |
| C83 | `Scena18_Run` | the run called twice | a fault (0xC0000005) |
| C84 | `Scena18_ObjectTrigger` | object and row swapped | 4,000 |
| C85 | `Scena19_Frame` | the next state's entry | 4,000 |
| C86 | `Scena18_EnterArea` | state 1 | 4,000 |
| C87 | `Scena19_Run` | the run called twice | a fault (0xC0000005) |
| C88 | `Scena19_Run0` | kind 0x30 | 1,917 |
| C89 | `Scena19_ObjectTrigger` | object and row swapped | 4,000 |
| X1 | `Scena15_Run6` | step read after the area change | 4 |
| X2 | `Scena15_Run3` | Party_DropIn's whole eax tested | 43 |
| X3 | `Scena15_EnterArea` | the area read once | 14 |
| X4 | `Scena15_Run4` | the kept byte not read again | 2 |
| X5 | `Scena17_Intro09` | z read before the first elevation | 278 |
| C83b | `Scena18_Run` | the run called twice, one index | 4,000 |
| C87b | `Scena19_Run` | the run called twice, one index | 4,000 |

The thinnest: X4 (2 rounds; run 4 step 0xB with the timer 0, the file in and a party byte kept), X1 (4),
C12 and C14 (9 each: a deep step of runs 5 and 6), X3 (14), C28 (22), C9 (27) - each refused on the first
run. No control stood, so no seed was strengthened after the controls.

## 6. Cross-group calls

By raw address in `scena_sc15_callees.h` (the round's rebinding pass turns
them into names):

| Address | What | Owner this wave |
|---|---|---|
| `0x532ED0` | (x, z, kind): the party placed for an event battle | SX |
| `0x533E50` | (): the party's field pass | SX |
| `0x56D6F0` | (): `Field_StatusBits` \|= 0x80 | SX |
| `0x57CD90` | (): the first free `Sprite_Objects` record in al, 0xFF none | SX |
| `0x587860` | (): every sound channel stopped (`SndBuf_Stop` on each) | nobody (not in SX's list) |
| `0x5A7730` | (prim): a 16 x 16 SPRT's code 0x7C and the 0.01 depth float | nobody (the PSX library layer) |
| `0x423380` | (): a view shift on `MapView_FocusX` 0x63FF | nobody (area overlay code) |
| `0x42C0A0`, `0x42BA90` | (who): a message id by an area table | nobody (area overlay code) |

`0x5646B0` (state 0 of chapters 15, 18 and 19, SC13's band) is only a table
entry: ours reads the tables in place and calls what is there. `EventOp_6x`
and `Rand` are Capcom's and called by name; every other callee is ours
already and called by name. **No harness edit**: nothing the group takes is
in the harness's standard set.

## 7. Latent defects (Capcom's, described, kept)

- **Every dispatch table is unchecked**, as in every chapter: the frames and
  runs by an s8, the object hooks by `object +0x86`, `Scena17_EndTask` by the
  u16 `Game_Step`. Ours reads each in place and aborts where the original
  would jump to what is not code (`CodeAt`); nothing measured reaches one.
- **The effect slot byte `0x6BC743` is used unchecked.** When
  `Effect_FindFree` found no record, the slot is 0xFF and the runs index
  `Effect_Objects + 0xFF * 0x80` - 0x7F80 bytes past a pool of 0xA00: runs
  1, 3 and 6 read the live bit there, and **run 2 step 1 writes 2 to
  `0x7E9161` and run 4 step 0xB counts that byte up** (`Scena15_BattleSetup`
  and run 4 step 5 take the slot). Ours does the same.
- **`Scena17_ScrollRoll` reads past the roll.** The -1 that ends it is at
  index 351, but the window draws the fifteen lines after the top one, so at
  the end it reads indices 352..366: zeros, one more line pointer at 356 (on screen,
  y 110 less the scroll), and at 366 `Scena17_Hooks`' first word, which it
  hands to `Scena17_DrawLine` as a string - `0x56C130`'s code bytes (in this
  process the detour's `jmp`), drawn at y 309..330, below the screen.
- **`Scena17_DrawLine`'s glyph index is unchecked** against the 64 widths of
  `Scena17_GlyphWidths`: any character outside the classes above (and `#`
  with a byte past 0x3B) reads a width from `Scena17_EndSteps` and the
  strings after it - the case above does.
- **`Scena15_Battles` is indexed unchecked** by `Scena15_BattleSetup`'s n and
  by the effect's `+0xB` (ten records).
- The draws' x and y are s16s in 32-bit registers whose upper bits are
  whatever the registers held (`Scena17_DrawLine`'s x picks up the glyph
  draw's eax); only the low 16 bits are ever read. Ours computes the same low
  bits and the fuzz compares only them.

## 8. What reaches it

No recorded route plays chapters 15, 17, 18 or 19 - the catalogue's reach
columns are empty for the 89 (`remaining_catalog.tsv`; chapter 17's show
only that their host `ClutStrip_Restore` was entered) - and the attract
cycle's chapter 16 never calls slot 1 ([`field-modes.md`](field-modes.md)
§1). So this group is **fuzz only**, and none of it is on the attract path:
the frame hash is untouched. A live check wants a save in chapter 15 (the
plan's recipe saves, [`takeover-queue-scenario.md`](takeover-queue-scenario.md)
§5) - the roll and the end task follow from it.

**What nothing reached**, by the fuzz's own design: the coroutine behaviour
(`Task_Sleep` and `Task_Restart` switch tasks in the game, recorders here);
the real draws and GTE (recorded by what they are handed); the PSX
comparison call for call (the twins are by slot and pairs only).

**`entries_logic.txt`**: a line per function with the extent above
(83 added to the main checkout's file; 6 were already there exactly,
`0x568510`, `0x56AD80`, `0x56ADC0`, `0x56CBA0`, `0x56D070`, `0x56D110`;
the catalogue's run-ons for `0x537580`, `0x5685D0`, `0x569DA0`, `0x56ADF0`,
`0x56CCF0`, `0x56D1A0` and `0x56D350` corrected by the smaller extent).
