# The port's PSX library layer, `0x5A7540`..`0x5A94BE`

**Status:** IN PROGRESS (2026-09-20)

The porting house did not rewrite the game's calls into Sony's libraries; it
wrote the libraries. Between the game code and Direct3D sits a layer with
libgpu's and libgte's entry points - `SetPolyFT4`, `getTPage`, `ClearOTagR`,
`ApplyMatrix`, `PushMatrix` - working on globals where the PlayStation had a
coprocessor. It is where the attract run spends its calls: the takeover
queue's 38 hottest layer-0 logic functions are in it, all but one
(`python tools/calltrace.py queue ...`, 2026-09-20).

This matters beyond the call counts. It is the seam
[`PLAN.md`](PLAN.md) wants: game logic above it is PSX-shaped and can stay so;
everything platform-specific is below it.

All addresses are `BOF3.exe`; disassembly by `python tools/pe_disasm.py`,
2026-09-20. `symbols.toml` has the instruction-level evidence per function.
**Names are Sony's only where the binary identifies the function outright** -
a GPU code byte, a bit layout - and say so; nothing here was matched against
the PSX binary.

## 1. The GPU half - fourteen functions, ours

`src/game/psx_gpu.cpp`.

| Function | Address | Calls | Identified by |
|---|---|---|---|
| `Gpu_LinkPrim` | `0x5A7560` | 892,889 | `*tail = item`; the append [`sprite-draw-order.md`](sprite-draw-order.md) §2 inferred |
| `Gpu_SetDrawMode` | `0x5A77C0` | 137,406 | libgpu's argument order; but the packet is `0xE8000000 \| tpage`, flags in bits 16-17, not the PSX `0xE1` |
| `Gpu_SetSemiTrans` | `0x5A7780` | 117,292 | bit 1 of the code byte `+7` |
| `Math_Sin` | `0x5A7A00` | 110,034 | 4096-step angle, quarter-wave table of 1,025 words at `0x66BC58`, mirrored and signed |
| `Gpu_SetPolyFT4` | `0x5A75D0` | 75,728 | code `0x2C` |
| `Gpu_GetClut` | `0x5A79E0` | 67,410 | `((x >> 4) & 0x3F) \| (y << 6)` |
| `Gpu_AddPrim` | `0x5A7540` | 32,768 | push at a list head, keeping bit 31 of the primitive's link |
| `Gpu_SetCode6C` | `0x5A7760` | 22,268 | code `0x6C` - no libgpu primitive; named by the code |
| `Gpu_SetSprt` | `0x5A7710` | 10,863 | code `0x64` |
| `Gpu_SetShadeTex` | `0x5A77A0` | 9,156 | bit 0 of the code byte |
| `Gpu_SetCode84` | `0x5A7770` | 7,064 | code `0x84` - likewise |
| `Gpu_SetLineF2` | `0x5A7650` | 4,273 | code `0x40` |
| `Gpu_ClearOTagR` | `0x5A7960` | 4,099 | reverse-linked table, terminator `0x66BC40` |
| `Gpu_GetTPage` | `0x5A79A0` | 3,937 | libgpu `getTPage`, bit for bit |

Two things the port changed, both visible here:

- **A primitive is not the PSX struct.** The setters store the float `0.01`
  (`0x3C23D70A`) at `+0x10` and every `0x10` after it for a four-vertex
  primitive, `+0x10` and `+0x1C` for a line. The PSX primitives have no room
  there. By the look of it a per-vertex depth or reciprocal-w for Direct3D;
  unread.
- **Every list link carries bit 31.** `ClearOTagR` sets it on each link and
  `AddPrim` preserves it from the primitive's own first dword. On the PSX the
  top byte of a tag is the primitive's length; what bit 31 means here is
  unread.

Start-up fuzz against clones, `BOF3X_SHADOW=psx_gpu`: 47,884 rounds over the
fourteen, **0 mismatches**; ours always called through the signature the
game's callers assume, with noise above whatever the original reads.
`Math_Sin` sees every 13-bit angle twice. Negative controls, each refused:
the sine's mirror off by one (7,776 rounds), the terminator always at
`ot[0]` (57 - the original puts it at `ot[n - 1]` for `n < 2`, outside the
table for `n < 1`), the texture window stored only with `dtd` (970),
`AddPrim` dropping the flag (1,996), `SetSemiTrans` on any non-zero argument
rather than bit 0 (473).

Not observable, so not claimed: that `getTPage` and `getClut` shift
arithmetically - the masks that follow hide it.

## 2. The GTE half

The coprocessor's registers are globals at `0x7DE4xx`: `Gte_Matrix`
`0x7DE4A0` is a PSX `MATRIX` to the byte (nine `s16`, two of padding, three
`s32` of translation); `Gte_MatrixStack` `0x7DE500` holds twenty of them where
the PSX's `PushMatrix` had room for one; three 8-byte vertices at `0x7DE468`;
three screen points at `0x7DE4C8`, a pair of dwords each where the register
was two `s16`.

Fifteen integer functions, ours, `src/game/psx_gte.cpp`:

