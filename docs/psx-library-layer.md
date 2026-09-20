# The port's PSX library layer, `0x5A7540`..`0x5A94BE`

**Status:** IN PROGRESS (2026-09-20)

The porting house did not rewrite the game's calls into Sony's libraries; it
wrote the libraries. Between the game code and Direct3D sits a layer with
libgpu's and libgte's entry points - `SetPolyFT4`, `getTPage`, `ClearOTagR`,
`ApplyMatrix`, `PushMatrix` - working on globals where the PlayStation had a
coprocessor. It is where the attract run spends its calls: the takeover
queue's 38 hottest layer-0 logic functions are in it, all but one
(`python tools/calltrace.py queue ...`, 2026-09-20). Sixty-two of its
functions are ours.

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
  there. It is the vertex's depth, `sz / 16384` - section 3 - and `0.01` is
  what a primitive nobody transformed gets.
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

Fifteen integer functions, ours, `src/game/psx_gte.cpp` (the x87 ones are section 3):

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
| `Gte_SetMatrix2` | `0x5A8DA0` | 2,843 | 8 dwords to a second matrix at `0x7DE4E0` - the light matrix: `Gte_NormalColor` puts the normal through it first (section 4) |
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

## 3. The functions that go through x87 - nineteen, ours

`src/game/psx_gte_float.cpp`. What an x87 instruction computes depends on the
precision-control field of the control word - each `fmul`, `fdiv` and `fiadd`
rounds to 24, 53 or 64 bits before the next begins - so "faithful" here needed
a measurement before it needed code.

**The measurement.** `BOF3X_SHADOW=psx_gte_float` reads the control word
(`fnstcw`) on every live call of the two functions below that depend on it. A
hands-off attract run of 8,872 logic frames (`analysis/attract/ab12_shadow.log`,
2026-09-20): **`0x027F` on all 11,272,192 calls** - 53-bit precision, round to
nearest, every exception masked - the value the C runtime starts a process
with. Whatever Direct3D does with the FPU inside its own calls, the game's
code does not run at 24 bits.

**The decision about `long double`: not needed.** At 53 bits every x87
operation on operands this size *is* the IEEE double operation - same inputs,
same correctly rounded result; the extended exponent range never comes into
it - so the functions are written in `double`, which this toolchain compiles
to SSE2, where the rounding does not depend on the x87 control word at all.
The square root is `sqrtsd` by intrinsic rather than a libm call. Had the
word been `0x007F` this would have needed exact 24-bit rounding of 56-bit
products, which `double` cannot give.

| Function | Address | Calls (`all_b`) | What |
|---|---|---|---|
| `Gte_Perspective` | `0x5A8380` | 10,965,378 | the perspective division: `Gte_Transformed` to a screen position **in floats** and a depth. Three branches: in front of the near plane, `x * h / z + offset` with the product an integer one that wraps; at or behind the eye, the same over the near plane's depth, depth out 0; between, all in floating point by way of `z / near`, y's product passing through a 32-bit float on the stack |
| `Gte_DepthRamp` | `0x5A8340` | 6,345,796 | where a depth lies between `Gte_RampNear` and `Gte_RampFar`, 0 to `0x1000`, truncated by `_ftol` - the GTE's depth-cue factor by shape |
| `Gte_PrimDepths4_10` | `0x5A9290` | 1,971,642 | four depths, each `/ 16384` as a float, into a primitive at `+0x10`, `+0x20`, `+0x30`, `+0x40` |
| `Gte_StoreDepthF` | `0x5A9110` | 754,272 | the newest depth `/ 16384` through a pointer |
| `Gte_StoreDepthF3`, `F4` | `0x5A9130`, `0x5A9170` | 283,535 / 28,128 | the newest three, or all four, oldest first |
| nine more `Gte_PrimDepths*` | `0x5A91C0`..`0x5A9460` | 0 | the same for other primitive layouts - strides `0x0C`, `0x10`, `0x14`, three or four vertices, or one depth at all four. Several are byte for byte the same function at a second and third address |
| `Gte_InitGeom`, `Gte_SetGeomOffset`, `Gte_SetGeomScreen` | `0x5A7AA0`, `0x5A7AE0`, `0x5A7B00` | 1 / 10 / 10 | integer: what the above read, set. Near plane at half the projection distance; the ramp's ends `scale * h / 1000` |

This answers section 1's open question: **the float at `+0x10` of every
primitive vertex is its depth, `sz / 16384`** - the setters' `0.01` is a
placeholder the transform overwrites. The GTE's depth FIFO is `0x7DE7A0`,
`0x7DE79C`, `0x7DE798`, `Gte_Depth` `0x7DE7A4`, oldest first.

