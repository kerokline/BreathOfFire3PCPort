# World 0, areas 27..29 and 32..37: the band `0x403400..0x4053B0`

**Status:** IN PROGRESS (2026-09-27) - 52 functions ours
(`src/game/area_w0c.cpp`, shadow name `area_w0c`), fuzzed headless through
the area harness ([`area_harness.md`](area_harness.md)), one `Run` per area:
0 mismatches in 312,000 rounds (in this worktree); 123 controls planted, 122 refused by a count, 1 equivalent with its near variant refused (section 4). Fuzz
only: the combat and world-map routes enter areas 29 and 33, but reach none
of the 52 (section 8). No divergence; the five dispatchers through an area's
state table abort past it (section 6).

Group AR0C of round ten's second wave
([`takeover-queue-round10.md`](takeover-queue-round10.md) section 6). The
band is the tool's (`tools/area_rows.py`, [`area-rows.md`](area-rows.md)):
75 starts, 20 ours already (round eight's DA, [`worldmap_area.md`](worldmap_area.md),
and round seven's world-map frame and draws, [`world-map-hud.md`](world-map-hud.md)),
55 to take. **52 taken; 3 dropped** (section 7): `0x404180`, `0x4041B0`,
`0x4041E0` are the case blocks `WorldMap_FrameStep` (ours) carries inline.
Areas 30 and 31 have no code (their descriptors' `+0x34`, `+0x3C`, `+0x40`
are null). Area 29's only function, its init `Area29_PickFieldObject`, is
round eight's.

Every function was read to its last instruction with capstone
(2026-09-27); each extent is `magic_rows.descend`'s and agrees with the
tool's `analysis/area_funcs.tsv`; the clone tables are
`magic_rows.clone_sites`, the routine `area_rows.py --clones` prints with.
What an area *is* in the story is not read here. The PSX twins are the
sibling's `names/area_records.toml` (descriptor handlers only; choices,
tails and triggers have no pairing there).

## 1. The functions, area by area

The shape is the root the function hangs from (the harness's
`Shape`): `kChoice` a `+0x34` choice handler (the message box's answer
byte `0x7DEE67` in, the message word `0x7DEE48` read after), `kHandler` a
`+0x3C` handler (movement-script ops `03` / `DE`), `kTail` a
`Field_ModeTailKinds` phase (by the s8 `0x9039F3`), `kState` an entry of a
state table in the area's `.data` or of a world-map record, `kHook` a step
hook `(x, z)` answering in `al`, `kCallee` an object trigger
(`Field_ObjectTriggers` `0x662E20` by object `+0x86`, called
`(object, 0x904030)` by `0x56E020`, answering in `al`). A function that is
both a choice and a handler is fuzzed as a handler (none of them touches the
message word).

### Area 27 (descriptor `0x5ED848`; PSX `0x801F4D20`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x403400` | `Area27_ChoiceTail1` | `0x22` | choice 0 | kChoice | message 0xFFFF; an answer with bit 0 clear arms tail kind 1 at state 0 |
| `0x403430` | `Area27_ChoiceTail2` | `0x21` | choice 1 | kChoice | the same, tail kind 2 |
| `0x403460` | `Area27_ChoiceCounter5or6` | `0x28` | choice 2 | kChoice | answer (s8) 0: counter `0x903848` = 5, 1: 6 |
| `0x403490` | `Area27_ChoiceCounter5Bor5A` | `0x28` | choice 3 | kChoice | 0: `0x5B`, 1: `0x5A` |
| `0x4034C0` | `Area27_ChoiceCounter5Cor5D` | `0x28` | choice 4 | kChoice | 0: `0x5C`, 1: `0x5D` |
| `0x4034F0` | `Area27_SpawnEffect2F` | `0x28` | choice 5 = handler 0 (PSX `0x801F3710`) | kHandler | a free effect slot gets kind `0x2F` |
| `0x403520` | `Area27_ClearCells` | `0x46` | choice 6 = handler 1 (PSX `0x801F376C`) | kHandler | map cells x `0x27`, `0x28` by z `0x25..0x27` zeroed |
| `0x403570` | `Area27_TailDropIn1` | `0xEC` | tail kind 1 | kTail | a five-state scene: wait for `Field_Request` to leave 2, story flag 1 and `ScriptFlags_Set40`; `Party_DropIn(1)`; wait for counter 3 = `0x20`, a 0x10-frame timer; `Field_ChangeArea(0x1B, 0x4B0000, 0x380000, 0x8F)`, counter 3 + 1; wait for counter 3 = `0x31`, undo it all |
| `0x403660` | `Area27_TailDropIn2` | `0xEC` | tail kind 2 | kTail | the same scene: flag 0, `Party_DropIn(2)`, counter 3 = `0x10`, `Field_ChangeArea(0x1B, 0x260000, 0x380000, 0x90)` |

