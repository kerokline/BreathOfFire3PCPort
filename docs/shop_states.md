# The shop overlay's first table: the mode, the inn and the save menu

**Status:** IN PROGRESS (2026-09-25). Twenty-five functions are ours
(`src/game/shop_states.cpp`). Each is fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 40,000 rounds, 0
mismatches. 104 negative controls were planted, one at a time: all 104 were
refused by a count (section 4).

This is group DF of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)), wave B. Every
function is a *faithful* replacement, so no `DIVERGENCE.md` entry is owed.
The model was round seven's [`battle_flow.md`](battle_flow.md). The
neighbours are round six's: [`save-menu.md`](save-menu.md) (group X - the
callees `SaveMenu_DrawChoices`, `SaveMenu_DrawSlots`, `Save_ReadSummaries`,
`Party_RestoreAll`, `Menu_DrawBlackScreen`, `Sound_LoadStream`,
`Sound_StreamDone`) and [`menu-windows.md`](menu-windows.md) (group Y -
`Menu_DrawTitleBox`, `Menu_DrawMoneyBox`, `Menu_YesNo`, `Msg_OpenSystem`).

The code is the PSX `SHOP.EMI` overlay (the shop, the inn and the save point
by the sibling's alias) compiled into the exe, which `pe_funcs.py` folded
into the recorded start `0x57F340`. Four PSX twins are the catalogue's
(`analysis/remaining_catalog.tsv`, from `pairs_propagated.json`); none was
read this round, and the sibling names none of them. Every name here is from
the PC disassembly (capstone, 2026-09-25).

## 1. The shape: one mode, five tables

`Game_Mode` 7 is `0x496290`, `jmp [0x656AAC + 4 Game_Step]`. Its step 1 is
`0x517300` (group DB's), which runs six field updates and ends in
`jmp 0x57F500` (`0x517323`); `0x56D4D0` (no group's) ends the same way
(`0x56D4D5`). An E8/E9 scan of `.text` finds those two and nothing else, and
no dword in the image holds `0x57F500`: the dispatcher is **jumped to**, so
its return, and every state's, is `Field_Task`'s.

| Table | Entries | Index | Dispatcher |
|---|--:|---|---|
| `ShopMode_States` `0x663E40` | 11 | `0x929F00` | `ShopMode_Dispatch` `0x57F500` |
| `Inn_Steps` `0x663F4C` | 5 | `0x929F01` | `Inn_Dispatch` `0x57F650` (state 3) |
| `InnPrompt_States` `0x663F60` | 6 | `0x929F02` | `InnPrompt_Dispatch` `0x57F6E0` (step 1) |
| `InnNight_States` `0x663F78` | 7 | `0x929F02` | `InnNight_Dispatch` `0x57FB60` (step 2) |
| `FieldSave_States` `0x663F94` | 9 | `0x929F02` | `FieldSave_Dispatch` `0x57FDD0` (step 3) |

The last four lie end to end; `0x663FB8` starts the next table, which
`0x580300` (`ShopMode_States` 10) jumps through. `ShopMode_States` is
followed by a zero dword. No index is checked, in the original or in ours.

`ShopMode_States`: 0 `ShopMode_Begin`, 1 `ShopMode_End`, 2 `0x5818B0`
(group DG's - the shop), 3 `Inn_Dispatch`, 4 `ShopMode_End` again, 5..10
`0x5837E0`, `0x580A40`, `0x584180`, `0x582EB0`, `0x583350`, `0x580300` (the
overlay's other machines, no group's). `ShopMode_Begin` sets the state to
the touched object's `+0x18` plus 2, so an object of kind 1 is an inn.

`Inn_Steps`: 0 `Inn_Begin`, 1 the prompt, 2 the night, 3 the save menu, 4
`0x580280` (no group's: the title box slides out over five frames, then the
mode state 1 - `ShopMode_End` - with the object put back from `0x929F0F`
when the save point was forced).

### The data

- The menu state block `0x929F00` ([`menu-screens.md`](menu-screens.md)):
  `+0` the mode's state, `+1` the inn's step, `+2` the step's state, `+4` the
  frame counter, `+0xA` cleared at the start, `+0xB` `Menu_YesNo`'s answer,
  `+0xC` the object touched (s8: an index into `Sprite_Objects`, `0xFE` a
  save point, `0xFF` none), `+0xF` the object kept while a forced save point
  runs.
- `0x929EC2` the state less 2 when no object is touched; `0x929EC3` the shop
  number (the object's `+0x1C`) - the inn's night costs **ten times it**
  (`0x57F8A0`), and `0x903844` points at the 23-byte record
  `0x658930 + 23 n`.
- `0x6BC881` the inn's three-way cursor: 0 stay, 1 save, 2 leave (by where
  each goes; the labels are `SaveMenu_DrawChoices`'). `0x6BC882` set when the
  prompt was cancelled. `0x6BC880` and `0x66C7DA`, read by `FieldSave_End`
  (below); `0x6BC880` is set by the save menu's state 5 (`0x5800D0`).
- The save cursor `0x9036D4` and the first slot shown `0x8034D0`, as
  [`save-menu.md`](save-menu.md) §1 has them.
- The words `0x7E01B8` / `0x7E1BE0`, `Input_AutoRepeat`'s latch and timer.
- `0x66C810` (`MoveScript_WaitWordDA`): non-zero while a transition runs.
- Bit 1 of the text layer's flag word `0x7DEE44`: read here as "the system
  message is done" (after `Msg_OpenSystem`); its setter was not read.

## 2. What each function does

Each extent was read to its last instruction; the sizes are the
`entries_logic.txt` lines of section 6. The code comments hold each
function's full reading. The title box is always `Menu_DrawTitleBox(0x14,
0x12, 0x118, 0x13, colour 0x903A5A)`, a message `Text_DrawAt(0x1C, 0x16, 0,
0xFF, Msg_SystemPtr(id))`, the zenny box `Menu_DrawMoneyBox(0x62, 0x28, 0,
zenny 0x904058)`; "with an object" means `0x929F0C != 0xFF`.

| Function | Entry | Bytes | Does |
|---|---|--:|---|
| `ShopMode_Dispatch` | `0x57F500` | 0x11 | `jmp [ShopMode_States + 4 (dword 0x929F00 & 0xFF)]` |
| `ShopMode_Begin` | `0x57F520` | 0xA4 | the state from the object (or `0x929EC2`), `Sprite_Current` and the shop number from it; the eight character records' low-HP bit (`0x2000` of the status word when max HP / 4 > HP); step, sub-state and `+0xA` 0, counter 6, `0x903844` |
| `ShopMode_End` | `0x57F5D0` | 0x75 | with an object: `Sprite_Current` it, unless `+7` bit 3 its facing back from `+0x85` and `Sprite_FaceDirection`, `+0x80` bit 3 set and bit 5 cleared, `Field_ScriptFlags` bit 8 cleared; always `Window_ResetAll` and `Game_Step + 1` |
| `Inn_Dispatch` | `0x57F650` | 0xE | `jmp [Inn_Steps + 4 0x929F01]` |
| `Inn_Begin` | `0x57F660` | 0x78 | step 1, cursor 0, counter 4; `Field_InputFlags` 0x40 or area `0xBC` / `0x85` / `0xC1` force the save point (object kept, `0xFE`, step 3); an object of `0xFE` goes to step 3 too |
| `InnPrompt_Dispatch` | `0x57F6E0` | 0xE | `jmp [InnPrompt_States + 4 0x929F02]` |
| `InnPrompt_TitleIn` | `0x57F6F0` | 0x65 | the title box sliding down (`0x12 - 20 counter`); at 0 `0x929F0B = 1`, with an object `Msg_OpenSystem(0xD1)` |
| `InnPrompt_Greeting` | `0x57F760` | 0xA6 | the box; with an object the price (`"%d"` of 10 x the shop number, text record 0), message `0xA1`, the zenny box; the message done: counter 6, `Sound_PlayEffect(0x102)` |
| `InnPrompt_ChoicesIn` | `0x57F810` | 0x84 | the choices sliding in from `0x6E - 48 counter`, the box, message and zenny |
| `InnPrompt_Choose` | `0x57F8A0` | 0x191 | the cursor over 0..2 (up wraps below 0 as s8, down past 2), `0x101` when it moves; confirm: stay pays 10 x the shop number (`0x104`) or buzzes (`0x107`, to `0x57FA40`), save / leave `0x104`; cancel `0x106` and `0x6BC882` |
| `InnPrompt_ChoicesOut` | `0x57FAB0` | 0xAA | the choices sliding out; at 6: cancelled - step + 3, the way out; else step + cursor + 1, the counter `0x2D` for the night |
| `InnNight_Dispatch` | `0x57FB60` | 0xE | `jmp [InnNight_States + 4 0x929F02]` |
| `InnNight_Message` | `0x57FB70` | 0x65 | message `0xA4` for `0x2D` frames, then `Transition_Start(0)` |
| `InnNight_FadeOut` | `0x57FBE0` | 0x54 | the transition over: `Menu_DrawBlackScreen`, next |
| `InnNight_Jingle` | `0x57FC40` | 0x23 | black, `Sound_LoadStream(0)`, counter `0x96` |
| `InnNight_Wait` | `0x57FC70` | 0x39 | black; the counter to 0, then `Sound_StreamDone`: next, `Transition_Start(1)` |
| `InnNight_Restore` | `0x57FCB0` | 0x69 | the transition over: `Party_RestoreAll(1)`, `0x929F0B = 1`, the repeat latch and timer 0, with an object `Msg_OpenSystem(0xD5)` |
| `InnNight_AskSave` | `0x57FD20` | 0x5D | with an object, the message done; without, message `0x1D` and `Menu_YesNo` |
| `InnNight_Leave` | `0x57FD80` | 0x4D | step + 1 (the save menu) on a yes, + 2 (the way out) on a no |
| `FieldSave_Dispatch` | `0x57FDD0` | 0xE | `jmp [FieldSave_States + 4 0x929F02]` |
| `FieldSave_Begin` | `0x57FDE0` | 0x1F | `Save_ReadSummaries`, counter 4, cursor and first slot 0 (DIV-0002's "state 0") |
| `FieldSave_SlotsIn` | `0x57FE00` | 0x63 | `0x102` on the first frame; the slots lit sliding in from `0x20 - 80 counter` |
| `FieldSave_Choose` | `0x57FE70` | 0x10C | message `0x9D`, the slots; `Input_AutoRepeat(pad & 0x5000)` moves the cursor over 0..15 and scrolls the three shown; confirm `0x104` and state 3 (`0x57FF80`, Capcom's: a yes / no, message `0x9F`), cancel `0x106` and state 6 |
| `FieldSave_SlotsOut` | `0x580150` | 0x4B | the slots unlit sliding out to `0x20 + 80 counter`, then state 7 |
| `FieldSave_End` | `0x5801A0` | 0x89 | an inn whose cursor is not 2 goes back to the prompt (step 1, state 1); else with `0x66C7DA` set and `0x6BC880` clear, message `0xD0` and state 8 (`0x580230`); else step + 1, the way out |

**Where the original's details are kept:**

- **The dispatchers jump.** Each ends in `[[clang::musttail]]`; the built
  object is `movzbl <index>, %eax; jmpl *<table>(,%eax,4)` - the original's
  own two instructions, so a target that is still Capcom's
  (`0x5818B0`, `0x580280`, `0x57FA40`, `0x57FF80` ...) is entered with `eax`
  the index, zero-extended, as before. Every other function has
  `disable_tail_calls`.
- **A dead compare.** `InnPrompt_Choose` computes `0x929F0C == 0` into `edx`
  and compares it with -1 (`sete dl; cmp edx, -1; je`) before clearing the
  counter: never equal, so the branch is dead. Ours has no such test.
- **The pad read as a dword.** `InnPrompt_Choose` loads `Input_Pressed` into
  `ecx` whole; its tests use `ch` and `cx` only, so the word decides.
- **Register leftovers in arguments.** The title box's colour byte is pushed
  in a register whose upper bytes were never cleared (entry `eax`, or `edx`
  after a call); `Menu_DrawTitleBox` uses it only through `Gpu_GetClut`'s
  six-bit field. `FieldSave_SlotsIn` / `SlotsOut` build x with `movzx ax`
  over `Menu_DrawTitleBox`'s return, so its upper half is that call's
  leftover (ours: the true product's); `SaveMenu_DrawSlots` passes x on to
  `0x576960` and `Menu_DrawCursorBox`, which take it as a u16. The
  prompt's slides (`0x57F6F0`, `0x57F810`, `0x57FAB0`) build theirs over the
  dispatcher's `eax`, which is the index, so theirs are clean in the
  original too.
- **The save chooser** compares the old slot's low byte with the new slot's
  whole dword (as `LoadMenu_Choose` does, [`save-menu.md`](save-menu.md) §2),
  so a slot past 255 plays the move sound every frame. Nothing stores one.

## 3. Text

None of the 25 draws a string of its own. Every text is a system message
from the pool that DIV-0008 swaps for the overlay's language
([`dialogue-localisation.md`](dialogue-localisation.md) §7): `0xA1` (the
inn's greeting, with the price as text record 0 - `TextRecord_Set(0, 8,
"%d")` at `0x57F7A7`), `0xA4` (`0x57FB97`, `0x57FC1A`), `0x1D` (`0x57FD54`),
`0x9D` (`0x57FE8C`), all through `Text_DrawAt` at (`0x1C`, `0x16`), and
`Msg_OpenSystem` `0xD1` (`0x57F74E`), `0xD5` (`0x57FD12`), `0xD0`
(`0x58020E`). The only string in the exe they use is the format `"%d"`
(`0x5E10C0`). Nothing for [`dialogue-localisation.md`](dialogue-localisation.md) §6.

## 4. The fuzz and its negative controls

`src/game/shop_states_fuzz.cpp`, under `BOF3X_SHADOW=shop_states`. It makes
25 byte-copies, every call out re-aimed at a recorder (`CloneCall` with
`expected`: 66 sites, none already re-aimed). The five tables' 38 entries,
and the zero dword after `ShopMode_States`, are swapped for recorders for the
test and put back. Each round starts from
random bytes over 22 regions (6,395 bytes): the state block
`0x929EC0..0x929F0F`, `0x903844`, the eight character records, the 30
sprite objects and `Sprite_Current`, `Field_ScriptFlags`, `Game_Step`,
`Field_InputFlags`, the area, the colour, `0x7DEE44`, the print buffer, the
zenny, `0x6BC880..3`, `Input_Pressed`, the two button maps, `0x66C810`, the
repeat latch and timer, the save cursor and first slot, `0x66C7DA`. The
object byte is put back in range (or negative) and `Sprite_Current` on an
object. Then each function's boundaries are seeded:

- the dispatch indices inside their tables, and sometimes on into the next
  ones of the block;
- the object: none, a save point, 0, 29, `0x80`, `0xFD`, any of the 30;
- HP at a quarter of max HP and one either side;
- `Field_InputFlags` 0x40, the areas `0xBC` / `0x85` / `0xC1` and their
  neighbours;
- counters 0, 1, 2, `0x80`, `0xFF` and in the slides' ranges;
- the cursor 0..3, `0x7F`, `0x80`, `0xFF`; the pad's up, down, both and
  neither, with the confirm and cancel maps met or not;
- the zenny at the price, one below and one above;
- the save slot 0, 1, 2, 3, 14, 15, 16, -1, `0x100`, `0x80000000`, with the
  first shown at the slot and one, two and three below;
- `Sound_StreamDone` 0, `0x100` and others; `Menu_YesNo` 0, 1, 2, `0x80`.

**The recorders are loud.** Each logs its arguments with a hash of 30
watched bytes, four dwords (`Sprite_Current`, the zenny, the save cursor and
first slot) and the touched object's `+8` and `+0x80` as they were at
the call, so a store moved across a call shows; two calls in three move one
of those cells, and one in 17 repoints `Sprite_Current`. Where the original
passes a register with unset upper bits the log keeps what the callee uses:
the colour's low byte, x's and y's low words.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    shop_states self-test: 40000 rounds over 25 functions (1600 each), 81197 calls to the stand-ins, 0 MISMATCHES
    coverage: table entries reached 38 of 39; to the save menu at once 1216; inn paid 729, short of zenny 40,
      cancelled 391, cursor moved 1190; save slot moved 1256; asked to save 450; night over 578; message 0xD0 195

(The 39th entry is the zero dword after `ShopMode_States`, swapped only so
that a wrong index lands on a recorder; the original never reaches it.)

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1287 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`DF/controls.py` in the session scratchpad)
planted each one alone in `shop_states.cpp`: replace, build, self-test,
revert. The table gives the rounds that refused each one, of 1,600.

| | Planted | Refused in |
|---|---|---|
| D1 | ShopMode_Dispatch: indexed by the step | 1463 |
| D2 | ShopMode_Dispatch: the table one entry on | 1600 |
| D3 | Inn_Dispatch: indexed by the sub-state | 1420 |
| D4 | InnPrompt_Dispatch: the table one entry on | 1600 |
| D5 | InnNight_Dispatch: indexed by the counter | 1408 |
| D6 | FieldSave_Dispatch: the table one entry back | 1600 |
| B1 | ShopMode_Begin: no object: the kind + 1 | 491 |
| B2 | ShopMode_Begin: the state from +0x19 | 1108 |
| B3 | ShopMode_Begin: the shop number not taken | 1108 |
| B4 | ShopMode_Begin: the low-HP bit at a quarter too | 1396 |
| B5 | ShopMode_Begin: half the max HP | 1590 |
| B6 | ShopMode_Begin: seven records | 834 |
| B7 | ShopMode_Begin: the counter 5 | 1600 |
| B8 | ShopMode_Begin: 0x929F0A kept | 1591 |
| B9 | ShopMode_Begin: shop records 24 bytes | 1597 |
| E1 | ShopMode_End: the facing kept on bit 2 | 566 |
| E2 | ShopMode_End: +0x80 or 0x10 | 395 |
| E3 | ShopMode_End: +0x80 or 8 before the call | 277 |
| E4 | ShopMode_End: script flag bit 9 | 872 |
| E5 | ShopMode_End: Game_Step + 1 before Window_ResetAll | 1600 |
| E6 | ShopMode_End: the facing from +0x84 | 537 |
| N1 | Inn_Begin: any input flag 0x40 bit | 270 |
| N2 | Inn_Begin: area 0xC2 for 0xC1 | 88 |
| N3 | Inn_Begin: the object not kept | 783 |
| N4 | Inn_Begin: an object of 0xFF to the save menu | 295 |
| N5 | Inn_Begin: the counter 3 | 1600 |
| N6 | Inn_Begin: the cursor 1 | 1600 |
| T1 | InnPrompt_TitleIn: 16 a frame | 1492 |
| T2 | InnPrompt_TitleIn: the message for a save point too | 153 |
| T3 | InnPrompt_TitleIn: 0x929F0B = 0 | 257 |
| T4 | InnPrompt_TitleIn: the counter read after the call | 709 |
| G1 | InnPrompt_Greeting: the price 100 a number | 1142 |
| G2 | InnPrompt_Greeting: text record 1 | 1148 |
| G3 | InnPrompt_Greeting: message flag bit 0 | 803 |
| G4 | InnPrompt_Greeting: the sound before the stores | 813 |
| G5 | InnPrompt_Greeting: the counter 4 | 813 |
| C1 | InnPrompt_ChoicesIn: 40 a frame | 1489 |
| C2 | InnPrompt_ChoicesIn: no zenny box | 1181 |
| C3 | InnPrompt_ChoicesIn: the colour read before the choices | 797 |
| H1 | InnPrompt_Choose: wrap to 1 | 275 |
| H2 | InnPrompt_Choose: wrap past 1 | 91 |
| H3 | InnPrompt_Choose: down after up too | 386 |
| H4 | InnPrompt_Choose: the pad not re-read | 182 |
| H5 | InnPrompt_Choose: the cursor not re-read | 129 |
| H6 | InnPrompt_Choose: the exact price short | 28 |
| H7 | InnPrompt_Choose: the night free | 150 |
| H8 | InnPrompt_Choose: no buzzer | 40 |
| H9 | InnPrompt_Choose: cancel not flagged | 391 |
| H10 | InnPrompt_Choose: the wrap unsigned | 111 |
| H11 | InnPrompt_Choose: the zenny read before the sound | 74 |
| O1 | InnPrompt_ChoicesOut: 32 a frame | 1486 |
| O2 | InnPrompt_ChoicesOut: done at 5 | 425 |
| O3 | InnPrompt_ChoicesOut: the night counter 0x2E | 25 |
| O4 | InnPrompt_ChoicesOut: cancel to step + 2 | 101 |
| O5 | InnPrompt_ChoicesOut: step + cursor | 103 |
| O6 | InnPrompt_ChoicesOut: the zenny box always | 424 |
| M1 | InnNight_Message: transition 1 | 268 |
| M2 | InnNight_Message: message 0xA5 | 1169 |
| M3 | InnNight_Message: the transition before the store | 268 |
| F1 | InnNight_FadeOut: the fade word as a byte | 349 |
| F2 | InnNight_FadeOut: the box with no object | 349 |
| J1 | InnNight_Jingle: stream 1 | 1600 |
| J2 | InnNight_Jingle: the counter 0x95 | 1600 |
| J3 | InnNight_Jingle: the sub-state read before the stream | 829 |
| W1 | InnNight_Wait: the counter below 0 | 458 |
| W2 | InnNight_Wait: the answer as a byte | 109 |
| W3 | InnNight_Wait: transition 0 | 578 |
| R1 | InnNight_Restore: a partial restore | 359 |
| R2 | InnNight_Restore: the repeat timer kept | 359 |
| R3 | InnNight_Restore: message 0xD4 | 320 |
| R4 | InnNight_Restore: the object read before the restore | 43 |
| A1 | InnNight_AskSave: message 0x1E | 450 |
| A2 | InnNight_AskSave: only an answer of 1 | 121 |
| A3 | InnNight_AskSave: message flag bit 2 | 571 |
| L1 | InnNight_Leave: yes and no swapped | 1600 |
| L2 | InnNight_Leave: the counter kept | 1494 |
| S1 | FieldSave_Begin: the counter 3 | 1600 |
| S2 | FieldSave_Begin: the first slot 1 | 1600 |
| S3 | FieldSave_Begin: the stores before the read | 1587 |
| I1 | FieldSave_SlotsIn: the sound at 3 | 422 |
| I2 | FieldSave_SlotsIn: 64 a frame | 1469 |
| I3 | FieldSave_SlotsIn: the counter read before the box | 720 |
| X1 | FieldSave_Choose: up only from a positive slot | 53 |
| X2 | FieldSave_Choose: the first follows at equal | 45 |
| X3 | FieldSave_Choose: sixteen slots | 23 |
| X4 | FieldSave_Choose: the scroll unsigned | 97 |
| X5 | FieldSave_Choose: bytes compared | 43 |
| X6 | FieldSave_Choose: cancel to state 5 | 407 |
| X7 | FieldSave_Choose: 0x929F0B = 1 | 803 |
| X8 | FieldSave_Choose: the whole pad to the repeat | 1391 |
| X9 | FieldSave_Choose: the pad read before the sound | 203 |
| X10 | FieldSave_Choose: the slot read before the repeat | 613 |
| U1 | FieldSave_SlotsOut: 64 a frame | 1436 |
| U2 | FieldSave_SlotsOut: done at 5 | 449 |
| U3 | FieldSave_SlotsOut: the slots lit | 1600 |
| Z1 | FieldSave_End: no object instead of a save point | 641 |
| Z2 | FieldSave_End: back to prompt state 0 | 911 |
| Z3 | FieldSave_End: either flag | 311 |
| Z4 | FieldSave_End: state 7 | 195 |
| Z5 | FieldSave_End: message 0xD1 | 195 |
| Z6 | FieldSave_End: the counter kept | 456 |
| K1 | TitleBox (every state): the box 0x117 wide | 13 functions, 20095 rounds |
| K2 | Message (every state): x 0x1D | 7 functions, 7658 rounds |
| K3 | Zenny (every state): the zenny box at 0x63 | 4 functions, 4720 rounds |

**The first two runs, for the record.** In the first (with quieter
recorders) 13 controls were refused only by a crash or in fewer than 50 rounds:
D1, D2, D3 and D5 by an access violation - an index past the swapped
entries jumped to a 0 or to data - and H5 (2), F1 (3), R4 (6), H4 (16),
X10 (17), X9 (22), C3 (29), T4 (41), I3 (30). The fix was in the fuzz, not
in ours: every dispatch index is now kept inside the swapped entries (and
`ShopMode_States`' trailing zero swapped too), the watch hash takes the
touched object's `+8` and `+0x80`, the fade word is seeded with only one of
its bytes set, and eight stand-ins each move the cell their caller reads
again after them. The second run left H11 at 1, until the sound's stand-in
moved the zenny too. The table is the third run, with the fuzz as committed.

**The thinnest now:** X3 (23), O3 (25), H6 (28), H8 (40), R4 (43), X5 (43)
and X2 (45) - each needs two seeded edges at once (a slot of 14 or 15 with
down pressed; cursor 0 at the sixth frame; the zenny exactly the price with
confirm on cursor 0).

**What the fuzz cannot see:**

- anything the callees really do, and what the tables' other entries do;
- the order of stores with no call between them;
- the upper bits the original leaves in the pushed colour and x (above);
- the dead compare in `InnPrompt_Choose` (it has no effect to see).

## 5. Found on the way

- **No defect.** Nothing here differs from what the code evidently means.
  The two quirks (the dead compare, the byte-against-dword slot test) cannot
  be seen in play.
- **The inn's price is ten times the shop number** (`0x929EC3`, the
  object's `+0x1C`), and the same number picks the 23-byte record at
  `0x658930` for the shop (`0x903844`). What the record holds was not read.
- **After a save at an inn** the menu goes back to the prompt unless the
  cursor is 2 - and the save menu's state 4 (`0x580010`, Capcom's) sets the
  cursor to 2 after writing (`mov byte [0x6BC881], 2` at `0x5800B8`). So a
  save made from "save" returns to the prompt only when it was cancelled.
  By the code; not seen.

## 6. For the merger

### `entries_logic.txt`

Appended to the main checkout's `analysis/calltrace/entries_logic.txt`
(2026-09-25), under a `# --- ... group DF` header:

    0057F500 11
    0057F520 A4
    0057F5D0 75
    0057F650 E
    0057F660 78
    0057F6E0 E
    0057F6F0 65
    0057F760 A6
    0057F810 84
    0057F8A0 191
    0057FAB0 AA
    0057FB60 E
    0057FB70 65
    0057FBE0 54
    0057FC40 23
    0057FC70 39
    0057FCB0 69
    0057FD20 5D
    0057FD80 4D
    0057FDD0 E
    0057FDE0 1F
    0057FE00 63
    0057FE70 10C
    00580150 4B
    005801A0 89

`0057F340 12E8` (no group's) overlaps all of them; its body is `D5`, to
`0x57F414`.

### Left original, and why

Every function of the group is ours. Inside the group's extents, and in its
tables, these stay Capcom's - none is in any group's list (the route did not
enter them, or `pe_hidden.py` gave them no start):

| Entry | What | Where |
|---|---|---|
| `0x57F340` | the recorded start the catalogue folded all this into; `0xD5` bytes: the bytes `0x93993D..F` stepped by `0x9399E7`, `0x57EEF0(0x9398E0)`, the three put back (unread beyond that) | none |
| `0x57F420`, `0x57F450`, `0x57F4E0`, `0x57F4F0` | the four entries of the table ending at `0x663E3C`, each ending in a jump or call to `0x57F340` | `.data 0x663E30..` |
| `0x57FA40` | `InnPrompt_States` 4, after the buzzer: message `0xA2`, back to the chooser on any button | `0x663F70` |
| `0x57FF80`, `0x580010`, `0x5800D0` | `FieldSave_States` 3, 4, 5: a yes / no (message `0x9F`); the write (`0x5806F0`, the name from `0x664068`, `Save_WriteFile(0x904BA0, 0x12B0)`, the summary copied, the inn's cursor 2); message `0x8F` and `0x6BC880 = 1` | `0x663FA0..` |
| `0x580230` | `FieldSave_States` 8: after message `0xD0`, by `0x929F0B` | `0x663FB4` |
| `0x580280` | `Inn_Steps` 4: the way out | `0x663F5C` |

### Other groups' addresses (said, not acted on)

- `0x517300` (DB): `Game_Mode` 7's step 1 **tail-jumps** into this group's
  `0x57F500` (`0x517323`); its return is `Field_Task`'s.
- `0x5818B0` (DG) is `ShopMode_States` 2 - the shop's state machine, for an
  object of kind 0.
- The queue's row for `0x57F500` says "indirect call" with no pointer: it is
  jumped to by `jmp rel32` from `0x517323` and `0x56D4D5`.
- `0x56D4D0` is in no group; it calls `0x59E230` and jumps to `0x57F500`.

## 7. For the batch check

The shop route reaches all 25 (the queue's trace). The fuzz alone covers the
forced save point (`Field_InputFlags` 0x40 and the three areas), the object
of `0xFF` paths, the "not enough zenny" and cancel branches and the save
slot scroll's ends.
