# The event script's leader: the steps, the states, the placements

**Status:** IN PROGRESS (2026-09-25) - twenty-seven functions ours
(`src/game/event_leader.cpp`, shadow name `event_leader`), each read whole
by recursive descent and fuzzed headless against a copy of Capcom's with
every call and tail jump re-aimed at a recorder: 27,000 rounds, 0
mismatches; 95 negative controls, 94 refused by a count and one a change that changes nothing (section 5). The live check (the shop, world-map and combat
routes) is the coordinator's, after the merge.

Group DC of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). The layer is the
one [`field-event.md`](field-event.md) (group B) and
[`event-ops.md`](event-ops.md) (group V1) read: `ObjTrio` `0x802D40` is the
party, stride `0x14C`, member 0 the leader; `Sprite_Current` and
`Field_State` point into it. Every function is a faithful replacement, so no
`DIVERGENCE.md` entry is owed. Candidate defects of Capcom's are in section
6, kept as the original has them, for the coordinator to number.

## 1. Where they sit

`Field_LeaderFrame` (ours, field_event) jumps through **`Field_LeaderStates`**
`0x660918` by the leader's state `+1`; state 1, `Field_LeaderControl`, calls
through **`Field_LeaderControlSteps`** `0x660954` by `+2`. This group owns
state 1's sub-state 2 and five of the states. Each state's own table is
indexed by `+2`, a whole byte and unchecked, exactly as the original; ours
reads the same `.data` memory, so an index past the end calls what the next
table holds, as Capcom's does.

| Table | Entries | Indexed by | Entries' owners |
|---|--:|---|---|
| `0x660918` `Field_LeaderStates` | 15 | `+1` | 0, 1 group B; 2 `0x525370` group DB; **3, 4, 5, 7, 10 this group**; 6, 8, 9, 11..14 unread |
| `0x660954` `Field_LeaderControlSteps` | 3 | `+2` | 0, 1 ours earlier; **2 `Field_LeaderStepping`** |
| `0x660968` `Field_SwapSteps` | 5 | `+2` (state 3) | all this group's |
| `0x66097C` `Field_MenuSteps` | 2 | `+2` (state 4) | this group's |
| `0x660984` `Field_EncounterSteps` | 2 | `+2` (state 5) | this group's |
| `0x66098C` `Field_PassageSteps` | 4 | `+2` (state 7) | 0..2 this group's, 3 `0x52F8F0` unread (not reached) |
| `0x6609D0` `Field_ActionBySet` | 19 | `0x90412C & 0x7F` (state 10) | `0x51C760`..`0x525350`; entry 5 `0x51F1B0` is group DE's |
| `0x660B60` `Field_PendingJumps` | 4 | byte `0x904EF1` | **0 `Field_PendingWalkIn`**; 1..3 unread |

Three constant tables are named too: `Field_EncounterAreas` `0x660A90` (ten
word pairs), `Field_EventCells` `0x660B10` (11 codes and `0xFF`),
`Field_BlockingCells` `0x660C24` (7 codes and `0xFF`). All eleven are
`[[data]]` entries with counts. State 8's table starts at `0x66099C`, which
is how `Field_PassageSteps`' count is bounded; the step sounds (`u16`, event
ops) sit at `0x660960` between the control steps and the swap steps.

What sets each state was read from its setter: 3 `Field_LeaderSwapTest`
(Field_ScriptFlags2 bit 10), 4 `Field_LeaderMenuTest` (bit 11), 5
`Field_LeaderStepLands` on `Field_EncounterDue` (bit 12), 7
`Field_LeaderTalkTest` on a passage cell, 10 `Field_LeaderCheckTest` /
`Field_LeaderEffectTest`. Each state's last step clears its bit. The names
("swap", "menu", "encounter", "passage", "action") follow those setters;
what the player sees in each was not checked in game and the names are
hypotheses by shape.

## 2. The functions

