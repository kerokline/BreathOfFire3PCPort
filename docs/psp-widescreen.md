# The PSP release's 16:9 — what the ELF shows

**Status:** DRAFT — verified 2026-09-23 against `BOOT.BIN` of `psp-eu`
(`fixtures.toml`; the placement table also checked on `psp-jp`).

This is the "first step" of [`display-overhaul.md`](display-overhaul.md) §4d.
It reads the PSP build's code and data and reports what Capcom changed to fill
a 480 x 272 screen. It says nothing about what the PSP *looks like* while
playing — that is the owner's to report from the handheld; every on-screen
consequence below is marked as an inference from code.

## 0. The assumption and the finding, kept apart

**The owner's assumption** (2026-09-23): the PSP release cropped or
letterboxed the original 320 x 240 view to 16:9.

**What the ELF shows:** the PSP build does both, on different axes, and
widens the logical view rather than merely cropping it.

- The game draws into a **384 x 240 logical frame**: every primitive's x is
  shifted right by 32 at conversion time, the draw environment stays 320 x 240
  and the projection centre stays (160, 120), so the original view sits in the
  middle of a frame that is 32 px wider on each side (§2.3).
- The frame is presented by drawing a **384 x 216 window of it, starting at
  row 12, as a 480 x 272 textured quad** — a uniform 1.25x scale (§2.4). So the
  top and bottom 12 logical rows are cropped, and 64 logical columns are added.
  384 x 216 is exactly 16:9.
- Capcom then re-authored the things that assumed a 320-wide view: the
  terrain cull's x range, the two x ranges of the area-map frame function,
  the side positions in the message-box placement table, and the full-frame
  fills (§3). The sprite, object and remaining culls were left alone.
- A second presentation rectangle exists — the original 320 x 240 view's
  middle 180 rows scaled 1.5x, a true crop to 16:9 — but it is selected only
  by the PSP's boot-time splash sequence, never by game code (§2.5). No
  runtime aspect option was found (§4).

## 1. What was examined

`PSP_GAME/SYSDIR/BOOT.BIN` of the European disc, extracted to a scratch
directory with `7z e` (never into the tree — CLAUDE.md rule 1): a plain ELF,
`e_type 0xFFA0`, `e_machine 8` (MIPS), 5,075,540 bytes. Two program headers:

| PH | vaddr | filesz / memsz | holds |
|---|---|---|---|
| 0 | `0x0` | `0x3737a0` | `.text` (`0x0`, `0x2cf710` bytes), sce stubs, `.rodata` (`0x2d04e0`), `.data` (`0x2ddbc0`, `0x95b90` bytes) |
| 1 | `0x3737a0` | `0x64` / `0x11f094` | `.cplinit`/`.ctors`/…, `.bss` (`0x373840`, `0x11eff4` bytes) |

Relocations (`.rel.*`, type `0x700000a0`) are not applied, so **every PSP
address in this document is a link-time offset from 0**, as Ghidra shows it
after a `BinaryLoader` import of PH0 at base 0 (`MIPS:LE:32:default`, project
`BoF3PSP`, analysed 2026-09-23; the function bounds cited come from its
function list, 11,748 functions). A `lui`/`lo16` pair that resolves inside
`.text` but is used as data is a `.bss` reference and its real offset is the
raw value **+ 0x3737a0**; both are given where it matters. The runtime base
(`0x08804000` on hardware) is added to everything.

Two facts about the compiler shape the pairing with the PSX build: the PSP
code was built with a GCC-style Allegrex toolchain — it loads small constants
with `ori rX, $zero, imm` where the PSX's Psy-Q used `addiu`, and it writes a
range check as two `slti` against the low and high bound where the PSX wrote
`addiu +K; sltiu M`. The PSX build compares screen x through `sltiu` after
adding the margin, so its ranges below are derived from those two immediates.
Nothing in the PSP `.text` uses floating point for any of this: a scan of
every `lui` immediate for float-looking upper halves finds 0.5, 1.0, 2.0, 8.0
and π-related values only; the PC port's float bounds (`-50.0`, `370.0`, …)
appear neither in `.rodata` nor as `lui` immediates.

Tools: [`tools/psp_elf.py`](../tools/psp_elf.py) (`sections`, `dis`, `imm`,
`ranges`, `refs`, `find`), written for this; Ghidra's function and reference
dump; capstone for the PSX side on the sibling's `SLPS_009.90`; the PC side
via `tools/pe_disasm.py`. Every claim below names the command or the
instruction bytes that produced it.

