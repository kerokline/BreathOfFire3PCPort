# The render backend — Direct3D 11 behind DirectX 6's objects

**Status:** IN PROGRESS (2026-09-23). Step 2 of [`display-overhaul.md`](display-overhaul.md)
§5: the backend at `k = 2`, windowed, no preset, to be capture-identical to
the DirectDraw path. Built and running the game (2026-09-23); the batch
of §5 is the record of how close.

**What followed** (noted 2026-09-24): the target made wider for the 426 x 240
picture (DIV-0041, [`widescreen.md`](widescreen.md)); `k` taken from the
window as it resizes (DIV-0042); the two looks in the present (DIV-0037,
[`crt-look.md`](crt-look.md); DIV-0043); the zero-depth corner clamp in the
backend's `DrawPrimitive` (DIV-0044); and the released-surface snapshot fix
found in the DIV-0048 runs (the last section below).

## 1. The shape of it

Every function that draws — the Direct3D handlers, the glyph and cell and
page texture builders, the display environments, the present — is ours and
reaches DirectX only through COM objects that the set-up `0x5A5160` put in
seven globals (`Dd_DirectDraw` `0x7CC334`, `DDraw_Primary` `0x7CC338`,
`DDraw_BackBuffer` `0x7CC33C`, `Dd_StageSurface` `0x7CC344`, `D3d_Device`
`0x7CC350`, `D3d_Viewport` `0x7CC354`, `D3d_BackMaterial` `0x7CC358`) and
through the surfaces `IDirectDraw4::CreateSurface` makes. The start-up
fuzzes had already enumerated the method set, because they fake those same
objects to test the handlers (`src/game/d3d_fuzz.h`, `ddraw_fuzz.h`):

| Object | Methods the game calls |
|---|---|
| `IDirectDraw4` | `CreateSurface` (+0x18) |
| `IDirectDrawSurface4` | `QueryInterface` (for `IID_IDirect3DTexture2`), `AddRef`, `Release`, `Blt`, `BltFast`, `Flip`, `GetSurfaceDesc`, `IsLost`, `Lock`, `Restore`, `SetColorKey`, `Unlock` |
| `IDirect3DTexture2` | `GetHandle`, `PaletteChanged`, `Load`, and IUnknown's |
| `IDirect3DDevice3` | `BeginScene`, `EndScene`, `SetRenderState`, `DrawPrimitive` (TLVERTEX, FVF `0x1C4`), `SetTexture`, `SetTextureStageState` |
| `IDirect3DViewport3` | `SetBackground`, `Clear` |
| `IDirect3DMaterial3` | `SetMaterial`, `GetHandle` |

So the backend **is those objects** (`src/render/render_shim.cpp`), with a
Direct3D 11 device behind them (`src/render/render_d3d11.cpp`), and the set-up
that creates them is the one function taken over. No draw handler changes.
Any method not in the table ends the process naming the object and slot
(rule 4) — the same refusal the fuzz makes, so a call nobody thought of is
found the first time it happens, not answered with nothing.

The alternative — replacing the handlers' emit side with a new interface —
would have touched eleven fuzzed functions for no gain: the vtable *is* the
interface, and it is one the original code already documented for us.

## 2. Recording, then replaying

**Nothing in the object layer touches the GPU.** A surface is a block of
pixels in memory (16 or 32 bits with whatever RGB masks the game asked
for - 1-5-5-5 and 5-5-5 textures and an X-8-8-8 screen on the owner's
machine; pitch rounded to 16); `Lock` hands the block out, `Blt` copies or fills
or stretches it on the CPU, `SetColorKey` remembers the key. A
`DrawPrimitive` converts its strip, fan or list to a plain triangle (or
line, or point) list, applies flat shading if set (the first vertex's
colour over the face, as Direct3D defined it), snapshots the device's state
(texture, blend factors, alpha test, colour key, specular, alpha op,
filter) and appends a command. `Clear` appends a command. `Flip` on the
primary, or a `Blt` onto it, is the present: the frame's command list goes
to the GPU side and is emptied.

Why: draw handlers run wherever the game calls them, and the game runs its
logic on four 16 KB cooperative task stacks
([`SCAFFOLDING.md`](SCAFFOLDING.md) §3) - **the present included**, as it
turned out (§4). Direct3D 11 and the driver want far more stack than that,
so the GPU side runs on a fiber with a 1 MB stack of its own, entered for
the length of each present and left exactly as it was entered.

**Textures rewritten mid-frame keep their old pixels for the draws already
recorded.** The game refreshes a page texture when its CLUT changes and
reuses glyph slots within a frame; a replay that read the surface at
present time would draw the earlier primitives with the later pixels. So a
surface carries a *version*: the draws recorded against it reference the
version, and a write to the surface (Lock, a Blt into it, a key change)
with draws pending first copies the pixels into the frame arena and points
the version at the copy. The replay uploads a version's snapshot before the
draws that use it and the live pixels after. A surface not drawn this frame
is simply marked dirty and uploaded when next used.

Limits, all loud: 256 K vertices, 32 K commands and an 8 MB arena per
frame, 1,024 surfaces. `InitShim` sets the first three.

