# Group R2D: the masters' screen, and the field menu's Status and Items screens

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, on the
round branch's tip `fa583bc`. **51 functions ours** (`src/game/rest_2d.cpp`,
declarations in `src/game/rest_2d.h`, the raw addresses and cells in
`src/game/rest_2d_callees.h`, shadow name `rest_2d`): the cut's 51 rows for
R2D (`analysis/round14_cut.tsv`), each read to its last instruction with
capstone and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
306,000 rounds, 0 mismatches. 118 controls planted one at a time: 117 refused, 1 equivalent mutant not refused with its near variant refused (section 6). No recorded trace enters any
of the 51 (section 9). No ledger entry is needed.

The band is two things, each one thing:

- **`0x5869A0..0x587732`: the screen a master opens** - the machine on the
  bytes `0x9398CF` (its state; R2C's dispatcher `0x586670` jumps on it through
  `0x66450C`) and `0x9398D1` (the state's step), with the member cursor
  `0x9398CE` and the yes / no answer `0x9398D2`. The master is the field mode
  tail's argument byte `0x9039F5` (the tail's step `0x9039F4`). The cut's
  `unit` said SHISU; the code is the PC's own.
- **`0x589E00..0x58B1CD`: the field menu** (`FieldMenu_States`, the menu block
  `0x929F00`): the top bar's countdown into a screen and its camp check, the
  **Status** screen (`FieldMenu_States[6]`), the **Items** screen's first five
  states (`FieldMenu_States[2]`), and the **field abilities' effects**
  (`FieldAbility_Use` and the ten handlers of its table, called by R2E's
  Ability screen). The cut's labels ("Boot: field, map and sprites", 18
  `hypothesis` rows) were the hosts' guesses: every one is menu code.

No start is a case, a shared tail or data; none was dropped or added; the
tool's extents (`band_rows.py --byte-tables`) are right for all 51, the cut's
sizes are padding past them (39 of them). Four PSX twins are paired
(`pairs_propagated.json`, `0x586B90` `0x801D1CC8`, `0x586D20` `0x801D10AC`,
`0x5873C0` `0x801D3A2C`, all in overlay `7041d004`); the sibling names none of
them, so every name is from what the PC code does and the table that reaches
it. What the masters, abilities and item categories are in play is not read
here; the names say what the code does.

## 1. What each function does

All cdecl. "The master" is byte `0x9039F5`; "message + k" is
`Msg_OpenScript(word [0x6644E8 + 2 * master] + k)` - the master's message set,
the table read in place by the unbounded byte. "Once no message is open" is
`Field_Request != 2`; opening a message sets `Field_Request = 2`. "Window n" is
`WindowRecords` (`0x803160`) record n, `0x24` bytes: `+0` on, `+1` / `+2`
kind, `+3` state, `+4` / `+6` x / y words, `+0xA`.. per kind.

### 1.1 The masters' screen (21)