**Checks.** Start-up fuzz against clones: 48,000 rounds, a third under each
of `0x027F`, `0x007F`, `0x037F`, the clone run under that word. Under the
game's word, **0 mismatches**; and the depth stores match under all three, as
the arithmetic says they must (an `s32` times a power of two is exact at any
precision, and the store rounds once). Under the other two words the two
precision-dependent functions differ from ours in 253 of 3,369 rounds - which
is *expected*, is logged, and is the standing proof that this fuzz can see a
rounding difference; if that count is ever 0 the self-test refuses to go on.
`CloneOriginal` learned to re-aim a tail `jmp` for this (`Gte_DepthRamp` ends
in one, to `_ftol`).

Live, the same switch compares every call of the two bit for bit against the
clone: 4,161,663 + 7,110,529 calls, **0 mismatches** - all 7.1 million of
`Gte_Perspective`'s in the first branch. **The attract run never puts a
vertex behind the near plane**, so the other two branches rest on the fuzz
alone: 612 and 567 rounds, a third of each under the game's word.

Negative controls, each refused: the ramp rounded rather than truncated
(154), `0 / 0` giving `0x1000` (61), depth `z` rather than 0 behind the eye
(221), x's product without the 32-bit wrap (217), y's product not through a
float (38), the sum done in `float` (46), four depths in natural order
(2,869), near plane `h >> 1` (303), ramp ends without the wrap (1,475), init
leaving the matrix stack alone (2,526).

Four controls were **not** refused at first, and each said something:

- *The ramp's quotient through a 24-bit float* passed - the fuzz could not
  see precision in `Gte_DepthRamp` at all, because the truncated result only
  moves when the quotient is within 2^-25 of a 4096th. Seeded with wide ramps
  and values one either side of a 4096th: refused, 158.
- *`sy` divided by a z read once* passed, because no out pointer was ever
  aimed at a global the function reads. One round in eight now aims one
  there: refused - by 2 rounds, so this edge is thinly covered.
- *One depth at four vertices, all stored from the register* passed, and is
  unobservable: the original reads the first store back as an integer and
  copies that, the same dword either way. Ours is the simple form and the
  comment says so.
- *`(float)depth * 2^-14f`* passed: the same value by the argument above. Not
  a claimed quirk.

A trap on the way: a division by zero inside the start-up fuzz does not
crash, it **hangs** the game at start-up with nothing in the log after the
`cloned` lines. Why it hangs rather than reaching the crash reporter was not
looked into; the fuzz runs inside the DLL's load, which is the likely reason.

## 4. The transforms over them - fourteen, ours

`src/game/psx_gte_transform.cpp`.

With the division ours, the functions that call it are integer work over it,
plus two vector normalisations and a lighting function that are x87 again.

| Function | Address | Calls (`all_b`) | What |
|---|---|---|---|
| `Gte_Rtps` | `0x5A8E90` | 4,019,785 | the GTE command by shape: V0 through the matrix, translation added, both FIFOs moved up, the division, and the depth-cue factor to `Gte_Ir0` `0x7DE464` |
| `Gte_Rtpt` | `0x5A8F60` | 2,230,333 | V0, V1, V2: the newest depth copied to the oldest slot, the results stored straight to the three screen points and the three newest depths - in effect three RTPS (see the controls) |
| `Gte_VectorNormal`, `Gte_VectorNormalS` | `0x5A8B60`, `0x5A8C00` | 377,136 / 188,568 | a `VECTOR` to length 4096, out as `s32` or `s16`; returns the squared length. x and y pass through 32-bit floats first and z does not; the sum is z, y, x in that order; a zero length divides by 1; every result through `_ftol` |
| `Math_Cos` | `0x5A7A50` | 145,824 | `Math_Sin(angle + 0x400)` |
| `Gte_RotTransPers4` | `0x5A85F0` | 52,972 | libgte's by shape. The screen points go to the caller **as floats and not through the FIFO**; the depths do go to the depth registers |
| `Gte_RotTransPers` | `0x5A8250` | 42,706 | `Gte_Rtps` with the vertex an argument; the point and the factor handed back; returns depth `>> 2`. The flag argument libgte has is never read |
| `Gte_RotTransPers3`, `Gte_RotAverage3`, `Gte_RotAverage4`, `Gte_ScaleMatrix` | `0x5A84A0`, `0x5A87A0`, `0x5A8950`, `0x5A8120` | 0 | not reached by the attract run. The averages' outs are 12 bytes - x, y, depth `/ 16384` - and `Gte_Otz` `0x7DE794` is a sum over 12 toward zero for three, a sum `>> 4` for four |
| `Gte_NormalColor` | `0x5A8CA0` | 188,568 | libgte's by shape: a normal through the light matrix `Gte_Matrix2`, normalised, through `Gte_ColorMatrix` `0x7DE430`, normalised, `Gte_BackColor` added, times the colour in `/ 4096` - **and then the colour in is copied over the result** (`0x5A8D84`..`0x5A8D91`). As shipped, this lighting does nothing. The arithmetic is kept because an out a byte or two below the in can see it |
| `Gte_SetColorMatrix`, `Gte_SetBackColor` | `0x5A8DC0`, `0x5A7B60` | 9 / 9 | 8 dwords to `Gte_ColorMatrix`; three components `<< 4` as words to `Gte_BackColor` `0x7DE458` |

