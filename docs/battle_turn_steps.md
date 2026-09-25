# The per-turn steps: the round's end and the battle's end

**Status:** IN PROGRESS (2026-09-25) - twenty functions ours
(`src/game/battle_turn_steps.cpp`, shadow name `battle_turn_steps`), each
read whole and fuzzed headless against a copy of Capcom's with every call
re-aimed at a recorder: 30,000 rounds, 0 mismatches; 109 negative controls,
all 109 refused. The combat route's live check is owed (section 7).

Group CC of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). The battle engine is
the PSX's `BATTLE.EMI` compiled into the exe; the RAM layout is the one
[`battle_setup.md`](battle_setup.md) and [`battle_flow.md`](battle_flow.md)
read against the sibling's `docs/BATTLE_RAM.md`. Every function is a
faithful replacement, so no `DIVERGENCE.md` entry is owed. Two latent
defects of Capcom's are described in section 4, kept as the original has
them, for the coordinator to number.

## 1. What they are

`Battle_PhaseDispatch` (`0x42E400`, ours, battle_flow) calls entry
`0x904AA0 & 0xFF` of a six-entry table it builds on its stack. Phases 4 and
5 are this group's: `0x4302B0` and `0x4311E0`, each a 14-byte **dispatch
stub** `xor eax, eax; mov al, [state]; jmp [table + eax * 4]` over a table in
`.data`. A stub is entered by `Battle_PhaseDispatch`'s `call`, so its target
returns straight to `Battle_PhaseDispatch`: the prototype is the caller's -
no argument, and no caller reads what the step leaves in eax. Ours are the
same jump (`[[clang::musttail]]`, as `TitleFlow_Step` in
[`save-menu.md`](save-menu.md)), reading the same table from `.data`, the
index unchecked as the original's.

The state bytes: `0x904AA1` the phase's step, `0x904AA2` the step's
sub-step, `0x904AA3` / `0x904AA4` the result's step and page, `0x904AA5` a
member cursor.

**Phase 4, the round's end** (`BattleRoundEnd_Steps` `0x64AF2C`, by
`0x904AA1`):

| Step | Function | Entry | Bytes | Does |
|---|---|---|--:|---|
| (stub) | `BattleRoundEnd_Step` | `0x4302B0` | 0xE | phase 4 through `BattleRoundEnd_Steps` |
| 0 | `BattleRoundEnd_FasterStep` | `0x4302C0` | 0xE | stub through `BattleRoundEnd_FasterSteps` `0x64AF38` by `0x904AA2` |
| 0.0 | `BattleRoundEnd_CheckFaster` | `0x4302D0` | 0xF4 (with its 4-entry jump table at `0x4303B4`) | the extra round, or the round's end |
| 0.1 | `BattleRoundEnd_FasterNext` | `0x430500` | 0x7 | `inc byte ptr [0x904AA2]` |
| 0.2, 2 | `BattleRoundEnd_NextRound` | `0x431090` | 0x147 | the battle over, or the next round |
| 1 | `BattleRoundEnd_StatusChain` | `0x430510` | 0x124 (with its 14-entry jump table at `0x4305FC`) | the nine BattleSteps |

- **`BattleRoundEnd_CheckFaster`** (PSX `0x801D4B08`, read in the sibling's
  `generated/overlays_static_0002.c`: the same tests and loop bounds). Unless
  an extra round is running (`0x904B7A`) or the timer `0x904B8E` is,
  `Battle_MarkFasterSide`'s al picks the extra round `0x904B7A` through a
  four-entry jump table: al 1 gives 1, 2 gives 2, 3 gives 3; with `0x904AE4`
  at 2, 3 gives 2 and 1 gives none. An extra round steps `0x904AA2` on
  (`BattleRoundEnd_FasterNext`, then `BattleRoundEnd_NextRound`). None, or
  al above 3, ends the round: flag 0x8000 cleared from members 0..1 and
  enemies 0..6 (section 4), the round counted (`0x904B90`), `0x904B7A` = 0,
  `Battle_TickCounters`, and `0x904AA1` on. `Battle_MarkFasterSide`
  ([`battle_setup.md`](battle_setup.md) section 4) answers 0 or 1 only, so
  al 2 and 3 are dead code in this build; ours keeps them.
