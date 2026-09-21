# The sprite draw-order pass, `0x593060`

**Status:** IN PROGRESS (2026-09-20)

What the regenerated takeover queue ([`call-trace.md`](call-trace.md) §9) put
first, and where it led: its two hottest layer-0 logic functions are the
exchange helpers of the pass that decides the order field sprites are drawn
in, and the next half-dozen all touch the same structures - the sprite object
arrays, the view's cell grid, the draw-item pool, the animation script, the
per-layer draw lists. Nineteen functions are ours (sections 3 to 7); the pass itself is read but not taken over.

All addresses are `BOF3.exe` (`symbols.toml` `[meta]`). Disassembly by
`python tools/pe_disasm.py`, 2026-09-20.

## 1. How it was found

`python tools/calltrace.py queue analysis/calltrace/all_a/bof3x.callcounts.tsv`
(2026-09-20, twenty-five owned): 486 reached functions not in `symbols.toml`;
layers 0-7 hold 200 / 61 / 32 / 17 / 7 / 1 / 1 / 1, 166 in or above a cycle.
Layer 0, logic, below `0x5A6000`: 79. The top two by call count:

| Entry | Size | Calls in `all_a` | Now |
|---|---|---|---|
| `0x593570` | 53 | 421,058 | `Sprite_DrawRecordSwap` |
| `0x593530` | 54 | 16,444 | `Sprite_DrawListSwap` |

Each has one static caller (`callees` in `analysis/pc_funcs.json`): `0x593060`.

**A trap in `pc_funcs.json`:** it also lists a function at `0x5930A0` - mid
instruction, inside `0x593060`; the first bytes decode as garbage. Something
produced that address as a call target. Not chased. `0x593060` (`sub esp,
0x1C`, preceded by padding) is the real entry, and `0x592F00` calls it.

## 2. What `0x593060` does

Read to its `ret` at `0x59352A`; the callees named below were **not** read when this was written, so
what they are for is inference from how they are called, and is marked so.
Sections 8, 9 and 11 say what they turned out to be.

**Pass 1, `0x593063`..`0x59312A` - order the list.** A bubble sort of
`Sprite_DrawList[0 .. Sprite_DrawListCount)`, pointers to sprite objects (the
same objects `Sprite_Current` `0x937F88` points at; two of the list's three
writers store `Sprite_Current`). Ascending, comparing neighbours from the top of
the list down, exchanging through `Sprite_DrawListSwap` when the later one is
smaller on:

1. the `u16` at object `+0x32`, unsigned; on a tie
2. the sum of the dwords at `+0x34` and `+0x38`, signed; on a tie
3. the dword at `+0x3C`, signed.

Equal objects are not exchanged, so the sort is stable. The list holds at most
40: every writer (`0x510E26`, `0x57BA47`, `0x5890C3` - `pe_xref.py 0x9036E0`)
stores and increments only when the count is below `0x28`.

Then `0x5A77C0(dword [0x7E0670], 0, 0, 0x95, 0)` and `0x461E50(6, 0xC)`. Unread.

**Pass 2, `0x593151`..`0x59351D` - walk 0x37 layers.** For layer 0 to 0x36,
under the flag byte `0x7E0918`:

- bit 2: `0x56FD20(layer)` before the layer and `0x56FE80(layer)` after it.
- bit 4: if the dword at `0x8022C0 + (Gfx_BufferIndex + layer * 6) * 8` is
  non-zero, it is handed to `0x5A7560` with the dword at `0x929EA0 + slot * 4`,
  slot = byte `0x92BF19`, and the neighbouring dword at `0x8022C4 + ...` is
  then stored to that slot. *Inference:* `0x5A7560(tail, item)` followed by
  `tail = item` is the shape of appending to a linked draw list - the PSX
  ordering table, kept by the port. *(Read since, and so it is: section 7.)*

Within a layer it merges two ordered sources. A sprite belongs to the layer
when the high byte of its `+0x32` word equals the layer - so that word is
*layer, then order within the layer*, which is why pass 1 sorts on it. The
other source is a table of dwords at `0x801C00`, count byte `0x905BA0`: top
byte the layer, bits 16-31 the same 16-bit key, low 12 bits an index into
`0x48`-byte items at `0x905E80` (two per index, chosen by `Gfx_BufferIndex`).
Whichever key is lower goes first; table items are appended through `0x5A7560`
when flag bit 2 is set.

Sprites that share a key are gathered into 12-byte records at `0x7E01C0`:
object, then the object's dword `+0x60` with bit 31 set - or, for an object
with bit `0x40` of byte `+0x24`, whatever `0x57BAE0(record)` writes, which
returns how many records it added (unread). The run of records is bubble
sorted **descending** on the signed 16-bit word at record `+4`, exchanging
through `Sprite_DrawRecordSwap`, and then each is drawn: bit 31 set and flag
bit 1, `Sprite_Current = object; 0x5935B0()`; bit 31 clear and flag bit 3,
appended through `0x5A7560` to the slot named by byte `+0x29` of the object at
record `+8`.

With slot byte `0x92BF19` equal to 6 every sprite of the layer joins the run;
equal to 4, only those whose key equals the first one's; any other value, none.

**Why 421,058 calls:** two bubble sorts every logic frame, the second
exchanging twenty-five times as often as the first. In what order the records
arrive was not measured. It is why two three-line helpers top the queue.

## 3. The two helpers, taken over

`src/game/sprite_order.cpp`; `symbols.toml` has the instruction-level evidence.

- **`Sprite_DrawListSwap(i, j)`** `0x593530`. Kept from the original: only the
  low byte of each argument counts - the caller pushes whole registers of which
  it set one byte, so the mask is load-bearing - and nothing holds that byte to
  the list's 40 entries.
- **`Sprite_DrawRecordSwap(a, b)`** `0x593570`. Kept: the order of reads and
  writes (all of `b` read, `a` copied to `b` dword by dword, then the saved
  three to `a`), which is the result for overlapping records. The game passes
  neighbours, which do not overlap.

Neither has a call or a jump, so both clone. `BOF3X_SHADOW=sprite_order` runs a
start-up differential fuzz of ours against the clones, 2026-09-20,
`build/bof3x.log`:

| Function | Rounds | Of which | Mismatches |
|---|---|---|---|
| `Sprite_DrawListSwap` | 4,000 | 570 with `i == j`, 1,865 indexing past entry 39; upper 24 bits of both arguments random; 256 entries compared | 0 |
| `Sprite_DrawRecordSwap` | 4,000 | 1,931 with `a` and `b` overlapping | 0 |

**Negative controls, same day.** With `j` masked to `0x7F`: 985 of 4,000
mismatch and the DLL refuses to run. With the record swap saving `a` first -
identical for records that do not overlap - 1,505 mismatch, all from the
overlapping rounds, and it refuses to run. So the overlap rounds are what pins
the order, and nothing the game does would have.

