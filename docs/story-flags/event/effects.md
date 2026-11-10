# Event flags: the effect files

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: every `src/game/effect_*.cpp` except the `*_fuzz.cpp` files (1a..1g,
2a..2g, 3a..3d, 4a..4f, 5a..5g, 6a..6d, gte). The event bank is
`Cond_Flags + 0xA0` (0x904030, save block `+0x650`, flags 0..319). In these
files it is spelled:

- inline `At(::bof3::Img(Cond_Flags_addr) + 0xa0)` (2a, 2g, 3b, 4b, 5a, 5c,
  5d, 5e, 5f, 6b, 6c, 6d) or `P(...)` of the same address (3d);
- `Story()` in `effect_3c.cpp` (line 79);
- `Flags()` in `effect_5b.cpp` (line 194). `Row9()` beside it (line 195) is
  chapter 9's row and is left out;
- local wrappers: `On()` in `effect_5a.cpp` (line 156, a dword read),
  `Sub11Done()` / `Sub12Done()` in `effect_5c.cpp` (lines 877, 882),
  `Sub22Flag()` in `effect_5d.cpp` (line 1372), `FlagSet()` in
  `effect_6b.cpp` (line 762).

Left out as not the event bank: `Row()` in `effect_2d.cpp` (line 71) and the
`Chapter_FlagRow` tests in `effect_3d.cpp` (319, 333) and `effect_4b.cpp`
(1030) are the current chapter's row; `Cond_Flags + 0x10`, `+ 0x18`, `+ 0x48`,
`+ 0x50`, `+ 0x70` in 5a, 5b, 5c, 5d, 5e, 6c are chapter rows 2, 3, 9, 10 and
14; the word at `Cond_Flags + 0xCC` (0x90405C) read in `effect_6c.cpp:590` is
past the bank's 40 bytes. The fourteen files 1a..1g, 2b, 2c, 2e, 2f, 3a,
4a, 4c..4f, 5g, 6a and gte have no event-flag site (grep for every spelling
above found none).

