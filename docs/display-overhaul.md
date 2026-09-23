# Display overhaul — window modes, integer scaling, shaders, widescreen

**Status:** IN PROGRESS (2026-09-23). A plan and a brainstorm; step 2 of §5
is built ([`render-backend.md`](render-backend.md)), the rest is not. Addresses are `BOF3.exe`'s unless marked.

The owner's ask (2026-09-23, branch `phase-3/UI-overhaul`): borderless
windowed and fullscreen, integer scaling, an engine for shaders and advanced
filtering, and a brainstorm on widescreen — taking over what is still
Capcom's on the way. This doc says what is true about the display path today,
what each uplift needs, which functions stand in the way, how each gets
checked, and an order.

## 1. What is true today

Measured in [`display-env.md`](display-env.md), [`windowed-mode.md`](windowed-mode.md),
[`glyph-draw.md`](glyph-draw.md), [`launcher-settings.md`](launcher-settings.md)
and the round-5 takeover docs; this section only collects it.

- **API.** DirectDraw (`IDirectDraw4` / `IDirectDrawSurface4`) with an
  `IDirect3D3` device, viewport at `D3d_Viewport` `0x7CC354`, device at
  `[0x7CC350]`. Windows 11 runs it through the `d3dim700.dll` shim.
- **Modes.** `BOF3.CFG` line 1 is `Cfg_Fullscreen` `0x65DA44`, line 2 the
  renderer flag `0x65DA48`. WinMain `0x4FCB00` makes a fixed 640 x 480 window
  (`AdjustWindowRect`, centred) or, fullscreen, an exclusive 640 x 480 mode.
  F8 in WndProc `0x4FC6F0` toggles by calling the set-up again.
- **Set-up `0x5A5160`** (2,656 bytes, run once from WinMain, again on F8 /
  F7): cooperative level, primary + back buffer (windowed: a separate back
  surface, flag `0x200` in `Gfx_RenderFlags` `0x6C3A4C`; fullscreen: a flip
  chain, flag `0x100`), the device and viewport, the texture stage states
  (both filters linear, alpha test GREATER 8 — `BOF3X_FILTER=point` patches
  the two pushed immediates, DIV-0012), the 256-entry texture coordinate
  table `D3d_TexCoords` `0x7CA9E0`, the stage surface, and the scale globals.
- **Logical resolution is 320 x 240; the draw is at 2x.** Every draw handler
  multiplies PSX coordinates by `D3d_ScaleX` `0x7C9F4C` / `D3d_ScaleY`
  `0x7C9F48` - the mode's width / 320 and height / 240, 2.0 at 640 x 480 -
  straight into the 640 x 480 back
  buffer. There is no 320 x 240 picture that gets doubled afterwards. Text is
  rasterised at 640 (glyph textures are 2 x 2 per PSX texel).
- **Present.** `Gpu_PutDispEnv` `0x5A7860` → `Gfx_Present` `0x59EDF0`:
  windowed `Blt(primary, Gfx_WindowRect 0x6BE1D0, back, Gfx_ScreenRect
  0x66B708)`, fullscreen `Flip`. Ours (round 5, fuzzed, not yet run live).
- **FMV** is its own world: `Fmv_Play` `0x59E360` creates a second DirectDraw
  object, `SetDisplayMode(640,480,16)`, subclasses the window to
  `Fmv_WndProc` `0x59E570`, plays through MCI in a blocking loop; skipped
  entirely when windowed ([`replacing-mci.md`](replacing-mci.md)).
- **Focus.** The loop idles while app-active `0x6BC63B` is 0 and replays the
  missed time unrendered ([`IDEAS.md`](IDEAS.md) I12).
- **What is ours already** (this is the fact that shapes the plan): every
  Direct3D draw handler — `D3d_DrawPolyFT4` `0x5A0C40`, `D3d_DrawLineF2`
  `0x5A17A0`, `D3d_DrawSprt` / `8` / `16` `0x5A2300` / `0x5A2520` /
  `0x5A2710`, `D3d_DrawCellSprite` `0x5A2EB0`, the tiles, the glyph draw —
  the list walk `Gfx_DrawOTag` `0x59EE50`, the image upload `0x59EA70`, the
  page and cell texture builders, the display environments and the present.
  All read the scale from the globals at run time; none has a compiled-in 2
  for a position. **The render path is ours from the ordering table to the
  Blt. Only the set-up, the window and the FMV are not.**
