# The PSX library layer's last seventeen (group PL)

**Status:** IN PROGRESS (2026-10-05 - group PL of the platform round's step 2
([`platform-layers-plan.md`](platform-layers-plan.md) section 4): seventeen
functions ours in [`src/game/psx_rest.cpp`](../src/game/psx_rest.cpp),
faithful, fuzzed against Capcom's at start-up (`BOF3X_SHADOW=psx_rest`,
0 mismatches, 42 of 42 controls refused), headless only; no divergence, no new defect; not live-checked)

[`platform-read-pass.md`](platform-read-pass.md) section 2 left seventeen
starts of the PSX library layer alive under ours: one entered by a route,
sixteen held - called by address from bodies of ours at exit, on an error, or
from effect and magic code no route reached. Every one is a leaf or a thin
wrapper over a Windows or COM call. All addresses are `BOF3.exe`; disassembly
by capstone, 2026-10-05, each function to its last instruction; callers by
`analysis/platform_reach_1005.tsv` and a raw dword scan of the image.
`symbols.toml`'s group PL block has each entry's evidence.

## 1. What they are

| Function | Address | Bytes | What |
|---|---|---|---|
| `Gpu_SetPolyF3` | `0x5A7570` | `0x17` | code `0x20` to `+7`; 0.01 to `+0x10`, `+0x1C`, `+0x28` |
| `Gpu_SetPolyFT3` | `0x5A7590` | `0x17` | code `0x24`; 0.01 to `+0x10`, `+0x20`, `+0x30` |
| `Gpu_SetLineG4` | `0x5A76F0` | `0x1A` | code `0x5C`; 0.01 to `+0x10` .. `+0x40` by `0x10` |
| `Gpu_SetTexWindow` | `0x5A7840` | `0x13` | dword `+4` = `0xF0000000`, dword `+8` = the rect's **address** |
| `Gte_SquareRoot0` | `0x5A7A90` | `0xB` | `fild`, `fsqrt`, tail `jmp` to `_ftol` `0x5B9550` |
| `Gte_ApplyMatrixSV` | `0x5A7C70` | `0x7E` | rotation times an `SVECTOR`, `>> 12`, out as `s16` |
| `Display_Teardown` | `0x5A6380` | `0x122` | the seven below, then the DirectX 6 objects released |
| `Gfx_FreeClutRows` | `0x5A64B0` | `0x2C` | `Crt_free` every `Gfx_ClutRows` row buffer |
| `Gfx_ReleaseTexCache` | `0x5A64E0` | `0x55` | release the page texture cache's live entries |
| `Font_ReleaseTexCache` | `0x5A6540` | `0x46` | release all 128 glyph texture entries |
| `Display_ReleaseOrphans` | `0x5A6590` | `0x5F` | release six pointers nothing makes (section 2) |
| `Display_ReleaseBackdrop` | `0x5A65F0` | `0x55` | release the backdrop block, zero it, `0x12345678` |
| `D3d_ReleaseAfterDraw` | `0x5A6650` | `0x3E` | release `D3d_AfterDraw`'s three surfaces, zero its block |
| `D3d_FreeCellTextures` | `0x5A6760` | `0x17` | `D3d_FreeCellTexture(0..127)` |
| `Display_TextOut` | `0x5A66B0` | `0x6D` | GDI `TextOutA` on the back buffer's DC |
| `Display_ErrorBox` | `0x5A6720` | `0x39` | `MessageBoxA` of the error table's entry |
| `Sound_Shutdown` | `0x5A6980` | `0x3D` | stop and release the music, release DirectSound |

Sizes are each body to its last instruction and agree with
`platform_reach_1005.tsv` and with the lines `entries_logic.txt` already has
(section 8). Every jump in all seventeen stays inside. The relative calls out:
`Display_Teardown`'s seven (`+1`, `+6`, `+0xB`, `+0x10`, `+0x15`, `+0x1A`,
`+0x1F`), `Gfx_FreeClutRows`'s `Crt_free` (`+0xD`), `D3d_FreeCellTextures`'s
`D3d_FreeCellTexture` (`+4`), `Sound_Shutdown`'s `SndStream_Stop` (`+0`: the
entry is the call) and `Music_Release` (`+5`), and `Gte_SquareRoot0`'s tail
jump (`+6`). None of the seventeen is a start that turned out not to be a
function.

