# The display environments, the present, and the bank load (group S)

**Status:** IN PROGRESS (2026-09-23 - group S of the fifth parallel round:
eight functions ours in [`src/game/display_env.cpp`](../src/game/display_env.cpp),
faithful, fuzzed against Capcom's at start-up (`BOF3X_SHADOW=display_env`,
40,000 rounds, 0 mismatches) and controlled headless; no divergence, no new
defect. Not yet run live - the batch after the merge is the first check, and
the only one that sees a pixel)

The fifth round's queue ([`takeover-queue-round5.md`](takeover-queue-round5.md))
gave group S the PSX library's display calls as the port implements them over
DirectDraw and Direct3D, and `Snd_LoadBank`, which group Q
([`sound.md`](sound.md)) left because it was not in its queue. All addresses
are `BOF3.exe`; disassembly by `python tools/pe_disasm.py`, 2026-09-23;
callers by a scan of `.text` for `E8` / `E9` rel32 and by `pe_xref.py`.
`symbols.toml`'s group S block has each entry's evidence.

## 1. What they are

The PSX main loop the sibling reconstructed (its `symbols.toml`, the note on
the main loop) runs `PutDispEnv` and `PutDrawEnv` on the current double-buffer
block, then `DrawOTag` on the block's `+0x8C`. WinMain's rendered-frame branch
does the same three calls in the same order with the same offset
(`0x4FCE46`, `0x4FCE55`, `0x4FCE6F` with `[0x937F84] + 0x8C`). The port kept
the library's entry points and wrote their bodies over DirectDraw:

| Function | Address | Bytes | Calls (`attract-remaining.md`) | What |
|---|---|---|---|---|
| `Gpu_PutDispEnv` | `0x5A7860` | `0x26` | 10,860 | if the `DISPENV`'s y (the word at `+2`) differs from the last one's (`0x7DECE2`), `Gfx_Present`; then the 0x14 bytes to `Gpu_DispEnv` `0x7DECE0` |
| `Gpu_PutDrawEnv` | `0x5A7890` | `0x4D` | 10,860 | if the clear colour `+0x19..+0x1B` differs from the last one's, `D3d_SetBackColor(r, g, b)`; then the 0x5C bytes to `Gpu_DrawEnv` `0x7DED00` |
| `Gpu_SetDefDispEnv` | `0x5A78E0` | `0x28` | 2 | the display rect, four words; returns env |
| `Gpu_SetDefDrawEnv` | `0x5A7910` | `0x4A` | 2 | clip rect, texture window `{0, 0, 0x100, 0x100}`, dtd and dfe 1, isbg and colour 0; returns env |
| `Gfx_Present` | `0x59EDF0` | `0x60` | 9,466 | windowed a `Blt` of the back buffer onto the primary, else a `Flip`; `Restore` on `DDERR_SURFACELOST`; then the back buffer cleared if the draw environment says so |
| `Gfx_ClearPresent` | `0x59ECE0` | `0x107` | 9,465 | bit 0 clear the back buffer, bit 1 present and clear; called with 1 only |
| `D3d_SetBackColor` | `0x5A5050` | `0xE0` | not listed | the background material's colour and the viewport's background |
| `Snd_LoadBank` | `0x587CD0` | `0xDE` | - | a kind-2 `DAT` chunk into a sound bank (`pe_funcs.py`'s 0xBA9 was wrong) |

Sizes are each body to its last instruction (`Gfx_ClearPresent` has three
`ret`s, the last at `0x59EDE6`). Every jump in all eight stays inside; the
relative calls out are `0x5A7872` (to `Gfx_Present`), `0x5A78C6` (to
`D3d_SetBackColor`), `0x59EE49` (to `Gfx_ClearPresent`), and
`Snd_LoadBank`'s four: `Crt_free` `0x587CEF`, `SndBuf_Release` `0x587D09`,
`Crt_malloc` `0x587D51`, `SndBuf_FromWave` `0x587D99`. `Gfx_ClearPresent` and
`D3d_SetBackColor` make only COM calls.

The names: the four `Gpu_` ones by their libgpu shapes (the sibling has
`PutDispEnv` `0x8017C610`, `PutDrawEnv` `0x8017C3B8`, `SetDefDrawEnv`
`0x8017AE8C` and `SetDefDispEnv` `0x8017AF5C` by whole-object Psy-Q
signature); the other three by what they do.

### The data

| Name | Address | What, and the evidence |
|---|---|---|
| `DDraw_Primary` | `0x7CC338` | the set-up's `CreateSurface` out: caps `0x200` (primary alone) windowed at `0x5A557C`, caps `0x2218` (a flip chain with one back buffer) fullscreen at `0x5A56D7` |
| `DDraw_BackBuffer` | `0x7CC33C` | windowed its own surface (`0x5A55E2`), fullscreen `GetAttachedSurface` caps 4 (`0x5A5710`) |
| `D3d_Viewport` | `0x7CC354` | `CreateViewport` at `0x5A58D7`, added to the device at `0x5A5905`; not created under `Gfx_RenderFlags` bit 0 |
| `D3d_BackMaterial` | `0x7CC358` | `CreateMaterial` at `0x5A59C8`, set black and handed to the viewport's `SetBackground` at `0x5A5A57`..`0x5A5A80` |
| `D3d_BackMaterialHandle` | `0x6C3A40` | its `GetHandle` |
| `Gfx_ScreenRect` | `0x66B708` | `{0, 0, 640, 480}` in the image |
| `Gfx_WindowRect` | `0x6BE1D0` | `0x5A5130(x, y)` stores `{x, y, x + 640, y + 480}` |
| `Gpu_DispEnv`, `Gpu_DrawEnv` | `0x7DECE0`, `0x7DED00` | the last environments put |

`Gfx_RenderFlags` `0x6C3A4C` is read here as a dword: bit `0x200` is
**windowed** - the set-up ORs it in after `SetCooperativeLevel(DDSCL_NORMAL)`
and the clipper (`0x5A5521`..`0x5A5646`), where the fullscreen path ORs in
`0x100` and asks for `0x813`. So windowed and fullscreen differ at the present
and nowhere in these functions else.

**`Gpu_DrawEnv` holds four globals the draw already had names for**, at the
PSX `DRAWENV` offsets: `Gfx_TexCacheKey` `+0x0C` (the texture window),
`Gfx_DrawTpage` `+0x14`, `0x7DED16` `+0x16` (dtd), `Gfx_DrawEnable` `+0x17`
(dfe) - and `+0x18`, isbg, which `Gfx_Present` reads. The draw-mode primitives
of `Gfx_DrawOTag` overwrite the first four during a frame; the next
`Gpu_PutDrawEnv` puts the environment's values back. That corrects
[`d3d-draw.md`](d3d-draw.md) section 1, which read `Gfx_DrawEnable` as named
only by the walk (true of instructions, not of this `rep movsd`) and so
concluded a draw mode with bit 0 clear turns off the *next* draw: the next
frame's `PutDrawEnv` sets it again first. Both environments get dfe 1 from
`Gpu_SetDefDrawEnv` and no instruction names their `+0x17` (`pe_xref`
`0x9038AB`, `0x90393B`); `0x4FD200` sets isbg 1 and the colour 0 in each
(`+0x2C..+0x2F` of the block), and three functions at `0x494F00`,
`0x495620`, `0x4956A0` store the colour bytes of both - the screen's clear
colour, which is what reaches `D3d_SetBackColor`.

## 2. How each works, quirks kept

- **`Gpu_PutDispEnv`** compares only y, as a 16-bit word. The double buffer's
  environments differ in nothing else (`0x4FD110`: y `0xF0` and `0`), so the
  frame is presented exactly when the buffers swap. The present runs *before*
  the copy, so it sees the previous environment. The copy is `rep movsd`,
  first dword first: ours copies a dword at a time in that order, so an
  environment overlapping `Gpu_DispEnv` from below repeats as the original's
  does (a control below).
- **`Gpu_PutDrawEnv`** compares the three colour bytes in order and stops at
  the first difference. It pushes each byte in a dword whose upper bits are
  its own caller's registers (`mov cl, ...` leaves `ecx`'s upper 24 bits);
  `D3d_SetBackColor` masks each with `0xFF` (`0x5A5084`, `0x5A5089`,
  `0x5A509F`), so the upper bits are dead, and ours passes zero-extended
  bytes. The fuzz compares the low bytes only.