| Address | Name | What |
|---|---|---|
| `0x5869A0` | `MasterScreen_PickMember(message, apprentices)` | `Input_AutoRepeat(Input_Pressed & 0x5000)`. Confirm: the cursor's member taken when its record's master byte `+0x1F` equals the master **exactly when** the second argument's low byte is set (`Sound_PlayEffect(0x104)`, answer 0, step + 1), else `0x107`. Cancel: answer 1, `0x102`, `0x106`, step 4. Else `0x1000` / `0x4000` of the answer move the cursor over `Field_MemberCount`, wrapping (`0x100`). Then the screen (below) with the cursor's frame blinking. |
| `0x586B90` | `MasterScreen_DrawCursorFrame(x, y, w, h, blink, colours)` | A `LINE_F3` (x + 4, y), (x, y + 4), (x, y + h - 1) and a `LINE_F4` (x + 4, y), (x + w - 1, y), (x + w - 1, y + h - 1), (x, y + h - 1) at `Gfx_PacketNext` (the four words' low halves as floats, `fild` / `fst`), `Gfx_CommitPrim(1, 0x2C)` and `(1, 0x38)`. Colour c = `0xFF`, or with `blink` ((`Frame_Counter` bit 3 ? it : ~it) & 6) << 5 + `0x3F`, in each channel whose bit of `colours` is set (packet `+6`, `+5`, `+4` for bits 0, 1, 2). The arguments' slots are overwritten as scratch (the original's). |
| `0x586D20` | `MasterScreen_AskYesNo(message)` | `Input_AutoRepeat(Input_Pressed & 0xA000)`. Confirm: on yes (answer 0) `0x102` and step + 1, on no step - 1; then `0x104`. Cancel: answer 1, step - 1, `0x106`. Else any repeated bit flips the answer (`0x100`). The screen with the frame still, the line, and `Menu_DrawHand(0xCF + 36 * answer, 0x15, 0)`. **DIV-0027's two sites** are its line call `0x586E78` and hand call `0x586E97`: ours reads where each reaches now (section 2). |
| `0x587680` | `MasterScreen_NameToText(member)` | `CharacterRecords[member & 0xFF]`'s first nine bytes (two dwords and a byte: the name) to `Text_Records`. |
| `0x5876F0` | `MasterScreen_State6Leave` | State 6 (`0x66450C[6]`): once no message is open, `Sprite_Objects[byte 0x90384B] +1` = byte `0x66459C[master]` unless that is `0xFF`; state 0 and the tail's step + 1. |
| `0x586D00` | `MasterJoin_Step3Ask` | State 3 step 3 (R2C's `0x664548[3]`): `MasterScreen_AskYesNo(message + 0x11)`. |
| `0x586EB0` | `MasterJoin_Step5Apply` | Step 5: no - message + 4, state 6, step 0. Yes - the cursor's member's name to `Text_Records`; its record `+0x1F` = the master, `+0x88` = its level `+0xA`, `+0x89..+0x8E` = the master's six bytes of `0x664370`; message + `0xA`, step + 1. |
| `0x587010` | `MasterJoin_Step6Told` | Step 6: once no message is open, message + `0xB`, step + 1. |
| `0x587050` | `MasterJoin_Step7AllCheck` | Step 7: once no message is open - every member's record `+0x1F` the master (from the first, stopping at the first that is not): message + 7, step + 1; else step 0. |
| `0x5870E0` | `MasterJoin_Step8Close` | Step 8: once no message is open, message + 4, state 6. |
| `0x587120` | `MasterQuit_ByStep` | State 4 (`0x66450C[4]`): `jmp [MasterQuit_Steps + 4 * step]`, unchecked. |
| `0x587130` | `MasterQuit_Step2Pick` | `MasterScreen_PickMember(message + 0x10, 1)`: a member who is this master's. |
| `0x587150` | `MasterQuit_Step3Ask` | `MasterScreen_AskYesNo(message + 0x11)`. |
| `0x587170` | `MasterQuit_Step5Apply` | No - message + 4, step 0, state 6. Yes - the name to `Text_Records`, its record `+0x1F` = `0xFF` and `+0x88..+0x8E` = 0; message + `0xC`, step + 1. |
| `0x587260` | `MasterQuit_Step6Told` | Once no message is open, message + `0xD`, step + 1. |
| `0x5872A0` | `MasterQuit_Step7NoneLeftCheck` | Once no message is open - some member's record `+0x1F` still the master: step 0; none: message + 9, step + 1. |
| `0x587330` | `MasterQuit_Step8Close` | Once no message is open, message + 4, state 6. |
| `0x587370` | `MasterGrant_ByStep` | State 5 (`0x66450C[5]`): `jmp [MasterGrant_Steps + 4 * step]`, unchecked. |
| `0x587380` | `MasterGrant_Step0Open` | Message + 5, the cursor 0, step + 1. |
| `0x5873C0` | `MasterGrant_Step1Member` | Once no message is open, for the cursor's member when its record `+0x1F` is the master: g = level `+0xA` - the joining level `+0x88` (a byte). By the master - 0xB through the four-case table at `+0x2A4`: masters `0xB`, `0xD`, `0xE` at g >= 3 and bit 3 / 4 / 5 of `0x904061` clear: the bit set, the name, message + `0xE`; master `0xC` at g >= 3, story flag `0x6C` clear (`Flags_Test(0x904030, 0x6C)`) and `Inventory_Add(3, 0x16, 1)` taking: `Flags_Set`, the name, message + `0xE`. Any other master: the first of its six (level, ability) pairs at `0x6642A4 + 12 * master` with level <= g, the ability's bit of `0x904088` clear, and `AbilityList_Add(ability, member, 0, 0)` taking: the name, `Ability_Records + 0x18 * ability`'s first 16 bytes to `Text_Records + 0x20`, the bit set, message + `0xE`. Step + 1 on every path. |
| `0x5876C0` | `MasterGrant_Step2Next` | Once no message is open, the cursor + 1; at or past `Field_MemberCount` state 2; step 1. |

The states R2C holds of the same machine: `0x586670` (the state dispatcher,
`0x66450C`), its states 0..3 (`0x586680`, `0x5866D0`, `0x586750`,
`0x586970`), state 3's steps 0, 1, 2, 4 (`0x585A20`, `0x585A50`, `0x586980`,
`0x585B20`, through `0x664548`), and the panels and box the screen draws
(`0x585DC0`, `0x585BE0`, `0x586160`).

### 1.2 The field menu (30)

| Address | Name | What |
|---|---|---|
| `0x589E00` | `FieldMenu_TopBarCountdown` | `FieldMenu_TopBarSteps[1]`: `Menu_DrawBackdrop`; the timer `0x929F04` - 1, at 0 the timer 5, the mode `0x929F00` = the top bar's entry `0x929F05` + 2, `0x929F01..0x929F03` = 0 - the screen of the entry chosen. |
| `0x589FB0` | `FieldMenu_CampAllowedCell` | `AreaMap_ByteAt(word ObjTrio +0x36, +0x3A) & 0xF0` compared with `0xA0`, `0xA1`, `0xAF`, `0x91`: al 0 on a match, else 1 (L1). Called by `FieldMenu_TopBarInput`. |
| `0x58A3C0` | `FieldAbility_Use(caster, target, ability, battle)` | The caster's record - `0x802DC0 + 0x14C * caster` (the party's working copy) with `battle`'s low byte set, else `CharacterRecords` - and its AP word `+0x1A` below `Skill_ApCost(caster, ability, 0)`: al 0. Else `call [FieldAbility_Effects + 4 k](caster, target, battle)`, k by the ability byte: `0x46..0x4B` and `0xAE..0xB3` 1..6, `0x50` 7, `0x73` and `0xD7` 8, `0xA8` 9, any other 0. An answer 1 or 5 takes the cost (asked again) off the AP word. Answers the handler's al. |
| `0x58A0E0` | `FieldAbility_NotHere` | `[0]`: al 3. |
| `0x58A0F0` / `0x58A140` | `FieldAbility_HealOne20` / `_HealOne40` | `[1]` / `[2]`: `Char_HealHp(target, (word CharacterRecords[caster] +0x2A + 100) * 20 (40) / 100, battle)`; eax 1 when it took, else 4. |
| `0x58A190` | `FieldAbility_HealOneFull` | `[3]`: `Char_HealHp(target, 0, battle)` (0 heals to the maximum). |
| `0x58A1B0` / `0x58A260` | `FieldAbility_HealAll40` / `_HealAll120` | `[4]` / `[5]`: for each u8 i below `Party_Count(0)` (asked each pass), `Char_HealHp(Field_Members[i], (stat + 100) * 40 (120) / 100, battle)`, the stat re-read each pass; 1 when any took. |
| `0x58A310` / `0x58A350` | `FieldAbility_Clear80` / `_ClearA0` | `[6]` / `[8]`: `Char_ClearStatus(target, 0x80 / 0xA0, battle)`; 1 / 4. |
| `0x58A340` | `FieldAbility_NoEffect` | `[7]`: al 4. |
| `0x58A380` | `FieldAbility_HealFullClearA0` | `[9]`: `Char_HealHp(target, 0, battle)` then `Char_ClearStatus(target, 0xA0, battle)`; 1 when either took. |
| `0x58A4C0` | `FieldMenuStatus_ByState` | `FieldMenu_States[6]`: `jmp [FieldMenuStatus_States + 4 * 0x929F01]`, unchecked. |
| `0x58A4D0` | `FieldMenuStatus_Open` | `Menu_DrawBackdrop`, `FieldMenuStatus_PlaceWindows`; the cursor `0x929F06` 0, the timer 8, state + 1; `0x102`. |
| `0x58A510` | `FieldMenuStatus_SlideIn` | `TextRecord_Set(0, 8, CharacterRecords + 0xA4 * MoveScript_EffectState[0x904062[s8 cursor]])`, window 11 `+0x10` = `0x12`, `Menu_DrawBackdrop`; the timer - 1, state + 1 when it was 0. |
| `0x58A570` | `FieldMenuStatus_Choose` | `Menu_DrawBackdrop`; window 19 on at window (4 + cursor)'s place; the name record; `Input_AutoRepeat(Input_Pressed & 0x5000)`: `0x1000` cursor - 1 (negative to `Party_Count(0)` - 1), else `0x4000` + 1 (at or past the count, signed, to 0); a cursor unlike the one before `0x100` (L2). Confirm: `0x103`, `0x102`, `FieldMenuStatus_DetailWindows`, window 19 off, timer 8, state + 1. Cancel (`Input_Pressed` read again; both can run in one frame): `0x106`, `0x102`, windows 12 + i state 1, window 11 state 1, window 19 off, timer 8, state + 2. |
| `0x58A730` | `FieldMenuStatus_Detail` | `Menu_DrawBackdrop`, the name record, window 11 `+0x10` = `0x12`; with the timer set it counts down; at 0 a cancel: `0x106`, `0x102`, `FieldMenuStatus_ListWindows`, timer 8, state - 2. |
| `0x58A7D0` | `FieldMenuStatus_Close` | `Menu_DrawBackdrop`; the timer - 1, at 0 `FieldMenuStatus_ClearWindows`, windows 4 + i state 4, timer 0, windows 7..10 state 2, mode 1 and state 0 (the top bar). |
| `0x58A850` | `FieldMenuStatus_PlaceWindows` | Windows 12 + i for each member (kind 6 / 5, state 2, on, x `0x140`, y window (4 + i)'s, `+0xA` i); window 11 (6 / 9, state 2, on, (`0x14`, -20), `+0x10` 0); window 19 (6 / `0xA`, off, `+0xA` 0, `+0xB` 1). |
| `0x58A920` | `FieldMenuStatus_DetailWindows` | Windows 4 + i / 12 + i state 5 / 3 at the cursor, 7 / 5 elsewhere; windows 15, 16, 17 state 2, kind 6 / 6, 7, 8, on, at (`0x140`, `0x74`), (-100, `0x78`), (-100, `0xA8`), `+0xA` the cursor. |
| `0x58AA30` | `FieldMenuStatus_ListWindows` | 6 / 4 at the cursor, 8 / 6 elsewhere; windows 15, 16, 17 state 1. |
| `0x58AAB0` | `FieldMenuStatus_ClearWindows` | Windows 12..17 off. |
| `0x58AAE0` | `FieldMenuItems_ByState` | `FieldMenu_States[2]`: `jmp [FieldMenuItems_States + 4 * 0x929F01]`, unchecked. |
| `0x58AAF0` | `FieldMenuItems_Open` | `Menu_DrawBackdrop`, R2E's `0x58BC30` (the windows), `0x102`; state + 1, the category row's cursor `0x6BDFA0` 0, timer 5. |
| `0x58AB30` | `FieldMenuItems_SlideIn` | `Menu_DrawBackdrop`; timer - 1, at 0 timer 0, state + 1. |
| `0x58AB60` | `FieldMenuItems_Category` | The category row (`0x6BDFA0`, s8 0..3): the hand (window 21) at window 12's (x + 48 * row, y + 4), window 11 `+0x10` row + `0x13`; `Input_AutoRepeat(Input_Pressed & 0xA000)`: `0x8000` row - 1 (negative to 3) and `0x2000` + 1 (above 3 to 0), both tested; a change `0x101`. Confirm: window 12 `+0xB` the row, `0x104`; row 1 - step 0, state + 3 (`FieldMenuItems_State5ByStep`); row 3 - window 13's list `+0xA` 4 and its pick / top from `0x93989C` / `0x9398BC`, state + 7; rows 0 and 2 - window 13's pick / top from `0x939898` / `0x9398B8` by **its own list byte `+0xA`** (not the row), state + 1. Cancel: `0x106`, `0x102`, windows 11..13 state 1, the hand off, timer + 5, state + 2. |
| `0x58AD30` | `FieldMenuItems_List` | The item list of window 13's list `+0xA` (`Inventory_IdLists` / `Inventory_CountLists` by it): window 13 `+8` 3 with the row's cursor set, else 1; window 11 `+0x10` = `Item_HelpMessage(list, item)`; the hand at (x + 7, y + 13 (pick - top + 2)). `Input_AutoRepeat(Input_Pressed & 0xF00C)`: `0x8000` / `0x2000` another list (0..3 wrapping; the old one's pick and top kept in `0x939898` / `0x9398B8`, the new one's taken; window 13 `+0x10` `0x32` / `0x31`); else `0x1000` / `0x4000` the pick - / + 1 within 0..`0x7F` (leaving the nine rows shown sets window 13 `+0x12` to `0xF0` / `0x10`, the scroll); else 4 / 8 a page (the top by nine within 0..`0x77`); the pick and top kept; a moved pick `0x100`. With the scroll word 0: confirm on an empty slot `0x107`; else `0x103` - with the row's cursor set, `Item_CanUse(+8, 0, list, item)` refusing `0x107`, taking: `+0xD` the pick, `0x929F0B` 0, state + 3; with it 0, the same test, then an item whose `NameTable_Consumables` flags have bit 4 goes through `ItemUse_Dispatch(0, item, 0)` - answer 0 or 4: `0x104`, its count - 1 and the slot emptied at 0; any other `0x107` - and one without: `0x929F06` 0, `+0xD` the pick, the hand off, state + 4. Cancel: `0x106`, state - 1. |
| `0x58B130` | `FieldMenuItems_Close` | `Menu_DrawBackdrop`; timer - 1, at 0 R2E's `0x58C2A0`, windows 4 + i state 3, timer 0, windows 7..10 state 2, mode 1, state 0. |
| `0x58B1C0` | `FieldMenuItems_State5ByStep` | `FieldMenuItems_States[5]`: `jmp [FieldMenuItems_State5Steps + 4 * 0x929F02]`, unchecked. |

## 2. Calling convention, arguments, answers, the patch inside

Every function is cdecl. The state handlers and dispatchers take nothing.
Answers: `FieldMenu_CampAllowedCell`, `FieldAbility_Use`, `FieldAbility_NotHere`
and `FieldAbility_NoEffect` answer al (`ret_mask 0xFF`: the upper bytes are a
callee's leftover or the caller's); the other eight effect handlers answer a
whole eax of 1 or 4 (`ret_mask 0xFFFFFFFF`, compared whole). Arguments are
read as the originals read them: `MasterScreen_PickMember`'s message as its
low word and the flag as a byte; `MasterScreen_DrawCursorFrame`'s four words'
low halves and two bytes; `MasterScreen_NameToText`'s byte;
`FieldAbility_Use`'s caster and battle as bytes, the ability as a byte (passed
whole to `Skill_ApCost` and the handler, as the original passes them).

**DIV-0027 (amended 2026-10-03) re-aims two calls inside
`MasterScreen_AskYesNo`** under a Latin overlay (`yes_no_layout.cpp`:
`0x586E78` to `MasterAsk_Line`, `0x586E97` to `MasterAsk_Hand`). Once ours
replaces the function, Capcom's body no longer runs; ours reads each site's
`E8` and its rel32 at the call and calls where it reaches now - the original
callee by name (through the harness), else the address the patch put there.
So the patch, its switch `BOF3X_ORIGINAL=MasterAskLayout`, and the language
behave exactly as they did in Capcom's body; a site that is no longer an `E8`
aborts. `YesNoLayout_Inject` runs before this group's self-test; when it has
re-aimed the two sites (a self-test run under a Latin `BOF3X_LANG`), the copy
of `0x586D20` would carry the re-aimed calls, so the fuzz leaves that one clone
out and says so in the log - the headless default (no language) runs it.
No other patch names a byte in the band: `DIVERGENCE.md`, `cheats.cpp`,
`widescreen.cpp`, `labels.cpp` (grepped for every address of the 51). No
function draws a full-frame fill (DIV-0041 untouched).

## 3. The state tables

Named as `[[data]]` entries (`ctype = "unsigned long"`), each count its
reader's reach read by hand (every reader is an unbounded
`jmp` / `call [table + 4 * byte]`):

