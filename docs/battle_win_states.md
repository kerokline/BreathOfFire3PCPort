# The battle windows' states

**Status:** IN PROGRESS (2026-09-25) - nineteen functions ours
(`src/game/battle_win_states.cpp`, shadow name `battle_win_states`), each
fuzzed headless against a copy of Capcom's with every call and every
stack-table immediate re-aimed at a recorder: 57,000 rounds, 0 mismatches;
41 negative controls planted, 40 refused, one not refused because it changes
nothing the pool can hold. `BOF3X_SHADOW='*'` passes. Not yet live-checked:
the combat A/B after the merge is the check of the whole.

Group CL of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)): the battle's window
states - record handler 3 of `Field_RunTaskRecords` and everything under it.
Everything here is a *faithful* replacement: no `DIVERGENCE.md` entry is
owed (section 4 has the two places ours aborts where the original crashes).

## 1. The functions

Extents read by linear disassembly to the last instruction (capstone,
2026-09-25); every jump in them is internal. "Table" is the stack table the
function is found in: none of the nineteen is ever a `call rel32` target -
each is an immediate in a `mov [esp + 4 i], imm32` run followed by
`call [esp + index * 4]` in the function above it, which is why the
catalogue files them as "jumped to" (the first return address follows an
indirect call, not a rel32 one).

| PC | Name | Bytes | Table | What it does |
|---|---|--:|---|---|
| `0x596FA0` | `BattleWin_Run` | 0x56 | `Field_RunTaskRecords` slot 3 (`0x59E25C`) | the battle window by its kind +2, eight handlers |
| `0x597000` | `BattleWin_PartyRowStates` | 0x2E | kind 0 (`0x596FAF`) | three states: slide in, draw, idle |
| `0x597030` | `BattleWin_PartyRowSlideIn` | 0x3F | party state 0 | +6 less 4 a frame to 0xC8 (signed), then the state on; the row drawn |
| `0x597070` | `BattleWin_PartyRowDraw` | 0x18 | party state 1 | `BattleWin_DrawPartyStatus(+4, +6)` |
| `0x597090` | `BattleWin_CrossStates` | 0x2E | kind 1 (`0x596FB7`) | three states: grow, frame, idle |
| `0x5970C0` | `BattleWin_CrossGrow` | 0x92 | cross state 0 | the five arm icons growing in (`Menu_DrawIcon`), the growths counting down |
| `0x597160` | `BattleWin_CrossFrame` | 0x42 | cross state 1 | the selected arm's growth up to 8, the others down; the cross drawn |
| `0x5971B0` | `BattleWin_LabelStates` | 0x2B | kind 2 (`0x596FC2`) | idle, frame, idle (slots 0 and 2 from one `mov eax, imm32`) |
| `0x5971E0` | `BattleWin_LabelFrame` | 0x16 | label state 1 | `BattleWin_DrawCommandLabel(selection)` unless `0x903A5D` |
| `0x597200` | `BattleWin_BannerStates` | 0x2E | kind 3 (`0x596FCA`) | idle, frame, idle |
| `0x597230` | `BattleWin_BannerFrame` | 0xE5 | banner state 1 | a banner-pool record's text in a box, centred by its glyph count (section 5) |
| `0x597320` | `BattleWin_EnemyGaugeStates` | 0xDC | kind 4 (`0x596FD2`) | four states, then the gauge's step, the out test, the low-HP flag |
| `0x597400` | `BattleWin_EnemyGaugeOpen` | 0xB8 | enemy state 0 | the gauge set up: +0xB = 55 HP / top, +0x14, +0x1C |
| `0x5974C0` | `BattleWin_EnemyGaugeFrame` | 0x4E | enemy state 1 | the enemy's banner; targeted (the slot, or 0x40) steps on |
| `0x597510` | `BattleWin_EnemyTargetFrame` | 0xBE | enemy state 2 | the target banner at the enemy's place; untargeted goes back |
| `0x5975D0` | `BattleWin_MemberGaugeStates` | 0xF2 | kind 5 (`0x596FDA`) | three states, then the HP and AP gauges' steps, the low-HP flag |
| `0x5976D0` | `BattleWin_MemberGaugeOpen` | 0x172 | member state 0 | both gauges set up (AP 0 when its top is 0) |
| `0x597850` | `BattleWin_MemberGaugeWait` | 0x5B | member state 1 | nothing drawn; targeted (the slot, or 0x80) steps on |
| `0x5978B0` | `BattleWin_MemberTargetFrame` | 0x124 | member state 2 | the target banner at the member's place; untargeted goes back |

