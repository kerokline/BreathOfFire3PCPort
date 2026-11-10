# Save block range A: the head, `+0x000..+0x08F`

Status: reading pass, 2026-11-10. Source only (the reimplementation in this
worktree, `symbols.toml`, `analysis/data_census.tsv`, docs). No save file, no
executable and no game data was opened, so nothing here is MEASURED; where a
sample value is mentioned it is quoted from `docs/save-editor-spec.md` or
`docs/save-interchange.md`.

Addresses: block offset = address - `0x9039E0`. Range `0x9039E0..0x903A6F`.

Tags: **READ** = I read the code that writes or uses the byte. **CITED** = a
name or meaning from someone's note, not checked in code. **UNKNOWN** = no code
found. "Write-only" and "copy-only" are READ with no meaning-bearing reader.

## 0. Three facts that shape the whole range

1. **`+0x00..+0x0F` and `+0x20..+0x23` are snapshots of bytes that live
   elsewhere.** `Save_BuildBlock` (src/game/rest_2c.cpp:373-386, original
   `0x5806F0`) copies the 16 bytes `0x8034E0..0x8034EF` (the scenario chapter
   bytes) into `+0x00..+0x0F`, and the four movement-script counter bytes
   `0x903848..0x90384B` into `+0x20..+0x23`. `LoadMenu_Apply`
   (src/game/save_menu.cpp:1248-1263, original `0x5885D0`) copies them back
   out. Between a save and a load nothing reads the in-block copies; the game
   runs on `0x8034E0..` and `0x903848..`.
2. **`+0x10..+0x19` is a live field state machine, not a copy.** It is the
   field-frame "mode tail" (`Field_ModeTail` / `Field_ModeTailRun`,
   src/game/field_modes.cpp:124 and :128): phase `+0x12`, kind `+0x13`, state
   `+0x14`, argument `+0x15`, timer `+0x16`. It is saved and loaded as it
   stands, so a save records whatever tail was running when it was made, and
   the tail resumes after the load (section 4, finding on save points).
3. **`+0x24..+0x2F` is both the saved position and the "pending area" a new
   chapter writes.** `GameMode_Start` (src/game/mode_flow.cpp:328-346, original
   `0x495840`) always enters the area at `+0x24` with `+0x27`, `+0x28`,
   `+0x2C`. `Save_BuildBlock` fills it from the leader; `Scena00_Start`
   (src/game/scena_sc0.cpp:130-137) and `Scena16_Start`
   (src/game/field_modes.cpp:186-193) fill it at a chapter start, which is how
   a new game reaches its first area.

## 1. Byte map