## 3. The GPU side

Direct3D 11 on the game's own window: a swap chain (`DISCARD`, one buffer,
`B8G8R8A8`), a render target of the logical picture's size times the
integer scale (640 x 480 x 1 today), one dynamic vertex buffer the frame's
vertices go into with a single `Map`, two constant buffers (the target size;
per draw, the flag word and alpha test), blend states cached by
(src, dst) — Direct3D 6's `D3DBLEND` numbering is Direct3D 11's for 1..11 —
point and linear samplers with wrap addressing, a rasterizer with no
culling and no depth. Consecutive draws in the same state merge into one
`Draw`.

The shaders are compiled at start-up from source in the file (HLSL
`vs_4_0` / `ps_4_0`, `d3dcompiler_47.dll`, in every Windows the port
runs on). What they reproduce of the fixed pipeline of 1998:

- **Pixel centres.** A `D3DTLVERTEX`'s `sx`, `sy` name the pixel whose
  centre is at the integer; Direct3D 10 and later put the centre at
  `+0.5`. The vertex shader adds the half pixel. `rhw` is `1 / w`; the
  attributes interpolate against it as they did then.
- **Texturing.** Colour `= diffuse * texel` when a texture is set, the
  diffuse alone otherwise; alpha `= diffuse.a * texel.a` under
  `ALPHAOP MODULATE`, the diffuse's under `SELECTARG2`
  (`D3d_SetAlphaModulate`). `SPECULARENABLE` adds the specular after —
  which is where `D3d_PrimColor` puts a doubled channel's overflow.
- **Colour key.** A keyed surface uploads its keyed texels with alpha 0
  and every other with 255; `COLORKEYENABLE` discards a sample whose
  alpha is below one half. Exact at point sampling; a bilinear sample
  next to a keyed texel is a judgement DirectX 6 drivers made each their
  own way, which is one reason the acceptance A/B runs at `point`.
- **Alpha test.** `ALPHAFUNC` against `ALPHAREF` on the rounded 8-bit
  alpha, the eight `D3DCMP` functions.
- **Blending.** `ALPHABLENDENABLE`, `SRCBLEND`, `DESTBLEND` as given,
  `ADD`.
