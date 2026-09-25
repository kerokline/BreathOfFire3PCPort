# The field menu and the list draws

**Status:** IN PROGRESS (2026-09-25) - twenty functions ours, fuzzed
headless; the live check (the world-map route's A/B, then the shop's) is the
coordinator's, after the merge

Group DH of round eight ([`takeover-queue-round8.md`](takeover-queue-round8.md)),
wave B. Two runs of Capcom's code, both reached by the recorded routes:

- **The field menu**, `0x589970..0x58A0D0`: mode 3's per-frame call, its
  state dispatch, the set-up, the top bar with its steps, and the two helpers
  the top bar calls. The PSX's `START.EMI` compiled into the exe
  ([`menu-screens.md`](menu-screens.md) section 1).
- **Record handler 6** of `Field_RunTaskRecords`, `0x599B50..0x59A6A7`
  ([`window-task.md`](window-task.md) section 4): the top bar's five windows -
  the member panels, the money box, the play-time box, the icon row, the
  screen title - and six of the slide steps that the window records' state
  tables share.

Everything here is a faithful replacement: no `DIVERGENCE.md` entry is owed.
One divergence has patch sites inside these bodies and survives: DIV-0041
(section 2.3).

## 1. What is ours

Extents are read to the last instruction (`capstone`, 2026-09-25), not the
catalogue's; "Table" is where the entry is found when it is pointer-reached.

| PC | Bytes | Name | Table | What it does |
|---|--:|---|---|---|
| `0x589970` | 0x11 | `FieldMenu_Run` | recorded start, called by `0x5172F0` | `jmp [FieldMenu_States + 4 state]`, the byte `0x929F00` unchecked |
| `0x589990` | 0x1C3 | `FieldMenu_Open` | `FieldMenu_States[0]` | state 0: stats recomputed per member, the party saved, the "under a quarter" bit of eight records, nineteen globals zeroed, the windows placed; state + 1 (or straight to state 5 step 5 when `0x929F10` is 1) |
| `0x589B60` | 0xE | `FieldMenu_TopBar` | `FieldMenu_States[1]` | `jmp [FieldMenu_TopBarSteps + 4 step]` |
| `0x589B70` | 0x287 | `FieldMenu_TopBarInput` | `FieldMenu_TopBarSteps[0]` | the top bar: backdrop, the Camp test, left / right, cancel, confirm |
| `0x589E50` | 0xD | `FieldMenu_BackdropStep` | `FieldMenu_TopBarSteps[2]`, `0x667350` | the backdrop alone |
| `0x589E60` | 0x150 | `FieldMenu_PlaceWindows` | called by `FieldMenu_Open` | window records 4.. (one a member) and 7..10 placed under handler 6 |
| `0x589FE0` | 0xF1 | `FieldMenu_ReconcileParty` | called by `FieldMenu_TopBarInput` (twice) | `0x929F11` when a member left the party; the reserve slots' three bytes each follow their characters |
| `0x599B50` | 0x12 | `MenuList_Run` | handler 6 (`Field_RunTaskRecords`' stack table, `0x59E274`) | `jmp [MenuList_Kinds + 4 record[+2]]` |
| `0x599B70` | 0x20 | `MenuList_MemberPanel` | `MenuList_Kinds[0]` | its state, then the panel of the record |
| `0x599B90` | 0x91 | `MenuList_DrawMemberPanel` | recorded start (114 calls) | `0x573560` and the member's number cell through `0x5744B0` |
| `0x599D50` | 0x33 | `MenuList_MoneyBox` | `MenuList_Kinds[1]` | its state, then `Menu_DrawMoneyBox` with `0x904058` |
| `0x599DC0` | 0x2A | `MenuList_TimeBox` | `MenuList_Kinds[2]` | its state, then `0x5746C0` (the play-time box, hypothesis) |
| `0x599E50` | 0x117 | `MenuList_TopBarIcons` | `MenuList_Kinds[3]` | its state, then the seven icons, the cursor's growing over three frames |
| `0x599FA0` | 0x8F | `MenuList_TitleBox` | `MenuList_Kinds[4]` | its state, then the title box and the title text (section 3) |
| `0x59A3A0` | 0x28 | `MenuSlide_UpOff` | nine tables | y - 0x10 a frame to -20 |
| `0x59A3D0` | 0x27 | `MenuSlide_DownTo16` | five tables | y + 0x10 a frame to 0x10 |
| `0x59A580` | 0x28 | `MenuSlide_LeftOff` | four tables | x - 0x20 a frame to -200 (DIV-0041: -253) |
| `0x59A5B0` | 0x28 | `MenuSlide_RightTo17` | seven tables | x + 0x20 a frame to 0x11 |
| `0x59A5E0` | 0x28 | `MenuSlide_RightOff` | twenty tables | x + 0x20 a frame to 0x140 (DIV-0041: 373) |
| `0x59A680` | 0x28 | `MenuSlide_DownTo38` | four tables | y + 0x10 a frame to 0x26 |

The "Table" column's counts for the slides are a dword scan of the image
(every place the address appears); the tables are listed in each
`symbols.toml` evidence field.

### 1.1 What the reading found

- **`0x599E50`'s body is a tail jump away.** It calls its state and `jmp`s
  to `0x599E70`, inside its own extent. An E8/E9 scan finds no other way to
  `0x599E70`, so the two are one function here and in `entries_logic.txt`.
- **The top bar's step 1, `0x589E00`, is not in the queue** and stays
  Capcom's: the countdown from the confirm into the chosen screen (state =
  cursor + 2). `0x589FB0`, the Camp cell test, likewise. Both are in no
  group; both are reached every time the menu is used (`0x589FB0` whenever
  `0x904152` is set). They are the next two to take.