**The tool's gap `0x403570` is area 27's.** `area_rows.py` lists it as a gap
("tail kind 1; reached by none") because no code stores kind 1 as an
immediate. Area 27's choice 0 does, through a register: `mov al, 1 ... mov
[0x9039F3], al`. The tool's tail-kind scan reads `mov byte [0x9039F3], imm8`
only; a fix is to follow a register loaded with an immediate (as
`clone_sites` does for stack tables).

### Area 28 (descriptor `0x5EE1A0`; PSX `0x801F3DBC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x403750` | `Area28_SpawnEffect40` | `0x53` | handler 2 (PSX `0x801F3424`) | kHandler | an effect of kind `0x40` at cell (`0x15`, `0x5B`), its `+0x3C` the ground's elevation there |

Handlers 0 and 1 are `0x437CC0`, a shared `ret` outside the band.

### Area 29 (descriptor `0x5EE270`)

Nothing to take: its init `Area29_PickFieldObject` `0x4037B0` is round
eight's (the combat route enters it).

### Area 32 (descriptor `0x5EF2C8`; PSX `0x801F48BC`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x403880` | `Area32_ChoiceLeaderAnim` | `0x38` | choice 0 | kChoice | answer 1: animation `0x80` on the leader (`Sprite_Current` made `ObjTrio` for the call and put back) |
| `0x4038C0` | `Area32_RunA` | `0x12` | choice 2 = handler 0 (PSX `0x801F36E4`) | kHandler | `Area32_StatesA` by `Sprite_Current[4]` |
| `0x4038E0` | `Area32_ShadeStart` | `0x38` | `Area32_StatesA[0]` | kState | `Sprite_ShadeFadeBegin`; the object's `+0x5C` = 1, moved by its step, state 1; the leader's word `+0x12E` less 2 |
| `0x403920` | `Area32_ShadeStep` | `0x38` | `Area32_StatesA[1]` | kState | `Sprite_ShadeLower(8)`: while 0, the leader's `+0x12E` less 2 and the object moved; then state 0 |
| `0x403960` | `Area32_RunB` | `0x12` | choice 3 = handler 1 (PSX `0x801F3800`) | kHandler | `Area32_StatesB` by `Sprite_Current[4]` |
| `0x403980` | `Area32_TintStart` | `0x97` | `Area32_StatesB[0]` | kState | a tint `(0, 0, 0, 1)` on the object, its slot to the active member's `+0x9F`; the tint bytes `0x80`, step `0x2000`, state 1; the member's `+0x8A` less 2 |
| `0x403A20` | `Area32_TintStep` | `0xC2` | `Area32_StatesB[1]` | kState | the three tint bytes up by 4 (a signed compare with `0xC0`); all at `0xC0`: `Tint_Release`, the object's flags and tint cleared, state 0, the member's `+0x84` = 2 |
| `0x403AF0` | `Area32_SpawnEffect50Near` | `0x9A` | choice 4 = handler 2 (PSX `0x801F3A78`) | kHandler | an effect of kind `0x50` at the object, `0x4000` up, timer `0x78` |
| `0x403B90` | `Area32_SpawnEffect4F` | `0x67` | choice 5 = handler 3 (PSX `0x801F3C18`) | kHandler | an effect of kind `0x4F` whose `+0xB` is the active member's record index (a signed division by `0xA4` from `Sprite_Objects`) |
| `0x403C00` | `Area32_SpawnEffect4E` | `0x3A` | choice 6 = handler 4 (PSX `0x801F3D28`) | kHandler | an effect of kind `0x4E` |
| `0x403C40` | `Area32_SpawnEffect50Far` | `0x9A` | choice 7 = handler 5 (PSX `0x801F3DCC`) | kHandler | as `...Near`, `0x8000` up, timer `0x3C` |

Choice 1 is `0x403050`, area 22's block (a body nine areas share).

### Area 33 (descriptor `0x5EF4D8`; the world map of the owner's route)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x404680` | `Area33_Record08Run` | `0x12` | `WorldMap_Records[1] +8` (effect kind `0x16`) | kState | `WorldMap33_Record08States` by `Sprite_Current[1]` |
| `0x4046A0` | `Area33_Record08Start` | `0x154` | `WorldMap33_Record08States[1]` | kState | animation bank `0x46`; the direction `+8`'s step words to `+0xC` / `+0x10`; the leader's position, drawn back by the step `>> 13` (a variant `+6` nudges 2 along one axis) and `>> 9`; state + 1; animation and `+0x2A` by direction; `Sprite_UpdateScreen` |
| `0x404800` | `Area33_Record04Run` | `0x12` | `WorldMap_Records[1] +4` (effect kind `0xE`) | kState | `WorldMap33_Record04States` by `Sprite_Current[1]` |
| `0x404820` | `Area33_Record04Start` | `0xB5` | `WorldMap33_Record04States[0]` | kState | `Effect_Release` when character record 0's byte `+9` is 9 or `Field_StatusBits` bit 0; else animation bank `0x205`, the object's draw bytes cleared, the map cell of `WorldMap33_Record04Cells[+0xB]` set to `0xA0`, animation 0, state 1 |

`0x404680` and `0x404800` are the "record 1's `+8` / `+4`" that
[`worldmap_area.md`](worldmap_area.md) section 10 listed as in no group.
`Area33_Record04Start` calls `WorldMap_RecordIndex` and never reads its
answer. The area's other 20 functions are ours already and called by name
where these reach them (none do: the four touch only engine callees).

