# The page textures, their converters and the DirectDraw surface helpers

**Status:** IN PROGRESS (2026-09-23 - group T of the fifth parallel round:
eight functions ours, faithful, fuzzed and controlled headless on a new fake
DirectDraw, `src/game/ddraw_fuzz.*`; no divergence; through the batch `ab26` with
round six ([`takeover-queue-round5.md`](takeover-queue-round5.md) status). One latent defect written down,
D30)

Every texture the Direct3D draw binds for a textured PSX primitive is a
256 x 256 copy of a piece of the VRAM shadow, converted to the display's
pixel format and cached per PSX texture page. This group took over the two
functions that make and re-make those copies, the three pixel converters
under them and the three DirectDraw surface helpers that every texture
builder of the port shares - `src/game/tex_page.cpp`, fuzzed by
`src/game/tex_page_fuzz.cpp` under `BOF3X_SHADOW=tex_page` on the fake
DirectDraw `src/game/ddraw_fuzz.h`, which group U reuses (section 7).

Every claim about the binary below is from capstone over `bof3/BOF3.exe` on
2026-09-23 (the scratchpad's `tdis.py`, a linear sweep - the same reads
`tools/pe_disasm.py` gives) and a raw E8 / immediate scan of `.text` unless
it says otherwise. Call counts are the attract trace
`analysis/calltrace/hidden_b/bof3x.callcounts.tsv`. None of these functions
has a PSX twin: the PlayStation sampled VRAM on its GPU.

## 1. The functions

| PC | name | true size | calls (hidden_b) |
|---|---|---|---|
| `0x59F840` | `Dd_InitSurfaceDesc` | `0x1F` (to `0x59F85E`) | 10,155 |
| `0x59F860` | `Dd_CreatePlainSurface` | `0xA0` (to `0x59F8FF`) | 1 (the staging surface, `0x5A5886`) |
| `0x59F900` | `Dd_CreateTextureSurface` | `0xAB` (to `0x59F9AA`) | 224 |
| `0x5A0080` | `D3d_BuildPageTexture` | `0x434` (to `0x5A04B3`) | 107 |
| `0x5A0510` | `D3d_RefreshPageTexture` | `0x311` (to `0x5A0820`) | 3,482 |
| `0x5A9A59` | `Tex_Convert4` | `0x1A1` (to `0x5A9BF9`) | 2,242 |
| `0x5A9BFA` | `Tex_Convert8` | `0x135` (to `0x5A9D2E`) | 1,347 |
| `0x5A9D2F` | `Tex_Convert16` | `0xEF` (to `0x5A9E1D`) | not reached |

Sizes are to the byte after the last instruction; each function ends in its
`ret` with `nop` padding or the next function after it, and every jump of
each is inside it (the builders' `jmp` to their shared tails at `0x5A047D` /
`0x5A0817` included). `analysis/calltrace/entries.txt` already has the right
sizes for the four it lists (`0x59F840` `1F`, `0x5A0080` `434`, `0x5A0510`
`311`, `0x5A9A59` `1A1`). The three converters are hand-written assembly - an
`ebp` frame, `shld`, a `loop` instruction, odd entry addresses packed end to
end - and so is the packer under the 15-bit one, **`Gfx_PackRgb`
`0x5AA79C`** (`0x37` bytes), which takes its three channels in `eax`, `ebx`,
`edx` and returns in `ecx`: not callable from C, not taken over (ours does its
arithmetic inline; section 4). It has two other callers, `0x5AAA09` and
`0x5AC1D5`.

What the attract sequence reaches (the per-caller counts): only the Direct3D
path of both builders, only palettized textures - the build's 4-bit and 8-bit
converter sites `0x5A0427` (44) and `0x5A0400` (63), the refresh's `0x5A07C2`
(2,198) and `0x5A079F` (1,284). **Neither builder's software path nor any
direct-colour texture is reached**, so `Tex_Convert16` runs only in the fuzz.
`Dd_CreateTextureSurface` is reached from the build (107), the cell texture
`0x5A36D5` (94) and the glyph texture `0x5A2DA3` (23); `Dd_InitSurfaceDesc`
mostly from the glyph builder (6,005 at `0x5A2CEC`).

