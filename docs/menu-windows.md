# The menu and shop windows

**Status:** IN PROGRESS (2026-09-23) - thirty-seven functions ours
(`src/game/menu_windows.cpp`, shadow name `menu_windows`), each fuzzed
headless against a copy of Capcom's with every call re-aimed at a recorder:
74,000 rounds, 0 mismatches, with the language overlay's two divergences off
and on; 50 negative controls, 49 refused by a count, one not refused because
it changes nothing. **Not yet through the live batch check** - the shop A/B
(`analysis/validate_shop.sh`) runs centrally after the merge (section 6).

Group Y of the sixth parallel round
([`takeover-queue-round6.md`](takeover-queue-round6.md)): what the owner's
shop route reaches of the menu's drawing - the menu box and its pieces, the
two small fonts, the button row and the Yes / No chooser, the item list, the
equipment, money and shop panels, the backdrop, and the helpers under them.

## 1. The functions

Every extent measured by recursive descent from the entry (a scratch
capstone walker, 2026-09-23), not taken from the catalogue. The PSX column
is `analysis/pairs_propagated.json`'s twin and its tier.

| PC | Name | Bytes | PSX twin | What it does |
|---|---|---|---|---|
| `0x497710` | `Msg_OpenSystem` | 0x26 | `0x801503AC` symbols | system message id into the box: base and stepper pointer, index, `MsgBox_Reset` |
| `0x498D20` | `MsgBox_DrawArrow` | 0xB6 | - | the box's blinking "more" arrow, an 8 x 8 SPRT while `Frame_Counter` bit 5 |
| `0x516F60` | `Text_DrawFont12` | 0x123 | `0x8015002C` call-anchored | a string in the 12 x 12 numeral font |
| `0x517090` | `Text_DrawFont8` | 0x103 | `0x801501C0` call-anchored | the same in the 8 x 8 font (SPRT_8) |
| `0x596020` | `Window_Kind2List` | 0x6A | `0x8015AA88` gap47 | window kind 2's list through `Text_DrawImmediate` |
| `0x573A80` | `Menu_DrawEquipPanel` | 0x16B | `0x801D8A10` gap54 | a member's six equipment slots |
| `0x573CE0` | `Menu_DrawCursorBox` | 0x163 | `0x801D8E08` call | a LINE_F3 + LINE_F4 cursor outline, blinking |
| `0x573E50` | `Menu_DrawIcon` | 0xDE | `0x801D8F9C` call | a 0x28 x 0x30 icon of table `0x66367C` |
| `0x573F30` | `Menu_DrawItemIcon` | 0x32 | `0x801D90E8` call-anchored | the same, icon 4 as 0xB once `Cond_ByteFA` >= 8 |
| `0x574530` | `Menu_DrawExpBar` | 0xD4 | `0x801DA18C` call-anchored | the experience bar, a red LINE_F2 |
| `0x574610` | `Menu_DrawMoneyBox` | 0xAD | `0x801DA2A8` call-anchored | the money box: "%7d" in the 12 px font |
| `0x5747D0` | `Menu_YesNo` | 0xB2 | `0x801DA628` gap15 | the Yes / No chooser (DIV-0027) |
| `0x574890` | `Menu_DrawButtonRow` | 0x156 | `0x801DA738` callers | the verbs above a panel (DIV-0018) |
| `0x5749F0` | `Item_Price` | 0x6E | `0x801DA988` call-anchored | an item's price by category |
| `0x574A60` | `Char_ExpForLevel` | 0x45 | `0x801DAA30` call-anchored | the experience a level needs |
| `0x574AB0` | `Menu_DrawTitleBox` | 0x404 | `0x801DAB48` call-anchored | the screen title's box, and its outline |
| `0x575430` | `Shop_DrawMemberStats` | 0x25F | `0x801DB988` callers | the weapon shop's member panel |
| `0x575690` | `Menu_DrawBackdrop` | 0x195 | `0x801DBCBC` call | the menu's tiled backdrop |
| `0x575830` | `Menu_DrawPanel` | 0x18C | `0x801DBEB0` call-anchored | a panel: the box and its frame of pieces |
| `0x5759C0` | `Menu_DrawItemList` | 0x58C | - | the item list window |
| `0x5762D0` | `Menu_DrawBorder` | 0x11D | `0x801DD304` call-anchored | the shop windows' frame |
| `0x57CF60` | `Menu_DrawBox` | 0x3F8 | `0x801AF3F0` call-anchored | the menu box |
| `0x57D360` | `Menu_DrawIcon8` | 0xB8 | `0x801AFA80` call-anchored | an item kind's 8 x 8 icon |
| `0x57D420` | `Menu_DrawOutline` | 0xFF | - | a bevelled outline of four lines |
| `0x57D760` | `Menu_DrawLine` | 0xA0 | - | one semi-transparent LINE_F2 |
| `0x57D800` | `Text_CharCount` | 0x27 | `0x801B020C` call-anchored | characters in a string, at most 16 bytes |
| `0x57D830` | `Menu_PieceRect` | 0x2B | - | a piece's rectangle |
| `0x57D860` | `Menu_DrawPiece` | 0xAF | `0x801B02A0` call-anchored | one piece |
| `0x57D910` | `Menu_DrawPieces` | 0x8B | `0x801B0390` call-anchored | a list of pieces |
| `0x57D9A0` | `Item_CanUse` | 0xCC | `0x801B0494` callers | whether a list shows an item as usable |
| `0x57DBF0` | `Menu_DrawItemRow` | 0x99 | - | one item row: icon, name, count |
| `0x57DD10` | `Menu_DrawScrollBar` | 0x1E2 | `0x801B0A14` callers | a list's scroll bar |
| `0x57DF00` | `Menu_ListScroll` | 0xED | - | a list's scroll step |
| `0x59B580` | `Shop_DrawBuyList` | 0x22A | `0x801E2DBC` callers | a shop's buy list |
| `0x59B820` | `Shop_DrawBuyDetail` | 0x335 | `0x801E313C` callers | the buy list's detail |
| `0x59BBC0` | `Shop_DrawSellDetail` | 0x282 | - | the sell detail |
| `0x5A7720` | `Gpu_SetSprt8` | 0x10 | `0x8017B36C` (SetSprt8) | code 0x74, SPRT_8 |