**The data blocks the tool moved.** Area 33's tables (`0x5EF51C..0x5EF6C0`,
the place messages, plate and HUD states, the four tables named here, the
drift UVs) lie after area 33's own descriptor `0x5EF4D8`, below area 34's
`0x5F01A0`: by the default rule (a dword belongs to the descriptor ending
next above it) they were area 34's; the tool moved 20 pointers to 33.
Checked for the tables this group names: an image-wide scan for each
table's address as a dword (2026-09-27) finds it only in area 33's own code
(`0x40416E`, `0x40468E`, `0x4046D4`.., `0x40480E`, `0x4048AC`) and, for
`WorldMap33_Record04Cells`, in `WorldMap_Records[1] +0x14` (`0x653940`:
record 1's data pointer is this table). `0x5EF688` also appears at
`0x40442A` / `0x404454` in `WorldMap_DrawFrame`, as the end bound of the
table before it, not a read. Area 32's two state tables and area 36's one
sit after their own descriptors the same way and were moved the same way
(one reference each, their dispatcher's).

### Area 34 (descriptor `0x5F01A0`; PSX `0x801F3910`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x404D50` | `Area34_ChoiceTail5` | `0x89` | choice 0 | kChoice | answer 0 / 1 arms tail kind 5 with sub-kind 5 / 4 and story flag `0x1E`; then, the answer (re-read) not 2, `Cond_ByteFA` 8 and flag 3 of the chapter's row clear: the tail disarmed, `ScriptFlags_Set40`, `Party_DropIn(2)`, `MoveScript_Var7` = 2 |
| `0x404DE0` | `Area34_SkipScript` | `0x19` | choice 1 = handler 0 (PSX `0x801F2CDC`) | kHandler | unless the leader's `+0x89` is 7, the running script's position (`MoveScript_Object +0xA`) moves on `0x15` |
| `0x404E00` | `Area34_SpawnEffect51` | `0x5F` | choice 2 = handler 1 (PSX `0x801F2D1C`) | kHandler | an effect of kind `0x51`, state 6, at cell (`0x36`, `0x42`) on the ground |
| `0x404E60` | `Area34_Trigger51` | `0x40` | object trigger 51 | kCallee | story flag `0x6D`; when `0x6D..0x70` are all set, `0x71`; al 0 |

### Area 35 (descriptor `0x5F0330`; PSX `0x801F2EC8`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x404EA0` | `Area35_SetCounter1` | `0x38` | handler 0 (PSX `0x801F2C04`) | kHandler | the active member's `+0x80` bit 0 cleared, `ScriptFlags_Set40`, counters (1, 0, 0, 0), `MoveScript_Var7` = 4 with step `0x14` |
| `0x404EE0` | `Area35_SetCounter2` | `0x38` | handler 1 (PSX `0x801F2C74`) | kHandler | counter 0 = 2 |
| `0x404F20` | `Area35_SetCounter3` | `0x38` | handler 2 (PSX `0x801F2CE4`) | kHandler | counter 0 = 3 |
| `0x404F60` | `Area35_HideObject` | `0x1D` | handler 3 (PSX `0x801F2D54`) | kHandler | the active member's `+0x80` bit 0 cleared, the running object's `+0` = 0 |

### Area 36 (descriptor `0x5F0540`; PSX `0x801F32C0`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x404F80` | `Area36_StepHook` | `0x5E` | `Area_StepHook`'s case for area 36 | kHook | with `Cond_ByteFD` 2 and story flag `0x33`, a step whose high words are x `0x46..0x48`, z `0x23..0x25` with the leader's byte `+8` 0, 6 or 7: counter 0 = 0, `Party_DropIn(0)`, al 1; else al 0 |
| `0x404FE0` | `Area36_EffectRun` | `0x12` | `Effect_KindHandlers` entry `0xB5` | kState | `Area36_EffectStates` by `Sprite_Current[1]` |
| `0x405000` | `Area36_EffectStep` | `0x2B` | `Area36_EffectStates[1]` | kState | the object's position (`+0x34`, `+0x38`, `+0x3C`) copied to the stack and handed to `0x4220D0` |

The descriptor's two handlers are `0x419D80` and `0x421FB0`, bodies other
areas share.

### Area 37 (descriptor `0x5F14A0`; PSX `0x801F4A68`)

| PC | Name | Size | Root | Shape | What |
|---|---|--:|---|---|---|
| `0x405030` | `Area37_ChoiceAsk5E` | `0x24` | choice 0 | kChoice | answer 0: message `0x5E`; else message `0x60` and the byte `0x9398CF` = 6 |
| `0x405060` | `Area37_ChoiceConfirm60` | `0x24` | choices 3, 4 | kChoice | answer not 0: message `0x60`, `0x9398CF` = 6; else 0xFFFF |
| `0x405090` | `Area37_ChoiceAsk72` | `0x24` | choice 5 | kChoice | `0x72` / `0x74` |
| `0x4050C0` | `Area37_ChoiceConfirm74` | `0x24` | choices 8, 9 | kChoice | `0x74` |
| `0x4050F0` | `Area37_ChoiceAsk86` | `0x24` | choice 10 | kChoice | `0x86` / `0x88` |
| `0x405120` | `Area37_ChoiceConfirm88` | `0x24` | choices 13, 14 | kChoice | `0x88` |
| `0x405150` | `Area37_ChoiceAsk9A` | `0x24` | choice 15 | kChoice | `0x9A` / `0x9C` |
| `0x405180` | `Area37_ChoiceConfirm9C` | `0x24` | choices 18, 19 | kChoice | `0x9C` |
| `0x4051B0` | `Area37_ChoiceDropIn` | `0x35` | choice 20 | kChoice | answer 0: counter 3 = `0x64`, `Party_DropIn(3)`; else message `0xBD`, counter 3 = 0 |
| `0x4051F0` | `Area37_ChoiceTail2E` | `0x49` | choice 21 | kChoice | 0: `ScriptFlags_Set40`, tail kind `0x2E`; 1: counter 3 = `0x64`; 2: `0x5F`; any other answer leaves the message word |
| `0x405240` | `Area37_TailLeave` | `0xE0` | tail kind `0x2E` | kTail | four states: flag `0x72`, `Field_ChangeArea(0x25, 0x2C8000, 0x270000, 1)`, pending kind `0xFE`; message `0xB5` once `Field_Request` is 0; `Transition_Start(1)`, `Draw_PassFlags` `0x1F`; once the wait word is 0, all undone |
| `0x405320` | `Area37_Trigger45` | `0x16` | object trigger 45 | kCallee | `ScriptFlags_Set40`, tail kind 4 sub-kind `0xB`; al 0 |
| `0x405340` | `Area37_Trigger46` | `0x16` | object trigger 46 | kCallee | sub-kind `0xC` |
| `0x405360` | `Area37_Trigger47` | `0x16` | object trigger 47 | kCallee | sub-kind `0xE` |
| `0x405380` | `Area37_Trigger48` | `0x16` | object trigger 48 | kCallee | sub-kind `0xD` |
| `0x4053A0` | `Area37_ToggleScriptFlag8` | `0x9` | handler 0 (PSX `0x801F3B14`) | kHandler | `Field_ScriptFlags` bit 3 toggled |

Choices 1, 2, 6, 7, 11, 12, 16, 17 are `0x420850` / `0x420870`, bodies
another group's block holds (14 areas share them). The four "ask / confirm"
pairs are one shape each: the table names each ask once and its confirm
twice.

**Shared bodies.** None of the 52 is shared with an area outside the group,
and none between two of the group's areas; the eight names that area 37's
choice table holds twice are one function each, taken once, keyed by
address.

## 2. Ours

`src/game/area_w0c.cpp`, calling out only through the harness
(`AH_CALL(name)` for every named callee; `AH_AT` for `0x4220D0`). Shapes
that repeat are one helper with the function's constants: area 27's two
tails (`RunDropInTail`), the effect spawns (`SpawnFromCurrent`,
`SpawnEffect50`, `SpawnAtCell`), area 35's counters (`SetCounters`), area
37's ask / confirm pairs and triggers. Kept as the originals: every
re-read after a call (`Sprite_Current`, the tail state, counter 3, the
answer byte in `Area34_ChoiceTail5`, `Field_ActiveMember`), the order of
stores around each call, the s8 reads of the answer and the tail state
where the original sign-extends and the byte reads where it does not,
`Area33_Record04Start`'s unread `WorldMap_RecordIndex`, the unchecked table
reads by `+8` / `+0xB` in area 33's two starts.

## 3. The fuzz

`BOF3X_SHADOW=area_w0c` (`src/game/area_w0c_fuzz.cpp`): eight `Run`s under
the one shadow name, one per area with its `Group::area` (27, 28, 32..37),
6,000 rounds per function, the real descriptors and tables in place.

- **Callees the group lists:** `Effect_FindFree` as `kByte` `0xFF..0x03`
  (a slot inside the group's four effect records, or none a fifth of the
  time); `ScriptFlags_Set40` / `Clear40`, `Tint_Release`,
  `Transition_Start`, `Effect_Release`, `WorldMap_RecordIndex` (ours, not
  in the standard set); `0x4220D0` by its raw address, its pointer argument
  logged as a hash of the 12 bytes it reads. The rest are the standard set.
- **Data tables** (swapped for recorders): `Area32_StatesA` / `B` (2 each),
  `WorldMap33_Record08States` (3), `WorldMap33_Record04States` (2),
  `Area36_EffectStates` (2).
- **Regions beyond the field frame:** `Effect_Objects` records 0..3,
  `Field_ActiveMember`, `MoveScript_Object`, the chapter row pointer
  `0x929ED0`, the pending kind `0x937F98`, `0x9398CF`, `0x903A79`,
  `MoveScript_WaitWordDA`, `Draw_PassFlags` (29 regions, 16,738 bytes).
- **Every round:** `Field_ActiveMember` at one of the four party objects
  (`Sprite_ObjectsExtra`), a field object, a party record or the running
  object itself (the two alias in the game); `MoveScript_Object` at a field
  object or a party record.
- **Seeds:** the choice answer at each value a handler tests (0..3), its
  sign edge (`0x7F`, `0x80`, `0x81`, `0xFF`) and above; the tail states
  -1..last+1 and the last state alone; `Field_Request` 0..3; counter 3 at
  each value awaited and one either side; the timer at 1 (three times as
  often), 2, 0, `0x10`, `0xFFFF`; the dispatchers' index inside its table
  (section 6); the tint bytes at `0xC0` all three, a step short
  (`0xBC`), and either side of the signed compare (`0xBF`, `0xC1`, `0x7F`,
  `0x80`, `0x3F`, `0xFF`); `Field_ActiveMember` any value for
  `Area32_SpawnEffect4F` (it only subtracts and divides, never follows); the
  direction `+8` 0..3 and the variant `+6` 0, 1, 2, `0xFF`; the byte
  `0x903A79` 8, 9, 10, `Field_StatusBits` bit 0 clear, `+0xB` 0..2 and the
  map's width byte 1..`0x16` (any byte pair then lands inside the harness's
  8 KiB of the area block); `Cond_ByteFA` 8 (three times as often), 7, 9,
  `0x88`; the leader's `+0x89` 7, 6, 8, `0x87`; the step hook half the
  rounds with every test passing but at most one, drawn to fail
  (`Cond_ByteFD` 1, 3, `0x82`; the pose 1, 5, 8, `0x80`, `0x86`; an x high
  word of `0x45`, `0x49`, `0x145`, `0xFF46`; a z of `0x22`, `0x26`, `0x123`,
  `0xFF23`); the wait word 0, 1, `0x100`, `0xFFFF`. The object triggers are
  called `(a field object, 0x904030)`.
- **Three louder stand-ins** (the group's listing, an `effect`): `Field_ChangeArea` moves counter 3, `Flags_Set` the answer byte, `Msg_OpenScript` the tail state, each half the time (from `Noise`): the cells their callers read again after the call.
- **The group's disturbance** (from the hash it is given): the tail state,
  counter 3 (at `0x20`, `0x31` or any), the answer byte, the active member
  pointer, a byte of the row pointer, the timer word.

**Result (in this worktree):** 312,000 rounds over the 52 functions (6,000 each), 252,412 calls to the stand-ins, 0 mismatches, 16,738 bytes of state (29 regions) and the log compared. Coverage: every state-table entry reached 1,949..3,014 times; `Field_ChangeArea` 314 (area 27) / 537..561 (area 37), `Msg_OpenScript` 101..112, the step hook's success (`Party_DropIn`) 377..434, `Area34_ChoiceTail5`'s tail path 608, `Tint_Release` 887, `Effect_Release` 2,126.

`BOF3X_SHADOW='*'`: exit 0, `inject: 3736 ours`, 350 self-test lines, no mismatch.

## 4. Controls

Planted one at a time in `area_w0c.cpp` by a script (the scratch `controls.py`, not committed) that plants on a unique anchor, rebuilds, checks `area_w0c.cpp` recompiled, runs `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=area_w0c`, restores; after the last it rebuilt and ran the clean self-test (exit 0, 0 mismatches). **123 planted: 122 refused by a count (exit 3), 1 equivalent (B4) with its near variant B4b refused.** No hang, no fault. Every one of the 52 functions has at least one control of its own; a control in a shared helper lists every function it refused in. The thinnest (C15 40, E2 37, H18 36, F7 23..25, G7 39, A12 67 / 84) test a re-read after a call: the first run refused A12, E2 and H18 in one round each, and the three stand-ins that now move the re-read cell half the time (section 3: `Field_ChangeArea`, `Flags_Set`, `Msg_OpenScript`) raised them; the table is the second run, all 123 again.

| # | Function | Planted | Refused in rounds (of 6,000 per function) |
|---|---|---|---|
| A1 | `Area27_ChoiceTail1` | bit 1 tested for bit 0 | 2360 |
| A2 | `Area27_ChoiceTail2` | state 1 armed | 2792 |
| A3 | `Area27_ChoiceCounter5or6` | answer 0 sets 6 | 453 |
| A4 | `Area27_ChoiceCounter5Bor5A` | answer -1 for 1 | 895 |
| A5 | `Area27_ChoiceCounter5Cor5D` | message 0xFFFE | 6000 |
| A6 | `Area27_SpawnEffect2F` | kind 0x2E | 4830 |
| A7 | `Area27_SpawnEffect2F` | slot 3 taken for none | 1179 |
| A8 | `Area27_ClearCells` | one cell set to 1 | 6000 |
| A9 | `Area27_TailDropIn1` | counter 0x21 awaited | 82 |
| A10 | `Area27_TailDropIn2` | flags 0x8F | 158 |
| A11 | `RunDropInTail (both)` | state 0 waits on Field_Request 3 | TailDropIn1 177, TailDropIn2 211 |
| A12 | `RunDropInTail (both)` | counter 3 read before Field_ChangeArea | TailDropIn1 67, TailDropIn2 84 |
| A13 | `RunDropInTail (both)` | the tail kind not cleared | TailDropIn1 103, TailDropIn2 120 |
| A14 | `RunDropInTail (both)` | Party_DropIn(entry + 1) | TailDropIn1 576, TailDropIn2 574 |
| A15 | `RunDropInTail (both)` | timer 0x11 | TailDropIn1 43, TailDropIn2 43 |
| B1 | `Area28_SpawnEffect40` | kind 0x41 | 4805 |
| B2 | `SpawnAtCell (28, 34)` | elevation << 15 | 4805 |
| B3 | `SpawnAtCell (28, 34)` | elevation asked at (z, x) | 4805 |
| B4 | `SpawnAtCell (28, 34)` | elevation zero-extended | **0 - equivalent**: `(s32)(s16)g << 16` and `(u32)(u16)g << 16` keep the same low 16 bits and shift the rest out, so no elevation tells them apart; B4b, the near variant, refused |
| B4b | `SpawnAtCell (28, 34)` | elevation as a signed byte (near B4) | 4782 |
| C1 | `Area32_ChoiceLeaderAnim` | any answer not 0 | 5082 |
| C2 | `Area32_ChoiceLeaderAnim` | Sprite_Current left the leader | 380 |
| C3 | `Area32_RunA` | table B's | 6000 |
| C4 | `Area32_RunB` | the other entry | 6000 |
| C5 | `Area32_ShadeStart` | the leader word less 1 | 6000 |
| C6 | `Area32_ShadeStart` | Sprite_Current read before the call | 228 |
| C7 | `Area32_ShadeStep` | Sprite_ShadeLower(7) | 6000 |
| C8 | `Area32_ShadeStep` | state 1 when done | 3943 |
| C9 | `Area32_TintStart` | tint alpha 2 | 6000 |
| C10 | `Area32_TintStart` | bit 0x20 cleared too | 6000 |
| C11 | `Area32_TintStart` | the tint slot to +0x9E | 6000 |
| C12 | `Area32_TintStep` | the compare unsigned | 3574 |
| C13 | `Area32_TintStep` | member +0x84 = 3 | 887 |
| C14 | `Area32_TintStep` | member +0x8A less 4 | 5113 |
| C15 | `Area32_TintStep` | Sprite_Current not read again after Tint_Release | 40 |
| C16 | `Area32_SpawnEffect50Near` | timer 0x79 | 4845 |
| C17 | `Area32_SpawnEffect50Far` | lift 0x4000 | 4752 |
| C18 | `SpawnEffect50 (both)` | +6 from +9 | SpawnEffect50Near 4828, SpawnEffect50Far 4732 |
| C19 | `SpawnEffect50 (both)` | +0x3C raised 0x100000 | SpawnEffect50Near 4845, SpawnEffect50Far 4752 |
| C20 | `Area32_SpawnEffect4F` | divided by 0xA0 | 3900 |
| C21 | `Area32_SpawnEffect4F` | the division unsigned | 1220 |
| C22 | `Area32_SpawnEffect4E` | kind 0x4D | 4802 |
| C23 | `SpawnFromCurrent (4E, both 50s)` | +0 = 2 | SpawnEffect50Near 4845, SpawnEffect4E 4802, SpawnEffect50Far 4752 |
| C24 | `Area32_SpawnEffect4F` | +0 = 2 | 4790 |
| D1 | `Area33_Record08Run` | by +2 | 3966 |
| D2 | `Area33_Record04Run` | the other entry | 6000 |
| D3 | `Area33_Record08Start` | bank 0x47 | 6000 |
| D4 | `Area33_Record08Start` | +0x29 = 4 | 5998 |
| D5 | `Area33_Record08Start` | step >> 12 | 2056 |
| D6 | `Area33_Record08Start` | forward on variant 2 | 1938 |
| D7 | `Area33_Record08Start` | the axis by +0x10 | 3531 |
| D8 | `Area33_Record08Start` | z step >> 8 | 3069 |
| D9 | `Area33_Record08Start` | +0x2A from the animation byte | 3897 |
| D10 | `Area33_Record08Start` | +0x10 the x step | 5879 |
| D11 | `Area33_Record08Start` | Sprite_Current not read again | 188 |
| D12 | `Area33_Record08Start` | the step zero-extended | 1726 |
| D13 | `Area33_Record04Start` | byte 8 releases | 2225 |
| D14 | `Area33_Record04Start` | status bit 1 | 2285 |
| D15 | `Area33_Record04Start` | the cell 0xA1 | 3874 |
| D16 | `Area33_Record04Start` | x and z swapped | 3472 |
| D17 | `Area33_Record04Start` | animation 1 | 3874 |
| D18 | `Area33_Record04Start` | +0x2A = 1 | 3874 |
| D19 | `Area33_Record04Start` | no WorldMap_RecordIndex | 3874 |
| E1 | `Area34_ChoiceTail5` | sub-kinds swapped | 893 |
| E2 | `Area34_ChoiceTail5` | the answer not read again | 37 |
| E3 | `Area34_ChoiceTail5` | Cond_ByteFA 9 | 2420 |
| E4 | `Area34_ChoiceTail5` | row flag 4 | 1811 |
| E5 | `Area34_ChoiceTail5` | step 1 | 608 |
| E6 | `Area34_ChoiceTail5` | answer 1 not armed | 455 |
| E7 | `Area34_SkipScript` | pose 6 skips | 1974 |
| E8 | `Area34_SkipScript` | on 0x14 | 5008 |
| E9 | `Area34_SpawnEffect51` | no state 6 | 4810 |
| E10 | `Area34_SpawnEffect51` | the slot not stored | 5944 |
| E11 | `Area34_Trigger51` | flags 0x6D..0x6F | 1776 |
| E12 | `Area34_Trigger51` | flag 0x72 | 1169 |
| E13 | `Area34_Trigger51` | answers 1 on a clear flag | 4831 |
| F1 | `Area35_SetCounter1` | counter 0 | 6000 |
| F2 | `Area35_SetCounter2` | counter 3 | 6000 |
| F3 | `Area35_SetCounter3` | counter 4 | 6000 |
| F4 | `SetCounters (35 x3)` | bit 1 cleared too | SetCounter1 3011, SetCounter2 2994, SetCounter3 3041 |
| F5 | `SetCounters (35 x3)` | step 0x15 | SetCounter1 6000, SetCounter2 6000, SetCounter3 6000 |
| F6 | `SetCounters (35 x3)` | counter 2 kept | SetCounter1 5971, SetCounter2 5968, SetCounter3 5970 |
| F7 | `SetCounters (35 x3)` | the member read after the call | SetCounter1 23, SetCounter2 25, SetCounter3 24 |
| F8 | `Area35_HideObject` | +0 = 1 | 6000 |
| F9 | `Area35_HideObject` | bit 0 kept | 2977 |
| G1 | `Area36_StepHook` | Cond_ByteFD 3 | 3559 |
| G2 | `Area36_StepHook` | flag 0x32 | 2837 |
| G3 | `Area36_StepHook` | x span 4 | 79 |
| G4 | `Area36_StepHook` | z from 0x22 | 190 |
| G5 | `Area36_StepHook` | pose 5 for 6 | 157 |
| G6 | `Area36_StepHook` | answers 3 | 377 |
| G7 | `Area36_StepHook` | x high word as a byte | 39 |
| G8 | `Area36_EffectRun` | the other entry | 6000 |
| G9 | `Area36_EffectStep` | y and z swapped | 6000 |
| H1 | `Area37_ChoiceAsk5E` | no-message 0x61 | 5546 |
| H2 | `Area37_ChoiceConfirm60` | message 0x5F | 5563 |
| H3 | `Area37_ChoiceAsk72` | yes-message 0x73 | 435 |
| H4 | `Area37_ChoiceConfirm74` | message 0x75 | 5587 |
| H5 | `Area37_ChoiceAsk86` | no-message 0x89 | 5541 |
| H6 | `Area37_ChoiceConfirm88` | message 0x87 | 5523 |
| H7 | `Area37_ChoiceAsk9A` | yes-message 0x9B | 467 |
| H8 | `Area37_ChoiceConfirm9C` | message 0x9D | 5538 |
| H9 | `ChoiceAsk (37 x4)` | mark 5 | ChoiceAsk5E 5546, ChoiceAsk72 5565, ChoiceAsk86 5541, ChoiceAsk9A 5533 |
| H10 | `ChoiceConfirm (37 x4)` | none 0xFFFE | ChoiceConfirm60 437, ChoiceConfirm74 413, ChoiceConfirm88 477, ChoiceConfirm9C 462 |
| H11 | `Area37_ChoiceDropIn` | Party_DropIn(2) | 434 |
| H12 | `Area37_ChoiceDropIn` | message 0xBE | 5566 |
| H13 | `Area37_ChoiceTail2E` | tail kind 0x2F | 425 |
| H14 | `Area37_ChoiceTail2E` | counter 0x5E | 477 |
| H15 | `Area37_ChoiceTail2E` | answer 3 and up as 2 | 4639 |
| H16 | `Area37_TailLeave` | flags 0 | 561 |
| H17 | `Area37_TailLeave` | pending kind 0xFF | 561 |
| H18 | `Area37_TailLeave` | the state read before the message | 36 |
| H19 | `Area37_TailLeave` | state 1 waits on Field_Request not 2 | 425 |
| H20 | `Area37_TailLeave` | pass flags 0x1E | 562 |
| H21 | `Area37_TailLeave` | the wait word low byte | 284 |
| H22 | `Area37_TailLeave` | counter 3 kept | 297 |
| H23 | `Area37_Trigger45` | sub-kind 0xA | 6000 |
| H24 | `Area37_Trigger46` | sub-kind 0xD | 6000 |
| H25 | `Area37_Trigger47` | sub-kind 0xF | 6000 |
| H26 | `Area37_Trigger48` | sub-kind 0xC | 6000 |
| H27 | `TriggerTail4 (37 x4)` | tail kind 5 | Trigger45 6000, Trigger46 6000, Trigger47 6000, Trigger48 6000 |
| H28 | `TriggerTail4 (37 x4)` | answers 1 | Trigger45 6000, Trigger46 6000, Trigger47 6000, Trigger48 6000 |
| H29 | `Area37_ToggleScriptFlag8` | bit 4 | 6000 |

## 5. The tables named

In `symbols.toml` as `[[data]]` with counts (21): `Area27_Choices`
`0x5ED828` (7), `Area27_Handlers` `0x5ED83C` (2), `Area28_Handlers`
`0x5EE190` (3), `Area32_Choices` `0x5EF2A8` (8), `Area32_Handlers`
`0x5EF2B0` (6), `Area32_StatesA` `0x5EF30C` (2), `Area32_StatesB`
`0x5EF314` (2), `Area33_Choices` `0x5EF4D0` (1), `WorldMap33_FrameStates`
`0x5EF5F8` (4), `WorldMap33_Record08States` `0x5EF688` (3),
`WorldMap33_Record08Steps` `0x5EF694` (4 rows of two words),
`WorldMap33_Record08Anims` `0x5EF6A4` (4 byte pairs),
`WorldMap33_Record04States` `0x5EF6AC` (2), `WorldMap33_Record04Cells`
`0x5EF338` (3 rows of 4 bytes), `Area34_Choices` `0x5F0184` (3),
`Area34_Handlers` `0x5F0188` (2), `Area35_Handlers` `0x5F0320` (4),
`Area36_Handlers` `0x5F0538` (2), `Area36_EffectStates` `0x5F0584` (2),
`Area37_Choices` `0x5F1430` (22), `Area37_Handlers` `0x5F1498` (1). A
descriptor's `+0x3C` array is the tail of its `+0x34` table in areas 27, 32
and 34 (as [`area-rows.md`](area-rows.md) section 2 found generally).

## 6. Latent defects and the aborts

Described, not fixed:

- **Unchecked state indexes.** `Area32_RunA` / `RunB` index their 2-entry
  tables by `Sprite_Current[4]`; `RunA`'s index 2 and 3 would run
  `Area32_StatesB`'s handlers, 4 a null. `Area33_Record08Run` (3 entries),
  `Area33_Record04Run` (2) and `Area36_EffectRun` (2) index by `+1` into
  what follows (a table of words, `WorldMap33_DriftUV`, a zero). **Ours
  aborts with a message** past each table (the owner's rule, round9 doc
  section 6, no DIVERGENCE entry); the fuzz keeps the index inside. No
  route measured reaches any of them.
- **`Area33_Record04Start`'s cell write** indexes
  `WorldMap33_Record04Cells` by `+0xB` unchecked and writes
  `AreaMap_Bytes[z * width + x]` with no bound: a `+0xB` past 2 reads the
  zero row and then other data, and writes wherever that lands. Kept
  faithfully (it is a write the harness can hold only with the width
  seeded small).
- **`Area33_Record08Start`** reads `WorldMap33_Record08Steps` / `Anims` by
  the direction `+8` unchecked (4 rows each; reads on through `.data`).
- **`Area37_ChoiceTail2E`** leaves the message word as it was for an answer
  of 3 or more (every other choice here writes it); what the message box
  does with a stale word is not read.

## 7. What the tool listed, against the reading

- **Dropped (3):** `0x404180`, `0x4041B0`, `0x4041E0`, listed as area 33's
  data-table entries `0x5EF5FC..0x5EF604`: they are `WorldMap_FrameStep`'s
  states 1..3 (the table `0x5EF5F8`, now named `WorldMap33_FrameStates`),
  which `WorldMap_FrameStep` (ours since round seven, extent `0xC1` over
  them) carries inline; `0x404180` falls into `0x4041B0` by a `jmp`. The
  table has no other reader. As round eight's DA left `0x404180` (its
  section 9).
- **Added:** none; every gap between the band's functions is padding.
- **Every other row agrees** with the reading: start, extent, calls, jump
  tables (`0x403570`, `0x403660`: 5 entries at `+0xD8`; `0x405240`: 4 at
  `+0xD0`), and the shape its root gives. Two roots the tool had not
  resolved: `0x403570` (a gap there; area 27's, section 1) and `0x404FE0`
  ("named by `.data` `0x655624`": `Effect_KindHandlers` entry `0xB5`).

## 8. What reaches it

- **The live check.** The combat route enters area 29 and the world-map
  route area 33 (`analysis/hidden_reached_combat.json`,
  `hidden_reached_worldmap.json`); in this band they reach only round eight's
  and round seven's functions (`0x4037B0`; `0x403CE0..0x404620`,
  `0x4048E0`) and the dropped case block `0x404180` (reached before round
  seven took its host). **None of the 52 is on either route**, so the
  coordinator's route A/Bs after the merge check that nothing else moved,
  not these functions. What would reach them: area 33's `0x404680` /
  `0x404800` run when an effect of kind `0x16` / `0xE` is live on the
  world map (its record 1); area 29 has nothing of this group's.
- **Everything else is reached only in play**: the choices when the area's
  message box asks, the handlers from the area's movement scripts (ops `03`
  / `DE`), area 36's step hook on every step there, the tails once armed by
  a choice, and:
