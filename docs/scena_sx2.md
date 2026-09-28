# The engine callees nobody owned after SX (group SX2)

**Status:** IN PROGRESS (2026-09-28) - thirteen functions ours
(`src/game/scena_sx2.cpp`, shadow name `scena_sx2`): the round's twelve
addresses and one found beside them (`0x57C650`, section 1), fuzzed
headless through the scenario harness
([`scenario_harness.md`](scenario_harness.md)): **0 mismatches** in
39,000 rounds; **56 of 56 negative controls refused** (54 by a count, two by a fault with their near variants refused by a count; section 5). `BOF3X_SHADOW='*'` exit 0.
Fuzz only: no route is recorded through these.

Group SX2 of round ten's fourth wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) §10, §12): the
engine functions the chapter and area groups still called by raw address
after group SX ([`scena_sx.md`](scena_sx.md), the model) because nobody
owned them. **Harness:** the scenario harness, as SX's - every one of the
thirteen is a direct cdecl callee (`Shape::kEntry`), and what they touch
(the story flags, the party's field objects, the camera words, the effect
records, the sprite records) is in its standard regions; the area block,
the sound tables, the light angles, the key items and the battle bytes are
the group's own regions (section 4). Area code calls six of them; the
fuzz does not need the area harness's frame for any.

## 1. The addresses, read

Each read to its last instruction (capstone; `magic_rows.descend` /
`clone_sites` give the extents, every jump internal, no jump table,
nothing refused), and each caller found by an `E8` / `E9` scan of `.text`
and a dword scan of the image (no dword anywhere holds any of the
thirteen: none is in a table).

