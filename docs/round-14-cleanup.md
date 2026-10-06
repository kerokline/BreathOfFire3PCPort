# Round fourteen's cleanup: the debts the remainder round left

**Status:** MEASURED (2026-10-05 night) - the round's end, after round
fourteen's last wave. The debts are those of
[`takeover-queue-round14.md`](takeover-queue-round14.md) sections 9 to 13 and
the low items and nits of [`round-14-review.md`](round-14-review.md). Five
agents ran side by side from the round's tip `e3b98087` (branches
`phase-3/r14end-ea` .. `-ed2`), each with its own `'*'` narrow and wide, and
were merged one at a time into `phase-3/round14-end` (sections 1 to 4 are
their records, folded in by the coordinator from their reports); section 5 is
what the owner decided the same night (DIV-0076); section 6 is the merged
tip's verification. Nothing in sections 1 to 4 changes game behaviour.

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
with no duplicate `pc`. `BOF3X_SHADOW='*'` headless narrow and with
`BOF3X_WIDE=1` (2026-10-05, at `8af13a3b`, the code commit): both exit 0,
the log ending `self-test only: done`, `inject: 10009 ours, 0 left original
by BOF3X_ORIGINAL`, 1,049 `MISMATCHES` lines each and every one `0
MISMATCHES`. No stand-in's keying changed (every key is the same address),
so no fuzz count should move.

## 2. The harness fold and the host-extent lines (EB)

**Rows to the `_OURS` form: 67** in `src/game/scenario_harness.cpp` and
`src/game/boss_harness.cpp` - all of debt 14 (R3A 4, R3B 3, R3D 3, R3E 18,
R3F 5, R3G 7), debt 21's four (`0x452DD0`, `0x452EB0`, `0x452F10`,
`FX_RAW(0x462F10)`), debt 2's seven `0x52B...` rows, eleven address-keyed
rows from wave two, and the five `kStandard` rows keyed by `bof3::addr::`
(`Field_StartEventBattle` and four more). What stays keyed by address is
Capcom's: `0x5B9550`, `0x5A7...`, `0x59E930` (the platform round's, the same
night), `0x593950`, `0x591810`, `0x5B9450`. **Masks narrowed** to what the
function reads, the read cited: `0x491E30`'s size argument its low word,
`0x5100B0` `{kU16, kU16}`, `0x44F1D0` `{kU8, kU16}`. **Debt 2:**
`Sprite_LoadPalette`'s row logs its destination by value with no hash, a new
`FxPalette` filling the 0x40 bytes where they lie; `0x52B330` `{kU16}`
answering `kByte` 0..1; R1C's six field callees the standard set lacked
(`Effect_SpawnAtCell` / `High`, `Field_GiveZenny`, `AreaMap_ClearCell`,
`Field_EffectAhead` `kByte 0xFF..0x13`, `Sprite_TurnSense`) are `kField` rows
now (R0A's six party helpers stay with their groups: their answers differ by
group). `Gpu_SetLineF3` and `Gpu_SetSprt16` are in `kField` too (R2D, R4B,
R4E reach them). The write-up is
[`scenario_harness.md`](scenario_harness.md) 8.11 and
[`boss_harness.md`](boss_harness.md) 10.11. Counts that moved under the fold,
all still 0 mismatches: `rest_2d`, `rest_2f`, `rest_2g`, `magic_s16/17/34/35`,
`area_w0b/w1e/w3a` - none reaches a changed row through the standard set, so
EB read it as build-layout drift, not proved. Left: `Sprite_FlashClut`'s
`kEffectStd` row masks the whole word where R1C reads the byte.

