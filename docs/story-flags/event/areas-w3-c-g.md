# Event flags: areas of world 3, files c to g

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Scope: the event-flag bank at `0x904030` (`Cond_Flags + 0xA0`, save block
`+0x650`) as reached from `src/game/area_w3b.cpp`, `area_w3c.cpp`,
`area_w3d.cpp`, `area_w3e.cpp`, `area_w3f.cpp` and `area_w3g.cpp`.

How each file reaches the bank (checked at each file's helper definitions):

- `area_w3b.cpp`: no `Flags_*` call and no byte access in `0x904030..0x904057`.
  It has no event-flag sites.
- `area_w3c.cpp:45`, `area_w3e.cpp:49`, `area_w3f.cpp:53`: `StoryFlags()` =
  `Cond_Flags + 0xa0`, the event bank.
- `area_w3d.cpp`: no helper; every call spells `Mem(Img(Cond_Flags_addr) + 0xa0)`.
- `area_w3g.cpp:64`: `Bank()` = `Cond_Flags + 0xa0`, the event bank.
  `Bank904000()` (`area_w3g.cpp:65`) is `Cond_Flags + 0x70`, the chapter-14 row,
  and is not listed here.
- `FlagRow()` (`area_w3e.cpp:51`, `area_w3f.cpp:55`) is the current chapter row
  (`Chapter_FlagRow`), and `Cond_Flags + 0x68` (`area_w3f.cpp:744`) is the
  chapter-13 row. Those sites are not listed here.
- The bytes `Cond_Flags + 0xCC..0xCF` and `+0xD2..0xD4` that some functions read
  or write are past the bank's end (`+0xC7`), so they are not flags.

The Op column also uses `toggle` for `Flags_Toggle`, which inverts the bit.
`tools/story_flags.py` does not list the toggle sites.

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 50 | set | Area127_ClearActive80 | area_w3c.cpp:192 | Area 127. Set (0x32) when the leader's byte `+0x89` is 5, then `MoveCmd_TestFB(0x4E, 0x33)` and the running object's `+0` = 0. |
| 136 | set | Area128_TailLeave56 | area_w3c.cpp:254 | Area 128, tail kind 56, state 0: `Party_DropIn(6)`, then flag 0x88 set and the tail cleared. Tail 56 state 0 is armed by object trigger 61 (`Area128_Trigger61`). |
| 137 | set | Area128_TailLeave56 | area_w3c.cpp:258 | Area 128, tail kind 56, state 0xA: flag 0x89 set, then `Field_ChangeArea(0x79, ...)` to area 121. State 0xA is armed by choice answer 0 (`Area128_ChoiceArmTail56`, message 0x49). |
| 67 | set | Area128_InitPatches | area_w3c.cpp:283 | Area 128 init: when the previous area is 0x79 (121), flag 0x43 is set, 0x42 cleared and the patch chain applied. |
| 66 | clear | Area128_InitPatches | area_w3c.cpp:284 | Area 128 init, same branch: flag 0x42 cleared when the area is entered from area 121. |
| 67 | test | Area128_InitPatches | area_w3c.cpp:288 | Area 128 init: with the chapter byte `Cond_ByteFA` above 10 and flag 0x43 set, map cells (0x1F..0x20, 0x39) become 0xC0 and (0x1F..0x20, 0x3A) become 0xA1. |
| 152 | set | Area130_TailGiveItem | area_w3c.cpp:394 | Area 130, tail kind 63, step 1: after an item from a six-entry pick is added (step 0, message 0x49), sound 0x106 and flag 0x98 set, then the tail disarmed. |
| 119 | clear | Area131_TailLeave34 | area_w3c.cpp:503 | Area 131, tail kind 34, state 3: `Field_ScriptFlags ^ 0x16`, tail disarmed, the byte `0x904152` (`AreaW3c_CampFlag`) = 0, flag 0x77 cleared, `Field_ChangeArea(0x79, ...)` to area 121. Tail 34 is armed by object trigger 15. |
| 55 | set | Area135_ChoiceFlag37 | area_w3d.cpp:194 | Area 135, choice 3 (`Area135_Choices[3]`): answer 0 sets flag 0x37. |
| 55 | clear | Area135_ChoiceFlag37 | area_w3d.cpp:196 | Area 135, choice 3: any answer other than 0 clears flag 0x37. |
| 12 | clear | Area135_ClearFlagC | area_w3d.cpp:206 | Area 135, handler 0 (`Area135_Handlers[0]`): flag 0xC cleared and the tail's sub byte (a count) set to 0. |
| 12 | test | Area135_StepHook | area_w3d.cpp:566 | Area 135 step hook: in zones 4, 6, 7 (`Cond_ByteFD`) with flag 0xC set and the party near the marker object, `Party_DropIn(0)`. |
| 42 | toggle | Area135_Tail18 | area_w3d.cpp:602 | Area 135, tail kind 18, state 5: when the party is not in the box (`Area135_PartyInBox` 0), flag 0x2A toggled, sound 0x203, a 10-frame wait. |
| 12 | test | Area135_Tail18 | area_w3d.cpp:615 | Area 135, tail kind 18, state 0xA: flag 0xC set: the count in the tail's sub byte printed into message 0x84. |
| 12 | set | Area135_Tail18 | area_w3d.cpp:622 | Area 135, tail kind 18, state 0xA: flag 0xC clear: message 0x83 and flag 0xC set. |
| 12 | clear | PlaceMarker | area_w3d.cpp:769 | Area 135 init (`Area135_Init`, area_w3d.cpp:779): in zone 4 entered from zone 1, the marker placed by `EventOp_Bx`, flag 0xC cleared, sub byte 0. Also reached from area_w3d.cpp:785 under DIV-0180 when the marker was never placed. |
| 41 | toggle | Area136_Tail19 | area_w3e.cpp:268 | Area 136, tail kind 19, state 10 (reached when `KeyItem_Has(6)`): message 0x82, flag 0x29 toggled; the next states drop in and change to area 0x88 (136). Paired with the current chapter row's flag 0x2E, set at area_w3e.cpp:264 and cleared at :291. |
| 77 | set | Area139_FlagIfLeader89Is5 | area_w3e.cpp:321 | Area 139: when the leader's byte `+0x89` is 5, flag 0x4D set, `MoveCmd_TestFB(4, 0xD)`. |
| var | toggle | Area139_CellHook | area_w3e.cpp:332 | Area 139 cell hook: the matched entry of `Area139_CellEntries` gives the flag number in its byte `+3`; that flag toggled, sound 0x204. |
| 84 | set | Area140_FlagIfLeader89Is567 | area_w3e.cpp:364 | Area 140: when the leader's byte `+0x89` is 5, 6 or 7, flag 0x54 set, `MoveCmd_TestFB(2, 7)`. |
| 102 | test | Area140_FollowAndAct | area_w3e.cpp:383 | Area 140 handler: flag 0x66 clear skips the handler (script position + 4); set, the running object follows the leader and the action button runs `Area140_CellHook`. |
| 102 | set | Area140_Tail41 | area_w3e.cpp:447 | Area 140, tail kind 41, state 0: `Party_DropIn(0)`, `Field_ScriptFlags | 0x1270`, leader `+0x48` = 1, flag 0x66 set. Armed by choice answer 1 (`Area140_ChoiceArmTail41`). |
| 102 | clear | Area140_Tail41 | area_w3e.cpp:483 | Area 140, tail kind 41, state 7: `Field_ScriptFlags & 0xED8F`, leader `+0x48` = 0, flag 0x66 cleared, encounter roll reset, tail disarmed. |
| 102 | test | Area140_StepHook | area_w3e.cpp:509 | Area 140 step hook: zone 0, flag 0x66 set and the position x 0x558000, z high word 0xA..0xB: tail state 5 (which runs to state 7, the clear). |
| var | test | Area140_CellHook | area_w3e.cpp:531 | Area 140 cell hook: the flag number is byte `+3` of the matched `Area140_CellEntries` entry; already set, the hook does nothing. |
| var | set | Area140_CellHook | area_w3e.cpp:549 | Area 140 cell hook: unless a party member stands in the entry's rectangle (then tail 41 state 0xA, message 1), the entry's flag set, `MoveCmd_TestFB(+0, +1)`, and with `+4` an effect of kind 0x71 that holds the flag number in its `+0xB`. |
| var | clear | Area140_Effect71End | area_w3e.cpp:600 | Area 140, effect kind 0x71, state 2 (after 0xD2 frames or `Field_Request` 5): the flag in the effect's byte `+0xB` (copied from `Area140_CellEntries +3`) cleared, `MoveCmd_TestFB` at its cell. |
| 125 | set | Area141_ChoiceFlag7D | area_w3e.cpp:724 | Area 141, choice 0: flag 0x7D set for every answer; answer 0 then arms tail 52 at state 0x32 (message 5), else message 6. |
| 122 | test | Area141_InitCells | area_w3e.cpp:755 | Area 141 init, zone 1: runs 2 and 3 painted on while flag 0x7A is clear, off while set (test in `PaintPairByFlag`, area_w3e.cpp:740). |
| 123 | test | Area141_InitCells | area_w3e.cpp:756 | Area 141 init, zone 1: runs 4 and 5 by flag 0x7B, as above. |
| 124 | test | Area141_InitCells | area_w3e.cpp:757 | Area 141 init, zone 1: runs 6 and 7 by flag 0x7C, as above. |
| 122 | set | Area141_Tail52 | area_w3e.cpp:847 | Area 141, tail kind 52, state 0xE (after message 4 and `Kind2_Place(2)`, counter 0 at 1): flag 0x7A set, runs 2 and 3 off. |
| 123 | test | Area141_Tail52 | area_w3e.cpp:870 | Area 141, tail kind 52, state 0x1E (`FlagRunsOff`, area_w3e.cpp:766): flag 0x7B already set ends the tail (state 0x14). |
| 123 | set | Area141_Tail52 | area_w3e.cpp:870 | Area 141, tail kind 52, state 0x1E (`FlagRunsOff`, area_w3e.cpp:770): flag 0x7B set, runs 4 and 5 off, a 0x3C-frame wait. |
| 124 | test | Area141_Tail52 | area_w3e.cpp:874 | Area 141, tail kind 52, state 0x28 (`FlagRunsOff`, area_w3e.cpp:766): flag 0x7C already set ends the tail. |
| 124 | set | Area141_Tail52 | area_w3e.cpp:874 | Area 141, tail kind 52, state 0x28 (`FlagRunsOff`, area_w3e.cpp:770): flag 0x7C set, runs 6 and 7 off. |
| 45 | test | Area145_StepHook | area_w3f.cpp:722 | Area 145 step hook, zone 4: with flag 0x2D set, at x high 9..0xB, z high 0x62..0x64 and a standing pose, `Party_DropIn(3)`. |
| 44 | set | Area145_Tail20 | area_w3f.cpp:778 | Area 145, tail kind 20, state 0xA (armed by the cell hook, which needs chapter-13 row flag 9): flag 0x2C set, sound 0x200, `Kind2_Place(0)`, an effect of kind 0x13. |
| 45 | test | Area145_Tail20 | area_w3f.cpp:798 | Area 145, tail kind 20, state 0xC: when `Area145_TrailStart` answers 0 and flag 0x2D is clear, sounds 0x205 and 0x207 and state 0x1E (the sequence that sets 0x2D); otherwise state 0x14. |
| 44 | clear | Area145_Tail20 | area_w3f.cpp:825 | Area 145, tail kind 20, state 0x16: flag 0x2C cleared, tail disarmed. 0x2C is held only while tail 20 runs from state 0xA. |
| 46 | test | Area145_Tail20 | area_w3f.cpp:840 | Area 145, tail kind 20, state 0x1A (armed by the zone-0 step hook): after `Field_ChangeArea(0x91, ...)`, flag 0x2E clear plays sound 0x20C. |
| 46 | set | Area145_Tail20 | area_w3f.cpp:842 | Area 145, same branch: flag 0x2E set, so sound 0x20C plays only the first time. |
| 45 | set | Area145_Tail20 | area_w3f.cpp:872 | Area 145, tail kind 20, state 0x22 (after transitions 8 and 9 and an effect of kind 0x3E): flag 0x2D set, `Field_ChangeArea(0x91, 0xA0000, 0x630000, 0x80)`. |
| 51 | test | Area146_StepHook | area_w3f.cpp:1023 | Area 146 step hook, zone 1: with flag 0x33 set, at x high 0x44..0x46, z high 0x2A..0x2C and a standing pose, `Party_DropIn(0)`. |
| 70 | clear | Area146_ClearFlag46 | area_w3f.cpp:1053 | Handler in area 146's block, used as area 148 `+0x3C[1]` and area 167 `+0x3C[1]`: flag 0x46 cleared and `Cond_ByteFE` = 0. |
| 89 | set | Area148_ChoiceSetFlag59 | area_w3g.cpp:234 | Area 148, choice 1: answer 0 gives message 5 and sets flag 0x59; other answers no message. |
| 70 | set | Area148_Tail31 | area_w3g.cpp:266 | Area 148, tail kind 31, state 1: when the script byte (`kScriptVar`) reaches 0x18, tail disarmed, flag 0x46 set, `Field_ChangeArea(0x95, ...)` to area 149. |
| 90 | set | Area148_Tail31 | area_w3g.cpp:284 | Area 148, tail kind 31, state 0xC: when the script byte reaches 0x20, flag 0x5A set, timer 0x20, then a second kind-0x13 effect. |
| 106 | toggle | Area148_Tail31 | area_w3g.cpp:303 | Area 148, tail kind 31, state 0x14: flag 0x6A toggled, sound 0x204. |
| 106 | test | Area148_Tail31 | area_w3g.cpp:305 | Area 148, same state: flag 0x6A now set gives `Gfx_ClutAdjust(0, 0x380, -6, -4, -4)`; clear gives `Gfx_ClutAdjust(0, 0x180, 0, 0, 0)`. |
| 106 | test | Area148_DrawBeam | area_w3g.cpp:493 | Area 148 beam drawer: draws nothing while flag 0x6A is clear. |
| 138 | clear | Area150_Tail58 | area_w3g.cpp:899 | Area 150, tail kind 58, state 0x16 (after `Party_HealJoined` and a sound stream): `Music_Play(0x95, 0x10)`, chapter-14 row flag 3 cleared and event flag 0x8A cleared, tail disarmed. |

52 sites: 48 with a literal flag number, 4 through a data table.

## What each flag means here

"Saves" refers to the "event flags:" lines of `docs/story-flags/observed-rows.md`.
Saves 20, 21 and 22 are the late saves (chapters 14, 10 and 15).

- **12 (0xC)**: area 135. Set by tail 18 state 0xA (message 0x83); while set, the
  same state prints a count (message 0x84) and the zone 4/6/7 step hook drops the
  party in near the marker. Cleared by handler 0 and by the zone-4 init from
  zone 1. Reading, not checked: a "count running" mark local to area 135. Not set
  in any save.
- **41 (0x29)**: toggled by area 136 tail 19 when key item 6 is held (key item 6
  is added by area 135 tail 18 state 0). No reader in these files. Set in saves
  20..22.
- **42 (0x2A)**: toggled by area 135 tail 18 state 5. No reader in these files.
  Set in saves 20..22.
- **44 (0x2C)**: area 145; set at tail 20 state 0xA, cleared at state 0x16. A
  "sequence running" mark, reading, not checked. No reader in these files. Not
  set in any save, which fits a mark that is cleared again.
- **45 (0x2D)**: area 145; set by tail 20 state 0x22. It opens the zone-4 step
  hook drop-in and changes which branch tail 20 state 0xC takes. Set in saves
  20..22. Reading, not checked: a one-way progress flag for area 145.
- **46 (0x2E)**: area 145; set the first time tail 20 state 0x1A runs, to play
  sound 0x20C once. Set in saves 20..22. (Area 136's flag 0x2E is the chapter
  row's, not this one.)
- **50 (0x32)**: set by area 127 when the leader's `+0x89` is 5. No reader in
  these files. Set in saves 20..22.
- **51 (0x33)**: tested only, by the area 146 zone-1 step hook (drop-in). Set
  somewhere else. Set in saves 20..22.
- **55 (0x37)**: area 135 choice 3; answer 0 sets, other answers clear. No reader
  in these files. Set in saves 20..22.
- **66 (0x42)**: cleared by area 128's init when the area is entered from area
  121. Set somewhere else. Not set in any save.
- **67 (0x43)**: set by area 128's init when entered from area 121; with chapter
  byte above 10 it patches four map cells. Set in saves 20..22. Reading, not
  checked: 0x42 and 0x43 are a pair, 0x43 replacing 0x42.
- **70 (0x46)**: set by area 148 tail 31 just before the change to area 149;
  cleared by a handler on areas 148 and 167 (with `Cond_ByteFE` = 0). Not set in
  any save.
- **77 (0x4D)**: set by area 139 when the leader's `+0x89` is 5. No reader here.
  Set in saves 20..22.
- **84 (0x54)**: set by area 140 when the leader's `+0x89` is 5, 6 or 7. No reader
  here. Set in saves 20..22.
- **89 (0x59)**: set by area 148 choice 1, answer 0. No reader here. Set in saves
  20..22.
- **90 (0x5A)**: set by area 148 tail 31 state 0xC. No reader here. Set in saves
  20..22.
- **102 (0x66)**: area 140; set at tail 41 state 0, cleared at state 7. While set,
  the follow-and-act handler and the step hook run. A temporary mode flag,
  reading, not checked. Not set in any save, which fits.
- **106 (0x6A)**: area 148; toggled by tail 31 state 0x14, and it decides the
  palette adjustment and whether the beam is drawn. Set in saves 20..22.
- **119 (0x77)**: cleared by area 131 tail 34 state 3 with the byte `0x904152`
  before the change to area 121. Set somewhere else. Set in saves 9, 18 and 19
  (chapters 1, 4 and 5) and not in saves 20..22, which fits a flag that this
  site clears.
- **122 (0x7A)**, **123 (0x7B)**, **124 (0x7C)**: area 141; each set once by tail
  52 (states 0xE, 0x1E, 0x28) and each turns off two cell runs (2-3, 4-5, 6-7),
  also on the init in zone 1. 0x7B and 0x7C, once set, end tail 52 at once.
  122 is set in save 22 only; 123 and 124 in no save.
- **125 (0x7D)**: set by area 141 choice 0 for any answer. No reader here. Not set
  in any save.
- **136 (0x88)**: set by area 128 tail 56 state 0 (after `Party_DropIn(6)`). No
  reader here. Not set in any save.
- **137 (0x89)**: set by area 128 tail 56 state 0xA before the change to area 121.
  No reader here. Not set in any save.
- **138 (0x8A)**: cleared by area 150 tail 58 state 0x16 together with chapter-14
  row flag 3. Set somewhere else. Not set in any save.
- **152 (0x98)**: set by area 130 tail 63 after it gives an item. No reader here.
  Set in saves 20..22. Reading, not checked: an "item given" mark.
- **var (Area139_CellEntries +3)**: toggled by area 139's cell hook. Numbers in a
  data table.
- **var (Area140_CellEntries +3)**: area 140's cell hook sets the entry's flag
  and an effect of kind 0x71 clears it again after 0xD2 frames, so these flags
  are temporary. Numbers in a data table.

None of these sites runs an event battle. Only flag 70 crosses files in this
set: area 148 (`area_w3g.cpp`) sets it and a handler in area 146's block
(`area_w3f.cpp`), used by areas 148 and 167, clears it. Every other flag here is
read and written inside one area.

## Unresolved

- The flag numbers in `Area139_CellEntries` (byte `+3` of each 4-byte entry,
  `at::kArea139CellCount` entries) and in `Area140_CellEntries` (byte `+3`,
  stride `at::kArea140CellStride`) are game data. They need a read of the table
  (not done here, by rule) or a run.
- Flags 51, 66, 119 and 138 are only tested or cleared here; the setters are in
  other files.
- Flags 41, 42, 44, 50, 55, 77, 84, 89, 90, 125, 136, 137 and 152 are only set
  (or toggled) here; what reads them is not in these files. Their meaning is
  unknown from this code.
- The meaning of the area 141 cell runs (`at::Run(n)`) and the area 128 map-cell
  values 0xC0 / 0xA1 needs a run to describe in player terms.
- `AreaW3c_CampFlag` (`0x904152`, cleared with flag 119) is past the bank and
  is not a bit flag here; what it holds is not settled in these files.