| Address | Bytes | Sites | What it is | Taken |
|---|--:|--:|---|---|
| `0x469FE0` | 0x3C | 7 | an effect record of kind 4 that holds story flag 0x1C for n frames | `Effect_HoldFlag1C` |
| `0x532FD0` | 0x140 | 1 | `Sprite_Current` at the formation's offset for a slot, grounded | `Party_PlaceInFormation` |
| `0x572620` | 0x2F | 38 | one byte of the area block's height layer | `AreaMap_SetHeight` |
| `0x57C160` | 0x1F | 19 | `bits[i >> 3] ^= 1 << (i & 7)` (PSX `Flag_Toggle`) | `Flags_Toggle` |
| `0x57C5A0` | 0x5E | 1 | `Camera_Angles[0]` a step toward an angle, `Light_AnglesCopy[0]` at the end | `Camera_TurnStep` |
| `0x57C600` | 0x47 | 2 | `0x57C650` with the angle and step in degrees | `Camera_TurnFBToDegrees` |
| `0x57C650` | 0x5E | 1 | `Cond_AngleFB` a step toward an angle, `Light_AnglesCopy[2]` at the end (**not in the round's twelve**) | `Camera_TurnStepFB` |
| `0x57C8A0` | 0x35 | 6 | a party member's state 2, sub-state 8, `+3` 0, `+0xB` a value | `Member_SetState2_8` |
| `0x587860` | 0x28 | 4 | every `Sound_Channels` buffer stopped and forgotten | `Sound_StopChannels` |
| `0x587890` | 0x63 | 2 | the volume of each voice of a cue | `Sound_SetCueVolume` |
| `0x591920` | 0x20 | 1 | a key item out of the 32-byte list | `KeyItem_Remove` |
| `0x591EC0` | 0x64 | 1 | the ability list an id's type files it in (PSX `AbilityList_ForType`) | `AbilityList_ForType` |
| `0x5A7730` | 0x10 | 3 | a primitive's code 0x7C (`SPRT_16`) and depth 0.01 (PSX `SetSprt16`) | `Gpu_SetSprt16` |

The three sibling names (`Flag_Toggle` 0x8015BFE4, `AbilityList_ForType`
0x80167514, `SetSprt16` 0x8017B380) are confirmed in the sibling's
`symbols.toml` and taken because the PC reading says the same thing
(`Flags_` and `Gpu_` are this repo's prefixes for their families). The
other PSX twins `pairs_propagated.json` offers (`0x469FE0` 0x8019BBB8
"gap74", `0x572620` 0x80155AF4, `0x57C8A0` 0x8015CC04, `0x587860`
0x8015D8AC, `0x591920` 0x80166B54 "gap9") are cited in the evidence as
pairings, not read.

**`0x57C650` is taken though the round did not list it.** It is
`0x57C600`'s one callee (its only `E8` site is `0x57C63E`), nobody's,
and `0x57C5A0`'s twin instruction for instruction but for three
constants (the angle word `Cond_AngleFB` for `Camera_Angles[0]`, the
light word `0x90359C` / `0x7E0684` for `0x903598` / `0x7E0680`, the bias
`-0x200` for `+0x2AA`) - the pair SX's section 1 named "with `0x57C600` /
`0x57C650`, its twins for `Cond_AngleFB`". Leaving it raw would have left
`0x57C600` calling a nobody's function across its only call. It lies in
no area band.

**Nothing declined.** Each of the thirteen is a function of its own: its
own entry, reached only by direct `E8` calls, ending in its own `ret`; none
is a thunk (as SX's declined `0x587B80` is) or a case of a larger body.
The two the brief asked to decide:

- **`0x587860` is not part of `Sound_PauseAll` `0x587C30`.** The two walk
  the same 23 `Sound_Channels` dwords with `SndBuf_Stop`, but `0x587860`
  clears each dword after stopping it and returns; `Sound_PauseAll` keeps
  the dwords (a pause, resumed later), then stops the stream
  (`SndStream_Stop` when `0x6BDE40` is set) and tail-jumps to the music's
  stop `0x5A6FF0` - SX's declined thunk layer. `Sound_PauseAll` stays the
  sound module's (named, no `impl`); `0x587860` is taken as
  `Sound_StopChannels`, every one of its four callers restarting the game
  to `Boot_Task` next.
- **`0x57C5A0` reads the light angles but is its own function.** It reads
  `Light_Angles[0]` as a dword and writes `Light_AnglesCopy[0]` only when
  the turn ends (the light direction follows the camera's angle);
  `Light_ObjectDirection`, the light module's reader of both, is not
  called and not touched.

**Read beside them and left:** `0x469FB0` (effect kind 4's handler, 0x27
bytes: `+9` counted down a frame; at 0, `Flags_Clear(0x904030, 0x1C)` and
a tail jmp to `Effect_Release`) - the other half of `Effect_HoldFlag1C`,
reached only through `Effect_KindHandlers[4]`, nobody's and not listed;
`0x5A6C60` (the DirectSound `SetVolume` wrapper `Sound_SetCueVolume`
calls: nothing for a null buffer, else the buffer's vtable `+0x3C` with
the level scaled by two `.rdata` floats through `__ftol` `0x5B9550`) -
the sound module's, called by address. Both are candidates for whoever
takes the effect kinds and the sound layer.

## 2. The functions

| Function | Address | Args | Answers |
|---|---|---|---|
| `Effect_HoldFlag1C` | `0x469FE0` | the frames (a byte) | nothing read |
| `Party_PlaceInFormation` | `0x532FD0` | x, z (16.16), the slot (a byte) | nothing read |
| `AreaMap_SetHeight` | `0x572620` | x, z (s16s), the value (a byte) | nothing read (eax the row offset) |
| `Flags_Toggle` | `0x57C160` | the bits, the index (a byte) | nothing read (eax the byte's address) |
| `Camera_TurnStep` | `0x57C5A0` | the angle (its low word, 0..0xFFF from its caller), the step (s8) | al 1 while turning |
| `Camera_TurnFBToDegrees` | `0x57C600` | the angle (s16 degrees), the step (s8 degrees) | al 1 while turning |
| `Camera_TurnStepFB` | `0x57C650` | as `Camera_TurnStep` | al 1 while turning |
| `Member_SetState2_8` | `0x57C8A0` | the member (a byte), the value (a byte) | nothing |
| `Sound_StopChannels` | `0x587860` | none | nothing |
| `Sound_SetCueVolume` | `0x587890` | the cue (a word), the level (a dword) | nothing |
| `KeyItem_Remove` | `0x591920` | the item (a byte) | al 1 removed, 0 not there |
| `AbilityList_ForType` | `0x591EC0` | the member, the id, working (bytes) | eax: a pointer to a 10-byte list |
| `Gpu_SetSprt16` | `0x5A7730` | the primitive | nothing read (eax the primitive) |

What each does (the `evidence` fields have the addresses):

- **`Effect_HoldFlag1C(n)`**: `Effect_FindFree`; with a record, `+0` 1,
  `+5` 4 (the kind), `+9` n, then `Flags_Set(0x904030, 0x1C)`; without,
  nothing. Kind 4's handler `0x469FB0` counts `+9` down each frame and at
  0 clears flag 0x1C and releases the record: the flag is held n frames.
  Every caller passes 0xF. `Area49_CellHook` answers 0 while the flag is
  set, so a switch cell cannot fire again for 15 frames.
- **`Party_PlaceInFormation(x, z, slot)`**: a group g from the byte
  `0x904060` (+1 at two members, +4 at three, byte arithmetic), a row of
  `Formation_SlotVectors` `0x6698B0` (two dwords dx, dz) at `g * 3 +
  slot`, and the formation's s8 pair (a, b) at `BattleFormation_Offsets`
  `0x660B1C` (by the byte `0x904AAC`), each divided by 4 truncating: with
  a not 0, the sprite at (x + dx a/4, z + dz a/4); else at (x - dz b/4, z
  + dx b/4), the row's vector turned a quarter. Then `+0x3E` its ground
  (`MapView_GroundAt`). The slot is a local in `push ecx`'s slot, only its
  low byte written and read: nothing of the caller's `ecx` leaks. The
  formation byte and the pair are read again for z.
- **`AreaMap_SetHeight(x, z, v)`**: `AreaMap_Header[(width byte) * z +
  AreaMap_HeightBase * 4 + x] = v` - the byte layer `AreaMap_Elevation`
  reads, one per cell. Area 52's block puzzle moves its blocks' heights
  with it; the engine's `0x46BF80` / `0x46C100` write 0x10.
- **`Flags_Toggle(bits, i)`**: the fourth of the flag family (`Flags_Set`
  / `Clear` / `Test`, `0x57C0F0..0x57C140`).
- **`Camera_TurnStep(a, s)`**: with s not 0, `Camera_Angles[0] += s`; while
  the new angle `& 0xFFF` is below a (s > 0) or above a (s < 0), compared
  as s16, al 1. Else (the angle reached or passed, or s 0) the word
  becomes a exactly and `Light_AnglesCopy[0] = Light_Angles[0] + a +
  0x2AA`, al 0. `Camera_TurnToDegrees` (SX) is its caller. The answer is al
  only: the rest of eax is the dword's bits, `Light_Angles`' bits, or on
  the turning-down path the caller's own eax - every caller (`Camera_
  TurnToDegrees`' three SC1 sites, `Camera_TurnFBToDegrees`' two SC2 sites)
  tests `al`.
- **`Camera_TurnFBToDegrees(a, s)`**: SX's `Camera_TurnToDegrees` shape
  for `Cond_AngleFB`: both arguments x 4096 / 360 (truncated; the angle
  `& 0xFFF`), `Camera_TurnStepFB`. **`Camera_TurnStepFB`**: section 1.
- **`Member_SetState2_8(m, v)`**: `ObjTrio` record m: `+1` 2, `+2` 8, `+3`
  0, `+0xB` v - the state `Field_TileTurn` gives `Sprite_Current` on a
  turn cell ([`event-objs.md`](event-objs.md)), for a member by index.
- **`Sound_StopChannels`**: section 1.
- **`Sound_SetCueVolume(cue, level)`**: the cue as `Sound_PlayEffect`
  reads it (bank = bits 8..11 less 1, the 16-byte cue of four voice
  dwords); for each voice dword not 0 (read again each time),
  `0x5A6C60(the voice's buffer at the bank's +0x184 + (dword & 0xFF) * 8,
  the level)`. `Scena13_ToneLevels` sets cues 0x207 / 0x208 from a dial's
  angle.
- **`KeyItem_Remove(i)`**: `KeyItem_Add`'s opposite.
- **`AbilityList_ForType(m, id, working)`**: the saved record
  `CharacterRecords[m]` (working 0) or the member's working copy `ObjTrio`
  record m `+0x80`, plus `+0x60` / `+0x6A` / `+0x74` / `+0x7E` by the id's
  type byte `0x65C4D9 + 24 id` (`NameTable_Abilities + 1`, the record
  framing that table's note describes) `& 3`. `Char_AbilityList`
  `0x591E50` (ours) is its sibling by list slot.
- **`Gpu_SetSprt16(p)`**: as `Gpu_SetSprt8` with code 0x7C.

`symbols.toml` gains two `[[data]]`: `BattleFormation_Offsets` `0x660B1C`
(0x20 bytes before `BattleFormation_Anims`; its length is not
established) and `Formation_SlotVectors` `0x6698B0`.

## 3. Who calls each (for the rebinding pass)

By `E8` site, grouped by owner: the containing function from
`pc_funcs.json` and `symbols.toml`, area sites by
`analysis/area_funcs.tsv`'s unit; wave four's bands from round doc §12.
Every merged group calls these through a raw address in its
`_callees.h`; after this merge each is ours at that address, so the raw
calls stay correct until the rebinding pass names them. **None is in a
harness's standard set** (`scenario_harness.cpp` / `area_harness.cpp`
`kStandard` list none of the thirteen), so no harness column moves.

| Function | Sites by owner |
|---|---|
| `Effect_HoldFlag1C` `0x469FE0` | AR1C `Area49_CellHook` (`0x409836`; `area_w1c_callees.h` `kSpawnKind4`); **AR2A** area 77's `0x40EBB0` (`0x40EC36`); **AR2B** area 86's `0x40FA90` (`0x40FB5D`); areas 112 (`0x4187AB`), 117 (`0x41A3A6`), 118 (`0x41A716`); nobody's engine `0x4FEEB0` (`0x4FEF2C`) |
| `Party_PlaceInFormation` `0x532FD0` | SX `Party_PlaceForBattle` (`0x532F58`; `scena_sx_callees.h` `kFormationPlace`) |
| `AreaMap_SetHeight` `0x572620` | AR1C area 52, 24 (`Area52_StashBlock` 2, `ShiftBlockZ` 4, `ResetBlock` 4, `ShiftBlockX` 4, `DropColumn` 2, `RestoreBlock` 2, `TailBlock` 4, `ShiftBlockXBack` 2; `area_w1c_callees.h` `kSetLayerByte`); areas 108 (`0x417531`, `0x417558`), 135 (`0x41ED49`, `0x41ED88`, `0x41EE12`, `0x41EE33`); nobody's engine `0x46BF80` 4 (`0x46BF92`, `0x46BFEC`, `0x46C049`, `0x46C0B3`), `0x46C100` 4 (`0x46C111`, `0x46C14B`, `0x46C18A`, `0x46C1D2`) |
| `Flags_Toggle` `0x57C160` | AR1B `Area44_SwitchHook` (`0x40759C`; `area_w1b_callees.h` `kFlagsToggle`); AR1C `Area48_ToggleFlagD` (`0x40906B`), `Area49_CellHook` (`0x40982F`), `Area52_CellHook` (`0x40A5A9`) (`area_w1c_callees.h` `kFlagsToggle`); **AR2A** area 77 (`0x40EC25`); **AR2B** area 86 (`0x40FB4C`); **AR2D** area 99's `0x413F50` (`0x413F58`); areas 108 (`0x417185`), 117 (`0x41A39F`), 118 (`0x41A70F`), 135 (`0x41E6F1`), 136 (`0x41F402`), 139 (`0x41F59C`), 148 (`0x4226CE`), 167 (`0x425EBB`, `0x426217`, `0x4264EC`), 188 (`0x42A6D1`); nobody's engine `0x482930` (`0x48294B`) |
| `Camera_TurnStep` `0x57C5A0` | SX `Camera_TurnToDegrees` (`0x57C58E`; `scena_sx_callees.h` `kCameraTurnYaw`) |
| `Camera_TurnFBToDegrees` `0x57C600` | SC2 `Scena02_Scene10` 2 (`0x53FFD8`, `0x54002E`; `scena_sc2_callees.h` `kTurnTest`) |
| `Camera_TurnStepFB` `0x57C650` | ours: `Camera_TurnFBToDegrees` (by name) |
| `Member_SetState2_8` `0x57C8A0` | AR1C `Area51_MemberAtObject` (`0x409D1C`; `area_w1c_callees.h` `kMemberSetState`); area 148 (`0x422B25`); nobody's engine `0x4703F0` (`0x470453`), `0x4712E0` (`0x47139E`), `0x4849A0` 2 (`0x484A43`, `0x484AF4`) |
| `Sound_StopChannels` `0x587860` | SC15 `Scena15_Run5` (`0x569B5A`), `Scena17_Outro11` (`0x56CB49`), `Scena17_EndRestart` (`0x56D4F6`) (`scena_sc15_callees.h` `kSoundStop`); nobody's engine `0x432750` (`0x4327D5`, the battle result's neighbourhood) |
| `Sound_SetCueVolume` `0x587890` | SC13 `Scena13_ToneLevels` 2 (`0x5633BC`, `0x5633E2`; `scena_sc13_callees.h` `kVoiceLevel`) |
| `KeyItem_Remove` `0x591920` | SC13 `Scena13_Run3` (`0x5629EE`; `scena_sc13_callees.h` `kFindByte`) |
| `AbilityList_ForType` `0x591EC0` | SX `AbilityList_Add` (`0x590CAE`; `scena_sx_callees.h` `kAbilityListOf`) |
| `Gpu_SetSprt16` `0x5A7730` | SC15 `Scena17_DrawGlyph` (`0x56D2D8`; `scena_sc15_callees.h` `kPrimSprt16`); nobody's engine `0x459720` (`0x459746`), `0x464E40` (`0x464E55`) |

The wave-four area groups beside SX2 (AR2A, AR2B, AR2D) call
`Effect_HoldFlag1C` and `Flags_Toggle` by raw address this wave, as the
brief says; the other area sites are in bands not yet staged.

## 4. The fuzz (`scena_sx2_fuzz.cpp`)

Through the scenario harness, all thirteen clones called with ten words,
3,000 rounds each, `g.chapter = 0` (none reads the chapter bytes or the
flag row). Clone table: `magic_rows.descend` / `clone_sites` over the
thirteen (a scratch driver), names given; no correction by hand.

- **Callees** the group lists, beyond the standard set's `Effect_FindFree`
  (a byte 0xFF..0x13) and `Flags_Set`: ours by name - `MapView_GroundAt`,
  `SndBuf_Stop`, `Camera_TurnStepFB` (for `Camera_TurnFBToDegrees`, a
  `kFlag` answer); nobody's by address - `0x5A6C60`. Every argument logged
  whole (the step `Camera_TurnFBToDegrees` passes as a dword included).
- **Stir**: each of the group's callees except `Camera_TurnStepFB` moves
  one cell after it has logged (from the recorders' stream): `Sprite_Current`
  (read again after `MapView_GroundAt`), a `Sound_Channels` dword (the walk
  reads each in turn), a cue's voice dword (read again each voice), the
  formation byte. The group's `disturb` moves the same set from the hash it
  is given.
- **Regions** beyond the harness's 22: `CharacterRecords` 0..7, the key
  items `0x904554` (0x20), the battle bytes `0x904AA0` (0xB0), the light
  angles and their copy (8 each), the area block `AreaMap_Header` (0x4000),
  `Sound_Channels` (0x5C), `Sound_Banks`' six records (0x1518), and a
  0x20-byte primitive of the fuzz's own: 31 regions, 33,440 bytes.
- **Seeds**, per function: `Party_PlaceInFormation` - the member count 0..4
  and any byte, `0x904060` 0..3, 0xFF, 0xFC (the byte wraps) and 0..7, the
  formation byte half the time one whose first offset is 0 (44 of the 256
  in the image's table) and half not (212), x and z 0, extremes, any; the
  slot 0..2, sometimes 3 or 4. `AreaMap_SetHeight` - a width 0..0x40 and
  a base 0..0x2FF planted in the header, z -0x20..0x40 (s16, negative
  rows kept inside the layer), x 0, 1, any byte, the layer's lowest cell,
  every write inside the 0x4000-byte region. `Flags_Toggle` - the bits at
  `0x904030` or a `Cond_Flags` row, any index. `Camera_TurnStep` /
  `Camera_TurnStepFB` - the target 0..0xFFF, 0, 0xFFF or any dword; the
  step 0, 1, 2, 22, 0x7F, 0x80, -1, -2, -22 or any byte; the angle word
  0..4 steps short of the target, plus -1..+4, with bits 12..15 garbage
  half the time. `Camera_TurnFBToDegrees` - angles -720..720, 0, 90, -40,
  any; steps 0, 2, -2, 11, 12, -11, 127, -128, any.
  `Member_SetState2_8` - members 0..2. `Sound_StopChannels` - half the 23
  dwords 0. `Sound_SetCueVolume` - banks 1..6 (0 and 7 now and then), cues
  0..23 or any byte, bits 12..15 garbage; voice dwords 0 half the time,
  else 0..63 or any byte, garbage above a time in eight.
  `KeyItem_Remove` - the item 0, 0xB or any, planted at slot 0, 0x1F or
  any two times in three (earlier slots holding it moved to item ^ 0x80),
  absent otherwise. `AbilityList_ForType` - any bytes, `working` 0 half the
  time. `Gpu_SetSprt16` - the fuzz's primitive.

**Result** (2026-09-28, in this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=scena_sx2`, exit 0): 39,000 rounds over 13 functions, 50,583
calls to the stand-ins, **0 mismatches**, 33,440 bytes (31 regions).
Coverage: `MapView_GroundAt` 3,000, `SndBuf_Stop` 34,532,
`Camera_TurnStepFB` 3,000, `0x5A6C60` 4,198, `Flags_Set` 2,853,
`Effect_FindFree` 3,000.

Under `BOF3X_SHADOW='*'` (every group of every harness, 2026-09-28, this
worktree): exit 0 on the first run, no group with a mismatch or a Fatal;
`scena_sx2` 39,000 rounds, 0 mismatches (50,605 calls: the count moves
with the other groups' state, as round nine recorded).

## 5. Controls

| # | Function | Mutant | Mismatches |
|---|---|---|--:|
| C1 | `Effect_HoldFlag1C` | the flag set with no record too | 147 (351) |
| C2 |  | kind 5 | 2853 (0) |
| C3 |  | frames - 1 | 2853 (0) |
| C4 |  | flag 0x1D | 2853 (0) |
| C5 |  | +0 0x81 | 2853 (0) |
| C6 | `Party_PlaceInFormation` | three members +3 | 436 (40) |
| C7 |  | two members tested as one | 592 (14) |
| C8 |  | a from the second byte | 1419 (1) |
| C9 |  | the quarter by an arithmetic shift | 66 (222) |
| C10 |  | z from dx | 836 (14) |
| C11 |  | the turned x added | 410 (1) |
| C12 |  | Sprite_Current not re-read after the call | 443 (53) |
| C13 |  | the slot & 3 | 126 (1) |
| C14 | `AreaMap_SetHeight` | the base x 2 | 2991 (2) |
| C15 |  | z unsigned | fault (exit 0xC0000005: the write lands far outside the layer) |
| C15b |  | a negative row taken as 0 | 841 (54) |
| C16 |  | x unsigned | 758 (41) |
| C17 |  | the width a word | fault (exit 0xC0000005: the write lands far outside the layer) |
| C17b |  | the width & 0x3F | 47 (1185) |
| C18 | `Flags_Toggle` | or for xor | 1535 (16) |
| C19 |  | index & 0x7F | 1546 (16) |
| C20 |  | bits numbered from the top | 3000 (3) |
| C21 | `Camera_TurnStep` | turning while equal (up) | 179 (277); 201 (500) in `Camera_TurnStepFB` |
| C22 |  | turning while equal (down) | 198 (160); 159 (331) in `Camera_TurnStepFB` |
| C23 |  | the step unsigned | 984 (56); 928 (6) in `Camera_TurnStepFB` |
| C24 |  | the angle & 0x7FF | 323 (69); 326 (214) in `Camera_TurnStepFB` |
| C25 |  | bias 0x2AB | 1751 (4) |
| C26 | `Camera_TurnStepFB` | bias -0x1FF | 1811 (6) |
| C27 |  | Light_Angles[0] for [2] | 1811 (6) |
| C28 | `Camera_TurnStep` | a negative step not taken | 643 (56); 618 (32) in `Camera_TurnStepFB` |
| C29 |  | the angle stored & 0xFFF | 295 (368); 318 (19) in `Camera_TurnStepFB` |
| C30 | `Camera_TurnFBToDegrees` | the step / 361 | 1635 (18) |
| C31 |  | the angle & 0x7FF | 1201 (5) |
| C32 |  | the step unsigned | 1124 (18) |
| C33 |  | the step passed as a byte | 1637 (18) |
| C34 | `Member_SetState2_8` | sub-state 7 | 3000 (7) |
| C35 |  | +4 for +3 | 3000 (7) |
| C36 |  | +0xA for +0xB | 3000 (7) |
| C37 |  | member & 1 | 1044 (7) |
| C38 | `Sound_StopChannels` | the dword kept | 3000 (8) |
| C39 |  | the last dword not walked | 1502 (34) |
| C40 |  | cleared before the call | 142 (47) |
| C41 | `Sound_SetCueVolume` | bank not less 1 | 2134 (9) |
| C42 |  | voice & 0x7F | 441 (87) |
| C43 |  | cue & 0x7F | 370 (126) |
| C44 |  | three voices | 1031 (9) |
| C45 |  | the level a word | 1677 (9) |
| C46 | `KeyItem_Remove` | 31 slots | 705 (75) |
| C47 |  | the slot 0xFF | 1996 (10) |
| C48 |  | compared on 7 bits | 355 (23) |
| C49 | `AbilityList_ForType` | types 2 and 3 swapped | 2418 (11) |
| C50 |  | stride 20 | 2370 (11) |
| C51 |  | working by bit 0 | 775 (11) |
| C52 |  | working records 0xA4 apart | 1541 (11) |
| C53 | `Gpu_SetSprt16` | code 0x74 | 3000 (12) |
| C54 |  | depth at +0x14 | 3000 (12) |

**56 planted, 56 refused**: 54 by a count, C15 and C17 by a fault. Both
make `AreaMap_SetHeight` write far outside the layer (a negative row read
as 0xFFxx rows, a width read as a 16-bit word) - an access violation in
the self-test, not a count; their near variants, which keep the write
inside the layer (C15b: a negative row taken as 0; C17b: the width `&
0x3F`), are refused by a count. No equivalent mutant. The helper
`Camera_TurnStep` and `Camera_TurnStepFB` share (`TurnStep`) carries C21..C24,
C28 and C29 for both, and each was refused in both functions; C25 / C26 /
C27 are each one's own constants. The lowest counts (C17b 47, C9 66) are
single-value edges the seed reaches a few times in a hundred (a width of
0x40; a negative offset byte not a multiple of 4); each is still refused.

**Two seeding gaps closed before the controls ran**, found by reading the
planned mutants against the seed: `AreaMap_SetHeight`'s rows were never
negative (C15 / C15b would have been equivalent) and the cue voices never
reached 0x80 (C42 would have been). The seed was widened first (section
4), and every control above is from the run after it.

## 6. Latent defects (described, not fixed)

None of the thirteen divides, reads its own frame or jumps through a
table, so none aborts; every unchecked index is reproduced.

- **`Effect_HoldFlag1C` with every effect record in use sets nothing**:
  no record, no flag. Its callers do their work first (`Area49_CellHook`
  toggles the switch's flag before the call and plays its sound after),
  so a switch fired while all 20 effect records are busy has no cool-down
  and can fire again on the next frame.
- **`Camera_TurnStep` / `Camera_TurnStepFB` take the step as an s8**,
  while the degree wrappers pass `x * 4096 / 360` as a dword: a step of 12
  degrees or more (136 and up) turns the other way, and its test runs the
  other way too. Every caller passes 2 or -2 degrees (22 steps).
- **The turn test does not wrap**: it compares `angle & 0xFFF` with the
  target as numbers, so a step whose sign points the long way round (a
  positive step toward a smaller target) ends the turn at once, snapping
  the angle to the target.
- **`Sound_SetCueVolume` of a cue with bank bits 0** reads 0x384 bytes
  before `Sound_Banks` for the cue and its voices and hands whatever
  non-zero dword it finds there to `SetVolume` as a buffer; banks 7..15
  read past the six records. Its one caller passes 0x207 / 0x208 (bank 2).
- **`Member_SetState2_8`, `AbilityList_ForType` and
  `Party_PlaceInFormation` index by whole bytes**: a member past `ObjTrio`'s
  three, a record past eight, an id past the ability table, a group or slot
  past `Formation_SlotVectors` (whose length is not established), a
  formation past `BattleFormation_Offsets`' 16 pairs (into
  `BattleFormation_Anims`). At `0x904060` = 0xFF with two members the
  group wraps to 0.
- **`AreaMap_SetHeight` is unbounded**: any x, z (s16s) write that far
  from the layer.
- **`KeyItem_Remove(0)`** clears the first empty slot and answers 1.

**Numbered 2026-09-28** ([`round-10-cleanup.md`](round-10-cleanup.md) item 2):
the defects above are D134 (tables back to back), D135 (`Effect_FindFree`'s
none as a slot), D136 (reads and writes by an unchecked byte or count), D140
(the turn steps as s8), D141 (`Sound_SetCueVolume` bank 0), D161 (small slips)
in [`known-defects.md`](known-defects.md).

## 7. Found on the way

- **`0x57C650`**, the thirteenth (section 1), and the two left beside the
  group: `0x469FB0` (effect kind 4) and `0x5A6C60` (`SndBuf_SetVolume` by
  its reading).
- **`inject_all.cpp`: `ScenaSc13_Inject()` runs after `DrawPool_Grow()`**,
  whose comment says it must run last, after every module's self-test.
  Found placing this group's line (before `DrawPool_Grow()`); SC13's line
  is the coordinator's to move.
- **`Formation_SlotVectors` and `BattleFormation_Offsets`** named; the
  formation's placement is a base vector per (group, slot) scaled or
  turned a quarter by the formation's pair.