**Names.** The setters by their GPU code, the libgpu names the sibling's
Psy-Q signature matches carry (`SetPolyF3` `0x8017B2CC`, `SetPolyFT3`
`0x8017B2E0`, `SetLineG4` `0x8017B480`); `Gpu_SetTexWindow` by what
`Gfx_DrawOTag` does with code `0xF0` ([`d3d-draw.md`](d3d-draw.md) section 3:
`Gfx_TexCacheKey` from the pointer at `+8`) - libgpu `SetTexWindow(DR_TWIN *,
RECT *)` in the port's form, not the PSX's (the sibling's `0x8017CB20` packs an
`0xE2` word). `Gte_SquareRoot0` and `Gte_ApplyMatrixSV` are libgte's
`SquareRoot0` (the sibling's `0x80179060`) and `ApplyMatrixSV` (`0x80179790`),
both by whole-object Psy-Q signature there and by shape here:
`Gte_ApplyMatrixSV` is `Gte_ApplyMatrix` `0x5A7BF0` with an `SVECTOR` out, so
the `Gte_` prefix of [`psx-library-layer.md`](psx-library-layer.md) section 2.
The teardown's seven are named by what they tear down; `Display_ReleaseOrphans`
is a hypothesis (section 2).

### The data

`Snd_PrimaryBuffer` `0x7DE3C0` is named (`[[data]]`): the sound set-up
`0x5A6830` creates it with a `DSBUFFERDESC` whose flags are 1
(`DSBCAPS_PRIMARYBUFFER`) and sets its format (2 channels, 22,050 Hz, 8 bits).
The other blocks stay addresses in `psx_rest_callees.h`: `0x7CAE20..0x7CAE37`
(section 2), the backdrop block `0x6BE9F8..0x6BEA13` (`Dd_CellBackdrop`
`0x6BEA00` is its third dword), `D3d_AfterDraw`'s block `0x7CADE8..0x7CAE03`
([`display-setup.md`](display-setup.md) section 7), the error table
`0x66B6A8` and its caption `0x66BC1C`.

## 2. How each works, quirks kept

- **The three setters** write the code byte and then the depth floats, the
  size byte `+3` untouched, like every setter of the port's
  ([`psx-library-layer.md`](psx-library-layer.md) section 1). `eax` is the
  primitive; every caller of the three is ours and types them `void`.
- **`Gpu_SetTexWindow`** stores the rect's address, so the rect has to live
  until the frame is drawn. Its callers build it at `Gfx_PacketNext`, inside
  the frame's packet buffer.
- **`Gte_SquareRoot0`** rounds at the control word's precision before
  `_ftol` truncates. Under the game's `0x027F` (measured,
  [`psx-library-layer.md`](psx-library-layer.md) section 3) `fsqrt` is the
  correctly rounded double root, so ours is `sqrtsd` truncated. For any
  `n < 2^31` the root of `k^2 - 1` is about `1 / 2k` below `k`, far more than a
  double's last place, so the result is the integer square root. A negative
  `n` gives the NaN, and `_ftol` of it is `0x8000000000000000`: `eax` 0. Under
  a 24-bit word the same `k^2 - 1` would round up to `k` - which is what the
  fuzz uses to prove it can see the precision (section 4). `edx` (the high
  half) is not returned; every caller is ours and reads `eax`.
- **`Gte_ApplyMatrixSV`** reads all twelve words before its first store, so
  an `out` overlapping the inputs sees the original's result; the stores are
  z, x, y. Products and sums wrap in 32 bits; the shift is arithmetic (a
  logical one cannot be told apart: the `s16` store drops bits 16 and up).
- **`Gfx_FreeClutRows`** reads each of the 512 row pointers once, frees it
  when it is not null and zeroes the pair (the generation counter with it).
- **`Gfx_ReleaseTexCache`** walks each page only while the state byte is
  non-zero: an entry past a dead one is not released even if it holds
  objects. The surface `+0x10` is read after the texture `+0x14` is released.
- **`Font_ReleaseTexCache`** does all 128 with no state test, texture `+0xC`
  before surface `+8` (read after), and leaves the glyph word `0xFFFF`, the
  set-up's empty value, after zeroing the five dwords.
- **`Display_ReleaseOrphans`** releases and zeroes `0x7CAE30`, `0x7CAE28`,
  `0x7CAE34`, `0x7CAE2C`, `0x7CAE24`, `0x7CAE20`. A raw dword scan of the
  whole image finds these six addresses only in this function (`0x7CAE2C`
  and `0x7CAE34` not even there - they are reached as `esi + 8` and `esi`
  in its loop): no instruction of Capcom's writes them, the set-up does not
  zero them, and nothing reads them. Six COM pointers that some earlier
  version of the port made and this one does not; at exit they are zero and
  the function does nothing. The name says that much and no more.
