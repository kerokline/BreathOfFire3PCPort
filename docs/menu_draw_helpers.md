# The shop and menu draw helpers (group DI)

**Status:** IN PROGRESS (2026-09-25) - twenty-five functions ours in
[`src/game/menu_draw_helpers.cpp`](../src/game/menu_draw_helpers.cpp), shadow
name `menu_draw_helpers`. Each read to its last instruction with capstone over
`bof3/BOF3.exe`; each fuzzed at start-up against a copy of Capcom's (50,000
rounds, 0 mismatches, with `BOF3X_WIDE` off and on; `BOF3X_SHADOW='*'` exits 0 with
1,287 injects); 39 negative controls, 39 refused by a count. No divergence of
its own; DIV-0041's widened bound inside `0x59B440` is read back and kept
(section 4). Not yet seen live: the coordinator's shop and combat A/B after
the merge is the check.

Group DI of the eighth round's wave B
([`takeover-queue-round8.md`](takeover-queue-round8.md)): record handlers 7
and 8 of `Field_RunTaskRecords` - two `jmp [table + kind * 4]` stubs - and
the window kinds and steps under them that the shop route (20) and the combat
route (6) reach. Every one is pointer-reached: from the imm32 at `0x59E27C` /
`0x59E284` of `Field_RunTaskRecords`' stack table, or from a `.data` table.
None has a PSX twin in `analysis/pairs_propagated.json`; none was read
against the PlayStation.

## 1. The functions

The window record is the dword at `0x905B84` (one of the 22 at `0x803160`,
0x24 bytes each): +2 the kind the handler dispatches on, +3 the kind's step,
+4 / +6 x / y (s16), +0xA..+0xC bytes, +0x10 a word. A "run" function calls
its step through a step table by +3 (unbounded, a byte), then draws; a
"slide" steps x or y and, past a bound (signed 16-bit compares), stores the
bound and sets the step +3 to 0. No function returns anything a caller reads;
all are `void (void)`, cdecl.