- **`Gpu_SetDefDispEnv` / `Gpu_SetDefDrawEnv`** are not the PSX library's
  (`python tools/disasm_exe.py 8017AE8C:52 8017AF5C:16` in the sibling): the
  PSX `SetDefDispEnv` also zeroes the screen rect and the flag bytes; the PSX
  `SetDefDrawEnv` sets the draw offset to `(x, y)`, tpage `0x0A`, the texture
  window to zeros and dfe by the height. The port's writes neither the
  offset (`+8`) nor tpage (`+0x14`) and makes the window 256 x 256. Kept.
- **`Gfx_Present`**: windowed, `Blt(primary, Gfx_WindowRect, back,
  Gfx_ScreenRect, DDBLT_WAIT, NULL)`; fullscreen `Flip(primary, back,
  DDFLIP_WAIT)`. On `DDERR_SURFACELOST` (`0x887601C2`, compared exactly) it
  `Restore`s the primary, read again - never the back buffer, which windowed
  is a surface of its own. No other result is looked at. Then isbg, read after
  the present, and `Gfx_ClearPresent(1)`.
- **`Gfx_ClearPresent`** reads only the low byte of its argument. The clear
  is a colour fill of **0** through `Blt` under the software surfaces
  (`Gfx_RenderFlags` bit 0) - whatever the environment's colour - and the
  viewport's `Clear(1, Gfx_ScreenRect, D3DCLEAR_TARGET)` otherwise, which
  fills with the background material `D3d_SetBackColor` set. The `DDBLTFX` is
  a 0x64-byte stack block, zeroed, `dwSize` 0x64 and `dwFillColor` stored 0 a
  second time. Bit 1's present has no `Restore`, and windowed it returns
  without clearing.
- **`D3d_SetBackColor`** does nothing without both the material and the
  viewport. The `D3DMATERIAL` has diffuse and ambient `(c / 255, ..., 1.0)`,
  `dwRampSize` 0x20, the rest 0 - the set-up's black material with a colour.
  **The x87 here is exact before the store under any precision control:** a
  byte (8 bits) times a float's 24-bit mantissa fits in 32 bits, so `fild`,
  `fmul dword [0x5C4620]` lose nothing at 24, 53 or 64 bits and the `fstp` to
  a float rounds once. Ours computes the product in double (exact) and rounds
  it to float once; the fuzz runs the copy under `0x027F`, `0x007F` and
  `0x037F` in turn and agrees under all three. The material pointer is read
  once, at entry; the viewport is read again for `SetBackground`.