Sizes against `analysis/calltrace/entries_logic.txt`: three were wrong -
`0x59B580` 0x29D there (0x22A), `0x59B820` 0x39D (0x335), `0x59BBC0` 0x34D
(0x282); the extra bytes are pointer-reached neighbours (below). `0x575430`
(0x25F) and `0x57D9A0` (0xCC) include their jump tables. No listed function
was a case label.

**Pointer-reached neighbours, found and not taken:** `0x59B7B0`,
`0x59B7E0`, `0x59B810`, `0x59BB60`, `0x59BB80`, `0x59BBB0`, `0x59BE50`,
`0x59BE70`, `0x59BEA0` - the shop windows' task handlers (slide-ins of
+4 by 0x20 a frame; `0x59BB60` and `0x59BE50` dispatch through
`[0x66B2BC + state * 4]` / `[0x66B2D8 + ...]` and then draw through
`Menu_DrawItemList` / `0x585090`). None is in `entries.txt`, so the tracer
has never seen them; they belong with the window-record kinds
([`window-task.md`](window-task.md)), a cluster of their own.

**Read against the PlayStation:** `Text_DrawFont12` against `0x8015002C`
(the sibling's `tools/disasm_exe.py`): the same `y + 1`, CLUT
`0x7800 | (colour & 0x3F)`, `c % 21` / `c / 21` (a multiply by
`0x86186187`), the newline that resets x before the step that adds 12, and
the end test on the next byte - so the quirks below are the PlayStation's
too. `Gpu_SetSprt8` against `0x8017B36C`: code 0x74 (the PC writes its own
float 0.01 at +0x10 where the PSX writes the length 3). The rest of the
twins are in overlays (`0x801AF3F0` and up, `0x801D....`) that the boot EXE
does not hold; not read.

## 2. What they do, and the data

The window records the list and shop draws are handed (their fields as the
functions read them):

| Offset | Item list `0x5759C0` | Buy list / detail | Sell detail |
|---|---|---|---|
| +4, +6 | x, y (s16) | x, y | x, y |
| +8 | the `Item_CanUse` mode | | |
| +9 | greyed | | |
| +0xA | category (0..4) | greyed / price percent | category |
| +0xB | top index | percent / selected entry | id |
| +0xC | a marked index | cursor / quantity | quantity |
| +0xD | cursor | | `0x583100`'s flag |
| +0x10 | word: arrow bits 1, 0; countdown in the high nibble | | |
| +0x12 | scroll state (`Menu_ListScroll`) | | |
| +0x20 | | dword: the shop list (count, then (category, id) pairs) | |

Tables: inventory ids per category at `0x656B00` (`0x904154`, `0x9041D4`,
`0x904254`, `0x9042D4`, `0x904554`), counts at `0x656B14` (four, then 0 for
key items); prices in each item record (`Item_Price`); the experience table
`0x658F48`, 0x318 bytes a member; the piece rectangles `0x663B94` /
`0x663C8C`; icons `0x66367C`; the backdrop's rectangles `0x663874`, pattern
`0x663920`, starts `0x66396C`. The print buffer `0x904BA0` takes every
number (`%7d`, `%3d`, `%3d/%3d`, `*%2d`, `%6dZ`, `%7dZ`).

The draws write the PC team's own primitive format into the packet pool
(floats at +8 / +0xC and on, as [`menu-screens.md`](menu-screens.md) §2
found for the backdrop); `Menu_DrawBox` and `Menu_DrawTitleBox` put an 8-byte
RECT in the pool for a draw mode's texture window. Each function's comment
in `menu_windows.cpp` is the full read; the points that matter:

- **Upper halves.** The originals push many values built with 8- and 16-bit
  operations; every callee here reads only the low 16 bits of a coordinate
  and the low byte of a colour, flag or id (the stand-ins record exactly
  that), so ours passes values with the upper bits C++ gives them.
- **Stack reuse.** Several originals keep a value in an argument's own slot
  (`Menu_DrawButtonRow`'s colour in `set`'s, `Menu_DrawItemList`'s scroll
  offset in the pointer's low byte, `0x590960`'s outputs in `slot`'s).
  Unobservable to a caller; ours uses locals.
- **Reads after calls.** Where the original reads a field after a call, ours
  reads it after the same call; the fuzz found one where ours did not
  (`Shop_DrawBuyList` pushes the price percent `+0xB` before `Item_Price` and
  uses it for the scale after - fixed before any commit).
- **Kept quirks** (each named in the comment on ours): the fonts' newline
  indent and next-byte end test (the PlayStation's too, section 1);
  `Menu_DrawBox`'s middle quads' bottom v (D-NEW-Y-a); `Menu_DrawBox` places
  its quads at 0x48-byte steps from the first, each a byte copy of the one
  before; `Shop_DrawMemberStats` shows `0x590960`'s outputs 0, 1 and 3, not 2;
  `Menu_DrawExpBar`'s arithmetic is unsigned and wraps; `Menu_DrawPanel`'s
  byte counter (never ends for w >= 251), `Menu_DrawScrollBar`'s divide by a
  total of 0 (ours faults the same way, through the same `idiv`),
  `Menu_DrawItemList`'s row counter and null key-item counts (D-NEW-Y-b).

## 3. Not as the original

`Menu_DrawBackdrop` with a `kind` of 4 or more: the original reads its four
CLUT words past their end, into its own frame and its caller's; ours aborts
through `bof3::Fatal` (CLAUDE.md rule 4 - the read cannot be reproduced).
Config keeps `0x903A5B` in 0..3 (`0x461239` / `0x46126D`), so only a corrupted
save reaches it. **This is a loud abort for an unreachable input, not a
ledgered divergence**; whether it wants a DIVERGENCE entry is the owner's
call.

## 4. The divergences whose patch sites are inside these bodies

- **DIV-0018** re-aims `Menu_DrawButtonRow`'s label call `0x57499B` at
  `MenuVerbs_DrawLabel` under a language overlay (`BOF3X_ORIGINAL=MenuVerbs`).
- **DIV-0027** re-aims `Menu_YesNo`'s line call `0x5747D2` at `YesNo_Line`
  and patches the hand's stops at `0x5747F0` / `0x5747F7` / `0x5747FC`
  (`BOF3X_ORIGINAL=YesNoLayout`).

Both modules still patch Capcom's bytes exactly as before; `MenuWindows_Inject`
(which runs after them) reads back what went in - the call sites' targets
(`MenuVerbs_ActiveLabel`, `YesNoLayout_ActiveLine`) and the stop bytes
(`YesNoLayout_StopsMoved`) - and ours does the same. So both survive in ours
under their existing names, and `BOF3X_ORIGINAL=Menu_YesNo` /
`=Menu_DrawButtonRow` still runs Capcom's body with the divergence's bytes in
it. The log line `menu_windows: DIV-0018 label ..., DIV-0027 line ..., hand
stops ...` says which.

## 5. The fuzz (`BOF3X_SHADOW=menu_windows`, `menu_windows_fuzz.cpp`)

Thirty-seven byte-copies; every call out re-aimed at a recording stand-in,
for ours through `menu_windows::g` alike (the two divergence sites are
re-aimed without checking their original target); the two jump tables
(`0x575430`'s at +0x244, `0x57D9A0`'s at +0xBC) relocated in the copies.
2,000 rounds a function: random bytes in fourteen game regions (MsgBoxState,
the packet cursor, the pad, the leader's position, `Cond_ByteFA`, the
buttons, the window colour, ten character records, the gold and party lists,
the inventory, the print buffer, the current window record pointer, the menu
state, `Frame_Counter`) and in our own pool, window records, shop list,
strings and piece lists; then each branch's boundaries seeded; the copy,
then ours, both under x87 control word 0x027F; everything compared, with the
answer at the original's width (al for `Menu_YesNo`, `Text_CharCount`,
`Item_CanUse`, `Menu_ListScroll`; eax for `Item_Price`, `Char_ExpForLevel`,
`Menu_PieceRect`) and the stand-ins' log, strings compared by content.

The stand-ins do what callers read back: the commit moves the packet cursor
(and stamps the committed primitive's tag, which `Menu_DrawBox`'s copies
carry on), the setters write their codes, sprintf writes the print buffer
without the CRT (the self-test runs before it), the name and message lookups
answer our strings, `Text_DrawImmediate` answers where the next item starts,
the scroll step writes its outputs (moving 0, 1 or 2), `0x590960` writes its
eight outputs; and most disturb a cell some caller reads again after the
call - the colour, the selection, the pad, the window fields (a category
stays 0..4, and a 4 stays a 4: D-NEW-Y-b), the list count, the current
record, the inventory, the gold, `0x905BA2`, `Cond_ByteFA`.

Results (`build/bof3x.log`): 74,000 rounds, 3,646,663 stand-in calls, 0
mismatches; the same with `BOF3X_LANG=en` (DIV-0018 and DIV-0027 on in the
copies and in ours); `BOF3X_SHADOW='*'` passes. Coverage: Yes/No answered 0
1,554 and 1 446 times; `Item_CanUse` no 952, yes 1,048; 1,468 wide boxes;
12,551 greyed item rows; 28 Faerie Tiara area tests.

**Negative controls** (planted one at a time through a temporary switch,
all removed):

| # | Planted | Refused in |
|---|---|---|
| 1 | `Msg_OpenSystem` index + 1 | 2,000 |
| 2 | the arrow on bit 4 | 996 |
| 3 | 12 px font u by c mod 20 | 1,999 |
| 4 | 8 px font newline 12 | 1,886 |
| 5 | kind-2 list one item fewer | 1,311 |
| 7 | cursor box blinks on bit 2 | 457 |
| 8 | icon shade 1 as 0x31 | 467 |
| 9 | icon 4 swap at `Cond_ByteFA` >= 9 | 153 |
| 10 | exp bar signed division | 1,113 |
| 11 | money in `%3d` | 2,000 |
| 12 | Yes/No selection unsigned | 788 |
| 13 | verb 0x12 greyed on bit 1 | 112 |
| 14 | accessory price stride 26 | 281 |
| 15 | exp for levels up to 0x64 | 175 |
| 16 | title box half by logical shift | **not refused** |
| 17 | member stats third output kept | 816 |
| 18 | backdrop rows to 0xF0 inclusive | 2,000 |
| 19 | panel bottom w + 4 | 2,000 |
| 20 | item list mark shift on +0x12's low byte only | 170 |
| 21 | item list second mark row's category not re-read | 3 |
| 22 | border's last corner 0x2E | 2,000 |
| 23 | box middle's v from y + h (D-NEW-Y-a "fixed") | 1,883 |
| 24 | box wide from 0x101 | 290 |
| 25 | icon 15 not special | 163 |
| 26 | outline's top edge under the other abr | 2,000 |
| 27 | line abr & 7 | 965 |
| 28 | char count to 17 bytes | 1,465 |
| 29 | piece rectangle on flag bit 0 | 989 |
| 30 | piece CLUT on flag bit 2 | 1,045 |
| 31 | pieces' page on flag bit 1 | 946 |
| 32 | can-use mode 3 on bit 4 | 156 |
| 33 | item row count from 1 | 447 |
| 34 | scroll thumb not a byte | 326 |
| 35 | scroll step mod 11 | 309 |
| 36 | buy list percent read after the price (the bug the fuzz found) | 17 |
| 37 | buy detail equipped count as held | 2,000 |
| 38 | sell detail quantity at y + 0x13 | 2,000 |
| 39 | SPRT_8 code 0x75 | 2,000 |
| 40 | box left end 3 wide | 2,000 |
| 41 | equipment panel without its stat icon | 2,000 |
| 42 | item list shadow for a greyed mark row | 675 |
| 43 | box right end's top inset on flag 0x20 | 926 |
| 44 | 12 px font y not one below | 1,999 |
| 45 | 8 px font cell row by unsigned division | 1,893 |
| 46 | scroll tick height - 1 | 2,000 |
| 47 | Yes/No confirm on No plays 0x104 | 34 |
| 48 | member stats mask bit by m & 7 | 188 |
| 49 | cursor box w taken whole | 1,705 |
| 50 | kind-2 list x by logical shift | 856 |
| - | DIV-0027's stops inverted, overlay off and on | 1,565 each |

Control 16 is **a change that changes nothing**: `half` differs between the
two shifts only when `w + 1` is below 4, and then only above bit 15 - every
use of it takes its low 16 bits (`movsx`) or its low byte. Control 21 is
weakly seen (3 rounds): it needs the category to change under the shadow
draw's call.

**Not tested by the fuzz:** the order of `Menu_DrawEquipPanel`'s name
lookups against the stat icon (control 41 dropped the call rather than
moving the reads); reads of `.data` tables the fuzz does not disturb
(`Menu_ButtonSets`'s count, re-read per button, the piece and icon tables);
`Menu_DrawBackdrop`'s abort (kind >= 4 never seeded).

## 6. What nothing reached yet

The shop route (`tools/recipes/shop.txt`) reaches all thirty-seven by the
catalogue's counts (`analysis/shop_catalog.md`: from 4 calls of
`Msg_OpenSystem` to 257,728 of `Menu_DrawPiece`) - the live A/B after the
merge is the check of the whole. Nothing here has been seen drawn by ours
yet. Within the functions, what the route cannot reach: `Menu_DrawBox`'s
wide path (no box of 0x100 or more is known), the fonts' newline, the
backdrop's abort, the scroll bar's divide fault, the panel's hang, and
`Item_CanUse`'s Faerie Tiara test unless the route opens the item list on
the tiara.