## 2. The presentation path, as the code has it

### 2.1 The screen: 480 x 272, full

The GU set-up function `0xa3468` (1,288 bytes; 28 distinct libgu calls, the
most of any game function) does, in order (`psp_elf.py dis 0xa35f0 0xa36a0`):

```
000a360c  jal sceDisplaySetMode(0, 480, 272)          ; stub 0x2cfd40, a1=0x1e0 a2=0x110
000a366c  jal 0x2b9f78 (480, 272, 0x44000, 512)       ; display buffer, stride 512
000a3678  jal 0x2ba7b0 (0x710, 0x778)                 ; sceGuOffset(2048-240, 2048-136)
000a368c  jal 0x2ba6fc (0x800, 0x800, 480, 272)       ; sceGuViewport(2048, 2048, 480, 272)
```

The libgu routines were identified by the GE command bytes each one emits
(`0x2ba7b0` emits `0x4c 0x4d` OFFSETX/Y, `0x2bbb64` emits `0x42..0x46`
viewport scale/centre, `0x2bc81c` emits `0xd4 0xd5` SCISSOR, `0x2bb5c8`
emits `0x08 0x10` PRIM and its vertex list). The same function writes the
presentation rectangle's defaults (§2.4).

### 2.2 The draw environment, projection centre and perspective: unchanged

The PSP twin of the PSX's `SetDefDrawEnv`/`SetDefDispEnv` set-up
(`0x8014ad80` in `Main_FrameLoop`, sibling names) is `0xe1c` (132 bytes),
instruction for instruction the same four calls with the same arguments:

```
PSX 8014ad9c addiu a3,0x140 / 8014ada4 addiu s0,0xf0 -> jal SetDefDrawEnv(env, 0, 0, 320, 240)
PSP 00000e34 ori   a3,0x140 / 00000e44 ori   t0,0xf0 -> jal 0x9f8a8         (env, 0, 0, 320, 240)
```

and so on for the other three (`0xe48`, `0xe60`, `0xe78`; the PSP's
`0x9f890`/`0x9f8a8` store the four halfwords and return). The projection
centre is set at `0xe43c`..`0xe444` — `ori a0,0xa0; jal 0x9d22c; ori a1,0x78`
— i.e. `SetGeomOffset(160, 120)` after `SetGeomScreen(1000)` at `0xe434`,
exactly the PSX's `0x8014ad44`..`0x8014ad54`. `0x9d22c` stores the two values
to `.bss` `0x709e0`/`0x70a20` (+`0x3737a0`); their only reader is the
perspective divide `0x9d24c` (176 bytes), which computes
`sx = OFX + (x·H)/z` and `sy = OFY + (y·H)/z` with `mult`/`div` and no scale
(`0x9d29c`..`0x9d2f8`). Two further `SetGeomOffset` callers exist on the PSP
— `(160, 144)` at `0x681bc` and `(?, 185)` with `SetGeomScreen(300)` at
`0x26b038` — in code the PSX boot EXE does not contain (its two callers,
`0x8014ad50` and `0x801548d8`, both pass `(160, 120)`); they are recorded
here as unpaired, not as changes.

### 2.3 Primitive conversion: x + 32, into a 384 x 240 frame

The PSP twin of `DrawOTag` is `0xa3db4` (1,364 bytes): it walks the ordering
table and dispatches on the primitive code byte (`lbu a0, 7(s0)` at
`0xa3f24`, then a `slti` ladder) to 21 converter functions in
`0x9fb64`..`0xa1dd8`, each of which allocates a GE vertex list (`0x9eeb4`)
and issues it (`0x2ba3ac`). **Every converter adds 32 to each x** and copies
y unchanged. `POLY_F3`'s converter `0xa0220` (244 bytes) is the plain case:

```
000a0260  lh    a0, 8(s0)      ; x0
000a0264  addiu a0, a0, 0x20   ; + 32
000a0268  sh    a0, 4(s1)      ; vertex x
000a026c  lh    a0, 0xa(s0)    ; y0
000a0274  sh    a0, 6(s1)      ; vertex y, untouched
```

A scan of the converter range for `addiu rX, rX, 0x20` finds 51 sites across
the 21 functions (3 per triangle, 4 per quad, 1 per sprite/tile, 2 per line;
`psp_elf.py dis 0x9fb00 0xa1f00 | grep 'addiu.*0x20$'`).

