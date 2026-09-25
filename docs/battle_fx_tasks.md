# The battle effect tasks

**Status:** IN PROGRESS (2026-09-25). Nineteen functions are ours
(`src/game/battle_fx_tasks.cpp`): the eighteen of group CE in
[`takeover-queue-round8.md`](takeover-queue-round8.md) and one more, the
damage popup's state 0 (`0x432C40`), which sat inside `0x432B70`'s catalogue
extent and was never listed. Each is fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 19,000 rounds, 0
mismatches. 81 negative controls were planted, one wrong behaviour each: all 81 were
refused by a count (section 4). The `BOF3X_SHADOW='*'` self-test exited 0
with `inject: 1043 ours`.

This is group CE of the eighth parallel round. Every function is a
*faithful* replacement, so no `DIVERGENCE.md` entry is owed. No defect of the
original was found beyond the unchecked table indices every stack-table
dispatch in the battle engine has (section 3; known-defects.md D59, D64). The model is round seven's
[`battle_flow.md`](battle_flow.md), whose `BattleTask_RunAll` is the caller
of the three kinds here.

## 1. What is ours

Every extent was read to its last instruction (capstone, linear, 2026-09-25;
each ends at a `ret` or a tail `jmp` followed by padding). "Reached from" is
the immediate or pointer that names the function.

| Function | Entry | Bytes | Reached from | PSX |
|---|---|--:|---|---|
| `BattleFx_Dispatch` | `0x4352A0` | 0xAE | `BattleTask_RunAll`'s stack table, kind 0 (`.text 0x435118`) | none paired |
| `BattleMagicFx_Dispatch` | `0x435350` | 0x476 | kind 1 (`0x435120`) | none paired |
| `BattleMagicRow_Run` | `0x4378B0` | 0x1D | kind 2 (`0x435128`) | none paired |
| `BattleFx_DamagePopup` | `0x432B70` | 0xC8 (with its 4-entry jump table) | kind 0 effect 1 (`0x4352B7`) | `0x801E5BE4` (catalogue, not read) |
| `BattleFx_PopupStart` | `0x432C40` | 0x16C | popup state 0 (`0x432B7F`) | - |
| `BattleFx_PopupHold` | `0x432DB0` | 0x2B | popup state 1 (`0x432B87`) | - |
| `BattleFx_PopupRise` | `0x432DE0` | 0x64 | popup state 2 (`0x432B92`) | - |
| `BattleFx_PopupBounce` | `0x432E50` | 0x4C | popup state 3 (`0x432B9A`) | - |
| `BattleFx_PopupEnd` | `0x432EA0` | 0x6C | popup state 4 (`0x432BA2`) | - |
| `BattleFx_PoseTask` | `0x433190` | 0x3A | kind 0 effect 3 (`0x4352CA`) | none paired |
| `BattleFx_PoseStart` | `0x4331D0` | 0xB7 | pose state 0 (`0x4331A9`) | - |
| `BattleFx_PosePlay` | `0x433290` | 0x13 | pose state 1 (`0x4331B4`) | - |
| `BattleFx_StepReset` | `0x4332E0` | 0x12 | `0x4332B0`'s state 0 (`0x4332BF`) and `.data 0x65C3A0` | - |
| `BattleFx_ActorWatch` | `0x433460` | 0x5B | kind 0 effect 6 (`0x4352E2`) | none paired |
| `BattleFx_ActorWatchTest` | `0x4334C0` | 0x8A | watch state 0 (`0x43346C`) | - |
| `BattleFx_Follow` | `0x4337F0` | 0x1A | kind 0 effect 7 (`0x4352EA`) | none paired |
| `BattleFx_FollowOwner` | `0x433810` | 0x15A | follow state 0 (`0x4337FD`) | `0x800A5360` (catalogue, not read) |
| `BattleHook_Area189Script` | `0x437720` | 0x2E | stored at `0x904B64` by `0x436110` (`.text 0x436127`), called from `0x431464` | none paired |
| `BattleHook_Area189Transition` | `0x437750` | 0x2B | stored at `0x904B68` by `0x436110` (`0x436131`), called from `0x4317B9` | none paired |