## 2. The texture cache entry and the build

`Gfx_TexCache` `0x6C3F40` is 32 pages x 32 entries of `0x18` bytes
(`symbols.toml`, from `Gfx_InvalidateTextures`); a page's entries are a list
ended by the first with state 0. The entry, as the builders write it:

| offset | what | written by |
|---|---|---|
| `+0` | state: 0 free, 1 built, 2 stale | build (1, last), refresh (1, first) |
| `+1` | the colour mode's low byte (0 4-bit, 1 8-bit, 2 15-bit direct) | build |
| `+2` | CLUT id, u16 (palettized only) | build |
| `+4` | the CLUT row's generation, `Gfx_ClutRows[row].counter` (palettized only) | build, refresh |
| `+8` | the texture window, `Gfx_TexCacheKey`'s 8 bytes: s16 x, y, w, h | build |
| `+0x10` | the `IDirectDrawSurface4` | build (through the helpers) |
| `+0x14` | its `IDirect3DTexture2`; 0 under the software surfaces | build |

**`D3d_BuildPageTexture(page, clut, mode)`** (`0x5A0080`), returning what
`D3d_BindTexture` sets:

1. The first entry of the page with state 0 (`0x5A00B9`, `cmp byte [eax], 0`,
   up to 32); none returns 0.
2. The source in the VRAM shadow (read **before** any call, `0x5A00DD` -
   `0x5A0127`): row `((page & 0x10) << 4) + key y`, byte `((page & 0xF) << 7)`
   plus key x `sar 1` (mode 0), x (mode 1), `2x` (mode 2), nothing (any other
   mode - the switch is on the whole dword).
3. `Dd_InitSurfaceDesc` of a stack descriptor, then `Gfx_RenderFlags` bit 0.
4. **Software surfaces** (bit 0): `Dd_CreatePlainSurface(256, 256, &+0x10, 0)`;
   unless mode bit 1 (`test al, 2`), `Gfx_ClutPixels(clut)`; `Lock(NULL,
   &desc, DDLOCK_WAIT, 0)` on the new surface; the converter (below) into
   `lpSurface` at `lPitch`; `Unlock(0)` on `+0x10` re-read; unless direct, `+2`
   and `+4`; `+0x14 = 0`; the result is the slot.
5. **Direct3D**: `Dd_CreateTextureSurface(256, 256, &+0x10, &+0x14, 0)`;
   under `Gfx_RenderFlags` bit 5 (read after the create) `SetColorKey(8
   DDCKEY_SRCBLT, &{0, 0})`; unless direct `Gfx_ClutPixels(clut)`; `Lock` of
   **`Dd_StageSurface`** `0x7CC344` (re-read at every use); convert; `Unlock`;
   unless direct `+2` and `+4`; `Blt(&{0,0,256,256}, stage, &{0,0,256,256},
   DDBLT_WAIT, NULL)` into `+0x10`; the result is `+0x14`, read after the Blt.
6. Both: state 1, `+1` the mode's low byte, `+8` the key's 8 bytes re-read
   (`0x5A047D`).

The converter: `Tex_Convert16` if mode bit 1; else `Tex_Convert8` if the mode
**dword** is non-zero (`test eax, eax`), else `Tex_Convert4` - arguments
`(lpSurface, source, [clut pixels,] key w, key h, lPitch)`, w and h read after
the `Lock`. The generation is `Gfx_ClutRows` at `(clut sar 6) * 8` - signed,
on the whole argument. A failed create or `Lock` returns 0 at once - D30.

The mode's three tests disagree beyond 3 (the x offset and the 4-bit test on
the dword, the direct test on the low byte); the callers pass `(tpage >> 7) &
3` (`D3d_BindTexture`, `symbols.toml`), so they never do. Mode 3 is built as
direct with no x offset, and `Gfx_TexCacheFind` never finds it again (it
matches direct entries by `+1 == 2`): not a defect worth an entry - the PSX's
mode 3 is reserved.

`Dd_StageSurface` is made once by the Direct3D set-up `0x5A5160`
(`Dd_CreatePlainSurface(0x140, 0x100, 0x7CC344, 0)` at `0x5A5875..0x5A5886`),
a 320 x 256 system-memory surface.

