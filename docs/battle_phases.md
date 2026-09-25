# The battle's frame and the phase table's first half (group CA)

**Status:** IN PROGRESS (2026-09-25). Eighteen functions are ours
(`src/game/battle_phases.cpp`): the battle mode's frame and phases 0, 1 and
2 of `Battle_PhaseDispatch`. Each is fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 18,000 rounds, 0
mismatches. 140 negative controls were planted: 139 were refused by a count, and one is a change that changes nothing (section 4). The live check (the combat A/B) is the
coordinator's, after the merge.

This is group CA of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Every function is a
*faithful* replacement, so no `DIVERGENCE.md` entry is owed. The model is
round seven's [`battle_flow.md`](battle_flow.md), whose
`Battle_PhaseDispatch` calls the three phase entries taken here.

Every claim about the binary comes from capstone over `bof3/BOF3.exe` on
2026-09-25: a linear listing of `0x42E2F0..0x42F250`, each function read to
its last instruction, the three `.data` tables dumped and every pointer to
them searched for (the one reference each has is its dispatch stub). PSX
twins are the catalogue's (`pairs_propagated.json`); the sibling's names
(`names/functions.toml`: `Battle_Init`, `Battle_RoundStart`,
`BattleMenu_ConfirmDispatch`, `Battle_CommitRound`, `Cmd_AutoBattle_Begin`)
were read and their decomp notes compared term by term where they exist.
No PSX code was disassembled for this group.

## 1. What is ours

| Function | Entry | Bytes | PSX | Reached as |
|---|---|--:|---|---|
| `Battle_Frame` | `0x42E370` | 0x84 | none paired | state 5 of `0x656A84`, jumped to by `0x495E98` |
| `BattleStart_Dispatch` | `0x42E470` | 0x22 | none paired | phase 0 |
| `Battle_Init` | `0x42E4A0` | 0x285 | `Battle_Init` `0x801D1228` | phase 0 step 0 (stack table) |
| `BattleIntro_Dispatch` | `0x42E730` | 0x32 | none paired | phase 0 step 1 (stack table) |
| `BattleIntro_OpenWindows` | `0x42E770` | 0x145 | `0x801D1820` | intro 0 (stack table) |
| `BattleIntro_WaitWindow` | `0x42E8C0` | 0x10 | `0x801D1AAC` | intro 1 |
| `BattleIntro_PrepareCross` | `0x42E8D0` | 0x5F | `0x801D1AE0` | intro 2 |
| `BattleIntro_Finish` | `0x42E930` | 0x5E | `0x801D1B8C` | intro 3 |
| `BattleInput_Dispatch` | `0x42E990` | 0xE | none paired | phase 1 |
| `Battle_RoundStart` | `0x42E9A0` | 0x33 | `Battle_RoundStart` `0x801D1C88` | `Battle_InputSteps[0]` |
| `BattleInput_NextMember` | `0x42E9E0` | 0xE9 | `0x801D1D08` | `Battle_InputSteps[1]` |
| `BattleMenu_CommandSelect` | `0x42EAD0` | 0x2BB | `0x801D1E84` | `Battle_InputSteps[2]` |
| `BattleMenu_ConfirmDispatch` | `0x42EED0` | 0x13 | `BattleMenu_ConfirmDispatch` `0x801D24CC` | `Battle_InputSteps[4]` |
| `Cmd_ConfirmDefend` | `0x42EEF0` | 0x53 | `0x801D2514` | `Battle_MenuSteps[4]` |
| `BattleCommit_Dispatch` | `0x42F070` | 0xE | none paired | phase 2 |
| `Battle_CommitRound` | `0x42F080` | 0xB0 | `Battle_CommitRound` `0x801D2774` | `Battle_CommitSteps[0]` |
| `BattleCommit_QueueMessages` | `0x42F130` | 0x92 | `0x801D28E0` | `Battle_CommitSteps[1]` |
| `BattleCommit_WaitLoad` | `0x42F1D0` | 0x44 | `0x801D2A1C` | `Battle_CommitSteps[2]` |

All void(void), cdecl. Seventeen are "jumped to" in the catalogue: every
one's first return address is `Battle_PhaseDispatch`'s, whether the entry is
reached by that function's call (the three phase entries), by a stub's
`call [esp + eax*4]` (the two stack tables) or by a stub's tail `jmp` through
`.data` (the three `.data` tables). None reads a stack argument, so the
prototype is the caller's `call` of a void function either way.

