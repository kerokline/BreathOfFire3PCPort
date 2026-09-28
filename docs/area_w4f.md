# World 4, areas 192..199: the band `0x42BD60..0x42D710`

**Status:** IN PROGRESS (2026-09-28) - 49 functions ours
(`src/game/area_w4f.cpp`, shadow name `area_w4f`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area
with code: 0 mismatches in 294,000 rounds (in this worktree); 346 controls planted, 343 refused by a count, 3 equivalent mutants each with a refused variant (section 4).
Fuzz only: no recorded route reaches the band (section 8). No divergence; the
two two-state dispatchers abort past their tables and area 193's choice 4
past its three-row stack table, where the original would jump into data or
read its own frame (section 6).

Group AR4F of round ten's sixth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 18), the
last group of the area round. The band is the tool's (`tools/area_rows.py`,
[`area-rows.md`](area-rows.md)): 48 starts, none ours before, **49 taken** -
the tool's `0x42D4D0` is two functions (section 7). Areas 194 and 195 have
no code in the band (their descriptors `0x648DC0` / `0x648F28` have no
`+0x34`, `+0x3C` or `+0x40`, and no hook, tail or data table names code for
them).

Every function was read to its last instruction with capstone
(2026-09-28); each extent is the tool's (`area_funcs.tsv`) and agrees with
the reading but for that split; the clone tables are `area_rows.py
--clones`'s rows, each read against the disassembly. What an area *is* in
the story is not read here. The PSX twins are the sibling's
`names/area_records.toml` (descriptor handlers and inits only; area 199 has
neither there, and choices, hooks, tails, effect states have no pairing).

**`0x42D710`, where the band ends, is not area code.** Game mode 9's steps
table `0x656AC8` (`0x4965C0` jumps through it by `Game_Step`; mode 9 is what
`Field_Request` 7 leads to, [`mode-tasks.md`](mode-tasks.md) section 3) has
`0x517330` as its frame step, and that calls `0x42D710`: `jmp [0x64ADAC +
4 * (byte 0x929F00)]`, unchecked. The table `0x64ADAC` lies just past area
199's descriptor (`0x64AD68 + 0x44`), and its entries are `0x42D730` on -
code the catalogue labels BATE.EMI (`0x42D7A0`, `0x42D7F0`, `0x42D880`
paired with PSX `0x801D2114` / `0x801D2188` / `0x801D2290`). So `0x42D710`
is the BATE overlay's dispatcher by its state byte, the first function of
that overlay's code; it belongs with the battle-extra block, not with any
area. Not taken here; the 76 starts from it on are not this group's.

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's `Shape`):
`kChoice` a `+0x34` choice handler (the answer byte `0x7DEE67` in, the
message word `0x7DEE48` read after), `kHandler` a `+0x3C` handler
(movement-script ops `03` / `DE`), `kInit` the `+0x40` init, `kTail` a
`Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kHook` a step hook `(x,
z)` answering in `al`, `kState` a state handler reached through a table in
`.data`, `kCallee` a function called directly (by the group's own code, by
other code, or through an engine table). A function that is both a choice
and a handler is fuzzed by the shape of its first root.

### Area 192 (descriptor `0x647F18`; PSX `0x801F37C8`)

Seven choices (`0x647EFC`) and one handler (`0x647F14`, choice 6). Choices
0, 1, 3 and 6 (`0x42B550`, `0x42BD30`, `0x42B5B0`, `0x42B610`) are area
191's code, in AR4E's band; choice 4 is shared with area 191.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42BD60` | `Area192_ChoiceFocusPair` | `0x3C` | choice 2 | kChoice | the focus object `0x903804`'s dwords `+0x18` / `+0x1C` the byte pair `Area192_FocusPairs[s8 answer]` (unchecked); message `0xFFFF` stored between the first read and the first store; the focus pointer and the answer read again for the second |
| `0x42BDA0` | `Area192_ChoiceTailState` | `0x2A` | choice 4; area 191 choice 4 | kChoice | message `0xFFFF`; the tail state 2 for the answer 0, `0xA` for 1, `0x14` for any other (a `dec` / `neg` / `sbb` / `and` / `add` chain) |
| `0x42BDD0` | `Area192_ChoiceArmTail54` | `0x3E` | choice 5 | kChoice | message `0xFFFF`; 1: `ScriptFlags_Set40`, tail kind `0x36` at state `0x1E`; 2: `ScriptFlags_Set40`, `MoveScript_Var7` 1, its step `0x8034E5` `0x14` |
| `0x42BE10` | `Area192_Tail54` | `0x1EB` | tail kind 54 | kTail | the area's sequence (below) |
| `0x42C000` | `Area192_StepHook` | `0x95` | `Area_StepHook`'s case for area `0xC0` | kHook | `AreaMap_ByteAt` at the cell of x's and z's high words, and one on in x when x's low word is not 0, in z when z's is not, the diagonal when both - each asked whatever the others answered: any `0xA6` arms tail kind `0x36` at state 0 (after `ScriptFlags_Set40`), al 1; else al 0 |
| `0x42C0A0` | `Area192_TalkMessage` | `0x11B` | called by chapters 14 and 15 (section 9) | kCallee | the message a member says (below); ax |
| `0x42C1C0` | `Area192_TalkMessageB` | `0x3B` | called by `Area192_TalkMessage` | kCallee | the flagged talk's message (below); ax |
| `0x42C200` | `Area192_Init` | `0xC4` | init | kInit | Capcom's `jmp` over eleven `nop`s at the entry, the body at `0x42C210`: six placements by a set 0..2 (below) |
| `0x42C2D0` | `Area192_RestoreCharacters` | `0x79` | called by `Area192_Tail54` and area 191's `0x42B864` | kCallee | every character record with `+0xB` bit 0 ("joined"): HP `+0x18` = max HP `+0x20`, AP `+0x1A` = max AP `+0x22`, the status word `+0x10` 0; then each member's character record (`0x66972C` maps its id, `0x904062`) copied over its party record's `+0x80` block, `0xA4` bytes (`rep movsd`) |
| `0x42C350` | `Area192_Effect18Release77` | `0x35` | `EffectKind18_States[78]` (`0x6541A4`; a gap) | kState | story flag `0x77` clear: nothing; set: the word `0x90405C` `0x1E0`, bytes `0x929EC1` / `0x9036D0` 0, `Field_StatusBits` bit 0 cleared, a tail `jmp` to `Effect_Release` |

**Tail kind 54** (`Area192_Tail54`) switches on the s8 state through a byte
table of 31 (`+0x1CC`) into a jump table of 8 (`+0x1AC`), both inside its
extent; the `ja` makes every other state (and every negative one) nothing:

- 0: message `0x6D`, `Field_Request` 2, state 1 (1 waits for the message).
- 2 (unless `Field_Request` is 2): `Transition_Start(0)`, state 3.
- 3 (once `MoveScript_WaitWordDA` is 0): `Draw_PassFlags` 0,
  `Sound_StopMusic`, `Area192_RestoreCharacters`, `Sound_LoadStream(0)`,
  state 4.
- 4 (once `Sound_StreamDone`): story flag `0x82`; `Field_StatusBits` (read
  after the call) bit 0 cleared, the word `0x90405C` `0x1E0` and the bytes
  `0x929EC1` / `0x9036D0` 0 stored between; state `0xA`.
- `0xA` (unless request 2): `ScriptFlags_Clear40`, `Field_ChangeArea` to the
  return point `0x904148` (its area word `+8`, x, z; flags 4),
  `Field_ScriptFlags2` bit 6, the return point's byte `+0xA` 0, story flag
  `0x77` cleared, disarmed.
- `0x14` (unless request 2): `ScriptFlags_Clear40`, disarmed.
- `0x1E` (unless request 2): `ScriptFlags_Clear40`,
  `Field_ChangeArea(0x96, 0x180000, 0x300000, 1)`, bit 6, byte `+0xA` 0, flag
  `0x77` cleared, disarmed, `Field_StatusBits` (read after the last call) bit
  0 cleared.

The area word goes to `Field_ChangeArea` from `cx` with the register's high
half whatever the last call left; `Field_ChangeArea` reads it as a word
(its evidence), so the fuzz lists it with the area masked to 16 bits and the
flags to 8.

**The talk** (`Area192_TalkMessage(who)`): i = who's place among
`Area192_TalkWho`'s five ids (5 when none); the rank = byte `0x90405E`
capped at 8; the level = the byte `+0x1E` of the character record `0x66972C`
maps who to. `Cond_Flags` row 14's flag `0xA` set:
`Area192_TalkMessageB(i * 9, who, rank, level)` - the index plus
`Area192_TalkStepsB[rank]` for a level below 5, else plus 8, and the message
byte `Area192_TalkMessagesB` at it. Clear: the index plus 8 (level 8 or
more), 7 (level 5..7 with the rank below 7) or `Area192_TalkSteps[rank]`,
and the chapter row's flag 2 picks `Area192_TalkMessagesF` over
`Area192_TalkMessages` at it. Rows of 9 for the five ids; i = 5 reads on
into the next table (latent, section 6). The answer is a byte zero-extended
into `ax` (`movzx ax`); the callers hand `eax` to `Msg_OpenScript`, which
reads a word.

**The init** (`Area192_Init`): character record 0's level byte is read
first; story flag `0x77` set: nothing. Else a set: with row 14's flag `0xA`
clear, 2 for a level of 9 or more, 1 for 5..8, else whether the byte
`0x90405E` is 5 or more; with it set, 2 for a level of 5 or more, else the
same byte test. Then six times: `Sprite_FindFree` into the scratch word
`0x903850`, and for a slot `EventOp_0x` on op k of the set's block
(`Area192_PlaceOps[set] + k * 0x11`, the pointer read each time).

### Area 193 (descriptor `0x648908`; PSX `0x801F39B8`)

Six choices (`0x6488F0`) and one handler (`0x648904`, choice 5).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42C390` | `Area193_ChoiceMessage` | `0x17` | choice 0 | kChoice | the message word `Area193_Messages0[s8 answer]` |
| `0x42C3B0` | `Area193_ChoiceRunOnYes` | `0x2F` | choice 1 | kChoice | the message `Area193_Messages1[s8 answer]`; the answer 1: `ScriptFlags_Set40`, `MoveScript_Var7` 1, its step 0 |
| `0x42C3E0` | `Area193_ChoiceFill5B` | `0x44` | choice 2 | kChoice | the message `Area193_Messages2[s8 answer]`; the answer 1: `Inventory_Add(0, 0x5B, 0x10 - Inventory_Count(0, 0x5B, 0))` (the difference a byte; a fourth word 0 pushed, never read), `Sound_PlayEffect(0x106)` |
| `0x42C430` | `Area193_ChoiceFocusPair` | `0x3C` | choice 3 | kChoice | `Area192_ChoiceFocusPair`'s body over `Area193_FocusPairs` |
| `0x42C470` | `Area193_ChoiceMessageState` | `0x51` | choice 4 | kChoice | a three-row table built on the stack by the s8 answer: 0 message `0x1D` and tail state `0xC`, 1 `0x1E` / `0xE`, 2 `0xFFFF` / `0x10` |
| `0x42C4D0` | `Area193_MemberBit0Leader7` | `0x2F` | choice 5 = handler 0 (PSX `0x801F2E38`) | kChoice | `Field_ActiveMember`'s `+0x80` bit 0 cleared; the leader's `+0x89` 7: `ScriptFlags_Set40` and `Field_ActiveMember`'s (read again) word `+0x8A` + 1 |
| `0x42C500` | `Area193_Tail57` | `0x200` | tail kind 57 | kTail | the area's sequence (below) |
| `0x42C700` | `Area193_StepHook` | `0x3F` | `Area_StepHook`'s case for area `0xC1` | kHook | x or z exactly `0x18000`, or either exactly `0x338000` (whole dwords): `ScriptFlags_Set40`, tail kind `0x39` at state `0xA`, al 1; else al 0 |
| `0x42C740` | `Area193_Init` | `0x34` | init (PSX `0x801F31C0`) | kInit | story flag `0x8A`: `Draw_PassFlags` 0, tail kind `0x39` at state `0x14`; else the byte `0x937F98` 1 |

