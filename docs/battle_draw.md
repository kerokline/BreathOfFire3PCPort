# Three Gouraud handlers, the battle menu's two lists, the area tint (group BJ)

**Status:** IN PROGRESS (2026-09-23) - six functions ours in
[`src/game/battle_draw.cpp`](../src/game/battle_draw.cpp), each read to its
last instruction, fuzzed against Capcom's at start-up
(`BOF3X_SHADOW=battle_draw`, 102,000 rounds, 0 mismatches; `BOF3X_SHADOW='*'`
exit 0, 806 ours) with 66 negative controls: 64 refused by a comparison, one
refused only by a fault and replaced, one blind at first and refused once the
fuzz was sharpened (section 4.1). No divergence. One candidate defect of
Capcom's (section 5). **Through the wave-2 batch** (2026-09-24, 1,020 ours - [`takeover-queue-round7.md`](takeover-queue-round7.md) "Result": the combat A/B 5 of 43 at 4..15 px, the tile-edge class); the combat A/B reaches all six.

Group BJ of the seventh parallel round
([`takeover-queue-round7.md`](takeover-queue-round7.md)). Every claim below is
from capstone over `bof3/BOF3.exe` on 2026-09-23 (the scratchpad's `bjdis.py`,
the same reads `tools/pe_disasm.py` gives; call sites by the scratchpad's
`bjsites.py` and a rel32 scan, `bjcallers.py`). Call counts are the combat
route's trace `analysis/calltrace/recipe_combat/bof3x.callcounts.tsv`.

## 1. The functions

| PC | name | true size | calls (combat) | PSX twin |
|---|---|---|---|---|
| `0x5A0E80` | `D3d_DrawPolyG3` | `0x1C8` (to `0x5A1047`), code `0x30`, table entry 4 | 9,088 from `0x59F0E2` | none (the PSX drew on its GPU) |
| `0x5A18B0` | `D3d_DrawLineG2` | `0x14D` (to `0x5A19FC`), code `0x50`, entry 11 | 3,140 from `0x59F144` | none |
| `0x5A1B50` | `D3d_DrawLineG3` | `0x1BE` (to `0x5A1D0D`), code `0x58`, entry 12 | 3,140 from `0x59F152` | none |
| `0x59CD00` | `BattleMenu_DrawItemList` | `0x4F2` (to `0x59D1F1`), 25 calls | 222 from `0x59CB59` | none in `pairs_propagated.json` |
| `0x59D200` | `BattleMenu_DrawSkillList` | `0x436` (to `0x59D635`), 23 calls | 284 from `0x59CBD9` | none in `pairs_propagated.json` |
| `0x573050` | `AreaMap_TintClut` | `0x28` (to `0x573077`), a call and a tail `jmp` | 12 (6 from `0x4FB03A`, 6 from `0x4FB099`) | `0x801565B4` |

