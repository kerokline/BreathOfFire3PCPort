# Group E1G: the item-trade screen's rest, and the item icon

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 10), wave
one, on the round branch's tip `1bb41df`. **14 functions ours**
(`src/game/effect_1g.cpp`, shadow name `effect_1g`): the cut's fourteen rows
for E1G (`analysis/round13_cut.tsv`), each read to its last instruction with
capstone and fuzzed through the scenario harness's field mode
([`scenario_harness.md`](scenario_harness.md) sections 7 and 8, not edited):
84,000 rounds, 0 mismatches. 45 controls planted one at a time: **45 refused**
by a count (section 9). Fuzz-only: no recorded route enters any of them
(section 10).

## 0. What these are - not effect code

The cut filed the band `0x594060..0x594D8A` under the effect engine (unit
`T66A470`, "Sprite_ClutWord", and one row "Fn_466080 kind 0xF"). Read, **none
of the fourteen is an effect kind's state or reads an effect record**: no
function here reads `Sprite_Current`, `Effect_Objects` or `Effect_KindHandlers`.
They are the item-trade screen that round twelve's FE2 named
([`field_e2.md`](field_e2.md) section 1: "trade" names a list of entries whose
confirm adds an item after a test of three ingredient counts) - FE2 took the
screen's first seven states and called these fourteen by address:

- `0x66A470`, the run the cut's unit names, is **`ItemTrade_States`** (FE2's
  `[[data]]`), dispatched by `0x593950` by the byte `0x93985C`; its entry 2 is
  `0x5940F0` here. `0x66A494`, the run after `ItemTrade_RunSteps`, is this
  group's `ItemTrade_LeaveSteps`.
- `0x594D50` ("kind 0xF") is an item icon drawn beside a name; E1B's
  `0x469210` calls it eight times, which is how the labelling reached kind 0xF.

The rows are `part5` rows of the round's first scope (the area overlays'
remainder), not `hypothesis` rows of the labelling pass, and the plan's own
row for them already said "a run of code pointers at `0x66A470` and callees
FE2's `0x593960..` calls" (section 3's EKP). **Taken**, as the cut assigns
them; the coordinator may judge them field code filed here. Names follow FE2's
`ItemTrade_*`; `Item_DrawIcon` is not the screen's alone.

## 1. What each function does

