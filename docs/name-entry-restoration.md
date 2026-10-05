# Name entry: what the PlayStation has, what the PC stubbed, what bringing it back would take

**Status:** IN PROGRESS (verified 2026-10-05). The research is complete. **The
restoration is deferred to the localisation rework, by the owner's word of
2026-10-05**: that rework may change the structure of the font files, and the
font, encoding and glyph-grid choices all depend on it (section 7). Nothing is
built and nothing here is scheduled. Read-only work: no build and no game run.
Every PSX reading is capstone over the sibling's overlay captures
(`../BreathOfFire3Recomp/analysis/overlay_captures_all.json`, the bytes the
PSX loads). Every PC reading is capstone over `bof3/BOF3.exe` plus
`analysis/pc_xref.json`. Section 10 lists the scratch scripts. No byte, string
or table from either game is reproduced here: only addresses, sizes, codes and
shapes.

The owner asked for the name-entry screen to come back from the PlayStation
game, at least as an option. This doc records what that screen is in the PSX
code, where it is, what the PC port did to it, and what the sibling repo
already holds. It also gives the routine's skeleton in enough detail that
someone can write it later without repeating the search.

## 1. The answer in brief

- **The PSX has two copies of one name-entry routine, and both are in
  overlays, not in the resident executable.** One is in **`COMMU02.EMI`** (the
  community band's renamer, the code R4D just took over on the PC). The other
  is in **`START.EMI`** (the front end, used at New Game). The 288-byte glyph
  grid is byte-identical in both: `COMMU02` `0x801DACA4` and `START`
  `0x801ECD2C`. No other image holds it: the sibling's 406 overlay captures,
  `SLPS_009.90` and `BOF3.exe` were searched (section 10, `gridsearch.py`).
- **In the community band the PC kept the callers and stubbed three callees.**
  The grid drawer `0x801D94C8` and the controls legend `0x801D9810` became
  calls to `BareRet` `0x437CC0`, which still pass the PSX's arguments. The
  input step `0x801DA1D8` became `BareRetZero` `0x43C9F0`. The tiled frame
  around the grid, `0x801D9C44`, survives as the PC's `Commu_DrawTiledFrame`
  `0x45F1A0`.
- **At New Game the PC has no stub, because the whole path is gone.**
  `TitleFlow_NewGame` `0x5880E0` goes straight to `NewGame_InitCharacters`
  and the scenario. PSX `START`'s name-entry sub-machine has no PC
  counterpart and no call site.
- **Nothing of the routine's data survives in `BOF3.exe`.** There is no grid
  in any cell width, no legend strings and no special-cell labels
  (section 5.2).
- **Correction to R4D's L3.** On the PC an entry does *not* end in message
  0xF7. The PSX advances the entry's step from inside the input step that the
  PC replaced with `BareRetZero`. The PC's `CommuName_SlotEntry` `0x45D730` and
  `CommuName_MemberEntry` `0x45E2C0` never advance it, so a PC game that reached
  step 1.7.1 or 2.7.1 would stay there. It would draw the panel and an empty
  frame, with no input, every frame (section 5.1). Whether play can reach that
  step on the PC is the owner's check.
- **A false lead in the sibling.** The name-entry passages of
  `psxrecomp/docs/STRING_TRANSLATION.md` (lines ~635-850: the glyph grid, the
  drawer `0x8005443C`, the prompt at `0x80071360`, "Latin input is native") are
  about **Tsumu Light (SLPS-02253)**, the recompiler's first consumer
  (that file's line 4). They are not about BoF3, and none of those addresses
  applies here (section 6).

## 2. Where the routine lives on the PSX

| Copy | Overlay (sibling capture md5, load address) | Grid table | Drawer | Legend | Frame | Input step | Reached from |
|---|---|---|---|---|---|---|---|
| Community | `COMMU02.EMI` (`5434a7fa...`, `0x801D0C00`, 42,656 B) | `0x801DACA4` | `0x801D94C8` | `0x801D9810` | `0x801D9C44` | `0x801DA1D8` | `CommuName` entry steps (section 3) |
| New Game | `START.EMI` (`b8cc1561...`, `0x801D0C00`, 117,858 B) | `0x801ECD2C` | `0x801E6418` | `0x801E675C` | `0x801DF56C` | `0x801E459C` | the front end's New Game sub-machine (section 4.6) |

How the copies were placed:

- **Grid.** `gridsearch.py` takes `COMMU02`'s 288 bytes at `0x801DACA4`
  (three pages of 96) and finds each page whole in exactly two images:
  `COMMU02` and `START` at `0x801ECD2C` / `0x801ECD8C` / `0x801ECDEC`.
- **`COMMU02` functions.** These come from the `CommuName` state tables at
  `0x801DAFA0`, a run of 39 code pointers (`psxd.py 5434a7fa ptrs`). The run
  splits into the PC's tables entry for entry: 5 states, 9 slot steps, 3 + 5
  slot sub-steps, 9 member steps, then 3 + 5 member sub-steps (`CommuName_States`
  `0x652E48` and its neighbours, R4D section 2).
  `psxd.py 5434a7fa dis 0x801d6608 0x801d6aa0` then reads the slot-entry steps.
  The stubbed callees are the targets of the `jal`s that sit where the PC calls
  `BareRet`. Each target's callers are exactly the slot and member entry steps
  (`psxd.py 5434a7fa callers`): six sites each for `0x801D94C8` and
  `0x801D9810`, and two for `0x801DA1D8`.
- **`START` functions.** Four `addiu` operands form `0x801ECD2C`: one in
  `0x801E6418` (the drawer) and three in `0x801E459C` (the input step). The
  draw-all `0x801E6F80` calls the drawer, the legend and the frame
  (`psxd.py START.EMI jals 0x801e6f80 0x801e70dc`).

Function extents are the sibling's discovered starts for each capture
(`psxd.py <md5> starts`). Two jump tables sit inside the functions:
`0x801D0C64` (5 entries) in the drawer and `0x801D0C7C` (5 entries) in the
input step. They are cases, not starts.

## 3. What the PC stubbed: the call sites against the PSX

`python pcd.py dis 0x45D730 0x45D7C0` (PC) against
`psxd.py 5434a7fa dis 0x801d67c0 0x801d6884` (PSX), for one step. The same
three calls appear in all six entry steps on both sides.

| PC step (R4D) | PSX step | PSX callee | PC site | Arguments, PSX (a0..a3, stack) = PC (cdecl) |
|---|---|---|---|---|
| `CommuName_SlotEntryIn` `0x45D610`, `_SlotEntry` `0x45D730`, `_SlotEntryOut` `0x45D7C0` | `0x801D6608`, `0x801D67C0`, `0x801D6884` | legend `0x801D9810` | `BareRet` | (page, `0xB0` + 30 * slide, `0x2B`) |
| same | same | frame `0x801D9C44` | `0x45F1A0`, **kept** | (`0x12`, `0x61` + 30 * slide, `0x24`, `0x11`) |
| same | same | grid `0x801D94C8` | `BareRet` | (page, `0x18`, `0x67` + 30 * slide, column, row) |
| `_SlotEntry` only | `0x801D67C0` | input `0x801DA1D8` | `BareRetZero` | () -> answer byte |
| `CommuName_MemberEntryIn` `0x45E190`, `_MemberEntry` `0x45E2C0`, `_MemberEntryOut` `0x45E350` | `0x801D7690`, `0x801D7868`, `0x801D792C` | the same four | the same | the same |

In `SlotEntry` and `MemberEntry` the slide term is 0 and the step adds no
offsets. In the PSX's member steps the legend and frame sites are
`0x801D76FC` / `0x801D7724`, `0x801D78C8` / `0x801D78DC` and `0x801D79E4` /
`0x801D7A0C`, and the grid and input sites are `0x801D7760`, `0x801D7904`,
`0x801D7A48` and `0x801D790C`.

**The cells, PSX against PC.** R4D named the three entry bytes `kEntryA/B/C`
without saying which is which. The argument positions settle it. The PC pushes
`0x675FCC` where the PSX passes `a3`, and `0x675FCA` where it passes the stack
word. Inside `0x801DA1D8`, `a3`'s cell wraps at 12 and the stack word's cell
wraps at 8:

| PSX (`COMMU02`) | PC | Role, from the input step and the drawer |
|---|---|---|
| `0x801DB23C` (5 B) | `0x675F98` (5 B) | edit buffer; `0xFF` = blank cell on the PSX |
| `0x801DB270` | `0x675F8C` | list cursor (which slot or member) |
| `0x801DB280` | `0x675F95` | slide counter |
| `0x801DB28C` | `0x675F94` | name cursor: position 0..4 in the buffer; the grey underline at `0x4A` + 12 * it |
| `0x801DB290` | `0x675FCC` | grid column 0..11 |
| `0x801DB294` | `0x675FCA` | grid row 0..7 |
| `0x801DB298` | `0x675FCB` | grid page 0..2 |
| `0x801DB29C` | `0x675F96` | the answer: 1 = Start pressed, 0 = abandoned (or, on the PC, never set) |
| `0x801F293C` / `0x801F293D` / `0x801F293E` | `0x939A3E` / `0x939A40` / `0x939A3F` | state / step / sub-step (from the stores the unanswered branch makes on both sides) |
| `0x8014494E` | `0x903A5A` | the window-colour byte (the save block's delta `0x807BF10C`) |

The PSX keeps the entry state as separate bytes four apart. The PC packs the
three grid bytes into `0x675FCA..CC`.

**One change the port made in the surviving code.** At entry, the PSX copies
the slot's or member's five name bytes and turns a 0 byte into **`0xFF`**
(`0x801D6728`: `beqz` with `0xFF` in the delay slot). That is the blank marker
both the input step and the Start trim test for. The PC's `SlotEntryIn` /
`MemberEntryIn` turn it into **`0x20`** (R4D's code, "0 read as a space"). This
agrees with `save-interchange.md` section 2a, where the PC's commits are
space-padded: the port edited this path for its own encoding. It did not only
cut it.

## 4. The routine's skeleton (PSX, read to the last instruction)

The `COMMU02` copy is described first. Section 4.6 gives what differs in
`START`. Names here are this doc's labels. They are **hypotheses** under
README's evidence tiers, and the sibling names none of these functions
(section 6).

### 4.1 `NameGrid_Draw(page, x, y, column, row)`: `0x801D94C8..0x801D980F`

- A local copy of a 0x1C-byte block of short labels at `0x801D0C44` (overlay
  data) goes onto the stack.
- `Menu_DrawBox` (PSX `0x801AF3F0`, PC `0x57CF60`, a pair in `symbols.toml`):
  (x, y, `0x110`, `0x7B`, 0, window colour).
- Cell bytes are `grid + 96 * page + 12 * row + column`. One byte per cell, at
  `0x801DACA4`.
- For row 0..7, at y + 6 + 14 * row, and for column 0..11, from x + 4:
  - Cells in columns 0..9, and in columns 10..11 of rows 0..2, are glyphs. The
    drawer calls `Text_DrawAt` (PSX `0x8014F6BC`, PC `0x516B30`) with
    (cx, cy, 0, 1, &cell): one byte at a time, so a 0 cell draws nothing.
  - Column 10 of rows 3..7 is a **special cell**, code 1..5, reached through
    the jump table `0x801D0C64`. It draws a label from the local block with
    `Text_DrawAt` (cx, cy, 4 or 2, 4, label). Codes 1, 2 and 4 take the block's
    +0, +4 and +8, code 5 takes +0xC, and code 3 takes **+0x10 + 4 * page**,
    so its label names the page. Codes 1..3 pass 4 and codes 4..5 pass 2 in the
    third argument, which is the colour in the PC's parameter order. The PSX
    side's parameter order is not checked. Column 11 of those rows holds the
    same code and draws nothing: the label spans both columns.
  - x advances by `0x14`, or by `0x1C` after columns 4 and 9, which leaves
    gaps between three blocks of columns.
- The hand is drawn by `Menu_DrawHand` (PSX `0x801651B4`, PC `0x5905D0`) at
  (x + 6 + 20 * column, y + 8 + 14 * row). The drawer adds 8 when column >= 5
  and 8 more when column >= 10, and takes `0x14` off for column 11 in rows
  3..7, so the hand sits on the label.

**The grid's shape** (`gridsearch.py`, counts only). On every page the special
cells sit at columns 10..11 of rows 3..7 as codes 1, 2, 3, 4, 5, one per row.
Pages 0 and 1 have no empty cells: 86 glyph cells each. Page 2 has 42 glyph
cells and 44 empty ones: rows 4..7 of columns 0..9, plus row 2's columns 6..9.
The input step's page-2 clamps (section 4.4) exist because of those holes.
What the glyphs are is not stated here. The owner can see it on the PSX
screen, and the bytes come from the disc at run time (section 7).

### 4.2 `NameGrid_DrawLegend(page, x, y)`: `0x801D9810..0x801D9B8B`

- `Menu_DrawBox` (x + 3, y + 3, `0x4D`, `0x2F`, 0, window colour).
- Strings come through an 8-pointer table at `0x801DB1F4`, into overlay
  strings at `0x801DB1C4..`, drawn with `Text_DrawSmall` (PSX `0x8014FD78`, PC
  `0x516E70`) at count `0x10`:
  - entries 0..2 at (x + `0x2A`, y + 6 + 8i)
  - entry 3 at (x + `0x2A`, y + `0x26`)
  - entry **4 + page** at (x + `0x2A`, y + `0x1E`)
  - entry 7, one byte drawn 5 x 3 times at (x + `0x1A` + 4j, y + 6 + 8i): a
    row of leader marks
- Six 8 x 8 button icons go through the helper `0x801D9B8C` (x, y, u-tile,
  v-tile, CLUT, shade). The helper builds one `SPRT_8` (u-tile * 8,
  v-tile * 8, shade `0x7F`, semi-transparency off) and commits it with
  `Packet_Commit(1, 0x10)`:
  - tiles (`0x1B`,0), (`0x1D`,0), (`0x1E`,0) and (`0x1C`,0) at
    (x + `0xE`, y + 6 / `0xE` / `0x16` / `0x1E`), CLUT `GetClut(0x30, 0x1E0)`
  - tiles (4,2) and (5,2) at (x + `0xA` / `0x12`, y + `0x26`), CLUT
    `GetClut(0, 0x1E0)`
  - before them, a draw mode whose texture page depends on `GetGraphType`
- The frame is drawn by `Menu_DrawPieces` (PSX `0x801B0390`, PC `0x57D910`)
  (x, y, the piece list `0x801DB158`, 0).

`analysis/pairs_propagated.json` pairs `0x801D9810` with PC `0x585DC0`
("callers", a hypothesis). **The reading refutes it.** `0x585DC0` is
`MasterPanel_DrawMember` (R2C), which draws a portrait, a level and HP/AP, not
a legend. The two share callees, nothing more.

### 4.3 The frame `0x801D9C44` = PC `Commu_DrawTiledFrame` `0x45F1A0`

The PC already has this frame (R4E, ours): five windowed SPRTs and four corner
pieces. With (`0x12`, `0x61`, `0x24`, `0x11`) it surrounds the grid box at
(`0x18`, `0x67`). Note that `0x24` x 8 = 288 and `0x11` x 8 = 136, against the
box's `0x110` x `0x7B`; the two numbers are tile counts. A restoration reuses
this function.

### 4.4 `NameGrid_Input()` -> answer: `0x801DA1D8..0x801DA99F`

This step is called once a frame from the entry's middle step, and its result
goes to `0x801DB29C`. It owns the step advance. Bits are tested against the
game's pad words: the repeated word comes from the auto-repeat `0x8014E534`
(PC `Input_AutoRepeat` `0x461EB0`) over `0x80145AA4 & 0xF00F`, and the button
tests read `0x80145AA4` itself. The PC's twin word is `0x7E1BEC`: PSX
`0x801D5F38` and PC `0x45D170` read them at the same point of the slot chooser.
Which physical button each bit is follows the standard PSX pad layout. That
layout was not measured here, so the names in brackets are that layout's.

1. **Moving.** One of these per frame, each played with `SE_Play(0x100)`
   (`0x8015E908`):
   - **Up** (`0x1000`): row - 1, wrapping 0 -> 7, repeated while the cell is
     empty (0).
   - **Down** (`0x4000`): row + 1, wrapping at 8, skipping empty cells.
   - **Left** (`0x8000`): from a special cell, the column becomes 9. Otherwise
     column - 1, wrapping 0 -> 11, skipping empty cells.
   - **Right** (`0x2000`): from a special cell, the column becomes 0.
     Otherwise column + 1, wrapping at 12, skipping empty cells.
2. **Page-2 clamp.** On page 2, row >= 4 with column < 10 becomes row 3.
3. **Confirm** (`0x20`, Circle), on the cell under the hand:
   - **Glyph cell**: `SE_Play(0x103)`, the byte goes into `buffer[name
     cursor]`, and the name cursor advances if it is < 4.
   - **Special code 1**: name cursor + 1 if < 4 (`0x103`), else `0x107`.
   - **Code 2**: name cursor - 1 if > 0 (`0x103`), else `0x107`.
   - **Code 3**: ORs `0x10` into the pad word, so the triangle branch below
     changes the page.
   - **Code 4**: the sub-step advances and the step **returns 0**: the entry is
     abandoned and the caller takes its unanswered branch (message 0xF7,
     state 4).
   - **Code 5**: ORs `0x800` into the pad word, so the Start branch below
     finishes.
4. **Then exactly one of these**, in this order:
   - **`0x10`** (Triangle): `0x103`, then page = (page + 1) mod 3. On page 2,
     column < 10 with row >= 4 becomes row 3, and column 6..9 with row 2
     becomes row 1. Returns 0.
   - **`0x80`** (Square), or repeated **`0xA`** (R1/R2): name cursor + 1 if
     < 4, with `0x101`.
   - Repeated **`0x5`** (L1/L2): name cursor - 1 if > 0, with `0x101`.
   - **`0x40`** (Cross), backspace: if the name cursor is not 0 and
     `buffer[cursor]` is `0xFF` or 0, the cursor steps back first. Then `0x106`
     and `buffer[cursor] = 0xFF`.
   - **`0x800`** (Start), finish: `0x104`. If not every byte is `0xFF`, the
     trailing `0xFF`s are turned into 0. The sub-step advances and the step
     **returns 1**. An all-blank name stays `0xFF`s and is replaced by the
     caller's default (section 4.5).
5. Otherwise it returns 0 without advancing.

It keeps no state of its own. Everything is in the cells of section 3 and the
pad word, which it writes back for codes 3 and 5.

### 4.5 The callers' contract (`COMMU02`; the PC's are ours already)

- **Entry-in** (sub-step 0): slides in. At slide 0 it copies the target's five
  name bytes into the buffer (0 -> `0xFF`), zeroes the name cursor, column,
  row and page, and advances the sub-step. Slot names are PSX `0x801457E4` +
  5 * slot (PC `0x9048F0`). Member names are record +0, `0x80144964` + `0xA4`
  * member (PC `0x903A70`), five bytes on the PSX.
- **Entry** (sub-step 1): panel, underline, the three draws, then
  `answer = NameGrid_Input()`. **It does not advance the sub-step itself.**
- **Entry-out** (sub-step 2): slides out. At 4, when answered, an empty name
  takes a default: slots from `0x801F26F0` + **9** * slot (PC `0x653200` +
  **20** * slot), and members from the roster table `0x801DB214`, 5 bytes each
  (PC `CommuName_RecordNames` `0x669FA4`). Then message 0xF4 and the sub-step
  advances. When unanswered: message 0xF7, state 4.
- **The commits** are PSX `0x801DAAE4` (slot) and `0x801DA9A0` (member), PC
  `0x45F650` and `0x45F5A0` (R4E). Each copies the old name to the text insert
  record (PSX `0x801490D4`), writes the buffer into the target, and opens
  message 0xF5. The slot commit stores a 0 buffer byte as `0xFF`, and the
  member commit writes five bytes as they are. The PC writes 5 and 8 bytes respectively
  (`save-interchange.md` 2a).

### 4.6 The New Game copy in `START.EMI`

- **State tables.** The front end's step table is a 33-pointer run at
  `0x801ED078`. Its first five entries are the top-level steps, and the fifth,
  `0x801E6228`, is paired with PC `TitleFlow_EnterGame` `0x588800`. The third,
  `0x801E4490`, is the New Game sub-machine. It jumps through `0x801ED0A8` by
  the sub-step byte `0x801ED7E0`, then calls `0x8014B948`.
- **Name screen sub-steps:**
  - `0x801E4430` waits.
  - `0x801E44D4` opens the screen. It calls `0x801D15F4`, stores the buffer
    address `0x801ED070` at `0x801ED758`, zeroes the cells, sets `0x8014832A`
    to 1, draws, fades in through `0x8014EB68(1)`, and advances.
  - `0x801E4550` draws until the word `0x80143C40` is 0, then advances.
  - `0x801E459C` is the input step.
- **Cells.** Buffer `0x801ED070`, name cursor `0x801ED75C`, column
  `0x801ED760`, row `0x801ED764`, page `0x801ED768`.
- **Input `0x801E459C`.** It has the same skeleton as section 4.4, with its own
  cells and some differences:
  - it draws first, through the draw-all `0x801E6F80`
  - Start plays `0x105`
  - special code 4 sets the front end's mode word `0x80143B92` to 1 and the
    sub-step to 0 (where that leads was not traced)
  - finishing with an all-blank name copies **character record 0's name**
    (`0x80144964`) back into the buffer
  - it then writes the five bytes into record 0's name, fades out with
    `0x8014EB68(0)`, and advances
  - it names only record 0
- **Draw-all `0x801E6F80`.** Its draws, in order:
  - the title box (`0x801DDF28`: `0x14`, `0x12`, `0x118`, `0x13`)
  - system message `0xBA` or `0xBB`, chosen by bit `0x40` of `0x80143E6C`,
    through `Msg_SystemPtr` and `Text_DrawAt` at (`0x1C`, `0x15`)
  - `0x801E6308(0x20, 0x2B)`, a name panel (not read)
  - the legend `0x801E675C`(page, `0xA8`, `0x2B`)
  - the frame `0x801DF56C`(`0x12`, `0x61`, `0x24`, `0x11`)
  - the grid `0x801E6418`(page, `0x18`, `0x67`); it takes three arguments
    because this copy reads column and row from its own cells
  - then it animates an actor through the scratchpad pointer `0x1F800044` and
    `0x8014D5AC` / `0x8014D3D4` / `Actor_AnimTick`

  The sibling's live capture puts the New Game screen's text in the system
  pool at `0x80014000` (`docs/TEXT_ENGINE.md` 350-407).

## 5. What survives on the PC

### 5.1 The community entry: callers kept, callees cut, and a hang

R4D took over all six entry steps and R4E the frame and both commits, so every
caller is ours. Missing are the three callees of section 3. Without the input
step, nothing advances the sub-step:

- **The writers of `0x939A3F` in the band `0x45D000..0x45F800`**
  (`pc_xref.json`): 30 references. None falls in `0x45D730..0x45D7B1` or
  `0x45E2C0..0x45E34F`. `pcd.py dis 0x45D730 0x45D7C0` ends at the `ret` after
  the store to `0x675F96`.
- The other writers of `0x939A3F` are area 175's handlers (`0x429B1C`,
  `0x429B32`), `0x456DD1` and the other community games' states. None was
  shown to run while sub-step 1 is on screen.

So R4D L3's "an entry always ends in message 0xF7" holds only for
`SlotEntryOut` / `MemberEntryOut` once they are reached, and they are not
reached. What the code shows: if play reaches sub-step 1.7.1 or 2.7.1, the game
draws the panel, the underline and the empty frame each frame, takes no input,
and never leaves. Whether play reaches it depends on `0x939A3C`, the "how"
byte `CommuName_SlotHow` / `_MemberHow` test (6 = random, 7 = enter). It is
written by message choices 23 and 28 of areas 175..185 (`0x429270`,
`0x429D70`; `docs/area_w4c.md`, `area_w4d.md`) and by `0x456DDB`. Whether the
PC's messages still offer that choice is the owner's check (section 8).

### 5.2 Data: gone

| Searched for in `BOF3.exe` | Result |
|---|---|
| the 288-byte grid, and each 96-byte page | absent (`gridsearch.py`, `labels.py`) |
| any 8 x 12 block with codes 1..5 at columns 10..11 of rows 3..7, as 1-, 2- or 4-byte cells, either byte order | **0 hits** over the whole file (`pcgrid.py`) |
| the legend's eight strings | absent; three short ones (1..4 bytes) match by chance |

The port's own encoding would not hold the PSX bytes anyway, so this says
only that no PC-encoded grid of the same shape survives either.

### 5.3 New Game: no remnant

`TitleFlow_Step` `0x587DB0` jumps through `0x6671F4`: Begin, Menu, NewGame,
Load, config `0x460CB0`, EnterGame. The PSX's NewGame step is a sub-machine
with the name screen in it. The PC's `TitleFlow_NewGame` `0x5880E0` (0x48
bytes, `symbols.toml`) calls `NewGame_InitCharacters`, clears state, starts
scenario 0 and moves on. No `BareRet` call and no sub-step byte survive.
DIV-0020's statement that the port has no name entry at New Game is confirmed
at the code level: nothing here can be re-armed, and New Game would need a new
step.

## 6. What the sibling already has

| What | Where | Bearing |
|---|---|---|
| The New Game name screen, observed live: its text sits in the system pool block `0x80014000`, not the area script | `docs/TEXT_ENGINE.md` 350-407 | where the screen's messages come from on PSX |
| Reaching it in Mednafen: New Game then Circle; it cannot be cancelled (Cross only deletes) | `docs/MEDNAFEN.md` 77-81; `docs/SAVESTATES.md` row 7 (Down + Circle from `slot06` lands on it) | a way to watch the PSX behaviour this doc reads |
| The roster default names, `COMMU02` `0x801DB214`, 5-byte stride, 7 entries | `docs/TEXT_TABLES.md` 51, `docs/IDEAS.md` 451, `names/characters.toml` | the member entry's defaults (section 4.5) |
| New-game character templates, `START` `0x801EB4A4` | `docs/TEXT_TABLES.md` 52 | the record whose name the New Game copy writes |
| `Msg_OpenScript`, `Msg_SystemPtr`, `SE_Play`, `Packet_Commit` and the Psy-Q GPU calls | `symbols.toml` | the callees, named |
| Overlays `COMMU02` / `START` | `names/overlays.toml` (`COMMU02` status `unnamed`) | no function in either is named in `names/functions.toml` |
| **Not BoF3**: the glyph grid, drawer `0x8005443C`, prompt `0x80071360`, "Latin input is native" | `psxrecomp/docs/STRING_TRANSLATION.md` 630-860 | Tsumu Light (line 4: "the first consumer"); its boot EXE is `SLPS_022.53`, not BoF3's `SLPS_009.90` |

The sibling has no decompiled or named body for any function in section 4. Its
Ghidra project and `analysis/functions.tsv` cover the boot EXE, and the
overlay functions here were read straight from the captures.

## 7. What a restoration would need (deferred; short)

By the owner's word of 2026-10-05, the restoration waits for the localisation
rework. These are the pieces. The choices marked *dependent* wait on that
rework.

- **Code**, reimplemented from section 4. Nothing is vendored.
  - **Community entry:** the grid drawer, the legend and the input step,
    replacing the two `BareRet` calls and the `BareRetZero` call in R4D's
    `EntryDraws` / `DroppedZero`. `Commu_DrawTiledFrame`, the six steps and the
    two commits are ours already.
  - **New Game, a separate optional step:** a new sub-step in or after
    `TitleFlow_NewGame` that runs the same three functions on character record
    0 and opens the screen as `START` does. The PC has no hook to re-arm.
- **Data from the player's own files at run time, never committed.**
  - The grid, the special-cell labels, the legend strings and pointer order,
    and the roster defaults come from the user's disc (`COMMU02.EMI` /
    `START.EMI`), the way `tools/loc_build.py` builds the language overlays.
  - The system-pool messages come from the same source.
  - Or a localisation-owned replacement grid. *Dependent.*
- **Encoding.** *Dependent.*
  - The PSX grid is one byte per cell, and the PSX names are five one-byte
    glyphs with `0xFF` as blank.
  - PC character names are 8 bytes: four two-byte codes, field 9 bytes. PC slot
    names are 5 bytes. The PC's surviving entry code already uses `0x20` as
    blank.
  - The English / French / German overlays are one-byte Latin.
  - Which grid, which blank byte, and how many glyphs fit in 5 and 8 bytes all
    follow from the rework's text and font structure.
- **Font.** *Dependent.* The PSX draws cells with `Text_DrawAt` and the legend
  with `Text_DrawSmall`. The PC has both (`0x516B30`, `0x516E70`;
  `docs/dialogue-localisation.md`, `menu_draw_helpers.md`), so which font and
  which glyph table draw a cell is the rework's question.
- **Input.** The PC already has the pad words and `Input_AutoRepeat` with the
  same bit layout as the PSX (section 4.4). What the legend's six icons should
  show on a keyboard or modern pad is a presentation choice.
- **Widescreen.** The grid box is fixed at (`0x18`, `0x67`), `0x110` wide, as
  are the legend and the frame. They need whatever the menu draws already get
  under `BOF3X_WIDE`.
- **The ledger.** Wiring the calls in is a deliberate divergence and wants a
  `DIVERGENCE.md` entry when it is built. Fixing the hang of section 5.1 is a
  separate question for the owner, and is not covered by that entry.

## 8. For the owner

1. **Can play reach the PC's entry sub-step?** The check is in the Faerie
   Village renamer (USER_CHECKS item 4): does the PC offer "enter a name" as
   well as "random"? If it does and you pick it, section 5.1 predicts a screen
   that never closes. Keep a save from before you try.
2. **New Game naming:** whether to offer it at all, and whether for Ryu only as
   on the PSX (record 0) or for others.
3. **Which glyphs the grid should offer per language** once the rework sets the
   font. On the PSX the three pages are fixed: two full, one with 42 cells.
4. **The hang.** Leave it as Capcom's port has it (ledgered as a defect when
   the entry comes back), or close it sooner.