## 4. The draw-item index pool, and a primitive writer - taken over

Next in the queue, same day; `symbols.toml` has the instruction-level evidence.

**`DrawItemPool_Alloc` `0x56FBD0`, `DrawItemPool_Release` `0x56FC70`**
(`src/game/draw_pool.cpp`; 3,887 and 2,937 calls in `all_a`). A stack of free
indices: 1,024 words at `0x7E09E0`, initialised to 0, 1, 2, ... by
`0x56F8BF`..`0x56F8D1`, and a top-of-stack word `0x9039D4` initialised to 1, so
index 0 is never handed out and 0 can mean "none". Top wraps at `0x3FF`, and a
top of 0 reads as an empty pool. Release checks nothing.

What the indices are of: `0x56FC00` releases the low 12 bits of a word at
`+2` of its argument, then the words at `0x905E80 + index * 0x90 + 0x7E` and
`+ 0x8E`. So they index the 0x90-byte items at `0x905E80` - two 0x48-byte
halves chosen by `Gfx_BufferIndex`, the table section 2's pass 2 merges with
the sprite list. That is what ties this pool to the draw order, and the
"DrawItem" in the names rests on it.

**`Prim_SetShade` `0x462A70`** (`src/game/prim.cpp`; 6,474 calls): one byte
to `+4`, `+5`, `+6` of its first argument. Its nine call sites are all in
`0x462820`, `0x462930` and `0x462A00`, which pass it the return value of
`0x462560(0x106, 0x82, 1, 1)`-style calls; unread. Bytes `+4`..`+6` are
`r0 g0 b0` of a PSX GPU primitive, which is the whole basis of the name.

Start-up fuzz against clones, `BOF3X_SHADOW=draw_pool,prim`, 2026-09-20:

| Check | Rounds | Of which | Mismatches |
|---|---|---|---|
| pool | 6,000 | 2,989 allocs (168 from an empty pool), 3,011 releases, 1,467 with top at 0, 1, `0x3FE` or `0x3FF`; array, top and result compared | 0 |
| `Prim_SetShade` | 256 | every level, random bits above the low byte of the argument | 0 |

Negative controls: alloc with top + 1 unmasked, 188 of 6,000 mismatch; shade
without byte `+6`, 254 of 256 (the other two are levels the random buffer
already held). Both refuse to run.

## 5. Four more from the same corner - taken over

Same day, same recipe. They are here because each turned out to touch the
structures above; `symbols.toml` has the instruction-level evidence.

**The sprite objects have an address and a size.** `Sprite_FindNearby`
`0x589660` walks **30 objects of `0xA4` bytes at `0x7DEE80`**
(`Sprite_Objects`, ending `0x7E01B8`, 8 bytes below section 2's draw records)
and then **4 more at `0x802000`** (`Sprite_ObjectsExtra`), numbering them 0-29
and 30-33. Fields it fixes: `+0` bit 0 in use, `+6` a type byte, `+0x24` bit
`0x40`, `+0x34` / `+0x38` signed position dwords (pass 1 sorts on their sum),
`+0x3E` an s16, `+0x54` a pointer to a record with a signed reach at `+2` and
a skip bit at `+3`. `pe_xref.py`: 160 references to `0x7DEE80` in 53 functions,
17 to `0x802000` in 15 - this is the array most of the field code is about.

- **`Sprite_FindNearby`** (`src/game/sprite_find.cpp`, 8,919 calls): the
  number of the first type-`0x0A` object, not the current one, that has the
  current one within its reach (x and y, `reach << 15`) and within `0x300` in
  z; `0xFF` for none. Kept: the x and y differences wrap in 32 bits and the
  absolute value leaves `0x80000000` negative, so that difference is within
  any reach.
- **`MapView_CellToMap`** `0x56F910` (`src/game/map_view.cpp`, 13,664 calls):
  which cell of the area map a cell of the view shows. The view is a grid of
  56 rows by 28 columns of 4-byte cells at `0x937FA0` (caller `0x56F8A5`):
  map x, map y, and at `+2` the **draw-item index** that section 4's pool hands
  out and `0x56FC00` takes back. The arithmetic is isometric - x = origin +
  col + (row + 1) / 2, y = origin + row / 2 - col. The map is loaded data:
  `AreaMap_Header` `0x8CB580` is `LoadDatFile`'s arena plus `0xC8000`, a chunk
  tag [`asset-loading-path.md`](asset-loading-path.md) already lists. Kept:
  the map's outermost ring never shows, and "none" is written as cell (0, 0).
- **`Sprite_ScriptStep`** `0x589470` (`src/game/sprite_anim.cpp`, 2,920
  calls): two bytes of the current object's byte script (`+0x50`, position
  u16 `+0x58`): the first to `+0x4A`, the second an index into the word table
  at `+0x54`, the word to `+0x5A`. Kept: the position wraps at 16 bits between
  the two reads. Note `+0x54` is the same pointer `Sprite_FindNearby` reads a
  reach and a flag from - one record, a header and then a word table; unread
  beyond that.

- **`Field_CopyInput`** `0x531BD0` (`src/game/field_input.cpp`, 2,779 calls):
  copies the held-buttons word `Input_Held` `0x7E1BE8` to the field's own
  `0x905BA6` - exchanging bit pairs 12-13 and 14-15, and dropping bits 9 and
  10, while a member's state byte has bit `0x20`. The member records are
  `0x14C` bytes at `0x802E88`, count byte `0x929EC0`; byte `+0` of each indexes
  state records at `0x903A80` whose stride is `0xA4` - **the sprite object's
  size again, so possibly a third array of them**; not established, and those
  four data names are `hypothesis`. Bit 0 of `0x905BA2` chooses between "any
  member" and "the first". *On a PSX pad bits 12-15 are the four directions,
  which would make this "controls reversed while someone is in state 0x20" -
  but neither the bit layout nor the state is established from the binary.*
  **Owner, 2026-09-20: there is a field confusion status that reverses
  inputs - left is right, up is down.** That is what this function does to
  the word, so bit `0x20` is very probably that status and the members are
  the party on the field; it stays inference until seen in game with ours
  and with the original ([`USER_CHECKS.md`](USER_CHECKS.md) item 5). `Input_Held` itself is on firmer ground: `0x461EB0` is an
  auto-repeat on it, 12 frames and then every 3.

Start-up fuzz against clones,
`BOF3X_SHADOW=map_view,sprite_anim,sprite_find,field_input`:

