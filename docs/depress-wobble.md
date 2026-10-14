# Depress's screen wobble: the PlayStation's swirl, the PC's black disc

**Status:** BUILT (2026-10-13, group DP). The finding came first; the
restoration followed that evening at the owner's call (section 6).

The owner's question: in the recorded fight `tools/recipes/depress.txt` (save `depress`, en-US) an enemy
casts Depress; on the PlayStation the owner remembers "a wobble kind of
effect" on the screen. Is it broken or missing on the PC release, and on
ours?

**Verdict: missing on Capcom's PC port, and ours is the same as Capcom's.**
The PlayStation copies a square of the frame it is drawing into VRAM and
draws it back as a twisting vortex. The PC port copies the same square from
its *VRAM shadow*, which never receives the rendered frame, so the texture
is empty and the player sees only the effect's black backing polygons: a
black sixteen-sided disc, up to 168 game pixels across, over the party for
about 144 frames. Under the twin-check rule
([`psx-twin-check.md`](psx-twin-check.md)) it is **the port's defect**, not
Capcom's design: restoring it would be a restoration, with a ledger entry.
Nothing in our code regressed it. The owner chose the restoration, grabbing
at the window's resolution; it is built (section 6, ledger draft I) and on
by default, `BOF3X_FBGRAB=0` for Capcom's port.

## 1. What the effect is (the PC's code, and ours)

Depress is `MAGIC109`, Magic_Rows row 12, extent `0x4D49E0..0x4D52FC`
([`magic_s25.md`](magic_s25.md) section 2; `symbols.toml`; ours in
`src/game/magic_s25.cpp`). It has six functions:

| PC | What it does |
|---|---|
| `SpellDepress_Task` `0x4D49E0` | a four-entry phase table |
| `SpellDepress_Start` `0x4D4A20` | ends at once when cast on the caster's own side. Otherwise: sound `0x100`, then a 4 x 4 `DR_MOVE` (`Gpu_SetDrawMove`) of the frame-buffer box around the task's screen point (y + 240 * `Gfx_BufferIndex`) to VRAM (`0x340`, `0x100`), committed to OT slot 3. Then the point is moved to a corner from `SpellDepress_Corners` |
| `SpellDepress_Open` `0x4D4BA0` | the radius `+9` grows by 2 a frame, to `0x54` |
| `SpellDepress_Hold` `0x4D4BE0` | holds for `0x40` frames, flags the target `0x10`, and swings the twist `+0x64` between -0x80 and 0x80 |
| `SpellDepress_Close` `0x4D4C60` | the radius shrinks by 2 a frame; at 2 the effect ends |
| `SpellDepress_DrawVortex` `0x4D4CE0` | every frame: the `DR_MOVE` again of the box (`2 * +9` square) around the point, to (`0x340`, `0x100`), slot 3; the draw mode tpage `0x11D` (15-bit, page at x `0x340`, y `0x100`); then eight bands of sixteen quads. Each quad is a **black F4** and then a **GT4 textured from the grabbed box**, with each ring turned further by the twist |

So the "wobble" is a screen-space swirl. A square of whatever has been drawn
so far is grabbed and redrawn twisted. The F4s are a black backing: a
15-bit texel of `0x0000` is transparent on the PlayStation, so pure-black
pixels in the grab would otherwise show what lies under the vortex rather
than the vortex itself.

There is no `Port_DroppedCall` and no bare `ret`. The PC code makes every
call the PSX makes (section 2). The loss is further down, in what the PC's
`DR_MOVE` can reach:

- `Gpu_SetDrawMove` `0x5A7810` builds code `0xEC`. `Gfx_DrawOTag`'s walk
  hands it to `Gfx_MoveImage(prim + 8, +0x10, +0x14)`
  ([`d3d-draw.md`](d3d-draw.md), `src/game/d3d_list.cpp` `MoveImage`).
