# Group FS: the shop overlay's remainder and the equip screen's two choosers

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3), wave two, on the round branch's tip `61be26e`. **53 functions ours**
(`src/game/field_s.cpp`, shadow name `field_s`): the cut table's 52 rows for
FS (`analysis/round12_cut.tsv`) and `0x58CAE0`, flagged by
`tools/band_rows.py` as code no list has (section 5). Each read to its last
instruction with capstone and fuzzed through the scenario harness in field
mode ([`scenario_harness.md`](scenario_harness.md) section 7) without edits
to it: one `Run`, 212,000 rounds, 0 mismatches; 188 controls planted one at a time, 183 refused by a count, 5 equivalent mutants each with a near variant refused (section 6). Fuzz only: neither
recorded route enters any of the 53 (section 9).

| Part | Functions | Reached through |
|---|--:|---|
| The field save's confirm | 1 | `FieldSave_States[3]` (`0x663FA0`) |
| The rest sequence's first two states | 2 | `Rest_States` `0x663FB8` (named here), jumped through by `0x580300` = `ShopMode_States[10]` |
| The party formation screen | 15 | `PartyForm_States` `0x66409C` and its two step tables (named here), jumped through by `0x580A40` = `ShopMode_States[6]`; its draws and the swap by direct calls |
| ShopMode 9's two window set-ups | 2 | direct calls from `0x583370` and `0x5833E0` (nobody's) |
| The resistance shop | 11 | `ShopResist_States` `0x6641BC` (named here), jumped through by `0x5837E0` = `ShopMode_States[5]`; its draws and message run by direct calls |
| The shared ability list's screens | 20 | `SharedList_States` `0x664254` and three step / sort tables (named here), jumped through by `0x584180` = `ShopMode_States[7]`; three window-kind handlers called from `0x59BE50`, `0x59BEA0`, `0x59C260` (nobody's) |
| A menu timer step shared by two machines | 1 | `SharedList_MoveSteps[1]` and `0x6641B0[0]` (ShopMode 9's third step table) |
| The equip screen's slot and item choosers | 2 | entries 12 and 13 of `0x667380` (the field menu's Equipment states 5 and 6 through `0x66739C`, [`menu-screens.md`](menu-screens.md) section 1) |

**Every name is a hypothesis** (`symbols.toml` status `hypothesis`) from what
the code does: "rest", "party formation", "resistance shop", "shared list"
name the code's shape, not a play-tested fact. What each screen is in the game
- who offers it, what the item `0x58` of category 0 and key item `0xF` are - is
the owner's to say. The names read: the rest states play stream 0 over a
black screen as the inn's night does ([`shop_states.md`](shop_states.md)); the
formation screen swaps the party list `0x904062` with a list of the other
joined records; the resistance shop sets one bit of a record's `+0x1D`, the
byte `Char_RecalcStats` turns into resistances
([`char-stats.md`](char-stats.md) section 2), for ten zenny a level; the
shared list is the 128-byte list `AbilityList_Add` fills when asked for the
shared one ([`scena_sx.md`](scena_sx.md)).

## 1. What each function does

Every function's comment in `field_s.cpp` is the full read and each
`symbols.toml` `evidence` string cites its extent; this section is the map.
The menu block (`0x929F00`, [`menu-screens.md`](menu-screens.md)): `+0` the
mode, `+1` the state, `+2` the step, `+4` the counter.

