# Top-level modes, the system choice and the field core

**Status:** IN PROGRESS (2026-09-25) - twenty functions ours
(`src/game/mode_states.cpp`), each fuzzed at start-up against a copy of
Capcom's (`BOF3X_SHADOW=mode_states`, 43,500 rounds, 0 mismatches), with
107 negative controls, 104 refused by a count and 3 that change nothing (section 4.1). With `BOF3X_SHADOW='*'` the self-test exits 0 with 1,282
injects. No divergence. The live check (the shop and world-map routes' A/B)
is the coordinator's, after the merge.

Group DB of the eighth round's wave B
([`takeover-queue-round8.md`](takeover-queue-round8.md), "DB"). The queue
listed 21 entries; **20 are functions and are ours; `0x497C30` is not a
function** (section 1.6). Every claim about the binary is from capstone over
`bof3/BOF3.exe` on 2026-09-25: `tools/pe_disasm.py` for the bodies, and
three scripts in the worktree's gitignored `analysis/DB/` (`calls.py` lists
every rel32 call and jump leaving a range, `xr.py` every E8/E9 site and
dword naming an address, `pr.py` every dword pointing into a range).

The model is round seven's [`battle_flow.md`](battle_flow.md). It continues
[`mode-flow.md`](mode-flow.md) (the field task and `GameMode_Handlers`),
[`mode-tasks.md`](mode-tasks.md), [`msgbox.md`](msgbox.md) and
[`item-use.md`](item-use.md) (the system choice's callers and handlers),
[`field-frame.md`](field-frame.md), [`field-modes.md`](field-modes.md) (the
scenario chapters) and [`window-task.md`](window-task.md).

## 1. What is ours

"Bytes" is each body to its last instruction (jump tables included); the
queue's extents are `pe_hidden.py`'s, to the next start.

| Entry | Name | Bytes | Queue said | Reached from | Does |
|---|---|--:|--:|---|---|
| `0x496250` | `GameMode_LookEnd` | 0x32 | 64 | mode 6 `0x496230`, step 1 | `Look_Return`; back at rest, mode 2 |
| `0x496290` | `GameMode_Shop` | 0xF | 16 | `GameMode_Handlers` entry 7 | jump through `GameMode_ShopSteps` on `Game_Step` |
| `0x4962A0` | `Shop_Open` | 0xE5 | 240 | `GameMode_ShopSteps` 0 | the shop's file, or straight back to the field |
| `0x517300` | `Shop_Frame` | 0x28 | 48 | `GameMode_ShopSteps` 1 | the field drawn, then the shop overlay `0x57F500` |
| `0x496390` | `Shop_Close` | 0x95 | 160 | `GameMode_ShopSteps` 2 | the area's sound bank back, mode 2 |
| `0x496830` | `Menu_WaitTransition` | 0x3A | 58 | mode 3's steps 0 and 2 | `Field_WaitTransition` with the menu's frame |
| `0x5172F0` | `Menu_Frame` | 0xF | 16 | mode 3's step 1 (`0x656A78`) | `Field_ModeDispatch`, the menu `0x589970`, the task records |
| `0x496A00` | `Look_PadControl` | 0xC1 | 193 | mode 6 step 0, mode 11 step 1 | the pad turns the camera; release ends the look |
| `0x496AD0` | `Look_Return` | 0x88 | 144 | `GameMode_LookEnd`, mode 11 step 2 | one frame of the camera's way back |
| `0x498A30` | `MsgBox_SystemChoice` | 0x98 | 160 | the two message-box commits | choice ids `0x80..0x90` (section 2) |
| `0x516E70` | `Text_DrawSmall` | 0xEE | 238 | 18 call sites | the 8 px UI string draw (section 1.3) |
| `0x517290` | `Field_LoadingFrame` | 0x23 | 35 | 16 call sites and a tail jump | a field frame for the frames a load waits |
| `0x525370` | `FieldCore_State2` | 0x12 | 18 | state 2 of the members' and the leader's tables | jump through `FieldCore_State2Steps` on `+2` |
| `0x5258B0` | `FieldCore_Fade` | 0x12 | 32 | `FieldCore_State2Steps` 2 | jump through `FieldCore_FadeSteps` on `+3` |
| `0x5258D0` | `FieldCore_FadeBegin` | 0x4F | 80 | `FieldCore_FadeSteps` 0 | the shade fade started |
| `0x525920` | `FieldCore_FadeStep` | 0x39 | 64 | `FieldCore_FadeSteps` 1 | the fade stepped; done, back to state 1 |
| `0x539AD0` | `Scena01_Frame` | 0xE | 16 | chapter 1's vtable slot 0 | jump through `Scena01_States` on `0x8034E2` |
| `0x539B20` | `Scena01_EnterArea` | **0x788** | 416 | `Scena01_States` 1 | the scene the chapter plays next, per area |
| `0x53D830` | `Scena01_StepHook` | 0x4EE | 1264 | chapter 1's vtable slot 2 | a step that starts a scene (section 1.5) |
| `0x5960D0` | `Window_DrawCursor` | 0x50 | 80 | the tail jump at `0x595C44` | the window kind 2's pointing hand |

The names are by what the code does and who reaches it. The shop reading of
mode 7 is from three facts: `Field_ObjectIdle` sets `Game_Mode` 7 with the
object's index in `0x929F0C` ([`object-kinds.md`](object-kinds.md), where
the reading was a guess), the recorded shop route reaches all three steps
and nothing else reaches them, and step 1 runs the shop overlay's dispatch
`0x57F500` (group DF's). The look-around reading of mode 6 is the code's:
while a button of the map word `0x903586` is held the pad turns the camera,
and on release it swings back to yaw `0xFD56`, pitch `0x200`. Which button
that is was not measured.

### 1.1 The modes (6, 7 and the menu's wait)

- **`Look_PadControl`**: with none of the map word's buttons held in
  `Input_Held`, the look ends - the pitch word (`Camera_Angles[2]`,
  `Cond_AngleFB`'s low half) above `0x200` sets the turn byte `0x66C7D9` 0,
  below it 1, equal leaves it - and `Game_Step` + 1. Otherwise bits 14 / 12 /
  13 / 15 of the held word step the yaw by -22 while above `0xFD56`, +22
  while below `0xFE39`, the pitch +22 while below `0x355` and -22 while
  above `0xAA` - signed 16-bit compares, so a word may end up to 21 past its
  limit - each word with its `Light_AnglesCopy` twin.
- **`Look_Return`**: the yaw 22 down while above `0xFD56`, else snapped to
  it; the pitch 22 towards `0x200` (down with the turn byte 0, up
  otherwise) and snapped once at or past it. **`GameMode_LookEnd`** runs it
  and, with both words home, sets mode 2, `Field_Request` 0, `Game_Step` 0.
  Mode 11 (`0x496780`, not this group's) uses the same two functions for its
  steps 1 and 2.
- **`Shop_Open`**: the s8 `0x929F0C` is the object the shop was opened on.
  If it is an object (`>= 0`) it becomes `Sprite_Current`, and when its
  dword `+0x18` is 9 there is no shop: mode 2 at once, the object turned to
  face `+0x85` (unless `+7` bit 3; then `+0x80` bit 3 set), `+0x80` bit 5
  cleared, `Field_ScriptFlags` bit 8 cleared, a field frame. Otherwise DAT
  `0x31B` is loaded - `Field_LoadingFrame` and a sleep each frame of the
  wait - CLUT strip rows 1 and 2 are kept, a field frame, and step 1.
- **`Shop_Frame`**: `AreaMap_Frame`, `Party_ExtraScreens`,
  `Party_UpdateScreens`, `Field_ObjectsScreen`, `Effect_RunObjects`,
  `Field_DrawFrame`, `Field_RunTaskRecords`, then a tail jump into
  `0x57F500`. The objects' logic and the event script do not run.
- **`Shop_Close`**: the party combination byte `0x90412C` gets bit 7 and the
  sound bank `0x2C2 + (it & 0x7F)` is loaded with the framed wait (the shop's
  file had replaced the area's); with `Field_InputFlags` bit 4, DAT `0x12A`
  too, and the unread `0x4560D0`; then mode 2 and a tail jump to
  `Field_Frame`.
- **`Menu_WaitTransition(run)`** is `Field_WaitTransition`
  ([`mode-flow.md`](mode-flow.md) section 3) with `Menu_Frame` for the
  field's frame. **`Menu_Frame`** is mode 3's step 1: `Field_ModeDispatch`,
  the menu's state dispatch `0x589970` (group DH's), a tail jump to
  `Field_RunTaskRecords` - the menu runs with the event script
  ([`menu-screens.md`](menu-screens.md) section 1 said so).

### 1.2 The field's per-frame entries

`Field_LoadingFrame` (already named, from `Scenario_Start`) is `AreaMap_Frame`,
`Party_ExtraScreens`, `Party_UpdateScreens`, `Field_ObjectsScreen`,
`Effect_RunObjects`, `MoveScript_TintFrame` and a tail jump to
`Field_DrawFrame` - `Field_Frame` without the objects' logic. Its 16 call
sites are the load waits of modes 4..11 and three others (`0x544B1D`,
`0x551C60`, `0x56D645`).

### 1.3 `Text_DrawSmall`, the 8 px UI draw

`(x, y, colour, count, text)`, returning the text end - the draw the
dialogue-localisation work kept calling "the unowned 8-unit glyph draw
`0x516E70`" ([`dialogue-localisation.md`](dialogue-localisation.md)
sections 6 and 9, DIV-0016, DIV-0017). **It draws text**: the shipped
Chinese small text goes through it from 18 call sites, `0x42DA35` ..
`0x59AC38` - among them the Config screen's small rows (`0x4618F0`,
`0x461A29`), the battle windows' (`0x44318E`, `0x443CA4`, `0x443ED1`,
`0x4440AE`, the enemy status windows among them) and field-side windows in
`0x5736FF`..`0x59AC38` (not each read). The address that section 6 wants is `0x516E70`;
nothing here translates or expands anything - pair codes are the two enemy
windows' callers' business (`TextPairs_Expand`, already in place), and
ours draws exactly what Capcom's did. `text_pairs.h`'s comment calls the
draw "Capcom's"; it is ours now, the same behaviour.

