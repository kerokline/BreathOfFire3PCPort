# Round thirteen's cleanup: the debts the effect-engine round left

**Status:** IN PROGRESS (2026-10-03) - the list for sessions of their own,
after round thirteen's last wave. Every item is owed by
[`takeover-queue-round13.md`](takeover-queue-round13.md) section 18 (each
wave's "Debts" paragraph in sections 11..17 names the evidence); nothing here
changes game behaviour, so no DIVERGENCE entry is expected unless an item says
so. Section 1 landed on branch `phase-3/round13-rebind` from the round's tip
`8d3d064`; the other items of section 18 are other sessions'.

Round thirteen took 1,757 functions in 37 groups (EGT, EKH and six waves;
6,891 -> 8,648 ours), every group fuzz-only through `scenario_harness`. Each
group called its own wave's and later waves' functions by raw address; the
price is section 1.

## 1. The rebinding

**The form** is round ten's ([`round-10-cleanup.md`](round-10-cleanup.md)
section 1, [`round-11-cleanup.md`](round-11-cleanup.md) section 2): a raw
constant whose target is ours reads `bof3::addr::<Name>` from the generated
`symbols.gen.h`, **the value unchanged**, so every fuzz key and every call
stands. A `_callees.h` constant another file keys on stays a constant whose
initialiser names the symbol. No function body changed and no call path
changed (below).

**Method.** A scratch script (not committed) listed every hex literal in
`src/game/effect_*` whose value is the `pc` of a `symbols.toml` `[[func]]` or
`[[data]]` entry, by file, line and symbol, outside strings and comments, and
classed each one; a second listed the literals in every other `src` file whose
value is a round-thirteen function (an `impl` in `src/game/effect_*`). At the
round's tip: 7,612 matches in `src/game/effect_*` (7,091 ours, 172 Capcom's
functions, 349 named data).

### 1.1 What was rebound: 58 constants in 20 files

By file (the `_callees.h` constants, then the fuzz files' callee rows that
were `XX_RAW(0x...)`, then the two spell files):

| File | Constants | Targets (owner) |
|---|--:|---|
| `effect_1a_callees.h` | 11 | `UiSprite_SetMode`, `UiSprite_Draw`, `Sprite_UpdateScreenScaled` (E1F); `Panel_DrawWindow`, `EffectKind0F_DrawToggles`, `_DrawGlyph`, `_DrawEquipped`, `_DrawItemsB`, `_DrawItemsA`, `_DrawCountHeader` (E1B); `NameTable_Accessories` (data, no `ctype`) |
| `effect_1b_callees.h` | 10 | `UiSprite_SetMode`, `UiSprite_Draw` (E1F); `EffectHud_TwoBars`, `_Bar`, `_Marker`, `_Sprite8`, `_DrawCount`, `_DrawArrow`, `_DrawMark` (E1A); `Item_DrawIcon` (E1G) |
| `effect_1e_callees.h` | 2 | `Panel_DrawWindow` (E1B), `Effect_ResetFirstSeven` (E1F) |
| `effect_1g_callees.h` | 4 | `Panel_DrawWindow`, `Panel_DrawEdgeQuad` (E1B); `UiSprite_SetMode`, `UiSprite_Draw` (E1F) |
| `effect_2d_callees.h` | 1 | `EffectSpark_FindFree` (E2F) |
| `effect_2e_callees.h` | 2 | `EffectSpark_FindFree`, `EffectKind4E_DrawDisc` (E2F) |
| `effect_2f_callees.h` | 1 | `EffectAngle_Mean` (E2E) |
| `effect_3c_callees.h` | 2 | `EffectKind6C_ScatterSparks`, `EffectKind6C_DrawSpark` (E3B) |
| `effect_4a_callees.h` | 4 | `Effect_DrawScreenTint` (E4D); `EffectKind87_Setup`, `_StepPanes`, `_FadePanes` (E4B) |
| `effect_4c_callees.h` | 1 | `Effect_DrawScreenTint` (E4D) |
| `effect_4f_callees.h` | 1 | `EffectKindA0_DrawGlow` (E4E) |
| `effect_5c_callees.h` | 3 | `EffectKind18Sub17_CopyFrame`, `_DrawPatch`, `_ScrollTexture` (E5D) |
| `effect_6b_callees.h` | 3 | `EffectKind18Sub2F_Draw` (E6A); `EffectKind18Sub44_Follow`, `_Draw` (E6C) |
| `effect_1e_fuzz.cpp` | 2 | the `E_RAW` rows for `0x469750`, `0x52CE20` -> `at::kBoxPrims`, `at::kClearEffects` |
| `effect_3b_fuzz.cpp` | 1 | the `E3B_RAW` row for `0x48CA90` -> `at::kScreenTint` |
| `effect_5c_fuzz.cpp` | 2 | the `E5C_RAW` rows for `0x5043B0`, `0x503E50` -> `at::kDrawMoveStep`, `at::kDrawMoves` |
| `effect_6b_fuzz.cpp` | 3 | the `E6B_RAW` rows for `0x50E1C0`, `0x510C90`, `0x510EB0` -> `at::kE6ADraw`, `kE6CStep`, `kE6CTail` |
| `magic_c2_fuzz.cpp` | 2 | the `C2_RAW` rows for EGT's `0x494060`, `0x494110` |
| `magic_s32_fuzz.cpp` | 2 | the `S32_RAW` rows for EGT's `0x494060`, `0x494110` |
| `magic_s14.cpp` | 1 | `kStepOn` `0x492750` -> `Effect_StateNext` (the stack table `Weretiger_Task` hands `magic_harness::Phase`) |