- **Gap functions** (reached by no area's descriptor in the tool's walk):
  `0x403570` - area 27's choice 0 (section 1); `0x404E60` - object trigger
  51 (`Field_ObjectTriggers`, by an object's `+0x86` id set from area data;
  no immediate store of 51 to `+0x86` in `.text`); `0x405320..0x405380` -
  object triggers 45..48 (the same); `0x404FE0` - effect kind `0xB5`
  (`Effect_KindHandlers`; no immediate store of kind `0xB5` found in
  `.text`, so an effect record from data or a computed kind). Each sits in
  the block of the area named (34, 37, 36) and is attributed to it.

## 9. Calls across groups

- **Raw address:** `0x4220D0` (group AR3F's block, area 146's shared body;
  0x27A bytes; also called from engine code at `0x475CE2`, `0x47EF32`), by
  `Area36_EffectStep`, in `area_w0c_callees.h` as `kPositionHook`.
- **Through the tables, not called:** `0x4253C0` (AR4A), `0x40C490`
  (AR1E), `0x408990` (AR1B), `0x40B4F0` (AR1D) - other entries of the
  tables the dispatchers read in place.
- **By name, ours:** `Effect_FindFree`, `AreaMap_SetByte`,
  `AreaMap_Elevation`, `ScriptFlags_Set40` / `Clear40`, `Flags_Set` /
  `Clear` / `Test`, `Party_DropIn`, `Field_ChangeArea`, `Msg_OpenScript`,
  `Transition_Start`, `Sprite_SetAnimation`, `Sprite_SetAnimationBank`,
  `Sprite_UpdateScreen`, `Sprite_ShadeFadeBegin`, `Sprite_ShadeLower`,
  `Sprite_SetTint`, `Tint_Release`, `Effect_Release`,
  `WorldMap_RecordIndex`. No harness edit; no `AH_THEIRS` moved.
- `analysis/calltrace/entries_logic.txt`: 52 lines appended under a
  `# group AR0C` comment; the hosts `00402570 1240`, `00404620 2C0` and
  `00404F80 981` run over them (the consolidation keeps the smaller).