What each does is written above it in `battle_phases.cpp` and in its
`evidence` field. The short version:

- **The frame** (`Battle_Frame`). Runs the phase dispatch unless the round
  flags `0x904AA8` have bit 1, or window 0 is in use or `0x904AE9` bit 1 is
  set while the phase is not 5; then fifteen per-frame calls in a fixed
  order and a tail jump to `Field_DrawFrame`. The object screen pass is
  `Field_ObjectsScreen` while `0x904AAA` is set, `Sprite_UpdateObjectScreens`
  otherwise.
- **Phase 0, the start.** `BattleStart_Dispatch` picks `Battle_Init` or
  `BattleIntro_Dispatch` by the step `0x904AA1`; the intro dispatch picks
  one of four steps by the sub-step `0x904AA2`.
  - `Battle_Init`: counts the actors in, resets about thirty battle
    globals (the turn counter `0x904B90` to 1), snapshots each member's stat
    block `+0xA0..+0xBF` to `+0xC0`, sets or clears the low-HP bit (`+0x90`
    bit 13 when HP `+0x98` is below max `+0xA0` / 4), zeroes the command
    fields, runs `Formation_ApplyStatMods` and copies the block back; the
    CLUT set-up; the battle's music (DAT `0x904EFC + 0x15D`) unless
    `0x904AAA`; the event hook with 6 while `0x904AAA`. The PSX decomp's
    terms are the same, in the same order.
  - The intro: the windows (the name window `0x14`, one per member at
    `0xD + i` placed by the member's screen position and two offset
    tables), the initiative message (`0x904AE4` 1 or 2: system messages
    `0x16` / `0x17`), the status window; a wait for window 1; the command
    cross's growth bytes (zero when the initiative byte is 2 or more); the
    two sub-windows once the banner queue is empty and the files are in,
    and the phase on.
- **Phase 1, the command input.** `BattleInput_Dispatch` jumps through
  `Battle_InputSteps` by the step.
  - `Battle_RoundStart`: the commands cleared and the entry order built;
    in auto battle (round flags bit 4) the commands are filled
    (`0x446720`) and the phase goes straight to 2.
  - `BattleInput_NextMember`: the next member in the entry order `0x904AB6`
    who can act (`+0x90` bit 5 clear, `+0x134 & 0x14001` clear) becomes the
    menu actor `0x939EC4` with its command record `+0x124` at `0x939FA0`;
    with none left, phase 2.
  - `BattleMenu_CommandSelect`: the command cross. Cancel goes back one
    member (an item command gives its item back); confirm takes the
    command the cross points at; Select held opens the step after
    (`Battle_InputSteps[3]`); a held direction points the cross at command
    1..6 (`Battle_CommandPadMasks`), and pressing the same direction twice
    within 8 frames takes it. Commands 2 and 3 are refused (sound `0x107`)
    while the actor's `+0x134` has bit 1.
  - `BattleMenu_ConfirmDispatch`: the pulse stepped, then a jump through
    `Battle_MenuSteps` by the chosen command.
  - `Cmd_ConfirmDefend`: command 2 with the actor as its own target, the
    record's flag bit 1, the next member.
- **Phase 2, the commit.** `BattleCommit_Dispatch` jumps through
  `Battle_CommitSteps`: `Battle_CommitRound` (the turn order, items of item
  commands given back unless consumed, round flags bit 3, the enemies'
  choices, DAT file `0xD1` unless auto), `BattleCommit_QueueMessages` (one
  queued enemy message a frame into `Text_Records[0]`), and
  `BattleCommit_WaitLoad` (the files, then unless auto a 14-frame wait
  counted down from `0x904B70`, then phase 3).