**By wave** (the file's group): wave one 29 (E1A 11, E1B 10, E1E 4, E1G 4 -
E1E's two header constants and two rows), wave two 4 (E2D 1, E2E 2, E2F 1),
wave three 3 (E3B's row 1, E3C 2), wave four 6 (E4A 4, E4C 1, E4F 1), wave
five 5 (E5C 3 + 2 rows), wave six 6 (E6B 3 + 3 rows); and 5 in the spell
round's files that called EGT (stage A) or `Effect_StateNext` raw. The
header comments that say "the raw addresses ... symbols.toml does not name"
are older than this pass; each touched header now includes `symbols.gen.h`
with a one-line note pointing here.

**A `XX_RAW(0x...)` row** expands to `#address, address, address`, so a
named argument would have changed the row's label in the log. Each was
written out as `{"0x...", at::kName, at::kName, ...}` - the label the macro
produced, the value the constant holds - the form the same files' other
cross-group rows already had (`{"0x47CF20", at::kPuffFindFree, ...}`). The
third field stays the address (not `KeyOf(&::Name)`), so the raw calls that
look the stand-in up by address still find it.

**Local names that do not match the symbol** (the value was checked against
`symbols.toml` each time; the constant's name was the group's guess before the
owner named it): E1A's `kItemListA` is `EffectKind0F_DrawItemsB` and
`kItemListB` `..._DrawItemsA`; E1B's `kE1aGaugeA` / `kE1aGaugeB` /
`kE1aGaugeMark` / `kE1aDigits` / `kE1aMarker` / `kE1aRange` are
`EffectHud_Bar` / `_Marker` / `_Sprite8` / `_DrawCount` / `_DrawArrow` /
`_DrawMark`; E4A's `kKind87Reset` is `EffectKind87_FadePanes`; E5C's
`kDrawMoveStep` is `EffectKind18Sub17_CopyFrame`; E6B's `kE6CStep` is
`EffectKind18Sub44_Follow`. Renaming the constants is left for a tidy pass:
it touches the bodies' call lines.

### 1.2 What stays raw, and why

| Class | Count | Why |
|---|--:|---|
| `CallSite` / `Imm` / `JumpTable` tables in the fuzz files | 5,226 ours, 170 Capcom's | the clone byte-check keys: each is the `disp32` target of Capcom's instruction at that offset, checked against the image before the clone is re-aimed - a fact about the bytes, not a call of ours (round ten's rule) |
| A `Clone` row's base | 1,753 | the clone source address the image is read from (the row names its own function beside it) |
| The fuzz's clone-base selectors | 29 | `effect_5a_fuzz.cpp`'s `In(lo, hi)` ranges and `g_base ==` / `base ==` tests pick which clone is being seeded; `hi` is often an exclusive bound that happens to be the next function |
| Named data with a `ctype` | 346 | the generated name is a macro for the cell itself (`#define Name (*reinterpret_cast<T*>(pc))`), so `bof3::addr::Name` expands inside the qualified name and cannot be spelt without `push_macro`; the groups' own comments say so (`kKind14States`, `kLeaveSteps`, `kInputHeld`) |
| Capcom's functions (no `impl`) | 2 outside key tables | not ours (`effect_2g_fuzz.cpp`'s `Crt_sprintf` callee row, its address twice) |
| `0x401000` (`kTextLo`, E1G and E2G) | 2 | `.text`'s first byte, a bound; it happens to be `Area00_ChoiceVars3And6`'s pc - **not an address of a function** |
| Strings | - | the `Fatal` messages (`"Sprite_UpdateScreenScaled (0x52CD50): ..."`) and the callee rows' labels |
| Raw rows to functions in no group | - | `effect_1e_fuzz.cpp`'s `0x52B2A0`, `0x52B1B0`, `0x52B200`, `0x52B2E0`, `0x52B370`, `0x52B330` (the leader's state 9, section 12's "in no group"), `effect_3b_fuzz.cpp`'s `0x4837B0`, `magic_s32_fuzz.cpp`'s `0x4941B0`: not ours, not named |

Outside `src/game/effect_*`, every remaining raw literal whose value is a
round-thirteen function is a `CallSite` / `Imm` key (`area_w3f`, `area_w3g`,
`area_w4b`, `area_w4e`, `field_c2`, `field_e1`, `field_e2`, `magic_c2`,
`magic_s14`, `magic_s32` fuzz files), a harness row this pass may not touch
(`scenario_harness_ekh.cpp`'s eight `InPlace` / `CloneOriginal` rows - item 2 of
section 18 - and `scenario_harness.cpp`'s region bounds), or **not an
address**: `scena_sc13.cpp`'s `ChangeArea(0x8F, 0x78000, 0x508000, 0x88)`,
whose `0x508000` is a coordinate equal to `EffectKind18Sub25_Open`'s pc (E5E
found it; `tools/scenario_rows.py`'s `SE_ADDRS` still lists it, section 16).

### 1.3 The run-time raw calls, left for the coordinator

Every site below is ours calling ours **through an address at run time**
(`SH_AT(type, at::k...)`, `scenario_harness::Phase(at::k...)`, E1B's
`Call3..Call6(address, ...)`, `magic_harness::Phase` on a table entry). In the
game that address holds the `jmp` to ours, so the call reaches ours - unless
`BOF3X_ORIGINAL` names the callee, when it reaches Capcom's; a direct call by
name would not follow `BOF3X_ORIGINAL` (HANDOFF trap "Our code calls ours
directly"). **None was converted**: the constant is named, the call path is
as it was. 61 `SH_AT` / `Phase` lines in 16 files (one line may be a helper
many call: E5C's three wrap its 34 calls, E6B's `DrawE6A` its 13):

| Caller | Constant -> target (owner) | Lines |
|---|---|---|
| `effect_1a.cpp` | `kDrawModeRecord` -> `UiSprite_SetMode`, `kDrawSpriteRecord` -> `UiSprite_Draw`, `kDepthPair` -> `Sprite_UpdateScreenScaled` (E1F) | 152..154 |
| `effect_1a.cpp` | `kWindowBox` -> `Panel_DrawWindow`, `kOptionBoxes`, `kMessageLine`, `kMemberRows`, `kItemListA`, `kItemListB`, `kPanelTitle` -> kind 0xF's draws (E1B) | 156..165 |
| `effect_1b.cpp` | `kDrawMode`, `kDrawSprite` (E1F) | 166, 168 |
| `effect_1b.cpp` | `kE1aBarG4`, `kE1aGaugeA`, `kE1aGaugeB`, `kE1aGaugeMark`, `kE1aDigits`, `kE1aMarker`, `kE1aRange` -> `EffectHud_*` (E1A), through `Call3..Call6` | 426, 486, 487, 491, 554, 663, 664, 686, 687 |
| `effect_1b.cpp` | `kE1gItemIcon` -> `Item_DrawIcon` (E1G) | 1115 |
| `effect_1c.cpp` | `kShardSpawn` -> `EffectSpecks_Spawn` (E2A); `kDebrisDraw`, `kDebrisInit` -> `EffectDebris_*` (E3C); `kCone` -> `Effect_DrawEllipse` (E4F) | 279, 296, 393, 410; 818, 912; 1045 |
| `effect_1e.cpp` | `kBoxPrims` -> `Panel_DrawWindow` (E1B); `kClearEffects` -> `Effect_ResetFirstSeven` (E1F) | 124; 1070 |
| `effect_1g.cpp` | `kTradeBox`, `kQuad` (E1B); `kPieceMode`, `kPiece` (E1F) | 81..84 |
| `effect_2d.cpp` | `kPuffFindFree` -> `EffectSpark_FindFree` (E2F) | 850 |
| `effect_2e.cpp` | `kSparkFree` -> `EffectSpark_FindFree`, `kDiscDraw` -> `EffectKind4E_DrawDisc` (E2F); `kDebrisDraw` (E3C) | 288, 1042; 773 |
| `effect_2f.cpp` | `kAngleMean` -> `EffectAngle_Mean` (E2E) | 1141 |
| `effect_3a.cpp` | `kSparkSpawn` -> `EffectKindB9_SpawnSpark`, `kShardDraw` -> `EffectKindB9_DrawShard` (E4F) | 736, 1081 |
| `effect_3b.cpp` | `kScreenTint` -> `Effect_DrawScreenTint` (E4D) | 173 |
| `effect_3c.cpp` | `kShardsSpread`, `kShardQuad` (E3B); `kScreenTile` (E4D) | 229, 336; 1222, 1229 |
| `effect_4a.cpp` | `kFade` (E4D); `kKind87Setup`, `kKind87Step`, `kKind87Reset` (E4B) | 426; 603, 612, 614, 621 |
| `effect_4c.cpp` | `kScreenTile` (E4D) | 310, 335 |
| `effect_4f.cpp` | `kGlowDraw` -> `EffectKindA0_DrawGlow` (E4E) | 161 |
| `effect_5c.cpp` | `kDrawMoveStep`, `kDrawQuads`, `kDrawMoves` -> `EffectKind18Sub17_*` (E5D) | 238..240 |
| `effect_6b.cpp` | `kE6ADraw` -> `EffectKind18Sub2F_Draw` (E6A), through `Phase`; `kE6CStep`, `kE6CTail` -> `EffectKind18Sub44_*` (E6C) | 154; 904, 905 |
| `magic_s14.cpp` | `kStepOn` -> `Effect_StateNext`, through `magic_harness::Phase` | 167 |

The list was made by a scratch script (each `at::k` constant of a group's
header resolved through `symbols.toml`; lines that use it in `SH_AT`, `Phase`
or `CallN`), so a call through a local variable that holds such a constant
(`effect_1c.cpp:159`'s `draw`) is not in it. Earlier waves' constants that
were already named (`effect_1d.cpp`'s `kArmVertices`, `effect_2e.cpp`'s
`kSpiralInit`, ... ) and point at Capcom's code are not ours-to-ours and not
listed. Whether any of these become `SH_CALL(Name)` - and the `SH_OURS`
standard rows that would gate them in the same commit - is the coordinator's
call, with a `'*'` run each side (round ten's note: a named key with a raw row
is a `Fatal`).

### 1.4 Verification

At `phase-3/round13-rebind`'s tip: the i686 build (llvm-mingw) clean, no
warning or error from `src/`; `gen_symbols: 8649 ours`; `BOF3X_SHADOW='*'`
headless narrow and with `BOF3X_WIDE=1` (2026-10-03, at `de4f8ee`, the
code commit): both exit 0, `inject: 8648 ours, 0 left original by
BOF3X_ORIGINAL`, 1,021 `MISMATCHES` lines each and every one `0 MISMATCHES`;
`ledger_check.py` 72 entries, 0 errors.