Extents by recursive descent (capstone, 2026-09-25; every jump internal,
`nop` padding excluded). "Queue" is the round's routes.

| Function | Entry | Bytes | Reached from | Does |
|---|---|--:|---|---|
| `Field_LeaderStepping` | `0x52E110` | 0x28 | control step 2 | `+0x137` = 1; `+9` steps left: one fewer and `Field_LeaderStepTick`; at 0 `Field_LeaderStepLands` (both tail jumps) |
| `Field_LeaderStepLands` | `0x52E580` | 0x445 | `0x52E129`'s jmp only | where a step lands (below) |
| `Field_SwapState` | `0x52E9D0` | 0x12 | state 3 | `jmp [Field_SwapSteps + +2 * 4]` |
| `Field_SwapGather` | `0x52E9F0` | 0x1A4 | swap 0 | each member finds its spot; the ones after the leader that do fade (state 3) |
| `Field_SpotFree` | `0x52EBA0` | 0x7E | 7 calls in the swap | `+9` held at 0 over `Field_WayBlocked`, the ground within 0x40 of the leader's height; the spot to `0x903858` / `0x90385C`, its ground `0x903852` |
| `Field_SwapSpinOut` | `0x52ED80` | 0xE2 | swap 1 | the leader turns one step a frame, `+9` counting up to 4; the members darken (`+9 * 0xE0` in the three shade bytes) |
| `Field_SwapExchange` | `0x52EE70` | 0x336 | swap 2 | `ObjTrio_SwapFields` on the members in state 3, their sprites, records and places again; the camera's `Field_Kind2X` / `Z`, `F3Divisor` 0x20 and `FAWord` the height per frame |
| `Field_SwapSpinIn` | `0x52F1B0` | 0x196 | swap 3 | turning on, `+9` down; at 0 the palettes back and the shade cleared |
| `Field_SwapEnd` | `0x52F350` | 0x38 | swap 4 | once `Field_Kind2Hold` is 0: bit 10 cleared, `MapView_SetElevation`, state 1 |
| `Field_MenuState` | `0x52F390` | 0x12 | state 4 | the dispatch |
| `Field_MenuOpen` | `0x52F3B0` | 0x11 | menu 0 | `Field_Request` 1 |
| `Field_MenuWait` | `0x52F3D0` | 0x29 | menu 1 | while the request is 1 nothing; then bit 11 cleared, state 1 |
| `Field_EncounterState` | `0x52F400` | 0x12 | state 5 | the dispatch |
| `Field_EncounterStart` | `0x52F420` | 0x62 | encounter 0 | `Encounter_RollInitiative`, or in an event battle (`0x904AAA`) the members to state 5 and `+0xB` = 0xFF |
| `Field_EncounterWait` | `0x52F490` | 0x5C | encounter 1 | waits for the effect object `+0xB`, or the members in state 5; then bit 12 cleared, `Field_Request` 3 |
| `Field_PassageState` | `0x52F5E0` | 0x17 | state 7 | `call` the step, then a tail jump to `Sprite_ScriptTick` |
| `Field_PassageOpen` | `0x52F600` | 0x155 | passage 0 | the passage's message or event script (below) |
| `Field_PassageTake` | `0x52F760` | 0x13D | passage 1 | the passage's zenny or item, its flag in `0x90410C` (below) |
| `Field_PassageEnd` | `0x52F8A0` | 0x49 | passage 2 | once the request is 0, the pose and state 1 |
| `Field_ActionState` | `0x52FB60` | 0x43 | state 10 | the party set's handler; when it leaves `+0x137` at 0, the pose, member 0 cleared, state 1 |
| `Field_EncounterArea` | `0x5317F0` | 0x2E | `Field_LeaderWalk` | the encounter area of `Game_AreaNumber` from `Field_EncounterAreas` to `0x937F82` |
| `Field_CellHasEvent` | `0x531920` | 0x2B | the world map (11 sites), `Field_LeaderCellEvent` | the cell's code in `Field_EventCells` |
| `Field_ExitFromCell` | `0x531AF0` | 0x6F | `Field_LeaderCellEvent` | a `0xA0` cell's exit from the area's list (`0x462AC0`) |
| `Field_PendingWalkIn` | `0x533780` | 0x130 | pending jump 0 | the party walking in after `Area_Enter` (below) |
| `Field_WayBlocked` | `0x535610` | 0x29 | 11 sites | `0x535830` for a raised sprite (the byte), else `Field_WayBlocked4` |
| `Field_WayBlocked4` | `0x535640` | 0xE8 | `Field_WayBlocked` | blocking cells, the slopes by the position's fractions, the ground within 0xC0, an object there |
| `Field_CellsBlock` | `0x535730` | 0xFA | `Field_WayBlocked4`, `0x535830` | the `AreaMap_CellsNone` chain (below) |