**Tail kind 57** (`Area193_Tail57`): the s8 state - `0xA` through a jump
table of 13 (`+0x1CC`, in its extent); the `ja` makes every other state
nothing:

- `0xA`: message `0x1C`, request 2, state `0xB` (which waits).
- `0xC` (unless request 2): story flag `0x77` cleared, `ScriptFlags_Clear40`,
  `Field_ChangeArea(0xBD, 0x12000000, 0x17FF0000, 2)`; `Field_ScriptFlags2`
  bit 6, the return point's byte `+0xB` 2, the word `0x90405C` 0, `0x90405F`
  `0xF0`, `0x90405E`, `0x929EC1` and `0x9036D0` 0, `Field_StatusBits` (read
  after the call) bit 0 set; disarmed.
- `0xE` (unless request 2): flag `0x77` cleared, `ScriptFlags_Clear40`,
  `Field_ChangeArea(0x97, 0x160000, 0x150000, 3)`, bit 6, disarmed.
- `0x10` (unless request 2): `ScriptFlags_Clear40`, disarmed.
- `0x14` (only with request 0): message `0x25` with `Cond_Flags` row 14's
  flag 3, else `0x20`; request 2, state `0x15`.
- `0x15` (unless request 2): `Party_HealJoined`, `Sound_LoadStream(0)`,
  state `0x16`.
- `0x16` (once `Sound_StreamDone`): counter 0 (`0x903848`) 1,
  `Music_Play(0x95, 0x10)`, `Transition_Start(1)`, `Draw_PassFlags` `0x1F`,
  row 14's flag 3 and story flag `0x8A` cleared, disarmed.

The init arms state `0x14` when flag `0x8A` is set, and the step hook state
`0xA`; `0xC`, `0xE` and `0x10` are the three answers of choice 4.

### Area 196 (descriptor `0x6490A8`; PSX `0x801F42B0`)