Every one takes no arguments and returns nothing (`void (void)`, cdecl). The
jumped-to ones are entered by `call [esp + eax*4]` of their dispatcher, which
pushes nothing, and each reads nothing above its own frame; the two hooks by
`call [0x904B64]` / `call [0x904B68]` with nothing pushed.

What each does is written above it in `battle_fx_tasks.cpp`. The short
version:

- **The three kinds.** `BattleTask_RunAll` runs a slot by its kind `+6`:
  - kind 0, `BattleFx_Dispatch`: entry `+5` of a 19-entry stack table -
    `0x437CC0` (a bare `ret`), the damage popup, `0x432F90`, the pose task,
    `0x4332B0`, `0x433380`, the actor watch, the follower, then `0x43C740`
    .. `0x452B60`;
  - kind 1, `BattleMagicFx_Dispatch`: entry `+5` of a 110-entry stack table
    (0x1B8 bytes of `mov [esp+k], imm32`), the magic effects - most of them
    group CJ's or unqueued;
  - kind 2, `BattleMagicRow_Run`: while the round flags `0x904AA8` have
    `0x800`, a tail `jmp` to the code pointer of row `Sprite_Current +5` of
    **`Magic_Rows`** (section 2). This is the task `Battle_StartItemMagic`
    and `Battle_StartAbilityMagic` start with the item's or ability's magic
    row as its parameter.