- **`Display_ReleaseBackdrop`** releases `0x6BEA0C`, `0x6BEA04`, `0x6BEA10`,
  `0x6BEA08` and `Dd_CellBackdrop`, zeroing none of them itself; then zeroes
  the seven dwords from `0x6BE9F8` (which covers all five) and writes
  `0x12345678` to `0x6BE9FC`. That address is named by this one instruction
  in the whole image: a marker nothing reads.
- **`D3d_ReleaseAfterDraw`** releases the texture `0x7CAE00`, the texture
  surface `0x7CADFC` and the capture surface `0x7CADF8`, then zeroes the
  block's seven dwords.
- **`D3d_FreeCellTextures`** pushes the whole counter for each slot.
- **`Display_Teardown`** calls the seven in the order above - the cell
  textures **before** the after-draw block, not in address order - then
  releases and zeroes, each when not null and each read once: `D3d_BackMaterial`,
  `D3d_Viewport`, `D3d_Device`, `0x7CC34C` (the `IDirect3D3`), the clipper
  `0x7CC348`, the Z-buffer `0x7CC340`, `Dd_StageSurface`, `DDraw_BackBuffer`,
  `DDraw_Primary`. Last `Dd_DirectDraw`: nothing if null; `Gfx_RenderFlags` is
  read now, after every release before it, and tested for `0x100`
  (`test ch, 1`: fullscreen). Fullscreen: `SetCooperativeLevel(hwnd,
  DDSCL_NORMAL)` on the object as first read, then `RestoreDisplayMode` and
  `Release` each on the slot **read again** without a new test. Windowed:
  `Release`. Then the slot zeroed. (`symbols.toml` said `RestoreDisplayMode`
  came first; the code calls `SetCooperativeLevel` first - corrected.) It
  leaves `Gfx_RenderFlags` alone.
- **`Display_TextOut`** makes its `hdc` with `push ecx` - the slot starts as
  its caller's `ecx` - and `GetDC` writes it. A failed `GetDC` returns at once
  with nothing released. Then `SetTextColor(white)` and `SetBkMode(TRANSPARENT)`,
  the text's length counted **after** those two, `TextOutA`, and `ReleaseDC`
  on `DDraw_BackBuffer` read again. Ours starts the slot at null: the two
  differ only if a `GetDC` succeeded without writing its out, which DirectDraw
  does not do; the fuzz's `GetDC` always writes.
- **`Display_ErrorBox`** compares the code with 100 signed: below, the table
  `0x66B6A8` (entry 0 null, 1..19 strings); otherwise `0x66B568` indexed by
  the code itself, which puts 100..103 at the four dwords `0x66B6F8..0x66B704`
  right after the first table. The four are the sound set-up `0x5A6830`'s
  error returns (`0x64..0x67`) - but `Game_Init` drops that return
  (`0x4FD144`), so no code of 100 or more ever reaches the box. Nothing
  bounds the code.
- **`Sound_Shutdown`**: `SndStream_Stop`, `Music_Release`, then
  `Snd_PrimaryBuffer` and `Snd_Device`, each released and zeroed when not
  null.

What none of the void ones leaves in `eax` is read: every caller of the
seventeen is ours.

## 3. What the calls land on

**Under ours, live.** The DirectX 6 objects in the teardown's slots are the
backend's since DIV-0031 ([`render-backend.md`](render-backend.md) section 1):
`Release` (`+8`) on the material, viewport and device lands on singletons
that answer 1 and free nothing; on the primary, back buffer, stage and the
cache textures on `Surface_Release` / `Texture_Release`, one reference
counter per surface - the texture interface's `QueryInterface` took its own
reference, so a cache entry's two releases bring it to zero exactly. The
`IDirect3D3` slot holds the DirectDraw object (`display_setup.cpp`), whose
`Release` answers 1; the clipper and Z-buffer slots are null under ours and
skipped. **The fullscreen branch cannot run under ours**: our
`Display_Setup` sets `Gfx_RenderFlags` to `0x202` and nothing of ours sets
`0x100`; if it ever did, `RestoreDisplayMode` and `SetCooperativeLevel` are
not in the backend's table and the process would end naming slots 19 and 20
(rule 4). The two sound objects are DirectSound's own (Capcom's set-up
`0x5A6830` still makes them). `Crt_free` is the C runtime's; the row
buffers are its `Crt_malloc`'s.