Per byte, while the count byte is not 0: `0x01` a newline (x back to the
argument, y + 9), `0x20` a blank 8 wide, a byte with bit 7 a two-byte glyph
`((b0 & 0x7F) << 8) + b1`, anything else glyph `b - 0x26`. A glyph index goes
to word `+0x16` of the primitive at `Gfx_PacketNext` and the quad to
`Text_EmitGlyph(x, y, 8, 8 - v, u, v, 0x7800 | (colour & 0xF))`, with u and
v the byte pair `0x65F5A8[colour >> 4]`; then x + 8. A newline and a blank
use up the count like a glyph. It returns one past the NUL that ended the
text - or, when the count ran out, one past the byte after the last one
drawn (which was not looked at). Unlike `Text_DrawString`, there is no glyph
bound (DIV-0016's `0xA00` test is not in this draw). The original pushes u,
v and `8 - v` with the registers' upper halves left over; `Text_EmitGlyph`
uses their low words, so ours passes them clean.

### 1.4 The field core's state 2 and its fade

`FieldCore_State2` is entry 2 of **both** the members' state table `0x65F960`
and the leader's `0x660918` (the catalogue folded it into `0x525150`): a tail
jump on `Sprite_Current +2` through `FieldCore_State2Steps` (9 entries).
`FieldCore_Fade` is that table's entry 2, a jump on `+3` through
`FieldCore_FadeSteps` (2). `FieldCore_FadeBegin`: `Sprite_ShadeFadeBegin`,
the shade bytes `+0x5D..+0x5F` `0x80` and `+0x5C` 1, `Field_State +0x137` 7,
`+0` bit 6 off, `+3` 1. `FieldCore_FadeStep`: `Sprite_ShadeFadeStep(8)`, and
once it answers non-zero `Field_State +0x137` 0 and the object back to state
1 with both sub-states 0. al is 0 on both of the step's returns; no caller of
the state chain reads it (`0x5173AE`, `0x533525`).

### 1.5 Scenario chapter 1

`0x662C80` entry 1 is `Scena01_Hooks` `0x660D68`, the chapter's five-slot
vtable (as scenario 16's, [`field-modes.md`](field-modes.md) section 1): slot
0 `Scena01_Frame`, slot 1 `0x53D470` (the object hook), slot 2
`Scena01_StepHook`, slot 3 `Scenario_NoHook`, slot 4 `0x53DD20` (the cell
hook). `Scena01_Frame` jumps on the s8 `0x8034E2` through `Scena01_States`
`0x660D7C`: 0 `0x539AE0` (the start), 1 `Scena01_EnterArea`, 2 `0x53A2B0`
(the run, whose own table starts at `0x660D88`, one word on - the overlap
scenario 16 has too).

**`Scena01_EnterArea`** runs once per area entered and always ends with
state 2. By the area (`Game_AreaNumber` 0, 5, 7, 8, 9, `0xA`, `0xD`, `0xE`,
`0x10`, `0x11`, `0x16`, `0x17`, `0x21`, re-read at each test), the script
counter `0x90384A` and scenario flags (`Flags_Test` on the bank dword
`0x929ED0`), it sets the next scene - the run `MoveScript_Var7` and its step
`0x8034E5` - and clears counters, sets the pass flags, script flag bits,
loads music (`Music_LoadFile` 0 or 8 with the load wait), fades it
(`Music_FadeOutStop(0xA)`), moves the camera (distance + `0xA00`, +
`0xB80`), places an effect (kinds `0x12`, `0x11`), sets flags `0xB` `0xD`
`0x1C` `0x39` `0x3F`, calls `Scenario_CallA` 1 or 2, or (area `0x17`,
counter 4) `AreaMap_SetupEntries` and `MoveCmd_TestFB(0x49, 0xC)`. The body
is 0x788 bytes, not the catalogue's 416: `0x539CC0`, where the catalogue
ends it, is case 1 of its first jump table. What each scene is was not read.

**`Scena01_StepHook(x, z)`** (16.16, from `Scenario_StepHook`): 1 when the
step starts a scene, else 0. Area `0xA`: flag `0x1C` clear, x `0x698000`, z's
high word 6 or 7. Area 7 (flag `0x1B` clear): three door cells. Area 8: with
flag 6 clear, x `0x548000` at z word `0x1C` / `0x1D` (and sound `0x202`); with
it set and flag 5 clear, a rectangle to area 8 (`Field_ChangeArea(8,
0x340000, 0x178000, 0x83)`, script flags `^ 6` and bit 7 off). Area `0x13`:
two rectangles by flags `0x1B`, 2, 3. Area `0x17`: a rectangle to area
`0x17`. Area `0x16` (flag `0xC` set): three places by flags `0xD`, `0xF`,
`0x12`, `0x10`. Area `0x60`: flag `0x18` set over a strip, answer 0. Every
bound is inclusive and signed. Every hit but area `0x16`'s first calls
`ScriptFlags_Set40` first; that one does not, and stores counter 0 from the
flag test's answer (which is 0 on that path) - as the original has it.

### 1.6 `0x497C30` is not a function

It is case `0x14` of `MsgBox_StatePrint`'s jump table (`0x497E34`, entry 20
at `0x497E84`, [`msgbox.md`](msgbox.md)): `xor bl, bl; inc eax` into the
code-consumer's registers and a `jmp 0x497E12` back into the host.
`MsgBox_StatePrint` `0x497B30` is ours since round three
(`src/game/msgbox.cpp`, its `case 0x14`), so this code only ever runs with
`BOF3X_ORIGINAL=MsgBox_StatePrint`. It goes with its host - which has gone.
The queue's "OS callback" is the first-call trace seeing a case entered by a
`jmp [ecx*4 + table]`.

### 1.7 `Window_DrawCursor`

`Menu_DrawHand` at the window `0x905B84`'s x + 4 and y plus the list set's
first row plus stride times the cursor row: record `0x7DEE65` of the
6-byte table `0x66AE2C` (+2 the first row, +4 the stride), the s8 row
`0x7DEE67`; the window's `+4` / `+6` are 12.4 words shifted as signed. The
arithmetic is 16-bit; the original's coordinates carry the registers'
leftover upper halves, and `Menu_DrawHand` reads the low words
([`char-stats.md`](char-stats.md)), so ours passes them zero-extended.

## 2. `MsgBox_SystemChoice` and D22

The original stores sixteen handler addresses on its stack and calls
`[esp + 4 * id - 0x200]` for the choice id `0x7DEE64`, unbounded
([`known-defects.md`](known-defects.md) D22). Round six left it Capcom's:
"no faithful C++ for ids `0x90` and up". What each id does:

- **`0x80..0x8F`**: the handler `MsgBox_SysChoice80..8F` (`item_use.cpp`).
  Ours calls it.
- **`0x90`**: the word above the table is the function's own return address,
  so the caller's tail runs as a function, returns into the choice, which
  returns to the same tail: the tail runs twice (D22's "the sub-state
  advances by two"). The two callers' tails, `0x4981F1..0x49822C` and
  `0x4983F1..0x49840B`, are absolute stores and a `ret` - no stack word
  touched - so this is reproducible in C++: ours calls
  `__builtin_return_address(0)` when it is one of those two, and returns.
  **This is exact when the commit that called is Capcom's**
  (`BOF3X_ORIGINAL=MsgBox_ChoiceCommit` or `MsgBox_MenuCommit`).
- **`0x90` from any other caller, `0x91` and up, and below `0x80`**: abort
  through `bof3::Fatal` (CLAUDE.md rule 4, as `Transition_Task` and
  `Battle_PhaseDispatch` do past their stack tables). This is what stops a
  faithful C++ here, precisely: **both commits are ours** (`item_use.cpp`
  since round six, calling `0x498A30` by address from `CommitChoice`), so
  the frame the original would call into for `0x90` is compiled code of
  ours after a call, and `0x91` and up call further words of that frame -
  the commit's return address into `MsgBox_State4` / `State5` (also ours),
  then saved registers. In the build as it ships there is no Capcom frame
  left whose behaviour could be reproduced; Capcom's `0x498A30` there would
  jump into the middle of our functions. Below `0x80` is unreachable (both
  commits test `>= 0x80` first).

No DIVERGENCE entry: ids `0x80..0x90` are exactly the original, and the
abort replaces behaviour that was already undefined with ours in place.
Which ids the scripts give code `0x14` is still unmeasured (D22's "why it
never shows"); a script with `0x90` or more would now stop the game with a
log line naming the id instead of corrupting the message box.

## 3. Tables named

`symbols.toml` `[[data]]` entries, `ctype = "unsigned long"`:

| Table | Count | Read by | Entries |
|---|--:|---|---|
| `GameMode_ShopSteps` `0x656AAC` | 3 | `GameMode_Shop` on `Game_Step` | `Shop_Open`, `Shop_Frame`, `Shop_Close`; `0x656AB8` is mode 8's |
| `FieldCore_State2Steps` `0x66011C` | 9 | `FieldCore_State2` on `+2` | `0x525390` `0x5257A0` `FieldCore_Fade` `0x525960` `0x525CA0` `0x5261E0` `0x526490` `0x526A90` `0x526B80` |
| `FieldCore_FadeSteps` `0x660158` | 2 | `FieldCore_Fade` on `+3` | `FieldCore_FadeBegin`, `FieldCore_FadeStep`; `0x660160` is `0x525960`'s |
| `Scena01_Hooks` `0x660D68` | 5 | `Field_ModeDispatch` and the three hook chains, through `0x662C80[1]` | above |
| `Scena01_States` `0x660D7C` | 3 | `Scena01_Frame` on `0x8034E2` | `0x539AE0`, `Scena01_EnterArea`, `0x53A2B0` |

Not named, because they are no group's this round and each holds only one
or none of ours: mode 3's steps `0x656A74` (`0x495BC0`, `Menu_Frame`,
`0x495C50`), mode 8's `0x656AB8`, mode 11's `0x656AE0`; the members' state
table `0x65F960` and the leader's `0x660918` (their entries are split over
DB, DC and DE - a coordinator's call which group names them).

## 4. The fuzz

`mode_states_fuzz.cpp`. Twenty byte-copies, every call and tail jump out
re-aimed at a recording stand-in (`bof3::CloneCall` with `expected`);
`MsgBox_SystemChoice`'s sixteen `mov [esp + k], imm32` checked and re-aimed
at recorders inside its copy; `Scena01_EnterArea`'s three jump tables
(`0x53A24C` 6, `0x53A264` 9, `0x53A288` 8) relocated into its copy; the four
dispatch tables above swapped for recorders for the duration.

**The system choice's id `0x90`.** Both commits are injected before this
fuzz runs, so they cannot be cloned; each is stood in for by a thunk of the
fuzz's own - `call` the choice, then a byte-copy of the commit's tail - one
set calling the choice's copy, one calling ours (whose accepted tails, `g.tails`,
are pointed at the second set). A round of "the commits" gives an id
`0x80..0x90` and runs one side's thunk against the other's: the copy calls
its own return address for `0x90`, ours calls `__builtin_return_address`, and
the tail's stores and the log are compared. 547 of those rounds had id `0x90`.

One round: one function; random bytes in every region any of them touches
(the message box's bytes, 16 sprite objects, the chapter's bytes, the camera
and its copies, 8 effect records, the input word and button map, the
counters, the script flags, the area, the menu block, the task bytes and the
wait word, and two constant tables put back afterwards - the u/v pairs
`0x65F5A8` and the cursor rows `0x66AE2C`); `Sprite_Current`, `Field_State`,
`Gfx_PacketNext` and the window pointer put back inside buffers of the
fuzz's own; each function's boundaries seeded (every bound of every one of
the step hook's thirteen places, one either side, inside and anything; the
areas and counters `Scena01_EnterArea` switches on; the camera words at and
beside every limit; texts of NULs, newlines, blanks, two-byte codes and
glyphs with counts 0, 1, 7, 8, `0xFF` and garbage above the byte; the shop's
object index negative, 0..15, `+0x18` 9 or not). Theirs, then from the same
state ours; the regions, the buffers, the three pointers, the result and the
stand-ins' log compared. The stand-ins disturb what their callers read
again after a call (the step, the mode, the wait word, `Sprite_Current`,
`Field_State`, the camera, the area, the counters, the flag bank's dword,
`Cond_ByteFD`, the packet cursor, the text after the cursor, the input and
script flags). The load and menu waits end after 0..3 answers.

1,500 rounds of each function and of the commits, five times that for
`Scena01_EnterArea` and `Scena01_StepHook`: 43,500 rounds, 97,494 calls to
the stand-ins, 0 mismatches. Coverage (the original's side): all 17 swapped
table slots and all 16 choices; the step hook answered 1 in 668 rounds, from
every place (78, 80, 72, 83, 90, 34, 44, 39, 80, 43, 21 and 4 for area
`0x16`'s third), area changes 178, sounds 90; every area of
`Scena01_EnterArea` entered 281..314 times, with 19 effects placed, 35 music
loads, 41 fades, 13 `Scenario_CallA`, 10 `MoveCmd_TestFB`; 1,219 small texts
drawn (14,449 glyphs); the shop straight back to the field 363 times (287
with the facing).

### 4.1 Negative controls

Each control was planted alone in ours by `analysis/DB/controls.py`: a text
substitution in `mode_states.cpp`, build, headless self-test
(`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=mode_states`), restore. The count is
the function's mismatching rounds; every refused control ended the process
with exit 3 (the self-test's Fatal) by comparison - none by a fault or a
hang.

**107 controls, 104 refused, 3 that change nothing.** Two were rewritten
after the first run: X1 and X2 first read the dispatch byte from the other
sub-state byte, which jumps through a word past the swapped table and
faulted (exit 0xC0000005) - a control refused by a crash, not a count; they
now shuffle inside the table. H15 first dropped z's bit 16, which the
seeded z values never had set in a hit. And E3 (the look's end tested
before `Look_Return`) was blind until the stand-in for `Look_Return` was
made to bring the camera home half the time (commit `2c9c601`).

| # | control | refused |
|---|---|---|
| P1 | pad: pitch > 0x200 taken as >= | `Look_PadControl` 19 |
| P2 | pad: equal pitch sets the turn byte | `Look_PadControl` 52 |
| P3 | pad: yaw-down limit >= | `Look_PadControl` 31 |
| P4 | pad: yaw-up limit unsigned | `Look_PadControl` 120 |
| P5 | pad: pitch-up limit <= | `Look_PadControl` 24 |
| P6 | pad: pitch-down on bit 3 | `Look_PadControl` 281 |
| P7 | pad: button map low byte only | `Look_PadControl` 74 |
| P8 | pad: pitch copy not stepped up | `Look_PadControl` 322 |
| R1 | return: yaw limit >= | `Look_Return` 91 |
| R2 | return: yaw copy snapped to 0xFD57 | `Look_Return` 586 |
| R3 | return: pitch 0x200 goes on with turn 0 | `Look_Return` 47 |
| R4 | return: turn byte tested != 1 | `Look_Return` 701 |
| R5 | return: down snap < | **not refused** - changes nothing: a pitch of exactly 0x200 has returned two lines before, so <= and < agree |
| R6 | return: up step 0x15 | `Look_Return` 363 |
| R7 | return: down copy step 0x15 | `Look_Return` 371 |
| E1 | look end: yaw alone | `GameMode_LookEnd` 266 |
| E2 | look end: Field_Request kept | `GameMode_LookEnd` 510 |
| E3 | look end: tested before the call | `GameMode_LookEnd` 509 |
| S1 | shop: step & 1 | `GameMode_Shop` 462 |
| O1 | open: object 0 not an object | `Shop_Open` 63 |
| O2 | open: +0x18 as a byte | `Shop_Open` 1 |
| O3 | open: +7 bit 4 | `Shop_Open` 189 |
| O4 | open: +0x80 bit 4 cleared | `Shop_Open` 288 |
| O5 | open: script flags bit 0 | `Shop_Open` 280 |
| O6 | open: CLUT row 3 | `Shop_Open` 1119 |
| O7 | open: step before the frame | `Shop_Open` 50 |
| O8 | waits: sleep before the frame | `Shop_Open` 840, `Shop_Close` 1124 |
| O9 | open: Sprite_Current not set | `Shop_Open` 971 |
| C1 | close: bank from the whole byte | `Shop_Close` 1500 |
| C2 | close: input flags bit 5 | `Shop_Close` 745 |
| C3 | close: 0x4560D0 not called | `Shop_Close` 765 |
| C4 | close: Field_Request kept | `Shop_Close` 1420 |
| W1 | wait: or for and | `Menu_WaitTransition` 694 |
| W2 | wait: no second sleep | `Menu_WaitTransition` 551 |
| W3 | wait: the argument as a dword | `Menu_WaitTransition` 551 |
| M1 | choice: the neighbour handler | `MsgBox_SystemChoice` 1500, `the` commits |
| M2 | choice: 0x90 runs the tail once | the commits' rounds 401 |
| T1 | small: empty text returns text | `Text_DrawSmall` 68 |
| T2 | small: newline 8 | `Text_DrawSmall` 425 |
| T3 | small: newline keeps x | `Text_DrawSmall` 395 |
| T4 | small: blank is 0x21 | `Text_DrawSmall` 639 |
| T5 | small: two-byte code keeps bit 7 | `Text_DrawSmall` 1013 |
| T6 | small: glyph b - 0x25 | `Text_DrawSmall` 1112 |
| T7 | small: height 8 - u | `Text_DrawSmall` 1212 |
| T8 | small: CLUT five bits | `Text_DrawSmall` 612 |
| T9 | small: uv by colour >> 5 | `Text_DrawSmall` 1138 |
| T10 | small: count out returns p | `Text_DrawSmall` 847 |
| T11 | small: the whole count | `Text_DrawSmall` 847 |
| T12 | small: glyph to +0x14 | `Text_DrawSmall` 1219 |
| T13 | small: advance 9 | `Text_DrawSmall` 1019 |
| L1 | loading: tint after the draw | `Field_LoadingFrame` 1500 |
| F1 | menu frame: menu before the dispatch | `Menu_Frame` 1500 |
| F2 | shop frame: shop before the records | `Shop_Frame` 1500 |
| F3 | shop frame: no objects screen | `Shop_Frame` 1500 |
| X1 | state 2: sub-state 8 as 0 | `FieldCore_State2` 165 |
| X2 | fade: sub-states swapped | `FieldCore_Fade` 1500 |
| X3 | fade begin: a store before the call | `FieldCore_FadeBegin` 73 |
| X4 | fade begin: +0x137 = 6 | `FieldCore_FadeBegin` 1500 |
| X5 | fade begin: bit 7 cleared | `FieldCore_FadeBegin` 1116 |
| X6 | fade step: step 7 | `FieldCore_FadeStep` 1500 |
| X7 | fade step: +2 kept | `FieldCore_FadeStep` 988 |
| Y1 | frame: state & 1 | `Scena01_Frame` 487 |
| Y2 | enter: state 1 at the end | `Scena01_EnterArea` 7500 |
| Y3 | enter: area 0 flag 0x3B | `Scena01_EnterArea` 22 |
| Y4 | enter: distance + 0xA01 | `Scena01_EnterArea` 13 |
| Y5 | enter: area 0 other counters go on | **not refused** - changes nothing: with the area 0 and no call made on that path, every later area test fails and the exit is the same |
| Y6 | enter: area 5 case 2 or | `Scena01_EnterArea` 15 |
| Y7 | enter: area 5 case 4 run 0x11 | `Scena01_EnterArea` 18 |
| Y8 | enter: area 5 default goes on | **not refused** - changes nothing: the same, for area 5 with counter 3 or 6 and up |
| Y9 | enter: area 7 flag 0x18 | `Scena01_EnterArea` 282 |
| Y10 | enter: area 8 timer kept | `Scena01_EnterArea` 19 |
| Y11 | enter: area 9 counter before the flag | `Scena01_EnterArea` 282 |
| Y12 | enter: area 0xA effect kind 0x13 | `Scena01_EnterArea` 4 |
| Y13 | enter: effect record stride 0x40 | `Scena01_EnterArea` 12 |
| Y14 | enter: area 0xD any Cond_ByteFD | `Scena01_EnterArea` 35 |
| Y15 | enter: area 0xD counter 3 cleared | `Scena01_EnterArea` 46 |
| Y16 | enter: area 0xE distance + 0xA00 | `Scena01_EnterArea` 5 |
| Y17 | enter: area 0x12 for 0x11 | `Scena01_EnterArea` 310 |
| Y18 | enter: area 0x16 flag 0xE | `Scena01_EnterArea` 28 |
| Y19 | enter: area 0x17 call A with no slot | `Scena01_EnterArea` 4 |
| Y20 | enter: area 0x17 test before the set-up | `Scena01_EnterArea` 10 |
| Y21 | enter: area 0x17 test arguments swapped | `Scena01_EnterArea` 10 |
| Y22 | enter: area 0x10 not cleared | `Scena01_EnterArea` 295 |
| Y23 | enter: the last area not re-read | `Scena01_EnterArea` 4 |
| Y24 | enter: area 0x17 bank read once | `Scena01_EnterArea` 1 |
| H1 | hook: area 0xA z 6 only | `Scena01_StepHook` 33 |
| H2 | hook: area 7 z bound exclusive | `Scena01_StepHook` 27 |
| H3 | hook: area 7 x bound exclusive | `Scena01_StepHook` 31 |
| H4 | hook: area 7 one door x | `Scena01_StepHook` 34 |
| H5 | hook: sound 0x203 | `Scena01_StepHook` 90 |
| H6 | hook: bit 7 kept | `Scena01_StepHook` 19 |
| H7 | hook: area change flags 0x82 | `Scena01_StepHook` 34 |
| H8 | hook: counter 0x1F | `Scena01_StepHook` 44 |
| H9 | hook: area 0x13 z bound short | `Scena01_StepHook` 16 |
| H10 | hook: counter 2 = 6 | `Scena01_StepHook` 79 |
| H11 | hook: area 0x16 first place sets 0x40 | `Scena01_StepHook` 43 |
| H12 | hook: area 0x16 one x | `Scena01_StepHook` 10 |
| H13 | hook: flags 0x10 and 0x12 in the other order | `Scena01_StepHook` 160 |
| H14 | hook: area 0x60 flag 0x19 | `Scena01_StepHook` 142 |
| H15 | hook: z low word for the high | `Scena01_StepHook` 168 |
| H16 | hook: area 0x17 flag 0xB | `Scena01_StepHook` 519 |
| H17 | hook: answers 2 | `Scena01_StepHook` 4 |
| H18 | hook: area 8 flag 5 path without flag 6 | `Scena01_StepHook` 368 |
| D1 | cursor: x shifted unsigned | `Window_DrawCursor` 743 |
| D2 | cursor: stride from +2 | `Window_DrawCursor` 1499 |
| D3 | cursor: row unsigned | `Window_DrawCursor` 733 |
| D4 | cursor: third argument 1 | `Window_DrawCursor` 1500 |

## 5. What the fuzz does not reach

- **The real callees.** Every call is a recorder: `Task_Sleep`'s yield, the
  loads, the draws and the shade fade are not run. What is compared is that
  each is called with the same arguments in the same order from the same
  state, and that what is read after it is read afresh.
- **Out-of-table indices.** The four `.data` tables and the stack table are
  indexed without a bound in the original and in ours alike (ours reads the
  same words); the fuzz keeps every index inside the swapped words, since
  past them both sides jump through whatever follows (mode 8's steps,
  `0x525390`'s table, ...). The shop's object index is kept in 0..15 (16 of
  the 128 an s8 allows), the cursor set in 0..7.
- **`MsgBox_SystemChoice`'s aborts** (ids below `0x80`, `0x91` and up, `0x90`
  from another caller) are not run: they end the process.
- **The pixels.** `Text_DrawSmall`'s quads are `Text_EmitGlyph`'s (ours,
  `msgbox.cpp`, fuzzed there); here only its arguments and the glyph word
  are compared.
- **Area `0x16`'s third place** in the step hook needs five flag answers in a
  row and was hit 4 times.

## 6. For `analysis/calltrace/entries_logic.txt`

`analysis/` is gitignored; these lines were appended to the main checkout's
`analysis/calltrace/entries_logic.txt` on 2026-09-25 (with a comment line
naming the group):

```
00496250 32
00496290 F
004962A0 E5
00496390 95
00496830 3A
00496A00 C1
00496AD0 88
00498A30 98
00516E70 EE
00517290 23
005172F0 F
00517300 28
00525370 12
005258B0 12
005258D0 4F
00525920 39
00539AD0 E
00539B20 788
0053D830 4EE
005960D0 50
```

They replace `00496250 59A` (body 0x32; it ran over `0x496290..0x496390`),
`00498A30 A0` (body 0x98) and `005172F0 5A` (body 0xF; `0x517300` is its
own). Listed extents that overlap ours and are no group's: `00525150 232`
(first `ret` at `0x525269`) over `0x525370`; `00525390 1314` over
`0x5258B0..0x525958`; `0053D3B0 404E` (first `ret` at `0x53D462`) over
`0x53D830`; `00596090 BD` (first `ret` at `0x5960C3`) over `0x5960D0`.
`0x539AD0` and `0x539B20` were in no list (after `00539AC0 3`). `0x497C30` is
not listed (section 1.6).

## 7. Found on the way (other groups' addresses - said, not acted on)

- **`0x497C30` is misfiled** as a function in the queue (section 1.6).
- **`0x539CC0`** is a jump-table case of `Scena01_EnterArea`, not a start;
  `pe_hidden.py` cut the host there. If any list has it as a function, it
  goes with `0x539B20`.
- **Unowned neighbours**: the mode handlers `0x495BB0` (mode 3) with its
  steps `0x495BC0` / `0x495C50`, `0x496230` (mode 6), `0x496430` (mode 8),
  `0x496780` (mode 11) with its steps `0x496790` / `0x4967B0` / `0x4967C0`,
  and mode 11's frame `0x5172C0` (0x2D bytes: `0x517350`, `AreaMap_Frame`,
  `0x536F10`, `Party_ExtraScreens`, `Party_UpdateScreens`, `0x5372E0`,
  `Effect_RunObjects`, `MoveScript_TintFrame`, tail `Field_DrawFrame`) are no
  group's. So are chapter 1's `0x539AE0` (state 0: `Scenario_CallA(0)`,
  `Field_ChangeArea(9, 0x570000, 0x40000, 7)`, `0x903F98` and the counters
  0, state 1), `0x53A2B0` (the run), `0x53D470` and `0x53DD20`, and the
  unread `0x4560D0` that `Shop_Close` calls after DAT `0x12A`.
- **`text_pairs.h`** says the enemy windows' 8-unit draw `0x516E70` is
  Capcom's: it is `Text_DrawSmall` now, ours, the same behaviour. The
  comment wants a word when that file is next touched.
- **The members' and the leader's state tables** (`0x65F960`, `0x660918`)
  share their entry 2, `FieldCore_State2`; their other entries are groups
  DC's and DE's and older rounds'. Nobody names the tables this round.

## 8. Behaviours kept that look odd (not defects, no D-number asked)

- `Look_PadControl`'s limits are tested before the step, so a word can end up
  to 21 past its limit (the yaw is snapped back by `Look_Return` on the way
  home; nothing snaps the look's own overshoot).
- `Text_DrawSmall` returns one past the byte after the last one drawn when the
  count runs out, a byte it never looked at.
- `Scena01_StepHook`'s area `0x16` first rectangle changes the area without
  the `ScriptFlags_Set40` every other hit calls.
- `GameMode_Shop`, `FieldCore_State2`, `FieldCore_Fade` and `Scena01_Frame`
  index their tables unchecked; each table is followed by another table's
  code pointers, so an index one past would run a real but wrong handler.