The queue's sizes were right for all six. The handlers' codes are from
[`d3d-draw.md`](d3d-draw.md) section 2's table; the one call site of each is
its table case in `Gfx_DrawOTag` (ours, which calls them through the address,
`d3d_list.cpp`'s `H(0x5A0E80)` etc., so no change there). Every relative jump
of all six stays inside its body; the handlers' two COM calls go through
`D3d_Device`.

**The two lists' caller.** The queue names `0x59CA00`; the calls are really
from `0x59CB40` and `0x59CBC0` - entries 1 and 2 of the window-task handler
table `0x66B534` (Field_RunTaskRecords' dispatch, [`window-task.md`](window-task.md)),
each of which calls a step from its own table by the record's state byte +3
and then this with the current record `0x905B84`. Not ours; not taken (none
of the three is in this group). `0x59CA00` itself is a different function
(it draws one sprite primitive from the 4-byte records at `0x66B470`) and does not call either.

**The PSX twin of `AreaMap_TintClut`** is not in the propagated pairs; it was
found by scanning the sibling's generated code for a `jal 0x80158CE8`
(`Gfx_ClutAdjust`'s twin) followed by `0x80158B60` (`AreaMap_ClutCycleStart`'s):
one hit, `0x801565B4`, which tests bit 0 of a status byte and makes the same
two calls with `(0xFFFF, 0xFFF, a, a, a)`.

## 2. What each does

**The three handlers** have `D3d_DrawPolyG4`'s shape
([`field-misc.md`](field-misc.md) section 1.7) with fewer corners: n colour
calls `D3d_PrimColor(r, g, b, code +7, Gfx_DrawTpage & 0xFFFF, &local[i], NULL)`
from `+4 + 0x10 i` - the code byte and the mode re-read for each, all before
any corner - then n corners of `0x10` from `+8` (`sx`, `sy` scale times x, y
by `fld; fmul; fstp`, `sz` = z by `mov`, `rhw` = 0.1 / z by `fld; fdiv`, the
diffuse; specular, `tu`, `tv` left as the last draw left them),
`SetTexture(0, NULL)`, the bare `ret` `0x437CC0` twice, `D3d_SetBlend(code,
mode re-read)`, `D3d_SetShadeMode(2)`, `DrawPrimitive(type, 0x1C4,
D3d_Vertices, n, 0)` whose result is returned.

| handler | n | bare-ret args | primitive |
|---|---|---|---|
| `D3d_DrawPolyG3` | 3 | 0, 0 | `TRIANGLELIST` (4) of 3 |
| `D3d_DrawLineG2` | 2 | 0, 1 | `LINESTRIP` (3) of 2 |
| `D3d_DrawLineG3` | 3 | 0, 1 | `LINESTRIP` of 3 |

`D3d_DrawPolyG3` alone also stores 0 into the **fourth vertex's z**
(`0x7CA9C0`, between its third corner's stores) - no draw of its reads it
(three vertices); kept.

**`BattleMenu_DrawItemList(window)`** - `Menu_DrawItemList`'s shape
([`menu-windows.md`](menu-windows.md)) in seven rows and a shorter frame. The
record: `+4`/`+6` x, y; `+8` the `Item_CanUse` mode; `+9` greyed (and the
boxes' flags); `+0xA` the category; `+0xB` the top; `+0xC` a mark; `+0xD` the
cursor; word `+0x10` the arrow bits and a countdown; `+0x12` the scroll state.
`Menu_DrawBox(x + 3, y + 3, 0x99, 0x82, +9, 0x903A5A)`;
`Menu_ListScroll(+0xB, &offset, &moving, +0x12)` (the offset lives in the
function's own argument slot in the original, a local in ours); rows 13 apart
from `(u16)(s8 offset + y) + 0x1A`, `moving + 7` of them, over
`0x656B00[+0xA] + top` and `0x656B14[+0xA] + top` (both advanced on empty
rows too). A row with an item: dim when greyed or when
`Item_CanUse(+8, 0x66972C[0x904065[s8 0x929F06]], +0xA, id)` says 0 - the
**second** party list `0x904065`, where the field's list reads `0x904062`;
colour 7 dim / 0, and 2 when `i + +0xB == +0xD`; the count 1 for category 4,
else the counts array's; the cursor's and the marked row (`== +0xC`) raised 2
over a colour-7 dim shadow, which is left out when the colour is 7. Then the
title and footer boxes, the title `0x66B58C[+0xA]` centred on 13 characters by
`Text_CharCount` (count `0x10`), `sprintf("%3d/%3d", Inventory_CountUsed(cat),
cat == 4 ? 0x20 : 0x80)` through `Text_DrawFont8`, the arrows by word `+0x10`
bits 1 and 0 (`0x66B4B0/C8`, `0x66B4BC/D4`), the countdown (word less `0x10`
while its bits `0xF0` are set, else 0), 51 frame pieces, and
`Menu_DrawScrollBar(0x656B00[+0xA], +0xB, x + 0x90, y + 0x18, 7, 0x80, 0x5C)`
- a total of `0x80` even for category 4, where the field list passes `0x20`.

**`BattleMenu_DrawSkillList(window)`** - the same frame over a skill list.
The record: `+8` the usable test's mode; `+9` the arrow bits and the
countdown (a byte here); `+0xA` and `+0xB` the member and the list's kind,
handed to group BD's `0x591E50(+0xA, +0xB, 1)` (it answers a 10-byte list in
the member's battle record `0x802DC0 + 0x14C m`, `+0x6A/+0x74/+0x7E` for kinds
1..3, `+0x60` else - read, not taken); `+0xC` a mark, `+0xD` the cursor;
`+0x10` the top (a byte to the scroll step, read back as an **s16** per row);
word `+0x14` selects the title `0x66B5B0` (`0x66A220`, which
[`dialogue-localisation.md`](dialogue-localisation.md) names the skill list's
header 龙技) over `0x66B5A0[+0xB]`. The box with flags 0; the scroll step on
`+0x10`; per id: `0x57DA70(+8, 0x929F06, id)` - dim when it answers 0;
colour 7 / 0, and 2 when the **row index** equals `+0xD`; `0x591DB0(0x929F06,
id, 1)` (the number the row draws) and `0x5918A0(id)` (the icon kind); the row
through `0x57DC90(x + 7, y, colour, icon, 0x65C4C8 + 0x18 id, number, dim)`;
raised 2 over the dim shadow when `s16 +0x10 + i` equals `+0xD` or `+0xC`.
Then the title box, the title centred by its **byte length** (an inline
`repne scasb`), the arrows by `+9`, `+9`'s countdown, 51 frame pieces and two
more with word `+0x14`, and `Menu_DrawScrollBar(0x591E50(+0xA, +0xB, 1), +0x10,
x + 0x90, y + 0x18, 7, 0xA, 0x5C)` - x, y and the top read before that second
lookup.

**`AreaMap_TintClut(level)`** - nothing while `Field_StatusBits` bit 0; else
`Gfx_ClutAdjust(0xFFFF, 0xFFF, level, level, level)` (every column of CLUT
rows 3..14, all three channels) and a tail jump to `AreaMap_ClutCycleStart`.
Its three callers (`0x4B51E0`, `0x4FB010`, `0x4FB070`, reached through
pointer tables) step a byte of the record `0x937F88` by one and pass it
sign-extended, then `pop ecx; ret` - so they return whatever this leaves in
`eax`: their own `eax` on the early exit, `AreaMap_ClutCycleStart`'s (ours,
void) on the other. Nothing can rely on it; ours is void.

## 3. Upper halves, and what group BD's functions read

The list draws push many values built in 8- and 16-bit registers, and several
dword stack slots of which only the low byte was ever written (the dim flag,
the colour, the count, the number and icon kind). As `menu_windows.cpp` does,
ours passes clean values and the fuzz compares what each callee reads: 16 bits
of a coordinate, the low byte of a colour, flag, id or count. For the menus'
callees that is their reimplementations' reads (`menu-windows.md`). For group
BD's, from their first instructions, 2026-09-23:

- `0x57DC90(x, y, colour, icon, record, number, dim)`: `icon & 0xFF` indexes
  `0x663D70`; `x`, `y + 2` and `dim` go whole to `Menu_DrawIcon8` (16 bits,
  `dim & 0xFF` there); `record` is the name to `Text_CharCount` and
  `Text_DrawAt(x + 0xA, y, colour, n, record)`; `number & 0xFF` into
  `sprintf(0x904BA0, 0x64D3EC, ...)` and `Text_DrawFont8(x + 0x6D, y + 2,
  colour, ...)`. So: one skill row - icon, name, number.
- `0x591E50(a, b, c)`: `a & 0xFF`, `b & 0xFF`, `c` tested as a byte.
- `0x57DA70(mode, member, id)`: `id` tested as a byte and masked, `mode & 0xFF`,
  `member` masked for its own index and passed whole to `0x591DB0`.
- `0x591DB0`, `0x5918A0`: their answers are read as `al` only.

When group BD takes these over, theirs should read no more of each argument
than this, or the stand-ins' assumption has to be revisited.

## 4. The fuzz

`BOF3X_SHADOW=battle_draw`, one start-up run of a few seconds. Six
byte-copies, every `E8` (and `AreaMap_TintClut`'s `E9`) re-aimed at a
recording stand-in, ours on the same stand-ins through `battle_draw::g`.

- **The handlers** on the vertex-block harness (`d3d_fuzz.h`, unchanged: its
  log, fake device and `DrawPrimitive` snapshots), group R's float seeds
  (signed zeros, denormals, infinities, quiet and signalling NaNs, `FLT_MAX`,
  0.01, 0.99, 2^24 - 1, 1/3) and scales, colours 0, 1, `0x7F`, `0x80`, `0xFF`,
  code bytes `0x30..0x5A`, each round under a control word from `0x027F`,
  `0x007F`, `0x037F`. The stand-ins write the diffuse and a quarter of the
  time change a byte of the primitive, the vertex block, the scales or
  `Gfx_DrawTpage`. Compared: calls with arguments, vertex snapshots, the
  result, the vertex block, scales, tpage and primitive.
- **The lists** log into this file's own log (320 calls; a round makes up to
  about a hundred - more than `d3d_fuzz`'s 48 hold). Seeded: the window at
  x, y near 0 / `0x7FFF` / `0x8000` / `0xFFFF`; greyed 0 half the time; tops
  0..3 mostly, any byte else, and the skill list's high byte of `+0x10` 0,
  `0xFF`, 1 or `0x80`; cursor and mark as `top + 0..8` and (skill list) as a
  row index `0..8` - both of the original's tests; the categories 0..4; word
  `+0x14` zero half the time; a third of the inventory ids and skill ids 0;
  party bytes and the menu member small, now and then any byte. The scroll
  step answers offsets 0, -1..-11 or any byte, moving 0, 1 or 2, and moves
  the top or state now and then; `Item_CanUse` and `0x57DA70` answer 0, 1 or
  any byte; `Text_CharCount` 0..19 (the centring goes negative). The
  stand-ins then change, a quarter of the time each, the window colour, the
  menu member, a byte of the record, a party byte, **an id or count of a row
  in view** or a byte of the skill list in view (targeted - see I23 below),
  a mark or the cursor, or word `+0x10` / `+0x14`. The rules both originals
  would otherwise fault on are kept: a category 4 stays 4 (D40's null counts
  pointer), the skill title index stays 0..4 (`0x66B5A0[5]` is not a pointer).
  Compared: every call with its arguments (text by hash), the window, the
  skill list, the inventory `0x904154..0x904703`, the party lists, colour,
  menu member and the print buffer.
- **The tint**: the status byte with bit 0 set half the time; levels 0, 1, -1,
  `0x7F`, -128, `0x1F`, -31 or any dword.

Result, 2026-09-23: **102,000 rounds, 0 mismatches.** Per function (rounds /
calls out / covered): `D3d_DrawPolyG3` 20,000 / 180,000 / 6,633 under
`0x007F`; `D3d_DrawLineG2` 20,000 / 160,000 / 6,657; `D3d_DrawLineG3` 20,000 /
180,000 / 6,543; `BattleMenu_DrawItemList` 20,000 / 1,417,961 / 16,683 with a
scrolled top; `BattleMenu_DrawSkillList` 20,000 / 1,638,849 / 16,690;
`AreaMap_TintClut` 2,000 / 2,024 / 1,012 with the tint on.
`BOF3X_SHADOW='*'`: exit 0, no mismatch, 806 ours.

### 4.1 Negative controls

Each planted alone in `battle_draw.cpp` by the scratchpad's `bj_controls.py`
(edit, build, headless self-test, restore). The count is mismatching rounds
of 20,000 (2,000 for the tint), from the second run - on the final fuzz; the
first run's verdicts were the same except I23 and S15, below.

| # | control | refused |
|---|---|---|
| H1 | G3: triangle strip | PolyG3 20,000 |
| H2 | G3: no fourth-z store | PolyG3 20,000 |
| H3 | G3: the zero store on vertex 2 | PolyG3 20,000 |
| H4 | G3: flat shade | PolyG3 20,000 |
| H5 | G3: second bare ret gets 1 | PolyG3 20,000 |
| H6 | LG2: second bare ret gets 0 | LineG2 20,000 |
| H7 | LG2: three vertices | LineG2 20,000 |
| H8 | LG3: a strip of 2 | LineG3 20,000 |
| H9 | colour stride `0xC` | PolyG3 20,000, LineG2 19,992, LineG3 20,000 |
| H10 | mode read once for the colours | 1,865 / 976 / 1,934 |
| H11 | code byte read once | 70 / 77 / 88 |
| H12 | `rhw` in plain C float | 28 / 10 / 21 (under `0x007F`) |
| H13 | `sx` times the y scale | 19,204 / 18,728 / 19,162 |
| H14 | `sz` through the x87 | 2,718 / 1,883 / 2,796 (signalling NaNs) |
| H15 | a specular pointer | 20,000 each |
| H16 | colours interleaved with corners | 3,249 / 1,565 / 3,363 |
| H17 | G3: `SetTexture(0, 1)` | PolyG3 20,000 |
| H18 | G3: blend mode masked to a byte | PolyG3 12,980 |
| I1 | items: `moving + 9` rows | 17,718 |
| I2 | items: the first party list | 7,068 |
| I3 | items: member index unsigned | 128 |
| I4 | items: dim inverted | 9,911 |
| I5 | items: colour 2 on the row index | 6,755 |
| I6 | items: key items' count from memory | 3,942 |
| I7 | items: shadow even at colour 7 | 3,359 |
| I8 | items: raised row's category not re-read | 47 |
| I9 | items: raised row's x not re-read | 140 |
| I10 | items: offset unsigned | 11,127 |
| I11 | items: title centred on 12 | 20,000 |
| I12 | items: title x read before the count | 410 |
| I13 | items: room `0x80` for key items too | 3,944 |
| I14 | items: sprintf arguments swapped | 19,934 |
| I15 | items: arrow bits swapped | 8,256 |
| I16 | items: countdown tested on `0xFF` | 618 |
| I17 | items: bottom row of 10 pieces | 20,000 |
| I18 | items: scroll total `0x20` for key items (the field list's) | 3,944 |
| I19 | items: no marked row | 4,838 |
| I20 | items: first box flags 0 | 9,886 |
| I21 | items: footer box colour not re-read | 1,248 |
| I22 | items: counts not advanced on an empty row | 14,242 |
| I23 | items: the shadow row's id read before `Item_CanUse` | 5 - **not refused on the first run** (the fuzz was blind: a random byte of the whole inventory hardly ever hit the row in view); the disturbance was aimed at the rows in view and the control refused |
| I24 | items: `Item_CanUse` called when greyed too | 10,928 |
| S1 | skills: colour 2 on top + row (section 5's quirk "fixed") | 7,043 |
| S2 | skills: top read as an unsigned byte | 2,374 |
| S3 | skills: record stride `0x14` | 19,990 |
| S4 | skills: number and icon swapped | 19,980 |
| S5 | skills: usable inverted | 19,991 |
| S6 | skills: title by the member | 5,659 |
| S7 | skills: countdown keeps a zero-nibble byte | 1,821 |
| S8 | skills: bottom row of 9 pieces (the item list's) | 20,000 |
| S9 | skills: the two extra pieces always | 6,202 |
| S10 | skills: scroll top read after the lookup | 731 |
| S11 | skills: scroll total `0x80` | 20,000 |
| S12 | skills: box flags `+9` | 10,017 |
| S13 | skills: cursor read once for both tests | 383 |
| S14 | skills: list arguments swapped | 19,930 |
| S15 | skills: scroll step on `+0xB` | refused by a **fault** only (the step's stand-in writes the top, here the title index, out of 0..4, and both sides read a non-pointer) - replaced by S15b |
| S15b | skills: scroll step on `+0x11` | 20,000 |
| S16 | skills: title centred on 12 | 20,000 |
| S17 | skills: number's id read before the usable test | 324 |
| S18 | skills: raised row at `+ 2` | 5,056 |
| T1 | tint: bit 1 tested | 1,001 |
| T2 | tint: rows `0xFFFF` | 1,012 |
| T3 | tint: blue from the level's byte | 505 |
| T4 | tint: no cycle restart | 1,012 |

## 5. Found on the way

**Candidate defect (Capcom's, kept): the skill list's highlight colour and its
raised row disagree once the list is scrolled.** `BattleMenu_DrawSkillList`
colours a row 2 when its **row index** `i` equals `+0xD` (`cmp dl, al` at
`0x59D2D6`, `dl` the byte counter), but raises it over a shadow when **top +
i** (`s16 +0x10`) equals `+0xD` (`cmp ebp, edx` at `0x59D31B`). With the top at
0 the two agree. Scrolled by t, one of them is off by t rows: if `+0xD` is the
absolute index (as the raise test, the item list and the field list all
treat theirs), the raised cursor row is drawn in the plain colour and the row
at screen position `+0xD` gets colour 2, unraised. A
10-entry list in 7 rows scrolls by up to 3. Read, not seen: whether it shows
depends on the step code (`0x66B560`'s, not read) keeping `+0xD` absolute, and
on a member having more than seven entries in one list. Ours keeps it
(control S1 refuses the fix). For the coordinator to number; worth a look in
game with a long skill list scrolled down.

**D40's two bounds recur** in `BattleMenu_DrawItemList`: the byte row counter
against `moving + 7`, and the category read once for the arrays but again per
row with key items' null count array. Safe as shipped for the same reasons.
And its scroll bar always counts `0x80` entries: on category 4 that reads 96
bytes past the 32-byte key-item list `0x904554` (D34's region), where
`Menu_DrawItemList` passes `0x20`. Whether the battle list is ever opened on
category 4 is not known here; latent at worst.

**The fourth z** that `D3d_DrawPolyG3` zeroes is harmless (section 2).

## 6. What the fuzz does not reach, and the live check

- The real callees: the menus' and the D3D helpers are ours and fuzzed in
  their own modules; group BD's five are Capcom's for now and stand-ins here.
- Pixels, and the x87 control word the renderer really runs under (three are
  checked).
- The window-task step code that moves `+0xB`/`+0x10`, `+0xC`, `+0xD` - the
  seeds cover the tests' both sides, not the values the game produces.

**Reached by the combat route: all six** (section 1). None is fuzz-only. The
combat A/B (`analysis/validate_combat.sh`) is the live check: a wrong
handler would show as missing or miscoloured Gouraud triangles and lines
(the battle's effects - 9,088 triangles in the route); a wrong list as the
battle's item or skill window with rows, colours, the title, the count or
the frame out of place; a wrong tint as a palette fade that does not happen
or happens during a status-bit hold (12 calls in the route).

## 7. Changes to shared code

None. `d3d_fuzz.h` / `.cpp` used as they are. `symbols.toml`: one block at its
end, the six. `inject_all.cpp`: `BattleDraw_Inject()` after
`Widescreen_Inject()` (nothing before it patches bytes inside the six).
