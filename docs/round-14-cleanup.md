# Round fourteen's cleanup: the debts the remainder round left

**Status:** IN PROGRESS (2026-10-05) - the round's end, after round fourteen's
last wave. The debts are those of
[`takeover-queue-round14.md`](takeover-queue-round14.md) sections 9 to 13 and
the low items and nits of [`round-14-review.md`](round-14-review.md). Section
1 landed on branch `phase-3/r14end-ea` from the round's tip `e3b98087`; the
other sections are other sessions' and are folded in here by the
coordinator. Nothing in section 1 changes game behaviour, so it wants no
DIVERGENCE entry.

Round fourteen took 1,361 functions in 28 groups (R0A and four waves; 8,648
-> 10,009 ours). The groups of a wave ran side by side, so each called its
neighbours' functions by raw address; the price is section 1.

## 1. The rebinding

**The form** is round ten's, kept by every round's end since
([`round-10-cleanup.md`](round-10-cleanup.md) section 1,
[`round-13-cleanup.md`](round-13-cleanup.md) section 1): a raw constant whose
target is ours reads `bof3::addr::<Name>` from the generated `symbols.gen.h`,
**the value unchanged**, so every fuzz key and every call stands. A
`_callees.h` constant another file keys on stays a constant whose initialiser
names the symbol; each touched header now includes `symbols.gen.h` and its
comment says whose function the target is now. No function body and no call
path changed: never the bare function name, which would bind to our DLL's
function and take the call out of `BOF3X_ORIGINAL`'s and the call trace's
reach ([`round-14-review.md`](round-14-review.md) item 4, CLAUDE.md rule 3).