"Set in the late saves" below means the three chapter-14-row saves at the end
of [`observed-rows.md`](../observed-rows.md).

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 11 | test | EffectKind28_Start | effect_2a.cpp:349 | Kind 0x28, state 0. Clear: state 1 (wait); set: state 2 (beam drawn). |
| 11 | test | EffectKind28_WaitFlag | effect_2a.cpp:356 | Kind 0x28, state 1. Once set, back to state 0, which then goes to the beam state. |
| 11 | test | EffectKind28_Beam | effect_2a.cpp:365 | Kind 0x28, state 2: draws the beam and pushes party members off its row (`EffectKind28_PushParty`). Once clear, back to state 0. So the beam stands while the flag is set. |
| 13 | test | EffectKind2A_Start | effect_2a.cpp:623 | Kind 0x2A, state 0. Clear: state 2 (two beams); set: state 1 (one beam). |
| 13 | test | EffectKind2A_Beam | effect_2a.cpp:635 | Kind 0x2A, state 1: one beam at z 0x118000, its row pushed. Once clear, state 2. |
| 13 | test | EffectKind2A_Beams | effect_2a.cpp:648 | Kind 0x2A, state 2: two beams at z 0xD8000 and 0x158000, rows pushed. Once set, state 1. The flag picks which beams stand. |
| 42 | test | EffectKind18Sub20_Start | effect_2g.cpp:280 | Kind 0x18 sub-kind 0x20, state 0. Set: starts lowered (height -0x180, state 3); clear: raised (-0x90, state 1). Flag is `kSub20Flag` = 0x2A (`effect_2g_callees.h:28`). |
| 42 | test | EffectKind18Sub20_WaitSet | effect_2g.cpp:294 | Sub-kind 0x20, state 1. Once set: sound 0x202, then it lowers (state 2) and `MoveCmd_TestFB(0x27, 0x2F)`. |
| 42 | test | EffectKind18Sub20_WaitClear | effect_2g.cpp:317 | Sub-kind 0x20, state 3. Once clear: `MoveCmd_TestFB(0x27, 0x2F)`, sound 0x202, it rises. The flag holds the object down. |
| 79 | toggle | EffectKind65_WaitInput | effect_3b.cpp:478 | Kind 0x65, state 2 (reached only when `Game_BattleCount` is a multiple of 5): a held button plays sound 0x202 and flips the flag (`Flags_Toggle`), then a screen shake and `Msg_OpenScript(1)`. Flag is `kKind65Flag` = 0x4F (`effect_3b_callees.h:44`). Op is a toggle, not a plain set or clear. |
| 92 | test | EffectKind6F_Start | effect_3c.cpp:487 | Kind 0x6F, state 0. `Cond_ByteFD` 2 with the flag clear: state 1 (the line along z). |
| 93 | test | EffectKind6F_Start | effect_3c.cpp:491 | Kind 0x6F, state 0. `Cond_ByteFD` 3 with the flag set: state 2 (the line along x). Else state 3. |
| 92 | test | EffectKind6F_AlongZ | effect_3c.cpp:544 | Kind 0x6F, state 1: draws the line at x 0x5C0000 and pushes the party. Once set: sound 0x206, state 3 (line gone). |
| 93 | test | EffectKind6F_AlongX | effect_3c.cpp:576 | Kind 0x6F, state 2: draws the line at z 0x440000 and pushes the party. Once clear: sound 0x206, state 3. |
| 92 | test | EffectKind6F_Watch | effect_3c.cpp:585 | Kind 0x6F, state 3. `Cond_ByteFD` 2 with the flag clear: sound 0x206, back to state 0. |
| 93 | test | EffectKind6F_Watch | effect_3c.cpp:589 | Kind 0x6F, state 3. `Cond_ByteFD` 3 with the flag set: sound 0x206, back to state 0. |
| 126 | test | EffectKind7D_SetMap | effect_3d.cpp:686 | Kind 0x7D's map writer (also called by `Area170_Init`). Clear: each of the 7 x 9 cells is on or off by the three dial grids; set: every cell is off (height 0). Flag is `kDialsSolvedFlag` = 0x7E (`effect_3d_callees.h:58`). |
| 145 | test | EffectKind8A_Lines | effect_4b.cpp:1076 | Kind 0x8A, state 1: draws three lines at x 0x100000. Once set: state 2, which zeroes six map cells and releases the effect. |
| 4 | test | On | effect_5a.cpp:158 | Kind 0x18 sub-kind 5's test reads the bank's first dword: "on" when flags 4..7, masked by bits 4..7 of the word +0x3A, equal bits 0..3 of +0x3A. Callers: `EffectKind18_05_Start` (321), `_Idle` (343), `_Pulse` (392); on starts or keeps a palette pulse. Which of flags 4..7 count comes from +0x3A. |
| 5 | test | On | effect_5a.cpp:158 | As the row for flag 4: the same dword read. |
| 6 | test | On | effect_5a.cpp:158 | As the row for flag 4: the same dword read. |
| 7 | test | On | effect_5a.cpp:158 | As the row for flag 4: the same dword read. |
| var | test | DoorFlag | effect_5a.cpp:168 | Kind 0x18 sub-kind 7 (door): tests `Cond_Flags + 8 * row`, row from byte table 0x65DDB4[k] (`kFlagRows7`), bit from 0x65DDB8[k] (`kFlagBits7`), k the door +0xB (0..2). It is the event bank only if the row byte is 0x14..0x18. Callers `EffectKind18_07_Start` (694: set opens at once) and `_Wait` (712: once set, the door slides open). |
| 7 | test | EffectKind18_1F_Start | effect_5a.cpp:527 | Kind 0x18 sub-kind 0x1F, state 0. Set: state 3 (palette cycle runs at once); clear: state 1. |
| 7 | test | EffectKind18_1F_Wait | effect_5a.cpp:537 | Sub-kind 0x1F, state 1. Once set: brighten (state 2), then the cycle. |
| var | test | EffectKind18_06_Wait | effect_5a.cpp:584 | Kind 0x18 sub-kind 6, state 1. Flag is +0x34, written in state 0 (`EffectKind18_06_Start`, 573) from byte table 0x65DB84 (`kFlags6`) by the set index +0x36 (0..2). Once set: sounds 0x202 and 0x201, state 2 (turn). |
| var | clear | EffectKind18_06_Turn | effect_5a.cpp:603 | Sub-kind 6, state 2: after 0x10 frames the level +0x3A steps to (+0x3A + 1) & 3, the flag +0x34 is cleared and it waits again. The flag is a one-shot "turn once" request. |
| 13 | test | EffectKind18Sub0D_Start | effect_5b.cpp:306 | Kind 0x18 sub-kind 0x0D, state 0. Column 0 (+0x36 = 0) with the flag set: frame 3, state 3 (lit). Other columns use `AreaW1c_Count48`, not a flag. |
| 13 | test | EffectKind18Sub0D_Wait | effect_5b.cpp:319 | Sub-kind 0x0D, state 1. Column 0 with the flag set: sound 0x20D, opens. |
| 13 | test | EffectKind18Sub0D_Idle | effect_5b.cpp:343 | Sub-kind 0x0D, state 3. Column 0: the light stays while the flag is set, closes when clear. |
| 17 | test | EffectKind18Sub0B_Start | effect_5b.cpp:494 | Kind 0x18 sub-kind 0x0B, state 0. Waits until set; then sound 0x202, 18 tiles rise and fade, and the effect releases. |
| var | test | EffectKind18Sub0C_Start | effect_5b.cpp:581 | Kind 0x18 sub-kind 0x0C, state 0. Flag from byte table 0x65DFC4 (`kSub0CFlags`, 8 entries) by the column +0x36 (`Sub0CFlag`, line 566). Set: frame 3, state 3; clear: frame 0, state 1. |
| var | test | EffectKind18Sub0C_Wait | effect_5b.cpp:592 | Sub-kind 0x0C, state 1. Same flag; once set: sound 0x201, opens. |
| var | test | EffectKind18Sub0C_Idle | effect_5b.cpp:609 | Sub-kind 0x0C, state 3. Same flag; once clear: closes. |
| 34 | test | Sub11Done | effect_5c.cpp:878 | Kind 0x18 sub-kind 0x11's end test (callers `EffectKind18Sub11_Start` 972, `_Wait` 974). Set: done. |
| 33 | test | Sub11Done | effect_5c.cpp:879 | Same test: if flag 34 is clear, flag 33 set with `Cond_ByteFA` (signed) at most 7 is also done. Done: `Cond_ByteFE` = 1, the tiles at cells (0x45..0x46, 0x3C) open and sink; at start, done releases at once. |
| 32 | test | Sub12Done | effect_5c.cpp:882 | Kind 0x18 sub-kind 0x12's end test (callers `EffectKind18Sub12_Start` 988, `_Wait` 990). Set: the same tile opening as sub-kind 0x11. |
| 28 | clear | EffectKind18Sub1B_Run | effect_5d.cpp:648 | Kind 0x18 sub-kind 0x1B: a 15-frame run with two `MoveCmd_TestFB` calls on its cell; at frame 0xF it clears the flag and releases. Reading, not checked: the flag is a request that this effect consumes. |
| var | test | Sub22Flag | effect_5d.cpp:1372 | Kind 0x18 sub-kind 0x22. Flag is +0x34, written in state 0 from byte table 0x65E348 (`kSub22Flags`, 4 places) by place +0x36. Callers: `EffectKind18Sub22_Start` (1391: set starts 0x80 lower), `_WaitSet` (1408: once set, lowers), `_WaitClear` (1436: once clear, rises). |
| var | test | EffectKind18Sub22_SetMap | effect_5d.cpp:1453 | Sub-kind 0x22's map writer: same flag +0x34; its answer picks the row of table 0x65E364 written to the cells. |
| 40 | test | EffectKind18Sub23_Start | effect_5e.cpp:191 | Kind 0x18 sub-kind 0x23, state 0. Set: releases at once; clear: waits. Flag is `kSub23Flag` = 0x28 (`effect_5e_callees.h:37`). |
| 40 | test | EffectKind18Sub23_WaitFlag | effect_5e.cpp:201 | Sub-kind 0x23, state 1. Once set: slides (+0x30 up to 0x100) and releases. |
| 90 | test | EffectKind18Sub39_Start | effect_5e.cpp:1213 | Kind 0x18 sub-kind 0x39, state 0. Set: drawn open (+0x30 0x80, state 3); clear: closed, waits. Flag is `kSub39Flag` = 0x5A (`effect_5e_callees.h:38`). |
| 90 | test | EffectKind18Sub39_WaitFlag | effect_5e.cpp:1228 | Sub-kind 0x39, state 1. Once set: sound 0x208 (unless `Field_Request`), opens. |
| 84 | test | EffectKind18Sub28_Start | effect_5f.cpp:357 | Kind 0x18 sub-kind 0x28 (a gate), state 0. Clear: state 5 (wait for the flag). Set: the gate works by the leader's distance (opens when near, closes when far). |
| 84 | test | EffectKind18Sub28_WaitFlag | effect_5f.cpp:403 | Sub-kind 0x28, state 5. Once set: sound 0x200, state 2 (opening). |
| var | test | EffectKind18Sub42_WaitFlag | effect_5f.cpp:518 | Kind 0x18 sub-kind 0x42, state 1. Flag is +0xB, from byte 3 of the 4-byte place record at 0x65E8E0 (`kSub42Places`, 3 places) by +0x36. When the flag differs from the variant +8 and no member is near: sound 0x205, raises. |
| var | test | EffectKind18Sub42_WaitFlagBack | effect_5f.cpp:595 | Sub-kind 0x42, state 4. Same flag +0xB; when "flag clear" differs from +8: sound 0x205, lowers. |
| 143 | test | EffectKind18Sub49_WaitCue | effect_5f.cpp:770 | Kind 0x18 sub-kind 0x49 (a gate), state 1. Set, with the leader near the gate: a step on (open). `Cond_ByteFE` 0x20 is the other trigger. |
| 103 | test | EffectKind18Sub3C_WaitFlag | effect_5f.cpp:1022 | Kind 0x18 sub-kind 0x3C, state 1. Once set: sound 0x200, rises, then `MoveCmd_TestFB(9, 0xF)` and releases. |
| 79 | test | FlagSet | effect_6b.cpp:763 | Kind 0x18 sub-kind 0x41's helper, called with 0x4F only. `EffectKind18Sub41_Turn` (794): at the end of the turn, sound `kSoundOpen` when set, `kSoundShut` when clear. `EffectKind18Sub41_Hold` (816): while set, two extra Capcom calls (0x5100B0 / 0x5101C0) run each frame. |
| 80 | set | EffectKind18Sub41_Hold | effect_6b.cpp:812 | Sub-kind 0x41, state 3: at frame +9 = 0x17 it sets the flag and calls `MoveCmd_TestFB(0x13, 0x25)`. |
| var | test | EffectKind18Sub51_Wait | effect_6c.cpp:914 | Kind 0x18 sub-kind 0x51, state 1. Variant +0x36 of 2, 3 or 4 tests flag 0x7A + (variant - 2), so 122, 123 or 124. Once set: the cells fade. Variant 0 uses chapter 14's row, variant 1 `Cond_ByteFE`. |
| 128 | test | EffectKind18Sub5A_Start | effect_6c.cpp:1459 | Kind 0x18 sub-kind 0x5A, state 0. Set: state 5 (the second cycle); clear: waits for `Cond_ByteFE` 2 and sweeps the map rows. |
| 128 | set | EffectKind18Sub5A_Set | effect_6c.cpp:1483 | Sub-kind 0x5A, state 3, after the sweep: sets the flag and writes cells (0x11..0x12, 0x61). So the flag records that the sweep is done. |
| 129 | test | EffectKind18Sub5B_Start | effect_6c.cpp:1578 | Kind 0x18 sub-kind 0x5B, state 0. Set: state 6 ("opened already"); clear: `Cond_ByteFE` 3, waits. |
| 129 | set | EffectKind18Sub5B_Lift | effect_6c.cpp:1652 | Sub-kind 0x5B, state 5: after the three parts slide and lift, sets the flag, `MoveCmd_TestFB(0x30, 0x71)`, releases. |
| 129 | clear | EffectKind18Sub5B_Reopen | effect_6c.cpp:1661 | Sub-kind 0x5B, state 6: once `Cond_ByteFE` is 2, clears the flag, sound, parts drawn lifted. |
| var | set | EffectKind18Sub5C_Place | effect_6d.cpp:271 | Kind 0x18 sub-kind 0x5C (a two-panel door), state 0. Flag is +8, from byte table 0x65F38C (`kSub5CFlags`) by the variant +0xB (0..1). Sets it when placed. |
| var | clear | EffectKind18Sub5C_WaitNear | effect_6d.cpp:292 | Sub-kind 0x5C, state 1. Leader near (or `Cond_ByteFE` 1): clears flag +8, sound, opens. |
| var | set | EffectKind18Sub5C_Close | effect_6d.cpp:324 | Sub-kind 0x5C, state 4. Closed again: sets flag +8. Reading, not checked: the flag says "this door is shut" while the effect lives. |