| Function | Rounds | Of which | Mismatches |
|---|---|---|---|
| `MapView_CellToMap` | 20,000 | 4,726 on a cell, 2,379 on an empty cell, 12,895 outside; a quarter aimed at x or y of 0, 1, size - 2, size - 1 | 0 |
| `Sprite_ScriptStep` | 4,000 | 415 with no script, 460 with the position at `0xFFFE` / `0xFFFF` | 0 |
| `Sprite_FindNearby` | 8,000 | 7,015 found in the 30, 89 in the extra 4, 896 none; positions at the wrap, at the reach and one past it | 0 |
| `Field_CopyInput` | 8,000 | 4,034 looking at every member, in 1,226 of which the first alone would have answered otherwise; 2,362 changed the word | 0 |

Negative controls, each refusing to run: the map's `x < width - 1` as
`x < width`, 326 of 20,000; the script's second read not wrapping, 243 of
4,000; the search with an absolute value that treats `0x80000000` as far,
2,383 of 8,000, and with z `< 0x300`, 2,822; the input copy keeping bits 9 and
10, 1,883 of 8,000, and letting the last member decide, 1,155.

## 6. Seven more, second pass of the queue - taken over

Naming section 4's pool made its caller a leaf, and the regenerated queue put
it first. `symbols.toml` has the instruction-level evidence for all seven.

**The animation script, complete** (`src/game/sprite_anim.cpp`). Four
contiguous functions, `0x589350`..`0x5894CF`, that only ever transfer among
themselves - so one byte-copy of the block holds runnable copies of all four:

| | |
|---|---|
| header | 2 bytes; byte 1 is the number of steps |
| steps | 2 bytes each: ticks to hold, then a frame number - or, where a step would start, a byte `>= 0x80` and the number of the step to go to |
| frames | `u16` each, straight after the last step |

- `Sprite_ScriptStart(position)` `0x589350` (240 calls): step count to object
  `+0x49`, `+0x50` past the header, `+0x54` to the frame words, first step.
- `Sprite_ScriptTick` `0x5893A0` (11,899): counts `+0x4A` down; at 0 takes
  the next step, wrapping to the top after the last; returns 1 when it wrapped
  or followed a jump.
- `Sprite_ScriptTickOnce` `0x589410` (4,311): the same, but past the last step
  it sets the hold back to 1 and returns 1 - a script that plays once and then
  reports "finished" every tick.
- `Sprite_ScriptStep` (section 5) is what all three end in. This also
  bears on section 5: `+0x54` is the frame-word table, so - if the type-`0x0A`
  objects' `+0x54` is set this way too, which was not checked - the "record"
  whose bytes `+2` and `+3` `Sprite_FindNearby` reads is frame word 1.

**`DrawItemPool_ReleaseCell`** `0x56FC00` (`draw_pool.cpp`, **863,659 calls,
the most in the queue**): gives back what a view cell holds - the item named
by the low 12 bits of the cell's word `+2`, and the two further indices that
item may own at `+0x7E` and `+0x8E`. `DrawItems` `0x905E80` is 1,024 items of
`0x90` bytes, ending `0x20` below the slot table `0x929EA0`. The count is the
view's 1,568 cells all being put through it whenever the view is rebuilt.
Its three calls are all to `DrawItemPool_Release`; the clone's are pointed at
the clone of that (`bof3::CloneCall`), the first use of that feature here.

**`AreaMap_ByteAt(x, y)`** `0x536700` (`map_view.cpp`, 619 calls): the byte at
`AreaMap_Bytes` `[0x905D94]` `+ x + y * width` - a per-cell byte plane beside
the word plane. Signed 16-bit arguments, no bounds check.

**`Sprite_PointInReach`** `0x531C70` (`sprite_find.cpp`, 2,450 calls): whether
a point is within an object's reach (byte `+0x70`, plus a margin, plus 2) of
the object's position less dword `+0xC` / `+0x10` times byte `+9` - read as
"where it is about to be", which is inference. z inclusive, x and y exclusive.

**`Sprite_RestoreClut`** `0x534E50` (`sprite_clut.cpp`, 2,779 calls): if bit 3
of byte `+0x138` of the block at `[0x905D98]` is set, copies entries 1-31 of
CLUT number `Sprite_Current[+0x27]` from `Gfx_ClutStripSource` `0x80B580` to
`Gfx_ClutStrip`, sets `Gfx_ClutStripDirty`, clears the bit. **This answers
part of what [`call-trace.md`](call-trace.md) §10 left open about the strip:**
it is the game's CLUTs, 32 words to a number, kept twice in the DAT arena - as
loaded at `+0x8000`, and the working copy the game recolours and uploads at
`+0xC000`.

Start-up fuzz against clones,
`BOF3X_SHADOW=sprite_anim,draw_pool,map_view,sprite_find,sprite_clut`:

| Check | Rounds | Of which | Mismatches |
|---|---|---|---|
| script block, four functions | 12,000 | about 3,000 each; of the ticks 3,909 fired, 825 at the script's end, 1,566 followed a jump; a quarter of script bytes exactly `0x7F` or `0x80` | 0 |
| `DrawItemPool_ReleaseCell` | 6,000 | 1,215 with nothing to release; 1,227 / 2,332 / 1,226 releasing one, two, three indices | 0 |
| `AreaMap_ByteAt` | 8,000 | 6,033 with a negative coordinate; noise above bit 15 of both arguments | 0 |
| `Sprite_PointInReach` | 20,000 | 1,773 within reach; offsets at the reach, one either side, `0x80000000` away | 0 |
| `Sprite_RestoreClut` | 3,000 | 2,013 with the bit set; every CLUT byte | 0 |

Negative controls, each refusing to run: a jump threshold of `> 0x80`, 341 of
12,000 (4 of 12,000 before the script was seeded with threshold bytes - **a
fuzz of random bytes barely tests a byte comparison**); `TickOnce` not
resetting the hold, 447; `ReleaseCell` keeping the cell word's top bits, 4,466
of 6,000; `PointInReach` with x inclusive, 907 of 20,000; `RestoreClut` copying
entry 0 too, 2,013 of 3,000. `AreaMap_ByteAt` with y unsigned logged
mismatches from round 2 and then faulted reading outside the test plane,
before its refusal line.

## 7. Three more: the layer lists, the draw table, the elevation - taken over

The two structures section 2 could only describe from the outside now have
their writers (`src/game/draw_layers.cpp`):

- **`DrawLayers_Reset`** `0x56F5B0` (1,272 calls). `DrawLayers` `0x8022A0` is
  0x38 layers of `0x30` bytes; a layer is three pairs of 8-byte list heads,
  one of each pair per display buffer. A head is `{first, last}` and is
  emptied as `first = 0, last = the head's own address`. **Section 2's
  inference is now a reading:** `0x5A7560(tail, item)` is three instructions,
  `*tail = item` (disasm 2026-09-20), and its callers then make the item the
  new tail - an append to a singly linked list whose link is the item's first
  dword, as in a PSX ordering table. With `last` pointing at the head, the
  first append and every later one are the same store. Not yet taken over.
  Reset empties the pairs at `+0` and `+0x10`; the pair at `+0x20` is the one
  pass 2 reads (`0x8022C0`), and it is left alone. Kept:
  0x38 layers where the pass walks 0x37, and `Gfx_BufferIndex` used as a
  whole byte.
