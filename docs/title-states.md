# The title's state handlers and the scenario start

**Status:** IN PROGRESS (2026-09-22) - seventeen functions ours: the eight
handlers of `Title_States`, the title's two music helpers and the demo's
corner badge, the logo's three draw sets, and the scenario start the demo
goes through with the two functions under it. Each fuzzed against a copy of
Capcom's with every call and tail jump re-aimed at a recorder; 40 negative
controls, all refused. **Through the live batch check `ab22` + `ab22b`** (2026-09-22, the second parallel round - [`HANDOFF.md`](HANDOFF.md)); section 6 had it owed.

This continues [`mode-tasks.md`](mode-tasks.md), which read task 1's body
`Title_Task` `0x4621C0` and the three functions it runs around its state
handler every frame, and listed the eight handlers as "read, left original".
They are taken over here. The task machinery (`Task_Sleep`, `Task_Create`,
the `0x4000`-byte stacks) is [`attract-mode.md`](attract-mode.md) section 2;
task 1's private words (`Title_*`) are the table in `mode-tasks.md`
section 1, with two more named here: `Title_BadgeOn` `0x66C7F8` and
`Title_BlinkCount` `0x66C806`.

## 1. What is ours

| Function | Entry | Bytes | PSX (GAME.EMI) | hidden_b calls |
|---|---|--:|---|--:|
| `Title_StateLoad` (state 0) | `0x462200` | 0x8A | §1 `0x801D0C90` | 3 |
| `Title_StateFadeIn` (1) | `0x462290` | 0x1A | §1 `0x801D0D5C` | 96 |
| `Title_StateBackdrop` (2) | `0x4622B0` | 0x28 | §1 `0x801D0D94` | 1,080 |
| `Title_StateLogo` (3) | `0x4622E0` | 0x20 | §1 `0x801D0DF0` | 2,700 |
| `Title_StateStartDemo` (4) | `0x462300` | 0x6A | §1 `0x801D0E54` | 48 |
| `Title_StateDemo` (5) | `0x462370` | 0x55 | §1 `0x801D0F00` | 12,200 |
| `Title_StateDemoEnd` (6) | `0x4623D0` | 0x20 | §1 `0x801D0FB8` | 0 |
| `Title_StateMenu` (7) | `0x4623F0` | 0x2B | §1 `0x801D1000` | 0 |
| `Title_FadeMusic` | `0x4624B0` | 0x12 | §1 `0x801D1134` | 3 |
| `Title_StopMusic` | `0x4624D0` | 0x1B | §1 `0x801D1184` | 3 |
| `Title_DrawBadge` | `0x4624F0` | 0x6B | §1 `0x801D11E4` | 12,198 |
| `Title_DrawSetA` | `0x462820` | 0x105 | §1 `0x801D12CC` | 2,748 |
| `Title_DrawSetC` | `0x462930` | 0xC2 | §1 `0x801D1494` | 2,748 |
| `Title_DrawSetB` | `0x462A00` | 0x6D | §1 `0x801D1664` | 2,748 |
| `Scenario_Start` | `0x56D5E0` | 0x82 | §0 `0x801A870C` | 3 |
| `Scenario_Load` | `0x56D670` | 0x14 | §0 `0x801A880C` | 3 |
| `Scenario_SubInit` | `0x56D8C0` | 0x5B | §0 `0x801A8C34` | 1 |

Bytes are each body to its last instruction (capstone, 2026-09-22; no jump
tables, every jump but the calls and tail jumps below internal). The
catalogue's sizes (`attract_catalog.md`) include the `nop` padding and, for
`0x462370`, run on into state 6. **State 6 and 7 (`0x4623D0`, `0x4623F0`)
were missing from the round's roster** - `pe_hidden.py` never saw them, as
the attract never calls them - and are taken over here with the other six.
All PSX twins read side by side (capstone MIPS of the sibling's overlay
captures `093aeda4`, GAME.EMI section 1, and `9d00fd19`, section 0); the
table `Title_States` is the PSX's `0x801D1BD4` entry for entry.

## 2. The eight states

`Title_Task` calls `Title_States[Title_State]` once a frame, between
`Title_CheckStart` and the backdrop and logo draws. Every handler moves the
state with a 16-bit `inc` in memory, read after its calls.