**Method.** A scratch script (not committed) listed every six-digit hex
literal in `src/game/rest_*` and `window_task_callees.h` whose value is the
`pc` of a `symbols.toml` `[[func]]` or `[[data]]` entry, outside strings and
comments, with the owner's `impl`; a second pass over all of `src/` listed
the literals whose target is a round-fourteen function (an `impl` in
`src/game/rest_*`). At `e3b98087`: 6,147 matches in the round's files - 3,746
on clone byte-check key lines (3,600 ours, 146 Capcom's), 1,917 other lines
naming ours, 414 named data, 70 Capcom's functions - each classed by hand
into section 1.1 or 1.2.

### 1.1 What was rebound: 101 constants in 18 files

| File | Constants | Targets (owner) |
|---|--:|---|
| `rest_1c.cpp` | 28 | the dispatchers' `.data` tables, `PartyFormAction6_Forms` .. `PartyAction9_Form1State0Steps` (R1C's own; the review's nit, below) |
| `rest_2b_callees.h` | 1 | `kModelADraw` -> `MasterFigure_DrawFaded` (R2C) |
| `rest_2c_callees.h` | 3 | `kFigureDraw` -> `Shisu_DrawModel` (R2B), `kPickAsk` -> `MasterScreen_PickMember` (R2D), `kGlyph` -> `BattleEquipWin_DrawBar` (R2H) |
| `rest_2d_callees.h` | 5 | `kMemberPanel` -> `MasterPanel_DrawMember`, `kMemberLabel` -> `MasterPanel_DrawStats`, `kPromptBox` -> `Menu_DrawPanelBox` (R2C); `kItemsWindows` -> `FieldItems_InitWindows`, `kItemsReset` -> `FieldItems_CloseWindows` (R2E) |
| `rest_2e_callees.h` | 5 | `kAbilityUse` -> `FieldAbility_Use` (R2D); `kAbilityWindows` -> `AbilityMenu_InitRecords`, `kAbilitySort` -> `AbilityList_SortBy`, `kAbilityViewWindow` -> `AbilityMenu_InitRecord17`, `kAbilityCloseWindows` -> `FieldMenu_FreeRecords13To18` (R2F) |
| `rest_2f_callees.h` | 2 | `kSwapBytes` -> `FieldMenu_SwapBytes` (R2E); `kConfigMachine` -> `ConfigScreen_Run` (R4F, debt 20) |
| `rest_2g_callees.h` | 1 | `kReserveList` -> `MenuList_ReserveWinDraw` (R2H) |
| `rest_3b_callees.h` | 6 | R3D's: `kMissTail` -> `Effect_NoHitReaction`, `kStatMod` -> `Effect_RollStatStepQuiet`, `kInflictMiss` -> `Effect_RollInflictQuiet`, `kInflict` -> `Effect_RollInflict`, `kHpShare` -> `Effect_HpBasedDamage`, `kResisted` -> `Battle_StatusResisted` |
| `rest_3c_callees.h` | 12 | R3B's: `kSkillByAbility` -> `EffectSlot04_SkillPower`, `kHealByAbility` -> `EffectSlot07_Heal`, `kHealMaxHp` -> `EffectSlot11_HealFull`, `kFlag200` -> `EffectSlot47_MissMark200` (review item 16); R3D's: `kInflict` -> `Battle_InflictStatus`, `kRaiseStat` -> `Effect_StepStatByte`, `kResisted`, `kMissTail`, `kRaiseByAbility`, `kMissInflict`, `kInflictUnlessResisted`, `kHpDamage` (as R3B's six) |
| `rest_3d_callees.h` | 1 | `kAbility6AHelper` -> `Effect_DamageAllHp` (R3C; R3D's tail jump) |
| `rest_3e_callees.h` | 1 | `kR3FRing` -> `EffectKind5F_DrawLineDisc` (R3F; R3E's five sites) |
| `rest_3g_callees.h` | 1 | `kFadeDraw` -> `EffectKindAA_DrawFill` (R3F; R3G's two sites) |
| `rest_4a_callees.h` | 1 | `kCommuCount` -> `CommuName_CountSlots` (R4D) |
| `rest_4b_callees.h` | 4 | `kR4ACountSlots` -> `CommuSim_SumKindsAB`, `kR4ASettle` -> `CommuSim_TickKind5` (R4A); `kR4CAsk` -> `Commu_PushSubscreen` (R4C); `kR4EHand` -> `CommuCursor_DrawArrow` (R4E) |
| `rest_4c_callees.h` | 3 | `kGuessPanel` -> `CommuBoard_DrawRows`, `kGuessRow` -> `CommuBoard_DrawRowCells`, `kSecret` -> `CommuBoard_DrawCells` (R4D) |
| `rest_4d_callees.h` | 14 | `kBoardCell` -> `Commu_DrawCard`, `kBoardPiece` -> `Commu_DrawPiece` (R4C); R4E's `CommuEntry_DrawPanel`, `Commu_DrawPiece6`, `CommuCursor_DrawArrow`, `CommuName_MakeRandom`, `CommuMember_DrawPanel`, `_DrawAll`, `_Count`, `_Nth`, `_DrawFrame`, `Commu_DrawTiledFrame`, `CommuName_CommitMember`, `_CommitEntry` |
| `rest_4e_callees.h` | 1 | `kEntryNth` -> `CommuName_NthSlot` (R4D) |
| `window_task_callees.h` | 11 | `kRecordHandlers`' eight raw entries (`Window_Run`, `Window_Handler1Kinds`, `_2Kinds` (R2F), `BattleWin_Run`, `Window_Handler4Kinds`, `MenuList_Run`, `Window_Handler7Kinds`, `_8Kinds`; debt 9); `kMsgBoxFrameTask` -> `MsgBox_FrameTask`, `kSetDraw` -> `Window_Kind2List`, `kSetCursorDraw` -> `Window_DrawCursor` (older rounds', found by the scan) |

**By debt.** Debt 13 (section 11): R3B's six and R3C's twelve constants,
R3D's `kAbility6AHelper`, R3E's `kR3FRing`, R3G's `kFadeDraw` - every one of
the 59 sites of R3B and R3C, the tail jump and the seven sites of R3E and
R3G reads its constant, so each takes the name through it; R3C's four into
R3B (review item 16's fourth bullet) are in `rest_3c_callees.h`'s twelve.
Debt 20 (section 13): `rest_4a..4e_callees.h`'s 23 and `kConfigMachine`.
Debt 9 (section 10): `kRecordHandlers`. The review's nit "Raw cross-group
constants": R2B's, R2C's, R2D's and R2E's ten, and R1C's dispatcher tables -
**28, not 34** (`rest_1c.cpp:434-489` is 28 lines, one table each).

**R1C's tables** are named `[[data]]` entries already (`ctype = "unsigned
long"`, a `count`), so the generated name is a pointer macro and
`bof3::addr::<Name>` cannot be spelt (round thirteen's 1.2). Each dispatcher
now hands `ByForm` / `ByState` / `ByStep` `reinterpret_cast<U>(<Name>)` - the
macro's pointer, the same address, at run time (the argument is not a
constant expression). `symbols.toml` is unchanged.

**Local names that do not match the symbol** (the value was checked each
time; the constant's name was the group's guess before the target was
read): R2C's `kGlyph` is `BattleEquipWin_DrawBar`; R3B's `kInflict` is
`Effect_RollInflict` while R3C's `kInflict` is `Battle_InflictStatus`; R3C's
`kFlag200` is `EffectSlot47_MissMark200`; R3D's `kAbility6AHelper` is
`Effect_DamageAllHp`; R4B's `kR4ACountSlots` is `CommuSim_SumKindsAB`,
`kR4ASettle` `CommuSim_TickKind5`, `kR4CAsk` `Commu_PushSubscreen`; R4D's
`kBoardCell` is `Commu_DrawCard`, `kMemberHand` `CommuMember_DrawFrame`.
Renaming the constants is left for a tidy pass: it touches the bodies' call
lines.

**Comments.** The "raw until it merges" / "until the round's rebinding" /
"not ours yet" notes the pass met were brought forward (each header's top
comment; `rest_2d.cpp:69`, `rest_3b.cpp:70`; the fuzz callee-row comments of
`rest_2b`, `rest_2d`, `rest_3b`, `rest_3c` (two), `rest_3d`, `rest_3e`,
`rest_4a`, `rest_4e`, one line each). Review item 11's `0x492400` notes
(`rest_3g_callees.h`, `rest_3g.cpp:131`, `rest_3g_fuzz.cpp:228`) already said
ours, opaque and widened at `e3b98087`; only the header's "raw until the
round's rebinding" was left, now gone. `rest_3e_callees.h`'s and
`rest_3e_fuzz.cpp`'s argument lists for `0x480300` still disagree with R3F's
(review item 16's third bullet) - not this pass's.

### 1.2 What stays raw, and why

| Class | Why |
|---|---|
| `CallSite` / `Imm` / `JumpTable` tables in the fuzz files (3,746 literals in the round's files) | the clone byte-check keys: the `disp32` target of Capcom's instruction at that offset, checked against the image before the clone is re-aimed - a fact about the bytes (round ten's rule) |
| A clone row's base and the fuzz's clone selectors (`R1C_DISPATCH`, `R1D_LEAF`, `G_LEAF`, `R2C_LEAF`, ..., `r.clone.base ==`, `base !=`) | the address the image is read from; the row names its own function beside it |
| The fuzz callee rows `{"0x...", at::kX, at::kX, ...}` | already the round-thirteen form: the label the log has always had, the value from the (now named) constant; the third field stays the address, so the raw calls that look the stand-in up by address still find it |
| R2G's `R2G_STATE(0x...)` rows (23), R2D's `kEffectAddress` (10) | table entries read in place (R2G's "own table handlers", the slide states its window kinds jump to; `FieldAbility_Effects`), stood in for by address and labelled by it (`"state 0x..."`); R2D's are its own functions |
| Harness rows keyed by address (`scenario_harness.cpp`'s `FX_RAW` and `{"", 0x..., 0x...}` rows, `boss_harness.cpp`'s, `scenario_harness_fh.cpp`'s) and the older fuzzes' `case 0x...:` stand-in selectors | the `_OURS` form is debts 14 and 21, with the standard rows moved in the same commit (round ten's note: a named key with a raw row is a `Fatal`); not this pass's, and EB edits those files tonight |
| Capcom's targets: `rest_2c`'s `kStrncpy` (`0x5B9450`), `rest_4c` / `rest_4e`'s `kSetPolyF3` (`0x5A7570`), `rest_3e`'s `kSqrt` (`0x5A7A90`) | not ours: the runtime and the platform layer (the platform round's step 2) |
| Named `.data` with a `ctype` in the groups' headers (`rest_2c`'s `kRestStates` .., `rest_1d`'s `kAction9Forms` ..) | the groups' own tables, read in place; the name is a macro for the cell (round thirteen's 1.2). R1C's were named because the review asked; the same form would serve these |
| `0x401000` (`kTextLo`, R1A, R1B, R1C) | `.text`'s first byte, a bound - `Area00_ChoiceVars3And6`'s pc, not a call |
| `0x520000` in `scena_sc1/sc6/sc9b/sc15`, `mode_states` | coordinates that equal `PartyAction7_CellPickup`'s pc, not addresses |
| Strings | the `Fatal` messages and the rows' labels |

**Outside the round's files**, the scan found 120 single raw constants in 35
files of earlier rounds whose target is ours (`battle_windows_callees.h`'s
twelve, `save_menu_callees.h`'s fifteen, `event_ops_callees.h`'s twelve,
`menu_windows_callees.h`'s twelve, ...). Some are patch-site or entry
constants that want reading before a rebinding; none is a round-fourteen
debt, so they are left for a pass of their own.

### 1.3 The run-time raw calls

[`round-13-cleanup.md`](round-13-cleanup.md) 1.3's policy is kept: **none was
converted**. Every `SH_AT(type, at::k...)` that calls through a rebound
constant still calls through the address, so `BOF3X_ORIGINAL` on the callee
still reaches Capcom's code for that caller and the call trace still sees the
call. Whether these become `SH_CALL(Name)` with their `_OURS` rows stays the
coordinator's call.

### 1.4 Verification

`git diff -U0 e3b98087 -- src` checked by a scratch script: every hex literal
a removed line carried is the `pc` of the `bof3::addr::<Name>` or `[[data]]`
name its added line carries - 101 addresses, 0 hunks differing. The i686
build (llvm-mingw) clean, no warning from a changed file; `gen_symbols:
10010 ours`; `ledger_check.py` 75 entries, 0 errors; `symbols.toml` parses
with no duplicate `pc`. `BOF3X_SHADOW='*'` headless: see below.
