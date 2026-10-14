# The faster slide: the slide-off bounds split from the picture (ultrawide round, group SL)

**Status:** BUILT (2026-10-13), on `a826c9cb` (the round branch
`wide/ultrawide-round` after EB-2), branch `wide/sl`. Self-tested headless at
`BOF3X_WIDE` unset, 1 and 2, with a property check over every start and a fuzz
control per touched module. Nothing here has been seen in play or run live.

This builds the owner's call of 2026-10-13 on EB-2's cause 2
([`ultrawide-census-2.md`](ultrawide-census-2.md) section 3.2): **the faster
slide.** In the wide modes a sliding box clears the wide bound on exactly the
frame the original's reached Capcom's bound, so the frame count and every
state the game reads are Capcom's, and the box never hangs in the band.

## 0. The answer

- **Logic reads the window's x** (section 2): the slide state's own hold or
  free test does, and so does at least one other window's state
  (`MenuList_SlideLeftBesidePanel` `0x59A070` stops beside record 4's x).
  So the change takes the "split" shape UC2 used for the off-screen flag:
  **the record keeps Capcom's x, on Capcom's schedule, and only the draw sees
  a mapped x.**
- **Every one of the fourteen states now decides by Capcom's bound at every
  width.** The fourteen immediates are no longer patched. The wide bound
  (`widescreen_rule::SlideBound`, the same rule as before: the bound moved
  out by the columns) is only where the box is *drawn* on the frame it holds
  or is freed.
- **The drawn x** runs on a straight line from where the slide started to the
  wide bound, reaching it on the frame Capcom's x reaches Capcom's bound: a
  faster slide. A box parked at a hold is drawn at the wide bound until
  something moves it.
- **The drawn x is in the record only while the record's own handler draws
  it.** `Field_RunTaskRecords` (ours) puts Capcom's x back after each handler
  returns, so anything that runs outside that handler - another window's
  state, the menu code, the free test of the next frame - reads Capcom's x.
- **Proved** (section 4): a start-up property check runs all fourteen states
  from 1,278 start x each (every x in `[-640, 640)` and every 257th across
  the s16 range) at 0, 53 and 120 columns through the handler cycle: the
  hold or free frame and every x outside the handler equal narrow's, and the
  drawn x is at or past the wide bound on that frame (3,380,148 frames). The
  six touched modules' fuzzes stay at 0 mismatches; their control, the late
  free of the build before this one, is refused by every one at
  `BOF3X_WIDE=1` (and `battle_e7`'s at `=2`).
- **Narrow is bit for bit:** every hook returns before touching anything
  when the columns are 0, and the states read the same bound they always
  read there.

## 1. The fourteen states

All fourteen are ours (`impl` in `symbols.toml`), and each reads its bound
through `Widescreen_SlideBound` / `Widescreen_SlideOperand` (`widescreen.cpp`
`g_slides`). Read 2026-10-13 from our source and the `symbols.toml` evidence
lines; every one steps x by 0x20 a frame, none differs. "Hold" states step,
then hold x at the bound with `+3 = 0` when the stepped x is past it.
"Free" states draw, then free the window (`Window_FreeCurrent` `0x59E310`)
when the x before the step is at or past the bound, else step.

| State (ours) | Original | Kind | Test | Capcom's bound | Drawn bound at 53 / 120 |
|---|---|---|---|---|---|
| `MenuSlide_LeftOff170` (`rest_2f.cpp`) | `0x596920` | hold, -0x20 | `x < b` | -170 | -223 / -290 |
| `MenuList_SlideLeftOff300` (`rest_2g.cpp`) | `0x599CC0` | hold, -0x20 | `x <= b` | -300 | -353 / -420 |
| `MenuList_SlideLeftOff100` | `0x599DF0` | hold, -0x20 | `x < b` | -100 | -153 / -220 |
| `MenuList_SlideLeftOff180` | `0x59A130` | hold, -0x20 | `x <= b` | -180 | -233 / -300 |
| `MenuList_SlideLeftOff110` | `0x59A2B0` | hold, -0x20 | `x < b` | -110 | -163 / -230 |
| `MenuSlide_LeftOff` (`menu_lists.cpp`) | `0x59A580` | hold, -0x20 | `x < b` | -200 | -253 / -320 |
| `MenuWin_SlideOutLeft` (`menu_draw_helpers.cpp`) | `0x59B440` | hold, -0x20 | `x < b` | -150 | -203 / -270 |
| `MasterWin_SlideOut` (`rest_2h.cpp`) | `0x59C130` | hold, -0x20 | `x < b` | -120 | -173 / -240 |
| `MenuSlide_RightOff` (`menu_lists.cpp`) | `0x59A5E0` | hold, +0x20 | `x > b` | 320 | 373 / 440 |
| `GeneWin_GridSlideOut` right (`rest_2g.cpp`) | `0x5989F0` | free, +0x20 | `x >= b` | 322 | 375 / 442 |
| `GeneWin_GridSlideOut` left | `0x5989F0` | free, -0x20 | `x <= b` | -190 | -243 / -310 |
| `GeneWin_ListSlideOut` (`battle_e7.cpp`) | `0x599050` | free, -0x20 | `x <= b` | -165 | -218 / -285 |
| `GeneWin_List2SlideOut` | `0x599440` | free, +0x20 | `x >= b` | 347 | 400 / 467 |
| `GeneWin_List3SlideOut` | `0x599540` | free, +0x20 | `x >= b` | 323 | 376 / 443 |

**What the late free cost, as a rule.** With the bound moved by `c`, a box
needed about `c / 32` more frames: from x 17 (`MenuSlide_RightTo17`'s rest)
`MenuSlide_LeftOff` holds after 7 frames narrow, 9 at 53 columns and 11 at
120 - EB-2's measured 2 and 4 frames.

**Where they are reached.** The nine holding states sit in thirteen state
tables (`scratch uw_sl/tables.py` over `BOF3.exe` and `symbols.toml`, the
dword scan `refs.py`): `Window_Handler2KindTable` `0x66AEC8` (four
sub-tables), `MenuList_Kinds` `0x66AF94` (eight), `Window_Handler1KindTable`
`0x66AE40`, `MenuList_IconWheelStates` `0x66B0A0`,
`MenuList_EquipCompareStates` `0x66B184`, `ShopWin_MemberStatsSteps`
`0x66B26C`, `ShopWin_ItemListSteps` `0x66B2BC`, `ShopWin_SharedListSteps`
`0x66B2D8`, `ShopWin_MasterListSteps` `0x66B384` and
`Window_Handler8KindTable` `0x66B534`. **Entry 0 of every sub-table is
`BareRet` `0x437CC0`** - state 0, which a hold sets, runs nothing. The five
freeing ones are pushed by `Window_Handler5Kinds` `0x598890`'s stack tables
(`0x5988F2`, `0x598F82`, `0x599382`, `0x599492`). No `call` / `jmp rel32` in
`.text` reaches any of the fourteen or any of the record handlers they run
under (`relcalls.py`, a byte scan that over-reports and cannot miss); the
handlers are reached only from `Field_RunTaskRecords`' stack table
(`0x59E249..0x59E284`), and none of our source calls them directly (grep of
`SH_CALL` / `BH_CALL` / `SH_AT` outside the fuzz files).

## 2. Who reads the window's x

The question the owner put: does anything other than the draw read the x
during the slide?

1. **The state itself.** The hold test (`x < b` and kin, then the clamp) and
   the free test read +4 every frame. That is logic: it decides `+3` and the
   free, which the game waits on (EB-2's routes).
2. **The kind function around it.** Every kind whose step table holds one of
   the nine is "call `[steps + 4 * +3]`, then draw": the evidence lines of
   `ShopWin_*Run`, `BattleMenuWin_*Run`, `Win1_*`, `Win2_*`, `MenuList_*` in
   `symbols.toml`, and their bodies in our source (scratch `bodies.py`).
   After the step +4 is read only as arguments to draw calls - `Menu_DrawBox`,
   `Menu_DrawPiece(s)`, `Text_DrawAt`, the list and panel draws. The draws
   that take the record (`Win2_DrawItemList`, `Menu_DrawItemList`,
   `BattleMenu_DrawItemList` / `_DrawSkillList`, `Shop_DrawBuyList`,
   `SharedList_DrawList` / `_DrawMember`, `MasterWin_DrawList`,
   `BattleEquipWin_Draw` / `_DrawItems`, `Win1_DrawShisuPanel`,
   `Menu_DrawEquipCompare`, the gene lists' and grid's draws) use +4 only to
   place what they draw; the record fields they write (`+0x10`, `+0x12`, the
   scroll) do not depend on it.
3. **Another window's state.** `MenuList_SlideLeftBesidePanel` `0x59A070`
   (`MenuList_StatsStates[2]`) slides the stats panel left until it is
   beside **record 4's x** + 0x78 (`0x8031F4`). Record 4 is a member panel
   (`MenuList_MemberPanel`), whose table holds `MenuList_SlideLeftOff300`
   and `MenuSlide_RightOff`. Under the patched bounds a member panel parked
   by the first sat at -353 (16:9) instead of -300, and this state, run
   then, read that: one more way the old build's wide modes could fork
   logic (not seen on a route; read from the code).
4. **Anything else** - the menu code that places a hand, another record's
   draw, the field menu's controller - runs outside the record's handler.
   The split makes enumerating them unnecessary: there +4 is always Capcom's.

So logic does read it, and the drawn x must not be what logic reads.

## 3. The shape: Capcom's x in the record, a drawn x for the handler

`widescreen.cpp` keeps one `Split` per window record (22, `WindowRecords`
`0x803160`, 0x24 each): Capcom's x `L`, the drawn x `D`, and the *leg* the
drawn x is on, from where the slide started `(aL, aD)` to `(endL, endD)` =
(Capcom's bound, `SlideBound(bound, c)`). The drawn x for an `L` is on the
straight line between them; at or past the end it is the end moved as `L`
moved; at or behind the start, the start moved as `L` moved; saturated to an
s16. The way (left or right) is the bound's side, as `SlideBound` reads it.

- **A holding state** (`Widescreen_SlideHeld`, after Capcom's step and
  clamp): records `L`, starts a new leg unless this state is continuing its
  own (same record kind and handler, same bound, `L` where it left it), and
  writes `D` into +4 for the draw that follows in the same handler.
- **A freeing state** (`Widescreen_SlideDraw` before its draw,
  `_SlideUndraw` after): `D` in +4 for the draw only, then Capcom's x back
  before the free test reads it. A leg continues when x is the last frame's
  plus the step.
- **`Field_RunTaskRecords`** (`window_task.cpp`): `Widescreen_SlideBefore`
  draws a parked box at its `D` - only for a holding state's split, only
  while +3 is 0 (the bare ret), and only if the record is still that kind,
  active, and at Capcom's `L` - and `Widescreen_SlideAfter` puts `L` back
  after the handler (or ends the split when the handler moved the record
  some other way, or freed it). `Widescreen_SlideIdle` ends the split of a
  record the loop skips as free.
- **Armed** only by `Widescreen_ArmFills` (after every module's self-test,
  so every fuzz compares Capcom's code), only when the view is wide, only
  with the switch on (`PatchBytes "Widescreen"` on a byte of ours, so
  `BOF3X_ORIGINAL=Widescreen` leaves every box sliding and parking as
  Capcom's does), and only while `Field_RunTaskRecords` is ours (Capcom's
  loop would not put the x back).

Every value is a rule in the columns: the drawn end is `SlideBound(bound,
c)` and the line between is derived from it, so at 0 columns `D = L` and
nothing is written.

**What the picture does.** `MenuSlide_LeftOff` from x 17: Capcom's x -15,
-47, -79, -111, -143, -175, -200 (hold); drawn at 53 columns -22, -62,
-102, -142, -182, -221, -253; at 120, -32, -82, -132, -181, -231, -281,
-320. `MenuSlide_RightOff` from 194: Capcom's 226, 258, 290, 320; drawn at
53 239, 284, 330, 373; at 120 256, 318, 381, 440.

## 4. The proof

**The property check** (`Widescreen_CheckSlides`, run at every start-up by
`Widescreen_ArmFills`, whatever the width). For each of the fourteen
states, at 0, 53 and 120 columns, from every start x in `[-640, 640)` and
every 257th across the s16 range (1,278 starts), a scratch record is run
through the cycle the loop gives it: `SlideBefore`, the state, the draw (+4
read as the drawn x), `SlideAfter`, and three parked frames after a hold.
The nine holding states are **ours, called** through `0x905B84` (if
`BOF3X_ORIGINAL` left one Capcom's, Capcom's step as read plus the hook);
the five freeing ones call draws, so the check runs Capcom's step as read
with `SlideDraw` / `SlideUndraw` around the draw, as ours have them. Every
frame: +4 and +3 (or the free) outside the handler equal a model of
Capcom's code, so the hold or free frame is narrow's - which is also
compared directly; narrow, the drawn x is Capcom's; wide and from a start
that slides toward its bound without wrapping, the drawn x never moves back;
on the hold or free frame it is at or past the wide bound (exactly on it at
a hold); a parked box is drawn there with Capcom's x outside. Fatal on any
difference. Result in every run: 14 states (9 of ours called), 1,278 starts
each, 3,380,148 frames, longest slide 1,036 frames (a start that wraps the
s16). A first cut was refused by it: a freeing slide started already past
its bound drew at its own x, inside the band, for its one frame (fixed:
the way comes from the bound's side, not from start to end).

**The fuzzes.** The six modules holding the fourteen (`menu_lists`,
`rest_2f`, `rest_2g`, `rest_2h`, `menu_draw_helpers`, `battle_e7`) and
`window_task` (the loop) run their start-up fuzzes against Capcom's clones
with the split unarmed: section 5.

**The control** (`BOF3X_SL_CONTROL=1`): the states decide by the bound moved
out by the columns, which is the late free of the build before this one.
Run per module with `BOF3X_SHADOW=<module>` (`uw_sl/controls.sh`):

| Module | `BOF3X_WIDE` | exit | refused in |
|---|---|---|---|
| `menu_lists` | 1 | 3 | 913 rounds (`MenuSlide_LeftOff` 455, `MenuSlide_RightOff` 458) |
| `rest_2f` | 1 | 3 | 1,587 (`MenuSlide_LeftOff170`) |
| `rest_2g` | 1 | 3 | 8,948 (`GeneWin_GridSlideOut` 958, `_SlideLeftOff300` 2,406, `_100` 1,542, `_180` 2,532, `_110` 1,510) |
| `rest_2h` | 1 | 3 | 2,353 (`MasterWin_SlideOut`) |
| `menu_draw_helpers` | 1 | 3 | 1,168 of 50,000 (`MenuWin_SlideOutLeft`) |
| `battle_e7` | 1 | 3 | 1,800 of 186,000 (`GeneWin_ListSlideOut` 484, `_List2` 440, `_List3` 876) |
| `battle_e7` | 2 | 3 | 1,849 (492, 444, 913) |
| all six and `window_task` | unset | 0 | not refused, as it must be: narrow the control moves nothing (`battle_e7` 0 of 186,000) |

Each refusal names exactly the slide states: no other function of the
modules moved. (`window_task` has no slide bound; its change is the loop's
two calls, unarmed in its fuzz.)

## 5. Measured

Builds at the branch tip with `cmake --build build` (no diagnostic from a
changed line; the two warnings in touched files, `battle_e7.cpp`'s unused
`Addr` and `rest_2h.cpp`'s function cast, are in lines this does not
touch). Self-tests: `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original
BOF3X_SHADOW='*' bof3x-launcher.exe --game bof3 --no-config` from three
ini-less copies of the launcher, `bof3x.exe` and the dll (`uw_sl/w0`, `w1`,
`w2`; `uw_sl/selftest.sh`):

| `BOF3X_WIDE` | exit | time | `inject:` | `patch ON Widescreen` | the faster slide's lines |
|---|---|---|---|---|---|
| unset | 0 | 1,149 s | 10087 ours, 0 left original by BOF3X_ORIGINAL | 0 | the check (14 states, 9 of ours called, 1,278 starts each, 3,380,148 frames, longest 1,036); no "faster slide: on" line (narrow) |
| 1 | 0 | 1,106 s | the same | 5 (the 4 range operands, the switch) | the check as narrow; "the faster slide: on ... moved out by 53 columns" |
| 2 | 0 | 1,108 s | the same | 5 | the check as narrow; "... moved out by 120 columns" |

The touched modules' fuzzes, the same at all three widths, every one 0
mismatches: `menu_lists` 20,000 rounds over 20 functions; `rest_2f`
294,000 over 49; `rest_2g` 192,000 over 48; `rest_2h` 144,000 over 36;
`menu_draw_helpers` 50,000 over 25; `battle_e7` 186,000 over 31;
`window_task` 33,600 over 24. No log line of any run names a non-zero
mismatch count. The `DIV-0041` lines: at every width the width-rules line
("slide bounds -420..467" at 120, now the drawn ends) and the check's line;
at `=1` also "53 columns a side (426 x 240) ...", "full-frame fills at
(-53, 0) 426 x 240" and "the faster slide: on"; at `=2` the same with 120,
560 x 240 and (-120, 0). UC2's two loops ran as before (`sprite_draw` 0
mismatches, control refused in 4,112 / 4,429; `field_frame` 0). Before this
amendment the wide runs logged 18 `patch ON Widescreen` (the 14 bounds and
the 4 ranges; [`ultrawide-frame.md`](ultrawide-frame.md) section 6).

`python tools/ledger_check.py`: 114 entries, 0 errors, 2 notes (draft G is
not in the ledger yet).

## 6. What changes, and what is left

- **The picture against the build before this one:** the box slides out
  faster in the wide modes and is gone on Capcom's frame. The boxes that
  freed 2 frames late at 16:9 and 4 at 21:9 now free on time.
- **A slide back in from a parked spot** starts from Capcom's x, which is
  where the original starts it. If the box was parked by one of the nine,
  the first frame of the slide-in is drawn at Capcom's bound plus a step,
  which is inside the band whenever the box is wider than that x is far
  past the 4:3 edge, less the columns. The build before this one started
  such a slide-in from the wide bound, and took longer for it. Census-1
  section 6.3's slide-ins (`sortScreens` 1650..1800) stood at the same place
  relative to the 4:3 view at 16:9 and 21:9 on every frame, which suggests
  (an inference, not checked) that they start from placements, not from a
  parked spot. Whether any recipe slides a box back from a parked spot was
  not looked for. If one does and it shows, the slide-in states of the same
  tables (all ours) can take the same hook with their own bound as the end;
  logic would not change.
- **A parked box moved vertically** (`MenuSlide_UpOff` and kin on a record
  parked off the side) is drawn at Capcom's x while that state runs, since
  the loop draws the wide x only at state 0. No such sequence is known.
- **Something drawn from logic's x** - a hand or cursor placed from a
  sliding window's x by other code - is placed by Capcom's x and so trails
  the faster box during the slide. None was found; none was looked for
  beyond section 2.

## 7. Not verified

- Nothing was run live. No box has been seen sliding at either width. The
  state hash was not run: the claim to check is that dragonTransform and
  cutsceneAndNue now stay on the narrow route at 16:9 and 21:9 (with EB-2's
  cause 1, the inset's `Rand`, removed or isolated by
  `BOF3X_WIDE_INSET=0`).
- The freeing states' composition (ours with the hooks, armed) is not run by
  the check: it calls Capcom's step as read, with the hooks around the draw
  as ours place them. Ours equal Capcom's step by their fuzzes (unarmed).
- That no draw function of the kinds in section 2 feeds +4 into a stored
  value: by reading, helped by a line scan of their bodies (`bodies.py`).
  A draw that culls by x would change only what is drawn.
- The draw-item pool: drawing a box further out can change how many
  primitives a frame queues (as any wide draw does); not logic, not
  measured.
- A record freed by something other than `Window_FreeCurrent` and allocated
  again before the loop sees it free, at the same x, kind and handler, with
  state 0, would be drawn at the old split's wide bound for a frame. Not
  seen; the loop ends a split for every record it skips as free.

## 8. The owner's calls met

- **The faster slide** (2026-10-13), as built. Nothing tuned by eye here:
  the end is UW's rule, the line straight.
- Open, for the owner's eye: whether the faster slide reads well. The drawn
  speed is (wide bound - start) / (Capcom's bound - start) of Capcom's: at
  120 columns 1.55 for `MenuSlide_LeftOff` from 17 and 1.95 for
  `MenuSlide_RightOff` from 194 (1.24 and 1.42 at 53). Also the slide-in
  first frames of section 6, if one shows.

## 9. For the other files

**`docs/DIVERGENCE.md`, draft G: an amendment of DIV-0041:**

> **Amended (ultrawide round, group SL, [`ultrawide-slides.md`](ultrawide-slides.md)): the faster slide - the slide-off bounds split from the picture, 2026-10-13.** The fourteen slide-off states of the menu and shop windows (`0x596000..0x59D000`, `widescreen.cpp` `g_slides`) step a window record's x by 0x20 a frame and hold it at a bound just past the 4:3 edge (nine) or free the window past it (five). **The 2026-09-23 amendment above changed logic, not only the picture.** It moved each bound out by the columns, so in the wide modes a box took longer to hold or be freed - 2 frames at 16:9 and 4 at 21:9 for a short slide - and the game waits on the window: EB-2 measured dragonTransform and cutsceneAndNue leaving the narrow route by it ([`ultrawide-census-2.md`](ultrawide-census-2.md) 3.2). A parked box's x was also read by another window's state (`MenuList_SlideLeftBesidePanel` `0x59A070` stops beside record 4's x). *Original behaviour:* one bound per state for the hold or free and the picture. *New behaviour:* every state decides by Capcom's bound at every width (the immediates are no longer patched), so the record's x, its state and the free are Capcom's on every frame. In the wide modes the box is drawn on a straight line from where its slide started to the bound moved out by the columns (`widescreen_rule::SlideBound`, unchanged), reaching it on the frame Capcom's x reaches Capcom's bound - a faster slide - and a parked box is drawn there. The drawn x is in the record only while the record's own handler draws it: `Field_RunTaskRecords` puts Capcom's x back after each handler, so every other reader sees Capcom's. Off until `Widescreen_ArmFills`, narrow, under `BOF3X_ORIGINAL=Widescreen`, and while `Field_RunTaskRecords` is Capcom's. *Why:* the owner, 2026-10-13: widen the picture, not the logic; the faster slide over freeing at Capcom's bound with the box vanishing in the band. *Verification:* a start-up property check (`Widescreen_CheckSlides`): all fourteen states, 1,278 start x each, at 0, 53 and 120 columns, the nine holding ones ours called - the hold or free frame and every x outside the handler equal narrow's, the drawn x on or past the wide bound on that frame (3,380,148 frames). The six modules' fuzzes at 0 mismatches at `BOF3X_WIDE` unset, 1 and 2; their control (`BOF3X_SL_CONTROL=1`, the late free) refused by all six at `=1` and by `battle_e7` at `=2`. *Owed:* the state hash at `=1` and `=2` on dragonTransform and cutsceneAndNue against the narrow references (with the inset's `Rand` cause removed), and the owner's eye on the faster slide. The 2026-09-23 note that "a takeover of a `kSlides` site must read its bound from the operand" no longer holds: the operand is Capcom's at every width.

**`docs/README.md`**, a row:

| [`ultrawide-slides.md`](ultrawide-slides.md) | The ultrawide round's faster slide (group SL, 2026-10-13): EB-2's cause 2 fixed by splitting the fourteen slide-off states' x - Capcom's bound and Capcom's x in the record for every reader, a mapped x drawn to the wide bound on the same frame, put back by `Field_RunTaskRecords` after each handler; who reads the x (the state's test, and `MenuList_SlideLeftBesidePanel` on record 4's); a property check over every start at 0 / 53 / 120 and a refused control per module. BUILT; not seen. |

**`docs/STATUS.md`**, the round's line: SL - the slide-off bounds split
(DIV G): the wide modes hold and free the menu windows on Capcom's frames;
the box is drawn to the wide bound on that frame. Headless only.

**`docs/HANDOFF.md`:**
- The state hash on dragonTransform and cutsceneAndNue at `=1` / `=2`
  against the narrow references, with `BOF3X_WIDE_INSET=0` until cause 1
  is fixed: the routes should stay on the narrow route through the slides.
- The owner's eye on the faster slide (a menu with the time and money
  boxes, a shop, a gene screen), and on any slide back in from a parked
  spot (section 6).
- `inject_all.cpp`'s comments at `MenuDrawHelpers_Inject`, `Rest2G_Inject`,
  `Rest2H_Inject` and `Rest2F_Inject` still say those modules read
  DIV-0041's widened bound back; they read Capcom's now (the order no
  longer matters for the slides).