Kinds 6 and 7 of `BattleWin_Run`'s table are `0x597C70` and `0x597D50`,
group CM's ([`window_kinds.md`](window_kinds.md)); ours calls them by
address. The idle entries are `0x437CC0`, a bare `ret`.

**`0x596A90` is not the window task.** The brief and `battle_windows.md`
call `0x596A90` "the window task" because `pe_funcs.py` gave it an extent
of 0xF44 that swallows all of this group. It is a menu panel draw
(`Menu_DrawRowBox` `0x57CF60`, `Text_DrawAt`, `Crt_sprintf`, `0x57D860`
eleven times, ...) whose first `ret` is at `0x596F97` - 0x508 bytes; it was
not walked for jumps past that `ret`, and it is no group's. The window
records' driver is `Field_RunTaskRecords` `0x59E230` (ours,
[`window-task.md`](window-task.md)), whose nine-entry stack table has
`0x596FA0` at slot 3 (the immediate at `0x59E25C`, the catalogue's
"Pointer in").

## 2. What they do, and the data

### The battle window record

The same 22 records of 0x24 bytes at `0x803160` as the field's windows
([`window-task.md`](window-task.md) section 3), with handler +1 = 3 and a
different layout past +3:

| offset | width | what |
|---|---|---|
| +2 | u8 | the **kind**, 0..7 |
| +3 | u8 | the **state** |
| +4, +6 | u16 | x, y (not 12.4 here: handed to the draws as they are) |
| +8, +9 | u8 | the HP and AP gauges' kind (`Window_DispatchKind` dispatches on it) |
| +0xA | u8 | the **slot**: party member 0..2, enemy 3..8, or (kind 3) a banner-pool record |
| +0xB, +0xC | u8 | the HP and AP gauges' length, 0..55 |
| +0xD, +0xE | u8 | cleared on set-up; the out test waits on +0xD being 0 |
| +0xF | u8 | the pass: `Window_FlagAndAdvance` sets 1, the target states clear it |
| +0x10, +0x12 | u16 | the home place `Window_RestoreAndBack` puts back |
| +0x14 / +0x16 | u16 | HP / AP as shown |
| +0x18 / +0x1A | | handed to the gauge step (group CM's handlers); not read here |
| +0x1C / +0x1E | u16 | HP / AP top as shown |

`0x905B84` holds the record being run; it is re-read wherever the original
re-reads it, and the fuzz's stand-ins repoint it between calls (control 14,
24 and 18 are the refusals).

### Data read

- `0x904AAF` - non-zero while a target is being chosen; `0x939FA0` points at
  the **target byte**: a slot, `0x40` all enemies, `0x80` all members.
- `0x904AB4` the cross's selection, `0x904ABC..0x904AC2` the seven growths,
  `0x64E2AC` the arms' (x, y) bytes (as `battle_windows.md`).
- `0x903A5D` - the command label is drawn only while this is 0. Its writer
  was not looked for.
- The **banner pool** `0x93B8E0`, 12 bytes a record (the pool
  `BattleBanner_Dispatch` `0x44A5C0` walks): +2 wide, +4 the text pointer,
  +0xA the colour.
- **Enemy slot t** is two views of one object: the enemy object
  `0x93B960 + 0x128 (t - 3)` (the one `Battle_ActorIsOut` reads) and its
  working record `EnemyWorkingRecords` `0x93B9E0` = object + 0x80. The
  target banner reads the object's +8 (a side byte), +0x2E / +0x30 (the
  place) and +0xF2 / +0xF3 (s8 offsets) - the same +8, +0x2E, +0x30 the
  member's banner reads from ObjTrio `0x802D40 + 0x14C m`. The gauge reads
  the record's +0x24 HP and +0x30 top; the low-HP flag is bit 13 of its
  word +0x12 (the member's is bit 13 of +0x90, the value's warning colour
  in `BattleWin_DrawPartyStatus`).
- `0x64E2BC` / `0x64E2BE` - the target banner's x offset, s8: `0x15` for a
  side byte of 0 or 3, else `-0x60` (the banner to the right of or left of
  its actor, by the look of the values; not confirmed on screen).