## 3. The refresh

**`D3d_RefreshPageTexture(page, slot)`** (`0x5A0510`) re-fills an existing
entry in place, for `D3d_BindTexture` when the entry is in state 2:

- state 1 **first** (`0x5A0559`), before anything can fail;
- the source as the build's, from the entry's own mode byte (zero-extended:
  `and eax, 0xFF` at `0x5A0574`) and key;
- direct (mode byte bit 1): `Dd_InitSurfaceDesc`; software - `Lock` the
  entry's surface, `Tex_Convert16`, `Unlock`, done (no generation); Direct3D -
  `Lock` the stage, convert, `Unlock`, `Blt` into `+0x10`, done;
- palettized: `Gfx_ClutPixels` of the CLUT word (pushed in `ecx` with its
  upper half left over from the x arithmetic, `0x5A06AC`; `Gfx_ClutPixels`
  masks it at `0x5A04C5`, so ours passes the word alone), `Dd_InitSurfaceDesc`,
  `Lock` as above, `Tex_Convert8` if the mode **byte** (re-read after the
  `Lock`, `test al, al`) is non-zero else `Tex_Convert4`, `Unlock`, the `Blt`
  on Direct3D, then `+4` = the generation of row `word >> 6` - unsigned here
  (`shr`), where the build shifted the signed argument.

Each `Lock` failure returns with nothing more done (D30). The return value
is not read by either caller (group R, `symbols.toml`).

## 4. The converters and the pixel formats

All three tile: the w x h texel block at `src` (VRAM rows `0x800` bytes
apart) is repeated across and down until 256 x 256 destination texels are
written, row by row at `pitch`. The loop counts are the original's - `0x100`
less w per tile across (the converters keep it in their own `dst` argument
slot, `[ebp + 8]`), `0x100` less h per tile down (in their `clut` slot or
`ebx`), and the texel count stepping by 8 / 4 / 1 - and every loop stops only
at exactly 0. So w and h must divide 256 and w be a multiple of the step, or
the loop runs on through memory; ours does the same arithmetic. The window
comes from `Gfx_TexCacheKey`, which the PSX draw mode sets; what the game puts
there has not been measured (section 8).

- **`Tex_Convert4`** (`0x5A9A59`): each source dword is 8 texels, low nibble
  first (`shld edx, eax, 0x1C` ... `4`), each looked up in the converted
  palette.
- **`Tex_Convert8`** (`0x5A9BFA`): 4 texels a dword, low byte first.
- **`Tex_Convert16`** (`0x5A9D2F`): each cell's red bits 0..4, green 5..9 and
  blue 10..14 are moved to bits 19..23 (`shl 0x13`, `and 0x3E0; shl 0xE`,
  `and 0x7C00; shl 9`) and packed by `Gfx_PackRgb`:
  `((r >> [0x7DED64]) & [0x7DED70]) | ((g >> [0x7DED68]) & [0x7DED74]) | ((b
  >> [0x7DED6C]) & [0x7DED78])`, shifts by `cl` (five bits). The PSX's bit 15
  is dropped, and a cell of 0 - transparent on the PSX - is packed like any
  other: unlike `Gfx_ConvertRow`'s palettes, which keep 0 as 0 and force other
  cells opaque (`symbols.toml`), a direct-colour texture has no transparent
  texel. Not an entry of its own: nothing the attract sequence draws is
  direct-colour (section 1).

The destination is 16 bits a texel, or 32 when `Gfx_PixelFormat` byte `+3`
(`0x7DED63`) is 4 - read once on entry by all three (`cmp byte [0x7DED63], 4`;
any other value is 16). The palettes the palettized ones index are
`Gfx_ClutPixels`'s, already in the display format.