Before the walk, the same function sets the frame's scissor and clears it:

```
000a3e9c  ori a2, 0x180 ; ori a3, 0xf0 ; jal 0x2baf2c   ; sceGuScissor(0, 0, 384, 240)
000a3ec4  ori a1, 0x1a0 ; sh a1, 4(a0) ; sh s4(=240), 6(a0)  ; clear quad (0,0)-(416,240), bg colour
```

and selects the draw buffer as `frame × 0x3c000` (512 x 240 x 2 bytes) at
`0xa3e64`..`0xa3e90` — two 512-wide, 240-row frames in VRAM.

### 2.4 Presentation: the 384 x 216 window at row 12, scaled 1.25x

After the walk (`0xa4204` on), the function switches the draw buffer to the
display buffer, sets the scissor to `(0, 0, 480, 272)` (`0xa4244`), binds the
frame it just drew as a 512 x 512 texture (`0x2baab0`, `0x2bac84`), and
draws it as one sprite through `0x2ba4f4` + `0x2ba510` → `0x2bb5c8`:

```
000a42a8  lh a0, 4(s1)   ; src w  = [0x7b428]  (.bss 0x3eebc8)
000a42ac  lh a1, 6(s1)   ; src h  = [0x7b42a]
000a42b0  ori a2, 0x1e0  ; dst w  = 480
000a42b4  jal 0x2ba4f4   ; ori a3, 0x110 ; dst h = 272  -> stored at ctx+0x38..0x44
000a42bc  lw a0, [0x7b420] (frame index) ... t0 = frame*240 + [0x7b426] (src y)
000a42e8  jal 0x2ba510 (dst x=0, dst y=0, 0, t0=src y, 0, 0)   ; src x = [0x7b424] via a3
```

`0x2bb5c8` (344 bytes) writes the two vertices: `(u, v, x, y) = (srcx, srcy,
0, 0)` and `(srcx + srcw, srcy + srch, 0 + 480, 0 + 272)` (`0x2bb628`..
`0x2bb668`). So the source rectangle in `.bss` at raw `0x7b424` (`x, y, w, h`
halfwords; `0x3eebc4` resolved) is stretched onto the whole 480 x 272 screen.

The rectangle has exactly three writers (Ghidra references plus
`psp_elf.py refs 0x3eebc4`):

| Writer | Values | Where it runs |
|---|---|---|
| GU init `0xa349c`..`0xa34b8` | `(0, 12, 384, 216)` — `ori a0,0xc; ori t1,0x180; ori a0,0xd8` | once at start-up |
| `0xa2198` (28 bytes, a plain 4-halfword store) called from `0x2b79e4` | `(0, 12, 384, 216)` — `ori a1,0xc; ori a2,0x180; ori a3,0xd8` | the shell's third sub-state, every frame (§2.5) |
| `0xa2198` called from `0x2b8224` (in `0x2b8210`, 40 bytes) | `(32, 30, 320, 180)` — `ori a0,0x20; ori a1,0x1e; ori a2,0x140; ori a3,0xb4` | the shell's first two sub-states (§2.5) |

`(0, 12, 384, 216)` to 480 x 272 is 1.25x on both axes. `(32, 30, 320, 180)`
is the original 320-wide view (columns 32..352 of the frame) cut to its
middle 180 rows, and 480/320 = 1.5 against 272/180 = 1.511 — a crop, and a
fraction off square.

Inference for the owner to check: **in play, logical columns -32..352 and
rows 12..228 of the original 320 x 240 space are on screen, at 1.25 PSP
pixels each.** Anything the original drew in rows 0..11 or 228..239 is off
the top or bottom.

### 2.5 Which rectangle when: the boot shell, not the game

The three rectangle writers hang off a small state machine that the PSP build
adds around the game. Its state block is `.bss` raw `0x1006a0` (`0x473e40`):
byte `+2` is the major state, `+3` a sub-state, `+4` a step, `+5` a counter.
`0x2b6fb0` (60 bytes) dispatches by `+2` into a 7-entry pointer table in
`.data` at `0x363b88`:

