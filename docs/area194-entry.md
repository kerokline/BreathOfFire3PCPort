# Area 194's first visit: the crafted-save crash

**Status:** MEASURED headless, 2026-10-21, wide-ring round group CR, branch
`wide-ring/cr` off `wide-ring/round` `4b07b9a8`. Not seen by the owner.
Runs in `C:\Users\kerok\bof3x-runs\r_ring\CR` (own game directories
`g1..g4`, launcher copies `L1..L4`, the `main` baseline's binaries in `B`;
`cap.py`, `batch.py`, `gen.py`, `summ.py`, `diff.py`, `sheet.py` there).

## 0. The answer

**The crafted state, not ours and not a defect of Capcom's that play can
reach.** The crash is Capcom's own code doing what it does on an
inconsistent party:

- It reproduces with **0 functions of ours** (`BOF3X_ORIGINAL=*,-Game_Clock`,
  "inject: 0 ours, 10087 left original"): the fault is then in the
  original's `Sprite_DrawPass` at `0x5930B1`, reading the same `0x80903932`
  (pointer `0x80903900`, count `0x3E`).
- It reproduces in `main`'s build (`31249d0c`, before this round) and on
  this branch at `BOF3X_WIDE` 0, 1 and 2, with `BOF3X_WIDE_RING=0`,
  `BOF3X_PILLARBOX=0`, `BOF3X_LAYERING=0` each alone and all three
  together.
- The cause is in the save: CS's donor (`adult_ryu`, chapter 14) has
  already been through the first visit, so its party is the one-member
  party `Scena13_SaveParty7` leaves. The rewound save makes the entry run
  `Scena13_SaveParty7` a second time; its three `Leave` calls take
  `Field_MemberCount` below zero and Capcom's `Leave` (`0x534030`) smears
  the member records over `Sprite_DrawListCount` (section 2).
- With the party restored from the donor's own record of the real
  arrival, the scene loads cleanly - three ways, one of them with the
  scenario's own `Field_ChangeArea` into 194 - in ours and with
  `BOF3X_ORIGINAL=*` (section 3). **The close camera is captured**
  (section 4).

No code change: nothing of ours is at fault, and the state cannot arise in
play (the owner: at that point the party is taken away and nothing but the
scenario enters or leaves area 194).

## 1. The bisect (crafted save `cs194`, regenerated with `chapter_jump.py`)

Recipe `CR\recipes\cr194.txt` (CS's load opening, then a shot every 5
frames from f0400 with peeks of area, mode, camera, run, step,
`Field_MemberCount` `0x929EC0`, party list `0x904062..64`,
`Sprite_DrawListCount` `0x8034F0`, leader x / z `0x802D74` / `0x802D78`).
Every run's result line from `batch.py`, `CR\shots\cr194_<tag>\bof3x.log`:

| Build / switches | Result |
|---|---|
| branch, WIDE 0 / 1 / 2 | crash, `bof3x.dll+0xCF692` (`draw_pass.cpp:245`), reading `0x80903932` |
| branch, WIDE 2, `BOF3X_WIDE_RING=0` | same |
| branch, WIDE 2, `BOF3X_PILLARBOX=0` | same |
| branch, WIDE 2, `BOF3X_LAYERING=0` | same |
| branch, WIDE 0 and 2, all three `=0` | same |
| `main` 31249d0c (baseline worktree), WIDE 0 and 2 | crash, `bof3x.dll+0xC890B` (its `Sprite_DrawPass`), reading `0x80903932`, eax `0x3E` |
| branch, `BOF3X_ORIGINAL=` Sprite_DrawPass, Field_DrawFrame, Scena13_Frame, Scena13_EnterArea, Scena13_Run, Scena13_Run2, Scena13_SaveParty7, Party_Join, PartySet_Load, Area_Enter, Field_ChangeArea (11 left original), WIDE 0 and 2 | crash, `BOF3.exe+0x1930B1` (the original's `Sprite_DrawPass`), reading `0x80903932`, ebx `0x3E` |
| branch, `BOF3X_ORIGINAL=*,-Game_Clock` (0 ours), WIDE 0 and 2 | crash, `BOF3.exe+0x1930B1`, reading `0x80903932` |

(The hosted runs need `bof3x-launcher-dev.exe`; a first attempt with the
release launcher put up its modal refusal and was discarded.)

**Position does not matter.** `cs194p` - the same party-less craft at
run 2's other entry position (`0x3C8000, 0xB0000`, flags `0x81`) - crashes
the same way. `cs194e` - the original position (`0x130000, 0x190000`) with
the party restored - does not. The owner's suspicion of an off-map
position: the placed position is exactly where `Scena13_Run2` step 0's own
`Field_ChangeArea(0xC2, 0x130000, 0x190000, 1)` puts the party, and after a
clean load the leader reads `x 0x130000, z 0x190000` (peeked from f0430).

## 2. Why: the party the craft carries

The peeks before the entry (every crashing run, e.g. `cr194_w0`): party
list `0x904062..64` holds **one member** (two slots `0xFF`) and the saved
party record `0x903A10` (save `+0x30`) is already filled - the donor
passed the first visit, where `Scena13_SaveParty7` (`0x51A5D0`,
`scena_calls.cpp:719`) wrote it. Run on that state again:

1. It saves the lists (one member, two `0xFF`) and calls
   `Leave` (`0x534030`, Capcom's, not taken over) for each of the three.
2. `Leave` ends `dec al; mov [0x929EC0], al` with no floor
   (`0x53418E..0x534190`, `tools/pe_disasm.py 0x534030`): the first call
   takes `Field_MemberCount` 1 -> 0, the second 0 -> `0xFF`.
3. With the count `0xFF` the third call's loops run to 254: the list shift
   at `0x534065` walks bytes past `0x904065`, and the record shift at
   `0x5340AD` copies member record `k+1` over `k` (`rep movsd`, `0x14C`
   bytes, from `0x802D40`) up to ~`0x817600` - across
   `Sprite_DrawListCount` (`0x8034F0`, `0x7B0` past the first record).
4. The count byte now holds a smeared value (`0x3E`); the appenders
   (`sprite_screen.cpp:126`, `field_frame.cpp:447`) see it at or above
   `0x28` and add nothing, and the sort walks entries 40..61 of a 40-entry
   list into `Gfx_OtHeads` (`0x9037C0`), whose words are tagged pointers
   (`0x80903900`): the read of `+0x32` faults.

In real play the first visit begins with the full party (the saved record
in this very donor says so), so step 2 never goes below zero. Restoring the
two lists from the record is the whole repair of the craft.

## 3. Crafts that reach the scene

New crafts in `tools/chapter_jump.py` (`party='saved'` copies save
`+0x30..+0x35`, the record `Scena13_SaveParty7` wrote, to the party lists
`+0x682..+0x687`):

| Craft | Placed | After load (peeked) | Result |
|---|---|---|---|
| `cs194` (CS's) | 194 at `0x130000, 0x190000`, run 2 step 1, one-member party | - (crash on the entry frame) | crash |
| `cs194p` | 194 at `0x3C8000, 0xB0000` flags `0x81`, one-member party | - | crash |
| `cs194e` | 194 at `0x130000, 0x190000`, run 2 step 1, party restored | 194, `x 0x130000 z 0x190000`, members 1 (after the swap) | **clean**, camera `0xF4CA` at f0430 to 0 by f0460 |
| `cs194f` | area **155** at `0x28000, 0x308000` (the step-hook cell that starts run 2: `Scena13_StepHook`, `scena_sc13.cpp` "area != 0x9B"), run 2 step 0, party restored | 155 at the placed cell, members 3; f0460 area 194, `x 0x130000 z 0x190000`, members 1 | **clean**: the scenario's own `Field_ChangeArea` and party swap, camera `0xF277` f0460 to 0 by f0490 |
| `cs194g` | as `cs194f`, donor `ending17` (chapter 17; its own three-member party, as found) | the same, f0460 area 194, members 1 | **clean** |

`cs194f` also loads cleanly with `BOF3X_ORIGINAL=*,-Game_Clock`
(`cr194fc_w2star`, the same camera values frame for frame). Of the
recipe saves, `adult_ryu` is the only chapter-13-or-later save with chapter
13 played and `ending17` the only other later one; none is a save from
before the first visit, so every way in is a craft - `cs194f` is the one
whose arrival the game itself performs. Committed recipe:
`tools/recipes/cs_194f.txt` (reproduces `cr194fc_w2` pixel for pixel, 24
of 24 shots).

## 4. The close camera, captured

`CR\sheets\cr194_close.png`: rows 4:3, 16:9, 16:9 `WIDE_RING=0`, 21:9,
21:9 `WIDE_RING=0`, 21:9 `PILLARBOX=0`, 21:9 both `=0`; columns f0455
(area 155), f0460 (camera `0xF277`), f0465, f0475, f0485, f0500 (0). Shots
in `CR\shots\cr194fc_<tag>`. `CR\diff.py` over f0455..f0510 (10 shots):

- **Ring on against `=0`: identical**, 10 of 10 at 16:9 and at 21:9, and
  with the bars off (`w2off` against `w2both0`). The close camera is
  inside Capcom's ring at both widths.
- **Bars:** area 194 is framed at 426 (`pillarbox area 194: ... framed at
  53 columns a side`, bars on from frame 458). Inside the frame the 21:9
  picture is identical with and without bars (columns 134..985, at
  each shot checked: f0460, f0465, f0475, f0485, f0500); the bands under the bars are black. What they hide: at f0465 the
  close camera's **character sprite at the fire** runs past the left bar
  (6,605 lit pixels in the left band, the figure's back cut at x 134); from
  f0475 the bands hold the camp's ground and a tent (about 60,000 lit
  pixels a side), as CS found for the field (narrow-pillarbox.md 10).
- 4:3 against `BOF3X_ORIGINAL=*` differs everywhere (the PC's own filter
  and drawing with nothing of ours) - not a comparison of the camera.

## 5. Questions for the owner

1. **The bars cut a character in the close camera** (f0465: the seated
   figure at the fire, cut at the left bar; `sheets\cr194_close.png`, rows
   21:9 against 21:9 `PILLARBOX=0`). *Recommendation:* the same as
   narrow-pillarbox.md 10.1 question 3 - leave the camps (194 among them)
   unframed at 560; what the bars hide here is a character as well as
   scenery.
2. **Guard Capcom's `Leave` against a party already empty?** Not
   reachable in play; only a craft gets there. *Recommendation:* no -
   leave Capcom's behaviour (and the ledger) alone; the crafts now restore
   the party instead.
