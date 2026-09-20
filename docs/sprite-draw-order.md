# The sprite draw-order pass, `0x593060`

**Status:** IN PROGRESS (2026-09-20)

What the regenerated takeover queue ([`call-trace.md`](call-trace.md) §9) put
first, and where it led: its two hottest layer-0 logic functions are the
exchange helpers of the pass that decides the order field sprites are drawn
in, and the next half-dozen all touch the same structures - the sprite object
arrays, the view's cell grid, the draw-item pool, the animation script.
Sixteen functions are ours (sections 3 to 6); the pass itself is read but not taken over.

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

Read to its `ret` at `0x59352A`; the callees named below are **not** read, so
what they are for is inference from how they are called, and is marked so.

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
  ordering table, kept by the port.

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

## 7. Live checks, 2026-09-20

All hands-off runs from a fresh launch, everything ours, against the
all-original references of 2026-09-19.

| Owned | `attract_diff.py orig_a.tsv` | `mem_dump.py --compare clutref_a` | Frame hash, original vs ours |
|---|---|---|---|
| 30 (sections 3, 4) | identical, 7,478 frames | arena, vram, clut identical | - |
| 33 (plus section 5's first three) | identical, 7,478 frames | identical | identical, 4,528 frames (`ab5_orig` / `ab5_ours`) |
| 34 (plus `Field_CopyInput`) | identical, 7,478 frames | identical | identical, 4,530 frames (`ab6_orig` / `ab6_ours`) |
| 41 (plus section 6) | identical, 7,478 frames | identical | identical, 4,604 frames (`ab7_orig` / `ab7_ours`) |

The frame hash needs its all-original side re-recorded whenever a *logic*
function changes hands, because owned functions are left unarmed in both
configurations ([`call-trace.md`](call-trace.md) §7); `ab5`, `ab6` and `ab7`
are those pairs, each side's `inject:` line checked in its saved `bof3x.log`.

With 41 the identical arena is a stronger statement than before:
`Sprite_RestoreClut` writes `Gfx_ClutStrip`, which is inside it.

What none of this reached: the attract sequence has nobody in state `0x20`
and no input, so `Field_CopyInput`'s exchange rests on the fuzz alone, as does
any `Sprite_FindNearby` hit in the extra four. Whether the others' edge cases
occur in the attract run was not measured.

## 8. Not done

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
- `0x593060` itself. Its unread callees: `0x56FD20`, `0x56FE80`, `0x57BAE0`,
  `0x5935B0` (683 bytes, the sprite draw), `0x5A7560`, `0x5A77C0`, `0x461E50`.
  It is checkable without Direct3D if `0x5A7560` is what it looks like: the
  linked list it builds is memory, which is [`IDEAS.md`](IDEAS.md) I14 level 1.
- Whether the PSX side has a name for any of this. Not looked up.