**Stack tables and bad indices** (known-defects.md D59). As in `battle_flow`, the two stack-built
tables' indices are unchecked in the original, and an index past them calls
through its own stack; ours aborts (rule 4). The three `.data` dispatches
are *not* bounded in ours either: they read the entry at the index, as the
original does. Past each table lie more tables and code pointers
(`Battle_MenuSteps[8]` is `Battle_CommitSteps[0]`, `Battle_CommitSteps[3]`
is phase 3's first entry), so a bound would be a behaviour change on an
index the original survives. No index past a table was seen or is reachable
from the code read here.

## 2. The tables named

In `symbols.toml` as `[[data]]` entries:

| Name | Address | Type | Count | Dispatched by |
|---|---|---|--:|---|
| `Battle_InputSteps` | `0x64AE28` | `unsigned long` | 5 | `BattleInput_Dispatch` by `0x904AA1` |
| `Battle_CommandPadMasks` | `0x64AE3C` | `unsigned short` | 6 | read by `BattleMenu_CommandSelect` |
| `Battle_MenuSteps` | `0x64AE54` | `unsigned long` | 8 | `BattleMenu_ConfirmDispatch` by `0x904AA2` |
| `Battle_CommitSteps` | `0x64AE74` | `unsigned long` | 3 | `BattleCommit_Dispatch` by `0x904AA1` |

- `Battle_InputSteps`: `0x42E9A0`, `0x42E9E0`, `0x42EAD0`, `0x42ED90`,
  `0x42EED0`.
- `Battle_CommandPadMasks`: `0x1000` up, `0x4000` down, `0x8000` left,
  `0x2000` right, `0x0004` L1, `0x0008` R1 - command k + 1 for word k.
- `Battle_MenuSteps`: `0x447110`, `0x447430`, `0x448180`, `0x447FD0`,
  `0x42EEF0`, `0x42EF50`, `0x44A000`, `0x44FF00`.
- `Battle_CommitSteps`: `0x42F080`, `0x42F130`, `0x42F1D0`.

The table `0x64AE48` (three entries: `0x42EDA0`, `0x42EE00`, `0x42EEA0`,
dispatched by `0x42ED90` on `0x904AA2`) sits between them. Its dispatcher is
no group's, so it is not named here.

## 3. Left original, and what was learned about other addresses

**Nothing of the group was left original.** These addresses inside the
group's range are no group's (the combat route does not reach them) and are
called or dispatched to raw:

- `0x42ED90` (`Battle_InputSteps[3]`, reached after Select is held) and its
  three steps `0x42EDA0`, `0x42EE00`, `0x42EEA0`: a cross-growth animation
  and the draws it makes.
- `0x42EF50` (`Battle_MenuSteps[5]`): the twin of the PSX
  `Cmd_AutoBattle_Begin` `0x801D2598` by the sibling's notes. It sets window
  4, gives back pending item commands through `0x446D90`, puts the living
  members in state 2, sets round flags bit 4 and loads DAT `0xD1`.
- `0x446720`: called by `Battle_RoundStart` in auto battle. It is the PSX
  `AutoBattle_FillCommands` `0x801DD264` by the sibling's
  `Battle_RoundStart` note. It walks the entry order from `0x904AC3` (while
  below 3 and not `0xFF`) and gives each member's command record command 1
  (`+0x125`) and target 3 (`+0x124`), with `0x904AAE` the member.
- `0x446D90`: puts an item back into the inventory (`(slot, item) -> al`;
  PSX `0x801DDE44`). `battle_setup_callees.h` already calls it as
  `kReturnItem`.
- `0x44A000` and `0x44FF00` (`Battle_MenuSteps[6]`, `[7]`): each is a
  one-line stub, `jmp [0x64E4FC / 0x64ECCC + byte 0x904AA3 * 4]`.

**Other groups' addresses.** `0x447110`, `0x447430` (CH) and `0x448180`,
`0x447FD0` (CI) are `Battle_MenuSteps[0..3]`. They are entered by
`BattleMenu_ConfirmDispatch`'s tail jump, so their return goes to
`Battle_PhaseDispatch`, and ours now calls them. That is the same as long
as they read no stack argument. The PSX decomp gives them none. CB's
`0x42F220` is phase 3's entry; it calls through `0x64AE80`, the table right
after `Battle_CommitSteps`.

**Corrections for other documents** (said, not acted on):

- **`0x42E2F0` is not the battle's frame.** `battle_flow.md` calls
  `0x42E2F0` "the battle's frame" and has it call `Battle_PhaseDispatch`
  (and `symbols.toml`'s evidence for `BattleParty_RunStates` and others
  repeats it). `0x42E2F0` ends at `0x42E361` (`ret`, then nop padding). It
  calls `0x5917A0` with window 0's bytes `+8` / `+0xD` and copies
  `0x903EFE..0x903F03` to `0x675EB8..`. The frame is `0x42E370`, which
  `pe_funcs.py` folded into it.
- **`entries_logic.txt`.** `0042E2F0 104` should be `0042E2F0 72`. The
  battle_flow doc's `0042E400 15C9` -> `62` still stands. The eighteen lines
  of section 6 are appended at the end of the file.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_phases`
(`src/game/battle_phases_fuzz.cpp`). It makes 18 byte-copies:

- Every call and tail jump out (70 sites) is re-aimed at a recorder, using
  `CloneCall` with `expected`. Three entries are themselves a `call` and are
  named at offset 0.
- The two stack tables' immediates are checked and re-aimed in the copies.
- The three `.data` tables' entries are checked against the disassembly and
  swapped for recorders; the event hook `0x904B6C` points at one. All are
  put back afterwards.

**Each round** starts from random bytes over 8.6 KB:

- ObjTrio; windows 0..15, and window `0xFF`'s first bytes;
- the 16-word copy's source and target;
- the pad words, the confirm and cancel buttons;
- the battle's globals `0x904A00..0x904D00` and `0x904EFC`;
- `Sprite_Current`;
- `0x939EC0..0x93A3C0` (the menu pointers and the message records);
- `0x93B8E0..0x93C2B0` (the enemy objects and `0x93C2A0..A2`);
- the two offset tables at `0x64DF70` (constant; put back).

The pointers and indices are then put back inside what they index: the
menu pointers at members, the entry order at members, the menu index 0..3,
the message records' enemies 0..7. Each branch's boundaries are seeded:

- the frame's three holds and phase 5;
- every step and sub-step of every table;
- actor counts to 15;
- HP at max / 4 - 1, 0, + 1;
- the boss `0x25` and other event values;
- the initiative 0..3 and `0xFF`;
- window 1 and 2 bytes at 1;
- auto on and off;
- the entry order with `0xFF`s, and the three "cannot act" bits;
- for the command cross, one of: cancel, confirm, Select, a direction
  (held, pressed, a second tap within the timer), nothing. Also the
  greying bit, item commands with and without bit 14, and random extra pad
  bits.
- messages busy / none;
- the wait at 0, 1, 2, `0x8000`, `0xFFFF`.

**The recorders are not quiet.** Besides a general disturber, which moves
one of 22 watched bytes three calls in four, each recorder moves (three
calls in four) a byte its caller reads again after the call:

- `0x904AAA` and the auto bit (after the frame's calls, the round start's,
  the commit's and the load);
- the confirm index (after the pulse);
- the member bits (after `Battle_Init`'s set-up calls);
- a byte of each stat block and its snapshot (after the formation mods);
- the actor count (after `Battle_ActorIsOut`);
- the member's `+0x89`, `+8`, x and y and the facing (after that member's
  `Window_Alloc`);
- the initiative (after the intro's calls);
- the message count (after the push);
- the menu index, the auto bit and the wait (after `File_LoadDone`);
- the step (after the name, the copies, the hook);
- after a sound: the menu index, the step, the command, the tap bytes,
  window 2's x and y, the pad words, the greying bit;
- the menu actor (after an item is given back);
- the command, window 1's x and y, the step (after the select path's
  draws).

`Window_Alloc`'s recorder answers `0xFF` one call in five, with random
upper bytes. `File_LoadDone`'s answers 0 a third of the time and `0x100`
now and then.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    battle_phases self-test: 18000 rounds over 18 functions (1000 each), 65409 calls to the stand-ins, 0 MISMATCHES; the party records, the windows, the battle's globals, the menu pointers and messages, the enemy objects, the tables and the stand-ins' log compared
    battle_phases coverage: start steps 2 of 2, intro steps 4 of 4, input steps 5 of 5, menu steps 8 of 8, commit steps 3 of 3; frame dispatched 507 / held 493; init: actors tested 13626, clut stp 4104, event hook 717, music 961, low HP set 1023 / cleared 1458; windows allocated 3275 (the first 0xFF 219), messages 921; sub-windows 1000 / 247 / 247; auto filled 492; member found 242, none left 414; sounds 0x101 239, 0x104 290, 0x106 216, 0x107 24; items returned 242; tap chose 66, greyed 39, started 139; command cleared 86; crosses 154; enemy messages 460; wait counted 566, advanced 434

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1042 ours` and no mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`ca_agent/controls.py` in the session
scratchpad) planted each one alone: replace, build, self-test, revert. The
table gives the rounds that refused each one, of 1,000.