- **`BattleWin_MemberTargetOffsets` `0x64DF70`** (named, `[[data]]`): (dx,
  dy) s8 pairs, row the member's +0x89 (the character id), column its +8;
  unbounded (known-defects.md D64). The bytes run as 11 rows of four pairs to `0x64DFC7`.

Points that matter for faithfulness:

- **The slot's width.** `0x597320` and `0x597400` take the enemy as
  `slot - 3` at full width; `0x597510` as `(slot - 3) & 0xFF`. Identical for
  slots 3..255; below 3 the first two index before the records and the
  third 0x128 x 253.. past them. Kept as each has it.
- **The banner's colour** is pushed as `eax` over the record offset
  `12 k`'s upper bits (`mov al, [eax + 0x93B8EA]`). Ours computes the same;
  for the pool's eight records `12 k` is below 0x100, so the upper bits are
  0 - control 20 (colour pushed as its byte alone) is the one not refused.
- **The two tests after a step.** `0x5974C0` and `0x597850` test the slot,
  step, re-read the target byte, test the side bit and step again (the
  second a tail jump) - a byte of both would step twice (section 4).

## 3. The fuzz (`BOF3X_SHADOW=battle_win_states`, `battle_win_states_fuzz.cpp`)

Nineteen byte-copies; every relative call and tail jump out re-aimed at a
recording stand-in, for ours through `battle_win_states::g` alike, and every
immediate of the seven stack tables (26 of them; `0x5971B0`'s one
`mov eax, imm32` fills two slots) aimed at 26 numbered recording handlers - each immediate is checked against the handler the
disassembly says is there before it is patched. 3,000 rounds a function:
random bytes in seven game regions (the party objects, the label gate, the
battle's bytes `0x904AA0..0x904AEF`, the current window pointer, the target
pointer, the banner pool, the enemy objects and records `0x93B960..0x93C0CF`)
and in our three window records and eight target bytes; then each branch's
boundaries seeded: kind and state bytes over each table, y at and around
0xC8 and past 0x7FFF, growths 0..9 and random, selections 0..7, the label
gate 0 or not, banners wide or not with glyph counts 0..19 and random, HP at
0, at the gauge's floor, a quarter of the top either side, the top and past
it, AP tops 0 half the time, side bytes 0..5, character rows 0..10, target
bytes the slot, 0x40, 0x80, a slot with a side bit, another slot, random.
The copy runs, then ours from the same state; everything compared.

The slot byte +0xA is kept within the round's range (banners 0..7, enemies
3..8, members 0..4) - by the seeds and by the stand-ins' disturbances - so
no address computed from it leaves the regions. Gauge tops are never 0
(section 4).

Results (`build/bof3x.log`): 57,000 rounds, 80,641 stand-in calls, 0
mismatches; `BOF3X_SHADOW='*'` passes (exit 0). Coverage, stand-in calls:
party row 6,000, cross 3,000, label 1,507, small / medium box 1,547 /
1,453, target enemy 3,000, enemy status 3,000, target member 596, advance
1,090, back 2,409, gauge steps 9,000, glyph counts 3,000, texts 3,000,
icons 15,000, out tests 4,507, `Battle_ReturnTrue` 1,532; every one of the
26 table slots 360..2,011 times.

**Negative controls** (planted one at a time through a temporary switch,
all removed; refusals are rounds of 3,000):