| Offset | Address | Size | Name / meaning | Writers | Readers | Tag | Conf. |
|---|---|---:|---|---|---|---|---|
| `+0x00` | `0x9039E0` | 1 | Copy of `Cond_ByteFA` `0x8034E0`: the scenario chapter, s8. Indexes the three 20-entry chapter tables (docs/scenario-roots.md section 2) and `Cond_Flags + 8 * chapter` | `Save_BuildBlock` rest_2c.cpp:373 (dword from `0x8034E0`); boot zero `Boot_Task` mode_flow.cpp:685 | `LoadMenu_Apply` save_menu.cpp:1252-1257: back to `0x8034E0`, and **signed** into `Chapter_FlagRow` = `Cond_Flags + 8 * c`. Live byte: `Field_ModeDispatch` field_modes.cpp:118, `Scenario_CallA` field_modes.cpp:145, `Scenario_Load` title_states.cpp:430 (DAT `0x306 + chapter`), `Scenario_CellHook` field_e2.cpp:1093. Live writers: `Scenario_Start` title_states.cpp:407, `Area_Enter` mode_flow.cpp:548-554 | READ | high |
| `+0x01` | `0x9039E1` | 1 | Copy of `Field_StatusBits` `0x8034E1`. Bit 7: advance the chapter by one (16 skipped to 17) at the next area entry (`Area_Enter` mode_flow.cpp:548-556); set by `Field_SetStatus80` from chapter code (scena_sc1/2/3/7/11/15 callees). Bit 5 camera turning, bit 6 idle turn, bit 0 movement variable 9 (symbols.toml `Field_StatusBits`) | `Save_BuildBlock` rest_2c.cpp:373 | `LoadMenu_Apply` save_menu.cpp:1252. Live: `Area_Enter` mode_flow.cpp:548; zeroed by `Scenario_Start` title_states.cpp:413 | READ | high (bit 7), medium (others, from symbols.toml) |
| `+0x02` | `0x9039E2` | 1 | Copy of `0x8034E2`: the chapter's state, s8; every chapter's frame jumps on it (0 start, 1 area set-up, 2 run: docs/scena_sc11.md, docs/mode_states.md) | `Save_BuildBlock` rest_2c.cpp:373 | `LoadMenu_Apply` save_menu.cpp:1252. **Overwritten with 1 on every area entry** before the chapter's frame runs (`Area_Enter` mode_flow.cpp:561), and a load always enters an area (`GameMode_Start` -> `GameMode_Enter` mode_flow.cpp:376-380 -> `Area_Enter`). Zeroed by `Scenario_Start` title_states.cpp:408 | READ | high |
| `+0x03` | `0x9039E3` | 1 | Copy of `0x8034E3`: a per-chapter byte. Chapter 17: the part (`Scena17_Parts`, scena_sc15.cpp:1893-2152); chapter 3: an effect slot kept across frames (scena_sc3.cpp:131, :1654) | `Save_BuildBlock` rest_2c.cpp:373 | `LoadMenu_Apply`; live as listed; zeroed by `Scenario_Start` title_states.cpp:409 | READ | high |
| `+0x04` | `0x9039E4` | 1 | Copy of `MoveScript_Var7` `0x8034E4`: the chapter's current run (state 2 dispatches on it); also movement variable 7 (op F6) | `Save_BuildBlock` rest_2c.cpp:374 | `LoadMenu_Apply` save_menu.cpp:1250; zeroed by `Scenario_Start` title_states.cpp:410 | READ | high |
| `+0x05` | `0x9039E5` | 1 | Copy of `0x8034E5`: the step within the run (field_modes.cpp:314; 323+89 references) | `Save_BuildBlock` rest_2c.cpp:374 | `LoadMenu_Apply`; zeroed by `Scenario_Start` title_states.cpp:412 | READ | high |
| `+0x06` | `0x9039E6` | 2 | Copy of `0x8034E6`, u16: the chapter's timer (scenario_harness.h:50; field_modes.cpp:316) | `Save_BuildBlock` rest_2c.cpp:374 | `LoadMenu_Apply`; zeroed by `Scenario_Start` title_states.cpp:411 | READ | high |
| `+0x08` | `0x9039E8` | 2 | Copy of `Field_StepCountPrev` `0x8034E8`, u16: `Field_StepCount` as of the last field pass | `Save_BuildBlock` rest_2c.cpp:377 | `LoadMenu_Apply` save_menu.cpp:1249. Live: written by `Field_ObjectsFrame` field_frame.cpp:335, read by object_kinds.cpp:237; zeroed by `Scenario_Start` title_states.cpp:414 | READ | high. Explains the spec's "scene byte 8 differs on every save" |
| `+0x0A` | `0x9039EA` | 2 | Copy of `0x8034EA..EB` | `Save_BuildBlock` rest_2c.cpp:377 (upper half of the dword) | `LoadMenu_Apply` save_menu.cpp:1249. No other code reads or writes `0x8034EA..EB`; `Scenario_Start` does not zero them (title_states.cpp:400) | READ, copy-only; meaning UNKNOWN | - |
| `+0x0C` | `0x9039EC` | 4 | Copy of `0x8034EC..EF` | `Save_BuildBlock` rest_2c.cpp:375 | `LoadMenu_Apply` save_menu.cpp:1251-1253. No other access to `0x8034EC..EF` found (grep of literals and of `MoveScript_PartyRecords[0x6C..0x6F]`) | READ, copy-only; meaning UNKNOWN | - |
| `+0x10` | `0x9039F0` | 1 | First byte of the tail block (symbol `ScenaSc7_DrawPool`, a misnomer: the writes it names land at `+3`, `+4`, `+6`) | `Scenario_SubInit` title_states.cpp:443 (0) | none. symbols.toml: one operand in the exe, a store | READ, write-only | high |
| `+0x11` | `0x9039F1` | 1 | `AreaW2a_ModeByte` | `Scenario_SubInit` title_states.cpp:448 (0); `Area76_StepDisarmTail5` area_w2a.cpp:130 (0, with kind 5) | none. symbols.toml: two operands, both stores | READ, write-only | high |
| `+0x12` | `0x9039F2` | 1 | Tail phase (`SaveMenu_NewGameByte`): 0 runs `Scenario_SubInit` once (seeds event flags), then 1. Settled, not redone | `TitleFlow_NewGame` save_menu.cpp:1025 (0), `Scenario_SubInit` :442 (1) | `Field_ModeTail` field_modes.cpp:124 | READ | settled |
| `+0x13` | `0x9039F3` | 1 | Tail kind, s8 (`AreaW0a_TailKind`): `Field_ModeTailRun` jumps through `Field_ModeTailKinds` `0x662CE8` (64 slots, 0 a bare ret) by it every field frame. Unchecked | area code (over 200 sites, 35 files), `Field_LeaderCellEvent` (0x37 on cell 0xAE, 0x1B on 0xA0/0xA1: symbols.toml), `Scena08_Scene2` scena_sc7.cpp:1669 (5), every tail's end `TailDone` field_e2.cpp:1119 (0); `Scenario_SubInit` :444 (0) | `Field_ModeTailRun` field_modes.cpp:128 and the area tails | READ | high |
| `+0x14` | `0x9039F4` | 1 | Tail state, s8 (`AreaW0c_TailState`): each tail kind's state machine | the tails themselves (460 lines in 35 files), arming sites | the tails | READ | high |
| `+0x15` | `0x9039F5` | 1 | Tail argument (`AreaW0c_TailSub`), u8: what the armed tail works on - a message id or 0xFF (`FieldTail_HealAndMenu` field_e2.cpp:1286-1290), a drop-in table index (`FieldTail_DropInMove` :1184), the master (rest_2c.cpp:719), the faerie village entry (rest_4e.h:16), a minigame (rest_4b) | arming sites, `TailDone`, `Scenario_SubInit` :447 | the tails | READ | high |
| `+0x16` | `0x9039F6` | 2 | Tail timer, u16 (`kTailTimer` in 15 files); also the three-dial puzzle's answer word, 1 confirmed / 0xFF cancelled (`EffectKind7D_Input` effect_3d.cpp:519, :529) | tails (`FieldTail_DropInMove` field_e2.cpp:1189-1194, area_w*.cpp), `Scena08_Scene2` scena_sc7.cpp:1668 (1), `Scenario_SubInit` :446 (0) | the same tails | READ | high |
| `+0x18` | `0x9039F8` | 2 | Word zeroed with the tail block | `Scenario_SubInit` title_states.cpp:449 (0) | none found | READ, write-only | high |
| `+0x1A` | `0x9039FA` | 6 | - | none (boot clear only) | none | UNKNOWN | - |
| `+0x20` | `0x903A00` | 4 | Copy of the movement script's four counter bytes `0x903848..0x90384B` (`MoveScript_CounterOps`, ops A0..AF set / test / increment them; movement variables 3..6, area_w0a_callees.h:23). Per-area and per-chapter scratch: choices, trade counters, gates (area_w0b_callees.h:60-62, area_w1b_callees.h:52-54) | `Save_BuildBlock` rest_2c.cpp:378-380, :386 | `LoadMenu_Apply` save_menu.cpp:1254-1263 (back to `0x903848..4B`) | READ | high |
| `+0x24` | `0x903A04` | 2 | Area, u16 (`FieldModes_Pending`) | `Save_BuildBlock` rest_2c.cpp:388 (`Game_AreaNumber`); `Scena00_Start` scena_sc0.cpp:134 (0x18); `Scena16_Start` field_modes.cpp:190 (4) | `GameMode_Start` mode_flow.cpp:336 -> `Field_ChangeArea` | READ | high |
| `+0x26` | `0x903A06` | 1 | No code reads or writes it. All four writers of the pending change skip it (rest_2c.cpp:381-388, scena_sc0.cpp:134-137, field_modes.cpp:190-193); `GameMode_Start` reads `+0x24` as u16 and `+0x27` as a byte (mode_flow.cpp:332-336) | none | none | UNKNOWN | high that the PC ignores it |
| `+0x27` | `0x903A07` | 1 | Entry flags: facing in the low bits (the leader's `+0x08` direction byte, `& 7` indexes `Field_DirectionSteps`, records.h:67), bit 7 = drop-in (sets `Field_ScriptFlags2` bit 15, `Area_Enter` mode_flow.cpp:509-511; `place_party(flags & ~0x80)` :569). Handed whole to `Field_PartySetUp` (field_event.cpp:546) | `Save_BuildBlock` rest_2c.cpp:383 (`ObjTrio[0].direction`); `Scena00_Start` scena_sc0.cpp:137 (3); `Scena16_Start` field_modes.cpp:193 (5) | `GameMode_Start` mode_flow.cpp:332 | READ | high |
| `+0x28` | `0x903A08` | 4 | Position x, s32 16.16 (the leader's `0x802D74` = ObjTrio `+0x34`) | `Save_BuildBlock` rest_2c.cpp:381; `Scena00_Start` scena_sc0.cpp:135; `Scena16_Start` field_modes.cpp:191 | `GameMode_Start` mode_flow.cpp:334 | READ | high |
| `+0x2C` | `0x903A0C` | 4 | Position z, s32 16.16 (`0x802D78` = ObjTrio `+0x38`) | `Save_BuildBlock` rest_2c.cpp:382; `Scena00_Start` scena_sc0.cpp:136; `Scena16_Start` field_modes.cpp:192 | `GameMode_Start` mode_flow.cpp:333 | READ | high |
| `+0x30` | `0x903A10` | 8 | **Chapter scratch, three users** (none checks the others): (a) a saved party - three member ids at `+0x30..+0x32`, the second party list at `+0x33..+0x35` (scena_calls.cpp: `ScenaCall_Party72Saved` :489, `scena_calls_SaveAndLeave` :579, `Scena10_RestoreSaved` :665, `Scena13_SaveParty7` :730, `Scena13_RestoreSaved` :757, `Scena14_SaveLeaveAllPartyA` :835, `Scena14_Party74First` :888, `Scena14_SaveLeaveAllParty7` :900, `Scena14_Party74Other` :933); (b) eight offering counts, one per item 0x4E..0x55, held at 100, written by effect kind 0x5E (`EffectKind5E_Pick` rest_3e.cpp:1040), drawn by `EffectKind5D_DrawBoard` rest_3e.cpp:1107, tested by `Scena10_TallyMet` scena_sc9b.cpp:2066-2073 (needs `+0x30`>=2, `+0x32`>=3, `+0x33`>=2, `+0x35`>=1, `+0x37`>=2), all 8 cleared at scena_sc9b.cpp:1572-1573 and :1651-1652; (c) member ids from chapter 10 run 7 step 1 (scena_sc9b.cpp:857) | as listed | as listed | READ | high. Matches the spec's "6 bytes live" in `+0x30..+0x6F` |
| `+0x38` | `0x903A18` | 56 | - (`+0x38..+0x6F`). Harness region lists (scenario_harness.h:90, boss_harness.cpp:1271) span it; no game code | none (boot clear only) | none | UNKNOWN | - |
| `+0x70` | `0x903A50` | 2 | Checksum. Settled, not redone. In memory the live word is always 0: `Save_BuildBlock` zeroes it (rest_2c.cpp:392) and stores the sum into the staging copy (:412); `LoadMenu_Read` zeroes the staging word before copying (save_menu.cpp:1138) | as said | `LoadMenu_Read` save_menu.cpp:1137, :1146 | READ | settled |
| `+0x72` | `0x903A52` | 2 | Upper half of the dword `LoadMenu_Read` takes as the checksum (save_menu.cpp:1137); only the low word is compared (:1146). Never written | none | read and ignored | READ, ignored | high |
| `+0x74` | `0x903A54` | 4 | - | none | none | UNKNOWN | - |
| `+0x78` | `0x903A58` | 1 | Options row 0, **message speed** 0..2 (docs/config-screen.md section 2: Fast / Med. / Slow). Boot and defaults: 1 | `Boot_Task` mode_flow.cpp:695; `SetDefaults` rest_4f.cpp:136; `ConfigScreen_Rows` rest_4f.cpp:329 (steps 0..2) | `MsgBox_StatePrint` msgbox.cpp:1280 (row of the delay table `0x658E94`); `Config_DrawPanel` rest_4f.cpp:513 | READ; labels CITED | high |
| `+0x79` | `0x903A59` | 1 | Options row 3, **sound** 0 stereo / 1 mono (`Rest4f_Setting3`) | `Boot_Task` :696; `SetDefaults` rest_4f.cpp:132; `ConfigScreen_Rows` :335 (flip) | sound layer `Law` sound_sp.cpp:266, music_seq.cpp:103; `CommuMusic_DrawList` rest_4e.cpp:827; `Config_DrawPanel` | READ | high |
| `+0x7A` | `0x903A5A` | 1 | Options row 1, **window colour** 0..3 (`Config_WindowColour`): a CLUT row, `(s8) v * 32 + 0x10` (`Window_DrawFrame`, symbols.toml) | `Boot_Task`; `SetDefaults` :130; `ConfigScreen_Rows` :330 | 105 lines in 31 files (every window frame); `Save_BuildBlock` copies it to the summary `0x904689` = `+0xCA9` (rest_2c.cpp:399) | READ | high |
| `+0x7B` | `0x903A5B` | 1 | Options row 2, **background** 0..3 (`Config_Background`), `Menu_DrawBackdrop`'s kind | `Boot_Task`; `SetDefaults` :131; `ConfigScreen_Rows` :331 | save_menu.cpp:140, :161, rest_2d.cpp:682, rest_2f.cpp:334, rest_4f.cpp:220; `Save_BuildBlock` copies it to `0x90468A` = `+0xCAA` (rest_2c.cpp:400) | READ | high |
| `+0x7C` | `0x903A5C` | 1 | `Rest4f_Setting5C`. No option row sets it. The PlayStation's op B8 reads its twin as a music gate (sound_sp.cpp:58, :428); the PC original never reads it (symbols.toml: 2 operands, both stores) | `Boot_Task`; `SetDefaults` rest_4f.cpp:133 (both 0) | ours only: `OpB8` sound_sp.cpp:428 | READ, write-only in the original | high |
| `+0x7D` | `0x903A5D` | 1 | `BattleWinStates_LabelGate`: non-zero suppresses the battle command's label banner. No option row sets it | `Boot_Task`; `SetDefaults` :134 (both 0) | `BattleWin_LabelFrame` battle_win_states.cpp:306 | READ | high |
| `+0x7E` | `0x903A5E` | 1 | Options row 4, **dash** 0 manual / 1 auto (`FieldE2_RunDefault`): XORed with the run button | `Boot_Task`; `SetDefaults` :135; `ConfigScreen_Rows` :341 (flip) | `Field_LeaderSetPace` event_ops.cpp:463; `Mode11_ObjectControl` field_e2.cpp:822; rest_4f.cpp:410, :513 | READ | high |
| `+0x7F` | `0x903A5F` | 1 | - (the boot loop stops at `+0x7E`, mode_flow.cpp:696) | none | none | UNKNOWN | - |
| `+0x80` | `0x903A60` | 16 | - (`+0x80..+0x8F`). msgbox's `kCharRecords` `0x903A6F` is a base minus one; the byte read is the one after it (msgbox_callees.h:66, msgbox.cpp:459), so `+0x8F` itself is not read | none | none | UNKNOWN | high |

Every byte of `0x9039E0..0x903A6F` is zeroed at boot (`Boot_Task`
mode_flow.cpp:685, 0x10B0 bytes) and overwritten whole by a load
(`LoadMenu_Read` save_menu.cpp:1140-1144). Those two are left out of the
writer column above.

## 2. The current spec rows (docs/save-editor-spec.md section 7)

| Spec row | Verdict |
|---|---|
| `+0x000` 16, scene state, byte 0 chapter, byte 2 state | **Confirmed and extended.** A snapshot of `0x8034E0..EF`: chapter, status bits, state, per-chapter byte, run, step, u16 timer, u16 previous step count, 6 copy-only bytes. Correction to its use: the saved state byte is overwritten with 1 at the load's area entry, so it does not select the handler that runs first |
| `+0x010` 2, unnamed, UNKNOWN | **Corrected:** both are written (0) and never read: `+0x10` by `Scenario_SubInit`, `+0x11` by it and by area 76's step hook |
| `+0x012` | Settled, not redone. Seen to be the tail phase byte |
| `+0x013` 3, scene event mode, step, selection, CITED | **Confirmed, READ:** field tail kind, state, argument. The argument is a message id, a master, a table index, depending on the kind |
| `+0x016` 10, unnamed, zero on every sample | **Corrected:** `+0x16..+0x17` is the tail timer (also the dial answer), `+0x18..+0x19` a word zeroed with the block, `+0x1A..+0x1F` unused. Zero on the samples fits saves made at a save point, whose tail does not use the timer |
| `+0x020` 4, script variables, CITED | **Confirmed, READ:** the movement script's counter bytes `0x903848..4B`, copied at save and load |
| `+0x024` 2, area | **Confirmed, READ.** Also the new game's and the demo's start area |
| `+0x026` 1, entry kind, CITED | **Corrected:** no PC code reads or writes it. Whatever frost's "entry kind" is, the PC ignores the byte |
| `+0x027` 1, facing | **Confirmed and extended:** entry flags; low bits facing, bit 7 drop-in. This explains the sample value `0x80` |
| `+0x028` 8, position x, z | **Confirmed, READ** (s32 16.16, x then z) |
| `+0x030` 64, unnamed, 6 live | **Partly resolved:** `+0x30..+0x37` is chapter scratch (saved party, offering counts, member ids); `+0x38..+0x6F` has no access |
| `+0x070` checksum | Settled. Noted: the load reads a dword and compares the low word, so `+0x72..+0x73` is read and ignored |
| `+0x072` 6, zero | **Confirmed** no writer; `+0x72..+0x73` read and ignored, `+0x74..+0x77` no access |
| `+0x078` 8, options | **Corrected.** `+0x78` is message speed and `+0x7E` is dash; neither follows the button mapping. The mapping is the nine words at `+0xCBC` (`SaveMenu_SavedButtons`, `LoadMenu_Apply` save_menu.cpp:1217-1224). The two bytes changed with the buttons in the US/PC comparison because the US save also carries its own option choices. frost's `+0x79` sound mono and `+0x7A` window style: confirmed. `+0x7B` background: confirmed. `+0x7C`, `+0x7D`: two bytes no option sets (always 0 in a PC-made save). `+0x7F`: no access |
| `+0x080` 16, unnamed | **Confirmed unused:** no access in `+0x80..+0x8F`. The spec's "2 bytes live across `+0x07B..+0x08F`" are then most probably `+0x7B` and `+0x7E` |

With this reading the samples quoted in docs/save-interchange.md section 3 decode
as: PC-made `01 00 00 00 00 00 00 00` = Med. speed, defaults; US `00 .. 01 00` =
Fast, dash auto; JP `02 00 00 03 00 00 01 00` = Slow, background 4, dash auto.
That is an inference from the code and the doc's bytes, not a measurement.

## 3. What a save editor may edit

**Safe, with range checks (class E or A):**

- `+0x24` area (u16, high byte 0), `+0x27` facing (keep bit 7 clear unless a
  drop-in is meant), `+0x28` / `+0x2C` position. Read only by `GameMode_Start`.
- `+0x78` message speed 0..2, `+0x79` sound 0/1, `+0x7A` window colour 0..3,
  `+0x7B` background 0..3, `+0x7E` dash 0/1. Each is used as a table index or
  CLUT row without a bound (msgbox.cpp:1280, `Window_DrawFrame`), so out-of-range
  values must be refused. Options travel with the save: a load replaces the
  options the player set on the title screen.
- `+0x00` chapter, for chapter jumps (docs/ending-backdrop.md section 1): 0..19,
  not 16 (the demo; `Area_Enter` skips 16). The value is used signed and
  unchecked in `LoadMenu_Apply` (`Cond_Flags + 8 * c`) and `Scenario_Load` (DAT
  `0x306 + c`). With a new chapter, `+0x03..+0x09` should be zeroed as
  `Scenario_Start` does.

**Must agree with another byte:**

- `+0x7A` with the slot summary `+0xCA9`, `+0x7B` with `+0xCAA`
  (`Save_BuildBlock` rest_2c.cpp:399-400). Regenerate on write.
- `+0x00` selects the flag row `Cond_Flags + 8 * chapter` (`+0x5B0 + 8 *
  chapter`) as the current chapter's flags; a chapter change moves which row the
  chapter code reads.
- `+0x01` bit 7 makes the first area entry after the load start the next
  chapter. Clear it when setting a chapter by hand.
- `+0x13..+0x16` with the script flags restored from `+0x74D`
  (`LoadMenu_Read` save_menu.cpp:1145): a tail that set `Field_ScriptFlags`
  bit 0x40 clears it when it ends. Clearing the tail (kind, state, argument and
  timer to 0) without clearing that bit leaves the field locked; keep them as a
  pair, or edit neither.

**Preserve (class P):** `+0x0A..+0x0F` (copied, meaning unknown), `+0x10`,
`+0x11`, `+0x18..+0x1F`, `+0x26`, `+0x30..+0x37` (chapter scratch: editing it
can change which party a later scene restores or whether chapter 10's offering
test passes), `+0x38..+0x6F`, `+0x72..+0x77`, `+0x7C`, `+0x7D`, `+0x7F`,
`+0x80..+0x8F`. `+0x04..+0x09` and `+0x20..+0x23` are mid-scene state; edit only
on the Advanced page.

## 4. Findings, open questions, possible defects

**Finding: a save point's save records its own tail, and the tail finishes
after the load.** `Area22_ArmTailOnYes` (area_w0b.cpp:865-872, one body for
nine areas' choice entries) arms tail kind 0xA, state 5, argument 0xFF.
`FieldTail_HealAndMenu` (field_e2.cpp:1232) goes 5 -> 6 (script flag 0x40 set)
-> 7 (the menu: `Game_Mode` 7, shop object 0xFE, which symbols.toml
`FieldSave_States` names the save point's) and waits in state 7 for `Game_Mode`
2. The save is written from that menu, so such a save should hold
`+0x13..+0x15` = `0A 07 FF`. After the load, `GameMode_Enter` sets mode 2
(mode_flow.cpp:360-368), the tail goes to state 10 (no message for 0xFF), then
11, clears flag 0x40 and ends. **Prediction, to be measured on the samples:**
saves made at that kind of save point carry `0A 07 FF` at `+0x13`; an F12 quick
save (`Save_QuickWrite`, rest_2c.cpp:420) carries whatever tail was running.

**Open questions**

1. `+0x0A..+0x0F` (`0x8034EA..EF`): copied both ways, no other access found in
   the reimplemented source. A sample diff would show whether they are ever
   non-zero; if always zero they are padding of the PSX's 16-byte copy.
2. `+0x26`: frost calls it an entry kind. The PC ignores it; check whether the
   PSX build reads its twin, for conversion (it can be carried as is).
3. The saved run, step and timer (`+0x04..+0x07`) come back on load, but the
   chapter's state is forced to 1 (area set-up). Whether each chapter's
   EnterArea handler resets the run is per chapter and was not checked; a save
   made mid-run could resume that run after the set-up.
4. `+0x30..+0x37` has three unrelated users. Which chapter's data a given save
   holds depends on where it was made; the editor cannot label it without the
   chapter byte, and even then only as "scratch of chapters 9, 10, 13, 14".
5. sound_sp.cpp:57-58 gives the PlayStation config block as `0x80144950..56`
   and the mono byte as `0x80144951`. With the block at `0x801448D4`
   (symbols.toml `CharacterRecords`, delta `0x807BF10C`) those are block
   `+0x7C..+0x82`, four past the PC's `+0x78..+0x7E`; the sibling's notes put
   the window colour at `0x8014494E` = `+0x7A`, matching the PC. The sound
   comment's addresses are probably from another image (JP) or off by four;
   worth a check before conversion code trusts them.

**Possible defects in the original (behaviour as the code reads, not tested)**

- `LoadMenu_Apply` uses the chapter byte signed and unbounded
  (save_menu.cpp:1255): a damaged or hand-made save with `+0x00` >= 0x80 sets
  the chapter flag row below `Cond_Flags`, and `Scenario_Load` loads DAT
  `0x306 + c` for any value. The checksum is the only guard.
- `+0x7A` and `+0x78` are used as a CLUT row and a table index with no bound;
  only the options screen keeps them in range. A converted save with other
  values would draw wrong frames or read past the speed table.
- `+0x7C` and `+0x7D` are options no screen can set on the PC; a PlayStation
  save converted with non-zero values there would silently hide the battle
  command banner (`+0x7D`). Conversion should zero them unless that is wanted.

## 5. Counts

144 bytes. **READ with a meaning: 48** (`+0x00..+0x09`, `+0x12..+0x17`,
`+0x20..+0x25`, `+0x27..+0x37`, `+0x70..+0x71`, `+0x78..+0x7E`). **READ but
write-only, copy-only or ignored: 12** (`+0x0A..+0x0F`, `+0x10`, `+0x11`,
`+0x18..+0x19`, `+0x72..+0x73`). **UNKNOWN, no access: 84** (`+0x1A..+0x1F`,
`+0x26`, `+0x38..+0x6F`, `+0x74..+0x77`, `+0x7F`, `+0x80..+0x8F`). CITED: only
the option labels (docs/config-screen.md) and the bit meanings of `+0x01` other
than bit 7 (symbols.toml).