| Function | Address | Calls | What |
|---|---|---|---|
| `Gte_ApplyMatrix` | `0x5A7BF0` | **2,625,115** | libgte `ApplyMatrix(MATRIX *, SVECTOR *, VECTOR *)`: rotation times vector, `>> 12`. The most-called function of the attract run |
| `Gte_LoadVertex` | `0x5A8E30` | 1,066,958 | one 8-byte vertex into the *middle* of three slots |
| `Gte_StoreScreenXY` | `0x5A90B0` | 1,066,958 | the newest of the three screen points out |
| `Gte_LoadVertices3` | `0x5A8E50` | 487,822 | three vertices: first to the middle slot, second to the first, third to the last |
| `Gte_StoreScreenXY3` | `0x5A90D0` | 487,822 | all three screen points out, oldest first |
| `Gte_StoreDepthQuarter` | `0x5A94B0` | 270,659 | `*out = Gte_Depth >> 2` |
| `Gte_SetRotMatrix` | `0x5A8DE0` | 18,339 | 5 dwords to `Gte_Matrix` |
| `Gte_SetTransMatrix` | `0x5A8E00` | 18,339 | the 3 dwords at `+0x14` to `Gte_Matrix + 0x14` |
| `Gte_PushMatrix` | `0x5A7B90` | 12,721 | silent when the stack of 20 is full |
| `Gte_PopMatrix` | `0x5A7BC0` | 12,721 | silent when it is empty |
| `Gte_TransMatrix` | `0x5A8100` | 9,878 | libgte `TransMatrix`: a `MATRIX`'s translation from a `VECTOR` |
| `Gte_SetMatrix2` | `0x5A8DA0` | 2,843 | 8 dwords to a second matrix at `0x7DE4E0`; which one is unread |
| `Gte_ApplyMatrixLV` | `0x5A7CF0` | 69,415 | libgte `ApplyMatrixLV`: `ApplyMatrix` instruction for instruction, with 32-bit vector components, so the products wrap |
| `Gte_RotTrans` | `0x5A8200` | 12,721 | `ApplyMatrix` with the current matrix, then its translation added; libgte `RotTrans` without the flag argument. Its one call is to `Gte_ApplyMatrix`; the clone's goes to that function's clone |
| `Gte_TransposeMatrix` | `0x5A81B0` | 2,843 | libgte `TransposeMatrix`, nine `s16` read and stored one at a time - in place it does **not** transpose, and ours keeps the order |

`0x5A7C70`, the `s16`-out variant between the two `ApplyMatrix`es, is not
reached by the attract run and was left.

Start-up fuzz against clones, `BOF3X_SHADOW=psx_gte`: 24,000 rounds, every
register block randomised each round, 2,639 with the stack full or past it,
3,475 with it empty or below, 5,963 with overlapping arguments; every block
and the arguments compared; **0 mismatches**. Negative controls refused:
`PushMatrix` allowed at depth 20 (106 rounds), vertices loaded in the natural
order (2,000), `ApplyMatrixLV` with the vector cut to 16 bits (1,600), a
`TransposeMatrix` that reads all nine before storing any (412, all of them
overlapping rounds), `RotTrans` without z's translation (1,600).

**The control that was not refused, and what it changed.** A
`Gte_ApplyMatrix` that stores each row as it goes - wrong when `out` overlaps
the matrix - passed. Not because the fuzz lacked overlapping rounds: because
the matrix is `const short *` and `out` is `long *`, and under C++'s
strict-aliasing rule the compiler may assume a store through one never
changes a read through the other, and it moved the reads up. The optimiser
had repaired the broken build. **`-fno-strict-aliasing` is now a project-wide
compile option** (`CMakeLists.txt`): the 2001 source read and wrote game
memory through whatever type it liked, and a reimplementation that may be
handed the same overlapping pointers has to be compiled to mean what it says.
With the flag the same wrong build fails 307 of 24,000 rounds. All 25
start-up self-tests were re-run under the new flag (`BOF3X_SHADOW=*`), 0
mismatches in each.

**Not taken over, deliberately:** the functions that go through x87 -
`0x5A8340` (a clamp-and-scale ending in `_ftol`), `0x5A8380` (2,543,047
calls, 32 floating-point instructions: the perspective transform, by its
place), `0x5A9110`, `0x5A9130`, `0x5A9290`. Being faithful there means
reproducing x87 rounding and the precision-control word the game runs under,
not just the arithmetic; that wants its own session and a fuzz that compares
bit patterns.

## 3. Live checks

All hands-off from a fresh launch, everything ours, against the all-original
references of 2026-09-19; the frame hash with both sides re-recorded
([`call-trace.md`](call-trace.md) §7).

| Owned | `attract_diff.py orig_a.tsv` | `mem_dump.py --compare clutref_a` | Frame hash |
|---|---|---|---|
| 58 (plus section 1) | identical, 7,478 frames | arena, vram, clut identical | identical, 4,738 frames (`ab9`) |
| 70 (plus twelve of section 2) | identical | identical | **29 of 7,428 frames differed - and so did original against original**: see below |
| 73 (all of section 2) | identical, 7,478 frames | identical | identical, 7,428 frames, with original against original identical beside it (`ab11`) |

**The frame hash needed repair before it could be read.** Taking over this
layer removed several million trapped calls a run, the traced game sped up
from about 17 to about 27 logic frames a second, and at that speed the draw
takes a branch no earlier run had entered - wall-clock work the exclusion
list had never seen. Settled by an original-against-original comparison and
fixed in `calltrace.py wallclock --static`;
[`call-trace.md`](call-trace.md) §6 has the whole account.
