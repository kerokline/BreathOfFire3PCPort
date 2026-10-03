# The yes / no prompts outside Menu_YesNo (DIV-0027 amended)

**Status:** IN PROGRESS (2026-10-03, fix wave group YN, branch `fix/1003-yn`).
Built and self-tested headless; **nothing seen on screen yet** - the live
check is section 6, for the coordinator. The masters screen's headers are
section 5: found, not translated (no US source string located).

DIV-0027 fitted `Menu_YesNo`'s four prompts (save, overwrite, load, "Is this
what you want?") to the English line in 2026-09; the owner confirmed it at an
inn. The same fault - the hand's stops fitted to the Chinese line, so under
the English overlay the hand stops a word short of `Yes` and, on `No`, sits
over `Yes` - is on choosers `Menu_YesNo` does not reach. The method is
[`glyph-draw.md`](glyph-draw.md) section 7's; the ledger entry is DIV-0027's
amendment.

## 1. What the owner saw

| Prompt | Picture (main checkout) | The owner |
|---|---|---|
| The master's "Is this OK?  Yes No" over the party's stat panels | `analysis/shots/owner_catalogue/master_yes_no.webp` | the hand a word's width left of `Yes` |
| Manillo's "Will that be all?  Yes No" | `owner_catalogue/manillo_will_that_be_all.png`; `analysis/shots/manillo_1003/manillo_sheet.png` panels 8, 9 (`caughFish.txt` frames 3690, 3735) | on Yes a word short; on No over `Yes`. "If we could move the yes hand and yes word to the left to match the spacing on the load save screen, I think that would look best." |
| Manillo's "Want to buy anything?" (the question, then `Yes` and `No` on rows of their own) | `manillo_sheet.png` panels 3, 4 (frames 3465, 3510) | the hand on the question's second row, one above `Yes`: "the hand is offset by one row - it just needs to be one row lower, and it should be aligned" |
| The shop's "Sell Ammonia?", "Buy Bracers?", "Sell Green Apple?" | `analysis/shots/shop_1003/shop_yesno_sheet.png` (`shop.txt` frames 1050, 1680, 1980; frame 270, "Load game?", the reference) | as Manillo's panel 9 |

## 2. The choosers and their causes

Every `Menu_DrawHand` (`0x5905D0`) call site was listed by an E8 scan of
`BOF3.exe` (44 sites; the scan script is in the session scratchpad,
`fixwave/yn/hand_calls.py`), then each prompt matched to its site by the
captures' coordinates and the area pools' strings (`en.AREA030.DAT`'s pool
at `+0x10`: message `0x43` "Welcome to the Manillo Trading Shop", `0x44`
"Want to buy anything? / Yes / No", `0x45` "Will that be all?  Yes No"). The
English lines (read from the overlays, not copied here) all end at the
line's 33rd character with the two answers one space apart: the master's is
the question, 16 spaces, `Yes No`; Manillo's 10 spaces; system message `0xF`
27 spaces then `Yes No`. French and German carry other answer lengths
(`Oui Non`, `Ja Nein`), so ours measures the stops from the line.

1. **The master's prompt** - Capcom's `0x586D20` (not ours; called by the
   stubs `0x586D00` / `0x587150` with message `word [0x6644E8 + 2 * master] +
   0x11`). It draws the party's panels (`0x585DC0`, `0x585BE0`), a box
   (`0x586160(0x14, 0x10, 0x118, 0x13)`), the line `Text_DrawAt(0x1B, 0x13,
   0, 0xFF, MessagePools + word)` at `0x586E78`, and the hand
   `Menu_DrawHand(0xCF + 36 * byte 0x9398D2, 0x15, 0)` at `0x586E97` (0 Yes,
   1 No; up/down flips the byte at `0x586DC2`). Stops 207 / 243; English
   `Yes` at 27 + 8 * 27 = 243, `No` 275. The areas whose tables reach this
   chooser are the fourteen of `area_w3f.md`'s shared row (3, 37, 41, 50, 55,
   59, 61, 68, 74, 91, 98, 113, 116, 143) - the same fourteen whose English
   pools hold "Is this OK?".