- **What is not ours, display side** (sizes from
  [`attract-remaining.md`](attract-remaining.md)): WinMain `0x4FCB00` (1,322),
  WndProc `0x4FC6F0` (964), the input latch `0x4FC6A0` (80), the pad read
  `0x5A9700` (341), `0x4FD110` (226), `0x4FD290` (80); the set-up `0x5A5160`
  (2,656) and `0x5A5130` (46, window rect); the display-mode enumeration
  callbacks and comparator `0x5A5BC0` (640), `0x5A5E40` (96), `0x5A5EA0`
  (240), `0x5A5F90` (90), `0x5A5FF0` (82), `0x5A6050` (136), `0x5A60E0`
  (336), `0x5A6230` (129), `0x5A62C0` (186); `0x5A6690` (a device record's
  name, for F7's overlay - not a re-init, [`display-setup.md`](display-setup.md) §5.2);
  `DirectSoundCreate` `0x5A6830` (325); `DirectInputCreateA` `0x5A94C0`
  (352); `Fmv_Play` `0x59E360` (387) and `Fmv_WndProc` `0x59E570` (212); the
  lock wrapper `0x5A3CC0`. About 8 KB of code in twenty functions.

## 2. The one decision: own the presentation

Every item on the list — borderless, integer scaling, shaders, widescreen —
is a property of *where the picture goes* and *what size it is*. Today that
is decided by `0x5A5160` and WinMain, which we do not own, on an API
(DirectDraw 4 / Direct3D 3) that cannot express a shader and stretches only
by `Blt`. Two routes:

**Route 1 — keep DirectDraw, patch it wider.** What the peer project
`bof3ext` does (read, not copied — [`CLAUDE.md`](../CLAUDE.md) rule 5): the
window size immediates in WinMain, the mode-set's width/height/bpp in
`0x5A5160`, the scale globals rewritten after set-up, the texcoord table
rebuilt. It gets a bigger window and a bigger render, and its README
records fullscreen as "completely borked" and falling back to 640 x 480.
Filtering stays whatever `SetTextureStageState` can say; no shaders, no
post-process, exclusive fullscreen still mode-sets, FMV still takes the
screen. It is a day of work and a dead end for three of the four asks.

**Route 2 — a render backend of our own** ([`IDEAS.md`](IDEAS.md) I8, no
longer LOW: its prerequisite, "read the renderer's DirectDraw use", was done
in rounds four and five and the handlers are ours). The draw handlers keep
their logic and stop calling `IDirect3DDevice3` through the vtable; they
emit into our vertex stream instead. We own the window, the swap chain, the
render target, and the last pass that puts the target on screen. Then:

- **Borderless / fullscreen** is a window style and a swap chain, no
  mode-set, no exclusive mode. Alt-tab works. FMV has to draw into the same
  window (see §4).
- **Integer scaling** is the size of the render target and the scale
  globals: render at `k` x 320 by `k` x 240 with `D3d_ScaleX/Y = k`, present
  centred with borders. `k = floor(min(W/320, H/240))`, the I9 rule.
- **Shaders** are the present pass: the target is a texture, the last pass is
  a full-screen quad with a pixel shader — nearest, bilinear, a CRT, xBR,
  anything loaded from a file. The owner's point filter (DIV-0012) becomes
  the nearest preset. I15's live toggle is a key that swaps the preset.
- **Widescreen** is the target's width and the logical view width `W/k`,
  which is then the game-logic half (§5).

API choice: **Direct3D 11**. Ships with every Windows the port runs on, no
vendored library (rule 5, [`LICENSING.md`](LICENSING.md)), the shader
compiler is in the OS (`d3dcompiler_47.dll`), and llvm-mingw has the
headers. OpenGL would do as well and is what `bof3ext` half-built and left
disabled; there is no portability argument yet (the game is Win32 x86 through
and through), so take the API with the least to bundle. Revisit if a
non-Windows target ever appears.

Cost: a backend of a few hundred lines (device, swap chain, one dynamic
vertex buffer, one texture class in the two formats the handlers use, one
constant buffer for the scale and view, the present pass), plus the emit
interface in the handlers, plus taking over `0x5A5160` and WinMain's window
creation. The handlers' *logic* — vertex arithmetic, DIV-0010's far edge,
DIV-0025's texel centres, the CLUT and page lookups — does not change, and
the fuzz that checks each of them keeps checking it, because the fuzz already
records the emitted vertices through stand-ins rather than a real device.

**Recommendation: route 2.** Route 1 buys one of four asks and has to be
undone to get the rest.

## 3. How it gets checked

The overhaul is invisible to the logic oracles *by construction* and that is
worth keeping true: an oracle, memory-dump or frame-hash run is at `k = 2`,
view 320, null shader, and must stay identical to `ab26`'s references. Any
option that changes game logic (widescreen's cull bounds, camera clamp) is a
toggle, off in `attract_run.py` like the language and the filter are
([`launcher-settings.md`](launcher-settings.md) §4).

The *picture* has an oracle we already own: **`input_run.py`'s frozen window
captures** ([`input-script.md`](input-script.md) §3, 55 of 55 identical
between runs). The new backend at `k = 2`, null shader, windowed, against the
DirectDraw path on the same recipes, must produce identical captures (mask
the rounded corners; check none are black — HANDOFF Traps). That is the
acceptance test for §2 before any of §4–§5 starts, and it is I14 level 3
without the extra plumbing. Expect one class of legitimate difference: the
DirectDraw path's bilinear sampling versus ours, which is DIV-0012's choice
either way — run the A/B at point.

Beyond `k = 2` there is no reference picture; the check is the owner's eye
and a capture per recipe kept under `analysis/shots/` for the next change.

## 4. What each uplift needs, and what must be taken over

### 4a. Borderless window and fullscreen

Take over: WinMain `0x4FCB00` (the window class, the window, the message
loop, the frame pacing it holds — DIV-0022 and I16 live here too), WndProc
`0x4FC6F0` (F7 / F8, `WM_ACTIVATEAPP` → `0x6BC63B`, the input latch), the
set-up `0x5A5160` and `0x5A5130`, the mode enumeration `0x5A5BC0`..`0x5A62C0`
(read to learn what `0x5A5160` consumes; with our own backend the
enumeration itself is not needed and is retired rather than reimplemented —
say so in the ledger), `0x5A6690`. WinMain and WndProc are the two functions
the whole exe hangs off, so they go last in the group and get a live A/B of
the attract oracle, not a fuzz.

Modes: windowed (any size, `k` from the client), borderless (a `WS_POPUP`
window the size of the monitor, `k` from it), and "fullscreen" as the same
borderless window — no exclusive mode at all. A DIVERGENCE entry: the
original's exclusive 640 x 480 mode is gone; D3's fullscreen fallback
([`known-defects.md`](known-defects.md)) becomes moot rather than fixed.

FMV: with no exclusive mode, `Fmv_Play` cannot take the screen. Two ways:
the MCI player told to render into our window's client rect, letterboxed —
what `bof3ext` does — and the swap chain paused around it; or
[`replacing-mci.md`](replacing-mci.md), a bundled decoder drawing frames as
textures through the new backend (I7). The first is a day and keeps the
Indeo dependency; the second is the durable one. Do the first, keep I7 on
the list.

Input: `DirectInputCreateA` `0x5A94C0` and the pad read `0x5A9700` set a
cooperative level that assumes the old window; read them before changing the
window style. **Running unfocused (I12) belongs to this group**: once
WndProc is ours, "do not clear `0x6BC63B` on deactivate" is one line, and
the debt clamp beside it ([`windowed-mode.md`](windowed-mode.md)) is the
other thing the owner asked for.

### 4b. Integer scaling

Backend (§2) plus: `D3d_ScaleX/Y = k` at set-up and on resize; the render
target at `320k x 240k`; the present pass centres it. Our code's compiled-in
2s and 320s to audit first (an agent's grep, 2026-09-23):

- `sprt_draw.cpp:44-52` — DIV-0010's far-edge table is derived for scale 2;
  generalise it to `k` (the derivation is written out there) or the sprite
  edges drift at `k = 3`. **Done 2026-09-23**: inset
  `0.012 - (8k - 15) / (2 (8k - 1))`, the pinned table kept at k = 2.
- `glyph_draw.cpp:120` — the `2.0` is the glyph texture's 2 x 2 texel layout,
  not the screen scale; correct as it is. At `k > 2` glyphs are point-scaled
  from 640-res textures. A `k`-res glyph texture is a later, separate
  uplift (the font is the US cells doubled — [`dialogue-localisation.md`](dialogue-localisation.md);
  a better upscale is already on its list).
- Full-screen tiles `mode_flow.cpp:171`, `save_menu.cpp:391` (320 x 240 in
  logical units — right at any `k`, wrong once the view widens, §5).
- `tex_cells.cpp:412` — the 320 x 240 backdrop Blt into the stage surface:
  a VRAM-side size, stays.

Ledger: one entry, default on, "the picture is scaled by an integer and
centred; borders are black". Reversible with `k = 2` and a 640 x 480 window.

### 4c. Shaders and filtering

**Decided 2026-09-23** (the owner): no preset loader - a pre-packaged look
built into the dll. The first is the CRT look ([`crt-look.md`](crt-look.md),
DIV-0037): our own shaders, since the model the owner named
(`crt-easymode-halation`) is GPL. The rest of this section is the plan as it
stood before the decision.

The present pass takes a preset: a pixel shader file under a directory the
launcher knows (`shaders/`), compiled at start-up with `D3DCompile`, with the
uniforms every retro shader wants (source size, output size, frame count).
Ship nearest and bilinear as the two built-ins so DIV-0012 keeps its two
looks, and one CRT preset as the proof. A hotkey cycles presets (I15); the
launcher's Display box lists them. Design question for the owner: whether to
adopt an existing preset format (the slang / RetroArch shader format is the
ecosystem's lingua franca, but its presets are GLSL and multi-pass; a
single-pass HLSL contract is a fraction of the work and covers CRT, xBR and
the like) — **decide before writing the pass**, since it fixes the contract.

Where texture-stage filtering used to be, the backend samples with nearest
inside the 3D pass (the game's own bilinear-on-texels was the "glow", D17 /
DIV-0025 / DIV-0012), and the preset decides the look of the scaled picture.
The "soft" original look is then the bilinear preset at `k = 2`. Ledger: one
entry per built-in look that differs from the original's.

### 4d. Widescreen — the brainstorm

The picture side is free with §2: target `W/k x 240` logical, view width
`W/k` (426 at 1280 x 720, 480 at 1920 x 1080 with `k = 4`). The game side is
where the cost is, and it is a genuine game-behaviour divergence, so a
separate toggle, off by default and off in every oracle run. What has to
change, with the evidence of which functions hold it:

1. **The projection's centre.** `Gte_SetGeomOffset` `0x5A7AE0` (ours),
   `Gpu_SetDefDrawEnv` / `DispEnv` (ours): the view's x origin shifts by
   half the extra width, the environment's width widens. Trivial.
2. **Cull bounds.** The field culls terrain, objects and sprites against
   320 x 240 with float immediates (`bof3ext` found the sites; we locate them
   independently and they are listed by containing function in the survey
   of 2026-09-23). **25 of the 46 sites are in functions we own** —
   `MapView_Build` `0x56EC00`, `Area_TestCondition` `0x56FF00`,
   `AreaMap_ApplyPatch` `0x571110`, `Sprite_Draw` `0x5935B0`,
   `AreaMap_HeaderPass` `0x571AF0`, `Transition_DrawTile` `0x495750` — and
   become `view.left - margin` / `view.right + margin` under the toggle.
   The other 21 sit in **ten functions still Capcom's**: `0x510780` (1,060
   bytes), `0x4CDDC0` (5,860 — the big one), `0x4FF610` (1,141), `0x505480`
   (847), `0x5928F0` (222), `0x4112A0` (74), `0x589E60` (336), `0x443D90`
   (459), and two inside merged blobs `0x576960`, `0x597230` (battle's
   action text panel — stage 3's). **That is the widescreen takeover
   queue**, about 10 KB, none of it reached by the attract sequence; each
   wants a recorded route (HANDOFF 0000 item 1) before it can be A/B'd.
3. **UI anchoring.** Menus, the title, the save list, the money box, the
   backdrop tile count — most of it is ours already (`save_menu.cpp`,
   `menu_windows.cpp`, `mode_tasks.cpp`, `mode_flow.cpp`), the rest is in
   the ten functions above and `MsgBox_PlacementTable` `0x66AE10` (I9). Each
   UI element needs a policy: centre (menus, dialogue box), anchor left /
   right (the money box, location popups), or stretch (backdrops, fades,
   the black screen tiles). Written per element in the ledger.
4. **Map edges and staging.** Areas barely wider than a screen show the void
   past the edge; actors waiting off-screen for an entrance become visible;
   spawns pop. Needs a per-area cap on the extension (the camera clamp in
   `AreaMap_Frame` `0x56E6C0`, ours) and, for staging, either living with it
   or a per-scene list. **Unknown until seen**: the first widescreen build
   is a survey tool, not a release.
5. **Fixed art.** Battle backgrounds, the title, FMV: pillarbox. Battles
   proper are stage 3.

**The PSP release** (owner, 2026-09-23: "they render at 16:9, we might mine
those"; the owner's assumption is that the PSP cropped or letterboxed the
original view). Checked the same day: `PSP_GAME/SYSDIR/BOOT.BIN` on the EU
disc (`psp-eu`, `fixtures.toml`) is a plain ELF, machine 8 (MIPS), 5.0 MB,
with the game's `.EMI` data beside it — a native build, the third
compilation of the one source tree ([`SHARED_SOURCE.md`](SHARED_SOURCE.md)),
not a PS1 image under an emulator. So whatever Capcom did for the PSP's
480 x 272 is *in that ELF as constants and code*, and it answers the owner's
question directly: if the cull bounds and `MsgBox_PlacementTable` twin match
the PSX's, the PSP letterboxed or cropped; if they are wider, Capcom already
did item 2 and 3 and we can read their choices. A raw scan for the PC's
cull-bound float bit patterns found the minimums (`-50`, `-200`, `-100`,
`-60`, `-20`) several times each and **none of the maximums** (`370`,
`420`, `380`, `340`) — suggestive of widened upper bounds, but MIPS loads
floats through `lui`/`ori` immediates as often as from `.rodata`, so this
is a lead, not a finding. What the PSP actually does on screen is the
owner's to say from playing it; the ELF says how.

**Answered 2026-09-23** ([`psp-widescreen.md`](psp-widescreen.md)): the
PSP *widens*, by 32 logical columns each side into a 384 x 240 frame, and
presents rows 12..228 of it at 1.25x - a 16:9 logical window of 384 x 216,
one uniform scale, the projection centre and draw environment untouched.
Capcom re-authored only the terrain cull (`[-50, 370]` to `[-96, 416]`),
the area-map frame pass's two x ranges (widened by 31 a side), the
message box's side placements (kept at their distance from the screen
edge) and the full-frame fills; sprite and object culls were left alone.
No aspect option exists. §5 there lists the choices in this section's
terms: items 1..3 above have Capcom's own answers to copy.

The first step, as it was planned: `python tools/ghidra_pc.py`-style import of `BOOT.BIN` into the
`BoF3PC` project (language `MIPS:LE:32:default`, Allegrex is MIPS II plus
VFPU — Ghidra's stock MIPS decodes everything but the VFPU ops), then find
the placement table twin by its PSX bytes and the cull sites by the
functions' PSX pairs (`tools/psx_pair.py`'s method carries over: a third
build of the same tree). Half a day for the answer.

## 5. Order

1. **I12 in WinMain / WndProc is not first** — it needs 4a's takeover. But
   the D5 clock deadline (HANDOFF 00000) is unaffected by any of this and
   stays on its own schedule.
2. **The backend at `k = 2`, windowed, null preset, capture-identical** (§2,
   §3). Takes over `0x5A5160`, `0x5A5130`, the mode enumeration (retired),
   and the present's live run. Everything after stands on this.
3. **Window modes and I12** (4a): WinMain, WndProc, DirectInput's set-up,
   FMV into the window. The owner plays borderless. **Built 2026-09-23**
   ([`window-modes.md`](window-modes.md), DIV-0032..0035): WinMain, WndProc
   and `Fmv_Play` ours, MCI into the window (the owner's choice over I7);
   DirectInput's set-up read and left as it is - its keyboard is
   `DISCL_BACKGROUND`, which DIV-0033 answers by zeroing the pads while
   unfocused. Owner's eye owed.
4. **Integer scaling** (4b) — small once 2 exists; the `sprt_draw.cpp` table
   is the only real work. **Built 2026-09-23 evening** (DIV-0036), to the
   owner's rule: a window's k is the launcher's "Window size" (2..8,
   `BOF3X_SCALE`), a borderless window takes the largest k that fits the
   monitor, both chosen once at set-up; the far-edge table follows k
   (`SprtDraw_SetScale`); a client smaller than the target gets a fit, not
   a crop. Owner's eye owed (k = 6 borderless, k = 3 windowed). "If we do
   widescreen eventually, this will get re-evaluated" (the owner).
5. **Presets** (4c): the contract decided with the owner, nearest / bilinear
   / one CRT, the hotkey, the launcher box.
6. **Widescreen** (4d): the PSP answer first (a survey, half a day), then the
   ours-side cull and anchor changes under a toggle as a survey build, then
   the ten-function queue from recorded routes, then per-area work as the
   owner finds it.

Each takeover follows the recipe (HANDOFF "How to run things"); each look
that differs from the original's is a ledger entry; the oracle runs stay at
`k = 2`, view 320, preset off.

## 6. Open questions for the owner

- Preset contract (4c): single-pass HLSL of our own, or a slang-compatible
  loader? The first is days, the second weeks, and they are not both.
  **Answered 2026-09-23: neither loader nor files - looks built into the
  dll ([`crt-look.md`](crt-look.md)).**
- FMV (4a): MCI into the window for now, or straight to I7?
- Widescreen's default once it works: the PSP's choice, if the ELF shows
  Capcom made one, is a reasonable default to copy; otherwise off.
- The saved 640 x 480 window vs a remembered size and position in
  `bof3x.ini`.