- **The top bar's five windows are records 4..10**, each with handler 6 and
  a kind: 0 member panel (records 4 + i), 1 money (7), 2 time (8), 3 icons
  (9), 4 title (10). The top bar talks to them through their fields: the
  cursor goes to record 9's `+0xA`, the title's text id to record 10's
  `+0x10`, "Camp refused" to record 9's `+0xC`, and a confirm sets the four
  side windows and every member panel to a slide state (1, and the panels 1
  for Items, 7 for Tactics, 2 for the rest).
- **Camp.** `0x8032B0 = 0` (allowed) when `0x904152` is set and the
  leader's cell passes `0x589FB0` (its `AreaMap_ByteAt & 0xF0` is none of
  `0xA0`, `0xA1`, `0xAF`, `0x91`); or when `Field_InputFlags` bit 0 is set
  or the area is `0xBD`, and either the gateway exit `0x531820` answers
  non-zero or `0x904152` (read again) is set and the cell passes. The two
  helpers run with `Sprite_Current` on ObjTrio. What `0x904152` means is not
  read; "camping allowed" is a guess from this use.
- **`FieldMenu_ReconcileParty`**, on cancel and Camp, compares the party the
  menu opened on (saved by `FieldMenu_Open` to `0x6BDF98`) with the party
  now. The three bytes per reserve slot at `0x9045FC` are read into a copy
  first and then follow their characters to their new slots.

## 2. The calls out, and what they read

Every call goes through `menu_lists::g` (`menu_lists_callees.h`). Eight of
the twenty dispatch through code-pointer tables in `.data`, which are read
afresh and unchecked (known-defects.md D59), as the originals read them; a state or kind past a
table's end reaches the next table's entries in both, since they are data
and ours reads the same dword.

### 2.1 Other groups' functions, called by address