- `Gfx_MoveImage` `0x59E9A0` / `Gfx_MoveCells` `0x5AA5D6`
  (`src/game/gfx_vram_ops.cpp`, faithful by fuzz,
  [`asset-loading-path.md`](asset-loading-path.md)) copy cells **within
  `Gfx_VramShadow`** (`0x6C9F44`, 1024 x 512 cells). Then they mark the
  destination's textures stale.
- The frame itself is drawn by Direct3D: Capcom's DirectX 6 device, or our
  Direct3D 11 backend behind the same objects
  ([`render-backend.md`](render-backend.md)). Neither writes the rendered
  picture back into the shadow. `grep -n "VramShadow" src/render/*.cpp`
  finds nothing, and the backend's command list has two kinds only,
  `kClear` and `kDraw` (`src/render/render_shim.h`).
- So the source rectangle, in the shadow's rows 0..479 (the PlayStation's
  two display buffers), holds no picture. The texture page built from
  (`0x340`, `0x100`) is empty, the GT4s add nothing, and the black F4s are
  all that shows.

Capcom's porting house did start on a frame capture. `D3d_AfterDraw`
`0x59F580` locks the back buffer and copies the frame into a 320-wide
texture (`D3d_CaptureTexture`). But nothing ever draws that texture, and no
writer of `D3d_AfterDrawRequest` was found (`symbols.toml`, group PH,
[`d3d-rest.md`](d3d-rest.md)). It is not connected to `DR_MOVE`.

## 2. The PlayStation twin

The JP disc's `BIN/BMAGIC/MAGIC109.EMI` was read with the sibling's tools:

```
python ../BreathOfFire3Recomp/tools/disc_ls.py "disc/Breath of Fire III (Japan).cue" --extract BIN/BMAGIC/MAGIC109.EMI --out <scratch>
python ../BreathOfFire3Recomp/tools/emi.py list <scratch>/MAGIC109.EMI
```

Its code section is section 3: 4,984 bytes at `0x801EEC00`, in file
offset `0x10000`. It was disassembled with capstone (MIPS32 LE), using the
scratch `dis.py`. The section has six functions ending in `jr $ra`, which
match the PC's six:

| PSX | PC twin |
|---|---|
| `0x801EEC00` | `SpellDepress_Task` |
| `0x801EEC80` | `SpellDepress_Start` |
| `0x801EEEAC` | `SpellDepress_Open` |
| `0x801EEF30` | `SpellDepress_Hold` |
| `0x801EF008` | `SpellDepress_Close` |
| `0x801EF0F8` | `SpellDepress_DrawVortex` |

Call for call (the sibling's `symbols.toml` names; Psy-Q signature matches):

- **Start** `0x801EED50..`: `SE_Play(0x100)`, then `+9 = 2`. The RECT is
  (`+0x2E - +9`, `db * 240 + +0x30 - +9`, `2 * +9`, `2 * +9`). Here `db` is
  `0x80143D44`: `sll 4 / subu / sll 4` is * 240. Then
  `SetDrawMove(cursor, rect, 0x340, 0x100)` (`0x8017B554`) and
  `Packet_Commit(3, 0x18)` (`0x8014E494`). This matches the PC.
- **DrawVortex** `0x801EF0F8`: nine rings from `rsin` / `rcos`. Then the
  same RECT, `SetDrawMove(.., 0x340, 0x100)` and `Packet_Commit(3, 0x18)`
  at `0x801EF25C..0x801EF268`. Then `SetDrawMode(cursor, 0, 1, tpage, 0)`
  and `Packet_Commit(3, 0xC)`. Then, per quad, `SetPolyF4` with colour
  0, 0, 0, `SetPolyGT4` at cursor + `0x18`, and `Packet_Commit(3, 0x18)`
  followed by `Packet_Commit(3, 0x34)` (`0x801EF910` / `0x801EF91C`).
  The F4 is committed first and the GT4 after it, in the same slot.
- **The one difference** is that the PSX picks the tpage at run time:
  `GetGraphType()` (`0x8017BC2C`) 1 or 2 gives `0x42D`, and otherwise
  `0x11D` (`0x801EF270..0x801EF298`, and again in the band loop at
  `0x801EF4A0..`). This is libgpu's `getTPage` written for both GPU
  revisions. The PC hard-codes `0x11D`, the standard GPU's value. It is
  equivalent.

`Packet_Commit` appends at a layer's tail (the sibling's note:
`CatPrim(tail[layer], cursor)`). So within slot 3 the move runs first, then
the draw mode, then each black F4 under its GT4. When the move runs, the
draw buffer already holds the layers drawn before slot 3: the battle scene
and the party sprites. On the PC the disc covers the party, which shows they
are under it. That picture is what the PSX grabs and swirls.

