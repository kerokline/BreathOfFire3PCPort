# World 4, areas 173 and 174: the band `0x428450..0x4292C0`

**Status:** IN PROGRESS (2026-09-28) - 39 functions ours
(`src/game/area_w4c.cpp`, shadow name `area_w4c`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), four `Run`s (areas
173, 174, 175 and 198): 0 mismatches in 246,000 rounds (in this worktree); 261 controls planted, 252 refused by a count, 2 by a fault, 7 equivalent (each with a refused variant) (section 4). Fuzz only: no recorded route reaches either
area (section 5). No divergence; where the original writes through an effect
slot past the 20 effect records or calls through its two-entry stack table
past its end, ours aborts with a message (section 6).

Group AR4C of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)): 39
starts, none ours before, **39 taken**; no start dropped, none added (section
7). Area 173 contributes 9, area 174 24, and **six choice bodies areas
175..185 share** lie at the band's end (`0x4291F0..0x4292BA`; the tool counts
them in area 174's block, which is where the linker put them).

Every function was read to its last instruction with capstone (2026-09-28);
each extent is the tool's (`analysis/area_funcs.tsv`) and agrees with the
reading; the clone table is `area_rows.py --unit AREA173 / AREA174 --clones`'s
rows, each read against the disassembly. What the areas *are* in the story is
not read here. The PSX twins are the sibling's `names/area_records.toml`
(area 173 descriptor `0x801F58D0`: the init and handlers 0..6; area 174
`0x801F6668`: handlers 0..21); the tail, the arrive hook, the pose helper, the
two stack states and area 175's choices have no pairing there.

## 1. The functions

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase, `kHook` a hook `(x, z)` answering in `al`,
`kState` an entry of a dispatcher's table, `kCallee` a function the area's own
code calls directly.

