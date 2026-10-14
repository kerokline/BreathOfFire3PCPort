# Depress's screen wobble: the PlayStation's swirl, the PC's black disc

**Status:** FINDING (2026-10-13, group DP). The owner's question: in the
recorded fight `tools/recipes/depress.txt` (save `depress`, en-US) an enemy
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
Nothing in our code regressed it. The proposal in section 5 is not built.

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

## 5. Proposal: a frame grab for `DR_MOVE` (not built)

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

## 6. For the other files

- **HANDOFF / IDEAS (A):** "Depress's swirl (MAGIC109) is missing on the PC
  port and ours: a `DR_MOVE` from the frame buffer reads the empty VRAM
  shadow, so a black disc covers the party for about 144 frames. The PSX
  twin is the same code (the port's defect). Proposal:
  [`depress-wobble.md`](depress-wobble.md) section 5, a `kGrab` command
  in the backend (150 to 250 lines). It would also serve three other
  frame-buffer moves."
- **Ledger:** none yet. Nothing was changed. If the proposal is built, it
  needs a restoration entry (section 5, item 4).
- **README:** an index line for `depress-wobble.md` (FINDING, 2026-10-13).