| Address | Group | Called by | What we pass, and what it reads (disassembly 2026-09-25) |
|---|---|---|---|
| `0x573560` | DD | `MenuList_DrawMemberPanel` | (x, y, record, flag, 0): the record `and eax, 0xFF`; the flag's low byte through `neg` / `sbb`, the dword passed on to `0x57CF60`'s colour, which takes it into `GetClut(colour * 32 ...)` (low bits) |
| `0x5744B0` | DD | `MenuList_DrawMemberPanel` | (x, y, u / 8, v / 8, clut, shade): x and y `and 0xFFFF`, bytes 3, 4 and 6, word 5 |
| `0x5746C0` | DD | `MenuList_TimeBox` | (x, y), passed on as `lea` sums to the box and font draws, which take words |
| `0x531820` | none | `FieldMenu_TopBarInput` | no arguments; its answer's `al` |
| `0x589FB0` | none | `FieldMenu_TopBarInput` | no arguments; its answer's `al` |

### 2.2 Upper halves

The originals push coordinates as 16-bit registers and ids, counts and
colours as byte registers, whose upper bits are whatever the register held
(record addresses, a callee's leftovers). Every callee reads only the low
word of a coordinate and the low byte of an id, count, flag or colour: the
table above for the three DD functions; `Menu_DrawIcon` takes x and y as
words, w, h and shade as bytes (`char_stats.cpp`); `Text_DrawAt` stores
word x and y and `Text_DrawString`'s count is a byte; `Msg_SystemPtr` masks
to the low word; `Menu_DrawTitleBox`'s colour reaches only `GetClut`. So
ours passes the value as C++ computes it, and the stand-ins record exactly
the bits the callee reads - the convention of `menu_windows.cpp` and
`battle_win_states.cpp`.

### 2.3 DIV-0041's two patch sites

`widescreen.cpp`'s `kSlides` patches the bounds of two of these bodies:
`0x59A586` (`mov ecx, -200` in `0x59A580`) and `0x59A5E6` (`mov ecx, 0x140`
in `0x59A5E0`), widening them by the 53 columns under `BOF3X_WIDE=1`. Once
the functions are ours the patched bytes no longer run, so:

- `MenuSlide_LeftOff` and `MenuSlide_RightOff` read the bound from those
  operands on every call - whatever `PatchBytes` left there (-200 / 320, or
  -253 / 373), and the original's when `BOF3X_ORIGINAL=Widescreen` leaves
  them unpatched.
- `MenuLists_Inject` refuses to start unless the byte before each operand is
  `B9` and the operand holds the original bound or the widened one.
- The fuzz ran with `BOF3X_WIDE` off and on. `Widescreen_Inject` runs before
  `MenuLists_Inject`, so under `BOF3X_WIDE=1` the clones copy the patched
  bytes and the slides are seeded around the widened bounds. Controls S3 and
  S5 (section 4) are the literal bounds, refused only in the wide run.

No other `widescreen.cpp` site is in our extents: `0x599CC6`, `0x599DF6`,
`0x59A136` and `0x59A2B6` are inside `0x599CC0`, `0x599DF0`, `0x59A130` and
`0x59A2B0`, which are in no group and stay Capcom's (section 6).

No cheat, and none of DIV-0010, 0011, 0018, 0027 or 0030, patches an
address in our extents (`grep` of `src/game/*.cpp` for `0x5899..` to
`0x59A6..`, 2026-09-25). DIV-0030 is inside `Menu_DrawBackdrop`, which we
call and do not own.

## 3. Chinese text

**`MenuList_TitleBox` `0x599FA0` draws the top bar's screen title** - the
system text (`Msg_SystemPtr`) of `FieldMenu_TitleIds` `0x6672E4` (9, 0xB,
0xA, 0xC, 8, 0xD, 0xE, by the cursor), stored in record 10's `+0x10` by
`FieldMenu_TopBarInput`. It centres the text for **12-pixel glyphs**: the
box is `0x48` wide at the record's x, and the text starts at x + `0x25` - 6
n, n the `Text_CharCount` of the string. That is the case
[`dialogue-localisation.md`](dialogue-localisation.md) section 6 item 1a
predicts - "a caller that centres text by counting characters at 12 px
would now sit left of centre" - and a candidate for the owner's "the screen
title sits left of centre" ([`menu-screens.md`](menu-screens.md) section 3
item 3). Not translated or re-centred here; ours draws what the original
draws.