## What each flag means here

- **4, 5, 6, 7**: tested as a group by kind 0x18 sub-kind 5 (`On`, effect_5a.cpp:158) to start a palette pulse; which of them count comes from the effect's +0x3A. Flag 7 alone gates sub-kind 0x1F's palette cycle (527, 537). Nothing here sets them. Set in the late saves.
- **11**: while set, kind 0x28's beam stands and pushes the party (effect_2a.cpp:349..365). Not set here. Not set in any observed save.
- **13**: picks kind 0x2A's beams (set: one, clear: two) and lights column 0 of sub-kind 0x0D (effect_2a.cpp:623..648, effect_5b.cpp:306..343). Not set here. Set in the chapter 5 and 6 saves and the late saves.
- **17**: when set, sub-kind 0x0B's tiles rise and the effect ends (effect_5b.cpp:494). Not set here. Not set in any observed save.
- **28**: cleared by sub-kind 0x1B when its 15-frame run ends (effect_5d.cpp:648). Not set here; meaning unknown beyond that. Not set in any observed save.
- **32**: set: sub-kind 0x12's tiles open (effect_5c.cpp:882). Not set here. Set in most saves from the later chapter 1 rows onward (not in one chapter 1 save, observed-rows.md line 85).
- **33**: set, with `Cond_ByteFA` at most 7: sub-kind 0x11's tiles open (effect_5c.cpp:879). Not set here. Not set in any observed save.
- **34**: set: sub-kind 0x11's tiles open (effect_5c.cpp:878). Not set here. Not set in any observed save.
- **40**: set: sub-kind 0x23 slides away and releases (effect_5e.cpp:191, 201). Not set here. Not set in any observed save.
- **42**: set: sub-kind 0x20's object lowers; clear: it rises (effect_2g.cpp:280..317). Not set here. Set in the late saves.
- **79**: toggled by a held button in kind 0x65 (effect_3b.cpp:478, every fifth battle count); read by sub-kind 0x41 for its sound and two extra Capcom calls (effect_6b.cpp:794, 816). Set in the late saves.
- **80**: set by sub-kind 0x41 at frame 0x17 of its hold state (effect_6b.cpp:812). Not tested here. Set in the late saves.
- **84**: set: sub-kind 0x28's gate works; clear: it waits shut (effect_5f.cpp:357, 403). Not set here. Set in the late saves.
- **90**: set: sub-kind 0x39 opens or starts open (effect_5e.cpp:1213, 1228). Not set here. Set in the late saves.
- **92**: with `Cond_ByteFD` 2 and the flag clear, kind 0x6F draws a line along z and pushes the party; set removes it (effect_3c.cpp:487, 544, 585). Not set here. Not set in any observed save.
- **93**: with `Cond_ByteFD` 3 and the flag set, kind 0x6F draws a line along x; clear removes it (effect_3c.cpp:491, 576, 589). Not set here. Set in the late saves.
- **103**: set: sub-kind 0x3C rises and releases (effect_5f.cpp:1022). Not set here. Set in the late saves.
- **122, 123, 124**: set: sub-kind 0x51 variants 2, 3, 4 fade their cells (effect_6c.cpp:914). Not set here. 122 is set in the last late save only; 123 and 124 in none.
- **126**: set: all 7 x 9 cells of kind 0x7D's dial map are off (effect_3d.cpp:686); the constant's name in the source calls it "dials solved". Not set here. Set in the late saves.
- **128**: set by sub-kind 0x5A after its row sweep (effect_6c.cpp:1483); tested at start to skip to the second cycle (1459). Not set in any observed save.
- **129**: set by sub-kind 0x5B when its parts lift away (effect_6c.cpp:1652), cleared when it reopens (1661), tested at start (1578). Not set in any observed save.
- **143**: set: sub-kind 0x49's gate opens when the leader is near (effect_5f.cpp:770). Not set here. Set in the late saves.
- **145**: set: kind 0x8A's lines go and six map cells are freed (effect_4b.cpp:1076). Not set here. Set in the late saves.
- **var** (table-driven): sub-kind 6 (table 0x65DB84), sub-kind 7's doors (0x65DDB4 / 0x65DDB8, only if the row is 0x14..0x18), sub-kind 0x0C (0x65DFC4), sub-kind 0x22 (0x65E348), sub-kind 0x42 (0x65E8E0 byte 3), sub-kind 0x5C (0x65F38C). See Unresolved.