**Twin verdict:** the same code on both machines. The PSX's GPU executes
`DR_MOVE` from the frame buffer it is drawing; the PC's equivalent cannot.
**The defect is the port's.**

The PSX effect was not run here. The owner's PlayStation frame (2a) confirms
the reading: the party's corner of the screen twisting, darkening to black
at the centre.

### 2a. The owner's PlayStation reference (2026-10-13)

The owner sent one PlayStation frame of the effect. It is kept in this
session's scratch and never committed. The battle is against Spectres, with
Peco, Ryu and Momo in the party. In this layout the party stands at the
upper right. The frame matches the reading of section 2 point for point:

- **It is local, not the whole screen.** A round whirlpool sits over the
  party's corner. The stairs, the floor and the Spectres around it are
  untouched. That is the 168 px box around the task's corner point.
- **What swirls is the scene itself.** The floor's own tiles are twisted into
  the spiral, and the party's sprites are smeared into coloured streaks
  near the centre. That is the `DR_MOVE` of the frame being drawn,
  redrawn as GT4 bands that are each turned further by the twist.
- **It darkens towards the centre.** The outer rim blends with the floor,
  and the middle goes to black. This is the band shading: the outer ring
  is shaded `0x80`, which is neutral in the PSX's texture modulation, and
  each band inwards is `0x10` darker, down to 0 at the centre. With the
  black F4 backing this reads as a hole.
- **The "translucent" look is not semi-transparency.** No code byte is
  written after `SetPolyGT4` in the band loop (no `setSemiTrans`; the only
  stores into the GT4 at `+0x1A`, `+0x16` are the tpage and a y). The
  quads are opaque. They look translucent because their texture is the
  floor that was there a moment before, at full brightness at the rim.

**At the same moment the PC draws** the same disc in the same place, solid
black from rim to centre (f01116..f01188 in both runs). The party's corner
of the field disappears instead of twisting.

## 3. Live: the captures

Both runs used a scratch copy of `depress.txt` with a shot every 8 frames
from 900 to 1632 (`tools/recipe_shots.py --every 8 --from 900 --to 1632`,
92 shots, total kept at 1632). They used an ini-less launcher copy of this
branch's build (tip `a80f30a5`), `--lang en-US --no-front --wide 0`,
`BOF3X_OPT=none BOF3X_FILTER=point`, and the game at the main checkout's
`bof3/`. The captures are in `analysis/shots/` (gitignored). A shot named
`fNNNNN` is taken at recipe frame NNNNN + 1.

- **Ours**, `analysis/shots/depress_dp_ours`: `inject: 10088 ours, 0 left
  original`, exe host.
- **Capcom's effect**, `analysis/shots/depress_dp_orig`:
  `--original SpellDepress_Task,SpellDepress_Start,SpellDepress_Open,SpellDepress_Hold,SpellDepress_Close,SpellDepress_DrawVortex,Gpu_SetDrawMove,Gfx_MoveImage,Gfx_MoveCells`.
  The log shows `inject: 10079 ours, 9 left original by BOF3X_ORIGINAL`,
  with each of the nine logged `OFF`. This run used the hosted `BOF3.exe`
  (the runner's default with `--original`). The renderer stays ours in
  this run. Capcom's DirectX 6 path cannot write its own frames (HANDOFF
  traps). It needs no separate run, because that path also has no
  frame-to-shadow copy (section 1).