The long ones in full, as ours has them (each `evidence` field in
`symbols.toml` has the same, with addresses):

- **`Field_LeaderStepLands`**: the run counter `+7` (while running up to
  0x11, else 0); the pace saved; `Field_EquipTick` unless on foot;
  `Field_Bit20Tick`, `Field_FloorDamage`; `Field_Tile89(0)`, `Field_Tile8A(0)`,
  `Field_TileD0`, `Field_TileA4` (the last with `MoveCmd_TestFB`), any one
  ending it. On foot: cell `0xAF` is the stored exit (`Field_ChangeArea` of
  `0x937F82`, `0x903860`, `0x90384C`, `0x905B88`; `0x904EE0` = 0, `0x937F98` =
  0xC); `Field_ScriptFlags2` bit 13 stops (`Field_ScriptFlags` bit 8, the
  pose); cell `0xC0` asks `Area_LinkAt`. With input bit 1, the map's edge -
  `Field_DirectionSteps[+8] * +0x70` on from the position, against 0x20000
  and `(AreaMap_Header[0 or 1] - 3) << 16` - leaves by `0x802290`,
  `0x7E091C`, `0x7E0920`. Then: footprint all `0xA7`, a link and
  `Field_Request` 5; none `0xA6`, the return point `0x904148`
  (`0x904152` = 0); `Field_ScriptFlags2` bit 5, a jump (`Field_JumpStart`,
  `+9 - 1`, the step tick, `+2` = 2). Otherwise `Field_EdgeBits` + 1 (+2 on
  the diagonals 2 and 6), `Sprite_ClearSteps`, `F3Divisor` / `FAWord` 0,
  the encounter (unless on foot or `Field_ScriptFlags2 & 0x60`: pace 3,
  bit 12, state 5); pace 3; `Field_LeaderEffectTest` **with the pace saved
  at the start**; `Scenario_ArriveHook` (its whole dword; non-zero stops);
  `Field_Bit80Tick`; `Field_LeaderWalk` again while `Field_InputHeld &
  (0x903580 | 0xF000)` and neither flag bit; else standing.