**`analysis/calltrace/entries_logic.txt`** (gitignored, the main checkout's;
backup `<this session's scratchpad>/end14/eb/entries_logic_backup_1005.txt`):
**75 host lines split** - 55 cut to the function's own extent, 20 dropped
where a smaller line already existed - debts 1, 10, 15 and 21 and R0A's other
five of the same class, the sizes from the group docs, agreeing with
`symbols.toml`; then `consolidate_entries.py` (10,294 entries, six more
extents cut; the file is LF only now - some 570 appended lines were CRLF).
`entries_audit.py` without `--reach`: 9 covered, 9 uncovered, identical
before and after; seven older hosts still cover an owned start with no line
of its own (`0x453FA0`, `0x454DF0`, `0x494280`, `0x4B98B0`, `0x4FD350`,
`0x577760`, `0x5786C0`) - outside the debts, left. The platform round's
groups listed their own lines in their docs; the new ones (PS's `005A6FF0 29`,
`005A7080 16`; PH's nine) are appended by the coordinator with the platform
round's record.

## 3. The review's low items and nits (EC)

Items 7, 8, 12 to 17, 19 and every nit but the raw constants (section 1),
plus debts 16 and 19. The policies, chosen so the groups agree:

- **Item 7:** R1A, R1B and R1C abort at the table's `symbols.toml` count,
  before the read, as R1D..R1G do (the round-nine rule: a loud abort before
  the fault, never a read on into the next table); one sentence in
  `rest_1a.md`, `rest_1b.md` section 6, `rest_1c.md`, `rest_1d.md` L3. No fuzz
  re-seeded.
- **Item 16:** R3D's slots 117 / 118 abort past a party size of 3 as R3C
  does (R3D's fuzz re-seeded to 0..3 in seed and disturbance); R3B's three
  effect slots stay `void` - `Effect_ApplyResult` calls them as void and
  nothing reads the eax R3C forwards (`rest_3c.md` corrected); `0x480300`
  is `(point, unused, wobble, dy)` by capstone - R3F was right, R3E's header
  fixed.
- **Item 17:** R2B's reading of `0x9398E0` (two 0x80-byte models, then the
  screen's cells) is right, by capstone; both docs carry it. `0x5B9450` is
  MSVC's `strncpy`: `Crt_strncpy` in `symbols.toml`, the `memcpy` comments
  corrected, [`platform-read-pass.md`](platform-read-pass.md) section 3
  confirmed.
- **Item 13:** R2F's grid loops read through a `volatile`. **Item 14:** R2E's
  three count-list takers abort at the access, not at entry. **Item 15:**
  `StateHash_Flush()` from `Fatal` and the crash reporter (a deliberate
  `Fatal` at tick 100 under `DUMP=50` left all 100 records where 65 would
  have survived; [`state-hash.md`](state-hash.md)). **Item 12:** the texts.
  **Item 8:** the fuzz comments; `rest_1e.md` gives the bound as 10 and the
  reason it is unreachable (directions stay 0..7); EC's view, no ledger
  entry - no state of play reaches it. **Item 19:** I31, I23 and the plan's
  2.4 heading match the owner's answers.
- **Nits:** sets 16..18 renamed to the R1A..R1E pattern (10 functions, 10
  tables); the `TurnToSide` pair kept (a rename would reuse names);
  `cmd_info` was already fixed; **the count**: the one `impl` the inject log
  does not count is `Config_DrawControllerCell`, injected only under a Latin
  overlay (`takeover-queue-round14.md:13` corrected).
- **Debt 16:** DIV-0027's note names `MenuList_WideTitleBox` `0x59A2E0` (its
  `rest_2g.cpp:297-298` is the only code that tests help `0x1A` / `0x31` and
  draws message `0xF`); `0x59AE00` is `MenuList_GeneWinDraw` and draws no
  message `0xF` - the note corrected by the coordinator (section 5).
- **Debt 19:** done; `rest_1g.md`'s round-count lines told once more by the
  coordinator after section 4 moved the count again.

## 4. The thin and unrefused controls (ED1, ED2)

Each fix is a louder stand-in on the path the control changes, with a seed at
the boundary where one was missing; only `_fuzz.cpp` files and the docs'
controls tables changed (and debt 24's three lines in `rest_4d`). Figures are
refusals in the group's normal round count, before -> after.

| Group | Control | Before | After | What reached it |
|---|---|--:|--:|---|
| `field_c3` | C179 / C180 / C181 (and C178) | 0 / 0 / 0 (0) of 30,000 | 105 / 43 / 18 (116) of 3,000 | not equivalent: the fuzz never reached them. `MapView_SlopeAt`'s stand-in flips `Sprite_Current`'s low x / z word to or from 0 for directions 3 and 5 and answers steep; C167..C170 rose to 119 / 107 / 46 / 20 too |
| `effect_1e` | C59 / C62 | 1 / 7 | 19 / 449 | `Effect3Mode` moves record 4's state; `PoseSound` the y step |
| `effect_5a` | `_05_Pulse` | 0 of 2,000 | 45 of 2,000 (76 of 4,000) | `ClutStripCopy16` moves `+0x3A` in sub-kind 5's states; `+9` seeded at the end test |
| `field_e2` | D1 / DS5 / DS8 | 4 / 1 / 1 of 6,000 | 49 / 151 / 48 | `GroundAt` moves the actor's `+0x89` and answers at the seeded ground's edges (0, +-1, +-0xC0 / 0xC1 - W2 fell to 0 without the last); `0x594700` moves the trade pick |
| `rest_1g` | D11 (debt 18) | 2 of 60,000 | 50 of 6,000 | **back to 6,000 rounds a function** (96 s -> 2 s a `'*'`): `Rand` re-listed with an effect (R4A's pattern), louder `LureInReach`, `Elevation`, `DrawBox3` / `DrawShade`, `ScriptTick`, `LureClose`; `Fish_Swim`'s height seeded at its kind's bounds; D09 / D10 24 / 14, D13 50, C56 1 -> 39; `BOF3X_R1G_ROUNDS` overrides the count for a control run |
| `rest_4f` | C21 | 3 of 60,000 | 99 | `Sound_PlayEffect` stand-in moves the five settings half the time |
| `rest_4d` | N70 | 3 | 618 | a stand-in on `0x45EE10` moves the cursor to 0..7; N71 the equivalent |
| `rest_4a` | C3 / C21 / C29 / C32 | 3 / 4 / 4 / 4 | 697 / 125 / 189 / 265 | boundary seeds; `ClockEffect`, `Rand` moving building levels in `TickKind9` |
| `rest_4a` | C33b | - | equivalent | the tier table at `0x652928` (100, 300, 500, 1,000, 3,000, 10,000, 30,000, 65,535): a u16 price can never pass the eighth bound, so the original stops at tier 7 too; near variant C33d (two tiers early) refused 1,153 |
| `rest_4b` | C36 / B36 / D20 / D44 | 4 / 8 / 4 / 2 | 133 / 1,057 / 2,384 of 12,000 / 979 | `RandMove`, `ListBoxMove`, `SpriteMove`, y at the bound; C30 and C39 the same equivalents as before |
| `rest_2f` | C150 | 2 of 6,000 | 436 | `SoundEffect` moves the members row while `TacticsMembers_Pick` runs, after sound 0x103 |

Every older control of each group re-run and still refused. **Debt 24:**
`Rest4D_Inject` runs `rest_4d::EntryAbandonTest()` once DIV-0075's switch is
set - ours with the switch on against Capcom's two steps with the step byte
taken back by one, 8,000 rounds, 0 mismatches, three plants refused
3,989..4,000 times (DIV-0075's verification line names it). **Still thin,
outside the night's lists:** `rest_4d` D33 (4), B12, N47, N07 (5..7); `rest_4a`
C16 (9); `field_e2` DS9 (1), DS6 (2), IT7 (3); `field_c3` C172 (0 at 3,000);
`effect_1e` C58 (3), C61 (2); `rest_1g` C76 / C77 (9), C15 (11), C71 (13), D10
(14). The controls drivers are in this session's scratchpad (`end14/ed1/`,
`end14/ed2/ctl.py`), not in git.

## 5. The owner's decisions the same night

- **DIV-0076** (`Save_BuildBlock`, R2C's debt 12): the owner confirmed in
  game that a save's summary shows the leader's name beside record 0's level,
  and chose record 0 for both. Built on `phase-3/round14-end` behind a switch
  set after the self-test (DIV-0075's form); `rest_2c` 244,000 rounds, 0
  mismatches with it off. Owed the owner's eye on the load screen.
- **Debt 3** (`PartyAction_WaitEffectDone` reading inside `Gfx_PacketPools`
  with all 20 effect records in use): a field action, not a battle; how full
  the pool gets in play is unmeasured. Not decided; a log line on
  `Effect_FindFree` answering `0xFF` would tell.
- **Debt 12's other half** (`FieldMenu_CampAllowedCell`'s three dead
  compares), measured the same night at the owner's word
  ([`rest_2d.md`](rest_2d.md) L1): `0xA1` and `0xAF` cells exist and are
  refused anyway through the mask; **`0x91` cells exist, 91 of them on nine
  world maps** (the owner: map 2's bridges, Lost Shore's harbour), and the camp
  opens on them where the compare says refuse. The PlayStation's twin
  `0x801D21C4` masks first too - Capcom's bug on both. A fix is a one-line
  divergence; not decided.

## 6. The merged tip

Merged in the order the reports arrived: EA `1dbba0b`, EC `4386a35` (one
hand-resolved conflict, `rest_3e_callees.h`: EA's binding under EC's
comment), ED2 `f32ae5e`, EB `615e04f` (one, `boss_harness.cpp`: EB's row,
EC's comment), DIV-0076 `19f5de5`, ED1 `12cd36a`. Each agent's own `'*'`
narrow and wide passed at its branch; the merged tip's verification is below
(filled in when it ran).