## 9. Not established

- **What `START` does after special code 4.** It sets mode word `0x80143B92` to
  1; the next step was not traced. Nor was whether `START`'s input step differs
  from `COMMU02`'s in anything beyond the points listed in 4.6. Each was read
  at its grid references and its end only.
- **`0x801E6308`**, the New Game screen's name panel, was not read.
- **The PSX parameter order of `Text_DrawAt` / `Text_DrawSmall`.** It is
  assumed from the PC pairs.
- **Which physical buttons the bits are, on both sides**: the standard PSX
  layout, not measured.
- **Whether any writer of `0x939A3F` outside the entry steps runs while
  sub-step 1 is shown** (section 5.1).
- **What the labels, legend lines and grid glyphs say.** Not decoded, on
  purpose (rule 1; and no gameplay facts from memory).

## 10. Reproducing (scratch scripts, not committed)

The scripts lived in the session scratchpad
(`.../scratchpad/name_entry/`). They read the sibling's captures and
`bof3/BOF3.exe`, and print addresses, counts and disassembly only.

- **`psxd.py <md5-prefix|file-suffix> dis|jals|callers|ptrs|starts`**: capstone
  MIPS over one overlay capture, decoding word by word so data does not stop
  it.
- **`pcd.py dis|calls|find`**: capstone x86 over `BOF3.exe` by VA.
- **`pair.py <addr..>`**: `analysis/pairs_propagated.json` lookups with the
  overlay's file name.
- **`xr.py <addr..>`**: `analysis/pc_xref.json` lookups. `tools/pe_xref.py`
  does the same from the main checkout.
- **`gridsearch.py`**: the grid's per-page shape counts, and where each page
  appears.
- **`pcgrid.py`**: the shape search over `BOF3.exe`.
- **`labels.py`**: whether the legend strings and grid occur in `BOF3.exe`.

The main commands:

- `psxd.py 5434a7fa ptrs` gives the state tables.
- `psxd.py 5434a7fa dis 0x801d94c8 0x801db000` covers sections 4.1 to 4.5.
- `psxd.py START.EMI dis 0x801e4430 0x801e4e0c` and
  `psxd.py START.EMI dis 0x801e6f80 0x801e70dc` cover section 4.6.
- `pcd.py dis 0x45D730 0x45D7C0` covers section 5.1.