- **A corner at depth 0** (the port's `rhw = 0.1 / z` infinite) is drawn at
  the nearest depth the game uses, `rhw` 409.6 and `z` 1/4096, instead of
  vanishing as it did on Capcom's device: DIV-0044, the world map's compass
  needle (D41). `BOF3X_DRAWLOG_RGB=RRGGBB` logs the first 64 draws holding a
  vertex of that diffuse colour, the way that one was found.
- **Not built, and said so at run time:** a depth test, culling, fog,
  lighting, a fill mode other than solid, a `Lock` of the back buffer or a
  `Blt` reading it (only `D3d_AfterDraw` would, [`display-setup.md`](display-setup.md)
  §7, and nothing has been seen to request it), sub-rectangle locks, a
  surface format that is not RGB with masks.

The present draws the target onto the swap chain's back buffer at the
largest integer scale the client area holds, centred, black around it,
through the point or the linear sampler (`BOF3X_FILTER`), then `Present`.
So integer scaling of the *picture* is already here; rasterising *at* a
scale (target `k` x 320 by `k` x 240, `D3d_ScaleX/Y = k`) is the
display-overhaul plan's item 4b and is a set-up parameter.

## 4. The set-up

`Display_Setup` (`src/game/display_setup.cpp`) replaces `0x5A5160`
([`display-setup.md`](display-setup.md), read to its last instruction) and
writes every global the original writes, in the original's order: the
desktop's size and depth; the caches zeroed (page, CLUT rows, glyph with its
`0xFFFF` words, backdrop, after-draw, cell) and the three format records
cleared; `Gfx_RenderFlags` 0, then bit 1 (windowed allowed) and bit 9
(windowed); `D3d_TexCoords` as `i / 256 + 0.002` in double; two device
records (the original's "Software Render" and one named for this backend) so
that F7's overlay reads a name; the screen rect's width and height, the mode
depth, `D3d_ScaleX/Y` = target / 320 and / 240; `D3d_DeviceDesc`'s texture
caps and size limits; the objects into their seven slots (`0x7CC34C`, the
`IDirect3D3`, gets the DirectDraw object - nothing reads it but the
teardown's `Release`); the staging surface through `Dd_CreatePlainSurface`
(ours) and the back buffer's pitch; the material's handle; and the device
state the original's ten `SetTextureStageState` and seven `SetRenderState`
calls established, seeded into the shim's state directly.

The pixel-format records, the device caps and the scale are **what the
HAL device reported through the original set-up on the owner's machine**,
sampled read-only with `tools/mem_watch.py` on 2026-09-23 (the constants at
the top of the file): record 0 rank 2, 1-5-5-5 with alpha; record 1 rank 3,
5-5-5; record 2 rank 1, X-8-8-8; `Gfx_RenderFlags` `0x202`; texture caps
`0xCCD`, 1 x 1 to 0x4000 x 0x4000; scale 2.0; mode depth 16; back pitch
`0xA00`. Reproducing them means the texture builders (ours, fuzzed against
Capcom's) make the same texels as they did under DirectDraw, which is what
lets the captures be compared pixel for pixel. Another machine's HAL might
have reported 5-6-5 or no alpha format; the backend takes any RGB masks, so
the choice can become a setting later.

What the replacement does **not** do, on purpose (DIV-0031): enumerate
drivers or modes, set a display mode, force `Cfg_Fullscreen`, create a
clipper, or offer the software renderer.

Two things learned building it, both now in HANDOFF's traps:

- **The game calls the present from a 16 KB task stack, and those stacks
  lie inside the main thread's stack**, so a check against the TEB's bounds
  passes on them. The first present ran DXGI off the end of the task's
  stack into the scheduler's records, and the game returned into garbage a
  few calls later (`ret` to `0x1FD`, `0`, `0xFE` - task-record fields). The
  bisect: a 20 ms `Sleep` in the present's place is fine, a non-waiting
  `Present` still crashes, draws alone are fine. So every entry into
  Direct3D runs on a fiber with a 1 MB stack (`RunOnFiber`).
- `DrawState` is a Win32 macro (`winuser.h`); the shim's state struct is
  `PipeState`. `pass` is an HLSL keyword.

## 5. Checking it

The acceptance test is the picture: the same recipe captured through
Capcom's set-up (`--original Display_Setup`, which is Capcom's DirectDraw
and Direct3D 3 end to end) and through the backend, at `k = 2`, windowed,
point filter, both sides' game logic identical.

**Four shots of the attract sequence's first minute** (`analysis/shots/rb1_orig`
against `rb1_ours`, 2026-09-23): the title screen and the narration
pixel-identical; the two field scenes 13 and 3 pixels apart, every one a
single texel on a tile's edge whose colour is its neighbour's - the two
rasterisers' edge rules at a texel boundary, not a wrong texture, position
or blend. Whether the +0.5 pixel convention can be made to match those too
is open; it is not a defect of the game.

**The batch** `analysis/validate_rb1.sh` (log `analysis/attract/rb1_batch.log`):
the self-tests; the 55-shot attract cycle A/B against the DirectDraw path;
the frame hash all-ours against `ab27_orig`; the oracle and the memory
dump against their references. Results, 2026-09-23 13:08: self-tests 0
mismatches; **the attract A/B 28 of 55 identical and the other 27 between 1
and 12 pixels apart** (about 90 pixels in 55 frames of 307,200, the same
spots frame after frame); **the frame hash identical on 10,318 of 10,319
frames** - frame 0 differs by exactly the 153 calls the original set-up and
its start-up helpers made (`0x5A5160`, `0x5A5FF0`, `0x5A6050` x 51,
`0x5A60E0`, `0x5A62C0` x 5, the import thunks `0x5ACBB0` / `0x5ACBB6`, the
CRT's `0x5B9B80` / `0x5B9CD4` / `0x5B9D22`), the per-function counts
otherwise equal; **the oracle identical** on 12,126 frames and **the memory
dump identical** in all three regions.

The logic oracles are indifferent to the backend by construction - the
frame hash counts calls, the oracle counts `Rand`, the dump reads memory
the draw does not write - so a difference there would mean the set-up
wrote a global wrongly, not that a pixel moved.

## Released surfaces with pending draws (fixed 2026-09-24)

A draw is recorded against a `TexVersion`; when the game writes or releases
the surface before the present, `BeforeWrite` snapshots the pixels into the
frame arena so the draw still sees what it was recorded against. For a
*release* the snapshot was unreachable: `Surface_Release` zeroed the width
and height the snapshot is drawn through, `PresentOnFiber` swept the
released surface's GPU object before `RunFrame`, and `Bind`'s guard tested
the live `pixels` rather than the version's. Only a frame the loop had not
presented could hit it - a skipped present across an area change - which
DIV-0048's speed runs did within seconds (`analysis/attract/x4.log`,
`unlocked.tsv`), and which 1x can do after a stall under DIV-0034's 500 ms.
Now the release keeps the dimensions, the sweep runs after the frame, and
the guard fires only with neither live pixels nor a snapshot. Verified by
the 4x and 1 ms attract runs completing (`period_8.3417.log`, `period_1.log`).

Two holes the 2026-09-25 audit found by reading ([`new-code-audit.md`](new-code-audit.md)
A1, A8), closed the same day:

- **The slot.** A surface released before it was ever bound has no GPU
  object, so its slot looked free to `MakeSurface`, which wiped it for the
  next `CreateSurface` of the frame, and the snapshot was then drawn through
  the new surface's size and format. A release that takes a snapshot now
  marks the slot `snapshot_held`, which only `ResetFrame` clears.
- **The key.** A snapshot now keeps the colour key it was taken under
  (`TexVersion::color_key`), and `SetColorKey` counts as a write whether it
  sets the key or clears it. Before, clearing did not mark the surface for
  re-upload, and a key changed mid-frame re-keyed the draws already pending.