- **`BattleRoundEnd_StatusChain`** is the chain driver `battle_setup.md`
  section 2 calls "`0x430510` (not ours)" (PSX `0x801D4FF4` by its place).
  By `0x904AA2` (0..13; above, nothing): entering a step increments
  `0x904AA2` in memory and calls the next BattleStep; a step answering al 1
  ends the frame there, otherwise the next falls through. After
  `Expire4000`, `Status80`, `HpDrift` and `ApUpkeep` the next state
  (1, 8, 10, 12) waits for the pending word `0x904B82` to be 0, then clears
  round flag 4 (`0x904AA8`) and steps on; after the other five, the next
  state is the next step's, so a message from `Restore800` or a wake is
  followed at once by the next step. State 13 zeroes `0x904AA2` and steps
  `0x904AA1` on.
- **`BattleRoundEnd_NextRound`** (PSX `0x801D653C`, pairs_propagated, not
  read). Nothing while any of the eight dwords `0x803540..0x80355C` is set,
  or, in an event battle (`0x904AAA`), while the hook `0x904B6C` called with
  2 answers al 0xFF. With `0x904AE8` set the battle is over: round flag 0x10
  cleared, byte +0x24 of eleven of the twelve message-window records from
  `0x803160` zeroed (all but the third), phase 5 from step 0. Otherwise:
  unless round flag 0x10 is set, the command bytes (`0x904AB4`, `0x904ABC`
  = 8, the dword `0x904ABD`, the word `0x904AC1`) reset and DAT file 0x131
  loaded; `Battle_ClearCommands`; each member `Battle_ActorIsOut` does not
  rule out to state 2 (+1); round flag 8 cleared, `0x904AE4` zeroed, phase 1
  from step 0.

**Phase 5, the battle's end** (`BattleEnd_Steps` `0x64AF44`, by `0x904AA1`):