Three handlers (`0x64909C`), no choices, no init.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42C780` | `Area196_MessageByMember0` | `0x8B` | handler 0 (PSX `0x801F3F90`) | kHandler | the member search (below) over `Area196_Keys0` |
| `0x42C810` | `Area196_MessageByMember1` | `0x8B` | handler 1 (PSX `0x801F4054`) | kHandler | the member search over `Area196_Keys1` |
| `0x42C8A0` | `Area196_SetCondFE` | `0x8` | handler 2 (PSX `0x801F4118`); area 148 choice 2 = handler 0, area 167 choice 14 = handler 9, area 173 handler 0 | kHandler | `Cond_ByteFE` 1 |

**The member search** (nine bodies here, `0x8B` bytes each, and AR3F's
`Area143_MessageByMember` / area 145's three: a capstone compare of all nine
against `0x420960` differs only in the two table addresses): for each of the
four keys in turn, each of `Field_MemberCount`'s party records (the count
read once, compared unsigned): the first whose `+0x89` is the key opens that
key's message word (`Msg_OpenScript`) and sets `Field_Request` 2; none:
nothing. Each handler's keys are four bytes, its four message words four
bytes after them. Ours is one body (`MessageByMember`) behind the nine names.

### Area 197 (descriptor `0x649780`; PSX `0x801F49E4`)

Ten handlers (`0x649754`), no choices, no init. Handler 9 is
`Area143_SkipIfLeader89Is7` (`0x4209F0`, AR3F's).

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42C8B0` .. `0x42CC10` | `Area197_MessageByMember0` .. `6` | `0x8B` each | handlers 0..6 (PSX `0x801F39E0` .. `0x801F3E78`) | kHandler | the member search over `Area197_Keys` + `0xC` x n |
| `0x42CCA0` | `Area197_RunShake` | `0x12` | handler 7 (PSX `0x801F3F3C`) | kHandler | `Area197_ShakeStates` by the running object's `+4` |
| `0x42CCC0` | `Area197_ShakeStart` | `0x22` | `Area197_ShakeStates[0]` | kState | the running object's `+0xA` 4 and `+4` 1; `Field_ActiveMember`'s word `+0x8A` - 2 |
| `0x42CCF0` | `Area197_ShakeStep` | `0x62` | `Area197_ShakeStates[1]` | kState | `+0xA` 0: `+4` 0. Else x += step << 11, z -= step << 11 (`Area197_ShakeSteps` by `+0xA`'s low nibble, s8, the object and the step read again for z), `+0xA` - 1, the active member's word `+0x8A` - 2 |
| `0x42CD60` | `Area197_SpawnEffect92` | `0x67` | handler 8 (PSX `0x801F4050`) | kHandler | an effect of kind `0x92` (`Effect_FindFree`; none: nothing): `+0` 1, `+5` `0x92`, words `+0x2E` 0 and `+0x30` `-0x1E`, `+0x29` 6, `+0xB` the active member's index in `Sprite_Objects` (read after the search; a signed quotient by `0xA4`, as a byte) |
| `0x42CDD0` | `Area197_Tail51` | `0x5A` | tail kind 51 | kTail | state 0: `Party_DropIn(0)`, state 1; state 1 with counter 3 (`0x90384B`) `0x24`: story flag `0x55`, `Field_ChangeArea(0xAA, 0x60000, 0x3F0000, 0x82)`, state `0x1E`; any other state nothing |
| `0x42CE30` | `Area197_StepHook` | `0xC2` | `Area_StepHook`'s case for area `0xC5` | kHook | only with `Cond_ByteFD` 4 (below) |

**Area 197's step hook**: (A) z exactly `0x708000` or `0x738000` with x's
high word 3..5, or x exactly `0x28000` or `0x58000` with z's high word
`0x71..0x73` (each a 16-bit `(high - base) < n`); (B) x exactly `0xD8000`
with z's high word `0x71` or `0x72` and key item `0xD` not held. A with key
item `0xD` held: `ScriptFlags_Set40`, tail kind `0x33` at state 0, al 1; A
without it, and B: `ScriptFlags_Set40`, `MoveScript_Var7` 5, its step `0x14`,
al 1. Anything else al 0 (so tail kind 51 is armed only by this hook, with
the key item).

### Area 198 (descriptor `0x649CC0`; PSX `0x801F4294`)

Twelve handlers (`0x649C90`), no choices, no init. Handlers 1 and 2
(`0x428AB0`, `0x428B10`) are area 173 / 174's code in AR4C's band; handler 3
is `Area85_ReleaseSlots` (`0x40F990`, AR2B's), read in place.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42CF00` | `Area198_StartSlotScript0` | `0x31` | handler 0 (PSX `0x801F2E00`) | kHandler | `0x454A80(Sprite_Current)` (its `Field_Slots` scripts released), `0x455290(Sprite_Current, Area198_SlotScript0)` (one started), `+0x2A` 1, `Sprite_SetAnimation(0)` - `Sprite_Current` read for each. `Area85_StartSlotScript` (`0x40F960`) plus the `+0x2A` store |
| `0x42CF40` | `Area198_SinkFade` | `0xBD` | handler 4 (PSX `0x801F2FF8`) | kHandler | the running object's word `+0x3E` - `0x10`; the script object's byte `+2` below `0x80` with bits 0..1 clear: `Sprite_SetAnimationAt(Area198_SinkAnims[bits 2..3], word +0x58 - 2)` and the script object read again; `+2` + 1; the word `+0x3E` below `-0x280` (signed): `+0x5D`, `+0x5E`, `+0x5F` each - 1 unless it is `0x80`; exactly `0xF600`: counter 0 + 1; above `-0xC80` (signed): the script position - 2 (it runs again), else the script object's `+2` 0 |
| `0x42D000` | `Area198_SpawnEffectsA6` | `0xF9` | handler 5 (PSX `0x801F3198`) | kHandler | three effects of kind `0xA6` (each `Effect_FindFree`; none: that one skipped): `+0` 1, `+5` `0xA6`, `+6` 1, 0, 2, `+7` the active member's index (signed quotient, read after each search) |
| `0x42D100` | `Area198_Shake` | `0xFD` | handler 6 (PSX `0x801F3384`) | kHandler | x += step << 11, z -= step << 11, the word `+0x3E` += step << 4 (`Area198_ShakeSteps` by `+0xA`'s low nibble, s8, read again for each), `+0xA` - 1, the script position - 2; counter 1 (`0x903849`) not 0: an effect of kind `0xAC` (`+0` 1, `+5` `0xAC`; none: no effect), counter 1 0, `+0x5F`, `+0x5E`, `+0x5D` `0xC0`; counter 1 0: each of `+0x5D..+0x5F` + 8 unless 0 |
| `0x42D200` | `Area198_WalkToX190` | `0x43` | handler 7 (PSX `0x801F3524`) | kHandler | d = (`0x190000` - x) >> 15 (32-bit, arithmetic); not 0: the script object's `+7` abs(d) as a byte, the running object's direction `+8` 3, `MoveCmd_Move(script object, +8)` |
| `0x42D250` | `Area198_ReleaseOnRequest5` | `0x24` | handler 8 (PSX `0x801F35A0`); area 174 handler 1 | kHandler | `Field_Request` 5: `0x454A80(Sprite_Current)`; else the script position - 2 (it waits) |
| `0x42D280` | `Area198_SpawnDrops` | `0x53` | handler 9 (PSX `0x801F35FC`) | kHandler | every eighth frame (`Frame_Counter & 7` 0) with counter 0 above 2: `+0xB` with bits 0..2 clear - `Sound_PlayEffect(0)` (section 6); `Area198_SpawnDrop`, and again with counter 0 (read again) above 3. The script position - 2 every time |
| `0x42D2E0` | `Area198_SpawnDrop` | `0x105` | called by `Area198_SpawnDrops` | kCallee | a drop (below) |
| `0x42D3F0` | `Area198_SpawnEffectA7` | `0x80` | handler 10 (PSX `0x801F3850`) | kHandler | `Effect_FindFree` kept at the running object's `+0xB` (read back for each store): none - the script position - 2 (it waits); else an effect of kind `0xA7` at the object's x, z and y + `0x1000000` |
| `0x42D470` | `Area198_StartSlotScript11` | `0x39` | handler 11 (PSX `0x801F39B0`) | kHandler | as handler 0 with `Area198_SlotScript11`, and `+0` bit 5 set after `+0x2A` |
| `0x42D4B0` | `Area198_EffectA6Run` | `0x12` | `Effect_KindHandlers[0xA6]` (`0x6555E8`; a gap) | kCallee | `Area198_EffectA6States` by the record's `+1` |
| `0x42D4D0` | `Area198_EffectA6Start` | `0xB0` | `Area198_EffectA6States[0]` | kState | the record's x, z, y field object `+7`'s (`Sprite_Objects`, the byte unchecked; re-read for each); `Sprite_SetAnimationBank` of record `+6` of `Area198_EffectA6Records`; `+0x48` 1, `+0x24` 0, `+0x2A` 0; `Sprite_SetAnimation` of the record; `+1` 1; a tail `jmp` into state 1 |
| `0x42D580` | `Area198_EffectA6Follow` | `0x16B` | `Area198_EffectA6States[1]`, state 0's tail | kState | field object `+7`'s `+0` to the record's `+0`; `Sprite_ScriptTick`, `Sprite_QueueOverlay`; the object's `+0x5C..+0x5F`, `+0x27`, `+0x48`, dwords `+0x40` / `+0x44`, words `+0x2E` / `+0x30` / `+0x32` and dword `+0x60` copied; `+0x29` 6; then the words `+0x2E` / `+0x30` offset by `0x441090` of the record's s16 x / z scale (below) |

**The drop** (`Area198_SpawnDrop`): the running object kept;
`Sprite_FindFree` into the scratch word `0x903850`; a slot:
`EventOp_0x(Area198_DropOp)` places an object and makes it `Sprite_Current`;
its dwords `+0x10`, `+0xC` 0 and `+0x14` 8, `+9` `0x10`, the word `+0x3E`
`(Rand() % 30 - 6) << 5`, `+0x2B` 1, and a cell (a, b) drawn as `a = Rand()
% 14 + 0x10`, `b = Rand() % 25 + 0xB` (bytes; `idiv`, so a negative `Rand`
would give a negative remainder) again while a is below `0x1A` and b below
`0x15`: x = a << 16, z = b << 16. Then `Sprite_Current` is put back and the
kept object's `+0xB` + 1 (whether a drop was placed or not).

**Effect kind `0xA6`**: area 198's handler 5 spawns three with `+6` 1, 0, 2
and `+7` the member whose script ran it. State 0 puts the record where field
object `+7` is and sets its animation bank and animation from its record
(`+6`), then falls into state 1, which each frame copies the object's flags,
tint, `+0x27`, `+0x48`, `+0x40` / `+0x44`, `+0x2E..+0x32` and `+0x60`, runs
the record's script and overlay, and offsets `+0x2E` / `+0x30` by the
record's s16 scales: with `+0x48` set, `0x441090(scale * +0x40, +0x40)` and
`0x441090(scale * +0x44, +0x44)` (the 32-bit products); without it,
`0x441090(scale << 16, 0x10000)` each - `0x441090` answering the product's
high word, rounded up for a non-negative second argument when the low word is
not 0. Each word's address is taken before its call and added to after it.

### Area 199 (descriptor `0x64AD68`)

One choice (`0x64AD60`), nothing else; no PSX record in
`names/area_records.toml`.

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x42D6F0` | `Area199_ChoiceStepIfAnswer` | `0x19` | choice 0 | kChoice | message `0xFFFF`; the byte `0x8034E5` 1 for any answer but 0, else 0 (`setne`) |

## 2. Ours

`src/game/area_w4f.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee, ours or Capcom's; `AH_AT` for the
three raw addresses of section 9). The group's own callees are called the
same way (`AH_CALL(Area192_TalkMessageB)`, `AH_CALL(Area198_SpawnDrop)`,
`AH_CALL(Area198_EffectA6Follow)` for state 0's tail `jmp`,
`AH_CALL(Area192_RestoreCharacters)`), so the fuzz stands a recorder in for
each and every function is tested alone; the two state dispatchers read
their `.data` tables in place and call the entry, so the fuzz's `DataTable`
swap stands recorders there. Shapes that repeat are one helper: the member
search (nine names), the focus pair (two), the choice's message by answer
(three), the slot-script pair (`ReleaseSlots` / `StartSlot`), the shakes' x /
z step (`ShakeXZ`, areas 197 and 198), the tail's arming (`ArmTail`,
`ArmRun`, `Disarm`), the member's index (`MemberIndex`). Kept as the
originals: every re-read after a call or a store (`Sprite_Current` in the
shakes, the drop and effect kind `0xA6`, `MoveScript_Object` after the sink's
animation, `Field_ActiveMember` after `ScriptFlags_Set40` and after
`Effect_FindFree`, `Field_StatusBits` after the tails' calls, the focus
pointer and the answer in the pair choices, the effect slot read back from
the object's `+0xB`), the order of every call (the tail's `Sound_StopMusic`
before the restore), the 16-bit compares of the hooks' high words, the
signed quotients (`/ 0xA4` truncating toward 0), the signed 16-bit compares
of the sink's height, the CRT's signed remainders in the drop, the byte
arithmetic of the talk index, and the unchecked reads of section 6.

## 3. The fuzz

`BOF3X_SHADOW=area_w4f` (`src/game/area_w4f_fuzz.cpp`): six `Run`s under the
one shadow name, one per area with code, each with its `Group::area` (192,
193, 196, 197, 198, 199), 6,000 rounds per function, the real descriptors
and tables in place. Area 198's effect kind `0xA6` runs under area 198 (its
spawner), though its states sit in area 199's data.

- **Clones** as the tool's rows but two: area 192's init is cloned from its
  body `0x42C210` (`0xB4` bytes; `CloneOriginal` refuses the entry, Capcom's
  own `jmp` over eleven `nop`s, as already patched - the round's AR1D / AR1F
  finding; `BOF3_INJECT` patches `0x42C200` as usual), and the tool's
  `0x42D4D0` is cloned as `0x42D4D0` (`0xB0`, its tail `jmp` a call site
  re-aimed at the state-1 recorder) and `0x42D580` (`0x16B`, the tool's call
  offsets less `0xB0`).