| | Planted | Refused in |
|---|---|--:|
| F1 | Frame: the round flags bit 2, not bit 1 | 356 |
| F2 | Frame: window 0 holds in phase 5 too | 174 |
| F3 | Frame: 0x904AE9 bit 0 | 92 |
| F4 | Frame: the object screens inverted | 1000 |
| F5 | Frame: enemies before the party | 1000 |
| F6 | Frame: no Field_DrawFrame | 1000 |
| F7 | Frame: 0x904AAA read before the calls | 491 |
| S1 | StartDispatch: the other entry | 1000 |
| S2 | IntroDispatch: the next entry | 1000 |
| I1 | Init: the out ones counted | 365 |
| I2 | Init: the actor count read once | 347 |
| I3 | Init: the turn counter 0 | 1000 |
| I4 | Init: the cross growth 0 | 1000 |
| I5 | Init: 0x904B89 kept | 997 |
| I6 | Init: the task slots cleared before the encounter kind | 1000 |
| I7 | Init: low HP at a quarter too | 432 |
| I8 | Init: the clear takes bit 0 too | 439 |
| I9 | Init: +0x12D kept | 902 |
| I10 | Init: 28 bytes copied back | 149 |
| I11 | Init: copied back for every member | 869 |
| I12 | Init: 15 words to 0x811560 | 1000 |
| I13 | Init: CLUT row 0x1C | 1000 |
| I14 | Init: the boss 0x24 | 136 |
| I15 | Init: actors to 9 | 1000 |
| I16 | Init: the music + 0x15C | 465 |
| I17 | Init: the hook with 5 | 717 |
| I18 | Init: the event byte read once, before the load | 182 |
| I19 | Init: the step set to 1 | 970 |
| W1 | OpenWindows: y -21 | 875 |
| W2 | OpenWindows: message 0x17 on 3 | 489 |
| W3 | OpenWindows: at (2, 0x31) | 223 |
| W4 | OpenWindows: member windows of kind 2 | 992 |
| W5 | OpenWindows: the facing without ^ 2 | 987 |
| W6 | OpenWindows: +0x89 by 3 | 990 |
| W7 | OpenWindows: y + 11 | 992 |
| W8 | OpenWindows: +0xA 0 | 948 |
| W9 | OpenWindows: the pair read before Window_Alloc | 472 |
| W10 | OpenWindows: the answer & 0xF | 219 |
| W11 | OpenWindows: status opened with 1 | 1000 |
| W12 | OpenWindows: the initiative read once | 43 |
| V1 | WaitWindow: window 2 | 480 |
| P1 | PrepareCross: at 3 or more | 238 |
| P2 | PrepareCross: growth word 0x1000 | 491 |
| P3 | PrepareCross: sub-window 1 with 1 | 1000 |
| N1 | Finish: window 2 at 2 | 200 |
| N2 | Finish: a pending banner ignored | 420 |
| N3 | Finish: File_LoadDone tested as a byte | 36 |
| N4 | Finish: sub-window 2 always with 2 | 96 |
| N5 | Finish: the step kept | 244 |
| N6 | Finish: the initiative not re-read after the calls | 77 |
| D1 | InputDispatch: the next entry | 1000 |
| K1 | ConfirmDispatch: the index read before the pulse | 663 |
| K2 | ConfirmDispatch: no pulse | 1000 |
| K3 | ConfirmDispatch: one entry on | 1000 |
| X1 | CommitDispatch: the next entry | 1000 |
| R1 | RoundStart: the flags read before the calls | 475 |
| R2 | RoundStart: auto on bit 3 | 483 |
| R3 | RoundStart: phase 3 | 492 |
| R4 | RoundStart: the menu index kept | 742 |
| R5 | RoundStart: the copies spawned in auto too | 492 |
| M1 | NextMember: Input_Held kept | 147 |
| M2 | NextMember: +0x90 bit 4 | 162 |
| M3 | NextMember: mask 0x14000 | 55 |
| M4 | NextMember: 0xFF not skipped | 162 |
| M5 | NextMember: 0x904AAE not stored | 449 |
| M6 | NextMember: the record + 0x120 | 232 |
| M7 | NextMember: window 4 +3 = 2 | 242 |
| M8 | NextMember: the index stored only at the end | 337 |
| M9 | NextMember: the wait 13 | 410 |
| M10 | NextMember: File_LoadDone ignored | 348 |
| M11 | NextMember: the step not moved | 242 |
| C1 | CommandSelect: the tap timer not counted down | 486 |
| C2 | CommandSelect: cancel sound 0x105 | 216 |
| C3 | CommandSelect: cancel keeps the step | 215 |
| C4 | CommandSelect: cancel clears +0x134 bit 3 | 158 |
| C5 | CommandSelect: cancel +1 = 1 | 216 |
| C6 | CommandSelect: cancel returns whatever bit 14 | 70 |
| C7 | CommandSelect: cancel, the actor not re-read after the return | 42 |
| C8 | CommandSelect: cancel keeps +0x125 | 216 |
| C9 | CommandSelect: cancel, the index and step read before the sound | 35 |
| C10 | CommandSelect: confirm greys 1 and 2 | 25 |
| C11 | CommandSelect: greyed by bit 0 | 35 |
| C12 | CommandSelect: confirm steps on one | 136 |
| C13 | CommandSelect: confirm, the command read before the sound | 13 |
| C14 | CommandSelect: confirm sound 0x107 for the greyed ones 0x108 | 24 |
| C15 | CommandSelect: select on bit 9 | 154 |
| C16 | CommandSelect: select, the cross at window 1 | 154 |
| C17 | CommandSelect: select, x and y swapped | 154 |
| C18 | CommandSelect: select, the label read before the cross | 34 |
| C19 | CommandSelect: select, window 3 not set | 150 |
| C20 | CommandSelect: select, the window read before the sound | 10 |
| C21 | CommandSelect: directions 0xF000 | 75 |
| C22 | CommandSelect: the command kept when none held | 86 |
| C23 | CommandSelect: directions upward | 168 |
| C24 | CommandSelect: five directions | 56 |
| C25 | CommandSelect: direction sound 0x102 | 230 |
| C26 | CommandSelect: a second press whatever the timer | 46 |
| C27 | CommandSelect: the tap greys 2 and 3 | 18 |
| C28 | CommandSelect: a tap steps on one | 75 |
| C29 | CommandSelect: pressed not re-read | 4 |
| C30 | CommandSelect: held not re-read | 8 |
| C31 | CommandSelect: the tap timer 7 | 131 |
| C32 | CommandSelect: the tap command k | 131 |
| C33 | CommandSelect: the command only on a press | 161 |
| C34 | CommandSelect: the tap bytes read before the sound | 15 |
| C35 | CommandSelect: the tap chooses, the tap bytes kept | 75 |
| C36 | CommandSelect: cancel before the tap timer counts | 371 |
| C37 | CommandSelect: the cancel test with the confirm word | 172 |
| E1 | Defend: command 1 | 1000 |
| E2 | Defend: the target from +4 | 995 |
| E3 | Defend: +0xC bit 0 | 735 |
| E4 | Defend: step 0 | 1000 |
| E5 | Defend: 0x904AA3 kept | 993 |
| E6 | Defend: the menu index kept | 1000 |
| E7 | Defend: the actor +1 not set | 997 |
| O1 | CommitRound: window 4 in auto too | 492 |
| O2 | CommitRound: 0x93B8E0 kept | 997 |
| O3 | CommitRound: two members | 1000 |
| O4 | CommitRound: the out ones too | 521 |
| O5 | CommitRound: any item word | 295 |
| O6 | CommitRound: bit 14 kept | 140 |
| O7 | CommitRound: flags | 4 | 742 |
| O8 | CommitRound: auto read before the enemies choose | 378 |
| O9 | CommitRound: file 0xD0 | 496 |
| O10 | CommitRound: the windows before the turn order | 354 |
| O11 | CommitRound: the slot +0x12F | 164 |
| Q1 | QueueMessages: busy ignored | 317 |
| Q2 | QueueMessages: none left, the step kept | 223 |
| Q3 | QueueMessages: 16 bytes cleared | 460 |
| Q4 | QueueMessages: records by n - 1 | 460 |
| Q5 | QueueMessages: the name from +0x84 | 460 |
| Q6 | QueueMessages: 8 bytes of the name | 460 |
| Q7 | QueueMessages: the message a byte | 459 |
| Q8 | QueueMessages: at (1, 1) | 460 |
| Q9 | QueueMessages: the count not re-read | 167 |
| L1 | WaitLoad: File_LoadDone ignored | 349 |
| L2 | WaitLoad: the wait in auto too | 283 |
| L3 | WaitLoad: pre-decrement | 122 |
| L4 | WaitLoad: the wait not zeroed | 356 |
| L5 | WaitLoad: the word not stored when the wait ends | **not refused; no input could tell** |