| Step | Function | Entry | Bytes | Does |
|---|---|---|--:|---|
| (stub) | `BattleEnd_Step` | `0x4311E0` | 0xE | phase 5 through `BattleEnd_Steps` |
| 0 | `BattleEnd_TaskStep` | `0x4311F0` | 0xE | stub through `BattleEnd_TaskSteps` `0x64AF58` by `0x904AA2` |
| 0.0 | `BattleEnd_TasksBegin` | `0x431200` | 0x13 | `0x802D20` and the cursor zeroed |
| 0.1 | `BattleEnd_StartMemberTask` | `0x431220` | 0xF6 | a task for the next member with flag 1 or 2 |
| 0.2 | `BattleEnd_AwaitMemberTasks` | `0x431320` | 0x18C | wait, restore, pick the way out |
| 1 | `BattleEnd_WinStep` | `0x4314B0` | 0xE | stub through `BattleEnd_WinSteps` `0x64AF64` by `0x904AA2` |
| 1.0 | `BattleEnd_WinBegin` | `0x4314C0` | 0x60 | music, the window, the lines |
| 1.1 | `BattleEnd_WinAwaitInput` | `0x431520` | 0x1A | wait for a press |
| 1.2 | `BattleEnd_ResultStep` | `0x431910` | 0xE | stub through `BattleEnd_ResultSteps` `0x64AFA0` by `0x904AA3` |
| 1.2.0 | `BattleEnd_ResultPage` | `0x431920` | 0x11 | stub through `BattleEnd_ResultPages` `0x64AFAC` by `0x904AA4` (group CD's pages) |
| 2, 3 | `0x431540`, `0x4315B0` | | | the other way out: **left Capcom's** (section 6) |
| 4 | `BattleEnd_ExitStep` | `0x431760` | 0xE | stub through `BattleEnd_ExitSteps` `0x64AF90` by `0x904AA2` |
| 4.0 | `Battle_WriteBackParty` | `0x431770` | 0x31 | the members back into their records |
| 4.1 | `BattleEnd_ExitHook` | `0x4317B0` | 0x39 | the exit hook, the music out |
| 4.2 | `BattleEnd_Finish` | `0x4317F0` | 0xF9 | the fallen at 1 HP, the battle state zeroed |

- **`BattleEnd_StartMemberTask`**: from the cursor `0x904AA5` on (to the
  count `0x904AB0`), the first member `Battle_ActorIsOut` does not rule out
  that has flag 1 or 2 (+0x134) gets flag 0x2000 (+0x130) and a kind-0
  battle task, parameter 0xB for character 4 (+0x89) and 0xC for the others
  - who first clear bit 0 of +0x134 in the three records at `0x939AE0` -
  owned by the member (the slot's +0x80). The cursor is re-read after each
  call, the count at each test. What flags 1 and 2 and the task mean was not
  read; flag 2 is the one `BattleStep_ApUpkeep` charges AP for.
- **`BattleEnd_AwaitMemberTasks`** (PSX `0x801D69E4`, pairs_propagated, not
  read): waits while a member has flag 0x2000 (the task clears it,
  presumably - not read), goes back a step while the cursor is short of the
  count. Then, unless `0x904AE8` bit 0: each member, `Sprite_Current` set to
  it, with status 0x800 gets HP and AP to their maxima, +0x9C from +0xAE and
  `Battle_ClearStatus(status & 0xBFFF)`; then every member
  `Battle_ClearStatus(status & 0xBF5F)` (the status re-read),
  `Sprite_ReleaseTint`, `Battle_ReturnQueuedItem`; the timer `0x904B8E`
  zeroed. Outside an event battle `0x904AE8` picks the way out: bit 0 step 2,
  bit 1 step 1 (bit 1 wins; `Battle_EnemyDefeated` sets bit 1 when the last
  enemy falls, so step 1 is the win). The hook `0x904B64`,
  `Port_DroppedCall(0)`, DAT 0xCD for step 1 and 0xCE for step 2.
- **`BattleEnd_WinBegin`**: the members present to state 2; unless
  `0x904AE5` bit 0x40, `Music_FadeOutStop(10)` and `Music_Play(0xA5, 10)`;
  `Battle_OpenMsgWindow`, `Battle_OpeningMessage` (section 5).
  **`BattleEnd_WinAwaitInput`** steps on once `File_LoadDone` answers (it
  always does) and `Input_Pressed` is not 0.
- **`Battle_WriteBackParty`** keeps the sibling's name for its PSX twin
  `0x801D71B0` (`names/functions.toml`: `Battle_WriteBackMember(0..2)`,
  `0x8015990C()`, `0x801629CC(0x256)`, the state on) - term for term
  `0x446A80(0..2)`, `Window_ResetAll`, `LoadDatFile(2)`.
- **`BattleEnd_ExitHook`**: once loaded, the hook `0x904B68`; unless
  `0x904AE5` bit 0x40, `Music_FadeOutStop(10)`, and `0x904AE8 |= 8` when
  `Music_Track` is then 0xFF.
- **`BattleEnd_Finish`** (PSX `0x801D7320`, pairs_propagated, not read):
  `0x494E70` (bytes +0..+4 of the eight enemy objects zeroed),
  `BattleTask_ClearAll`; each member with status 0x4000 has its
  CharacterRecord (+0x148 the index) set to HP 1, +0x1C = 1, +0x1E up by one
  below 5, status 0x4000 and +0xB bit 2 cleared, and `Char_RecalcStats`.
  Then `0x446600` (each member's +0x80 re-copied from its record),
  `Game_Step` up by one, `0x904AE9` and the five state bytes
  `0x904AA0..0x904AA4` zeroed.

## 2. The tables named

Eight `[[data]]` entries in `symbols.toml`, `unsigned long` each, the counts
from where the next table starts (every stub's own table ends where another
stub's begins):

| Table | At | Entries | By | Stub |
|---|---|---|---|---|
| `BattleRoundEnd_Steps` | `0x64AF2C` | 3: `0x4302C0` `0x430510` `0x431090` | `0x904AA1` | `0x4302B0` |
| `BattleRoundEnd_FasterSteps` | `0x64AF38` | 3: `0x4302D0` `0x430500` `0x431090` | `0x904AA2` | `0x4302C0` |
| `BattleEnd_Steps` | `0x64AF44` | 5: `0x4311F0` `0x4314B0` `0x431540` `0x4315B0` `0x431760` | `0x904AA1` | `0x4311E0` |
| `BattleEnd_TaskSteps` | `0x64AF58` | 3: `0x431200` `0x431220` `0x431320` | `0x904AA2` | `0x4311F0` |
| `BattleEnd_WinSteps` | `0x64AF64` | 3: `0x4314C0` `0x431520` `0x431910` | `0x904AA2` | `0x4314B0` |
| `BattleEnd_ExitSteps` | `0x64AF90` | 4: `0x431770` `0x4317B0` `0x4317F0` `0x4318F0` | `0x904AA2` | `0x431760` |
| `BattleEnd_ResultSteps` | `0x64AFA0` | 3: `0x431920` `0x431B60` `0x431D50` | `0x904AA3` | `0x431910` |
| `BattleEnd_ResultPages` | `0x64AFAC` | 5: `0x431940` `0x431A20` `0x431A90` `0x431AB0` `0x431B30` | `0x904AA4` | `0x431920` |

Two tables between them are not named: `0x64AF70` (`0x431550` `0x4315A0`
`0x432430`, by `0x904AA2`, stub `0x431540`) and `0x64AF7C` (`0x431200`
`0x431220` `0x4315C0` `0x431710` `0x431740`, stub `0x4315B0`). Their stubs
are the other way out, reached by no route and in no group.

`BattleEnd_ResultPages` is named here with its stub; its five entries are
group CD's functions. If CD names it too, the merge has a duplicate
`[[data]]` at `0x64AFAC` (`gen_symbols.py` refuses it): keep one.

## 3. Latent, as the original has them

- The stubs do not check their state byte. One past a table reads the next
  table's first entry.
- `BattleEnd_StartMemberTask` does not test `BattleTask_Create`'s 0xFF (no
  slot free); the owner then goes to `0x9423FC` (section 4).
- The member index passed to `Battle_ActorIsOut`, `Battle_ClearStatus` and
  `Battle_ReturnQueuedItem` from `BattleRoundEnd_NextRound`,
  `BattleEnd_StartMemberTask` and `BattleEnd_AwaitMemberTasks` is a stack
  slot whose low byte is the index and whose upper bytes are the caller's
  `ecx` at entry. All three callees (ours) mask the byte, so ours passes the
  index alone.

## 4. Defects, for `known-defects.md` (the coordinator numbers them)

**The round's end leaves the last member's and the last enemy's flag
0x8000.** `BattleRoundEnd_CheckFaster` clears flag 0x8000 (+0x134 / +0x114,
the mark `Battle_MarkFasterSide` sets on the side that outpaces) with
`ecx = 2` over the members from `0x802E74` and `ecx = 7` over the enemies
from `0x93BA74`: members 0 and 1, enemies 0..6. The actor loops read beside
it (`Battle_TickCounters`, `Battle_MarkFasterSide`, the one that sets the
mark) run 0..2 and 3..10. The PSX twin `0x801D4B08` has the same bounds
(`sltiu 2` from 0, `sltiu 0xA` from 3), so the PC inherited it. Effect: a
third member or eighth enemy once marked keeps the mark into every later
round. What reads flag 0x8000 was not read, so whether that gives an extra
action is open. **Kept**; controls F7 and F8 plant the full loops and are
refused (584, 589 rounds).

**A full task table at the battle's end writes past the image.**
`BattleEnd_StartMemberTask` stores the member at `0x93A080 + 0x84 n` with
`n = BattleTask_Create`'s al unchecked; with all 48 slots taken that is
`0x9423FC`. The image ends at `0x93F000`; at start-up (the self-test's
`VirtualQuery`, 2026-09-25) `0x940000..` is reserved but not committed
(state `0x2000`), so the store would fault unless something commits that
range first. `battle_flow.md` section 3 records the same unchecked 0xFF in
the magic starters. Latent: 48 tasks alive at a battle's end was not seen.
**Kept**.

## 5. Found about other groups' addresses (not acted on)

- **`Battle_OpeningMessage` `0x44AA00`** (group BA's, round 7): its only call
  site `0x431514` is in `BattleEnd_WinBegin`, after the win's music and
  window. Its lines (by `0x904B90`, the round count) are the win's, not the
  encounter's opening; the name's hypothesis looks wrong. Not renamed.
- **Group CD's `0x431B60` and `0x431D50`** are dispatch stubs of the same
  kind, `mov eax, [0x904AA4]; and eax, 0xFF; jmp [eax * 4 + 0x64AFC0]` and
  `... 0x64AFC8`; `0x432430` (no group's) dispatches by `0x904AA3` through
  `0x64AFD8`.
- **Callees in no group**, called by raw address from
  `battle_turn_steps_callees.h`: `0x446A80` (the member's HP, AP and status
  into its CharacterRecord; the sibling's `Battle_WriteBackMember`),
  `0x494E70` (bytes +0..+4 of the eight enemy objects zeroed), `0x446600`
  (each member's +0x80 re-copied from its CharacterRecord, `0x29` dwords).
- **`entries_logic.txt`** has three over-long lines of group BA's that
  cover this group: `004301B0 214` (its body is 0xF6), `004303D0 269` (0x123)
  and `00431030 97E` (0x5F). This group's twenty lines are added after them
  (section 6).

## 6. The fuzz and its negative controls

`src/game/battle_turn_steps_fuzz.cpp`, under `BOF3X_SHADOW=battle_turn_steps`:

- twenty byte-copies, each call out re-aimed at a recording stand-in with
  `CloneCall` and `expected` (the two with a call at offset 0,
  `BattleEnd_WinAwaitInput` and `BattleEnd_ExitHook`, named there);
- the two inline jump tables (`0x4303B4`, `0x4305FC`) relocated into their
  copies;
- the eight tables' 29 entries (checked first against the reading above)
  and the three hooks `0x904B64` / `0x904B68` / `0x904B6C` swapped for
  recorders, and put back;
- a page committed at `0x942000` for the run, so that task slot 0xFF's
  owner store can be answered; decommitted after.

**Each round** starts from random bytes over 16 KB: the party's records
`0x802D20..0x803560` (with the window bytes and the busy dwords), the
CharacterRecords and the battle globals `0x903A70..0x904D00`, the second
party array at `0x939AE0`, the task slots and the enemies, `Sprite_Current`,
`0x9423FC`, `Game_Step`, `Input_Pressed`. The member count is kept to 0..4
and each member's record index to 0..9; a stub's state byte to its table.
Seeded per function: the extra round and the timer at 0; `0x904AE4` at 2;
chain states 0..14 and 0xFF; the pending and input words 0, one high-byte
bit, or random; busy dwords all 0 or one set; the event byte, `0x904AE8` and
round flag 0x10 both ways; the cursor at, below and above the count; flags
1 / 2, character 4; flag 0x2000 clear; statuses 0x800 / 0x4000 both ways;
`0x904AE8` 0..3 and with the other bits set; the record counts 3..6.

**The stand-ins are loud.** Each logs its arguments and a hash of the
watched state (the state bytes, the counts, `0x904AE4`..`0x904AE9`,
`0x904B7A`, the pending word, the timer, the round count, the members'
state, HP, AP, +0x9C and flag bytes, the second array's bytes, the enemies'
flag bytes, the window bytes, `Game_Step`, `Sprite_Current`) at the call,
so a store moved across a call shows. Three calls in four move one of the
cells a caller reads again; besides, the steps move `0x904AA2`, the file
loads `0x904AA1`, `Battle_ActorIsOut` and `BattleTask_Create` the cursor
and the count, the item return `0x904AE8` and the count, `Char_RecalcStats`
the count, `Battle_ClearStatus` the status word, `Music_FadeOutStop`
`Music_Track`. `File_LoadDone` answers 0, al 0 over a non-zero eax, or
non-zero; the answers in al carry random upper bits.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=battle_turn_steps`,
exit 0:

    battle_turn_steps: 0x9423FC (task slot 0xFF's owner) was state 0x2000, is a page committed for the run, base 00940000
    battle_turn_steps self-test: 30000 rounds over 20 functions (1500 each), 44736 calls to the stand-ins, 0 MISMATCHES; ... task slot 0xFF answered
    battle_turn_steps coverage: table slots 29 of 29; extra round 0/1/2/3 1165 74 189 72, faster checks 672, ticks 1202;
      chain steps 9 of 9 (2059 calls), waits passed 261, chains done 340; next round: waited 584, battle over 469, next
      round 447, event hooks 566, files 1950, out tests 2342; tasks started 384; await: back 642, done 434 (win 101,
      other 88), status clears 503, tints 334, items 334, end hooks 431; music 1298 / 744, windows 1500, messages 1500,
      load waits 3000; write-backs 4500, exit hooks 1120; finish: records raised 1418, clears 1500, reloads 1500

With `BOF3X_SHADOW='*'`: exit 0, `inject: 1044 ours`, no mismatch or Fatal
anywhere in the log.

**Negative controls.** `build/controls.py` (not committed) planted each one
alone in `battle_turn_steps.cpp`: replace, build, self-test, restore. **109
planted, 109 refused.** The first run, with quieter recorders, left two
blind (A15 and I3, 0 rounds) and seven thin (C4 1, I2 3, C7 4, A11 8, T9 7,
T5 12, A19 25); each rose once the recorder its caller reads back was given
that side effect, or the boundary was seeded (the table below is the second
run). The thinnest now: A15 (11), C7 (18), A11 (37), C1 (42), N1 (44), A12
(45), X3 (48).

| | Planted | Refused in (of 1,500) |
|---|---|--:|
| S1 | `BattleRoundEnd_Step`: by 0x904AA2, not 0x904AA1 | 994 |
| S2 | `BattleRoundEnd_FasterStep`: by 0x904AA1, not 0x904AA2 | 987 |
| S3 | `BattleEnd_Step`: by 0x904AA2 | 1204 |
| S4 | `BattleEnd_TaskStep`: one entry on | 1500 |
| S5 | `BattleEnd_WinStep`: through BattleEnd_TaskSteps | 1500 |
| S6 | `BattleEnd_ResultStep`: by 0x904AA4 | 1000 |
| S7 | `BattleEnd_ResultPage`: by 0x904AA3 | 1200 |
| S8 | `BattleEnd_ExitStep`: entries 0 and 1 only | 758 |
| Q1 | `BattleRoundEnd_FasterNext`: 0x904AA1 stepped | 1500 |
| F1 | `BattleRoundEnd_CheckFaster`: a running extra round not tested | 348 |
| F2 | `BattleRoundEnd_CheckFaster`: the timer not tested | 331 |
| F3 | `BattleRoundEnd_CheckFaster`: al 1: the 0x904AE4 test inverted | 127 |
| F4 | `BattleRoundEnd_CheckFaster`: al 1 with 0x904AE4 2: the dead step of 0x904AA2 left out | 65 |
| F5 | `BattleRoundEnd_CheckFaster`: al 3 always 3 | 65 |
| F6 | `BattleRoundEnd_CheckFaster`: al 2 turned to 1 by 0x904AE4 | 51 |
| F7 | `BattleRoundEnd_CheckFaster`: flag 0x8000 cleared from all three members | 584 |
| F8 | `BattleRoundEnd_CheckFaster`: flag 0x8000 cleared from all eight enemies | 589 |
| F9 | `BattleRoundEnd_CheckFaster`: the round counted by 2 | 1202 |
| F10 | `BattleRoundEnd_CheckFaster`: the extra round cleared after Battle_TickCounters | 539 |
| F11 | `BattleRoundEnd_CheckFaster`: 0x904AA1 set to 1, not stepped | 1176 |
| F12 | `BattleRoundEnd_CheckFaster`: al masked to 2 bits | 61 |
| C1 | `BattleRoundEnd_StatusChain`: state 1 not a wait | 42 |
| C2 | `BattleRoundEnd_StatusChain`: a wait after Restore800 | 93 |
| C3 | `BattleRoundEnd_StatusChain`: round flag 4 kept | 133 |
| C4 | `BattleRoundEnd_StatusChain`: the pending word by its low byte | 84 |
| C5 | `BattleRoundEnd_StatusChain`: state 13 leaves 0x904AA2 at 14 | 284 |
| C6 | `BattleRoundEnd_StatusChain`: state 4 runs the fifth step | 231 |
| C7 | `BattleRoundEnd_StatusChain`: 0x904AA2 stored, not stepped in memory | 18 |
| C8 | `BattleRoundEnd_StatusChain`: state 14 as 13 | 95 |
| C9 | `BattleRoundEnd_StatusChain`: ApUpkeep's whole eax tested | 195 |
| N1 | `BattleRoundEnd_NextRound`: seven busy dwords | 44 |
| N2 | `BattleRoundEnd_NextRound`: the hook called with 3 | 566 |
| N3 | `BattleRoundEnd_NextRound`: any non-zero al holds the round | 354 |
| N4 | `BattleRoundEnd_NextRound`: the hook outside an event battle too | 563 |
| N5 | `BattleRoundEnd_NextRound`: the third window zeroed too | 471 |
| N6 | `BattleRoundEnd_NextRound`: the battle over to phase 4 | 471 |
| N7 | `BattleRoundEnd_NextRound`: round flag 0x10 kept at the end | 252 |
| N8 | `BattleRoundEnd_NextRound`: file 0x131 under flag 0x10, not without | 448 |
| N9 | `BattleRoundEnd_NextRound`: file 0x132 | 210 |
| N10 | `BattleRoundEnd_NextRound`: 0x904ABC = 9 | 210 |
| N11 | `BattleRoundEnd_NextRound`: the members out get state 2 | 448 |
| N12 | `BattleRoundEnd_NextRound`: 0x904AE4 kept | 447 |
| N13 | `BattleRoundEnd_NextRound`: round flag 8 kept | 226 |
| N14 | `BattleRoundEnd_NextRound`: the command bytes reset after the file | 210 |
| N15 | `BattleRoundEnd_NextRound`: members 0..1 only | 448 |
| B1 | `BattleEnd_TasksBegin`: the cursor kept | 1493 |
| B2 | `BattleEnd_TasksBegin`: 0x802D20 = 1 | 1500 |
| T1 | `BattleEnd_StartMemberTask`: flag 1 only | 128 |
| T2 | `BattleEnd_StartMemberTask`: the cursor not re-read after Battle_ActorIsOut | 210 |
| T3 | `BattleEnd_StartMemberTask`: parameter 0xB for all | 203 |
| T4 | `BattleEnd_StartMemberTask`: two of the three records | 88 |
| T5 | `BattleEnd_StartMemberTask`: the owner by the cursor of before the call | 102 |
| T6 | `BattleEnd_StartMemberTask`: a kind-1 task | 384 |
| T7 | `BattleEnd_StartMemberTask`: flag 0x1000 for 0x2000 | 331 |
| T8 | `BattleEnd_StartMemberTask`: 0x904AA2 kept when none is found | 315 |
| T9 | `BattleEnd_StartMemberTask`: the count read once | 58 |
| T10 | `BattleEnd_StartMemberTask`: the slot index as 5 bits | 142 |
| T11 | `BattleEnd_StartMemberTask`: the character test on +0x88 | 181 |
| A1 | `BattleEnd_AwaitMemberTasks`: waits on flag 0x1000 | 600 |
| A2 | `BattleEnd_AwaitMemberTasks`: no step back | 642 |
| A3 | `BattleEnd_AwaitMemberTasks`: the members by bit 1 | 231 |
| A4 | `BattleEnd_AwaitMemberTasks`: status 0x400 restores | 117 |
| A5 | `BattleEnd_AwaitMemberTasks`: AP from the maximum HP | 116 |
| A6 | `BattleEnd_AwaitMemberTasks`: first mask 0xFFFF | 75 |
| A7 | `BattleEnd_AwaitMemberTasks`: second mask 0xBF7F | 117 |
| A8 | `BattleEnd_AwaitMemberTasks`: the status not re-read after the first clear | 66 |
| A9 | `BattleEnd_AwaitMemberTasks`: Sprite_Current not set | 160 |
| A10 | `BattleEnd_AwaitMemberTasks`: the timer set to 1 | 232 |
| A11 | `BattleEnd_AwaitMemberTasks`: 0x904AE8 not re-read after the members | 37 |
| A12 | `BattleEnd_AwaitMemberTasks`: bit 0 wins over bit 1 | 45 |
| A13 | `BattleEnd_AwaitMemberTasks`: the way out picked in an event battle too | 148 |
| A14 | `BattleEnd_AwaitMemberTasks`: file 0xCE for step 1 | 124 |
| A15 | `BattleEnd_AwaitMemberTasks`: 0x904AA1 read once for the files | 11 |
| A16 | `BattleEnd_AwaitMemberTasks`: Port_DroppedCall before the hook | 431 |
| A17 | `BattleEnd_AwaitMemberTasks`: 0x904AA2 kept at the end | 423 |
| A18 | `BattleEnd_AwaitMemberTasks`: back only while the cursor is below the count | 542 |
| A19 | `BattleEnd_AwaitMemberTasks`: the members to the entry count | 59 |
| A20 | `BattleEnd_AwaitMemberTasks`: the item back for the next member | 160 |
| A21 | `BattleEnd_AwaitMemberTasks`: +0x9C from +0xAD | 115 |
| W1 | `BattleEnd_WinBegin`: two members | 741 |
| W2 | `BattleEnd_WinBegin`: the music flag inverted | 1500 |
| W3 | `BattleEnd_WinBegin`: track 0xA6 | 744 |
| W4 | `BattleEnd_WinBegin`: the message before the window | 1500 |
| W5 | `BattleEnd_WinBegin`: state 1 | 1322 |
| I1 | `BattleEnd_WinAwaitInput`: no input needed | 572 |
| I2 | `BattleEnd_WinAwaitInput`: the input by its low byte | 293 |
| I3 | `BattleEnd_WinAwaitInput`: File_LoadDone by its low byte | 208 |
| I4 | `BattleEnd_WinAwaitInput`: File_LoadDone not asked | 1500 |
| R1 | `Battle_WriteBackParty`: member 2 not written back | 1500 |
| R2 | `Battle_WriteBackParty`: file 3 | 1500 |
| R3 | `Battle_WriteBackParty`: the file before Window_ResetAll | 1500 |
| R4 | `Battle_WriteBackParty`: 0x904AA2 kept | 1500 |
| X1 | `BattleEnd_ExitHook`: the hook before the load is done | 380 |
| X2 | `BattleEnd_ExitHook`: the music flag inverted | 1120 |
| X3 | `BattleEnd_ExitHook`: Music_Track read before the fade | 48 |
| X4 | `BattleEnd_ExitHook`: 0x904AE8 |= 4 | 199 |
| X5 | `BattleEnd_ExitHook`: 0x904AA2 on before the load is done | 380 |
| E1 | `BattleEnd_Finish`: status 0x2000 | 869 |
| E2 | `BattleEnd_Finish`: HP 0 | 921 |
| E3 | `BattleEnd_Finish`: the count capped at 6 | 265 |
| E4 | `BattleEnd_Finish`: the record status kept | 576 |
| E5 | `BattleEnd_Finish`: +0xB bit 3 | 748 |
| E6 | `BattleEnd_Finish`: records 0xA0 apart | 869 |
| E7 | `BattleEnd_Finish`: the count read once | 179 |
| E8 | `BattleEnd_Finish`: Game_Step kept | 1500 |
| E9 | `BattleEnd_Finish`: 0x904AA4 kept | 1495 |
| E10 | `BattleEnd_Finish`: the task slots cleared before the enemies | 1500 |
| E11 | `BattleEnd_Finish`: +0x1C = 2 | 921 |
| E12 | `BattleEnd_Finish`: 0x904AE9 kept | 1492 |

**What the fuzz cannot see:**

- anything the callees and the table entries really do (phase 4's and 5's
  steps are each checked alone, their chain only on the combat route);
- the order of stores with no call between them;
- the eax a step leaves (no caller reads it);
- a stub's state byte past its table: in range only, since one past reaches
  the next table and, in the unnamed ones, code that is not stood in;
- the upper bytes of the member index (section 3).

## 7. Left original, the extents, the live check

- **Left Capcom's:** nothing of the group. Outside it and not reached by the
  combat route: the other way out `0x431540` / `0x4315B0` and their steps
  (`0x431550`, `0x4315A0`, `0x432430`, `0x4315C0`, `0x431710`, `0x431740`),
  and `0x4318F0`, `BattleEnd_ExitSteps`' fourth entry.
- **`analysis/calltrace/entries_logic.txt`**: the twenty lines are added
  (2026-09-25, after a comment line naming the group), sizes as read, the two
  jump tables inside theirs: `004302B0 E`, `004302C0 E`, `004302D0 F4`,
  `00430500 7`, `00430510 124`, `00431090 147`, `004311E0 E`, `004311F0 E`,
  `00431200 13`, `00431220 F6`, `00431320 18C`, `004314B0 E`, `004314C0 60`,
  `00431520 1A`, `00431760 E`, `00431770 31`, `004317B0 39`, `004317F0 F9`,
  `00431910 E`, `00431920 11`. BA's three over-long lines (section 5) are
  left for the merger.
- **The live check** (the coordinator's, after the merge): the combat A/B
  (`analysis/validate_combat.sh`) reaches all twenty - phases 4 and 5 every
  round and at the win. `BOF3X_ORIGINAL` with all twenty names restores
  Capcom's.

## 8. Open

- What flag 0x8000 does once marked, and so the effect of the first defect.
- What flags 1 and 2 (+0x134) are, and what the kind-0 task with parameter
  0xB / 0xC does before it clears flag 0x2000.
- The other way out (step 2, `0x904AE8` bit 0), and what sets that bit.
- What `0x803540..0x80355C` are (the next round waits on them).