- **`DrawTable_Sort`** `0x56F5F0` (1,272 calls). `DrawTable` `0x801C00`, 256
  dwords, count byte `0x905BA0` - the second source of pass 2's merge - sorted
  ascending on the high 16 bits, layer then key. An exchange sort, each entry
  against every later one, so **not stable**. Kept, and the reason it is
  worth a line: every exchange also stores the value it moved to
  `Scratch_Swap` `0x90385C`, a global with **505 references** - the source's
  all-purpose swap temporary, by the look of it, written all over the game
  and meaningfully read nowhere yet seen. A reimplementation that swapped
  through a local would differ in memory and nowhere else.
- **`MapView_SetElevation`** `0x5725F0` (`map_view.cpp`, 1,168 calls): stores
  a value to `0x929F1C`, moves the word `0x92BEE2` by twice the change unless
  the area header's word `+0x1E` is set, and sets byte `0x905E69` to 2 (the
  view's initialiser sets it to 3). The three data names are `hypothesis`.

Start-up fuzz against clones, `BOF3X_SHADOW=draw_layers,map_view`:

| Function | Rounds | Of which | Mismatches |
|---|---|---|---|
| `DrawLayers_Reset` | 600 | 155 with a buffer index above 1; `0x1278` bytes compared | 0 |
| `DrawTable_Sort` | 3,000 | 92 empty, 1,166 certain to hold equal keys; the table and `Scratch_Swap` compared | 0 |
| `MapView_SetElevation` | 8,000 | 2,716 with the offset switched off, 4,086 with an argument wider than 16 bits | 0 |

Negative controls, each refusing to run: the sort leaving the *other* value
in `Scratch_Swap`, 2,724 of 3,000; comparing whole entries instead of the
high half, 1,325; Reset emptying the third list too, 600 of 600; the
elevation change not doubled, 5,284 of 8,000.

**One control passed, and was right to.** The original takes the elevation
difference from all 32 bits of its argument; a build taking it from 16 sailed
through the fuzz - because the offset it feeds is a 16-bit word and the
difference is doubled into it, so the bits in question cannot reach it. The
"quirk" had been written into the source comment and `symbols.toml` as kept
behaviour; both now say it is unobservable. A negative control that is *not*
refused is information about the claim, not only about the fuzz.

## 8. Two of the pass's own callees - taken over

`src/game/draw_emit.cpp`, 2026-09-20.

| Function | Address | Calls (`all_b`) | What |
|---|---|---|---|
| `Gfx_CommitPrim` | `0x461E50` | 640,316 | `(slot, size)`, both bytes: the primitive just built at `Gfx_PacketNext` is linked onto ordering-table slot `slot` and the packet pointer moves on by `size` - **if the pool has room, and silently not at all if it has not.** Room is `Gfx_PacketNext + size` below `0x7F1BAC + (Gfx_BufferIndex << 16)`, the pool's end less `0x54`. The slot is not checked against the eight there are |
| `DrawLayer_Close` | `0x56FE80` | 403,315 | what section 2 calls `0x56FE80(layer)`: a draw-mode primitive (texture page `0x95`) committed to slot 6, then the layer's **second** list - each layer's `0x30` bytes are three (first, last) pairs per buffer - appended to the slot `Draw_OtSlot` `0x92BF19` names |

So the two lines section 2 left unread after pass 1 -
`0x5A77C0(dword [0x7E0670], 0, 0, 0x95, 0)` and `0x461E50(6, 0xC)` - are
"build a draw-mode primitive at the packet pointer, commit its 12 bytes to
slot 6".

Start-up fuzz, `BOF3X_SHADOW=draw_emit`: 12,000 rounds, the clones' calls
re-aimed at clones of `Gpu_LinkPrim`, `Gpu_SetDrawMode` and each other; the
packet pointer kept inside the real pool - the room test compares addresses -
and half the time within `0x100` bytes of the limit (3,323 rounds with no
room); 5,538 closes that linked something; **0 mismatches**. Negative
controls, each refused: the room test strict the other way (11), the size not
masked to a byte (4,067), the draw mode not committed (4,633), the tail not
moved to the list's end (3,924), the first list for the second (6,000).

## 9. The pass itself - taken over

`Sprite_DrawPass`, `src/game/draw_pass.cpp`, 2026-09-20; 8,846 calls in
`all_b`. Section 2 is still the account of what it does. Reading it again to
write it added four things:

- **Every index is a byte and the record count is a signed byte** (`movsx`
  at `0x5933AB`, `0x59340A`): the pass stops making sense at 128 records.
  The draw list holds 40, so only `Sprite_AddDrawRecords` could get it there.
- **A tie between a sprite and a table item goes to the table** (`ja` at
  `0x59327F`), and a gather stops at a sprite whose key is not below the next
  table item's - *unless the table is used up*, tested as index `==` count,
  not `>=` (`0x593371`).
- After emitting one table item the code falls straight into the sprite
  gather; it does not go round the merge loop first.
- **Two inputs it never returns from**, by reading, not by test: a slot byte
  `Draw_OtSlot` other than 4 or 6 while a layer has a sprite, and a draw table
  out of order. Either leaves the merge loop with a sprite it will not gather
  and a table item it will not emit, and nothing moves. `DrawTable_Sort` and
  the slot's only immediate stores (4, 6) are what keep the game out of both.

Three callees are not ours - `DrawLayer_Open` `0x56FD20`,
`Sprite_AddDrawRecords` `0x57BAE0`, `Sprite_Draw` `0x5935B0`, all three named
from how the pass calls them (`hypothesis` in `symbols.toml`) - so the start-up
fuzz stands **recording stand-ins** in for them, for the original's copy (its
three calls re-aimed) and for ours (three function pointers) alike. The
stand-in for `Sprite_AddDrawRecords` adds none, one or two records by a hash,
some to draw and some to link, and returns the count in `al` under noise. The
copy's other eight calls go to copies of functions that are ours.
`BOF3X_SHADOW=draw_pass`: 3,000 rounds - 22,930 sprites with 9,424 equal-key
neighbours after the sort, 13,494 table items, 169,705 stand-in calls; the
list, the records, the ordering-table tails, the packet pool's head, the
layers, `DrawItems` and the stand-ins' log compared - **0 mismatches**.