**L5** changes nothing. When the wait is 0 the original stores 0xFFFF and then 0 over it with no call between, so storing nothing first is the same. It is not counted.

**Four controls were blind in the first run** and are refused now: I10 (now 149), C7 (now 42), C20 (now 10), C34 (now 15). Each became visible once the recorder it depended on moved the byte its caller reads back (section 4); the thin ones rose the same way (W12 1 -> 43, I18 9 -> 182, K1 8 -> 663, R1 23 -> 475, O8 7 -> 378).

**The thinnest now:** C29 (4), C30 (8), C20 (10), C13 (13), C34 (15) and C27 (18) - the command cross's re-reads after a sound and the second-tap greying, each needing a direction pressed on a round whose sound recorder moves the one byte concerned.

**What the fuzz cannot see:**

- anything the callees really do, and what the handlers behind the tables
  do;
- the order of stores with no call between them (`Battle_Init`'s thirty
  resets, the window bytes);
- the argument bytes the original leaves as garbage:
  - `Battle_Init` pushes the actor as a byte in a dword whose upper bytes
    are the caller's `ecx`;
  - the cross and status draws get words with whatever the registers held
    above them;
  - `0x446D90` gets a slot byte and an item word the same way.

  The callees read only the byte or word (`Battle_ActorIsOut` masks,
  `0x446D90` masks), and the recorders record only those.