| Table | Count | Reader, by | Entries | Why the count |
|---|--:|---|---|---|
| `FieldAbility_Effects` `0x6672EC` | 10 | `FieldAbility_Use`, its own mapping | the ten `FieldAbility_*` handlers | the mapping gives 0..9; `FieldMenuStatus_States` follows |
| `FieldMenuStatus_States` `0x667314` | 5 | `FieldMenuStatus_ByState`, `0x929F01` | Open, SlideIn, Choose, Detail, Close | its states write 0..4 only; the next table follows |
| `FieldMenuItems_States` `0x667328` | 11 | `FieldMenuItems_ByState`, `0x929F01` | Open, SlideIn, Category, List, Close, State5ByStep, R2E's `0x58B750`, `0x58B8A0`, `BareRet`, R2E's `0x58BA40`, `FieldMenu_BackdropStep` | the run of code pointers to the next table (`0x667354`, its own reader's); Category reaches 9 (+ 7 from 2) |
| `FieldMenuItems_State5Steps` `0x667354` | 3 | `FieldMenuItems_State5ByStep`, `0x929F02` | R2E's `0x58B1D0`, `0x58B330`, `0x58B4B0` | data (`0x667360`) follows |
| `MasterQuit_Steps` `0x66456C` | 9 | `MasterQuit_ByStep`, `0x9398D1` | R2C's `0x585A20`, `0x585A50`, Step2Pick, Step3Ask, R2C's `0x585B20`, Step5Apply, Step6Told, Step7NoneLeftCheck, Step8Close | the next table, `0x664590`, is `MasterGrant_ByStep`'s |
| `MasterGrant_Steps` `0x664590` | 3 | `MasterGrant_ByStep`, `0x9398D1` | Step0Open, Step1Member, Step2Next | byte data (`0x66459C`) follows |

The tables R2C's dispatchers read and that hold this group's functions -
`0x66450C` (by `0x586670`: entries 4, 5, 6 are `MasterQuit_ByStep`,
`MasterGrant_ByStep`, `MasterScreen_State6Leave`) and `0x664548` (by
`0x586970`: entries 3, 5..8 are `MasterJoin_*`) - are R2C's to name; not named
here so that no address is bound twice at the merge. `band_rows.py` printed
counts that run on into the next table (12, 19, 14 and 29); the counts above
are the readers' own.

## 4. The fuzz (`rest_2d_fuzz.cpp`)

51 clones, the tool's rows with each call site checked; one jump table moved
into its copy (`MasterGrant_Step1Member`'s four cases at `+0x2A4`). Shapes:
the masters' handlers `kState`, the field menu's states and dispatchers `kMenu`
(`menu_span` 11), the helpers `kCall` with their arguments set by `Args`.
Field mode; **6,000 rounds a function**. The five state tables are
`DataTable`s (recorders while the fuzz runs). **`FieldAbility_Effects` holds
ten typed stand-ins of the fuzz file's while the fuzz runs** (each logs
`(caster, target, battle)` whole under its handler's address, answers an al
of 1, 5, 4, 3, 0, 2 or any under garbage) and is put back after: a handler
recorder logs no arguments.

Every callee is re-listed with the width it reads (the originals push whole
registers whose upper bytes ours cannot hold - `Msg_OpenScript`'s word over
`mov cx, word`, `Menu_DrawHand`'s x over Text_DrawAt's answer, `Menu_DrawBackdrop`'s
and the box's style bytes, the members' bytes to R2C's panels, the 16-bit cells
to `AreaMap_ByteAt`). Answers around what the code tests: `Input_AutoRepeat`
none a third of the time, else one or two of `0x8000`, `0x4000`, `0x2000`,
`0x1000`, 8, 4; `AreaMap_ByteAt` the camp check's codes and their neighbours;
`Skill_ApCost` around the seeded AP words; `ItemUse_Dispatch` 0 and 4 against
the others; `Party_Count` 0..3; the item and status callees flags.

Regions beyond field mode's standard ones: `WindowRecords` (22 x `0x24`),
`CharacterRecords` past the style region, `0x939880..0x9398DF` (the lists'
picks and tops, the masters' cells), `Text_Records`' first `0x30`, the tail
bytes `0x9039F0..0x9039F7`, `0x6BDFA0`, the inventory's lists
`0x904160..0x90455F`, the confirm / cancel words. **Seeds:** the buttons (one
confirm bit, one cancel bit, the pressed word either, both or neither); the
master among `0xB..0xE`, 3, 5, 9 (masters whose leave byte is not `0xFF`) and
any; each record's master byte the master's or not, the level `+0xA` 0..9
above the joining level `+0x88`, the AP and stat words at their limits;
`Field_Members` 0..7, the cursor 0..2, the member count 0..3; the steps and
states; `Field_Request` 2 or not; the master bits and ability bits; the menu's
timer, cursors (the Status cursor also `0xFF` and `0x80`, signed), the row 0..3
and 4 and `0xFF`; window 13's list 0..3, pick and top at the page and scroll
boundaries, the scroll word; the inventory's empty slots and counts 1 / 2. Per
function: each dispatcher's byte below its own table's count;
`MasterGrant_Step1Member` the cursor's member this master's half the time with
g around 3; `FieldMenuItems_List` half the time a confirm with the scroll
still on a consumable id; `FieldMenu_CampAllowedCell` the leader's words.
**Disturbance** (from its hash): the masters' step, answer, cursor, state,
master and records' master bytes; `Field_Request`; the member count; the
menu's timer and Status cursor; the row and window 13's list, pick, top and
scroll word; the master bits.

What the harness lacks (for the coordinator's fold): a `DataTable` whose
entries take arguments - the typed-stand-in table here is this file's own.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **L1 `FieldMenu_CampAllowedCell`'s three dead compares.** The cell byte is
  masked `& 0xF0` and then compared with `0xA0`, `0xA1`, `0xAF` and `0x91`;
  only `0xA0` can match, so cells `0xA1`, `0xAF` and the `0x9x` codes do not
  refuse the camp as the compares read. Ours keeps the mask and the four
  compares (the three cannot match). Whether play has such cells is not
  measured.
- **L2 `FieldMenuStatus_Choose`'s sound** compares the cursor read before
  (zero-extended) with the new one (sign-extended): a negative cursor sounds
  `0x100` every frame. The cursor is negative only with `Party_Count(0)` 0,
  which the menu does not reach.
- **L3 `FieldMenuItems_List` on window 13's list 4** (the key items, set by
  the row's 3): `Inventory_CountLists`' fifth pointer is 0
  (`known-defects.md` D35), so a used item there would write at the pick's
  address. Row 3 leads to state 9 (R2E's), not to this list; ours aborts with a
  message if the write is ever reached. A list byte of 5 or more reads past
  the five-pointer tables: ours aborts there.
- **Unbounded indexes** where the original writes (ours aborts with a message;
  no path in play gives such an index): the five dispatchers past their
  tables (section 3); `MasterJoin_Step5Apply` / `MasterQuit_Step5Apply` by a
  `Field_Members` byte past `CharacterRecords`' 8; `FieldAbility_Use`'s AP write
  by a caster past 8 (or 3, working records); `MasterScreen_State6Leave` by the
  counter byte `0x90384B` past `Sprite_Objects`' 30; the Status screens'
  windows 12 + i and 4 + i past the 22 records (a member count above 9). Reads
  past a table by such an index (the masters' `.data` tables by the master
  byte, `MoveScript_EffectState` by a party byte, the records the loops read)
  are made in place, as the original makes them.
- No read of memory the original never wrote reaches what is drawn or decided
  (the locals each function reads are written first): **no ledger entry is
  needed.**

## 6. Controls

`r2d/controls.py` (scratch): each plant replaces a string that occurs exactly
once in `rest_2d.cpp`, rebuilds, runs the self-test on the clone it names
(`BOF3X_R2D_ONLY`), restores the file and rebuilds. **118 planted, at least
two a function: 117 refused by a count, 1 not refused - an equivalent
mutant, its near variant refused.** A plant in a shared helper was run on
one caller (named). The counts are this worktree's (the harness's call
counts depend on the build directory).

| # | Clone | Plant | Refused (rounds of 6,000) |
|---|---|---|--:|
| C1 | MasterScreen_PickMember | the apprentice test inverted | 2,932 |
| C2 |  | cancel to step 3, not 4 | 1,471 |
| C3 |  | the cursor wraps to count - 2 | 124 |
| C4 |  | the frame not blinking | 6,000 |
| C5 |  | the panels 53 apart | 4,020 |
| C6 | MasterScreen_AskYesNo | the box's style the backdrop byte | 5,978 |
| C7 | MasterScreen_DrawCursorFrame | the blink's base 0x3E | 3,204 |
| C8 |  | the F3's second y + 3 | 5,983 |
| C9 |  | the F4's right x + w | 6,000 |
| C10 |  | the channels by bit 1 | 5,279 |
| C11 |  | the F4 committed as 0x2C | 6,000 |
| C12 | MasterScreen_AskYesNo | the answer flipped by ^ 3 | 1,042 |
| C13 |  | the hand's step 35 | 4,203 |
| C14 |  | a repeat of 0x8000 ignored | 53 |
| C15 | MasterScreen_NameToText | the name's ninth byte from +9 | 5,985 |
| C16 | MasterScreen_State6Leave | the leave byte's none 0xFE | 2,636 |
| C17 |  | the pose to +2 | 2,371 |
| C18 | MasterJoin_Step3Ask | the prompt's message + 0x12 | 6,000 |
| C19 | MasterJoin_Step5Apply | the master to +0x1E | 2,435 |
| C20 |  | the joining level from +0xB | 2,429 |
| C21 |  | five stats copied, not six | 2,426 |
| C22 |  | message + 9 | 2,438 |
| C23 |  | no: state 5 | 3,562 |
| C24 | MasterJoin_Step6Told | message + 0xC | 5,017 |
| C25 |  | the wait on Field_Request 3 | 986 |
| C26 | MasterJoin_Step7AllCheck | the loop steps two | 367 |
| C27 |  | message + 8 | 1,613 |
| C28 | MasterJoin_Step8Close | state 7 | 5,017 |
| C29 | MasterQuit_ByStep | step 3 through entry 2 | 690 |
| C30 | MasterQuit_Step2Pick | the pick of non-apprentices | 6,000 |
| C31 | MasterQuit_Step3Ask | message + 0x10 | 6,000 |
| C32 | MasterQuit_Step5Apply | the master byte 0xFE | 2,436 |
| C33 |  | +0x8E left | 2,425 |
| C34 |  | message + 0xD | 2,438 |
| C35 | MasterQuit_Step6Told | message + 0xE | 5,017 |
| C36 | MasterQuit_Step7NoneLeftCheck | the loop steps two | 446 |
| C37 |  | message + 8 | 2,375 |
| C38 | MasterQuit_Step8Close | message + 5 | 5,017 |
| C39 | MasterGrant_ByStep | step 1 through entry 0 | 2,002 |
| C40 | MasterGrant_Step0Open | the cursor 1 | 6,000 |
| C41 | MasterGrant_Step1Member | granted from g >= 2 | 177 |
| C42 |  | masters 0xD / 0xE's bits swapped | 542 |
| C43 |  | item 0x17 | 101 |
| C44 |  | a pair's level must be below g | 193 |
| C45 |  | 15 bytes of the ability's record | 1,083 |
| C46 |  | message + 0xD | 1,743 |
| C47 |  | the taught bit by & 0xF | 141 |
| C48 | MasterGrant_Step2Next | state 2 only past the count | 1,164 |
| C49 |  | step 0 | 5,017 |
| C50 | FieldMenu_TopBarCountdown | the mode the entry + 1 | 1,741 |
| C51 |  | the timer 4 | 1,741 |
| C52 | FieldMenu_CampAllowedCell | 0xB0 refused, not 0xA0 | 2,615 |
| C53 |  | z from +0x38 | 6,000 |
| C54 | FieldAbility_NotHere | al 2 | 6,000 |
| C55 | FieldAbility_HealOne20 | * 21 | 6,000 |
| C56 | FieldAbility_HealOne40 | * 41 | 6,000 |
| C57 | FieldAbility_HealOneFull | amount 1 | 6,000 |
| C58 | FieldAbility_HealAll40 | * 39 | 4,503 |
| C59 | FieldAbility_HealAll120 | * 119 | 4,503 |
| C60 | FieldAbility_HealAll40 | the answers not OR'ed | 541 |
| C61 | FieldAbility_Clear80 | mask 0x40 | 6,000 |
| C62 | FieldAbility_NoEffect | al 5 | 6,000 |
| C63 | FieldAbility_ClearA0 | mask 0xA1 | 6,000 |
| C64 | FieldAbility_HealFullClearA0 | both must take | 2,621 |
| C65 | FieldAbility_HealOne20 | the stat from +0x28 | 5,998 |
| C66 | FieldAbility_Use | AP equal to the cost refused | 117 |
| C67 |  | 0x4B to handler 0 | 56 |
| C68 |  | 0x51 to handler 7 | 179 |
| C69 |  | any other to handler 7 | 1,355 |
| C70 |  | answer 4 takes the cost, not 5 | 558 |
| C71 |  | the records swapped | 2,720 |
| C72 |  | the first cost taken, not the second | 522 |
| C73 | FieldMenuStatus_ByState | state 2 through entry 3 | 1,208 |
| C74 | FieldMenuStatus_Open | the cursor 1 | 5,979 |
| C75 | FieldMenuStatus_SlideIn | on at a timer of 1 | 2,521 |
| C76 | FieldMenuStatus_Choose | the cursor wraps to count - 2 | 476 |
| C77 |  | the sound test unsigned on both sides | 1,044 |
| C78 |  | window 11 state 2 on cancel | 2,640 |
| C79 |  | window 19's y from +4 | 5,155 |
| C80 | FieldMenuStatus_Detail | the state back one | 335 |
| C81 |  | the timer stops at 1 | 1,690 |
| C82 | FieldMenuStatus_Close | the members' windows state 3 | 1,324 |
| C83 |  | the mode 2 | 1,741 |
| C84 | FieldMenuStatus_PlaceWindows | window 11's y -19 | 6,000 |
| C85 |  | the index to +0xB | 4,532 |
| C86 |  | y from +4 | 4,532 |
| C87 | FieldMenuStatus_DetailWindows | the others' list state 4 | 4,087 |
| C88 |  | window 17's y 0xA9 | 6,000 |
| C89 | FieldMenuStatus_ListWindows | the others' list state 5 | 4,087 |
| C90 | FieldMenuStatus_DetailWindows | the cursor compared unsigned | **not refused**: equivalent - the cursor is compared with i < `Party_Count(0)` <= 3; a cursor of `0x80` and up is negative signed and 128 or more unsigned, never i either way. Near variant C90b refused |
| C90b |  | the cursor compared & 0x7F (C90's near variant) | 593 |
| C91 | FieldMenuStatus_ClearWindows | window 17's +5 cleared, not +0 | 6,000 |
| C92 | FieldMenuStatus_ListWindows | windows 15..17 state 2 | 6,000 |
| C93 | FieldMenuItems_ByState | state 4 through entry 5 | 582 |
| C94 | FieldMenuItems_Open | the row 1 | 6,000 |
| C95 | FieldMenuItems_SlideIn | the timer 1 | 1,741 |
| C96 | FieldMenuItems_Category | the title row + 0x12 | 6,000 |
| C97 |  | the hand 47 apart | 5,094 |
| C98 |  | the row wraps past 2 | 222 |
| C99 |  | row 3 to state + 6 | 610 |
| C100 |  | window 12 +0xB the row before | 1,050 |
| C101 |  | the timer + 4 | 1,468 |
| C102 |  | the top from the picks | 1,977 |
| C103 | FieldMenuItems_List | +8 = 2 | 4,080 |
| C104 |  | the hand's rows 12 apart | 5,995 |
| C105 |  | scroll at pick = top | 41 |
| C106 |  | scroll a row early | 15 |
| C107 |  | page up below 8 | 32 |
| C108 |  | page down above 0x6F | 21 |
| C109 |  | flag bit 5 | 82 |
| C110 |  | answer 5 a use, not 4 | 30 |
| C111 |  | the slot emptied at 1 | 16 |
| C112 |  | the lists wrap past 4 | 182 |
| C113 |  | 0x929F0B 1 | 656 |
| C114 |  | state + 3 | 419 |
| C115 | FieldMenuItems_Close | the members' windows state 4 | 1,324 |
| C116 | FieldMenuItems_State5ByStep | step 1 through entry 2 | 1,992 |
| C117 | MasterScreen_PickMember | the line one byte on | 6,000 |

## 7. Calls across groups

Out of the group (`band_rows.py --edges`), all raw (`rest_2d_callees.h`,
`SH_AT`) until the round's rebinding: **R2C** `0x585DC0` (the member panel,
2 sites: `MasterScreen_PickMember`, `MasterScreen_AskYesNo`), `0x585BE0` (its
label, 2), `0x586160` (the prompt's box, 2); **R2E** `0x58BC30`
(`FieldMenuItems_Open`, 1), `0x58C2A0` (`FieldMenuItems_Close`, 1). Every other
callee is ours already, called by name.

**Inbound** (from outside the group): by `E8`, R2C's `0x586980` (state 3 step
2) calls `MasterScreen_PickMember` (`0x586996`); R2E's `0x58DC40` (`0x58DEEA`)
and `0x58E070` (`0x58E201`) call `FieldAbility_Use` (both with battle 0);
`FieldMenu_TopBarInput` (ours, `menu_lists.cpp`) calls
`FieldMenu_CampAllowedCell` through `kCampCell`. Through `.data`:
`FieldMenu_States[2]` and `[6]`, `FieldMenu_TopBarSteps[1]` (ours,
`FieldMenu_Run` / `FieldMenu_TopBar`); R2C's `0x66450C[4..6]` and
`0x664548[3, 5..8]`.

## 8. The rebinding

`band_rows.py --refs` and a scan of `src/game` / `src/hook` for every address
in `0x5869A0..0x587740` and `0x589E00..0x58B1D0`:

- `menu_lists_callees.h`: `kCampCell = 0x589FB0` now
  `bof3::addr::FieldMenu_CampAllowedCell` (the value unchanged, so
  `menu_lists`' fuzz keys stand); `menu_lists_fuzz.cpp`'s case label and its
  `FieldMenu_TopBarInput` call-site row name it the same way.
- Left raw: `yes_no_layout.cpp`'s `kMasterLineCall` / `kMasterHandCall`
  (`0x586E78`, `0x586E97`) are call sites inside the original body, not
  functions - the patch's addresses, which ours reads; their comments now name
  `MasterScreen_AskYesNo`. The comments of `menu_lists.cpp` that list the
  field menu's states by address (lines 125..126, 217, 231) are left as
  history.
- No raw reference is in a file another group of this round writes. No row of
  `scenario_harness*.cpp` or `boss_harness*.cpp` names any of the 51.

## 9. The live route

The cut's `reach` column marks `0x589E00` and the ten effect handlers - their
**hosts'** reach (`FieldMenu_Run`, `FieldMenu_ReconcileParty`), an upper
bound. No call trace under `analysis/calltrace` (every `callcounts.tsv`,
scanned for the 51 addresses) entered any of them. `menu_screens.txt` walks
the field menu's screens and likely enters the top bar's countdown, the Status
and Items screens and the camp check; the masters' prompt is on the owner's
`masterAndManillo.txt` ([`yes-no-prompts.md`](yes-no-prompts.md) section 7:
the prompt seen under DIV-0027 on 2026-10-03) - neither measured here. So
**all 51 are fuzz only** in this group's evidence; the coordinator's state
hash on `menu_screens.txt` and the master route covers them after the merge.

## 10. Self-tests and the entry list

SELFTESTS

**`analysis/calltrace/entries_logic.txt`** (the main checkout's): 41 lines
appended with the extents read here. Five of the 51 already had lines with
**longer** extents (the catalog's, over this group's own hidden starts):
`00586B90 18B` (read `0x163`), `00586D20 959` (`0x181`), `00587680 B3`
(`0x35`), `0058A3C0 48F` (`0xF7`), `0058AAB0 1173` (`0x21`) - left as they
are (a second line for the same address would duplicate it), for the round's
end.

**Code in the band that no group holds**: none - between the 51 extents are
only `nop` / `int3` padding (`band_rows.py`: "0 not listed").