- **`Field_SwapGather`**: slot `i - 1`, the leader's the member count less
  one (the count byte decremented in place). The leader, and every member
  while `ObjTrio +0x138` bit 0 is set, try the leader's own spot; the
  others the formation spot `0x6608F6[2 * (slot + 2 * the leader's
  direction)]` (two `s8` << 15 on the leader's position) if
  `Field_SpotFree` and `0x52EC20` (the way there, unread) both allow, else
  the leader's. Then sub-state 1 if any member after the leader is in
  state 3, else 4 (straight to the end: nobody could move).
- **`Field_SwapExchange`**: `ObjTrio_SwapFields(0, 1, 1)` with member 1 in
  state 3 (and `(1, 2, 1)` with member 2 too), else `(0, 2, 1)` for member 2
  alone. Each member in state 3 becomes `Sprite_Current` and `Field_State`:
  `Field_MemberSprite(+0x89, i)`, `+0x138` from the start's `Field_State`,
  its actor record copied whole to `+0x80`, `+0x148`, the party list. The
  ones after the leader fade, face the leader's direction ^ 4 and find their
  spot again as the gather does; the spot (whatever `0x903858` holds - the
  fallback's answer is not read) becomes the position. The camera then
  follows: `Field_Kind2X` / `Z` = the leader's, and `MoveScript_FAWord` =
  `(s16 leader height - s16 MapView_Elevation) / frames` where frames is
  the larger distance moved `>> 13` (no division at 0).
- **`Field_PassageOpen`**: the passage entry `e` (`Area_PassageAhead`); the
  pose; `e +4` bit 15 clear or kind `e[6]` 0xFF: system message 0 and
  sub-state 1; kind 0 / 1 the system / script message `word & 0xFFF` and
  sub-state 2; kind 2 the area's event script: `Field_ActiveMember` pointed
  at a 0xA4-byte buffer on this frame's stack, `+0x86` = 0xFF,
  `EventScript_Run(Area_Descriptors[area] +4 [word & 0xFFF])`, the script
  message of the buffer's word `+0x88` unless 0xFFFF, `+0x86` to
  `Field_State +0x12A`, the word to `+0x12C`, sub-state 3 (`0x52F8F0`); any
  other kind sub-state 2. `Field_Request` 2.
- **`Field_PassageTake`**: once the request is 0: the flag `e[4]` already
  in `0x90410C` - system message 1; kind 0xFF - `0x90413C` + 1, the flag
  (unless 0xFF), sound 0x106, `0x5307C0(e[5] * 40)` (zenny: it prints
  n into `Text_Records`, opens message 5 and calls `0x591BE0(n, 0)`); an item (category
  `e[6]`, id `e[5]`, passed as the word `e +4 >> 8`) - its 16-byte name to
  `Text_Records`, `Inventory_Add(.., 1)`: taken, the flag then the count,
  sound 0x106, message 2; no room, message 3. `Field_Request` 2, sub-state 2.
- **`Field_PendingWalkIn`**: first frame (`0x904EF2` 0) the leader's `+0`
  bit 6 and state cleared, `+0x138` bit 1, the count 1. Then, once the
  leader is a whole unit or more from the stored point on either axis,
  each member after the leader whose bit `1 << count` is clear in
  `Field_ScriptFlags` is released into state 2 sub-state 2 (and the bit
  cleared in `Field_ScriptFlags2`), the count one on per member. When the
  count reaches the member count: `MoveCmd_TestFC` at the point, bit 4
  cleared, `0x904EF0` = 0.
- **`Field_CellsBlock`**: `AreaMap_CellsNone(x, z, wide, code, mask)` zero
  (a cell is the code) for any of `Field_BlockingCells` with mask 1; `0x10`;
  `0x11` unless `Field_State +0x138` bit 0; `0x20` and `0x21` both; `0xC0`
  and `0xA0` (mask 1) both; the answer is whether `0xA2` is there.

## 3. Quirks kept

All reproduced; none is a divergence.

1. Every dispatch index is a whole byte, unchecked (section 1). Ours reads
   the same memory.
2. `Field_LeaderStepLands` gives `Field_LeaderEffectTest` the pace read
   before the ticks, after it has set `+0x128` to 3 (control L11).
3. `Field_SpotFree` puts `+9` back only when the spot is free, into
   `Sprite_Current` read again (F2); on a blocked spot the member's `+9`
   stays 0.
4. `Field_SwapExchange`'s fallback spot search's answer is not read: a
   member with no free spot takes whatever `0x903858` / `0x90385C` held.
5. `Field_ExitFromCell`'s search has no end test: a `0xA0` cell with no
   record in the area's list walks on through memory.
6. `Field_PassageOpen` leaves `Field_ActiveMember` pointing at its dead
   stack buffer, and reads the buffer's `+0x88` whatever the script left
   there - the original never initialises it.
7. Arguments pushed with stale upper bytes (the byte arguments of
   `Field_SpotFree`, `0x52EC20`, `Field_LeaderEffectTest`,
   `MapView_SetElevation`, `Item_NamePtr`, `Inventory_Add`,
   `AreaMap_CellsNone`'s code): every callee reads only the low byte (word
   for the elevation), checked in each owned callee's source - for the
   unowned `0x52EC20`, the one use read is its pass to `Field_WayBlocked`,
   which tests the byte; ours passes clean
   values and the recorders record what the callees read.
8. `Field_SpotFree`'s third argument is never read.
9. **Ours aborts where the original faults**: a null passage in
   `Field_PassageOpen` / `Field_PassageTake` (the original reads through
   it) is a `bof3::Fatal` naming the function (rule 4, as round seven's
   stack tables). `Field_LeaderTalkTest` only sets state 7 on a non-null
   passage, so this needs the passage to vanish between two frames.

## 4. Callees, and other groups' addresses

Everything but four callees is ours already and called by name through
`event_leader::g`. The four nobody owns go through raw addresses in
`event_leader_callees.h`:

- `0x52EC20` - the way from the leader to a spot (walks both axes calling
  `MapView_GroundAt` and `Field_WayBlocked`; 0x15D bytes). **Not in any
  group of this round** although the queue's "Folded into" column names it
  as the host of `0x52ED80`..`0x52F490`; it is reached (the swap calls it).
- `0x5307C0` - the zenny: sound 0x106, `sprintf(Text_Records, [0x5E10C0], n)`,
  `Msg_OpenSystem(5)`, `Field_Request` 2, `0x591BE0(n, 0)`.
- `0x462AC0` - the area's exit list: `0x653924 + 28 * 0x462A90()`'s dword.
  Group DA takes `0x462AE0`.. but not this one.
- `0x535830` - `Field_WayBlocked` for a raised sprite (unread beyond its
  first call, `Field_CellsBlock(x, z, 1)`).

For other groups (said, not acted on):

- **Group DE's `0x51F1B0` is entry 5 of `Field_ActionBySet` `0x6609D0`**,
  which this group names. DE should not name that table again.
- **Group DB's `0x525370` is entry 2 of `Field_LeaderStates` `0x660918`**,
  named here; the same applies.
- `Field_LeaderStates` and `Field_LeaderControlSteps` were constants in
  `field_event_callees.h` (group B) and are now `[[data]]` entries too; the
  constants were left as they are.

## 5. The fuzz and its negative controls

`BOF3X_SHADOW=event_leader` (`src/game/event_leader_fuzz.cpp`), before the
module's own Inject. 27 byte-copies, every E8 / E9 leaving each extent
re-aimed at a recorder (`CloneCall` with `expected`); the five dispatch
tables' 32 entries swapped for numbered recorders in `.data` (ours and the
copies read the same tables); `Area_Descriptors[0..7]` pointed at fake
descriptors whose script lists hold tokens. Everything is put back.

**Each round** starts from random bytes over 0x1B63 bytes: `ObjTrio`, 32
effect objects, the eight actor records, the scratch `0x90384C..`, the
field's flags and input, the party lists, the passage flags / party set /
count / return point `0x90410C..0x904154`, the pending walk-in and the
area `0x904EE0..`, the edge and exit words, `Field_State`,
`Sprite_Current`, `Field_Kind2X` / `Z`, `Field_Kind2Hold`,
`MapView_Elevation`, `Field_Request`; and over the passage, item-name and
exit records the recorders hand out. Pointers are put back inside the
party, the member count at 3 or less, `+0xB` inside the 32 effects,
`+0x89` below 24 and the area below 8. Each branch's edges are seeded: the
run counter at 0x10 / 0x11, the input bits, the map edge on the step, the
held buttons, the sub-states in range, the swap members in state 3, the
shade at `-32 n` and one either side, the distance at `0x2000` and
`0x1FFF`, heights at 0x40 / 0xC0 from the recorders' ground, fractions 0,
the passage kinds 0, 1, 2, 3, 0xFF and 7, the walk-in point at 0xFFFF /
0x10000 / 0x10001, the encounter areas from the table.

**The recorders are not quiet**: three calls in four move one of 22
watched bytes (the two pointers, the member count and states, the flags,
the request, the target, the slope byte, the walk-in count, the passage
bytes, the pace, the camera, the held buttons, the shades, the effects).
In addition the ground recorder and the object recorder move
`Sprite_Current` half the time, `Sprite_ClearSteps`' flips input bit 0,
`Flags_Set`'s moves the passage count, `EventScript_Run`'s moves the
passage word, `MapView_SlopeAt`'s sets the slope byte, and the exit list
plants the matching record (with same-x decoys before it) after its move.
`Field_ActiveMember`, once a function points it at its own frame, is
compared as a marker.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=event_leader`, exit 0:

    event_leader self-test: 27000 rounds over 27 functions (1000 each), 54147 calls to the stand-ins, 0 MISMATCHES
    coverage: table entries 32 of 32; lands: change area 194, link 129, cells 472 / 7215, jump 38, encounter 89,
      effect 143, arrive 109, walk 37; swap: spot 3766, path 489, sprite 1222, palette 225, elevation 506; passage:
      script 121 (frame pointed 121), messages 200 / 1178, flags 834 / 206, zenny 166, item 392 / 392; walk-in 266;
      slopes 884, objects 149; spot free 1 114 0 886, event cell 1 519 0 481, way blocked 1 502 0 498,
      footprint 1 922 0 78, cells block 1 695 0 305

With `BOF3X_SHADOW='*'`: exit 0, `inject: 1289 ours, 0 left original`, no mismatch and no Fatal anywhere in the log.

**Negative controls.** A script (`controls.py`, in the session scratchpad
under `DC/`) planted each alone: replace, build, self-test, revert. The
count is the refusing rounds of that function's 1,000.

| | Planted | Refused in |
|---|---|--:|
| S1 | Stepping: lands at +9 = 1 too | 332 |
| S2 | Stepping: +0x137 = 2 | 993 |
| L1 | StepLands: the run counter to 0x10 | 118 |
| L2 | StepLands: the equipment tick by input bit 1 | 458 |
| L3 | StepLands: tile 8A not tested | 882 |
| L4 | StepLands: the exit cell 0xAE | 32 |
| L5 | StepLands: 0x937F98 = 0xD | 13 |
| L6 | StepLands: the stop sets flags bit 9 | 42 |
| L7 | StepLands: the edge at size - 2 | 9 |
| L8 | StepLands: the 0xA7 test with mask 1 | 472 |
| L9 | StepLands: 0x904152 kept | 134 |
| L10 | StepLands: the edge bits twice on direction 7 | 27 |
| L11 | StepLands: the effect test with the pace now | 143 |
| L12 | StepLands: the arrive hook tested as a byte | 25 |
| L13 | StepLands: the held mask 0xE000 | 3 |
| L14 | StepLands: the stop clears +0x137 | 94 |
| L15 | StepLands: the input flags not re-read after the ticks | 90 |
| L16 | StepLands: running counts past 0x11 | 483 |
| G1 | SwapGather: ObjTrio +0x138 not tested | 177 |
| G2 | SwapGather: the count not decremented for the leader | 676 |
| G3 | SwapGather: the fallback with the member's +0x70 | 10 |
| G4 | SwapGather: the leader faded too | 531 |
| G5 | SwapGather: sub-state 3 when none | 487 |
| G6 | SwapGather: the count not re-read per member | 606 |
| F1 | SpotFree: a rise of 0x40 refused | 63 |
| F2 | SpotFree: +9 back into the first Sprite_Current | 32 |
| F3 | SpotFree: the ground word not stored | 114 |
| O1 | SpinOut: the shade at the edge too | 207 |
| O2 | SpinOut: the next sub-state at 3 | 562 |
| O3 | SpinIn: turning back | 1000 |
| X1 | Exchange: the second swap without keep | 159 |
| X2 | Exchange: +0x138 with bit 1 | 411 |
| X3 | Exchange: facing the leader ^ 2 | 448 |
| X4 | Exchange: the leader at a formation spot too | 364 |
| X5 | Exchange: the smaller distance | 936 |
| X6 | Exchange: the frames >> 12 | 888 |
| X7 | Exchange: the party list from Sprite_Current | 50 |
| X8 | Exchange: the actor index from the record copy | 769 |
| I1 | SpinIn: the end at +9 = 1 too | 268 |
| I2 | SpinIn: +0x20 kept | 172 |
| I3 | SpinIn: the count re-read for every member | **not refused; changes nothing** |
| E1 | SwapEnd: bit 9 cleared | 368 |
| E2 | SwapEnd: the hold not waited for | 494 |
| M1 | MenuOpen: request 2 | 1000 |
| M2 | MenuWait: waits for 0 | 265 |
| M3 | MenuWait: bit 0 cleared too | 228 |
| N1 | EncounterStart: event battle 1 only | 493 |
| N2 | EncounterStart: +2 = 5 | 246 |
| N3 | EncounterStart: +0xB = 0 | 498 |
| W1 | EncounterWait: member 2 not waited for | 66 |
| W2 | EncounterWait: request 4 | 521 |
| W3 | EncounterWait: the effect done to 0 | 149 |
| D1 | SwapState: sub-state ^ 1 | 1000 |
| D2 | MenuState: by +1 | 471 |
| D3 | EncounterState: always the first | 497 |
| D4 | PassageState: no script tick | 1000 |
| D5 | ActionState: +0x137 of Sprite_Current | 336 |
| D6 | ActionState: member 1 cleared | 490 |
| P1 | PassageOpen: the reach pose + 0x11 | 484 |
| P2 | PassageOpen: bit 14 for the content | 363 |
| P3 | PassageOpen: the buffer +0x86 = 0xFE | 121 |
| P4 | PassageOpen: no message on 0 | 29 |
| P5 | PassageOpen: +0x12C the word from before the script | 63 |
| P6 | PassageOpen: kind 1 to sub-state 3 | 108 |
| T1 | PassageTake: zenny * 32 | 165 |
| T2 | PassageTake: the zenny flag set before the count | 37 |
| T3 | PassageTake: the item id >> 7 | 388 |
| T4 | PassageTake: 15 bytes of name | 391 |
| T5 | PassageTake: no room is message 2 | 142 |
| T6 | PassageTake: the request not waited for | 166 |
| T7 | PassageEnd: the pose by input bit 1 | 417 |
| A1 | EncounterArea: the last pair skipped | 75 |
| A2 | EncounterArea: the first word stored | 677 |
| C1 | CellHasEvent: the first code skipped | 71 |
| H1 | ExitFromCell: cell 0xA1 | 131 |
| H2 | ExitFromCell: x 0xE0000 | 70 |
| H3 | ExitFromCell: the z not compared | 66 |
| K1 | WalkIn: a whole unit is near | 34 |
| K2 | WalkIn: the member's bit | 137 |
| K3 | WalkIn: sub-state 1 | 114 |
| K4 | WalkIn: the count not re-read after the clear | 16 |
| K5 | WalkIn: bit 5 cleared at the end | 203 |
| B1 | WayBlocked: the raised dword whole | 492 |
| Q1 | WayBlocked4: below as direction 6 | 363 |
| Q2 | WayBlocked4: a ground of 0xC0 blocks | 17 |
| Q3 | WayBlocked4: the height back into the first Sprite_Current | 46 |
| Q4 | WayBlocked4: object 0 is none | 78 |
| Q5 | WayBlocked4: the right slope without the slope byte | 54 |
| R1 | CellsBlock: the list with mask 0 | 1000 |
| R2 | CellsBlock: 0x11 whatever +0x138 | 185 |
| R3 | CellsBlock: 0xA0 with mask 0 | 50 |
| R4 | CellsBlock: 0xA3 last | 342 |
| R5 | CellsBlock: 0x20 or 0x21 | 354 |

**Not counted:** I3, `Field_SwapSpinIn`'s count read at every member
instead of only after a member in state 3, is a change that changes
nothing: every call in that loop is followed by the re-read, and the first
loop makes no call, so the two counts are always equal.

**The thinnest:** L5 (13), L7 (9), L13 (3), G3 (10), K4 (16), Q2 (17).

**What the fuzz cannot see:**

- anything the callees really do - `0x52EC20`, `0x535830`, the states' own
  handlers (state 10's 19, `0x52F8F0`), the event script;
- the order of stores with no call between them;
- the garbage upper bytes of the original's pushed arguments (section 3,
  item 7) - the recorders record what the callees read;
- the uninitialised `+0x88` of `Field_PassageOpen`'s buffer - the script
  recorder always writes it;
- a null passage (ours aborts, the original faults), an exit list with no
  match (both walk on), and dispatch indices past their tables (never
  generated: they would call real code).

## 6. Candidate defects (for [`known-defects.md`](known-defects.md); the coordinator numbers them)

1. **An uninitialised message id.** `Field_PassageOpen`'s kind-2 path reads
   the word `+0x88` of a stack buffer it never initialises and opens that
   script message (`& 0xFFF`) unless it is 0xFFFF. Unless the event script
   always writes `+0x88` through `Field_ActiveMember`, a kind-2 passage can
   open a message chosen by stack garbage. Latent: which scripts write it
   was not read, and no kind-2 passage is on the routes as far as was
   checked.
2. **A dangling pointer.** The same path leaves `Field_ActiveMember`
   pointing at the dead buffer. Whatever next reads it before it is set
   again reads a dead stack frame. Latent; its other users were not read.
3. **A blocked spot zeroes a member's step count.** `Field_SpotFree` zeroes
   `Sprite_Current +9` and restores it only on success; the swap calls it
   for each member, so a member whose spot is blocked loses its `+9`. What
   `+9` holds for a member at that point was not established.
4. **No end to the exit search** (`Field_ExitFromCell`, quirk 5): a map
   whose `0xA0` cell has no record hangs or faults.

## 7. For the batch check

The shop route reaches the passage states (7), the world map the swap (3),
the menu (4), the action (10), the cells and the walk-in, the combat route
the encounter (5); all three the step's landing. `BOF3X_ORIGINAL` with all
27 names restores Capcom's.

`analysis/calltrace/entries_logic.txt` (gitignored; appended in the main
checkout, 2026-09-25):

```
0052E110 28     0052E580 445    0052E9D0 12     0052E9F0 1A4
0052ED80 E2     0052EE70 336    0052F1B0 196    0052F350 38
0052F390 12     0052F3B0 11     0052F3D0 29     0052F400 12
0052F420 62     0052F490 5C     0052F5E0 17     0052F600 155
0052F760 13D    0052F8A0 49     0052FB60 43     00533780 130
```

The other seven were already listed with the right sizes (`0052EBA0 7E`,
`005317F0 2E`, `00531920 2B`, `00531AF0 6F`, `00535610 29`, `00535640 E8`,
`00535730 FA`). Two existing lines overrun this group and should be cut by
the merger: **`0052EC20 943`** (its body is 0x15D; the 943 runs over
`0x52ED80`..`0x52F490`) and **`0052F570 934`** (a 0x41-byte function; the
934 runs over `0x52F5E0`..`0x52FB60`).