**Descriptors.** Area 173 `0x63FF68`: no choices; `+0x3C` = `0x63FF48`
(`Area173_Handlers`, 7: handler 0 is `0x42C8A0`, outside the band); `+0x40`
the init; the arrive hook is `Area_ArriveHook`'s case for area 173 (a `call`
at `0x56E5A9`, `event_ops.cpp`'s `kArriveHandlers[7]`); tail kind 38
(`Field_ModeTailKinds[38]`, `0x662D80`). Area 174 `0x641650`: `+0x34` =
`0x641648` (one choice, which is handler 21: the choice table is the handler
array's last entry), `+0x3C` = `0x6415F4` (`Area174_Handlers`, 22: handler 1
is `0x42D250`, outside the band); no init; its step hook is engine code
(`0x539AC0`, `Area_StepHook`'s `kStepHandlers[22]`), not in the band.

"Script back 2" is `MoveScript_Object`'s u16 `+0xA` less 2 (the op runs
again next frame); "the script" of areas 174 / 198 is
`[Area_Descriptors[Game_AreaNumber] +0x10][MoveScript_Object +3]` + the u16
`+0xA`, the running movement script's bytes. "The member index" is
`(Field_ActiveMember - Sprite_Objects) / 0xA4`, C's truncating signed
division (`imul 0x63E7063F`, `sar 6`, plus the sign bit), as a byte.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x428450` | `Area173_MessageByMemberA` | `0x8B` | 173 handler 1 (PSX `0x801F48C4`) | kHandler | over the four bytes of `Area173_MembersA`, the first some party member below `Field_MemberCount` has at `+0x89` (the member loop unsigned, unchecked): `Msg_OpenScript(Area173_MessagesA[i])`, `Field_Request` = 2 |
| `0x4284E0` | `Area173_MessageByMemberB` | `0x8B` | 173 handler 2 (PSX `0x801F4988`) | kHandler | the same over `Area173_MembersB` / `_MessagesB` (the two member lists hold the same four bytes; the messages differ) |
| `0x428570` | `Area173_PlaceObject` | `0x56` | 173 handler 3 (PSX `0x801F4A4C`) | kHandler | the running object at x `0x168000`, y `0x5000000`, z `0x7A8000` when `Field_State +0x89` is 2 else `0x7A0000`; `+8` = 1 |
| `0x4285D0` | `Area173_ScriptOnIfMember2` | `0x18` | 173 handler 4 (PSX `0x801F4A9C`); area 128 choice 3 and handler 1 | kHandler | `Field_State +0x89` at 2: the script on 1 |
| `0x4285F0` | `Area173_Effect9DSub0` | `0x67` | 173 handler 5 (PSX `0x801F4ADC`) | kHandler | `Effect_FindFree`; a slot: `+0` = 1, kind `+5` = `0x9D`, `+6` = 0, `+0xB` the member index, word `+0x2E` = `0xC`, `+0x30` = `0xA5` |
| `0x428660` | `Area173_Effect9DSub1` | `0x67` | 173 handler 6 (PSX `0x801F4BB0`) | kHandler | the same with `+6` = 1 |
| `0x4286D0` | `Area173_Tail38` | `0x10D` | tail kind 38 | kTail | section 1.1 |
| `0x4287E0` | `Area173_ArriveHook` | `0x54` | `Area_ArriveHook` (`0x56E5A9`) | kHook | zone 1 (`Cond_ByteFD`) with z exactly `0x460000` and x's high word `0x11..0x13`, or zone 2 with z `0x600000` and x's high word `0x30..0x32` (a 16-bit unsigned test): `ScriptFlags_Set40`, tail kind `0x26` at state 0, al 1; else al 0 |
| `0x428840` | `Area173_Init` | `0x22` | 173 `+0x40` (PSX `0x801F4E90`) | kInit | story flag `0x58` set (`Flags_Test`, al): tail kind `0x26` at state `0xA` |
| `0x428870` | `Area174_RestartSlotScript` | `0x31` | 174 handler 0 (PSX `0x801F3C8C`) | kHandler | `0x454A80(Sprite_Current)` (its `Field_Slots` scripts released); `0x455290(Sprite_Current` read again, `Area174_SlotScript)` (one started, the slot not read); `+0x2A` = 1; `Sprite_SetAnimation(9)` |
| `0x4288B0` | `Area174_WaitWhileRequest5` | `0x15` | 174 handler 2 (PSX `0x801F3D40`) | kHandler | `Field_Request` 5: script back 2 |
| `0x4288D0` | `Area174_Effect9DSub0` | `0x8E` | 174 handler 3 (PSX `0x801F3D78`) | kHandler | `Effect_FindFree`; a slot: kind `0x9D`, `+6` = 0, `+0xB` the member index n, word `+0x2E` = n x 24 + `0xC`, `+0x30` = `0xA5`; the running object's `+0xB` = the slot (`0xFF` for none too) |
| `0x428960` | `Area174_Effect9DSub1` | `0x74` | 174 handler 4 (PSX `0x801F3E68`) | kHandler | the same with `+6` = 1 and the object's `+0xB` not written |
| `0x4289E0` | `Area174_EffectState3` | `0x33` | 174 handler 5 (PSX `0x801F3F4C`) | kHandler | the effect in the running object's `+0xB` (unchecked): `+1` = 3, `+0x5D` = `0x70`, `+0x5E` = `0x30` |
| `0x428A20` | `Area174_SinkAndBrighten` | `0x8D` | 174 handler 6 (PSX `0x801F3FFC`) | kHandler | word `+0x3E` less `0x14`; each of `+0x5D..+0x5F` below `-0x40` (signed) raised by 2; the word at `0xA00`: `+0` bit 5 cleared, `+0x5C..+0x5F` = 0; else `Field_ActiveMember`'s word `+0x8A` less 2 |
| `0x428AB0` | `Area174_ScriptAnimationAt` | `0x5D` | 174 handler 7 (PSX `0x801F4100`); area 198 handler 1 | kHandler | `Sprite_SetAnimationAt(script[2], word +0x58 - 2)`; `+0x2A` = `script[3]` (the object read again); script on 2 |
| `0x428B10` | `Area174_ScriptAnimationOn2` | `0x6A` | 174 handler 8 (PSX `0x801F41A8`); area 198 handler 2 | kHandler | word `+0x58` at 2: `Sprite_SetAnimationAt(script[2], 0)`, `+0x2A` = `script[3]`, script on 2; else script back 2 |
| `0x428B80` | `Area174_EffectA2State0` | `0x8F` | 174 handler 9 (PSX `0x801F4278`) | kHandler | `Effect_FindFree` to the running object's `+0xB`; none: script back 2; a slot (read again from `+0xB` for each store): `+0` = 1, kind `0xA2`, dwords `+0x34` / `+0x38` the object's x / z, `+0x3C` its y + `0x1000000`, `+1` = 0 |
| `0x428C10` | `Area174_EffectA1` | `0x81` | 174 handler 10 (PSX `0x801F43FC`) | kHandler | `Effect_FindFree` to `+0xB`; none: script back 2; a slot: `+0` = 1, kind `0xA1`, `+6` = 1, `+7` = 0, `+0xB` = 1, dword `+0x4C` = the running object |
| `0x428CA0` | `Area174_StepByScript` | `0xA5` | 174 handler 11 (PSX `0x801F4594`) | kHandler | b = byte `+2` of `Field_State`'s script (dword `+0x130`) at `MoveScript_Object`'s u16 `+0xA`; the byte `0x903848` below b (unsigned): x += d `<< 11`, z -= d `<< 11` with d = `Area174_StepDeltas[+0xA & 0xF]` (s8; the index read again), `+0xA` less 1, `Field_State`'s word `+0x12E` less 2; else x and z masked to `0xFFFF8000`, script on 1 |
| `0x428D50` | `Area174_EffectA2State2` | `0x8F` | 174 handler 12 (PSX `0x801F4688`) | kHandler | as `Area174_EffectA2State0` with `+1` = 2 |
| `0x428DE0` | `Area174_TurnRightToScript` | `0x8A` | 174 handler 13 (PSX `0x801F4810`) | kHandler | the direction `+8` at `script[2]`: the script (its object as read at entry) on 2; else `+8` = (`+8` + 1) & 7, `Area174_SetPose(Area174_PoseTables[script[3]], +8)`, script back 2 |
| `0x428E70` | `Area174_TurnLeftToScript` | `0x8A` | 174 handler 14 (PSX `0x801F48EC`) | kHandler | the same turning (`+8` - 1) & 7 |
| `0x428F00` | `Area174_FaceAwayFromLeader` | `0x50` | 174 handler 15 (PSX `0x801F49C8`) | kHandler | `Area174_SetPose(Area174_PoseTables[script[2]], the leader's direction ^ 4)`; script on 1 |
| `0x428F50` | `Area174_SetPose` | `0x3E` | called by handlers 13..15 | kCallee | `(poses, direction)`: `+8` = the direction byte; i = (direction `<< 1`) & `0xFF`; `Sprite_SetAnimation(poses[i])`; the object (read again) `+0x2A` = `poses[i + 1]` (read after the call) |
| `0x428F90` | `Area174_FadeRun` | `0x26` | 174 handler 16 (PSX `0x801F4AB4`) | kHandler | `call [esp + +4 * 4]` over a two-entry table on its stack (`0x428FC0`, `0x429080`), unchecked |
| `0x428FC0` | `Area174_FadeTintUp` | `0xBA` | `Area174_FadeRun` state 0 | kState | word `+0x3E` += 8; `Field_ActiveMember`'s tint record (`MoveScript_TintRecords` by its `+0x9F`): byte `+2` below `0x1E` (signed): `+2`, `+3`, `+4` += 2 (the index read again), script back 2; else `+0` \|= `0x20`, `+0x5C` = 1, `+0x5F`, `+0x5E`, `+0x5D` = `0xC0`, state 1, script back 2 |
| `0x429080` | `Area174_FadeOut` | `0x54` | `Area174_FadeRun` state 1 | kState | word `+0x3E` += 8; `+0x5D` at `0x80`: `+0` \|= `0x40`, state 0; else `+0x5D`, `+0x5E`, `+0x5F` less 4, script back 2 |
| `0x4290E0` | `Area174_ReleaseOnRequest5` | `0x16` | 174 handler 17 (PSX `0x801F4D1C`) | kHandler | `Field_Request` 5: `0x454A80(Sprite_Current)` |
| `0x429100` | `Area174_LoadPalette` | `0x1F` | 174 handler 18 (PSX `0x801F4D54`) | kHandler | `Sprite_LoadPalette(0x80D380 + +5 x 0x40, 1)` (a row of `Gfx_ClutStripSource`) |
| `0x429120` | `Area174_EffectA3` | `0x81` | 174 handler 19 (PSX `0x801F4D90`) | kHandler | as `Area174_EffectA1` with kind `0xA3` |
| `0x4291B0` | `Area174_WaitLoad` | `0x15` | 174 handler 20 (PSX `0x801F4F28`) | kHandler | `File_LoadDone` 0 (all of eax): script back 2 |
| `0x4291D0` | `Area174_ChoiceByteE5` | `0x1C` | 174 choice 0 = handler 21 (PSX `0x801F4F6C`) | kChoice | message `0xFFFF`; the byte `0x8034E5` (beside `MoveScript_Var7`) = `0x1E` for an answer not 0, else `0xA` |
| `0x4291F0` | `Area175_ChoiceMessage62` | `0x17` | choice 4 of areas 175..185 | kChoice | message `0x62`, or `0x63` for an answer not 0 |
| `0x429210` | `Area175_ChoiceMessage70` | `0x1A` | choices 7, 8 of areas 175..185 | kChoice | answer 0: message `0xFFFF`; else `0x70` |
| `0x429230` | `Area175_ChoiceMessage7E` | `0x1A` | choices 11, 12 | kChoice | answer 0: `0xFFFF`; else `0x7E` |
| `0x429250` | `Area175_ChoiceMessage8A` | `0x1A` | choices 16, 17 | kChoice | answer 0: `0xFFFF`; else `0x8A` |
| `0x429270` | `Area175_ChoiceStore3C` | `0x14` | choice 23 | kChoice | message `0xFFFF`; the byte `0x939A3C` = the answer |
| `0x429290` | `Area175_ChoiceByte3E` | `0x2B` | choice 25 | kChoice | answer 0: message `0xFFFF`, the byte `0x939A3E` = 0; else message `0xF7`, the byte 4 |

### 1.1 The tail, kind 38

`Area173_Tail38` runs by the s8 tail state `0x9039F4`: a state above 12
unsigned (so every negative one) returns; else the byte table `0x4287D0`
(13 entries, in its extent) picks one of six entries of the jump table
`0x4287B8` (also in its extent), entry 5 a bare `ret` for states 2..9. Two
entries arm it: the arrive hook (state 0) and the init (state `0xA`, when
story flag `0x58` is set).

- **0**: `Party_DropIn(0)`, state 1.
- **1**: counter 3 (`0x90384B`, which the movement script moves) at `0x18`:
  `Field_ChangeArea(0xBA, 0x48000, zone 2 ? 0xA0000 : 0x640000, 0x80)`, story
  flag `0x58` set, kind and state 0.
- **0xA**: counter 3 at `0x20`: `Cond_ByteFE` = 1, the word timer `0x9039F6` =
  `0x1E`, state `0xB`.
- **0xB**: the timer less 1; at 0: counter 3 = `0x24`, state `0xC`.
- **0xC**: counter 3 at 0: story flag `0x58` cleared, kind and state 0.

So the flag the area leaves by is the flag its init re-arms the second half
on; what either stands for in the story is not read here.

### 1.2 The shared bodies

- **`0x4285D0`** is area 173's handler 4 and area 128's choice 3 and handler
  1 (area 128's `+0x34` is `+0x3C` less 8): one body in area 173's block.
  AR3C (area 128) left it raw; it is ours here, under area 173's name.
- **`0x428AB0` / `0x428B10`** are area 174's handlers 7 and 8 and area 198's
  handlers 1 and 2. Both read the script through `Area_Descriptors` by
  `Game_AreaNumber`, so they behave per area; the fuzz runs them under both
  (section 3). Area 198 is AR4F's; its descriptor names these two by
  address.
- **`0x4291F0..0x429290`** are six choices areas 175..185 each list in
  their `+0x34` tables (entries 4, 7..8, 11..12, 16..17, 23, 25 - the same
  bodies in eleven tables); none reads `Game_AreaNumber`. Named `Area175_*`
  after the first area that lists them.

## 2. Ours

`src/game/area_w4c.cpp`, calling out only through the harness (`AH_CALL(name)`
for every named callee, `AH_AT` for the two engine callees nobody owns,
`0x454A80` and `0x455290`). The group's own callee (`Area174_SetPose`) is
called the same way, so the fuzz stands a recorder in for it. Shapes that
repeat are one helper: the two member searches (`MessageByMember`), the four
`0x9D` effects (`Effect9D`), the two `0xA2` effects (`EffectA2`), the `0xA1` /
`0xA3` effects (`EffectOnObject`), the two turns (`TurnToScript`), the script
bytes (`ScriptBase`) and the pose tables (`PoseTable`). Kept as the
originals: every re-read of `Sprite_Current` and `MoveScript_Object` after a
call (handler 0's release and start, the animation handlers, the pose
helper's `+0x2A` and its pose byte read after `Sprite_SetAnimation`); the
turns' "there already" step through the script object read at entry; the
effect slot read again from `+0xB` before each store; `Field_ActiveMember`
read after `Effect_FindFree`; the signed byte compares (`+0x5D..+0x5F` below
`-0x40`, the tint below `0x1E`) and the unsigned ones (`0x903848` against the
script byte, the arrive hook's 16-bit window); every store's width (the word
timer, the words `+0x2E`, `+0x3E`, `+0x58`, `+0x8A`, `+0x12E`).

## 3. The fuzz

`BOF3X_SHADOW=area_w4c` (`src/game/area_w4c_fuzz.cpp`): four `Run`s under the
one shadow name, 6,000 rounds per function - area 173 (its 9), area 174 (its
24 with the pose helper and the two stack states), area 175 (the six shared
choices) and area 198 (`0x428AB0` / `0x428B10` again, over area 198's 15
scripts). The real descriptors and tables stay in place. **A `Run` that
fails stops the self-test**, so a control in a helper two `Run`s share is
listed under the first `Run` that refuses it.

- **Callees the group lists:** its own `Area174_SetPose` (the pointer whole,
  the direction a byte); the raw `0x454A80`, `0x455290` (`kByte 0xFF..0x07`);
  `Effect_FindFree` (a slot of the 20 or none, a third of the time none),
  `ScriptFlags_Set40`, `File_LoadDone` (`kFlag`: the caller tests all of eax,
  and half the flag's zeros are a whole-eax 0); standard ones listed again:
  `Sprite_SetAnimation`, `Sprite_SetAnimationAt` (the byte and the word).
- **Louder stand-ins** (half the time, from `Noise`): every callee its caller
  reads `Sprite_Current` or `MoveScript_Object` again after moves one or both
  (`Area174_SetPose`, the two raw callees, both animation setters,
  `Effect_FindFree`); `Effect_FindFree` also moves `Field_ActiveMember` (the
  `0x9D` handlers read it after the call).
- **Regions beyond the field frame:** `MoveScript_TintRecords` through the 20
  effect records as one block (`0x7E0700..0x7E1BE0`, with `Sprite_Kind2`
  between), the script object pointer, `Field_ActiveMember`, area 175's two
  bytes `0x939A3C..0x939A3F`, `Cond_ByteFE` (`0x905E20`, which the tail's state 10 sets), and the `+0x89` bytes of the three records after the party's (the member search reads them for a count past 3) - 28 regions with the harness's, 21,568 bytes.
- **Every round:** the script object at a field object, a party record or the
  running object itself; `Field_ActiveMember` at a field object, one of the
  four extra objects or a party record.
- **Seeds:** the party members' `+0x89` (the six records the member search can reach) at the lists' bytes or beside them, `Field_MemberCount` 0..5 a third of the time; `Field_State +0x89` at 2 and beside; `Field_ActiveMember` for the `0x9D` effects at any address half the time (a difference of either sign from `Sprite_Objects`, it is not read through); every tail state 0, 1, `0xA..0xC` and the empty or out-of-range ones (2, 5, 9, `0xD`, `0x7F`, `0x80`, `0xF4`, `0xFF`) with counter 3 at the value the state waits on two times in three and beside it otherwise (the empty states given `0x18`, `0x20` or 0), `Cond_ByteFD` 2 or not for state 1, the timer at 1, 0, 2, `0x101`, `0x8001`, `0xFFFF`; the arrive hook's zone 1, 2 or other, z exact, the other zone's, one off, a high-word off or any, x's high word at each end of its window and one past it (a low word or none); `Field_Request` 5 and beside; the effect slot in `+0xB` inside the 20; the sink's word at `0xA14` and beside and its three bytes at the signed edges (`0xBF`, `0xC0`, `0x80`, `0x7F`, ...); the script object's `+3` inside the running area's script table (37 for 174, 15 for 198) and its `+0xA` small half the time; the word `+0x58` at 2 and beside; `Field_State +0x130` pointed into the area block with the byte it reads planted and the limit byte `0x903848` one below, at, one above, or far; the turns' direction at the script's byte, one off either way or any; the pose helper's table one of the three real ones two rounds in three (else the area block) and any direction; the fade state 0 or 1, the tint index under 32 half the time and its byte at `0x1E` and beside; `+0x5D` at `0x80` and beside; the choice answers 0, 1, 2, `0xFF`, `0x80`, `0x7F`, `0x10`.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3, the script object, `Field_ActiveMember`, the word timer,
  `Cond_ByteFD`.

**Result (in this worktree):** area 173 54,000 rounds, 27,185 calls to the stand-ins; area 174 144,000 rounds, 102,801 calls; area 175 36,000 rounds (the choices call nothing); area 198 12,000 rounds, 7,351 calls - 246,000 rounds, 0 mismatches, 21,568 bytes of state (28 regions) and the log compared. Coverage: every callee each function can reach was called - `Party_DropIn` 328, `Field_ChangeArea` / `Flags_Set` 450, `Flags_Clear` 467, `ScriptFlags_Set40` 373 (the arrive hook's matches), `Msg_OpenScript` 7,117, `Area174_SetPose` 15,367, `0x454A80` 8,038, `0x455290` 6,000, both stack states (`0x428FC0` 3,003, `0x429080` 2,997).

`BOF3X_SHADOW='*'`: exit 0, `inject: 5397 ours`, 491 self-test lines, no mismatch (in this worktree, first run; no silent death).

## 4. Controls

Planted one at a time in `area_w4c.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w4c.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w4c`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches). **261 planted (256, then five near variants), 254 refused (exit 3 or a fault) - 252 by a count, 2 by a fault (each with a variant refused by a count) - and 7 equivalent**, each with a near variant or a neighbouring control refused by a count. No hang. Five of the seven were planted as equivalence checks (C32, C71, C79, C127, C197); C19 and C167 are equivalent by the shipped data. Every one of the 39 functions has at least one control of its own; a control in a shared helper lists every function it refused in, within the first `Run` that refused it (section 3).

A first pass left three standing that were the fuzz's fault and were refused after it was strengthened: `Cond_ByteFE` (`0x905E20`, which the tail's state 10 sets) was in no compared region (C50); the `+0x89` bytes of the records after the party's, which the member search reads for a `Field_MemberCount` past 3, were outside the regions, so their constant bytes never matched a member byte (C17: three one-byte regions now, seeded with the lists' bytes); the tail's empty states were never given the counter value a live state waits on, so "state 13 runs state 12" never showed (C63).

The thinnest (under the final fuzz where re-planted): C4 18 (the member index's rounding of a negative difference), C123 35 (`Area174_SinkAndBrighten`'s member word as a byte), C56 36 (`Area173_Tail38` state 11 done at 1), C67 37 (the arrive hook's z by its high word), C70 42 (its window tested signed), C59 49, C62 53 (the tail's state 9 run as 10: 1 round on the first pass, before the empty states were given the live states' counter values).

Counts are in this worktree; the second and later passes ran on the strengthened fuzz, the first-pass counts on the fuzz before it (regions and seeds only added since, so each first-pass refusal stands).

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| C1 | `EffectAt (every effect handler)` | effect stride 0x7C | Area173_Effect9DSub0 3870, Area173_Effect9DSub1 3757 |
| C2 | `ActiveMemberIndex (0x9D handlers)` | the index divided unsigned | Area173_Effect9DSub0 461, Area173_Effect9DSub1 480 |
| C3 | `ActiveMemberIndex (0x9D handlers)` | the index one on | Area173_Effect9DSub0 4074, Area173_Effect9DSub1 3970 |
| C4 | `ActiveMemberIndex (0x9D handlers)` | the division as >> 2 then / 41 (rounds differently for negatives) | Area173_Effect9DSub0 5, Area173_Effect9DSub1 11 on the first pass; Area173_Effect9DSub0 11, Area173_Effect9DSub1 7 under the final fuzz |
| C5 | `Effect9D (all four)` | +0 = 2 | Area173_Effect9DSub0 4074, Area173_Effect9DSub1 3970 |
| C6 | `Effect9D (all four)` | kind 0x9C | Area173_Effect9DSub0 4074, Area173_Effect9DSub1 3970 |
| C7 | `Effect9D (all four)` | the index to +0xC | Area173_Effect9DSub0 4074, Area173_Effect9DSub1 3969 |
| C8 | `Effect9D (all four)` | word +0x30 0xA4 | Area173_Effect9DSub0 4074, Area173_Effect9DSub1 3970 |
| C9 | `Effect9D (all four)` | word +0x2E stored as a byte | Area173_Effect9DSub0 4056, Area173_Effect9DSub1 3947 |
| C10 | `MessageByMember (A and B)` | three pairs searched | Area173_MessageByMemberA 655, Area173_MessageByMemberB 677 |
| C11 | `MessageByMember (A and B)` | one member more | Area173_MessageByMemberA 819, Area173_MessageByMemberB 791 |
| C12 | `MessageByMember (A and B)` | member 0 skipped | Area173_MessageByMemberA 1683, Area173_MessageByMemberB 1640 |
| C13 | `MessageByMember (A and B)` | Field_Request 3 | Area173_MessageByMemberA 3461, Area173_MessageByMemberB 3439 |
| C14 | `MessageByMember (A and B)` | the search goes on after a match | Area173_MessageByMemberA 801, Area173_MessageByMemberB 778 |
| C15 | `MessageByMember (A and B)` | message words indexed by bytes | Area173_MessageByMemberA 2359, Area173_MessageByMemberB 2369 |
| C16 | `MessageByMember (A and B)` | member byte +0x8A | Area173_MessageByMemberA 3492, Area173_MessageByMemberB 3472 |
| C17 | `MessageByMember (A and B)` | the count clamped to 3 | not refused on the first pass (the fuzz's fault, below); after it: Area173_MessageByMemberA 152, Area173_MessageByMemberB 154 |
| C18 | `Area173_MessageByMemberA` | A opens B's messages | Area173_MessageByMemberA 3461 |
| C19 | `Area173_MessageByMemberB` | B searches A's members (the same bytes) | equivalent: `Area173_MembersA` and `_MembersB` hold the same four bytes; V1 (B's list one byte on) refused |
| C20 | `Area173_MessageByMemberB` | B opens A's messages | Area173_MessageByMemberB 3439 |
| C21 | `Area173_PlaceObject` | x 0x168001 | Area173_PlaceObject 6000 |
| C22 | `Area173_PlaceObject` | y 0x5000001 | Area173_PlaceObject 6000 |
| C23 | `Area173_PlaceObject` | the test at 3 | Area173_PlaceObject 3030 |
| C24 | `Area173_PlaceObject` | z else 0x7A0001 | Area173_PlaceObject 3970 |
| C25 | `Area173_PlaceObject` | z then 0x7A8001 | Area173_PlaceObject 2030 |
| C26 | `Area173_PlaceObject` | direction 2 | Area173_PlaceObject 6000 |
| C27 | `Area173_ScriptOnIfMember2` | the test inverted | Area173_ScriptOnIfMember2 6000 |
| C28 | `Area173_ScriptOnIfMember2` | on 2 | Area173_ScriptOnIfMember2 2035 |
| C29 | `Area173_ScriptOnIfMember2` | the running object's byte | Area173_ScriptOnIfMember2 1721 |
| C30 | `Area173_Effect9DSub0` | +6 = 1 | Area173_Effect9DSub0 4074 |
| C31 | `Area173_Effect9DSub0` | word +0x2E 0xD | Area173_Effect9DSub0 4074 |
| C32 | `Area173_Effect9DSub0` | none also 0xFE (equivalent-check: the stand-in never answers 0xFE) | equivalent: `Effect_FindFree` answers a slot 0..19 or `0xFF`, never `0xFE`; V5 (a slot of 0 taken as none) refused |
| C33 | `Area173_Effect9DSub1` | +6 = 0 | Area173_Effect9DSub1 3970 |
| C34 | `Area173_Effect9DSub1` | word +0x2E 0x18 | Area173_Effect9DSub1 3970 |
| C35 | `Area173_Effect9DSub1` | +0xB 0 | Area173_Effect9DSub1 3921 |
| C36 | `Area173_Tail38` | Party_DropIn(1) | Area173_Tail38 375 |
| C37 | `Area173_Tail38` | state 0 -> 2 | Area173_Tail38 375 |
| C38 | `Area173_Tail38` | state 1 waits for 0x19 | Area173_Tail38 518 |
| C39 | `Area173_Tail38` | state 1 at 0x18 or more | Area173_Tail38 121 |
| C40 | `Area173_Tail38` | area 0xBB | Area173_Tail38 460 |
| C41 | `Area173_Tail38` | x 0x48001 | Area173_Tail38 460 |
| C42 | `Area173_Tail38` | zone 1 picks the z | Area173_Tail38 245 |
| C43 | `Area173_Tail38` | z 0xA0001 | Area173_Tail38 156 |
| C44 | `Area173_Tail38` | z 0x640001 | Area173_Tail38 304 |
| C45 | `Area173_Tail38` | flags 0x81 | Area173_Tail38 460 |
| C46 | `Area173_Tail38` | flag 0x59 set | Area173_Tail38 460 |
| C47 | `Area173_Tail38` | state 1 keeps the kind | Area173_Tail38 458 |
| C48 | `Area173_Tail38` | state 1 leaves state 1 | Area173_Tail38 460 |
| C49 | `Area173_Tail38` | state 10 waits for 0x21 | Area173_Tail38 545 |
| C50 | `Area173_Tail38` | Cond_ByteFE 2 | not refused on the first pass (the fuzz's fault, below); after it: Area173_Tail38 467 |
| C51 | `Area173_Tail38` | timer 0x1F | Area173_Tail38 489 |
| C52 | `Area173_Tail38` | timer stored as a byte | Area173_Tail38 488 |
| C53 | `Area173_Tail38` | state 10 -> 12 | Area173_Tail38 489 |
| C54 | `Area173_Tail38` | timer less 2 | Area173_Tail38 705 |
| C55 | `Area173_Tail38` | timer counted as a byte | Area173_Tail38 184 |
| C56 | `Area173_Tail38` | state 11 done at 1 too | Area173_Tail38 36 |
| C57 | `Area173_Tail38` | counter 3 = 0x25 | Area173_Tail38 485 |
| C58 | `Area173_Tail38` | state 11 stays | Area173_Tail38 485 |
| C59 | `Area173_Tail38` | state 12 at 1 too | Area173_Tail38 49 |
| C60 | `Area173_Tail38` | flag 0x57 cleared | Area173_Tail38 509 |
| C61 | `Area173_Tail38` | state 12 keeps its state | Area173_Tail38 509 |
| C62 | `Area173_Tail38` | state 9 runs state 10 | Area173_Tail38 1 on the first pass; Area173_Tail38 53 under the final fuzz |
| C63 | `Area173_Tail38` | state 13 runs state 12 | not refused on the first pass (the fuzz's fault, below); after it: Area173_Tail38 128 |
| C64 | `Area173_Tail38` | the state masked to 7 bits (0x80 | 1 runs 1) | Area173_Tail38 356 |
| C65 | `Area173_ArriveHook` | zone 3 for 1 | Area173_ArriveHook 262 |
| C66 | `Area173_ArriveHook` | z 0x460001 | Area173_ArriveHook 189 |
| C67 | `Area173_ArriveHook` | z high word only | Area173_ArriveHook 37 |
| C68 | `Area173_ArriveHook` | x from 0x10 | Area173_ArriveHook 89 |
| C69 | `Area173_ArriveHook` | x through 0x14 | Area173_ArriveHook 54 |
| C70 | `Area173_ArriveHook` | a signed test (below 0x11 passes) | Area173_ArriveHook 42 |
| C71 | `Area173_ArriveHook` | zone 2 test as written (equivalent check: unreachable for 1) | equivalent: zone 1 has taken the first branch already; C72 (zone 3 for 2) refused |
| C72 | `Area173_ArriveHook` | zone 3 for 2 | Area173_ArriveHook 179 |
| C73 | `Area173_ArriveHook` | z 0x600001 | Area173_ArriveHook 196 |
| C74 | `Area173_ArriveHook` | x from 0x31 | Area173_ArriveHook 98 |
| C75 | `Area173_ArriveHook` | x to 0x31 | Area173_ArriveHook 56 |
| C76 | `Area173_ArriveHook` | x >> 15 | Area173_ArriveHook 350 |
| C77 | `Area173_ArriveHook` | kind 0x27 | Area173_ArriveHook 350 |
| C78 | `Area173_ArriveHook` | state 1 | Area173_ArriveHook 350 |
| C79 | `Area173_ArriveHook` | answer 0x101 (equivalent check: al 1) | equivalent: the arrive hook's answer is read in al (`Area_ArriveHook`, the harness's `ret_mask 0xFF`); C80 (answer 2) refused |
| C80 | `Area173_ArriveHook` | answer 2 | Area173_ArriveHook 350 |
| C81 | `Area173_ArriveHook` | ScriptFlags_Set40 not called | Area173_ArriveHook 350 |
| C82 | `Area173_Init` | the test inverted | Area173_Init 6000 |
| C83 | `Area173_Init` | flag 0x59 | Area173_Init 6000 |
| C84 | `Area173_Init` | kind 0x25 | Area173_Init 3991 |
| C85 | `Area173_Init` | state 0xB | Area173_Init 3991 |
| C86 | `Area174_RestartSlotScript` | +0x2A = 2 | Area174_RestartSlotScript 6000 |
| C87 | `Area174_RestartSlotScript` | animation 8 | Area174_RestartSlotScript 6000 |
| C88 | `Area174_RestartSlotScript` | the script one byte on | Area174_RestartSlotScript 6000 |
| C89 | `Area174_RestartSlotScript` | released for Field_State | Area174_RestartSlotScript 5034 |
| C90 | `Area174_RestartSlotScript` | +0x2A after the animation call | Area174_RestartSlotScript 2675 |
| C91 | `Area174_RestartSlotScript` | the object not read again after the release | Area174_RestartSlotScript 2666 |
| C92 | `Area174_WaitWhileRequest5` | request 4 | Area174_WaitWhileRequest5 3046 |
| C93 | `Area174_WaitWhileRequest5` | back 1 | Area174_WaitWhileRequest5 2047 |
| C94 | `Area174_WaitWhileRequest5` | request 5 or more | Area174_WaitWhileRequest5 1969 |
| C95 | `Area174_Effect9DSub0` | +6 = 1 | Area174_Effect9DSub0 3983 |
| C96 | `Area174_Effect9DSub0` | n * 25 | Area174_Effect9DSub0 3914 |
| C97 | `Area174_Effect9DSub0` | + 0xD | Area174_Effect9DSub0 3983 |
| C98 | `Area174_Effect9DSub0` | none not stored in +0xB | Area174_Effect9DSub0 2007 |
| C99 | `Area174_Effect9DSub0` | the slot to +0xC | Area174_Effect9DSub0 6000 |
| C100 | `Area174_Effect9DSub0` | n + 1 | Area174_Effect9DSub0 3983 |
| C101 | `Area174_Effect9DSub1` | +6 = 0 | Area174_Effect9DSub1 4042 |
| C102 | `Area174_Effect9DSub1` | + 0xB | Area174_Effect9DSub1 4042 |
| C103 | `Area174_Effect9DSub1` | n masked to 7 bits | Area174_Effect9DSub1 1515 |
| C104 | `Area174_Effect9DSub1` | +0xB written too | Area174_Effect9DSub1 4030 |
| C105 | `Area174_EffectState3` | +1 = 4 | Area174_EffectState3 6000 |
| C106 | `Area174_EffectState3` | +0x5D 0x71 | Area174_EffectState3 6000 |
| C107 | `Area174_EffectState3` | +0x5E 0x31 | Area174_EffectState3 6000 |
| C108 | `Area174_EffectState3` | +0x5C for +0x5D | Area174_EffectState3 6000 |
| C109 | `Area174_EffectState3` | the slot from +0xA | Area174_EffectState3 5675 |
| C110 | `Area174_SinkAndBrighten` | y less 0x13 | Area174_SinkAndBrighten 6000 |
| C111 | `Area174_SinkAndBrighten` | at -0x40 too | Area174_SinkAndBrighten 956 |
| C112 | `Area174_SinkAndBrighten` | an unsigned test | Area174_SinkAndBrighten 3578 |
| C113 | `Area174_SinkAndBrighten` | +1 | Area174_SinkAndBrighten 3084 |
| C114 | `Area174_SinkAndBrighten` | +0x5F not raised | Area174_SinkAndBrighten 1424 |
| C115 | `Area174_SinkAndBrighten` | +0x5D not raised | Area174_SinkAndBrighten 1451 |
| C116 | `Area174_SinkAndBrighten` | done at 0xA01 | Area174_SinkAndBrighten 1993 |
| C117 | `Area174_SinkAndBrighten` | done at 0xA00 or below | Area174_SinkAndBrighten 2094 |
| C118 | `Area174_SinkAndBrighten` | bit 4 cleared | Area174_SinkAndBrighten 1008 |
| C119 | `Area174_SinkAndBrighten` | +0x5C = 1 | Area174_SinkAndBrighten 1364 |
| C120 | `Area174_SinkAndBrighten` | +0x5D kept | Area174_SinkAndBrighten 1266 |
| C121 | `Area174_SinkAndBrighten` | the member word less 3 | Area174_SinkAndBrighten 4636 |
| C122 | `Area174_SinkAndBrighten` | the running object's word | Area174_SinkAndBrighten 4363 |
| C123 | `Area174_SinkAndBrighten` | the member word as a byte | Area174_SinkAndBrighten 34 on the first pass; Area174_SinkAndBrighten 35 under the final fuzz |
| C124 | `ScriptBase (7, 8, 13..15)` | the script by +2 | by a fault (the script object's unseeded `+2` byte indexes past the 37 scripts, and both sides read through a dword that is not a pointer); V3 refused by a count |
| C125 | `ScriptBase (7, 8, 13..15)` | the descriptor's +0x14 | Area174_ScriptAnimationAt 5836, Area174_ScriptAnimationOn2 1260, Area174_TurnRightToScript 5256, Area174_TurnLeftToScript 5287, Area174_FaceAwayFromLeader 5278 |
| C126 | `ScriptBase (7, 8, 13..15)` | area 174 always (the area 198 Run tells) | Area174_ScriptAnimationAt 5928, Area174_ScriptAnimationOn2 1300 |
| C127 | `ScriptBase (7, 8, 13..15)` | the area number as a byte (equivalent check: seeded areas are below 0x100) | equivalent: `Game_AreaNumber` is an area below 200 when an area handler runs; C126 (area 174 always) refused by the area 198 `Run` |
| C128 | `Area174_ScriptAnimationAt` | pose byte +1 | Area174_ScriptAnimationAt 5286 |
| C129 | `Area174_ScriptAnimationAt` | animation byte +3 | Area174_ScriptAnimationAt 5187 |
| C130 | `Area174_ScriptAnimationAt` | start + 0x58 less 1 | Area174_ScriptAnimationAt 6000 |
| C131 | `Area174_ScriptAnimationAt` | start from +0x5A | Area174_ScriptAnimationAt 6000 |
| C132 | `Area174_ScriptAnimationAt` | on 3 | Area174_ScriptAnimationAt 6000 |
| C133 | `Area174_ScriptAnimationAt` | the object not read again after the call | Area174_ScriptAnimationAt 2735 |
| C134 | `Area174_ScriptAnimationAt` | the offset as a byte | Area174_ScriptAnimationAt 3008 |
| C135 | `Area174_ScriptAnimationOn2` | at 3 | Area174_ScriptAnimationOn2 2026 |
| C136 | `Area174_ScriptAnimationOn2` | the word as a byte | Area174_ScriptAnimationOn2 696 |
| C137 | `Area174_ScriptAnimationOn2` | back 1 | Area174_ScriptAnimationOn2 4705 |
| C138 | `Area174_ScriptAnimationOn2` | start 1 | Area174_ScriptAnimationOn2 1295 |
| C139 | `Area174_ScriptAnimationOn2` | animation byte +1 | Area174_ScriptAnimationOn2 1119 |
| C140 | `Area174_ScriptAnimationOn2` | pose byte +4 | Area174_ScriptAnimationOn2 1099 |
| C141 | `Area174_ScriptAnimationOn2` | on 4 | Area174_ScriptAnimationOn2 1295 |
| C142 | `EffectA2 (9, 12)` | +0 = 3 | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C143 | `EffectA2 (9, 12)` | kind 0xA3 | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C144 | `EffectA2 (9, 12)` | x from z | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C145 | `EffectA2 (9, 12)` | z + 1 | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C146 | `EffectA2 (9, 12)` | y + 0x1000001 | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C147 | `EffectA2 (9, 12)` | y + 0x100000 | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C148 | `EffectA2 (9, 12)` | the state to +2 | Area174_EffectA2State0 4001, Area174_EffectA2State2 4026 |
| C149 | `EffectA2 (9, 12)` | none: back 3 | Area174_EffectA2State0 1999, Area174_EffectA2State2 1974 |
| C150 | `EffectA2 (9, 12)` | the object read before the call | Area174_EffectA2State0 2662, Area174_EffectA2State2 2700 |
| C151 | `Area174_EffectA2State0` | state 1 | Area174_EffectA2State0 4001 |
| C152 | `Area174_EffectA2State2` | state 0 | Area174_EffectA2State2 4026 |
| C153 | `EffectOnObject (10, 19)` | +6 = 2 | Area174_EffectA1 3990, Area174_EffectA3 4040 |
| C154 | `EffectOnObject (10, 19)` | +7 = 1 | Area174_EffectA1 3990, Area174_EffectA3 4040 |
| C155 | `EffectOnObject (10, 19)` | +0xB = 0 | Area174_EffectA1 3990, Area174_EffectA3 4040 |
| C156 | `EffectOnObject (10, 19)` | +0x4C Field_State | Area174_EffectA1 3312, Area174_EffectA3 3357 |
| C157 | `EffectOnObject (10, 19)` | the object to +0x48 | Area174_EffectA1 3990, Area174_EffectA3 4040 |
| C158 | `EffectOnObject (10, 19)` | the kind to +4 | Area174_EffectA1 3990, Area174_EffectA3 4040 |
| C159 | `EffectOnObject (10, 19)` | none: the script not moved | Area174_EffectA1 2010, Area174_EffectA3 1960 |
| C160 | `Area174_EffectA1` | kind 0xA0 | Area174_EffectA1 3990 |
| C161 | `Area174_EffectA3` | kind 0xA1 | Area174_EffectA3 4040 |
| C162 | `Area174_StepByScript` | at the limit too | Area174_StepByScript 666 |
| C163 | `Area174_StepByScript` | a signed test | Area174_StepByScript 2686 |
| C164 | `Area174_StepByScript` | x step << 10 | Area174_StepByScript 675 |
| C165 | `Area174_StepByScript` | x step zero-extended | Area174_StepByScript 317 |
| C166 | `Area174_StepByScript` | z step not negated | Area174_StepByScript 675 |
| C167 | `Area174_StepByScript` | x index & 7 | equivalent: `Area174_StepDeltas` repeats with period 4, so `& 7` reads what `& 0xF` reads; V2 (`& 0xE`) refused |
| C168 | `Area174_StepByScript` | z index one on | Area174_StepByScript 1378 |
| C169 | `Area174_StepByScript` | +0xA less 2 | Area174_StepByScript 1378 |
| C170 | `Area174_StepByScript` | Field_State +0x12E less 1 | Area174_StepByScript 1378 |
| C171 | `Area174_StepByScript` | the script object back, not Field_State | Area174_StepByScript 1378 |
| C172 | `Area174_StepByScript` | the script at +0x12C | by a fault (`Field_State +0x12C` is not the seeded pointer; both sides fault); V4 refused by a count |
| C173 | `Area174_StepByScript` | byte +1 | Area174_StepByScript 1296 |
| C174 | `Area174_StepByScript` | the limit from 0x903849 | Area174_StepByScript 1329 |
| C175 | `Area174_StepByScript` | x masked to 0xFFFF0000 | Area174_StepByScript 2336 |
| C176 | `Area174_StepByScript` | z masked to 0xFFFFC000 | Area174_StepByScript 2297 |
| C177 | `Area174_StepByScript` | on 2 | Area174_StepByScript 4622 |
| C178 | `TurnToScript (13, 14)` | compared with byte +3 | Area174_TurnRightToScript 1155, Area174_TurnLeftToScript 1210 |
| C179 | `TurnToScript (13, 14)` | there: on 1 | Area174_TurnRightToScript 1280, Area174_TurnLeftToScript 1338 |
| C180 | `TurnToScript (13, 14)` | the turn & 0xF | Area174_TurnRightToScript 2318, Area174_TurnLeftToScript 2509 |
| C181 | `TurnToScript (13, 14)` | the pose table by byte +2 | Area174_TurnRightToScript 4028, Area174_TurnLeftToScript 3903 |
| C182 | `TurnToScript (13, 14)` | back 1 | Area174_TurnRightToScript 4720, Area174_TurnLeftToScript 4662 |
| C183 | `TurnToScript (13, 14)` | the pose direction one on | Area174_TurnRightToScript 4720, Area174_TurnLeftToScript 4662 |
| C184 | `PoseTable (13..15)` | the pose table one on | Area174_TurnRightToScript 4485, Area174_TurnLeftToScript 4431, Area174_FaceAwayFromLeader 5650 |
| C185 | `Area174_TurnRightToScript` | turns left | Area174_TurnRightToScript 4720 |
| C186 | `Area174_TurnLeftToScript` | turns two back | Area174_TurnLeftToScript 4662 |
| C187 | `Area174_FaceAwayFromLeader` | ^ 2 | Area174_FaceAwayFromLeader 6000 |
| C188 | `Area174_FaceAwayFromLeader` | the leader's +9 | Area174_FaceAwayFromLeader 5981 |
| C189 | `Area174_FaceAwayFromLeader` | the pose table by byte +3 | Area174_FaceAwayFromLeader 5108 |
| C190 | `Area174_FaceAwayFromLeader` | on 2 | Area174_FaceAwayFromLeader 6000 |
| C191 | `Area174_FaceAwayFromLeader` | the script object not read again after the call | Area174_FaceAwayFromLeader 2597 |
| C192 | `Area174_SetPose` | direction + 1 | Area174_SetPose 5969 |
| C193 | `Area174_SetPose` | d << 2 | Area174_SetPose 5554 |
| C194 | `Area174_SetPose` | the index not wrapped to a byte | Area174_SetPose 1442 |
| C195 | `Area174_SetPose` | +0x2A from pose[2] | Area174_SetPose 5316 |
| C196 | `Area174_SetPose` | animation pose[1] | Area174_SetPose 5480 |
| C197 | `Area174_SetPose` | kept pointer for +8 (equivalent check: no call before) | equivalent: no call lies between the read of `Sprite_Current` and the `+8` store; C198 (the object not read again after the call) refused |
| C198 | `Area174_SetPose` | the object not read again after the call | Area174_SetPose 2682 |
| C199 | `Area174_FadeRun` | the two states swapped | Area174_FadeRun 6000 |
| C200 | `Area174_FadeRun` | the state from +5 | Area174_FadeRun 3078 |
| C201 | `Area174_FadeRun` | state 0 always | Area174_FadeRun 2926 |
| C202 | `Area174_FadeTintUp` | y + 9 | Area174_FadeTintUp 6000 |
| C203 | `Area174_FadeTintUp` | at 0x1E too | Area174_FadeTintUp 522 |
| C204 | `Area174_FadeTintUp` | an unsigned test | Area174_FadeTintUp 1995 |
| C205 | `Area174_FadeTintUp` | +2 by 3 | Area174_FadeTintUp 3697 |
| C206 | `Area174_FadeTintUp` | +3 by 1 | Area174_FadeTintUp 3697 |
| C207 | `Area174_FadeTintUp` | +5 for +4 | Area174_FadeTintUp 3697 |
| C208 | `Area174_FadeTintUp` | stride 11 | Area174_FadeTintUp 5020 |
| C209 | `Area174_FadeTintUp` | the index from +0x9E | Area174_FadeTintUp 5068 |
| C210 | `Area174_FadeTintUp` | back 1 while it tints | Area174_FadeTintUp 3697 |
| C211 | `Area174_FadeTintUp` | bit 4 | Area174_FadeTintUp 1718 |
| C212 | `Area174_FadeTintUp` | +0x5C = 2 | Area174_FadeTintUp 2303 |
| C213 | `Area174_FadeTintUp` | +0x5F = 0xC1 | Area174_FadeTintUp 2303 |
| C214 | `Area174_FadeTintUp` | state 2 | Area174_FadeTintUp 2303 |
| C215 | `Area174_FadeTintUp` | the last step not moved back | Area174_FadeTintUp 2303 |
| C216 | `Area174_FadeOut` | y + 7 | Area174_FadeOut 6000 |
| C217 | `Area174_FadeOut` | done at 0x84 | Area174_FadeOut 1667 |
| C218 | `Area174_FadeOut` | done at 0x80 or below | Area174_FadeOut 2589 |
| C219 | `Area174_FadeOut` | bit 5 | Area174_FadeOut 622 |
| C220 | `Area174_FadeOut` | state 1 kept | Area174_FadeOut 819 |
| C221 | `Area174_FadeOut` | +0x5D less 3 | Area174_FadeOut 5181 |
| C222 | `Area174_FadeOut` | +0x5E less 3 | Area174_FadeOut 5181 |
| C223 | `Area174_FadeOut` | +0x5F from +0x5E | Area174_FadeOut 5158 |
| C224 | `Area174_FadeOut` | the script not moved | Area174_FadeOut 5181 |
| C225 | `Area174_ReleaseOnRequest5` | the test inverted | Area174_ReleaseOnRequest5 6000 |
| C226 | `Area174_ReleaseOnRequest5` | released for Field_State | Area174_ReleaseOnRequest5 1685 |
| C227 | `Area174_ReleaseOnRequest5` | request 6 | Area174_ReleaseOnRequest5 3010 |
| C228 | `Area174_LoadPalette` | rows of 0x20 | Area174_LoadPalette 5981 |
| C229 | `Area174_LoadPalette` | the row from +6 | Area174_LoadPalette 5972 |
| C230 | `Area174_LoadPalette` | palette 2 | Area174_LoadPalette 6000 |
| C231 | `Area174_LoadPalette` | one row on | Area174_LoadPalette 6000 |
| C232 | `Area174_WaitLoad` | the test inverted | Area174_WaitLoad 6000 |
| C233 | `Area174_WaitLoad` | al tested, not eax | Area174_WaitLoad 1006 |
| C234 | `Area174_WaitLoad` | back 1 | Area174_WaitLoad 999 |
| C235 | `Area174_ChoiceByteE5` | 0x1F | Area174_ChoiceByteE5 4500 |
| C236 | `Area174_ChoiceByteE5` | 0xB | Area174_ChoiceByteE5 1500 |
| C237 | `Area174_ChoiceByteE5` | answer 1 as 0 | Area174_ChoiceByteE5 760 |
| C238 | `Area174_ChoiceByteE5` | to 0x8034E4 | Area174_ChoiceByteE5 5999 |
| C239 | `Area174_ChoiceByteE5` | message 0xFFFE | Area174_ChoiceByteE5 6000 |
| C240 | `Area175_ChoiceMessage62` | swapped | Area175_ChoiceMessage62 6000 |
| C241 | `Area175_ChoiceMessage62` | answer 1 as 0 | Area175_ChoiceMessage62 747 |
| C242 | `Area175_ChoiceMessage62` | negative answers as 0 | Area175_ChoiceMessage62 1547 |
| C243 | `Area175_ChoiceMessage70` | 0x71 | Area175_ChoiceMessage70 4541 |
| C244 | `Area175_ChoiceMessage70` | none 0xFF | Area175_ChoiceMessage70 1459 |
| C245 | `Area175_ChoiceMessage70` | only answer 1 | Area175_ChoiceMessage70 3830 |
| C246 | `Area175_ChoiceMessage7E` | 0x7F | Area175_ChoiceMessage7E 4448 |
| C247 | `Area175_ChoiceMessage7E` | swapped | Area175_ChoiceMessage7E 6000 |
| C248 | `Area175_ChoiceMessage8A` | 0x8B | Area175_ChoiceMessage8A 4488 |
| C249 | `Area175_ChoiceMessage8A` | answer & 0x7F | Area175_ChoiceMessage8A 800 |
| C250 | `Area175_ChoiceStore3C` | answer + 1 | Area175_ChoiceStore3C 6000 |
| C251 | `Area175_ChoiceStore3C` | to 0x939A3D | Area175_ChoiceStore3C 6000 |
| C252 | `Area175_ChoiceStore3C` | the message not written | Area175_ChoiceStore3C 2048 |
| C253 | `Area175_ChoiceByte3E` | answer 1 | Area175_ChoiceByte3E 2196 |
| C254 | `Area175_ChoiceByte3E` | the byte 1 | Area175_ChoiceByte3E 1446 |
| C255 | `Area175_ChoiceByte3E` | message 0xF8 | Area175_ChoiceByte3E 4554 |
| C256 | `Area175_ChoiceByte3E` | the byte 5 | Area175_ChoiceByte3E 4554 |
| V1 | `Area173_MessageByMemberB` | B searches its members one on (variant of C19) | Area173_MessageByMemberB 3352 |
| V2 | `Area174_StepByScript` | x index & 0xE (variant of C167) | Area174_StepByScript 660 |
| V3 | `ScriptBase (7, 8, 13..15)` | the script by +3 / 2 (variant of C124, inside the table) | Area174_ScriptAnimationAt 5602, Area174_ScriptAnimationOn2 1312, Area174_TurnRightToScript 5094, Area174_TurnLeftToScript 5059, Area174_FaceAwayFromLeader 5077 |
| V4 | `Area174_StepByScript` | the script pointer + 1 (variant of C172) | Area174_StepByScript 1288 |
| V5 | `Area173_Effect9DSub0` | slot 0 taken as none (variant of C32) | Area173_Effect9DSub0 202 |

## 5. What nothing reached

No recorded route enters area 173 or 174 (`area_rows.tsv`'s route count 0
for both, and none of the band in `area_funcs.tsv`'s live column), so every
function here is proven by the fuzz alone. Area 175's choices and area 198's
two handlers are fuzz-only for the same reason. The route A/Bs at the
round's end are the check that the band behaves in play.

## 6. Latent defects (described, not fixed)

- **`Area174_EffectState3` writes through the running object's `+0xB`
  unchecked.** `Area174_Effect9DSub0`, `_EffectA2State0` / `_State2`,
  `_EffectA1` and `_EffectA3` store `Effect_FindFree`'s "none", `0xFF`, in
  `+0xB`; handler 5 run after such a none writes `0x7E11E0 + 0xFF x 0x80`
  `+1`, `+0x5D`, `+0x5E` - past the 20 effect records (they end at
  `0x7E1BE0`). Ours aborts with a message there; which script orders the
  handlers is not read, so whether a full effect pool can reach it in play is
  not known.
- **`Area174_FadeRun` indexes a two-entry table on its own stack by the
  running object's `+4`, unchecked**: a state of 2 or more calls through its
  saved registers or return address. Its states only write 0 and 1; ours
  aborts past 1.
- **`Area173_MessageByMemberA` / `B` walk `Field_MemberCount` records
  unchecked**: a count past 3 compares bytes past the party records (reads
  only; kept).
- **`Area174_TurnRightToScript` / `_TurnLeftToScript` /
  `_FaceAwayFromLeader` index `Area174_PoseTables` (3 entries) by a script
  byte, unchecked**: a byte past 2 reads `Area174_StepDeltas` and what follows
  as a pointer, which `Area174_SetPose` reads through (reads only; kept - the
  shipped scripts' bytes are not read here).

## 7. The band against the tool

Every start the tool lists is a function, and every function in the band is
a start (39 of 39). `area_rows.py`'s note on `0x4287E0` ("code immediates
0x460000") is the arrive hook's compare constant, not a pointer. The stack
dispatcher's two immediates (`0x428FC0`, `0x429080`) are its states and are
re-aimed at recorders (`Clone::imms`). The six area-175 choices are counted
in area 174's block by the tool's data-block rule; their owners by table are
areas 175..185 (AR4D's band starts at `0x4292C0`, right after them).

## 8. Calls across groups

- **Raw-address callees nobody owns:** `0x454A80`, `0x455290` (the
  `Field_Slots` release and start, AR2B's reading; handlers 0 and 17).
- **Inbound calls from outside the band** (for the rebinding pass):
  `Area_ArriveHook`'s case at `0x56E5A9` (`event_ops.cpp`'s
  `kArriveHandlers[7]`) into `Area173_ArriveHook`; `Field_ModeTailKinds[38]`
  (`0x662D80`) names `Area173_Tail38`; area 128's descriptor tables
  (`0x627A80` and its choice table) name `Area173_ScriptOnIfMember2` (AR3C);
  area 198's handler array (`0x649C94`, `0x649C98`) names
  `Area174_ScriptAnimationAt` / `_ScriptAnimationOn2` (AR4F); areas 175..185's
  choice tables (`0x6423CC..0x644610`, eleven tables) name the six
  `Area175_*` choices (AR4D). All read in place; nothing to rebind
  but the arrive handler's table entry.
- **Outbound to other groups' bands:** none; area 173's handler 0
  (`0x42C8A0`) and area 174's handler 1 (`0x42D250`) are other bands'
  functions named only by these areas' tables.
