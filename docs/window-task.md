# The window/task layer, and the area change above it

**Status:** IN PROGRESS (2026-09-22 — twenty-seven functions ours, fuzzed
headless; through the live batch `ab24`)

**The live batch, 2026-09-22 (`ab24`, `analysis/validate_ab24.sh`):** the whole third round - groups H, J, K, L and M, 137 functions, 466 ours - checked at once, original against ours: the field, new-game, field-menu and menu-screens capture pairs identical (4, 9, 7, 5 of each), the 9-minute attract 55 of 55 (and 55 of 55 against `ab22`'s ours), the same attract in English 55 of 55, the oracle identical at all 7,478 compared frames, the memory dump identical, and the frame hash identical on all 10,063 frames (`ab24_orig` / `ab24_oursb`, beside an original-vs-original pair identical on 10,062).

The layer that drives every window record a frame: `Field_RunTaskRecords`
`0x59E230` walks the 22 records at `0x803160` in three passes, and the first
of its nine handlers is the window proper — three kinds, five states each,
the box opening and closing on its own centre, and the six primitives an open
window draws. With it, the area-change funnel that sits immediately above it
in the image: `Field_ChangeArea` `0x594E00` and the three lookups it uses.

Group J of the third parallel round. Everything here is a *faithful*
replacement: no `DIVERGENCE.md` entry is owed.

## 1. What was wrong before this

`symbols.toml` carried, since 2026-09-18, a `hypothesis` that `0x595450` was
the PSX's `Window_Task` `0x80159F00`, with the note that it was "613
instructions against a small PSX function". **It is not that function and it
is not 613 instructions.** `tools/pe_funcs.py` had folded fourteen
pointer-reached functions into one body:

    entries.txt / entries_logic.txt:   00595450 7F9
    measured (capstone, 2026-09-22):   00595450 53

The 0x7F9 run covers `0x595450..0x595C48` — the lookup itself plus the whole
window layer. Nothing between `0x5954B0` and `0x595C10` was in `entries.txt`,
in `entries_logic.txt`, or in `analysis/pc_hidden.json`; they are reached only
through stack-built call tables, which `tools/pe_hidden.py` cannot see either
(it looks for pointers in data). `0x595450` itself turns out to be
`Area_ZoneAt` `0x52FFD0` with the area as an argument — a zone lookup, not a
task.

The entry has been withdrawn and replaced. Twelve of the twenty-seven sizes in
the catalogue were the alignment padding rather than the body; the table in
§2 has the measured extents, and §7 has the block for `entries_logic.txt`.

## 2. The functions

Sizes are the measured extent of the body, by capstone, 2026-09-22; call
counts are `analysis/calltrace/hidden_b/bof3x.callcounts.tsv`. PSX addresses
are `analysis/pairs_propagated.json` unless the note says otherwise.

| PC | bytes | catalogue said | PSX | name | what it does |
|---|--:|--:|---|---|---|
| `0x594E00` | 0x52 | 82 | `0x801A0A30` | `Field_ChangeArea` | stash the pending area, position and flags; classify; pick the music; `Field_Request = 5` |
| `0x595350` | 0x33 | 51 | `0x801A12F0` | `Area_ClassifyPending` | `0x937F98` = 0xB for eleven areas, 1 for the rest |
| `0x595390` | 0x1D | 29 | `0x801A1350` | `Area_ZoneIdAt` | the zone record's byte +5 at (x, z) in the current area |
| `0x5953B0` | 0x98 | 152 | `0x801A1388` | `Area_PickMusic` | the track the new area wants, into `0x904CD0` |
| `0x595450` | **0x53** | 96 / 0x7F9 | `0x801A1498` | `Area_ZoneAtIn` | the zone record for (x, z) in a given area |
| `0x5954B0` | **0x2E** | 48 | — | `Window_Run` | handler 0 of the nine: dispatch on record byte +2 |
| `0x5954E0` | **0x3E** | 64 | — | `Window_Kind0States` | kind 0's five states, by record byte +3 |
| `0x595520` | **0x14D** | 336 | — | `Window_Kind0Open` | state 0: the placement code, the box, the step |
| `0x595670` | **0x1A3** | 432 | — | `Window_Grow` | state 1 of all three kinds |
| `0x595820` | **0x3A** | 64 | — | `Window_Kind0Frame` | state 2: the frame, then `MsgBox_FrameTask` |
| `0x595860` | **0x213** | 544 | — | `Window_Shrink` | state 3 of all three kinds |
| `0x595A80` | **0x14** | 32 | — | `Window_Kind0Close` | state 4: wait on the box, then free |
| `0x595AA0` | 0x3E | — | — | `Window_Kind1States` | kind 1's five states |
| `0x595AE0` | 0x0E | — | — | `Window_Kind1Open` | state 0: the unread set-up `0x596330` |
| `0x595AF0` | 0x39 | — | — | `Window_Kind1Frame` | state 2: the frame and the fixed-place list |
| `0x595B30` | 0x05 | — | — | `Window_FreeState` | state 4 of kinds 1 and 2: free, nothing else |
| `0x595B40` | 0x3E | — | — | `Window_Kind2States` | kind 2's five states |
| `0x595B80` | 0x89 | — | — | `Window_Kind2Open` | state 0: a fixed 0x47 x 0x20 box |
| `0x595C10` | 0x39 | — | — | `Window_Kind2Frame` | state 2: the frame and the window-relative list |
| `0x595C50` | 0x302 | 770 | `0x8015A58C` | `Window_DrawFrame` | the six primitives of an open window |
| `0x595F60` | 0xBE | 190 | `0x8015A990` | `Window_DrawOutline` | the outline alone, in 0x78 |
| `0x596150` | 0x68 | 104 | `0x8015ABA0` | `Window_DrawLine` | one `LINE_F2` |
| `0x59E230` | 0x94 | 148 | `0x8015973C` | `Field_RunTaskRecords` | three passes over the 22 records |
| `0x59E310` | 0x1D | 29 | `0x801598DC` | `Window_FreeCurrent` | free the current record |
| `0x5A7630` | 0x1A | 26 | — | `Gpu_SetPolyGT4` | in `src/game/psx_gpu.cpp` |
| `0x5A7690` | 0x1A | 26 | `0x8017B460` | `Gpu_SetLineF4` | likewise |
| `0x5A7740` | 0x10 | 16 | `0x8017B3E4` | `Gpu_SetTile` | likewise |

**Bold** sizes were wrong in `analysis/attract_catalog.md` and in the two
`entries*.txt` lists: `pe_funcs.py` measures to the next entry it knows of, so
a pointer-reached function with alignment padding before it absorbs the
padding, and a *run* of them absorbs the lot.

`Window_DrawFrame` is not only this family's: three battle functions
(`0x47DAC0`, `0x47F2D0`, `0x47F9E0`) call it too.

## 3. What a window record is

`Window_Alloc` `0x59E2D0` hands out slots of the 22 records of `0x24` bytes at
`0x803160` (symbols.toml block `WindowRecords`, PSX `0x8014832C`). What this
layer reads and writes:

| offset | width | what |
|---|---|---|
| +0 | u8 | in use; `Window_Alloc` sets 1, `Window_FreeCurrent` clears it |
| +1 | u8 | the record **handler**, index into `Field_RunTaskRecords`' nine; never cleared |
| +2 | u8 | the window **kind**, 0..2 |
| +3 | u8 | the **state**, 0..4 |
| +4 | u16 | centre x, 12.4 fixed |
| +6 | u16 | centre y, 12.4 fixed |
| +0xA | u8 | half-width, in pixels |
| +0xB | u8 | half-height |
| +0xD | u8 | flags: bit 0 "do not draw the frame", bit 1 "the narrow box" |
| +0xF | u8 | the **pass** the record runs in, 0..2 |
| +0x10 | u16 | current width, 12.4 |
| +0x12 | u16 | current height, 12.4 |
| +0x14 | u16 | width step a frame |
| +0x16 | u16 | height step a frame |
| +0x18, +0x1A | u16 | kind 1's settled centre |

`0x905B84` holds the record the layer is running. **It is re-read from memory
at every point the original re-reads it** — after every call out, and in most
of these functions after every store as well. That is not an optimisation the
port missed; the fuzz's stand-ins repoint `0x905B84` between calls, and a
missing re-read is refused (§6, control "the record not re-read after the
outline", 8 of 1,400 rounds).

## 4. The three kinds and their five states

`Field_RunTaskRecords` runs the passes `[0x802D20] .. 2` in order; within each
pass it walks all 22 records and runs handler `record[+1]` for those whose
`+0xF` equals the pass. The pass and the walk are registers in the original, so
a handler that writes `0x802D20` (state 4 does, to 0) or repoints `0x905B84`
changes neither this frame.

Handler 0 is `Window_Run`, which dispatches on `+2`:

| state | kind 0 (message box) | kind 1 (list at fixed place) | kind 2 (list in its window) |
|---|---|---|---|
| 0 | `Window_Kind0Open` | `Window_Kind1Open` | `Window_Kind2Open` |
| 1 | `Window_Grow` | `Window_Grow` | `Window_Grow` |
| 2 | `Window_Kind0Frame` | `Window_Kind1Frame` | `Window_Kind2Frame` |
| 3 | `Window_Shrink` | `Window_Shrink` | `Window_Shrink` |
| 4 | `Window_Kind0Close` | `Window_FreeState` | `Window_FreeState` |

**Kind 0** is the dialogue box: its states 2, 3 and 4 all reach
`MsgBox_FrameTask` `0x4977F0` (group H's), two of them as tail jumps. State 4
waits for the box to answer 0, then zeroes the pass byte and frees the record.

**Kind 1** draws its items at a fixed screen place: `0x596090` walks
`0x7DEE66 + 1` items and draws each at x `0x3A`, y `[0x7DEE6F + i]`, through
the text routine `0x5961C0`; `0x596120` draws the cursor. Its settled centre
while shrinking comes from the 8-byte records at `0x66ADFC` by `0x7DEE65`,
where the grow takes it from the record's own `+0x18` / `+0x1A` — the two
directions genuinely disagree, and both negative controls that make them agree
are refused.

**Kind 2** draws relative to its own window: `0x596020` places item `i` at the
window's centre plus `0x66AE2C[set]`, with a per-set stride, and `0x5960D0`
draws the cursor. Those five functions (`0x596020`, `0x596090`, `0x5960D0`,
`0x596120`, `0x596330`) are **not read** and stay Capcom's; ours and the copies
alike call them through a recording stand-in.

### The box's animation

`Window_Kind0Open` reads the `0x0C` placement code `0x7DEE6E` — the byte
`MsgBox_Reset` stores there, and the index `MsgBox_PlacementTable` `0x66AE10`
is documented against. Bit 7 becomes flag 1 and bit 6 flag 2, each cleared
from the code as it is taken. Flag 2 gives a narrow 0x78 x 0x36 box from
`0x66ADE8`; otherwise 0xC8 x 0x36 from `0x66ADD8`. The step is
`(half-size << 4) / 5` in each axis, so **the box opens over five frames**.

`Window_Grow` adds the step to the size and subtracts half of it (truncated
toward zero) from the centre, so the box grows about its middle. When
`(s16)(size >> 4)` passes the half-size byte the centre snaps to the settled
place and the size to `half << 4`. `Window_Shrink` is the mirror, except that
its snap is to the settled place **plus half the half-size** — the box closes
onto its own middle, not onto its left edge — and that the half-size is halved
there as an unsigned byte, where the centre drift halves a signed word.

Both draw `Window_DrawOutline` at the size this frame reached, unless flag 1
is set, and both raise the state only when **both** axes settled in the same
frame. `Window_Shrink` returns before the draw in that case; `Window_Grow`
draws first and raises the state after.

## 5. What `Window_DrawFrame` draws

Six primitives, each through `Gfx_CommitPrim` into ordering-table slot 1,
with `x` and `y` taken as s16 and `w` and `h` as a single byte:

1. a **black `TILE`** at (x, y) of (w + 1, h + 1), semi-transparent, under
   texture page 0xF;
2. an **8-byte primitive** holding the words 0, 0xF0, 0x10, 0x10 — the packet
   cursor is advanced by hand over it, and its *address* is then passed as the
   next `Gpu_SetDrawMode`'s texture-window argument;
3. the window's **`POLY_GT4`**: corners (x, y)..(x + w + 1, y + h + 1), 0xFF at
   every vertex, texture (u, v) (0, 0)..(w, h), page
   `Gpu_GetTPage(0, 0, 0x3C0, 0)` and CLUT
   `Gpu_GetClut((s8)[0x903A5A] * 32 + 0x10, 0x1E1)` — `0x903A5A` is the window
   colour the Config screen sets;
4. a **second 8-byte primitive**, words 0, 0, 0x100, 0x100, likewise the
   texture window of a draw mode under page 5;
5. a **`LINE_F4`** in 0xC8C8C8 round (x - 1, y - 1)..(x + w + 1, y + h + 1) —
   a `LINE_F4` is three segments, so this is three sides of the rectangle;
6. **five `LINE_F2`s**: the missing left side in 0xC8, then a 0x8C border two
   pixels out on each of the four sides.

`Window_DrawOutline` is items 5 and 6's first line alone, in 0x787878, with no
tile, no quad and no draw mode of its own — so the growing box draws under
whatever mode the frame before it left.

All the vertex coordinates are `fild` of a 32-bit sum stored as a single. The
sums are small integers, exactly representable, so the x87 rounding mode does
not reach them; the fuzz sets the control word to `0x027F` around both sides
anyway.

## 6. The fuzz and its negative controls

`BOF3X_SHADOW=window_task`, at start-up
(`src/game/window_task_fuzz.cpp`): **twenty-four byte-copies**, every call out
re-aimed at a recording stand-in — the three tail jumps to `MsgBox_FrameTask`
and the four to `Window_FreeCurrent` / `0x596120` / `0x5960D0` included — and
**every stack-built dispatch table's immediates re-aimed inside the copy**.
That last one is the trap of this family: `mov dword ptr [esp + k], imm32`
carries an absolute address, so an unpatched copy of `Window_Run` calls the
*original* `Window_Kind0States`, which is exactly what is under test. The same
hazard `text_draw.cpp` has with a jump table, one instruction earlier.

Each round: random bytes over the 22 records, `0x905B84`, the pass byte,
`MsgBoxState` `0x7DEE40..7F`, the six scattered pending-area cells,
`Field_Request`, `Field_ScriptFlags`, the window colour, `Game_AreaNumber` and
`Cond_Flags`; then the indices put back inside their tables and the zone lists
and music sets rebuilt in buffers of our own (the zone list always ends in a
record that matches everything, since the original's search has no end test);
then each branch's boundaries seeded — the sizes at `half << 4` plus
−0x11..+0x11, at 0, 1, 0xFFF0, 0xFFFF, 0x8000; the half-sizes at 0, 1, 2,
0x20, 0x36, 0x47, 0x78, 0xC8, 0xFF; the placement code inside and outside
0..7; the pending area at each of the eleven listed ones. Theirs runs, then
ours from the same state, and the regions, the buffers, the 0x600-byte
primitive pool, the packet cursor, the result and the stand-ins' log are
compared.

    shadow  window_task self-test: 33600 rounds over 24 functions (1400 each),
            74433 calls to the stand-ins, 0 MISMATCHES
    shadow  window_task coverage: pending area one of the eleven 940 / not 460;
            music kept 352, straight from the zone or an empty list 767, off a
            flag that was set 173, the byte after the list 108; grow settled
            x 778 y 778 both 482, shrink settled x 679 y 710 both 423; an
            outline drawn 1181, narrow box 1446, frame hidden 1379; records run
            1048, pass above 2 174, record freed 700

`Gpu_SetPolyGT4`, `Gpu_SetLineF4` and `Gpu_SetTile` are fuzzed with their
fourteen siblings in `psx_gpu.cpp` (`BOF3X_SHADOW=psx_gpu`, 200 rounds each of
a random 0x50-byte primitive): *17 functions, 48484 rounds in all, 0
MISMATCHES*.

### The stand-ins are not quiet

The movement-script trap (`HANDOFF.md`) applies here: a stand-in that does
less than the real callee hides what the caller reads back. So
`Gfx_CommitPrim`'s stand-in **moves the packet cursor** (wrapping inside our
pool), the four `Gpu_Set*` stand-ins **write the real GPU code byte and the
0.01 floats**, `Gpu_SetDrawMode`'s writes the real `0xE8......` dword and the
texture window, and `MsgBox_FrameTask`'s returns 0 or a non-zero byte by the
round's hash. Between calls a `Disturb()` writes to one of nine watched bytes
and, once in 23, **repoints `0x905B84` at another of the 22 records** — which
is what makes the re-read controls fail.

### Negative controls

Fifty-eight planted bugs, one at a time, rebuilt and re-run
(`BOF3X_SELFTEST_ONLY=1`). **Fifty-six were refused by a count of
mismatches**; the two that were not are written up below, because a control
that is not refused is information about the claim.

| the bug | rounds that refused it (of 1,400) |
|---|--:|
| `Field_ChangeArea`: x shifted 15, not 16 | 1400 |
| `Field_ChangeArea`: the pending area kept in a register, not re-read after the classify | 213 |
| `Field_ChangeArea`: `Field_Request` 4, not 5 | 1400 |
| `Area_ClassifyPending`: 0xA for a listed area, not 0xB | 940 |
| `Area_ClassifyPending`: ten entries scanned, not eleven | 537 |
| `Area_ZoneIdAt`: the record's byte +4, not +5 | 1394 |
| `Area_PickMusic`: flag bit 0x40, not 0x80 | 744 |
| `Area_PickMusic`: byte +6 tested for the list, not +7 | 667 |
| `Area_PickMusic`: the list stops on a flag that is CLEAR | 281 |
| `Area_PickMusic`: the flag row at 4 bytes, not 8 | 279 |
| `Area_ZoneAtIn`: the near edge compared against r1, not r0 | 232 |
| `Area_ZoneAtIn`: x masked to 7 bits, not 8 | 171 |
| `Window_Run`: the kind taken from byte +3, not +2 | a `Fatal` from the bound check |
| `Window_Run`: kinds 0 and 2 swapped | 926 |
| `RunState`: the state byte read off record 0, not the current record | 3209 |
| `Window_Kind0States`: kind 1's table, not kind 0's | 1400 |
| `Window_Kind1States`: the state taken from byte +2, not +3 | 3337 |
| `Window_Kind0Open`: bit 7 left in the placement code | 695 |
| `Window_Kind0Open`: the narrow box centred at +0x3D, not +0x3C | 704 |
| `Window_Kind0Open`: the box opens over four frames, not five | 1400 |
| `Window_Kind0Open`: the wide box's half-width 0xC9, not 0xC8 | 696 |
| `Window_Grow`: the x axis settles when it REACHES the half-size | 162 |
| `Window_Grow`: the centre drifts the wrong way on x | 449 |
| `Window_Grow`: the record not re-read after the outline | 8 |
| `Window_Grow`: the y axis sets the settle counter rather than incrementing it | 490 |
| `Window_Grow`: kind 1's settled x from the list table, as the shrink takes it | 272 |
| `Window_Shrink`: the x axis closes at zero, not below it | 40 |
| `Window_Shrink`: the closing centre takes a quarter of the half-size | 549 |
| `Window_Shrink`: the settled frame goes on to draw | 305 |
| `Window_Shrink`: the message box runs for every kind, not kind 0 alone | 649 |
| `Window_Shrink`: kind 1's settled x from the record, as the grow takes it | 256 |
| `Window_Kind0Frame`: the frame drawn when flag 1 is SET | 1400 |
| `Window_Kind0Close`: the record freed while the box still answers non-zero | 1400 |
| `Window_Kind1Open`: the state raised before the set-up call, not after | 52 |
| `Window_Kind1Frame`: flag 1 consulted, as kind 0's state 2 does | 688 |
| `Window_Kind2Frame`: kind 1's two draws, not kind 2's | 1400 |
| `Window_FreeState`: the record not freed at all | 1400 |
| `Window_Kind2Open`: the box placed at +0x24, not +0x23 | 1400 |
| `Window_Kind2Open`: the y step 0x67, not 0x66 | 1400 |
| `Window_DrawFrame`: the tile w pixels wide, not w + 1 | 1400 |
| `Window_DrawFrame`: the quad's v3 texture row from w, not h | 1396 |
| `Window_DrawFrame`: the dim border one pixel out on the left, not two | 1400 |
| `Window_DrawFrame`: the second texture window 0x100 wide but 0xFF tall | 1400 |
| `Window_DrawFrame`: the first texture window's height word 0x11, not 0x10 | 1400 |
| `Window_DrawFrame`: the CLUT row read unsigned, not as an s8 | 691 |
| `Window_DrawFrame`: the quad's tpage from (0, 0, 0x3C0, 1) | 1400 |
| `Window_DrawFrame`: the bright outline committed into slot 0 | 1400 |
| `Window_DrawOutline`: the outline in 0x79, not 0x78 | 1400 |
| `Window_DrawOutline`: the closing line stops at the bottom, not one past it | 1400 |
| `Window_DrawLine`: the second end's x and y swapped | 1400 |
| `Window_DrawLine`: the colour taken as a whole word, not a byte | 1395 |
| `Field_RunTaskRecords`: a record runs when its pass is at or below this one | 1226 |
| `Field_RunTaskRecords`: a starting pass of 3 still runs a pass | 39 |
| `Field_RunTaskRecords`: a record's pass read from `0x802D20` afresh, not from the register | 889 |
| `Field_RunTaskRecords`: the record walk restarts from the current record each turn | 1180 |
| `Window_FreeCurrent`: the handler byte +1 cleared as well | 1238 |
| `Window_FreeCurrent`: the state byte +3 left alone | 1120 |

**Two that were not refused by a count:**

- *`Area_ZoneAtIn`: the box's right edge exclusive, not inclusive.* This one
  **hung** rather than mismatching: the fuzz's catch-all zone record is
  `(0, 0, 0xFF, 0xFF)`, and an exclusive right edge makes it miss `x == 0xFF`,
  so the original's endless search never ends. A control refused by a hang
  proves less than one refused by a count (`HANDOFF.md`), so it was replaced by
  the near-edge control above, which fails by comparison in 232 rounds.
- *`Window_DrawLine`: the colour byte read after the line setter, not before.*
  **Not refused, and no input could tell the two apart**: `colour` is an
  argument, not memory, and the only thing between the two points is
  `Gpu_SetLineF2`, which cannot touch it. The claim in the comment is about
  the original's instruction order, not about behaviour, and it is recorded
  here as such rather than as a tested quirk.

## 7. For the batch check

**Add to `analysis/calltrace/entries_logic.txt`** (and to `entries.txt` if the
list is rebuilt), one per line, hex address and hex size — an owned function
missing from the list breaks the frame hash without changing a count
(`HANDOFF.md` "Pick up here" 1):

    # group J, 2026-09-22 (docs/window-task.md): the window/task layer. The
    # fourteen from 005954B0 to 00595C10 were in no list; 00595450 was listed
    # as 7F9, which is pe_funcs.py running through all fourteen of them.
    00594E00 52
    00595350 33
    00595390 1D
    005953B0 98
    00595450 53
    005954B0 2E
    005954E0 3E
    00595520 14D
    00595670 1A3
    00595820 3A
    00595860 213
    00595A80 14
    00595AA0 3E
    00595AE0 E
    00595AF0 39
    00595B30 5
    00595B40 3E
    00595B80 89
    00595C10 39
    00595C50 302
    00595F60 BE
    00596150 68
    0059E230 94
    0059E310 1D
    005A7630 1A
    005A7690 1A
    005A7740 10

`00595450`'s existing line says `7F9` and **must be replaced**, not added to.
Twelve of the others are already listed with the right size (`00594E00`,
`00595350`, `00595390`, `005953B0`, `00595C50`, `00595F60`, `00596150`,
`0059E230`, `0059E310` and the three `Gpu_Set*`); the fourteen in between —
`005954B0` to `00595C10` — are new lines.

**What the live batch should look at.** The attract sequence runs this layer
hard — `Window_DrawLine` 13,449 calls, `Window_Kind0Frame` and
`Window_DrawFrame` 2,637 each — so the ordinary checks cover kind 0 well:

- the **nine-minute attract capture** is the real test of `Window_DrawFrame`;
  every dialogue box in it is drawn by these six primitives. Compare its
  shots, not just the hash.
- the **oracle** and the **memory dump** as usual; the frame hash needs the
  list above first.
- `BOF3X_ORIGINAL` on all twenty-four names is 24 more names in the list
  (about 400 characters) — inside the 8,192-byte ceiling raised on 2026-09-22.
- a **menu recipe** (`tools/recipes/field_menu.txt`) is the only thing that
  would reach kinds 1 and 2 at all; see §8.

## 8. What no check reached

- **Kinds 1 and 2 entirely.** `hidden_b` counts zero calls of
  `Window_Kind1States` and `Window_Kind2States`: the attract sequence presses
  nothing, and both kinds are choice lists. Fuzz only. What would reach them:
  any dialogue with a yes/no or a multi-way answer — the field menu, a shop, a
  save prompt. `tools/recipes/field_menu.txt` on save 5 is the cheapest try.
  Their five unread callees (`0x596020`, `0x596090`, `0x5960D0`, `0x596120`,
  `0x596330`) stay Capcom's behind stand-ins and would be the obvious next
  group.
- **Record handlers 1..8** of `Field_RunTaskRecords` (`0x596530`, `0x5968E0`,
  `0x596FA0`, `0x597F60`, `0x598890`, `0x599B50`, `0x59B220`, `0x59CB00`) are
  not ours and not read. The walk that reaches them is.
- **A handler or kind or state index out of range.** The original's tables are
  built on its own stack, so index 9 of the nine is the function's own return
  address; that is not reproducible and ours aborts loudly instead (rule 4).
  No check has seen such an index, and `Window_Alloc` is the only writer of
  byte +1.
- **A starting pass above 2** runs nothing; seeded in the fuzz (174 rounds),
  never seen live.
- **`Area_ClassifyPending`'s reader.** Twenty-two functions write `0x937F98`
  and none that `analysis/pc_funcs.json` lists reads it. Until one is found the
  name is by the shape of what the function writes, and its `status` is
  `hypothesis`.
- **`Area_ZoneIdAt`'s caller** `0x594E60` is group K's next round and stays
  Capcom's, so the byte it returns is exercised only by the fuzz.
- **The eleven areas** `Area_ClassifyPending` answers 0xB for
  (0x10 0x21 0x2D 0x41 0x57 0x58 0x68 0x73 0x79 0x97 0x98) are read out of
  `0x66ADC0`; which places they are, and what 0xB means, are unread.
- **`Window_DrawFrame`'s three battle callers** are unexercised by anything
  automated: a real encounter is still not repeatable (`HANDOFF.md` 000).

## 9. Open

- What `0x937F98` is for (§8), and what the `0x904CD0` music sets at
  `0x669A48` correspond to — twelve records, counts 1..3, each entry a track
  under one story flag.
- The PSX twins of the fourteen state functions. `psx_pair` has none: they are
  pointer-reached on the PC and the PSX side is in an overlay. The three that
  do pair (`Window_DrawFrame`, `Window_DrawOutline`, `Window_DrawLine`) pair
  into the boot image, which is why `0x8015A58C` was already named in the
  sibling.
- The scattered pending-area cells. The PSX keeps area, position, flags and
  track in one block at `0x80143F10..1F`; the port put them at `0x937F82`,
  `0x903860`, `0x90384C`, `0x905B88`, `0x937F98` and `0x904CD0`. Whether that
  was the compiler or a deliberate rewrite is unread; `0x905B88` ended up one
  byte below `Gfx_BufferIndex`, which is worth remembering before anyone
  writes a block-wide memset near it.