### 1.1 The field save's confirm and the rest sequence

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `FieldSave_Confirm` | `0x57FF80` | 0x85 | the title box, message `0x9F`, the slots unlit, `Menu_YesNo`: an answer steps on (yes, sound `0x104`) or back (no, `0x106`) |
| `Rest_Begin` | `0x580310` | 0x6B | story flag `0x77` set: state 6 and script message `0xEF`; else the flag set (not in chapter `0xE` with the row's flag 5), `Transition_Start(0)`, the state up |
| `Rest_PlaceParty` | `0x580380` | 0x1D8 | after the transition: the black screen; areas `0xBB`, `0xC1` (and `0xBF` in chapter `0xE` with row flag 5) only step on; else `Music_Track` 0xFF, a place kind (1 for area `0x85`, which clears flag `0x77`; 2 for `0xBF`), the members reloaded and each placed from `0x663FD8` by kind and slot, the view reset, `Area_RunPlacement` of the area's descriptor, the state up |

### 1.2 The party formation (`PartyForm_States` `0x66409C`)

State 0 opens through `PartyForm_OpenSteps` (`0x6640AC`), 1 slides in, 2
chooses, 3 leaves through `PartyForm_LeaveSteps` (`0x6640B8`). The cells:
`0x929F08` the column (0 the party, 1 the reserve), `0x929F09` the row (s8),
`0x929F0A` / `0x929F0D` the first pick (`0x7F` none); the reserve
`0x6BC894` and its count `0x6BC897`, the flag pairs `0x6BC88C`, the refused
id `0x6BC898`.

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `PartyForm_OpenStep` | `0x580A50` | 0xE | `jmp [PartyForm_OpenSteps + 4 * step]`, unchecked |
| `PartyForm_Setup` | `0x580A60` | 0x116 | the reserve: the joined records (`+0xB` bit 0) whose id (`+9`) is not in the party list's first `Party_Count(0)`; the flag pairs; the cursor and picks reset; counter 3; `Transition_Start(2)` |
| `PartyForm_FadeWait` | `0x580B80` | 0x21 | after the transition, `Transition_Start(3)` and the step up |
| `PartyForm_OpenWait` | `0x580BB0` | 0x23 | the screen; after the transition, the state up two |
| `PartyForm_SlideIn` | `0x580BE0` | 0x35 | the backdrop, the screen at the slide's offset, three frames |
| `PartyForm_Choose` | `0x580C20` | 0x257 | the help line (message `0x60`, or the refused record's name and `0x61`); the column (the repeat's `0xA000`, with a reserve) and row (`0x1000` up, `0x4000` down) cut to the column's count; confirm picks, or swaps (`PartyForm_Swap`: sound `0x103`, or `0x107` refused); cancel drops a pick or leaves (`Transition_Start(2)`) |
| `PartyForm_LeaveStep` | `0x580E80` | 0xE | `jmp [PartyForm_LeaveSteps + 4 * step]`, unchecked |
| `PartyForm_LeaveFade` | `0x580E90` | 0x28 | while the transition runs a tail jump to `PartyForm_Draw`; then black, counter 2 |
| `PartyForm_LeaveBlack` | `0x580EC0` | 0x1A | black for the counter |
| `PartyForm_Reload` | `0x580EE0` | 0x2CC | `PartySet_Load` of the new list; one member (with `0x904152`, flag `0x77` clear or area `0xBB`, not area `0x85`: record 0 copied over the leader's `+0x80`) or the members; the leader stepped half a cell by `0x6640F8` when its `+0x89` became 2; area `0x85` places from `0x6640C8`, area `0x5C` at a fixed cell with `AreaMap_Elevation`; the view; `Transition_Start(3)` |
| `PartyForm_End` | `0x5811B0` | 0x29 | after the transition, mode 1 (the field), state and step 0, `0x929F0F` 1 |
| `PartyForm_DrawSliding` | `0x5811E0` | 0x113 | the screen with each part offset by the counter (panels and numbers 40 a frame, the reserve 45, the title 8) |
| `PartyForm_DrawReserve` | `0x581300` | 0x285 | DIV-0011's frame (section 2); per reserve record the box, portrait, name, level, status text and the HP / AP figures with their colours, and `0x574400` tags |
| `PartyForm_Swap` | `0x581590` | 0x18F | answers 0xFF when the picks were exchanged, else the refused id (a record with `+0xB` bit 1); in one column two bytes swapped (`0x58BD50`), across the ids exchanged in both party lists and the reserve |
| `PartyForm_Draw` | `0x581720` | 0x18F | the screen: backdrop, title box, members' panels and numbers, the reserve, the pick's cursor box and the blinking one (cells of `0x664078`) |

### 1.3 ShopMode 9's window set-ups, the shared timer step

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `ShopBrowse_InitWindows` | `0x5836E0` | 0x90 | `WindowRecords` 0, 21 and 1 set up (called by `0x583370`) |
| `ShopBrowse_OpenDetail` | `0x583770` | 0x65 | records 2 and 3 opened, 2's `+0xA` the chosen entry `0x803191` (called by `0x5833E0` on a confirm) |
| `Menu_StepAfterTimer` | `0x5845E0` | 0x15 | the counter down; at 0 the step up (`SharedList_MoveSteps[1]` and `0x6641B0[0]`) |

ShopMode 9 (`0x583350` and its states, `0x664190`..) is nobody's: its states
call these two, and nothing else of it is in the cut.

### 1.4 The resistance shop (`ShopResist_States` `0x6641BC`)

`0x929F06` the member row (s8, through `Field_Members`, a record index),
`0x929F0D` the bit (s8), `0x929F0B` / `0x929F0F` a run of system messages
(`0x63` + index) and its end.

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `ShopResist_Open` | `0x5837F0` | 0x82 | state 0: sound `0x102` at counter 6, the members and bits sliding in over six frames |
| `ShopResist_PickMember` | `0x583880` | 0x196 | state 1: the member; a record with any bit refused (the notice, state 7); cancel to the farewell (state 6) |
| `ShopResist_PickBit` | `0x583A20` | 0x1BA | state 2: the bit (eight, the hand); ten times the level above the zenny refused (the notice) |
| `ShopResist_Confirm` | `0x583BE0` | 0xF0 | state 3: message `0x6D` and `Menu_YesNo`; yes takes the zenny (state 5), no back to the member |
| `ShopResist_Close` | `0x583CD0` | 0x5D | state 4: sliding out over seven frames, then the field |
| `ShopResist_Grant` | `0x583D30` | 0xAB | state 5: the messages to `0xF`, then the record's `+0x1D` = 1 << bit (not OR-ed: a record has at most one), `Char_RecalcStats` |
| `ShopResist_Farewell` | `0x583DE0` | 0x47 | state 6: the messages to `0x10`, then the slide out |
| `ShopResist_Notice` | `0x583E30` | 0x64 | state 7: the messages to `0x929F0F`, then back to the member |
| `ShopResist_DrawBits` | `0x583EA0` | 0x166 | the bits box: the eight names (system messages `0xDE` + k) and the price, a bit held and the lit row in colour 2 |
| `ShopResist_DrawMembers` | `0x584010` | 0x103 | the title box, the members' panels (lit when `+0x1D` is not 0), the money box; offset by the counter when sliding |
| `ShopResist_Message` | `0x584120` | 0x5D | message `0x63 + *index` until the counter runs out or a pad bit is pressed; answers 1 when `*index` reached the end |

### 1.5 The shared ability list (`SharedList_States` `0x664254`)

State 0 sets up, 1 chooses (`0x6BC8C4`: 1 move abilities, 0 arrange the
list), 2 moves through `SharedList_MoveSteps` (`0x664268`), 3 arranges
through `SharedList_SortSteps` (`0x66427C`), 4 is `ShopTrade_Close` (group
DG's). The windows are `WindowRecords` 2 (the joined records), 3 (a member's
ten slots, record `+0x7E`), 4 (the shared list, 128 entries shown nine at a
time: `+0xA` the top, `+0xB` the cursor, `+0xC` a pick, `+0xD` the scroll), 5
(the item count), 6 (three sort rows).

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `SharedList_Begin` | `0x584190` | 0x19 | the set-up, choice 0 |
| `SharedList_Menu` | `0x5841B0` | 0x2A4 | the choice; 1 wants key item `0xF`, or one of item `0x58` (category 0) and then the use step; 0 opens window 4 |
| `SharedList_MoveStep` | `0x584460` | 0xE | `jmp [SharedList_MoveSteps + 4 * step]`, unchecked |
| `SharedList_UseItem` | `0x584470` | 0x16D | a yes / no; yes removes one of item `0x58` and opens windows 2 and 3 |
| `SharedList_PickMember` | `0x584600` | 0x1E3 | the joined record (`0x6BC8B8` over `0x6BC8BC`), its name to text record 0 |
| `SharedList_PickSlot` | `0x5847F0` | 0x1B3 | the member's slot; pad bit 4 puts the slot's ability into the shared list (`AbilityList_Add(id, 0, 1, 0)`) and empties the slot |
| `SharedList_PickShared` | `0x5849B0` | 0x1FD | an entry of the shared list; confirm swaps it with the slot (both empty buzzes) |
| `SharedList_SortStep` | `0x584BB0` | 0xE | `jmp [SharedList_SortSteps + 4 * step]`, unchecked |
| `SharedList_SortOpen` | `0x584BC0` | 0x50 | window 6 opened after the counter |
| `SharedList_SortMenu` | `0x584C10` | 0x117 | three rows: 0 arrange by hand, 1 and 2 `SharedList_Sort(row)` |
| `SharedList_Arrange` | `0x584D30` | 0x23A | two entries picked and swapped |
| `SharedList_Setup` | `0x584F90` | 0xFC | the joined records' list, windows 0, 1, 21 (and 5 without key item `0xF`) |
| `SharedList_DrawList` | `0x585090` | 0x469 | window kind handler: the shared list's rows (`Menu_ListScroll`, `Menu_DrawSkillRow`), the title, the count (`0x591AC0`), the frame, the scroll bar |
| `SharedList_DrawMember` | `0x585500` | 0x2D3 | window kind handler: a record's ten slots, its name, the frame |
| `SharedList_Sort` | `0x5857E0` | 0x10 | `jmp [SharedList_Sorts + 4 * (arg & 0xFF)]`, unchecked |
| `SharedList_Compact` | `0x5857F0` | 0x4E | the empty entries bubbled to the end (127 passes of `0x58BD50` swaps) |
| `SharedList_SortCostDown` | `0x585840` | 0x76 | compacted, then sorted by the abilities' byte `+0x12` (the cost `Menu_DrawSkillRow` shows), largest first |
| `SharedList_SortCostUp` | `0x5858C0` | 0x76 | the same, smallest first |
| `SharedList_DrawItemCount` | `0x585940` | 0xB9 | window kind handler: the count of item `0x58` |

### 1.6 The equip screen's choosers

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `Equip_ChooseSlot` | `0x58C7A0` | 0x338 | the slot (0..5, a five-case jump table at `+0x324`) and the member; the slot's item and help word; confirm to the item list; pad bit 4 takes an item off (not the weapon: `Inventory_Add`, `Char_RecalcStats`); cancel back two, each member's window `+3` 8 |
| `Equip_ChooseItem` | `0x58CAE0` | 0x251 | the item list scrolled nine rows a page with the preview `0x58D640`; the top and cursor kept at `0x939880` / `0x9398A0` [slot + 6 member] (slots 4 and 5 both); confirm applies (`0x58D570`) |

### 1.7 The PSX twins

`analysis/pairs_propagated.json` pairs 44 of the 53 (the catalog's
hypotheses, cited in each `evidence` string); none of the twins has a name
in the sibling's `names/*.toml` or `symbols.toml`, so every name here is
from the PC code. None was read on the PSX side this round.

## 2. Divergence

**DIV-0011 lives inside `PartyForm_DrawReserve`.** Its first call, the site
`0x581313`, was the empty `0x4DF820`; `menu_frame.cpp`'s `RetargetCall`
re-aims it at `Menu_DrawFrame`, the frame the PlayStation draws round the
reserve list ([`DIVERGENCE.md`](DIVERGENCE.md) DIV-0011). Ours calls
whatever that site reaches - `field_s::FrameCallTarget()` reads its rel32 -
so the frame is drawn as before with ours, `BOF3X_ORIGINAL=Menu_DrawFrame`
still turns it off, and `BOF3X_ORIGINAL=PartyForm_DrawReserve` runs Capcom's
body with the retargeted site as before. No new divergence: every function
is otherwise a faithful replacement, and no ledger entry is owed. The DIV-0011
entry's "the other 21 are untouched" and its site list stand; its host is
ours now.

`grep` of `DIVERGENCE.md` and `src/game/cheats.cpp` for the 53 entries and
their bodies: only `0x581313` (above). `DIVERGENCE.md`'s `0x580074` /
`0x5809FF` (DIV-0002) lie in `0x580010` and `Save_QuickWrite`, not ours.

## 3. The arguments pushed with leftovers

Capcom pushes whole registers whose upper bytes are a callee's leftover, the
caller's `esi`, or stack it never wrote; ours passes the value the callee
reads. Each callee's reading, and the fuzz's mask (`field_s_fuzz.cpp`,
`g_callees`):

| Callee | What the originals push | What it reads (ours, cited) | Mask |
|---|---|---|---|
| `Menu_DrawTitleBox` | the style in `al`; `y` = `0x10 - 10 esi` over the caller's `esi` (`0x584010`) or `0x12 - 8 ecx` over a callee's `ecx` (`0x5811E0`) | `x`, `y` as `U16`, `w + 1`, `h + 1` as words, the colour's byte (`menu_windows.cpp`) | `0xFFFF` x4, `0xFF` |
| `Menu_DrawMemberStatus` | `x` over `esi` (`0x584010`); the member and the lit flag over record offsets | `x`, `y` passed to 16-bit draws, `member & 0xFF`, the flag's byte (`map_field_objects.cpp`) | `0xFFFF`, `0xFFFF`, `0xFF`, `0xFF` |
| `Menu_DrawMoneyBox` | `x` over `esi` | `x`, `y` into 16-bit draws; the third argument unread | `0xFFFF`, `0xFFFF`, 0, all |
| `Menu_DrawCursorBox` | `x`, `y`, `w`, `h` as words over `0x664078`'s address or a callee's leftover | all four as `U16`, blink and flags bytes | `0xFFFF` x4, `0xFF` x2 |
| `Menu_DrawBox` | the colour over entry registers; `x`, `y` words | `x`, `y`, `w`, `h` as `U16`, flags and colour bytes | `0xFFFF` x4, `0xFF` x2 |
| `Menu_DrawBackdrop` | the kind in `al` over `eax` | `kind & 0xFF` | `0xFF` |
| `Menu_DrawItemIcon`, `Menu_DrawHand`, `Menu_DrawBorder`, `Menu_DrawPiece`, `Menu_DrawPieces`, `Menu_DrawScrollBar`, `0x574400` | coordinates as words over callee leftovers | 16-bit coordinates, bytes (each function's comment) | `0xFFFF` / `0xFF` |
| `Menu_DrawSkillRow` | the kind as a flag index over a stack dword; the lit row's colour likewise | `kind & 0xFF`, six bits of the colour (via `Text_DrawAt` / `Text_DrawFont8`), `cost & 0xFF` | `0xFFFF`, `0xFFFF`, `0xFF`, `0xFF`, -, `0xFF`, `0xFF` |
| `Text_DrawAt`, `Text_DrawFont8`, `Text_DrawFont12` | colours as a byte over stack or argument upper bytes; `x` over callee leftovers | `x`, `y` as shorts; `Text_DrawString` reads the colour's byte, the fonts six bits | `0xFFFF`, `0xFFFF`, `0xFF` / `0x3F` |
| `Msg_SystemPtr` | `0x63 + index`, `0xDE + k`, `0x65 + bit` over leftovers | `id & 0xFFFF` (`msg_pool.cpp`) | `0xFFFF` |
| `Skill_FlagIndex`, `Item_HelpMessage`, `Inventory_Add`, `Inventory_Count`, `AbilityList_Add`, `PartySet_Load`, `Field_MemberSprite`, `Party_Count` | bytes loaded into `al` / `cl` / `dl` over leftovers | each reads the low bytes (`char_stats.cpp`, `scena_sx.cpp`, `field_event.cpp`) | `0xFF` each |

`Crt_sprintf` is re-listed with three words (the standard row logs a fourth,
the caller's stack: every format here takes one number). **For the harness
fold:** these masks, a three-word `Crt_sprintf`, `Port_DroppedCall` at four
words (`0x581300` pushes four), `Party_Count` answering 0..3 (a random count
runs the callers' loops past `ObjTrio` and the party lists), and
`Menu_ListScroll` writing its two out-bytes (below).

## 4. The fuzz (`field_s_fuzz.cpp`)

One `Group`, field mode, `menu_span` 3, 4,000 rounds a function (212,000),
`BOF3X_FS_RUN=<name>` runs one. **0 mismatches** in this worktree, 8,032,176
calls to the stand-ins, 24,800 bytes of state (41 regions). The shapes:
every state handler `kMenu`; the helpers `kCall`; `PartyForm_Swap` and
`ShopResist_Message` answer in `al` (`ret_mask` `0xFF`).

- **The regions** beyond the field set: `WindowRecords` (22 x `0x24`),
  `0x6BC880..0x6BC8C8` (the reserve, its flags, the joined list, the
  cursors), `CharacterRecords` past the style region (`0x903A94..0x903F90`),
  `Field_ConfirmButtons` / `Field_CancelButtons` (`0x903584`, 0x10 - not in
  any standard region, so confirm and cancel were never pressed until
  listed), and the field menu's per-member bytes `0x939880..0x9398CF`.
- **The five `.data` tables** the dispatchers read are `DataTable`s
  (`PartyForm_OpenSteps` 3, `PartyForm_LeaveSteps` 4, `SharedList_MoveSteps`
  5, `SharedList_SortSteps` 3, `SharedList_Sorts` 3); each dispatcher's
  step is seeded inside its table (`menu_span` 3 for the rest).
- **The group's own callees** are listed with their arguments' masks (not
  `kPhase`, which logs no arguments): `PartyForm_Draw`, `_DrawSliding`,
  `_DrawReserve`, `_Swap` (answering `0xFE..0x07`), `ShopResist_DrawBits`,
  `_DrawMembers`, `_Message`, `SharedList_Setup`, `_Sort`, `_Compact`.
- **`PartyForm_DrawReserve`'s copy is the fuzz file's**: its first site is
  DIV-0011's, re-aimed before this module's inject, which the harness's
  `CloneOriginal` refuses ("the site is re-aimed already"). The file copies
  `0x581300` itself with every site re-aimed at a trampoline into the
  recorder the harness stands in for the callee (`sh::StandIn`), the first
  keyed by where the site reaches, and hands the harness a six-byte `jmp
  [copy]` (`scena_sc12_fuzz.cpp`'s way). A row `Menu_DrawFrame (DIV-0011)`
  (address `0x581313`, key `Menu_DrawFrame`) stands for it: 4,000 calls.
- **`Menu_ListScroll`** writes both out-bytes on every path; its stand-in
  here does (the offset any byte, the scroll flag 0 or 1, now and then the
  top and the state), since `SharedList_DrawList` reads all four after it -
  one of them is the low byte of the window argument's own stack slot.
- **The seeds**: the buttons one bit each and the pad hitting either, both
  or none; the party list ids inside `MoveScript_EffectState`'s 24 (so a
  record index is one of the eight); `Field_Members` a record index; the
  records' `+0xA`, `+0xB`, `+0x1D`, HP and AP at their colour thresholds; the
  menu cells at their boundaries (rows -1..5, picks `0x7F`, the answer's
  message values); the reserve's bytes and then its count (0..5 - the count
  is the reserve's fourth byte, section 10 L2); the windows' cursors and tops
  at the page limits (`0`, `8`, `9`, `0x6E`, `0x6F`, `0x77`, `0x7F`); the
  zenny at ten times the cursor member's level and one either side; the area
  among those compared (`0xBB`, `0xC1`, `0xBF`, `0x85`, `0x5C`) and ones
  whose descriptor is set; chapter `0xE` with its flag row half the time.
- **The disturbance** (`Move`, from the hash only): the cursors and picks,
  the reserve count and answer, the shared list's cells, the item and slot
  windows' cells, a party list id, the zenny, a record's `+0xA` / `+0xB` /
  `+0x1D`, `0x904152`, `Field_StatusBits` bit 6, the member count; also after
  `Sound_PlayEffect`, `Field_MemberSprite`, `Field_PartyLoad`, `PartyForm_Swap`,
  `ShopResist_Message`, `SharedList_Setup` and `0x58D570` (their `Stir`); the
  area among the moves, and `Flags_Clear` moving it (Rest_PlaceParty reads it
  again after).

What the fuzz does not reach: the tables past their counts (seeded inside:
section 10); the real draws (every draw is a recorder, so what the screens
look like is untested here - section 9).

## 5. What the cut and the tool said, settled

- **`0x58CAE0` is a function** in no cut row: `band_rows.py` flags it as code
  after `0x58C7A0` (whose cut size 1440 spans both); it is reached through
  `0x6673B4`, the Equipment states' entry 6. Taken.
- **The cut's sizes** are padding for 35 rows and wrong for one:
  `0x58C7A0`'s 1440 is 0x338 (its jump table ends it) plus `0x58CAE0`.
- **Every start is a function**: no case of another's in the 53. The
  dispatchers (`0x580A50`, `0x580E80`, `0x584460`, `0x584BB0`, `0x5857E0`)
  are three-instruction functions of their own, each a table's entry.
- **Hidden starts inside hosts already ours: none.** The hosts are
  `0x57F340` (Capcom's, its line in `entries_logic.txt` ends at `0x57F500`),
  `Save_QuickWrite` `0x5809C0` (Capcom's), `0x58C2A0` (Capcom's), and three of
  the group's own (`0x583770`, `0x584120`, `0x5857F0`), whose code ends
  before their hidden neighbours.
- **`scenario_harness_fh` copies two of the 53** (`0x5811B0`, `0x5845E0`)
  from the image for its self-test: `FieldS_Inject` goes after
  `ScenarioHarnessFh_Inject` in `inject_all.cpp`.

## 6. Controls

Planted one at a time in `field_s.cpp` (a scratch script: anchor on a unique
string, rebuild, `BOF3X_SHADOW=field_s BOF3X_FS_RUN=<function>`, restore,
rebuild), 4,000 rounds each, against the final fuzz (`cac09bb`). **188
planted: 183 refused by a count, 5 not** - each an equivalent mutant (no
input can tell it apart), each with a near variant planted and refused:

- **C5** reads the column register where the original re-reads
  `0x929F08`; no call lies between the store and the read. Near variant C5b
  (the column before the flip) refused in 418.
- **Q7** drops the elevation's sign extension before `<< 16`, which keeps
  only its low word anyway. Q7b (`<< 15`) refused in 489.
- **T25** drops the colour argument's upper bytes from the text colour:
  `Text_DrawString` reads the colour's byte and `Text_DrawFont12` six bits,
  so the game cannot see them either (the masks say so). T25b (colour 6)
  refused in 2,868.
- **M18** clamps the slot at `> 8` to 9 - for 9 the same store. M18b (clamped
  to 8) refused in 144.
- **M24** stores through the cursor register where the original re-reads
  `0x8031FB`; no call between. M24b (the cursor before the move) refused in
  688.

Two controls were not refused on the first run and were the fuzz's fault,
fixed before this run: P7 (the area not read again after `Flags_Clear`:
its stand-in now moves the area half the time) and Q5 (the leader's half-cell
step: `Field_PartyLoad`'s stand-in now sets each member's `+0x89` and
`+0x148` as the real one does, so the kept id and the new one can differ).

| Id | Function | Planted | Refused in (of 4,000) |
|---|---|---|--:|
| S1 | `FieldSave_Confirm` | message 0xA0 | 4000 |
| S2 | `FieldSave_Confirm` | yes steps two | 2254 |
| S3 | `FieldSave_Confirm` | slots lit | 4000 |
| R1 | `Rest_Begin` | state 6 after the message | 34 |
| R2 | `Rest_Begin` | chapter 0xD | 694 |
| R3 | `Rest_Begin` | row flag 6 | 694 |
| P1 | `Rest_PlaceParty` | area 0xC0 | 302 |
| P2 | `Rest_PlaceParty` | kind 1 for 0xBF | 200 |
| P3 | `Rest_PlaceParty` | script flags bit 3 cleared too | 674 |
| P4 | `Rest_PlaceParty` | row by i + 1 | 1015 |
| P5 | `Rest_PlaceParty` | +4 not cleared | 1006 |
| P6 | `Rest_PlaceParty` | no Music_Track | 1349 |
| P7 | `Rest_PlaceParty` | area not read again after Flags_Clear | 50 |
| D1 | `PartyForm_OpenStep` | step ^ 1 | 4000 |
| D2 | `PartyForm_LeaveStep` | step & 2 | 2049 |
| D3 | `SharedList_MoveStep` | step % 4 | 798 |
| D4 | `SharedList_SortStep` | step & 1 | 1311 |
| D5 | `SharedList_Sort` | the next sort | 2631 |
| F1 | `PartyForm_Setup` | joined bit 1 | 3930 |
| F2 | `PartyForm_Setup` | the second list | 171 |
| F3 | `PartyForm_Setup` | one reserve flag fewer | 3970 |
| F4 | `PartyForm_Setup` | clearing when below 2 | 989 |
| F5 | `PartyForm_Setup` | counter 4 | 3940 |
| F6 | `PartyForm_Setup` | result 0xFE | 3995 |
| F7 | `PartyForm_Setup` | transition 1 | 4000 |
| W1 | `PartyForm_FadeWait` | transition 2 | 2018 |
| W2 | `PartyForm_OpenWait` | state up one | 2012 |
| W3 | `PartyForm_SlideIn` | counter 2 | 938 |
| W4 | `PartyForm_SlideIn` | backdrop by the style byte | 3987 |
| C1 | `PartyForm_Choose` | message 0x62 | 2011 |
| C2 | `PartyForm_Choose` | name of 7 | 1989 |
| C3 | `PartyForm_Choose` | column flip on 0x8000 only | 876 |
| C4 | `PartyForm_Choose` | reserve cut one late | 168 |
| C5 | `PartyForm_Choose` | the column not read again | **not refused** (equivalent, below) |
| C6 | `PartyForm_Choose` | down wraps at the last | 80 |
| C7 | `PartyForm_Choose` | the column compared unsigned | 29 |
| C8 | `PartyForm_Choose` | refusal 0x106 | 997 |
| C9 | `PartyForm_Choose` | step kept on leaving | 301 |
| C10 | `PartyForm_Choose` | the pick row the column | 720 |
| L1 | `PartyForm_LeaveFade` | counter 1 | 2018 |
| L2 | `PartyForm_LeaveFade` | no draw while waiting | 1982 |
| L3 | `PartyForm_LeaveBlack` | steps at 1 | 450 |
| L4 | `PartyForm_End` | 0x929F0F 2 | 2018 |
| L5 | `PartyForm_End` | mode 2 | 2018 |
| Q1 | `PartyForm_Reload` | area 0xBC | 175 |
| Q2 | `PartyForm_Reload` | record copy 0xA0 | 758 |
| Q3 | `PartyForm_Reload` | +0x48 = 1 | 758 |
| Q4 | `PartyForm_Reload` | leader +0x89 3 | 467 |
| Q5 | `PartyForm_Reload` | step << 16 | 399 |
| Q6 | `PartyForm_Reload` | area 0x85 y from +4 | 426 |
| Q7 | `PartyForm_Reload` | elevation not sign-extended | **not refused** (equivalent, below) |
| Q8 | `PartyForm_Reload` | state bytes without 0x904152 | 2402 |
| Q9 | `PartyForm_Reload` | ids 2 and 1 swapped | 3777 |
| Q10 | `PartyForm_Reload` | area 0x5D | 634 |
| G1 | `PartyForm_DrawSliding` | lit by bit 0 | 2516 |
| G2 | `PartyForm_DrawSliding` | number 32 per frame | 2706 |
| G3 | `PartyForm_DrawSliding` | reserve y 0x3C | 4000 |
| G4 | `PartyForm_DrawSliding` | title 9 per frame | 3594 |
| H1 | `PartyForm_DrawReserve` | frame 0x14 high | 4000 |
| H2 | `PartyForm_DrawReserve` | shade from bit 6 | 2347 |
| H3 | `PartyForm_DrawReserve` | blink on bit 4 | 831 |
| H4 | `PartyForm_DrawReserve` | HP colour at 0 only | 1431 |
| H5 | `PartyForm_DrawReserve` | AP below the quarter | 616 |
| H6 | `PartyForm_DrawReserve` | one row fewer | 2614 |
| H7 | `PartyForm_DrawReserve` | max HP colour by +0x1F | 49 |
| X1 | `PartyForm_Swap` | party refused on bit 0 | 1415 |
| X2 | `PartyForm_Swap` | second list the other way | 200 |
| X3 | `PartyForm_Swap` | reserve kept | 418 |
| X4 | `PartyForm_Swap` | party at the row from column 1 | 1375 |
| X5 | `PartyForm_Swap` | three, not the count | 900 |
| Y1 | `PartyForm_Draw` | lit by bit 0 | 1821 |
| Y2 | `PartyForm_Draw` | number colour 6 | 1835 |
| Y3 | `PartyForm_Draw` | pick box blinking | 2016 |
| Y4 | `PartyForm_Draw` | cell by row + 2 column | 3489 |
| B1 | `ShopBrowse_InitWindows` | +0xC 0xFF | 4000 |
| B2 | `ShopBrowse_InitWindows` | x -0x63 | 4000 |
| B3 | `ShopBrowse_OpenDetail` | entry from +0xC | 3988 |
| B4 | `ShopBrowse_OpenDetail` | y 0xEF | 4000 |
| T1 | `ShopResist_Open` | sound at 5 | 886 |
| T2 | `ShopResist_Open` | bits x + 1 | 4000 |
| T3 | `ShopResist_Open` | 0x929F05 kept | 938 |
| T4 | `ShopResist_PickMember` | down wraps past the count | 171 |
| T5 | `ShopResist_PickMember` | up wraps to the count | 1170 |
| T6 | `ShopResist_PickMember` | refused on +0x1C | 988 |
| T7 | `ShopResist_PickMember` | notice to 3 | 1039 |
| T8 | `ShopResist_PickMember` | farewell from 0xE | 1784 |
| T9 | `ShopResist_PickBit` | message by the unsigned bit | 431 |
| T10 | `ShopResist_PickBit` | hand 12 apart | 3531 |
| T11 | `ShopResist_PickBit` | the price at the zenny refused | 317 |
| T12 | `ShopResist_PickBit` | up wraps to 6 | 542 |
| T13 | `ShopResist_Confirm` | nine a level | 1882 |
| T14 | `ShopResist_Confirm` | no bit lit | 3570 |
| T15 | `ShopResist_Confirm` | no back one | 399 |
| T16 | `ShopResist_Close` | closes at 6 | 860 |
| T17 | `ShopResist_Grant` | the bit OR-ed | 1223 |
| T18 | `ShopResist_Grant` | back three | 2687 |
| T19 | `ShopResist_Grant` | messages to 0xE | 4000 |
| T20 | `ShopResist_Farewell` | counter 1 | 2682 |
| T21 | `ShopResist_Notice` | back five | 2661 |
| T22 | `ShopResist_Notice` | cursor not blinking | 4000 |
| T23 | `ShopResist_DrawBits` | price a byte | 2280 |
| T24 | `ShopResist_DrawBits` | colour 1 | 740 |
| T25 | `ShopResist_DrawBits` | the upper bytes dropped | **not refused** (equivalent, below) |
| T26 | `ShopResist_DrawBits` | highlight one off | 2425 |
| T27 | `ShopResist_DrawBits` | border 0xE | 4000 |
| T28 | `ShopResist_DrawMembers` | lit inverted | 2967 |
| T29 | `ShopResist_DrawMembers` | sliding bit 0 | 457 |
| T30 | `ShopResist_DrawMembers` | money y 0x27 | 4000 |
| T31 | `ShopResist_Message` | the pad low byte | 548 |
| T32 | `ShopResist_Message` | end + 1 | 1806 |
| T33 | `ShopResist_Message` | counter 0x3B | 3604 |
| M1 | `SharedList_Begin` | choice 1 | 4000 |
| M2 | `SharedList_Menu` | help 0x74 | 4000 |
| M3 | `SharedList_Menu` | hand 40 apart | 1171 |
| M4 | `SharedList_Menu` | count a byte | 2 |
| M5 | `SharedList_Menu` | step 0 with the key item | 860 |
| M6 | `SharedList_Menu` | window 4 +3 2 | 755 |
| M7 | `SharedList_Menu` | window 5 with the key item | 966 |
| M8 | `SharedList_Menu` | cancel to state 3 | 972 |
| M9 | `SharedList_UseItem` | the answer unsigned | 297 |
| M10 | `SharedList_UseItem` | the pad not read again | 24 |
| M11 | `SharedList_UseItem` | two used | 1772 |
| M12 | `SharedList_UseItem` | the pick not read again | 4 |
| M13 | `Menu_StepAfterTimer` | step up two | 958 |
| M14 | `SharedList_PickMember` | wraps to the count | 574 |
| M15 | `SharedList_PickMember` | +9 0x32 going on | 1029 |
| M16 | `SharedList_PickMember` | hand x + 0x2B | 1610 |
| M17 | `SharedList_PickMember` | window 3 +3 2 | 2072 |
| M18 | `SharedList_PickSlot` | slots to 8 | **not refused** (equivalent, below) |
| M19 | `SharedList_PickSlot` | pad bit 5 | 527 |
| M20 | `SharedList_PickSlot` | not the shared list | 415 |
| M21 | `SharedList_PickSlot` | hand one row up | 1610 |
| M22 | `SharedList_PickSlot` | window 4 +3 0 | 728 |
| M23 | `SharedList_PickShared` | either empty buzzes | 140 |
| M24 | `SharedList_PickShared` | the cursor not read again | **not refused** (equivalent, below) |
| M25 | `SharedList_PickShared` | hand x + 8 | 4000 |
| M26 | `SharedList_PickShared` | scroll at 8 (ScrollList) | 48 |
| M27 | `SharedList_PickShared` | page down past 0x6F (ScrollList) | 23 |
| M28 | `SharedList_PickShared` | page up below 8 (ScrollList) | 67 |
| M29 | `SharedList_SortOpen` | x 0xC9 | 958 |
| M30 | `SharedList_SortMenu` | four rows | 186 |
| M31 | `SharedList_SortMenu` | sort row - 1 | 1462 |
| M32 | `SharedList_SortMenu` | window 4 +3 3 | 972 |
| M33 | `SharedList_SortMenu` | hand y + 1 | 4000 |
| M34 | `SharedList_Arrange` | the pick kept | 251 |
| M35 | `SharedList_Arrange` | +0xB 0xFE | 236 |
| M36 | `SharedList_Arrange` | hand x + 7 | 4000 |
| M37 | `SharedList_Setup` | joined by bit 1 | 3982 |
| M38 | `SharedList_Setup` | window 1 +0xE 3 | 4000 |
| M39 | `SharedList_Setup` | window 5 y 0x25 | 1336 |
| M40 | `SharedList_DrawList` | no adjust when scrolling down | 30 |
| M41 | `SharedList_DrawList` | colour 3 | 162 |
| M42 | `SharedList_DrawList` | eight rows | 3525 |
| M43 | `SharedList_DrawList` | title centred on 12 | 4000 |
| M44 | `SharedList_DrawList` | count of the current | 4000 |
| M45 | `SharedList_DrawList` | eleven pieces | 4000 |
| M46 | `SharedList_DrawMember` | colour by +0xB | 434 |
| M47 | `SharedList_DrawMember` | name six | 2724 |
| M48 | `SharedList_DrawMember` | +0xD kept at 0 | 206 |
| M49 | `SharedList_DrawMember` | second list by bit 1 | 1896 |
| M50 | `SharedList_Compact` | swapped with an empty next | 4000 |
| M51 | `SharedList_Compact` | 126 passes | 3185 |
| M52 | `SharedList_SortCostDown` | equal swapped | 4000 |
| M53 | `SharedList_SortCostUp` | equal swapped | 4000 |
| M54 | `SharedList_SortCostUp` | stops at an empty next | 4000 |
| M55 | `SharedList_DrawItemCount` | count x + 0x35 | 4000 |
| M56 | `SharedList_DrawItemCount` | eight lists | 4000 |
| E1 | `Equip_ChooseSlot` | five slots | 217 |
| E2 | `Equip_ChooseSlot` | slot 4 armour | 509 |
| E3 | `Equip_ChooseSlot` | weapon +0x13 | 1279 |
| E4 | `Equip_ChooseSlot` | cursor from the top bytes | 1762 |
| E5 | `Equip_ChooseSlot` | the weapon taken off too | 88 |
| E6 | `Equip_ChooseSlot` | member windows +3 7 | 580 |
| E7 | `Equip_ChooseSlot` | member wraps late | 194 |
| E8 | `Equip_ChooseSlot` | hand y + 1 | 4000 |
| E9 | `Equip_ChooseSlot` | item window +3 2 | 769 |
| E10 | `Equip_ChooseItem` | page down past 0x6F | 33 |
| E11 | `Equip_ChooseItem` | accessory top at +3 | 1565 |
| E12 | `Equip_ChooseItem` | slot 4 alone | 389 |
| E13 | `Equip_ChooseItem` | the member byte | 401 |
| E14 | `Equip_ChooseItem` | +0xC 1 | 376 |
| E15 | `Equip_ChooseItem` | scroll word 0x11 | 489 |
| E16 | `Equip_ChooseItem` | help arguments swapped | 3601 |
| C5b | `PartyForm_Choose` | the column before the flip | 418 |
| Q7b | `PartyForm_Reload` | elevation << 15 | 489 |
| T25b | `ShopResist_DrawBits` | colour 6 when refused | 2868 |
| M18b | `SharedList_PickSlot` | slots clamped to 8 | 144 |
| M24b | `SharedList_PickShared` | the cursor before the move | 688 |

## 7. Calls across groups

- **Out, to this wave's groups**: `0x574400` (FO's), three sites in
  `PartyForm_DrawReserve`, called raw through `field_s_callees.h`
  (`at::kDrawTag`) and listed in the fuzz. The coordinator rebinds it after
  FO merges (FO before FS in the merge order).
- **Out, to nobody's**: `0x58BD50` (the byte swap; standard), `0x591AC0`
  (standard), `0x58D640` and `0x58D570` (the equip preview and its apply,
  listed here), `0x4DF820` / `Menu_DrawFrame` (section 2).
- **In, from outside the group** (for the rebinding pass): the dispatchers
  `0x580300` (`Rest_States`), `0x580A40` (`PartyForm_States`), `0x5837E0`
  (`ShopResist_States`), `0x584180` (`SharedList_States`), `0x58C2C0`
  (`0x66739C`, the Equipment states) read the tables in place - no raw
  address to rebind; `ShopMode_States[5..7, 10]` hold those dispatchers;
  `FieldSave_Dispatch` (DF's, ours) reaches `FieldSave_Confirm` through
  `FieldSave_States[3]`; `0x6641B0[0]` (ShopMode 9's) holds
  `Menu_StepAfterTimer`; direct calls: `0x583370` -> `ShopBrowse_InitWindows`,
  `0x5833E0` -> `ShopBrowse_OpenDetail`, `0x59BE50` -> `SharedList_DrawList`,
  `0x59BEA0` -> `SharedList_DrawMember`, `0x59C260` ->
  `SharedList_DrawItemCount` - all Capcom's code, nobody's.

## 8. The rebinding

`band_rows.py --refs` found 11 raw references to 6 of the 53 in `src/game`,
none a call or table value in a file this group may edit:

| File | Reference | Left raw because |
|---|---|---|
| `scenario_harness.cpp:850` | the field runs `{0x57FF80, 0x5859FA}`, `{0x58C7A0, 0x58C900}` | band limits, not calls; a harness file (the coordinator's fold). The second band stops at `0x58C900`, inside `Equip_ChooseSlot` (to `0x58CAD8`) - `0x58CAE0` lies outside the runs and is logged so |
| `scenario_harness_fh.cpp:40..44` | `0x5811B0`, `0x5845E0` cloned from the image | FH's self-test copies Capcom's bytes by address on purpose (a harness file) |
| `menu_frame.cpp:58, 63` | `0x581300` in a comment; `kReserveListSite = 0x581313` | a call site inside a function, not a function's address |
| `shop_states2.cpp:1018` | "the menu kind `0x584120`'s states" | a comment (the dispatcher is `0x584180`; `0x584120` is `ShopResist_Message`) |

## 9. The live route

`grep -i` of the 53 entries in both routes' reach traces
(`analysis/calltrace/reach_whelp`, `reach_dragon`, 2026-09-29 at `979a567`):
**none is entered.** The coordinator's frame-hash A/B covers none of them;
all 53 are fuzz-only. A route through a save point's party change, the
resistance shop, the shared list and the equip screen would reach them
(`tools/recipes/menu_screens.txt` walks the Equipment screen - its states 5
and 6 are these two - but no reach trace of it at this tip was read).

## 10. Latent defects (Capcom's, described, not fixed)

- **L1 `PartyForm_Setup` writes the reserve's flags past their three pairs.**
  The reserve's flag bytes are written two apart from `0x6BC88D`, one per
  reserve entry: from the fourth entry on they land on `0x6BC893`,
  `0x6BC895` (the reserve's second entry: overwritten with 1) and
  `0x6BC897` (the count: 1). The clearing loop meant for the rest never runs
  (`cmp esi, eax; jge` with `esi` set to the count).
- **L2 The reserve holds three.** Its count `0x6BC897` is its own fourth
  byte: `PartyForm_Setup` writes the fourth record there, then the count over
  it; a fifth lands on `0x6BC898`, which the set-up then sets to 0xFF. With
  four or more records outside the party the list and its count are
  garbled (and L1 garbles them further). Whether ordinary play reaches four
  outside the party is the owner's to say; if it does, the formation screen
  shows the wrong reserve.
- **L3 The dispatchers are unchecked**: `PartyForm_OpenStep`,
  `_LeaveStep`, `SharedList_MoveStep`, `_SortStep` jump through their step
  byte and `SharedList_Sort` through its argument's byte with no bound (the
  round's common class). Ours reads the word in place and aborts on one that
  is not code.
- **L4 `ShopResist_Grant` replaces the bits**: `+0x1D` = 1 << bit, not OR-ed.
  The member picker refuses a record with any bit set, so the shop itself
  never replaces one; another writer of `+0x1D` was not looked for.
- **L5 `SharedList_Compact`'s passes and `SharedList_SortCost*`'s sorts are
  quadratic** (127 x 127 steps, each swap a call): harmless, noted for the
  fuzz's call counts.

## 11. For `analysis/calltrace/entries_logic.txt`

41 lines appended to the main checkout's file (2026-09-29): every one of the
53 entries with its code extent, except the twelve already listed with the
same extent or its padding. `00583770`, `00584120` and `005857F0` were host
lines spanning their hidden neighbours (`724`, `E50`, `146`); their smaller
extents (`65`, `5D`, `4E`) are added (the consolidation keeps the smaller).