- **Callees the group lists** (beyond the standard set, or recorded
  differently): `ScriptFlags_Set40` / `Clear40`, `Transition_Start`,
  `Sound_StopMusic` (Capcom's), `Sound_LoadStream`, `Sound_StreamDone`
  (`kFlag`: tested as a whole `eax`), `Flags_Test` (`kFlag` rather than the
  standard `kBool`: tested in `al`, so garbage above a 0 tells an `eax` test),
  `KeyItem_Has`, `Field_ChangeArea` (area masked to 16 bits, flags to 8),
  `AreaMap_ByteAt` (`0xA6` half the time), `Sprite_FindFree` (none a third of
  the time, else 0..29), `EventOp_0x`, `Effect_FindFree` (none a third of the
  time, else a slot of the first four records), `Effect_Release`,
  `Party_HealJoined`, `Sprite_SetAnimationAt`, `Sprite_ScriptTick`,
  `Sprite_QueueOverlay`, `MoveCmd_Move` (Capcom's), `0x454A80`, `0x455290`
  (`kByte 0xFF..0x07`) and `0x441090` by raw address, and the group's own
  called directly: `Area192_TalkMessageB` (the index, rank and level as
  bytes; `who` passed and unread, masked 0), `Area192_RestoreCharacters`,
  `Area198_SpawnDrop`, `Area198_EffectA6Follow` (also the table's entry 1:
  the harness shares one recorder by address).
- **Louder stand-ins** (each part of the time, from `Noise`):
  `ScriptFlags_Set40` and `Effect_FindFree` move `Field_ActiveMember` (area
  193's handler 0 and the effect spawns read it after); `Flags_Set`,
  `Flags_Clear`, `Field_ChangeArea` and `Flags_Test` move `Field_StatusBits`
  (the tails of areas 192 and 193 read it on both sides of those calls), and
  `Flags_Test` in area 192's run also character record 0's level byte (the
  init reads it before the call); `EventOp_0x` moves
  `Sprite_Current` (the placement makes the new object current; the drop
  writes it after); `Sprite_SetAnimationAt` and `MoveCmd_Move` move
  `MoveScript_Object`; `Sprite_ScriptTick` / `Sprite_QueueOverlay` move
  `Sprite_Current` (effect kind `0xA6`'s state 1 reads it for every store
  after them).
- **Data tables** swapped for recorders: `Area197_ShakeStates`,
  `Area198_EffectA6States`.
- **Regions beyond the field frame**, per area (the harness puts back only
  its regions, so every cell a seed, stand-in or the disturbance writes is
  one): the focus object, flag row, active member and script object pointers
  and `MoveScript_WaitWordDA` in every area; area 192 the eight character
  records (`0x520` bytes), `0x929EC1`, `0x9036D0`, `Draw_PassFlags`; area 193
  the last three and `0x937F98`; area 196 `Cond_ByteFE`; areas 197 and 198
  all twenty `Effect_Objects` records and the record an unchecked slot of
  `0xFF` would write (`0x7E9160`, past the pool).
- **Seeds:** each choice's answer at every row it has, one past, a negative
  byte and anything (area 193's choice 4 at its three rows only: ours aborts
  past them); the tails at every state their tables name, their neighbours,
  the states that do nothing and negative bytes, with `Field_Request` 2 or
  not and the wait word 0 or not; the step hooks' cells on, beside and past
  their exact values and high-word spans (area 192's with the low words 0
  half the time, else a low word with a zero low byte or any; area 197's `Cond_ByteFD` 4 mostly); the talk's `who` one of
  the five ids (or one beside), the rank byte `0x90405E` around 5, 7 and 8,
  each character record's level byte around 5, 8 and 9 (the init's too); the
  talk B's index a row start, its rank 0..8 or any, its level around 5; each
  member's `+0x89` a key of the handler's table (or one past), the member
  count 0 a tenth of the time; the leader's `+0x89` 7 or beside; the shakes'
  count `+0xA` 0..5, `0x10`, `0x14`, `0xFF`; the sink's height (after its
  `- 0x10`) on and beside `-0x280`, `0xF600` and `-0xC80`, the tint bytes at
  `0x80`, beside and 0, the script object's `+2` at multiples of 4, beside
  and above `0x80`; the walk's x a whole step from `0x190000`, 0, +-1,
  +-`0x8000` and far; `Field_Request` 5 or beside; the drops' frame counter
  a multiple of 8 and counter 0 around 2..4; the effect record as
  `Sprite_Current` with state 0 or 1, its object `+7` 0..29 and record `+6`
  0..2 mostly, the object's `+0x48` 0 half the time; `Field_ActiveMember` on
  and between the records of `Sprite_Objects` and just below the first (the
  quotient truncates toward 0).
- **The group's disturbance** (from the hash it is given): the tail state,
  counters 0, 1 and 3, the three pointers, the active member, the rank byte
  `0x90405E`, the wait word, the leader's `+0x89`, the answer (0..2),
  `Field_StatusBits`, and in area 192's run character record 0's level byte.

**Result (in this worktree):** 294,000 rounds over the 49 functions (6,000
each), 293,145 calls to the stand-ins, 0 mismatches. Coverage: every callee
each function can reach was called - e.g. area 192's run `AreaMap_ByteAt`
13,413, `Flags_Test` 22,009, `EventOp_0x` 8,142 and `Area192_TalkMessageB`
4,037, `Field_ChangeArea` 681 from tail 54; area 193's `Field_ChangeArea` 696,
`Music_Play` 374, `Inventory_Add` 649; area 197's `Msg_OpenScript` 27,169
over its seven searches, the shake's two states 2,971 / 3,029; area 198's
`Area198_SpawnDrop` 6,040, `Rand` 15,358, `0x441090` 12,000,
`Area198_EffectA6Follow` 9,061.

`BOF3X_SHADOW='*'`: exit 0, `inject: 5407 ours` (one below the 5,408 `impl`
lines, the off-by-one the round doc section 10 notes), 493 self-test lines, 790
of them with a mismatch count and every one 0. Run three times (after the
first fuzz and twice on the final one); it did not die silently.

## 4. Controls

Planted one at a time in `area_w4f.cpp` by a script (the scratch `controls.py`, not committed) that plants on an anchor it checks is unique, rebuilds, checks `area_w4f.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w4f`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches in all six runs). **346 planted, 343 refused by a count (exit 3), 3 not refused - each an equivalent mutant with a refused near variant**; no hang, no fault. Every one of the 49 functions has at least one control of its own; a control in a helper shared across functions (`MessageByMember`, `FocusPair`, `ShakeXZ`, `ArmTail`, `StateEntry`, ...) is refused in the first function's run, whose Fatal ends the self-test.

- **Four runs.** The first (the table's plants but the three variants) left five standing: A33 (`Field_StatusBits` read before `Flags_Set` in tail 54's state 4) - the fuzz's `Flags_Set` was the standard quiet one; E39 (the kind-`0xAC` spawn testing `0xFE` for none) - the write to record 255 lies past the pool, outside every region. The fuzz then gained a `Flags_Set` that moves the status bits and `Effect_Objects + 0xFF * 0x80` (`0x7E9160`) as a region of areas 197 and 198; the second run refused both, but three read-order plants were refused in only 1..4 rounds (A45, A88, B28: only the group's rare disturbance moved the cells they read). `Flags_Clear` and `Field_ChangeArea` now move the status bits and `Flags_Test` record 0's level byte (in area 192's run), the step hook's seed gives z low words with a zero low byte, and the init's seeds sit on the rank and level edges more often; a third run of the fifteen affected controls refused each in 145..187 rounds, and the fourth, of all 346 on the final fuzz, gives the table's counts. A7, D10 and D13 stood on every run: equivalent (the table says why), their variants A7b, D10b, D13b refused.
- The thinnest: A80 7, A81 14, A65 23, D37 24, D43 33, B39 39 rounds; every other control needs more than 39.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `FocusPair (192, 193)` | first byte of the pair +1 | Area192_ChoiceFocusPair 4775 |
| A2 | `FocusPair (192, 193)` | +0x14 for +0x18 | Area192_ChoiceFocusPair 6000 |
| A3 | `FocusPair (192, 193)` | +0x20 for +0x1C | Area192_ChoiceFocusPair 6000 |
| A4 | `FocusPair (192, 193)` | message 0xFFFE | Area192_ChoiceFocusPair 6000 |
| A5 | `FocusPair (192, 193)` | answer unsigned | Area192_ChoiceFocusPair 1639 |
| A6 | `Area192_ChoiceFocusPair` | area 193's pairs | Area192_ChoiceFocusPair 3336 |
| A7 | `FocusPair (192, 193)` | focus not read again | not refused: equivalent - the focus pointer is read again with no call between, and the only store between (the focus object's `+0x18`) cannot reach the pointer cell `0x903804`; variant A7b refused |
| A8 | `Area192_ChoiceTailState` | answer 0 state 3 | Area192_ChoiceTailState 826 |
| A9 | `Area192_ChoiceTailState` | answer 1 state 0xB | Area192_ChoiceTailState 828 |
| A10 | `Area192_ChoiceTailState` | else state 0x15 | Area192_ChoiceTailState 4346 |
| A11 | `Area192_ChoiceTailState` | state 0xA for 2 | Area192_ChoiceTailState 1655 |
| A12 | `Area192_ChoiceTailState` | message 0xFFFE | Area192_ChoiceTailState 6000 |
| A13 | `Area192_ChoiceArmTail54` | state 0x1F | Area192_ChoiceArmTail54 594 |
| A14 | `Area192_ChoiceArmTail54` | kind 0x37 | Area192_ChoiceArmTail54 594 |
| A15 | `Area192_ChoiceArmTail54` | Var7 2 | Area192_ChoiceArmTail54 655 |
| A16 | `Area192_ChoiceArmTail54` | step 0x15 | Area192_ChoiceArmTail54 655 |
| A17 | `Area192_ChoiceArmTail54` | tail on 3 | Area192_ChoiceArmTail54 1223 |
| A18 | `Area192_ChoiceArmTail54` | run on 2 or more | Area192_ChoiceArmTail54 4134 |
| A19 | `Area192_ChoiceArmTail54` | message 0xFFFE | Area192_ChoiceArmTail54 5960 |
| A20 | `ArmTail (192, 193, 197)` | kind and state swapped | Area192_ChoiceArmTail54 594, Area192_StepHook 4414 |
| A21 | `ArmRun (192, 193, 197)` | Var7 + 1 | Area192_ChoiceArmTail54 655 |
| A22 | `Area192_Tail54` | message 0x6E | Area192_Tail54 458 |
| A23 | `Area192_Tail54` | state 0 to 2 | Area192_Tail54 458 |
| A24 | `Area192_Tail54` | state 2 waits on 3 | Area192_Tail54 70 |
| A25 | `Area192_Tail54` | transition 1 | Area192_Tail54 382 |
| A26 | `Area192_Tail54` | state 2 to 4 | Area192_Tail54 382 |
| A27 | `Area192_Tail54` | wait word low byte | Area192_Tail54 66 |
| A28 | `Area192_Tail54` | pass flags 1 | Area192_Tail54 132 |
| A29 | `Area192_Tail54` | restore before the music stops | Area192_Tail54 132 |
| A30 | `Area192_Tail54` | stream 1 | Area192_Tail54 132 |
| A31 | `Area192_Tail54` | stream test on al | Area192_Tail54 69 |
| A32 | `Area192_Tail54` | flag 0x83 | Area192_Tail54 401 |
| A33 | `Area192_Tail54` | status read before the call | Area192_Tail54 203 |
| A34 | `Area192_Tail54` | word 0x1E1 | Area192_Tail54 401 |
| A35 | `Area192_Tail54` | status & 0xFC | Area192_Tail54 191 |
| A36 | `Area192_Tail54` | state 4 to 0xB | Area192_Tail54 401 |
| A37 | `Area192_Tail54` | x and z swapped | Area192_Tail54 337 |
| A38 | `Area192_Tail54` | flags 5 | Area192_Tail54 337 |
| A39 | `Area192_Tail54` | area as a byte | Area192_Tail54 336 |
| A40 | `Area192_Tail54` | byte +0xB for +0xA | Area192_Tail54 337 |
| A41 | `Area192_Tail54` | bit 5 | Area192_Tail54 248 |
| A42 | `Area192_Tail54` | state 0x14 without Clear40 | Area192_Tail54 362 |
| A43 | `Area192_Tail54` | flags 2 | Area192_Tail54 344 |
| A44 | `Area192_Tail54` | area 0x95 | Area192_Tail54 344 |
| A45 | `Area192_Tail54` | status read before the call | Area192_Tail54 184 |
| A46 | `Area192_Tail54` | status & 0x7E | Area192_Tail54 177 |
| A47 | `Area192_Tail54` | flag 0x78 | Area192_Tail54 344 |
| A48 | `Disarm (192, 193)` | kind 1 | Area192_Tail54 1043 |
| A49 | `Area192_StepHook` | cell 0xA7 | Area192_StepHook 1619 |
| A50 | `Area192_StepHook` | second cell diagonal | Area192_StepHook 2960 |
| A51 | `Area192_StepHook` | z low byte | Area192_StepHook 1490 |
| A52 | `Area192_StepHook` | diagonal on x alone | Area192_StepHook 1492 |
| A53 | `Area192_StepHook` | state 1 | Area192_StepHook 4414 |
| A54 | `Area192_StepHook` | answers 2 | Area192_StepHook 4414 |
| A55 | `Area192_StepHook` | x + 2 | Area192_StepHook 2960 |
| A56 | `Area192_StepHook` | x shift 15 | Area192_StepHook 2999 |
| A57 | `Area192_TalkMessage` | four ids | Area192_TalkMessage 2734 |
| A58 | `Area192_TalkMessage` | rows of 8 | Area192_TalkMessage 5445 |
| A59 | `Area192_TalkMessage` | rank cap 7 | Area192_TalkMessage 2371 |
| A60 | `Area192_TalkMessage` | byte +0x1F | Area192_TalkMessage 4273 |
| A61 | `Area192_TalkMessage` | flag 0xB | Area192_TalkMessage 6000 |
| A62 | `Area192_TalkMessage` | rank and level swapped | Area192_TalkMessage 3652 |
| A63 | `Area192_TalkMessage` | level 9 | Area192_TalkMessage 168 |
| A64 | `Area192_TalkMessage` | level above 5 | Area192_TalkMessage 58 |
| A65 | `Area192_TalkMessage` | rank below 8 | Area192_TalkMessage 23 |
| A66 | `Area192_TalkMessage` | plus 6 | Area192_TalkMessage 121 |
| A67 | `Area192_TalkMessage` | next step | Area192_TalkMessage 86 |
| A68 | `Area192_TalkMessage` | row flag 3 | Area192_TalkMessage 1963 |
| A69 | `Area192_TalkMessage` | one table | Area192_TalkMessage 1150 |
| A70 | `Area192_TalkMessage` | record of who + 1 | Area192_TalkMessage 4246 |
| A71 | `Area192_TalkMessage` | plus 9 | Area192_TalkMessage 860 |
| A72 | `Area192_TalkMessageB` | level below 6 | Area192_TalkMessageB 1012 |
| A73 | `Area192_TalkMessageB` | rank & 7 | Area192_TalkMessageB 690 |
| A74 | `Area192_TalkMessageB` | plus 7 | Area192_TalkMessageB 4024 |
| A75 | `Area192_TalkMessageB` | next message | Area192_TalkMessageB 5861 |
| A76 | `Area192_TalkMessageB` | level as a dword | Area192_TalkMessageB 1976 |
| A77 | `Area192_TalkMessageB` | base + 1 | Area192_TalkMessageB 5861 |
| A78 | `Area192_Init` | flag 0x78 | Area192_Init 6000 |
| A79 | `Area192_Init` | level 10 | Area192_Init 92 |
| A80 | `Area192_Init` | level 6 | Area192_Init 7 |
| A81 | `Area192_Init` | byte 6 | Area192_Init 14 |
| A82 | `Area192_Init` | flagged level 4 | Area192_Init 76 |
| A83 | `Area192_Init` | five ops | Area192_Init 2046 |
| A84 | `Area192_Init` | none 0xFE | Area192_Init 1869 |
| A85 | `Area192_Init` | ops of 0x10 | Area192_Init 2036 |
| A86 | `Area192_Init` | set & 1 | Area192_Init 1572 |
| A87 | `Area192_Init` | index & 0x7F | Area192_Init 690 |
| A88 | `Area192_Init` | level read after the call | Area192_Init 124 |
| A89 | `Area192_Init` | flag branches swapped | Area192_Init 687 |
| A90 | `Area192_RestoreCharacters` | seven records | Area192_RestoreCharacters 2935 |
| A91 | `Area192_RestoreCharacters` | bit 1 | Area192_RestoreCharacters 5980 |
| A92 | `Area192_RestoreCharacters` | HP from +0x22 | Area192_RestoreCharacters 5982 |
| A93 | `Area192_RestoreCharacters` | AP from +0x20 | Area192_RestoreCharacters 5982 |
| A94 | `Area192_RestoreCharacters` | word +0x12 | Area192_RestoreCharacters 5982 |
| A95 | `Area192_RestoreCharacters` | one member fewer | Area192_RestoreCharacters 5261 |
| A96 | `Area192_RestoreCharacters` | a dword short | Area192_RestoreCharacters 5261 |
| A97 | `Area192_RestoreCharacters` | party +0x84 | Area192_RestoreCharacters 5261 |
| A98 | `Area192_RestoreCharacters` | record of id + 1 | Area192_RestoreCharacters 4487 |
| A99 | `Area192_RestoreCharacters` | next list byte | Area192_RestoreCharacters 4645 |
| A100 | `Area192_Effect18Release77` | flag 0x76 | Area192_Effect18Release77 6000 |
| A101 | `ResetWord1E0 (192)` | word 0x1F0 | Area192_Effect18Release77 3996 |
| A102 | `ResetWord1E0 (192)` | byte 1 | Area192_Effect18Release77 3996 |
| A103 | `Area192_Effect18Release77` | bit 1 | Area192_Effect18Release77 2996 |
| A104 | `Area192_Effect18Release77` | no release | Area192_Effect18Release77 3996 |
| B1 | `Area193_ChoiceMessage` | other table | Area193_ChoiceMessage 5746 |
| B2 | `MessageByAnswer (193 x3)` | answer unsigned | Area193_ChoiceMessage 1747, Area193_ChoiceRunOnYes 1681, Area193_ChoiceFill5B 1679 |
| B3 | `Area193_ChoiceRunOnYes` | step 1 | Area193_ChoiceRunOnYes 683 |
| B4 | `Area193_ChoiceRunOnYes` | on 2 | Area193_ChoiceRunOnYes 1289 |
| B5 | `Area193_ChoiceRunOnYes` | Var7 2 | Area193_ChoiceRunOnYes 683 |
| B6 | `Area193_ChoiceRunOnYes` | other table | Area193_ChoiceRunOnYes 5307 |
| B7 | `Area193_ChoiceFill5B` | on any answer | Area193_ChoiceFill5B 4709 |
| B8 | `Area193_ChoiceFill5B` | counts 0x5C | Area193_ChoiceFill5B 649 |
| B9 | `Area193_ChoiceFill5B` | to 0xF | Area193_ChoiceFill5B 649 |
| B10 | `Area193_ChoiceFill5B` | count not a byte | Area193_ChoiceFill5B 604 |
| B11 | `Area193_ChoiceFill5B` | adds 0x5A | Area193_ChoiceFill5B 649 |
| B12 | `Area193_ChoiceFill5B` | sound 0x107 | Area193_ChoiceFill5B 649 |
| B13 | `Area193_ChoiceFill5B` | other table | Area193_ChoiceFill5B 5207 |
| B14 | `Area193_ChoiceFill5B` | count high byte | Area193_ChoiceFill5B 644 |
| B15 | `Area193_ChoiceFocusPair` | area 192's pairs | Area193_ChoiceFocusPair 2797 |
| B16 | `Area193_ChoiceMessageState` | row 1 message 0x1F | Area193_ChoiceMessageState 1963 |
| B17 | `Area193_ChoiceMessageState` | row 2 state 0x11 | Area193_ChoiceMessageState 2011 |
| B18 | `Area193_ChoiceMessageState` | row 0 state 0xD | Area193_ChoiceMessageState 2026 |
| B19 | `Area193_ChoiceMessageState` | row 2 message 0xFFFE | Area193_ChoiceMessageState 2011 |
| B20 | `Area193_MemberBit0Leader7` | bits 0..1 cleared | Area193_MemberBit0Leader7 2922 |
| B21 | `Area193_MemberBit0Leader7` | leader 6 | Area193_MemberBit0Leader7 2365 |
| B22 | `Area193_MemberBit0Leader7` | script + 2 | Area193_MemberBit0Leader7 1546 |
| B23 | `Area193_MemberBit0Leader7` | member not read again | Area193_MemberBit0Leader7 713 |
| B24 | `Area193_Tail57` | message 0x1D | Area193_Tail57 403 |
| B25 | `Area193_Tail57` | state 0xC | Area193_Tail57 403 |
| B26 | `Area193_Tail57` | flags 3 | Area193_Tail57 320 |
| B27 | `Area193_Tail57` | z 0x17FF8000 | Area193_Tail57 320 |
| B28 | `Area193_Tail57` | status read before the call | Area193_Tail57 160 |
| B29 | `Area193_Tail57` | byte +0xB 3 | Area193_Tail57 320 |
| B30 | `Area193_Tail57` | word 1 | Area193_Tail57 320 |
| B31 | `Area193_Tail57` | byte 0xF1 | Area193_Tail57 320 |
| B32 | `Area193_Tail57` | 0x90405E 1 | Area193_Tail57 320 |
| B33 | `Area193_Tail57` | bit 1 | Area193_Tail57 226 |
| B34 | `Area193_Tail57` | x and z swapped | Area193_Tail57 376 |
| B35 | `Area193_Tail57` | no flag clear | Area193_Tail57 376 |
| B36 | `Area193_Tail57` | bit 7 | Area193_Tail57 262 |
| B37 | `Area193_Tail57` | waits on 1 | Area193_Tail57 122 |
| B38 | `Area193_Tail57` | state 0x14 on request 2 | Area193_Tail57 297 |
| B39 | `Area193_Tail57` | messages swapped | Area193_Tail57 39 |
| B40 | `Area193_Tail57` | flag 4 | Area193_Tail57 39 |
| B41 | `Area193_Tail57` | state 0x16 | Area193_Tail57 39 |
| B42 | `Area193_Tail57` | no heal | Area193_Tail57 486 |
| B43 | `Area193_Tail57` | state 0x17 | Area193_Tail57 486 |
| B44 | `Area193_Tail57` | counter 2 | Area193_Tail57 367 |
| B45 | `Area193_Tail57` | frames 0x11 | Area193_Tail57 374 |
| B46 | `Area193_Tail57` | transition 2 | Area193_Tail57 374 |
| B47 | `Area193_Tail57` | pass flags 0x1E | Area193_Tail57 374 |
| B48 | `Area193_Tail57` | row flag 4 | Area193_Tail57 374 |
| B49 | `Area193_Tail57` | flag 0x8B | Area193_Tail57 374 |
| B50 | `Area193_Tail57` | waits on 5 | Area193_Tail57 87 |
| B51 | `Area193_StepHook` | no z 0x18000 | Area193_StepHook 290 |
| B52 | `Area193_StepHook` | x 0x18001 | Area193_StepHook 452 |
| B53 | `Area193_StepHook` | z 0x338001 | Area193_StepHook 481 |
| B54 | `Area193_StepHook` | state 0xB | Area193_StepHook 1426 |
| B55 | `Area193_StepHook` | x low word | Area193_StepHook 176 |
| B56 | `Area193_Init` | flag 0x8B | Area193_Init 6000 |
| B57 | `Area193_Init` | pass flags 1 | Area193_Init 3980 |
| B58 | `Area193_Init` | state 0x15 | Area193_Init 3980 |
| B59 | `Area193_Init` | pending 2 | Area193_Init 2020 |
| B60 | `Area193_Init` | kind 0x3A | Area193_Init 3980 |
| C1 | `MessageByMember (196 x2, 197 x7)` | three keys | Area196_MessageByMember0 594, Area196_MessageByMember1 949 |
| C2 | `MessageByMember (196 x2, 197 x7)` | one record more | Area196_MessageByMember0 1322, Area196_MessageByMember1 1385 |
| C3 | `MessageByMember (196 x2, 197 x7)` | request 3 | Area196_MessageByMember0 3800, Area196_MessageByMember1 3847 |
| C4 | `MessageByMember (196 x2, 197 x7)` | next message | Area196_MessageByMember0 3800, Area196_MessageByMember1 3847 |
| C5 | `MessageByMember (196 x2, 197 x7)` | messages a byte on | Area196_MessageByMember0 3800, Area196_MessageByMember1 3847 |
| C6 | `MessageByMember (196 x2, 197 x7)` | byte +0x8A | Area196_MessageByMember0 3815, Area196_MessageByMember1 3838 |
| C7 | `MessageByMember (196 x2, 197 x7)` | goes on after a match | Area196_MessageByMember0 1387, Area196_MessageByMember1 1373 |
| C8 | `Area196_MessageByMember0` | handler 1's tables | Area196_MessageByMember0 3800 |
| C9 | `Area196_MessageByMember1` | handler 0's tables | Area196_MessageByMember1 3847 |
| C10 | `Area196_SetCondFE` | FE 2 | Area196_SetCondFE 6000 |
| C11 | `Area197_MessageByMember0` | handler 1's tables | Area197_MessageByMember0 3911 |
| C12 | `Area197_MessageByMember1` | handler 2's tables | Area197_MessageByMember1 3840 |
| C13 | `Area197_MessageByMember2` | handler 3's tables | Area197_MessageByMember2 3889 |
| C14 | `Area197_MessageByMember3` | handler 4's tables | Area197_MessageByMember3 3905 |
| C15 | `Area197_MessageByMember4` | handler 5's tables | Area197_MessageByMember4 3919 |
| C16 | `Area197_MessageByMember5` | handler 6's tables | Area197_MessageByMember5 3807 |
| C17 | `Area197_MessageByMember6` | handler 5's tables | Area197_MessageByMember6 3898 |
| D1 | `Area197_RunShake` | state ^ 1 | Area197_RunShake 6000 |
| D2 | `StateEntry (197, 198)` | entry ^ 1 | Area197_RunShake 6000 |
| D3 | `Area197_ShakeStart` | count 3 | Area197_ShakeStart 6000 |
| D4 | `Area197_ShakeStart` | state 2 | Area197_ShakeStart 6000 |
| D5 | `Area197_ShakeStart` | script - 1 | Area197_ShakeStart 6000 |
| D6 | `Area197_ShakeStep` | ends at state 1 | Area197_ShakeStep 851 |
| D7 | `ShakeXZ (197, 198)` | x shift 10 | Area197_ShakeStep 2588 |
| D8 | `ShakeXZ (197, 198)` | z the same sign | Area197_ShakeStep 2588 |
| D9 | `ShakeStep (197, 198)` | step phase + 1 | Area197_ShakeStep 5149 |
| D10 | `ShakeStep (197, 198)` | nibble & 7 (a period-4 table) | not refused: equivalent - both shake step tables repeat every 4 bytes, so a nibble `& 7` picks the same step as `& 0xF`; variant D10b refused |
| D11 | `Area197_ShakeStep` | count - 2 | Area197_ShakeStep 5149 |
| D12 | `Area197_ShakeStep` | word +0x8C | Area197_ShakeStep 5149 |
| D13 | `ShakeXZ (197, 198)` | object not read again | not refused: equivalent - `Sprite_Current` is read again with no call between, and the only store between (the object's `+0x34`) cannot reach the pointer cell; variant D13b refused |
| D14 | `Area197_SpawnEffect92` | kind 0x93 | Area197_SpawnEffect92 4059 |
| D15 | `Area197_SpawnEffect92` | +0x30 0xFFE3 | Area197_SpawnEffect92 4059 |
| D16 | `Area197_SpawnEffect92` | +0x29 7 | Area197_SpawnEffect92 4059 |
| D17 | `Area197_SpawnEffect92` | +0xC | Area197_SpawnEffect92 4059 |
| D18 | `Area197_SpawnEffect92` | +0x2E 1 | Area197_SpawnEffect92 4059 |
| D19 | `MemberIndex (197, 198)` | floor division | Area197_SpawnEffect92 85 |
| D20 | `MemberIndex (197, 198)` | stride 0xA0 | Area197_SpawnEffect92 125 |
| D21 | `Area197_SpawnEffect92` | member read before the search | Area197_SpawnEffect92 1748 |
| D22 | `Area197_SpawnEffect92` | +0 2 | Area197_SpawnEffect92 4059 |
| D23 | `Area197_Tail51` | drop-in 1 | Area197_Tail51 879 |
| D24 | `Area197_Tail51` | state 2 | Area197_Tail51 879 |
| D25 | `Area197_Tail51` | counter 0x23 | Area197_Tail51 540 |
| D26 | `Area197_Tail51` | flag 0x56 | Area197_Tail51 351 |
| D27 | `Area197_Tail51` | area 0xAB | Area197_Tail51 351 |
| D28 | `Area197_Tail51` | flags 0x83 | Area197_Tail51 351 |
| D29 | `Area197_Tail51` | state 0x1F | Area197_Tail51 351 |
| D30 | `Area197_Tail51` | state 2 drops in | Area197_Tail51 446 |
| D31 | `Area197_StepHook` | FD 3 | Area197_StepHook 562 |
| D32 | `Area197_StepHook` | z 0x738001 | Area197_StepHook 105 |
| D33 | `Area197_StepHook` | x span 2 | Area197_StepHook 73 |
| D34 | `Area197_StepHook` | no x 0x58000 | Area197_StepHook 81 |
| D35 | `Area197_StepHook` | z span 2 | Area197_StepHook 60 |
| D36 | `Area197_StepHook` | x 0xD8001 | Area197_StepHook 59 |
| D37 | `Area197_StepHook` | key row span 3 | Area197_StepHook 24 |
| D38 | `Area197_StepHook` | key item 0xE | Area197_StepHook 59 |
| D39 | `Area197_StepHook` | key item 0xC | Area197_StepHook 380 |
| D40 | `Area197_StepHook` | state 1 | Area197_StepHook 244 |
| D41 | `Area197_StepHook` | step 0x15 | Area197_StepHook 159 |
| D42 | `Area197_StepHook` | Var7 6 | Area197_StepHook 159 |
| D43 | `Area197_StepHook` | x span as an int | Area197_StepHook 33 |
| D44 | `Area197_StepHook` | x shift 15 | Area197_StepHook 247 |
| D45 | `Area197_StepHook` | answers 2 | Area197_StepHook 159 |
| E1 | `Area198_StartSlotScript0` | handler 11's script | Area198_StartSlotScript0 6000 |
| E2 | `Area198_StartSlotScript0` | +0x2A 2 | Area198_StartSlotScript0 6000 |
| E3 | `Area198_StartSlotScript0` | animation 1 | Area198_StartSlotScript0 6000 |
| E4 | `ReleaseSlots (198)` | object + 1 | Area198_StartSlotScript0 6000, Area198_ReleaseOnRequest5 1213, Area198_StartSlotScript11 6000 |
| E5 | `StartSlot (198)` | script + 4 | Area198_StartSlotScript0 6000, Area198_StartSlotScript11 6000 |
| E6 | `Area198_SinkFade` | sinks 0xF | Area198_SinkFade 6000 |
| E7 | `Area198_SinkFade` | up to 0x80 | Area198_SinkFade 395 |
| E8 | `Area198_SinkFade` | every eighth | Area198_SinkFade 1224 |
| E9 | `Area198_SinkFade` | bits 3..4 | Area198_SinkFade 914 |
| E10 | `Area198_SinkFade` | start - 1 | Area198_SinkFade 2064 |
| E11 | `Area198_SinkFade` | script object not read again | Area198_SinkFade 977 |
| E12 | `Area198_SinkFade` | +2 by 2 | Area198_SinkFade 4064 |
| E13 | `Area198_SinkFade` | fade from -0x27F | Area198_SinkFade 312 |
| E14 | `Area198_SinkFade` | floor 0x7F | Area198_SinkFade 1812 |
| E15 | `Area198_SinkFade` | two channels | Area198_SinkFade 3318 |
| E16 | `Area198_SinkFade` | counter at 0xF610 | Area198_SinkFade 311 |
| E17 | `Area198_SinkFade` | counter + 2 | Area198_SinkFade 311 |
| E18 | `Area198_SinkFade` | at 0xF380 too | Area198_SinkFade 350 |
| E19 | `Area198_SinkFade` | script - 3 | Area198_SinkFade 4064 |
| E20 | `Area198_SinkFade` | +2 set 1 | Area198_SinkFade 1936 |
| E21 | `Area198_SinkFade` | unsigned compare | Area198_SinkFade 1667 |
| E22 | `Area198_SinkFade` | fade by 2 | Area198_SinkFade 3660 |
| E23 | `Area198_SpawnEffectsA6` | records 0, 1, 2 | Area198_SpawnEffectsA6 4778 |
| E24 | `Area198_SpawnEffectsA6` | kind 0xA5 | Area198_SpawnEffectsA6 5794 |
| E25 | `Area198_SpawnEffectsA6` | +8 | Area198_SpawnEffectsA6 5794 |
| E26 | `Area198_SpawnEffectsA6` | two effects | Area198_SpawnEffectsA6 6000 |
| E27 | `Area198_SpawnEffectsA6` | stops at none | Area198_SpawnEffectsA6 3376 |
| E28 | `Area198_SpawnEffectsA6` | +0 3 | Area198_SpawnEffectsA6 5794 |
| E29 | `Area198_Shake` | x and z steps phase + 1 | Area198_Shake 5620 |
| E30 | `Area198_Shake` | height shift 3 | Area198_Shake 3015 |
| E31 | `Area198_Shake` | count - 2 | Area198_Shake 5982 |
| E32 | `Area198_Shake` | script - 4 | Area198_Shake 5996 |
| E33 | `Area198_Shake` | kind 0xAB | Area198_Shake 2035 |
| E34 | `Area198_Shake` | counter 1 left 1 | Area198_Shake 3019 |
| E35 | `Area198_Shake` | +0x5F 0xC1 | Area198_Shake 3019 |
| E36 | `Area198_Shake` | +0x5C | Area198_Shake 3019 |
| E37 | `Area198_Shake` | brightens by 4 | Area198_Shake 2963 |
| E38 | `Area198_Shake` | skips 8 | Area198_Shake 1958 |
| E39 | `Area198_Shake` | none 0xFE | Area198_Shake 984 |
| E40 | `Area198_Shake` | height the other way | Area198_Shake 3015 |
| E41 | `Area198_WalkToX190` | shift 14 | Area198_WalkToX190 4412 |
| E42 | `Area198_WalkToX190` | target 0x198000 | Area198_WalkToX190 6000 |
| E43 | `Area198_WalkToX190` | no absolute value | Area198_WalkToX190 2656 |
| E44 | `Area198_WalkToX190` | direction 4 | Area198_WalkToX190 4812 |
| E45 | `Area198_WalkToX190` | byte +7 | Area198_WalkToX190 4799 |
| E46 | `Area198_WalkToX190` | z for x | Area198_WalkToX190 5984 |
| E47 | `Area198_ReleaseOnRequest5` | request 4 | Area198_ReleaseOnRequest5 1826 |
| E48 | `Area198_ReleaseOnRequest5` | script - 1 | Area198_ReleaseOnRequest5 4787 |
| E49 | `Area198_ReleaseOnRequest5` | steps back after the release too | Area198_ReleaseOnRequest5 1213 |
| E50 | `Area198_SpawnDrops` | every fourth frame | Area198_SpawnDrops 214 |
| E51 | `Area198_SpawnDrops` | counter above 1 | Area198_SpawnDrops 412 |
| E52 | `Area198_SpawnDrops` | sound 0x203 | Area198_SpawnDrops 1931 |
| E53 | `Area198_SpawnDrops` | bits 0..1 | Area198_SpawnDrops 213 |
| E54 | `Area198_SpawnDrops` | second above 4 | Area198_SpawnDrops 794 |
| E55 | `Area198_SpawnDrops` | script - 1 | Area198_SpawnDrops 6000 |
| E56 | `Area198_SpawnDrop` | +0x14 9 | Area198_SpawnDrop 3951 |
| E57 | `Area198_SpawnDrop` | +9 0x11 | Area198_SpawnDrop 3868 |
| E58 | `Area198_SpawnDrop` | height - 5 | Area198_SpawnDrop 3951 |
| E59 | `Area198_SpawnDrop` | +0x2B 2 | Area198_SpawnDrop 3951 |
| E60 | `Area198_SpawnDrop` | a mod 13 | Area198_SpawnDrop 3725 |
| E61 | `Area198_SpawnDrop` | b + 0xC | Area198_SpawnDrop 3952 |
| E62 | `Area198_SpawnDrop` | or | Area198_SpawnDrop 3076 |
| E63 | `Area198_SpawnDrop` | a below 0x19 | Area198_SpawnDrop 154 |
| E64 | `Area198_SpawnDrop` | x shift 15 | Area198_SpawnDrop 3952 |
| E65 | `Area198_SpawnDrop` | z from a | Area198_SpawnDrop 3830 |
| E66 | `Area198_SpawnDrop` | +0xB by 2 | Area198_SpawnDrop 6000 |
| E67 | `Area198_SpawnDrop` | not put back | Area198_SpawnDrop 2136 |
| E68 | `Area198_SpawnDrop` | index + 1 | Area198_SpawnDrop 5979 |
| E69 | `Area198_SpawnDrop` | op + 1 | Area198_SpawnDrop 3952 |
| E70 | `Area198_SpawnDrop` | +0x10 1 | Area198_SpawnDrop 3952 |
| E71 | `Area198_SpawnDrop` | b below 0x16 | Area198_SpawnDrop 149 |
| E72 | `Area198_SpawnEffectA7` | kind 0xA8 | Area198_SpawnEffectA7 4055 |
| E73 | `Area198_SpawnEffectA7` | x from z | Area198_SpawnEffectA7 4055 |
| E74 | `Area198_SpawnEffectA7` | y + 0x100000 | Area198_SpawnEffectA7 4055 |
| E75 | `Area198_SpawnEffectA7` | script - 1 | Area198_SpawnEffectA7 1945 |
| E76 | `Area198_SpawnEffectA7` | kept at +0xC | Area198_SpawnEffectA7 6000 |
| E77 | `Area198_SpawnEffectA7` | +0 2 | Area198_SpawnEffectA7 4055 |
| E78 | `Area198_StartSlotScript11` | bit 6 | Area198_StartSlotScript11 4535 |
| E79 | `Area198_StartSlotScript11` | handler 0's script | Area198_StartSlotScript11 6000 |
| E80 | `Area198_StartSlotScript11` | +0x2B | Area198_StartSlotScript11 6000 |
| E81 | `Area198_StartSlotScript11` | animation 2 | Area198_StartSlotScript11 6000 |
| E82 | `Area198_EffectA6Run` | state ^ 1 | Area198_EffectA6Run 6000 |
| E83 | `Area198_EffectA6Start` | x from z | Area198_EffectA6Start 4366 |
| E84 | `Area198_EffectA6Start` | y of object +6 | Area198_EffectA6Start 5449 |
| E85 | `Area198_EffectA6Start` | bank from +2 | Area198_EffectA6Start 5941 |
| E86 | `Area198_EffectA6Start` | +0x48 2 | Area198_EffectA6Start 6000 |
| E87 | `Area198_EffectA6Start` | +0x24 1 | Area198_EffectA6Start 6000 |
| E88 | `Area198_EffectA6Start` | +0x2A 1 | Area198_EffectA6Start 6000 |
| E89 | `Area198_EffectA6Start` | animation from +7 | Area198_EffectA6Start 3121 |
| E90 | `Area198_EffectA6Start` | state 2 | Area198_EffectA6Start 5967 |
| E91 | `Area198_EffectA6Start` | no state 1 | Area198_EffectA6Start 6000 |
| E92 | `A6Record (198)` | records of 4 | Area198_EffectA6Start 4710, Area198_EffectA6Follow 5664 |
| E93 | `ObjectAt (198)` | stride 0xA0 | Area198_EffectA6Start 4244, Area198_EffectA6Follow 4306 |
| E94 | `Area198_EffectA6Follow` | +0 from +1 | Area198_EffectA6Follow 4357 |
| E95 | `Area198_EffectA6Follow` | calls swapped | Area198_EffectA6Follow 6000 |
| E96 | `Area198_EffectA6Follow` | +0x5C from +0x5D | Area198_EffectA6Follow 4407 |
| E97 | `Area198_EffectA6Follow` | +0x27 from +0x26 | Area198_EffectA6Follow 4395 |
| E98 | `Area198_EffectA6Follow` | +0x48 from +0x49 | Area198_EffectA6Follow 4397 |
| E99 | `Area198_EffectA6Follow` | +0x44 from +0x40 | Area198_EffectA6Follow 4421 |
| E100 | `Area198_EffectA6Follow` | +0x32 from +0x30 | Area198_EffectA6Follow 4421 |
| E101 | `Area198_EffectA6Follow` | +0x60 from +0x64 | Area198_EffectA6Follow 4421 |
| E102 | `Area198_EffectA6Follow` | +0x29 7 | Area198_EffectA6Follow 6000 |
| E103 | `Area198_EffectA6Follow` | +0x48 exactly 1 | Area198_EffectA6Follow 2253 |
| E104 | `Area198_EffectA6Follow` | x scale from +0x44 | Area198_EffectA6Follow 2264 |
| E105 | `Area198_EffectA6Follow` | sign negated | Area198_EffectA6Follow 2264 |
| E106 | `Area198_EffectA6Follow` | x into +0x30 | Area198_EffectA6Follow 2264 |
| E107 | `Area198_EffectA6Follow` | z by the x scale | Area198_EffectA6Follow 2022 |
| E108 | `Area198_EffectA6Follow` | object read after the call | Area198_EffectA6Follow 82 |
| E109 | `Area198_EffectA6Follow` | x shift 15 | Area198_EffectA6Follow 3180 |
| E110 | `Area198_EffectA6Follow` | sign 0x8000 | Area198_EffectA6Follow 3736 |
| E111 | `Area198_EffectA6Follow` | scale unsigned | Area198_EffectA6Follow 1007 |
| E112 | `RoundHigh (198)` | arguments swapped | Area198_EffectA6Follow 6000 |
| E113 | `Area198_EffectA6Follow` | the next object | Area198_EffectA6Follow 4443 |
| E114 | `Area198_EffectA6Follow` | +0x2E + 1 | Area198_EffectA6Follow 5928 |
| F1 | `Area199_ChoiceStepIfAnswer` | answer 1 only | Area199_ChoiceStepIfAnswer 4354 |
| F2 | `Area199_ChoiceStepIfAnswer` | step 2 | Area199_ChoiceStepIfAnswer 5111 |
| F3 | `Area199_ChoiceStepIfAnswer` | message 0xFFFE | Area199_ChoiceStepIfAnswer 6000 |
| A7b | `FocusPair (192, 193)` | second answer + 1 (A7 variant) | Area192_ChoiceFocusPair 2851 |
| D10b | `ShakeStep (197, 198)` | nibble & 6 (D10 variant) | Area197_ShakeStep 2561 |
| D13b | `ShakeXZ (197, 198)` | z from x (D13 variant) | Area197_ShakeStep 5149 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (24): `Area192_FocusPairs`
`0x647F5C` (12 bytes), `Area192_TalkMessages` `0x647F68` (48),
`Area192_TalkMessagesF` `0x647F98` (48), `Area192_TalkWho` `0x647FC8` (5),
`Area192_TalkSteps` `0x647FD0` (9), `Area192_TalkMessagesB` `0x647FE0` (48),
`Area192_TalkStepsB` `0x648010` (9), `Area192_PlaceOps` `0x648158` (3
pointers to blocks of six `0x11`-byte ops), `Area193_Messages0` / `1` / `2`
`0x64894C` / `0x648958` / `0x648960` (6, 4, 4 words), `Area193_FocusPairs`
`0x648968` (8 bytes), `Area196_Keys0` / `1` `0x6490EC` / `0x6490F8` (12 bytes
each: four keys, four message words), `Area197_Keys` `0x6497C4` (seven of 12),
`Area197_ShakeStates` `0x649818` (2), `Area197_ShakeSteps` `0x649820` (16),
`Area198_SlotScript0` / `11` `0x649D4C` / `0x649D90` (lengths not measured),
`Area198_SinkAnims` `0x649DD4` (4), `Area198_ShakeSteps` `0x649DD8` (16),
`Area198_DropOp` `0x649DE8` (17), `Area198_EffectA6Records` `0x649E00` (3 of
8), `Area198_EffectA6States` `0x649E18` (2). The counts of the answer-indexed
tables are the distance to the next table; the choices' row counts are the
message box's, not read here. Both shake step tables have period 4 (their
nibble index `& 0xF` could be `& 3`), and the two are byte-identical. The
descriptors' own `+0x34` / `+0x3C` arrays are not named here (read in place;
area 192's and 193's choice arrays run into their handler arrays, as in
worlds 2 and 3).