The frames:

| Frames | What shows (both runs) |
|---|---|
| f01044..f01100 | the "Depress" banner over the enemy's turn |
| f01084 | the first black dot, at the party's corner |
| f01092..f01108 | the disc grows: a black 16-gon over the party (the open phase, +2 radius a frame) |
| f01116..f01188 | full size: about 168 game px across (336 px in the 640 x 480 capture), centred near (92, 92) game px. In f01148, every pixel within 70 capture px of the centre is (0, 0, 0): 15,361 of 15,361 |
| f01196..f01228 | it shrinks |
| f01236 | the party is visible again |

`cmp.py` (scratch) compared the two runs frame by frame. **91 of 92
captures are identical**, including every frame of the effect. The one
difference is f00972, before the cast, in the party's command input:
`Attack` is shown on ours, and Nina's command ring still shows on the other
run. Every later frame matches. It is outside the effect, and its cause was
not chased (the hosts differ: exe host against hosted `BOF3.exe`).

**On ours: no regression.** Our six functions and the move path draw
exactly what Capcom's do.

## 4. What was not verified

- The PlayStation effect was not run in an emulator. Section 2 is a
  reading, confirmed by the owner's one PlayStation frame (2a).
- The shadow's contents at the source rectangle were not dumped. The
  captures show the result (pure black), which an empty texture over black
  F4s gives. A texture of black texels would give the same, so the reading
  of section 1 does not depend on which it is.
- A wide (`BOF3X_WIDE=1/2`) run was not made. The effect draws in game
  coordinates on the narrow 320 picture, so it should look the same there.
- Capcom's DirectX 6 renderer was not run. Section 1 says why it cannot
  grab either.

## 5. Proposal: a frame grab for `DR_MOVE` (as proposed; section 6 is what was built)

What is missing is that **a `DR_MOVE` whose source lies in the display area
must copy rendered pixels, at that point in the frame, into a texture that
the later draws of the destination page sample.** In the backend:

1. **Record the grab in order.** In the walk's move case (ours:
   `d3d_list.cpp` `MoveImage`, code `0xEC`), when the source rect lies
   inside a display buffer (x 0..319, y 0..239 or 240..479), keep the
   shadow copy as it is. Also append a new command, `Cmd::kGrab`, to the
   frame. It carries the source in game pixels (y modulo 240) and the
   destination in VRAM cells. The command list replays in the walk's order,
   so the grab lands after slots 0..2 and before the vortex. That is the
   PlayStation's timing.
2. **Replay it** (`render_d3d11.cpp`): `CopySubresourceRegion` from the
   render target into a grab texture, with the source scaled by the
   window's `k` and moved by the wide columns (DIV-0041). The grab texture
   holds one VRAM page (256 x 256 cells at `k`). The render target is not
   bound as a shader resource there, so the copy is legal between draws.
3. **Bind it.** The page texture that `Gfx_TexCacheFind` builds for tpage
   `0x11D` from the shadow becomes stale after the move. Draws recorded
   against it after a grab would carry a flag on their `TexVersion`
   (`render_shim.h`) naming the grab. The replay binds the grab texture for
   those draws. Their UVs are already page-relative, so no vertex rewrite
   is needed. PSX transparency: a grabbed pure-black texel should be
   transparent (alpha 0). It changes nothing visible here, because the F4
   under it is black.
4. **Ledger**: a restoration entry. The PlayStation's `DR_MOVE` from the
   frame buffer is honoured, with Capcom's port's black disc as the
   original behaviour and `BOF3X_ORIGINAL`-style switching off. The "Also
   in the PSX version?" answer is: yes, this is what the PSX does.