`Gfx_PixelFormat` `0x7DED60` is the first of several 0x40-byte format
records (`0x5A60E0` copies `0x7DED60` to and from `0x7DEDA0`, `0x5A6147..`;
`0x5A62C0` is handed `0x7DEDE0`); each record's `DDPIXELFORMAT` is at
`+0x20`, which is why the helpers' format index `n` reads `0x7DED80 + n *
0x40`. The page builders pass 0, `D3d_AfterDraw` 2 (`0x59F616`).

## 5. The surface helpers

- **`Dd_InitSurfaceDesc(desc)`**: `rep stosd` of 0x1F zero dwords, `dwSize`
  0x7C, the pixel format's `dwSize` 0x20. A `DDSURFACEDESC2` - so the port's
  DirectDraw is `IDirectDraw4` (DirectX 6), not `IDirectDraw2`; `CreateSurface`
  is `+0x18` in both.
- **`Dd_CreatePlainSurface(w, h, &surface, fmt)`**: w and h rounded up to 4,
  flags `0x1007`, caps `0x840` (OFFSCREENPLAIN | SYSTEMMEMORY) under
  `Gfx_RenderFlags` bit 0, else `0x1800` (TEXTURE | SYSTEMMEMORY), the pixel
  format of record `fmt`; returns `CreateSurface(...) == DD_OK`.
- **`Dd_CreateTextureSurface(w, h, &surface, &texture, fmt)`**: w, h as given,
  flags `0x101007`, caps `0x1000` TEXTURE, caps2 `0x10` TEXTUREMANAGE, stage 0;
  failure returns 0; else `QueryInterface(IID_IDirect3DTexture2 at 0x5C4488,
  &texture)` on the new surface, returning 1 whatever it answers. (Group N
  wrote "caps `0x1800`" for the glyph texture ([`glyph-draw.md`](glyph-draw.md)
  §3); that is the plain helper's Direct3D value, not this one's.)

## 6. The fuzz and the controls

`BOF3X_SHADOW=tex_page`, one start-up run, about 3 seconds. Clones: the
three helpers as one block (their two internal calls stay inside it), the
three converters as one block (its two calls of `Gfx_PackRgb` go where the
original called - nobody owns it), and each builder with its 11 / 9 calls
re-aimed: the helpers at the helper block's copy, the converters at recorders
that log the call (where it writes, as surface and offset; where it reads, as
an offset into VRAM; which palette; w, h, pitch) and then run the converter
block's copy, `Gfx_ClutPixels` at a stand-in. Ours runs on recorders that log
alike and run ours, through `tex_page::g` - so a builder round compares
Capcom's whole tree against ours. `Dd_DirectDraw` holds the fake IDirectDraw4
for the length of the fuzz.

Compared each round: the log (every COM call with its arguments - rectangles
and colour keys by value, surfaces by id - every converter call, every CLUT
lookup), the `DDSURFACEDESC2` each `CreateSurface` was handed, **every byte
of every fake surface's buffer** (the pixels the converters wrote, and the
margins they must not), every byte of state either side could touch
(`Gfx_TexCache`, `Gfx_ClutRows`, `Gfx_TexCacheKey`, `Gfx_RenderFlags`, the
format records `0x7DED40..0x7DEE60`, `Dd_StageSurface`), and the return value
(the helpers' out-pointers folded into it).

The disturbances: after a quarter of the calls, a fake method, a recorder or
the CLUT stand-in flips `Gfx_RenderFlags` bit 0 or 5, rewrites the key, moves
`Dd_StageSurface` to the other staging surface, points the entry's `+0x10` at
another surface, changes the entry's mode byte, CLUT word, `+0x14`, key or
the generation of the round's CLUT row - always to values the converters can
take. That orders every read the builders make against every call they make.

**Rounds and coverage** (the last run): `Tex_Convert4` 2,000 and
`Tex_Convert8` 2,000 (about 30 % of each at 4 bytes a texel), `Tex_Convert16`
1,000; `Dd_InitSurfaceDesc` 2,000; the two create helpers 4,000 together (991
with `CreateSurface` failing, a quarter of the texture ones with
`QueryInterface` failing); `D3d_BuildPageTexture` 4,000 (page full 1,127,
software 1,411, Direct3D 1,462, direct 1,284, 4-bit 540, 8-bit 1,049,
colour-keyed 725, `CreateSurface` failed 464, `Lock` failed 839);
`D3d_RefreshPageTexture` 4,000 (software 2,023, Direct3D 1,977, direct 1,590,
4-bit 812, 8-bit 1,598, `Lock` failed 1,304). Seeded: pages 0, 0xF, 0x10,
0x1F; the first free entry 0, 1, 31, none, anywhere; modes 0..3, `0x100`,
`0x102`, `0xFFFFFFFF`, `0x80000000`; mode bytes 0..4, `0x80`, `0xFF`; CLUTs 0,
1, `0x3F`, `0x40`, `0x7FC0`, `0x7FFF`, `0x8000`, `0xFFFF`, `0x10000`,
`0xFFFFFFC0`; key x 0..3, 7, 8, 63, 64, 127, 128, 255, -1, -2, -8; key y 0, 1,
2, 100, 255; every w and h a converter can take (4-bit 8..256, 8-bit 4..256,
direct 1..256, h 1..256); pitches 2, 3, 256 (rows overlapping), 512, 513, 514,
640, 1024, 1030, 1280, 2048, 4000 and the width's own; bytes a texel 2, 4, 3,
0 and random; shifts 0, 3, 8, 11, 16, 19, 24, 31, 35, `0xFFFFFF0B`; masks for
555, 565 and 888 and 0, `0x8000`, `0xFFFFFFFF`; create sizes 0..5, 255..257,
320, 1021, `0x7FFFFFFF`, `0xFFFFFFFD`; format indices 0, 1, 2, `0x04000000`,
`0x04000001` (the `shl 6` wraps), `0xFFFFFFFF`. Result: **0 mismatches**.

Two guards keep a wrong function from faulting before it is compared: the fuzz
**stops at the first mismatch** (a `Fatal` naming it), and our pass's
converter recorders run the converter only when the call they just logged is
the one Capcom's copy made at the same place - a converter handed the wrong
source or destination would read or write where the original never does. Both
came from the first run of the controls below, in which five were refused by a
comparison and then faulted in a later round.

**Negative controls: 56 planted, one at a time, by a script that edits
`tex_page.cpp`, builds, runs the headless self-test and restores** (the
scratchpad's `controls.py`; run twice, the second time on the final fuzz).
52 were refused by a comparison - a descriptor, the pixels, a call's
arguments, the call count, memory, the return - and none by a hang. One was
refused by a fault, one was not refused, and three were planted as changes
that change nothing:

- **27, the build's generation row shifted unsigned**, faults: the two shifts
  differ only for a negative CLUT argument, and then the unsigned one reads
  `0x6C2A40 + 0x1FFFFFF8`, outside the image, before anything can be
  compared. Capcom's reads inside it. As group R's "CLUT compared as 16 bits"
  was, it is refused, but by the fault (`0xC0000005`), not by a count.
- **16, `Gfx_PackRgb`'s shift count not masked to five bits** (`& 63` for `&
  31` in ours), not refused: unobservable on this target - C++ leaves a shift
  of 32 or more undefined, and the compiler emits `shr reg, cl`, which masks
  the count to five bits as the original's does. Not blind: the fuzz seeds
  shifts of 35 and `0xFFFFFF0B`.
- **53-55, changes that change nothing**, not refused, as expected: the
  refresh's CLUT word sign-extended before `Gfx_ClutPixels` (which masks it to
  16 bits - the fuzz's stand-in records the argument masked for that reason);
  the build's key x and y read after `Dd_InitSurfaceDesc` rather than before
  (no call that could change them lies between); `Dd_CreatePlainSurface`
  testing the render flags' dword rather than its byte (bit 0 either way).

| # | control | refused by |
|---|---|---|
| 0 | init: pixel format `dwSize` `0x1C` | memory |
| 1 | init: zeroes `0x78` bytes | memory |
| 2 | plain: width not rounded | the descriptor |
| 3 | plain: height rounded down | the descriptor |
| 4 | plain: caps on flag bit 1 | the descriptor |
| 5 | plain: returns the HRESULT | the return |
| 6 | plain: format index `shl 5` | the descriptor |
| 7 | texture: caps2 0 | the descriptor |
| 8 | texture: width rounded | the descriptor |
| 9 | texture: returns 0 when `QueryInterface` fails | the return |
| 10 | texture: flags without `DDSD_TEXTURESTAGE` | the descriptor |
| 11 | 4-bit: high nibble first | the pixels |
| 12 | converters: 32-bit on bytes a texel `>= 4` | the pixels |
| 13 | converters: pitch added to the row's end | the pixels |
| 14 | 15-bit: green `shl 13` | the pixels |
| 15 | 15-bit: bit 15 kept | the pixels |
| 16 | 15-bit: shift count `& 63` | not refused: unobservable (above) |
| 17 | 15-bit: red and blue masks swapped | the pixels |
| 18 | 15-bit: 32-bit on bytes a texel `!= 2` | the pixels |
| 19 | build: slot search from 1 | the call count |
| 20 | build: page full returns `0x20` | the return |
| 21 | build: mode 0 x not halved | a converter's source |
| 22 | build: mode 0 x shifted unsigned | a converter's source |
| 23 | build: mode 3 offset as mode 2 | a converter's source |
| 24 | build: page column `page & 7` | a converter's source |
| 25 | build: w and h read before the `Lock` | a converter's width |
| 26 | build: 4-bit against 8-bit on the low byte | the converter called |
| 27 | build: generation row unsigned | a fault (above) |
| 28 | build: CLUT word not stored | memory |
| 29 | build: `+0x14` not zeroed (software) | memory |
| 30 | build: colour key on flag bit 4 | the call count |
| 31 | build: colour key `{0, 1}` | `SetColorKey`'s key |
| 32 | build: stage read once for `Lock` and `Unlock` | `Unlock`'s surface |
| 33 | build: `Blt` without `DDBLT_WAIT` | `Blt`'s flags |
| 34 | build: texture read before the `Blt` | the return |
| 35 | build: a failed `Lock` returns the slot | the return |
| 36 | build: a failed `Lock` marks the entry | memory |
| 37 | build: mode byte stored `& 3` | memory |
| 38 | refresh: state set last | memory |
| 39 | refresh: mode byte not re-read after the `Lock` | the converter called |
| 40 | refresh: generation row signed | memory |
| 41 | refresh: no `Blt` (Direct3D, palettized) | the call count |
| 42 | refresh: generation stored after a failed `Lock` | memory |
| 43 | refresh: `Blt` before the `Unlock` (direct) | call 2 |
| 44 | converters: source not restarted per tile across | the pixels |
| 45 | 15-bit: source not restarted per tile across | the pixels |
| 46 | build: key x and y read after the create and the CLUT | a converter's source |
| 47 | build: generation read before the `Lock` | memory |
| 48 | build: key stored as it was on entry | memory |
| 49 | build: colour-key flag read before the create | the call count |
| 50 | build: software test re-read after the create | the call count |
| 51 | refresh: CLUT word read once for lookup and generation | memory |
| 52 | refresh: surface read once for `Lock` and `Unlock` | `Unlock`'s surface |
| 53 | refresh: CLUT word sign-extended for the lookup | nothing, as expected |
| 54 | build: key x and y read after the descriptor is zeroed | nothing, as expected |
| 55 | plain: caps from the flags' dword | nothing, as expected |

(Control 28 exited 1 without a log line on the second run - the launcher, not
the fuzz; run alone it was refused by memory in round 13, as on the first
run.)

## 7. The fake DirectDraw, for group U

`src/game/ddraw_fuzz.h` (namespace `ddraw_fuzz`). A round:

```cpp
ddraw_fuzz::GlobalSwap dd(ddraw_fuzz::kDirectDraw, ddraw_fuzz::FakeDirectDraw());  // once, around the fuzz
ddraw_fuzz::SetDisturb(&MyDisturb);            // optional: called after every recorded call
// per round
ddraw_fuzz::Reset(seed);                       // forget the round's surfaces and plans
void* stage = ddraw_fuzz::MakeSurface(320, 256, bpp, pitch);   // what exists before the call
PutLong(ddraw_fuzz::kStage, stage);
ddraw_fuzz::SetPitches(pitches, n);            // the pitch of each surface CreateSurface makes
ddraw_fuzz::FailAt(ddraw_fuzz::kSurfaceCall + ddraw_fuzz::kLock, 0);   // optional: fail the first Lock
Capture(start);
ddraw_fuzz::BeginPass(0); theirs_log.Clear(); ddraw_fuzz::g_log = &theirs_log;  run Capcom's copy;
Restore(start);
ddraw_fuzz::BeginPass(1); ours_log.Clear();   ddraw_fuzz::g_log = &ours_log;    run ours;
ddraw_fuzz::SameLog(ours_log, theirs_log, why) && ddraw_fuzz::SamePixels(why);
```

- The objects are one static set, so a surface or texture pointer stored in
  game memory is the same in both passes; each surface's pixels are per pass
  (two arenas, filled alike), and `SamePixels` compares them over 256 rows at
  the surface's pitch plus a margin.
- Recorded `what`: `kDirectDrawCall + 6` CreateSurface (a copy of the
  descriptor goes to `Log::descs`); `kSurfaceCall +` `kQueryInterface`,
  `kAddRef`, `kRelease`, `kBlt`, `kBltFast`, `kFlip`, `kGetSurfaceDesc`,
  `kIsLost`, `kLock`, `kRestore`, `kSetColorKey`, `kUnlock`; `kTextureCall +`
  `kQueryInterface`, `kAddRef`, `kRelease`, `kGetHandle`, `kPaletteChanged`,
  `kLoad`. Surfaces are `kSurfaceId + index` in the log, textures
  `kTextureId + index`; rectangles and colour keys by value (`kNullRect` x 4
  for NULL). A user's own recorders use `what` below `0x100` and
  `ddraw_fuzz::Record`. Any other slot `Fatal`s naming it.
- `Lock` refuses a descriptor whose `dwSize` is not `0x7C`, a locked surface
  and a rectangle outside the surface, as DirectDraw does, and fills `dwFlags`,
  height, width, `lPitch`, `lpSurface`, the pixel format and caps. `Blt` /
  `BltFast` / `Load` copy pixels when the two rectangles are the same size and
  the surfaces the same depth; they refuse a locked surface.
- `Locate(p, &surface, &offset)` turns a pointer into a buffer into (surface,
  offset) for a recorder; `Id(p)` names a surface or texture; `Pixels(i)` is
  the current pass's buffer.
- Limits: 8 surfaces a pass, a pitch of at most 4,096, a width of at most
  1,024; 40 calls and 4 descriptors a log (more are counted, not stored).

The glyph and cell builders (group U) lock `Dd_StageSurface` and a plain
surface, `Blt` from the stage and set a colour key - all covered. Their
unpackers are other hand-written converters (`0x5A9E1E`, `0x5AA03E` ...),
cloned the same way as this group's block.

## 8. What none of it reached

- **The real DirectDraw.** The fake answers as the documentation says; the
  real one's lost surfaces, pitch rules and managed-texture behaviour are not
  exercised. The live batch is the check: every textured primitive of the
  attract sequence is drawn from a texture these functions built.
- **The texture window the game really sets.** The converters run away on a
  window that does not divide 256 (section 4); the fuzz never generates one,
  because Capcom's copy would run away too. Whether the game ever sets one is
  unmeasured - `BOF3X_TEXTLOG`-style logging of `Gfx_TexCacheKey` at each
  build would say.
- **The software surfaces and direct-colour textures**: the fuzz runs both;
  the attract sequence neither.
- `Gfx_PackRgb` itself is Capcom's on both sides of the converter rounds and
  is not taken over; `Gfx_ClutPixels` (ours, `gfx_clut.cpp`) is a stand-in
  here, checked by its own fuzz.

**What would show a wrong one, live.** The renderer is excluded from the frame
hash, so the batch checks this group by A/B captures (`BOF3X_ORIGINAL` with
these eight names against ours). A wrong build or refresh shows as a textured
primitive with the wrong texels - garbage, the wrong palette, a texture
shifted by a few texels or a page off, or none at all; a wrong converter as
wrong colours or a wrong nibble order (texels swapped in pairs), a wrong
helper as nothing textured at all (a failed `CreateSurface`). Every field
map, menu panel and portrait in the attract sequence is a palettized page
texture, so any of these differs pixel for pixel in a capture of the same
frame.

## 9. Changes to shared code

None to existing files but the lists: `CMakeLists.txt` (three sources),
`inject_all.cpp` (`TexPage_Inject` last), `symbols.toml` (one block at its
end: the eight, `Gfx_PackRgb`, `Dd_DirectDraw`, `Dd_StageSurface`; the two
builders' entries moved into it with `impl`).
