# Group PM: game modes 3 to 6 and their steps, the game code the count missed

**Status:** MEASURED (2026-10-05) - the platform round's step 2
([`platform-layers-plan.md`](platform-layers-plan.md) section 4), group PM,
from `phase-3/round14-end` at `e3b98087`. **14 functions ours**
(`src/game/mode_rest.cpp`, shadow name `mode_rest`): `GameMode_Handlers`'
four entries no catalogue held (modes 3, 4, 5, 6), the steps of modes 3 and 5
that were not ours (nine), and the five-byte jump `0x587C20` only mode 3
calls. Each read to its last instruction with capstone and fuzzed through the
scenario harness in field mode ([`scenario_harness.md`](scenario_harness.md),
used unchanged): 56,000 rounds, 0 mismatches; **49 of 49 controls refused**.
Two step tables named. Not yet live-checked: the routes reach every mode
(section 8) but no trace ever armed these starts; the coordinator's state
hash after the merge is the live check.

## 0. For the record: how four modes stayed outside the count

`GameMode_Handlers` `0x656A44` holds twelve code pointers; `Field_Task`
`0x495800` calls `[0x656A44 + 4 * Game_Mode]` every frame. Eight of the
twelve were ours by round fourteen's end. The other four - `0x495BB0`,
`0x495E60`, `0x495E90`, `0x496230` - and the steps under them had no
symbol, no `impl`, and no row in any start list:

1. **The host's extent.** `pc_funcs.json` gave `GameMode_Field` `0x4959F0`
   an extent of 2,139 bytes (`0x85B`): `0x4959F0 + 0x85B = 0x49624B`, which
   is exactly the end of mode 6's tail jump at `0x496246`. `pe_funcs.py`
   found no direct call to any of the thirteen starts between - each is
   reached only through `.data` (`GameMode_Handlers`, the two step tables
   below) - so it ran on through them. `GameMode_Field`'s own code is 0x1BC
   bytes (its `symbols.toml` evidence, [`mode-tasks.md`](mode-tasks.md)
   section 3). The same shape hid twelve more starts after it, in
   `GameMode_LookEnd` `0x496250`'s 0x59A (round fourteen's R3G,
   [`rest_3g.md`](rest_3g.md) section 1.4).
2. **The start lists.** The catalogue's parts 0 and 1, and every round's cut
   built from them, list starts, not extents' contents: a start inside a
   host's extent is invisible unless a pass cuts the host. `pe_hidden.py`'s
   cuts did not reach this host.
3. **The rounds.** Each round's cut was the catalogue's remaining rows; with
   no row these were in no cut. Round eight's group DB had seen them: its
   doc ([`mode_states.md`](mode_states.md) sections 3 and 7) lists "the mode
   handlers `0x495BB0` (mode 3) with its steps `0x495BC0` / `0x495C50`,
   `0x496230` (mode 6)" as unowned neighbours and leaves mode 3's table
   unnamed as "no group's this round". Nothing carried that note into a
   later cut.