**Size:** about 150 to 250 lines across `render_shim.{h,cpp}` (the command
and the flag), `render_d3d11.cpp` (grab textures, the copy, the binding)
and the move case in `d3d_list.cpp`, plus the ledger entry and a headless
check of the recording. That is one agent session. Then one live run of
this recipe, and the owner's eye.

**The owner decides:** grab at the window's resolution (sharp, more detail
than the PSX had) or at 320 x 240 (the PSX's 15-bit, coarser look).

**The same mechanism would serve the other frame-buffer moves** found with
`grep -rn Gpu_SetDrawMove src/game`. Each moves from
`Gfx_BufferIndex * 240`, and each is presumably missing on the PC in the
same way (not checked live):

- `EffectKind51_DrawMoves` (`src/game/effect_2f.cpp`, two moves to
  `0x340, 0x100/0x180`);
- `GameMode8_Enter` (`src/game/rest_3g.cpp`, a 64 x 120 strip at the
  fishing mode's start);
- `Scena17_EnterArea` (`src/game/scena_sc15.cpp`, the whole 320 x 240 frame
  to `0x2C0, 0`).

The four effect states that *read* (`Gfx_StoreImage`) the grabbed
rectangle back into particle buffers on the CPU
([`d3d-rest.md`](d3d-rest.md) section 6) would need a GPU-to-CPU readback
as well. That is a larger, separate step.

## 6. The restoration, built (ledger draft I)

The owner's call (2026-10-13): build it, grabbing at the window's
resolution. It is on by default; `BOF3X_FBGRAB=0` is Capcom's port.

### 6.1 How it works

- **Recording the grab** (`src/game/d3d_list.cpp`, the walk's move case for
  codes `0xEC..0xEF`). The rectangle is read before `Gfx_MoveImage` clips
  it in place. The move into the VRAM shadow runs as before, so game code
  that reads the shadow later reads what it always did. Then
  `render::MoveFromFrame` decides. If the source lies in a display buffer,
  the backend appends a `Cmd::kGrab` to the frame:
  - a display buffer means rows 0..239 or 240..479, with the whole
    rectangle in one of them, and columns starting left of 320 that reach
    past 0 (`fb_grab::SourceInDisplay`);
  - the command carries the rectangle in the picture's coordinates (y less
    the buffer's 240 rows) and a slot;
  - either buffer counts as the frame being drawn, because the PC has one
    render target.

  The command list replays in the walk's order. So the grab sees exactly
  the ordering-table slots drawn before it, which is the PlayStation's
  timing.
- **Where grabs live** (`src/render/fb_grab.{h,cpp}`, `Registry`). Each
  grab sits in one of 32 slots, keyed by its VRAM destination.
  - A grab stays until the shadow is written over any part of it.
    `Gfx_InvalidateTextures` sees every such write and calls
    `render::ShadowWritten`. On the PlayStation, too, VRAM keeps a moved
    picture until something overwrites it.
  - Depress's own next move invalidates last frame's grab before it records
    the new one.
  - More than 32 live grabs at once stops the game loudly. None of the four
    users comes near that: each keeps at most two.
- **Which draws use it** (`src/game/d3d_draw.cpp` `D3d_BindTexture`, and
  the shim's `DrawPrimitive`).
  - For a direct-colour page (tpage bits 7..8 with bit 1 set),
    `D3d_BindTexture` now tells the shim which VRAM cell the texture's
    texel (0, 0) comes from: the page's corner plus `Gfx_TexCacheKey`'s x,
    y. It also passes the window `w x h` that the 256 texels tile, the
    same arithmetic as `D3d_BuildPageTexture`'s source.
  - A textured draw whose page meets a live grab is tagged with the grab's
    slot and the offsets.
  - Under Capcom's DirectX 6 objects the note does nothing. It checks the
    texture's table is ours.
- **Replay** (`src/render/render_d3d11.cpp`).
  - `kGrab` copies the target's rectangle (`CopySubresourceRegion`) into the
    slot's texture, sized `w*k x h*k`. Its x is `x*k + pad`, the wide
    columns moved as every vertex is, so at 16:9 and 21:9 a rectangle
    reaching into a side band takes the band's picture.
  - Parts outside the target are left transparent black.
  - The slot is remade when its size changes, as with `k` after a DIV-0042
    resize. A persisted grab keeps its old resolution until it is grabbed
    again; its UVs are normalised, so it still lines up.
  - A tagged draw binds the slot's texture on `t1`. The scene shader samples
    it wherever the texel's VRAM cell falls inside the grab:
    `fmod(uv*256, window) + page origin - grab origin`, divided by the
    grab's cells. That is continuous, so the grab shows at full window
    resolution with the page's own UVs.
  - A grabbed pure-black texel is transparent, as a 15-bit `0` is on the
    PlayStation. Outside the grab the draw samples the page as before.
  - The grab sampler clamps and follows the draw's point / linear filter.

None of the four `Gfx_StoreImage` CPU readers was touched. They still read
the shadow (empty where a grab went), as Capcom's port does:
`EffectKind6B_States[1]` `0x475EC0`, `EffectKind61_States[1]` `0x480730`,
`EffectKindA1_ReadBack` `0x490BB0` and `EffectKindA3_ReadBack` `0x491410`.

### 6.2 Checks

- **Property check, `BOF3X_SHADOW=fb_grab`** (`fb_grab::SelfTest`, runs
  under `'*'`):
  - `SourceInDisplay` against a cell-by-cell model: 20,000 random
    rectangles, 2,727 in a buffer;
  - the registry against a list model of every grab and write in order: 200
    runs of 200 operations, 16,030 finds, 8,292 that found a grab, 2,673
    refused full;
  - fixed cases: Depress's box in both buffers, the fishing strip, the whole
    frame, a move from x `0x170` (not the frame), a rectangle across the
    buffers' seam, a re-grab reusing its slot, the page to the left not
    meeting it.

  Result: `0 FAILURES`.

  Three planted controls were each refused, exit 3 (one at a time, then
  restored):

  | Control | What was planted | Failures |
  |---|---|---:|
  | C1 | `Find` returns the oldest grab | 3,785 |
  | C2 | the seam test dropped | 3,053 |
  | C3 | a write drops a grab only if it starts at or before its corner | 37,780 |
- **The recording side's existing fuzzes** cover the changed functions.
  `d3d_draw` covers `D3d_BindTexture` (20,000 rounds) and `Gfx_DrawOTag`
  (30,000 rounds, 54 of 54 table indices). `Gfx_InvalidateTextures` has
  4,000 rounds. All are 0 mismatches under `'*'`. The new calls are no-ops
  until the backend records frames, so the fuzzes compare the same game
  state as before.
- **Headless self-tests**, `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original
  BOF3X_SHADOW='*'`, from an ini-less copy of this branch's build:
  - `BOF3X_WIDE` unset: exit 0, `inject: 10087 ours, 0 left original by
    BOF3X_ORIGINAL`, and the `DIV-0041` width-rule and faster-slide lines
    as before (3,380,148 frames);
  - `=1` and `=2`: exit 0 each, with the same `inject:` line. Each logs the `DIV-0041` full-frame fills, `(-53, 0) 426 x 240` and `(-120, 0) 560 x 240`, and the faster slide on, at 53 and 120 columns, and the width-rule and slide-check lines as the unset run;
  - the `fb_grab` line, 0 failures, appears in all three.

  The three ran on the build made before the controls, and the source after the controls were restored is that build's (`git diff`: no control left).

### 6.3 Live: depress.txt

These are the owner's frames to look at (shot `fNNNNN` = recipe frame
NNNNN + 1, `analysis/shots/`, gitignored). The shots ran every 8 frames from
1084 to 1236 (`recipe_shots.py --every 8 --from 1084 --to 1236`, 20
shots). Each run used `--lang en-US --no-front`, `BOF3X_OPT=none
BOF3X_FILTER=point`, the launcher copy at scratch `L2`, and `inject: 10088
ours`.

- **Narrow** (`depress_grab_w0`, 640 x 480, k = 2): the swirl, the field's
  floor and the party twisted into a whirlpool over the party's corner and
  darkening to the centre. f01132..f01180 are the full size, and f01140 is
  the clearest. It matches the owner's PlayStation frame (2a).
  - f01156 and f01188 are the swing's still points, where the twist passes
    0, so the picture is barely turned and only darkens.
  - f01196..f01228 close it, and f01236 is the party again.
  - The log has `fb_grab move (82, 320) 4 x 4 to VRAM (832, 256): grabbed
    from the picture at (82, 80)` (Start's 4 x 4), then each frame's box
    growing by 4 (buffer 0 and 1 alternating).
  - Against the black-disc run of section 3: the 19 shots with the effect
    differ only inside the vortex's box, with bounding boxes no larger than
    (16, 21)..(352, 352) in capture pixels, game 8..176. f01236 is
    identical.
- **16:9** (`depress_grab_w1`, 852 x 480, 53 columns a side): the same
  swirl in the same place over the party, the band's picture where the box
  reaches into it. f01140 again.
- **Switch:** `BOF3X_FBGRAB=0` (`depress_grab_off`): 20 of 20 shots are
  identical to section 3's black-disc run of ours. Off is Capcom's port to
  the pixel.

### 6.4 The other three moves from the frame

| Move | Grab drawn? | Logic reads the shadow? |
|---|---|---|
| `GameMode8_Enter` (fishing; `rest_3g.cpp`): a 64 x 120 strip at x 0, rows 80.. to (`0x340`, `0x100`) | **Yes, live.** `caughFish.txt` was played with the grab on and off (shots every 60 frames, `fish_grab_on` / `_off`). The log shows `move (0, 320) 64 x 120 to VRAM (832, 256): grabbed from the picture at (0, 80)`. In 40 of 64 shots the two runs differ, and only in one place: the round pedestal under Ryu in the fishing panel (capture 72..162 x 376..420). With the grab it shows the field's ground; Capcom's port draws it blank, light grey-white (f01200 is a clear pair). This is presumably the PlayStation's look. **For the owner's eye.** | Unchanged either way: the shadow move still runs. The only readers of (`0x340`, `0x100`) found are the four `Gfx_StoreImage` effects of 6.1, which read the shadow as before; the fishing code reads nothing back |
| `EffectKind51_DrawMoves` (`effect_2f.cpp`): two 64 x 128 strips beside the effect's point to (`0x340`, `0x100`) and (`0x340`, `0x180`), every frame | By reading, yes: `EffectKind51_DrawColumn` draws its swaying column of gouraud-textured quads from `Gpu_GetTPage(2, 0, 0x340, 0x100)`, a direct-colour page, through `D3d_BindTexture`, so the column now shows the scene behind it warped (a heat-shimmer column). **Not seen live**: kind 0x51 runs in areas 34 and 108 at given chapter counters, and no recipe reaches it | Unchanged: the shadow move still runs; the effect itself only draws |
| `Scena17_EnterArea` (the ending, `scena_sc15.cpp`): the whole 320 x 240 frame to (`0x2C0`, `0`) | **Unknown.** No consumer of (`0x2C0`, `0`) was found by reading. A `grep` of `0x2C0` finds only the 4- and 8-bit effect pages at (`0x2C0`, `0x100`). If the ending draws it through a page primitive of direct colour it now shows; if through a sprite (the cell-texture path, which `D3d_BindTexture` does not serve), it does not. No recipe reaches the ending | Unchanged: the shadow move still runs; no reader of (`0x2C0`, `0`) found |

What the grab does not reach, as built:
- **Sprites** (`D3d_DrawSprt*`, `D3d_DrawCellSprite`) of a grabbed page:
  they take their textures from the cell cache, not from `D3d_BindTexture`.
- **4- and 8-bit pages** over a grab: they read the grabbed area as indices
  on the PlayStation, a combination none of the four users draws.
- **A write that covers only part of a grab** drops the whole grab. The
  rest of it then shows the shadow (Capcom's look) rather than the rest of
  the picture.

## 7. For the other files

- **Ledger (I), a restoration:**
  - **Title:** the frame-buffer grab: a `DR_MOVE` out of the display buffer
    textures what follows.
  - **Date:** 2026-10-13.
  - **Subsystem:** render (`src/render/fb_grab.{h,cpp}`,
    `render_shim.{h,cpp}`, `render_d3d11.cpp`; `src/game/d3d_list.cpp`
    `MoveImage`, `d3d_draw.cpp` `D3d_BindTexture`, `gfx_texcache.cpp`
    `Gfx_InvalidateTextures`; [`depress-wobble.md`](depress-wobble.md)).
  - **Tier:** Intent. The port could not do what the code plainly asks for,
    and the PlayStation shows what was meant.
  - **Original behaviour:** Capcom's port carries out a `DR_MOVE` as a copy
    inside its VRAM shadow (`Gfx_MoveImage` `0x59E9A0`), which never holds
    the rendered frame. Primitives that texture from a move out of the
    display buffer therefore sample nothing:
    - Depress (`MAGIC109`) draws a solid black disc over the party's
      corner for about 144 frames;
    - the fishing panel's pedestal is blank;
    - kind 0x51's column and the ending's frame copy lose their picture.
  - **New behaviour:**
    - The shadow move is unchanged, and the backend also copies that part
      of the picture drawn so far, at the window's resolution (the
      owner's call), into a texture.
    - Direct-colour page textures over the destination sample it until the
      shadow there is written again.
    - Depress shows the PlayStation's whirlpool; the fishing pedestal shows
      the ground.
    - Off: `BOF3X_FBGRAB=0`. Also `BOF3X_ORIGINAL=Gfx_DrawOTag` (no grab
      recorded) or `D3d_BindTexture` (no draw tagged).
  - **Rationale:** the PlayStation's GPU moves from the frame buffer. The
    PC's code issues the same moves call for call (the twin of `MAGIC109`,
    section 2), and the owner's PlayStation frame shows the swirl.
  - **Also in the PSX version?** Yes: this is what the PlayStation draws
    (section 2a).
  - **Verification:**
    - `BOF3X_SHADOW=fb_grab`: property checks, 0 failures, three controls
      refused;
    - `'*'` at `BOF3X_WIDE` unset, 1 and 2: exit 0;
    - `depress.txt` narrow and 16:9 (section 6.3); `caughFish.txt` with the
      grab on and off;
    - off is pixel-identical to the black-disc run (20 of 20 shots).
  - **Reversible?** `BOF3X_FBGRAB=0`.
- **HANDOFF (A):** "Depress's swirl restored (ledger I, `BOF3X_FBGRAB`):
  `DR_MOVE` out of the display buffer now grabs the picture for
  direct-colour pages. The owner's eye is owed on these frames:
  - `analysis/shots/depress_grab_w0` and `_w1`, f01132..f01180;
  - the fishing pedestal, `fish_grab_on` against `fish_grab_off`, f01200.

  Kind 0x51's column and the ending's frame copy are unseen (no recipe).
  The four `Gfx_StoreImage` CPU readers still read the empty shadow; that
  needs a GPU readback, not built."
- **README:** `depress-wobble.md` is now BUILT (2026-10-13).

