# The operand store's bytes as values of ours

**Status:** MEASURED (2026-10-12, branch `disc-only/operands` off `6e6b4f3b`;
the disc-only round's group OP, [`disc-only-round.md`](disc-only-round.md)).
Headless self-tests only; the full `'*'` runs, the state-hash routes and the
live checks are the round's batch.

## 1. What changed

Until this round our code read about forty places of Capcom's `.text`
([`state3-build.md`](state3-build.md) 2.3): an operand a divergence patched,
the target of a call a divergence re-aimed, a table the compiler put in the
code, and two verifications. Hosted it read the code itself; in the exe host
it read a store of ours filled from the player's `BOF3.exe` on first use (the
exe-host self-test's last count: `codebytes after InjectAll: 43 reads, 450
bytes`, `build/st_exe/bof3x.log`, 2026-10-11). A disc carries no PC operands,
so the owner's call of 2026-10-11 was that **each operand becomes a value of
ours**. Now:

- **A patched operand is held by the divergence that patches it.**
  `PatchBytes` and `RetargetCall` answer whether they went in (`false` under
  `BOF3X_ORIGINAL=<name>`); the divergence keeps the value it wrote, or the
  original's, from that answer. Hosted they still write Capcom's code; in the
  exe host they write nothing in the code range (there is no code) and give
  the same answer.
- **A re-aimed call is held by the divergence that re-aims it**, as the
  address the site calls in this build (Capcom's callee or ours, chosen from
  `RetargetCall`'s answer); the screens of ours call that.
- **A compiler table is held by our reimplementation of the function that
  held it**, as the `switch` the compiler built it from: the case labels are
  the function's logic, ours since the function is. No table of bytes is
  copied into `src/`; the bytes the hosted check compares are generated from
  the switch.
- **A verification of Capcom's code** (New Game's operands, a label's push
  witness) is registered with what it expects and compared hosted only.

Every reader is the same code in both hosts and reads no byte of Capcom's
code. `CodeBytes`, `CodeByte`, `CodeWord`, `CodeLong`, `CodeCallTarget`,
`CodeBytesForWrite`, `CodeBytes_Open`, `CodeBytes_Report` and the store are
gone (`grep -rn "CodeBytes\|CodeByte(\|CodeWord(\|CodeLong(\|CodeCallTarget(" src`
is empty). In `exe_host.cpp` the `CodeBytes_Open` call, the two lines that
handed it `.text`'s file offset and size, and `CodeBytes_Report` are removed;
`CopyData` still reads the section table (its `.text` row checks the layout),
and its two out-parameters are now unused (named in comments; the RD group
owns the rest of that file).

`src/hook/code_bytes.{h,cpp}` **stays, as the hosted check's home**:
`Operand` / `OperandExpect` / `OperandCall` / `OperandRule` register a value of
ours against the address it stands for; `Operands_Check` compares every one
with the code hosted (after `InjectAll`, from `InjectReport`, and after each
late patch: DIV-0015's labels with FIRST.DAT, DIV-0014's widths with
START.DAT; an entry registered after `InjectAll` is compared as it comes). A
difference is a Fatal naming the site, the address and the byte. In the exe
host the same table is the inventory of what used to be read: one log line,
nothing read.

A site under the first five bytes of a function of ours is overwritten by
its jmp: `Menu_YesNo` 0x5747D0's call at 0x5747D2 (DIV-0027's line). The
first hosted run found it (`FATAL: operands: DIV-0027 yes / no line -
0x005747D2 is not a call to 0x00497740, our value (0xEC, reaching
0x0049A467)`): the old `YesNoLayout_ActiveLine` read the site in
`MenuWindows_Inject`, before `Menu_YesNo`'s jmp went in. So `Inject` calls
`Operands_BeforeJmp(original)` before it writes a jmp: every entry under
those five bytes is compared then, while the code still holds the patch,
and the later checks pass over it. An entry registered after its jmp could
not be compared and would be named in the log; there is none.

## 2. The sites

Address: the bytes the value stands for. Reader: our function that reads it.
Kind: **P** a patched operand, **C** a re-aimed call, **T** a compiler table,
**V** a verification. Value: what the reader reads now. Check: the
registration name in the hosted check (`Operands_Check`'s table).

| Address | n | Reader | Kind | Value now | Check |
|---|---|---|---|---|---|
| 0x599061 | 2 | `GeneWin_ListSlideOut` (battle_e7) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x599451 | 2 | `GeneWin_List2SlideOut` (battle_e7) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x599551 | 2 | `GeneWin_List3SlideOut` (battle_e7) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x59B446 | 4 | `MenuWin_SlideOutLeft` (menu_draw_helpers) | P | `Widescreen_SlideOperand` through `g.slide_left_bound` (the fuzz aims it at its clone's immediate) | DIV-0041 slide bound |
| 0x59A586, 0x59A5E6 | 4 | `MenuSlide_LeftOff`, `MenuSlide_RightOff` (menu_lists) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x596926 | 4 | `MenuSlide_LeftOff170` (rest_2f) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x599CC6, 0x599DF6, 0x59A136, 0x59A2B6 | 4 | `MenuList_SlideLeftOff300/100/180/110` (rest_2g) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x598A08, 0x598A1C | 2 | `GeneWin_GridSlideOut` (rest_2g) | P | `Widescreen_SlideBound` | DIV-0041 slide bound |
| 0x59C136 | 4 | `MasterWin_SlideOut` (rest_2h) | P | `Widescreen_SlideOperand` through `g_master_bound` (the fuzz aims it at its clone's) | DIV-0041 slide bound |
| 0x51097E, 0x510991, 0x5109BB, 0x5109D2 | 4 | `AreaMapBD_BuildView` (rest_3g) | P | `Widescreen_RangeOperand`: the float's address, .rdata's or widescreen.cpp's copy | DIV-0041 area-map range |
| 0x510968, 0x5109A4 | 4 | `AreaMapBD_BuildView` (rest_3g) | P (never patched) | 0x5C4244, 0x5C4238 (rest_3g.cpp) | AreaMapBD_BuildView y bound |
| 0x51087F | 2 | `AreaMapBD_BuildView` (rest_3g) | P | `draw_pool::Count()` (0x400, DIV-0062's 0x800) | DIV-0062 draw-item bound |
| 0x5109FC | 4 | `AreaMapBD_BuildView` (rest_3g) | P | `draw_pool::Items()` | DIV-0062 draw-item array |
| 0x486EBD (+12 more) | 4 | `DrawPool_Grow` (draw_pool; was the "did the patch go in" read-back) | P | `PatchBytes`' answer; each site's value in `site_now` | DIV-0062 draw-item array |
| 0x461832 + 8 r | 4 x 6 | `Config_DrawRowLabel` (field_c1) | P | `ConfigText_Operands().label[r]` (DIV-0015, set when FIRST.DAT's chunk patches) | DIV-0015 Config label |
| 0x461894 | 5 | `Config_DrawRowLabel` (field_c1) | P | `.label_x4` | DIV-0017 Config label width |
| 0x46189D, 0x4618ED | 1 | `Config_DrawRowLabel` (field_c1) | P | `.anchor` (0x3A, 0x5F) | DIV-0017 Config label anchor |
| 0x46189F | 5 | `Config_DrawRowLabel` (field_c1) | C | `.big_draw` | DIV-0017 Config selected draw |
| 0x4619E1 | 3 | `Config_DrawRowOptions` (rest_4f) | P | `.option_x8` | DIV-0017 Config option width |
| 0x4619F9 | 5 | `Config_DrawRowOptions` (rest_4f) | C | `.big_draw` | DIV-0017 Config selected draw |
| 0x461778 | 5 | `Config_DrawPanel` (rest_4f) | C | `MenuFrame_Draw()` | DIV-0011 panel frame |
| 0x461A84 | 5 | `Config_DrawControllerPanel` (rest_4f) | C | `MenuFrame_Draw()` | DIV-0011 panel frame |
| 0x461A62 | 1 | `Config_DrawControllerPanel` (rest_4f) | P | `.frame_w` (0xC, 0xD) | DIV-0051 controller frame (3 bytes from 0x461A61) |
| 0x461A60 | 1 | `Config_DrawControllerPanel` (rest_4f) | P (never patched) | `kPadFrameRows` 0xF (rest_4f.cpp) | Config_DrawControllerPanel frame rows |
| 0x461B07 | 1 | `Config_DrawControllerRow` (rest_4f) | P | `.box_width` (0x68, 0x58) | DIV-0051 controller box width (2 bytes from 0x461B06) |
| 0x461B36 | 3 | `Config_DrawControllerRow` (rest_4f) | P | `.name_x4` | DIV-0026 controller name width |
| 0x461B41 | 1 | `Config_DrawControllerRow` (rest_4f) | P | `.name_edge` (0x20, 0x36) | DIV-0026 controller name edge (3 bytes from 0x461B3F) |
| 0x461B43 | 5 | `Config_DrawControllerRow` (rest_4f) | C | `.name_draw` | DIV-0026 controller name draw |
| 0x461BAC | 6 | `Config_DrawControllerRow` (rest_4f) | P | `.second_cell` | DIV-0051 controller second cell (and 0x461BE9's 3) |
| 0x5888E8, 0x5888ED, 0x5888F2 | 1 | `TitleMenu_DrawRow` (save_menu); `TitleMenu_SetWidths`' read-back (title_menu) | P | `TitleMenu_Width(row)` (DIV-0014, set when START.DAT's chunk patches) | DIV-0014 title menu width |
| 0x576A48 | 1 | `Menu_DrawSaveSlot` (field_o) | P | `YesNoLayout_SaveNameInset()` (0x13, DIV-0029's 0x15) | DIV-0029 save name inset |
| 0x5747F0 | 15 | `Menu_YesNo` (menu_windows, through `YesNoLayout_StopsMoved`) | P | `g_stops_moved` (yes_no_layout) | DIV-0027 yes / no stops |
| 0x4B5691, 0x4F51EF | 4 | `Cheats_PilferRollMask` / `_StealRollMask` (magic_steal, magic_fx_reached) | P | `g_pilfer_mask`, `g_steal_mask` (0xFF, DIV-0046's 0) | DIV-0046 Pilfer / Steal roll mask |
| 0x5747D2 | 5 | `Menu_YesNo` (menu_windows, through `YesNoLayout_ActiveLine`) | C | `g_line_call` (yes_no_layout) | DIV-0027 yes / no line (compared before `Menu_YesNo`'s jmp) |
| 0x586E78, 0x586E97 | 5 | `MasterScreen_AskYesNo` (rest_2d) | C | `YesNoLayout_MasterLineDraw()`, `_MasterHandDraw()` | DIV-0027 master's line / hand |
| 0x596D13 | 5 | `Win2_DrawItemList` (rest_2f) | C | `ListTitle_SiteDraw()` (battle_draw) | DIV-0059 list title draw |
| 0x59DEFA | 5 | `BattleEquipWin_DrawItems` 0x59DBF0's title (rest_2h) | C | `ListTitle_SiteDraw()` | DIV-0059 list title draw |
| 0x59AA98 | 5 | `MenuList_ReserveWinDraw` (rest_2h) | C | `MenuFrame_Draw()` | DIV-0011 panel frame |
| 0x581313 | 5 | `PartyForm_DrawReserve` (field_s, `field_s::FrameCallTarget`) | C | `MenuFrame_Draw()` | DIV-0011 panel frame |
| 0x57499B | 5 | `Menu_DrawButtonRow` (menu_windows, through `MenuVerbs_ActiveLabel`) | C | `g_label_call` (menu_verbs) | DIV-0018 menu verb label |
| 0x426440 | 0x21 | `Area167_Tail32` (area_w4a) | T | `Tail32Case(state)` | Area167_Tail32 case index |
| 0x59F304 | 0xD1 | `Gfx_DrawOTag` (d3d_list) | T | `Entry(index, true)` | Gfx_DrawOTag software byte table |
| 0x59F440 | 0xD5 | `Gfx_DrawOTag` (d3d_list) | T | `Entry(index, false)` | Gfx_DrawOTag Direct3D byte table |
| 0x49C0CC | 0x99 | `ElemStrike_Kind` (magic_s02) | T | `KindCase(index)` | ElemStrike_Kind byte table |
| 0x4C488C | 0xA3 | `Magic088_Variant` (magic_s20) | T | `VariantCase(k)` | Magic088_Variant byte table |
| 0x518654, 0x518660 | 0xC + 0x100 | `AreaMap_CellBlocked` & co. (field_blocked) | T | `NibbleBlocks(nibble)` | AreaMap_CellBlocked cell table (a rule: the two tables resolved per nibble, `CellTableIsOurs`) |
| 0x437834, 0x437891, 0x42E09D, 0x52D1EC | 6, 5, 5, 6 | `CharNames_Apply`, `_ApplyMerchant` (char_names) | V | the instructions expected | DIV-0020 ... |
| 18 push operands (0x459144 .. 0x59C5B6) | 4 | `Labels_Apply` (labels) | V | the slot (or table base) expected | the table's name; a witness in `.data` is read as before, in both hosts |

The compiler tables, as switches (the case labels; every other value takes
the last case): `Tail32Case` states 0, 1, 5, 6, 0xA, 0xB, 0xC, 0x14, 0x15,
0x18, 0x1E, 0x1F, 0x20 to cases 0..12; `Entry` the PlayStation GPU codes
0x20..0x3C, 0x40, 0x48..0x5C, 0x60..0x6C, 0x74, 0x7C, 0x84, 0xE0 / 0xE8,
0xEC, 0xF0, 0xF4 to entries 0..24 (d3d_list.cpp names each); `KindCase`
ability - 4 = 0, 1, 3, 0x19, 0x2D, 0x81..0x84, 0x97, 0x98 to cases 0..10;
`VariantCase` id - 0x1E = 0 / 0x3A / 0xA0 to 0, 1 / 0x3B / 0xA1 to 1, 0x3C /
0xA2 to 2; `NibbleBlocks` nibbles 1..5, 0xA, 0xB, 0xF block. Read from
`BOF3.exe` once to write the switches (a session script in the group's
scratch, 2026-10-12, over `.text` at 0x426440, 0x59F304, 0x59F440,
0x49C0CC, 0x4C488C, 0x518654 / 0x518660), and compared hosted against the
same bytes at every start since.

`exe-table-audit.md` ?1 (2026-10-08) had made `field_blocked`'s mask a read
of the image so that no copy of it sat in `src/`; the same audit counted a
`switch`'s case list as code. This round's rule for a compiler table settles
it the second way: the switch is the function's logic, held as code, and the
image's tables are what the hosted check compares.

## 3. Measured

The inventory: **77 values of ours for 1,310 bytes** of Capcom's code (the
`operands` line). The old store's 43 reads / 450 bytes were the ranges
touched by `InjectAll`; the inventory adds the whole compiler tables (the
store loaded only the bytes a read reached), the late chunks' sites, and the
sites no reader asked for in that run.

Self-tests, from an ini-less copy of `build/` (`op_scratch/st`), 2026-10-12:

- Hosted, `BOF3X_SHADOW=battle_e7,menu_draw_helpers,menu_lists,rest_2f,
  rest_2g,rest_2h,rest_2d,rest_3g,rest_4f,field_c1,field_s,field_o,
  save_menu,yes_no_layout,draw_pool,area_w4a,d3d_draw,magic_s02,magic_s20,
  field_blocked,battle_draw,menu_windows,magic_steal,magic_fx_reached`:
  exit 0, 35 `0 MISMATCHES` lines, none other; `inject: 10087 ours, 0 left
  original by BOF3X_ORIGINAL`; `operands after InjectAll: 77 values of ours
  (1310 bytes) - 76 equal to the patched code at their addresses now, 1
  before a jmp of ours covered them, 0 never compared`.
- The same list less `field_c1` (the Latin clone-check trap, HANDOFF) under
  `BOF3X_LANG=en-US BOF3X_WIDE=1 BOF3X_STEAL=1` - every patch above in:
  exit 0, 34 `0 MISMATCHES`, `inject: 10088 ours`, the same `operands` line.
- Hosted, no shadow, the same three switches and `BOF3X_ORIGINAL=Widescreen,
  ConfigText,ConfigController,YesNoLayout,SaveNameInset,DrawPool,
  Menu_DrawFrame,BattleListTitleCentre,MenuVerbs,MasterAskLayout,
  Cheat_StealAlways` - every patch left out (62 `OFF` lines): exit 0, the
  same `operands` line.
- Exe host, plain and under the three switches: exit 0, `inject: 10087`
  (10088) `ours`, `operands after InjectAll: 77 values of ours stand for
  1310 bytes of Capcom's code (docs/operand-store.md); none read, there is
  no code to compare in this host`, `stub: inventory zero ... no PatchBytes
  or RetargetCall wrote it`; **no `codebytes` line**.

Not run here: the late checks (FIRST.DAT's labels, START.DAT's widths), which
need the game to load the title; the full `'*'`; the routes.

## 4. Ledger

No entry: nothing the game does changes. The values are the ones the code
held, in both hosts, and every divergence that patches keeps its name for
`BOF3X_ORIGINAL`.