| PC | Name | Bytes | Reached by | What |
|---|---|--:|---|---|
| `0x59B220` | `Window_Handler7Kinds` | 0x12 | handler 7 (`0x59E27C`) | `jmp [0x66B1F8 + kind * 4]`, nineteen kinds |
| `0x59B240` | `ShopWin_TitleRun` | 0xC2 | kind 0 | step; the title box (0x118 x 0x13, the window colour `0x903A5A`); system message +0x10 at (+4 + 7, +6 + 3) when not 0, then message 0xF there when +0x10 is 0x36 / 0x49 / 0x4A / 0x52 |
| `0x59B350` | `ShopWin_MoneyRun` | 0x33 | kind 1 | step; `Menu_DrawMoneyBox(+4, +6, 0, zenny 0x904058)` |
| `0x59B390` | `ShopWin_MoneySlideUp` | 0x28 | kind 1 step 1 | y -= 0x10; above -0x14: -0x14, step 0 (section 5) |
| `0x59B310` | `ShopWin_ButtonsRun` | 0x34 | kind 2 | step; `Menu_DrawButtonRow(+4, +6, set +0xA, selected +0xB, 0)` |
| `0x59CB20` | `MenuWin_Hand` | 0x1A | kind 3; handler 8 kind 0; three more kind tables | `Menu_DrawHand(+4, +6, 0)` |
| `0x59B3C0` | `ShopWin_CursorBoxDraw` | 0x2E | kind 4 | `Menu_DrawCursorBox(+4, +6, width[+0xA], 0x34, blink +0xB, 6)`, no step |
| `0x59B3F0` | `ShopWin_MemberStatsRun` | 0x46 | kind 5 | step; `Shop_DrawMemberStats(+4, +6, record of party slot +0xA, +0xB, +0xC)` |
| `0x59B440` | `MenuWin_SlideOutLeft` | 0x28 | kind 5 / 6 step 1, `0x66B304` | x -= 0x20; below -150 (DIV-0041's bound): the bound, step 0 |
| `0x59B470` | `ShopWin_MemberSlideUp` | 0x28 | kind 5 step 3 | y -= 0x10; below 0x3E: 0x3E, step 0 |
| `0x59B4A0` | `ShopWin_MemberSlideDown` | 0x4C | kind 5 step 4 | y += 0x10; above +0xA * 0x36 + 0x3E: that, step 0 |
| `0x59B4F0` | `ShopWin_EquipRun` | 0x3E | kind 6 | step; `Menu_DrawEquipPanel(+4, +6, record of party slot +0xA)` |
| `0x59B530` | `ShopWin_EquipSlideIn` | 0x28 | kind 6 step 2 | x += 0x20; above 0xF: 0xF, step 0 |
| `0x59B560` | `ShopWin_BuyListRun` | 0x20 | kind 7 | step; `Shop_DrawBuyList(record)` |
| `0x59B7B0` | `ShopWin_BuyListSlideTo84` | 0x28 | kind 7 step 2 | x -= 0x20; below 0x84: 0x84, step 0 |
| `0x59B7E0` | `ShopWin_BuyListSlideTo46` | 0x28 | kind 7 step 3 | x -= 0x20; below 0x46: 0x46, step 0 |
| `0x59B810` | `ShopWin_BuyDetailRun` | 0xD | kind 8 | `Shop_DrawBuyDetail(record)`, no step |
| `0x59BB60` | `ShopWin_ItemListRun` | 0x20 | kind 9 | step; `Menu_DrawItemList(record)` |
| `0x59BB80` | `ShopWin_ItemListSlideIn` | 0x28 | kind 9 step 2 | x -= 0x20; below 0x4B: 0x4B, step 0 |
| `0x59BBB0` | `ShopWin_SellDetailRun` | 0xD | kind 10 | `Shop_DrawSellDetail(record)`, no step |
| `0x59CB00` | `Window_Handler8Kinds` | 0x12 | handler 8 (`0x59E284`) | `jmp [0x66B534 + kind * 4]`, six kinds |
| `0x59CB40` | `BattleMenuWin_ItemListRun` | 0x20 | handler 8 kind 1 | step; `BattleMenu_DrawItemList(record)` |
| `0x59CB60` | `BattleMenuWin_ItemListSlideRight` | 0x26 | its step 2 | x += 0x20; above 0x53: **0x52**, step 0 |
| `0x59CBC0` | `BattleMenuWin_SkillListRun` | 0x20 | handler 8 kind 2 | step; `BattleMenu_DrawSkillList(record)` |
| `0x59CBE0` | `BattleMenuWin_SkillListSlideIn` | 0x28 | its step 2 | x -= 0x20; below 0x53: 0x53, step 0 |

The extents are to the last instruction (1,125 bytes of code in all; the
queue's 1,290 counts to the next start, padding included). Every callee is ours already
(`menu_windows`, `char_stats`, `battle_draw`, `msgbox`, `msg_pool`); nothing
is called through another round-eight group.

**Read for each** (the `evidence` field in `symbols.toml` has the
instruction-level account):

- **The jumps.** `0x59B220` / `0x59CB00` load ecx with the record and eax
  with the kind, then jump. Every kind in both tables reloads both before
  use (their first instructions read 2026-09-25, the other groups' kinds
  included), so ours calls the table entry and returns - the same.
- **Re-reads.** Every run function re-reads `0x905B84` after the step and
  after each call; `ShopWin_TitleRun` re-reads the word +0x10 after the first
  `Text_DrawAt` before its four-way test, and reads the window colour after
  the step. Ours reads each where the original does (the record through a
  volatile load).
- **Upper halves.** The originals push coordinates made by `mov cx, [..]`
  and bytes made by `mov cl, [..]` into registers whose other bits are left
  over (in `ShopWin_CursorBoxDraw` the record pointer from the jump stub's
  ecx; elsewhere whatever the step left). Every callee reads only the low
  word of a coordinate and the low byte of a byte argument
  ([`menu-windows.md`](menu-windows.md) section 2; `Msg_SystemPtr` masks its
  id with 0xFFFF, read 2026-09-25), so ours passes them zero-extended.
  `ShopWin_TitleRun`'s id is the record pointer's high word over the word
  +0x10; `Msg_SystemPtr` drops it.
- **Unbounded indices**, as in the original, read from the same memory:
  the kind (+2), each step (+3), the cursor box's width (+0xA, words past
  the two of its table are the low halves of `ShopWin_MemberStatsSteps`'
  pointers), the party slot +0xA (into `0x904062`, then `0x66972C`).

## 2. The tables

Named in `symbols.toml` as `[[data]]` entries (read 2026-09-25):

| Address | Name | Type, count | Entries |
|---|---|---|---|
| `0x66B1F8` | `Window_Handler7KindTable` | u32 x 19 | `0x59B240` `0x59B350` `0x59B310` `0x59CB20` `0x59B3C0` `0x59B3F0` `0x59B4F0` `0x59B560` `0x59B810` `0x59BB60` `0x59BBB0`, then eight not in this group: `0x59BE50` `0x59BEA0` `0x59BEC0` `0x59BF00` `0x59C110` `0x59C190` `0x59C1E0` `0x59C260` |
| `0x66B244` | `ShopWin_TitleSteps` | u32 x 3 | ret `0x59A3A0` `0x59A3D0` |
| `0x66B250` | `ShopWin_ButtonsSteps` | u32 x 3 | ret `0x59A3A0` `0x59A680` |
| `0x66B25C` | `ShopWin_MoneySteps` | u32 x 3 | ret `0x59B390` `0x59A680` |
| `0x66B268` | `ShopWin_CursorBoxWidths` | u16 x 2 | 0x70, 0 |
| `0x66B26C` | `ShopWin_MemberStatsSteps` | u32 x 5 | ret `0x59B440` `0x59A5B0` `0x59B470` `0x59B4A0` |
| `0x66B280` | `ShopWin_EquipSteps` | u32 x 3 | ret `0x59B440` `0x59B530` |
| `0x66B28C` | `ShopWin_BuyListSteps` | u32 x 4 | ret `0x59A5E0` `0x59B7B0` `0x59B7E0` |
| `0x66B2BC` | `ShopWin_ItemListSteps` | u32 x 3 | ret `0x59A5E0` `0x59BB80` |
| `0x66B534` | `Window_Handler8KindTable` | u32 x 6 | `0x59CB20` `0x59CB40` `0x59CBC0`, then `0x59CC10` `0x59CC90` `0x59CCE0` (not in this group) |
| `0x66B54C` | `BattleMenuWin_ItemListSteps` | u32 x 5 | ret `0x59A580` `0x59CB60` `0x59CB90` `0x59A5E0` |
| `0x66B560` | `BattleMenuWin_SkillListSteps` | u32 x 3 | ret `0x59A5E0` `0x59CBE0` |

"ret" is `0x437CC0`, a bare `ret`. The queue gave `0x66B1F8..0x66B288` and
`0x66B538..0x66B568`; `0x66B28C` and `0x66B2BC` are named too because they
are this group's functions' own step tables (between them sits
`menu_windows`' icon table `0x66B29C`), and the handler 8 table starts at
`0x66B534` (its kind 0), one below the queue's range.

## 3. The fuzz (`BOF3X_SHADOW=menu_draw_helpers`)

`src/game/menu_draw_helpers_fuzz.cpp`: twenty-five byte-copies, every
`call rel32` re-aimed at a recording stand-in (thirteen callees). The eleven
`.data` dispatch tables (both kind tables and the nine step tables, 53
entries, each checked against what it held on 2026-09-25) have their entries
swapped for numbered recorders during the run - both sides read the same
`.data` - and put back after. `MenuWin_SlideOutLeft`'s bound is the imm32 in
its copy (+6), which ours is aimed at through `g.slide_left_bound` and which
each round seeds (-150, -203, 0, -1, 0x7FFF, -0x8000 or random, with random
upper bits).

2,000 rounds a function. Each round: random bytes over the 22 window
records, `0x905B84`, the window colour, the zenny and the party list with
every byte +0xA can reach (`0x904058`, 0x10A bytes), and the member map
`0x66972C` (0x100 bytes); the current record put on one of the 22; the kind
and step put inside their tables; each slide's position seeded at and around
the arrival (`bound - by` and +/-1, 2, 0x10, 0x20, 0x40), the money box's y
at -4 / -3 / -5 / -20 / -19 / -21; the title's +0x10 at 0, the four ids and
their neighbours, 0xF, 0x136, 0x8036. Theirs, then ours from the same state;
every region, the bound and the stand-ins' log compared. The stand-ins
record what their callee reads and, from a hash both sides share, change
something read again after the call - a record field (+3, +4..+7, +0xA..+0xC,
+0x10, +0x11), the colour, the zenny, a party-list or member-map byte, or
(1 in 23) `0x905B84` itself.

    shadow      menu_draw_helpers self-test: 50000 rounds over 25 functions (2000 each), 52200 calls to the stand-ins, 0 MISMATCHES; the 22 window records, the current record, the window colour, the zenny and party list, the member map, the slide bound and the stand-ins' log compared
    shadow      menu_draw_helpers coverage: title with no text 154, one 1592, two 254; slides stopped 9047, moving 10953; current record repointed 1470

The same with `BOF3X_WIDE=1` (the log's bound line reads -203).

### Negative controls

39 planted bugs, one at a time, each rebuilt and re-run headless (`BOF3X_SELFTEST_ONLY=1`, a scratch
driver that edits `menu_draw_helpers.cpp`, builds, runs and restores). **39 of 39 were refused by a count
of mismatches** (exit 3), each in the function it was planted in and nowhere else. The first run's
control 2 read the kind from +3, which ran off the table into a random dword and crashed the process
(0xC0000005) rather than being counted; it was replaced by "bit 4 dropped", and control 5's pattern
was made unique; the table is the run with those two re-run.

| # | the bug | refused in (of 2,000) |
|--:|---|--:|
| 1 | Window_Handler7Kinds: the kind one on | 2,000 |
| 2 | Window_Handler7Kinds: the kind's bit 4 dropped | 323 |
| 3 | TitleRun: no step | 2,000 |
| 4 | TitleRun: the colour read before the step | 156 |
| 5 | TitleRun: the box 0x117 wide | 2,000 |
| 6 | TitleRun: the text at x + 6 | 1,846 |
| 7 | TitleRun: the record not re-read after Msg_SystemPtr | 99 |
| 8 | TitleRun: +0x10 not re-read after the first text | 42 |
| 9 | TitleRun: 0x4A not among the four | 71 |
| 10 | TitleRun: the second message 0x10 | 254 |
| 11 | ButtonsRun: set and selection swapped | 1,994 |
| 12 | ButtonsRun: the record read before the step | 53 |
| 13 | MoneyRun: the zenny read before the step | 169 |
| 14 | MoneySlideUp: the twin's jge (a real slide) | 1,877 |
| 15 | MoneySlideUp: the bound -0x13 | 136 |
| 16 | CursorBoxDraw: the width by +0xB | 1,938 |
| 17 | CursorBoxDraw: flags 7 | 2,000 |
| 18 | MemberStatsRun: the party list's byte, not its record | 1,994 |
| 19 | MemberStatsRun: slot and item swapped | 1,992 |
| 20 | SlideOutLeft: the constant -150, the patched imm ignored | 1,045 |
| 21 | SlideOutLeft: the step not cleared | 338 |
| 22 | MemberSlideUp: the bound 0x3F | 156 |
| 23 | MemberSlideDown: 0x35 a slot | 1,133 |
| 24 | MemberSlideDown: the y compared unsigned | 327 |
| 25 | EquipRun: the member by +0xB | 1,976 |
| 26 | EquipSlideIn: stops at 0xF itself (>=) | 125 |
| 27 | BuyListRun: the record read before the step | 53 |
| 28 | BuyListSlideTo84: arrives at 0x85 | 887 |
| 29 | BuyListSlideTo46: 0x10 a frame | 1,517 |
| 30 | BuyDetailRun: draws the buy list | 2,000 |
| 31 | ItemListRun: the buy list's steps | 2,000 |
| 32 | ItemListSlideIn: the bound 0x4C | 141 |
| 33 | SellDetailRun: draws the item list | 2,000 |
| 34 | Window_Handler8Kinds: handler 7's table | 2,000 |
| 35 | MenuWin_Hand: x and y swapped | 2,000 |
| 36 | BattleMenuWin_ItemListRun: the skill list's steps | 2,000 |
| 37 | BattleMenuWin_ItemListSlideRight: stops at 0x53 (the store 'fixed') | 1,010 |
| 38 | BattleMenuWin_SkillListRun: no step | 2,000 |
| 39 | BattleMenuWin_SkillListSlideIn: 0x21 a frame | 1,129 |

## 4. DIV-0041 inside `MenuWin_SlideOutLeft`

`widescreen.cpp`'s `kSlides` patches the imm32 at `0x59B446` - the -150 of
`0x59B440`'s `mov ecx, imm32` - to -203 under `BOF3X_WIDE=1`, so the member
and equipment panels clear the wide view's left edge. Once `0x59B440` is
ours the original body no longer runs, so ours reads the bound's low word
from `0x59B446` at every call, exactly as the original's `cmp word [eax + 4],
cx` uses it. The patch still goes in (the five-byte detour ends at
`0x59B444`), `BOF3X_ORIGINAL=Widescreen` still gives -150, and
`BOF3X_ORIGINAL=MenuWin_SlideOutLeft` runs Capcom's body with the patch in it.
`MenuDrawHelpers_Inject` refuses to start unless `0x59B445` is still the
`mov ecx` opcode (0xB9) and logs the bound it reads (`menu_draw_helpers:
MenuWin_SlideOutLeft's bound -150`, or -203 wide). Fuzzed both ways (section
3); negative control "the constant -150" is refused. The other DIV-0041
slide bounds in this address range (`0x59A586`, `0x59A5E6`) are inside group
DH's `0x59A580` and `0x59A5E0`, not here. No cheat and no other divergence
patches a byte of these twenty-five (`grep` of `src/` for their addresses
and the tables', 2026-09-25).

## 5. What the original does that looks wrong (not numbered)

- **`ShopWin_MoneySlideUp` `0x59B390` does not slide.** It is
  `0x59A3A0` (group DH's slide-up to -0x14) byte for byte but for the
  branch: `jle` where `0x59A3A0` has `jge`. So when the money box is sent up,
  from any y above -4 it jumps to -0x14 in one frame and stops; from -4 or
  less it moves up 0x10 a frame and the step never ends (the word wraps round
  to positive, where it snaps). Which case the shop shows, and whether the player can see the
  difference, is not checked (unseen; the owner's eye after the merge). Kept (faithful); a candidate for
  `known-defects.md`, for the coordinator to number. Negative control 14
  plants the fix and is refused.
- **`BattleMenuWin_ItemListSlideRight` `0x59CB60` tests 0x53 and stores
  0x52**, so an x landing exactly on 0x53 sits there a frame and goes on to
  0x73, then 0x52; the window rests at 0x52. `0x59CB90` (step 3, in no group) has the same 0x53 /
  0x52 pair. Harmless; kept.

## 6. What the fuzz did not reach, what stays original, and other groups' addresses

- **Live.** Nothing here has been seen drawn by ours. The shop route reaches
  handler 7's kinds 0..10 and the combat route handler 8's (the queue's
  "Routes" column); the coordinator's A/B after the merge is the check.
  What to look at: the shop's title caption, money box, verb row, member and
  equipment panels sliding in and out, the buy / sell lists and details; the
  battle item and skill lists sliding in.
- **Not reached by the fuzz:** kinds and steps past their tables (ours reads
  the same memory the original would, but a random dword there is not a
  function, so no round seeds it); the real callees (stand-ins only); the
  order of reads with no call between them (the originals re-read `0x905B84`
  between two stores with nothing in between, which no stand-in can
  disturb).
- **Left original:** none of the twenty-five. `0x59CB90` (a step of
  `BattleMenuWin_ItemListSteps`) is pointer-reached but in no group - no
  route reached it - and stays Capcom's.
- **Chinese text.** `ShopWin_TitleRun` `0x59B240` draws system-pool
  messages (`Msg_SystemPtr` of the record's word +0x10, and message 0xF after
  it for ids 0x36 / 0x49 / 0x4A / 0x52) through `Text_DrawAt` at the call
  sites `0x59B2AA` and `0x59B2F9` - the shop window's caption. It draws
  whatever language the system pool holds; for
  [`dialogue-localisation.md`](dialogue-localisation.md) section 6 the
  address is `0x59B240`. Not translated here.
- **Other groups' addresses, said and not acted on:**
  - `0x66972C` is named `MoveScript_EffectState` (count 24, status
    hypothesis) in `symbols.toml`, but every reader this group met - and
    `item_use.cpp`, `battle_draw_callees.h` - uses it as the map from a party
    member id (`0x904062[slot]`) to its character record. The name looks
    misfiled; its owner should look.
  - `0x59A3A0` (DH's) is the correct twin of `0x59B390` (section 5).
  - DH's `0x59A580` and `0x59A5E0` hold DIV-0041 slide bounds (`0x59A586`,
    `0x59A5E6`); they need the same read-back as section 4 when DH takes them.
  - `0x59CB20` (ours) is also kind 0 of handler 1 (`0x596530`, table
    `0x66AE40`), kind 1 of handler 2 (`0x5968E0`, `0x66AEC8`) and kind 14 of
    handler 6 (`0x599B50`, DH's, `0x66AF94`); all jump to it.
  - `0x59B440` (ours) is also step 1 of `0x59BEC0`'s table `0x66B300`
    (`0x66B304`), in no group.

## 7. `entries_logic.txt`

Appended to the main checkout's `analysis/calltrace/entries_logic.txt`
(2026-09-25), the extents read:

    # round 8 group DI (menu_draw_helpers.cpp, 2026-09-25): extents to the last instruction (capstone)
    0059B220 12
    0059B240 C2
    0059B310 34
    0059B350 33
    0059B390 28
    0059B3C0 2E
    0059B3F0 46
    0059B440 28
    0059B470 28
    0059B4A0 4C
    0059B4F0 3E
    0059B530 28
    0059B560 20
    0059B7B0 28
    0059B7E0 28
    0059B810 D
    0059BB60 20
    0059BB80 28
    0059BBB0 D
    0059CB00 12
    0059CB20 1A
    0059CB40 20
    0059CB60 26
    0059CBC0 20
    0059CBE0 28

The hosts they were folded into (`0x59AE00` 0x780, `0x59CA00` 0x2FC in the
file) end at the first of these once the file is consolidated.