| # | Function | Planted | Refused in |
|---|---|---|---|
| 1 | `BattleWin_Run` | kind + 1 | 3,000 |
| 2 | `BattleWin_Run` | dispatched by the state byte | 2,639 |
| 3 | `BattleWin_PartyRowStates` | dispatched by the kind byte | 2,000 |
| 4 | `BattleWin_CrossStates` | state 2 runs state 0 | 1,042 |
| 5 | `BattleWin_LabelStates` | state 2 runs state 1 | 1,000 |
| 6 | `BattleWin_BannerStates` | dispatched by +4 | 2,033 |
| 7 | `BattleWin_PartyRowSlideIn` | stops at 0xC9 | 193 |
| 8 | `BattleWin_PartyRowSlideIn` | unsigned compare | 833 |
| 9 | `BattleWin_PartyRowSlideIn` | state + 2 | 1,513 |
| 10 | `BattleWin_PartyRowDraw` | x and y swapped | 3,000 |
| 11 | `BattleWin_CrossGrow` | size 0x11 - g | 3,000 |
| 12 | `BattleWin_CrossGrow` | four arms | 3,000 |
| 13 | `BattleWin_CrossGrow` | five growths counted down | 1,430 |
| 14 | `BattleWin_CrossGrow` | the record read once | 421 |
| 15 | `BattleWin_CrossFrame` | grows while at most 8 | 160 |
| 16 | `BattleWin_CrossFrame` | a growth of 1 floors at 0 | 1,256 |
| 17 | `BattleWin_LabelFrame` | gate inverted | 3,000 |
| 18 | `BattleWin_BannerFrame` | record not re-read after the box | 108 |
| 19 | `BattleWin_BannerFrame` | narrow count 0x12 | 1,547 |
| 20 | `BattleWin_BannerFrame` | colour without the offset's upper bits | **not refused** |
| 21 | `BattleWin_BannerFrame` | wide text at x + 0x28 | 1,453 |
| 22 | `BattleWin_EnemyGaugeStates` | idle on out without +0xD | 766 |
| 23 | `BattleWin_EnemyGaugeStates` | low flag at a quarter inclusive | 600 |
| 24 | `BattleWin_EnemyGaugeStates` | record not re-read after the step | 88 |
| 25 | `BattleWin_EnemyGaugeStates` | HP and top pointers swapped | 3,000 |
| 26 | `BattleWin_EnemyGaugeOpen` | 56 for 55 | 1,748 |
| 27 | `BattleWin_EnemyGaugeOpen` | floor of 1 at HP 0 | 530 |
| 28 | `BattleWin_EnemyGaugeFrame` | side bit 0x80 | 818 |
| 29 | `BattleWin_EnemyGaugeFrame` | target not re-read | 15 |
| 30 | `BattleWin_EnemyTargetFrame` | side 3 takes the other offset | 385 |
| 31 | `BattleWin_EnemyTargetFrame` | y + 0xB | 3,000 |
| 32 | `BattleWin_EnemyTargetFrame` | not choosing stays | 1,437 |
| 33 | `BattleWin_MemberGaugeStates` | AP step handed +0xB | 3,000 |
| 34 | `BattleWin_MemberGaugeStates` | low flag from AP | 1,218 |
| 35 | `BattleWin_MemberGaugeOpen` | AP top 0 gives 1 | 1,509 |
| 36 | `BattleWin_MemberGaugeOpen` | +0xE not cleared | 2,983 |
| 37 | `BattleWin_MemberGaugeWait` | one try of `Battle_ReturnTrue` | 516 |
| 38 | `BattleWin_MemberGaugeWait` | side bit 0x40 | 668 |
| 39 | `BattleWin_MemberTargetFrame` | row and column swapped | 2,666 |
| 40 | `BattleWin_MemberTargetFrame` | not choosing draws | 575 |
| 41 | `BattleWin_MemberTargetFrame` | dy from the dx byte | 2,780 |

**Control 20** changes nothing: the pool holds eight records, so `12 k` is
at most 84 and its upper bits are 0; it would show only for a slot byte of
22 or more on a kind-3 window. **Control 29** first stood at 2 rounds; the
first advance's stand-in now moves the target byte half the time (15).

**What the fuzz did not reach:** a kind or state byte past its table and a
gauge top of 0 (ours aborts; both would crash the original - section 4);
slots outside the ranges above; the real callees (all stood in - each is
ours already and fuzzed in its own module).

## 4. Defects of Capcom's, and not-as-the-original

Latent defects, kept as the original has them where it can be kept
(numbered in [`known-defects.md`](known-defects.md): D59, D68, D69, D70):

- **Every stack table here is unbounded** (D59). A kind byte of 8 or more, or a
  state byte past its table, calls through the words above the table on the
  dispatcher's stack - its own return address first. The member gauge's
  table has no idle entry, so **state 3 of kind 5 calls `0x596FF2`**, the
  middle of `BattleWin_Run` after its call (`add esp, 0x20; ret` on the
  wrong frame). What reaches state 3 there: `BattleWin_MemberGaugeWait`
  steps twice when the target byte equals the slot *and* has bit 7, which
  a slot of 0..2 never does; the enemy's twin (`0x5974C0`, slot 3..8 with
  bit 6) likewise, and its table has an idle entry at 3 anyway. Ours
  aborts past each table (CLAUDE.md rule 4, as `Battle_PhaseDispatch` and
  `Field_RunTaskRecords` do).