- aliasing through out-of-range indices:
  - a negative menu index would read the entry order from below
    `0x904AB6`, after the stores the cancel path makes. Ours keeps that
    order, but no round reaches it.
  - an entry of `0xFF` on the cancel path would index member 255.
- `Window_Alloc` answers 16..254: the stand-in answers 0..15 or `0xFF`.
- the stack tables past their end (ours aborts there).

## 5. Latent in the original

Numbered in [`known-defects.md`](known-defects.md) (2026-09-25): D60 and
D59.

- **`BattleIntro_OpenWindows` does not test `Window_Alloc`'s `0xFF`**
  (known-defects.md D60). When
  window `0x14` is already taken at a battle's start, the original writes
  +2, +3, x and y into "window 255": `0x80553C + 2..+7`, which is inside
  the script message pool `MessagePools` (`0x803580..0x807580`). What lies
  there, and whether window `0x14` can be taken at that moment, was not
  read. Kept, as the original has it; the fuzz seeds it (the first answer
  `0xFF` in about 210 rounds) and control W10 depends on it.
- Neither stack table is bounded (section 1); ours aborts (known-defects.md D59).

## 6. For `analysis/calltrace/entries_logic.txt`

Appended 2026-09-25 (the file is not committed):

```
0042E370 84
0042E470 22
0042E4A0 285
0042E730 32
0042E770 145
0042E8C0 10
0042E8D0 5F
0042E930 5E
0042E990 E
0042E9A0 33
0042E9E0 E9
0042EAD0 2BB
0042EED0 13
0042EEF0 53
0042F070 E
0042F080 B0
0042F130 92
0042F1D0 44
```

The existing line `0042E2F0 104` overlaps `0042E370`; section 3.

## 7. Open

- What the eight menu steps (the commands) and `0x42ED90`'s three do.
- What `0x904AE4` means. The name "initiative" is a guess: 1 and 2 show
  messages `0x16` and `0x17` at the start, and 2 or more skips the cross's
  growth.
- What queues the enemy messages `0x939FBC` that
  `BattleCommit_QueueMessages` shows, and what `0x939F60` is.
- Window `0xFF` (section 5).