The skill list's and the item list's headers are not in this group.

## 4. The fuzz and its negative controls

`BOF3X_SHADOW=menu_lists` (`src/game/menu_lists_fuzz.cpp`), at start-up:

- Twenty byte-copies, every call out re-aimed at a recorder (`CloneCall`
  with `expected`); `0x599E50`'s internal tail `jmp` stays inside its copy.
- The two `.data` dispatch blocks swapped entry by entry for recorders -
  `0x6672B4..0x6672E3` (9 states, 3 top-bar steps) and `0x66AF94..0x66B043`
  (21 kinds and the five kinds' 23 states) - and put back afterwards.
- **Each round** starts from random bytes over 17 regions (4.0 KB): the icon
  and title ids, `0x66972C`, `Game_Mode` / `Game_Step`, the saved party,
  `Gfx_PacketNext` and `0x7E0674`, `Input_Pressed`, the 22 window records,
  the confirm and cancel words, Config's two bytes and the character records,
  `0x904040..0x904610` (money, the party lists, `0x904152`, the reserve's
  bytes), the area, `0x905B60..0x905BA8`, `0x905D90`, the menu's state block
  `0x929EC0..0x929F20`, `Sprite_Current`, and `0x939880..0x9398D0`. The
  current record and `Sprite_Current` are put back inside the records, the
  packet cursor inside a pool of ours, the member count below 5, the state
  and step inside their tables, each record's kind and state inside handler
  6's.
- **Boundaries seeded:** menu states 0..8 and steps 0..2; the member count
  0..4, `0x929F10` 1 or not, each record's `+0x18` at `+0x20 / 4` and one
  either side; the cursor 0..7, `0x7F`, `0x80`, `0x81`, `0xFF`, with 5 and 6
  weighted; `0x904152`, bit 0 of `Field_InputFlags` and area `0xBD` / `0xBC`
  on and off; a cancel hit, a confirm hit with no cancel bit, or neither;
  party bytes from {0..4, `0xFF`} so matches happen; kinds 0..20; the icon
  count at -2..4, `0x7F`, `0xF8`, `0x7FFF`, `-0x8000` with `+0xB` equal to the
  cursor or not; every slide's coordinate within three of the frame that
  crosses its bound.
- **The recorders are not quiet.** Each dispatch recorder and each draw after
  which a record is re-read repoints `0x905B84` half the time; the placement
  moves the state, the party check the step, the backdrop and the sounds the
  cursor, the gateway `0x904152`, the auto-repeat `Input_Pressed`, the panel
  and `Gpu_GetClut` the argument record's fields and the panel the packet
  cursor; a general disturber moves one of 24 watched bytes three calls in
  four. `Party_Count`'s recorder answers 0..8 with random bits above the low
  byte.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=menu_lists`, exit 0:

    menu_lists self-test: 20000 rounds over 20 functions (1000 each), 42485 calls to the stand-ins, 0 MISMATCHES
    coverage: menu states and steps 12 of 12, kinds 21 of 21, kind states 23 of 23; opened on 5 498, stepped 502;
      cancelled 404, camped 39, camp refused 31, entries chosen 242; recalcs 1970, gateway 382, cell 693,
      sounds 748, panels 1000, faces 1000, money 1000, time 1000, icons 7803 (grown 885), titles 1000,
      texts 1000; slides clamped 1942

The same with `BOF3X_WIDE=1`: exit 0, 0 mismatches. With `BOF3X_SHADOW='*'`:
exit 0, every module's self-test 0 mismatches, `inject: 1282 ours`.

**Negative controls.** `controls.py` (session scratchpad, `DH/`) planted
each alone - replace, build, self-test, revert - and all 56 were refused
(exit 3). The table gives the rounds that refused each, of 1,000.

| | Planted | Refused in |
|---|---|---|
| R1 | Run: the state table indexed by the step byte | 887 |
| O1 | Open: the member count read once | 43 |
| O2 | Open: the rest of the three filled with 0xFE | 862 |
| O3 | Open: the quarter test at or above | 864 |
| O4 | Open: 0x929F10 == 1 opens on step 4 | 498 |
| O5 | Open: the state read before the placement | 243 |
| O6 | Open: Party_Count asked once for the copy loop | 779 |
| O7 | Open: 0x9398CC not zeroed | 997 |
| T1 | TopBar: the step table indexed by the cursor | 665 |
| I1 | Input: Sprite_Current not set to ObjTrio | 877 |
| I2 | Input: area 0xBC | 191 |
| I3 | Input: 0x904152 not re-read after the gateway | 198 |
| I4 | Input: left wraps to 5 | 46 |
| I5 | Input: right wraps above 7 | 28 |
| I6 | Input: Input_Pressed not re-read after the moves | 482 |
| I7 | Input: cancel steps by 1 | 404 |
| I8 | Input: Tactics panels to state 6 | 24 |
| I9 | Input: Party_Count asked once for the panels | 181 |
| I10 | Input: the title index unsigned | 137 |
| I11 | Input: the cursor not re-read after the sound (left) | 105 |
| I12 | Input: Camp taken without 0x905B60 | 39 |
| I13 | Input: the refused Camp sound 0x106 | 31 |
| B1 | BackdropStep: the colour byte, not the background | 997 |
| P1 | PlaceWindows: panels 0x35 apart | 413 |
| P2 | PlaceWindows: the time box at x 0x11 | 1000 |
| P3 | PlaceWindows: Party_Count asked once | 775 |
| P4 | PlaceWindows: the reset after the panels | 1000 |
| Q1 | Reconcile: 0x929F11 only when two have left | 379 |
| Q2 | Reconcile: the bytes read as they are written | 177 |
| Q3 | Reconcile: a slot not found keeps its bytes | 822 |
| Q4 | Reconcile: the reserve searched in the party saved | 789 |
| L1 | ListRun: the kind from +3 | 947 |
| L2 | MemberPanel: the record of before the state | 485 |
| M1 | DrawMember: the row 2 wide | 859 |
| M2 | DrawMember: the number at y + 4 | 1000 |
| M3 | DrawMember: Gfx_PacketNext read before the panel | 512 |
| M4 | DrawMember: the slot read before the calls | 109 |
| N1 | MoneyBox: the record of before the state | 498 |
| N2 | TimeBox: x and y swapped | 1000 |
| C1 | Icons: the count capped at 4 | 18 |
| C2 | Icons: Camp shaded whatever +0xC | 72 |
| C3 | Icons: +0xB not updated | 978 |
| C4 | Icons: the record not re-read in the loop | 980 |
| C5 | Icons: the count compared unsigned | 80 |
| C6 | Icons: the grown icon not raised | 982 |
| E1 | Title: x from the first count | 944 |
| E2 | Title: y + 2 | 1000 |
| E3 | Title: the record not re-read after the box | 488 |
| E4 | Title: the colour byte after the box | 999 |
| S1 | UpOff: bound -21 | 454 |
| S2 | DownTo16: held at the bound too | DownTo16 70, DownTo38 69 |
| S3 | LeftOff: the literal -200 | 839 (under BOF3X_WIDE=1) |
| S4 | RightTo17: 0x10 a frame | 828 |
| S5 | RightOff: the literal 320 | 828 (under BOF3X_WIDE=1) |
| S6 | DownTo38: bound 0x27 | 459 |
| S7 | Slides: the state left alone at the bound | LeftOff 311, RightTo17 317, RightOff 339 |

Two controls of the first draft were **equivalent builds**, not misses, and
were replaced: letting `0xFF` in the party match (the saved byte has already
been tested against `0xFF`, so the second test never decides), and the grown
icon's size not truncated to a byte (`Menu_DrawIcon` reads the low byte
only). One was a miss: reading the member panel's slot before the calls
passed, because no recorder moved the *argument* record's fields; the
panel's two recorders now do, and M4 is refused.

## 5. What the fuzz did not reach

- The callees themselves: every one is a recorder. The live check is what
  shows the panels, icons and title drawn.
- A state or kind past its table, a member count of 5 or more, a
  `Party_Count` above 8 (a count of 128 or more never ends `FieldMenu_Open`'s
  loops, in both; 18 or more writes window records past the 22 in
  `FieldMenu_PlaceWindows`).
- The `0x5902D0` / `0x5902E0` / screen states themselves: recorders.
- Upper halves of the callees' arguments (section 2.2): recorded masked, by
  design.

## 6. Left original, and why

None of the twenty. In the same runs and not in the queue, so Capcom's:
`0x589E00` (the top bar's step 1) and `0x589FB0` (the Camp cell test),
section 1.1; and in handler 6's range, the unreached kinds 5..13
(`0x59A030`..`0x59A640`) and the other slide steps (`0x599C30`..`0x59A2B0`,
`0x59A0B0`, `0x59A0E0`, `0x59A130`, `0x59A160`, `0x59A610`, ...) that the
catalogue lists at `-114` - reached by no route.

## 7. `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (append only; `analysis/` is not
committed):

    # round 8 group DH (menu_lists.cpp), extents read 2026-09-25
    00589970 11
    00589990 1C3
    00589B60 E
    00589B70 287
    00589E50 D
    00589E60 150
    00589FE0 F1
    00599B50 12
    00599B70 20
    00599B90 91
    00599D50 33
    00599DC0 2A
    00599E50 117
    00599FA0 8F
    0059A3A0 28
    0059A3D0 27
    0059A580 28
    0059A5B0 28
    0059A5E0 28
    0059A680 28

The consolidated file's host lines `00589970 4ED` and `00599B90 B2D` are the
hosts these came out of; the consolidation keeps the smaller extent.

## 8. Other groups' addresses (said, not acted on)

- **`0x66972C` is named `MoveScript_EffectState`** in `symbols.toml`, but
  other modules and this one use it as the character-to-record byte table
  (`0x903A70 + 0xA4 * 0x66972C[character]`; `battle_draw_callees.h`,
  `battle_window_draw_callees.h` call it `kMemberRecord`). The name is
  probably wrong.
- **DD's `0x573560`, `0x5744B0` and `0x5746C0`**: what they read of each
  argument is in section 2.1, for DD's prototypes.
- **DI's tables**: `0x66B1F8` is the next handler's kind table (DI's
  `0x59B240`..), and its state tables `0x66B244..` hold four of our slides
  (`0x66B248`, `0x66B24C`, `0x66B254`, `0x66B258`); `MenuList_Kinds[14]` is
  DI's `0x59CB20`. Nothing of DI's is named here.
- **`0x59A6B0`** (`MenuList_Kinds[15]`) is a one-line `0x59A6C0(record)`, in
  no group.

## 9. Defects of the original

None that a player meets. Noted from the reading, numbered in
[`known-defects.md`](known-defects.md): D85, D64, D86.

- `FieldMenu_Open` copies one party pair per `Party_Count` without a bound:
  a count above 3 writes the saved reserve's bytes over the saved party's
  (the two lists are three bytes apart). The count is at most 3 in play
  (D85).
- `FieldMenu_TopBarInput` indexes the title ids by the cursor as a signed
  byte; only a cursor of `0x80` or more (a corrupted state block) reads
  before the table (D64).
- The title's centring for 12-pixel glyphs (section 3) is right for the
  Chinese it was written for and wrong for any narrower font (D86).