4. **The read pass.** [`platform-read-pass.md`](platform-read-pass.md) arms
   only listed starts; these were not armed, so no trace shows them, and a
   callee whose only callers lie here was attributed to `GameMode_Field`
   (ours) - which is how `0x587C20` came to be classed **original** ("one
   caller, a body of ours") when it is reached from Capcom's live mode 3
   (section 1.4).

So "10,009 ours, 0 left original" was a count of catalogued starts. It is
10,023 with this group.

**Can the same shape hide more?** The coordinator's scan of every named
`void *` table found only these four entries without a symbol; tables not
named as `[[data]]` (like this group's two) were not scanned. A cheap,
complete scan is described in the report (every `jmp` / `call [reg * 4 +
imm32]` in `.text`, its table walked while the cells point into `.text`,
each target that is neither a start nor inside the dispatching function's
own extent a candidate; and every catalogue extent compared with its code's
real end). Not run here (the brief).

## 1. What each function does

All are `cdecl`, `void (void)` but `Sound_MusicPlaying` (`int`). Every
address below that is not a name is in `mode_rest_callees.h`.

### 1.1 The modes and their dispatchers

`GameMode_Field` ([`mode-tasks.md`](mode-tasks.md) section 3) turns
`Field_Request` into the next mode: 1 -> 3, 2 -> 4, 3 -> 5 (the battle bytes
cleared, step 0), 4 -> 6.

| PC | Name | Bytes | PSX twin | What |
|---|---|--:|---|---|
| `0x495BB0` | `GameMode3_Run` | 0xF | `0x801985E4` | `xor eax, eax; mov ax, Game_Step; jmp [GameMode3_Steps + eax * 4]`, unchecked |
| `0x495E60` | `GameMode4_Run` | 0x28 | `0x801989D4` | with the message cells' `0x7DEE44` bit 1: `0x905B82 = 0x10`, `Field_Request` 0, `Game_Mode` 2; a tail jump to `Field_FrameScripted` either way |
| `0x495E90` | `GameMode5_Run` | 0xF | `0x80198A24` | `jmp [GameMode5_Steps + Game_Step * 4]`, unchecked |
| `0x496230` | `GameMode6_Run` | 0x1B | `0x80199170` | `Look_PadControl` at `Game_Step` 0, `GameMode_LookEnd` otherwise; a tail jump to `Field_LoadingFrame` |

What each mode *is*, from the code only: mode 3 runs the field menu
(`Menu_Frame` is its step 1; [`menu-screens.md`](menu-screens.md) section 1
had it as "mode 3 the menu"); mode 4 is the field's scripted frame until the
message cells' bit 1 (`Field_Request` 2 is "a message is open" in
`scenario_harness.h`'s cell notes); mode 5 runs `Battle_Frame` at step 5 and
the party's encounter functions around it - the battle; mode 6 is the
camera look of `Look_PadControl` / `Look_Return` (both ours,
[`mode_states.md`](mode_states.md) section 1.1).

### 1.2 Mode 3's steps (`GameMode3_Steps` `0x656A74`)

| # | PC | Name | Bytes | PSX |
|--:|---|---|--:|---|
| 0 | `0x495BC0` | `GameMode3_Enter` | 0x82 | `0x80198620` |
| 1 | `0x5172F0` | `Menu_Frame` (ours, mode_states) | | `0x8019A368` |
| 2 | `0x495C50` | `GameMode3_Leave` | 0x202 | `0x801986EC` |

**`GameMode3_Enter`**: `LoadDatFile(0x31E)` (the PSX loads `0x269`) and its
wait (`Task_Sleep(1)` a try while `File_LoadDone` answers 0); CLUT strip rows
1 and 2 (`Gfx_ClutStripCopyRow`); `Gfx_ClutStripDirty` 1, the menu block's
mode byte `0x929F00` 0 and byte `0x929F04` 3 - stored after `Transition_Start`'s
3 is pushed and before the call; `Transition_Start(3)`;
`Menu_WaitTransition(1)`; with the word `0x7E0678` (PSX `0x80143F20`) equal
to 0x6E and `Sound_MusicPlaying` answering 0, `Music_FadeOut(0x10)`;
`Game_Step` up.

**`GameMode3_Leave`**, in order:

- `Menu_Frame`; `Transition_Start(2)`; `Menu_WaitTransition(0)`; one sleep;
- `0x929F11` set (the menu changed the party): `PartySet_Load(0x904062,
  0x904063, 0x904064, 0)`, its wait, `0x929F11` 0;
- the party set byte `0x90412C |= 0x80` and `Snd_LoadBankFile((it & 0x7F) +
  0x2C2)` - the set's mode-0 file, as `PartySet_Select` would load it -
  not waited for ([`menu-screens.md`](menu-screens.md) section 1 read this as
  "reloads the area's file (`0x2C2 + n`)"; the `n` is the party set);
- `Field_InputFlags` bit 4: `LoadDatFile(0x12A)`, its wait,
  `CommuSim_RollOffers`;
- `Transition_Start(3)`; `Field_WaitTransition(1)`; the word `0x7E0678` 0x6E
  and the music not playing: `Music_FadeIn(0x10)`;
- then one of three ways back:
  - **`0x905B60` set** (cleared here) - a two-way trip, by the cells it
    keeps: with `0x904152` 0 and (`Field_InputFlags` bit 0 or area 0xBD)
    *out*: the leader's x and z (`0x802D74`, `0x802D78`) and
    `Game_AreaNumber` saved to `0x904148` / `0x90414C` / `0x904150`,
    `0x904152` 1, `0x904153` = (`0x656A80[Rand & 3]` + the leader's facing
    `0x802D48`) & 0xF, `Field_ChangeArea(word 0x937F82, 0x903860, 0x90384C,
    byte 0x905B88)`; otherwise *home*: `0x904152` 0, story flag 0x77 cleared
    (`Flags_Clear(0x904030, 0x77)`), `Field_ChangeArea(the saved area, x, z,
    4)`. What the trip is in the game is not read here (a hypothesis to the
    owner: the cells say "go to a fixed place and come back to where you
    were");
  - **else `0x905B61` set**: `Field_ChangeArea(it, 0x430000, 0x160000, 4)`,
    cleared;
  - **else** `Field_Request` 0;
- `Window_ResetAll`; `Game_Mode` 2, `Game_Step` 0.

Read after a call, as the original reads them: the party list after the
sleep; the leader's facing, the jitter and the pending area's cells after
`Rand`; the saved trip after `Flags_Clear`. The leader's x, z and the area
word are read before `Rand`.

### 1.3 Mode 5's steps (`GameMode5_Steps` `0x656A84`)

| # | PC | Name | Bytes | PSX | What |
|--:|---|---|--:|---|---|
| 0 | `0x495EA0` | `GameMode5_TurnSense` | 0x17 | `0x80198A60` | `Encounter_PartyTurnSense`, `Field_ObjectsFrame`, `Field_LoadingFrame`, step up |
| 1 | `0x495EC0` | `GameMode5_Turn` | 0xCF | `0x80198AA4` | below |
| 2 | `0x495F90` | `GameMode5_ToPlaces` | 0x31 | `0x80198C1C` | `Encounter_PartyToPlaces` (al), the two frames; step up when `File_LoadDone` (asked first), that al and `Field_Kind2Hold` 0 |
| 3 | `0x495FD0` | `GameMode5_Load` | 0xF1 | `0x80198C94` | below |
| 4 | `0x4960D0` | `GameMode5_Script` | 0x5D | `0x80198E54` | `Encounter_PartyScriptOnce` (al), the two frames; with `File_LoadDone` and that al: `MoveScript_F3Divisor` 0, `0x904AE9` and `0x904AA0..A4` 0, `Party_Count(1)`, step up, its al to `0x904AB0` |
| 5 | `0x42E370` | `Battle_Frame` (ours, battle_phases) | | `0x801D1014` | |
| 6 | `0x496130` | `GameMode5_Place` | 0x1C | `0x80198EFC` | `Party_PlaceAtSlots`, `Party_ScriptTicks`, the two frames, step up |
| 7 | `0x496150` | `GameMode5_Leave` | 0xD7 | `0x80198F48` | below |

**`GameMode5_Turn`**: `Encounter_PartyTurn` (al), the two frames; once it
answers non-zero: `Port_DroppedCall(1)`; then the flags byte `0x904AE5`
(`EventBattle_Records` +0's, set by `Field_StartEventBattle`) and the dword
`0x904AAC` (its low byte the formation) are read once:

- bit 0 clear: `MoveScript_F3Divisor` 0x40; the upper words of
  `Field_Kind2Z` (`0x905E62`) and `Field_Kind2X` (`0x905E66`) stepped by the
  s8 pair `0x656AA4[2 * formation]` (+1 to z, +0 to x);
  `MoveScript_FAWord` = (the dword `0x939860` - `MapView_Elevation`) sar 4;
- bit 4 clear: `PartySet_Select(0x90412C & 0x7F, formation bit 1 ? 2 : 1)`
  and the formation read again;

then step up, `Draw_OtSlot` 4, `Draw_SortOnX` = formation bit 0,
`MapView_Redraw` 2.

**`GameMode5_Load`**: `Field_PartyLoad(1)`, `Encounter_PartyAtPlaces`, the
two frames; `Music_FadeOutStop(0xA)` unless `0x904AE5` bit 6 or `Music_Track`
0xFF; the battle's file and track: an event battle (`0x904AAA` non-zero) by
its record - `EventBattle_Records[4 * id + 3]` indexes the word pairs at
`0x64DECC` (file, track; stride 4), the id read again after the load - else
DAT 0xD2 and track 0x97 below chapter 8 (`Cond_ByteFA`, s8), DAT 0xD3 and
0x99 from it; `Music_Play(track, 0xA)`; `Window_ResetAll`; step up;
`0x904AE9` 0, the word `0x904AA8` 0, the bytes `0x904AA0..A4` 0.
`EventBattle_Records` (56 records of 4) ends where the pairs begin
(`0x64DDEC + 0xE0 = 0x64DECC`).

**`GameMode5_Leave`**: with `Field_Kind2Hold` 0 and `File_LoadDone` (in that
order): `Draw_OtSlot` 6; `Field_PartyLoad(0)`; `Party_PlacesByList`;
`Field_EdgeBits` 0; unless `0x904AE5` bit 1,
`MapView_SetElevation(AreaMap_Elevation(Field_Kind2X, Field_Kind2Z))`; the
field's track again - `Music_Play(Music_Track, 0xA)` - unless `0x904AE5`
bit 6 (read again after the elevation), `0x904AE8` bit 3 or the track 0xFF;
`0x904AE5` and the event battle 0; `Field_ZoneCounterRoll(0)`;
`Field_AfterBattleTally`. Either way `Party_ScriptTicks` and the two frames;
after the first branch `Game_Mode` 2, `Game_Step` 0, `Field_Request` 0.

### 1.4 `Sound_MusicPlaying` `0x587C20`

Five bytes: `jmp Music_IsPlaying` (`0x5A7020`, ours since round six,
[`save-menu.md`](save-menu.md)). Every register is handed on, and that
matters to its target: `Music_IsPlaying` tests the slot its own `push ecx`
made when `IDirectSoundBuffer::GetStatus` writes nothing. Ours is a naked
entry that saves ecx and edx around the lookup of the target (the address
`0x5A7020`, so `BOF3X_ORIGINAL=Music_IsPlaying` still reaches Capcom's) and
jumps. Its two callers are `GameMode3_Enter` (`0x495C27`) and
`GameMode3_Leave` (`0x495D26`), both behind the word `0x7E0678` being 0x6E.
No PSX twin: the PSX asks the CD player (`0x8015E050`) instead.

## 2. The PSX twins, read

`GameMode_Handlers`' twin is `0x801C8B0C` (its `symbols.toml` evidence);
entries 3..6 read from the sibling's GAME.EMI section 0 capture
(`9d00fd19...`, capstone MIPS, 2026-10-05) are `0x801985E4`, `0x801989D4`,
`0x80198A24`, `0x80199170`. Mode 3's step table is `0x801C8B3C`
(`0x80198620`, `0x8019A368`, `0x801986EC`, then the same four jitter bytes
as the PC's) and mode 5's `0x801C8B4C` (eight entries, step 5 `0x801D1014`,
then the approach bytes). Each pair was read side by side: the same calls in
the same order, but for the port's file numbers (menu 0x269 -> 0x31E,
the community file 0x258 -> 0x12A) and its music (the PSX queries the CD and
fades through `0x8015DEE8` / `0x8015DE8C` where the port asks
`Sound_MusicPlaying` and calls `Music_FadeOut` / `Music_FadeIn`). The sibling's
`names/*.toml` and `symbols.toml` name nothing in `0x80198000..0x8019A000`,
so no name was transferred. **`pairs_propagated.json` has two of these
wrong** (callers tier): `0x80198620` -> `0x4964E0` (it is `0x495BC0`;
`0x4964E0`'s twin is `0x801995C8`, also listed) and `0x80198E54` ->
`0x5885D0` (it is `0x4960D0`).

## 3. The tables

| Table | Entries | Count from |
|---|---|---|
| `GameMode3_Steps` `0x656A74` | `GameMode3_Enter`, `Menu_Frame`, `GameMode3_Leave` | the step word's writers: step 0 + 1, the field menu's + 1 (`FieldMenu_TopBarInput` `0x589C92` / `0x589CCA`), step 2 back to 0. The next four bytes `0x656A80` are the facing jitter (s8, `Rand & 3`), not a pointer |
| `GameMode5_Steps` `0x656A84` | the eight of section 1.3 | steps 0..4 and 6 + 1, the battle's + 1 for step 5 (`BattleExtra_Leave` `0x42D755`, `BattleEnd_Finish` `0x4318B4`), step 7 back to 0. The next eight bytes `0x656AA4` are the approach pairs; `GameMode_ShopSteps` starts at `0x656AAC` |

The step word's writers: a scan of `.text` for every instruction storing to
the word `[0x66C7EA]` (55 sites by a byte search and a decode at each hit:
`inc`, `mov` of 0, 1, 3, 5 or a register),
each attributed to its function by `symbols.toml`.

Two byte tables are kept as constants, not named: the jitter `0x656A80` (four
s8, indexed `Rand & 3`) and the approach pairs `0x656AA4` (s8 x, z by the
formation byte, unbounded - section 6).

## 4. The fuzz (`mode_rest_fuzz.cpp`)

`BOF3X_SHADOW=mode_rest`, at start-up, through `scenario_harness::Run` in
field mode - the harness group R3G took modes 8..11 with. Fourteen byte
copies, every call or tail jump leaving one re-aimed at a recorder (the rows
by capstone, 2026-10-05); the two step tables swapped for recording handlers
while it runs. Shapes: the dispatchers and steps `kState`;
`Sound_MusicPlaying` `kCall` with `ret_mask 0xFFFFFFFF` (both callers `test
eax, eax`). 4,000 rounds a function; `BOF3X_PM_ONLY=<name>` runs the clones
whose name holds it.

- **Callees listed** beyond the standard sets: the group's own
  `Sound_MusicPlaying` (`kFlag`) and `Music_IsPlaying` by its address
  (`kFlag`); `Menu_Frame`, `CommuSim_RollOffers`, `Window_ResetAll`,
  `Field_FrameScripted`, `Field_ObjectsFrame`, `Encounter_PartyTurnSense`,
  `Encounter_PartyAtPlaces`, `Party_PlaceAtSlots`, `Party_ScriptTicks`,
  `Party_PlacesByList`, `Field_AfterBattleTally`, `Look_PadControl`,
  `GameMode_LookEnd` (`kPhase`); `Encounter_PartyTurn`, `_PartyToPlaces`,
  `_PartyScriptOnce` (`kFlag`: each caller keeps al in bl and tests it);
  `Menu_WaitTransition`, `Field_WaitTransition` (one byte), `PartySet_Select`
  (two bytes, `field_event.cpp` reads both as bytes), `Field_ZoneCounterRoll`
  (the whole word).
- **Effects** (re-listings of standard rows, masks unchanged): the harness's
  own disturbance reaches a group's cells on about one call in 24 - too
  seldom for a read that follows one call of one branch, which three controls
  first showed (15, 16, 44 passed). So `Rand` moves the leader's facing and the
  pending area's cells; `Flags_Clear` the saved trip; `AreaMap_Elevation`
  `0x904AE5` bit 6, `0x904AE8` bit 3 and the track; `PartySet_Select` the
  formation; `LoadDatFile` the event battle.
- **Regions** beyond field mode's: `Game_Mode` / `Game_Step` (effect mode's,
  not field mode's), `0x7DEE44`, the word `0x7E0678`, `0x903860`,
  `0x905B60..63`, `Draw_SortOnX`, `Draw_OtSlot`, the battle bytes
  `0x904AA0..0x904AEF`, `0x939860`, `0x937F82`. None overlaps a standard one.
- **Seeds**: each dispatcher's `Game_Step` inside its table (mode 6: 0 half
  the time); `0x7DEE44`; the word `0x7E0678` at and around 0x6E; for the way
  back from the menu, `0x929F11`, `0x905B60`, `0x905B61`, `0x904152` 0 often
  and the area word at and around 0xBD; the formation's low byte 0..3 two
  times in three; `Field_Kind2Hold` 0 often; `Music_Track` 0xFF often; the
  event battle 0, below 56 or any; `Cond_ByteFA` at 7, 8, 9, 0, -1, 0x7F,
  -0x80. The group's disturbance moves fifteen cells read again after a call.

Result (2026-10-05, final build):

    shadow      mode_rest self-test: 56000 rounds over 14 functions (4000 each), 237566 calls to the stand-ins, 0 MISMATCHES; 23702 bytes of state (48 regions) and the stand-ins' log compared

Under `BOF3X_SHADOW='*'` (every module's fuzz, one process), narrow and with
`BOF3X_WIDE=1`, 2026-10-05: both `self-test only: done`, `inject: 10023 ours,
0 left original by BOF3X_ORIGINAL`, no `MISMATCH` line in either log;
`mode_rest` 56,000 rounds, 237,495 calls, 0 mismatches in each (the call
count differs from the module run alone by where the shared random stream
stands when it starts).

Every callee of the fourteen was called by the originals (the coverage line:
`Music_FadeOut` 222 and `Music_FadeIn` 336 the rarest, `AreaMap_Elevation` 855,
`Rand` 703), every table entry as a handler (each of the eight battle steps
about 500 times, mode 3's two of ours about 1,320).

**What the fuzz cannot see**: anything a callee really does (each function is
tested alone against its copy); the upper bytes of the arguments the
original pushes as whole registers (masked to what each callee reads, the
callee rows cited); ecx at `Sound_MusicPlaying` (no recorder reads it - see
section 6).

## 5. Controls

`controls.py` in the group's scratch: each plant anchored on a unique string
of `mode_rest.cpp`, the DLL rebuilt, the shadow run under
`BOF3X_PM_ONLY=<filter>`, the file restored and rebuilt at the end. **49 of 49
refused**, every one exit 3 on `MISMATCH` lines (count of mismatching rounds
of 4,000 a function, first differing round):

| # | Filter | Plant | Refused |
|--:|---|---|---|
| 1 | `GameMode3_Run` | step 1 dispatched as 2 | 1,375 (round 8) |
| 2 | `GameMode3_Run` | mode 5's table | 4,000 (0) |
| 3 | `GameMode3_Enter` | the menu file + 1 | 4,000 (0) |
| 4 | `GameMode3_Enter` | `Menu_WaitTransition(0)` for 1 | 4,000 (0) |
| 5 | `GameMode3_Enter` | menu byte +4 = 2 | 3,878 (0) |
| 6 | `GameMode3_Enter` | `Gfx_ClutStripDirty` not set | 3,989 (0) |
| 7 | `GameMode3_E` | the music word 0x6F | 2,013 (1) |
| 8 | `GameMode3_Leave` | `Field_WaitTransition(0)` for 1 | 4,000 (0) |
| 9 | `GameMode3_Leave` | the party set unmasked | 4,000 (0) |
| 10 | `GameMode3_Leave` | jitter by `Rand & 7` | 364 (16) |
| 11 | `GameMode3_Leave` | home with flags 5 | 1,308 (1) |
| 12 | `GameMode3_Leave` | `Field_Request` not cleared | 935 (2) |
| 13 | `GameMode3_Leave` | the asked area's x and z swapped | 1,001 (0) |
| 14 | `GameMode3_Leave` | area 0xBE for 0xBD | 267 (19) |
| 15 | `GameMode3_Leave` | the saved area read before `Flags_Clear` | 434 (4) |
| 16 | `GameMode3_Leave` | the facing read before `Rand` | 124 (32) |
| 17 | `GameMode4_Run` | bit 0 for bit 1 | 2,019 (1) |
| 18 | `GameMode4_Run` | `0x905B82` = 0x11 | 2,027 (0) |
| 19 | `GameMode4_Run` | no scripted frame when it ends | 2,027 (0) |
| 20 | `GameMode5_Run` | step 5 dispatched as 6 | 492 (9) |
| 21 | `GameMode5_Run` | the step masked to two bits | 1,975 (4) |
| 22 | `GameMode5_TurnSense` | no step up | 4,000 (0) |
| 23 | `GameMode5_TurnSense` | the frames first | 4,000 (0) |
| 24 | `GameMode5_Turn` | sar 3 | 1,327 (3) |
| 25 | `GameMode5_Turn` | the select's mode by formation bit 0 | 675 (21) |
| 26 | `GameMode5_Turn` | the formation not read again | 446 (13) |
| 27 | `GameMode5_Turn` | the z step from the next pair | 754 (31) |
| 28 | `GameMode5_Turn` | divisor 0x80 | 1,327 (3) |
| 29 | `ToPlaces` | the hold test inverted | 1,809 (2) |
| 30 | `ToPlaces` | `File_LoadDone` asked second | 1,299 (0) |
| 31 | `GameMode5_Load` | chapter 8 takes the first file | 234 (12) |
| 32 | `GameMode5_Load` | the record not read again | 518 (3) |
| 33 | `GameMode5_Load` | `Music_Play` fade 0xB | 4,000 (0) |
| 34 | `GameMode5_Load` | the word `0x904AA8` kept | 4,000 (0) |
| 35 | `GameMode5_Load` | bit 5 for bit 6 | 954 (0) |
| 36 | `Script` | the count + 1 | 1,765 (3) |
| 37 | `Script` | divisor 1 | 1,765 (3) |
| 38 | `Script` | `Party_Count(0)` | 1,765 (3) |
| 39 | `GameMode5_Place` | the two calls swapped | 4,000 (0) |
| 40 | `GameMode5_Place` | no step up | 4,000 (0) |
| 41 | `GameMode5_Leave` | the elevation's x and z swapped | 895 (6) |
| 42 | `GameMode5_Leave` | bit 2 for bit 3 | 264 (12) |
| 43 | `GameMode5_Leave` | `Field_ZoneCounterRoll(1)` | 1,769 (6) |
| 44 | `GameMode5_Leave` | `0x904AE5` not read again | 112 (6) |
| 45 | `GameMode5_Leave` | `Field_Request` kept | 1,764 (6) |
| 46 | `GameMode6_Run` | the step test inverted | 4,000 (0) |
| 47 | `GameMode6_Run` | no loading frame | 4,000 (0) |
| 48 | `Sound_MusicPlaying` | the answer + 1 (call, inc, ret) | 4,000 (0) |
| 49 | `Sound_MusicPlaying` | 0 without the call | 4,000 (0) |

15, 16 and 44 passed before the stand-in effects of section 4 and are refused
since; the table is the final build's run. `GameMode5_Turn`'s filter also runs
`GameMode5_TurnSense` (a substring), and 21 plants the shared dispatcher with
mode 5's filter.

## 6. Latent defects (Capcom's, described, not fixed)

- **Unchecked step dispatchers**: `GameMode3_Run` and `GameMode5_Run` index
  their tables by the word `Game_Step`, unbounded. Past mode 3's three the
  original jumps through the jitter bytes (`0x100FF` read as an address),
  past mode 5's eight through the approach bytes. Every writer of the word
  keeps it inside (section 3). Ours aborts with a message past each table,
  as R3G's dispatchers do.
- **The approach pairs unbounded**: `GameMode5_Turn` reads the s8 pair
  `0x656AA4[2 * formation byte]`; the table holds four pairs before
  `GameMode_ShopSteps` begins, and nothing here bounds the byte. A formation
  of 4 or more steps the battle's position by the bytes of the shop's step
  pointers and beyond. What values `0x904AAC` takes is not read here
  (`Encounter_PartyTurnSense` indexes `0x660B3C` by it too).
- **The event battle's records unbounded**: `GameMode5_Load` indexes
  `EventBattle_Records` (56 records) by the byte `0x904AAA` and the pairs at
  `0x64DECC` by the record's byte +3, neither checked.
- **Stale upper bytes passed**: the original pushes whole registers for
  `PartySet_Load`'s three list bytes, `PartySet_Select`'s set,
  `Field_ChangeArea`'s area word and flags byte, and the area asked by
  number (`xor dx, dx; mov dl, al`, the upper half of edx stale). Each callee
  reads only the byte or word (the rows in `scenario_harness.cpp` and
  `field_event.cpp`); nothing to level.
- **`Sound_MusicPlaying` hands ecx on**: in mode 3 ecx at the jump is
  whatever `Menu_WaitTransition` or `Field_WaitTransition` left - ours,
  compiled, so already undefined under ours before this group, and read by
  `Music_IsPlaying` only when `GetStatus` fails without writing. Ours keeps
  ecx through the jump; no recorder can see it (no control).
- **Behaviour that reads oddly, kept**: the entry fades the music *out* when
  the word `0x7E0678` is 0x6E and the music is *not* playing, the way back
  fades it *in* on the same test. As read; the PSX's twin makes a CD query
  and a fade at the same two places.

**Needs a ledger entry: none.** Every function is a faithful replacement;
the aborts past the step tables follow R3G's precedent (no reachable state
takes them).

## 7. The rebinding

`grep -rn -i` of the fourteen addresses and the two tables in `src`: no
reference in code, only comments - `battle_phases.cpp` 92 ("the tail jump
at 0x495E98"), `inventory_ops.h` 6 ("the intro's state machine at
0x495E90"), `field_e2.cpp` 153 (a return address `0x4961F2`), `save_menu.cpp`
268 ("the tail jump at 0x587C20"), `mode_states.cpp` 2 / 328 (the
neighbours). Each cites an address beside what it does; left as they are.
Nothing to rebind. `Sound_MusicPlaying` is called by name through
`SH_CALL` from the group's own two callers.

## 8. Live coverage

These starts were never armed, so no trace names them. The routes run them
by the code, and the reach of callees whose only caller is here says which
(their `symbols.toml` evidence):

| Mode | Reached on | Evidence |
|---|---|---|
| 3 (steps 0 and 2) | the world map route; every route that opens the field menu (`field_menu.txt`, `menu_screens.txt`) | `Menu_WaitTransition` (callers mode 3's steps only): world map route, 2 calls; `Menu_Frame` 38 |
| 4 | the attract cycle | `Field_FrameScripted` (one caller, `0x495E83`): `hidden_b` 2,997 calls |
| 5 (steps 0..4; 5..7 by the steps' own path) | the combat route; every recorded fight (`combat*.txt`, `whelpBoss.txt`, `bossAndFlash.txt`, ...) | `Encounter_PartyTurnSense` / `_PartyTurn` / `_PartyToPlaces` / `_PartyAtPlaces` / `_PartyScriptOnce` (each one caller, here): combat route 1, 6, 16, 1, 42 calls; `Battle_Frame` reached |
| 6 | the world map route | `Look_PadControl` (callers mode 6 and mode 11) 176 calls, `GameMode_LookEnd` (caller mode 6 only) 18 |
| `Sound_MusicPlaying` | **none recorded** | the read pass armed `0x587C20` over all eleven runs: not entered, so the word `0x7E0678` was never 0x6E at a menu's entry or exit on any route |

The two-way trip of `GameMode3_Leave` and the area asked by number are not
known to be on any route (which menu choice sets `0x905B60` / `0x905B61` is
not read here). The coordinator's state hash after the merge compares
`.data` frame for frame on every route above.

## 9. For `analysis/calltrace/entries_logic.txt`

Not appended (the brief: another agent is restructuring the main checkout's
file tonight). Thirteen lines for the coordinator, the read extents;
`00587C20 5` is already listed:

```
00495BB0 F
00495BC0 82
00495C50 202
00495E60 28
00495E90 F
00495EA0 17
00495EC0 CF
00495F90 31
00495FD0 F1
004960D0 5D
00496130 1C
00496150 D7
00496230 1B
```

`004959F0 1BC` (`GameMode_Field`) is already the code's size, so no host line
changes.