2. **Manillo's "Will that be all?"** - `ItemTrade_LeaveAsk` `0x594100` and
   `ItemTrade_LeaveWait` `0x594240` (ours, E1G's, `effect_1g.cpp`'s
   `LeavePrompt`): the line at x `0x18`, the hand at `0xE0 + 36 * byte
   0x6BE08F` (0 Yes). Stops 224 / 260; English `Yes` 240, `No` 272. **It is
   not the shop's code**: the trade screen is its own state machine (FE2 /
   E1G), so it is fixed in its own place.
3. **The shop's yes / no** - `ShopWin_TitleRun` `0x59B240` (ours,
   `menu_draw_helpers.cpp`) draws the help window's message at window x + 7
   and, for help ids `0x36`, `0x49`, `0x4A`, `0x52`, system message `0xF`
   - `Menu_YesNo`'s own line, 27 spaces `Yes No` - over it. The hand is a
   window record of its own (record 21, `0x803454`, drawn by `MenuWin_Hand`
   at its `+4` / `+6`) placed by the step: `YesNoFrame` in
   `shop_states2.cpp` (help `0x49` "buy?", `0x4A` "equip it?", `0x52`
   "sell?") and `SharedList_UseItem` `0x584470` in `field_s.cpp` (help
   `0x36`, the shared list's "use the item?"), both at window x `+ 0xE8 - 36
   * byte 0x929F0B` (1 Yes): relative to the line, `Yes` at + 216 and the
   hand at + 189 / + 225 - Menu_YesNo's original stops less one unit (218 /
   254 over its line at 28), which DIV-0027 moved only inside `Menu_YesNo`. (The
   help window's x itself was not read; the captures put the line where
   `Menu_YesNo`'s is.)
4. **Manillo's "Want to buy anything?"** - `LeaderPanel_S4Again` `0x529FA0`
   (ours, E1E's, `effect_1e.cpp`; stage 4 of the leader's state 9, which the
   panel's messages `0x43` / `0x44` now identify as Manillo's shop entry):
   `FieldPanel_DrawMessage(8, 0x94, 0x44)` puts the text at (0x12, 0x9C),
   rows 12 apart, and the hand at `(0x20, 0xAA + 12 * Sprite_Current +6)` -
   the row-1 answer of a one-row question. The English question wraps to two
   rows, so the hand points at its second. The x was already right: the
   answers are `  Yes` / `  No`, so `Yes` starts at 0x12 + 16 = 34 and the
   hand's tip at 31 - three units before it, DIV-0027's own gap.

## 3. What ours does (under a Latin overlay only)

The load / save screen's relations, from `Menu_YesNo` under DIV-0027: the
hand at `Yes - 2` (tip 3 units before it), `No` 56 units after `Yes` (24 for
the word, 4 spaces), the hand at `No - 2`. Getting there needs three spaces
moved from before `Yes` into the gap, so **`Yes` moves left and `No` stays**
(the owner: "move the yes hand and yes word to the left"; `No` already sat
where those relations put it, and 4 spaces are the least gap that clears a
24-unit hand). The stops are measured on the re-spaced line with the pen's
own advances (DIV-0006), not assumed at 8 units:

| Prompt | Line x | English `Yes` / `No`, before | after | hand x, before | after |
|---|---|---|---|---|---|
| master | 0x1B | 243 / 275 | 219 / 275 | 207 / 243 | 217 / 273 |
| Manillo, leave | 0x18 | 240 / 272 | 216 / 272 | 224 / 260 | 214 / 270 |
| shop, help window at x w | w + 7 | w + 223 / w + 255 | w + 199 / w + 255 | w + 196 / w + 232 | w + 197 / w + 253 |
| Manillo, want to buy | (0x12, 0x9C) | `Yes` row 2 (y 0xB4) | - | y 0xAA / 0xB6 | y 0xB6 / 0xC2 |

- `YesNoLayout_Tail` (`yes_no_layout.cpp`): a question, spaces, a word,
  spaces, a word - any other shape (a control byte, fewer than four spaces
  before the first answer) aborts with the prompt named.
- The master's: both calls of `0x586D20` re-aimed (`MasterAsk_Line`,
  `MasterAsk_Hand`; the hand's answer recovered from Capcom's `0xCF + 36 *
  answer`, anything else aborts). Switch `BOF3X_ORIGINAL=MasterAskLayout`.
- Manillo's leave: `LeavePrompt` behind the byte `g_trade_leave_layout`,
  switch `TradeLeaveLayout`.
- The shop: `ShopWin_TitleRun` draws the line `Respace`d exactly as
  `Menu_YesNo`'s (`YesNoLayout_SystemLine`); the two hand sites take
  `YesNoLayout_ShopHandX(window x + 7, answer)`. Three bytes, one switch
  `ShopYesNoLayout`.
- "Want to buy": `LeaderPanel_S4Again` counts message `0x44`'s newlines
  (stepping `0x05` / `0x07` arguments and two-byte codes): the first answer's
  row is newlines - 1; the hand moves by 12 per row past the original's
  row 1, so a one-row question draws exactly the original. Switch
  `ShopAskRow`. Fewer than two newlines aborts.

Each byte is set after its module's self-test (which compares ours with the
original's draw, so the fuzz runs with the fix off), and only when
`Lang_Latin()` - `BOF3X_LANG=original`, unset and the full-width languages
are Capcom's.

**A long help line under the shop's hand.** "Equip" with a 16-letter item
name is 23 characters; the hand on `Yes` now covers the line's columns 21..23
- exactly the columns the original's hand covered over the English line, so
no prompt is worse than it was. Not measured on screen.

## 4. Every hand on Manillo's screen (for the owner's marks)

The trade screen and its entry, every `Menu_DrawHand` the code can draw
there, x and y in game units (320 x 240):

| Where | Function (ours) | Hand at | Changed? |
|---|---|---|---|
| "Want to buy anything?  Yes / No" (panel 3, 4) | `LeaderPanel_S4Again` `0x529FA0` (`effect_1e.cpp`) | (0x20, 0xAA + 12 * answer) | **yes**, a row lower under English (section 3) |
| the trade list (panels 5..7) | `ItemTrade_Run` `0x5939F0` (`field_e2.cpp`) | (0x1A, 0x51 + 13 * pick), hidden while the pick has bit 6 or 7 | no (not marked) |
| "How many do you want?" | `ItemTrade_PickCount` `0x593C60` (`field_e2.cpp`) | (0x1A, 0x6B) | no |
| "Is <item> OK?  Yes No" per item | `ItemTrade_Confirm` `0x593E60` (`field_e2.cpp`) | (0xDC + 36 * answer, 0x16); "Yes No" drawn at (0xDE, 0x14), so English `Yes` 222, `No` 254: on `No` the hand (sprite 234..257) covers `Yes` | **no - likely the same fault, not marked** |
| "Will that be all?  Yes No" (panels 8, 9) | `ItemTrade_LeaveAsk` `0x594100`, `ItemTrade_LeaveWait` `0x594240` (`effect_1g.cpp`) | (0xE0 + 36 * answer, 0x16) | **yes** (section 3) |

Nearby in the same machine, not seen on Manillo's screen: `LeaderPanel_S1Choose`
(0x70, 0x82 - 16 * answer), `LeaderPanel_S9Menu` (0x58 + 48 * choice,
0x2E), `ExtraSlots_*` (0x1C, 0x58 + 16 * slot) and (0xA8, 0x5A + 13 * row).

## 5. The masters screen's headers (`campingFishing.txt` frame 2400)

The two Chinese headers are **`.data` label slots of `BOF3.exe`**, 8 bytes
each, beside DIV-0064's: `0x66A1F0` (the list's title, drawn by `0x59C2C0`
through `0x57D800` at `0x59C5C4`) and `0x66A1F8` (the portrait box's label,
`Text_DrawAt` at `0x59C9F0` in `0x59C8F0`); `0x66A1E8` before them is not
referenced. Both drawers are window kinds of `Window_Handler7KindTable`'s
camp set - **not ours** (`HANDOFF.md`'s round 14 candidates). The data side
would be DIV-0064's way, a kind-15 group written into the two slots (7
letters each), but **no source on the US disc was found**: a case-blind
search of every file of the US image for `master`, `pupil` and `apprentice`
finds the system pool's "Master:" and "View master's profile", the master
list's menu entries and dialogue - no short header string, and `BATTLE.EMI`
has none before the skill types (its `ITEM`..`VITAL`, `HEAL`..`DRAGON` are
DIV-0064's). The US screen may draw them as graphics, or use other words.
**A question for the owner:** what do the PlayStation's two headers say on
that screen (a capture from the sibling, or memory)? With the words and
their place on the disc, the group is a few lines of `loc_build.py` and
`labels.cpp`. Nothing changed for this item.

## 6. For the coordinator's live check

Shot copies with `tools/recipe_shots.py` (never by hand); English, the
owner's settings (`--lang en`, `BOF3X_FILTER=point` as recorded), wide as
the captures were.

1. **Manillo** - `python tools/recipe_shots.py
   C:/Users/kerok/Documents/GitHub/BreathOfFire3PCPort/tools/recipes/caughFish.txt
   --every 45 --out <scratch>/caughFish_45.txt`, then `python
   tools/input_run.py <scratch>/caughFish_45.txt --out
   analysis/shots/yn_manillo --lang en --minutes 5`. Frames **3465, 3510**
   ("Want to buy"): right - the hand's tip just left of `Yes` (on its row),
   not over the question; wrong - the hand on "anything?". Frames **3690,
   3735** ("Will that be all?"): right - on Yes the hand's tip 3 units (6 px
   at 2x) left of `Yes`, `Yes` three spaces further left than in
   `manillo_sheet.png` panel 9, `No` where it was; on No the hand between
   the words, touching neither. Compare against `f00270` of the shop run
   below for the spacing.
2. **The shop** - `shop.txt` with `--every 30`, the same way, to
   `analysis/shots/yn_shop`. Frames **1050** (sell, item shop), **1680**
   (buy, weapon shop), **1980** (sell): right - the hand, `Yes` and `No`
   spaced as frame **270**'s "Load game?" (hand touching `Yes`, `No` well
   apart), `Yes` now 24 units left of where the sheet shows it. Unchanged:
   **2520** (the inn's menu) and **2880** (the inn's stacked "Save?").
3. **The master's "Is this OK?"** - **on neither route**: `campingFishing.txt`
   reaches the camp's master list (area 90), whose pool has no such prompt;
   the prompt is in the fourteen master areas (section 2). It needs a route
   the owner records (talking to a master and taking the apprenticeship).
   Right looks like the shop's: hand at 217 on `Yes` (219), at 273 on `No`
   (275).
4. Also worth a look in the same runs if they pass: the per-item "Is <item>
   OK?" on Manillo's screen (section 4) - unchanged, likely wrong.

`BOF3X_ORIGINAL=MasterAskLayout,TradeLeaveLayout,ShopYesNoLayout,ShopAskRow`
gives the before picture with everything else equal.

## 7. Checked

- `BOF3X_SHADOW=yes_no_layout,effect_1g,effect_1e,menu_draw_helpers,shop_states2,field_s`
  headless: 0 mismatches each (the fuzzes run with the fixes off).
- `BOF3X_SHADOW=yes_no_layout` re-spaces four prompt shapes - English
  (both), German "Ja Nein", French with a two-byte code in the question -
  and checks both stops against the pen summed character by character.
- With `BOF3X_LANG=en` the log shows the two retargets and four bytes on
  (`MasterAskLayout`, `ShopYesNoLayout` x 3, `TradeLeaveLayout`,
  `ShopAskRow`).
- `BOF3X_SHADOW='*'` headless: see the branch's report.

**Owed the owner's eye:** all four prompts on screen; whether the per-item
"Is <item> OK?" should get the same treatment; the masters' headers' words.