- **The damage popup** (kind 0, effect 1). Five states by `+1`: start
  (placed over its owner `+0x80`, the digit count from the value `+0x60`,
  the owner's flag byte `|= 0x80`, timer 8), hold (timer down to 0, then
  timer 4), rise (y up 4 a frame; at the timer's end a velocity of 0.5 / -6.0
  in 16.16), bounce (gravity 1.25 a frame until y is back at its start with
  x moved; timer 10), end (the flag byte cleared, the slot freed). While
  `Sprite_Current` bit 0 is set it draws by its mode `+7` (a jump table):
  mode 0 a number - the rolling digits (`BattleFx_RollingDigits`, BB's)
  during states 0..2, then `Battle_DrawNumber` of the value; modes 1..3 a
  24 x 8 label, cells 2, 1, 0. That this is the damage number is read from
  what it draws; which labels cells 0..2 hold (MISS and the like) was not
  looked at.
- **The pose task** (effect 3): with the sprite animation set pointer
  `0x9039D8` at `0x8C5D80` (restored to `0x8B3580` after, the pool
  mode_flow sets), state 0 sets the sprite's draw bytes, its CLUT row from
  `0x64B058` by the mode `+7`, a flip, and an animation from `0x64B064`, and
  queues the overlay; state 1 ticks the script until it ends, then frees
  the slot.
- **The actor watch** (effect 6): nothing in phase 5; with the same set
  pointer around it, state 0 waits until its owner's actor is not out and
  its status byte has any of `0x58`, unless the round flags have `0x400` and
  the actor is byte `0x904B34`. States 1..4 (`0x433550` ..) are not in the
  queue and stay Capcom's.
- **The follower** (effect 7): every frame the sprite copies its owner's
  (`0x93B940`) place and look - seventeen fields, byte `+0` with `0x20` -
  updates its screen position, and frees itself when the owner is gone or
  the phase is not 1 (both can happen in one frame; the slot is freed
  twice, which is harmless: `BattleTask_FreeCurrent` only zeroes bytes).
- **`BattleFx_StepReset`**: `+9 = 0`, `+1` on. Shared: state 0 of
  `0x4332B0` (kind 0 effect 4) and entry 0 of the `.data` state table
  `0x65C3A0`, which `0x4FAFF0` (group CJ's) dispatches through
  (`.text 0x4FAFFE`).
- **The round hooks**: with bit 0 of `0x904AE8` and the previous area
  `0x802290` equal to `0xBD`, the first sets `MoveScript_Var7 = 1` and the
  byte after it `0x19`, calls `ScriptFlags_Set40` and tail-jumps to
  `0x446E20` (unread); the second calls `Transition_Start(4)` and adds one
  to `0x904AA2`. The names say only what they test; what area `0xBD` is and
  what the script does there was not looked up.

**Two pointers, not one.** Every function here reads either the battle-task
slot being run (`0x93B8C4`) or `Sprite_Current` (`0x937F88`), and not the
same one throughout: the popup's states read the slot pointer, the pose
task, the watch, the follower and `BattleMagicRow_Run` read
`Sprite_Current`, and the popup reads both (its states by the slot, its draw
gate by `Sprite_Current`). `BattleTask_RunAll` sets both to the slot, so in
the game they agree; ours reads the one the original reads, and the fuzz
gives them different values half the time so a mix-up shows (controls D1,
D2, R2, P1, T3, Z2).

## 2. Tables named

| Name | Address | Entries | What |
|---|---|--:|---|
| `Magic_Rows` | `0x64C2B8` | 151 rows of 2 dwords (`count = 302`) | a u16 DAT file (read by BB's `Magic_LoadForItem` / `Magic_LoadForAbility`, which call it the magic files) and a code pointer, the kind-2 task's handler. All 151 pointers are in `.text` (137 distinct, highest `0x4FC330`); row 151 holds `0x9A1A9B1B`. The item and ability row tables name rows 0..150 only (their maxima, dumped 2026-09-25). |
| `BattleFx_PopupFaces` | `0x432C28` (`.text`) | 4 | the popup's switch on `+7`: `0x432BCB`, `0x432C04`, `0x432C08`, `0x432C0C` |

The six stack tables have no address to name; their immediates are the
arrays in `battle_fx_tasks_callees.h`, and the fuzz checks each against the
copy's bytes before it re-aims it (a transcription slip in the 110 would be
refused at start-up), and checks `kOriginals` against the same list.

`0x65C3A0` (four entries: `0x4332E0`, `0x4FB010`, `0x4FB050`, `0x4FB070`) is
a state table of `0x4FAFF0`'s and belongs with group CJ; it is not named
here.

## 3. Unchecked indices (known-defects.md D59, D64)

Every dispatch here indexes a table it does not bound, as the battle
engine's others do: the six stack tables (19, 110, 5, 2, 5, 1 entries) by a
byte of the slot or the sprite, and `Magic_Rows` by `Sprite_Current +5`. An
index past the end calls through the dispatcher's own stack or through the
data after the table. Ours aborts instead (CLAUDE.md rule 4, as
battle_flow's `BattleTask_RunAll` and `Battle_PhaseDispatch`); none is
reachable with the game's own values, so no divergence entry is owed.
Likewise, as the original has it: the enemy index `actor - 3` of the popup
and the watch is not checked, and the popup's party offset table `0x64DF70`
is indexed by the owner's `+0x2C * 4 + +8` unchecked.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_fx_tasks`
(`src/game/battle_fx_tasks_fuzz.cpp`). It makes 19 byte-copies:

- every call and tail jump out re-aimed at a recorder, using `CloneCall` with
  `expected` (22 sites);
- the six stack tables' 142 immediates checked, then re-aimed at recorders
  in the copies;
- `BattleFx_DamagePopup`'s jump table relocated into its copy;
- `Magic_Rows`' 151 code pointers swapped for 151 distinct recorders, and
  put back afterwards.

**Each round** starts from random bytes over the task slots, the slot and
owner pointers and the enemy objects (`0x93A000`, 0x22A0 bytes),
`Sprite_Current`, the battle globals `0x904AA0..0x904B40`, ObjTrio, the
animation set pointer, the area word and the two script bytes. Then every
slot's owner points at a slot or an enemy object, every object's actor `+5`
is 0..10 (both sides of the 2 / 3 split), the owners' `+0x2C` below 8,
`0x93B8C4` at a slot and `Sprite_Current` at the same slot half the time and
another the rest. Each branch's boundaries are seeded: every table index,
the popup's states 2 / 3 and modes 0..4 and beyond, values 9 / 10 / 99 /
100 / 101 and the extremes, the timers at 0 / 1 / 0xFF, the bounce's start
at y - 1 / y / y + 1 and x / x + 1, mode 6 and `+8` 0..3 for the pose, phase
5 and 1, the `0x400` and `0x800` flags, the turn gate equal to the actor, the
area `0xBD` and `0x1BD`.

**The recorders are not quiet.** Each one that a caller reads past moves
what it reads: the free moves the phase (the follower reads it after), the
actor test moves the owner pointer and its actor (the watch re-reads both),
the screen update the owner pointer and its bit 0, the transition the count
`0x904AA2`; the state recorders log the animation set pointer (stored before
the call, restored after); the script-flag recorder logs the two script
bytes. A general disturber moves one of 14 watched bytes three calls in
four.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    battle_fx_tasks self-test: 19000 rounds over 19 functions (1000 each), 14271 calls to the stand-ins, 0 MISMATCHES
    coverage: kind-0 effects 19 of 19, magic effects 110 of 110, magic rows 134 of 151; popup states 60 85 386 395 74;
      faces: digits 205, number 204, label 183, none 408; digit counts 0 480, 1 149, 2 371; bounce ended 408, moved 592;
      timers at 0 819; watch stepped 130; pose states 473 527, watch states 93 92 80 86 89, follow 1000; actor tests 869,
      frees 1881 (twice in one follow 309), screen updates 1000, animations 1000, overlays 1501, ticks 1000;
      hooks fired 325, script flags 159, transitions 166

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1043 ours` and no
mismatch or Fatal anywhere in the log.

**Negative controls.** A script (`controls.py` in the session scratchpad)
planted each one alone in `battle_fx_tasks.cpp`: replace, build, self-test,
revert. The table gives the rounds that refused each one, of 1,000. **81
planted, 81 refused, none blind.**

| | Planted | Refused in |
|---|---|---|
| D1 | Dispatch: the effect by Sprite_Current +5 | 452 |
| D2 | MagicFx_Dispatch: the effect by Sprite_Current +5 | 482 |
| D3 | MagicFx_Dispatch: the neighbouring entry | 1000 |
| R1 | MagicRow_Run: flag 0x400 | 490 |
| R2 | MagicRow_Run: the row by the slot pointer | 247 |
| R3 | MagicRow_Run: the file word taken as the code pointer | 504 |
| P1 | DamagePopup: the draw gate by the slot pointer | 199 |
| P2 | DamagePopup: digits through state 1 only | 151 |
| P3 | DamagePopup: the number 8 left | 204 |
| P4 | DamagePopup: cells 0, 1, 2 for modes 1, 2, 3 | 118 |
| P5 | DamagePopup: the value from +0x62 | 204 |
| P6 | DamagePopup: mode 4 drawn as a label | 63 |
| P7 | DamagePopup: Sprite_Current read before the state call | 17 |
| P8 | DamagePopup: the digits CLUT from +0x28 | 205 |
| S1 | PopupStart: an enemy y from its +0xF2 | 600 |
| S2 | PopupStart: the party pair by +0x2C * 2 | 235 |
| S3 | PopupStart: actor 2 an enemy | 312 |
| S4 | PopupStart: 8 higher, not 9 | 513 |
| S5 | PopupStart: three digits from 101 | 38 |
| S6 | PopupStart: the flag bit 0x40 | 736 |
| S7 | PopupStart: timer 7 | 1000 |
| S8 | PopupStart: a member flag at +0x134 | PopupStart 295, PopupEnd 66 |
| S9 | PopupStart: the +0xB test at 2 | 514 |
| S10 | PopupStart: the value unsigned | 333 |
| H1 | PopupHold: timer 5 | 264 |
| H2 | PopupHold: the timer tested after the decrement | 397 |
| I1 | PopupRise: 2 a frame | 1000 |
| I2 | PopupRise: -5.0 | 263 |
| I3 | PopupRise: the start x from +0x34 | 263 |
| I4 | PopupRise: the start y unsigned | 128 |
| I5 | PopupRise: x velocity 0x10000 | 263 |
| B1 | PopupBounce: y test >= | 143 |
| B2 | PopupBounce: ends while x is still | 621 |
| B3 | PopupBounce: gravity 1.0 | 592 |
| B4 | PopupBounce: end timer 11 | 408 |
| B5 | PopupBounce: y by the x velocity | 592 |
| E1 | PopupEnd: the flag and 0x77 | 146 |
| E2 | PopupEnd: freed whatever the timer | 708 |
| E3 | PopupEnd: the flag not cleared | 138 |
| T1 | PoseTask: the battle set not stored before the call | 1000 |
| T2 | PoseTask: the field set not put back | 1000 |
| T3 | PoseTask: the state by the slot pointer | 232 |
| A1 | PoseStart: +0x25 = 0x1C | 1000 |
| A2 | PoseStart: the CLUT less 0x4F | 940 |
| A3 | PoseStart: mode 6 by +8 & 2 | 272 |
| A4 | PoseStart: other modes flip at +8 == 1 only | 92 |
| A5 | PoseStart: the animation by +8 >> 2 | 513 |
| A6 | PoseStart: the animation by mode * 3 | 906 |
| A7 | PoseStart: the state not stepped | 949 |
| A8 | PoseStart: only the low byte of +0x2C cleared | 995 |
| A9 | PoseStart: no overlay queued | 1000 |
| A10 | PoseStart: the CLUT table by +8 | 853 |
| L1 | PosePlay: freed when the tick answers 0 | 1000 |
| Z1 | StepReset: timer 1 | 1000 |
| Z2 | StepReset: the state through the slot pointer | 476 |
| W1 | ActorWatch: phase 4 skipped | 640 |
| W2 | ActorWatch: the field set not put back | 440 |
| W3 | ActorWatchTest: flag 0x200 | 129 |
| W4 | ActorWatchTest: the gate inverted | 515 |
| W5 | ActorWatchTest: a member status mask 0x18 | 6 |
| W6 | ActorWatchTest: a member status at +0x91 | 22 |
| W7 | ActorWatchTest: an enemy status at +0x93 | 84 |
| W8 | ActorWatchTest: a member re-read from the pointer before the call | 22 |
| W9 | ActorWatchTest: an enemy re-read from the pointer before the call | 45 |
| W10 | ActorWatchTest: an enemy's answer ignored | 206 |
| W11 | ActorWatchTest: the step on Sprite_Current +2 | 123 |
| F1 | FollowOwner: byte +0 with 0x10 | 658 |
| F2 | FollowOwner: +0x58 a byte | 980 |
| F3 | FollowOwner: +0x3C not copied | 986 |
| F4 | FollowOwner: the owner tested before the screen update | 441 |
| F5 | FollowOwner: kept in phase 2 | 526 |
| F6 | FollowOwner: the phase read before the first free | 118 |
| F7 | FollowOwner: the second free skipped after the first | 309 |
| K1 | Area189Script: area 0xBC | 159 |
| K2 | Area189Script: the second byte 0x18 | 159 |
| K3 | Area189Script: the bytes stored after ScriptFlags_Set40 | 159 |
| K4 | Area189Script: 0x446E20 not reached | 159 |
| K5 | Area189Script: the area as a byte | 166 |
| K6 | Area189Transition: kind 3 | 166 |
| K7 | Area189Transition: the count stepped before the call | 88 |
| K8 | Area189Transition: bit 1 of 0x904AE8 | 164 |

**One control was blind in the first run** and is refused now: A8 (the word
`+0x2C` cleared as a byte) - the fuzz's owners had `+0x2C` below 8, so the
high byte was always 0; the pose's own sprite now gets a random `+0x2D`
(995). The watch's status controls W5..W7 went from 5 / 10 / 30 to 6 / 22 /
84 once the status bytes were seeded at each bit of `0x58` alone.

**The thinnest:** W5 (6), P7 (17), W6 (22), W8 (22), S5 (38) and W9 (45). W5
is thin because the status test is reached only past the turn gate and the
actor test (about a third of the rounds), and a mask that differs in one bit
of three shows only on the byte that has that bit alone.

**What the fuzz cannot see:**

- anything the callees really do, and what the kind-0 effects, the 110 magic
  effects and the 151 magic rows not ours do;
- the order of stores with no call between them (the follower's seventeen
  copies, the popup start's stores);
- re-reads of a pointer with no call or aliasing store between them (the
  popup start re-reads the slot and its owner between x and y; ours does the
  same, but no input could tell);
- the upper bits of the registers the popup and the watch push above a byte
  or a word (the slot's or `Sprite_Current`'s address): ours pushes the
  same, but `BattleFx_RollingDigits`, `Battle_DrawNumber`,
  `Battle_DrawLabel` and `Battle_ActorIsOut` read only the byte or the word,
  and so do the recorders;
- the abort on an out-of-range index (the original's behaviour there is a
  call through garbage; the fuzz keeps every index in range).

## 5. For the batch check

Every function is on the combat route (the catalogue's first-call trace),
so `analysis/validate_combat.sh` A/B's them; the damage popup and its five
states, the pose task, the watch and the follower draw or move sprites on
screen during a fight. The round hooks fire only after a battle in area
`0xBD` with `0x904AE8` bit 0 set - whether the combat route's fight is there
was not checked; if not, they are fuzz only. `BOF3X_ORIGINAL` with all
nineteen names adds about 480 characters.

## 6. `analysis/calltrace/entries_logic.txt`

The nineteen are appended with the sizes above (2026-09-25). Two older
lines overlap them and are not this group's to change:

- `00432A30 4DC` (the unowned host `0x432A30`, which ends at `0x432B6A`;
  its extent as listed runs over `0x432B70..0x432F0B`);
- `00432F10 E47` (BB's `BattleFx_RollingDigits`, 0x7A by BB's own reading -
  [`battle_flow.md`](battle_flow.md) section 7 asks for the cut) runs over
  `0x433190..0x433969`.

## 7. About other groups' addresses

Said here, not acted on:

- **`0x432C40`** was in no group: `pe_hidden.py` folded it into `0x432B70`'s
  576 bytes. It is the popup's state 0 and is taken here with its host; the
  catalogue's extent for `0x432B70` is 0xC8.
- **`0x4332E0`'s "first caller `0x435350`"** is the magic effects' dispatch:
  it reaches `0x4332E0` through `0x4FAFF0` (CJ's) and `0x65C3A0`, not
  through `0x4332B0` (kind 0 effect 4, unqueued). `0x4332B0`'s other states
  `0x433300` / `0x433350` and `0x432F90` / `0x433380` (effects 2 and 5) are
  unqueued and stay Capcom's.
- **`0x437CC0`** is a bare `ret`, the null entry of every battle stack
  table. [`battle_flow.md`](battle_flow.md) sections 1 and 8 say the event
  hooks are "stored by `0x437CC0`"; the hooks at `0x904B64` / `0x904B68`
  are stored by `0x436110` (group CF's enemy state, `mov [0x904B64],
  0x437720` at `0x436121`), and `0x904B6C`'s store was not traced here.
- **`0x446E20`** (tail-jumped to by `BattleHook_Area189Script`) is in no
  group of round eight and not in `symbols.toml`.
- **`0x4376F0`**, the catalogue's host for the two hooks, is a separate
  function (`0x4376F0..0x43771A`: with the enemy's `+0x90` bit 3 and an odd
  `Rand`, the round flags `|= 0x80` and `BattleTask_Create(0, 2)`); it is in
  no group.

## 8. Open

- What kind 0's effects 2, 4, 5 and 8..18 are, and the watch's states 1..4.
- Which labels the popup's cells 0..2 are.
- What area `0xBD` is, and what `0x446E20` does after the script flag.
- The PSX twins of the popup and the follower (paired by the catalogue, not
  read).
