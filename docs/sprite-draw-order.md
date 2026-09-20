# The sprite draw-order pass, `0x593060`

**Status:** IN PROGRESS (2026-09-20)

What the regenerated takeover queue ([`call-trace.md`](call-trace.md) §9) put
first: its two hottest layer-0 logic functions are the exchange helpers of one
sort, and that sort is the pass that decides the order field sprites are drawn
in. The helpers are ours; the pass itself is read but not taken over.

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

## 4. Not done

- **Live checks of the two helpers** - `attract_diff.py` against
  `orig_a.tsv`, `mem_dump.py --compare`. Both are logic functions in
  `entries_logic.txt`, and owned functions are left unarmed, so **the
  frame-hash reference `analysis/calltrace/ab3_orig/` must be re-recorded**
  before the next frame-hash A/B ([`HANDOFF.md`](HANDOFF.md)).
- `0x593060` itself. Its unread callees: `0x56FD20`, `0x56FE80`, `0x57BAE0`,
  `0x5935B0` (683 bytes, the sprite draw), `0x5A7560`, `0x5A77C0`, `0x461E50`.
  It is checkable without Direct3D if `0x5A7560` is what it looks like: the
  linked list it builds is memory, which is [`IDEAS.md`](IDEAS.md) I14 level 1.
- Whether the PSX side has a name for any of this. Not looked up.
