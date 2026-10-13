# The port's arena edits and Ryu's byte, rendered: PC against the Japanese disc

**Status:** MEASURED (2026-10-13; rendered, not played; every claim below from
the runs in section 1)

The owner's answer to [`disc-data-inventory.md`](disc-data-inventory.md)
section 6, Q1 and Q3: "go ahead and render". This renders every frame the port
edited in the 11 hand-edited arenas (and `START`, which carries `PL012`'s), and
image 10 of Ryu's form bank. The PC's chunk is rendered beside the Japanese
disc's form of the same chunk. It gives a reading per edit and the rule rows for
either answer. Nothing here is applied, and no output of the importer changes.

Hard rule 1: the renders, the per-frame lists and anything with a byte of the
game in it are under `{main}/analysis/shots/arena-render-1013/`. This page
carries structure only: offsets, counts, coordinates and descriptions.

## 0. The answer in five lines

1. **Ryu's byte is a corruption, not an edit (Q3).** The byte at `+0x7ACE` is a
   flag byte of image 10's LZSS stream, and the port's value turns one match
   into a literal. The PC's stream then decodes out of step. 332 of the image's
   2,048 texels change from (59, 6) to the end, 14 of them become indices
   outside the 32-colour palette, and the stream stops 39 bytes short of its
   record. The disc's stream ends exactly. The one frame that draws the image
   shows stray noise along its parts' lower edges on the PC. **Take the
   disc's.**
2. **The field edit (`PL` member 2, scripts 0, 4, 18..21, 34..37) is an art
   repair of Capcom's own.** It moves the sprite's unmirrored half one pixel
   toward its mirrored half. On the disc, 3 of the 10 frames those scripts draw
   have a see-through slit on the centre line (4 px between the horns in
   s0 f0; 11 px down the head in s20 f2 and s21 f1). The PC closes the first
   and shortens the others to 4-5 px. The cost is one new see-through pixel at
   the right-hand edge of five front-view frames. Read against the
   PlayStation's own part draw, the slits are on the PlayStation too.
3. **The other two edits draw nothing from their data.** Field script 42 and
   battle script 1 (`BPLD012`/`015`/`016`) each gain a part that samples VRAM
   outside the uploaded image: 0 pixels differ in all 16 frames. The only
   measurable effect is on the PC renderer's cell-texture cache key. The
   disc's script-42 frames share their key with the same member's battle
   script 1 in `BPLU`; the PC's frames do not. The two pictures are identical,
   so the collision this breaks shows nothing wrong. **Take the disc's**,
   unless a live capture shows otherwise (section 5).
4. Only one frame per field script is ever drawn (each of the ten has one
   step). Of the 79 differing frame pairs of the member-2 bank, 24 are drawn:
   10 nudged and 14 of script 42. The rest are copies no step names.
5. All eight `PL` arenas and `START` carry the same member-2 bank, byte for
   byte (one hash each side: PC `46a7856dc296`, disc `2fd0d7222b96`), and the
   three `BPLD` arenas carry the same member-1 bank. So there are two answers
   to make, not eleven.

## 1. Method and runs

The two sources are the PC's `DAT/` and `CDImage/Breath of Fire III
(Japan).cue`, through `importer.open_source`. Each chunk's disc form is
`cache_inventory.disc_form`, the same function the inventory measured with
(run D3 there). For the arenas that is `own:arena` (the disc's section,
type-1 decoded), and for `START` it is `own:start-arena` (the disc's `PL012`).
For `RYUD` it is `rule:ryud` taken raw, before the rule: the disc's own
bytes.

The new tool is `tools/arena_render.py`. It draws a frame the way the PC's code
does:

- **image**: the frame entry's byte 0 names the image
  (`Sprite_SetFrameQueueUpload` `0x5894D0`). Its record is s16 w, s16 h, u32
  kind, then six 5-bit texels a dword (`Gfx_UploadPacked5`) or LZSS
  (`Gfx_UploadLzss`). That gives eight-bit texels, 2w wide. Every image of
  these banks is 256 texels wide.
- **palette**: a u32 offset table, then 32 BGR555 colours each
  (`Sprite_LoadPalette`); colour 0 is transparent.
- **frame**: a count, then 5-byte parts (`Sprite_Draw` `0x5935B0`). Byte 0's
  low nibble is the size, 8..32 by 8..32 (`SpriteCell_Sizes`), and bit 7
  mirrors the part. Then come s8 x, s8 y, u, v.
- **drawing**: each part's texels are copied over what is already drawn, so
  later parts sit on top. A mirrored part's row is reversed inside its own box
  (`tex_cells.cpp` `DrawCells` / `CellUnpack`).

Two things are left out, on both sides alike: the whole-sprite flip (`+0x2A`)
and the object's palette choice (palette 0 is used).

**Does the PlayStation draw the same?** The PSX twin of `Sprite_Draw` is
`0x8014C3C8` (`Actor_Task` in the sibling's `symbols.toml`; twin as
[`psp-widescreen.md`](psp-widescreen.md) cites it). Disassembled from the JP
`SLPS_009.90` with capstone (scratch `render/psxdis.py`), its part loop picks a
mode: part bit 7, `| 2` with the sprite's `+0x2A`, `| 4` when scaled.

- Modes 0 and 3 go to `0x8014CE90`, a `SPRT` at pos + x, or at pos - x - w
  under the flip.
- Mode 1 is a `POLY_FT4` over x..x + w with u running from u + w - 1 to u - 1.
- Mode 2 is the same quad placed at pos - x - w.

So an unflipped frame places every part where the PC's cells place it, and
mirrors in-box. The u - 1 is the usual correction for the GPU's
left-inclusive sampling, which this pass did not measure on hardware. So a
seam in these renders is taken to be on the PlayStation as well. That is an
inference from the code, not a capture.

| Run | Command (from `{main}`) | Result |
|---|---|---|
| R1 | `python <wt>/tools/arena_render.py arenas --date 2026-10-13 --source bof3/DAT --source "CDImage/Breath of Fire III (Japan).cue" --out analysis/shots/arena-render-1013` | 12 arenas, 12 sheets + 12 `_seams` sheets + `README.md` (per frame: the part changes, the pixel counts, the holes), 11.7 s |
| R2 | the same, `ryud` | `RYUD_image10.png`, `RYUD.md` |
| R3 | scratch `render/seams.py`, `collide.py`, `added.py`, `keys2.py`, `bplu.py`, `ryudcheck.py` | the metrics quoted below; `PL012_s42_vs_BPLU012_s1.png` |

**Metrics.** A *1-px hole* is a transparent pixel with opaque neighbours on
both sides, left and right or above and below: a see-through crack. *Pixel
difference* means the two frames rasterised in sprite coordinates, counted as
PC-only opaque, disc-only opaque, and recoloured. A *drawn* frame is one that
some step of its script names (`stepped`); jumps are skipped.

## 2. The sheets (`{main}/analysis/shots/arena-render-1013/`)

| Sheet | What |
|---|---|
| `PL012.png` `PL025.png` `PL026.png` `PL247.png` `PL257.png` `PL267.png` `PL278.png` `PL27A.png` `START.png` | member 2's bank: 79 frame pairs each, drawn frames first. PC, JP and a difference panel at 4x nearest, with part outlines: red moved or changed, green PC-only, blue disc-only. The nine are the same bank, so they are the same pictures |
| `<arena>_seams.png` for the same nine | the 24 drawn differing frames, PC then JP, on a magenta ground (see-through pixels show), holes outlined white, x = 0 in yellow |
| `BPLD012.png` `BPLD015.png` `BPLD016.png` (+ `_seams`) | member 1's battle bank: script 1 frames 6 and 15 |
| `RYUD_image10.png` | image 10 of `RYUD00`, PC and JP at 8x with the 332 differing texels outlined; columns 35..98 at 16x with the first changed texel circled; script 10 frame 0 as drawn, PC and JP, at 8x, with the parts that sample a changed texel outlined |
| `PL012_s42_vs_BPLU012_s1.png` | the cache-key collision of section 4: the disc's field frames beside the battle frames with the same cells |
| `README.md`, `RYUD.md` | each sheet's frames, part by part, and the counts |

## 3. Q3: Ryu's one byte

`RYUD00..03` differ from the disc in one byte, `+0x7ACE`, the same in all four
(R2). It falls in image 10 (256 x 8 texels, a kind-2 LZSS record of 828 bytes
after its header, at `0x7834`). **It is a flag byte of the stream:** the PC's
value sets bit 3, which turns that group's fourth token from a two-byte match
into a one-byte literal. Everything after it decodes out of step.

| | Stream bytes consumed (of 828) | Texels produced (of 2,048) | Texels above 31 |
|---|---:|---:|---:|
| JP disc | 827 | 2,048 | 0 |
| PC | 789 | 2,058 (10 past the end) | 14 |

Every other LZSS image of the bank, on both sides, consumes its record to
within 3 bytes and has no index above 31 (`ryudcheck.py`: images 2..9 and
12..15). The image is drawn by one script, 10, through its frame 0, whose
parts cover rows 0..7 of u 0..191. On the PC that frame has stray pixels along
the lower two rows of the parts that sample u 59 and up (outlined red on the
sheet). Some are wrong colours of the palette, and the magenta ones are
indices outside the 32 (sheet
`RYUD_image10.png`, bottom panels). The disc's frame is clean.

**Reading: a corruption in the port's data, not an edit.** A one-bit change
that desynchronises a compressed stream draws nothing anyone would choose.
The inventory's "at most two texels" (section 2, P7) assumed an uncompressed
image. It is 332.

## 4. Q1: the arenas

### 4.1 Member 2's field bank (`PL012` `025` `026` `247` `257` `267` `278` `27A`, and `START`)

**The nudges (scripts 0, 4, 18..21, 34..37).** Each script has one step, so
each draws one frame:

| Script | 0 | 4 | 18 | 19 | 20 | 21 | 34 | 35 | 36 | 37 |
|---|---|---|---|---|---|---|---|---|---|---|
| frame drawn | 0 | 0 | 3 | 4 | 2 | 1 | 4 | 3 | 2 | 1 |

Scripts 0 and 18..21 show the front view (image 0); 4 and 34..37 the back
(image 4).

In the pairs where the parts keep their order, every unmirrored part moved +1
in x (625 by +1, 24 by +2). Of the mirrored parts, 301 stayed and 34 moved
+1 (scratch `probe2.py`; 625 + 34 is `arena-edits.md`'s 659). The sprite is built as a left half and its mirror, so the left
half moves one pixel toward the right. The difference panel of every drawn
frame shows exactly that: the left half recoloured and the right half
unchanged. Per drawn frame (R1 `README.md`):

| Frame | Pixels PC-only / disc-only / recoloured | 1-px holes PC / disc | What it looks like |
|---|---|---|---|
| s0 f0 (front) | 40 / 70 / 983 | 7 / 10 | the disc has a 4-px see-through slit on the centre line between the horns (x 0, y -47..-44); the PC has none |
| s20 f2 (front) | 55 / 78 / 897 | 13 / 23 | the disc has an 11-px slit down the head at x 4 (y -45..-35) and a short one at x 0; the PC has a 4..5-px slit at x 1 |
| s21 f1 (front) | 56 / 79 / 1,187 | 10 / 21 | as s20 f2 |
| s18 f3, s19 f4 (front) | 71 / 95 / 981; 89 / 113 / 1,279 | 10 / 9 each | the half shifted; three parts also exchange a mirrored copy for an unmirrored one (or back) on the other side; the slits between the horns move but stay |
| s4 f0, s34 f4, s35 f3, s36 f2, s37 f1 (back) | 22..62 PC-only, 52..75 disc-only | equal | the dark centre line of the back one pixel narrower |

Two side effects are measured. The edit opens a new see-through pixel at
(31, -12), the right hand's edge, in all five front-view drawn frames, which
the disc does not have. Five of the never-drawn copies (s4 and s34 frames 1
and 2) also rearrange five parts each.

**Reading: a repair, of Capcom's art.** The disc's frames have real
see-through cracks along the axis where the two halves meet. They are 1 px
wide and up to 11 tall, on the head of three of the five front poses. The
port's one-pixel tightening closes or shortens them. By the PSX part draw read
in section 1, the PlayStation draws the same cracks, so this repairs the
original's art rather than a port seam. Under the stance
(a defect the PlayStation shares is Capcom's; the owner, 2026-10-05) keeping it
is a ledger entry. Everything else
the nudges do is a one-pixel narrowing nobody would see without the
difference panel.

**Script 42 (16 frames, 14 of them differ, all drawn).** Each of frames 2..15
gains one 8 x 8 part at (-13, -10) that samples u 248, v 248. Image 18 is
256 x 80, so v 248 is outside it: the cell's `v` is `+0x26 + 248` kept as a
byte, which is 8 rows above the image in its page when `+0x26 >= 8`. That
part's texels are not this bank's data. Pixels differing: 0 in all 14 frames.
The extent is unchanged too (`added.py`). The one measurable effect is the cell
cache: `D3d_CellTexture` `0x5A3160` hits on the part count, the CLUT and the
32-bit sum of the cell records, not on which image was uploaded.

- Over 33,929 drawn frames of every PC arena, the disc's s42 frames share
  their key with member 2's battle script 1 frame for frame in all eight
  `BPLU` arenas.
- The PC's frames share it with nobody. Their s42 f9 / f10 / f15 share it
  only among themselves, and those are identical frames.

The colliding pictures are identical: same cells, same palette, the same
pixels from their own images (`bplu.py`, sheet
`PL012_s42_vs_BPLU012_s1.png`). One thing outside the key is the build's
backdrop flag (`flags & 0x800`, `tex_cells.cpp` 0x5A32B0). A field-built
texture reused in battle could carry the wrong canvas only if the two
differed in that flag, and that was not measured.

**Reading: no visible content.** If the port's purpose was the cache
collision, the collision draws the same picture. The added part's own texels
are whatever VRAM holds outside the image, so on the PC it is the one change
that could draw something unintended.

### 4.2 Member 1's battle bank (`BPLD012` `015` `016`)

Script 1 (29 frames, every frame drawn) gains one part in frame 6, an 8 x 16
at (-8, -22) sampling v 128, and one in frame 15, a mirrored 8 x 8 at (12, -28)
sampling v 200. Image 2 is 256 x 56, so both are outside it. Pixels
differing: 0. Extent: unchanged. Cache key: the disc's two frames share their
key with no other drawn arena frame (`added.py`).

**Reading: no visible content from the data.** No purpose was found. The
sheet (`BPLD012.png`) shows the added boxes (green) inside the sprite's extent
in both frames. What they draw at run time is
whatever VRAM holds at those rows.

## 5. What is not settled, and how to settle it

- **Whether the slits show in play.** They are 1-px see-through pixels on the
  head of three front poses. The background behind decides how visible they
  are. Which game situations step scripts 0 and 18..21 was not traced. A
  frame capture of member 2 standing in a field (`BOF3X_SHOT_DIR`), with the
  PC's arena and with the disc's, would show it.
- **What the added parts draw on the PC.** A VRAM read (`tools/mem_dump.py`
  region of the slot's page) during script 42 or battle script 1 would show
  whether rows `+0x26 - 8` (field) and `+0x26 + 128` / `+0x26 + 200` (battle)
  are blank.
- **The PlayStation's sampling.** The reading that the slits are Capcom's rests
  on the PSX code's u + w - 1 .. u - 1 and the GPU's usual left-inclusive
  sampling, not on a capture. The sibling's emulator could capture member 2 in
  the same pose.

## 6. The owner's choice, and the rule rows for each (not applied)

The two banks are independent answers. One also has to ask what a **disc-only**
install gets, since it cannot carry the port's bytes (rule 1,
[`arena-edits.md`](arena-edits.md) 4).

**Ryu's byte (Q3).** Recommended: take the disc's.

- PC + disc cache: in `recipes/pc-zh.toml`, `RYUD00..03` chunk 1 stops listing
  `psx-*:ryud` as a source of the PC's bytes. The disc's own section is taken
  as is (`copy`, a new `own_why = "ryud-lzss"` stand-in preferred over
  `pc-zh:chunk` by the base policy of [`disc-data-inventory.md`](disc-data-inventory.md)
  section 9), and the `ryud` transform is deleted from `importer.py`.
- PC-only install: a rule can repair the PC's own file without copying a
  value. Clear bit 3 of the byte at `+0x7ACE` of each `RYUD`, and verify by
  structure: image 10's stream must consume its record and produce exactly
  w x h x 2 texels, all below 32. As an `importer.py` transform it would be
  `lzss_flag`, applied when the PC is the source and refused if the check
  fails.
- Ledger (DIV-XXXX, draft):
  - **Original behaviour:** the PC port (2001) ships Ryu's form data with one
    flag bit of image 10's LZSS stream set. The stream decodes out of step from
    texel (59, 6), so 332 texels are wrong, 14 outside the palette, and the one
    frame drawing that image (script 10 frame 0) shows stray pixels. The
    PlayStation's stream is whole.
  - **New behaviour:** the image decodes as the disc has it: from the disc when
    one is a source, else by clearing the bit.
  - **Why:** a data corruption, not an edit (this page, section 3).
  - **PSX?** No: PC-only.
  - **Reversible?** Turn the rule off.

  It is a port defect, so it may equally go to `known-defects.md` as a
  repaired defect. The coordinator decides the form.

**Member 2's nudges (Q1, field).** My advice is to keep the PC's for the
nudges, as a repair of Capcom's art, and take the disc's for the added part.
That split needs a transform, because the PC's bank carries both:

- **Keep the PC's (today's behaviour for a PC + disc cache).** Owed: a DIV,
  since the PC's arena repairs the PlayStation's art. A disc-only cache keeps
  the disc's (DIV-0096 stands). So the two installs differ in these ten
  frames.
- **Take the disc's (the stance's default).** The inventory's section 8.3 row:
  DIV-0096 widens from "a cache built without the PC's `DAT/`" to "any cache
  with a PSX disc". In `pc-zh.toml` the eight `PL` rows and `START` prefer
  `own:arena` / `own:start-arena` over `pc-zh:chunk`. That is a base-policy
  change, inventory section 9.
- **The split (keep the nudges, drop script 42's part).** Not derivable as a
  rule of the disc's data alone: the nudges are not every unmirrored part
  (34 mirrored parts moved, and five frames rearranged), so a rule would have
  to list the port's per-part steps, which rule 1 keeps out. From the PC's
  own arena it is derivable: remove the one part that samples outside its
  image from each frame of script 42 (a structural test). Then rewrite the
  script offsets, the frame entries and the arena header (the -352 / +70
  arithmetic of [`arena-edits.md`](arena-edits.md) 2). A disc-only install
  still gets the disc's nudges.

**Member 1's battle part (Q1, battle).** Recommended: take the disc's. It is
the stance's default and nothing visible is lost. Rows as "take the disc's"
above, for the three `BPLD` rows. If the owner prefers to wait for the VRAM
read of section 5, today's behaviour stands.