## Unresolved

- Flag numbers from tables in `BOF3.exe`, not read here (the brief forbids a read of game data):
  - sub-kind 6: byte table 0x65DB84 (`kFlags6`, 3 entries) by +0x36; the flag goes to +0x34 (effect_5a.cpp:573) and is tested (584) and cleared (603).
  - sub-kind 7: row bytes 0x65DDB4 and bit bytes 0x65DDB8 (3 doors) by +0xB (effect_5a.cpp:168). Whether any row byte is 0x14..0x18 (the event bank) or below (a chapter row) is not known.
  - sub-kind 0x0C: byte table 0x65DFC4 (`kSub0CFlags`, 8 entries) by +0x36 (effect_5b.cpp:566).
  - sub-kind 0x22: byte table 0x65E348 (`kSub22Flags`, 4 entries) by +0x36 (effect_5d.cpp:1390).
  - sub-kind 0x42: byte 3 of the 4-byte records at 0x65E8E0 (`kSub42Places`, 3 records) by +0x36 (effect_5f.cpp:501).
  - sub-kind 0x5C: byte table 0x65F38C (`kSub5CFlags`, 2 entries) by +0xB (effect_6d.cpp:270).
- Sub-kind 5 (`On`, effect_5a.cpp:158): which of flags 4..7 matter, and their wanted values, come from each effect's word +0x3A, which the spawner writes. Not known which areas spawn it or with what word.
- Sub-kind 0x51 (effect_6c.cpp:914): which variants the areas spawn is not known, so whether 122..124 are all used is open.
- Flag 79 is toggled (`Flags_Toggle`), which the set/clear/test column does not cover; the row uses "toggle".
- Who sets the flags these effects only test (11, 13, 17, 32..34, 40, 42, 84, 90, 92, 93, 103, 122..124, 126, 143, 145) is outside these files: area scripts or field code. Who sets flag 28, which sub-kind 0x1B clears, is also outside.
- Which area spawns each kind and sub-kind is not shown in these files; tying a flag to a place needs the area files or a run.