`Display_TextOut` would end the process under the backend (`GetDC`, slot 17,
is not in its table): our WinMain calls it only when the back buffer is not
one of ours, that is with `BOF3X_ORIGINAL=Display_Setup` (DIV-0032), and
logs the overlay otherwise. `Display_ErrorBox` is unreachable under ours:
our `Display_Setup` returns 0 or ends the process.

**In the fuzz.** Every COM slot is a recorder (section 4); every relative
call out is re-aimed; the four Win32 calls go through the game's import
slots (`SetTextColor` `0x5C4020`, `SetBkMode` `0x5C4024`, `TextOutA`
`0x5C4028`, `MessageBoxA` `0x5C4174`, by name from the import directory): the
copies' absolute operands are moved onto slots of ours that hold recorders,
and ours calls the same recorders through `psx_rest::g`, whose live entries
read the game's import slot at each call as the original does.

## 4. The fuzz

`src/game/psx_rest_fuzz.cpp`, `BOF3X_SHADOW=psx_rest`.

**The leaves**, on scratch buffers, each run on the same buffer so that a
stored address compares equal:

- the three setters and `Gpu_SetTexWindow`: 3,000 rounds each, the primitive
  at any of sixteen offsets in a random 0x60-byte buffer, the whole buffer
  compared; the rect a random dword or an address inside the buffer;
- `Gte_SquareRoot0`: 98,304 rounds, a third each under `0x027F`, `0x037F`
  and `0x007F` (the copy under that word, ours unaffected - it is SSE2):
  exact squares and their two neighbours half the time, the edges (0..4, -1,
  -2, `INT_MIN`, `INT_MAX`, `46340^2` and one below it, `2^30` and one below,
  `0xFFFF`, `0x10000`), small and full-range noise. Under `0x027F` and `0x037F`
  the two must agree; under `0x007F` they must **not** always agree - 5,785
  of 32,768 rounds differ there, and if that count is ever 0 the self-test
  refuses to go on: it is the standing proof that this fuzz sees precision.
  The copy keeps its tail jump to the runtime's `_ftol` (a `CloneCall` with
  `expected` `0x5B9550`), which needs no runtime state;
- `Gte_ApplyMatrixSV`: 12,000 rounds, matrix, vector and out anywhere in one
  0x60-byte buffer, out overlapping an input in 5,912; a third of the words
  from the edges (`0x7FFF`, `-0x8000`, `0x1000`, ...), so sums wrap.

**The teardown chain**, 2,000 rounds of each of twelve kinds - the eleven
alone, then the teardown as a tree (a copy calling copies of the seven,
against ours calling ours) - against byte-copies:

- **Fake COM objects**: eight on one vtable - `Release` `+0x08`, `GetDC`
  `+0x44`, `RestoreDisplayMode` `+0x4C`, `SetCooperativeLevel` `+0x50`,
  `ReleaseDC` `+0x68`; every other slot ends the process naming itself. Each
  records the object and its arguments into a hash chain and answers from a
  hash; `GetDC` writes a handle and fails one call in four
  (`DDERR_SURFACELOST`) or with a random value.
- **Stand-ins** for `Crt_free`, `D3d_FreeCellTexture`, `SndStream_Stop`,
  `Music_Release`, the teardown's seven (alone) and the four imports, each
  recording.