Negative controls, each refused: the list sort not stable on its third key
(33), its second key summed without the 32-bit wrap (124), slot 4 gathering as
slot 6 does (903), the records sorted ascending (2,100), or on an unsigned
word (1,684), a linked record not moving the tail (1,720), `Sprite_Current`
not set before a draw (1,933), the third list appended under another flag bit
(183), a table item linked under another (161), `0x38` layers (2,759), no
draw mode committed first (2,218). **Three did not get as far as a verdict -
the broken build hung or faulted at start-up**: a tie going to the sprite, the
gather not ignoring a used-up table, and all of `eax` counted from
`Sprite_AddDrawRecords`. The first two are the never-returns case above,
reached from the other side; they are evidence for the reading, not a
comparison passed.

A trap in the harness, not the function: the layers' block restored 2 KB too
far runs over `Sprite_DrawListCount`, and the *original's copy* then walks a
list as long as a random byte says, of garbage pointers, and faults during the DLL's load - which looks exactly
like a hang. Process CPU time told them apart.

## 10. Live checks, 2026-09-20

All hands-off runs from a fresh launch, everything ours, against the
all-original references of 2026-09-19.

| Owned | `attract_diff.py orig_a.tsv` | `mem_dump.py --compare clutref_a` | Frame hash, original vs ours |
|---|---|---|---|
| 30 (sections 3, 4) | identical, 7,478 frames | arena, vram, clut identical | - |
| 33 (plus section 5's first three) | identical, 7,478 frames | identical | identical, 4,528 frames (`ab5_orig` / `ab5_ours`) |
| 34 (plus `Field_CopyInput`) | identical, 7,478 frames | identical | identical, 4,530 frames (`ab6_orig` / `ab6_ours`) |
| 41 (plus section 6) | identical, 7,478 frames | identical | identical, 4,604 frames (`ab7_orig` / `ab7_ours`) |
| 44 (plus section 7) | identical, 7,478 frames | identical | identical, 4,609 frames (`ab8_orig` / `ab8_ours`) |
| 108 (the library layer's x87 and transform functions, then section 8) | identical, 7,478 frames | identical | identical, 7,937 frames, with original against original identical beside it (`ab13_orig` / `ab13_origb` / `ab13_ours`) |
| 109 (plus section 9, the pass itself) | identical, 7,478 frames | identical | identical, 7,936 frames, with original against original identical beside it (`ab14`) |

The frame hash needs its all-original side re-recorded whenever a *logic*
function changes hands, because owned functions are left unarmed in both
configurations ([`call-trace.md`](call-trace.md) §7); `ab5` to `ab8` are those pairs, each side's `inject:` line checked in its saved `bof3x.log`.

With 41 the identical arena is a stronger statement than before:
`Sprite_RestoreClut` writes `Gfx_ClutStrip`, which is inside it.

What none of this reached: the attract sequence has nobody in state `0x20`
and no input, so `Field_CopyInput`'s exchange rests on the fuzz alone, as does
any `Sprite_FindNearby` hit in the extra four. Whether the others' edge cases
occur in the attract run was not measured.

## 11. Not done

- **`0x57C0A0`** (14,312 calls, third in the queue) was read and left. For an
  argument byte `c` without bit 7 it returns `(c & 0x3F) + 0x1E`. With bit 7 it
  walks 30 records of `0xA4` bytes looking for the `(c & 0x3F)`-th whose byte
  at `0x7DEE86 + n * 0xA4` is `0x0A` - and then returns `c & 0x3F` in `al`
  whatever it found: the loop's index lives in `dl` and never reaches `al`.
  Both callers (`0x5192AC`, `0x58977F`) read `al`. Section 5 says what it is
  for: the records are `Sprite_Objects`, the byte is the type, and the result
  is an object number - `+ 0x1E` being `Sprite_ObjectsExtra`. So a handle with
  bit 7 means "the n-th type-`0x0A` object" and comes back as plain `n`, which
  is only right while those objects are the first in the array. Either the
  search is dead code in the source too, or the port lost a `return`. **The PSX side can
  say which**; not looked up. It should not be taken over before that is
  known - not because it is hard, but because it may be a defect to record.
- The pass's three callees that are not ours. It was taken over before them
  (section 9): it only calls them.
  - **`0x56FD20(layer)`**, 403,315 calls: **ours since 2026-09-21, section
    15.** Was: appends the layer's *first* list to
    slot 6 (`0x929EB8` directly), then walks a window of the word table
    `0x904F20` - 28 columns, wrapping, rows offset by the words at `0x929F20`
    and `0x929F24` - and for each non-zero word a run of 4-byte records under
    `0x8CB580`, each handed to a handler chosen by its top byte through the
    table **`0x663008`**. By its place the map's cells; what the records are
    was not read. An indirect call per record: no clone, check it live.
  - **`0x57BAE0(record)`**, 7,824 calls, `0x3E1` bytes: **ours since
    2026-09-21, section 12.** Was: not read. Its call
    tree (`0x57BED0`, `0x57BFF0`, `0x57C070`) is matrix work through the GTE
    and one `Gpu_SetPolyFT4`; every library call in it is ours except the
    matrix product `0x5A7D70`
    ([`psx-library-layer.md`](psx-library-layer.md) section 4 says why not).
  - **`0x5935B0`**, 33,850 calls: **ours since 2026-09-21, section 14.**
    `0x593860` under it turned out to be 213 bytes, not 2,644; the rest was
    other functions.
  The pass is still checkable as memory - the linked list it builds - which
  is [`IDEAS.md`](IDEAS.md) I14 level 1, and would be the first check of it
  that is not the fuzz's stand-ins or an identical frame hash.
- Whether the PSX side has a name for any of this. Not looked up.

## 12. A 3D object's quads - taken over (2026-09-21)

`src/game/sprite_records.cpp`. `Sprite_AddDrawRecords` `0x57BAE0` was the
pass's second callee still Capcom's (section 11). It waited on the matrix
product, and with that ours (DIV-0021) its whole call tree is ours.

| Function | Address | Calls (`all_b`) | What |
|---|---|---|---|
| `Sprite_AddDrawRecords` | `0x57BAE0` | 7,824 | The current object's quads as `POLY_FT4`s of 0x48 bytes in the packet pool, and one 12-byte record per quad for the pass to sort |
| `Sprite_ObjectMatrix` | `0x57BFF0` | 7,824 | The object's matrix. The position is `(+0x34 >> 9) - 0x4000`, `(+0x38 >> 9) - 0x4000`, and `-(s16 +0x3E / 2)` rounded toward zero, taken through the current GTE matrix into the translation. The low words of `+0x64` / `+0x68` / `+0x6C` are the rotation's angles |
| `Light_ObjectDirection` | `0x57BED0` | 7,824 | The first row of `Light_Matrix` `0x803560`. `(0, 0, Light_Angles[3])` is turned by the light's angles `0x903598`, then by the object's rotation run through `Gte_TransposeMatrix` in place |

What the main function does:

1. Pushes the GTE matrix.
2. Builds and loads the object's matrix, scaled by the dword at `+0x40`
   unless byte `+0x48` is set.
3. Sets the light direction, loads a copy composed with the camera
   (`Camera_LoadMatrix`), and loads `Light_Matrix` as the second matrix.
4. Takes the count: the `s8` at byte 0 of the object's `+0x54` data, widened
   to a `u16`. If `count * 0x48` bytes from `Gfx_PacketNext` would pass the
   buffer's limit, it **returns 0 without popping**.
5. For each 0x28-byte quad under `+0x50`:
   - its four vertices go through `Prim_VertexScratch` `0x9037A0` and
     `Gte_Rtps` into the primitive's screen points and float depths;
   - the colour comes from `+0x5D..+0x5F`, lit through `Gte_NormalColor` when
     bit 7 of byte 3 of the data is set;
   - the tpage is for abr `(bit 6 ? that byte : 2) & 3` at `(0x2C0, 0x100)`,
     the CLUT is at `(quad word 0 << 4, 0x1E3)`, and semi-transparency is
     bit 6;
   - the record is: the primitive, the largest depth / 4, the first depth
     / 4, and the object.
6. Advances `Gfx_PacketNext`, pops, and returns the count's low byte, which
   the pass adds to its record count.

Quirks kept, each in the code's comment:

- A negative count is some 65,000 quads, which the room check then refuses.
- When there is no room, the matrix stays pushed.
- The record's depth is a `u16`, compared zero-extended against the whole
  long. **The pass then sorts on it as an `s16`** (section 2), so a depth
  past 0x7FFF sorts nearest.
- `Gte_TransposeMatrix` in place is not a transpose. The upper triangle is
  lost, so the light direction is not the one intended. Nothing shows it,
  because `Gte_NormalColor` as shipped copies the unlit colour over the lit
  one ([`psx-library-layer.md`](psx-library-layer.md) §4).

**Checks.** Start-up fuzz, `BOF3X_SHADOW=sprite_records`. This module
injects *first*, so while it runs every one of the three originals' ~25
callees is still Capcom's. The clones' calls go where the originals went, and
Capcom's whole call tree runs against ours; the two helpers' clones stand in
for the helpers.

Each round randomises a fake object with its quads and data, the GTE's
globals (`0x7DE428..0x7DE7A8`), the light and scratch globals, the camera,
`Gfx_PacketNext`, `Gfx_BufferIndex`, `Sprite_Current` and a 0x300-byte
window of the packet pool. All of that and the result's low byte are
compared. Half the rounds are a scene rather than noise, with depths around
0..0xFFFF. A matrix's padding word is compared apart (DIV-0021).

Result: 12,000 rounds, 32,476 quads, 3,925 with no room, 1,206 empty, 772
negative counts, 5,101 lit, 7,998 unscaled, 5,947 scenes: **0 mismatches**.
The padding differed in 2,121 rounds.

Negative controls, each refused:

| Control | Mismatches |
|---|--:|
| The depth compared as `s16` | 217 (scenes only) |
| A pop before the no-room return | 3,728 |
| A true transpose | 11,999 |
| Semi-transparency from bit 5 | 1,827 |
| The count as a `u8` | 417 |
| z by `>> 1` for `/ 2` | 2,983 |
| The CLUT row `0x1E2` | 7,243 |

Live: the attract oracle is identical with all 125 ours, over the 1,202
frames a 3-minute run compares (2026-09-21, pinned to the original
language); then the batch check, 2026-09-21, all 131 ours (`analysis/attract/ab17_cycle.log`):
the full-cycle oracle identical over 7,478 frames; the memory dump identical
in the arena, VRAM and the CLUT; the frame hash identical on all 10,062
frames, original against original and original against ours (`ab17_*`,
now the reference).

## 13. The texture word - taken over (2026-09-21)

`src/game/prim.cpp`, next to `Prim_SetShade`. `Prim_SetTexture` `0x572A00`
(1,225 bytes, 123,820 calls in `all_b`, 91 call sites) textures a run of
`count` `POLY_FT4`s of 0x48 bytes from one packed word. Three of its callers
are `0x56F9B0`, `0x570660` and `0x571500`, near the map view. It was the
queue's biggest x87 leaf; the float work turned out to be exact (below).

The word, bit by bit:

| Bits | What |
|---|---|
| 0..11 | where the four `(u, v)` come from - below |
| 12, 13 | the semi-transparency mode, into the tpage |
| 15 | semi-transparency on (`Gpu_SetSemiTrans`) |
| 16 / 17 / 18 | mirror left to right / top to bottom / a quarter turn |
| 19..23 | the shade, `r = g = b = (word >> 16) & 0xF8` |
| 24..27 | the palette: a 4-bit texture's CLUT column `16n`, an 8-bit one's row `0x1E3 + n` |
| 28, 29 | the texture page, `x = 320 + 128n`, `y = 256` |
| 31 | set: 4-bit texture; clear: 8-bit |

The corners, in `POLY_FT4` order (top left, top right, bottom left, bottom
right), come from one of three sources:

- **bits 8..11 clear**: a 16-texel cell of a 16 x 16 grid. Bits 0..3 are its
  column and 4..7 its row; the corners are 15 apart.
- **bit 11 set**: two dwords of the area block `AreaMap_Header` `0x8CB580`,
  at dword `u16[+6 + 4n] + 2 * (bits 0..10)`, the four corners a byte each.
- **otherwise**: one dword at dword `u16[+4 + 4n] + bits 0..7` - the top
  left's `u` and `v`, then a width and a height.

The quarter turn is applied first (corners 0..3 take old 2, 0, 3, 1), then
the two mirrors. A count of 0 or less draws nothing.

**Why integers, not x87.** The original loads each coordinate into x87 as a
float, does one float add for the rectangle's far edges, and converts each
back through `_ftol`. Every value is an integer of magnitude under 400, so
each step is exact at any precision and in any rounding mode, and `_ftol`
truncates. Integer arithmetic gives the same result; section 3's
`long double` question of [`psx-library-layer.md`](psx-library-layer.md)
does not arise. The function also swaps four float locals it never writes,
along with the corners - stale stack that never reaches the primitive.

**Checks.** Start-up fuzz, `BOF3X_SHADOW=prim`, against a clone whose nine
calls (`_ftol` x 8, `Gpu_SetSemiTrans`) still go to Capcom's code
(`Prim_Inject` runs before `PsxGpu_Inject`). Each round sets a random word,
biased a third to each source, a random run of up to five primitives plus a
guard, and random area tables. The clone runs a quarter of the rounds under
each of the x87 control words `027F` (the game's), `007F`, `037F` and `0F7F`
(round toward zero).

Result: 64,000 rounds - 21,330 from the grid, 21,337 four-corner, 21,333
rectangles; 31,971 turned, 47,974 flipped, 19,676 empty, 132,945 primitives
drawn - **0 mismatches** under all four words.

Negative controls:

| Control | Mismatches |
|---|--:|
| The turn the other way | 22,131 |
| Grid corners 16 apart | 14,680 |
| An 8-bit CLUT row by `\|` not `+` | 16,585 |
| The shade keeping the flip bits | 38,734 |
| The mirrors before the turn | 11,053 |
| The tpage's x from `0x100` | 44,324 |
| The four-corner `u0` unsigned | **0** |
| The rectangle's `u` unsigned | **0** |

The last two are not refused, and cannot be: only a coordinate's low byte
reaches the primitive, and the sign does not change it, with or without the
width added. Ours keeps the sign only so that it reads like the original; the
code's comment says so.

Live: the attract oracle is identical with all 126 ours over the 1,202
frames a 3-minute run compares (2026-09-21, `analysis/attract/ab18_smoke.tsv`);
then the batch check, 2026-09-21, all 131 ours (`analysis/attract/ab17_cycle.log`):
the full-cycle oracle identical over 7,478 frames; the memory dump identical
in the arena, VRAM and the CLUT; the frame hash identical on all 10,062
frames, original against original and original against ours (`ab17_*`,
now the reference).

## 14. The sprite draw - taken over (2026-09-21)

`src/game/sprite_draw.cpp`. `Sprite_Draw` `0x5935B0` was the pass's last
callee still Capcom's apart from `DrawLayer_Open` (section 11). It had been
filed as 3.3 KB with the 2,644-byte `0x593860` under it. **`0x593860` is 213
bytes.** A four-entry jump table follows it at `0x593938`, and `pe_funcs.py`
ran on through the table into some ten functions reached only through
pointers (the next paragraph). So the whole branch was about 900 bytes, and
four functions:

| Function | Address | Calls (`all_b`) | What |
|---|---|---|---|
| `Sprite_Draw` | `0x5935B0` | 33,850 | The current object as one primitive of the port's own - GPU code `0x84`, 0x20 bytes - and one cell record per piece of its frame |
| `Sprite_ClutWord` | `0x593860` | 23,950 | The object's CLUT word: palette `+0x27`, 2^s palettes to a row with s = 4, 0, 3, 2 for `+0x28` = 0..3 and 1 above, from row `0x1E0` (`0x1F0` with bit 2 of `+0x24`) |
| `SpriteCell_Add` | `0x5A6790` | 109,000 | An 8-byte record at `SpriteCell_Table` `0x6BEA18` [`SpriteCell_Count` `0x7CC374`]: u16 x, u8 y, u8 size `((h & 0xF8) << 1) \| w >> 3`, u16 flags, u8 u, u8 v; returns its index. No bound |
| `SpriteCell_Reset` | `0x5A6780` | 12,801 | The count to 0, once a logic frame (WinMain, `0x4FCF75`) |

What `Sprite_Draw` does:

1. Bit 6 of byte 0 set: returns.
2. Bit 7 of `+0x24` clear and `+0x48 == 1`: the scale `+0x40` = `+0x44` =
   `(0x4650000 / +0x60) & ~0xFF` - 1125 over a depth, in 16.16. A depth of 0
   stores 0 to both and returns before the cull.
3. The cull: the s16 position `+0x2E` outside `-0x40..0x180`, or `+0x30`
   outside `-0x40..0x130`, sets bit 7 of byte 0 and returns; inside, clears it.
4. The frame: a count byte at `+0x54`'s data plus the u16 `+0x5A`, then that
   many 5-byte pieces. A count of 0 returns.
5. At `Gfx_PacketNext`: code `0x84`, the CLUT word at `+0x1C`, then per piece
   a `SpriteCell_Add` - signed x and y offsets, flags
   `((piece bit 7 | wide << 5) << 2) | +0x25`, u, `+0x26` + v, and a size from
   `SpriteCell_Sizes` `0x66A450` by the piece's low nibble (8..32 by 8..32).
   The first index goes to `+0x18` and the count to `+0x1A`. Then a mode word
   `((+0x5C | wide << 2) << 5) | +0x25`, `| 0x400` with `+0x2A`, at `+0x1E`;
   the dwords `+0x74` / `+0x78` at `+8` / `+0xC`; the scale as floats (x87,
   an integer times 2^-16 - exact) at `+0x10` / `+0x14`, 1.0 when `+0x48` is
   0; the shade `+0x5D..+0x5F` each plus `0x80`; semi-transparency from bit 5
   of byte 0. "Wide" is `+0x28` non-zero.
6. `Gfx_CommitPrim(+0x29, 0x20)`.

So the PC port does not build a sprite from `SPRT`s or `POLY_FT4`s at all: it
emits one code-`0x84` primitive and a run of cell records, and the Direct3D
end (`0x5A32B0`, the table's one reader, unread) draws the cells.

**Hidden functions found on the way.** The bytes `pe_funcs.py` gave to
`0x593860` hold a dispatcher at `0x593950` - `jmp [0x66A470 + byte
0x93985C * 4]` - and the handlers its tables name, `0x593960`..`0x594240`,
about ten. `tools/pe_hidden.py` missed them too. Its rule wants a hidden
entry to follow a `ret` or `jmp` and padding, and the first one follows the
jump table's data. After that, a linear sweep through the table's bytes falls
out of step. None of them is in `entries.txt`, so no call trace has counted
them. [`attract-remaining.md`](attract-remaining.md) section 3's hidden count is
short by these at least, and any function with an inline jump table may hide
more the same way.

**Checks.** Start-up fuzz, `BOF3X_SHADOW=sprite_draw`, against clones. The
module injects before `PsxGpu_Inject` and `DrawEmit_Inject`, so the clone's
calls to `Gpu_SetCode84`, `Gpu_SetSemiTrans` and `Gfx_CommitPrim` still run
Capcom's code. Its calls to the other two go to their clones. The clone of
`Sprite_ClutWord` jumps through the original's table into the original's
body - Capcom's bytes either way, since nothing is patched yet.

Each round randomises an object and its frame. The position is biased to the
cull's edges and the depth to 0, ±1 and small values. The packet pointer sits
inside the real pool, a quarter of the time at the room test's limit. The
ordering-table pointers are aimed at a scratch row of tails, and the cell
count is under 0x200 with a random window of the table. One round in sixteen
is `SpriteCell_Reset`. Compared: the object, its frame, the cell window and
count, the packet pointer and 0x40 bytes at it, the ordering-table pointers
and their tails.

Result: 24,000 rounds - 2,797 hidden, 239 at depth 0, 12,780 culled, 624
empty frames, 6,043 drawn with 237,190 cells (4,461 scaled, 1,884 with no
room in the pool), 1,517 resets - **0 mismatches**. `Sprite_ClutWord` alone,
65,536 rounds, and `SpriteCell_Add` alone with whole random dwords, 65,536
rounds: **0** each.

Negative controls, each refused:

| Control | Mismatches |
|---|--:|
| Cull x above `0x17F` | 925 |
| The scale keeping its low byte | 3,423 |
| Depth 0 going on to the cull | 157 |
| The cull leaving bit 7 set | 3,352 |
| The first index from every piece | 5,401 |
| A piece's x zero-extended | 5,425 |
| The shade plus `0x7F` | 6,043 |
| "Wide" from bit 0 of `+0x28` | 1,524 |
| The CLUT's shift 0 above mode 3 | 5,923 (and 32,341 of the CLUT rounds) |
| The cell height masked with `0xF0` | 5,462 (and 16,597 of the add rounds) |
| Reset to 1 | 1,517 |

Two tries at controls that said nothing, for the record: letting depth 0 fall
through to the division hangs the game at start-up (the trap of
[`psx-library-layer.md`](psx-library-layer.md) section 3), and
`(h >> 3) << 4` is `(h & 0xF8) << 1` written differently.

**Live.** With the switch set, every call in game runs the clone, puts back
what it wrote, runs ours, and compares the object, the primitive, the packet
pointer, the cell records and count, and the ordering-table slot with its
tail. 2026-09-21: a 6-minute attract run, 8,192 calls compared (5,570 drawn
with 25,174 cells; the report then came every 8,192 calls, so the run's
last few thousand went unlogged - it is every 1,024 now), and save 5's field
through a recipe, 1,024 calls (341 drawn, 1,304 cells): **0 mismatches**.
The attract oracle was identical with the check on, over 3,926 frames
(`analysis/attract/ab18_live.tsv`), and a capture of the field
(`analysis/shots/sprite_field/`) shows Ryu drawn as he should be. Then the batch check, 2026-09-21, all 131 ours (`analysis/attract/ab17_cycle.log`):
the full-cycle oracle identical over 7,478 frames; the memory dump identical
in the arena, VRAM and the CLUT; the frame hash identical on all 10,062
frames, original against original and original against ours (`ab17_*`,
now the reference).

## 15. The layer's map cells - taken over (2026-09-21)

`src/game/draw_emit.cpp`, beside `DrawLayer_Close`. `DrawLayer_Open`
`0x56FD20` (0x159 bytes, 403,315 calls in `all_b`) was the draw pass's last
callee still Capcom's. With it ours, **every function the pass calls is
ours**; what those handlers call is not.

What it does, per layer:

1. The layer's first list - the (first, last) pair at
   `DrawLayers + (layer * 6 + Gfx_BufferIndex) * 8` - if non-empty, is
   appended to ordering-table slot 6 through `Gpu_LinkPrim`, and its last
   becomes slot 6's tail. The index is read again after the call, as in
   `DrawLayer_Close`.
2. One row of the field's map cells. The row is `(MapView_Row` `0x929F24` `+
   layer + 1) mod 0x38` of `MapView_Cells` `0x904F20`, 28 words wide. The
   inset is `MapView_Inset` `0x905D80`, less 2 (not below 0) with bit 0 of
   `Field_InputFlags`. From `(MapView_Column` `0x929F20` `+ inset + (1 if
   the row is even)) mod 0x1C` it takes `(14 - inset) * 2` columns, each
   advanced before it is read, wrapping `0x1B` to 0. The half-column shift on
   even rows is the diamond grid of an isometric map.
3. A non-zero cell word `w` names a run at dword `w + (AreaMap_CellBase &
   0xFFFF)` (`0x8CB5A4`) of the area block. The dword before the run heads
   it: the run's length in dwords plus one in its high half, and two bytes
   every handler is given. Each 4-byte record goes to
   `MapCell_Handlers[top byte]` (`0x663008`, 77 handlers) as `(record, byte
   1, byte 0)`, and the walk moves on by the record's byte `+2` in dwords,
   **read after the call**.

Kept as the original has it: each "mod" is one subtraction; the record's top
byte is not checked against the 77; a step of 0, or steps that pass the run's
end, never stop. Handler `0x570660`, the one read so far, draws through
`Prim_SetTexture` (section 13) - so a record is a piece of a cell's picture,
by that one example.

**Checks.** Start-up fuzz, `BOF3X_SHADOW=draw_emit`, against a clone whose
`Gpu_LinkPrim` call is Capcom's. The clone reads `MapCell_Handlers` just as
ours does, so for the fuzz the table's 77 entries become four recording
stand-ins, put back afterwards. Each round randomises the layers, the buffer
byte (0..5), the view's row, column and inset, the input flag, and a row of
cells naming up to 24 record runs laid out in the area block. In half the
rounds every record is one dword and a stand-in lengthens a third of the
records it is handed to two, where the run has room - so a walk that read
the step before the call would go astray. Compared: the stand-ins' log (which
one, which record, both bytes), the layers, the area window, the cells, slot 6
and its tails.

Result: 12,000 rounds - 7,926 linked the first list, 11,012 walked cells with
219,759 handler calls, 5,907 with lengthened steps, 499 with no columns, 6,065
rows wrapped - **0 mismatches**. The draw pass's own fuzz, which stands in
for this function, still passes.

Negative controls:

| Control | Mismatches |
|---|--:|
| The step read before the call | 5,262 |
| The column read before it advances | 3,348 |
| Odd rows shifted instead of even | 3,293 |
| The inset not floored at 0 | 1,187 |
| Half the columns | 10,200 |
| Wrap after `0x1C` | 3,475 |
| The two bytes swapped | 11,007 |
| Row wrap at `0x37` | 5,855 |
| The first list to slot 7 | crashed at start-up - slot 7's pointer is still null then; not a clean refusal |

**Live.** The handlers draw, so a call cannot be run twice and compared the
way section 14's is. Instead: save 5's field through a recipe, once with the
whole draw path original (117 functions; only file, save, text and input
ours) and once all ours - **the four captures identical pixel for pixel**
across the frame (2026-09-21, `analysis/shots/sprite_field_orig/`,
`sprite_field_ours/`). Then the batch check, 2026-09-21, all 131 ours (`analysis/attract/ab17_cycle.log`):
the full-cycle oracle identical over 7,478 frames; the memory dump identical
in the arena, VRAM and the CLUT; the frame hash identical on all 10,062
frames, original against original and original against ours (`ab17_*`,
now the reference).