Every function's comment in `effect_1g.cpp` is the full read and each
`symbols.toml` `evidence` string cites its extent. The screen's cells
(`effect_1g_callees.h`, the same as `field_e2_callees.h`'s): the state
`0x93985C`, the step `0x93985E`, the pick `0x6BE08C` (low nibble the entry,
bit 7 and bit 6 marks), the row count `0x6BE08D`, the s8 quantity `0x6BE08E`,
the yes / no hand `0x6BE08F`, the row byte `0x905B88` choosing ten bytes of
`0x66AD10` (record numbers, `0xFF` ends a row), the records `0x66AB58` (8
bytes: item, category, three ingredient bytes, their three counts; an
ingredient is consumable `byte + 0x38`).

| Function | Entry | Bytes | PSX twin | Reached by | What |
|---|---|--:|---|---|---|
| `ItemTrade_FullMessage` | `0x594060` | 0x8C | `0x800F5AE0` | `ItemTrade_RunSteps[3]` | the step `ItemTrade_PickItem` sets at 99 held: the frame; a button (confirm or cancel) - quantity 1, sound 0x106, step 0, pick bit 7 cleared, the list; else the notice, the list, `ItemTrade_DrawCount` |
| `ItemTrade_Leave` | `0x5940F0` | 0xE | `0x800F5BAC` | `ItemTrade_States[2]` | `jmp [ItemTrade_LeaveSteps + 4 * step]`, unbounded |
| `ItemTrade_LeaveAsk` | `0x594100` | 0x132 | `0x800F5BE8` | `ItemTrade_LeaveSteps[0]` | the yes / no to leave: left / right flip the hand; cancel - hand 1, state down; confirm on yes - `Transition_Start(0)`, step up; on no - sound 0x104, state down; then the prompt |
| `ItemTrade_LeaveWait` | `0x594240` | 0x74 | `0x800F5DA8` | `ItemTrade_LeaveSteps[1]` | the wait word 0: `Game_Step = 1`; else the prompt |
| `ItemTrade_DrawBackground` | `0x5942C0` | 0x141 | `0x800F5E5C` | FE2's two states, the two leave steps (tail `jmp`) | two POLY_FT4 across 320 x 240 from page 0x95, CLUT 0x7A80, under a texture window that steps every 16 frames through four 32-pixel cells; the window reset after |
| `ItemTrade_DrawList` | `0x594410` | 0x1F1 | `0x800F6084` | FE2's four states, the three states here | the frame, the heading, every entry's name and icon; the pick raised 2 over a shadow, an entry short of an ingredient in colour 7 and dim |
| `ItemTrade_DrawListFrame` | `0x594610` | 0xE1 | `0x800F63E4` | `ItemTrade_DrawList` | `Menu_DrawBox` 0x8C x 0xA0 and E1F's / E1B's border pieces |
| `ItemTrade_Lacks` | `0x594700` | 0x90 | `0x800F65C0` | FE2's pick and count, `ItemTrade_DrawList` | al 1 when a consumable held is below an ingredient's count times the quantity |
| `ItemTrade_RowCount` | `0x594790` | 0x36 | `0x800F66EC` | `ItemTrade_OpenStart` (FE2's) | the row's bytes before its first 0xFF, ten at most |
| `ItemTrade_DrawNeeds` | `0x5947D0` | 0x21C | `0x800F6748` | FE2's two states, the two leave steps | two windows: each ingredient's name and count needed times the quantity; its name and the count held, colour 7 when held is below one's need |
| `ItemTrade_DrawNeedsFrame` | `0x5949F0` | 0xD2 | `0x800F6A9C` | `ItemTrade_DrawNeeds` (twice) | `Menu_DrawBox` 0x84 x 0x42 and the border pieces |
| `ItemTrade_DrawCount` | `0x594AD0` | 0x1B7 | `0x800F6C74` | FE2's count and confirm (one a tail `jmp`), `ItemTrade_FullMessage` (tail `jmp`) | the entry's name, the quantity, its icon, and `Inventory_Count(category, item, 0)` / `(.., 1)` under two labels |
| `ItemTrade_DrawCountFrame` | `0x594C90` | 0xBE | `0x800F6EF8` | `ItemTrade_DrawCount` | `Menu_DrawBox` 0x88 x 0x36 and the border pieces |
| `Item_DrawIcon` | `0x594D50` | 0x3A | `0x800F70AC` | the list (two sites), the count window, E1B's `0x469210` (eight) | `(x, y, item, category, dim)`: nothing for item 0; else `Item_IconKind`'s nibble through `Item_IconByKind` to `Menu_DrawIcon8` |

The twins are the cut's; the sibling names none of them (`names/*.toml`,
`symbols.toml`: no entry at any of the fourteen), so every name is from what
the PC code does. What the player sees on this screen, and where the game
opens it, is not established here.

**Tables named** (`[[data]]`): `ItemTrade_LeaveSteps` `0x66A494`, count 2
(`ItemTrade_Leave`'s, by the step; `Item_IconByKind`'s bytes follow - not
code); `Item_IconByKind` `0x66A49C`, 16 bytes (`Item_IconKind` answers
`and eax, 0xF`; other data follows at `0x66A4AC`).

## 2. Divergence

None. Where an original indexes past its table ours aborts with a message
(the round-nine rule, no ledger entry): `ItemTrade_Leave` past the two steps,
`Item_DrawIcon` past sixteen icon kinds (unreachable: the real
`Item_IconKind` answers a nibble). `tools/ledger_check.py`: 63 entries, 0
errors.

*Since 2026-10-03:* one, under a wide picture only - the backdrop's quads
reach the band edges (DIV-0041's amendment of that date; section 12).

## 3. The arguments pushed with leftovers - the re-listed stand-ins

| Callee | Mask | The read | Pushed by |
|---|---|---|---|
| `Text_DrawAt` | u16, u16, u8, u8, whole (string hashed) | `msgbox.cpp`: x, y shorts, colour and count bytes | the list (colour `al * 7` in `ax` over `Text_CharCount`'s eax), every name draw (the count in eax) |
| `Item_NamePtr` | u8, u8 | its evidence: the category's and the item's low bytes | the list, the needs (bytes loaded into registers holding a record offset) |
| `Item_IconKind` | u8, u8, and a nibble answered | `0x591724` / `0x591736 and eax, 0xFF`; `0x591748 and eax, 0xF` | `Item_DrawIcon` (whole words, and `dim` as an unread third) |
| `Menu_DrawIcon8` | u16, u16, u8, u8 | `0x57D3A0` / `0x57D3BB and 0xFFFF`, `0x57D3D1 and eax, 0xFF`, `0x57D390 test al, al` | `Item_DrawIcon` (`dim` from the list's stack dword whose low byte it wrote) |
| `Inventory_Count` | u8 x 3 | `char_stats.cpp` | `ItemTrade_Lacks`, the needs, the count window |
| `Item_DrawIcon` (ours) | whole, whole, u8, u8, u8 | `0x594D54 test al, al`; `Item_IconKind`'s and `Menu_DrawIcon8`'s reads | the list's `dim` is `[esp + 0x10]`, a stack dword with the answer in its low byte |
| `ItemTrade_DrawList` (ours) | u8 | `0x594431 mov al, bl`, `0x59448D mov al, [esp + 0x28]`, `0x594610 mov cl` | FE2's states and these |
| `ItemTrade_Lacks` (ours) | u8, u8 | `0x594711 and ebp, 0xFF`; `0x594767 and ecx, 0xFF` | - |

`Menu_DrawBox`'s flag and style bytes (`flag | 0x80` in `cl`, the style in `al`
over stale registers) are masked by the field-standard row already.

## 4. The fuzz (`effect_1g_fuzz.cpp`)

**Mode and shapes:** field mode (`g.field`), not effect mode - nothing here
runs on an effect record. `kState` for the four states, `kCall` for the ten
helpers; `ret_mask` 0xFF on `ItemTrade_Lacks` and `ItemTrade_RowCount` (both
answer in al). No clone past 64 call sites; no `REFUSED` line.

**Stand-ins beyond the standard sets** (17): the group's own nine called by
another of its own (by name in ours, by address in the clones), the four
state-side ones (`_DrawList`, `_DrawNeeds`, `_DrawCount`, `_DrawBackground`)
logging the trade bytes (pick, step, state, quantity, answer) when they run;
the re-listed callees of section 3; louder `Input_AutoRepeat` (the pad's
moves, FE2's), `Inventory_Count` (a count 0..7 half the time, else 0..99 - the
records' needs are small), `Item_IconKind` (a nibble), `Sound_PlayEffect` and
`Transition_Start` logging the trade bytes (the order of a state's writes
against its sound). **Table swapped for recorders:** `ItemTrade_LeaveSteps`
(2).

**Regions** beyond field mode's standard: the trade bytes `0x6BE080` (0x20),
the states `0x939850` (0x20), the button words `0x903580` (0x14), `Game_Mode` /
`Game_Step`, `MessagePools` `0x803580` (0xE8: the prompts' words
`0x80360A..0x80361B`, so a wrong prompt shows as a wrong pointer). 41 regions,
22,884 bytes. The row byte, the style byte, the text scratch, the wait word,
`Input_Pressed` and the packet buffer are field-standard.

**Seeds** (every function): the row byte 1, 2 or 8 (rows with entries) or any
below 10; the row count 0..10; the pick inside it, bit 7 / bit 6 a third of the
time; the quantity at 0, 1, 2, 0x62, 0x63, 0x80, 0xFF or below 100; the answer
0 / 1; the step below the leave table's two; `Input_Pressed` random with the
confirm and cancel words each a bit of it half the time; the wait word 0 half
the time. **Arguments:** the list's flag 0, 1 or a byte; `ItemTrade_Lacks`'
entry below 10 two times in three and its quantity at the boundaries; the
icon's item byte 0 a third of the time. **Disturbance** (from the hash only):
the pick, the quantity, the answer, the step, the state, the row byte, the row
count.

**Measured** (2026-09-29, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_1g`, exit
0, in this worktree): 84,000 rounds over 14 functions (6,000 each), 1,195,014
calls to the stand-ins, **0 mismatches**; 250 stand-ins registered (174 of the
field-standard set). Every callee reached, both `ItemTrade_LeaveSteps`
entries (2,975 / 3,025 calls), `Transition_Start` 274, `Item_IconKind` 3,984.
`BOF3X_E1G_ONLY=first,count` and `BOF3X_E1G_ROUNDS` select a subset (the
controls' speed-up).

`BOF3X_SHADOW='*'`: see section 11.

## 5. What the cut and the tool said, settled

- **Extents**: the tool's, read from the code, are right; three differ from
  the cut by padding only (`0x594060` 0x8C of 144, `0x5940F0` 0xE of 16,
  `0x594100` 0x132 of 320). No start dropped, merged or added; no jump table;
  no shared tail.
- **The four hidden starts** (`0x594060`, `0x5940F0`, `0x594100`, `0x594240`)
  lie in `Sprite_ClutWord` `0x593860`'s catalog extent, not in its code: ours
  (`sprite_draw.cpp`) ends at the original's jump table `0x593938`, and
  `entries_logic.txt` already gives the host 0x100 (FE2's fix). Each is
  reached by address (a `.data` cell) and is taken as its own function.
- **`0x594D90`** (FE2's `kTradeTake`, the ingredients taken) is not in the cut
  and lies past this band (`0x594D8A`): nobody's, left original and called
  raw. **`0x593950`**, `ItemTrade_States`' dispatcher (reached from E1F's thunk
  `0x52CF30`), is in no cut row either (taken 2026-10-06 as
  `ItemTrade_Dispatch`, [`game-last.md`](game-last.md)).

## 6. Latent defects (Capcom's, described, not fixed)

- **L1 `ItemTrade_LeaveAsk` with confirm and cancel in one frame** (D238) runs both
  tests: cancel sets the hand to 1 and the state down, then confirm on that
  hand ("no") takes the state down again - state 2 to 0, `ItemTrade_Open`,
  whose step (still the leave step, 0) is `ItemTrade_OpenStart`: the screen
  opens again. Whether both buttons can be pressed in one frame in play was
  not traced; ours keeps it.
- **L2 `ItemTrade_Leave`'s index is unbounded** (D200) - a step of 2 or more jumps
  through `Item_IconByKind`'s bytes (ours aborts). Unreachable by the code
  read: the state is entered from `ItemTrade_PickItem`'s cancel at step 0, and
  only `ItemTrade_LeaveAsk` moves the step (to 1).
- **L3 an entry past its row** (D200) - the record numbers are read from
  `0x66AD10 + 10 * row + k` and the record from `0x66AB58 + 8 * number`
  unchecked: a pick past the row count, or an entry byte of `0xFF`, reads a
  record past the table's 55 (within `.data`, no fault). Kept unchecked as
  FE2's `TradeRecord` is; the pick is held below the row count by
  `ItemTrade_PickItem`.
- **Not a defect, noted:** `ItemTrade_DrawNeeds` colours the count held
  against **one** item's need, while it prints the need times the quantity and
  `ItemTrade_Lacks` tests that product; `ItemTrade_PickCount` lowers the
  quantity until nothing lacks, so the two agree on the screen as the states
  drive it.

## 7. Calls across groups

**Out, raw (`SH_AT`, `effect_1g_callees.h`)**: E1B's `0x469750` (the trade
frame, three sites) and `0x468950` (the border sides, six); E1F's `0x52CF60`
(three) and `0x52CFE0` (twenty sites) - `band_rows.py --edges`: E1G to E1B 9,
to E1F 23. Everything else is ours or Capcom's by name (`Crt_sprintf`
`0x5B9380`).

**In, from outside the group**: FE2's seven trade states (ours) through the
six `field_e2_callees.h` constants (section 8); E1B's `0x469210` calls
`Item_DrawIcon` at `0x46925F`, `0x46929A`, `0x4692E2`, `0x46931A`,
`0x46935D`, `0x469390`, `0x4693CA`, `0x4693FF` (for E1B's merge: by name);
`0x593950` (nobody's then; `ItemTrade_Dispatch`, ours since 2026-10-06, [`game-last.md`](game-last.md)) jumps to `ItemTrade_Leave` through `ItemTrade_States`.
`scenario_harness.cpp`'s `kField` rows `"0x594410"`, `"0x5947D0"`,
`"0x5942C0"`, `"0x594700"`, `"0x594AD0"`, `"0x594790"` (FE2's, keyed by
address) still serve FE2's raw calls; they are the harness's, not edited.

## 8. The rebinding

`grep -rn -i "0x5940\|0x5941\|0x5942\|0x5944\|0x5946\|0x5947\|0x5949\|0x594a\|0x594c\|0x594d" src/game`:

- **Rebound** (the round-ten form, the value unchanged):
  `field_e2_callees.h` `kTradeList` = `bof3::addr::ItemTrade_DrawList`,
  `kTradeCursor` = `ItemTrade_DrawNeeds`, `kTradeFrame` =
  `ItemTrade_DrawBackground`, `kTradeRows` = `ItemTrade_RowCount`,
  `kTradeLacks` = `ItemTrade_Lacks`, `kTradeCount` = `ItemTrade_DrawCount`.
  FE2's shadow after it: 306,000 rounds, 0 mismatches.
- **Left raw on purpose**: `field_e2_fuzz.cpp`'s `CallSite` targets and its
  `FE2_AT(594700)` stand-in (the fuzz's keys); `scenario_harness.cpp`'s six
  `kField` rows (section 7; a harness); `field_e2_callees.h`'s comment "the
  fourth, `0x594060`, nobody's" on `kTradeRunSteps` and `field_e2.cpp`'s
  comments naming the addresses (text, not references); `kTradeTake`
  `0x594D90` (nobody's).
- Nothing in this round's other groups' files names these fourteen.

## 9. Controls

Planted one at a time in `effect_1g.cpp` by a script (anchor, rebuild, the
function's subset `BOF3X_E1G_ONLY=k,1` at 6,000 rounds, restore, rebuild at
the end; `e1g/controls.py` in the session scratchpad). **45 of 45 refused**,
each a `Fatal` (exit 3) with the count of differing rounds:

| # | Function | Mutant | Refused in |
|--:|---|---|--:|
| 1 | `ItemTrade_FullMessage` | the quantity set after the sound | 2,206 |
| 2 | | the bag label for the notice | 25 |
| 3 | | step 1 on leaving | 2,538 |
| 4 | `ItemTrade_Leave` | the other step's entry | 6,000 |
| 5 | `ItemTrade_LeaveAsk` | only bit 15 moves the hand | 876 |
| 6 | | cancel sets the hand 0 | 644 |
| 7 | | `Transition_Start(1)` | 274 |
| 8 | | sound 0x105 on no | 456 |
| 9 | | the hand at x 0xDC | 6,000 |
| 10 | `ItemTrade_LeaveWait` | `Game_Step` 2 | 3,051 |
| 11 | | the backdrop before the needs | 2,949 |
| 12 | `ItemTrade_DrawBackground` | the window steps every 8 frames | 4,478 |
| 13 | | CLUT 0x7A81 | 5,999 |
| 14 | | the bottom at 239.0 | 6,000 |
| 15 | | v2 0xEF | 6,000 |
| 16 | | the reset window 0xFF high | 6,000 |
| 17 | `ItemTrade_DrawList` | the heading in colour 6 * flag | 4,013 |
| 18 | | the picked name at +0x50 | 1,094 |
| 19 | | the shadow when short | 1,094 |
| 20 | | the pick compared without its sign | 174 |
| 21 | | the flagged icons not dim | 3,672 |
| 22 | | `ItemTrade_Lacks(i, 2)` | 5,497 |
| 23 | `ItemTrade_DrawListFrame` | nine top pieces | 6,000 |
| 24 | | the right side piece 0 | 6,000 |
| 25 | | the flag `| 0x40` | 5,482 |
| 26 | `ItemTrade_Lacks` | short at equal | 480 |
| 27 | | an 0xFF ingredient answers 1 | 1,006 |
| 28 | | the quantity's low 7 bits | 50 |
| 29 | | two ingredients | 1,296 |
| 30 | `ItemTrade_RowCount` | nine at most | 1,633 |
| 31 | | a row ends at 0xFE | 4,367 |
| 32 | `ItemTrade_DrawNeeds` | bit 6 hides the ingredients | 1,373 |
| 33 | | the quantity unsigned | 1,323 |
| 34 | | short at equal | 590 |
| 35 | | the held count at +0xA9 | 4,622 |
| 36 | `ItemTrade_DrawNeedsFrame` | fourteen bottom pieces | 6,000 |
| 37 | | a side 0x29 long | 6,000 |
| 38 | `ItemTrade_DrawCount` | the second count by flag 0 | 6,000 |
| 39 | | the icon dim | 6,000 |
| 40 | | the quantity unsigned | 1,545 |
| 41 | `ItemTrade_DrawCountFrame` | the box 0x37 high | 6,000 |
| 42 | | the right side piece 2 | 6,000 |
| 43 | `Item_DrawIcon` | item 0x80 counted as none | 17 |
| 44 | | the next kind's icon | 2,511 |
| 45 | | `Item_IconKind`'s arguments swapped | 4,012 |

## 10. The live route

None. The catalog's reach columns (`analysis/remaining_catalog.tsv`) are empty
for all fourteen except the four hidden starts' attract value -63,734, which
is `Sprite_ClutWord`'s count carried to a start inside its catalog extent
(`remaining_catalog.py`: negative - the host reached, the entry not armed),
not a call to these. No trace under `analysis/calltrace` lists an entry of
the band as reached. **Fuzz-only**; a route that opens the trade screen would
cover it with FE2's states in the coordinator's frame-hash A/B.

## 11. Self-tests, and for `analysis/calltrace/entries_logic.txt`

`BOF3X_SHADOW=effect_1g`: exit 0, 0 mismatches (section 4).
`BOF3X_SHADOW=field_e2`: exit 0, 306,000 rounds, 0 mismatches (after the
rebinding). `BOF3X_SHADOW='*'`: exit 0, 681 self-test lines, `inject: 6909 ours, 0 left original`, `effect_1g`
84,000 rounds, 0 mismatches there too (1,195,303 calls - another stream). **With
`BOF3X_WIDE=1`** (DIV-0041's operands patched): `effect_1g` alone exit 0, 0
mismatches (no widescreen site lies in the band); `'*'` stops at exit 3 in
**`battle_e7`** (1,800 of 186,000 rounds, `GeneWin_List*SlideOut`), which fails
the same alone under `BOF3X_WIDE=1` - not this group's (no E1G code or callee in
it); the main checkout holds another session's uncommitted `battle_e7` /
`widescreen.h` edits, which may be that fix.

Appended to the main checkout's `analysis/calltrace/entries_logic.txt` (the
four hidden starts, which had no line; the other ten had theirs):
`00594060 8C`, `005940F0 E`, `00594100 132`, `00594240 74`.

## 12. The backdrop under a wide picture (fix wave MB, 2026-10-03)

The owner saw Manillo's trade screen with its tiled backdrop 320 wide under
`BOF3X_WIDE=1`. `ItemTrade_DrawBackground` now takes its columns from
`Widescreen_Fill()` and its two quads from `ItemTrade_BackdropSpan`
(`effect_1g.h`): the left from `0 - c` with u from `(-c) mod 32`, the right
to `320 + c`, each u range grown by `c` - more tiles, never a stretch, the
pattern's phase kept. The cause, the geometry and the coordinator's live
check are in [`widescreen.md`](widescreen.md) section 5 ("Manillo's trade
screen"); the ledger is DIV-0041.

**Verification.** `BOF3X_SHADOW=effect_1g`, narrow and `BOF3X_WIDE=1`: 84,000
rounds, 0 mismatches each (the fuzz compares the original's quads: the
columns are 0 until `InjectAll` arms the fills). `SelfTest` now opens with
`CheckWideSpans`, a property check of the spans for 0..63 columns: the edges,
texels equal to columns, the phase at column 0 and at 0xA0, columns 0 the
original's u `0..0xA0` with a +0.0 left edge. Six controls planted one at a
time by a script (`mb/controls.py` in the session scratchpad), **6 of 6
refused** (exit 3):

| # | Mutant | Run | Refused by |
|--:|---|---|---|
| 1 | the columns from `Widescreen_Live()` (wide during the fuzz) | wide | the clone fuzz, 6,000 of 6,000 rounds |
| 2 | the left u not phased (u0 0) | narrow | the span check: phase |
| 3 | the left u range not grown | narrow | the span check: a stretch |
| 4 | the right u range not grown | narrow | the span check: a stretch |
| 5 | the right edge one column short | narrow | the span check: the edges |
| 6 | `-columns` for the left edge (-0.0 narrow) | narrow | the span check: -0.0 |

`BOF3X_SHADOW='*'` narrow and with `BOF3X_WIDE=1` on the branch tip: exit 0
both, every group at 0 mismatches. `tools/ledger_check.py`: 67 entries, 0
errors.