- **Disturbances**: one call in four, a recorder changes something its
  caller reads next, from the round's hot list: a COM slot swapped for
  another fake or nulled - `Dd_DirectDraw` and, for the text, the back buffer
  only ever swapped, since the original reads them again without a test - a
  texture-cache state byte (the walk's end), `Gfx_RenderFlags` bit `0x100`, a
  byte of the text (its terminator moves), a row-buffer pointer.
- **Seeded**: each texture-cache page live to a prefix - none, all 32, or
  mostly short - so the walk's stop is hit on every page; every COM slot null
  a third or a quarter of the time; fullscreen half the time (1,618
  fullscreen teardowns with a DirectDraw object); error codes 0..19, 100..103,
  96..103 across the boundary and -1..-8 (the table's text pointer is
  recorded by value, never followed); the text 0..0x46 characters.
- **Compared** every round: `Gfx_ClutRows` and the 16 bytes below it, to
  `Gfx_RenderFlags`; `Gfx_TexCache` and `Font_TexCache` with 16 bytes either
  side; `0x7CADE0..0x7CAE40`; the backdrop block with 8 bytes either side;
  `0x7CC330..0x7CC360`; `0x7DE3B8..0x7DE3C8`; the text; the hash and length
  of the whole call log.

Result (2026-10-05, headless): **the six leaves 0 mismatches; the chain
24,000 rounds, 0 mismatches**; the originals made 3,447,559 calls, 1,722,932
disturbances; COM calls on both sides `Release` 3,049,992, `GetDC` 4,000,
`RestoreDisplayMode` 3,486, `SetCooperativeLevel` 3,486, `ReleaseDC` 2,548;
631 rounds with a code of 100 or more. With every module's self-test
(`BOF3X_SHADOW='*'`, 2026-10-05): exit 0, `self-test only: done`, `inject:
10026 ours`, narrow and again with `BOF3X_WIDE=1`.

## 5. Negative controls

Forty-two bugs planted one at a time in `psx_rest.cpp` - at least two per
function - each built and self-tested headless (a scratch driver,
`controls.py` in the group's scratch directory, not committed), then removed
and the build restored. **All 42 refused**, every one by a count of differing
rounds and exit 3, none by a hang or a fault. Counts are rounds (the leaves'
per function; the chain's of 24,000, alone and tree together).

| Function | Planted | Refused in |
|---|---|---|
| `Gpu_SetPolyF3` | code `0x28` | 3,000 of 3,000 |
| | vertex stride `0x10` | 3,000 |
| `Gpu_SetPolyFT3` | code `0x2C` | 3,000 |
| | third depth dropped | 3,000 |
| `Gpu_SetLineG4` | code `0x58` | 3,000 |
| | fourth depth dropped | 3,000 |
| `Gpu_SetTexWindow` | only byte `+7` stored (bytes `+4..+6` left) | 3,000 |
| | the address at `+0xC` | 3,000 |
| `Gte_SquareRoot0` | rounded, not truncated | 22,536 |
| | a negative gives -1 | 7,668 |
| | the root through a `float` | 11,252 - and the `0x007F` proof fell to 0, refused twice |
| `Gte_ApplyMatrixSV` | z stored from the y row | 11,996 of 12,000 |
| | y computed after the first two stores | 4,104 (only overlap can see it) |
| `Gfx_FreeClutRows` | the counter not zeroed | 4,000 (2,000 alone, 2,000 tree) |
| | a null row freed too | 4,000 |
| `Gfx_ReleaseTexCache` | the walk goes on past a state 0 | 4,000 |
| | surface released before texture | 4,000 |
| `Font_ReleaseTexCache` | `0xFFFF` at `+2` | 4,000 |
| | surface before texture | 4,000 |
| `Display_ReleaseOrphans` | the `+8` cells not zeroed | 3,351 |
| | `0x7CAE20` before `0x7CAE24` | 1,574 |
| `Display_ReleaseBackdrop` | no `0x12345678` | 4,000 |
| | six dwords zeroed, not seven | 2,526 |
| | `Dd_CellBackdrop` released first | 2,606 |
| `D3d_FreeCellTextures` | slots 0..126 | 4,000 |
| | in reverse | 4,000 |
| `D3d_ReleaseAfterDraw` | six dwords zeroed | 2,634 |
| | texture surface before texture | 1,518 |
| `Display_Teardown` | after-draw block before the cell textures | 3,937 |
| | fullscreen as bit `0x200` | 1,714 |
| | `Dd_DirectDraw` not read again after `SetCooperativeLevel` | 17 (15 alone, 2 tree) |
| | `SetCooperativeLevel` flags 1 | 1,743 |
| | the viewport not zeroed | 2,789 |
| `Display_TextOut` | the length counted before the two GDI calls | 89 |
| | `ReleaseDC` on the back buffer as first read | 108 |
| | `SetBkMode(OPAQUE)` | 1,274 |
| | goes on after a failed `GetDC` | 726 |
| `Display_ErrorBox` | the compare unsigned | 260 |
| | `MB_OK` | 2,000 |
| | the boundary at 101 | 146 |
| `Sound_Shutdown` | `Music_Release` before `SndStream_Stop` | 2,000 |
| | `Snd_Device` not zeroed | 1,280 |

Thin but refused: the DirectDraw re-read (17 rounds) and the text's length
after the GDI calls (89) - each needs a disturbance at exactly one call. Not
planted because it cannot be seen: `Display_ErrorBox`'s high table read as
`0x66B6F8 + (code - 100) * 4`, the same address as the original's
`0x66B568 + code * 4`; and `Gte_ApplyMatrixSV`'s shift made logical (the
`s16` store drops the bits it changes).

## 6. What none of this reached

- **No route enters sixteen of the seventeen.** The reach trace of
  2026-10-05 (`analysis/calltrace/platform_1005/`, the attract sequence and
  ten routes) entered only `Gpu_SetTexWindow`: `whelpBoss`, frame 1862, from
  `EffectKind10_Run` - so the state hash's `whelpBoss` route is its live
  check. The setters, `Gte_SquareRoot0` and `Gte_ApplyMatrixSV` are called
  by effect and magic bodies of ours that no recorded route reached. The
  teardown chain, `Display_TextOut`, `Display_ErrorBox` and `Sound_Shutdown`
  are reached by exits and errors: WinMain's loop end - **every time a player
  closes the game normally** (the window's close, or F9 twice while the title
  is up - `Game_WndProc`'s evidence in `symbols.toml`),
  which the routes never do (the runners end the process) - and the set-up's
  error path, which ours cannot take. The exit path under the backend has not
  been seen to run cleanly with these functions ours or Capcom's; one normal
  exit with `BOF3X_EXITTRACE=1` would say so.
- **No real `Release`.** The fuzz sees which slots are released in which
  order; that the backend's reference counts reach zero at exit is read from
  `render_shim.cpp` (section 3), not observed.
- **The fullscreen branch** runs only in the fuzz; under ours it cannot
  (section 3).
- **`Display_TextOut` with GDI** and **`Display_ErrorBox` with a real box**
  - only under `BOF3X_ORIGINAL=Display_Setup` and only for the first.
- **SquareRoot0's `edx`**: not compared; no caller reads it.

## 7. Latent defects, described

None of the original's that this group would fix. Two oddities, neither
reachable as a fault: `Display_ReleaseOrphans` releases six pointers nothing
makes (harmless while they are zero), and `Display_ErrorBox` reads past its
tables for a code outside 0..19 and 100..103 (no caller passes one).

## 8. For `analysis/calltrace/entries_logic.txt`

Nothing to add: all seventeen are listed with these sizes already (the
platform read pass armed them), checked 2026-10-05 against the main
checkout's file:

```
005A6380 122
005A64B0 2C
005A64E0 55
005A6540 46
005A6590 5F
005A65F0 55
005A6650 3E
005A66B0 6D
005A6720 39
005A6760 17
005A6980 3D
005A7570 17
005A7590 17
005A76F0 1A
005A7840 13
005A7A90 B
005A7C70 7E
```

## 9. The rebinding

Every raw constant in `src/game` naming one of the six called by address now
reads `bof3::addr::<Name>`, the value unchanged: `kSetPolyF3` / `kPolyF3` in
`area_w2e_callees.h`, `effect_2b`, `effect_2c`, `effect_3c`, `effect_3d`,
`effect_4c`, `rest_4c`, `rest_4e` callees and `magic_s04.cpp`,
`magic_s21.cpp`, `magic_s25.cpp`; `kSetPolyFT3` in `magic_s22.cpp`;
`kSetLineG4` / `kSetPoly5C` in `magic_s21.cpp`, `magic_s25.cpp`;
`kPrimFromRect` / `kPrimRect` in `effect_1a`, `effect_2f`, `effect_2g`
callees; `kSqrt` / `kRootWord` in `effect_2a`, `effect_2b`, `effect_2e`,
`effect_2f`, `rest_3e`, `rest_3f` callees and `magic_c3.cpp`;
`kMatrixVector` in `effect_1c`, `effect_2e`, `effect_3a`, `effect_3c`,
`effect_4d`, `effect_4e` callees. Six of those headers gained the include of
the generated header. Left as they are: the call-site tables of the fuzzes
(`{offset, 0x5A7570}` - a record of the disassembly) and the fuzz rows that
take the address as a literal (`W2E_RAW("0x5A7570", ...)`, `S04_RAW(0x5A7570)`,
`S21_AT(0x5A76F0)`, `S22_THEIRS(0x5A7590)`, `C3_RAW(0x5A7A90)`, and the
string labels in `effect_2e_fuzz.cpp`, `rest_4c_fuzz.cpp`,
`rest_4e_fuzz.cpp`), which key on the value. The teardown chain's callers
already called by name (`win_main.cpp`), and now reach ours.

The module injects after every other (before `FishingText_Arm`): every caller
of the seventeen is ours, and the fuzzes that stand them in or run Capcom's
under their own copies run first.
