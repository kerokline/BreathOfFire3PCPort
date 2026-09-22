# The top-level task flow: boot, the field task, the area entry, the transitions

**Status:** IN PROGRESS (2026-09-22) - forty-two functions ours
(`src/game/mode_flow.cpp`), fuzzed headless against copies of Capcom's with
every call re-aimed at a recorder; 95 negative controls, 94 refused by a count and the other one unobservable. **Not yet through the
live batch check** - that runs centrally after the merge (section 8).

Group K of the third parallel round
([`takeover-queue-round3.md`](takeover-queue-round3.md)). Everything here is a
*faithful* replacement: no `DIVERGENCE.md` entry is owed. It continues
[`mode-tasks.md`](mode-tasks.md) (the title task and `GameMode_Field`) and
[`title-states.md`](title-states.md) (the title's states, which start the
field task this file owns); the scheduler it runs on - `Task_Create`,
`Task_Sleep`, `Task_Exit`, the `0x4000`-byte stacks - is
[`attract-mode.md`](attract-mode.md) section 2 and stays Capcom's.

## 1. What is ours

Sizes are each body to its last instruction (capstone, 2026-09-22, every jump
but the calls and tail jumps listed in `mode_flow_fuzz.cpp` internal); counts
are `analysis/calltrace/hidden_b/bof3x.callcounts.tsv` (one attract cycle).
PSX twins are `analysis/pairs_propagated.json`'s unless the note says
otherwise; `callers` and `gap` tiers are a few points less sure
([`attract-remaining.md`](attract-remaining.md) section 5.1), and a
`call-disputed` pairing is not used.