## 6. Latent defects

Described, not fixed:

- **Unchecked state dispatch.** `Area197_RunShake` (by the object's `+4`)
  and `Area198_EffectA6Run` (by the record's `+1`) jump through two-entry
  tables; an index of 2 or more jumps through the dwords after the table
  (the shake steps; `0x00170809` and on - not code). Ours aborts with a
  message there (the owner's rule: no DIVERGENCE entry). The shake's own
  states store 1 and 0 into `+4`, and effect kind `0xA6`'s store 1.
- **Area 193's choice 4 reads past its stack table** for an answer outside
  0..2: the row is `[esp + answer * 4]`, so a negative answer reads below
  the stack pointer and 3 reads the return address's low word as the
  message. Ours aborts. The message box's rows for this choice are not read
  here.
- **Area 198's handler 9 always plays sound 0.** Its argument is computed by
  `and eax, 8; add eax, 0x203; neg eax; sbb eax, eax; inc eax` - the value
  before `neg` is never 0, so the result is always 0. What the PSX twin
  (`0x801F35FC`) passes, and what `Sound_PlayEffect(0)` does on the PC, are
  not read here; for the owner's attention.
- **Tables read past their ends, kept.** Area 192's talk index for a who
  that is none of the five (i = 5, index 45 and more) reads on into the next
  table (the flag-2 table into the ids, the other into the flag-2 table);
  the choices' tables are indexed by any s8 answer; the character record
  `0x66972C` maps an id to is unchecked (the level byte of a record past the
  eight); effect kind `0xA6`'s object `+7` and record `+6` are unchecked
  bytes. All stay inside `.data` / `.bss`; ours reads the same.
- **The restore copies up to `Field_MemberCount` records** (not bounded by
  three) into the party's `+0x80` blocks. Ours does the same.
- **Effect slots unchecked.** Every spawn writes `Effect_Objects + slot << 7`
  for any slot but `0xFF`; `Effect_FindFree` answers only 0..19 or `0xFF`.
- **The drop's cell loop** draws again while a < `0x1A` and b < `0x15`; with
  the CRT's `rand` (0..`0x7FFF`) it ends with probability about 0.7 a draw.

## 7. What the tool listed, against the reading

- **Every row agrees** with the reading but one: 48 starts, extents, call
  sites, the two in-function jump tables (`0x42BE10`: 8 entries at `+0x1AC`
  through a byte table of 31 at `+0x1CC`; `0x42C500`: 13 at `+0x1CC`), and
  the shape each root gives.
- **Added: `0x42D580`.** The tool's `0x42D4D0` (`0x21B`) is two functions:
  state 0 ends in `jmp 0x42D580` with a displacement of 0 (the next byte), so
  the descent fell through into state 1; `Area198_EffectA6States`
  (`0x649E18`) names both. 49 functions, not 48.
- **Dropped:** none. Every gap between the band's functions is padding.
- **Gaps of the tool** (reached by no area table in its walk), each read to
  its root: `0x42C0A0` (called by chapters 14 and 15), `0x42C1C0` (called by
  it), `0x42C350` (`EffectKind18_States[78]`), `0x42D4B0`
  (`Effect_KindHandlers[0xA6]`, the kind area 198's handler 5 spawns). Each
  sits in the block of the area named.
- **Areas by data position:** the tool files effect kind `0xA6`'s states
  (`0x42D4D0`, `0x42D580`) under area 199 because their table follows area
  198's descriptor (`0x649CC0`); the kind is area 198's (its handler 5 spawns
  it). No area 199 code but the choice.
- **Register-armed tail kinds:** none; kinds 51, 54, 57 are armed by
  immediates (the step hooks, area 192's choice 5 and area 193's init).
- **The band's end:** `0x42D710` is BATE's dispatcher (above), not area code.

## 8. What reaches it

- **No recorded route reaches the band** (`area_funcs.tsv`'s live column is
  empty for all 48 listed). Every function is reached only in play: the
  choices when the area's message box asks, the handlers from its movement
  scripts, the step hooks on a step in the area, the tails once armed, the
  inits on entry, the talk from chapters 14 and 15's objects in area `0xC0`,
  effect kind `0xA6` and `EffectKind18_States[78]` while such an effect
  lives.

## 9. Calls across groups

- **Raw addresses nobody owns**, in `area_w4f_callees.h`: `0x454A80` and
  `0x455290` (the `Field_Slots` release and start, engine; AR2B called them
  raw too) and `0x441090` (a 16.16 round-up helper, engine; new this wave).
- **By name, Capcom's:** `Sound_StopMusic`, `MoveCmd_Move`, `Rand`.
- **By name, ours:** `ScriptFlags_Set40` / `Clear40`, `Transition_Start`,
  `Sound_LoadStream`, `Sound_StreamDone`, `Sound_PlayEffect`, `Music_Play`,
  `Flags_Set` / `Clear` / `Test`, `KeyItem_Has`, `Field_ChangeArea`,
  `AreaMap_ByteAt`, `Sprite_FindFree`, `EventOp_0x`, `Effect_FindFree`,
  `Effect_Release`, `Party_HealJoined`, `Party_DropIn`, `Msg_OpenScript`,
  `Inventory_Count`, `Inventory_Add`, `Sprite_SetAnimation`,
  `Sprite_SetAnimationAt`, `Sprite_SetAnimationBank`, `Sprite_ScriptTick`,
  `Sprite_QueueOverlay`. No harness edit; no `AH_THEIRS` moved.
- **Table entries of other groups', read in place:** `Area143_SkipIfLeader89Is7`
  (AR3F) is area 197's handler 9; `Area85_ReleaseSlots` (AR2B) area 198's
  handler 3; `0x428AB0` / `0x428B10` (AR4C's band) area 198's handlers 1 and
  2; `0x42B550`, `0x42BD30`, `0x42B5B0`, `0x42B610` (AR4E's band) area 192's
  choices 0, 1, 3 and 6.
- **Inbound, for the rebinding pass:**
  - `0x42C0A0` (`Area192_TalkMessage`) by raw address from
    `scena_sc13_callees.h` (`kArea192`: chapter 14's objects 0..4, `0x5677DD`,
    `0x56781D`, `0x56785D`, `0x56789D`, `0x5678DD`) and
    `scena_sc15_callees.h` (`kTalkIdC0`: chapter 15's talk, `0x56AEC6`).
  - `0x42C2D0` (`Area192_RestoreCharacters`) from area 191's code at
    `0x42B864` (AR4E's band this wave).
  - `Area_StepHook` (`0x56E050`, ours in `event_ops.cpp`) calls `0x42C000`,
    `0x42C700`, `0x42CE30` (`0x56E2C2`, `0x56E2D2`, `0x56E2E2`) through its
    raw `kStepHandlers` table, now `Area192_StepHook`, `Area193_StepHook`,
    `Area197_StepHook`.
  - Read in place by tables, no caller to rebind: `Field_ModeTailKinds` 51,
    54, 57; `Effect_KindHandlers[0xA6]`; `EffectKind18_States[78]`; the
    choice and handler tables of areas 148, 167, 173 (`0x42C8A0`), 174
    (`0x42D250`) and 191 (`0x42BDA0`).
- `analysis/calltrace/entries_logic.txt`: 47 lines appended under a
  `# group AR4F` comment (two of the 49 were listed already at the same
  extent, `0042C000 95` and `0042C0A0 11B`); the hosts `0042C1C0 104`,
  `0042C2D0 B5A`, `0042CE30 4A3`, `0042D2E0 429` ran over the band (the
  consolidation keeps the smaller).
