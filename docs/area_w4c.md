# World 4, areas 173 and 174: the band `0x428450..0x4292C0`

**Status:** IN PROGRESS (2026-09-28) - 39 functions ours
(`src/game/area_w4c.cpp`, shadow name `area_w4c`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), four `Run`s (areas
173, 174, 175 and 198): @RESULT@ Fuzz only: no recorded route reaches either
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
  bytes `0x939A3C..0x939A3F`@REGIONS@.
- **Every round:** the script object at a field object, a party record or the
  running object itself; `Field_ActiveMember` at a field object, one of the
  four extra objects or a party record.
- **Seeds:** @SEEDS@
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3, the script object, `Field_ActiveMember`, the word timer,
  `Cond_ByteFD`.

@FUZZRESULT@

## 4. Controls

@CONTROLS@

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
