# The battle windows

**Status:** IN PROGRESS (2026-09-23) - twenty-two functions ours
(`src/game/battle_windows.cpp`, shadow name `battle_windows`), each fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder:
66,000 rounds, 0 mismatches; 53 negative controls planted, 50 refused by a
count, three not refused because they change nothing. `BOF3X_SHADOW='*'` passes.
**Through the wave-2 batch** (2026-09-24, 1,020 ours - [`takeover-queue-round7.md`](takeover-queue-round7.md) "Result": the combat A/B 5 of 43 at 4..15 px, the tile-edge class); section 6 had it owed.

Group BC of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)): the handlers the
window task `0x596A90` calls in battle - the party row, the command cross, the
command label and target banners, the plain boxes - its small helpers, and
the battle party's per-frame state step. Everything here is a *faithful*
replacement: no `DIVERGENCE.md` entry is owed.

## 1. The functions

Every extent measured by recursive descent from the entry (a scratch capstone
walker, 2026-09-23), not taken from the catalogue. The PSX column is
`analysis/pairs_propagated.json`'s twin in `BATTLE.EMI` (section
`8a80230e...`, loaded at `0x801D0C00`) and its tier; the twins marked *read*
were disassembled from the sibling's overlay capture and compared.

| PC | Name | Bytes | PSX twin | What it does |
|---|---|---|---|---|
| `0x441100` | `BattleParty_RunStates` | 0x3C | - | each of ObjTrio's three objects made `Field_State` / `Sprite_Current`, then its state step (the third a tail jump) |
| `0x441140` | `BattleParty_UpdateScreens` | 0x33 | - | the same three, `Sprite_UpdateScreen` when +0 has bit 0 and not bit 6 |
| `0x441180` | `BattleObj_ScriptTick` | 0x29 | `0x801DEDA0` call-anchored, *read* | `Sprite_ScriptTick` unless +0x90 bit 2, or the gate `0x904B8E` with +0x134 lacking bit 4; else al 1 |
| `0x4411B0` | `BattleObj_ScriptTickOnce` | 0x29 | `0x801DEE08` call-anchored | the same gate before `Sprite_ScriptTickOnce` |
| `0x4411E0` | `BattleObj_RunState` | 0x17 | `0x801DEE70` gap34, *read* | tail jump through the 27 handlers at `0x64DFE0` by the state byte +1 |
| `0x4412B0` | `BattleObj_PickPose` | 0x246 + table | `0x801DEFA0` call-anchored, *read* (first block) | the party object's standing pose |
| `0x442FA0` | `BattleWin_DrawPartyStatus` | 0x342 | `0x801D74DC` callers | the party row: per member the gauge, name, two values, box |
| `0x4432F0` | `BattleWin_DrawCommandCross` | 0x1C9 | `0x801D7A10` call-anchored | the command cross: five arms and two extras |
| `0x4434C0` | `BattleWin_DrawCommandIcon` | 0x14D | `0x801D7CE0` call-anchored, *read* | a grown command icon, a POLY_FT4 |
| `0x443610` | `BattleWin_DrawMessageBox` | 0x12F | `0x801D7E80` gap67 | a plain box 0x113 wide (the battle message's) |
| `0x443740` | `BattleWin_DrawSmallBox` | 0x12C | `0x801D8060` gap67 | a plain box 0x43 wide |
| `0x443870` | `BattleWin_DrawMediumBox` | 0x12C | `0x801D8240` gap67 | a plain box 0x63 wide |
| `0x4439A0` | `BattleWin_DrawCommandLabel` | 0x165 | `0x801D8420` callers | the chosen command's label banner (section 5) |
| `0x443B10` | `BattleWin_DrawTargetEnemy` | 0x274 | `0x801D8660` callers | an enemy target banner, two layers |
| `0x443D90` | `BattleWin_DrawEnemyStatus` | 0x1CB | `0x801D8AB4` gap67 | an enemy banner, one layer, the name only when `0x444EB0` allows |
| `0x443F60` | `BattleWin_DrawTargetMember` | 0x2C5 | `0x801D8DC8` callers | a party member target banner |
| `0x5979E0` | `Window_FlagAndAdvance` | 0x12 | - | current window +0xF = 1, state +3 on |
| `0x597A00` | `Window_RestoreAndBack` | 0x2C | - | current window's position from +0x10 / +0x12, +0xF = 0, state +3 back |
| `0x597A30` | `Window_DispatchKind` | 0x4F | - | one of three handlers by `*arg7`, all eight arguments passed on |
| `0x597ED0` | `BattleWin_DrawMessage` | 0x4A | - | the message box, then the ring's current entry through `Text_DrawAt` |
| `0x597F20` | `BattleMsg_Advance` | 0x1A | - | the message ring's read index `0x93C2A0` on (mod 16); al 1 at the write index |
| `0x597F40` | `Text_GlyphCount` | 0x1D | - | characters in a string, a byte >= 0x80 taking the next |

**Sizes against `analysis/calltrace/entries_logic.txt`** (pe_funcs.py's).
Four are wrong and matter now that the functions are owned (the trap in
HANDOFF: the tracer reads an owned entry's size): `0x4411E0` 0xC4 (really
0x17 - it runs into the first state handler `0x441200`), `0x4412B0` 0x105F
(really 0x246 of code and a six-entry switch table at `0x4414F8`, 0x260 in
all), `0x597A30` 0x4A0 (really 0x4F - it runs over the three window-kind
handlers), `0x597F40` 0x38A (really 0x1D). The other eighteen agree.
**`0x4412B0` is not recursive**: `pc_funcs.json`'s "`0x4412B0` calls
`0x4412B0`" is its 0x105F-byte extent swallowing the state handlers after
`0x441510`, which call it; likewise its 30 "callees".

**Pointer-reached neighbours, found and not taken:** the 27 state handlers
at `0x64DFE0` (`0x441200`, `0x441550`, `0x441570`, `0x441860`, `0x441930`,
`0x441A10`, `0x4429E0`, `0x442BA0`, `0x442C70`, `0x442D30`, `0x442D60`,
`0x4417A0`, `0x4417F0`, `0x441890` x 11, `0x442E40`; `0x437CC0` is a bare
`ret`) - they are what call `0x441180`, `0x4411B0` and `0x4412B0`; and the
three window-kind handlers `0x597A80`, `0x597BD0`, `0x597C10` that
`Window_DispatchKind` stores on its stack - they call `0x443610`,
`0x597ED0` and `0x597F20`. None is in this group.

**PSX twins read:** `0x4434C0` against `0x801D7CE0` (the same seven CLUT
bytes 8 9 8 9 8 8 8, copied there from `0x801D0C64`, indexed by `a0 & 0xFF`
without a bound, u = k << 5 .. + 0x18, v 0xE0 .. 0xF8); `0x441180` against
`0x801DEDA0` (the same gate, fields +0x80 / +0x128 there); `0x4411E0`
against `0x801DEE70` (the same table jump, `jalr` through `0x801EB094`);
`0x4412B0`'s first block against `0x801DEFA0` (the same kind-5 test, the same
+0x1C / +0x4B / +0x20 poses through `0x8014D4E4`, set `0x800F0800`, 0x1800).
No twin was found for the eight without one in the table.

## 2. What they do, and the data

The notes below are the reads; each function's comment in
`battle_windows.cpp` is the full one.

- **Party objects.** ObjTrio `0x802D40`, stride 0x14C: +0 flags (bit 0
  running, bit 6 hidden), +1 state, +8 pose base, +0x4B current pose, +0x80
  the name, +0x90 / +0x91 status flags (bit 14 grey, 0xBFC coloured, bit 13
  the value's warning colour, bit 11 and bit 2 poses), +0x98 / +0x9A / +0xA0 /
  +0xA2 the drawn values, +0x125 action, +0x126 skill, +0x130, +0x134.
  Per-member bar bytes at `0x80333F + 36 i`.
- **The command cross.** `0x904AB4` the selection (a byte, read as a dword
  and re-read after each icon), `0x904ABC..0x904AC2` seven growths, arm
  points `0x64E2AC` (two bytes an arm), `0x939EC4` the acting member's
  record (+0x134 bit 1 dims arms 2 and 3).
- **Banners.** `Battle_CommandBoxes` `0x64E2C8` and
  `Battle_CommandLabelPointers` `0x669D60` for the label; enemies at
  `EnemyWorkingRecords` `0x93B9E0`, stride 0x128 (+0 name, +0xF 1 = has a
  gauge; slot t is enemy t - 3); the window colour's line colour is the
  first word of CLUT shadow row s8 `0x903A5A` (`0x80B7A8 + 64 row`), 5-bit
  components shifted to 8.
- **The message ring.** 16 entries of 8 bytes at `0x93C2C4` (+0 the text),
  read index `0x93C2A0`, write index `0x93C2A1`.

Points that matter for faithfulness:

- **Upper halves.** Every callee reads the low 16 bits of a coordinate and
  the low byte of a colour, size or flag (the stand-ins in the fuzz mask
  exactly that; each BD helper's reads were checked in its disassembly:
  `movsx word` coordinates, `and 0xFF` bytes, `0x4449E0`'s colour its low 15
  bits, `0x444C40`'s digit its low 4). The one exception is `0x516E70`,
  which hands x and y on whole - ours computes them at full width as the
  original does, and its stand-in records them whole.
- **Reads after calls** are kept where the original has them: the party
  count after each member, the window colour after the first draw mode (and
  in `0x442FA0` after the frame's tile), the charge byte +3 after the first
  line, the member's flags before each colour, the cross's selection after
  each icon, the extras' growths, the enemy's gauge flag, the current window
  and the ring index after the message box.
- **Colours kept in argument slots.** `0x443F60` keeps its status colours in
  its first argument's slot, and the slash's colour overwrites the first
  value's there - so its second value (+0xA0) is drawn in the slash's colour
  (2 on bit 14, else 0), not the first value's (4 on bit 13). The fuzz found
  ours drawing it with the first colour; fixed before any commit.
- **Tail jumps.** `0x441100`, `0x441180`, `0x4411B0` and `0x4411E0` end in
  a jump, so their answer is the callee's eax; ours returns it. Callers of
  the two ticks read al only (30-odd call sites, each a `test al, al` or a
  store of al).

## 3. The fuzz (`BOF3X_SHADOW=battle_windows`, `battle_windows_fuzz.cpp`)

Twenty-two byte-copies; every relative call and tail jump out re-aimed at a
recording stand-in, for ours through `battle_windows::g` alike; the state
table's `jmp [ecx*4 + 0x64DFE0]` operand in `0x4411E0`'s copy aimed at 256
entries cycling 32 recording handlers; the three handler immediates in
`0x597A30`'s copy aimed at three recording handlers; `0x4412B0`'s switch
table relocated. 3,000 rounds a function: random bytes in twelve game
regions (the packet cursor, ObjTrio and the bar bytes, the CLUT shadow rows
-2..9, the window colour, the battle's bytes `0x904AA0..0x904AEF`, the tick
gate and the print buffer, the current window pointer, `Field_State`,
`Sprite_Current`, the actor pointer, six enemy records, the message ring)
and in our own pool, objects, window records and strings; then each
branch's boundaries seeded (party counts 0..5, status flags on and off each
tested bit, a value at and around a quarter of its maximum, selections
0..7, growths zero or not, command icons 0..6 and 8..43, CLUT colours with
components 0..3 either side of the darker lines' clamp, gauge flags 1 or
not, pose actions 0..7 with skills whose flag byte tells bit 3 apart, ring
indices at the wrap, strings ending in a lead byte); the copy, then ours,
under x87 control word 0x027F; everything compared, with the answer at the
original's width (al for the ticks, `BattleMsg_Advance`, `Text_GlyphCount`;
eax for `BattleParty_RunStates`, `BattleObj_RunState`, `BattleObj_PickPose`,
`Window_DispatchKind`) and the stand-ins' log.

The stand-ins do what callers read back: the commit moves the packet cursor
and stamps a tag, the draw mode and POLY_FT4 setters write their bytes,
sprintf writes the print buffer without the CRT, and most disturb a cell some
caller reads again (the colour, the party count, the cross's selection and
growths, members' flags and values, bar bytes, enemies' gauge flags, the
current window and its fields, the ring index, the actor's flag).

Results (`build/bof3x.log`): 66,000 rounds, about 447,000 stand-in calls, 0
mismatches; `BOF3X_SHADOW='*'` passes. Coverage: 6,422 members drawn, 999
enemy names hidden, 992 command icons past the CLUT table, 2,471 state
handlers and 3,000 window-kind handlers reached, 300 poses left alone.

**Negative controls** (planted one at a time through a temporary switch, all
removed; refusals are rounds of 3,000):

| # | Function | Planted | Refused in |
|---|---|---|---|
| 1 | `BattleParty_RunStates` | second object's `Field_State` the third | 3,000 |
| 2 | `BattleParty_RunStates` | second object's `Sprite_Current` not set | 3,000 |
| 3 | `BattleParty_UpdateScreens` | bit 6 not tested | 1,765 |
| 4 | `BattleParty_UpdateScreens` | running on bit 1 | 1,737 |
| 5 | `BattleObj_ScriptTick` | gate on +0x134 bit 5 | 562 |
| 6 | `BattleObj_ScriptTick` | al 0 when +0x90 bit 2 | 752 |
| 7 | `BattleObj_ScriptTickOnce` | ticks with `Sprite_ScriptTick` | 1,452 |
| 8 | `BattleObj_ScriptTickOnce` | gate byte `0x904B8F` | 895 |
| 9 | `BattleObj_RunState` | not running answers 0 | 529 |
| 10 | `BattleObj_RunState` | state + 1 | 2,471 |
| 11 | `BattleObj_PickPose` | the +0x4B pose as +8 + 0x20 | 165 |
| 12 | `BattleObj_PickPose` | bit 11's pose 0x2C | 300 |
| 13 | `BattleObj_PickPose` | skill flag bit 4 | 62 |
| 14 | `BattleObj_PickPose` | action 3 / past 5 answers 0 | 300 |
| 15 | `BattleObj_PickPose` | own poses under kind 4 | 585 |
| 16 | `BattleWin_DrawPartyStatus` | members to the count inclusive | 1,607 |
| 17 | `BattleWin_DrawPartyStatus` | colour read before the frame's tile | 107 |
| 18 | `BattleWin_DrawPartyStatus` | charge byte +3 read before the first line | 35 |
| 19 | `BattleWin_DrawPartyStatus` | warning colour at a quarter inclusive | 97 |
| 20 | `BattleWin_DrawPartyStatus` | member step 0x5D | 1,366 |
| 51 | `BattleWin_DrawPartyStatus`, `BattleWin_DrawTargetMember` | name colour mask 0xFFC | 37, 82 |
| 21 | `BattleWin_DrawCommandCross` | selection not re-read | 452 |
| 22 | `BattleWin_DrawCommandCross` | arm 1 dimmed too | 1,455 |
| 23 | `BattleWin_DrawCommandCross` | extra 6 at arm 5's point | 1,495 |
| 24 | `BattleWin_DrawCommandCross` | shade 0x80 recomputed for a selection >= 5 | **not refused** |
| 52 | `BattleWin_DrawCommandCross` | growth halved by 4 | 2,863 |
| 25 | `BattleWin_DrawCommandIcon` | CLUT byte 1 as 8 | 291 |
| 26 | `BattleWin_DrawCommandIcon` | frame byte one off past the table | 767 |
| 27 | `BattleWin_DrawCommandIcon` | the float x + w slot not emulated | 88 |
| 28 | `BattleWin_DrawCommandIcon` | bottom v 0xF7 | 3,000 |
| 29 | `BattleWin_DrawMessageBox` | right edge 0x114 | 3,000 |
| 30 | `BattleWin_DrawSmallBox` | tile 5 | 3,000 |
| 31 | `BattleWin_DrawMediumBox` | last line g and b swapped | 2,897 |
| 32 | `BattleWin_DrawCommandLabel` | 7 bytes of label | 3,000 |
| 33 | `BattleWin_DrawCommandLabel` | label at y + 4 | 3,000 |
| 34 | `BattleWin_DrawTargetEnemy`, `BattleWin_DrawTargetMember` | darker clamp at -8 | 1,224, 1,172 |
| 50 | the same | darker clamp at 0xFF removed | **not refused** |
| 35 | `BattleWin_DrawTargetEnemy` | enemy t - 2 | 3,000 |
| 36 | `BattleWin_DrawTargetEnemy`, `BattleWin_DrawEnemyStatus` | gauge flag 2 | 1,229, 1,225 |
| 37 | `BattleWin_DrawEnemyStatus` | name always drawn | 3,000 |
| 38 | `BattleWin_DrawEnemyStatus` | `0x444EB0` handed t | 3,000 |
| 39 | `BattleWin_DrawTargetMember` | slash colour from bit 13 | 1,249 |
| 40 | `BattleWin_DrawTargetMember` | name in the value's colour | 1,551 |
| 41 | `Window_FlagAndAdvance` | +0xF = 2 | 3,000 |
| 42 | `Window_RestoreAndBack` | y from +0x10 | 3,000 |
| 43 | `Window_DispatchKind` | handler index mod 2 | 1,045 |
| 44 | `BattleWin_DrawMessage` | window not re-read after the box | 81 |
| 45 | `BattleWin_DrawMessage` | count 0xFE | 3,000 |
| 46 | `BattleMsg_Advance` | index mod 32 | 656 |
| 47 | `BattleMsg_Advance` | compared with the old index | 1,767 |
| 48 | `Text_GlyphCount` | lead bytes from 0x81 | 131 |
| 49 | `Text_GlyphCount` | count from 1 | 3,000 |
| (34) | first form: darker clamp at 1 | - | **not refused** |

The three not refused change nothing, and why is worth keeping:

- **24**: with a selection of 5 or more no arm is skipped, so arm 4 was
  drawn last and left its shade, 0x80 - the original's "shade left from the
  loop" is always 0x80 there.
- **50** and the first form of **34**: the line colour's components are
  multiples of 8 up to 0xF8, so c - 0x10 is never above 0xFF, and a clamp
  at 1 differs from one at 0 only at c - 0x10 = 0, which clamps to 0 either
  way. The control re-planted at -8 is refused (components 0 and 1 seeded).

Control 18 (35 rounds) needs the charge byte changed under the first line's
call; control 13 (62) needs a skill whose static flag byte tells bit 3 from
bit 4 - the table is `.data` the fuzz does not vary, so the seed picks such
skills from it.

## 4. Defects of Capcom's, and not-as-the-original

Latent defects, kept as the original has them (the coordinator numbers them
in [`known-defects.md`](known-defects.md)):

- **`0x4434C0`'s CLUT table has no bound** (the PlayStation's too): a command
  index k past 6 reads its own stack frame - 7 an unwritten byte, 8..11 its
  return address, 12..35 its arguments (two slots already overwritten with
  the float x + w and the int y + h), 36 and up its caller's frame. The
  selection `0x904AB4` is written by the battle task's cursor code
  (`0x42E505` 0, `0x42ED05`, `0x42ED46`, `0x42ED80` 0, `0x43116F`) as a
  position on the five-arm cross, and the extras pass 5 and 6; the ranges of
  the cursor writes were not proven, but 7 or more would need the cursor off
  the cross. Ours reads the same bytes from its own frame, as
  `Menu_DrawIcon`'s `IconClutPastTable` does (`char_stats.cpp`); the fuzz
  checks 8..43, not 7 (undefined on both sides).
- **`0x597A30`'s three handlers are on its stack with no bound**: a kind byte
  of 3 calls its own return address, 4..11 its arguments as function
  pointers, 12 and up words of its caller's frame. Ours calls the same
  words. Fuzz-seeded only 0..2 (anything else jumps into garbage on both
  sides).
- **`0x4411E0`'s state table has no bound**: 27 handlers, a state byte to
  255 (the PlayStation's too); past 26 it jumps through the bytes after the
  table. Ours reads the same table.
- **`0x4439A0`'s command index has no bound**: `Battle_CommandBoxes` has 7
  records and `Battle_CommandLabelPointers` 7 entries; k past 6 reads the
  data after them.
- **`0x597F40` reads past the NUL** when a lead byte (>= 0x80) stands right
  before it: it steps over the NUL as the second byte and counts on.
- **`0x444EB0`'s answer** (group BD's) decides whether an enemy's name is
  shown; noted here only because `0x443D90` depends on it.
- **Enemies of one kind show as nested, overlapping boxes, by design.** In
  `Battle_OpenEnemyNames` `0x494A80`, a kind's windows sit 8 apart. Each is
  a full `0x443D90` banner, 0x15 tall:
  - the see-through tile 8 (76 x 17, blend mode 0);
  - the gauge's black tile 9;
  - edge shapes 2 and 3;
  - four lines.

  Only the name is left out after the first. The bodies stack into darker
  bands, and each later frame shows as a box inside the one above. The
  owner took it for a port artefact (2026-09-24). A research pass read it as
  the PlayStation's: the US disc's `0x801D8AE4`, the tables at `0x801EAE50`
  and `0x801EAD30`, and the layout at `0x800AC964` all match. The owner's
  capture of the Japanese release confirmed it the same day. So did the faint
  line under the name. A one-box drawing was built, captured on the owner's
  `combatGroup` route and dropped: **the owner chose Capcom's design**, which
  also closes each banner on its own as an enemy dies. Not a defect; no
  ledger entry.

**Not as the original, in upper halves nobody reads in the game:** where the
original pushes a value whose upper bits are stack or register garbage,
ours pushes the C++ value (section 2). Every callee masks those bits away
in its ordinary path, but two callees read their argument slots whole when
handed an index past their tables: `Menu_DrawIcon` (icon >= 21) and
`BattleWin_DrawCommandIcon` (k >= 7). Called from `0x4432F0` with a
corrupted selection, the original would read uninitialised stack in those
slots and ours reads other values - undefined against undefined, reachable
only when `0x904AB4` holds 7 or more. Said here so nobody mistakes it for a
fault in the frame emulation; it needs no ledger entry, and the coordinator
may judge otherwise.

## 5. The Chinese labels (for the localisation work)

- **The command label banner** is `BattleWin_DrawCommandLabel` `0x4439A0`:
  the chosen command k's label, from `Battle_CommandLabelPointers`
  `0x669D60 + 4 k` (pointing at `0x669D28 + 8 k` - `攻 击` for Attack),
  through `Text_DrawAt` at (box x + 8, box y + 3), 8 bytes, colour 0. This is
  the "target banner seen as `攻 击` after choosing Attack". Re-aiming its
  `Text_DrawAt` call (`0x443AF5`) or the pointer table is the patch point.
- **The turn counter's `残留` / `回合`** (`0x669D10` / `0x669D18`) are **not**
  drawn by any function here: the pointer pair `0x669D20` / `0x669D24` is
  read only by the battle effect host `0x43B130` (pe_xref, 2026-09-23), not
  in this round's groups.
- Also Chinese and drawn here, from data rather than the exe: the members'
  names (`0x802DC0 + 0x14C m`, 5 bytes) and the enemies' names
  (`0x93B9E0 + 0x128 e`, 8 bytes) through the 8 px UI font `0x516E70`, and
  the battle messages (the ring `0x93C2C4`) through `Text_DrawAt` in
  `0x597ED0`.

## 6. What the combat route reaches

The combat route (`tools/recipes/combat_ab.txt`, `analysis/combat_catalog.md`)
reaches all twenty-two, from 5 calls of `BattleMsg_Advance` (via a
`0x597A30` neighbour) and 11 of `Window_RestoreAndBack` to 14,590 of
`Window_DispatchKind`. Within them it cannot reach: every out-of-table path
of section 4 (fuzz only), `BattleObj_PickPose`'s kind-5 block unless the
fight is of that kind, and whichever cross selections, statuses and actions
the recorded fight did not use. Nothing here has been seen drawn by ours
yet; the combat A/B after the merge is the check of the whole.

## 7. Other groups' functions, as learned (not bound)

Read far enough to type the calls; each group owns its own names.

| Address | Group | Arguments and reads |
|---|---|---|
| `0x444340` | BD | (x, y, colour, value): `"%4d"` (`0x64ADD8`) of the value's low word through sprintf into `0x904BA0`, or the string `":"` (`0x64E320`) for 0xFFFF; each character but a space as a 6 x 5 SPRT at x + 5 per character, u = 6 (c - 0x30) - 0x50, v 0xD8, CLUT `Gpu_GetClut(colour << 4, 0x1E0)`; x, y their low words |
| `0x4447B0` | BD | (x, y, piece, semi): a flat POLY_F4 (`0x5A75B0`, code 0x28), its corners (x, y) plus the eight words at `0x64E148 + 16 piece`, its colour the CLUT shadow word at `0x80B788 + 64 row + 32 semi` (row the s8 `0x903A5A`), `Gpu_SetSemiTrans(p, semi)` |
| `0x444900` | BD | (x, y, size, semi): a TILE sized by the words at `0x64E268 + 4 size`, coloured as `0x4447B0` |
| `0x4449E0` | BD | (x, y, size, colour, semi): the same TILE in its 15-bit colour argument |
| `0x444A90` | BD | (x, y, width, fill, flag): two POLY_FT4 (x .. x + width, then + fill), the flag byte choosing the first's CLUT (`0xE0` or `0xB0`, row `0x1E0`) and u |
| `0x444C40` | BD | (x, y, n): a 16 x 8 SPRT at (x, y) low words, u = n << 4 (a byte), v 0xD8, CLUT 0x7800, shade 0x80 |
| `0x444CE0` / `0x444D50` / `0x444E00` | BD | (x0, y0, x1, y1, r, g, b): a LINE_F2; plain, under abr 0, under abr 1 |
| `0x444EB0` | BD | (e) -> al: 0 when a later battle slot (`0x80321E + 36 s`, its kind `0x803217 + 36 s` not 2 or 3) holds an enemy with e's +0xC byte, else 1 |
| `0x516E70` | none | (x, y, colour, count, text) -> text end: the 8 px UI font, glyphs through `0x516D50`, x and y handed on whole |
| `0x589110` | the next round | (pose, set, size): `0x589160(pose, set, size)`, then `0x589350` with +0x58 - 2 when +0x4B already holds the pose, else +0x4B = pose and `0x589350(0)` |