| # | Does (as read) |
|--:|---|
| 0 | `Port_DroppedCall(2)` (the PSX's call there, emptied by the port), `LoadDatFile(0x136)` (PSX `0x25F`), sleep a frame at a time until `File_LoadDone` (asked first; on the PC always done), `Gfx_ClutStripRestore` `0x4549B0`, `Gfx_ClutStripDirty` **+ 1** (not = 1), `Music_Play(0x8D, 8)` (the PSX passes a third argument), next; then Fade 1, Scroll `0xC8`, Bright / LogoState / both shades 0, StartArmed 1 |
| 1 | when `Title_Fade` is 2 (the backdrop's fade-in done): next, Timer `0x168` |
| 2 | Timer - 1 as a u16; at 0: next, Fade 3, LogoState 1, Timer `0x384` |
| 3 | Timer - 1; at 0: `Transition_Start(0)`, `Title_FadeMusic`, next |
| 4 | once `MoveScript_WaitWordDA` is 0: Fade / LogoState / StartArmed 0, `Title_StopMusic`, `Port_DroppedCall(0)`, `Effect_ClearAll` `0x5898A0`, the play clock `0x9040C8..CB` (PSX `0x80144FBC`) and `Draw_PassFlags` (PSX `0x8014832A`) zeroed, `Scenario_Start(0x10)`, `Task_Create(0, 0x495800)` - **the demo** - next |
| 5 | task 0 gone (`Task_Records` +0 is 0): state 0 and return. Else Start (`File_LoadDone`, the wait word 0, `Input_Pressed` bit `0x800`): `MoveScript_Var7` 1, `Transition_Start(4)`, `Music_FadeOutStop(8)` (the PSX passes the track and a word `0x80143F20`), next. Then a tail jump to `Title_DrawBadge` |
| 6 | task 0 gone: the byte `0x90412C` `0xFF` (PSX `0x80145020`, the party combination - the sibling's `names/plchar_records.toml`) and state 0; else the badge (tail jump) |
| 7 | once the wait word is 0: `Title_StopMusic`, `Port_DroppedCall(0)`, `Task_Create(0, 0x588E70)` - **the title menu** - and a tail jump to `Task_Exit` `0x5A99AD`, which clears task 1's record and enters the scheduler on its own stack: task 1 ends here |

`Title_FadeMusic`: `Music_FadeOut(0x10)` unless the track byte `0x904131` is
`0xFF`. `Title_StopMusic`: `Music_FadeOutStop(0xA)` and the track `0xFF`,
unless it is already. The PSX does both through a per-track table
(`0x80182832`); the port passes only the frames.

`Title_DrawBadge`: with `Field_Request` 5, `Game_Mode` 2 and the wait word 0
(the field's first area change), `Title_BadgeOn` goes 0 for good; while it
and `Draw_PassFlags` are both set, sprite `0xA` at (`0xC0`, 4) under tpage
`0x9D` - the demo's corner badge. The PSX asks a video-mode function for the
tpage; the port takes one answer. Who sets `Title_BadgeOn` is outside this
group (`0x56B368..0x56B57C`, the field's mode handlers, per `mode-tasks.md`).

### 2.1 The logo's three sets

Called only by `Title_DrawLogo` (ours, `mode_tasks.cpp`) as
`SetA(flag B, shade B)`, `SetB(0x1A, 0x18, flag B, shade B)`,
`SetC(-6, 0x1C, flag A, shade A)`. Every sprite goes through `Title_Sprite`
(ours, `mode_tasks.cpp`) and its colour through `Prim_SetShade`; every draw
mode is `Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, tpage, 0)` with the cursor
read at the call, then `Gfx_CommitPrim(slot, 0xC)`.

- **A** (`0x462820`): tpage `0x2F` / slot 1: sprite 1 at (`0x106`, `0x82`);
  tpage `0xBD` / slot 2: sprites 8 at (`0x14`, `0xD0`) and 9 at (`0xA4`,
  `0xD0`); all semi-transparent by the flag. Then, with `Title_Fade` 0 and
  `Title_LogoState` 2 (read after those sprites): sprite 7 at (`0x30`,
  `0xB8`), opaque, `Title_BlinkCount` + 1 and the sprite's three colour
  bytes from its low byte b - `(b & 0x1F) * 4` rising, or `0x80` minus that
  while bit `0x20` is set: a 64-frame triangle. Otherwise the counter is
  zeroed. Sprite 7 blinking below the logo once it is up is the "press
  Start" prompt, by where it appears (hypothesis).
- **B** (`0x462A00`): tpage `0xBB`: sprites 2 at (x, y) and 3 at
  (x + `0xF0`, y + `0x70`).
- **C** (`0x462930`): while the flag's low byte is set (the logo fading in),
  tpage `0xD9`: sprites `0xF` and `0x10` semi-transparent at x and x + `0xE0`;
  always tpage `0xB9`: sprites 4 and 5 there, semi-transparent by the flag.

The PSX twins choose each tpage through the same video-mode function as the
badge. **The arguments are retyped** from `unsigned char` to dwords in
`symbols.toml`: the original `Title_DrawLogo` pushes dwords with stale upper
bytes (mode-tasks.md section 2.4) and the sets read only the low byte
(`test bl, bl`; `Title_Sprite` masks the semi; `Prim_SetShade` stores a
byte). Ours mask explicitly, so a Capcom caller's garbage cannot reach
clang's assumption that a `char` argument arrives extended.
`mode_tasks.cpp`'s recorders for the sets follow the new types and log the
low bytes.

## 3. The scenario start

`Scenario_Start(chapter)` `0x56D5E0` is how the demo begins (state 4 calls
it with `0x10`), and by its shape how any scenario chapter does.
`Cond_ByteFA` `0x8034E0` - named for its use by the map conditions' kind
`0xFA` - is the **scenario chapter**, the PSX's `s8 0x8014686C`
(the sibling's `names/scenario_records.toml`: file `0x295` + chapter,
`SCENA<cc>.EMI`, a five-slot vtable per chapter that the GAME.EMI thunks
`0x801A8834`.. call). Not renamed here; the name belongs to whoever owns
the conditions.

- The chapter's low byte to `Cond_ByteFA`; bytes `0x8034E1..E5` and words
  `0x8034E6`, `0x8034E8` zeroed (`MoveScript_Var7` is `0x8034E4`,
  `Field_EdgeBitsPrev` `0x8034E8`); the dword at `Cond_Flags` + 8 * chapter
  (sign-extended) zeroed and its address stored at `0x929ED0` (PSX
  `0x80144E84` + 8 * chapter and `0x80146868`).
- `Scenario_Load` `0x56D670`: `LoadDatFile(chapter + 0x306)`, the chapter
  sign-extended (PSX `0x295` + chapter).
- While `File_LoadDone` is 0: `Field_LoadingFrame` `0x517290` (a field frame
  without the event script: `0x56E6C0`, `0x57B780`, `Party_UpdateScreens`,
  `0x5173E0`, `Effect_RunObjects`, `MoveScript_TintFrame`, tail `0x592F00`)
  unless `Game_AreaNumber` is `0xFFFF` or `Field_Request` 5, then
  `Task_Sleep(1)`. On the PC the load is synchronous, so the loop never runs.
- A tail jump to `Field_ModeDispatch` `0x56D690` (group A's): the chapter's
  vtable slot 0, then `0x56D8B0`, which dispatches on the byte `0x9039F2`
  through the table `0x662CD0` (two entries).

`Scenario_SubInit` `0x56D8C0` is that table's entry 0: the byte `0x9039F2` 1
(entry 1, `0x56D920`, from the next frame), `0x9039F0` / `F1` / `F3` / `F4` /
`F5` and the words `0x9039F6` / `F8` zeroed, and the 16 bytes at `0x662CD8`
copied to `0x904030` - `Cond_Flags` + `0xA0`, the PSX's `0x80144F24`, the
story-flag array (the sibling's `Flag_Test(0x80144F24, ..)`). Four dword
copies where the PSX loops over bytes; the two do not overlap, so the order
is unobservable. It runs once in `hidden_b`, reached from state 4 through
`Scenario_Start`'s tail.

## 4. The fuzz

`BOF3X_SHADOW=title_states`, at start-up: seventeen byte-copies (capstone
lists in `kClones`, `src/game/title_states.cpp`), every call and the four
tail jumps out re-aimed at a recorder, none a jump table. The recorders
never switch or create a task: `Task_Sleep`, `Task_Create` and `Task_Exit`
record and return, `File_LoadDone` answers 0 for the round's first 0..3
questions and then 1, `LoadDatFile` and the music record. 34,000 rounds,
2,000 per function; random bytes in every region any of them touches (task
records 0..2 with `Field_Request`, `Game_Mode` and the title's words and
wait word, `Input_Pressed` as a dword, `Draw_PassFlags`,
`Gfx_ClutStripDirty`, `0x8034E0..EF`, `0x929ED0`, `0x9039F0..FF`,
`Cond_Flags` - `0x400` .. + `0x408` (every chapter's dword, the story flags,
the play clock, the party byte, the track), `Game_AreaNumber`, the table at
`0x662CD8`), each branch's boundaries seeded (timers 0 / 1 / 2 / `0x8000` /
`0xFFFF`, fade 2, the pad `0x800` against `0x7FF`, `0xF7FF`, `0x1000`, the
track `0xFF` / `0xFE` / `0x7F`, request 5 against 4 / 6 / `0x85`, mode 2
against `0x102`, the blink counter at `0x1F` / `0x20` / `0x3F` / `0x40` /
`0xFF` / `0x100`, chapters `0x7F` / `0x80` / `0xFF` with stale upper bytes,
flag dwords whose low byte is 0 above garbage), both sides run from the same
state; the regions, two primitive buffers, the packet cursor and the
recorders' log compared. Every recorder but the shade may change one of
nineteen bytes the handlers read or store after a call (the state word,
fade, logo state, armed, badge, blink counter, track, dirty flag, play clock,
pass flags, `MoveScript_Var7`, party byte, request, area, task 0's state);
`Gfx_CommitPrim`'s moves the cursor; `Music_Play`'s sets the track, the
fades' scribble it; `Title_Sprite`'s returns a noisy primitive of its own.

Result (2026-09-22): **0 mismatches**, with a coverage line:

    shadow      title_states self-test: 34000 rounds (2000 per function), 97800 calls to the stand-ins, 0 MISMATCHES; ...
    shadow      title_states coverage: state advanced 0..7: 1303 1477 252 215 931 203 0 1345; demo over 1484,
                ended by Start 265; badge off 104, drawn 863; blinking 493; music faded 2895; load waits 1300

(State 6 never advances the word - it sets 0 - and is in "demo over"; state
7's figure counts `Task_Exit` calls.) `mode_tasks` re-run after the
retyping: its figures unchanged, 0 mismatches. `BOF3X_SHADOW='*'`: exit 0.

**Negative controls**, planted one at a time by a script, built, run
(`BOF3X_SELFTEST_ONLY=1`) and reverted; every one refused with exit 3, and
each only in its own function - mismatching rounds in brackets:

| | Planted | Refused |
|---|---|--:|
| C1 | load: sleeps before the first `File_LoadDone` | 2,000 |
| C2 | load: `Gfx_ClutStripDirty` set to 1, not counted | 1,942 |
| C3 | load: dirty counted before the CLUT restore | 70 |
| C4 | load: state bumped before `Music_Play` | 72 |
| C5 | fade-in: fade 2 or above | 364 |
| C6 | backdrop: timer tested as s16 > 0 | 875 |
| C7 | logo: state bumped before the calls | 17 |
| C8 | logo: no music fade | 252 |
| C9 | start demo: `Draw_PassFlags` not zeroed | 1,221 |
| C10 | start demo: play clock zeroed after `Scenario_Start` | 93 |
| C11 | start demo: fades cleared after the music stop | 76 |
| C12 | start demo: the demo in task slot 1 | 1,319 |
| C40 | start demo: wait word not tested | 681 |
| C13 | demo: Start read as bit `0x1000` | 321 |
| C39 | demo: pad tested against `0x8800` | 116 |
| C14 | demo: no badge after Start ends it | 265 |
| C15 | demo: badge drawn when task 0 is gone | 509 |
| C16 | demo: `MoveScript_Var7` set after the transition | 10 |
| C17 | demo end: party combination not reset | 972 |
| C18 | menu: task created before the music stop | 1,345 |
| C19 | menu: task 1 not ended | 1,345 |
| C20 | fade music: track tested by its low 7 bits | 279 |
| C21 | stop music: `0xFF` stored before the call | 1,427 |
| C22 | badge: `Game_Mode` compared as a byte | 25 |
| C23 | badge: `Draw_PassFlags` bit 0 only | 605 |
| C24 | badge: the request not tested | 181 |
| C25 | set A: the packet cursor read once | 2,000 |
| C26 | set A: blink turns on bit `0x40` | 247 |
| C27 | set A: fade and logo state read before the sprites | 229 |
| C28 | set A: the blinking sprite semi-transparent | 513 |
| C29 | set C: the whole flag dword tested | 833 |
| C30 | set C: right column at x + `0xF0` | 2,000 |
| C31 | set B: second sprite not moved down | 2,000 |
| C32 | scenario: chapter zero-extended for the flag index | 841 |
| C33 | scenario: pending area change not tested | 145 |
| C34 | scenario: sleep before the loading frame | 368 |
| C35 | scenario: word `0x8034E8` kept | 2,000 |
| C36 | scenario load: chapter zero-extended | 857 |
| C37 | sub init: state byte 2 | 2,000 |
| C38 | sub init: 12 bytes copied | 2,000 |

C16 and C7 are refused in few rounds (10, 17): only a recorder that
happens to rewrite `MoveScript_Var7` or the state word between the two
stores tells the orders apart - enough, but the thinnest.

**What the fuzz cannot see:** anything the callees really do. The coroutine
side - `Task_Sleep` returning a frame later on task 1's stack, `Task_Exit`
never returning, `Task_Create` starting the demo or the menu - is not
exercised at start-up at all; only the game runs it. Unobservable and
without a control: the stray pushed arguments left under later calls
(`Port_DroppedCall`'s in states 4 and 7, until one `add esp`), and the
sets' stale upper bytes (ours mask what the callees mask).

## 5. Found on the way

No defect of the 2001 code, and no divergence: everything above is Capcom's
on both platforms or the port's own documented change (DAT numbers, the
music calls, the tpage test). Kept as they are, each a latent edge nothing
reaches:

- `Scenario_Start` sign-extends the chapter for the `Cond_Flags` index (as
  the PSX's `sra` does), so a chapter `0x80..0xFF` would zero a dword up to
  `0x400` bytes below `Cond_Flags`; the one caller here passes `0x10`.
- `Title_StateBackdrop` / `Title_StateLogo` decrement before testing, so a
  timer of 0 would hold 65,535 frames; nothing stores 0 there
  (`Title_CheckStart` cuts to 1).
- `Title_StateLoad` counts `Gfx_ClutStripDirty` up rather than setting it;
  the renderer clears it every rendered frame, so it would take 255 title
  loads without a rendered frame to wrap it to 0. The PSX increments too.

## 6. In game: what the attract cycle reaches

From `hidden_b` (a whole attract cycle, 16,128 frames): states 0-5 round and
round (3 / 96 / 1,080 / 2,700 / 48 / 12,200 calls), the badge 12,198 times
(drawing 9,562 times - `Title_Sprite`'s calls from `0x462557`), the sets
2,748 times each with the blinking sprite on 2,559 frames and set C's
fading pair on 93, `Scenario_Start` / `Scenario_Load` 3 times,
`Scenario_SubInit` once. Not reached, and what would reach it:

- **State 6 and state 7** (`Title_StateDemoEnd`, `Title_StateMenu`): only
  Start reaches them - during the demo for 6, on the logo for 7. Every
  recipe that loads a save goes through the title menu, so through state 7
  (`tools/recipes/field_menu.txt`, `menu_screens.txt`,
  `battle_commands.txt`); one that presses Start during the demo reaches 6.
- State 5's Start branch (`Transition_Start(4)`, `Music_FadeOutStop(8)`):
  the same demo-time Start press.
- The load waits in state 0 and `Scenario_Start` (`File_LoadDone` is always
  1 on the PC): nothing reaches them; fuzz only.
- `Scenario_Start` with any chapter but `0x10`: a new game presumably starts
  another chapter through it (not measured here); `Title_StopMusic` /
  `Title_FadeMusic` on the menu path likewise.

The live batch check (after the merge) should put all seventeen on its
`--original` list and add each to the frame hash's trace list
(`entries_logic.txt`) with the sizes in section 1. Ten of them are hidden (states 0-7 and
`Scenario_SubInit` pointer-reached, the badge tail-jumped; states 6 and 7
in no list at all); whether the trace list armed them before is for the
lead to check, but taking over any logic function re-records the reference
anyway.

## 7. Open

- What sets `Title_BadgeOn` (`0x56B368..0x56B57C`, group A's handlers) and
  what the badge sprite shows.
- The chapters other than `0x10`, and what `0x56D920` (entry 1 of the
  sub-dispatch) does with the flags seeded here.
- The video-mode function the PSX's badge and sets consult (`0x8017BC2C`),
  and so which of its two tpages the port's single one is.