This settles the note on `Gte_Vertices`: **the vertex registers in memory
order are V1, V0, V2** - `Gte_Rtps` takes the middle slot, `Gte_Rtpt` the
middle, the first, the last.

**Checks.** Start-up fuzz, `BOF3X_SHADOW=psx_gte_transform`: 36,000 rounds
over the fourteen, the clones run under `0x027F` with every call of theirs
re-aimed at a clone of what it calls - `Gte_ApplyMatrix`, `Gte_ApplyMatrixLV`,
`Gte_Perspective`, `Gte_DepthRamp`, `Math_Sin`, `Gte_VectorNormal` - and
`_ftol` left the C runtime's; 4,586 rounds with overlapping arguments, 2,251
with an all-zero scratch, 663 with the two colours a few bytes apart; every
register block, the arguments and the results compared; **0 mismatches**. A
function can only be cloned before its entry is patched, so this module's
`Inject` runs *before* the ones that own its callees
(`src/hook/inject_all.cpp`). All 28 start-up self-tests re-run together
(`BOF3X_SHADOW=*`), 0 mismatches in each.

Negative controls, each refused: RTPS without the depth FIFO moving (5,137),
the vertex registers in memory order (5,115), `RotTransPers` storing the point
before the factor (38), depth `/ 4` for `>> 2` (127), `RotAverage3` as
`>> 2` then `/ 3` (62), `RotAverage4` as `/ 16` (217), the normal's z through
a float too (7), its x not through one (16), the returned square not through
one (4,540), `_ftol` out of range giving -1 (75), `ScaleMatrix` reading all
nine first (2,118), the cosine a quarter turn back (2,571), `NormalColor` with
only the copy (267), with the lit colour kept (2,235), without the background
colour (105), the background colour not `<< 4` (2,497).

Three were **not** refused, and each corrected a comment that had claimed a
quirk:

- *`Gte_Rtpt` as three `Gte_Rtps`.* They leave every register the same: three
  pushes through a four-deep depth FIFO end where the original's
  copy-then-store ends. The difference is one of form; ours keeps the
  original's.
- *The normal's sum in the other order*, and *a zero length left to divide.*
  Neither can be seen: a sum large enough to round differently is a squared
  length the `_ftol` of a float hides, and a zero length means a zero vector,
  whose components are 0 either way - `_ftol` of a NaN is 0 too. Both kept as
  the original has them, and the comment now says they are unobservable.

**Not taken over, and why: `0x5A7D70`**, the matrix product (153,648 calls;
`0x5A7F10`, `0x5A7F80`, `0x5A7FF0`, `0x5A8060` and `0x57C070` sit on it). It
builds the nine `s16` of the result on the stack and then copies **five
dwords** to the out - 20 bytes for an 18-byte result. Bytes 18 and 19 of the
out, the `MATRIX`'s padding, receive whatever the stack held at `esp + 0x3E`,
which the function never writes. That cannot be reproduced, only replaced -
by zeros, or by leaving the out's padding alone - and either is a divergence
for the ledger, small as it is. The owner's call; the arena dump may well be
able to see those two bytes.


## 5. Live checks

All hands-off from a fresh launch, everything ours, against the all-original
references of 2026-09-19; the frame hash with both sides re-recorded
([`call-trace.md`](call-trace.md) §7).

| Owned | `attract_diff.py orig_a.tsv` | `mem_dump.py --compare clutref_a` | Frame hash |
|---|---|---|---|
| 58 (plus section 1) | identical, 7,478 frames | arena, vram, clut identical | identical, 4,738 frames (`ab9`) |
| 70 (plus twelve of section 2) | identical | identical | **29 of 7,428 frames differed - and so did original against original**: see below |
| 73 (all of section 2) | identical, 7,478 frames | identical | identical, 7,428 frames, with original against original identical beside it (`ab11`) |
| 92 (plus section 3) | identical, 7,478 frames | identical | identical, 7,936 frames, original against original identical beside it (`ab12`); and section 3's live shadow |
| 108 (plus section 4, and [`sprite-draw-order.md`](sprite-draw-order.md) §8) | identical, 7,478 frames | identical | identical, 7,937 frames, original against original identical beside it (`ab13`) |

**The frame hash needed repair before it could be read.** Taking over this
layer removed several million trapped calls a run, the traced game sped up
from about 17 to about 27 logic frames a second, and at that speed the draw
takes a branch no earlier run had entered - wall-clock work the exclusion
list had never seen. Settled by an original-against-original comparison and
fixed in `calltrace.py wallclock --static`;
[`call-trace.md`](call-trace.md) §6 has the whole account.