| Function | Entry | Bytes | Catalogue said | PSX | hidden_b |
|---|---|--:|--:|---|--:|
| `Transition_Start` | `0x495040` | 0x26 | - | `0x8014EB68` (read) | - |
| `Transition_Task` | `0x495070` | 0xBD | 192 | `0x8014EC28` (read: the entry the PSX start creates) | 54 |
| `Transition_Kind00`..`Kind09` | `0x495130`..`0x495250`, 0x20 apart | 0x12 each | 32 | - | 28, 26, then 0 |
| `Transition_Kind10` / `Kind17` | `0x495270` / `0x495450` | 0x5C | - | - | 0 |
| `Transition_Kind11` / `Kind18` | `0x4952D0` / `0x4954B0` | 0xC9 | - | - | 0 |
| `Transition_Kind12` | `0x4953A0` | 0x22 | - | - | 0 |
| `Transition_Kind13`..`Kind16` | `0x4953D0`..`0x495430`, 0x20 apart | 0x12 each | - | - | 0 |
| `Transition_Kind19` | `0x495580` | 0x12 | - | - | 0 |
| `Transition_Kind20` | `0x4955A0` | 0x71 | - | - | 0 |
| `Transition_FadeSub` | `0x495620` | 0x7E | 126 | - | 54 |
| `Transition_FadeAdd` | `0x4956A0` | 0xA2 | - | - | 0 |
| `Transition_DrawTile` | `0x495750` | **0xAD** | 176 / **0xAFB** | - | 864 |
| `Field_Task` | `0x495800` | 0x3A | 64 | `0x80198068` (callers) | 3 |
| `GameMode_Start` | `0x495840` | 0xBB | 192 | `0x801980EC` (callers) | 3 |
| `GameMode_Enter` | `0x495900` | 0xE1 | 240 | `0x801981E8` (callers) | 12 |
| `Field_WaitTransition` | `0x4967F0` | 0x3A | 58 | (call-disputed, not used) | 18 |
| `Game_ClockTick` | `0x496870` | 0x18F | 399 | `0x80199CAC` (gap; read, the same) | 11,901 |
| `Area_Enter` | `0x594E60` | 0x2FD | 765 | `0x801A0AA8` (callers) | 12 |
| `Area_EntryWalk` | `0x595160` | 0x67 | - | `0x801A0F90` (gap) | not traced |
| `Window_ResetAll` | `0x59E330` | 0x26 | - | `0x8015990C` | 4 |
| `Boot_Task` | `0x496B60` | 0x127 | 304 / **0xBAA** (in `0x496AD0`'s) | two callers candidates, none used | 1 |
| `Title_LoadTask` | `0x496C90` | 0x2E | 48 | - | 1 |
| `File_LoadDone` | `0x454810` | 0x6 | 6 | (call-disputed, not used) | 12,224 |
| `Gfx_ClutStripRestore` | `0x4549B0` | 0x34 | 52 | `0x8014E178` (read, the same) | 13 |
| `MoveScript_TintReset` | `0x454A20` | 0x25 | 37 | `0x8014E2E8` (gap) | 13 |
| `Field_SlotRelease` | `0x454A50` | 0x21 | 33 | `0x8014E378` | 8 |
| `Field_SlotsReleaseAll` | `0x454AB0` | 0x14 | 20 | - | 1 |
| `Gfx_ClearRect` | `0x461E10` | 0x3F | 63 | `0x8014E458` | 1 |

The queue listed eighteen. **`0x495070`'s table has 21 entries, not 12**
(`0x54` bytes of stack, `mov [esp + 0..0x50], imm32`), and they are
`0x495130`..`0x4955A0` - so all 21 were taken with it, with the two fade loops
under them (`0x4956A0` was in no list). `0x495750`'s catalogue size 0xAFB ran
through `0x495800`, `0x495840`, `0x495900` and on into `GameMode_Field`; and
`0x496AD0`'s 0xBAA (its body is 0x88) swallows `Boot_Task` and
`Title_LoadTask` - `pe_funcs.py` running through pointer-reached neighbours
again (HANDOFF "Pick up here" 0000). `Transition_Start`, `0x59E330` and
`0x595160` were taken because they are part of the flow and cheap to check.

**Left Capcom's**, each behind a recording stand-in:

- The scheduler (`Task_Sleep`, `Task_Exit`, `Task_Create`,
  `Task_ClearPrivate`) - hand-written `esp` swaps (the round's boundary).
- `0x5951D0` - GameMode_Enter's call on input bit 1: 0x177 bytes that walk the
  area descriptor's entry list (+0x20, +0x24, +0x28, `Flags_Test` per entry)
  and rewrite the pending area cells before calling `Area_ClassifyPending` and
  `Area_PickMusic`. Readable and fuzzable, but twelve callers, most of them
  menu and event code this round did not read; the next group's.
- `0x531F90` - the drop-in party placement (0x227 bytes, 94 call sites);
  `field_modes.cpp` calls it as `kPlaceParty` too.
- `Effect_ClearAll` `0x5898A0` - group M's (the round's boundary), called by
  name.

## 2. The transition task

`Transition_Start(kind)` (170 call sites) refuses while
`MoveScript_WaitWordDA` is set - a transition is already running - else
stores the kind in the word `0x66C828` (PSX `0x80143C90`) and creates task 2
at `0x495070`. **Many callers push whole registers** with the kind in the low
byte (`push eax` at `0x495950`, `push ecx` at `0x495AC6`, `push ebx` at
`0x53FE8F`); the original's `movzx` reads the byte. Ours reads the byte from
its argument slot through a volatile pointer, because clang may assume an
`unsigned char` argument arrives extended, and the fuzz passes garbage above
it (control K2 is the proof). `Field_WaitTransition(run)` gets the same
treatment, although its four callers push constants.

`Transition_Task` calls entry `[0x66C828]` of a 21-entry table it builds on its
own stack; every entry ends the task through `Task_Exit`. Index 21 or more
would call its own return address; ours aborts (CLAUDE.md rule 4).

| # | Entry | Does |
|--:|---|---|
| 0 / 1 | `0x495130` / `0x495150` | `FadeSub(0x800 / -0x800, 1, 0)`: 16 frames to / from black. **The only two the attract reaches** (title, menu) |
| 2 / 3 | `0x495170` / `0x495190` | `FadeSub(+-0x2000, 1, 0)`, 4 frames |
| 4 / 5 | `0x4951B0` / `0x4951D0` | `FadeSub(+-0x400, 1, 0)`, 32 frames |
| 6 / 7 | `0x4951F0` / `0x495210` | `FadeSub(+-0x800, 1, 2)` - into OT slot 2 |
| 8 / 9 | `0x495230` / `0x495250` | `FadeAdd(+-0x800, 1, 0)`: to / from white |
| 10 / 17 | `0x495270` / `0x495450` | fade to black (step 0x400 / 0x200) while the camera swings away: each frame the tile is not done, yaw (`Camera_Angles[0]`) and the angle word (`Cond_AngleFB`'s low word) + 11, `Camera_Distance` - 50, `MapView_Redraw` 2 |
| 11 / 18 | `0x4952D0` / `0x4954B0` | the swing back (byte for byte the same body): yaw `0xFEAB`, angle `0x355`; five frames of fade-in whatever the tile says; then each frame yaw and angle - 11 and distance + 50 until it **equals** `0x5DC`, the tile drawn until done; then yaw `0xFD56`, angle `0x200` |
| 12 | `0x4953A0` | distance `0x5DC` and a redraw, then `FadeSub(-0x800, 1, 0)` |
| 13 / 14 | `0x4953D0` / `0x4953F0` | `FadeSub(+-0x200, 1, 0)`, 64 frames |
| 15 / 16 | `0x495410` / `0x495430` | `FadeAdd(+-0x200, 1, 0)` |
| 19 | `0x495580` | `FadeSub(0x100, 1, 0)`, 128 frames |
| 20 | `0x4955A0` | 16 frames to black while the angle word spins `+0x20` a frame inside `0..0xFFF` (read as a dword, stored as a word); then the hold bytes 0 |

`Transition_FadeSub(step, semi, slot)` and `Transition_FadeAdd` keep a level
in the step's own argument slot - 0 for a positive step, `0x7FFF` otherwise -
and call `Transition_DrawTile(&level, step, semi, slot, abr)` a frame at a
time until it answers non-zero; the subtractive one passes blend mode 2, the
additive one 1. After a fade to black `FadeSub` zeroes six bytes
(`0x90393D..3F`, `0x9038AD..AF` - read by nothing this round looked at);
`FadeAdd` zeroes them before a fade in and sets them `0xFF` after a fade out,
and tail-jumps to `Task_Exit`. The sign test is on the step's low 16 bits.

`Transition_DrawTile`: a draw mode under `Gpu_GetTPage(1, abr, 0x140, 0)`
(texture window 0 - a push left under the tpage call) committed to `slot`;
then a TILE at the cursor as it is after that commit: `Gpu_SetTile`, then the
grey `(s16) level >> 7` in all three colour bytes, x y 0.0, w h 320.0 240.0,
`Gpu_SetSemiTrans(semi)`, committed at `0x1C`; then `level += step` as a word;
it answers `(s16) level < 0`. A 16-frame fade is exactly 16 tiles: the level
passes `0x7FFF` after the sixteenth `0x800`.

## 3. The field task and its first two modes

`Field_Task` `0x495800` is task 0's body for the demo and for every started or
loaded game: `Game_Mode` and `Game_Step` 0, `Task_ClearPrivate`,
`Window_ResetAll`, then for ever `GameMode_Handlers[Game_Mode]` (a table in
`.data`, `0x656A44`, twelve entries, indexed unchecked), `Game_ClockTick`, a
frame's sleep.

**Mode 0, `GameMode_Start`:** `Draw_PassFlags` `0x1F`, `Draw_OtSlot` 6,
`Field_PartyLoad(0)`; then the saved position - area word `0x903A04`, flags
byte `+3`, x `+4`, z `+8` - `Field_ScriptFlags2 |= 0x4040`, eight bytes zeroed
(`0x905B82`, `0x7E0941..43`, `0x904AE5`, `0x904AAA`, `0x929EC1`, `0x9036D0`),
`Game_AreaNumber = 0xFFFF` so that the area always counts as changed,
`Field_ChangeArea(area, x, z, flags)`. With the play clock not at 0:00:00:00 -
a loaded game - the area's wanted track `0x904CD0` is set to the one playing;
the playing track byte goes `0xFF`; mode 1.

**Mode 1, `GameMode_Enter`:** bit 3 of `Field_ScriptFlags2` off, then
`Area_Enter` with `Field_ChangeArea`'s four cells. Input flag bit 3 (from the
area descriptor, set by `Area_Enter`) goes straight to mode 8. Otherwise the
pending kind `0x937F98`: `0xFF` nothing, `0xFE` only `Draw_PassFlags` 0,
anything else `Transition_Start(kind)`, `Field_WaitTransition(1)`, and with
input bit 0 the area's entry list `0x5951D0` at the leader's integer x, z.
Then the kind `GameMode_Field` will use on the next area change
(`0x904EE0`): `0xA` with input bit 0, `0x14` in area `0xBD`, else 0; the zone
counter rolled (`Field_ZoneCounterRoll(1)`) except in area `0xBD`;
`Field_ScriptFlags2` bit 6 off; `Field_Request` 0; mode 2.

The flags reach `Area_Enter` as a byte in `al` above which the original's
`eax` holds whatever it held on entry - `Field_Task`'s mode index, 1, so the
upper bytes are zero in game. Ours passes the byte zero-extended and the
stand-in records only the byte; a direct caller of `0x495900` with other
upper bytes is not possible (it is reached only through the table).

**`Field_WaitTransition(run)`:** loop { sleep; if the wait word is 0 and `run`
is 0: sleep once more and return; the field frame; if the wait word is 0,
return }. The symbols entry used to say "on the first pass when run is
non-zero" - `run` is tested every pass, so with `run` set the call always
runs at least one field frame and returns only after one.

**`Game_ClockTick`** (PSX `0x80199CAC`, the same code): unless
`Field_Request` is 9, the play clock `0x9040C8..CB` - hours, minutes,
seconds, frames at 30 - ticks, and stops at exactly 99:59:59 (the test is
hours `0x63`, minutes `0x3B`, seconds equal to minutes; hours above 99 would
keep counting). Then two countdowns of the same shape, `0x90465C` and
`0x904660`, each byte decremented and tested as s8. **A countdown never
reaches all-zero on its own**: when the minutes run out at hour 0 the minutes
and seconds are zeroed but the frames byte was already reloaded with 29, so it
circles 0:00:00 every 30 frames. The PSX `0x80199D9C` is the same, so this is
Capcom's design, not the port's; whatever reads the countdowns presumably
tests the minutes and seconds (not read).

## 4. `Area_Enter` `0x594E60`

Once per area change, from `GameMode_Enter`, with the pending area, x, z and
flags:

1. Flag `0x80` - "dropped in" - sets bit 15 of `Field_ScriptFlags2`, its
   absence clears it; the flag is kept for step 12 (the original writes it
   over its own argument slot).
2. `Effect_ClearAll` (group M's), `MoveScript_TintReset`.
3. The previous area number to `0x802290`, `Cond_ByteFD` to `0x905E68`,
   `Field_Kind2Z` / `X` to `0x7E0920` / `0x7E091C`.
4. The area's music: `Music_Play(0x904CD0, 8)` unless it is `0xFF`, in which
   case the playing track byte is set `0xFF`.
5. Only when the area number differs from the argument **and**
   `Field_Request` is 5: the area number stored, `LoadDatFile(area + 3)` and
   its wait, `Field_InputFlags` from descriptor `+0x32`,
   `Gfx_ClutStripRestore`, `Gfx_ClutStripDirty = 1`.
6. `Field_PartySetUp(x, z, flags)` - the flags whole.
7. `Cond_ByteFD = Area_ZoneIdAt(Kind2 x, Kind2 z)` (the integer words; the
   lookup keeps their low bytes), `Field_ViewReset`.
8. The colour matrix: the descriptor's `+0x38` if not null - then
   `Light_Angles` from its `+0x20` / `+0x24`, **re-reading the descriptor
   after the call** - else `0x6BE090` and the light pair at `0x663B20`.
9. With `Field_StatusBits` bit 7: the scenario chapter `Cond_ByteFA` + 1,
   `0x10` skipped to `0x11`, and `Scenario_Start(chapter)`.
10. `Area_RunPlacement(descriptor +0)`; **`call` descriptor `+0x40`** when not
    null (the round's indirect call - the fuzz points `Area_Descriptors` 0..3
    at descriptors of its own whose `+0x40` is a recorder or null);
    `Flags_Clear(0x904030, 0x1C)`; `0x8034E2 = 1`; `Field_ModeDispatch`;
    `Field_PartyFirstFrame`.
11. The flags word as it is now: with bit 15 (dropped in) go to 12. Else
    unless `Field_ScriptFlags` bit 11: bit 7, 8 or 9 of `Field_ScriptFlags2`
    walks the party in from direction 1, 2 or 3 (`Area_EntryWalk`); with none
    of them and more than one member, direction 0 and then - unless bit 11 or
    bit 15 is now set - `MoveCmd_TestFB(Kind2 x, z)`; then, with more than
    one member, `Field_ScriptFlags2 = (flags | 0x12 | (4 if three members))
    & 0xEE7F` and return.
12. Dropped in (bit 15 and flag `0x80`): the walk off (`0x904EF0` 0),
    `0x531F90(flags & ~0x80)`, bit 4 off, `Field_MembersFrame`.
13. Every path but the walk's return: `Field_ScriptFlags2 &= 0xEE7F`.

`Area_EntryWalk(direction, x, z)`: the walk on at `0x904EF0`, the direction,
a zero, x and z at `0x904EF4` / `F8`; bit 3 of `Field_ScriptFlags2` for a
non-zero direction; each member's two walk bytes (`0x802E77` + `0x14C` i):
the direction's pose from `0x66ADBC` and bit 1. The name is by its caller and
flag; what the pose bytes select is not read (`hypothesis`).

## 5. Boot and the small resets

`Boot_Task` `0x496B60` is task 0's first body (WinMain creates it):
`Gfx_ClearRect(0, 0, 0x400, 0x200)`, `Port_DroppedCall(0)`,
`LoadDatFile(0x225)` and its wait; **`0x10B0` bytes from `0x9039E0` zeroed**
(Field_ActorStates, Cond_Flags, the clock, the party byte, the track, the
countdowns - the whole game state); the 18-byte button map `0x903580` from
its defaults `0x656AEC`; the party combination `0x90412C` `0xFF`; both script
flag words 0; `0x903A58` 1 and the next six bytes 0 (`0x903A5A` is the window
colour); `Window_ResetAll`, `MoveScript_TintReset`,
`Field_SlotsReleaseAll`; `Draw_PassFlags` `0x1F`, `Draw_OtSlot` 6, two pool
pointers (`0x9039D8 = 0x8B3580`, `0x7E0880 = 0x8C3584`);
`Gfx_ClutStripRestore` and `Gfx_ClutStripDirty` + 1; task 1 at `0x496C90`;
`Task_Exit`. `Title_LoadTask` `0x496C90` loads DAT `0x226`, waits, and
tail-jumps into `Title_Task`.

- `File_LoadDone` `0x454810`: `return 1` - the PC's loads are synchronous.
- `Gfx_ClutStripRestore` `0x4549B0`: zeroes the source's last `0x800` bytes -
  its rows 28..31 - and then copies all 32 rows to `Gfx_ClutStrip`, so the
  strip's rows 28..31 come out zero too. The PSX `0x8014E178` zeroes the
  same four rows first: Capcom's, deliberate or not.
- `MoveScript_TintReset` `0x454A20`: byte 0 of the 32 tint records, and 16
  dwords from `0x7E06A0` to -1.
- `Field_SlotRelease(slot)` `0x454A50` frees `Field_Slots` record
  `slot & 0xFF` if its bit 0 is set (unchecked index);
  `Field_SlotsReleaseAll` `0x454AB0` does slots 0..7.
- `Gfx_ClearRect(x, y, w, h)` `0x461E10`: a RECT of the four low words and
  `Gfx_ClearImage(&rect, 0, 0)` (a third 0 is pushed that nothing reads).
- `Window_ResetAll` `0x59E330`: bytes `+0..+3` and `+0xF` of all 22 window
  records, and the pass byte `0x802D20`. (window-task.md's "zeroes slot 0's
  four bytes" was a first reading - it is every record.)

## 6. The fuzz and its negative controls

`BOF3X_SHADOW=mode_flow`, at start-up (`src/game/mode_flow_fuzz.cpp`):
**forty-two byte-copies**, every call and tail jump out re-aimed at a
recording stand-in (`bof3::CloneCall` with `expected`), `Transition_Task`'s 21
stack-built immediates re-aimed inside its copy (checked against the 21
addresses first), `GameMode_Handlers`' twelve entries swapped for recorders
and four entries of `Area_Descriptors` pointed at descriptors of our own;
all put back afterwards.

Each round runs one function from random bytes over 23 regions (8.6 KB: the
task words, the whole `0x9039E0` game-state block, the field, camera, flag,
window, tint and slot bytes, the three constant tables the functions read),
a random primitive pool and descriptor set, with the indices put back inside
what the tables hold (mode < 12, area 0..3, members 0..4, kind < 21) and each
branch's boundaries seeded: fade steps `+-0x800`, 0, 1, -1, `0x7FFF`,
`0x8000`, `0x10000`; levels 0, `0x7F`, `0x80`, `0x7FFF`, `0x8000`, `0x7800`;
the distance a whole number of 50s below `0x5DC` or not; clock bytes at 0,
29, 30, 58, 59, 60, `0x7F`, `0x80`, `0xFF` and hours 98, 99, 100; countdowns
zero or at hour 0; pending kinds `0xFE`, `0xFF`, 0, 1, `0xA`, `0x14`; area
`0xBD`; request 5 and 9; flag `0x80` on and off; the flags words' bits 7,
8, 9, 11 and 15; chapters `0xE..0x11`; argument dwords with garbage above a
byte or a word. Theirs runs, then ours from the same state; the regions, the
pool, the descriptors, the level word, the packet cursor, the result and the
stand-ins' log are compared.

The task bodies never return and several loops wait on a stand-in (the load
waits, the fades, the swing back, `Field_WaitTransition`): the `Task_Sleep`
recorder ends a round by a long jump after 1..8 frames, on both sides alike,
through a setjmp of our own (`mode_tasks.cpp`'s). `File_LoadDone`'s recorder
answers 0 for the round's first 0..3 questions. The stand-ins are not quiet:
`Transition_DrawTile`'s moves the level as the real one does, the commit
moves the cursor, `Gpu_SetTile`'s scribbles over the whole primitive (so a
colour stored before it shows), `Gpu_GetTPage`'s returns a full dword (so a
missing 16-bit mask shows), and every recorder may rewrite one of 22 watched
bytes between calls - the mode, the area, the request, both flag words, the
input flags, the member count, the wait word, the camera distance and angles,
the chapter, the saved position, the tracks, the pending kind, the clock.
**One constraint on that**: inside `Area_Enter` the area is only moved among
descriptors that all have a colour matrix, because the original re-reads
`+0x38` after testing it and a callee that moved the area to one without
would fault it - no real callee does.
`Gfx_ClutStripRestore`'s 32 KB is checked apart, twenty rounds.

Result (2026-09-22), `BOF3X_SELFTEST_ONLY=1`:

    shadow      mode_flow self-test: 41000 rounds over 41 functions (1000 each) and 20 of Gfx_ClutStripRestore,
                126568 calls to the stand-ins, 0 MISMATCHES
    shadow      mode_flow coverage: transition kinds dispatched 21 of 21, game modes 12 of 12, area hook 461;
                tiles 19964, sleeps 25944, rounds cut by the sleep 4690; clock frames rolled 294, minutes moved 144,
                hours moved 51, capped 76, a countdown's hours moved 33, a countdown round 0:00:00 40; area DAT
                loaded 335, own colour 832, scenario 491 (0x10 skipped 136), dropped in 393, member tail 224,
                walks 242, TestFB 5; enter mode 8 509, kind FE 107, transitions 388, area BD 119;
                swing-in finished 279; wait returned 656

("Rounds cut by the sleep" are rounds the `Task_Sleep` recorder ended, the
task bodies' every round among them; "capped" counts rounds that started at
99:59:59.)

`BOF3X_SHADOW='*'`: exit 0, `inject: 441 ours`.

**Negative controls**, planted one at a time by a script
(`controls.py` in the session scratchpad: replace, build, run
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=mode_flow`, revert), with the rounds that
refused each (of 1,000 per function; two functions where a bug is in a body
two kinds share):

| | Planted | Refused in (rounds) |
|---|---|---|
| K1 | Transition_Start: the wait word not tested | Start 213 |
| K2 | Transition_Start: the kind read as the whole argument dword | Start 782 |
| K3 | Transition_Start: the task in slot 1 | Start 787 |
| K4 | Transition_Task: each kind runs the next entry | Task 1000 |
| K5 | FadeSub: the direction from the whole step dword | FadeSub 289 |
| K6 | FadeSub: blend mode 1 | FadeSub 1000 |
| K7 | FadeSub: the hold bytes cleared after a fade in too | FadeSub 316 |
| K8 | FadeSub: the level of a fade in starts at 0x7FFE | FadeSub 419 |
| K9 | FadeAdd: the hold bytes set to 0xFE after a fade out | FadeAdd 425 |
| K10 | FadeAdd: the hold bytes cleared before a fade out too | FadeAdd 154 |
| K11 | DrawTile: the tpage abr not masked to a byte | DrawTile 507 |
| K12 | DrawTile: the tpage not masked to 16 bits for the draw mode | DrawTile 1000 |
| K13 | DrawTile: the tile at the cursor from before the first commit | DrawTile 1000 |
| K14 | DrawTile: the grey is the level >> 8 | DrawTile 549 |
| K15 | DrawTile: the colour stored before Gpu_SetTile | DrawTile 1000 |
| K16 | DrawTile: the tile 256 tall | DrawTile 1000 |
| K17 | DrawTile: done at a level of 0 or below | DrawTile 300 |
| K18 | DrawTile: the semi-transparency flag passed whole | DrawTile 495 |
| K19 | Kind 10/17: the distance shrinks by 49 | Kind10 628; Kind17 611 |
| K20 | Kind 10/17: the view not asked to redraw | Kind10 666; Kind17 650 |
| K21 | Kind 20: the angle stored as a dword, clobbering its upper word | Kind20 678 |
| K22 | Kind 11/18: four warm-up frames, not five | Kind11 473; Kind18 513 |
| K23 | Kind 11/18: the stop tested on the distance before the add | Kind11 168; Kind18 197 |
| K24 | Kind 11/18: the tile drawn after it reported done | Kind11 87; Kind18 78 |
| K25 | Kind 11/18: the final frame slept on before the stop | Kind11 131; Kind18 148 |
| K26 | Kind 12: the distance set after the fade | Kind12 531 |
| K27 | Kind 6: into slot 0 | Kind06 1000 |
| K28 | Kind 9: through black, not white | Kind09 1000 |
| K29 | Field_Task: the windows reset before the private words | Field_Task 1000 |
| K30 | Field_Task: the clock before the mode handler | Field_Task 1000 |
| K31 | Field_Task: the mode read once, before the loop | Field_Task 293 |
| K32 | GameMode_Start: the saved position read before the party load | GameMode_Start 380 |
| K33 | GameMode_Start: a loaded game told by the hours alone | GameMode_Start 415 |
| K34 | GameMode_Start: the current area not made 0xFFFF | GameMode_Start 961 |
| K35 | GameMode_Start: bit 0x4000 set, not 0x4040 | GameMode_Start 483 |
| K36 | GameMode_Enter: bit 3 cleared after Area_Enter | GameMode_Enter 474 |
| K37 | GameMode_Enter: mode 8 on input bit 0x10 | GameMode_Enter 519 |
| K38 | GameMode_Enter: kind 0xFE runs a transition too | GameMode_Enter 49 |
| K39 | GameMode_Enter: the input bit for the entry list read before the transition | GameMode_Enter 204 |
| K40 | GameMode_Enter: the zone counter rolled in area 0xBD too | GameMode_Enter 224 |
| K41 | GameMode_Enter: Field_Request not cleared | GameMode_Enter 491 |
| K42 | GameMode_Enter: Field_WaitTransition(0) | GameMode_Enter 388 |
| K43 | Field_WaitTransition: returns before the frame whenever the word is 0, run or not | Field_WaitTransition 247 |
| K44 | Field_WaitTransition: run read as the whole argument dword | Field_WaitTransition 471 |
| K45 | Field_WaitTransition: no second sleep on the early return | Field_WaitTransition 471 |
| K46 | Game_ClockTick: stopped by request 8, not 9 | Game_ClockTick 600 |
| K47 | Game_ClockTick: frames roll at 31 | Game_ClockTick 65 |
| K48 | Game_ClockTick: the stop without seconds == minutes | Game_ClockTick 85 |
| K49 | Game_ClockTick: hours counted to 100 | Game_ClockTick 25 |
| K50 | Game_ClockTick: the rollover tested signed | Game_ClockTick 87 |
| K51 | CountDown: frames reloaded with 30 | Game_ClockTick 287 |
| K52 | CountDown: bytes tested unsigned (only 0xFF is below zero) | Game_ClockTick 149 |
| K53 | CountDown: at hour 0 the frames zeroed too (the loop at 0:00:00 fixed) | Game_ClockTick 32 |
| K54 | Game_ClockTick: countdown B not ticked | Game_ClockTick 432 |
| K55 | Area_Enter: bit 15 left set when the flags have no 0x80 | Area_Enter 177 |
| K56 | Area_Enter: the DAT loaded whenever the area differs | Area_Enter 426 |
| K57 | Area_Enter: DAT area + 2 | Area_Enter 335 |
| K58 | Area_Enter: a sleep before the first load test | Area_Enter 335 |
| K59 | Area_Enter: the input flags from descriptor +0x33 | Area_Enter 184 |
| K60 | Area_Enter: the dirty flag counted, not set | Area_Enter 199 |
| K61 | Area_Enter: the light from the colour pointer read before the call | Area_Enter 318 |
| K62 | Area_Enter: chapter 0x10 skipped to 0x12 | Area_Enter 90 |
| K63 | Area_Enter: the descriptor hook not called | Area_Enter 461 |
| K64 | Area_Enter: 0x1D story flags cleared | Area_Enter 945 |
| K65 | Area_Enter: the drop-in leaves the walk on | Area_Enter 393 |
| K66 | Area_Enter: the placement gets flag 0x80 too | Area_Enter 393 |
| K67 | Area_Enter: directions 2 and 3 swapped | Area_Enter 93 |
| K68 | Area_Enter: bit 4 for a party of two as well | Area_Enter 62 |
| K69 | Area_Enter: TestFB without the second flags test | Area_Enter 10 |
| K70 | Area_Enter: Field_PartySetUp gets the flags byte | Area_Enter 945 |
| K71 | Area_Enter: the last Kind2 x and z swapped | Area_Enter 1000 |
| K72 | Area_Enter: the drop-in flag re-read from the flags word at the end | Area_Enter 90 |
| K73 | Area_Enter: the no-walk end clears 0xEF7F, not 0xEE7F | Area_Enter 359 |
| K74 | Area_Enter: the music track 0xFF test on the current track | Area_Enter 985 |
| K75 | Area_EntryWalk: bit 3 set for direction 0 too | Area_EntryWalk 134 |
| K76 | Area_EntryWalk: member records 0x140 apart | Area_EntryWalk 635 |
| K77 | Area_EntryWalk: the pose of the next direction | Area_EntryWalk 811 |
| K78 | Window_ResetAll: the pass byte +0xF kept | Window_ResetAll 1000 |
| K79 | Window_ResetAll: 21 records | Window_ResetAll 1000 |
| K80 | Boot_Task: one dword fewer cleared | Boot_Task 815 |
| K81 | Boot_Task: 16 bytes of button defaults | Boot_Task 815 |
| K82 | Boot_Task: the first config byte 0 | Boot_Task 815 |
| K83 | Boot_Task: the dirty flag set, not counted | Boot_Task 753 |
| K84 | Boot_Task: task 1 created before the CLUT restore | Boot_Task 815 |
| K86 | Title_LoadTask: DAT 0x225 | Title_LoadTask 1000 |
| K87 | File_LoadDone: returns 2 | File_LoadDone 1000 |
| K88 | Gfx_ClutStripRestore: the source rows 28..31 not zeroed first | Gfx_ClutStripRestore 1 (20 of 20 of its own rounds) |
| K89 | MoveScript_TintReset: 31 records | MoveScript_TintReset 998 |
| K90 | MoveScript_TintReset: 15 marks | MoveScript_TintReset 1000 |
| K91 | Field_SlotRelease: freed whatever its bit 0 | Field_SlotRelease 495 |
| K92 | Field_SlotsReleaseAll: seven slots | Field_SlotsReleaseAll 1000 |
| K93 | Gfx_ClearRect: w and h swapped | Gfx_ClearRect 1000 |
| K94 | Area_Enter: Cond_ByteFD from Area_ZoneIdAt at (z, x) | Area_Enter 939 |
| K95 | GameMode_Start: the music track not cleared | GameMode_Start 506 |
| K85 | Boot_Task: the button map copied before the 0x10B0-byte clear | **not refused - and no input could tell** |

K85 is a change that changes nothing: the button map `0x903580` lies below the cleared block `0x9039E0..0x904A90`, so the two orders write the same bytes. It stays in the list as the reminder, and is not counted. **One control was blind in the first run and is refused now:** K69 (`MoveCmd_TestFB` without its second test of bit 15) was not refused until the entry-walk recorder was given the side effect the caller reads back (it flips bit 15 of `Field_ScriptFlags2` or bit 11 of `Field_ScriptFlags` a third of the time each); likewise the `Area_Enter`, `Field_WaitTransition`, party-load, colour-matrix and fade recorders were given theirs, which took K32, K36, K39, K61 and K26 from 12..42 rounds to hundreds. The thinnest now: K69 (10), K88 (every one of its 20), K49 (25), K53 (32), K38 (49).

**What the fuzz cannot see:** anything the callees really do, and the
coroutine side - `Task_Sleep` coming back a frame later, `Task_Exit` never
returning, `Task_Create` starting the transition or the title. Unobservable
and without a control: the order of stores with no call between them (the
six hold bytes, the eight bytes `GameMode_Start` zeroes, the button map's
five stores), the stray pushed arguments (`Port_DroppedCall`'s under
`LoadDatFile`, `Gfx_ClearImage`'s fourth zero), and `Kind20`'s dword read of
the angle - `& 0xFFF` keeps only bits a word read has too.

## 7. Found on the way

No divergence, and one entry for [`known-defects.md`](known-defects.md)
(provisional `DK1`); the rest are Capcom's on both platforms or unreachable:

- **DK1** - the swing back (kinds 11 and 18) ends only when
  `Camera_Distance` comes to exactly `0x5DC` in steps of 50, as 16 bits. Paired
  with its swing away (kind 10 takes 50 off on each of the 31 frames its fade
  is not done, kind 17 on 63) it does; entered with any other distance it
  circles for up to 32,768 frames
  (an even difference) or for ever (an odd one), and the transition's wait
  word holds everything that waits on it. Latent; which scripts start kinds
  10, 11, 17, 18 was not measured.
- The countdowns never reach all-zero (section 3) - the PSX's too.
- `Gfx_ClutStripRestore` zeroes the source rows it then copies (section 5) -
  the PSX's too.
- `Transition_Task`'s index and `Field_Task`'s mode are unchecked; the index
  past 21 is its own return address (ours aborts), the mode past 11 reads the
  next table's pointers.
- `Area_Enter` re-reads the descriptor's colour pointer after testing it: an
  area change inside `Gte_SetColorMatrix` would fault. Nothing does that.

## 8. For the batch check

**`analysis/calltrace/entries_logic.txt`**, one per line, hex address and hex
size (the file's format). Already listed with the right size, no change:
`00454810 6`, `004549B0 34`, `00454A20 25`, `00454A50 21`, `00454AB0 14`,
`00461E10 3F`, `00495620 7E`, `004956A0 A2`, `004967F0 3A`, `00496870 18F`,
`00594E60 2FD`, `00595160 67`, `0059E330 26`. **Replace**
`00495750 AFB` with `00495750 AD`. **`00496AD0 BAA` covers `Boot_Task` and
`Title_LoadTask`**: its own body is `0x88` (to `0x496B58`); cutting it to
`88` leaves whatever else `0x496CC0..0x49767A` holds unlisted, which is for
the merger to judge. New lines:

    # group K, 2026-09-22 (docs/mode-flow.md): the transition task and its 21
    # kinds (in no list - a stack-built table), the field task's modes 0 and 1,
    # the boot task and the title's loader.
    00495040 26
    00495070 BD
    00495130 12
    00495150 12
    00495170 12
    00495190 12
    004951B0 12
    004951D0 12
    004951F0 12
    00495210 12
    00495230 12
    00495250 12
    00495270 5C
    004952D0 C9
    004953A0 22
    004953D0 12
    004953F0 12
    00495410 12
    00495430 12
    00495450 5C
    004954B0 C9
    00495580 12
    004955A0 71
    00495750 AD
    00495800 3A
    00495840 BB
    00495900 E1
    00496B60 127
    00496C90 2E

**What the live batch should watch:**

- The attract cycle reaches `Boot_Task`, `Title_LoadTask`, `Field_Task`
  (three demo starts), modes 0 and 1 (`GameMode_Start` 3, `GameMode_Enter`
  12, `Area_Enter` 12 - every area of the demo), the clock (11,901 frames),
  kinds 0 and 1 with `FadeSub` and `DrawTile` (864 tiles), and the resets.
  The oracle's per-frame area and message, the memory dump (the clock and the
  `0x9039E0` block are in its arena) and the 9-minute capture cover them; the
  fades are the capture's to judge (a fade's 16 frames, grey `level >> 7`).
- `BOF3X_ORIGINAL` on all 42 names is 42 more names (about 800 characters).
- A **loaded save** (any recipe that loads one goes through
  `GameMode_Start` with the clock not zero, so the playing track is kept) and
  the **field menu**: `GameMode_Field`'s request 1 calls
  `Transition_Start(2)`, a 4-frame fade - the first contact with a kind other
  than 0 and 1 (`tools/recipes/field_menu.txt`).

## 9. What no check reached

- Kinds 2..20 and `Transition_FadeAdd`: fuzz only. Kind 2 is the menu's
  (`GameMode_Field` request 1 calls `Transition_Start(2)`), so
  `tools/recipes/field_menu.txt` reaches it; kinds 8/9 (white) and 10/11,
  17/18 (the camera swings) are in event and battle code not measured.
- `GameMode_Enter`'s mode-8 exit, kind `0xFE`, the entry list `0x5951D0`,
  area `0xBD`: which areas do what was not measured.
- `Area_Enter`'s drop-in path (flag `0x80`), the three walk directions, the
  scenario chapter step, a descriptor with a `+0x40` hook or without a colour
  matrix: fuzz only - which of the 200 descriptors have them is a count
  `analysis/area_desc_pc.json` could answer.
- Every load wait (`File_LoadDone` is always 1), the countdowns (no reader
  found), the clock's stop at 99:59:59.
- `Gfx_ClearRect`'s seven other callers (battle and menu).

## 10. Open

- Who reads the six fade-hold bytes and the countdowns.
- What the descriptor `+0x40` hooks are, and which areas have one.
- `0x5951D0` (the entry list) and `0x531F90` (the drop-in placement) - the
  next takeover under this one.
