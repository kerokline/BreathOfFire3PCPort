# The CRT look — scanlines and halation in the present

**Status:** IN PROGRESS (2026-09-23). Built and running; the defaults are a
first guess awaiting the owner's eye. DIV-0037.

## 1. The decision

[`display-overhaul.md`](display-overhaul.md) §4c left one question for the
owner: adopt the slang / RetroArch preset format, or a single-pass HLSL
contract of our own. The owner, 2026-09-23: "for a single game like this, if
we're going the executable route, it should be a pre-packaged deal" - no
loader, no preset files, the look built into `bof3x.dll`. The model named was
libretro's `crt-easymode-halation`, "just the halation / scanlines, not the
curvature".

**That shader is GPL** (its header: "CRT Shader by EasyMode / License: GPL"),
and so are most of libretro's. Vendoring it, or translating it line by line
into HLSL, would put copyleft into the engine and end the commercial path
([`LICENSING.md`](LICENSING.md) §4, `CLAUDE.md` rule 5). So the look is **our
own shader, written from the general technique**, which is decades older than
any one implementation. Read of the GPL file, for the record: its license
header, its parameter names and defaults, and the preset's pass list
(linearize, blur across, blur down, threshold, the CRT pass) - not its code.

## 2. What it does (`src/render/crt.cpp`)

The present, instead of scaling the render target onto the window, runs four
passes of ours:

1. **Shrink** the target (320k x 240k) to the game's own 320 x 240, each
   texel the mean of its k x k block, in linear light (a power of 2.2).
2. **Blur** it across, then 3. down: a Gaussian of sigma 2 source pixels,
   13 taps. That is the glow.
4. **The picture.** For each window pixel, the source lines above and below
   it are sampled at their centres (bilinear across the target, so detail
   finer than 320 columns survives at k > 1). Each is a Gaussian beam whose
   width, in lines, runs from `beam_dark` to `beam_bright` with the line's
   brightest channel: bright lines bloom into the gap, dark lines leave it
   dark. The beams are mixed with the flat picture by a scanline strength
   running from `scan_dark` (on dark lines) to `scan_bright` (on bright
   ones). Then the glow: added at `halation`, and mixed in at `diffusion`.
   Times `brightness`, back out through the inverse power.

No curvature, no corners, no interlacing: the owner's "not the curvature".
**No phosphor mask**: the first build had an aperture grille (one of R, G,
B per window column); the owner asked for it removed the same evening, and
it is gone from the shader, not merely set to zero.

## 3. Using it

- Launcher: **Look → "CRT - scanlines and glow"** (`screen=crt` in
  `bof3x.ini`; it keeps the sharp point filter underneath).
- Environment: `BOF3X_PRESENT=crt` (`clean` is the default).
- Tuning, no rebuild: `BOF3X_CRT="halation=0.1,brightness=1.2"` -
  names `halation`, `diffusion`, `scan_dark`, `scan_bright`, `beam_dark`,
  `beam_bright`, `brightness`, `gamma_in`, `gamma_out`. An unknown
  name or a bad number is a Fatal. The `DIV-0037` log line prints the values
  in force.
- Defaults: halation 0.06, diffusion 0, scanlines 0.825 on dark lines and
  0.375 on bright, beam 0.55 .. 0.85 lines, brightness 1.25, gamma 2.2 /
  2.2. The scanlines are the first guess's 0.55 / 0.25 made 50% stronger,
  with brightness back to 1.25 to match (the owner, 2026-09-23).
- It wants at least three window pixels per game line: the borderless
  window at k = 6 on a 1440-row monitor has six; a 640 x 480 window has two,
  which reads as plain alternating lines.
- `attract_run.py` and `input_run.py` pin `BOF3X_PRESENT=clean`, so the
  owner's setting never reaches an A/B's captures.

## 4. Checked

2026-09-23, windowed at k = 3 (960 x 720), English, point filter, the
attract sequence's opening mural captured through `PrintWindow`
(`analysis/shots/crt1/`): 240 scanlines at three pixels each, the grille's
R / G / B columns, the glow around the bright figure; again without the
grille (`analysis/shots/crt2/`): line structure only; the log line with the
defaults; self-tests unchanged (0 mismatches, 796 ours). **Owed:** the
owner's eye and the numbers - above all on the borderless window at k = 6.

## 5. Open

- The defaults, in game (the owner).
- A hotkey to cycle clean / CRT live ([`IDEAS.md`](IDEAS.md) I15's kind).
- A mask stays out unless the owner asks for one back.