- **A gauge top of 0 faults** (D68). `0x597400` and `0x5976D0` divide by the HP
  top with `idiv` unchecked (the AP top is checked). An enemy or member
  with a top HP of 0 opening a gauge window would raise a divide fault;
  ours aborts with a message instead. Neither is seen in play.
- **A growth of 1 wraps** (D69). `0x597160` lowers an unselected arm's growth by
  2 while it is not 0, so an odd growth goes 1 -> 0xFF (then 0xFD ..);
  growths are raised by 2 from 0 and lowered by 1 only in `0x5970C0`,
  which runs until arm 0's is 0 - an odd value in arms 1..6 after the grow
  is possible when they started unequal. Kept.
- **The member's target banner does not put its place back** (D70) where the
  enemy's does (`Window_RestoreAndBack`); state 1 of kind 5 draws nothing,
  so it does not show.

**Not as the original, in upper halves nobody reads:** where the original
pushes a coordinate or a byte whose upper bits are register garbage, ours
pushes the C++ value (the note at the top of `battle_win_states.cpp`). The
one upper half that is arithmetic, not garbage - the banner's colour - is
reproduced. Every callee reads the low word or byte, except that
`BattleWin_DrawPartyStatus` and the target banners hand x and y whole to
the 8 px font `0x516E70`, whose glyph draw stores them as words
(`Text_EmitGlyph`), so nothing reaches the screen. Said here so nobody
mistakes it for a fault; no ledger entry, and the coordinator may judge
otherwise.

## 5. The Chinese text (for the localisation work)

- **`BattleWin_BannerFrame` `0x597230` draws a banner-pool record's text**
  through `Text_DrawAt`: `0x5972BB` for a wide record (count 0x12, x = box x
  + 0x26 - 6 n) and `0x59730B` for a narrow one (count 8, x = box x +
  6 (6 - n)), y + 3, colour +0xA, the text pointer at `0x93B8E4 + 12 k`. It
  centres by `Text_GlyphCount` `0x597F40` at 6 px a glyph (half the 12 px
  cell); DIV-0057 already counts a text pair as two there. What the pool's
  texts are (by the kind, the action's name - an ability or item as it is
  used) was not established: the pool's writer was not read.
- Also drawn from here, by other groups' functions: the command label
  (`BattleWin_DrawCommandLabel`, `battle_windows.md` section 5), the party
  row's and the banners' names.

Not translated here.

## 6. `entries_logic.txt`

Added to `analysis/calltrace/entries_logic.txt` (main checkout) on
2026-09-25, under a group CL comment line, with the sizes of section 1.
Before, none of the nineteen was listed: all of `0x596A90..0x5979D3` was
one entry, `00596A90 F44`, which the tracer reads only while `0x596A90` is
unowned; its body is 0x508 (section 1) and it is no group's.

## 7. What the combat route reaches

The round's catalogue has all nineteen entered by the combat route
(`analysis/remaining_catalog.tsv`, the first-call trace of 2026-09-25); no
per-function counts were taken here. Within them the route cannot reach the
out-of-table paths and the divide fault of section 4, the banner colour's
upper bits, and whatever targets, statuses and banner kinds the recorded
fight did not use. Nothing here has been seen drawn by ours yet.

## 8. Other groups' functions and data, as learned (not bound)

| Address | Group | What |
|---|---|---|
| `0x597C70`, `0x597D50` | CM | kinds 6 and 7 of `BattleWin_Run`'s table - window kinds under record handler 3, like this group's six |
| `0x597A80`, `0x597BD0`, `0x597C10` | CM | `Window_DispatchKind`'s three handlers, called here with the eight gauge pointers `(len, top, shown top, shown value, value, +0xD, +0x18, kind)`: the gauge's animation |
| `0x596A90` | none | a menu panel draw, 0x508 bytes to its first `ret`; not the window task |
| `0x93B8E0` | (battle misc's `BattleBanner_Dispatch`) | the banner pool: +2 wide, +4 text, +0xA colour |
| `0x93B960` | - | the enemy object; `EnemyWorkingRecords` `0x93B9E0` is its +0x80; +8 side, +0x2E / +0x30 place, +0xF2 / +0xF3 banner offsets |