- **`Snd_LoadBank`** (group Q's `sound.md` has the bank layout): the record is
  `0x6BC928 + (bank - 1) * 0x384` in 32 bits; nothing checks the bank or the
  size. A record that holds data: `Crt_free` of it **first**, then for each
  voice entry with a buffer, `SndBuf_Release` and every `Sound_Channels` dword
  equal to the entry's buffer *as read again after the release* cleared; the
  entries are not cleared. Then the payload's first 0x380 bytes over the
  record, `Crt_malloc(size - 0x380)` - not tested for null - at `+0x380` and
  the rest copied there (`rep movsd` then `rep movsb`); then each entry whose
  first dword is non-zero becomes `data + offset - 0x380`, the data pointer
  read from the record each time, and `SndBuf_FromWave` of it the buffer. An
  entry whose offset is 0 keeps whatever buffer dword the payload carried.

What none of them returns is read: `Gpu_PutDispEnv` and `Gpu_PutDrawEnv`'s
only caller loads a register over `eax` at once (`0x4FCE4B`, and a call at
`0x4FCE5A`), and the other three void ones pass their `eax` only up to them.
Ours are `void`; the SetDef pair return env as the originals do.

## 3. The fuzz

`src/game/display_env_fuzz.cpp`, `BOF3X_SHADOW=display_env`: 4,000 rounds of
each of ten kinds - the eight alone, then two trees - against byte-copies:

- **Alone**: each copy's relative calls re-aimed at recording stand-ins
  (`CloneCall` with the expected callee, so a re-aimed site is refused), and
  ours put on the same stand-ins through `display_env::g`.
- **Trees**: a copy of `Gpu_PutDispEnv` whose call reaches a copy of
  `Gfx_Present` whose call reaches a copy of `Gfx_ClearPresent`, against ours
  calling ours; `Gpu_PutDrawEnv` over `D3d_SetBackColor` likewise.
- **Fake COM objects**, written locally (group T's `ddraw_fuzz.*` was being
  written in parallel): eight objects on one vtable - surface `Blt` `+0x14`,
  `Flip` `+0x2C`, `Restore` `+0x6C`, viewport `SetBackground` `+0x20`, `Clear`
  `+0x30`, material `SetMaterial` `+0x0C`; every other slot ends the process
  naming itself. Each records the object and its arguments - the `DDBLTFX` and
  the `D3DMATERIAL` by a hash of their bytes, with two material floats raw -
  and answers from a hash: `DDERR_SURFACELOST` three times in eight, its two
  neighbours `0x887601C1` / `0x887601C3`, the same code without the severity
  bit, a random value, or 0.
- **Disturbances**: a stand-in or a method may, one call in four, swap the
  primary, the back buffer or the viewport for another fake, flip
  `Gfx_RenderFlags` bit 0 or `0x200`, set or clear isbg, change the material
  handle or the material (to null too - only its tested reader reads it),
  change a byte of the environment being put, of `Gpu_DispEnv` or of the
  colour bytes. `Snd_LoadBank`'s stand-ins may move the record's data
  pointer, change an entry's buffer or a later entry's offset, or copy an
  entry's buffer into a channel.
- **Seeded**: the same y half the time; each colour byte the same two times in
  three (all three about 30%); environments overlapping `Gpu_DispEnv` /
  `Gpu_DrawEnv` from 8 or 4 below, 4 or 8 above, or exactly, three rounds in
  eight; the render flag's two bits half the time each; material or viewport
  null one `D3d_SetBackColor` in six; colour arguments with random upper
  bits, some black or white bytes; banks 0 and 7 (whose records overlap the
  channels and the music globals) one round in eight, entries with and without
  buffers sharing six values with the channels, sizes with every `& 3`, a
  `Crt_malloc` answer that is sometimes odd.
- **Compared** every round: `0x7DECD0`..`0x7DED70` (both environments, with
  room for the overlaps), `0x7CC338`..`0x7CC35C`, `0x6C3A40`..`0x6C3A50`,
  banks 0..7 with the channels (`0x6BC5A4`..`0x6BE1C4`), the fuzz's
  environment, heap and payload buffers, the whole log of calls (up to 192 -
  a bank load makes up to 130), and the SetDef results. `Crt_malloc` and
  `Crt_free` are stand-ins: the self-test runs before the game's C runtime.

Result (2026-09-23, headless): **40,000 rounds, 0 mismatches**; 4,475 of the
8,000 `PutDispEnv` rounds with the same y, 3,004 of the `PutDrawEnv` rounds
with the same colour, 469 bank loads of bank 0 or 7; calls seen from the
originals: present 1,795, clear 1,967, colour 2,500, `Blt` 5,794, `Flip`
3,895, `Restore` 2,155, `Clear` 1,979, `SetBackground` and `SetMaterial`
4,639 each, `Crt_free` 2,989, release 127,704, `Crt_malloc` 4,000, from-wave
170,035. With every module's self-test (`BOF3X_SHADOW='*'`): exit 0.

## 4. Negative controls

Twenty-eight bugs planted one at a time in `display_env.cpp`, each built and
self-tested headless (a scratch driver, `analysis/controls.py`, not
committed), then removed. Every refusal was by comparison - a count of
differing rounds, exit 3 - never a hang or a fault.

| Planted | Refused in (of 4,000 per kind) |
|---|---|
| `PutDispEnv` compares x and y (the dword) | 1,726 alone, 1,779 tree |
| `PutDispEnv` copies before the present | 52, 105 |
| `PutDispEnv` copies as `memmove` (overlap from below) | 1,025, 979 |
| `PutDrawEnv` ignores the blue byte | 553, 389 |
| `PutDrawEnv` copies 0x16 dwords | 3,526, 3,524 |
| `PutDrawEnv` passes b, g, r | 2,495, 1,874 |
| `SetDefDispEnv` zeroes `+8..+0x13` as the PSX library does | 4,000 |
| `SetDefDrawEnv` sets the draw offset as the PSX library does | 4,000 |
| `SetDefDrawEnv` texture window 0 x 0 | 4,000 |
| `Present` restores the primary as first read | 18, tree 10 |
| `Present` reads isbg before presenting | 54, 21 |
| `Present` restores the back buffer too | 1,486, 669 |
| `Present` tests bit 2 of the flag byte for windowed | 2,002, 865 |
| `ClearPresent` reads the render flag once (bit 1's second clear) | 25 |
| `ClearPresent` fills with white | 1,250, tree 452 |
| `ClearPresent` clears after the windowed `Blt` too | 978 |
| `SetBackColor` keeps the viewport read at entry | 32, tree 32 |
| `SetBackColor` divides by 255 in float | 1,258, 899 |
| `SetBackColor` ambient alpha 0 | 2,757, 1,882 |
| `SetBackColor` blue not masked to a byte | 2,757 |
| `LoadBank` scans the channels for the buffer as read before the release | 24 |
| `LoadBank` reads the data pointer once | 3,896 |
| `LoadBank` drops the tail bytes | 2,958 |
| `LoadBank` releases before it frees | 2,989 |
| `LoadBank` clears the entry it released | 2,989 |

Thin but refused: the primary read again for `Restore` (28 rounds in all),
the render flag re-read in bit 1 (25), the channel scan's re-read (24) - each
needs a disturbance at exactly one call, and each is seen.

Three were **not** refused, and each is a change that changes nothing
(HANDOFF Traps):

- *`D3d_SetBackColor` reads the material again before `SetMaterial`.* No call
  lies between the entry's read and the use, so nothing can change it. The
  comment says "read at entry" as a description, not a claimed quirk.
- *The colour products in float* (`(float)c * (float)k` rather than the
  double product rounded once): the product is exact in 32 bits, so a float
  multiply rounds the same exact value once. This is the section 2 argument
  that the x87 precision does not matter here, confirmed; the fuzz's rounds
  under `0x007F` and `0x037F` confirm it from the other side.
- *`Snd_LoadBank`'s tail copied by `memmove`*: it differs from `rep movsd`
  only when the payload overlaps the fresh `Crt_malloc` buffer, which a real
  allocator cannot return. Unreachable, so not seeded.

## 5. What none of this reached

- **No pixel.** The fuzz sees which COM calls are made with which arguments;
  what DirectDraw does with them - that the frame appears, that the clear is
  the right colour - only the live batch's captures can say, and those are
  the check that matters for this group ([`takeover-queue-round5.md`](takeover-queue-round5.md),
  "The live check").
- **A lost surface in game.** `DDERR_SURFACELOST` is fuzzed, never seen live;
  that windowed only the primary is restored (the back buffer is a surface of
  its own there) is read, not observed - it may matter after a mode change or
  a locked desktop, and nobody has looked.
- **Bit 1 of `Gfx_ClearPresent`** has no caller: only the fuzz runs it.
- **The software surfaces** (`Gfx_RenderFlags` bit 0): what selects them
  was not read here, and their clear path is fuzz-only.
- **Banks outside 1..6, a size below 0x380, a null from `Crt_malloc`** - the
  first is fuzzed (0 and 7), the other two would write through wild pointers
  in both and were not.
- `Snd_LoadBank`'s second caller `0x4547E9` (a loop over kind-2 chunks, not
  ours) and what reaches it were not read further than the call.