| `+2` | handler | does |
|---|---|---|
| 0 | `0x2b6fec` | boot: sets `+2 = 1` |
| 1 | `0x2b7050` | one pass; sets `+2 = 2`, `+3 = 1` (`0x2b7128`, `0x2b712c`) |
| 2 | `0x2b713c` | dispatches by `+3` into the table's last four entries |
| 3 | `0x2b7178` | `jr ra` |
| 4 | `0x2b7180` | per-step table `0x363ba4` (16 steps, fades and waits — `0x2b71c4` re-runs the graphics init `0xe408`), **then the crop rectangle** via `0x2b8210` |
| 5 | `0x2b7878` | per-step table `0x363be4` (3 steps: a file load `0x97d44(8, 1)`, a wait on `0x96ee8`, then `+3 = 3` at `0x2b7984`), **then the crop rectangle** |
| 6 | `0x2b79a8` | per-step table `0x363bf0` (14 steps; `0x2b79f8` re-runs `0xe408` with the game's VRAM layout), **then the wide rectangle** `(0, 12, 384, 216)` |

The last step of the wide table, `0x2b7fe4`, zeroes `+3` and `+4` and
registers `0x2b8700` through `0x1834`. `0x2b8700` (88 bytes) is the game's
main loop: `0x18d0`, `0x133f4`, then forever — dispatch by the mode halfword
at `.bss` raw `0xd1f2e` into the table `0x363c68` (entry 0 `0x2b8758` writes
the area and mode bytes and calls the mode set-up `0x8db30`, `0x8e538`), then
`0x1758(1)`. From then on `0x2b713c` lands on the `jr ra` entry and the game
runs with the rectangle left as the wide one. Game code writes `1` and `2` to `+2` at many sites (events, the
ending region `0x26f000`..`0x2b6000`), which re-enters the sequence, so the
crop rectangle can recur on a return to the shell; **no game-side code writes
the rectangle itself** (the three writers above are the only ones). The one
movie on the disc, `USRDIR/USA/PSMF/caplogo.pmf`, is 480 x 272 by its PSMF
header (`0x8e`/`0x8f` = `0x1e`/`0x11`) and is presented by a separate path
(`0xa4334`, selected by the flag at `.bss` raw `0x70aa0`, drawn centred on
`(240, 136)`), not through either rectangle.

Hypothesis, from the step contents: sub-states 1 and 2 are the splash / logo
sequence and sub-state 3 leads into the title. What the owner sees during
boot versus in the game is the check.

## 3. Every changed constant and table

PSX addresses are the sibling's boot EXE (`SLPS_009.90` at `0x80093800`);
PC addresses are `BOF3.exe`; the PC's sites and their containing functions
are the ones §4d item 2 lists (and the peer project `bof3ext`'s
`widescreen_patches.ixx` patches — read for its site list only). Ranges are
the *kept* screen-x interval, inclusive. PSP twins were paired by call
sequence: each cull follows the same load-vector / perspective / store-xy
triple (`0x9da54` / `0x9da78` / `0x9dad4`; `0x9da78` calls the perspective
divide `0x9d24c` of §2.2) as the PSX's `lwc2` / `cop2 0x0180001` (RTPS) /
`swc2` at the same point, then reads the projected x from `.bss` raw
`0x104fe0` (`0x478780`) where the PSX reads `0x1f800034` (scratchpad SXY).

### 3a. Changed

| # | Item | PSX | PC | PSP | PSP evidence |
|---|---|---|---|---|---|
| 1 | Terrain cull, x, in the `MapView_Build` twin `0xd840` (3,012 bytes; PSX `0x80153b8c`, PC `0x56EC00` ours) | `[-50, 370]` — `80153ea8 addiu +0x32; 80153eb0 sltiu 0x1a5` | `-50.0`/`370.0` at `0x56EDFA`/`0x56EE14` | **`[-96, 416]`** | `0000db90 slti a0,a3,-0x60` / `0000db98 slti a0,a3,0x1a1` |
| 2 | Same function, y | `[-200, 290]` — `80153ec8 addiu +0xc8; sltiu 0x1eb` | — | `[-200, 290]` unchanged | `0000dba8 slti -0xc8` / `0000dbb0 slti 0x123` |
| 3 | Area-map frame function, wide x range: PSP `0x7c918` (1,448 bytes) ↔ PC `AreaMap_FrameAreaBD` `0x510780` (PSX `0x801f2c04`, overlay, hypothesis) | overlay — not in the boot EXE | `-200.0`/`520.0` at `0x51097E`/`0x510991` | **`[-231, 551]`** | `0007cc40 slti -0xe7` / `0007cc48 slti 0x228` |
| 4 | Same function, narrow x range | overlay | `-50.0`/`370.0` at `0x5109BB`/`0x5109D2` | **`[-81, 401]`** | `0007cc60 slti -0x51` / `0007cc68 slti 0x192` |
| 5 | `MsgBox_PlacementTable` entries 3..6 (x of the side placements; y unchanged) | `0x801802BC`: `145, 226, 145, 226` (JP; US `SLUS_004.22` file `0xe9944` identical) | `0x66AE10`: `145, 226, 145, 226` | **`113, 258, 113, 258`** (`.data` `0x2df0e0`; `psp-jp` identical at file `0x2df970`) | bytes `71 00 56 00 02 01 56 00 71 00 82 00 02 01 82 00`; reader `0x138a8` (in `0x13838`) keeps the `(x + 0x23) << 4` arithmetic |
| 6 | Full-frame fills and fades: the `(0, 0, 320, 240)` rectangles | e.g. tile `(0, 0, 320, 240)` | — | **`(-32, 0, 384, 240)`** | 16 sites pair `-32` with `240`, 21 pair `384` with `240`, 10 pair `352` with `240` within four instructions; e.g. `000567b8 addiu a0,-0x20; sh a0,8(s1); ori a0,0x180; sh a0,0xc(s1); ori a1,0xf0` (in `0x56728`), `00007768`..`00007784` (in the frame function `0x76d8`), `0003413c ori s4,0x160; 00034140 addiu s5,-0x20` (in `0x340c0`) |
| 7 | The presentation rectangle and x shift themselves (§2.3, §2.4) | — | — | `+32` per vertex; `(0, 12, 384, 216)` → 480 x 272 | `0xa0264` and 50 more; `0xa349c`..`0xa34b8` |

The pairing of #3/#4 with the PC function rests on structure: both project a
point, test a y-derived value to choose the wide or the narrow x range, and
keep the point on either (PC `0x510960`..`0x5109db`: `fcomp [0x5c4244]` then
`[0x5c4240]`/`[0x5c423c]`, else `[0x5c4238]` then `[0x5c4234]`/`[0x5c4230]`;
PSP `0x7cc2c slti t2,t2,0x79` selecting `0x7cc40` or `0x7cc60`). Both PSP
ranges are the PC's widened by 31 on each side; #1 is widened by 46. Two
different margins, so this was not one global constant.

Reading #5 against §2.4: the original left placement sat 145 px from the
left screen edge and the right one 94 px from the right edge (320 - 226).
On the PSP the visible edges are -32 and 352, and 113 - (-32) = 145,
352 - 258 = 94. **The side boxes keep their distance from the screen edge**;
the centre placements (188) are untouched. That is a per-element anchoring
policy §4d item 3 can copy directly.

### 3b. Checked and unchanged

Every other screen-x range the PSX boot EXE holds (its complete
`addiu +K; sltiu M` and `slti`/`slti` inventory, 14 sites) has a PSP twin
with the same bounds (`psp_elf.py ranges`, 23 pairs; the PSP ELF links code
the PSX kept in overlays, so some ranges appear two or three times):

| Range (x unless noted) | PSX | PC (`bof3ext` name) | PSP |
|---|---|---|---|
| `[-99, 419]` | `80156b84` | `-100/420` "object" `0x4CEC09`, `0x5700D1` | `0x1061c`/`0x10624` (fn `0x10528`), `0x19e8cc` (fn `0x19e7bc`) |
| `[-149, 299]`, `[-150, 300]` (y) | `80156ba0`, `80158524` | — | `0x10634`, `0x12cf0`, `0x63440`, `0x19f1c4` |
| `[-60, 380]` (`[-59, 379]` at `80156dac`) | `80158504` | `-60/380` "sprite" `0x4CF319`, `0x4FF6A3`, `0x571366` | `0x12cd4` (fn `0x12c08`), `0x63420`, `0x19f1a8` |
| `[-80, 400]` | `80157050` | `-80/400` "object 2" `0x570319`/`0x570333` | `0x11c20` (fn `0x11ad4`) |
| `[-20, 260]` (y) | `80157070` | — | `0x11c38` |
| `[-64, 384]`, `[-64, 304]` (y) — the `Actor_Task` twin `0x258c` (PSX `0x8014c3c8`) | `8014c4d8`, `8014c4f4` | int16 `384/-64` in `Sprite_Draw` `0x59360B`/`0x593615` | `0x2674`/`0x267c`, `0x268c`/`0x2694` |
| `[-64, 304]`, `[-40, 360]` | `80161ea4`, `80161ef4` | — | `0x968f4`, `0x96934` (fn `0x96884`) |
| `[-20, 340]` | overlay | `-20/340` "unknown" `0x5054E3`/`0x5054FA` | `0x6b490`, `0x6b4a8` (fn `0x6b418`) |
| `SetDefDrawEnv`/`DispEnv` `(0, 0, 320, 240)` x4 | `8014ad80` | `Gpu_SetDefDispEnv` `0x5A78E0` / `DrawEnv` `0x5A7910` callers | `0xe1c` (§2.2) |
| `SetGeomOffset(160, 120)`, `SetGeomScreen(1000)` | `8014ad44`..`ad54`, `801548cc`..`d8` | `Gte_SetGeomOffset` `0x5A7AE0` callers | `0xe434`..`0xe444` (§2.2) |
| The placement table's neighbours `(54,44) (48,82) (220,68) (48,24) (220,192)` and entries 0..2, 7..11 | `0x801802A8`.. | `0x66ADFC`.. | `0x2df0cc`.. identical |

Seven sites still pair `320` with `240` within four instructions: the four
draw-environment calls of §2.2 (`0xe34`, `0xe54`, `0xe6c`, `0xe84`); the
shell's mode-1 handler copying a `(0, frame·240, 320, 240)` rectangle
(`0x2b70cc`, §2.5); `0x1b42c8 slti a2,a1,0x140` / `0x1b42d8 slti a2,a0,0xf0`
in `0x1b4160`, a sprite's `x < 320 && y < 240` test; and `0x1da144`, which
passes `(320, 240)` as a position to `0x1dadb8`. So the
PSP widened the culls that cause visible pop-in (terrain and the area map's
frame pass) and left the sprite/object culls, whose margins of 64 and 100
already cover the extra 32 columns.

## 4. No runtime aspect option

- The presentation rectangle `0x3eebc4` has the three writers of §2.4 and
  nothing else references it; none reads the config block or a save.
- The `.text`/`.data` strings (4,643 of length ≥ 5) contain no "wide",
  "aspect", "16:9", "4:3", "zoom", "stretch", "screen size" or similar; the
  config screen's labels are the PSX's (`Window`, `Background`, `Button`,
  `Config>`, message-speed rows).
- `sceDisplaySetMode(0, 480, 272)` and the GU offset/viewport are called
  once, at init (`0xa360c`, `0xa3678`, `0xa368c`). The only other `SetMode`
  pair is `0xa536c` (saves the current mode with `sceDisplayGetMode` into
  `.bss` raw `0x7c9dc`..`0x7c9e4`, then sets `(0, 480, 272)` at `0xa53a0`)
  and `0xa54fc`, which restores the saved triple; nothing else writes it.

## 5. What §4d can take from this

Capcom's choices, in the form the display-overhaul brainstorm asks for:

1. **Logical view:** widen, do not stretch. 320 + 2 × 32 = 384 logical
   columns at the original scale, centred; the projection centre, draw
   environment and perspective untouched. Vertically, present rows 12..228
   only (a 12-row crop top and bottom), giving a 16:9 logical window
   (384 x 216) at one uniform scale.
2. **Cull bounds:** terrain x `[-50, 370]` → `[-96, 416]`; area-map frame
   x `[-200, 520]` → `[-231, 551]` and `[-50, 370]` → `[-81, 401]`. Sprite,
   object and the remaining ranges unchanged.
3. **UI anchoring:** the message box's side placements keep their distance
   from the screen edge (table entries 3..6 shifted by ∓32); centre placements
   unchanged; full-frame fills widened to `(-32, 0, 384, 240)`.
4. **Fixed art:** the one movie was re-rendered at 480 x 272 and drawn by its
   own path; there is no letterboxing code for it.
5. **No option.** One presentation for the game, one for the boot shell.

Open, for the owner and for the next pass:

- What the handheld shows during the splash sequence versus in play (§2.5's
  hypothesis), and whether any HUD element sits in the cropped 12-row bands.
- Per-element UI positions that live in code rather than in the placement
  table (the title rows, the load menu's panel x, the money box — the PC sites
  `bof3ext` patches at `0x588C26`, `0x5881AC`, `0x589F9C`, …) were not paired
  on the PSP; a survey of `ori`/`addiu` immediates in the PSP's menu code
  against those PC values is the next half-day.
- The PSP's other two `SetGeomOffset` callers (`(160, 144)`, `(?, 185)`) want
  their PC twins before they can be called unchanged.
